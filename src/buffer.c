#include <buffer.h>

#include <assert.h>
#include <environment.h>
#include <error.h>
#include <errno.h>
#include <file_io.h>
#include <rope.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <types.h>
#include <utility.h>


const char *buf_hst_type_string(BufferHistoryType type) {
  switch (type) {
  case BUF_HST_INSERT: return "INSERT";
  case BUF_HST_REMOVE: return "REMOVE";
  default: return "INVALID";
  }
  return "UNREACHABLE";
}

void buf_hst_print_node(BufferHistoryNode *node) {
  if (!node) {
    printf("\n");
    return;
  }
  buf_hst_print_node(node->next);
  printf("%s:%zu:%zu:\"%s\"\n",
         buf_hst_type_string(node->type),
         node->offset, node->length,
         node->data);
}

void buf_hst_create_node(BufferHistoryType type, BufferHistoryNode **head, char *data, size_t offset, size_t length) {
  BufferHistoryNode *new_node = calloc(1, sizeof *new_node);

  new_node->type   = type;
  new_node->offset = offset;
  new_node->length = length;
  new_node->data   = data;

  new_node->next = *head;
  *head = new_node;
}

Error buffer_create_hst_node(Buffer *buffer, BufferHistoryType type, char *data, size_t offset, size_t length) {
  // Everywhere the buffer is modified and requires a new history node,
  // there /should/ be a modification that's been done, so this flag
  // being set here means we don't have to set it everywhere.
  buffer->modified = 1;
  // If the previous head is of the same type...
  if (buffer->history.undo && buffer->history.undo->type == type) {
    // If the cursor is in the correct position.
    switch (type) {
    case BUF_HST_INSERT:{
      // NOTE: Prepend insertion is possible, but it would be counter-intuitive to use.
      if (buffer->point_byte == buffer->history.undo->offset + buffer->history.undo->length) {
        // Append insertion.
        size_t new_length = buffer->history.undo->length + length;
        char *new_data = malloc(new_length + 1);
        memcpy(new_data, buffer->history.undo->data, buffer->history.undo->length);
        memcpy(new_data + buffer->history.undo->length, data, length);
        new_data[new_length] = '\0';
        char *old_data = buffer->history.undo->data;

        buffer->history.undo->length = new_length;
        buffer->history.undo->data = new_data;

        free(old_data);
        return ok;
      }
    } break;
    case BUF_HST_REMOVE: {
      // NOTE: Append removal is possible, but it would be counter-intuitive to use.
      if (buffer->point_byte + length == buffer->history.undo->offset) {
        // Prepend removal.
        size_t new_offset = buffer->history.undo->offset - length;
        size_t new_length = buffer->history.undo->length + length;
        char *new_data = malloc(new_length + 1);
        memcpy(new_data, data, length);
        memcpy(new_data + length, buffer->history.undo->data, buffer->history.undo->length);
        new_data[new_length] = '\0';
        char *old_data = buffer->history.undo->data;

        buffer->history.undo->offset = new_offset;
        buffer->history.undo->length = new_length;
        buffer->history.undo->data = new_data;

        free(old_data);
        return ok;
      }
    } break;
    default: {
      MAKE_ERROR(err, ERROR_GENERIC, nil, "Unhandled buffer history type in buffer_create_hst_node merging.", NULL);
      return err;
    } break;
    }
  }
  buf_hst_create_node(type, &buffer->history.undo, data, offset, length);
  return ok;
}

Buffer *buffer_create(char *path) {
  if (!path) {
    MAKE_ERROR(args, ERROR_ARGUMENTS, nil
                 , "buffer_create: PATH must not be NULL!"
                 , NULL);
    print_error(args);
    return NULL;
  }
  Buffer *buffer = calloc(1,sizeof(Buffer));
  if (!buffer) {
    MAKE_ERROR(oom, ERROR_MEMORY, nil
               , "buffer_create: Could not allocate new buffer (out of memory)."
               , "Find a way to free memory on your system."
               );
    print_error(oom);
    return NULL;
  }
  buffer->path = path;

  Rope *rope = NULL;
  const SimpleFile file = get_file(buffer->path);
  if (file.flags & SMPL_FILE_FLAG_OK) {
    rope = rope_from_buffer(file.contents, file.size);
    free_file(file);
  } else {
    rope = rope_create("");
  }
  if (!rope) {
    free(buffer);
    MAKE_ERROR(err, ERROR_MEMORY, nil
               , "buffer_create: Could not create rope for new buffer."
               , NULL);
    print_error(err);
    return NULL;
  }
  buffer->rope = rope;
  return buffer;
}

