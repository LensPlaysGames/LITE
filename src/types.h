#ifndef LITE_TYPES_H
#define LITE_TYPES_H

#include <stdbool.h>
#include <stddef.h>

/// If this is non-zero, NOTHING should be output except LITE LISP
/// evaluated output and errors (for testing).
/// This mostly is here to disable small but helpful messages in the
/// output log (i.e. symbol table expansion, garbage collection, etc).
extern bool strict_output;

struct Error;

struct Atom;
/// All C functions that are to be called from LISP will have this prototype.
typedef struct Error (*BuiltInFunction)(struct Atom arguments, struct Atom *result);

typedef struct BuiltIn {
  char *name;
  BuiltInFunction function;
} BuiltIn;

struct Environment;

typedef struct Buffer Buffer;
typedef struct Error Error;
typedef struct GenericAllocation GenericAllocation;
typedef long long int integer_t;

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
    ATOM_TYPE_ENVIRONMENT,
    ATOM_TYPE_MAX,
  } type;
  union AtomValue {
    struct Pair *pair;
    char *symbol;
    Buffer *buffer;
    BuiltIn builtin;
    integer_t integer;
    struct Environment *env;
  } value;
  char *docstring;
  GenericAllocation *galloc;
} Atom;

typedef struct Pair {
  Atom atom[2];
} Pair;

typedef struct EnvironmentValue {
  char *key;  //> Symbol pointer
  Atom value;
} EnvironmentValue;

typedef struct Environment {
  struct Atom parent;
  size_t data_count;
  size_t data_capacity;
  struct EnvironmentValue *data;
} Environment;

static const Atom nil = { ATOM_TYPE_NIL,     { 0 }, NULL, NULL };

#define nilp(a)     ((a).type == ATOM_TYPE_NIL)
#define pairp(a)    ((a).type == ATOM_TYPE_PAIR)
#define symbolp(a)  ((a).type == ATOM_TYPE_SYMBOL)
#define integerp(a) ((a).type == ATOM_TYPE_INTEGER)
#define builtinp(a) ((a).type == ATOM_TYPE_BUILTIN)
#define closurep(a) ((a).type == ATOM_TYPE_CLOSURE)
#define macrop(a)   ((a).type == ATOM_TYPE_MACRO)
#define stringp(a)  ((a).type == ATOM_TYPE_STRING)
#define bufferp(a)  ((a).type == ATOM_TYPE_BUFFER)
#define envp(a)     ((a).type == ATOM_TYPE_ENVIRONMENT)

#define car(a) ((a).value.pair->atom[0])
#define cdr(a) ((a).value.pair->atom[1])

//================================================================ BEG garbage_collection

/* The garbage collector in LITE LISP is mark-and-sweep. That means
 * that, on each garbage collection, each piece of allocated memory
 * (that is to be garbage collected) must be marked, otherwise it will
 * be freed in the subsequent sweep. The sweep is simply freeing all
 * unmarked memory.
 * There are two types of memory that may be garbage collected:
 * - ConsAllocation :: A pair Atom. Contains space for two child Atoms.
 * - GenericAllocation :: Data attached to any Atom that must be freed
 *   along with it. This includes strings for String Atoms, and is
 *   generic enough to allow any amount of data to be allocated and
 *   de-allocated along with any Atom.
 */

typedef struct ConsAllocation {
  struct ConsAllocation *next;
  Pair pair;
  char mark;
} ConsAllocation;

extern ConsAllocation *global_pair_allocations;
extern size_t pair_allocations_count;
extern size_t pair_allocations_freed;

typedef struct GenericAllocation {
  struct GenericAllocation *next;
  struct GenericAllocation *more;
  void *payload;
  char mark;
} GenericAllocation;

extern GenericAllocation *generic_allocations;
extern size_t generic_allocations_count;
extern size_t generic_allocations_freed;

/** Attach allocated pointer to an existing Atom to be freed when the
 *  Atom is.
 *
 * @param ref The referring atom that the payload will be attached to.
 * @param payload A pointer to allocated memory that will be freed when
 *                the Atom is no longer needed.
 *
 * @retval ERROR_NONE Success
 * @retval ERROR_ARGUMENTS One of the arguments was NULL.
 * @retval ERROR_MEMORY Could not allocate memory for new generic
 *                      allocation.
 */
Error gcol_generic_allocation(Atom *ref, void *payload);

/** Mark atoms that are accessible from a given root as in-use,
 *  preventing them from being garbage collected.
 *
 * This will mark pairs that have been allocated with `cons`, as well
 * as generic allocations registered to atoms.
 *
 * DO NOT CALL WITH NULL ARGUMENT!
 *
 * @param root All atoms accessible from this atom will be marked.
 *
 * @see gcol()
 */
