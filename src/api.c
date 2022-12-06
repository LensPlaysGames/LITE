#include <api.h>

#include <assert.h>
#include <buffer.h>
#include <environment.h>
#include <evaluation.h>
#include <gfx.h>
#include <gui.h>
#include <stdlib.h>
#include <string.h>
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

/// Add PROPERTY to the end of STRING properties linked list.
/// Returns boolean-like value (0 == failure).
int add_property_end(GUIString *string, GUIStringProperty *property) {
  if (!string || !property) { return 0; }
  if (!string->properties) {
    return add_property(string, property);
  }
  GUIStringProperty *last_valid = string->properties;
  for (; last_valid->next; last_valid = last_valid->next);
  last_valid->next = property;
  property->next = NULL;
  return 1;
}


GUIStringProperty *string_property
(size_t offset, size_t length, GUIColor fg, GUIColor bg) {
  GUIStringProperty *prop = malloc(sizeof(GUIStringProperty));
  if (!prop) { return NULL; }
  prop->next = NULL;
  prop->offset = offset;
  prop->length = length;
  prop->fg = fg;
  prop->bg = bg;
  return prop;
}

GUIStringProperty *string_property_copy_shallow(GUIStringProperty *original) {
  if (!original) { return NULL; }
  GUIStringProperty *prop = malloc(sizeof(GUIStringProperty));
  if (!prop) { return NULL; }
  *prop = *original;
  prop->next = NULL;
  return prop;
}

typedef struct GUIModifierKeyState {
  uint64_t bitfield; // Indexed by GUIModifierKey enum
} GUIModifierKeyState;

static GUIModifierKeyState gmodkeys;

