#include <gui.h>

#include <stdlib.h>

int create_gui() {
  return CREATE_GUI_OK;
}

void destroy_gui() {
  ;
}

int handle_events() {
  return 1;
}

void draw_gui(GUIContext *ctx) {
  ;
}

int do_gui(GUIContext *ctx) {
  if (!ctx) return 0;
  draw_gui(ctx);
  return 0;
}

int change_font(const char *path, size_t size) {
  if (!path) return 1;
  return 1;
}

int change_font_size(size_t size) {
  return 1;
}

void window_size(size_t *width, size_t *height) {
  if (!width || !height) return;
  *width = 1;
  *height = 1;
}

void window_size_row_col(size_t *rows, size_t *cols) {
  *rows = 1;
  *cols = 1;
}

void change_window_size(size_t width, size_t height) {
  ;
}

void set_clipboard_utf8(const char *data) {
  ;
}

char *get_clipboard_utf8() {
  return calloc(1,1);
}

char has_clipboard_utf8() {
  return 0;
}

void change_window_mode(enum GFXWindowMode mode) {
  ;
}

void change_window_visibility(enum GFXWindowVisible visible) {
  ;
}
