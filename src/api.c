#include <api.h>

#include <assert.h>
#include <buffer.h>
#include <error.h>
#include <environment.h>
#include <evaluation.h>
#include <gfx.h>
#include <gui.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <types.h>
#include <utility.h>

#if defined(TREE_SITTER)
#include <tree_sitter.h>
#endif

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
GUIContext *gui_ctx(void) { return gctx; }

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
  if (out == 0) return 0;
  out = handle_modifier(GUI_MODKEY_RSUPER, 1, &current_keymap, keybind_recurse_count);
  if (out == 0) return 0;
  out = handle_modifier(GUI_MODKEY_LCTRL,  1, &current_keymap, keybind_recurse_count);
  if (out == 0) return 0;
  out = handle_modifier(GUI_MODKEY_RCTRL,  1, &current_keymap, keybind_recurse_count);
  if (out == 0) return 0;
  out = handle_modifier(GUI_MODKEY_LALT,   1, &current_keymap, keybind_recurse_count);
  if (out == 0) return 0;
  out = handle_modifier(GUI_MODKEY_RALT,   1, &current_keymap, keybind_recurse_count);
  if (out == 0) return 0;
  out = handle_modifier(GUI_MODKEY_LSHIFT, 1, &current_keymap, keybind_recurse_count);
  if (out == 0) return 0;
  out = handle_modifier(GUI_MODKEY_RSHIFT, 1, &current_keymap, keybind_recurse_count);
  if (out == 0) return 0;
  env_set(*genv(), make_sym("CURRENT-KEYMAP"), current_keymap);
  return 1;
}

