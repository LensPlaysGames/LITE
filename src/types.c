#include <types.h>

#include <error.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Atom symbol_table = { ATOM_TYPE_NIL };
Atom *sym_table() { return &symbol_table; }

ConsAllocation *global_pair_allocations = NULL;
size_t pair_allocations_count = 0;
size_t pair_allocations_freed = 0;

GenericAllocation *generic_allocations = NULL;
size_t generic_allocations_count = 0;
size_t generic_allocations_freed = 0;

int gcol_generic_allocation(Atom *ref, void *payload) {
  if (!ref || ref->type == ATOM_TYPE_NIL) {
    return ERROR_ARGUMENTS;
  }
  if (!payload) {
    return ERROR_MEMORY;
  }
  GenericAllocation *alloc = malloc(sizeof(GenericAllocation));
  if (!alloc) {
    printf("GARBAGE COLLECTION: Could not allocate memory for new generic allocation!");
    return ERROR_MEMORY;
  }
  ref->allocation = alloc;
  alloc->mark = 0;
  alloc->payload = payload;
  alloc->next = generic_allocations;
  generic_allocations = alloc;
  generic_allocations_count += 1;
  return ERROR_NONE;
}

void gcol_mark(Atom root) {
  if (nilp(root)) {
    return;
  }
  if (root.type == ATOM_TYPE_STRING) {
    GenericAllocation *galloc = (GenericAllocation *)root.allocation;
    if (galloc->mark) {
      return;
    }
    galloc->mark = 1;
    return;
  } else if (root.type == ATOM_TYPE_PAIR
             || root.type == ATOM_TYPE_CLOSURE
             || root.type == ATOM_TYPE_MACRO)
    {
      ConsAllocation *alloc = (ConsAllocation *)
        ((char *)root.value.pair - offsetof(ConsAllocation, pair));
      if (!alloc || alloc->mark) {
        return;
      }
      alloc->mark = 1;
      gcol_mark(car(root));
      if (!nilp(cdr(root))) {
        gcol_mark(cdr(root));
      }
      return;
    }
}

void gcol() {
  // Sweep cons allocations (pairs).
  ConsAllocation **pair_allocations_it = &global_pair_allocations;
  ConsAllocation *prev_pair_allocation = NULL;
  ConsAllocation *pair_allocation;
  while ((pair_allocation = *pair_allocations_it) != NULL) {
    if (!pair_allocation->mark) {
      *pair_allocations_it = pair_allocation->next;
      if (prev_pair_allocation) {
        prev_pair_allocation->next = pair_allocation->next;
      }
      free(pair_allocation);
      pair_allocations_freed += 1;
    } else {
      pair_allocations_it = &pair_allocation->next;
      prev_pair_allocation = pair_allocation;
    }
  }
  // Clear mark.
  pair_allocation = global_pair_allocations;
  while (pair_allocation != NULL) {
    pair_allocation->mark = 0;
    pair_allocation = pair_allocation->next;
  }
  // Sweep generic allocations (strings).
  GenericAllocation **generic_allocations_it = &generic_allocations;
  GenericAllocation *generic_allocation;
  while (*generic_allocations_it != NULL) {
    generic_allocation = *generic_allocations_it;
    if (!generic_allocation->mark) {
      *generic_allocations_it = generic_allocation->next;
      free(generic_allocation->payload);
      free(generic_allocation);
      generic_allocations_freed += 1;
    } else {
      generic_allocations_it = &generic_allocation->next;
    }
  }
  // Clear mark.
  generic_allocation = generic_allocations;
  while (generic_allocation != NULL) {
    generic_allocation->mark = 0;
    generic_allocation = generic_allocation->next;
  }
}

Atom cons(Atom car_atom, Atom cdr_atom) {
  ConsAllocation *alloc;
  alloc = malloc(sizeof(ConsAllocation));
  if (!alloc) {
    printf("CONS: Could not allocate memory for new allocation!");
    return nil;
  }
  alloc->mark = 0;
  alloc->next = global_pair_allocations;
  global_pair_allocations = alloc;
  pair_allocations_count += 1;

  Atom newpair;
  newpair.type = ATOM_TYPE_PAIR;
  newpair.value.pair = &alloc->pair;
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
  if (!a.value.symbol) {
    printf("Could not allocate memory for new symbol.\n");
    return nil;
  }
  // Add new symbol to symbol table.
  symbol_table = cons(a, symbol_table);
  a.docstring = NULL;
  return a;
}

void free_symbol_table() {
  if (!nilp(symbol_table)) {
    Atom symbol_table_it = symbol_table;
    while (!nilp(symbol_table_it)) {
      free((void *)car(symbol_table_it).value.symbol);
      symbol_table_it = cdr(symbol_table_it);
    }
  }
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

Error make_closure(Atom environment, Atom arguments, Atom body, Atom *result) {
  Error err;
  Atom arguments_it;
  if (!listp(body)) {
    PREP_ERROR(err, ERROR_SYNTAX
               , body
               , "Body of a closure must be a list!"
               , NULL);
    return err;
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
        PREP_ERROR(err, ERROR_TYPE
                   , arguments_it
                   , "Closure argument definition must be either a pair or a symbol."
                   , "Usage: (LAMBDA <argument> <body>...)"
                   );
        return err;
      }
    arguments_it = cdr(arguments_it);
  }
  Atom closure = cons(environment, cons(arguments, body));
  closure.type = ATOM_TYPE_CLOSURE;
  *result = closure;
  return ok;
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

Atom list_get(Atom list, int k) {
  while (k--) {
    list = cdr(list);
  }
  return car(list);
}

void list_set(Atom list, int k, Atom value) {
  while (k--) {
    list = cdr(list);
  }
  car(list) = value;
}

void list_reverse(Atom *list) {
  Atom tail = nil;
  while (!nilp(*list)) {
    Atom p = cdr(*list);
    cdr(*list) = tail;
    tail = *list;
    *list = p;
  }
  *list = tail;
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
    printf("#<UNKNOWN>:%d", atom.type);
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
        pretty_print_atom(car(atom));
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
