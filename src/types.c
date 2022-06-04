#include <types.h>

#include <error.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Atom symbol_table = { ATOM_TYPE_NIL, 0, NULL };
Atom *sym_table() { return &symbol_table; }

//================================================================ BEG garbage_collection

ConsAllocation *global_pair_allocations = NULL;
size_t pair_allocations_count = 0;
size_t pair_allocations_freed = 0;

GenericAllocation *generic_allocations = NULL;
size_t generic_allocations_count = 0;
size_t generic_allocations_freed = 0;

Error gcol_generic_allocation(Atom *ref, void *payload) {
  if (!ref) {
    MAKE_ERROR(err, ERROR_ARGUMENTS, nil,
               "GALLOC: Can not allocate when NULL referring Atom is passed."
               , NULL);
      return err;
  }
  if (!payload) {
    MAKE_ERROR(err, ERROR_ARGUMENTS, *ref
               , "GALLOC: Can not allocate NULL payload."
               , NULL);
    return err;
  }
  GenericAllocation *galloc = malloc(sizeof(GenericAllocation));
  if (!galloc) {
    MAKE_ERROR(err, ERROR_MEMORY, *ref
               , "GALLOC: Could not allocate memory for generic allocation."
               , NULL);
    return err;
  }
  galloc->mark = 0;
  galloc->payload = payload;
  galloc->next = generic_allocations;
  generic_allocations = galloc;
  generic_allocations_count += 1;
  galloc->more = NULL;
  if (ref->galloc) {
    galloc->more = ref->galloc;
  }
  ref->galloc = galloc;
  return ok;
}

void gcol_mark(Atom root) {
  if (nilp(root)) {
    return;
  }
  if (root.galloc) {
    root.galloc->mark = 1;
  }
  // Any type made with `cons()` belongs here.
  if (pairp(root) || closurep(root) || macrop(root)) {
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
  }
}

