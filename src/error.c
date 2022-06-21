#include <error.h>

#include <stddef.h>
#include <stdio.h>
#include <types.h>


Error ok = { ERROR_NONE, NULL, NULL, { ATOM_TYPE_NIL, 0, NULL, NULL} };

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
