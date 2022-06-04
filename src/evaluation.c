#include <evaluation.h>

#include <ctype.h>
#include <environment.h>
#include <error.h>
#include <stdlib.h>
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
Error evaluate_next_expression(Atom *stack, Atom *expr, Atom *environment) {
  *environment = list_get(*stack, 1);
  Atom body = list_get(*stack, 5);
  *expr = car(body);
  body = cdr(body);
  if (nilp(body)) {
    *stack = car(*stack);
  } else {
    list_set(*stack, 5, body);
  }
  return ok;
}

Error evaluate_bind_arguments(Atom *stack, Atom *expr, Atom *environment) {
  Atom body = list_get(*stack, 5);
  // If there is an existing body, then simply evaluate the next expression within it.
  if (!nilp(body)) {
    return evaluate_next_expression(stack, expr, environment);
  }
  // Else, bind the arguments into the current stack frame.
  Atom operator = list_get(*stack, 2);
  Atom arguments = list_get(*stack, 4);
  *environment = env_create(car(operator));
  Atom argument_names = car(cdr(operator));
  body = cdr(cdr(operator));
  list_set(*stack, 1, *environment);
  list_set(*stack, 5, body);
  // Bind arguments into local environment.
  MAKE_ERROR(error_args, ERROR_ARGUMENTS, arguments, "Could not bind arguments.", NULL);
  while (!nilp(argument_names)) {
    // Handle variadic arguments.
    if (argument_names.type == ATOM_TYPE_SYMBOL) {
      env_set(*environment, argument_names, arguments);
      arguments = nil;
      break;
    }
    if (nilp(arguments)) {
      return error_args;
    }
    env_set(*environment, car(argument_names), car(arguments));
    argument_names = cdr(argument_names);
    arguments = cdr(arguments);
  }
  if (!nilp(arguments)) {
    return error_args;
  }
  list_set(*stack, 4, nil);
  return evaluate_next_expression(stack, expr, environment);
}

Error evaluate_apply(Atom *stack, Atom *expr, Atom *environment, Atom *result) {
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
        MAKE_ERROR(err, ERROR_SYNTAX, arguments, "Arguments must be a list.", NULL);
        return err;
      }
      list_set(*stack, 2, operator);
      list_set(*stack, 4, arguments);
    }
  }
  if (operator.type == ATOM_TYPE_BUILTIN) {
    *stack = car(*stack);
    *expr = cons(operator, arguments);
    return ok;
  }
  if (operator.type != ATOM_TYPE_CLOSURE) {
    MAKE_ERROR(err, ERROR_TYPE
               , operator
               , "APPLY: Expected operator type of #<BUILTIN> or #<CLOSURE>."
               , NULL);
    return err;
  }
  return evaluate_bind_arguments(stack, expr, environment);
}

/// EXPR is expected to be in form of '(OPERATOR . ARGUMENTS)'
Error evaluate_return_value(Atom *stack, Atom *expr, Atom *environment, Atom *result) {
  *environment = list_get(*stack, 1);
  Atom operator = list_get(*stack, 2);
  Atom body = list_get(*stack, 5);
  Atom arguments;
  if (!nilp(body)) {
    return evaluate_apply(stack, expr, environment, result);
  }
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
      Atom arguments = list_get(*stack, 4);
      Atom symbol = car(arguments);
      Atom docstring = cdr(arguments);
      if(!nilp(docstring)) {
        // FIXME: These docstrings are leaked.
        (*result).docstring = strdup(docstring.value.symbol);
      }
      env_set(*environment, symbol, *result);
      *stack = car(*stack);
      *expr = cons(make_sym("QUOTE"), cons(symbol, nil));
      return ok;
    } else if (strcmp(operator.value.symbol, "IF") == 0) {
      arguments = list_get(*stack, 3);
      *expr = nilp(*result) ? car(cdr(arguments)) : car(arguments);
      *stack = car(*stack);
      return ok;
    } else {
      // Store argument
      arguments = list_get(*stack, 4);
      list_set(*stack, 4, cons(*result, arguments));
    }
  } else if (operator.type == ATOM_TYPE_MACRO) {
    *expr = *result;
    *stack = car(*stack);
    return ok;
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
  return ok;
}

