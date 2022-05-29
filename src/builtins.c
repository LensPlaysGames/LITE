#include <builtins.h>

#include <error.h>
#include <evaluation.h>
#include <types.h>

int builtin_car(Atom arguments, Atom *result) {
  if (nilp(arguments) || !nilp(cdr(arguments))) {
    return ERROR_ARGUMENTS;
  }
  if (nilp(car(arguments))) {
    *result = nil;
  } else if (car(arguments).type != ATOM_TYPE_PAIR) {
    return ERROR_TYPE;
  } else {
    *result = car(car(arguments));
  }
  return ERROR_NONE;
}

int builtin_cdr(Atom arguments, Atom *result) {
  if (nilp(arguments) || !nilp(cdr(arguments))) {
    return ERROR_ARGUMENTS;
  }
  if (nilp(car(arguments))) {
    *result = nil;
  } else if (car(arguments).type != ATOM_TYPE_PAIR) {
    return ERROR_TYPE;
  } else {
    *result = cdr(car(arguments));
  }
  return ERROR_NONE;
}

int builtin_cons(Atom arguments, Atom *result) {
  if (nilp(arguments) || nilp(cdr(arguments)) || !nilp(cdr(cdr(arguments)))) {
    return ERROR_ARGUMENTS;
  }
  *result = cons(car(arguments), car(cdr(arguments)));
  return ERROR_NONE;
}

int builtin_add(Atom arguments, Atom *result) {
  if (nilp(arguments) || nilp(cdr(arguments)) || !nilp(cdr(cdr(arguments)))) {
    return ERROR_ARGUMENTS;
  }
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (lhs.type != ATOM_TYPE_INTEGER || rhs.type != ATOM_TYPE_INTEGER) {
    return ERROR_TYPE;
  }
  *result = make_int(lhs.value.integer + rhs.value.integer);
  return ERROR_NONE;
}

int builtin_subtract(Atom arguments, Atom *result) {
  if (nilp(arguments) || nilp(cdr(arguments)) || !nilp(cdr(cdr(arguments)))) {
    return ERROR_ARGUMENTS;
  }
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (lhs.type != ATOM_TYPE_INTEGER || rhs.type != ATOM_TYPE_INTEGER) {
    return ERROR_TYPE;
  }
  *result = make_int(lhs.value.integer - rhs.value.integer);
  return ERROR_NONE;
}

int builtin_multiply(Atom arguments, Atom *result) {
  if (nilp(arguments) || nilp(cdr(arguments)) || !nilp(cdr(cdr(arguments)))) {
    return ERROR_ARGUMENTS;
  }
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (lhs.type != ATOM_TYPE_INTEGER || rhs.type != ATOM_TYPE_INTEGER) {
    return ERROR_TYPE;
  }
  *result = make_int(lhs.value.integer * rhs.value.integer);
  return ERROR_NONE;
}

int builtin_divide(Atom arguments, Atom *result) {
  if (nilp(arguments) || nilp(cdr(arguments)) || !nilp(cdr(cdr(arguments)))) {
    return ERROR_ARGUMENTS;
  }
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (lhs.type != ATOM_TYPE_INTEGER || rhs.type != ATOM_TYPE_INTEGER) {
    return ERROR_TYPE;
  }
  if (rhs.value.integer == 0) {
    return ERROR_ARGUMENTS;
  }
  *result = make_int(lhs.value.integer / rhs.value.integer);
  return ERROR_NONE;
}

int builtin_numeq(Atom arguments, Atom *result) {
  if (nilp (arguments) || nilp(cdr(arguments)) || !nilp(cdr(cdr(arguments)))) {
    return ERROR_ARGUMENTS;
  }
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (lhs.type != ATOM_TYPE_INTEGER || rhs.type != ATOM_TYPE_INTEGER) {
    return ERROR_TYPE;
  }
  *result = lhs.value.integer == rhs.value.integer ? make_sym("T") : nil;
  return ERROR_NONE;
}

int builtin_numlt(Atom arguments, Atom *result) {
  if (nilp (arguments) || nilp(cdr(arguments)) || !nilp(cdr(cdr(arguments)))) {
    return ERROR_ARGUMENTS;
  }
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (lhs.type != ATOM_TYPE_INTEGER || rhs.type != ATOM_TYPE_INTEGER) {
    return ERROR_TYPE;
  }
  *result = lhs.value.integer < rhs.value.integer ? make_sym("T") : nil;
  return ERROR_NONE;
}

int builtin_numlt_or_eq(Atom arguments, Atom *result) {
  if (nilp (arguments) || nilp(cdr(arguments)) || !nilp(cdr(cdr(arguments)))) {
    return ERROR_ARGUMENTS;
  }
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (lhs.type != ATOM_TYPE_INTEGER || rhs.type != ATOM_TYPE_INTEGER) {
    return ERROR_TYPE;
  }
  *result = lhs.value.integer <= rhs.value.integer ? make_sym("T") : nil;
  return ERROR_NONE;
}

int builtin_numgt(Atom arguments, Atom *result) {
  if (nilp (arguments) || nilp(cdr(arguments)) || !nilp(cdr(cdr(arguments)))) {
    return ERROR_ARGUMENTS;
  }
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (lhs.type != ATOM_TYPE_INTEGER || rhs.type != ATOM_TYPE_INTEGER) {
    return ERROR_TYPE;
  }
  *result = lhs.value.integer > rhs.value.integer ? make_sym("T") : nil;
  return ERROR_NONE;
}

int builtin_numgt_or_eq(Atom arguments, Atom *result) {
  if (nilp (arguments) || nilp(cdr(arguments)) || !nilp(cdr(cdr(arguments)))) {
    return ERROR_ARGUMENTS;
  }
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (lhs.type != ATOM_TYPE_INTEGER || rhs.type != ATOM_TYPE_INTEGER) {
    return ERROR_TYPE;
  }
  *result = lhs.value.integer >= rhs.value.integer ? make_sym("T") : nil;
  return ERROR_NONE;
}

int builtin_apply(Atom arguments, Atom *result) {
  if (nilp(arguments) || nilp(cdr(arguments)) || !nilp(cdr(cdr(arguments)))) {
    return ERROR_ARGUMENTS;
  }
  Atom function;
  function = car(arguments);
  arguments = car(cdr(arguments));
  if (!listp(arguments)) {
    return ERROR_SYNTAX;
  }
  return apply(function, arguments, result);
}
