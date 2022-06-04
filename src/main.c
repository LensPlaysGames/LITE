#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <environment.h>
#include <evaluation.h>
#include <error.h>
#include <parser.h>
#include <repl.h>
#include <types.h>

//================================================================ BEG file_io
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
  buffer[size] = 0;
  fclose(file);
  return buffer;
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

//================================================================ END file_io

int main(int argc, char **argv) {
  printf("LITE will guide the way through the darkness.\n");
  Atom environment = default_environment();
  // Treat every given argument as a file to load, for now.
  if (argc > 1) {
    for (size_t i = 1; i < argc; ++i) {
      load_file(environment, argv[i]);
    }
  }
  enter_repl(environment);
  int debug_memory = env_non_nil(environment, make_sym("DEBUG/MEMORY"));
  free_symbol_table();
  // Test generic allocations.
  Atom test0;
  Atom test1;
  Atom test2;
  gcol_generic_allocation(&test0, malloc(8));
  gcol_generic_allocation(&test1, malloc(8));
  gcol_generic_allocation(&test2, malloc(8));
  gcol_generic_allocation(&test2, malloc(8));
  printf("test0: %p payload=%p\n"
         "test1: %p payload=%p\n"
         "test2: %p payload=%p extra_payload=%p\n"
         , test0.galloc, test0.galloc->payload
         , test1.galloc, test1.galloc->payload
         , test2.galloc, test2.galloc->payload, test2.galloc->more->payload
         );
  // Garbage collection with no marking means free everything.
  gcol();
  if (debug_memory) {
    print_gcol_data();
  }
  return 0;
}
