#include <repl.h>

#include <environment.h>
#include <error.h>
#include <parser.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <types.h>

//================================================================ BEG evaluation

int apply(Atom function, Atom arguments, Atom *result);

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
      } else if (memcmp(operator.value.symbol, "DEFINE", 6) == 0) {
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
      } else if (memcmp(operator.value.symbol, "LAMBDA", 6) == 0) {
        if (nilp(arguments) || nilp(cdr(arguments))) {
          return ERROR_ARGUMENTS;
        }
        return make_closure(environment, car(arguments), cdr(arguments), result);
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

//================================================================ END evaluation

//================================================================ BEG builtins

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
  if (nilp (arguments) || nilp(cdr(arguments)) || !nilp(cdr(cdr(arguments)))) {
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
  if (nilp (arguments) || nilp(cdr(arguments)) || !nilp(cdr(cdr(arguments)))) {
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
  if (nilp (arguments) || nilp(cdr(arguments)) || !nilp(cdr(cdr(arguments)))) {
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
  if (nilp (arguments) || nilp(cdr(arguments)) || !nilp(cdr(cdr(arguments)))) {
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

//================================================================ END builtins

static const char *repl_prompt = "lite|> ";
static char user_input[MAX_INPUT_BUFSZ];

/// Returns a heap-allocated C string containing
/// a single line of input from the user.
char* readline() {
  char *line = NULL;
  unsigned line_length = 0;
  fputs(repl_prompt, stdout);
  fgets(user_input, MAX_INPUT_BUFSZ, stdin);
  line_length = strlen(user_input);
  line = malloc(line_length + 1);
  strcpy(line, user_input);
  line[line_length] = '\0';
  return line;
}

void enter_repl() {
  Atom environment = env_create(nil);
  env_set(environment, make_sym("CAR"), make_builtin(builtin_car));
  env_set(environment, make_sym("CDR"), make_builtin(builtin_cdr));
  env_set(environment, make_sym("CONS"), make_builtin(builtin_cons));
  env_set(environment, make_sym("+"), make_builtin(builtin_add));
  env_set(environment, make_sym("-"), make_builtin(builtin_subtract));
  env_set(environment, make_sym("*"), make_builtin(builtin_multiply));
  env_set(environment, make_sym("/"), make_builtin(builtin_divide));
  while (1) {
    //==== READ ====
    // Get current input as heap-allocated C string.
    char *input = readline();
    const char *source = input;
    // If any input starts with "quit", exit REPL.
    if (memcmp(source, "quit", 4) == 0) {
      break;
    }
    enum Error err;
    Atom expr;
    err = parse_expr(source, &source, &expr);
    if (err) {
      print_error(err);
      return;
    }
    //==== EVAL ====
    Atom result;
    err = evaluate_expr(expr, environment, &result);
    //==== PRINT ====
    switch (err) {
    case ERROR_NONE:
      print_atom(result);
      putchar('\n');
      break;
    default:
      print_error(err);
    }
    //==== LOOP ====
    // Free heap-allocated memory.
    free(input);
  }
}
