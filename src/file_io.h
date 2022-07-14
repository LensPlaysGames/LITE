#ifndef LITE_FILE_IO_H
#define LITE_FILE_IO_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <error.h>
#include <types.h>

typedef enum SimpleFileFlags {
  SMPL_FILE_FLAG_INVALID = 0,
  SMPL_FILE_FLAG_OK,
} SimpleFileFlags;

typedef struct SimpleFile {
  SimpleFileFlags flags;
  char* path;
  uint8_t *contents;
  size_t size;
} SimpleFile;

SimpleFile get_file(char* path);
void free_file(SimpleFile file);

size_t file_size(FILE *file);

/// Returns a heap-allocated buffer containing the
/// contents of the file found at the given path.
Error file_contents(const char* path, char **result);

Error evaluate_file(Atom environment, const char* path, Atom *result);

#endif /* LITE_FILE_IO_H */