void handle_keydown(char *keystring) {
#ifdef LITE_DBG
  int debug_keybinding = env_non_nil(*genv(), make_sym("DEBUG/KEYBINDING"));
  if (debug_keybinding) printf("Keydown: %s\n", keystring ? keystring : "NULL");
#endif /* LITE_DBG */

  if (!keystring) return;
  const size_t keybind_recurse_limit = 256;
  size_t keybind_recurse_count = 0;

  // Get current keymap from global LISP environment.
  // This allows for two separate keypresses to further navigate the
  // keymap hierarchy: that is, if key "a" is bound to a keymap, and
  // within that keymap the key "b" is bound to the string "ab", then
  // first pressing "a" will cause CURRENT-KEYMAP to be updated to the
  // keymap bound to "a". Pressing "b" once more will find the
  // valid keybinding (string "ab") and insert its contents.
  Atom current_keymap = nil;
  env_get(*genv(), make_sym("CURRENT-KEYMAP"), &current_keymap);

  // Confidence Check: If the user has borked their current keymap,
  // somehow (accidental let binding, possibly), we reset a `nil`
  // keymap to the top-level root keymap (stored at "KEYMAP" in global
  // environment).
  if (nilp(current_keymap)) {
    env_get(*genv(), make_sym("KEYMAP"), &current_keymap);
    if (nilp(current_keymap)) {
      // At this point, it is most likely that LITE has been started
      // without any keymap configuration whatsoever (no standard
      // library), or the user has purposefully borked it.
      // TODO: Do something like restoring default keymap or something
      // that is more useful than just flopping around like a dead fish.
      // Maybe even crashing the program would be better than just
      // sitting there unresponsively. The user pressed a button, and
      // acknowledging that is important!
      fprintf(stdout, "ERROR: There is no bound keymap!\n");
    }
  }

#ifdef LITE_DBG
  if (debug_keybinding) {
    printf("Current keymap: ");
    pretty_print_atom(current_keymap);
    putchar('\n');
  }
#endif /* LITE_DBG */

  Atom current_buffer = nil;
  env_get(*genv(), make_sym("CURRENT-BUFFER"), &current_buffer);
  // TODO: If CURRENT-BUFFER is somehow borked, maybe we should wait
  // until a keybind actually requires it to fail. Either way, the way
  // we silently return here with no error or message to the user about
  // where their keypress went is bad.
  if (!bufferp(current_buffer)) {
    return;
  }

  // Basically, modifiers have specially-named keymaps that are
  // traversed in a specific order; this function does that.
  if (!handle_character_dn_modifiers(current_keymap, &keybind_recurse_count)) {
    return;
  }

  // ENTER THE LOOP
  // This is where it gets a bit messy, but it's not /too/ bad. The
  // gist of it is this: within the current-keymap, find the mapping
  // corresponding to the keystring we have recieved (the key pressed):
  // we call this a `keybind`. The keybind is then handled in different
  // ways based on it's type.
  //
  // POSSIBLE KEYBIND TYPES:
  // - nil       :: If a keystring is not bound within an alist, keybind
  //                will be nil. If we are in the root keymap, then the key
  //                simply isn't bound. Right now, the behaviour of
  //                pressing an unbound key is to just insert the keystring
  //                into the current buffer. Otherwise, if the current
  //                keymap is /not/ the root keymap, then we reset the
  //                current keymap to the root keymap and try again. I'm
  //                not so sure if this is correct/expected behaviour.
  //                It seems kind of confusing, but at the same time
  //                allows one to press "C-x" followed by "z", and when
  //                "z" isn't bound it will default to the root keymap
  //                (which inserts the character). It would probably
  //                make a million times more sense to just ignore the
  //                unbound keypress and give the user a message
  //                (somehow) like "blah is undefined keystring".
  // - string    :: Sets the keystring representing the pressed key to
  //                this new string, without modifying keymap(s) at all.
  //                If "a" is bound to "b" and "b" is bound to
  //                "(do-thing)", then pressing "a" will (eventually)
  //                call "(do-thing)". This also works in tandem with
  //                the behaviour regarding unbound keys, as if the
  //                user provides "I am a big boi!" as the keystring,
  //                it will almost certainly not be bound, and in that
  //                case it will insert itself into the buffer as is.
  //                This gives the /effect/ of strings just being data
  //                to insert while also keeping the ability to have a
  //                key rebound to another key, or have eight keys all
  //                bound to one virtual key that does something
  //                (like "<backspace>").
  // - symbol    :: If it's a symbol, we first check if it's "IGNORE"
  //                or "SELF-INSERT". IGNORE is self-explanatory.
  //                SELF-INSERT will insert the keystring itself into
  //                the current buffer.
  // - alist     :: A keystring bound to an alist is a "nested keymap".
  //                When a binding of this type is found, CURRENT-KEYMAP is
  //                updated to the bound alist.
  // - anything  :: Anything that isn't one of the types up above will
  //                be evaluated as LITE LISP, with the result of the
  //                evaluation being drawn to the footline.
  while (keystring && keybind_recurse_count < keybind_recurse_limit) {
    env_get(*genv(), make_sym("CURRENT-KEYMAP"), &current_keymap);

#ifdef LITE_DBG
    if (debug_keybinding) {
      printf("Current keymap: ");
      pretty_print_atom(current_keymap);
      putchar('\n');
    }
#endif /* LITE_DBG */

    Atom keybind = alist_get(current_keymap, make_string(keystring));

#ifdef LITE_DBG
    if (debug_keybinding) {
      printf("Got keybind: ");
      pretty_print_atom(keybind);
      putchar('\n');
    }
#endif /* LITE_DBG */

    if (alistp(keybind)) {
      // Nested keymap, rebind current keymap.
      if (debug_keybinding) printf("Nested keymap found, updating CURRENT-KEYMAP\n");
      env_set(*genv(), make_sym("CURRENT-KEYMAP"), keybind);
      break;
    }
    if (nilp(keybind)) {
      Atom root_keymap = nil;
      env_get(*genv(), make_sym("KEYMAP"), &root_keymap);
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
        fprintf(stdout, "Can not follow rebind of %s to empty string!\n", keystring);
        return;
      }
      keystring = (char *)keybind.value.symbol;
      // Go around again!
      keybind_recurse_count += 1;

#ifdef LITE_DBG
      if (debug_keybinding) printf("keybind_recurse_count: %zu\n", keybind_recurse_count);
#endif /* LITE_DBG */

      continue;
    }

#ifdef LITE_DBG
    if (debug_keybinding) {
      printf("Attempting to evaluate keybind: ");
      print_atom(keybind);
      putchar('\n');
    }
#endif /* LITE_DBG */

    Atom result = nil;
    Error err = evaluate_expression(keybind, *genv(), &result);
    if (err.type) {
      printf("KEYBIND ");
      print_error(err);
      update_gui_string(&gctx->footline, error_string(err));
      Atom root_keymap = nil;
      env_get(*genv(), make_sym("KEYMAP"), &root_keymap);
      env_set(*genv(), make_sym("CURRENT-KEYMAP"), root_keymap);
      return;
    }

    // After evaluation, we need to check if the user has quit, in
    // which case we need to immediately return in order to respect the
    // wishes of the user.
    if (user_quit) return;

#ifdef LITE_DBG
    if (debug_keybinding) {
      printf("Result: ");
      print_atom(result);
      putchar('\n');
    }
#endif /* LITE_DBG */

    // Set the contents of the footline based on the result of the evaluated keybind.
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
#ifdef LITE_GL
  default_default.bg.a = UINT8_MAX / 2 + UINT8_MAX / 3;
#endif

  default_default.offset = 0;
  default_default.length = 0;

  create_gui_property(GUI_PROP_ID_DEFAULT, &default_default);

  ctx->default_property.fg = default_default.fg;
  ctx->default_property.bg = default_default.bg;

  ctx->reading = 0;

  return ctx;
}

