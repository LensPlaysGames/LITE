#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <repl.h>
#include <types.h>

void print_atom(Atom atom) {
  switch (atom.type) {
  case ATOM_TYPE_NIL:
    printf("nil");
    break;
  case ATOM_TYPE_PAIR:
    putchar('(');
    print_atom(car(atom));
    atom = cdr(atom);
    while (!nilp(atom)) {
      if (atom.type == ATOM_TYPE_PAIR) {
        putchar(' ');
        print_atom(car(atom));
        atom = cdr(atom);
      } else {
        printf(" . ");
        print_atom(atom);
        break;
      }
    }
    putchar(')');
    break;
  case ATOM_TYPE_SYMBOL:
    printf("%s", atom.value.symbol);
    break;
  case ATOM_TYPE_INTEGER:
    printf("%lli", atom.value.integer);
    break;
  }
}

}

int main(int argc, char **argv) {
  printf("LITE will guide the way through the darkness.\n");
  (void)argc;
  (void)argv;
  enter_repl();
  return 0;
}
