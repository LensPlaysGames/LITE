/* api.h -- This is the other side of the LITE GFX library
 *
 * This header has functions that are implemented in the
 * graphics frontend, or at least must be stubbed out.
 *
 * This is in order to allow the backend a channel of
 * communication back for user input (among other things).
 */

#ifndef API_H
#define API_H

#include <stdint.h>

#include <gfx.h>

/// Get global GUI Context.
typedef struct GUIContext GUIContext;
GUIContext *gui_ctx(void);

extern char *getlitedir(void);

/** Handle a character down event sent from the OS to the application.
 * Characters are converted to strings. Most of the time the string is
 * just the bytes of a character, but certain strings are handled
 * specially.
 */
void handle_keydown(char *keystring);

typedef enum GUIModifierKey {
  GUI_MODKEY_LSUPER,
  GUI_MODKEY_RSUPER,
  GUI_MODKEY_LCTRL,
  GUI_MODKEY_RCTRL,
  GUI_MODKEY_LALT,
  GUI_MODKEY_RALT,
  GUI_MODKEY_LSHIFT,
  GUI_MODKEY_RSHIFT,
  GUI_MODKEY_MAX,
} GUIModifierKey;

// Handlers for modifier keys.

/// When called with GUI_MODKEY_MAX, all modifier keys are pressed down.
void handle_modifier_dn(GUIModifierKey);

/// When called with GUI_MODKEY_MAX, all modifier keys are released.
void handle_modifier_up(GUIModifierKey);

/// Enter LITE GFX, creating a window and entering appropriate loop.
int enter_lite_gui(void);

/// One iteration of the LITE GFX runtime loop.
int gui_loop(void);

/// Add PROPERTY to beginning of STRING properties linked list.
/// Returns boolean-like value (0 == failure).
/// NOTE: Used by LITE as well as backend.
int add_property(GUIString *string, GUIStringProperty *property);

#endif /* API_H */
