#include <ctype.h>
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


#ifdef LITE_GFX
char *allocate_string(const char *string) {
  if (!string) {
    return NULL;
  }
  size_t string_length = strlen(string);
  if (!string_length) {
    return NULL;
  }
  char *out = malloc(string_length + 1);
  if (!out) {
    return NULL;
  }
  strncpy(out, string, string_length);
  out[string_length] = '\0';
  return out;
}

void update_headline(GUIContext *ctx, char *new_headline) {
  if (!ctx || !new_headline) { return; }
  if (ctx->headline) {
    free(ctx->headline);
  }
  ctx->headline = new_headline;
}

void update_contents(GUIContext *ctx, char *new_contents) {
  if (!ctx || !new_contents) { return; }
  if (ctx->contents) {
    free(ctx->contents);
  }
  ctx->contents = new_contents;
}

void update_footline(GUIContext *ctx, char *new_footline) {
  if (!ctx || !new_footline) { return; }
  if (ctx->footline) {
    free(ctx->footline);
  }
  ctx->footline = new_footline;
}

#endif /* #ifdef LITE_GFX */

//================================================================ BEG api.c
#ifdef LITE_GFX
typedef struct GUIModifierKeyState {
  uint64_t bitfield; // Indexed by GUIModifierKey enum
} GUIModifierKeyState;

static GUIModifierKeyState gmodkeys;

// Return bit value of given modifier key. 1 if pressed.
int modkey_state(GUIModifierKey key);
int modkey_state(GUIModifierKey key) {
  switch (key) {
  default:
    return 0;
  case GUI_MODKEY_LALT:
    return ((uint64_t)1 << GUI_MODKEY_LALT) & gmodkeys.bitfield;
  case GUI_MODKEY_RALT:
    return ((uint64_t)1 << GUI_MODKEY_RALT) & gmodkeys.bitfield;
  case GUI_MODKEY_LCTRL:
    return ((uint64_t)1 << GUI_MODKEY_LCTRL) & gmodkeys.bitfield;
  case GUI_MODKEY_RCTRL:
    return ((uint64_t)1 << GUI_MODKEY_RCTRL) & gmodkeys.bitfield;
  case GUI_MODKEY_LSHIFT:
    return ((uint64_t)1 << GUI_MODKEY_LSHIFT) & gmodkeys.bitfield;
  case GUI_MODKEY_RSHIFT:
    return ((uint64_t)1 << GUI_MODKEY_RSHIFT) & gmodkeys.bitfield;
  }
}

// All these global variables may be bad :^|

Rope *ginput = NULL;

Atom *genv = NULL;

GUIContext *gctx = NULL;

void handle_character_dn(uint64_t c) {
  // TODO: Use LISP env. variables to determine a buffer
  // to insert into, at what offset (point/cursor), etc.
  // TODO: We maybe should gather-and-test to determine a key command?
  const char *ignored_bytes = "\e\f\v";
  if (strchr(ignored_bytes, (unsigned char)c)) {
    return;
  }
  // TODO: Figure out how input could be handled better.
  if (ginput) {
    if (c == '\r' || c == '\n') {
      // TODO: Parse input through LITE, display output (in footline for now?).
      char *input = rope_string(NULL, ginput, NULL);
      if (!input) { return; }
      rope_free(ginput);
      ginput = rope_create("");
      const char *source = input;
      size_t source_len = strlen(source);
      if (source_len >= 4 && memcmp(source, "quit", 4) == 0) {
        int debug_memory = env_non_nil(genv ? *genv : default_environment()
                                       , make_sym("DEBUG/MEMORY"));
        // Garbage collection with no marking means free everything.
        gcol();
        if (debug_memory) {
          print_gcol_data();
        }
        destroy_gui();
        exit(0);
        return;
      }
      // PARSE
      Atom expr;
      const char *expected_end = source + source_len;
      Error err = parse_expr(source, &source, &expr);
      if (err.type) {
        printf("\nPARSER ");
        print_error(err);
        return;
      }
      const char *ws = " \t\n";
      size_t span = 0;
      while ((span = strspn(source, ws))) {
        source += span;
      }
      if (source < expected_end) {
        printf("ERROR: Too much input: %s\n", source);
        return;
      }
      // EVALUATE
      Atom result;
      err = evaluate_expression
        (expr
         , genv ? *genv : default_environment()
         , &result
         );
      // DISPLAY
      switch (err.type) {
      case ERROR_NONE:
        update_footline(gctx, atom_string(result, NULL));
        break;
      default:
        // FIXME: This is awful!
        // We need `error_string()`!!
        update_footline(gctx, allocate_string("ERROR!"));
        printf("\nEVALUATION ");
        print_error(err);
        printf("Faulting Expression: ");
        print_atom(expr);
        putchar('\n');
        break;
      }
      // LOOP
      free(input);

    } else if (c == '\b') {
      rope_remove_from_end(ginput, 1);
    } else if (c == '\a') {
      // TODO: It would be cool to have `gui.h` include a `visual_beep()` and `audio_beep()`.
    } else if (c == '\t') {
      // TODO: Handle tabs (LISP environment variable to deal with spaces conversion).
    } else {
      if (modkey_state(GUI_MODKEY_LSHIFT) || modkey_state(GUI_MODKEY_RSHIFT)) {
        if (genv) {
          Atom shift_remap = nil;
          env_get(*genv, make_sym("SHIFT-CONVERSION-ALIST"), &shift_remap);
          if (!nilp(shift_remap)) {
            // TODO/FIXME: This falsely assumes one-byte character.
            char *tmp_str = malloc(2);
            tmp_str[0] = (char)c;
            tmp_str[1] = '\0';
            Atom remapping = alist_get(shift_remap, make_string(tmp_str));
            free(tmp_str);
            if (stringp(remapping)) {
              if (remapping.value.symbol && strlen(remapping.value.symbol)) {
                c = remapping.value.symbol[0];
              }
            }
          }
        }
      }
      if (modkey_state(GUI_MODKEY_LCTRL) || modkey_state(GUI_MODKEY_RCTRL)) {
        // TODO: Look in a keymap for command? Or maybe
        // begin building command input string somewhere?
      } else {
        // FIXME: This falsely assumes one-byte content.
        rope_append_byte(ginput, (char)c);
      }
    }
  }
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
    break;
  case GUI_MODKEY_LCTRL:
    gmodkeys.bitfield |= ((uint64_t)1 << GUI_MODKEY_LCTRL);
    break;
  case GUI_MODKEY_RCTRL:
    gmodkeys.bitfield |= ((uint64_t)1 << GUI_MODKEY_RCTRL);
    break;
  case GUI_MODKEY_LALT:
    gmodkeys.bitfield |= ((uint64_t)1 << GUI_MODKEY_LALT);
    break;
  case GUI_MODKEY_RALT:
    gmodkeys.bitfield |= ((uint64_t)1 << GUI_MODKEY_RALT);
    break;
  case GUI_MODKEY_LSHIFT:
    gmodkeys.bitfield |= ((uint64_t)1 << GUI_MODKEY_LSHIFT);
    break;
  case GUI_MODKEY_RSHIFT:
    gmodkeys.bitfield |= ((uint64_t)1 << GUI_MODKEY_RSHIFT);
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
    break;
  case GUI_MODKEY_LCTRL:
    gmodkeys.bitfield &= ~((uint64_t)1 << GUI_MODKEY_LCTRL);
    break;
  case GUI_MODKEY_RCTRL:
    gmodkeys.bitfield &= ~((uint64_t)1 << GUI_MODKEY_RCTRL);
    break;
  case GUI_MODKEY_LALT:
    gmodkeys.bitfield &= ~((uint64_t)1 << GUI_MODKEY_LALT);
    break;
  case GUI_MODKEY_RALT:
    gmodkeys.bitfield &= ~((uint64_t)1 << GUI_MODKEY_RALT);
    break;
  case GUI_MODKEY_LSHIFT:
    gmodkeys.bitfield &= ~((uint64_t)1 << GUI_MODKEY_LSHIFT);
    break;
  case GUI_MODKEY_RSHIFT:
    gmodkeys.bitfield &= ~((uint64_t)1 << GUI_MODKEY_RSHIFT);
    break;
  }
}
#endif /* #ifdef LITE_GFX */
//================================================================ END api.c

