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

SimpleFile get_file(char *path);
void free_file(SimpleFile file);

size_t file_size(FILE *file);

/// @return Boolean-like value: 1 if file at path exists, 0 otherwise.
char file_exists(const char *path);

/// Returns a heap-allocated buffer containing the
/// contents of the file found at the given path.
Error file_contents(const char *path, char **result);

/** Get the current working directory.
 *
 * OS specific implementation.
 * WINDOWS: https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/getcwd-wgetcwd?view=msvc-170
 * LINUX:   `man getcwd.3`
 *
 * @return heap-allocated null-terminated string containing current
 * working directory, or NULL.
 */
char *get_working_dir();

/** Get the absolute path of a filepath as a heap allocated string.
 *
 * OS specific implementation.
 * WINDOWS: https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/fullpath-wfullpath?view=msvc-170
 * LINUX:   `man realpath.3`
 *
 * @return heap-allocated null-terminated string containing absolute path, or NULL.
 */
char *getfullpath(const char *path);

/** Get the absolute path of a filepath as a heap allocated string.
 *
 * OS specific environment variables.
 * WINDOWS: $APPDATA/lite
 * LINUX:   $HOME/lite
 *
 * @return heap-allocated null-terminated string containing absolute
 * path to lite directory, or NULL.
 */
char *getlitedir(void);

Error evaluate_file(Atom environment, const char* path, Atom *result);

#endif /* LITE_FILE_IO_H */