// Return bit value of given modifier key. 1 if pressed.
int modkey_state(GUIModifierKey key) {
  assert(GUI_MODKEY_MAX == 8);
  switch (key) {
  default:
    return 0;
  case GUI_MODKEY_LSUPER:
    return ((uint64_t)1 << GUI_MODKEY_LSUPER) & gmodkeys.bitfield;
  case GUI_MODKEY_RSUPER:
    return ((uint64_t)1 << GUI_MODKEY_RSUPER) & gmodkeys.bitfield;
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
GUIContext *gui_ctx() { return gctx; }

/// Return zero iff input should be discarded.
int handle_modifier
(GUIModifierKey modkey,
 char should_discard_when_no_bind,
 Atom *current_keymap,
 size_t *keybind_recurse_count
 )
{
  const char *modifier_keystrings[GUI_MODKEY_MAX];
  modifier_keystrings[GUI_MODKEY_LSUPER] = "LEFT-SUPER";
  modifier_keystrings[GUI_MODKEY_RSUPER] = "RIGHT-SUPER";
  modifier_keystrings[GUI_MODKEY_LCTRL] = "LEFT-CONTROL";
  modifier_keystrings[GUI_MODKEY_RCTRL] = "RIGHT-CONTROL";
  modifier_keystrings[GUI_MODKEY_LALT] = "LEFT-ALT";
  modifier_keystrings[GUI_MODKEY_RALT] = "RIGHT-ALT";
  modifier_keystrings[GUI_MODKEY_LSHIFT] = "LEFT-SHIFT";
  modifier_keystrings[GUI_MODKEY_RSHIFT] = "RIGHT-SHIFT";
  if (modkey_state(modkey)) {
    Atom bind = alist_get(*current_keymap, make_string((char *)modifier_keystrings[modkey]));
    if (stringp(bind)) {
      bind = alist_get(*current_keymap, bind);
      *keybind_recurse_count += 1;
    }
    if (alistp(bind)) {
      *current_keymap = bind;
    } else {
      update_gui_string(&gctx->footline, allocate_string("Undefined keybinding!"));
      return !should_discard_when_no_bind;
    }
  }
  return 1;
}

/// This will set the environment variable `CURRENT-KEYMAP` based on modifiers
/// that are currently being held down. This is to be used in `handle_character_dn()`.
/// Returns boolean-like value (0 == discard keypress, 1 == use keypress)
int handle_character_dn_modifiers(Atom current_keymap, size_t *keybind_recurse_count) {
  // HANDLE MODIFIER KEYMAPS
  int out = 1;
  out = handle_modifier(GUI_MODKEY_LSUPER, 1, &current_keymap, keybind_recurse_count);
  if (out == 0) { return 0; }
  out = handle_modifier(GUI_MODKEY_RSUPER, 1, &current_keymap, keybind_recurse_count);
  if (out == 0) { return 0; }
  out = handle_modifier(GUI_MODKEY_LCTRL,  1, &current_keymap, keybind_recurse_count);
  if (out == 0) { return 0; }
  out = handle_modifier(GUI_MODKEY_RCTRL,  1, &current_keymap, keybind_recurse_count);
  if (out == 0) { return 0; }
  out = handle_modifier(GUI_MODKEY_LALT,   1, &current_keymap, keybind_recurse_count);
  if (out == 0) { return 0; }
  out = handle_modifier(GUI_MODKEY_RALT,   1, &current_keymap, keybind_recurse_count);
  if (out == 0) { return 0; }
  out = handle_modifier(GUI_MODKEY_LSHIFT, 1, &current_keymap, keybind_recurse_count);
  if (out == 0) { return 0; }
  out = handle_modifier(GUI_MODKEY_RSHIFT, 1, &current_keymap, keybind_recurse_count);
  if (out == 0) { return 0; }
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
  const size_t keybind_recurse_limit = 256;
  size_t keybind_recurse_count = 0;
  // Get current keymap from LISP environment.
  Atom current_keymap = nil;
  env_get(*genv(), make_sym("CURRENT-KEYMAP"), &current_keymap);
  if (nilp(current_keymap)) {
    env_get(*genv(), make_sym("KEYMAP"), &current_keymap);
    if (nilp(current_keymap)) {
      // At this point, it is most likely that LITE has been started
      // without any keymap configuration whatsoever (no standard
      // library), or the user has purposefully borked it.
      // TODO: Do something like restoring default keymap or something
      // that is more useful than just flopping around like a dead fish.
      fprintf(stderr, "ERROR: There is no bound keymap!\n");
    }
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
      break;
    }
    Atom root_keymap = nil;
    env_get(*genv(), make_sym("KEYMAP"), &root_keymap);
    if (nilp(keybind)) {
      // If keybind is nil, it means that the keystring is not bound in current keymap.
      if (pairp(current_keymap) && pairp(root_keymap)
          && current_keymap.value.pair == root_keymap.value.pair)
        {
          // If current keymap is root keymap, key is not bound at all.
          if (debug_keybinding) {
            printf("Key not bound: \"%s\"\n", keystring);
          }
          // Default behaviour of un-bound input, just insert it.
          // TODO/FIXME: Maybe this should change? I'm not certain.
          Error err = buffer_insert(current_buffer.value.buffer, keystring);
          if (err.type) {
            print_error(err);
            update_gui_string(&gctx->footline, error_string(err));
            return;
          }
          break;
        }
      // Key not bound in current keymap, set current to root_keymap and try again.
      // FIXME: I don't think this makes any sense whatsoever.
      env_set(*genv(), make_sym("CURRENT-KEYMAP"), root_keymap);
      keybind_recurse_count += 1;
      if (debug_keybinding) {
        printf("keybind_recurse_count: %zu\n", keybind_recurse_count);
      }
      continue;
    }
    if (symbolp(keybind)) {
      if (keybind.value.symbol && keybind.value.symbol[0] != '\0') {
        // Intentionally ignore with 'IGNORE' symbol.
        if (strcmp(keybind.value.symbol, "IGNORE") == 0) {
          return;
        }
        // Explicitly insert with 'SELF-INSERT' symbol.
        if (strcmp(keybind.value.symbol, "SELF-INSERT") == 0) {
          Error err = buffer_insert(current_buffer.value.buffer, keystring);
          if (err.type) {
            print_error(err);
            update_gui_string(&gctx->footline, error_string(err));
            return;
          }
          break;
        }
        // If symbol is not recognized, just attempt to evaluate it
        // (fallthrough).
      }
    } else if (stringp(keybind)) {
      // Rebind characters using a string associated value.
      if (!keybind.value.symbol || keybind.value.symbol[0] == '\0') {
        fprintf(stderr, "Can not follow rebind of %s to empty string!\n", keystring);
        return;
      }
      keystring = (char *)keybind.value.symbol;
      // Go around again!
      keybind_recurse_count += 1;
      if (debug_keybinding) {
        printf("keybind_recurse_count: %zu\n", keybind_recurse_count);
      }
      continue;
      // String is either empty or invalid, either way we should probably return an error.
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

    if (user_quit) {
      return;
    }

    if (debug_keybinding) {
      printf("Result: ");
      print_atom(result);
      putchar('\n');
    }
    update_gui_string(&gctx->footline, atom_string(result, NULL));

    // Keystring handled through evaluation, break out of keystring
    // handling loop.
    break;
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

}

void handle_modifier_dn(GUIModifierKey mod) {
  assert(GUI_MODKEY_MAX == 8);
  if (mod > GUI_MODKEY_MAX) {
    return;
  } else if (mod == GUI_MODKEY_MAX) {
    for (size_t i = 0; i < GUI_MODKEY_MAX; ++i) {
      gmodkeys.bitfield |= ((uint64_t)1 << i);
    }
    return;
  }
  switch (mod) {
  default:
    printf("API::GFX:ERROR: Unhandled modifier keydown: %d\n"
           "              : Please report as issue on LITE GitHub.\n"
           , mod);
    break;
  case GUI_MODKEY_LSUPER:
    gmodkeys.bitfield |= ((uint64_t)1 << GUI_MODKEY_LSUPER);
    break;
  case GUI_MODKEY_RSUPER:
    gmodkeys.bitfield |= ((uint64_t)1 << GUI_MODKEY_RSUPER);
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
  assert(GUI_MODKEY_MAX == 8);
  if (mod > GUI_MODKEY_MAX) {
    return;
  } else if (mod == GUI_MODKEY_MAX) {
    for (size_t i = 0; i < GUI_MODKEY_MAX; ++i) {
      gmodkeys.bitfield &= ~((uint64_t)1 << i);
    }
    return;
  }
  switch (mod) {
  default:
    printf("API::GFX:ERROR: Unhandled modifier keyup: %d\n"
           "              : Please report as issue on LITE GitHub.\n"
           , mod);
    break;
  case GUI_MODKEY_LSUPER:
    gmodkeys.bitfield &= ~((uint64_t)1 << GUI_MODKEY_LSUPER);
    break;
  case GUI_MODKEY_RSUPER:
    gmodkeys.bitfield &= ~((uint64_t)1 << GUI_MODKEY_RSUPER);
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
  GUIContext *ctx = calloc(1, sizeof(GUIContext));
  if (!ctx) { return NULL; }
  ctx->title = "LITE GFX";
  update_gui_string(&ctx->headline, allocate_string("LITE Headline"));
  update_gui_string(&ctx->contents, allocate_string("LITE Contents"));
  update_gui_string(&ctx->footline, allocate_string("LITE Footline"));
  update_gui_string(&ctx->popup,    allocate_string("LITE Popup"));
  if (!ctx->headline.string || !ctx->contents.string || !ctx->footline.string) {
    return NULL;
  }

  GUIStringProperty default_cursor;
  default_cursor.fg.r = 0;
  default_cursor.fg.g = 0;
  default_cursor.fg.b = 0;
  default_cursor.fg.a = UINT8_MAX;

  default_cursor.bg.r = UINT8_MAX;
  default_cursor.bg.g = UINT8_MAX;
  default_cursor.bg.b = UINT8_MAX;
  default_cursor.bg.a = UINT8_MAX;

  default_cursor.offset = 0;
  default_cursor.length = 1;

  create_gui_property(GUI_PROP_ID_CURSOR, &default_cursor);

  GUIStringProperty default_region;
  default_region.fg.r = UINT8_MAX;
  default_region.fg.g = UINT8_MAX;
  default_region.fg.b = UINT8_MAX;
  default_region.fg.a = UINT8_MAX;

  default_region.bg.r = 53;
  default_region.bg.g = 53;
  default_region.bg.b = 53;
  default_region.bg.a = UINT8_MAX;

  default_region.offset = 0;
  default_region.length = 0;

  create_gui_property(GUI_PROP_ID_REGION, &default_region);

  GUIStringProperty default_default;
  default_default.fg.r = UINT8_MAX;
  default_default.fg.g = UINT8_MAX;
  default_default.fg.b = UINT8_MAX;
  default_default.fg.a = UINT8_MAX;

  default_default.bg.r = 22;
  default_default.bg.g = 23;
  default_default.bg.b = 24;
  default_default.bg.a = UINT8_MAX;

  default_default.offset = 0;
  default_default.length = 0;

  create_gui_property(GUI_PROP_ID_DEFAULT, &default_default);

  ctx->default_property.fg = default_default.fg;
  ctx->default_property.bg = default_default.bg;

  ctx->reading = 0;

  return ctx;
}

HOTFUNCTION
int gui_loop() {
  // Call all "refresh" functions. Just a list of LISP forms that
  // we then call each `car` of...
  Atom refresh_hook = nil;
  Atom result = nil;
  Error err = env_get(*genv(), make_sym("REFRESH-HOOK"), &refresh_hook);
  if (!err.type) {
    for(; !nilp(refresh_hook); refresh_hook = cdr(refresh_hook)) {
      err = evaluate_expression(car(refresh_hook), *genv(), &result);
      if (err.type) {
        printf("REFRESH HOOK ");
        print_error(err);
      }
    }
  }

  Atom current_buffer = nil;
  err = env_get(*genv(), make_sym("CURRENT-BUFFER"), &current_buffer);
  if (err.type) {
    print_error(err);
  }
  Atom active_window_index = nil;
  err = env_get(*genv(), make_sym("ACTIVE-WINDOW-INDEX"), &current_buffer);
  if (err.type) {
    print_error(err);
  }
  Atom window_list = nil;
  err = env_get(*genv(), make_sym("WINDOWS"), &current_buffer);
  if (err.type) {
    print_error(err);
  }

  size_t index = 0;
  for (Atom window_it = window_list; !nilp(window_it); window_it = cdr(window_it), ++index) {
    Atom window = car(window_it);
    if (index == active_window_index.value.integer) {
      // TODO: Active window specific stuff, like cursor ig
    }
  }

  return do_gui(gctx);
}

int enter_lite_gui() {
  int status = create_gui();
  if (status != CREATE_GUI_OK && status != CREATE_GUI_ALREADY_CREATED) {
    return 69;
  }

  gctx = initialize_lite_gui_ctx();
  if (!gctx) { return 70; }

  change_window_visibility(GFX_WINDOW_VISIBLE);

  Error err = ok;
  int open = 1;
  while (open) {
    open = gui_loop();

    // This is the one and only place that USER/QUIT should ever be set
    // to nil when LITE_GFX is defined. Every other place should simply
    // return if 'USER/QUIT' is non-nil, and hopefully we eventually
    // end up back at the top of the call stack, right here :^).

    if (user_quit) {
      // Reset 'USER/QUIT'.
      user_quit = 0;

      // Reset current keymap to root keymap.
      // TODO/FIXME: Should probably return error code here.
      Atom keymap = nil;
      err = env_get(*genv(), make_sym("KEYMAP"), &keymap);
      if (err.type) {
        print_error(err);
        return 0;
      }
      err = env_set(*genv(), make_sym("CURRENT-KEYMAP"), keymap);
      if (err.type) {
        print_error(err);
        return 0;
      }
    }
  }
  destroy_gui();
  return 0;
}
