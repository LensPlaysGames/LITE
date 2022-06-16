#include <builtins.h>

#include <error.h>
#include <environment.h>
#include <evaluation.h>
#include <string.h>
#include <types.h>

symbol_t *builtin_not_docstring =
  "Given ARG is nil, return 'T', otherwise return nil.";
int builtin_not(Atom arguments, Atom *result) {
  if (nilp (arguments) || !nilp(cdr(arguments))) {
    return ERROR_ARGUMENTS;
  }
  *result = nilp(car(arguments)) ? make_sym("T") : nil;
  return ERROR_NONE;
}

symbol_t *builtin_car_docstring =
  "Given ARG is a pair, return the value on the left side.";
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

symbol_t *builtin_cdr_docstring =
  "Given ARG is a pair, return the value on the right side.";
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

symbol_t *builtin_cons_docstring =
  "Return a new pair, with LEFT and RIGHT on each side, respectively.";
int builtin_cons(Atom arguments, Atom *result) {
  if (nilp(arguments) || nilp(cdr(arguments)) || !nilp(cdr(cdr(arguments)))) {
    return ERROR_ARGUMENTS;
  }
  *result = cons(car(arguments), car(cdr(arguments)));
  return ERROR_NONE;
}

symbol_t *builtin_add_docstring =
  "Add two integer numbers A and B together, and return the computed result.";
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

symbol_t *builtin_subtract_docstring =
  "Subtract integer B from integer A and return the computed result.";
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

symbol_t *builtin_multiply_docstring =
  "Multiply integer numbers A and B together and return the computed result.";
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

symbol_t *builtin_divide_docstring =
  "Divide integer B out of integer A and return the computed result.";
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

symbol_t *builtin_numeq_docstring =
  "Return 'T' iff the two given arguments have the same integer value.";
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

symbol_t *builtin_numnoteq_docstring =
  "Return 'T' iff the two given arguments *do not* have the same integer value.";
int builtin_numnoteq(Atom arguments, Atom *result) {
  if (nilp (arguments) || nilp(cdr(arguments)) || !nilp(cdr(cdr(arguments)))) {
    return ERROR_ARGUMENTS;
  }
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (lhs.type != ATOM_TYPE_INTEGER || rhs.type != ATOM_TYPE_INTEGER) {
    return ERROR_TYPE;
  }
  *result = lhs.value.integer != rhs.value.integer ? make_sym("T") : nil;
  return ERROR_NONE;
}

symbol_t *builtin_numlt_docstring =
  "Return 'T' iff integer A is less than integer B.";
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

symbol_t *builtin_numlt_or_eq_docstring =
  "Return 'T' iff integer A is less than or equal to integer B.";
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

symbol_t *builtin_numgt_docstring =
  "Return 'T' iff integer A is greater than integer B.";
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

symbol_t *builtin_numgt_or_eq_docstring =
  "Return 'T' iff integer A is greater than or equal to integer B.";
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

symbol_t *builtin_apply_docstring =
  "Call FUNCTION with the given ARGUMENTS and return the result.";
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
  if (function.type == ATOM_TYPE_BUILTIN) {
    return (*function.value.builtin)(arguments, result);
  } else if (function.type != ATOM_TYPE_CLOSURE) {
    printf("APPLY: Given function is not a BuiltIn or a closure.\n");
    return ERROR_TYPE;
  }
  // Handle closure.
  // Layout: FUNCTION ARGUMENTS . BODY
  Atom environment = env_create(car(function));
  Atom argument_names = car(cdr(function));
  Atom body = cdr(cdr(function));
  // Bind arguments into local environment.
  while (!nilp(argument_names)) {
    // Handle variadic arguments.
    if (argument_names.type == ATOM_TYPE_SYMBOL) {
      env_set(environment, argument_names, arguments);
      arguments = nil;
      break;
    }
    if (nilp(arguments)) {
      return ERROR_ARGUMENTS;
    }
    env_set(environment, car(argument_names), car(arguments));
    argument_names = cdr(argument_names);
    arguments = cdr(arguments);
  }
  if (!nilp(arguments)) {
    return ERROR_ARGUMENTS;
  }
  // Evaluate body of closure.
  while (!nilp(body)) {
    Error err = evaluate_expression(car(body), environment, result);
    if (err.type) { return err.type; }
    body = cdr(body);
  }
  return ERROR_NONE;
}

symbol_t *builtin_pairp_docstring =
  "Return 'T' iff ARG has a type of 'PAIR', otherwise return nil.";
int builtin_pairp(Atom arguments, Atom *result) {
  if (nilp(arguments) || !nilp(cdr(arguments))) {
    return ERROR_ARGUMENTS;
  }
  *result = car(arguments).type == ATOM_TYPE_PAIR ? make_sym("T") : nil;
  return ERROR_NONE;
}

symbol_t *builtin_eq_docstring =
  "Return 'T' iff A and B refer to the same Atomic LISP object.";
int builtin_eq(Atom arguments, Atom *result) {
  if (nilp(arguments) || nilp(cdr(arguments)) || !nilp(cdr(cdr(arguments)))) {
    return ERROR_ARGUMENTS;
  }
  Atom a = car(arguments);
  Atom b = car(cdr(arguments));
  int equal = 0;
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
      equal = (strcmp(a.value.symbol, b.value.symbol) == 0);
      break;
    case ATOM_TYPE_INTEGER:
      equal = (a.value.integer == b.value.integer);
      break;
    case ATOM_TYPE_BUILTIN:
      equal = (a.value.builtin == b.value.builtin);
      break;
    default:
      equal = 0;
      break;
    }
  }
  *result = equal ? make_sym("T") : nil;
  return ERROR_NONE;
}

symbol_t *builtin_symbol_table_docstring =
  "Return the LISP symbol table.";
int builtin_symbol_table(Atom arguments, Atom *result) {
  if (!nilp(arguments)) {
    return ERROR_ARGUMENTS;
  }
  *result = *sym_table();
  return ERROR_NONE;
}

symbol_t *builtin_print_docstring =
  "Print the given ARG to standard out, prettily.";
int builtin_print(Atom arguments, Atom *result) {
  if (nilp(arguments) || !nilp(cdr(arguments))) {
    return ERROR_ARGUMENTS;
  }
  pretty_print_atom(car(arguments));
  putchar('\n');
  *result = nil;
  return ERROR_NONE;
}
