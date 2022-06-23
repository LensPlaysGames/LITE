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

char *buffer_string(Buffer buffer);
void buffer_print(Buffer buffer);

Error buffer_save(Buffer buffer);

void buffer_free(Buffer* buffer);

#endif /* LITE_BUFFER_H */
