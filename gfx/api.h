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

// Handlers for non-modifier characters.
// Hopefully 64 bits is enough for a character ;^).
// At this point we assume UTF8, but it's too early to matter.
void handle_character_dn(uint64_t c);
void handle_character_up(uint64_t c);

typedef enum GUIModifierKey {
  GUI_MODKEY_LCTRL,
  GUI_MODKEY_RCTRL,
  GUI_MODKEY_LALT,
  GUI_MODKEY_RALT,
  GUI_MODKEY_LSHIFT,
  GUI_MODKEY_RSHIFT,
  GUI_MODKEY_MAX,
} GUIModifierKey;


// Handlers for modifier keys.
void handle_modifier_dn(GUIModifierKey);
void handle_modifier_up(GUIModifierKey);

#endif /* API_H */
