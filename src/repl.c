#include <repl.h>

#include <error.h>
#include <environment.h>
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
    if (env_non_nil(environment, make_sym("DEBUG/ENVIRONMENT"))) {
      printf("Environment:\n");
      pretty_print_atom(environment);
      putchar('\n');
      printf("Symbol Table:\n");
      print_atom(*sym_table());
      putchar('\n');
    }
    //==== READ ====
    putchar('\n');
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
      printf("Error during parsing!\n");
      print_error(err);
      continue;
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
      printf("Error during evaluation!\n");
      print_error(err);
      printf("Faulting Expression: ");
      print_atom(expr);
      putchar('\n');
      break;
    }
    //==== LOOP ====
    // Free heap-allocated memory.
    free(input);
  }
}
