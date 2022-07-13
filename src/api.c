#include <api.h>

#include <buffer.h>
#include <environment.h>
#include <evaluation.h>
#include <gui.h>
#include <stdlib.h>
#include <types.h>
#include <utility.h>

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

void free_properties(GUIString string) {
  GUIStringProperty *it = string.properties;
  while (it) {
    GUIStringProperty *property_to_free = it;
    it = it->next;
    free(property_to_free);
  }
}

void reset_properties(GUIString *string) {
  if (!string) { return; }
  free_properties(*string);
  string->properties = NULL;
}

void update_gui_string(GUIString *string, char *new_string) {
  if (!string || !new_string) { return; }
  if (string->string) {
    free(string->string);
  }
  string->string = new_string;
  reset_properties(string);
}

/// Add PROPERTY to beginning of STRING properties linked list.
/// Returns boolean-like value (0 == failure).
int add_property(GUIString *string, GUIStringProperty *property) {
  if (!string || !property) { return 0; }
  property->next = string->properties;
  string->properties = property;
  return 1;
}

GUIStringProperty *string_property
(size_t offset
 , size_t length
 , GUIColor fg
 , GUIColor bg
 )
{
  GUIStringProperty *prop = malloc(sizeof(GUIStringProperty));
  if (!prop) { return NULL; }
  prop->next = NULL;
  prop->offset = offset;
  prop->length = length;
  prop->fg = fg;
  prop->bg = bg;
  return prop;
}

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
inline GUIContext *gui_ctx() { return gctx; }

