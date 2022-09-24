#include <error.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <types.h>


Error ok = { ERROR_NONE, NULL, NULL, { ATOM_TYPE_NIL, { 0 }, NULL, NULL} };

void print_error(Error e) {
  if (e.type == ERROR_NONE) {
    return;
  }
  printf("ERROR: ");
  switch (e.type) {
  case ERROR_TODO:
    puts("TODO: not yet implemented.");
    break;
  case ERROR_SYNTAX:
    puts("Syntax error.");
    break;
  case ERROR_TYPE:
    puts("Type error.");
    break;
  case ERROR_ARGUMENTS:
    puts("Invalid arguments.");
    break;
  case ERROR_NOT_BOUND:
    puts("Symbol not bound.");
    break;
  case ERROR_MEMORY:
    puts("Could not allocate memory.");
    break;
  case ERROR_FILE:
    puts("File error.");
    break;
  case ERROR_GENERIC:
    puts("Generic error.");
    break;
  default:
    puts("Unrecognized error.");
    break;
  }
  if (!nilp(e.ref)) {
    printf("> ");
    print_atom(e.ref);
    putchar('\n');
  }
  if (e.message) {
    printf(": %s\n", e.message);
  }
  if (e.suggestion) {
    printf(":: %s\n", e.suggestion);
  }
  putchar('\n');
}

#define ERR_STR_BUFFER_SZ 1024
static char error_string_buffer[ERR_STR_BUFFER_SZ];
static size_t error_string_point = 0;
static void reset_error_string() {
  error_string_point = 0;
  error_string_buffer[0] = '\0';
}
static void append_error_string(char* str) {
  size_t len = strlen(str);
  if (error_string_point + len + 1 >= ERR_STR_BUFFER_SZ) {
    return;
  }
  memcpy(error_string_buffer + error_string_point, str, len);
  error_string_point += len;
  error_string_buffer[error_string_point + 1] = '\0';
}
char *error_string(Error e) {
  if (e.type == ERROR_NONE) {
    return NULL;
  }
  reset_error_string();
  switch (e.type) {
  case ERROR_TODO:
    append_error_string("TODO: not yet implemented.\n");
    break;
  case ERROR_SYNTAX:
    append_error_string("Syntax error.\n");
    break;
  case ERROR_TYPE:
    append_error_string("Type error.\n");
    break;
  case ERROR_ARGUMENTS:
    append_error_string("Invalid arguments\n");
    break;
  case ERROR_NOT_BOUND:
    append_error_string("Symbol not bound.\n");
    break;
  case ERROR_MEMORY:
    append_error_string("Allocation failure\n");
    break;
  case ERROR_FILE:
    append_error_string("File error.\n");
    break;
  case ERROR_GENERIC:
    append_error_string("Generic error.\n");
    break;
  default:
    append_error_string("Unrecognized error. Please report as issue on GitHub.\n");
    break;
  }
  if (!nilp(e.ref)) {
    char *refer_string = atom_string(e.ref, NULL);
    if (refer_string) {
      append_error_string("> ");
      append_error_string(refer_string);
      append_error_string("\n");
      free(refer_string);
    }
  }
  if (e.message) {
    append_error_string(": ");
    append_error_string((char *)e.message);
    append_error_string("\n");
  }
  if (e.suggestion) {
    append_error_string(":: ");
    append_error_string((char *)e.suggestion);
    append_error_string("\n");
  }
  return strdup(error_string_buffer);
}