size_t buffer_size(Buffer buffer) {
  if (!buffer.rope) { return 0; }
  return buffer.rope->weight;
}

Error buffer_insert(Buffer *buffer, char* string) {
  if (!string) {
    MAKE_ERROR(args, ERROR_ARGUMENTS, nil
               , "buffer_insert: Can not insert NULL string into buffer."
               , NULL);
    return args;
  }
  if (!buffer) {
    MAKE_ERROR(args, ERROR_ARGUMENTS, nil
               , "buffer_insert: Can not insert into NULL buffer."
               , NULL);
    return args;
  }
  Rope *new_rope = rope_insert(buffer->rope, buffer->point_byte, string);
  if (!new_rope) {
    MAKE_ERROR(err, ERROR_MEMORY, nil
               , "buffer_insert: Could not insert into buffer."
               , NULL);
    return err;
  }

  char *data = strdup(string);
  buffer_create_hst_node(buffer, BUF_HST_INSERT, data, buffer->point_byte, strlen(data));

  buffer->point_byte += strlen(string);
  // Clear mark activation bit.
  buffer->mark_byte &= ~BUFFER_MARK_ACTIVATION_BIT;
  buffer->rope = new_rope;
  return ok;
}

Error buffer_insert_indexed(Buffer *buffer, size_t byte_index, char* string) {
  if (!string) {
    MAKE_ERROR(args, ERROR_ARGUMENTS, nil
               , "buffer_insert: Can not insert NULL string into buffer."
               , NULL);
    return args;
  }
  if (!buffer) {
    MAKE_ERROR(args, ERROR_ARGUMENTS, nil
               , "buffer_insert: Can not insert into NULL buffer."
               , NULL);
    return args;
  }
  Rope *new_rope = rope_insert(buffer->rope, byte_index, string);
  if (!new_rope) {
    MAKE_ERROR(err, ERROR_MEMORY, nil
               , "buffer_insert: Could not insert into buffer."
               , NULL);
    return err;
  }

  char *data = strdup(string);
  buffer_create_hst_node(buffer, BUF_HST_INSERT, data, byte_index, strlen(data));

  if (byte_index > new_rope->weight) {
    buffer->point_byte = new_rope->weight;
  } else {
    buffer->point_byte = byte_index + strlen(string) + 1;
  }
  // Clear mark activation bit.
  buffer->mark_byte &= ~BUFFER_MARK_ACTIVATION_BIT;
  buffer->rope = new_rope;
  return ok;
}

Error buffer_prepend(Buffer* buffer, char *string) {
  return buffer_insert_indexed(buffer, 0, string);
}

Error buffer_append(Buffer* buffer, char *string) {
  return buffer_insert_indexed(buffer, SIZE_MAX, string);
}

Error buffer_insert_byte(Buffer *buffer, char byte) {
  if (!buffer) {
    MAKE_ERROR(args, ERROR_ARGUMENTS, nil
               , "buffer_insert_byte: Buffer must not be NULL."
               , NULL);
    return args;
  }
  Rope *rope = rope_insert_byte(buffer->rope, buffer->point_byte, byte);
  if (!rope) {
    MAKE_ERROR(args, ERROR_ARGUMENTS, nil
               , "buffer_insert_byte: Could not insert byte into buffer rope."
               , NULL);
    return args;
  }

  char *data = calloc(1,2);
  data[0] = byte;
  buffer_create_hst_node(buffer, BUF_HST_INSERT, data, buffer->point_byte, 1);

  buffer->point_byte += 1;
  // Clear mark activation bit.
  buffer->mark_byte &= ~BUFFER_MARK_ACTIVATION_BIT;
  buffer->rope = rope;
  return ok;
}

