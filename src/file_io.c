#include <file_io.h>

#include <error.h>
#include <parser.h>
#include <evaluation.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <types.h>

size_t file_size(FILE *file) {
  if (!file) {
    return 0;
  }
  fseek(file, 0, SEEK_END);
  size_t length = ftell(file);
  fseek(file, 0, SEEK_SET);
  return length;
}

/// Returns a heap-allocated buffer containing the
/// contents of the file found at the given path.
char *file_contents(const char* path) {
  char *buffer = NULL;
  FILE *file = fopen(path, "r");
  if (!file) {
    printf("Couldn't open file at %s\n", path);
    return NULL;
  }
  size_t size = file_size(file);
  if (size == 0) {
    printf("File has zero size at %s\n", path);
    return NULL;
  }
  buffer = malloc(size + 1);
  if (!buffer) {
    printf("Could not allocate buffer for file at %s\n", path);
    return NULL;
  }
  fread(buffer, 1, size, file);
  buffer[size] = '\0';
  fclose(file);
  return buffer;
}

const SimpleFile get_file(char* path) {
  SimpleFile smpl;
  smpl.path = NULL;
  smpl.contents = NULL;
  smpl.flags = SMPL_FILE_FLAG_INVALID;
  smpl.size = 0;
  if (!path) {
    return smpl;
  }
  smpl.path = strdup(path);

  FILE *file = fopen(path, "r");
  if (!file) {
    fclose(file);
    printf("get_file(): Couldn't open file at %s\n", path);
    return smpl;
  }

  size_t size = file_size(file);
  if (size == 0) {
    fclose(file);
    printf("get_file(): File has zero size at %s\n", path);
    return smpl;
  }
  smpl.size = size;

  uint8_t *buffer = NULL;
  buffer = malloc(size);
  if (!buffer) {
    fclose(file);
    printf("get_file(): Could not allocate buffer for file at %s\n", path);
    return smpl;
  }
  memset(buffer, 0, size);

  if (fread(buffer, 1, size, file) != size) {
    printf("SimpleFile: Could not read %zu bytes from file at \"%s\"\n"
           , size, path);
    fclose(file);
    return smpl;
  }

  smpl.contents = buffer;
  fclose(file);

  smpl.flags &= ~SMPL_FILE_FLAG_INVALID;
  smpl.flags |= SMPL_FILE_FLAG_OK;
  return smpl;
}

void free_file(SimpleFile file) {
  if (!(file.flags & SMPL_FILE_FLAG_OK) || file.size == 0) {
    return;
  }
  if (file.path) {
    free(file.path);
  }
  if (file.contents) {
    free(file.contents);
  }
}

Error load_file(Atom environment, const char* path) {
  char *input = file_contents(path);
  Error err;
  if (!input) {
    PREP_ERROR(err, ERROR_ARGUMENTS, nil
               , "Could not get contents of file."
               , path)
    return err;
  }
  const char* source = input;
  Atom expr;
  while (parse_expr(source, &source, &expr).type == ERROR_NONE) {
    Atom result;
    err = evaluate_expression(expr, environment, &result);
    if (err.type) { return err; }
    print_atom(result);
    putchar('\n');
  }
  free(input);
  return ok;
}
