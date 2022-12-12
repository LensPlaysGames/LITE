#include <types.h>

#include <assert.h>
#include <buffer.h>
#include <builtins.h>
#include <error.h>
#include <file_io.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool strict_output = false;

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
  GenericAllocation *galloc = calloc(1, sizeof(GenericAllocation));
  if (!galloc) {
    MAKE_ERROR(err, ERROR_MEMORY, *ref
               , "GALLOC: Could not allocate memory for generic allocation."
               , NULL);
    return err;
  }

  galloc->payload = payload;

  galloc->next = generic_allocations;
  generic_allocations = galloc;
  generic_allocations_count += 1;

  if (ref->galloc) {
    galloc->more = ref->galloc;
  }
  ref->galloc = galloc;

  return ok;
}

void gcol_mark(Atom *root) {
  if (invp(*root) || nilp(*root)) {
    return;
  }
  if (root->galloc) {
    GenericAllocation *galloc = root->galloc;
    while (galloc) {
      galloc->mark = 1;
      galloc = galloc->more;
    }
  }
  // Any type made with `cons()` belongs here.
  if (pairp(*root) || closurep(*root) || macrop(*root)) {
    ConsAllocation *alloc = (ConsAllocation *)
      ((char *)root->value.pair - offsetof(ConsAllocation, pair));
    if (!alloc || alloc->mark == 1) { return; }
    alloc->mark = 1;
    gcol_mark(&car(*root));
    gcol_mark(&cdr(*root));
  }
  if (envp(*root)) {
    size_t index = 0;
    while (index < root->value.env->data_capacity) {
      gcol_mark(root->value.env->data + index);
      ++index;
    }
    if (envp(root->value.env->parent)) {
      gcol_mark(&root->value.env->parent);
    }
  }
}

void gcol_mark_explicit(Atom *root) {
  if (invp(*root) || nilp(*root)) {
    return;
  }
  if (root->galloc) {
    GenericAllocation *galloc = root->galloc;
    while (galloc) {
      galloc->mark = 2;
      galloc = galloc->more;
    }
  }
  // Any type made with `cons()` belongs here.
  if (pairp(*root) || closurep(*root) || macrop(*root)) {
    ConsAllocation *alloc = (ConsAllocation *)
      ((char *)root->value.pair - offsetof(ConsAllocation, pair));
    if (!alloc || alloc->mark == 2) { return; }
    alloc->mark = 2;
    gcol_mark_explicit(&car(*root));
    gcol_mark_explicit(&cdr(*root));
  }
  if (envp(*root)) {
    size_t index = 0;
    while (index < root->value.env->data_capacity) {
      gcol_mark_explicit(root->value.env->data + index);
      ++index;
    }
    if (envp(root->value.env->parent)) {
      gcol_mark_explicit(&root->value.env->parent);
    }
  }
}

void gcol_unmark(Atom *root) {
  if (invp(*root) || nilp(*root)) {
    return;
  }
  if (root->galloc) {
    GenericAllocation *galloc = root->galloc;
    while (galloc) {
      galloc->mark = 0;
      galloc = galloc->more;
    }
  }
  // Any type made with `cons()` belongs here.
  if (pairp(*root) || closurep(*root) || macrop(*root)) {
    ConsAllocation *alloc = (ConsAllocation *)
      ((char *)root->value.pair - offsetof(ConsAllocation, pair));
    if (alloc->mark == 0) { return; }
    alloc->mark = 0;
    gcol_unmark(&car(*root));
    gcol_unmark(&cdr(*root));
  }
  if (envp(*root)) {
    size_t index = 0;
    while (index < root->value.env->data_capacity) {
      gcol_unmark(root->value.env->data + index);
      ++index;
    }
    if (envp(root->value.env->parent)) {
      gcol_unmark(&root->value.env->parent);
    }
  }
}


void gcol_cons(void) {
  // Sweep cons allocations (pairs).
  ConsAllocation **pair_allocations_it = &global_pair_allocations;
  ConsAllocation *prev_pair_allocation = NULL;
  ConsAllocation *pair_allocation;
  while ((pair_allocation = *pair_allocations_it)) {
    if (pair_allocation->mark == 0) {
      *pair_allocations_it = pair_allocation->next;
      if (prev_pair_allocation) {
        prev_pair_allocation->next = pair_allocation->next;
      } else {
        global_pair_allocations = pair_allocation->next;
      }
      free(pair_allocation);
      pair_allocation = NULL;
      pair_allocations_freed += 1;
      continue;
    }
    pair_allocations_it = &pair_allocation->next;
    prev_pair_allocation = pair_allocation;
  }
  // Clear mark.
  pair_allocation = global_pair_allocations;
  while (pair_allocation) {
    if (pair_allocation->mark == 1) {
      pair_allocation->mark = 0;
    }
    pair_allocation = pair_allocation->next;
  }
}

