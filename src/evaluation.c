#include <evaluation.h>

#include <ctype.h>
#include <environment.h>
#include <error.h>
#include <string.h>
#include <types.h>

int evaluate_expr(Atom expr, Atom environment, Atom *result) {
  Atom operator;
  Atom arguments;
  Atom arguments_it;
  enum Error err = ERROR_NONE;
  if (expr.type == ATOM_TYPE_SYMBOL) {
    err = env_get(environment, expr, result);
  } else if (expr.type != ATOM_TYPE_PAIR) {
    *result = expr;
  } else if (!listp(expr)) {
    err = ERROR_SYNTAX;
  } else {
    operator = car(expr);
    arguments = cdr(expr);
    if (operator.type == ATOM_TYPE_SYMBOL) {
      if (strcmp(operator.value.symbol, "QUOTE") == 0) {
        if (nilp(arguments) || !nilp(cdr(arguments))) {
          return ERROR_ARGUMENTS;
        }
        *result = car(arguments);
        return ERROR_NONE;
      } else if (strcmp(operator.value.symbol, "DEFINE") == 0) {
        Atom symbol;
        Atom value;
        // Ensure two arguments.
        if (nilp(arguments) || nilp(cdr(arguments)) || !nilp(cdr(cdr(arguments)))) {
          return ERROR_ARGUMENTS;
        }
        symbol = car(arguments);
        if (symbol.type != ATOM_TYPE_SYMBOL) {
          return ERROR_TYPE;
        }
        err = evaluate_expr(car(cdr(arguments)), environment, &value);
        if (err) { return err; }
        *result = symbol;
        return env_set(environment, symbol, value);
      } else if (strcmp(operator.value.symbol, "LAMBDA") == 0) {
        if (nilp(arguments) || nilp(cdr(arguments))) {
          return ERROR_ARGUMENTS;
        }
        return make_closure(environment, car(arguments), cdr(arguments), result);
      } else if (strlen(operator.value.symbol) >= 3
                 && toupper(operator.value.symbol[0]) == 'S'
                 && toupper(operator.value.symbol[1]) == 'Y'
                 && toupper(operator.value.symbol[2]) == 'M')
        {
          if (!nilp(arguments)) {
            return ERROR_ARGUMENTS;
          }
          *result = *sym_table();
          return ERROR_NONE;
        } else if (strlen(operator.value.symbol) >= 2
                   && toupper(operator.value.symbol[0]) == 'I'
                   && toupper(operator.value.symbol[1]) == 'F')
        {
          if (nilp(arguments) || nilp(cdr(arguments))
              || nilp(cdr(cdr(arguments))) || !nilp(cdr(cdr(cdr(arguments)))))
            {
              return ERROR_ARGUMENTS;
            }
          Atom condition;
          Atom value;
          err = evaluate_expr(car(arguments), environment, &condition);
          if (err) { return err; }
          value = nilp(condition) ? car(cdr(cdr(arguments))) : car(cdr(arguments));
          return evaluate_expr(value, environment, result);
        }
    }
    err = evaluate_expr(operator, environment, &operator);
    if (err) { return err; }
    arguments = copy_list(arguments);
    arguments_it = arguments;
    while (!nilp(arguments_it)) {
      err = evaluate_expr(car(arguments_it), environment, &car(arguments_it));
      if (err) { return err; }
      arguments_it = cdr(arguments_it);
    }
    return apply(operator, arguments, result);
  }
  return err;
}

int apply(Atom function, Atom arguments, Atom *result) {
  if (function.type == ATOM_TYPE_BUILTIN) {
    return (*function.value.builtin)(arguments, result);
  } else if (function.type != ATOM_TYPE_CLOSURE) {
    return ERROR_TYPE;
  }
  // Handle closure.
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
    enum Error err = evaluate_expr(car(body), environment, result);
    if (err) { return err; }
    body = cdr(body);
  }
  return ERROR_NONE;
}
