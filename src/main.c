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

GUIContext *gctx = NULL;

/// This will set the environment variable `CURRENT-KEYMAP` based on modifiers
/// that are currently being held down. This is to be used in `handle_character_dn()`.
/// Returns boolean-like value (0 == discard keypress, 1 == use keypress)
int handle_character_dn_modifiers(Atom current_keymap, size_t *keybind_recurse_count) {
  // HANDLE MODIFIER KEYMAPS

  if (modkey_state(GUI_MODKEY_LCTRL)) {
    Atom lctrl_bind = alist_get(current_keymap, make_string("LEFT-CONTROL"));
    // Allow string-rebinding of ctrl to another character.
    // Mostly, this is used to rebind to 'CTRL', the generic L/R ctrl keymap.
    if (stringp(lctrl_bind)) {
      lctrl_bind = alist_get(current_keymap, lctrl_bind);
      *keybind_recurse_count += 1;
    }
    if (alistp(lctrl_bind)) {
      current_keymap = lctrl_bind;
    } else {
      // TODO: What keybind is undefined???
      // I guess we need to keep track of how we got here.
      // This could be done by saving keys of each keybind.
      update_footline(gctx, allocate_string("Undefined keybinding!"));
      // Discard character if no ctrl keybind was found.
      return 0;
    }
  } else if (modkey_state(GUI_MODKEY_RCTRL)) {
    Atom rctrl_bind = alist_get(current_keymap, make_string("RIGHT-CONTROL"));
    if (stringp(rctrl_bind)) {
      rctrl_bind = alist_get(current_keymap, rctrl_bind);
      *keybind_recurse_count += 1;
    }
    if (alistp(rctrl_bind)) {
      current_keymap = rctrl_bind;
    } else {
      // TODO: What keybind is undefined???
      update_footline(gctx, allocate_string("Undefined keybinding!"));
      // Discard character if no ctrl keybind was found.
      return 0;
    }
  }
  if (modkey_state(GUI_MODKEY_LALT)) {
    Atom lalt_bind = alist_get(current_keymap, make_string("LEFT-ALT"));
    // Allow string-rebinding of ctrl to another character.
    // Mostly, this is used to rebind to 'CTRL', the generic L/R ctrl keymap.
    if (stringp(lalt_bind)) {
      lalt_bind = alist_get(current_keymap, lalt_bind);
      *keybind_recurse_count += 1;
    }
    if (alistp(lalt_bind)) {
      current_keymap = lalt_bind;
    } else {
      // TODO: What keybind is undefined???
      update_footline(gctx, allocate_string("Undefined keybinding!"));
      // Discard character if no alt keybind was found.
      return 0;
    }
  } else if (modkey_state(GUI_MODKEY_RALT)) {
    Atom ralt_bind = alist_get(current_keymap, make_string("RIGHT-ALT"));
    if (stringp(ralt_bind)) {
      ralt_bind = alist_get(current_keymap, ralt_bind);
      *keybind_recurse_count += 1;
    }
    if (alistp(ralt_bind)) {
      current_keymap = ralt_bind;
    } else {
      update_footline(gctx, allocate_string("Undefined keybinding!"));
      // Discard character if no keybind was found.
      return 0;
    }
  }
  if (modkey_state(GUI_MODKEY_LSHIFT)) {
    Atom lshift_bind = alist_get(current_keymap, make_string("LEFT-SHIFT"));
    // Allow string-rebinding of shift to another character.
    // Mostly, this is used to rebind to 'SHFT', the generic L/R shift keymap.
    if (stringp(lshift_bind)) {
      lshift_bind = alist_get(current_keymap, lshift_bind);
      *keybind_recurse_count += 1;
    }
    if (alistp(lshift_bind)) {
      current_keymap = lshift_bind;
    }
  } else if (modkey_state(GUI_MODKEY_RSHIFT)) {
    Atom rshift_bind = alist_get(current_keymap, make_string("RIGHT-SHIFT"));
    if (stringp(rshift_bind)) {
      rshift_bind = alist_get(current_keymap, rshift_bind);
      *keybind_recurse_count += 1;
    }
    if (alistp(rshift_bind)) {
      current_keymap = rshift_bind;
    }
  }
  env_set(genv(), make_sym("CURRENT-KEYMAP"), current_keymap);
  return 1;
}

