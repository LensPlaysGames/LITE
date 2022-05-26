#ifndef LITE_TYPES_H
#define LITE_TYPES_H

typedef long long integer_t;
typedef const char symbol_t;
struct Atom {
  enum {
    ATOM_TYPE_NIL,
    ATOM_TYPE_PAIR,
    ATOM_TYPE_SYMBOL,
    ATOM_TYPE_INTEGER,
  } type;
  union {
    struct Pair *pair;
    symbol_t *symbol;
    integer_t integer;
  } value;
};
struct Pair {
  struct Atom atom[2];
};
// This allows not including the 'struct' keyword.
typedef struct Atom Atom;

#define nilp(a) ((a).type == ATOM_TYPE_NIL)
#define car(a) ((a).value.pair->atom[0])
#define cdr(a) ((a).value.pair->atom[1])

static const Atom nil = { ATOM_TYPE_NIL, 0 };

#endif /* #ifndef LITE_TYPES_H */
