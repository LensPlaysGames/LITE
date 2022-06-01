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

int env_non_nil(Atom environment, Atom symbol) {
  Atom bind = nil;
  enum Error err = env_get(environment, symbol, &bind);
  if (err) {
    return 0;
  }
  return !nilp(bind);
}

Atom default_environment() {
  Atom environment = env_create(nil);
  env_set(environment, make_sym("T"),     make_sym("T"));
  env_set(environment, make_sym("!"),     make_builtin(builtin_not         , builtin_not_docstring));
  env_set(environment, make_sym("CAR"),   make_builtin(builtin_car         , builtin_car_docstring));
  env_set(environment, make_sym("CDR"),   make_builtin(builtin_cdr         , builtin_cdr_docstring));
  env_set(environment, make_sym("CONS"),  make_builtin(builtin_cons        , builtin_cons_docstring));
  env_set(environment, make_sym("+"),     make_builtin(builtin_add         , builtin_add_docstring));
  env_set(environment, make_sym("-"),     make_builtin(builtin_subtract    , builtin_subtract_docstring));
  env_set(environment, make_sym("*"),     make_builtin(builtin_multiply    , builtin_multiply_docstring));
  env_set(environment, make_sym("/"),     make_builtin(builtin_divide      , builtin_divide_docstring));
  env_set(environment, make_sym("="),     make_builtin(builtin_numeq       , builtin_numeq_docstring));
  env_set(environment, make_sym("!="),    make_builtin(builtin_numnoteq    , builtin_numnoteq_docstring));
  env_set(environment, make_sym("<"),     make_builtin(builtin_numlt       , builtin_numlt_docstring));
  env_set(environment, make_sym("<="),    make_builtin(builtin_numlt_or_eq , builtin_numlt_or_eq_docstring));
  env_set(environment, make_sym(">"),     make_builtin(builtin_numgt       , builtin_numgt_docstring));
  env_set(environment, make_sym(">="),    make_builtin(builtin_numgt_or_eq , builtin_numgt_or_eq_docstring));
  env_set(environment, make_sym("APPLY"), make_builtin(builtin_apply       , builtin_apply_docstring));
  env_set(environment, make_sym("PAIRP"), make_builtin(builtin_pairp       , builtin_pairp_docstring));
  env_set(environment, make_sym("EQ"),    make_builtin(builtin_eq          , builtin_eq_docstring));
  env_set(environment, make_sym("PRINT"), make_builtin(builtin_print       , builtin_print_docstring));
  // TODO: Add docstrings to debug flags.
  env_set(environment, make_sym("DEBUG/ENVIRONMENT"), nil);
  env_set(environment, make_sym("DEBUG/MACRO"), nil);
  return environment;
}