Error buffer_insert_byte_indexed(Buffer *buffer, size_t byte_index, char byte) {
  if (!buffer) {
    MAKE_ERROR(args, ERROR_ARGUMENTS, nil
               , "buffer_insert_byte_indexed: Buffer must not be NULL."
               , NULL);
    return args;
  }
  Rope *rope = rope_insert_byte(buffer->rope, byte_index, byte);
  if (!rope) {
    MAKE_ERROR(args, ERROR_ARGUMENTS, nil
               , "buffer_insert_byte_indexed: Could not insert byte into buffer rope."
               , NULL);
    return args;
  }

  char *data = calloc(1,2);
  data[0] = byte;
  buffer_create_hst_node(buffer, BUF_HST_INSERT, data, byte_index, 1);

  if (byte_index > buffer->rope->weight) {
    buffer->point_byte = buffer->rope->weight;
  } else {
    buffer->point_byte = byte_index + 1;
  }
  // Clear mark activation bit.
  buffer->mark_byte &= ~BUFFER_MARK_ACTIVATION_BIT;
  buffer->rope = rope;
  return ok;
}
Error buffer_prepend_byte(Buffer *buffer, char byte) {
  return buffer_insert_byte_indexed(buffer, 0, byte);
}
Error buffer_append_byte(Buffer *buffer, char byte) {
  return buffer_insert_byte_indexed(buffer, SIZE_MAX, byte);
}

Error buffer_remove_bytes(Buffer *buffer, size_t *count) {
  if (!buffer || !buffer->rope) {
    MAKE_ERROR(err, ERROR_ARGUMENTS, nil
               , "Can not remove bytes from NULL buffer."
               , NULL);
    return err;
  }
  if (!count || *count == 0) {
    MAKE_ERROR(err, ERROR_ARGUMENTS, nil
               , "Can not remove zero bytes from buffer."
               , NULL);
    return err;
  }
  if (buffer->point_byte >= *count){
    buffer->point_byte -= *count;
  } else {
    *count = buffer->point_byte;
    buffer->point_byte = 0;
  }
  char *span = rope_span(buffer->rope, buffer->point_byte, *count);
  Rope *rope = rope_remove_span(buffer->rope, buffer->point_byte, *count);
  if (!rope) {
    MAKE_ERROR(err, ERROR_GENERIC, nil
               , "Failed to remove span from buffer's rope."
               , NULL);
    return err;
  }

  buffer_create_hst_node(buffer, BUF_HST_REMOVE, span, buffer->point_byte, *count);

  // Clear mark activation bit.
  buffer->mark_byte &= ~BUFFER_MARK_ACTIVATION_BIT;
  buffer->rope = rope;
  return ok;
}

Error buffer_remove_byte(Buffer *buffer) {
  static size_t i = 1;
  return buffer_remove_bytes(buffer, &i);
}

Error buffer_remove_bytes_forward(Buffer *buffer, size_t *count) {
  if (!buffer) {
    MAKE_ERROR(err, ERROR_ARGUMENTS, nil
               , "Can not remove bytes from NULL buffer."
               , NULL);
    return err;
  }
  if (!count || *count == 0) {
    MAKE_ERROR(err, ERROR_ARGUMENTS, nil
               , "Can not remove zero bytes from buffer."
               , NULL);
    return err;
  }
  if (!buffer->rope) {
    MAKE_ERROR(err, ERROR_GENERIC, nil
               , "Can not remove from buffer with NULL rope."
               , NULL);
    return err;
  }
  size_t size = buffer->rope->weight;
  if (buffer->point_byte + *count >= size) {
    *count = size - buffer->point_byte;
    if (*count == 0) {
      return ok;
    }
    // Protect against unsigned underflow on subtraction.
    if (*count > size) {
      MAKE_ERROR(err, ERROR_GENERIC, nil
                 , "Can not remove from buffer when point is greater than size."
                 , NULL);
      return err;
    }
  }
  char *span = rope_span(buffer->rope, buffer->point_byte, *count);
  Rope *rope = rope_remove_span(buffer->rope, buffer->point_byte, *count);
  if (!rope) {
    MAKE_ERROR(err, ERROR_GENERIC, nil
               , "Failed to remove span from buffer rope."
               , NULL);
    return err;
  }

  buffer_create_hst_node(buffer, BUF_HST_REMOVE, span, buffer->point_byte, *count);

  // Clear mark activation bit.
  buffer->mark_byte &= ~BUFFER_MARK_ACTIVATION_BIT;
  buffer->rope = rope;
  return ok;
}

