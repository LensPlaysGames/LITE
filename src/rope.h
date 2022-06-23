#ifndef LITE_ROPE_H
#define LITE_ROPE_H

#include <stddef.h>

typedef struct Rope {
  size_t weight;
  char *string;
  struct Rope *left;
  struct Rope *right;
} Rope;

/// Get the total length of the string that the rope represents.
size_t rope_length(Rope *);

/// Get the byte at index.
char rope_index(Rope *rope, size_t index);

/// Create a new rope with contents of string.
/// Be sure to rope_free() when done!!
Rope *rope_create(const char *string);
Rope *rope_copy(Rope *original);

/// Return a new rope with string inserted at the beginning,
/// or NULL if the operation is not able to be completed.
Rope *rope_prepend(Rope *rope, char *string);
/// Return a new rope with string inserted at the end,
/// or NULL if the operation is not able to be completed.
Rope *rope_append(Rope *rope, char *string);
/// Return a new rope with string inserted at index,
/// or NULL if the operation is not able to be completed.
Rope *rope_insert(Rope *rope, size_t index, char *string);

/// Return a new rope with byte inserted at the beginning,
/// or NULL if the operation is not able to be completed.
Rope *rope_prepend_byte(Rope *rope, char c);
/// Return a new rope with byte inserted at the end,
/// or NULL if the operation is not able to be completed.
Rope *rope_append_byte(Rope *rope, char c);
/// Return a new rope with byte inserted at index,
/// or NULL if the operation is not able to be completed.
Rope *rope_insert_byte(Rope *rope, size_t index, char c);

/// Remove a given amount of bytes from the beginning of the given rope.
Rope *rope_remove_from_beginning(Rope *rope, size_t length);
/// Remove a given amount of bytes from the end of the given rope.
Rope *rope_remove_from_end(Rope *rope, size_t length);
/// Remove a given amount of bytes starting at byte offset.
Rope *rope_remove_span(Rope *rope, size_t offset, size_t length);

/// Convert a rope into a string.
/// Pass NULL as PARENT if passing root node as ROPE.
/// Pass NULL as STRING for a newly allocated string,
/// otherwise STRING will be appended to.
char *rope_string(Rope *parent, Rope *rope, char *string);

/// Starting at line `lines` (zero-based), return `count` lines as a string.
char *rope_lines(Rope *rope, size_t lines, size_t count);
/// Return line number `line` (zero-based) as a string.
char *rope_line(Rope *rope, size_t line);

// TODO: This function is a WIP and is not yet complete.
char *rope_span(Rope *rope, size_t amount, size_t index);

/// Free a rope's allocated memory, including itself.
/// Do NOT use any part of a rope after it has been freed.
void rope_free(Rope *rope);

/// Print a representation of a rope structure to stdout.
void rope_print(Rope *rope, size_t given_depth);

#endif /* LITE_ROPE_H */