void gcol_cons() {
  // Sweep cons allocations (pairs).
  ConsAllocation **pair_allocations_it = &global_pair_allocations;
  ConsAllocation *prev_pair_allocation = NULL;
  ConsAllocation *pair_allocation;
  while ((pair_allocation = *pair_allocations_it) != NULL) {
    if (!pair_allocation->mark) {
      *pair_allocations_it = pair_allocation->next;
      if (prev_pair_allocation) {
        prev_pair_allocation->next = pair_allocation->next;
      } else {
        global_pair_allocations = pair_allocation->next;
      }
      free(pair_allocation);
      pair_allocation = NULL;
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
}

void gcol_generic() {
  GenericAllocation **galloc_it = &generic_allocations;
  GenericAllocation *prev_galloc = NULL;
  GenericAllocation *galloc = generic_allocations;
  while ((galloc = *galloc_it) != NULL) {
    if (!galloc->mark) {
      *galloc_it = galloc->next;
      if (prev_galloc) {
        prev_galloc->next = galloc->next;
      } else {
        generic_allocations = galloc->next;
      }
      free(galloc->payload);
      galloc->payload = NULL;
      free(galloc);
      galloc = NULL;
      generic_allocations_freed += 1;
    } else {
      galloc_it = &galloc->next;
      prev_galloc = galloc;
    }
  }
  // Clear mark.
  galloc = generic_allocations;
  while (galloc != NULL) {
    galloc->mark = 0;
    galloc = galloc->next;
  }
}

void gcol() {
  gcol_generic();
  gcol_cons();
}

void print_gcol_data() {
  printf("Cons Allocations Count:    %zu\n"
         "Cons Allocations Freed:    %zu\n"
         "Generic Allocations Count: %zu\n"
         "Generic Allocations Freed: %zu\n"
         , pair_allocations_count
         , pair_allocations_freed
         , generic_allocations_count
         , generic_allocations_freed
         );
}

//================================================================ END garbage_collection

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

  Atom newpair = nil;
  newpair.type = ATOM_TYPE_PAIR;
  newpair.value.pair = &alloc->pair;
  car(newpair) = car_atom;
  cdr(newpair) = cdr_atom;
  return newpair;
}

Atom nil_with_docstring(symbol_t *docstring) {
  Atom doc = nil;
  doc.docstring = docstring;
  return doc;
}

Atom make_int(integer_t value) {
  Atom a = nil;
  a.type = ATOM_TYPE_INTEGER;
  a.value.integer = value;
  return a;
}

Atom make_int_with_docstring(integer_t value, symbol_t *docstring) {
  Atom a = nil;
  a.type = ATOM_TYPE_INTEGER;
  a.value.integer = value;
  a.docstring = docstring;
  return a;
}

Atom make_sym(symbol_t *value) {
  Atom a = nil;
  // TODO: uppercase value symbol in search.
  // Attempt to find existing symbol in symbol table.
  Atom symbol_table_it = symbol_table;
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
  // Register allocated symbol in garbage collector.
  gcol_generic_allocation(&a, (void *)a.value.symbol);
  // Add new symbol to symbol table.
  symbol_table = cons(a, symbol_table);
  return a;
}

Atom make_string(symbol_t *contents) {
  Atom string = nil;
  string.type = ATOM_TYPE_STRING;
  string.value.symbol = strdup(contents);
  if (!string.value.symbol) {
    printf("Could not allocate memory for new string.\n");
    return nil;
  }
  // Register allocated string in garbage collector.
  gcol_generic_allocation(&string, (void *)string.value.symbol);
  return string;
}

Atom make_builtin(BuiltIn function, symbol_t *docstring) {
  Atom builtin = nil;
  builtin.type = ATOM_TYPE_BUILTIN;
  builtin.value.builtin = function;
  builtin.docstring = docstring;
  return builtin;
}

Error make_closure(Atom environment, Atom arguments, Atom body, Atom *result) {
  Error err;
  if (!listp(body)) {
    PREP_ERROR(err, ERROR_SYNTAX
               , body
               , "Body of a closure must be a list!"
               , NULL);
    return err;
  }
  // Ensure all arguments are valid symbols.
  Atom arguments_it = arguments;
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
  if (nilp(list)) {
    return nil;
  }
  Atom newlist = cons(car(list), nil);
  Atom it = newlist;
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

int format_bufsz(const char *format, ...) {
  va_list args;
  va_start(args, format);
  int result = vsnprintf(NULL, 0, format, args);
  va_end(args);
  return result + 1;
}

char *atom_string(Atom atom, char *buffer) {
  char *left;
  char *right;
  size_t leftlen;
  size_t rightlen;
  size_t length = buffer ? strlen(buffer) : 0;
  size_t to_add = 0;
  switch (atom.type) {
  case ATOM_TYPE_NIL:
    to_add = 4;
    buffer = realloc(buffer, length+to_add);
    if (!buffer) { return NULL; }
    memmove(buffer+length, "NIL", to_add);
    break;
  case ATOM_TYPE_SYMBOL:
    to_add = strlen(atom.value.symbol);
    buffer = realloc(buffer, length+to_add);
    if (!buffer) { return NULL; }
    snprintf(buffer+length, to_add, "%s", atom.value.symbol);
    break;
  case ATOM_TYPE_STRING:
    to_add = format_bufsz("\"%s\"", atom.value.symbol);
    buffer = realloc(buffer, length+to_add);
    if (!buffer) { return NULL; }
    snprintf(buffer+length, to_add, "\"%s\"", atom.value.symbol);
    break;
  case ATOM_TYPE_INTEGER:
    // FIXME: Format only works when interger_t is long long int
    to_add = format_bufsz("%lli", atom.value.integer);
    buffer = realloc(buffer, length+to_add);
    if (!buffer) { return NULL; }
    snprintf(buffer+length, to_add, "%lli", atom.value.integer);
    break;
  case ATOM_TYPE_PAIR:
    // TODO: Handle lists better.
    // Currently, there's a lot of parens.
    left = atom_string(car(atom), NULL);
    leftlen = left == NULL ? 0 : strlen(left);
    right = atom_string(cdr(atom), NULL);
    rightlen = strlen(right);
    // Finally, allocate new buffer of left+right length,
    // and copy from left+right buffer into new buffer.
    to_add = leftlen + rightlen + 3;
    buffer = realloc(buffer, length+to_add);
    if (!buffer) { return NULL; }
    buffer[length] = '(';
    memcpy(buffer+length+1, left, leftlen);
    buffer[length+1+leftlen] = ' ';
    memcpy(buffer+length+1+leftlen+1, right, rightlen);
    buffer[length+to_add-1] = ')';
    buffer[length+to_add] = '\0';
    break;
  case ATOM_TYPE_BUILTIN:
    to_add = format_bufsz("#<BUILTIN>:%p", atom.value.builtin);
    buffer = realloc(buffer, length+to_add);
    if (!buffer) { return NULL; }
    snprintf(buffer+length, to_add, "#<BUILTIN>:%p", atom.value.builtin);
    break;
  case ATOM_TYPE_CLOSURE:
    to_add = format_bufsz("#<CLOSURE>:%p", atom.value.builtin);
    buffer = realloc(buffer, length+to_add);
    if (!buffer) { return NULL; }
    snprintf(buffer+length, to_add, "#<CLOSURE>:%p", atom.value.builtin);
    break;
  case ATOM_TYPE_MACRO:
    to_add = format_bufsz("#<MACRO>:%p", atom.value.builtin);
    buffer = realloc(buffer, length+to_add);
    if (!buffer) { return NULL; }
    snprintf(buffer+length, to_add, "#<MACRO>:%p", atom.value.builtin);
    break;
  }
  return buffer;
}
