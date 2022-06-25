#ifndef LITE_BUFFER_H
#define LITE_BUFFER_H

#include <error.h>
#include <rope.h>

typedef struct Buffer {
  Atom environment;
  char *path;
  Rope *rope;
  size_t point_byte;
} Buffer;

/// Open file or create new if one doesn't exist.
Buffer *buffer_create(char *path);

/// Return the buffer's size in bytes.
size_t buffer_size(Buffer buffer);

/// Use `point_byte` to determine insertion point.
Error buffer_insert(Buffer *buffer, char *string);

Error buffer_insert_indexed(Buffer *buffer, size_t byte_index
                            , char *string);
Error buffer_prepend(Buffer *buffer, char *string);
Error buffer_append(Buffer *buffer, char *string);

/// Use `point_byte` to determine insertion point.
Error buffer_insert_byte(Buffer *buffer, char byte);

Error buffer_insert_byte_indexed(Buffer *buffer, size_t byte_index
                                 , char byte);
Error buffer_prepend_byte(Buffer *buffer, char byte);
Error buffer_append_byte(Buffer *buffer, char byte);

Error buffer_remove_bytes(Buffer *buffer, size_t count);
Error buffer_remove_byte(Buffer *buffer);

Error buffer_remove_bytes_forward(Buffer *buffer, size_t count);
Error buffer_remove_byte_forward(Buffer *buffer);

char *buffer_string(Buffer buffer);
char *buffer_lines(Buffer buffer, size_t line_number
                   , size_t line_count);
char *buffer_line(Buffer buffer, size_t line_number);
/// Return the line surrounding `point_byte`
char *buffer_current_line(Buffer buffer);

void buffer_print(Buffer buffer);

Error buffer_save(Buffer buffer);

void buffer_free(Buffer* buffer);

#endif /* LITE_BUFFER_H */
