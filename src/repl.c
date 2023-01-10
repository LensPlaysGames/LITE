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
char* readline(char *prompt) {
  char *line = NULL;
  size_t line_length = 0;
  fputs(prompt, stdout);
  fgets(user_input, MAX_INPUT_BUFSZ, stdin);
  line_length = strlen(user_input);
  line = malloc(line_length + 1);
  if (!line) { return NULL; }
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
      print_symbol_table();
      putchar('\n');
    }
    //==== READ ====
    putchar('\n');
    // Get current input as heap-allocated C string.
    char *input = readline((char *)repl_prompt);
    const char *source = input;
    if (strlen(source) >= 4
        && memcmp(source, "quit", 4) == 0)
      {
        break;
      }
    Atom expr;
    const char *expected_end = source + strlen(source);
    Error err = parse_expr(source, &source, &expr);
    if (err.type) {
      printf("\nPARSER ");
      print_error(err);
      continue;
    }
    // Skip trailing whitespace for end comparison.
    const char *ws = " \t\n";
    size_t span = 0;
    while ((span = strspn(source, ws))) {
      source += span;
    }
    if (source < expected_end) {
      printf("ERROR: Too much input: %s\n", source);
      continue;
    }
    //==== EVAL ====
    Atom result;
    err = evaluate_expression(expr, environment, &result);

    // TODO: Should user/quit actually close REPL? Or just stop all
    // evaluation up until the REPL is re-entered?

    if (user_quit) {
      fprintf(stderr, "\nUSER/QUIT::exiting REPL.");
      break;
    }

    //==== PRINT ====
    if (err.type) {
      printf("\nEVALUATION ");
      print_error(err);
      printf("Faulting Expression: ");
      print_atom(expr);
      putchar('\n');
    } else {
      print_atom(result);
      putchar('\n');
    }

    //==== LOOP ====
    // Free heap-allocated memory.
    free(input);
  }
}
