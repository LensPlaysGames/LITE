#include <evaluation.h>

#include <ctype.h>
#include <environment.h>
#include <error.h>
#include <string.h>
#include <types.h>

/* STACK-FRAME: (
 *   PARENT ENVIRONMENT
 *   EVALUATED-OPERATOR
 *   (PENDING-ARGUMENTS...)
 *   (EVALUATED-ARGUMENTS...)
 *   (BODY...)
 *   )
 */
Atom make_frame(Atom parent, Atom environment, Atom tail) {
  return cons(parent,
              cons(environment,
                   cons(nil,
                        cons(tail,
                             cons(nil,
                                  cons(nil,
                                       nil))))));
}

/// Set EXPR to next expression in body of STACK.
int evaluate_next_expression(Atom *stack, Atom *expr, Atom *environment) {
  *environment = list_get(*stack, 1);
  Atom body = list_get(*stack, 5);
  *expr = car(body);
  body = cdr(body);
  if (nilp(body)) {
    *stack = car(*stack);
  } else {
    list_set(*stack, 5, body);
  }
  return ERROR_NONE;
}

int evaluate_bind_arguments(Atom *stack, Atom *expr, Atom *environment) {
  Atom operator;
  Atom arguments;
  Atom argument_names;
  Atom body = list_get(*stack, 5);
  // If there is an existing body, then simply evaluate the next expression within it.
  if (!nilp(body)) {
    return evaluate_next_expression(stack, expr, environment);
  }
  // Else, bind the arguments into the current stack frame.
  operator = list_get(*stack, 2);
  arguments = list_get(*stack, 4);
  *environment = env_create(car(operator));
  argument_names = car(cdr(operator));
  body = cdr(cdr(operator));
  list_set(*stack, 1, *environment);
  list_set(*stack, 5, body);
  // Bind arguments into local environment.
  while (!nilp(argument_names)) {
    // Handle variadic arguments.
    if (argument_names.type == ATOM_TYPE_SYMBOL) {
      env_set(*environment, argument_names, arguments);
      arguments = nil;
      break;
    }
    if (nilp(arguments)) {
      return ERROR_ARGUMENTS;
    }
    env_set(*environment, car(argument_names), car(arguments));
    argument_names = cdr(argument_names);
    arguments = cdr(arguments);
  }
  if (!nilp(arguments)) {
    return ERROR_ARGUMENTS;
  }
  list_set(*stack, 4, nil);
  return evaluate_next_expression(stack, expr, environment);
}

int evaluate_apply(Atom *stack, Atom *expr, Atom *environment, Atom *result) {
  Atom operator = list_get(*stack, 2);
  Atom arguments = list_get(*stack, 4);
  if (!nilp(arguments)) {
    list_reverse(&arguments);
    list_set(*stack, 4, arguments);
  }
  if (operator.type == ATOM_TYPE_SYMBOL) {
    if (strcmp(operator.value.symbol, "APPLY") == 0) {
      // Replace current frame with new frame.
      *stack = car(*stack);
      *stack = make_frame(*stack, *environment, nil);
      operator = car(arguments);
      arguments = car(cdr(arguments));
      if (!listp(arguments)) {
        return ERROR_SYNTAX;
      }
      list_set(*stack, 2, operator);
      list_set(*stack, 4, arguments);
    }
  }
  if (operator.type == ATOM_TYPE_BUILTIN) {
    *stack = car(*stack);
    *expr = cons(operator, arguments);
    return ERROR_NONE;
  }
  if (operator.type != ATOM_TYPE_CLOSURE) {
    printf("APPLY: Expected operator type of #<BUILTIN> or #<CLOSURE>.\n");
    printf("  Operator: ");
    print_atom(operator);
    putchar('\n');
    return ERROR_TYPE;
  }
  return evaluate_bind_arguments(stack, expr, environment);
}

