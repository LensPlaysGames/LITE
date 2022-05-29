#include <error.h>

#include <stdio.h>

void print_error(enum Error e) {
  if (e == ERROR_NONE) {
    return;
  }
  printf("ERROR: ");
  switch (e) {
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
  default:
    puts("Unrecognized error.");
    break;
  }
  putchar('\n');
}
