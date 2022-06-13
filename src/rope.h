#ifndef LITE_ROPE_H
#define LITE_ROPE_H

#include <stddef.h>

typedef struct Rope {
  size_t weight;
  char *string;
  struct Rope *left;
  struct Rope *right;
} Rope;

size_t rope_length(Rope *);

char rope_index(Rope *rope, size_t index);

Rope *rope_create(const char *string);
Rope *rope_copy(Rope *original);

Rope *rope_prepend(Rope *rope, char *string);
Rope *rope_insert(Rope *rope, size_t index, char *string);
Rope *rope_append(Rope *rope, char *string);

// TODO: This function is a WIP and is not yet complete.
Rope *rope_insert_char(Rope *rope, size_t index, char c);

/// Convert a rope into a string.
/// Pass NULL as PARENT if passing root node as ROPE.
/// Pass NULL as STRING for a newly allocated string,
/// otherwise STRING will be appended to.
char *rope_string(Rope *parent, Rope *rope, char *string);

/// Free a rope's allocated memory, including itself.
/// Do NOT use any part of a rope after it has been freed.
void rope_free(Rope *rope);

/// Print a representation of a rope structure to stdout.
void rope_print(Rope *rope, size_t given_depth);

#endif /* LITE_ROPE_H */
