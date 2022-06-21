#ifndef LITE_TYPES_H
#define LITE_TYPES_H

#include <stddef.h>

struct Atom;
/// All C functions that are to be called from LISP will have this prototype.
typedef int (*BuiltIn)(struct Atom arguments, struct Atom *result);

typedef struct Buffer Buffer;

typedef struct Error Error;

struct GenericAllocation;

typedef long long int integer_t;
typedef const char symbol_t;
typedef struct Atom {
  enum AtomType {
    ATOM_TYPE_NIL,
    ATOM_TYPE_PAIR,
    ATOM_TYPE_SYMBOL,
    ATOM_TYPE_INTEGER,
    ATOM_TYPE_BUILTIN,
    ATOM_TYPE_CLOSURE,
    ATOM_TYPE_MACRO,
    ATOM_TYPE_STRING,
    ATOM_TYPE_BUFFER,
  } type;
  union {
    struct Pair *pair;
    symbol_t *symbol;
    Buffer *buffer;
    BuiltIn builtin;
    integer_t integer;
  } value;
  symbol_t *docstring;
  struct GenericAllocation *galloc;
} Atom;
struct Pair {
  Atom atom[2];
};

static const Atom nil = { ATOM_TYPE_NIL, 0, NULL, NULL };

#define nilp(a)     ((a).type == ATOM_TYPE_NIL)
#define pairp(a)    ((a).type == ATOM_TYPE_PAIR)
#define symbolp(a)  ((a).type == ATOM_TYPE_SYMBOL)
#define integerp(a) ((a).type == ATOM_TYPE_INTEGER)
#define builtinp(a) ((a).type == ATOM_TYPE_BUILTIN)
#define closurep(a) ((a).type == ATOM_TYPE_CLOSURE)
#define macrop(a)   ((a).type == ATOM_TYPE_MACRO)
#define stringp(a)  ((a).type == ATOM_TYPE_STRING)
#define bufferp(a)  ((a).type == ATOM_TYPE_BUFFER)

#define car(a) ((a).value.pair->atom[0])
#define cdr(a) ((a).value.pair->atom[1])

//================================================================ BEG garbage_collection

struct ConsAllocation {
  struct ConsAllocation *next;
  struct Pair pair;
  char mark;
};
typedef struct ConsAllocation ConsAllocation;

extern ConsAllocation *global_pair_allocations;
extern size_t pair_allocations_count;
extern size_t pair_allocations_freed;

typedef struct GenericAllocation {
  struct GenericAllocation *next;
  void *payload;
  char mark;
  struct GenericAllocation *more;
} GenericAllocation;

extern GenericAllocation *generic_allocations;
extern size_t generic_allocations_count;
extern size_t generic_allocations_freed;

/// payload is a malloc()-derived pointer,
/// ref is the referring Atom that, when
/// garbage collected, will free the payload.
Error gcol_generic_allocation(Atom *ref, void *payload);

/// Mark cons and generic allocations as needed,
/// so as to not free them on the next gcol().
void gcol_mark(Atom root);

/// Garbage collect all unmarked allocations.
void gcol();

void print_gcol_data();

//================================================================ END garbage_collection

/// Returns a heap-allocated pair atom with car and cdr set.
Atom cons(Atom car_atom, Atom cdr_atom);

/// Returns boolean-like value, 0 = false.
int listp(Atom expr);
/// Get a single element in a list from a given index.
Atom list_get(Atom list, int k);
/// Get all elements in a list past a given index.
Atom list_get_past(Atom list, int k);
/// Set an elements value from a given index.
void list_set(Atom list, int k, Atom value);
/// Push a value on to the beginning of a list.
void list_push(Atom *list, Atom value);
void list_reverse(Atom *list);
Atom copy_list(Atom list);

/// Returns boolean-like value, 0 = false.
int alistp(Atom expr);
/// Make a valid association list with only a nil entry.
Atom make_empty_alist();
/// Make an association list with a given initial key/value pair.
Atom make_alist(Atom key, Atom value);
/// Get the value associated with a key, otherwise return NIL.
Atom alist_get(Atom alist, Atom key);
/// Set the value associated with a key in a given alist.
void alist_set(Atom *alist, Atom key, Atom value);

Atom nil_with_docstring(symbol_t *docstring);
Atom make_int(integer_t value);
Atom make_int_with_docstring(integer_t value, symbol_t *docstring);
Atom make_sym(symbol_t *value);
Atom make_string(symbol_t *value);
Atom make_builtin(BuiltIn function, symbol_t *docstring);
Error make_closure(Atom environment, Atom arguments, Atom body, Atom *result);
Atom make_buffer(Atom environment, char *path);

Atom *sym_table();

Atom *buf_table();
void free_buffer_table();

char *atom_string(Atom atom, char *str);

void print_atom(Atom atom);
/// Print lists' CDR on newline.
void pretty_print_atom(Atom atom);

#endif /* #ifndef LITE_TYPES_H */