void handle_character_dn(uint64_t c) {
  const char *ignored_bytes = "\e\f\v";
  if (strchr(ignored_bytes, (unsigned char)c)) {
    return;
  }

  int debug_keybinding = env_non_nil(genv(), make_sym("DEBUG/KEYBINDING"));
  if (debug_keybinding) {
    if (c > 255) {
      printf("Keydown: %llu\n", c);
    } else {
      printf("Keydown: %c\n", (char)c);
    }
  }
  Atom current_buffer = nil;
  env_get(genv(), make_sym("CURRENT-BUFFER"), &current_buffer);
  if (bufferp(current_buffer)) {
    const size_t keybind_recurse_limit = 256;
    size_t keybind_recurse_count = 0;
    // Get current keymap from LISP environment.
    Atom current_keymap = nil;
    env_get(genv(), make_sym("CURRENT-KEYMAP"), &current_keymap);
    if (nilp(current_keymap)) {
      env_get(genv(), make_sym("KEYMAP"), &current_keymap);
    }
    if (debug_keybinding) {
      printf("Current keymap: ");
      pretty_print_atom(current_keymap);
      putchar('\n');
    }

    if (!handle_character_dn_modifiers(current_keymap, &keybind_recurse_count)) {
      return;
    }

    while (c && keybind_recurse_count < keybind_recurse_limit) {
      env_get(genv(), make_sym("CURRENT-KEYMAP"), &current_keymap);
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
        env_set(genv(), make_sym("CURRENT-KEYMAP"), keybind);
        c = 0;
      } else {
        Atom root_keymap = nil;
        env_get(genv(), make_sym("KEYMAP"), &root_keymap);
        if (nilp(keybind)) {
          if (pairp(current_keymap) && pairp(root_keymap)
              && current_keymap.value.pair == root_keymap.value.pair)
            {
              if (debug_keybinding) {
                printf("Key not bound: %c\n", (char)c);
              }
              // Default behaviour of un-bound input, just insert it.
              // Maybe this should change? I'm not certain.
              // FIXME: This assumes one-byte content.
              Error err = buffer_insert_byte(current_buffer.value.buffer, (char)c);
              if (err.type) {
                print_error(err);
                update_footline(gctx, error_string(err));
                return;
              }
              return;
            }
          env_set(genv(), make_sym("CURRENT-KEYMAP"), root_keymap);
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
              // FIXME: This assumes one-byte content.
              Error err = buffer_insert_byte(current_buffer.value.buffer, (char)c);
              if (err.type) {
                print_error(err);
                update_footline(gctx, error_string(err));
                return;
              }
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
        }
        if (debug_keybinding) {
          printf("Attempting to evaluate keybind: ");
          print_atom(keybind);
          putchar('\n');
        }
        Atom result = nil;
        Error err = evaluate_expression(keybind, genv(), &result);
        if (err.type) {
          printf("KEYBIND ");
          print_error(err);
          update_footline(gctx, error_string(err));
          env_set(genv(), make_sym("CURRENT-KEYMAP"), root_keymap);
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
      env_get(genv(), make_sym("KEYMAP"), &current_keymap);
      env_set(genv(), make_sym("CURRENT-KEYMAP"), current_keymap);
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
  while (open) {
    char *new_contents = NULL;
    Atom current_buffer = nil;
    // Maybe only redisplay on a change somehow?
    env_get(genv(), make_sym("CURRENT-BUFFER"), &current_buffer);
    if (bufferp(current_buffer)) {
      new_contents = buffer_string(*current_buffer.value.buffer);
    }
    update_contents(gctx, new_contents);
    do_gui(&open, gctx);
    // Only update GUI every 10 milliseconds.
    Atom sleep_ms = nil;
    env_get(genv(), make_sym("REDISPLAY-IDLE-MS"), &sleep_ms);
    if (integerp(sleep_ms)) {
      SLEEP(sleep_ms.value.integer);
    } else {
      SLEEP(10);
    }
  }
  destroy_gui();
  return 0;
}
#endif

int main(int argc, char **argv) {
  printf("LITE will guide the way through the darkness.\n");

  // Treat every given argument as a file to load, for now.
  if (argc > 1) {
    for (size_t i = 1; i < argc; ++i) {
      load_file(genv(), argv[i]);
    }
  }

  Atom buffer = make_buffer
    (env_create(nil), allocate_string("LITE_SHINES_UPON_US.txt"));
  if (nilp(buffer)) {
    return 1;
  }

  env_set(genv(), make_sym("CURRENT-BUFFER"), buffer);

#ifdef LITE_GFX
  enter_lite_gui();
#else
  enter_repl(genv());
#endif

  int debug_memory = env_non_nil(genv(), make_sym("DEBUG/MEMORY"));
  // Garbage collection with no marking means free everything.
  gcol();
  free_buffer_table();
  if (debug_memory) {
    print_gcol_data();
  }

  return 0;
}