void gcol_mark(Atom *root);

/** Just like gcol_mark() except it won't be unmarked at next gcol().
 *
 * Mark atoms that are accessible from a given root as in-use,
 * preventing them from being garbage collected.
 *
 * This will mark pairs that have been allocated with `cons`, as well
 * as generic allocations registered to atoms.
 *
 * DO NOT CALL WITH NULL ARGUMENT!
 */
void gcol_mark_explicit(Atom *root);

/** Unmark all allocations reachable from ROOT unconditionally.
 *
 * Used to unmark atoms marked with gcol_mark_explicit(), mainly,
 * due to it's unconditional unmarking.
 *
 * DO NOT CALL WITH NULL ARGUMENT!
 */
void gcol_unmark(Atom *root);

/** Do a garbage collection.
 *
 * For all allocations within the global allocation list, free
 * allocations not marked as in use.
 *
 * Both ConsAllocation and GenericAllocation are handled.
 */
void gcol();

/** Print all data collected surrounding garbage collection.
 *
 * This includes amount of created and freed allocations for both types
 * of allocation (pair/cons and generic).
 */
void print_gcol_data();

//================================================================ END garbage_collection

/// Returns a heap-allocated pair atom with car and cdr set.
Atom cons(Atom car_atom, Atom cdr_atom);

/// If given atom is a valid list, return 1. Otherwise, return 0.
int listp(Atom expr);

/** Get a single element in a list from a given index.
 *
 * This function does not check for nil, so ensure to give it an
 * in-bounds index.
 *
 * @param list The list to get an element from.
 * @param k The index of the element within the list to get.
 *
 * @return The element at index `k` within the given list.
 */
Atom list_get(Atom list, int k);

/** Get a single element in a list from a given index.
 *
 * @param list The list to get an element from.
 * @param k The index of the element within the list to get.
 *
 * @return The element at index `k` within the given list, or the last
 * element if `k` is out of bounds.
 */
Atom list_get_safe(Atom list, int k);

/** Get the rest of the elements in a list after a given index.
 *
 * This function does not check for nil, so ensure to give it an
 * in-bounds index.
 *
 * @param list The list to get the rest of the elements from.
 * @param k The index to get the returned elements after (inclusive).
 *
 * @return A list containing the rest of the elements of the given list
 *         starting at index `k`.
 */
Atom list_get_past(Atom list, int k);

/** Set the element at index `k` to the Atom `value`.
 *
 * This function does not check for nil, so ensure to give it an
 * in-bounds index.
 *
 * @param list The list to modify.
 * @param k The index to get the returned elements after (inclusive).
 * @param value The atom that will be placed at index `k` in the given
 *              list.
 */
void list_set(Atom list, int k, Atom value);

/** Push a value on to the given list, adding it to the beginning.
 *
 * @param list A pointer to the list to modify.
 * @param value The atom that will be placed at the beginning of the
 *              given list.
 */
void list_push(Atom *list, Atom value);

/** Reverse a given list.
 *
 * @param list A pointer to the list to reverse.
 */
void list_reverse(Atom *list);

/** Create a new Atom that is a copy of an existing list.
 *
 * @param list
 *   The list to copy.
 *
 * @return A list that is the exact copy of the given list.
 */
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

Atom nil_with_docstring(char *docstring);
Atom make_int(integer_t value);
Atom make_int_with_docstring(integer_t value, char *docstring);
Atom make_sym(char *value);
Atom make_string(char *value);
Atom make_builtin(BuiltInFunction function, char *name, char *docstring);
Error make_closure(Atom environment, Atom arguments, Atom body, Atom *result);
Atom make_buffer(Atom environment, char *path);

/// Print the global symbol table to stdout.
void print_symbol_table(void);
/// Build a LISP atom from the current global symbol table.
Atom symbol_table(void);

/// Return a pointer to the global buffer table (never NULL).
Atom *buf_table(void);

/** Get a heap-allocated string containing the textual representation
 *  of the given atom.
 *
 * @param atom The atom that will be represented in text.
 * @param str A pointer to a heap-allocated string that will be
 *            appended to. If NULL, a new string is created.
 *
 * @return A string containing the textual representation of the given
 *         atom.
 */
char *atom_string(Atom atom, char *str);

/** Compare two atoms.
 *
 * The `EQ` builtin uses this comparison function.
 *
 * @return T iff atoms a and b are equal, otherwise NIL.
 */
Atom compare_atoms(Atom a, Atom b);

/// Print an atom to standard out.
void print_atom(Atom atom);
/// Exactly like print_atom, except print lists' CDR on a newline.
void pretty_print_atom(Atom atom);

#endif /* #ifndef LITE_TYPES_H */