Error evaluate_expression(Atom expr, Atom environment, Atom *result) {
  MAKE_ERROR(err, ERROR_NONE, nil, NULL, NULL);
  Atom stack = nil;
  const static size_t gcol_count_default = 100000;
  static size_t gcol_count = gcol_count_default;
  do {
    if (!--gcol_count) {
      gcol_mark(expr);
      gcol_mark(environment);
      gcol_mark(stack);
      gcol_mark(*sym_table());
      gcol();
      if (env_non_nil(environment, make_sym("DEBUG/MEMORY"))) {
        printf("Garbage Collected\n");
        print_gcol_data();
      }
      size_t threshold;
      Atom threshold_atom = nil;
      err = env_get(environment, make_sym("GARBAGE-COLLECTOR-ITERATIONS-THRESHOLD"), &threshold_atom);
      if (err.type) { print_error(err); }
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
      err.type = ERROR_SYNTAX;
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
            PREP_ERROR(err, ERROR_ARGUMENTS
                       , arguments
                       , "QUOTE: Only a single argument may be passed."
                       , NULL);
            return err;
          }
          *result = car(arguments);
        } else if (strcmp(operator.value.symbol, "DEFINE") == 0) {
          const char *usage_define = "Usage: (DEFINE <symbol> <value> [docstring])";
          // Ensure at least two arguments.
          if (nilp(arguments) || nilp(cdr(arguments))) {
            PREP_ERROR(err, ERROR_ARGUMENTS
                       , arguments
                       , "DEFINE: Not enough arguments."
                       , usage_define);
            return err;
          }
          // Get docstring, if present.
          Atom doc = nil;
          symbol_t *docstring = NULL;
          if (!nilp(cdr(cdr(arguments)))) {
            // No more than three arguments.
            if (!nilp(cdr(cdr(cdr(arguments))))) {
              PREP_ERROR(err, ERROR_ARGUMENTS
                         , arguments
                         , "DEFINE: Too many arguments."
                         , usage_define);
              return err;
            }
            doc = car(cdr(cdr(arguments)));
            if (doc.type != ATOM_TYPE_STRING) {
              PREP_ERROR(err, ERROR_TYPE
                         , doc
                         , "DEFINE: Docstring must be a string!\n  Strings are text wrapped in double quotes."
                         , usage_define)
                return err;
            }
          }
          Atom symbol = car(arguments);
          if (symbol.type != ATOM_TYPE_SYMBOL) {
            PREP_ERROR(err, ERROR_TYPE
                       , symbol
                       , "DEFINE: The first argument must be a symbol."
                       , usage_define);
            return err;
          }
          // Create a new stack to evaluate definition of new symbol.
          stack = make_frame(stack, environment, nil);
          list_set(stack, 2, operator);
          list_set(stack, 4, cons(symbol, doc));
          expr = car(cdr(arguments));
          continue;
        } else if (strcmp(operator.value.symbol, "LAMBDA") == 0) {
          const char *usage_lambda = "Usage: (LAMBDA <argument> <body>...)";
          if (nilp(arguments) || nilp(cdr(arguments))) {
            PREP_ERROR(err, ERROR_ARGUMENTS
                       , arguments
                       , "LAMBDA: Not enough arguments."
                       , usage_lambda);
            return err;
          }
          err = make_closure(environment, car(arguments), cdr(arguments), result);
        } else if (strcmp(operator.value.symbol, "IF") == 0) {
          const char* usage_if = "Usage: (IF <condition> <then> <else>)";
          if (nilp(arguments) || nilp(cdr(arguments))
              || nilp(cdr(cdr(arguments)))
              || !nilp(cdr(cdr(cdr(arguments)))))
            {
              PREP_ERROR(err, ERROR_ARGUMENTS
                         , arguments
                         , "IF: Incorrect number of arguments."
                         , usage_if);
              return err;
            }
          stack = make_frame(stack, environment, cdr(arguments));
          list_set(stack, 2, operator);
          expr = car(arguments);
          continue;
        } else if (strcmp(operator.value.symbol, "MACRO") == 0) {
          // Arguments: MACRO_NAME ARGUMENTS DOCSTRING BODY
          const char* usage_macro = "Usage: (MACRO <symbol> <argument> <docstring> <body>...)";
          if (nilp(arguments) || nilp(cdr(arguments))
              || nilp (cdr(cdr(arguments))) || nilp(cdr(cdr(cdr(arguments))))
              || !nilp(cdr(cdr(cdr(cdr(arguments))))))
            {
              PREP_ERROR(err, ERROR_ARGUMENTS
                         , arguments
                         , "MACRO: Incorrect number of arguments."
                         , usage_macro);
              return err;
            }
          Atom name = car(arguments);
          if (name.type != ATOM_TYPE_SYMBOL) {
            PREP_ERROR(err, ERROR_TYPE
                       , name
                       , "MACRO: The first argument must be a symbol."
                       , usage_macro);
            return err;
          }
          Atom docstring = car(cdr(cdr(arguments)));
          if (docstring.type != ATOM_TYPE_STRING) {
            PREP_ERROR(err, ERROR_TYPE
                       , docstring
                       , "MACRO: Docstring must be a string!\n  Strings are text wrapped in double quotes."
                       , usage_macro);
            return err;
          }
          Atom macro;
          err = make_closure(environment
                             , car(cdr(arguments))
                             , cdr(cdr(cdr(arguments)))
                             , &macro);
          if (!err.type) {
            macro.type = ATOM_TYPE_MACRO;
            // FIXME: These docstrings are leaked!
            macro.docstring = strdup(docstring.value.symbol);
            (void)env_set(environment, name, macro);
            *result = name;
          }
        } else if (strcmp(operator.value.symbol, "DOCSTRING") == 0) {
          const char *usage_docstring = "(DOCSTRING <symbol>)";
          if (nilp(arguments) || !nilp(cdr(arguments))) {
            PREP_ERROR(err, ERROR_ARGUMENTS
                       , arguments
                       , "DOCSTRING: A single argument is expected."
                       , usage_docstring);
            return err;
          }
          Atom atom = car(arguments);
          if (atom.type == ATOM_TYPE_SYMBOL) {
            Atom symbol_in = atom;
            PREP_ERROR(err, ERROR_NONE, atom, "DOCSTRING: Error while evaluating symbol.", NULL);
            err = env_get(environment, symbol_in, &atom);
            if (err.type) { return err; }
          }
          // If atom is of type closure, show closure signature (arguments).
          // FIXME: The docstring could be set to this value instead of
          // creating this new string each time the docstring is fetched.
          char *docstring;
          if (atom.type == ATOM_TYPE_CLOSURE
              || atom.type == ATOM_TYPE_MACRO)
            {
              // Prepend docstring with closure signature.
              char *signature = atom_string(car(cdr(atom)), NULL);
              size_t siglen = 0;
              if (signature && (siglen = strlen(signature)) != 0) {
                if (atom.docstring) {
                  size_t newlen = strlen(atom.docstring) + siglen + 10;
                  char *newdoc = (char *)malloc(newlen);
                  if (newdoc) {
                    memcpy(newdoc, "ARGS: ", 6);
                    strcat(newdoc, signature);
                    free(signature);
                    strcat(newdoc, "\n\n");
                    strcat(newdoc, atom.docstring);
                    newdoc[newlen] = '\0';
                  } else {
                    // Could not allocate buffer for new docstring,
                    // free allocated memory and use regular docstring.
                    free(signature);
                    newdoc = strdup(atom.docstring);
                  }
                  docstring = newdoc;
                } else {
                  docstring = signature;
                }
              }
            } else {
            docstring = strdup(atom.docstring);
          }
          // FIXME: When docstring is not NULL, it is leaked.
          *result = docstring == NULL ? nil : make_string(docstring);
        } else if (strcmp(operator.value.symbol, "ENV") == 0) {
          // Ensure no arguments.
          // TODO: It would be cool to take a symbol argument
          // and return it's value in the environment.
          // We could probably achieve this rather quickly
          // by type checking then delegating to `env_get()`.
          if (!nilp(arguments)) {
            PREP_ERROR(err, ERROR_ARGUMENTS
                       , arguments
                       , "ENV: Arguments are not accepted."
                       , "Usage: (ENV)");
            return err;
          }
          *result = environment;
        } else if (strcmp(operator.value.symbol, "SYM") == 0) {
          // Ensure no arguments.
          if (!nilp(arguments)) {
            PREP_ERROR(err, ERROR_ARGUMENTS
                       , arguments
                       , "SYM: Arguments are not accepted."
                       , ("Usage: (SYM)"));
            return err;
          }
          *result = *sym_table();
        } else {
          // Evaluate operator before application.
          stack = make_frame(stack, environment, arguments);
          expr = operator;
          continue;
        }
      } else if (operator.type == ATOM_TYPE_BUILTIN) {
        PREP_ERROR(err, ERROR_NONE, operator, NULL, NULL);
        err.type = (*operator.value.builtin)(arguments, result);
      } else {
        // Evaluate operator before application.
        stack = make_frame(stack, environment, arguments);
        expr = operator;
        continue;
      }
    }
    if (nilp(stack)) {
      break;
    }
    if (!err.type) {
      err = evaluate_return_value(&stack, &expr, &environment, result);
    }
  } while (!err.type);
  return err;
}
