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
} GUIColor, GUIColour;

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
  /// The character that a carriage return (\r) will be rendered as.
  char cr_char;
} GUIContext;

enum CreateGUIReturnValue {
  CREATE_GUI_OK = 0,
  CREATE_GUI_ALREADY_CREATED = 3,
  CREATE_GUI_ERR,
};

/** Attempt to initialize and create the GUI.
 *
 * Any value not specified below is assumed to be a failure.
 *
 * @retval CREATE_GUI_OK Successful creation of GUI.
 * @retval CREATE_GUI_ALREADY_CREATED Can not create two GUIs.
 * @retval CREATE_GUI_ERR Attempted to but could not create GUI.
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
int change_font(const char *path, size_t size);

/// @return Zero upon success.
int change_font_size(size_t size);

/// @return Window size in pixels.
void window_size(size_t *width, size_t *height);

/// @return Window size in character rows and columns, approximately.
void window_size_row_col(size_t *rows, size_t *cols);

/// @return Zero upon success.
void change_window_size(size_t width, size_t height);

/// Set the data on the clipboard to this UTF8 string.
void set_clipboard_utf8(const char *data);

/** Get the data on the clipboard to this UTF8 string.
 *
 * Do not forget to free the returned string---ownership is given to
 * the callee.
 */
char *get_clipboard_utf8();

/// @return 1 if clipboard has data on it, otherwise 0.
char has_clipboard_utf8();

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

#ifdef LITE_GL
/// @return zero upon success.
int change_shaders(const char *vertex_shader_source, const char *fragment_shader_source);
#endif

#endif /* GUI_H */
