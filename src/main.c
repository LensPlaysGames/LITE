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

/// Returns a heap-allocated pair atom with car and cdr set.
Atom cons(Atom car_atom, Atom cdr_atom) {
  Atom pair_atom = { ATOM_TYPE_PAIR };
  pair_atom.value.pair = malloc(sizeof(struct Pair));
  car(pair_atom) = car_atom;
  cdr(pair_atom) = cdr_atom;
  return pair_atom;
}

Atom make_int(integer_t value) {
  Atom a;
  a.type = ATOM_TYPE_INTEGER;
  a.value.integer = value;
  return a;
}

static Atom symbol_table = { ATOM_TYPE_NIL };

Atom make_sym(symbol_t *value) {
  Atom a;
  Atom p;
  // Attempt to find existing symbol in symbol table.
  p = symbol_table;
  while (!nilp(p)) {
    a = car(p);
    if (strcmp(a.value.symbol, value) == 0) {
      return a;
    }
    p = cdr(p);
  }
  // Create a new symbol.
  a.type = ATOM_TYPE_SYMBOL;
  a.value.symbol = value;
  // Add new symbol to symbol table.
  symbol_table = cons(a, symbol_table);
  return a;
}

int main(int argc, char **argv) {
  printf("LITE will guide the way through the darkness.\n");
  (void)argc;
  (void)argv;
  Atom test = cons(make_sym("My first symbol"), cons(make_int(4), make_int(20)));
  print_atom(test);
  putchar('\n');
  enter_repl();
  return 0;
}
