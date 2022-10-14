#include <file_io.h>

#include <error.h>
#include <parser.h>
#include <evaluation.h>
#include <environment.h>
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
Error file_contents(const char* path, char **result) {
  if (!path) {
    MAKE_ERROR(err, ERROR_ARGUMENTS, nil,
               "file_contents(): PATH must not be NULL.",
               NULL);
    return err;
  }
  char *buffer = NULL;
  FILE *file = fopen(path, "rb");
  if (!file) {
    MAKE_ERROR(err, ERROR_FILE, nil,
               "file_contents(): Failed to open file: fopen() returned NULL.",
               NULL);
    return err;
  }
  size_t size = file_size(file);
  if (size == 0) {
    MAKE_ERROR(err, ERROR_FILE, nil,
               "file_contents(): File has zero size.",
               NULL);
    return err;
  }
  buffer = malloc(size + 1);
  if (!buffer) {
    MAKE_ERROR(err, ERROR_MEMORY, nil,
               "file_contents(): Could not allocate buffer for file.",
               NULL);
    return err;
  }
  fread(buffer, 1, size, file);
  buffer[size] = '\0';
  fclose(file);
  *result = buffer;
  return ok;
}

SimpleFile get_file(char *path) {
  const char* error_prefix = "get_file(): ";
  SimpleFile smpl;
  smpl.path = NULL;
  smpl.contents = NULL;
  smpl.flags = SMPL_FILE_FLAG_INVALID;
  smpl.size = 0;
  if (!path) {
    printf("%sPath must not be NULL\n", error_prefix);
    return smpl;
  }
  smpl.path = strdup(path);

  FILE *file = fopen(path, "rb");
  if (!file) {
    free(smpl.path);
    printf("%sCouldn't open file at %s\n", error_prefix, path);
    return smpl;
  }

  size_t size = file_size(file);
  if (size == 0) {
    fclose(file);
    free(smpl.path);
    printf("%sFile has zero size at %s\n", error_prefix, path);
    return smpl;
  }
  smpl.size = size;

  uint8_t *buffer = NULL;
  buffer = malloc(size + 1);
  if (!buffer) {
    fclose(file);
    free(smpl.path);
    printf("%sCould not allocate buffer for file at %s\n",
           error_prefix, path);
    return smpl;
  }
  memset(buffer, 0, size);

  if (fread(buffer, 1, size, file) != size) {
    fclose(file);
    free(smpl.path);
    printf("%sCould not read %zu bytes from file at \"%s\"\n",
           error_prefix, size, path);
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

Error evaluate_file(Atom environment, const char *path, Atom *result) {
  Error err;
  if (!path) {
    PREP_ERROR(err, ERROR_ARGUMENTS, nil
               , "Path must not be NULL."
               , NULL);
    return err;
  }
  char *input = NULL;
  err = file_contents(path, &input);
  if (err.type) { return err; }

  Atom debug_eval_file = nil;
  env_get(*genv(), make_sym("DEBUG/EVALUATE-FILE"), &debug_eval_file);
  if (!nilp(debug_eval_file)) {
    printf("Evaluating file at \"%s\"\n", path);
    //printf("Contents:\n"
    //       "-----\n"
    //       "\"%s\"\n"
    //       "-----\n",
    //       input);
  }

  const char* source = input;
  Atom expr = nil;
  Atom dummy_result = nil;
  if (!result) {
    result = &dummy_result;
  }
  while (parse_expr(source, &source, &expr).type == ERROR_NONE) {
    if (!nilp(debug_eval_file)) {
      printf("Parsed expression: ");
      print_atom(expr);
      putchar('\n');
    }
    err = evaluate_expression(expr, environment, result);
    if (err.type) { return err; }

    if (user_quit) {
      break;
    }

    if (!nilp(debug_eval_file)) {
      print_atom(*result);
      putchar('\n');
    }
  }
  free(input);
  return ok;
}