#ifdef TREE_SITTER

#if defined(_WIN32)
#include <windows.h>
#elif defined(__unix__)
#include <dlfcn.h>
#endif

/// Define `SO_EXIT` preprocessor directive to make errors exit the program
#if defined(SO_EXIT)
#define so_return_error exit(1)
#else
#define so_return_error return NULL
#endif

void *so_load(const char *library_path) {
  void *out = NULL;
#if defined(_WIN32)
  // Use WINAPI to load library
  HMODULE lib = LoadLibraryA(TEXT(library_path));
  if (lib == NULL) {
    fprintf(stdout, "Could not load tree sitter grammar library at \"%s\"\n", library_path);
    so_return_error;
  }
  out = lib;
#elif defined(__unix__)
  out = dlopen(library_path, RTLD_NOW);
  if (out == NULL) {
    fprintf(stdout, "Error from dlopen(\"%s\"):\n  \"%s\"\n", library_path, dlerror());
    so_return_error;
  }
  // Clear any existing error.
  dlerror();
#else
# error "Can not support dynamic loading on unrecognized system"
#endif
  return out;
}

void *so_get(void *data, const char *symbol) {
  void *out = NULL;
#if defined(_WIN32)
  // Get Tree Sitter language function address
  out = (void *)GetProcAddress(data, TEXT(symbol));
  if (out == NULL) {
    fprintf(stdout, "Could not load symbol \"%s\" from dynamically loaded library.\n", symbol);
    so_return_error;
  }
#elif defined(__unix__)
  out = (void *)dlsym(data, symbol);
  char *error = dlerror();
  if (error) {
    fprintf(stdout,
            "Could not load symbol \"%s\" from dynamically loaded library.\n"
            "  \"%s\"\n",
            symbol,
            error);
    so_return_error;
  }
#else
# error "Can not support dynamic loading on unrecognized system"
#endif
  return out;
}

void so_delete(void *data) {
#if defined(_WIN32)
  FreeLibrary(data);
#elif defined(__unix__)
  dlclose(data);
#else
# error "Can not support dynamic loading on unrecognized system"
#endif
}

void *so_load_ts(const char *language) {
  void *out = NULL;
  // Construct library path
#if defined(_WIN32)
  // ~/.tree_sitter/bin/libtree-sitter-<language>.dll
  const char *appdata_path = getenv("APPDATA");
  char *tree_sitter_bin = string_join(appdata_path, "/.tree_sitter/bin/");
  char *tmp = string_join("libtree-sitter-", language);
  char *library_path = string_trijoin(tree_sitter_bin, tmp, ".dll");
  free(tree_sitter_bin);
  free(tmp);
#elif defined(__APPLE__)
  // libtree-sitter-<language>.dylib
  // TODO: Tree sitter bin on MacOS?
  char *library_path = string_trijoin("libtree-sitter-", language, ".dylib");
#elif defined(__linux__)
  // ~/.tree_sitter/bin/libtree-sitter-<language>.so
  const char *home_path = getenv("HOME");
  char *tree_sitter_bin = string_join(home_path, "/.tree_sitter/bin/");
  char *tmp = string_trijoin("libtree-sitter-", language, ".so");
  char *library_path = string_join(tree_sitter_bin, tmp);
  free(tree_sitter_bin);
  free(tmp);

#else
# error "Can not support dynamic loading on unrecognized system"
#endif
  // Load library at path
  out = so_load(library_path);
  free(library_path);
  return out;
}