Error buffer_remove_byte_forward(Buffer *buffer) {
  static size_t i = 1;
  return buffer_remove_bytes_forward(buffer, &i);
}


Error buffer_undo(Buffer *buffer) {
  BufferHistoryNode **head = &buffer->history.undo;

  // Nothing to do.
  if (!*head) return ok;

  assert(BUF_HST_MAX == 3 && "Exhaustive handling of buffer history node types in buffer_undo().");

  BufferHistoryNode *node = *head;

  switch (node->type) {
  case BUF_HST_INSERT: {
    Rope *new_rope = rope_remove_span(buffer->rope, node->offset, node->length);
    if (!new_rope) {
      MAKE_ERROR(err, ERROR_GENERIC, nil, "UNDO Could not remove from buffer's rope.", NULL);
      return err;
    }
    buffer->point_byte = node->offset;
    buffer->rope = new_rope;
  } break;
  case BUF_HST_REMOVE: {
    // TODO: Use node->length
    Rope *new_rope = rope_insert(buffer->rope, node->offset, node->data);
    if (!new_rope) {
      MAKE_ERROR(err, ERROR_GENERIC, nil, "UNDO Could not insert into buffer's rope.", NULL);
      return err;
    }
    buffer->point_byte = node->offset + node->length;
    buffer->rope = new_rope;
  } break;
  default: {
    MAKE_ERROR(err, ERROR_GENERIC, nil, "Unhandled buffer history node type", NULL);
    return err;
  } break;
  }

  buffer->modified = 1;

  // Remove node by updating head to head->next;
  *head = (*head)->next;

  // Add to redo list.
  node->next = buffer->history.redo;
  buffer->history.redo = node;

  return ok;
}

Error buffer_redo(Buffer *buffer) {
  BufferHistoryNode **head = &buffer->history.redo;

  // Nothing to redo.
  if (!*head) return ok;

  assert(BUF_HST_MAX == 3 && "Exhaustive handling of buffer history node types in buffer_redo().");

  BufferHistoryNode *node = *head;
  switch(node->type) {
    case BUF_HST_INSERT: {
    // TODO: Use node->length
    Rope *new_rope = rope_insert(buffer->rope, node->offset, node->data);
    if (!new_rope) {
      MAKE_ERROR(err, ERROR_GENERIC, nil, "REDO Could not insert into buffer's rope.", NULL);
      return err;
    }
    buffer->point_byte = node->offset + node->length;
    buffer->rope = new_rope;
  } break;
  case BUF_HST_REMOVE: {
    Rope *new_rope = rope_remove_span(buffer->rope, node->offset, node->length);
    if (!new_rope) {
      MAKE_ERROR(err, ERROR_GENERIC, nil, "REDO Could not remove from buffer's rope.", NULL);
      return err;
    }
    buffer->point_byte = node->offset;
    buffer->rope = new_rope;
  } break;
  default: {
    MAKE_ERROR(err, ERROR_GENERIC, nil, "Unhandled buffer history node type", NULL);
    return err;
  } break;
  }

  buffer->modified = 1;

  // Remove node by updating head to head->next;
  *head = (*head)->next;

  // Add to undo list.
  node->next = buffer->history.undo;
  buffer->history.undo = node;

  return ok;
}


