#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <environment.h>
#include <evaluation.h>
#include <error.h>
#include <parser.h>
#include <repl.h>
#include <rope.h>
#include <types.h>

#ifdef LITE_GFX
#include <api.h>
#include <gui.h>
#endif

//================================================================ BEG file_io
size_t file_size(FILE *file) {
  if (!file) {
    return 0;
  }
  fseek(file, 0, SEEK_END);
  size_t length = ftell(file);
  fseek(file, 0, SEEK_SET);
  return length;
}

/// Returns a heap-allocated buffer containing the
/// contents of the file found at the given path.
char *file_contents(const char* path) {
  char *buffer = NULL;
  FILE *file = fopen(path, "r");
  if (!file) {
    printf("Couldn't open file at %s\n", path);
    return NULL;
  }
  size_t size = file_size(file);
  if (size == 0) {
    printf("File has zero size at %s\n", path);
    return NULL;
  }
  buffer = malloc(size + 1);
  if (!buffer) {
    printf("Could not allocate buffer for file at %s\n", path);
    return NULL;
  }
  fread(buffer, 1, size, file);
  buffer[size] = 0;
  fclose(file);
  return buffer;
}

Error load_file(Atom environment, const char* path) {
  char *input = file_contents(path);
  Error err;
  if (!input) {
    PREP_ERROR(err, ERROR_ARGUMENTS, nil
               , "Could not get contents of file."
               , path)
    return err;
  }
  const char* source = input;
  Atom expr;
  while (parse_expr(source, &source, &expr).type == ERROR_NONE) {
    Atom result;
    err = evaluate_expression(expr, environment, &result);
    if (err.type) { return err; }
    print_atom(result);
    putchar('\n');
  }
  free(input);
  return ok;
}

//================================================================ END file_io


//================================================================ BEG api.c
#ifdef LITE_GFX
void handle_character_dn(uint64_t c) {
  printf("Got character input: %c\n", (char)c);
  // TODO: Use LISP env. variables to determine a buffer to insert into,
  // at what offset (point/cursor), etc.
  // TODO: Handle modifier key input state being true.
  // For example, while holding ctrl, characters should not be entered.
  // Eventually, we should gather-and-test to determine a key command.
}

void handle_character_up(uint64_t c) {
  // We may never actually need to handle a regular character up...
  (void)c;
}

void handle_modifier_dn(GUIModifierKey mod) {
  if (mod >= GUI_MODKEY_MAX) {
    return;
  }
  switch (mod) {
  default:
    printf("API::GFX:ERROR: Unhandled modifier keydown: %d\n"
           "              : Please report as issue on LITE GitHub.\n"
           , mod);
  case GUI_MODKEY_LCTRL:
  case GUI_MODKEY_RCTRL:
    // TODO: Global input key state CTRL modifier -> true
    printf("Ctrl down.\n");
    break;
  case GUI_MODKEY_LALT:
  case GUI_MODKEY_RALT:
    // TODO: Global input key state ALT modifier -> true
    printf("Alt down.\n");
    break;
  case GUI_MODKEY_LSHIFT:
  case GUI_MODKEY_RSHIFT:
    // TODO: Global input key state SHIFT modifier -> true
    printf("Shift down.\n");
    break;
  }
}

void handle_modifier_up(GUIModifierKey mod) {
  if (mod >= GUI_MODKEY_MAX) {
    return;
  }
  switch (mod) {
  default:
    printf("API::GFX:ERROR: Unhandled modifier keyup: %d\n"
           "              : Please report as issue on LITE GitHub.\n"
           , mod);
  case GUI_MODKEY_LCTRL:
  case GUI_MODKEY_RCTRL:
    // TODO: Global input key state CTRL modifier -> true
    printf("Ctrl up.\n");
    break;
  case GUI_MODKEY_LALT:
  case GUI_MODKEY_RALT:
    // TODO: Global input key state ALT modifier -> true
    printf("Alt up.\n");
    break;
  case GUI_MODKEY_LSHIFT:
  case GUI_MODKEY_RSHIFT:
    // TODO: Global input key state SHIFT modifier -> true
    printf("Shift up.\n");
    break;
  }
}
#endif /* #ifdef LITE_GFX */
//================================================================ END api.c


int main(int argc, char **argv) {
  printf("LITE will guide the way through the darkness.\n");

  Atom environment = default_environment();
  // Treat every given argument as a file to load, for now.
  if (argc > 1) {
    for (size_t i = 1; i < argc; ++i) {
      load_file(environment, argv[i]);
    }
  }

#ifdef LITE_GFX
  create_gui();
  int open = 1;
  GUIContext ctx;
  ctx.headline = "LITE Headline";
  ctx.contents = "LITE";
  ctx.footline = "LITE Footline";
  while (open) {
    do_gui(&open, &ctx);
  }
  destroy_gui();
#else
  enter_repl(environment);
#endif /* #ifdef LITE_GFX */

  int debug_memory = env_non_nil(environment, make_sym("DEBUG/MEMORY"));
  // Garbage collection with no marking means free everything.
  gcol();
  if (debug_memory) {
    print_gcol_data();
  }

  return 0;
}