void *so_get_ts(void *data, const char *language) {
  void *out = NULL;
  // Construct function name
  char *lang_symbol = string_join("tree_sitter_", language);
  // Load symbol from library
  out = so_get(data, lang_symbol);
  free(lang_symbol);
  return out;
}

TreeSitterLanguage ts_langs[TS_LANG_MAX];
TreeSitterLanguage *ts_langs_find_lang(const char *lang) {
  for (int i = 0; i < TS_LANG_MAX; ++i) {
    if (ts_langs[i].used && ts_langs[i].lang && strcmp(lang, ts_langs[i].lang) == 0) {
      return ts_langs + i;
    }
  }
  return NULL;
}
TreeSitterLanguage *ts_langs_find_free(void) {
  for (int i = 0; i < TS_LANG_MAX; ++i) {
    if (!ts_langs[i].used) {
      return ts_langs + i;
    }
  }
  assert(0 && "Sorry, we don't support over TS_LANG_MAX tree sitter languages.");
  return NULL;
}
TreeSitterLanguage *ts_langs_new(const char *lang_string) {
  TreeSitterLanguage *lang = NULL;
  if ((lang = ts_langs_find_lang(lang_string))) {
    return lang;
  }
  lang = ts_langs_find_free();
  lang->used = 1;
  // FIXME: More error checks
  lang->lang = strdup(lang_string);
  lang->library_handle = so_load_ts(lang_string);
  lang->lang_func = (TSLanguage *(*)(void))so_get_ts(lang->library_handle, lang_string);
  lang->parser = ts_parser_new();
  lang->query_count = 0;
  lang->queries = NULL;
  ts_parser_set_language(lang->parser, lang->lang_func());
  return lang;
}
Error ts_langs_update_queries(const char *lang_string, struct Atom queries) {
  TreeSitterLanguage *lang = ts_langs_new(lang_string);
  // Free existing queries, if any.
  if (lang->query_count) {
    for (size_t i = 0; i < lang->query_count; ++i) {
      TreeSitterQuery ts_query = lang->queries[i];
      ts_query_delete(ts_query.query);
    }
    lang->query_count = 0;
    free(lang->queries);;
    lang->queries = NULL;
  }
  // Count queries
  for (Atom query_it = queries; pairp(query_it); query_it = cdr(query_it)) {
    lang->query_count += 1;
  }
  // If there are no queries, there is nothing to do.
  if (lang->query_count == 0) {
    MAKE_ERROR(err, ERROR_GENERIC, queries,
               "ts_langs_update_queries(): No queries supplied.",
               NULL);
    return err;
  }

  lang->queries = calloc(lang->query_count, sizeof *lang->queries);

  size_t i = 0;
  for (Atom query_it = queries; !nilp(query_it); query_it = cdr(query_it), ++i) {
    if (i >= lang->query_count) {
      MAKE_ERROR(err, ERROR_GENERIC, queries,
                 "Trouble updating tree sitter queries for %s: too many supplied (likely internal error)",
                 "Report this bug to the LITE developers with as much context and information as possible.");
      return err;
    }
    TreeSitterQuery *ts_it = lang->queries + i;
    Atom query = car(query_it);
    if (!pairp(query)) {
      // invalid query
      printf("ERROR in tree-sitter query specification: Query must be a list\n");
      continue;
    }
    Atom query_string = car(query);
    if (!stringp(query_string)) {
      // invalid query
      printf("ERROR in tree-sitter query specification: Query must be a string\n");
      continue;
    }

    if (!pairp(cdr(query))) {
      // invalid query
      printf("ERROR in tree-sitter query specification: Invalid amount of elements in query specification\n");
      continue;
    }

    Atom query_colors = car(cdr(query));
    if (!pairp(query_colors)) {
      // invalid query
      printf("ERROR in tree-sitter query specification: Invalid fg/bg color pair\n");
      continue;
    }

    // ((fg_r . (fg_g . (fg_b . (fg_a . nil)))) . ((bg_r . (bg_g . (bg_b . (bg_a . nil)))) . nil))

    // car: (fg_r . (fg_g . (fg_b . (fg_a . nil))))
    //   car: fg_r
    //   cdr: (fg_g . (fg_b . (fg_a . nil)))
    //     car: fg_g
    //     cdr: (fg_b . (fg_a . nil))
    //       car: fg_b
    //       cdr: (fg_a . nil)
    //         car: fg_a
    //         cdr: nil
    if (!pairp(car(query_colors))
        || !pairp(cdr(car(query_colors)))
        || !pairp(cdr(cdr(car(query_colors))))
        || !pairp(cdr(cdr(cdr(car(query_colors)))))
        || !integerp(car(car(query_colors)))
        || !integerp(car(cdr(car(query_colors))))
        || !integerp(car(cdr(cdr(car(query_colors)))))
        || !integerp(car(cdr(cdr(cdr(car(query_colors))))))
      ) {
      // invalid query
      printf("ERROR in tree-sitter query specification: Invalid foreground color\n");
      continue;
    }
    Atom query_fg = car(query_colors);
    Atom query_fg_r = car(query_fg);
    Atom query_fg_g = car(cdr(query_fg));
    Atom query_fg_b = car(cdr(cdr(query_fg)));
    Atom query_fg_a = car(cdr(cdr(cdr(query_fg))));


    // cdr: ((bg_r . (bg_g . (bg_b . (bg_a . nil)))))
    //   car: (bg_r . (bg_g . (bg_b . (bg_a . nil))))
    //     car: bg_r
    //     cdr: (bg_g . (bg_b . (bg_a . nil)))
    //       car: bg_g
    //       cdr: (bg_b . (bg_a . nil))
    //         car: bg_b
    //         cdr: (bg_a . nil)
    //           car: bg_a
    //           cdr: nil
    //   cdr: nil
    if (!pairp(car(cdr(query_colors)))
        || !pairp(cdr(car(cdr(query_colors))))
        || !pairp(cdr(cdr(car(cdr(query_colors)))))
        || !pairp(cdr(cdr(cdr(car(cdr(query_colors))))))
        || !integerp(car(car(cdr(query_colors))))
        || !integerp(car(cdr(car(cdr(query_colors)))))
        || !integerp(car(cdr(cdr(car(cdr(query_colors))))))
        || !integerp(car(cdr(cdr(cdr(car(cdr(query_colors)))))))
      ) {
      // invalid query
      printf("ERROR in tree-sitter query specification: Invalid background color\n");
      pretty_print_atom(cdr(query_colors));putchar('\n');
      continue;
    }
    Atom query_bg = car(cdr(query_colors));
    Atom query_bg_r = car(query_bg);
    Atom query_bg_g = car(cdr(query_bg));
    Atom query_bg_b = car(cdr(cdr(query_bg)));
    Atom query_bg_a = car(cdr(cdr(cdr(query_bg))));

    RGBA fg = RGBA_VALUE(query_fg_r.value.integer,
                         query_fg_g.value.integer,
                         query_fg_b.value.integer,
                         query_fg_a.value.integer);

    RGBA bg = RGBA_VALUE(query_bg_r.value.integer,
                         query_bg_g.value.integer,
                         query_bg_b.value.integer,
                         query_bg_a.value.integer);

    uint32_t query_error_offset = 0;
    TSQueryError query_error = TSQueryErrorNone;
    TSQuery *new_query = ts_query_new
                           (ts_parser_language(lang->parser),
                            query_string.value.symbol,
                            (uint32_t)strlen(query_string.value.symbol),
                            &query_error_offset, &query_error);

    if (query_error != TSQueryErrorNone) {
      fprintf(stderr, "Error in query at offset %"PRIu32"\n%s\n", query_error_offset, query_string.value.symbol);
      MAKE_ERROR(err, ERROR_GENERIC, query_string,
                 "Error in tree-sitter query",
                 NULL);
      return err;
    }

    ts_it->query = new_query;
    ts_it->fg = fg;
    ts_it->bg = bg;
  }
  return ok;
}
void ts_langs_delete_one(TreeSitterLanguage *lang) {
  if (!lang || !lang->used) return;
  ts_parser_delete(lang->parser);
  if (lang->library_handle) {
    so_delete(lang->library_handle);
  }
  free(lang->lang);
  lang->used = 0;
}
void ts_langs_delete(const char *lang_string) {
  ts_langs_delete_one(ts_langs_find_lang(lang_string));
}
void ts_langs_delete_all() {
  for (int i = 0; i < TS_LANG_MAX; ++i) {
    ts_langs_delete_one(ts_langs + i);
  }
}

