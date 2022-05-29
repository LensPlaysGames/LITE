#include <environment.h>

#include <builtins.h>
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

Atom default_environment() {
  Atom environment = env_create(nil);
  env_set(environment, make_sym("T"),     make_sym("T"));
  env_set(environment, make_sym("CAR"),   make_builtin(builtin_car));
  env_set(environment, make_sym("CDR"),   make_builtin(builtin_cdr));
  env_set(environment, make_sym("CONS"),  make_builtin(builtin_cons));
  env_set(environment, make_sym("+"),     make_builtin(builtin_add));
  env_set(environment, make_sym("-"),     make_builtin(builtin_subtract));
  env_set(environment, make_sym("*"),     make_builtin(builtin_multiply));
  env_set(environment, make_sym("/"),     make_builtin(builtin_divide));
  env_set(environment, make_sym("="),     make_builtin(builtin_numeq));
  env_set(environment, make_sym("<"),     make_builtin(builtin_numlt));
  env_set(environment, make_sym("<="),    make_builtin(builtin_numlt_or_eq));
  env_set(environment, make_sym(">"),     make_builtin(builtin_numgt));
  env_set(environment, make_sym(">="),    make_builtin(builtin_numgt_or_eq));
  env_set(environment, make_sym("APPLY"), make_builtin(builtin_apply));
  return environment;
}