void gcol_generic(void) {
  GenericAllocation **galloc_it = &generic_allocations;
  GenericAllocation *prev_galloc = NULL;
  GenericAllocation *galloc = generic_allocations;
  while ((galloc = *galloc_it) != NULL) {
    if (galloc->mark == 0) {
      *galloc_it = galloc->next;
      if (prev_galloc) {
        prev_galloc->next = galloc->next;
      } else {
        generic_allocations = galloc->next;
      }
      if (galloc->payload) {
        free(galloc->payload);
        galloc->payload = NULL;
      }
      free(galloc);
      galloc = NULL;
      generic_allocations_freed += 1;
      continue;
    }
    galloc_it = &galloc->next;
    prev_galloc = galloc;
  }
  // Clear mark.
  galloc = generic_allocations;
  while (galloc) {
    if (galloc->mark == 1) {
      galloc->mark = 0;
    }
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

Atom nil_with_docstring(char *docstring) {
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

Atom make_int_with_docstring(integer_t value, char *docstring) {
  Atom a = nil;
  a.type = ATOM_TYPE_INTEGER;
  a.value.integer = value;
  a.docstring = docstring;
  return a;
}

//================================================================ BEG SYMBOL_TABLE

typedef struct SymbolTable {
  size_t data_count;
  size_t data_capacity;
  char **data;
} SymbolTable;

static void symbol_table_print(SymbolTable table) {
  printf("Symbol table:\n");
  char *str = NULL;
  for (size_t i = 0; i < table.data_capacity; ++i) {
    printf("  %zu:", i);
    if ((str = table.data[i])) {
      printf(" '%s'", str);
    }
    putchar('\n');
  }
}

static SymbolTable symbol_table_create(size_t initial_capacity) {
  SymbolTable out;
  out.data_count = 0;
  out.data_capacity = initial_capacity;
  out.data = calloc(1, initial_capacity * sizeof(*out.data));
  return out;
}

static void symbol_table_expand(SymbolTable *table);

static size_t sdbm(unsigned char *str) {
  size_t hash = 0;
  int c;

  while ((c = *str++)) {
    hash = c + (hash << 6) + (hash << 16) - hash;
  }
  return hash;
}

static size_t djb2(unsigned char *str) {
  size_t hash = 5381;
  int c;
  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c;
  }
  return hash;
}

static size_t symbol_table_hash(SymbolTable table, char *key) {
  // NOTE: I've gotten less collisions using SDBM than with DJB2.
  return sdbm((unsigned char *)key) & (table.data_capacity - 1);
}

/// Return the entry for the given key string.
static char **symbol_table_entry(SymbolTable table, char *key) {
  if (!key) {
    fprintf(stderr, "Can not get symbol table entry for NULL key!\n");
    return NULL;
  }
  size_t index = symbol_table_hash(table, key);
  char **entry = table.data + index;

  size_t index_it = index;
  while (*entry && index_it < table.data_capacity) {
    // Return entry if it matches value.
    if (strcmp(*entry, key) == 0) {
      return entry;
    }
    index_it++;
    entry++;
  }
  if (index_it < table.data_capacity) {
    return entry;
  }
  entry = table.data;
  index_it = 0;
  while (*entry && index_it < index) {
    // Return entry if it matches value.
    if (strcmp(*entry, key) == 0) {
      return entry;
    }
    index_it++;
    entry++;
  }
  if (index_it >= index) {
    fprintf(stderr, "Could not find matching entry of key \"%s\" in symbol table...\n", key);
    assert(index < table.data_capacity && "Could not find matching entry in symbol table.");
  }
  return entry;
}

/// Attempt to get symbol at KEY, inserting KEY if not found.
static char *symbol_table_get_or_insert(SymbolTable *table, char *key) {
  // If data_count is too close to data_capacity, expand.
  if (table->data_count > (table->data_capacity >> 1)) {
    symbol_table_expand(table);
  }

  // Get entry at key in table.
  char **entry = symbol_table_entry(*table, key);
  // If entry contains NULL string, insert duplicate.
  if (!(*entry)) {
    //printf("Inserting \"%s\"\n", key);
    *entry = key;
    table->data_count += 1;
  }
  return *entry;
}

/// Attempt to get symbol at KEY, inserting a duplicate of KEY if not found.
static char *symbol_table_get_or_insert_duplicate(SymbolTable *table, char *key) {
  // If data_count is too close to data_capacity, expand.
  if (table->data_count > (table->data_capacity >> 1)) {
    symbol_table_expand(table);
  }

  // Get entry at key in table.
  char **entry = symbol_table_entry(*table, key);
  // If entry contains NULL string, insert duplicate.
  if (!(*entry)) {
    //printf("Inserting \"%s\"\n", key);
    *entry = strdup(key);
    table->data_count += 1;
  }
  return *entry;
}

static void symbol_table_free(SymbolTable table) {
  free(table.data);
}

static void symbol_table_expand(SymbolTable *table) {
  // Create a new, larger hash table.
  size_t old_capacity = table->data_capacity;
  size_t new_capacity = table->data_capacity << 1;
  SymbolTable new_table = symbol_table_create(new_capacity);

  // Rehash all values from old table into new table. This is needed
  // because the index where the symbol is stored is a function of the
  // capacity of the table: when the capacity changes, so does the
  // mapping of hashes to indices. There aren't really many ways to
  // avoid this. One would be to not actually get rid of the old table,
  // and have it implemented as a linked list of tables, but this very
  // quickly slows down lookup and insertion by quite a bit. This
  // rehashing method, while expensive, only occurs during expansion,
  // which very rarely occurs in the first place.
  char **entry = table->data;
  for (size_t i = 0; i < old_capacity; ++i, ++entry) {
    if (*entry) {
      symbol_table_get_or_insert(&new_table, *entry);
    }
  }

  // Update table data.
  symbol_table_free(*table);
  *table = new_table;

  // Debug output.
  size_t old_size = old_capacity * sizeof(*table->data);
  size_t new_size = old_size << 1;
  if (!strict_output) {
    printf("Symbol table size expanded from %zu to %zu entries (%zu to %zu bytes)\n",
           old_capacity, new_capacity, old_size, new_size);
  }
}

//================================================================ END SYMBOL_TABLE


static SymbolTable table = {0};

void print_symbol_table() { symbol_table_print(table); }

Atom symbol_table() {
  Atom out = nil;
  char **entry = table.data;
  for (size_t i = 0; i < table.data_capacity; ++i, ++entry) {
    if (*entry) {
      out = cons(make_sym(*entry), out);
    }
  }
  return out;
}

// TODO: uppercase value symbol in search.
Atom make_sym(char *value) {
  if (table.data_capacity == 0) {
    table = symbol_table_create(256);
  }

  // Try to get existing entry in symbol table.
  char *symbol = NULL;
  symbol = symbol_table_get_or_insert_duplicate(&table, value);

  // Create a new symbol.
  Atom a = nil;
  a.type = ATOM_TYPE_SYMBOL;
  a.value.symbol = symbol;
  return a;
}

Atom make_string(char *contents) {
  if (!contents) { return nil; }
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

Atom make_builtin(BuiltInFunction function, char *name, char *docstring) {
  Atom builtin = nil;
  builtin.type = ATOM_TYPE_BUILTIN;
  builtin.value.builtin.function = function;
  builtin.value.builtin.name = name;
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

  char *buffer_path = getfullpath(path);
  if (!buffer_path) {
    MAKE_ERROR(err, ERROR_FILE, nil
               , "make_buffer: Could not get full path of file\n"
               , NULL);
    print_error(err);
    return nil;
  }

  // Attempt to find existing buffer in buffer table.
  Atom buffer_table_it = buffer_table;
  while (!nilp(buffer_table_it)) {
    Atom a = car(buffer_table_it);
    if (strcmp(a.value.buffer->path, buffer_path) == 0) {
      return a;
    }
    buffer_table_it = cdr(buffer_table_it);
  }
  // Create new buffer and add it to buffer table.
  Buffer *buffer = buffer_create(buffer_path);
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

Atom list_get_safe(Atom list, int k) {
  while (k--) {
    if (nilp(cdr(list))) {
      return list;
    }
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
  for (Atom list = alist; !nilp(list); list = cdr(list)) {
    Atom item = car(list);
    // Every item within an association list needs to be a pair!
    if (!pairp(item)) {
      return nil;
    }
    // Return value if key matches.
    if (!nilp(compare_atoms(key, car(item)))) {
      return cdr(item);
    }
  }
  return nil;
}

void alist_set(Atom *alist, Atom key, Atom value) {
  if (!alist || !alistp(*alist)) {
    return;
  }
  list_push(alist, cons(key, value));
}

void print_atom(Atom atom) {
  assert(ATOM_TYPE_MAX == 10);
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
  case ATOM_TYPE_ENVIRONMENT: {
    putchar('(');
    size_t index = 0;
    Atom it;
    while (index < atom.value.env->data_capacity) {
      it = atom.value.env->data[index];
      if (!nilp(it)) {
        print_atom(it);
        putchar(' ');
      }
    }
    putchar(')');
    } break;
  case ATOM_TYPE_SYMBOL:
    printf("%s", atom.value.symbol);
    break;
  case ATOM_TYPE_INTEGER:
    printf("%lli", atom.value.integer);
    break;
  case ATOM_TYPE_BUILTIN:
    printf("#<BUILTIN>:%s", atom.value.builtin.name);
    break;
  case ATOM_TYPE_CLOSURE:
    printf("#<CLOSURE>:%p", (void *)&atom);
    break;
  case ATOM_TYPE_MACRO:
    printf("#<MACRO>:%p", (void *)&atom);
    break;
  case ATOM_TYPE_STRING:
    printf("\"%s\"", atom.value.symbol);
    break;
  case ATOM_TYPE_BUFFER:
    printf("#<BUFFER>:\"%s\":%zu",
           atom.value.buffer->path,
           atom.value.buffer->point_byte);
    break;
  }
}

void pretty_print_atom(Atom atom) {
  switch (atom.type) {
  case ATOM_TYPE_ENVIRONMENT: {
    putchar('(');
    size_t index = 0;
    Atom it;
    while (index < atom.value.env->data_capacity) {
      it = atom.value.env->data[index];
      if (!nilp(it)) {
        pretty_print_atom(it);
        putchar('\n');
      }
    }
    putchar(')');
    } break;
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
  assert(ATOM_TYPE_MAX == 9);
  char *left;
  char *right;
  size_t rightlen;
  size_t length = buffer ? strlen(buffer) : 0;
  size_t to_add = 0;
  const char *symbol_format  = "%s";
  const char *string_format  = "\"%s\"";
  const char *lr_format      = "(%s%s)";
  const char *l_format       = "(%s)";
  const char *builtin_format = "#<BUILTIN>:%s";
  const char *closure_format = "#<CLOSURE>:%p";
  const char *macro_format   = "#<MACRO>:%p";
  const char *buffer_format  = "#<BUFFER>:\"%s\":%zu";
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
    to_add = format_bufsz(builtin_format, atom.value.builtin.name);
    buffer = realloc(buffer, length+to_add);
    if (!buffer) { return NULL; }
    snprintf(buffer+length, to_add, builtin_format, atom.value.builtin.name);
    break;
  case ATOM_TYPE_CLOSURE:
    to_add = format_bufsz(closure_format, &atom);
    buffer = realloc(buffer, length+to_add);
    if (!buffer) { return NULL; }
    snprintf(buffer+length, to_add, closure_format, &atom);
    break;
  case ATOM_TYPE_MACRO:
    to_add = format_bufsz(macro_format, &atom);
    buffer = realloc(buffer, length+to_add);
    if (!buffer) { return NULL; }
    snprintf(buffer+length, to_add, macro_format, &atom);
    break;
  case ATOM_TYPE_BUFFER:
    to_add = format_bufsz(buffer_format, atom.value.buffer->path, atom.value.buffer->point_byte);
    buffer = realloc(buffer, length+to_add);
    if (!buffer) { return NULL; }
    snprintf(buffer+length, to_add, buffer_format, atom.value.buffer->path, atom.value.buffer->point_byte);
    break;
  default:
    break;
  }
  return buffer;
}

Atom compare_atoms(Atom a, Atom b) {
  int equal = 0;
  //assert(ATOM_TYPE_MAX == 10);
  _Static_assert(ATOM_TYPE_MAX == 10, "compare_atoms(): Exhaustive handling of atom types");
  if (a.type == b.type) {
    switch (a.type) {
    case ATOM_TYPE_NIL:
      equal = 1;
      break;
    case ATOM_TYPE_PAIR:
    case ATOM_TYPE_CLOSURE:
    case ATOM_TYPE_MACRO:
      equal = (a.value.pair == b.value.pair);
      break;
    case ATOM_TYPE_SYMBOL:
      equal = (a.value.symbol == b.value.symbol);
      break;
    case ATOM_TYPE_STRING:
      // Special case for empty string.
      if (a.value.symbol[0] == '\0' && b.value.symbol[0] == '0') {
        equal = 1;
      } else {
        equal = (strcmp(a.value.symbol, b.value.symbol) == 0);
      }
      break;
    case ATOM_TYPE_INTEGER:
      equal = (a.value.integer == b.value.integer);
      break;
    case ATOM_TYPE_BUILTIN:
      equal = (a.value.builtin.function == b.value.builtin.function);
      break;
    case ATOM_TYPE_BUFFER:
      equal = (a.value.buffer == b.value.buffer);
      break;
    default:
      equal = 0;
      break;
    case ATOM_TYPE_ENVIRONMENT:
      equal = (a.value.env == b.value.env);
      break;
    }
  }
  return equal ? make_sym("T") : nil;
}
