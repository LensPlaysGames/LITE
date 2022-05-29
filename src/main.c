#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <error.h>
#include <repl.h>

int main(int argc, char **argv) {
  printf("LITE will guide the way through the darkness.\n");
  (void)argc;
  (void)argv;
  enter_repl();
  return 0;
}