Error buffer_row_col(Buffer buffer, size_t offset, size_t *row, size_t *col) {
  // TODO: It may be faster to search by traversing rope nodes instead.
  char *str = buffer_string(buffer);
  if (!str) {
    MAKE_ERROR(err, ERROR_GENERIC, nil, "buffer_row_col could not make string from given buffer.", NULL);
    return err;
  }
  for (char *it = str; it && *it && it < str + offset; it = strchr(it+1, '\n')) {
    *row += 1;
  }
  // Rows were counted, we want the row index.
  if (*row) (*row)--;
  if (*row) {
    char *it = str + offset - 1;
    while (*it != '\n') {
      *col += 1;
      it -= 1;
    }
  } else {
    *col = offset;
  }
  free(str);
  return ok;
}

size_t buffer_mark(Buffer buffer) {
  return buffer.mark_byte &= ~BUFFER_MARK_ACTIVATION_BIT;
}

size_t buffer_mark_active(Buffer buffer) {
  return buffer.mark_byte & BUFFER_MARK_ACTIVATION_BIT ? 1 : 0;
}

Error buffer_set_mark_activation(Buffer *buffer, char active) {
  if (!buffer) {
    MAKE_ERROR(err, ERROR_ARGUMENTS, nil,
               "Can not set mark activation state on NULL buffer.",
               NULL);
    return err;
  }
  // Clear mark active bit.
  buffer->mark_byte &= ~BUFFER_MARK_ACTIVATION_BIT;
  if (active) {
    buffer->mark_byte |= BUFFER_MARK_ACTIVATION_BIT;
  }
  return ok;
}

Error buffer_toggle_mark(Buffer *buffer) {
  if (!buffer) {
    MAKE_ERROR(err, ERROR_ARGUMENTS, nil,
               "Can not toggle mark on NULL buffer.",
               NULL);
    return err;
  }
  if (buffer->mark_byte & BUFFER_MARK_ACTIVATION_BIT) {
    buffer->mark_byte &= ~BUFFER_MARK_ACTIVATION_BIT;
  } else {
    buffer->mark_byte |= BUFFER_MARK_ACTIVATION_BIT;
  }
  return ok;
}

Error buffer_set_mark(Buffer *buffer, size_t mark) {
  if (!buffer) {
    MAKE_ERROR(err, ERROR_ARGUMENTS, nil,
               "Can not set mark on NULL buffer.",
               NULL);
    return err;
  }
  size_t mark_active = buffer_mark_active(*buffer);
  buffer->mark_byte = mark & ~BUFFER_MARK_ACTIVATION_BIT;
  if (mark_active) {
    buffer->mark_byte |= BUFFER_MARK_ACTIVATION_BIT;
  }
  return ok;
}

char *buffer_region(Buffer buffer) {
  size_t offset = 0;
  size_t length = 0;
  size_t mark_byte = buffer_mark(buffer);
  if (buffer.point_byte < mark_byte) {
    offset = buffer.point_byte;
    length = mark_byte - buffer.point_byte;
  } else {
    offset = mark_byte;
    length = buffer.point_byte - mark_byte;
  }
  char *region = rope_span(buffer.rope, offset, length);
  return region;
}

Error buffer_remove_region(Buffer *buffer) {
  Error err = ok;
  if (!buffer) {
    PREP_ERROR(err, ERROR_ARGUMENTS, nil,
               "Can not remove region from NULL buffer.",
               NULL);
    return err;
  }
  size_t length = 0;
  size_t mark_byte = buffer_mark(*buffer);
  if (buffer->point_byte < mark_byte) {
    length = mark_byte - buffer->point_byte;
    buffer_remove_bytes_forward(buffer, &length);
  } else {
    length = buffer->point_byte - mark_byte;
    buffer_remove_bytes(buffer, &length);
  }

  return err;
}

size_t buffer_region_length(Buffer buffer) {
  size_t length = 0;
  size_t mark_byte = buffer_mark(buffer);
  if (buffer.point_byte < mark_byte) {
    length = mark_byte - buffer.point_byte;
  } else {
    length = buffer.point_byte - mark_byte;
  }
  return length;
}