#define GUI_COLOR_FROM_RGBA(name, rgba) GUIColor (name); (name).r = RGBA_R(rgba); (name).g = RGBA_G(rgba); (name).b = RGBA_B(rgba); (name).a = RGBA_A(rgba)

void add_property_from_query_matches(GUIWindow *window, TSNode root, size_t offset, size_t narrow_begin, size_t narrow_end, TSQuery *ts_query, RGBA fg, RGBA bg) {
  if (!ts_query) return;

  TSQueryCursor *query_cursor = ts_query_cursor_new();
  ts_query_cursor_exec(query_cursor, ts_query, root);

  GUI_COLOR_FROM_RGBA(fg_color, fg);
  GUI_COLOR_FROM_RGBA(bg_color, bg);

  // Foreach match...
  TSQueryMatch match;
  while (ts_query_cursor_next_match(query_cursor, &match)) {
    // Foreach capture...
    for (uint16_t i = 0; i < match.capture_count; ++i) {
      TSQueryCapture capture = match.captures[i];

      size_t start = ts_node_start_byte(capture.node) + offset;
      // NOTE: This assumes captures are in order, from beginning to end.
      if (start > narrow_end) break;

      size_t end = ts_node_end_byte(capture.node) + offset;
      size_t length = end - start;
      if (start < narrow_begin && end < narrow_begin) continue;

      GUIStringProperty *new_property = malloc(sizeof *new_property);
      new_property->offset = start;
      new_property->length = length;
      new_property->fg = fg_color;
      new_property->bg = bg_color;

      // Add new property to list.
      add_property(&window->contents, new_property);
    }
  }

  ts_query_cursor_delete(query_cursor);
}