/// EXPR is expected to be in form of '(OPERATOR . ARGUMENTS)'
int evaluate_return_value(Atom *stack, Atom *expr, Atom *environment, Atom *result) {
  *environment = list_get(*stack, 1);
  Atom operator = list_get(*stack, 2);
  Atom body = list_get(*stack, 5);
  Atom arguments;
  if (!nilp(body)) {
    return evaluate_apply(stack, expr, environment, result);
  }
  // FIXME: It seems like this is wrong.
  if (nilp(operator)) {
    // Operator has been evaluated.
    operator = *result;
    list_set(*stack, 2, operator);
    if (operator.type == ATOM_TYPE_MACRO) {
      arguments = list_get(*stack, 3);
      *stack = make_frame(*stack, *environment, nil);
      operator.type = ATOM_TYPE_CLOSURE;
      list_set(*stack, 2, operator);
      list_set(*stack, 4, arguments);
      return evaluate_bind_arguments(stack, expr, environment);
    }
  } else if (operator.type == ATOM_TYPE_SYMBOL) {
    if (strcmp(operator.value.symbol, "DEFINE") == 0) {
      // Here is where env_set is called, since
      // arguments have now been evaluated.
      Atom symbol = list_get(*stack, 4);
      env_set(*environment, symbol, *result);
      *stack = car(*stack);
      *expr = cons(make_sym("QUOTE"), cons(symbol, nil));
      return ERROR_NONE;
    } else if (strcmp(operator.value.symbol, "IF") == 0) {
      arguments = list_get(*stack, 3);
      *expr = nilp(*result) ? car(cdr(arguments)) : car(arguments);
      *stack = car(*stack);
      return ERROR_NONE;
    } else {
      // Store argument
      arguments = list_get(*stack, 4);
      list_set(*stack, 4, cons(*result, arguments));
    }
  } else if (operator.type == ATOM_TYPE_MACRO) {
    *expr = *result;
    *stack = car(*stack);
    return ERROR_NONE;
  } else {
    // Store argument
    arguments = list_get(*stack, 4);
    list_set(*stack, 4, cons(*result, arguments));
  }
  arguments = list_get(*stack, 3);
  if (nilp(arguments)) {
    // No arguments left to evaluate, apply operator.
    return evaluate_apply(stack, expr, environment, result);
  }
  // Evaluate next argument.
  *expr = car(arguments);
  // Eat next argument from pre-evaluated arguments list.
  list_set(*stack, 3, cdr(arguments));
  return ERROR_NONE;
}