size_t buffer_seek_until_byte
(Buffer *const buffer,
 char *control_string,
 char direction
 )
{
  if (!buffer || !buffer->rope
      || !control_string
      || control_string[0] == '\0')
    {
      return 0;
    }
  if (direction >= 0) {
    for (size_t i = buffer->point_byte + 1;
         i >= buffer->point_byte && i < buffer->rope->weight;
         ++i) {
      if (strchr(control_string, rope_index(buffer->rope, i))) {
        size_t offset = i - buffer->point_byte;
        buffer->point_byte = i;
        return offset;
      }
    }
  } else {
    for (long long i = (long long)buffer->point_byte - 1;
         i >= 0 && i <= (long long)buffer->point_byte;
         --i) {
           if (strchr(control_string, rope_index(buffer->rope, (size_t)i))) {
        size_t offset = buffer->point_byte - (size_t)i;
        buffer->point_byte = (size_t)i;
        return offset;
      }
    }
  }
  return 0;
}

size_t buffer_seek_while_byte
(Buffer *const buffer, char *control_string, char direction) {
  if (!buffer || !buffer->rope || !control_string || control_string[0] == '\0') {
    return 0;
  }
  if (direction >= 0) {
    for (size_t i = buffer->point_byte + 1;
         i >= buffer->point_byte && i < buffer->rope->weight;
         ++i) {
        if (strchr(control_string, rope_index(buffer->rope, (size_t)i)) == NULL) {
        size_t offset = i - buffer->point_byte;
        buffer->point_byte = i;
        return offset;
      }
    }
  } else {
    for (long long i = (long long)buffer->point_byte - 1;
         i >= 0 && i <= (long long)buffer->point_byte;
         --i) {
        if (strchr(control_string, rope_index(buffer->rope, (size_t)i)) == NULL) {
        size_t offset = buffer->point_byte - (size_t)i;
        buffer->point_byte = (size_t)i;
        return offset;
      }
    }
  }
  return 0;
}

size_t buffer_seek_until_substr
(Buffer *const buffer, char *substring, char direction) {
  if (!buffer || !buffer->rope || !substring || substring[0] == '\0') {
    return 0;
  }
  size_t substring_length = strnlen(substring, buffer->rope->weight + 1);
  if (substring_length >= buffer->rope->weight) {
    return 0;
  }
  char *allocated_string = buffer_string(*buffer);
  char *string = allocated_string;
  if (direction >= 0) {
    for (size_t i = buffer->point_byte + 1; i >= buffer->point_byte && i < buffer->rope->weight; ++i) {
      string = allocated_string + i;
      size_t bytes_left = strnlen(string, substring_length + 1);
      if (bytes_left <= substring_length) {
        free(allocated_string);
        return 0;
      }
      if (memcmp(string, substring, substring_length) == 0) {
        size_t offset = i - buffer->point_byte;
        buffer->point_byte = i;
        free(allocated_string);
        return offset;
      }
    }
  } else {
    for (long long i = (long long)buffer->point_byte - 1; i >= 0 && i <= (long long)buffer->point_byte; --i) {
      string = allocated_string + i;
      size_t bytes_left = strnlen(string, substring_length + 1);
      if (bytes_left <= substring_length) {
        free(allocated_string);
        return 0;
      }
      if (memcmp(string, substring, substring_length) == 0) {
        size_t offset = buffer->point_byte - (size_t)i;
        buffer->point_byte = (size_t)i;
        free(allocated_string);
        return offset;
      }
    }
  }
  free(allocated_string);
  return 0;
}

char *buffer_string(Buffer buffer) {
  //char *rope_contents = rope_string(buffer.rope, NULL);
  //if (rope_contents) {
  //  char *string = allocate_string_ignore_carriage_return(rope_contents);
  //  free(rope_contents);
  //  if (string) { return string; }
  //}
  //return NULL;
  return rope_string(buffer.rope, NULL);
}

char *buffer_lines(Buffer buffer, size_t line_number, size_t line_count) {
  return rope_lines(buffer.rope, line_number, line_count);
}

char *buffer_line(Buffer buffer, size_t line_number) {
  return buffer_lines(buffer, line_number, 1);
}

