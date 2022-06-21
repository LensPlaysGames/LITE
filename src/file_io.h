#ifndef LITE_FILE_IO_H
#define LITE_FILE_IO_H

#include <stddef.h>
#include <stdio.h>

#include <error.h>
#include <types.h>

size_t file_size(FILE *file);

/// Returns a heap-allocated buffer containing the
/// contents of the file found at the given path.
char *file_contents(const char* path);

Error load_file(Atom environment, const char* path);

#endif /* LITE_FILE_IO_H */