/// This will set the environment variable `CURRENT-KEYMAP` based on modifiers
/// that are currently being held down. This is to be used in `handle_character_dn()`.
/// Returns boolean-like value (0 == discard keypress, 1 == use keypress)
int handle_character_dn_modifiers(Atom current_keymap, size_t *keybind_recurse_count) {
  // HANDLE MODIFIER KEYMAPS
  if (modkey_state(GUI_MODKEY_LSUPER)) {
    Atom lsuper_bind = alist_get(current_keymap, make_string("LEFT-SUPER"));
    // Allow string-rebinding of super to another character.
    // Mostly, this is used to rebind to 'SUPR', the generic L/R super keymap.
    if (stringp(lsuper_bind)) {
      lsuper_bind = alist_get(current_keymap, lsuper_bind);
      *keybind_recurse_count += 1;
    }
    if (alistp(lsuper_bind)) {
      current_keymap = lsuper_bind;
    } else {
      // TODO: What keybind is undefined???
      // I guess we need to keep track of how we got here.
      // This could be done by saving keys of each keybind.
      update_gui_string(&gctx->footline, allocate_string("Undefined keybinding!"));
      // Discard character if no keybind was found.
      return 0;
    }
  } else if (modkey_state(GUI_MODKEY_RSUPER)) {
    Atom rsuper_bind = alist_get(current_keymap, make_string("RIGHT-SUPER"));
    if (stringp(rsuper_bind)) {
      rsuper_bind = alist_get(current_keymap, rsuper_bind);
      *keybind_recurse_count += 1;
    }
    if (alistp(rsuper_bind)) {
      current_keymap = rsuper_bind;
    } else {
      // TODO: What keybind is undefined???
      update_gui_string(&gctx->footline, allocate_string("Undefined keybinding!"));
      // Discard character if no keybind was found.
      return 0;
    }
  }
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
      update_gui_string(&gctx->footline, allocate_string("Undefined keybinding!"));
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
      update_gui_string(&gctx->footline, allocate_string("Undefined keybinding!"));
      // Discard character if no ctrl keybind was found.
      return 0;
    }
  }
  if (modkey_state(GUI_MODKEY_LALT)) {
    Atom lalt_bind = alist_get(current_keymap, make_string("LEFT-ALT"));
    // Allow string-rebinding of alt to another character.
    // Mostly, this is used to rebind to 'ALTS', the generic L/R alt keymap.
    if (stringp(lalt_bind)) {
      lalt_bind = alist_get(current_keymap, lalt_bind);
      *keybind_recurse_count += 1;
    }
    if (alistp(lalt_bind)) {
      current_keymap = lalt_bind;
    } else {
      // TODO: What keybind is undefined???
      update_gui_string(&gctx->footline, allocate_string("Undefined keybinding!"));
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
      update_gui_string(&gctx->footline, allocate_string("Undefined keybinding!"));
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
  env_set(*genv(), make_sym("CURRENT-KEYMAP"), current_keymap);
  return 1;
}

void handle_keydown(char *keystring) {
  int debug_keybinding = env_non_nil(*genv(), make_sym("DEBUG/KEYBINDING"));
  if (debug_keybinding) {
    printf("Keydown: %s\n", keystring ? keystring : "NULL");
  }
  if (!keystring) {
    return;
  }
  const char *ignored_bytes = "\e\f\v";
  if (strpbrk(keystring, ignored_bytes)) {
    return;
  }
  const size_t keybind_recurse_limit = 256;
  size_t keybind_recurse_count = 0;

  // Get current keymap from LISP environment.
  Atom current_keymap = nil;
  env_get(*genv(), make_sym("CURRENT-KEYMAP"), &current_keymap);
  if (nilp(current_keymap)) {
    env_get(*genv(), make_sym("KEYMAP"), &current_keymap);
  }
  if (debug_keybinding) {
    printf("Current keymap: ");
    pretty_print_atom(current_keymap);
    putchar('\n');
  }
  Atom current_buffer = nil;
  env_get(*genv(), make_sym("CURRENT-BUFFER"), &current_buffer);
  if (!bufferp(current_buffer)) {
    return;
  }

  if (!handle_character_dn_modifiers(current_keymap, &keybind_recurse_count)) {
    return;
  }
  while (keystring && keybind_recurse_count < keybind_recurse_limit) {
    env_get(*genv(), make_sym("CURRENT-KEYMAP"), &current_keymap);
    if (debug_keybinding) {
      printf("Current keymap: ");
      pretty_print_atom(current_keymap);
      putchar('\n');
    }

    Atom keybind = alist_get(current_keymap, make_string(keystring));

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
      env_set(*genv(), make_sym("CURRENT-KEYMAP"), keybind);
      keystring = NULL;
      continue;
    } else {
      Atom root_keymap = nil;
      env_get(*genv(), make_sym("KEYMAP"), &root_keymap);
      if (nilp(keybind)) {
        if (pairp(current_keymap) && pairp(root_keymap)
            && current_keymap.value.pair == root_keymap.value.pair)
          {
            // If current keymap is root keymap, key is not bound.
            if (debug_keybinding) {
              printf("Key not bound: \"%s\"\n", keystring);
            }
            // Default behaviour of un-bound input, just insert it.
            // Maybe this should change? I'm not certain.
            Error err = buffer_insert(current_buffer.value.buffer, keystring);
            if (err.type) {
              print_error(err);
              update_gui_string(&gctx->footline, error_string(err));
              return;
            }
            keystring = NULL;
            continue;
          }
        env_set(*genv(), make_sym("CURRENT-KEYMAP"), root_keymap);
        keybind_recurse_count += 1;
        if (debug_keybinding) {
          printf("keybind_recurse_count: %zu\n", keybind_recurse_count);
        }
        continue;
      }
      if (symbolp(keybind)) {
        // Explicitly insert with 'SELF-INSERT' symbol.
        if (keybind.value.symbol && keybind.value.symbol[0] != '\0') {
          if (strcmp(keybind.value.symbol, "IGNORE") == 0) {
            return;
          }
          if (strcmp(keybind.value.symbol, "SELF-INSERT") == 0) {
            // FIXME: This assumes one-byte content.
            Error err = buffer_insert(current_buffer.value.buffer, keystring);
            if (err.type) {
              print_error(err);
              update_gui_string(&gctx->footline, error_string(err));
              return;
            }
            keystring = NULL;
            continue;
          }
        }
      } else if (stringp(keybind)) {
        // Rebind characters using a string associated value.
        if (keybind.value.symbol && keybind.value.symbol[0] != '\0') {
          keystring = (char *)keybind.value.symbol;
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
      Error err = evaluate_expression(keybind, *genv(), &result);
      if (err.type) {
        printf("KEYBIND ");
        print_error(err);
        update_gui_string(&gctx->footline, error_string(err));
        env_set(*genv(), make_sym("CURRENT-KEYMAP"), root_keymap);
        return;
      }
      if (debug_keybinding) {
        printf("Result: ");
        print_atom(result);
        putchar('\n');
      }
      update_gui_string(&gctx->footline, atom_string(result, NULL));
    }
    // Reset current keymap
    env_get(*genv(), make_sym("KEYMAP"), &current_keymap);
    env_set(*genv(), make_sym("CURRENT-KEYMAP"), current_keymap);
    if (debug_keybinding) {
      printf("Keymap reset: ");
      print_atom(current_keymap);
      putchar('\n');
    }
    keybind_recurse_count += 1;
    if (debug_keybinding) {
      printf("keybind_recurse_count: %zu\n", keybind_recurse_count);
    }
    // End keybind loop.
    keystring = NULL;
  }
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

GUIContext *initialize_lite_gui_ctx() {
  GUIContext *ctx = malloc(sizeof(GUIContext));
  if (!ctx) { return NULL; }
  memset(ctx, 0, sizeof(GUIContext));
  ctx->title = "LITE GFX";
  update_gui_string(&ctx->headline, allocate_string("LITE Headline"));
  update_gui_string(&ctx->contents, allocate_string("LITE Contents"));
  update_gui_string(&ctx->footline, allocate_string("LITE Footline"));
  update_gui_string(&ctx->popup,    allocate_string("LITE Popup"));
  if (!ctx->headline.string || !ctx->contents.string || !ctx->footline.string) {
    return NULL;
  }

  ctx->default_property.fg.r = UINT8_MAX;
  ctx->default_property.fg.g = UINT8_MAX;
  ctx->default_property.fg.b = UINT8_MAX;
  ctx->default_property.fg.a = UINT8_MAX;

  ctx->default_property.bg.r = 46;
  ctx->default_property.bg.g = 46;
  ctx->default_property.bg.b = 46;
  ctx->default_property.bg.a = UINT8_MAX;

  ctx->reading = 0;

  return ctx;
}

// TODO: Make this LISP extensible.
// Maybe think about an equivalent to Emacs' faces?

static GUIColor cursor_fg = { 0,0,0,UINT8_MAX };
static GUIColor cursor_bg = { UINT8_MAX,UINT8_MAX,
                              UINT8_MAX,UINT8_MAX };

int gui_loop() {
  char *new_contents = NULL;
  Atom current_buffer = nil;
  env_get(*genv(), make_sym("CURRENT-BUFFER"), &current_buffer);
  if (bufferp(current_buffer)) {
    new_contents = buffer_string(*current_buffer.value.buffer);
  }

  GUIString *to_update =
    gctx->reading ? &gctx->popup : &gctx->contents;
  update_gui_string(to_update, new_contents);
  GUIStringProperty *cursor_property = string_property
    (current_buffer.value.buffer->point_byte, 1
     , cursor_fg, cursor_bg);
  add_property(to_update, cursor_property);

  int open = do_gui(gctx);
  if (!open) { return open; }

  Atom sleep_ms = nil;
  env_get(*genv(), make_sym("REDISPLAY-IDLE-MS"), &sleep_ms);
  if (integerp(sleep_ms)) {
    SLEEP(sleep_ms.value.integer);
  } else {
    SLEEP(20);
  }
  return 1;
}

int enter_lite_gui() {
  if (create_gui()) {
    return 69;
  }
  gctx = initialize_lite_gui_ctx();
  if (!gctx) { return 69; }

  int open = 1;
  while (open) {
    open = gui_loop();
  }
  destroy_gui();
  return 0;
}
