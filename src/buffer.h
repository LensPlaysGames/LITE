#ifndef LITE_BUFFER_H
#define LITE_BUFFER_H

#include <stddef.h>

#include <error.h>
#include <rope.h>

#if (SIZE_MAX == 0xffff)
#  define SIZE_T_BIT 16
#elif (SIZE_MAX == 0xffffffff)
#  define SIZE_T_BIT 32
#elif (SIZE_MAX == 0xffffffffffffffff)
#  define SIZE_T_BIT 64
#endif

#define BUFFER_MARK_ACTIVATION_BIT ((size_t)1 << (SIZE_T_BIT - 1))

typedef enum BufferHistoryType {
  BUF_HST_INVALID = 0,

  // Insert length bytes from data beginning at offset
  BUF_HST_INSERT,

  // Insert length bytes from data beginning at offset
  BUF_HST_REMOVE,

  BUF_HST_MAX
} BufferHistoryType;

const char *buf_hst_type_string(BufferHistoryType type);

typedef struct BufferHistoryNode {
  BufferHistoryType type;
  size_t offset; //> point_byte at time of action.
  size_t length; //> how many bytes the action applies to.
  char   *data;  //> data that the action was applied to.

  // FIXME: Use a vector, probably.
  struct BufferHistoryNode *next;
} BufferHistoryNode;

void buf_hst_print_node(BufferHistoryNode *node);

typedef struct BufferHistory {
  BufferHistoryNode *undo;
  BufferHistoryNode *redo;
} BufferHistory;

void buf_hst_create_node(BufferHistoryType type, BufferHistoryNode **head, char *data, size_t offset, size_t length);



typedef struct Buffer {
  Atom environment;
  char *path;
  Rope *rope;
  size_t point_byte;
  size_t mark_byte; // Highest bit denotes activation

  BufferHistory history;

  char modified;
  char needs_redraw;
} Buffer;

/** Open file or create new if one doesn't exist.
 *
 * @param[in] path A string denoting a file path to open a new buffer
 *                 at. The string transfers ownership to the buffer.
 *                 DO NOT FREE THE STRING. It is expected to be an
 *                 absolute path.
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

Error buffer_insert_indexed(Buffer *buffer, size_t byte_index, char *string);
Error buffer_prepend(Buffer *buffer, char *string);
Error buffer_append(Buffer *buffer, char *string);

/// Use `point_byte` to determine insertion point.
Error buffer_insert_byte(Buffer *buffer, char byte);

Error buffer_insert_byte_indexed(Buffer *buffer, size_t byte_index, char byte);
Error buffer_prepend_byte(Buffer *buffer, char byte);
Error buffer_append_byte(Buffer *buffer, char byte);

/** Remove the given amount of bytes starting at point going backward.
  * @param[in, out] count
  *   The amount of bytes to remove. Updated to the amount of bytes
  *   actually removed.
  */
Error buffer_remove_bytes(Buffer *buffer, size_t *count);
Error buffer_remove_byte(Buffer *buffer);

/// Remove the given amount of bytes starting at point going forward.
Error buffer_remove_bytes_forward(Buffer *buffer, size_t *count);
Error buffer_remove_byte_forward(Buffer *buffer);

Error buffer_undo(Buffer *buffer);
Error buffer_redo(Buffer *buffer);

/** Get row/col coordinates of offset within BUFFER.
 *
 * @param[in] buffer
 *   The buffer to calculate the position within.
 * @param[in] offset
 *   The byte position within buffer to calculate position of.
 * @param[out] row
 * @param[out] col
 */
Error buffer_row_col(Buffer buffer, size_t offset, size_t *row, size_t *col);

/** Get BUFFER's mark byte offset.
 *
 * @param[in] buffer
 *   The buffer to get the mark byte from.
 *
 * @return The mark byte offset of the buffer.
 */
size_t buffer_mark(Buffer buffer);

/** Return one iff BUFFER's mark is active. Otherwise, return zero.
 *
 * @param[in] buffer
 *
 * @return The activation state of the mark byte of the buffer.
 */
size_t buffer_mark_active(Buffer buffer);

/** Set mark activation state in given buffer to given state.
 *
 * @param buffer
 *   The buffer to alter mark activation state within.
 * @param active
 *   When non-zero, activate mark in buffer, otherwise de-activate.
 */
Error buffer_set_mark_activation(Buffer *buffer, char active);

/** Toggle BUFFER's mark activation state.
 *
 * When the mark is activated in the given BUFFER, it will be
 * de-activated. Otherwise, it will be activated.
 *
 * @param[in] buffer
 *   The buffer to toggle the mark within.
 */
Error buffer_toggle_mark(Buffer *buffer);

/** Set BUFFER's mark byte offset to MARK.
 *
 * @param[in] buffer
 *   The buffer to set the mark byte offset within.
 * @param[in] mark
 *   The new mark byte offset.
 */
Error buffer_set_mark(Buffer *buffer, size_t mark);

/** Get the selected region as a null-terminated string of bytes.
 * @param[in] buffer
 *   The buffer to read the point and mark from.
 *
 * @return
 *   A string containing the contents between the mark and the point.
 */
char *buffer_region(Buffer buffer);

/** Get the byte length of the selected region.
 * @param[in] buffer
 *   The buffer to read the point and mark from.

 * @return
 *   The absolute value of the byte difference between point and mark.
 */
size_t buffer_region_length(Buffer buffer);

/** Remove the text in the selected region from the given buffer.
 *
 * @param[in] buffer
 *   The buffer to remove the selected region from. The selected region
 *   is between the buffer's mark and point.
 */
Error buffer_remove_region(Buffer *buffer);

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
size_t buffer_seek_until_byte
(Buffer *const buffer,
 char *control_string,
 char direction);

/**
 * Move buffer point to the next byte that is *not* within the control
 * string.
 *
 * @param[in,out] buffer
 *   The buffer to seek within. May alter point_byte member.
 * @param[in] control_string
 *   The search stops when the byte under point is not found within
 *   this string.
 * @param[in] direction
 *   Search backwards when this value is negative, otherwise search
 *   forward.
 *
 * @return The amount of bytes point_byte was moved by.
 */
size_t buffer_seek_while_byte
(Buffer *const buffer,
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
size_t buffer_seek_until_substr
(Buffer *const buffer,
 char *substring,
 char direction);

char *buffer_string(Buffer buffer);
char *buffer_lines
(Buffer buffer,
 size_t line_number,
 size_t line_count);
char *buffer_line(Buffer buffer, size_t line_number);

/// Return the line surrounding `point_byte`
char *buffer_current_line(Buffer buffer);

/// Debug output to stdout concerning given buffer.
void buffer_print(Buffer buffer);

/// Save the given buffer to it's visited filepath.
Error buffer_save(Buffer buffer);

void buffer_free(Buffer* buffer);

#endif /* LITE_BUFFER_H */
