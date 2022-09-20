#include <evaluation.h>

#include <assert.h>
#include <ctype.h>
#include <environment.h>
#include <error.h>
#include <stdlib.h>
#include <string.h>
#include <types.h>
#include <utility.h>

/* STACK-FRAME: (
 *   PARENT
 *   ENVIRONMENT
 *   EVALUATED-OPERATOR
 *   (PENDING-ARGUMENTS...)
 *   (EVALUATED-ARGUMENTS...)
 *   (BODY...)
 *   )
 */
HOTFUNCTION
Atom make_frame(Atom parent, Atom environment, Atom tail) {
  return cons(parent,
              cons(environment,
                   cons(nil,
                        cons(tail,
                             cons(nil,
                                  cons(nil, nil))))));
}

void print_stackframe(Atom stack, int depth) {
  if (nilp(stack)) { return; }
  int d = depth;
  while (--d >= 0) { putchar(' '); }
  printf("operator: ");
  print_atom(list_get(stack, 2));
  printf(", pre-args: ");
  print_atom(list_get(stack, 3));
  printf(", args: ");
  print_atom(list_get(stack, 4));
  putchar('\n');
  print_stackframe(car(stack), depth + 2);
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
    if (symbolp(argument_names)) {
      env_set(*environment, argument_names, arguments);
      arguments = nil;
      break;
    }
    if (nilp(arguments)) {
      error_args.suggestion = "Not enough arguments passed.";
      return error_args;
    }
    env_set(*environment, car(argument_names), car(arguments));
    argument_names = cdr(argument_names);
    arguments = cdr(arguments);
  }
  if (!nilp(arguments)) {
    error_args.suggestion = "Too many arguments passed.";
    return error_args;
  }
  list_set(*stack, 4, nil);
  return evaluate_next_expression(stack, expr, environment);
}

