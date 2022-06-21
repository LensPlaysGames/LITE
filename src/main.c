#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <buffer.h>
#include <environment.h>
#include <evaluation.h>
#include <error.h>
#include <file_io.h>
#include <parser.h>
#include <repl.h>
#include <rope.h>
#include <types.h>

#ifdef LITE_GFX
#include <api.h>
#include <gui.h>
#endif

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

// Currently factored out due to REPL-like behaviour.
// Eventually this will be handled the same as all other characters.
void handle_newline() {
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
    free_buffer_table();
    if (debug_memory) {
      print_gcol_data();
    }
    destroy_gui();
    exit(0);
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
}

void reverse_memcpy(void *restrict destination, void *restrict source, size_t byte_count) {
  char *dst = destination;
  char *src = source;
  for (size_t i = 0; i < byte_count; ++i) {
    dst[byte_count - 1 - i] = src[i];
  }
}

void handle_character_dn(uint64_t c) {
  // TODO: Use LISP env. variables to determine a buffer
  // to insert into, at what offset (point/cursor), etc.
  const char *ignored_bytes = "\e\f\v";
  if (strchr(ignored_bytes, (unsigned char)c)) {
    return;
  }

  int debug_keybinding = env_non_nil(*genv, make_sym("DEBUG/KEYBINDING"));
  if (debug_keybinding) {
    if (c > 255) {
      printf("Keydown: %llu\n", c);
    } else {
      printf("Keydown: %c\n", (char)c);
    }
  }

  // TODO: Figure out how input could be handled better.
  if (ginput) {
    if (c == '\r' || c == '\n') {
      handle_newline();
    } else if (c == '\b') {
      rope_remove_from_end(ginput, 1);
    } else if (c == '\a') {
      // TODO: It would be cool to have `gui.h` include a `visual_beep()` and `audio_beep()`.
    } else if (c == '\t') {
      // TODO: Handle tabs (LISP environment variable to deal with spaces conversion).
    } else {
      // If no special handling is required, it's up to the keymap!
      if (genv) {
        const size_t keybind_recurse_limit = 256;
        size_t keybind_recurse_count = 0;
        // Get current keymap from LISP environment.
        Atom current_keymap = nil;
        env_get(*genv, make_sym("CURRENT-KEYMAP"), &current_keymap);
        if (nilp(current_keymap)) {
          env_get(*genv, make_sym("KEYMAP"), &current_keymap);
        }
        if (debug_keybinding) {
          printf("Current keymap: ");
          pretty_print_atom(current_keymap);
          putchar('\n');
        }

        // HANDLE MODIFIER KEYMAPS

        if (modkey_state(GUI_MODKEY_LCTRL)) {
          Atom lctrl_bind = alist_get(current_keymap, make_string("LEFT-CONTROL"));
          // Allow string-rebinding of ctrl to another character.
          // Mostly, this is used to rebind to 'CTRL', the generic L/R ctrl keymap.
          if (stringp(lctrl_bind)) {
            lctrl_bind = alist_get(current_keymap, lctrl_bind);
            keybind_recurse_count += 1;
          }
          if (alistp(lctrl_bind)) {
            current_keymap = lctrl_bind;
          } else {
            // TODO: What keybind is undefined???
            // I guess we need to keep track of how we got here.
            // This could be done by saving keys of each keybind.
            update_footline(gctx, allocate_string("Undefined keybinding!"));
            // Discard character if no ctrl keybind was found.
            return;
          }
          env_set(*genv, make_sym("CURRENT-KEYMAP"), current_keymap);
        } else if (modkey_state(GUI_MODKEY_RCTRL)) {
          Atom rctrl_bind = alist_get(current_keymap, make_string("RIGHT-CONTROL"));
          if (stringp(rctrl_bind)) {
            rctrl_bind = alist_get(current_keymap, rctrl_bind);
            keybind_recurse_count += 1;
          }
          if (alistp(rctrl_bind)) {
            current_keymap = rctrl_bind;
          } else {
            // TODO: What keybind is undefined???
            update_footline(gctx, allocate_string("Undefined keybinding!"));
            // Discard character if no ctrl keybind was found.
            return;
          }
          env_set(*genv, make_sym("CURRENT-KEYMAP"), current_keymap);
        }

        if (modkey_state(GUI_MODKEY_LALT)) {
          Atom lalt_bind = alist_get(current_keymap, make_string("LEFT-ALT"));
          // Allow string-rebinding of ctrl to another character.
          // Mostly, this is used to rebind to 'CTRL', the generic L/R ctrl keymap.
          if (stringp(lalt_bind)) {
            lalt_bind = alist_get(current_keymap, lalt_bind);
            keybind_recurse_count += 1;
          }
          if (alistp(lalt_bind)) {
            current_keymap = lalt_bind;
          } else {
            // TODO: What keybind is undefined???
            update_footline(gctx, allocate_string("Undefined keybinding!"));
            // Discard character if no alt keybind was found.
            return;
          }
          env_set(*genv, make_sym("CURRENT-KEYMAP"), current_keymap);
        } else if (modkey_state(GUI_MODKEY_RALT)) {
          Atom ralt_bind = alist_get(current_keymap, make_string("RIGHT-ALT"));
          if (stringp(ralt_bind)) {
            ralt_bind = alist_get(current_keymap, ralt_bind);
            keybind_recurse_count += 1;
          }
          if (alistp(ralt_bind)) {
            current_keymap = ralt_bind;
          } else {
            update_footline(gctx, allocate_string("Undefined keybinding!"));
            // Discard character if no keybind was found.
            return;
          }
          env_set(*genv, make_sym("CURRENT-KEYMAP"), current_keymap);
        }

        if (modkey_state(GUI_MODKEY_LSHIFT)) {
          Atom lshift_bind = alist_get(current_keymap, make_string("LEFT-SHIFT"));
          // Allow string-rebinding of shift to another character.
          // Mostly, this is used to rebind to 'SHFT', the generic L/R shift keymap.
          if (stringp(lshift_bind)) {
            lshift_bind = alist_get(current_keymap, lshift_bind);
            keybind_recurse_count += 1;
          }
          if (alistp(lshift_bind)) {
            current_keymap = lshift_bind;
            env_set(*genv, make_sym("CURRENT-KEYMAP"), current_keymap);
          }
        } else if (modkey_state(GUI_MODKEY_RSHIFT)) {
          Atom rshift_bind = alist_get(current_keymap, make_string("RIGHT-SHIFT"));
          if (stringp(rshift_bind)) {
            rshift_bind = alist_get(current_keymap, rshift_bind);
            keybind_recurse_count += 1;
          }
          if (alistp(rshift_bind)) {
            current_keymap = rshift_bind;
            env_set(*genv, make_sym("CURRENT-KEYMAP"), current_keymap);
          }
        }

        while (c && keybind_recurse_count < keybind_recurse_limit) {
          env_get(*genv, make_sym("CURRENT-KEYMAP"), &current_keymap);
          if (debug_keybinding) {
            printf("Current keymap: ");
            pretty_print_atom(current_keymap);
            putchar('\n');
          }

          const size_t tmp_str_sz = 2;
          char *tmp_str = malloc(tmp_str_sz);
          memset(tmp_str, '\0', tmp_str_sz);
          // TODO+FIXME: This falsely assumes one-byte character.
          tmp_str[0] = (char)c;
          Atom keybind = alist_get(current_keymap, make_string(tmp_str));
          free(tmp_str);

          if (debug_keybinding) {
            printf("Got keybind: ");
            pretty_print_atom(keybind);
            putchar('\n');
          }

          if (alistp(keybind)) {
            // Nested keymap, rebind current keymap.
            if (debug_keybinding) {
              printf("Nested keymap found, updating CURRENT-KEYMAP\n");
            }
            env_set(*genv, make_sym("CURRENT-KEYMAP"), keybind);
            c = 0;
          } else {
            Atom root_keymap = nil;
            env_get(*genv, make_sym("KEYMAP"), &root_keymap);
            if (nilp(keybind)) {
              if (pairp(current_keymap) && pairp(root_keymap)
                  && current_keymap.value.pair == root_keymap.value.pair)
                {
                  if (debug_keybinding) {
                    printf("Key not bound: %c\n", (char)c);
                  }
                  // Default behaviour of un-bound input, just insert it.
                  // Maybe this should change? I'm not certain.
                  rope_append_byte(ginput, (char)c);
                  return;
                }
              env_set(*genv, make_sym("CURRENT-KEYMAP"), root_keymap);
              keybind_recurse_count += 1;
              if (debug_keybinding) {
                printf("keybind_recurse_count: %zu\n", keybind_recurse_count);
              }
              continue;
            }

            if (symbolp(keybind)) {
              // Explicitly insert characters with 'SELF-INSERT-CHAR' symbol.
              if (keybind.value.symbol && strlen(keybind.value.symbol)) {
                if (strcmp(keybind.value.symbol, "SELF-INSERT-CHAR") == 0) {
                  rope_append_byte(ginput, (char)c);
                }
              }
            } else if (stringp(keybind)) {
              // Rebind characters (only one byte for now) using a string associated value.
              if (keybind.value.symbol && strlen(keybind.value.symbol)) {
                c = keybind.value.symbol[0];
                // Go around again!
                keybind_recurse_count += 1;
                if (debug_keybinding) {
                  printf("keybind_recurse_count: %zu\n", keybind_recurse_count);
                }
                continue;
              }
            } else {
              if (debug_keybinding) {
                printf("Attempting to evaluate keybind: ");
                print_atom(keybind);
                putchar('\n');
              }

              Atom result = nil;
              Error err = evaluate_expression(cons(keybind, nil), *genv, &result);
              if (err.type) {
                printf("KEYBIND ");
                print_error(err);
                update_footline(gctx, allocate_string("ERROR! (keybind)"));
                return;
              }
              if (debug_keybinding) {
                printf("Result: ");
                print_atom(result);
                putchar('\n');
              }
              update_footline(gctx, atom_string(result, NULL));
            }

            // Reset current keymap
            env_get(*genv, make_sym("KEYMAP"), &current_keymap);
            env_set(*genv, make_sym("CURRENT-KEYMAP"), current_keymap);
            if (debug_keybinding) {
              printf("Keymap reset: ");
              print_atom(current_keymap);
              putchar('\n');
            }

            // End keybind loop.
            c = 0;
          }
          keybind_recurse_count += 1;
          if (debug_keybinding) {
            printf("keybind_recurse_count: %zu\n", keybind_recurse_count);
          }
        }
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

  Atom buffer = make_buffer
    (env_create(nil), allocate_string("LITE_SHINES_UPON_US.txt"));
  if (nilp(buffer)) {
    return 1;
  }

  env_set(environment, make_sym("CURRENT-BUFFER"), buffer);

#ifdef LITE_GFX
  genv = &environment;
  enter_lite_gui();
#else
  enter_repl(environment);
#endif

  int debug_memory = env_non_nil(environment, make_sym("DEBUG/MEMORY"));
  // Garbage collection with no marking means free everything.
  gcol();
  free_buffer_table();
  if (debug_memory) {
    print_gcol_data();
  }

  return 0;
}
