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
  // TODO/FIXME: Should we create a window by default here?
  update_gui_string(&ctx->footline, allocate_string("LITE Footline"));
  update_gui_string(&ctx->popup,    allocate_string("LITE Popup"));
  if (!ctx->popup.string || !ctx->footline.string) {
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
int gui_loop(void) {
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
  err = env_get(*genv(), make_sym("ACTIVE-WINDOW-INDEX"), &active_window_index);
  if (err.type) {
    print_error(err);
  }
  Atom window_list = nil;
  err = env_get(*genv(), make_sym("WINDOWS"), &window_list);
  if (err.type) {
    print_error(err);
  }

  // Free all GUIWindows!
  GUIWindow *window = gctx->windows;
  while (window) {
    free_properties(window->contents);
    free(window->contents.string);
    GUIWindow *next_window = window->next;
    free(window);
    window = next_window;
  }
  gctx->windows = NULL;

  if (gui_ctx()->popup.string) {
    free_properties(gui_ctx()->popup);
    gui_ctx()->popup.properties = NULL;
    free(gui_ctx()->popup.string);
    gui_ctx()->popup.string = NULL;
  }

  integer_t index = 0;
  GUIWindow *last_window = gctx->windows;
  for (Atom window_it = window_list; !nilp(window_it); window_it = cdr(window_it), ++index) {
    Atom window = car(window_it);
    // Expected format:
    // (z (posx . posy) (sizex . sizey) (scrollx . scrolly) (contents . properties))
    // (z . ((posx . posy) . ((sizex . sizey) . ((scrollx . scrolly) . ((contents . properties) . nil)))))
    // properties:
    // ((id offset length (fg.r fg.g fg.b fg.r) (bg.r bg.g bg.b bg.a)))
    if (!(pairp(window)
          && pairp(cdr(window))
          && pairp(cdr(cdr(window)))
          && pairp(cdr(cdr(cdr(window))))
          && pairp(cdr(cdr(cdr(cdr(window)))))
          && nilp(cdr(cdr(cdr(cdr(cdr(window))))))

          // (posx . posy)
          && pairp(car(cdr(window)))
          && integerp(car(car(cdr(window))))
          && integerp(cdr(car(cdr(window))))

          // (sizex . sizey)
          && pairp(car(cdr(cdr(window))))
          && integerp(car(car(cdr(cdr(window)))))
          && integerp(cdr(car(cdr(cdr(window)))))

          // (scrollx . scrolly)
          && pairp(car(cdr(cdr(cdr(window)))))
          && integerp(car(car(cdr(cdr(cdr(window))))))
          && integerp(cdr(car(cdr(cdr(cdr(window))))))

          // (contents . properties)
          && pairp(car(cdr(cdr(cdr(cdr(window))))))
          // TODO: Allow for windows with contents other than buffers?
          && bufferp(car(car(cdr(cdr(cdr(cdr(window)))))))
          ))
      continue;

    GUIWindow *new_gui_window = calloc(1, sizeof(GUIWindow));

    // Set integer values
    new_gui_window->z     = car(window).value.integer;
    new_gui_window->posx  = car(car(cdr(window))).value.integer;
    new_gui_window->posy  = cdr(car(cdr(window))).value.integer;
    new_gui_window->sizex = car(car(cdr(cdr(window)))).value.integer;
    new_gui_window->sizey = cdr(car(cdr(cdr(window)))).value.integer;
    new_gui_window->contents.horizontal_offset = car(car(cdr(cdr(cdr(window))))).value.integer;
    new_gui_window->contents.vertical_offset   = cdr(car(cdr(cdr(cdr(window))))).value.integer;

    if (new_gui_window->posx > 100) {
      new_gui_window->posx = 100;
    }
    if (new_gui_window->posy > 100) {
      new_gui_window->posy = 100;
    }
    if (new_gui_window->posx + new_gui_window->sizex > 100) {
      new_gui_window->sizex = 100 - new_gui_window->posx;
    }
    if (new_gui_window->posy + new_gui_window->sizey > 100) {
      new_gui_window->sizey = 100 - new_gui_window->posy;
    }

    // Set contents
    Atom contents   = car(car(cdr(cdr(cdr(cdr(window))))));
    Atom properties = cdr(car(cdr(cdr(cdr(cdr(window))))));
    char *contents_string = NULL;
    if (bufferp(contents) && contents.value.buffer) {
      contents_string = buffer_string(*contents.value.buffer);
    } else if (stringp(contents) && contents.value.symbol) {
      contents_string = strdup(contents.value.symbol);
    }
    new_gui_window->contents.string = contents_string;

    if (index == active_window_index.value.integer) {
      // Active window specific properties, like cursor ig

      // TODO: Somehow think about how to rework these; it seems like
      // I'm doing something backwards or inside out.
      // The user *should* be able to set these as known IDS.
      // User added properties should have higher IDs.
      // So maybe *every* window should have three properties added;
      // default, region, and cursor. These could be something like
      // builtin properties and always accessible at the same ID.
      //
      // 1. We could have separate properties that are special. So like
      // CURSOR would be in the LISP environment by itself, gotten
      // here, and used to set the property. Not a bad way to go about
      // things, and it would avoid the entire ID debacle.

      GUIStringProperty *cursor_property = calloc(1, sizeof(GUIStringProperty));
      cursor_property->fg.r = 0;
      cursor_property->fg.g = 0;
      cursor_property->fg.b = 0;
      cursor_property->fg.a = UINT8_MAX;

      cursor_property->bg.r = UINT8_MAX;
      cursor_property->bg.g = UINT8_MAX;
      cursor_property->bg.b = UINT8_MAX;
      cursor_property->bg.a = UINT8_MAX;

      cursor_property->offset = current_buffer.value.buffer->point_byte;
      cursor_property->length = 1;

      if (gui_ctx()->reading) {
        add_property(&gui_ctx()->popup, cursor_property);
      } else {
        add_property(&new_gui_window->contents, cursor_property);
      }

      // If mark is activated, create property for selection.
      if (buffer_mark_active(*current_buffer.value.buffer)) {
        GUIStringProperty *region_property = calloc(1, sizeof(GUIStringProperty));
        region_property->fg.r = UINT8_MAX;
        region_property->fg.g = UINT8_MAX;
        region_property->fg.b = UINT8_MAX;
        region_property->fg.a = UINT8_MAX;

        region_property->bg.r = 53;
        region_property->bg.g = 53;
        region_property->bg.b = 53;
        region_property->bg.a = UINT8_MAX;

        size_t mark_byte = buffer_mark(*current_buffer.value.buffer);
        if (current_buffer.value.buffer->point_byte > mark_byte) {
          region_property->offset = mark_byte;
        } else {
          region_property->offset = current_buffer.value.buffer->point_byte;
        }
        region_property->length = buffer_region_length(*current_buffer.value.buffer);

        if (gui_ctx()->reading) {
          add_property(&gui_ctx()->popup, region_property);
        } else {
          add_property(&new_gui_window->contents, region_property);
        }
      }
    }

    // Add all text properties defined in the window data structure itself.
    for (Atom property_it = properties; !nilp(property_it); property_it = cdr(property_it)) {
      Atom property = car(property_it);

      GUIStringProperty *new_property = calloc(1, sizeof(GUIStringProperty));

      if (integerp(car(property))) {
        new_property->id = car(property).value.integer;
      }
      new_property->offset = car(cdr(property)).value.integer;
      new_property->length = car(cdr(cdr(property))).value.integer;

      Atom fg = car(cdr(cdr(cdr(property))));
      new_property->fg.r = car(fg).value.integer;
      new_property->fg.g = car(cdr(fg)).value.integer;
      new_property->fg.b = car(cdr(cdr(fg))).value.integer;
      new_property->fg.a = car(cdr(cdr(cdr(fg)))).value.integer;

      Atom bg = car(cdr(cdr(cdr(cdr(property)))));
      new_property->bg.r = car(bg).value.integer;
      new_property->bg.g = car(cdr(bg)).value.integer;
      new_property->bg.b = car(cdr(cdr(bg))).value.integer;
      new_property->bg.a = car(cdr(cdr(cdr(bg)))).value.integer;

      // Add new property to list.
      add_property(&new_gui_window->contents, new_property);
    }

    Atom property_it = properties;
    Atom last_prop_it = nil;
    while (!nilp(property_it)) {
      Atom property = car(property_it);
      if (!integerp(car(property))) {
        if (nilp(last_prop_it)) {
          // Remove from beginning.
          // [a b c], last_prop_it=nil, property_it=a
          cdr(car(cdr(cdr(cdr(cdr(window)))))) = cdr(cdr(car(cdr(cdr(cdr(cdr(window)))))));
          // [b c], last_prop_it=nil, property_it=a
          property_it = cdr(property_it);
          // [b c], last_prop_it=nil, property_it=b
        } else {
          // Remove from middle.
          // [a b c], last_prop_it=a, property_it=b
          cdr(last_prop_it) = cdr(property_it);
          // [a c], last_prop_it=a, property_it=b
          property_it = cdr(property_it);
          // [a c], last_prop_it=a, property_it=c
        }
        continue;
      }
      last_prop_it = property_it;
      property_it = cdr(property_it);
    }

    if (gui_ctx()->reading) {
      gui_ctx()->popup.string = buffer_string(*current_buffer.value.buffer);
    }

    // Add to windows linked list.
    if (last_window) {
      last_window->next = new_gui_window;
    } else {
      gctx->windows = new_gui_window;
    }
    last_window = new_gui_window;
  }

  return do_gui(gctx);
}

int enter_lite_gui(void) {
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
