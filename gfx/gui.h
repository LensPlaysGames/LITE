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

// TODO: Extend GUIContext (maybe even make it extensible).
//       Definitely split contents into a linked list of windows,
//       each with their own string of contents (or something similar).
typedef struct GUIContext {
  char *headline;
  char *contents;
  char *footline;
} GUIContext;

/// Returns 0 upon success.
int create_gui();

void destroy_gui();

/// Render a single frame based on graphical context.
void draw_gui(GUIContext *ctx);

void do_gui(int *open, GUIContext *ctx);

#endif /* GUI_H */