char *buffer_current_line(Buffer buffer) {
  if (!buffer.rope) { return NULL; }
  char *contents = rope_string(buffer.rope, NULL);
  if (!contents) { return NULL; }
  // Search backward for newline, or point_byte of zero.
  size_t point = buffer.point_byte ? buffer.point_byte - 1 : 0;
  char *beg = contents + point;
  while (point && *beg != '\n') {
    beg -= 1;
    point -= 1;
  }
  if (*beg == '\n') {
    beg += 1;
    point += 1;
  }
  char *current_line = NULL;
  size_t line_length = 0;
  // Search forward for end of string, newline, or end of rope.
  char *end = contents + buffer.point_byte;
  while (*end != '\0' && *end != '\n' && point < buffer.rope->weight) {
    end += 1;
  }
  end += 1;
  line_length = (size_t)(end - beg);
  current_line = malloc(line_length + 1);
  if (!current_line) {
    free(contents);
    return NULL;
  }
  memcpy(current_line, beg, line_length);
  current_line[line_length] = '\0';
  free(contents);
  return current_line;
}

void buffer_print(Buffer buffer) {
  char *contents = buffer_string(buffer);
  printf("Buffer:\n"
         "  path:  %s\n"
         "  point: %zu\n"
         "  mark:  %zu\n"
         "  contents:\n"
         "\"%s\"\n"
         , buffer.path ? buffer.path : "<no path>"
         , buffer.point_byte
         , buffer_mark(buffer)
         , contents ? contents : "<no contents>"
         );
  if (contents) {
    free(contents);
  }
}

Error buffer_save(Buffer buffer) {
  if (!buffer.rope) {
    MAKE_ERROR(args, ERROR_ARGUMENTS, nil
               , "buffer_save: Buffer rope may not be NULL."
               , NULL);
    return args;
  }
  if (!buffer.path) {
    MAKE_ERROR(args, ERROR_ARGUMENTS, nil
               , "buffer_save: Buffer path may not be NULL."
               , NULL);
    return args;
  }
  if (buffer.path[0] == '\0') {
    MAKE_ERROR(args, ERROR_ARGUMENTS, nil
               , "buffer_save: Buffer path may not be empty."
               , NULL);
    return args;
  }

  // TODO: Check if overwriting a file, ask user what to do or
  // something.

  FILE *file = fopen(buffer.path, "wb");
  if (!file) {
    printf("Failure to save buffer at \"%s\" -- failed to open file\n"
           "errno=%d\n", buffer.path, errno);
    MAKE_ERROR(err, ERROR_MEMORY, nil
               , "buffer_save: Could not open file for writing."
               , NULL);
    return err;
  }
  char *contents = buffer_string(buffer);
  if (!contents) {
    fclose(file);
    MAKE_ERROR(oom, ERROR_MEMORY, nil
               , "buffer_save: Could not create buffer string."
               , NULL);
    return oom;
  }
  size_t file_size = strlen(contents);
  size_t bytes = fwrite(contents, 1, file_size, file);
  free(contents);
  contents = NULL;
  int close_status = fclose(file);
  if (close_status != 0) {
    printf("Failure to save buffer at \"%s\" -- bad close\n"
           "errno=%d\n", buffer.path, errno);
    MAKE_ERROR(err, ERROR_FILE, nil
               , "buffer_save: Failed to save buffer -- bad close"
               , NULL);
    return err;
  }
  if (file_size != bytes) {
    MAKE_ERROR(err, ERROR_FILE, nil
               , "buffer_save: Could not write contents to file."
               , NULL);
    return err;
  }
  return ok;
}

void buffer_free(Buffer* buffer) {
  if (!buffer) { return; }
  if (buffer->rope) {
    rope_free(buffer->rope);
  }
  if (buffer->path) {
    free(buffer->path);
  }
  free(buffer);
}

Atom initialize_buffer_or_panic(const char *const path) {
  Atom buffer = make_buffer(env_create(nil, 0), (char *)path);
  // TODO: I don't think this check can ever succeed...
  if (nilp(buffer)) {
    exit(1);
  }
  return buffer;
}
