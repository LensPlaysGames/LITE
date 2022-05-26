#include <types.h>

#include <ctype.h>
#include <error.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Atom symbol_table = { ATOM_TYPE_NIL };

Atom cons(Atom car_atom, Atom cdr_atom) {
  Atom newpair;
  newpair.type = ATOM_TYPE_PAIR;
  newpair.value.pair = malloc(sizeof(struct Pair));
  car(newpair) = car_atom;
  cdr(newpair) = cdr_atom;
  return newpair;
}

Atom make_int(integer_t value) {
  Atom a;
  a.type = ATOM_TYPE_INTEGER;
  a.value.integer = value;
  return a;
}

Atom make_sym(symbol_t *value) {
  Atom a;
  Atom symbol_table_it;
  // TODO: uppercase symbol.
  // Attempt to find existing symbol in symbol table.
  symbol_table_it = symbol_table;
  while (!nilp(symbol_table_it)) {
    a = car(symbol_table_it);
    if (strcmp(a.value.symbol, value) == 0) {
      return a;
    }
    symbol_table_it = cdr(symbol_table_it);
  }
  // Create a new symbol.
  a.type = ATOM_TYPE_SYMBOL;
  a.value.symbol = strdup(value);
  // Add new symbol to symbol table.
  symbol_table = cons(a, symbol_table);
  return a;
}

Atom make_builtin(BuiltIn function) {
  Atom a;
  a.type = ATOM_TYPE_BUILTIN;
  a.value.builtin = function;
  return a;
}

int listp(Atom expr) {
  while (!nilp(expr)) {
    if (expr.type != ATOM_TYPE_PAIR) {
      return 0;
    }
    expr = cdr(expr);
  }
  return 1;
}

Atom copy_list(Atom list) {
  Atom newlist;
  Atom it;
  if (nilp(list)) {
    return nil;
  }
  newlist = cons(car(list), nil);
  it = newlist;
  list = cdr(list);
  while (!nilp(list)) {
    cdr(it) = cons(car(list), nil);
    it = cdr(it);
    list = cdr(list);
  }
  return newlist;
}

int apply(Atom function, Atom arguments, Atom *result) {
  if (function.type == ATOM_TYPE_BUILTIN) {
    return (*function.value.builtin)(arguments, result);
  }
  return ERROR_TYPE;
}

void print_atom(Atom atom) {
  switch (atom.type) {
  case ATOM_TYPE_NIL:
    printf("NIL");
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
  case ATOM_TYPE_BUILTIN:
    printf("#<BUILTIN>:%p", atom.value.builtin);
    break;
  }
}