int evaluate_expression(Atom expr, Atom environment, Atom *result) {
  enum Error err = ERROR_NONE;
  Atom stack = nil;
  const static size_t gcol_count_default = 1000;
  static size_t gcol_count = gcol_count_default;
  do {
    if (!--gcol_count) {
      gcol_mark(expr);
      gcol_mark(environment);
      gcol_mark(stack);
      gcol();
      size_t threshold;
      Atom threshold_atom;
      env_get(environment, make_sym("GARBAGE-COLLECTOR-ITERATIONS-THRESHOLD"), &threshold_atom);
      if (nilp(threshold_atom) || !integerp(threshold_atom)) {
        threshold = gcol_count_default;
      } else {
        threshold = threshold_atom.value.integer;
      }
      gcol_count = threshold;
    }
    if (expr.type == ATOM_TYPE_SYMBOL) {
      err = env_get(environment, expr, result);
    } else if (expr.type != ATOM_TYPE_PAIR) {
      *result = expr;
    } else if (!listp(expr)) {
      err = ERROR_SYNTAX;
    } else {
      Atom operator = car(expr);
      Atom arguments = cdr(expr);
      if (env_non_nil(environment, make_sym("DEBUG/EVALUATE"))) {
        printf("Evaluating expression: ");
        print_atom(expr);
        putchar('\n');
        printf("  Operator: ");
        print_atom(operator);
        putchar('\n');
        printf("  Arguments: ");
        print_atom(arguments);
        putchar('\n');
      }
      if (operator.type == ATOM_TYPE_SYMBOL) {
        if (strcmp(operator.value.symbol, "QUOTE") == 0) {
          if (nilp(arguments) || !nilp(cdr(arguments))) {
            return ERROR_ARGUMENTS;
          }
          *result = car(arguments);
        } else if (strcmp(operator.value.symbol, "DEFINE") == 0) {
          // Ensure at least two arguments.
          if (nilp(arguments) || nilp(cdr(arguments))) {
            printf("DEFINE: Not enough arguments\n");
            return ERROR_ARGUMENTS;
          }
          // Get docstring, if present.
          symbol_t *docstring = NULL;
          if (!nilp(cdr(cdr(arguments)))) {
            // No more than three arguments.
            if (!nilp(cdr(cdr(cdr(arguments))))) {
              printf("DEFINE: Too many arguments\n");
              return ERROR_ARGUMENTS;
            }
            Atom doc = car(cdr(cdr(arguments)));
            if (doc.type != ATOM_TYPE_STRING) {
              printf("DEFINE: Invalid docstring.\n");
              return ERROR_TYPE;
            }
            docstring = doc.value.symbol;
          }
          Atom value;
          Atom symbol = car(arguments);
          if (symbol.type != ATOM_TYPE_SYMBOL) {
            printf("DEFINE: Can not define what is not a symbol!\n");
            return ERROR_TYPE;
          }
          // Create a new stack to evaluate definition of new symbol.
          stack = make_frame(stack, environment, nil);
          list_set(stack, 2, operator);
          list_set(stack, 4, symbol);
          expr = car(cdr(arguments));
          continue;
        } else if (strcmp(operator.value.symbol, "LAMBDA") == 0) {
          // ARGUMENTS: LAMBDA_ARGS BODY
          if (nilp(arguments) || nilp(cdr(arguments))) {
            return ERROR_ARGUMENTS;
          }
          err = make_closure(environment, car(arguments), cdr(arguments), result);
        } else if (strcmp(operator.value.symbol, "IF") == 0) {
          if (nilp(arguments) || nilp(cdr(arguments))
              || nilp(cdr(cdr(arguments)))
              || !nilp(cdr(cdr(cdr(arguments)))))
            {
              return ERROR_ARGUMENTS;
            }
          stack = make_frame(stack, environment, cdr(arguments));
          list_set(stack, 2, operator);
          expr = car(arguments);
          continue;
        } else if (strcmp(operator.value.symbol, "MACRO") == 0) {
          // Arguments: MACRO_NAME ARGUMENTS DOCSTRING BODY
          if (nilp(arguments) || nilp(cdr(arguments))
              || nilp (cdr(cdr(arguments))) || nilp(cdr(cdr(cdr(arguments))))
              || !nilp(cdr(cdr(cdr(cdr(arguments))))))
            {
              // Hopefully this helps.
              printf("MACRO DEFINITION: (MACRO MACRO-NAME (ARGUMENT ...) \"DOCSTRING\" BODY)\n");
              return ERROR_ARGUMENTS;
            }
          Atom name = car(arguments);
          if (name.type != ATOM_TYPE_SYMBOL) {
            printf("MACRO DEFINITION: Invalid macro name (not a symbol).");
            return ERROR_TYPE;
          }
          Atom docstring = car(cdr(cdr(arguments)));
          if (docstring.type != ATOM_TYPE_STRING) {
            printf("MACRO DEFINITION: Invalid docstring (not a string).");
            return ERROR_TYPE;
          }
          Atom macro;
          err = make_closure(environment
                             , car(cdr(arguments))
                             , cdr(cdr(cdr(arguments)))
                             , &macro);
          
          if (!err) {
            macro.type = ATOM_TYPE_MACRO;
            macro.docstring = docstring.value.symbol;
            (void)env_set(environment, name, macro);
            *result = name;
          }
        } else if (strcmp(operator.value.symbol, "DOCSTRING") == 0) {
          if (nilp(arguments) || !nilp(cdr(arguments))) {
            return ERROR_ARGUMENTS;
          }
          Atom atom = car(arguments);
          if (atom.type == ATOM_TYPE_NIL
              || atom.type == ATOM_TYPE_INTEGER
              || atom.type == ATOM_TYPE_STRING
              || atom.type == ATOM_TYPE_PAIR)
            {
              return ERROR_TYPE;
            }
          if (atom.type == ATOM_TYPE_SYMBOL) {
            Atom symbol_in = atom;
            err = env_get(environment, symbol_in, &atom);
            if (err) { return err; }
          }
          *result = atom.docstring == NULL ? nil : make_string(atom.docstring);
        } else if (strcmp(operator.value.symbol, "ENV") == 0) {
          // Ensure no arguments.
          if (!nilp(arguments)) {
            return ERROR_ARGUMENTS;
          }
          *result = environment;
        } else if (strcmp(operator.value.symbol, "SYM") == 0) {
          // Ensure no arguments.
          if (!nilp(arguments)) {
            return ERROR_ARGUMENTS;
          }
          *result = *sym_table();
        } else {
          // Push
          // Handle function application
          stack = make_frame(stack, environment, arguments);
          expr = operator;
          continue;
        }
      } else if (operator.type == ATOM_TYPE_BUILTIN) {
        err = (*operator.value.builtin)(arguments, result);
      } else {
        // Push
        // Handle function application
        stack = make_frame(stack, environment, arguments);
        expr = operator;
        continue;
      }
    }
    if (nilp(stack)) {
      break;
    }
    if (!err) {
      err = evaluate_return_value(&stack, &expr, &environment, result);
    }
  } while (!err);
  return err;
}
