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

/** Open file or create new if one doesn't exist.
 *
 * @param[in] path A string denoting a file path to open a new buffer
 *                 at. The string is copied, and is not needed after
 *                 returning.
 *
 * @return The address to a Buffer structure with a rope containing
 *         the contents of the file at path (if it exists),
 *         or NULL if the operation can not be completed.
 */
Buffer *buffer_create(char *path);

/// Return the buffer's size in bytes.
size_t buffer_size(Buffer buffer);

/// Use `point_byte` to determine insertion point.
Error buffer_insert(Buffer *buffer, char *string);

Error buffer_insert_indexed(Buffer *buffer, size_t byte_index,
                            char *string);
Error buffer_prepend(Buffer *buffer, char *string);
Error buffer_append(Buffer *buffer, char *string);

/// Use `point_byte` to determine insertion point.
Error buffer_insert_byte(Buffer *buffer, char byte);

Error buffer_insert_byte_indexed(Buffer *buffer, size_t byte_index,
                                 char byte);
Error buffer_prepend_byte(Buffer *buffer, char byte);
Error buffer_append_byte(Buffer *buffer, char byte);

Error buffer_remove_bytes(Buffer *buffer, size_t count);
Error buffer_remove_byte(Buffer *buffer);

Error buffer_remove_bytes_forward(Buffer *buffer, size_t count);
Error buffer_remove_byte_forward(Buffer *buffer);

/**
 * Move buffer point to the next byte that is within the control
 * string. If no matching bytes are found, don't move point_byte.
 *
 * @param[in,out] buffer
 *   The buffer to seek within. May alter point_byte member.
 * @param[in] control_string
 *   The search stops when a byte from this string is found under
 *   point.
 * @param[in] direction
 *   Search backwards when this value is negative, otherwise search
 *   forward.
 *
 * @return The amount of bytes point_byte was moved by.
 */
size_t buffer_seek_until_byte(Buffer *const buffer,
                              char *control_string,
                              char direction);

/**
 * Move buffer point to the beginning of the given substring, if it
 * exists. If no matching substring is found, don't move point_byte.
 *
 * @param[in,out] buffer
 *   The buffer to seek within. May alter point_byte member.
 * @param[in] control_string
 *   The search stops when this string is found following point.
 * @param[in] direction
 *   Search backwards when this value is negative, otherwise search
 *   forward.
 *
 * @return The amount of bytes point_byte was moved by.
 */
size_t buffer_seek_until_substr(Buffer *const buffer, char *substring,
                                char direction);

char *buffer_string(Buffer buffer);
char *buffer_lines(Buffer buffer, size_t line_number,
                   size_t line_count);
char *buffer_line(Buffer buffer, size_t line_number);

/// Return the line surrounding `point_byte`
char *buffer_current_line(Buffer buffer);

void buffer_print(Buffer buffer);

Error buffer_save(Buffer buffer);

void buffer_free(Buffer* buffer);

#endif /* LITE_BUFFER_H */