#if defined (_WIN32) || defined (_WIN64)
#  include <windows.h>
#  define SLEEP(ms) Sleep(ms)
#elif defined (__unix)
#  ifdef _POSIX_C_SOURCE
#    define _POSIX_C_SOURCE_BACKUP _POSIX_C_SOURCE
#    undef _POSIX_C_SOURCE
#  endif
#  define _POSIX_C_SOURCE 199309L
#  include <time.h>
#  define SLEEP(ms) do {                        \
    struct timespec ts;                         \
    ts.tv_sec = ms / 1000;                      \
    ts.tv_nsec = ms % 1000 * 1000;              \
    nanosleep(&ts, NULL);                       \
  } while (0)
#  undef _POSIX_C_SOURCE
#  ifdef _POSIX_C_SOURCE_BACKUP
#    define _POSIX_C_SOURCE _POSIX_C_SOURCE_BACKUP
#    undef _POSIX_C_SOURCE_BACKUP
#  endif
#else
#  error "System unknown! Can not create SLEEP macro."
#endif

#ifdef LITE_GFX
GUIContext *initialize_lite_gui_ctx() {
  GUIContext *ctx = malloc(sizeof(GUIContext));
  if (!ctx) { return NULL; }
  memset(ctx, 0, sizeof(GUIContext));
  ctx->title = "LITE GFX";
  update_headline(ctx, allocate_string("LITE Headline"));
  update_contents(ctx, allocate_string("LITE Contents"));
  update_footline(ctx, allocate_string("LITE Footline"));
  if (!ctx->headline || !ctx->contents || !ctx->footline) {
    return NULL;
  }
  return ctx;
}

int enter_lite_gui() {
  int open;
  if ((open = create_gui())) {
    return open;
  }
  open = 1;
  gctx = initialize_lite_gui_ctx();
  if (!gctx) {
    return 2;
  }
  Rope *input = rope_create("");
  if (!input) { return 1; }
  ginput = input;
  while (open) {
    // TODO: Use LISP environment variable to get current buffer,
    // then convert that rope into a string and display it.
    update_contents(gctx, rope_string(NULL, ginput, NULL));
    do_gui(&open, gctx);
    // Only update GUI every 10 milliseconds.
    SLEEP(10);
  }
  destroy_gui();
  return 0;
}
#endif

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
  genv = &environment;
  enter_lite_gui();
#else
  enter_repl(environment);
#endif

  int debug_memory = env_non_nil(environment, make_sym("DEBUG/MEMORY"));
  // Garbage collection with no marking means free everything.
  gcol();
  if (debug_memory) {
    print_gcol_data();
  }

  return 0;
}
