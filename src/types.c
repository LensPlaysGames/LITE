#include <types.h>

#include <buffer.h>
#include <builtins.h>
#include <error.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Atom symbol_table = { ATOM_TYPE_NIL, { 0 }, NULL, NULL };
Atom *sym_table() { return &symbol_table; }

static Atom buffer_table = { ATOM_TYPE_NIL, { 0 }, NULL, NULL };
Atom *buf_table() { return &buffer_table; }

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

void gcol_mark(Atom *root) {
  if (!root || nilp(*root)) {
    return;
  }
  if (root->galloc) {
    root->galloc->mark = 1;
  }
  // Any type made with `cons()` belongs here.
  if (pairp(*root) || closurep(*root) || macrop(*root)) {
    ConsAllocation *alloc = (ConsAllocation *)
      ((char *)root->value.pair - offsetof(ConsAllocation, pair));
    if (!alloc || alloc->mark) {
      return;
    }
    alloc->mark = 1;
    gcol_mark(&car(*root));
    if (!nilp(cdr(*root))) {
      gcol_mark(&cdr(*root));
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
  int pair_allocations_in_use =
    pair_allocations_count - pair_allocations_freed;
  if (pair_allocations_in_use < 0) {
    printf("ERROR: More pair allocations freed than allocated.\n");
  }
  int generic_allocations_in_use =
    generic_allocations_count - generic_allocations_freed;
  if (generic_allocations_in_use < 0) {
    printf("ERROR: More generic allocations freed than allocated.\n");
  }
  printf("Pair Allocations Total:     %zu\n"
         "Pair Allocations Freed:     %zu\n"
         "Pair Allocations In Use:    %i (%zu bytes)\n"
         "Generic Allocations Total:  %zu\n"
         "Generic Allocations Freed:  %zu\n"
         "Generic Allocations In Use: %i (%zu bytes)\n"
         , pair_allocations_count
         , pair_allocations_freed
         , pair_allocations_in_use
         , pair_allocations_in_use * sizeof(ConsAllocation)
         , generic_allocations_count
         , generic_allocations_freed
         , generic_allocations_in_use
         , generic_allocations_in_use * sizeof(GenericAllocation)
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

Atom make_buffer(Atom environment, char *path) {
  if (!path) {
    MAKE_ERROR(args, ERROR_ARGUMENTS, nil
               , "make_buffer: PATH must not be NULL."
               , NULL);
    print_error(args);
    return nil;
  }
  // Attempt to find existing buffer in buffer table.
  Atom buffer_table_it = buffer_table;
  while (!nilp(buffer_table_it)) {
    Atom a = car(buffer_table_it);
    if (strcmp(a.value.buffer->path, path) == 0) {
      return a;
    }
    buffer_table_it = cdr(buffer_table_it);
  }
  // Create new buffer and add it to buffer table.
  Buffer *buffer = buffer_create(path);
  if (!buffer) {
    MAKE_ERROR(err, ERROR_MEMORY, nil
               , "make_buffer: `buffer_create(path)` failed!."
               , NULL);
    print_error(err);
    return nil;
  }
  buffer->environment = environment;
  Atom result = nil;
  result.type = ATOM_TYPE_BUFFER;
  result.value.buffer = buffer;
  buffer_table = cons(result, buffer_table);
  return result;
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

Atom list_get_past(Atom list, int k) {
  while (k--) {
    list = cdr(list);
  }
  return list;
}

void list_set(Atom list, int k, Atom value) {
  while (k--) {
    list = cdr(list);
  }
  car(list) = value;
}

void list_push(Atom *list, Atom value) {
  if (!list) {
    return;
  }
  *list = cons(value, *list);
}

void list_reverse(Atom *list) {
  if (!list) {
    return;
  }
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

int alistp(Atom expr) {
  if (nilp(expr)) {
    return 0;
  }
  while (!nilp(expr)) {
    if (expr.type != ATOM_TYPE_PAIR
        || car(expr).type != ATOM_TYPE_PAIR)
      {
        return 0;
      }
    expr = cdr(expr);
  }
  return 1;
}

Atom make_empty_alist() {
  return cons(cons(nil, nil), nil);
}

Atom make_alist(Atom key, Atom value) {
  return cons(cons(key, value), nil);
}

Atom alist_get(Atom alist, Atom key) {
  if (!alistp(alist) || nilp(key)) {
    return nil;
  }
  size_t i = 0;
  Atom list;
  Atom item;
  while (1) {
    list = list_get_past(alist, i);
    item = car(list);
    // Every item within an association list needs to be a pair!
    if (!pairp(item)) {
      return nil;
    }
    // Test if key matches.
    Atom is_match = nil;
    Error err;
    err.type = builtin_eq(cons(key, cons(car(item), nil)), &is_match);
    if (err.type) {
      printf("alist_get() ");
      print_error(err);
      return nil;
    }
    // Return value if key matched.
    if (!nilp(is_match)) {
      return cdr(item);
    }
    // Don't extend past end of alist.
    if (nilp(cdr(list))) {
      return nil;
    }
    i++;
  }
}

void alist_set(Atom *alist, Atom key, Atom value) {
  if (!alist || !alistp(*alist)) {
    return;
  }
  list_push(alist, cons(key, value));
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
  case ATOM_TYPE_BUFFER:
    printf("#<BUFFER>:\"%s\":%zu"
           , atom.value.buffer->path
           , atom.value.buffer->point_byte
           );
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
  size_t rightlen;
  size_t length = buffer ? strlen(buffer) : 0;
  size_t to_add = 0;
  const char *symbol_format  = "%s";
  const char *string_format  = "\"%s\"";
  const char *lr_format      = "(%s%s)";
  const char *l_format       = "(%s)";
  const char *builtin_format = "#<BUILTIN>:%p";
  const char *closure_format = "#<CLOSURE>:%p";
  const char *macro_format   = "#<MACRO>:%p";
  const char *buffer_format  = "#<BUFFER>:%p";
  switch (atom.type) {
  case ATOM_TYPE_NIL:
    to_add = 4;
    buffer = realloc(buffer, length+to_add);
    if (!buffer) { return NULL; }
    memmove(buffer+length, "NIL", to_add);
    break;
  case ATOM_TYPE_SYMBOL:
    to_add = format_bufsz(symbol_format, atom.value.symbol);
    buffer = realloc(buffer, length+to_add);
    if (!buffer) { return NULL; }
    snprintf(buffer+length, to_add, symbol_format, atom.value.symbol);
    break;
  case ATOM_TYPE_STRING:
    to_add = format_bufsz(string_format, atom.value.symbol);
    buffer = realloc(buffer, length+to_add);
    if (!buffer) { return NULL; }
    snprintf(buffer+length, to_add, string_format, atom.value.symbol);
    break;
  case ATOM_TYPE_INTEGER:
    // FIXME: Format only works when integer_t is long long int
    to_add = format_bufsz("%lli", atom.value.integer);
    buffer = realloc(buffer, length+to_add);
    if (!buffer) { return NULL; }
    snprintf(buffer+length, to_add, "%lli", atom.value.integer);
    break;
  case ATOM_TYPE_PAIR:
    right = malloc(8);
    if (!right) {
      return NULL;
    }
    right[0] = '\0';
    rightlen = 0;
    left = atom_string(car(atom), NULL);
    atom = cdr(atom);
    while (!nilp(atom)) {
      if (pairp(atom)) {
        char *new_right = atom_string(car(atom), NULL);
        if (!new_right) {
          break;
        }
        size_t new_rightlen = rightlen + strlen(new_right) + 2;
        right = realloc(right, new_rightlen);
        if (!right) {
          printf("could not reallocate string for right side of pair.\n");
          break;
        }
        rightlen = new_rightlen;
        strcat(right, " ");
        strcat(right, new_right);
        free(new_right);
        atom = cdr(atom);
      } else {
        char *new_right = atom_string(atom, NULL);
        if (!new_right) {
          break;
        }
        size_t new_rightlen = rightlen + strlen(new_right) + 2;
        right = realloc(right, new_rightlen);
        if (!right) {
          printf("could not reallocate string for right side of pair.\n");
          break;
        }
        rightlen = new_rightlen;
        strcat(right, ". ");
        strcat(right, new_right);
        free(new_right);
        break;
      }
    }
    if (left && right) {
      to_add = format_bufsz(lr_format, left, right);
      buffer = realloc(buffer, length+to_add);
      if (!buffer) { return NULL; }
      snprintf(buffer+length, to_add, lr_format, left, right);
      free(left);
      free(right);
    } else if (left) {
      to_add = format_bufsz(l_format, left);
      buffer = realloc(buffer, length+to_add);
      if (!buffer) { return NULL; }
      snprintf(buffer+length, to_add, l_format, left);
      free(left);
    }
    break;
  case ATOM_TYPE_BUILTIN:
    to_add = format_bufsz(builtin_format, atom.value.builtin);
    buffer = realloc(buffer, length+to_add);
    if (!buffer) { return NULL; }
    snprintf(buffer+length, to_add, builtin_format, atom.value.builtin);
    break;
  case ATOM_TYPE_CLOSURE:
    to_add = format_bufsz(closure_format, atom.value.builtin);
    buffer = realloc(buffer, length+to_add);
    if (!buffer) { return NULL; }
    snprintf(buffer+length, to_add, closure_format, atom.value.builtin);
    break;
  case ATOM_TYPE_MACRO:
    to_add = format_bufsz(macro_format, atom.value.builtin);
    buffer = realloc(buffer, length+to_add);
    if (!buffer) { return NULL; }
    snprintf(buffer+length, to_add, macro_format, atom.value.builtin);
    break;
  case ATOM_TYPE_BUFFER:
    to_add = format_bufsz(buffer_format, atom.value.buffer);
    buffer = realloc(buffer, length+to_add);
    if (!buffer) { return NULL; }
    snprintf(buffer+length, to_add, buffer_format, atom.value.builtin);
    break;
  }
  return buffer;
}
