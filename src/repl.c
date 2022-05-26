#include <repl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *repl_prompt = "lite|> ";
static char user_input[MAX_INPUT_BUFSZ];

/// Returns a heap-allocated C string containing
/// a single line of input from the user.
char* readline() {
  fputs(repl_prompt, stdout);
  fgets(user_input, MAX_INPUT_BUFSZ, stdin);
  unsigned line_length = strlen(user_input);
  char *line = malloc(line_length + 1);
  strcpy(line, user_input);
  line[line_length] = '\0';
  return line;
}

void enter_repl() {
  while (1) {
    //==== READ ====
    // Get current input as heap-allocated C string.
    char *line = readline();
    //==== EVAL ====
    // If any line starts with "quit", exit REPL.
    if (memcmp(line, "quit", 4) == 0) {
      break;
    }
    //==== PRINT ====
    // For now, just loop back input.
    printf("%s\n", line);
    //==== LOOP ====
    // Free heap-allocated memory.
    free(line);
  }
}
