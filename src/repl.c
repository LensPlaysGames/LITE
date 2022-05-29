#include <repl.h>

#include <error.h>
#include <evaluation.h>
#include <parser.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <types.h>

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

void enter_repl(Atom environment) {
  while (1) {
    putchar('\n');
    //==== READ ====
    // Get current input as heap-allocated C string.
    char *input = readline();
    const char *source = input;
    if (strlen(source) >= 4
        && memcmp(source, "quit", 4) == 0)
      {
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
