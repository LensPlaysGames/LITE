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
        // Ensure two arguments.
        if (nilp(arguments) || nilp(cdr(arguments)) || !nilp(cdr(cdr(arguments)))) {
          return ERROR_ARGUMENTS;
        }
        Atom value;
        Atom symbol = car(arguments);
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
      } else if (strcmp(operator.value.symbol, "MACRO") == 0) {
        if (nilp(arguments) || nilp(cdr(arguments))
            || nilp (cdr(cdr(arguments))) || !nilp(cdr(cdr(cdr(arguments)))))
          {
            return ERROR_ARGUMENTS;
          }
        Atom name = car(arguments);
        if (name.type != ATOM_TYPE_SYMBOL) {
          return ERROR_TYPE;
        }
        Atom macro;
        err = make_closure(environment, car(cdr(arguments)), cdr(cdr(arguments)), &macro);
        if (err) { return err; }
        macro.type = ATOM_TYPE_MACRO;
        *result = name;
        return env_set(environment, name, macro);
      } else if (strcmp(operator.value.symbol, "ENV") == 0) {
        // Ensure no arguments.
        if (!nilp(arguments)) {
          return ERROR_ARGUMENTS;
        }
        *result = environment;
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
    // Evaluate operator.
    err = evaluate_expr(operator, environment, &operator);
    if (err) { return err; }
    // Handle macros.
    if (operator.type == ATOM_TYPE_MACRO) {
      Atom expansion;
      operator.type = ATOM_TYPE_CLOSURE;
      err = apply(operator, arguments, &expansion);
      if (err) {
        printf("Error when evaluating macro operator!\n"
               "Operator: ");
        print_atom(operator);
        putchar('\n');
        return err;
      }
      // TODO: Use LISP environment value instead of C pre. directive.
      Atom debug_macro;
      env_get(environment, make_sym("DEBUG/MACRO"), &debug_macro);
      if (!nilp(debug_macro)) {
        // It's really helpful to be able to see
        // recursive macros expansion at each step.
        printf("Expansion: ");
        print_atom(expansion);
        printf("\n\n");
      }
      return evaluate_expr(expansion, environment, result);
    }
    // Evaluate arguments
    arguments = copy_list(arguments);
    arguments_it = arguments;
    while (!nilp(arguments_it)) {
      err = evaluate_expr(car(arguments_it), environment, &car(arguments_it));
      if (err) {
        printf("Error when evaluating argument!\n"
               "Argument: ");
        print_atom(arguments_it);
        putchar('\n');
        return err;
      }
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
