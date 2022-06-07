/* api.h -- This is the other side of the LITE GFX library
 *
 * This header has functions that are implemented in the
 * graphics frontend, or at least must be stubbed out.
 *
 * This is in order to allow the backend a channel of
 * communication back for user input (among other things).b
 */

#ifndef API_H
#define API_H

#include <stdint.h>

// Keydown/keyup events should call these handlers.
// Hopefully 64 bits is enough for a character ;^).
void handle_character_dn(uint64_t c);
void handle_character_up(uint64_t c);

#endif /* API_H */
