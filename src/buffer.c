#include <buffer.h>

#include <error.h>
#include <file_io.h>
#include <rope.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

Buffer *buffer_create(char *path) {
  if (!path) {
    MAKE_ERROR(args, ERROR_ARGUMENTS, nil
                 , "buffer_create: PATH must not be NULL!"
                 , NULL);
    print_error(args);
    return NULL;
  }
  Buffer *buffer = malloc(sizeof(Buffer));
  if (!buffer) {
    MAKE_ERROR(oom, ERROR_MEMORY, nil
               , "buffer_create: Could not allocate new buffer (out of memory)."
               , "Find a way to free memory on your system."
               );
    print_error(oom);
    return NULL;
  }
  buffer->path = path;
  buffer->point_byte = 0;
  char *contents = file_contents(path);
  Rope *rope = NULL;
  if (contents) {
    rope = rope_create(contents);
    free(contents);
  } else {
    rope = rope_create("\n");
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
  buffer->point_byte += strlen(string) + 1;
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
  if (byte_index > new_rope->weight) {
    buffer->point_byte = new_rope->weight;
  } else {
    buffer->point_byte = byte_index + strlen(string) + 1;
  }
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
  buffer->rope = rope_insert_byte(buffer->rope, buffer->point_byte, byte);
  buffer->point_byte += 1;
  return ok;
}

Error buffer_insert_byte_indexed(Buffer *buffer, size_t byte_index, char byte) {
  if (!buffer) {
    MAKE_ERROR(args, ERROR_ARGUMENTS, nil
               , "buffer_insert_byte: Buffer must not be NULL."
               , NULL);
    return args;
  }
  buffer->rope = rope_insert_byte(buffer->rope, byte_index, byte);
  if (byte_index > buffer->rope->weight) {
    buffer->point_byte = buffer->rope->weight;
  } else {
    buffer->point_byte = byte_index + 1;
  }
  return ok;
}
Error buffer_prepend_byte(Buffer *buffer, char byte) {
  return buffer_insert_byte_indexed(buffer, 0, byte);
}
Error buffer_append_byte(Buffer *buffer, char byte) {
  return buffer_insert_byte_indexed(buffer, SIZE_MAX, byte);
}

Error buffer_remove_bytes(Buffer *buffer, size_t count) {
  if (!buffer || !buffer->rope) {
    MAKE_ERROR(err, ERROR_ARGUMENTS, nil
               , "Can not remove bytes from NULL buffer."
               , NULL);
    return err;
  }
  if (count == 0) {
    MAKE_ERROR(err, ERROR_ARGUMENTS, nil
               , "Can not remove zero bytes from buffer."
               , NULL);
    return err;
  }
  if (buffer->point_byte >= count){
    buffer->point_byte -= count;
  } else {
    count = buffer->point_byte;
    buffer->point_byte = 0;
  }
  buffer->rope = rope_remove_span(buffer->rope, buffer->point_byte, count);
  return ok;
}

Error buffer_remove_byte(Buffer *buffer) {
  return buffer_remove_bytes(buffer, 1);
}

char *buffer_string(Buffer buffer) {
  return rope_string(NULL, buffer.rope, NULL);
}

void buffer_print(Buffer buffer) {
  char *contents = buffer_string(buffer);
  printf("Buffer:\n"
         "  path: %s\n"
         "  point: %zu\n"
         "  contents:\n"
         "\"%s\"\n"
         , buffer.path ? buffer.path : "<no path>"
         , buffer.point_byte
         , contents ? contents : "<no contents>"
         );
  if (contents) {
    free(contents);
  }
}

Error buffer_save(Buffer buffer) {
  if (!buffer.rope || !buffer.path) {
    MAKE_ERROR(args, ERROR_ARGUMENTS, nil
               , "buffer_save: Buffer rope and/or path may not be NULL."
               , NULL);
    return args;
  }

  FILE *file = fopen(buffer.path, "w");
  if (!file) {
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
  fclose(file);
  if (file_size != bytes) {
    // TODO: add file error or something.
    MAKE_ERROR(err, ERROR_TODO, nil
               , "buffer_save: Could not write contents to file."
               , NULL);
    return err;
  }

  return ok;
}

void buffer_free(Buffer* buffer) {
  if (buffer->rope) {
    rope_free(buffer->rope);
  }
  if (buffer->path) {
    free(buffer->path);
  }
  free(buffer);
}
