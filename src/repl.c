#include <repl.h>

#include <environment.h>
#include <error.h>
#include <parser.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <types.h>

//================================================================ BEG evaluation

int evaluate_expr(Atom expr, Atom environment, Atom *result) {
  Atom operator;
  Atom arguments;
  Atom arguments_it;
  enum Error err;
  if (expr.type == ATOM_TYPE_SYMBOL) {
    return env_get(environment, expr, result);
  } else if (expr.type != ATOM_TYPE_PAIR) {
    *result = expr;
    return ERROR_NONE;
  }
  if (!listp(expr)) {
    return ERROR_SYNTAX;
  }
  operator = car(expr);
  arguments = cdr(expr);
  if (operator.type == ATOM_TYPE_SYMBOL) {
    if (memcmp(operator.value.symbol, "QUOTE", 5) == 0) {
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

//================================================================ END evaluation

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
