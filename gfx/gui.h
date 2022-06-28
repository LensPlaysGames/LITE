/* gui.h -- This is the LITE GFX library
 *
 * This file contains only the interface details,
 * while each subdirectory contains a suitable
 * backend that implements these features.
 *
 * The idea is to keep LITE platform-agnostic,
 * the best solution is to provide a hot-swap
 * GUI-backend.
 */

#ifndef GUI_H
#define GUI_H

#include <stdint.h>

typedef struct GUIColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
} GUIColor;

typedef struct GUIStringProperty {
  size_t offset;
  size_t length;
  GUIColor fg;
  GUIColor bg;
  struct GUIStringProperty *next;
} GUIStringProperty;

typedef struct GUIString {
  char *string;
  GUIStringProperty *properties;
} GUIString;

// TODO: Extend GUIContext (maybe even make it extensible).
//       Definitely split contents into a linked list of windows,
//       each with their own string of contents (or something similar).
typedef struct GUIContext {
  char *title; /// Graphical window title
  GUIString headline;
  GUIString contents;
  GUIString footline;
  GUIStringProperty default_property;
} GUIContext;

/// Returns 0 upon success.
int create_gui();
void destroy_gui();

/// Render a single frame based on graphical context.
void draw_gui(GUIContext *ctx);

/// Handle pending events (user input, quit, etc).
/// Returns boolean-like value denoting whether
/// execution should continue or not (0 == HALT).
int handle_events();

/// Do one iteration of the GUI based on graphical context.
void do_gui(int *open, GUIContext *ctx);

#endif /* GUI_H */
