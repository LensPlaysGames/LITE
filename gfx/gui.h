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

#include <stddef.h>
#include <stdint.h>

typedef struct GUIColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
} GUIColor;
#define GUIColour GUIColor

typedef struct GUIStringProperty {
  size_t id;
  size_t offset;
  size_t length;
  GUIColor fg;
  GUIColor bg;
  struct GUIStringProperty *next;
} GUIStringProperty;

typedef struct GUIString {
  char *string;
  /// This is the amount of rows that should be skipped, from the
  /// beginning of the data string. Allows for vertical scrolling.
  size_t vertical_offset;
  /// This is the amount of columns that should be skipped at the start
  /// of each and every row. Allows for horizontal scrolling.
  size_t horizontal_offset;
  GUIStringProperty *properties;
} GUIString;

typedef struct GUIWindow {
  unsigned char z;
  unsigned char posx;
  unsigned char posy;
  unsigned char sizex;
  unsigned char sizey;
  GUIString contents;
  struct GUIWindow *next;
} GUIWindow;

typedef struct GUIContext {
  GUIStringProperty default_property;
  char *title; /// Graphical window title
  GUIWindow *windows;
  GUIString footline;
  GUIString popup;
  char reading;
} GUIContext;

enum CreateGUIReturnValue {
  CREATE_GUI_OK = 0,
  CREATE_GUI_ALREADY_CREATED = 3,
};

/** Attempt to initialize and create the GUI.
 *
 * Any value not specified below is assumed to be a failure.
 *
 * @retval CREATE_GUI_OK Successful creation of GUI.
 * @retval CREATE_GUI_ALREADY_CREATED Can not create two GUIs.
 */
int create_gui();
void destroy_gui();

/// Render a single frame based on graphical context.
void draw_gui(GUIContext *ctx);

/// Handle pending events (user input, quit, etc).
/// Returns boolean-like value denoting whether
/// execution should continue or not (0 == HALT).
int handle_events();

/// Do one iteration of the GUI based on graphical context.
int do_gui(GUIContext *ctx);

/// @return Zero upon success.
int change_font(char *path, size_t size);

/// @return Zero upon success.
int change_font_size(size_t size);

void window_size(size_t *width, size_t *height);

/// @return Zero upon success.
void change_window_size(size_t width, size_t height);

enum GFXWindowMode {
  GFX_WINDOW_MODE_WINDOWED = 0,
  GFX_WINDOW_MODE_FULLSCREEN,
};

void change_window_mode(enum GFXWindowMode mode);

enum GFXWindowVisible {
  GFX_WINDOW_VISIBLE = 0,
  GFX_WINDOW_INVISIBLE,
};

void change_window_visibility(enum GFXWindowVisible visible);

#endif /* GUI_H */