#endif

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

#if defined(TREE_SITTER)
  Atom ts_language = nil;
  err = env_get(*genv(), make_sym("TREE-SITTER-LANGUAGE"), &ts_language);
  if (err.type && err.type != ERROR_NOT_BOUND) {
    print_error(err);
  }
#endif

  // Free all GUIWindows!
  GUIWindow *gui_window = gctx->windows;
  while (gui_window) {
    free_properties(gui_window->contents);
    free(gui_window->contents.string);
    GUIWindow *next_gui_window = gui_window->next;
    free(gui_window);
    gui_window = next_gui_window;
  }
  gctx->windows = NULL;

  if (gctx->popup.string) {
    free_properties(gctx->popup);
    gctx->popup.properties = NULL;
    free(gctx->popup.string);
    gctx->popup.string = NULL;
  }

  integer_t index = 0;
  GUIWindow *last_window = gctx->windows;
  for (Atom window_it = window_list; !nilp(window_it); window_it = cdr(window_it), ++index) {
    Atom window = car(window_it);
    // Expected format:
    // (z (posx . posy) (sizex . sizey) (scrollx . scrolly) (contents . properties))
    // (z . ((posx . posy) . ((sizex . sizey) . ((scrollx . scrolly) . ((contents . properties) . nil)))))
    // property:
    // (id offset length (fg.r fg.g fg.b fg.r) (bg.r bg.g bg.b bg.a))
    // (id . (offset . (length . ((fg.r fg.g fg.b fg.r) . ((bg.r bg.g bg.b bg.a) . nil)))))
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
          // TODO: Allow for windows with contents other than buffers.
          && bufferp(car(car(cdr(cdr(cdr(cdr(window)))))))
          ))
      continue;

    GUIWindow *new_gui_window = calloc(1, sizeof(GUIWindow));

    // Set integer values
    new_gui_window->z     = (uint8_t)car(window).value.integer;
    new_gui_window->posx  = (uint8_t)car(car(cdr(window))).value.integer;
    new_gui_window->posy  = (uint8_t)cdr(car(cdr(window))).value.integer;
    new_gui_window->sizex = (uint8_t)car(car(cdr(cdr(window)))).value.integer;
    new_gui_window->sizey = (uint8_t)cdr(car(cdr(cdr(window)))).value.integer;
    new_gui_window->contents.horizontal_offset = (size_t)car(car(cdr(cdr(cdr(window))))).value.integer;
    new_gui_window->contents.vertical_offset   = (size_t)cdr(car(cdr(cdr(cdr(window))))).value.integer;

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
    Atom contents = car(car(cdr(cdr(cdr(cdr(window))))));
    char *contents_string = NULL;
    size_t contents_length = 0;
    char contents_modified = 1;
    if (bufferp(contents)) {
      contents_string = buffer_string(*contents.value.buffer);
      contents_length = contents.value.buffer->rope->weight;
      contents_modified = contents.value.buffer->modified;
      // If buffer needs redrawn, unset it's status, as it is about to be drawn.
      if (contents.value.buffer->needs_redraw) {
        contents.value.buffer->needs_redraw = 0;
      }
    } else if ((stringp(contents) || symbolp(contents)) && contents.value.symbol) {
      contents_string = strdup(contents.value.symbol);
      contents_length = strlen(contents_string);
    }
    new_gui_window->contents.string = contents_string;

    // NOTE: This optimisation only works for single-window setups, but
    // it doesn't cause under-rendering with multiple window setups, just
    // overrendering.
    // FIXME: We should eventually figure out a way to know if a window's
    // contents need redisplayed that aren't this squirrely and complex.
    static Atom last_contents = {ATOM_TYPE_NIL, {0}, NULL, NULL};
    if (nilp(compare_atoms(contents, last_contents))) {
      contents_modified = 1;
    }
    last_contents = contents;

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

      // Extend cursor property to include all utf8 continuation bytes
      // TODO: Make this a user-configurable option. i.e. the byte length of
      // the cursor depends on the byte length of the character at the cursor in
      // a given encoding. So the user option would be "default encoding" or
      // something like that, and that would let us know if we need to parse
      // utf8 continuation bytes, utf16 surrogate pairs, etc.
      char byte = rope_index(current_buffer.value.buffer->rope,
                             current_buffer.value.buffer->point_byte + cursor_property->length);
      // Max amount of iterations, protect against infinite loop when continuation byte at end...
      char protect = 3;
      // byte & 0b10000000 && !(byte & 0b01000000)
      while (--protect >= 0 && byte & 128 && !(byte & 64)) {
        cursor_property->length++;
        byte = rope_index(current_buffer.value.buffer->rope,
                          current_buffer.value.buffer->point_byte + cursor_property->length);
      }

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
    Atom properties = cdr(car(cdr(cdr(cdr(cdr(window))))));
    for (Atom property_it = properties; !nilp(property_it); property_it = cdr(property_it)) {
      Atom property = car(property_it);

      GUIStringProperty *new_property = calloc(1, sizeof(GUIStringProperty));

      if (integerp(car(property))) {
        integer_t id = car(property).value.integer;
        if (id < 0) id = 0;
        new_property->id = (size_t)id;
      }

      // TODO/FIXME: Do we need to handle negatives everywhere here? Is
      // cast enough?

      new_property->offset = (size_t)car(cdr(property)).value.integer;
      new_property->length = (size_t)car(cdr(cdr(property))).value.integer;

      Atom fg = car(cdr(cdr(cdr(property))));
      new_property->fg.r = (uint8_t)car(fg).value.integer;
      new_property->fg.g = (uint8_t)car(cdr(fg)).value.integer;
      new_property->fg.b = (uint8_t)car(cdr(cdr(fg))).value.integer;
      new_property->fg.a = (uint8_t)car(cdr(cdr(cdr(fg)))).value.integer;

      Atom bg = car(cdr(cdr(cdr(cdr(property)))));
      new_property->bg.r = (uint8_t)car(bg).value.integer;
      new_property->bg.g = (uint8_t)car(cdr(bg)).value.integer;
      new_property->bg.b = (uint8_t)car(cdr(cdr(bg))).value.integer;
      new_property->bg.a = (uint8_t)car(cdr(cdr(cdr(bg)))).value.integer;

      // Add new property to list.
      add_property(&new_gui_window->contents, new_property);
    }

