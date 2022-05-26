#include <environment.h>

#include <error.h>
#include <types.h>

Atom env_create(Atom parent) {
  return cons(parent, nil);
}

int env_set(Atom environment, Atom symbol, Atom value) {
  Atom bindings = cdr(environment);
  while (!nilp(bindings)) {
    Atom bind = car(bindings);
    if (car(bind).value.symbol == symbol.value.symbol) {
      cdr(bind) = value;
      return ERROR_NONE;
    }
    bindings = cdr(bindings);
  }
  bindings = cons(symbol, value);
  cdr(environment) = cons(bindings, cdr(environment));
  return ERROR_NONE;
}

int env_get(Atom environment, Atom symbol, Atom *result) {
  Atom parent = car(environment);
  Atom bindings = cdr(environment);
  while (!nilp(bindings)) {
    Atom bind = car(bindings);
    if (car(bind).value.symbol == symbol.value.symbol) {
      *result = cdr(bind);
      return ERROR_NONE;
    }
    bindings = cdr(bindings);
  }
  if (nilp(parent)) {
    return ERROR_NOT_BOUND;
  }
  return env_get(parent, symbol, result);
}
