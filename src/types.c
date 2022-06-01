#include <types.h>

#include <error.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Atom symbol_table = { ATOM_TYPE_NIL };
Atom *sym_table() { return &symbol_table; }

Atom cons(Atom car_atom, Atom cdr_atom) {
  Atom newpair;
  newpair.type = ATOM_TYPE_PAIR;
  // TODO: These are never freed, as there is no garbage collection.
  newpair.value.pair = malloc(sizeof(struct Pair));
  car(newpair) = car_atom;
  cdr(newpair) = cdr_atom;
  newpair.docstring = NULL;
  return newpair;
}

Atom make_int(integer_t value) {
  Atom a;
  a.type = ATOM_TYPE_INTEGER;
  a.value.integer = value;
  a.docstring = NULL;
  return a;
}

Atom make_sym(symbol_t *value) {
  Atom a;
  Atom symbol_table_it;
  // TODO: uppercase value symbol in search.
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
  a.docstring = NULL;
  return a;
}

Atom make_string(symbol_t *contents) {
  Atom string;
  string.type = ATOM_TYPE_STRING;
  string.value.symbol = contents;
  string.docstring = NULL;
  return string;
}

Atom make_builtin(BuiltIn function, symbol_t *docstring) {
  Atom builtin;
  builtin.type = ATOM_TYPE_BUILTIN;
  builtin.value.builtin = function;
  builtin.docstring = docstring;
  return builtin;
}

int make_closure(Atom environment, Atom arguments, Atom body, Atom *result) {
  Atom arguments_it;
  if (!listp(body)) {
    return ERROR_SYNTAX;
  }
  // Ensure all arguments are valid symbols.
  arguments_it = arguments;
  while (!nilp(arguments_it)) {
    // Handle variadic arguments.
    if (arguments_it.type == ATOM_TYPE_SYMBOL) {
      break;
    } else if (arguments_it.type != ATOM_TYPE_PAIR
               || car(arguments_it).type != ATOM_TYPE_SYMBOL)
      {
        return ERROR_TYPE;
      }
    arguments_it = cdr(arguments_it);
  }
  Atom closure = cons(environment, cons(arguments, body));
  closure.type = ATOM_TYPE_CLOSURE;
  *result = closure;
  return ERROR_NONE;
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

void print_atom(Atom atom) {
  switch (atom.type) {
  default:
    printf("#<UNKNOWN>");
    break;
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
  case ATOM_TYPE_CLOSURE:
    printf("#<CLOSURE>:%p", atom.value.builtin);
    break;
  case ATOM_TYPE_MACRO:
    printf("#<MACRO>:%p", atom.value.builtin);
    break;
  case ATOM_TYPE_STRING:
    printf("\"%s\"", atom.value.symbol);
    break;
  }
}

void pretty_print_atom(Atom atom) {
  switch (atom.type) {
  case ATOM_TYPE_PAIR:
    putchar('(');
    print_atom(car(atom));
    atom = cdr(atom);
    while (!nilp(atom)) {
      if (atom.type == ATOM_TYPE_PAIR) {
        printf("\n ");
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
  default:
    print_atom(atom);
    break;
  }
}
