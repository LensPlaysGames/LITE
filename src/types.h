#ifndef LITE_TYPES_H
#define LITE_TYPES_H

#include <stddef.h>

struct Atom;

/// All C functions that are to be called from LISP will have this prototype.
typedef int (*BuiltIn)(struct Atom arguments, struct Atom *result);

typedef long long integer_t;
typedef const char symbol_t;
typedef struct Atom {
  enum {
    ATOM_TYPE_NIL,
    ATOM_TYPE_PAIR,
    ATOM_TYPE_SYMBOL,
    ATOM_TYPE_INTEGER,
    ATOM_TYPE_BUILTIN,
    ATOM_TYPE_CLOSURE,
    ATOM_TYPE_MACRO,
    ATOM_TYPE_STRING,
  } type;
  union {
    struct Pair *pair;
    symbol_t *symbol;
    integer_t integer;
    BuiltIn builtin;
  } value;
  symbol_t *docstring;
  void *allocation;
} Atom;
struct Pair {
  struct Atom atom[2];
};

#define nilp(a) ((a).type == ATOM_TYPE_NIL)
#define car(a) ((a).value.pair->atom[0])
#define cdr(a) ((a).value.pair->atom[1])

#define integerp(a) ((a).type == ATOM_TYPE_INTEGER)

static const Atom nil = { ATOM_TYPE_NIL, 0, NULL, NULL };

struct ConsAllocation {
  struct ConsAllocation *next;
  struct Pair pair;
  char mark;
};
typedef struct ConsAllocation ConsAllocation;

extern ConsAllocation *global_pair_allocations;
extern size_t pair_allocations_count;
extern size_t pair_allocations_freed;

struct GenericAllocation {
  struct GenericAllocation *next;
  void *payload;
  char mark;
};
typedef struct GenericAllocation GenericAllocation;
extern GenericAllocation *generic_allocations;
extern size_t generic_allocations_count;
extern size_t generic_allocations_freed;

int gcol_generic_allocation(Atom *ref, void *payload);

void gcol_mark(Atom root);
void gcol();

/// Returns a heap-allocated pair atom with car and cdr set.
Atom cons(Atom car_atom, Atom cdr_atom);

/// Returns boolean-like value, 0 = false.
int listp(Atom expr);

Atom list_get(Atom list, int k);
void list_set(Atom list, int k, Atom value);
void list_reverse(Atom *list);

Atom copy_list(Atom list);

Atom make_int(integer_t value);
Atom make_sym(symbol_t *value);
Atom make_string(symbol_t *value);
Atom make_builtin(BuiltIn function, symbol_t *docstring);
typedef struct Error Error;
Error make_closure(Atom environment, Atom arguments, Atom body, Atom *result);

Atom *sym_table();
void free_symbol_table();

void print_atom(Atom atom);
/// Print lists' CDR on newline.
void pretty_print_atom(Atom atom);

#endif /* #ifndef LITE_TYPES_H */
