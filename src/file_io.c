#include <file_io.h>

#include <error.h>
#include <parser.h>
#include <evaluation.h>
#include <environment.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <types.h>
#include <utility.h>

#if defined (__unix__)
// #  include <stdlib.h
#  include <limits.h>
#  include <unistd.h>
#  include <errno.h>
#  include <assert.h>
#endif

#if defined (_WIN32)
#  include <direct.h>
#endif

size_t file_size(FILE *file) {
  if (!file) {
    return 0;
  }
  fseek(file, 0, SEEK_END);
  long length = ftell(file);
  fseek(file, 0, SEEK_SET);
  if (length < 0)
    return 0;
  return (size_t)length;
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
    //printf("%sCouldn't open file at %s\n", error_prefix, path);
    return smpl;
  }

  size_t size = file_size(file);
  if (size == 0) {
    fclose(file);
    free(smpl.path);
    //printf("%sFile has zero size at %s\n", error_prefix, path);
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
  smpl.flags &= SMPL_FILE_FLAG_INVALID;
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
  //printf("Attempting to get contents at path: %s\n", path);
  err = file_contents(path, &input);
  if (err.type) {
    //printf("    Could not get contents at path: %s\n", path);
    return err;
  }

#ifdef LITE_DBG
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

  Atom debug_memory = nil;
  size_t pair_allocations_count_before = pair_allocations_count;
  size_t pair_allocations_freed_before = pair_allocations_freed;
#endif

  const char* source = input;
  Atom expr = nil;
  Atom dummy_result = nil;
  if (!result) {
    result = &dummy_result;
  }
  while (parse_expr(source, &source, &expr).type == ERROR_NONE) {
#   ifdef LITE_DBG
    if (!nilp(debug_eval_file)) {
      printf("Parsed expression: ");
      print_atom(expr);
      putchar('\n');
    }
#   endif

    err = evaluate_expression(expr, environment, result);
    if (err.type) {
      printf("evaluate_file() ERROR: expression == ");
      print_atom(expr);putchar('\n');
      return err;
    }
    if (user_quit) {
      break;
    }

#   ifdef LITE_DBG
    if (!nilp(debug_eval_file)) {
      print_atom(*result);
      putchar('\n');
    }
#   endif
  }

# ifdef LITE_DBG
  env_get(*genv(), make_sym("DEBUG/MEMORY"), &debug_memory);
  if (!nilp(debug_memory)) {
    printf("%s:\n"
           "  allocated: %20zu\n"
           "      freed: %20zu\n",
           path,
           pair_allocations_count - pair_allocations_count_before,
           pair_allocations_freed - pair_allocations_freed_before);
  }
# endif

  free(input);
  return ok;
}

char file_exists(const char *path) {
  FILE *f = fopen(path, "r");
  if (f) {
    fclose(f);
    return 1;
  }
  return 0;
}

char *get_working_dir() {
#if _WIN32
  // Requires direct.h
  char *out = _getcwd(NULL, 0);
  if (!out) {
    perror("get_working_dir() -> _getcwd() ERROR\n");
    return NULL;
  }
  return out;
#elif defined (__unix__)
  // Requires unistd.h
  char *out = getcwd(NULL, PATH_MAX);
  if (!out) {
    fprintf(stderr, "get_working_dir() -> getcwd() returned NULL\n");
    return NULL;
  }
  return out;
#else
#  error "get_working_dir() does not support your platform."
#endif
}

char *getfullpath(const char *path) {
  if (!path) { return NULL; }
  char *out = NULL;
# ifdef _WIN32
  // _fullpath() and _MAX_PATH are in stdlib.h
  // char *_fullpath(char *absPath, const char *relPath, size_t maxLength);
  out = calloc(1, _MAX_PATH);
  if (!out) {
    fprintf(stderr, "getfullpath() -> calloc() ERROR\n");
    return NULL;
  }
  out = _fullpath(out, path, _MAX_PATH);
  if (!out) {
    fprintf(stderr,
            "getfullpath() -> _fullpath() ERROR\n"
            "    Path: %s\n"
            "     out: %s\n",
            path,
            out);
    return NULL;
  }
  //printf("getfullpath() got absolute %s from %s\n", out, path);
  return out;
# elif defined (__unix__)
  // realpath requires stdlib.h and limits.h to be included.
  // char *realpath(const char *path, char *resolved_path);
  out = calloc(1, PATH_MAX + 1);
  if (!out) {
    fprintf(stderr, "getfullpath() -> calloc() ERROR\n");
    return NULL;
  }
  out = realpath(path, out);
  // If file does not exist and that is why Unix realpath fails, create
  // the file and try again.
  if (!out && errno == ENOENT) {
    FILE *f = fopen(path, "w");
    assert(f && "realpath() required file to exist but it was not able to be created.");
    fclose(f);
    out = realpath(path, out);
  }
  if (!out) {
    if (!out) {
      fprintf(stderr,
              "realpath() ERROR: %s\n"
              "    Path: %s\n"
              "     out: %s\n",
              strerror(errno),
              path,
              out);
      return NULL;
    }
  }
  //printf("getfullpath() got absolute %s from %s\n", out, path);
  return out;
# else
#  error "getfullpath() doesn't support your system type, or at least doesn't think it does."
# endif
  return NULL;
}

char *getlitedir(void) {
# ifdef _WIN32
  static const char *app_data = "appdata";
# elif defined (__unix__)
  static const char *app_data = "HOME";
# else
#   error "getlitedir() unsupported platform."
# endif
  char *out = getenv(app_data);
  out = string_join(out, "/lite");
  return out;
}
