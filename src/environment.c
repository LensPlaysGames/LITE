#include <environment.h>

#include <builtins.h>
#include <error.h>
#include <types.h>

Atom env_create(Atom parent) {
  return cons(parent, nil);
}

Error env_set(Atom environment, Atom symbol, Atom value) {
  Atom bindings = cdr(environment);
  while (!nilp(bindings)) {
    Atom bind = car(bindings);
    if (car(bind).value.symbol == symbol.value.symbol) {
      cdr(bind) = value;
      return ok;
    }
    bindings = cdr(bindings);
  }
  bindings = cons(symbol, value);
  cdr(environment) = cons(bindings, cdr(environment));
  return ok;
}

Error env_get(Atom environment, Atom symbol, Atom *result) {
  Atom parent = car(environment);
  Atom bindings = cdr(environment);
  while (!nilp(bindings)) {
    Atom bind = car(bindings);
    if (car(bind).value.symbol == symbol.value.symbol) {
      *result = cdr(bind);
      return ok;
    }
    bindings = cdr(bindings);
  }
  if (nilp(parent)) {
    MAKE_ERROR(err, ERROR_NOT_BOUND
               , symbol
               // FIXME: Which environment?
               , "Symbol is not bound in environment."
               , NULL);
    return err;
  }
  return env_get(parent, symbol, result);
}

int env_non_nil(Atom environment, Atom symbol) {
  Atom bind = nil;
  Error err = env_get(environment, symbol, &bind);
  if (err.type) {
    return 0;
  }
  return !nilp(bind);
}

int boundp(Atom environment, Atom symbol) {
  Atom bind = nil;
  return !(env_get(environment, symbol, &bind).type);
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
  env_set(environment, make_sym("DEBUG/EVALUATE"), nil);
  env_set(environment, make_sym("DEBUG/MACRO"), nil);
  return environment;
}