#if defined(TREE_SITTER)
    // Add properties from tree sitter

    if (stringp(ts_language)) {
      TreeSitterLanguage *language = ts_langs_new(ts_language.value.symbol);
      if (language && language->query_count) {

        // TODO: Get rid of static variables here...
        static size_t last_frame_vertical_offset = (size_t)-1;
        static size_t last_frame_horizontal_offset = (size_t)-1;
        char view_modified = new_gui_window->contents.vertical_offset != last_frame_vertical_offset
                             || new_gui_window->contents.horizontal_offset != last_frame_horizontal_offset;
        last_frame_vertical_offset = new_gui_window->contents.vertical_offset;
        last_frame_horizontal_offset = new_gui_window->contents.horizontal_offset;

        static size_t parse_offset = 0;
        static char *begin_visual_range = NULL;
        static char *end_visual_range = NULL;
        if (!language->tree || contents_modified || view_modified) {
          if (language->tree) {
            ts_tree_delete(language->tree);
          }

          begin_visual_range = contents_string;
          for (size_t i = 0; i < new_gui_window->contents.vertical_offset; ++i) {
            char *new_begin = strchr(begin_visual_range + 1, '\n');
            if (!new_begin) break;
            begin_visual_range = new_begin;
          }
          end_visual_range = begin_visual_range;
          size_t rows = 0;
          size_t cols = 0;
          window_size_row_col(&rows, &cols);
          for (size_t i = 0; i <= rows; ++i) {
            char *new_end = strchr(end_visual_range + 1, '\n');
            if (!new_end) break;
            end_visual_range = new_end;
          }

          // FIXME: The reason for this "moving parser" optimisation is
          // because it is really slow to iterate many matches, even if we
          // don't do anything with them. That is why we reduce the size
          // of the input file, to get less matches to appear, and for it
          // to not take so long iterating over them. However, this is also
          // the cause of highlighting bugs due to the parser not really
          // being given a valid starting point...

#ifndef TREE_SITTER_MAX_PARSE_LENGTH
// This many bytes should be more than can be realistically displayed in the window, at once.
# define TREE_SITTER_MAX_PARSE_LENGTH (2 << 13)
#endif
#ifndef TREE_SITTER_MIN_PARSE_CONTEXT
// This many lines of context above cursor should hopefully be enough.
# define TREE_SITTER_MIN_PARSE_CONTEXT (2 << 7)
#endif
          // Limit max length of parsed contents, making sure to parse contents related to what is in view.
          if (contents_length > TREE_SITTER_MAX_PARSE_LENGTH) {
            const char *parse_string = contents_string;
            size_t parse_length = contents_length;
            const char *new_parse_string = NULL;
            if (new_gui_window->contents.vertical_offset > TREE_SITTER_MIN_PARSE_CONTEXT) {
              size_t upper_bound = new_gui_window->contents.vertical_offset - TREE_SITTER_MIN_PARSE_CONTEXT;
              for (size_t i = 0; i < upper_bound; ++i) {
                new_parse_string = strchr(parse_string + 1, '\n');
                if (!new_parse_string) break;
                parse_length -= (size_t)(new_parse_string - parse_string);
                parse_string = new_parse_string;
              }
            }
            parse_offset = (size_t)(parse_string - contents_string);
            if (parse_length > TREE_SITTER_MAX_PARSE_LENGTH) {
              parse_length = TREE_SITTER_MAX_PARSE_LENGTH;
            }
            language->tree = ts_parser_parse_string(language->parser, NULL, parse_string, (uint32_t)parse_length);
          } else {
            parse_offset = 0;
            begin_visual_range = NULL;
            end_visual_range = NULL;
            language->tree = ts_parser_parse_string(language->parser, NULL, contents_string, (uint32_t)contents_length);
          }
        }
        for (size_t i = 0; i < language->query_count; ++i) {
          TreeSitterQuery *query = language->queries + i;
          size_t begin = begin_visual_range ? (size_t)(begin_visual_range - contents_string) : 0;
          size_t end = end_visual_range ? (size_t)(end_visual_range - contents_string) : SIZE_MAX;
          add_property_from_query_matches(new_gui_window, ts_tree_root_node(language->tree),
                                          parse_offset, begin, end,
                                          query->query, query->fg, query->bg);
        }
      }
    }
#endif /* #if defined(TREE_SITTER) */

    // Remove properties with a non-integer ID from property list of
    // window. These are known as *once* properties, and are very
    // useful for dynamic highlighting every redisplay (in conjunction
    // with `refresh-hook`).
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

    if (gctx->reading) {
      gctx->popup.string = buffer_string(*current_buffer.value.buffer);
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
#if defined(TREE_SITTER)
  ts_langs_delete_all();
#endif
  destroy_gui();
  return 0;
}