Error evaluate_apply(Atom *stack, Atom *expr, Atom *environment) {
  Atom operator = list_get(*stack, 2);
  Atom arguments = list_get(*stack, 4);
  if (!nilp(arguments)) {
    list_reverse(&arguments);
    list_set(*stack, 4, arguments);
  }
  if (symbolp(operator)) {
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
    } else if (strcmp(operator.value.symbol, "WHILE-BODY") == 0) {
      Atom body = list_get(*stack, 5);
      assert(!nilp(body) && "WHILE-BODY: evaluate_apply should not be called when stack body is NIL!");
      *environment = list_get(*stack, 1);
      *expr = car(body);
      body = cdr(body);
      list_set(*stack, 5, body);
      return ok;
    } else if (strcmp(operator.value.symbol, "PROGN") == 0) {
      Atom body = list_get(*stack, 5);
      // If there is an existing body, then simply evaluate the next expression within it.
      assert(!nilp(body) && "PROGN: evaluate_apply should not be called when stack body is NIL!");
      *environment = list_get(*stack, 1);
      *expr = car(body);
      body = cdr(body);
      list_set(*stack, 5, body);
      return ok;
    }
  }
  if (builtinp(operator)) {
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
  Error err = ok;
  *environment = list_get(*stack, 1);
  Atom operator = list_get(*stack, 2);
  Atom body = list_get(*stack, 5);
  Atom arguments;
  if (!nilp(body)) {
    return evaluate_apply(stack, expr, environment);
  }
  if (nilp(operator)) {
    // Operator has been evaluated.
    operator = *result;
    list_set(*stack, 2, operator);
    if (macrop(operator)) {
      arguments = list_get(*stack, 3);
      *stack = make_frame(*stack, *environment, nil);
      operator.type = ATOM_TYPE_CLOSURE;
      list_set(*stack, 2, operator);
      list_set(*stack, 4, arguments);
      if (env_non_nil(*environment, make_sym("DEBUG/MACRO"))) {
        printf("Evaluating macro: (");
        print_atom(*expr);
        putchar(' ');
        print_atom(list_get(*stack, 4));
        printf(")\n");
      }
      return evaluate_bind_arguments(stack, expr, environment);
    }
  } else if (symbolp(operator)) {
    char define_locality = -1;
    if ((define_locality = strcmp(operator.value.symbol, "DEFINE")) == 0
        || (define_locality = !strcmp(operator.value.symbol, "SET")) != 0) {
      // Here is where env_set is called, since
      // arguments have now been evaluated.
      Atom arguments = list_get(*stack, 4);
      Atom symbol = car(arguments);
      Atom docstring = cdr(arguments);
      if(stringp(docstring)) {
        result->docstring = strdup(docstring.value.symbol);
        gcol_generic_allocation(result, result->docstring);
      }
      Atom *define_environment = define_locality == 0 ? environment : genv();
      err = env_set(*define_environment, symbol, *result);
      if (err.type) { return err; }
      *stack = car(*stack);
      *expr = cons(make_sym("QUOTE"), cons(symbol, nil));
      return ok;
    } else if (strcmp(operator.value.symbol, "IF") == 0) {
      arguments = list_get(*stack, 3);
      // `result` determines what to evaluate next ("then", or "else" branch).
      *expr = nilp(*result) ? car(cdr(arguments)) : car(arguments);
      // Continue execution, we've handled the IF entirely.
      *stack = car(*stack);
      return ok;
    } else if (strcmp(operator.value.symbol, "PROGN") == 0) {
      // Pop PROGN stack, we have finished evaluating it.
      Atom new_stack = car(*stack);
      if (!nilp(new_stack)) {
        // Update stack environment. This is needed to allow
        // for definitions in the body to affect outside of it.
        list_set(new_stack, 1, list_get(*stack, 1));
      }
      *stack = new_stack;
      *expr = *result; // Return result of last expression of body.
      return ok;
    } else if (strcmp(operator.value.symbol, "EVALUATE") == 0) {
      // Pop EVALUATE stack, we have finished evaluating it.
      Atom new_stack = car(*stack);
      if (!nilp(new_stack)) {
        // Update stack environment. This is needed to allow
        // for definitions in the body to affect outside of it.
        list_set(new_stack, 1, list_get(*stack, 1));
      }
      *stack = new_stack;
      *expr = *result; // Return result of expression.
      return ok;
    } else if (strcmp(operator.value.symbol, "WHILE") == 0) {
      arguments = list_get(*stack, 3);
      int debug_while = env_non_nil(*environment, make_sym("DEBUG/WHILE"));

      // Store recurse count in evaluated arguments list.
      Atom recurse_count = list_get(*stack, 4);
      if (!integerp(recurse_count)) {
        recurse_count = make_int(0);
      } else {
        recurse_count.value.integer += 1;
      }
      Atom recurse_maximum = nil;
      env_get(*environment, make_sym("WHILE-RECURSE-LIMIT"), &recurse_maximum);
      if (!integerp(recurse_maximum)) { recurse_maximum = make_int(10000); }
      list_set(*stack, 4, recurse_count);

      if (debug_while) {
        printf("WHILE: recurse count is ");
        print_atom(recurse_count);
        printf(" (max ");
        print_atom(recurse_maximum);
        printf(")\n");
        printf("  condition: ");
        print_atom(car(arguments));
        putchar('\n');
        printf("  result:    ");
        print_atom(*result);
        putchar('\n');
        if (recurse_count.value.integer == 0) {
          printf("  body:      ");
          print_atom(cdr(arguments));
          putchar('\n');
        }
      }
      // At this point, result contains condition return value.
      // If result is nil, or maximum recursion limit has been reached, exit the loop.
      if (nilp(*result) || recurse_count.value.integer >= recurse_maximum.value.integer) {
        if (debug_while) { printf("  Loop ending.\n"); }
        *stack = car(*stack);
        return ok;
      }
      if (debug_while) { printf("  Loop continuing.\n"); }
      *stack = make_frame(*stack, *environment, nil);
      list_set(*stack, 2, make_sym("WHILE-BODY"));
      list_set(*stack, 5, cdr(arguments));
      return evaluate_next_expression(stack, expr, environment);
    } else if (strcmp(operator.value.symbol, "WHILE-BODY") == 0) {
      // Pop WHILE-BODY stack, we have finished evaluating it.
      Atom new_stack = car(*stack);
      // Update WHILE stack environment. This is needed to allow
      // for definitions in the body to affect the conditional.
      list_set(new_stack, 1, list_get(*stack, 1));
      *stack = new_stack;
      *expr = *result;
      // Re-evaluate condition before continuing.
      *expr = car(list_get(*stack, 3));
      return ok;
    } else {
      // Store arguments.
      arguments = list_get(*stack, 4);
      list_set(*stack, 4, cons(*result, arguments));
    }
  } else if (macrop(operator)) {
    *expr = *result;
    if (env_non_nil(*environment, make_sym("DEBUG/MACRO"))) {
      printf("          result: ");
      print_atom(*result);
      putchar('\n');
    }
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
    return evaluate_apply(stack, expr, environment);
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
  // These numbers are tailored to free around twenty mebibytes at a time,
  // and to have both of the reasons for garbage collection actually used.
# define gcol_pair_allocations_threshold_default     290500
# define gcol_evaluation_iteration_threshold_default 100000
  static size_t evaluation_iterations_until_gcol = gcol_evaluation_iteration_threshold_default;
  do {
    char should_gcol = 0;
    if (!--evaluation_iterations_until_gcol) { should_gcol = 1; }
    // Check pair allocations count every 100 evaluation iterations.
    if (!should_gcol && evaluation_iterations_until_gcol % 100 == 0) {
      Atom pair_allocations_threshold = nil;
      err = env_get(environment, make_sym("GARBAGE-COLLECTOR-PAIR-ALLOCATIONS-THRESHOLD"),
                    &pair_allocations_threshold);
      if (err.type) { err = ok; }
      if (!integerp(pair_allocations_threshold) || pair_allocations_threshold.value.integer <= 0) {
        pair_allocations_threshold = make_int(gcol_pair_allocations_threshold_default);
      }
      integer_t pairs_in_use = pair_allocations_count - pair_allocations_freed;
      if (pairs_in_use >= pair_allocations_threshold.value.integer) {
        should_gcol = 1;
      }
    }
    if (should_gcol) {
      size_t pair_allocations_freed_before = pair_allocations_freed;
      size_t generic_allocations_freed_before = generic_allocations_freed;
      Atom debug_memory = nil;
      env_get(environment, make_sym("DEBUG/MEMORY"), &debug_memory);
      if (!nilp(debug_memory)) {
        printf("=====\nCollecting Garbage: ");
        if (evaluation_iterations_until_gcol) {
          printf("pair allocations threshold reached\n");
        } else {
          printf("evaluation iterations threshold reached\n");
        }
        print_gcol_data();
        printf("VVVVV\n");
      }
      size_t iterations_threshold = gcol_evaluation_iteration_threshold_default;
      Atom threshold_atom = nil;
      err = env_get(environment, make_sym("GARBAGE-COLLECTOR-EVALUATION-ITERATIONS-THRESHOLD"), &threshold_atom);
      if (err.type) { print_error(err); err = ok; }
      else if (integerp(threshold_atom) && threshold_atom.value.integer > 0) {
        iterations_threshold = (size_t)threshold_atom.value.integer;
      }
      evaluation_iterations_until_gcol = iterations_threshold;
      gcol_mark(&expr);
      gcol_mark(genv());
      gcol_mark(&environment);
      gcol_mark(&stack);
      gcol_mark(sym_table());
      gcol_mark(buf_table());
      gcol();
      if (!nilp(debug_memory)) {
        size_t pair_allocations_freed_this_iteration =
          pair_allocations_freed - pair_allocations_freed_before;
        size_t generic_allocations_freed_this_iteration =
          generic_allocations_freed - generic_allocations_freed_before;
        size_t pair_allocations_bytes_freed =
          pair_allocations_freed_this_iteration * sizeof(ConsAllocation);
        size_t generic_allocations_bytes_freed =
          generic_allocations_freed_this_iteration * sizeof(GenericAllocation);
        printf("Garbage Collected\n");
        print_gcol_data();
        printf("This iteration:\n"
               "|-- %zu pairs freed (%zu bytes)\n"
               "|-- %zu generics freed (%zu bytes)\n"
               "`-- %zu bytes freed\n",
               pair_allocations_freed_this_iteration,
               pair_allocations_bytes_freed,
               generic_allocations_freed_this_iteration,
               generic_allocations_bytes_freed,
               pair_allocations_bytes_freed + generic_allocations_bytes_freed);
        printf("=====\n");
      }
    }
    if (symbolp(expr)) {
      err = env_get(environment, expr, result);
    } else if (expr.type != ATOM_TYPE_PAIR) {
      *result = expr;
    } else if (!listp(expr)) {
      PREP_ERROR(err, ERROR_SYNTAX, expr,
                 "Can not evaluate expression unless it is in list form.",
                 NULL);
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
      if (symbolp(operator)) {
        // Special forms
        if (strcmp(operator.value.symbol, "QUOTE") == 0) {
          if (nilp(arguments) || !nilp(cdr(arguments))) {
            PREP_ERROR(err, ERROR_ARGUMENTS
                       , arguments
                       , "QUOTE: Only a single argument may be passed."
                       , NULL);
            return err;
          }
          *result = car(arguments);
        } else if (strcmp(operator.value.symbol, "DEFINE") == 0
                   || strcmp(operator.value.symbol, "SET") == 0) {
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
          // Evaluate condition of IF.
          // Result handled in `evaluate_return_value()`
          continue;
        } else if (strcmp(operator.value.symbol, "WHILE") == 0) {
          const char* usage_while = "Usage: (WHILE <condition> <body>)";
          if (nilp(arguments) || nilp(cdr(arguments))) {
            PREP_ERROR(err, ERROR_ARGUMENTS
                       , arguments
                       , "WHILE: Not enough arguments!"
                       , usage_while);
            return err;
          }
          if (env_non_nil(environment, make_sym("DEBUG/WHILE"))) {
            printf("WHILE: First encounter of ");
            print_atom(cons(operator, arguments));
            putchar('\n');
          }
          stack = make_frame(stack, environment, arguments);
          // Stack operator set to WHILE.
          list_set(stack, 2, operator);
          // Evaluate condition of WHILE loop.
          // Result handled in `evaluate_return_value()`
          expr = car(arguments);
          continue;
        } else if (strcmp(operator.value.symbol, "PROGN") == 0) {
          if (nilp(arguments)) {
            *result = nil;
          } else {
            stack = make_frame(stack, environment, arguments);
            // Stack operator set to PROGN.
            list_set(stack, 2, operator);
            // Stack body set to PROGN body.
            list_set(stack, 5, arguments);
            // Rest handled in `evaluate_return_value()` and `evaluate_apply()`.
          }
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
          if (err.type) { return err; }
          macro.type = ATOM_TYPE_MACRO;
          macro.docstring = strdup(docstring.value.symbol);
          gcol_generic_allocation(&macro, macro.docstring);
          (void)env_set(environment, name, macro);
          *result = name;
        } else if (strcmp(operator.value.symbol, "EVAL-SIMPLE") == 0) {
          const char *usage_evaluate = "Usage: (EVAL-SIMPLE <expression>)";
          if (nilp(arguments) || !nilp(cdr(arguments))) {
            PREP_ERROR(err, ERROR_ARGUMENTS
                       , arguments
                       , "EVAL-SIMPLE: Only a single expression is accepted."
                       , usage_evaluate);
            return err;
          }
          expr = car(arguments);
          continue;
        } else if (strcmp(operator.value.symbol, "EVALUATE") == 0) {
          const char *usage_evaluate = "Usage: (EVALUATE <expression>)";
          if (nilp(arguments) || !nilp(cdr(arguments))) {
            PREP_ERROR(err, ERROR_ARGUMENTS
                       , arguments
                       , "EVALUATE: Only a single expression is accepted."
                       , usage_evaluate);
            return err;
          }
          stack = make_frame(stack, environment, nil);
          list_set(stack, 2, operator);
          list_set(stack, 4, arguments);
          // Evaluate expression, result handled in evaluate_return_value().
          expr = car(arguments);
          continue;
        } else if (strcmp(operator.value.symbol, "ENV") == 0) {
          const char *usage_env = "Usage: (ENV)";
          // FIXME: Why does this crash the program???
          if (!nilp(arguments)) {
            PREP_ERROR(err, ERROR_ARGUMENTS
                       , arguments
                       , "ENV: Too many arguments! Zero arguments are accepted."
                       , usage_env);
            return err;
          }
          *result = environment;
        } else if (strcmp(operator.value.symbol, "ERROR") == 0) {
          const char *usage_error = "Usage: (ERROR \"message\")";
          if (nilp(arguments) || !nilp(cdr(arguments))) {
            PREP_ERROR(err, ERROR_ARGUMENTS
                       , arguments
                       , "ERROR: Only a single string argument is accepted."
                       , usage_error);
            return err;
          }
          Atom message = car(arguments);
          if (!stringp(message)) {
            PREP_ERROR(err, ERROR_TYPE
                       , arguments
                       , "ERROR: Only a single *string* argument is accepted."
                       , usage_error);
            return err;
          }
          printf("LISP ERROR: %s\n", message.value.symbol);
          *result = make_string(message.value.symbol);
          break;
        } else {
          // Evaluate operator before application.
          stack = make_frame(stack, environment, arguments);
          expr = operator;
          continue;
        }
      } else if (builtinp(operator)) {
        PREP_ERROR(err, ERROR_NONE, operator, NULL, NULL);
        err.type = (*operator.value.builtin.function)(arguments, result);
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
