#include <api.h>
#include <gui.h>
#include <utility.h>
#include <keystrings.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_clipboard.h>
#include <SDL_events.h>
#include <SDL_keycode.h>
#include <SDL_rect.h>
#include <SDL_render.h>
#include <SDL_stdinc.h>
#include <SDL_surface.h>
#include <SDL_ttf.h>
#include <SDL_video.h>
#include <SDL_version.h>

// These are backup, in case no default property colors are set in the context.
static SDL_Color bg = { 18, 18, 18, UINT8_MAX };
static SDL_Color fg = { UINT8_MAX, UINT8_MAX, UINT8_MAX, UINT8_MAX };

static SDL_Window *gwindow = NULL;
static SDL_Renderer *grender = NULL;
static SDL_Texture *texture = NULL;

static TTF_Font *font = NULL;
static size_t font_height = 0;

// Returns GUI_MODKEY_MAX for any key that is not a modifier,
// otherwise returns the corresponding GUI_MODKEY_* enum.
static inline GUIModifierKey is_modifier(SDL_Keycode key) {
  switch (key) {
  default:          return GUI_MODKEY_MAX;
  case SDLK_LGUI:   return GUI_MODKEY_LSUPER;
  case SDLK_RGUI:   return GUI_MODKEY_RSUPER;
  case SDLK_LCTRL:  return GUI_MODKEY_LCTRL;
  case SDLK_RCTRL:  return GUI_MODKEY_RCTRL;
  case SDLK_LALT:   return GUI_MODKEY_LALT;
  case SDLK_RALT:   return GUI_MODKEY_RALT;
  case SDLK_LSHIFT: return GUI_MODKEY_LSHIFT;
  case SDLK_RSHIFT: return GUI_MODKEY_RSHIFT;
  }
}

/// Return span with OFFSET and LENGTH from STRING as
/// a heap-allocated, null-terminated string.
static inline char *allocate_string_span
(char *string , size_t offset , size_t length) {
  if (!string) {
    return NULL;
  }
  char *out = malloc(length + 1);
  if (!out) {
    return NULL;
  }
  size_t len = strlen(string);
  if (length > len) {
    length = len;
  }
  memcpy(out, string + offset, length);
  out[length] = '\0';
  return out;
}

static inline size_t count_lines(char *str) {
  if (!str) {
    return 0;
  }
  size_t line_count = 0;
  char *string_iterator = str;
  do {
    line_count += 1;
    string_iterator = strchr(string_iterator + 1, '\n');
  } while (string_iterator);
  return line_count;
}

static inline void draw_bg() {
  // NOTE: Alpha has no effect :(.
  SDL_SetRenderDrawColor(grender, bg.r, bg.g, bg.b, bg.a);
  SDL_RenderClear(grender);
}

int change_font(const char *path, size_t size) {
  if (!path) { return 1; }

  TTF_Font *working_font = NULL;
  char *working_path = NULL;

  // Search current directory.
  working_font = TTF_OpenFont(path, size);
  if (working_font) {
    font = working_font;
    return 0;
  }

  // Search install directory.
  char *dir = getlitedir();
  if (dir) {

    working_path = string_trijoin(dir, "/gfx/fonts/apache/", path);
    free(dir);
    if (!working_path) { return 2; }
    working_font = TTF_OpenFont(working_path, size);
    free(working_path);
    if (working_font) {
      font = working_font;
      return 0;
    }

    working_path = string_trijoin(dir, "/gfx/fonts/", path);
    free(dir);
    if (!working_path) { return 2; }
    working_font = TTF_OpenFont(working_path, size);
    free(working_path);
    if (working_font) {
      font = working_font;
      return 0;
    }

    working_path = string_trijoin(dir, "/gfx/", path);
    free(dir);
    if (!working_path) { return 2; }
    working_font = TTF_OpenFont(working_path, size);
    free(working_path);
    if (working_font) {
      font = working_font;
      return 0;
    }

    working_path = string_trijoin(dir, "/", path);
    free(dir);
    if (!working_path) { return 2; }
    working_font = TTF_OpenFont(working_path, size);
    free(working_path);
    if (working_font) {
      font = working_font;
      return 0;
    }

  }

# if defined (_WIN32) || defined (_WIN64)

  working_path = string_join("C:/Windows/fonts/", path);
  if (!working_path) { return 2; }
  working_font = TTF_OpenFont(working_path, size);
  free(working_path);
  if (working_font) {
    font = working_font;
    return 0;
  }

# elif defined (__unix)

  working_path = string_join("/usr/share/fonts/", path);
  if (!working_path) { return 2; }
  working_font = TTF_OpenFont(working_path, size);
  free(working_path);
  if (working_font) {
    font = working_font;
    return 0;
  }

  working_path = string_join("/usr/local/share/fonts/", path);
  if (!working_path) { return NULL; }
  working_font = TTF_OpenFont(working_path, size);
  free(working_path);
  if (working_font) {
    font = working_font;
    return 0;
  }

# endif

  return 10;
}

int change_font_size(size_t size) {
# if SDL_TTF_VERSION_ATLEAST(2,0,18)
  int status = TTF_SetFontSize(font, size);
  if (status == 0) {
    font_height = TTF_FontHeight(font);
    return 0;
  }
  return 1;
#endif
}

static int created_gui_marker = 0;
int create_gui() {
  if (created_gui_marker != 0) { return CREATE_GUI_ALREADY_CREATED; }
  created_gui_marker = 1;
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    printf("GFX::SDL:ERROR: Failed to initialize SDL.\n");
    return 1;
  }
  gwindow = SDL_CreateWindow
    ("SDL2 Window"
     , SDL_WINDOWPOS_UNDEFINED
     , SDL_WINDOWPOS_UNDEFINED
     , 640, 480
     , SDL_WINDOW_RESIZABLE
     | SDL_WINDOW_INPUT_FOCUS
     | SDL_WINDOW_HIDDEN
     );
  if (!gwindow) {
    printf("GFX::SDL:ERROR: Could not create SDL window.\n");
    return 1;
  }
  grender = SDL_CreateRenderer(gwindow, -1, SDL_RENDERER_ACCELERATED);
  if (!grender) {
    printf("GFX::SDL:ERROR: Could not create SDL renderer.\n");
    return 1;
  }
  // Activate alpha blending.
  if (SDL_SetRenderDrawBlendMode(grender, SDL_BLENDMODE_BLEND) != 0) {
    return 1;
  }
  printf("GFX::SDL: SDL Initialized\n");
  if (TTF_Init() != 0) {
    printf("GFX::SDL:ERROR: Failed to initialize SDL_ttf.\n");
    return 1;
  }
  // TODO: Do not hard-code font size!
  // Attempt to load font(s) included with LITE distribution.
  const size_t font_size = 18;
  if (!font) { change_font("DroidSansMono.ttf"      , font_size); }
  if (!font) { change_font("RobotoMono-Regular.ttf" , font_size); }
  if (!font) { change_font("DroidSans.ttf"          , font_size); }
  if (!font) { change_font("Tinos-Regular.ttf"      , font_size); }
  // At this point, hail mary for a system font.
  if (!font) { change_font("UbuntuMono-R.ttf"       , font_size); }
  if (!font) { change_font("Arial.ttf"              , font_size); }
  // Finally, if no font was found anywhere, error out.
  if (!font) {
    printf("GFX::SDL: SDL_ttf could not open a font.\n"
           "        : %s\n"
           , TTF_GetError()
           );
    return 1;
  }
  int fheight = TTF_FontHeight(font);
  if (fheight <= 0) {
    printf("GFX::SDL: SDL_ttf could not get height of font.\n"
           "        : %s\n"
           , TTF_GetError()
           );
    return 1;
  }
  font_height = fheight;
  printf("GFX::SDL: SDL_ttf Initialized\n");
  return CREATE_GUI_OK;
}

static inline void gui_color_to_sdl(SDL_Color *dst, GUIColor *src) {
  dst->r = src->r;
  dst->g = src->g;
  dst->b = src->b;
  dst->a = src->a;
}

static inline SDL_Rect *rect_make_empty(SDL_Rect *rect) {
  if (!rect) { return NULL; }
  memset(rect, 0, sizeof(SDL_Rect));
  return rect;
}

#define SDL_RECT_EMPTY(n) SDL_Rect (n); do { \
    (n).x = 0;                               \
    (n).y = 0;                               \
    (n).w = 0;                               \
    (n).h = 0;                               \
  } while (0);

static inline SDL_Rect *rect_copy(SDL_Rect *dst, SDL_Rect *src) {
  if (!dst || !src) { return NULL; }
  memcpy(dst, src, sizeof(SDL_Rect));
  return dst;
}

static inline SDL_Rect *rect_copy_position(SDL_Rect *dst, SDL_Rect *src) {
  if (!dst || !src) { return NULL; }
  dst->x = src->x;
  dst->y = src->y;
  return dst;
}

static inline SDL_Rect *rect_copy_size(SDL_Rect *dst, SDL_Rect *src) {
  if (!dst || !src) { return NULL; }
  dst->w = src->w;
  dst->h = src->h;
  return dst;
}

static inline void draw_gui_string_into_surface_within_rect
(GUIString gui_string, SDL_Surface *surface, SDL_Rect *rect, char cr_char) {
  if (!gui_string.string || gui_string.string[0] == '\0' || !surface || !rect) {
    return;
  }
  SDL_Surface *text_surface = NULL;
  // srcrect is the rectangle within text_surface that will be copied from.
  SDL_RECT_EMPTY(srcrect);
  rect_copy_size(&srcrect, rect);

  // Iterator into contents of GUIString.
  char *string = gui_string.string;
  // Byte offset of `string` iterator into GUIString.
  size_t offset = 0;
  // Byte offset of newline previous to `offset` in GUIString.
  // Used in start of line calculation. `start_of_line_offset = last_newline_offset + 1`
  size_t last_newline_offset = -1;

  // Handle vertical line offset by skipping by newlines.
  size_t vertical_skip_count = gui_string.vertical_offset;
  while (vertical_skip_count) {
    if (*string == '\n') {
      last_newline_offset = offset;
      vertical_skip_count -= 1;
    }
    if (*string == '\0') {
      // Vertical scrolling has skipped entire GUIString.
      return;
    }
    string += 1;
    offset += 1;
  }

  text_surface = SDL_CreateRGBSurfaceWithFormat
    (0, rect->w, rect->h, 32, SDL_PIXELFORMAT_RGBA32);
  if (!text_surface) { return; }
  // Destination within text_surface to render text into.
  // SDL_BlitSurface overrides the destination rect w and h, so we
  // must use a copy to prevent clobbering our data.
  SDL_Rect destination = srcrect;
  SDL_Rect destination_copy = destination;
  char cr_skip = 0;
  while (1) {
    // TODO: What to display \r as when cr_char isn't valid?
    if (*string == '\r') {
      if (cr_char >= ' ') {
        *string = cr_char;
      } else if (*(string + 1) == '\n') {
        cr_skip = 1;
      }
    } else if (*string == '\n' || *string == '\0') {
      // Byte offset of start of line we are currently at the end of.
      // Add horizontal offset here. If that makes start past or equal
      // to end, skip line but increment destination height.
      size_t start_of_line_offset = gui_string.horizontal_offset + last_newline_offset + 1;
      size_t line_height = font_height;

      if (start_of_line_offset > offset) {
        goto at_end_of_line;
      }

      // Render entire line using defaults.
      // Calculate amount of bytes within the current line.
      size_t bytes_to_render = offset - start_of_line_offset;
      if (cr_skip) {
        cr_skip = 0;
        if (bytes_to_render) {
          bytes_to_render -= 1;
        }
      }
      char *line_text = allocate_string_span
        (gui_string.string, start_of_line_offset, bytes_to_render);
#ifndef NDEBUG
      if (!line_text) {
        fprintf(stderr,
                "GFX::SDL2::ERROR: Could not allocate string span for line text.");
        return;
      }
#endif
      // Skip empty lines.
      if (line_text[0] != '\0') {
        // Use FreeType subpixel LCD rendering, if possible.
#       if SDL_TTF_VERSION_ATLEAST(2,20,0) && !_WIN32
        SDL_Surface *line_text_surface =
          TTF_RenderUTF8_LCD(font, line_text, fg, bg);
#       else
        SDL_Surface *line_text_surface =
          TTF_RenderUTF8_Shaded(font, line_text, fg, bg);
#       endif
        if (line_text_surface) {
          // TODO: Handle BlitSurface failure everywhere (0 == success).
          SDL_BlitSurface(line_text_surface, NULL, text_surface
                          , rect_copy(&destination_copy,&destination));
          SDL_FreeSurface(line_text_surface);
        }
      }
      // Render text properties over current line.
      for (GUIStringProperty *it = gui_string.properties; it; it = it->next) {
        // Skip if property is not within current line.
        if (it->offset > offset || it->offset + it->length <= start_of_line_offset) {
          continue;
        }
        // How far the text property starts within the current line (index).
        size_t offset_within_line = 0;
        if (it->offset > start_of_line_offset) {
          offset_within_line = it->offset - start_of_line_offset;
        }
        // Get draw position.
        SDL_RECT_EMPTY(property_draw_position);
        property_draw_position.x = 0;
        property_draw_position.y = destination.y;
        property_draw_position.w = destination.w;
        property_draw_position.h = destination.h;
        if (offset_within_line != 0) {
          char *text_before = allocate_string_span
            (line_text, 0, offset_within_line);
          // Get width of text before beginning of text property.
          TTF_SizeUTF8(font, text_before,
                       &property_draw_position.x,
                       &property_draw_position.y);
          property_draw_position.y = destination.y;
          // Width must take into account new draw position X offset.
          property_draw_position.w -= property_draw_position.x;
          free(text_before);
        }
        // Calculate the length of the propertized text, in bytes.
        // We can't just use the iterator's length because of
        // multi-line text properties.
        size_t propertized_text_length = it->length;
        if (it->offset < start_of_line_offset) {
          propertized_text_length -= start_of_line_offset - it->offset;
        }
        // If property extends past end of line, truncate it.
        if (it->offset + it->length > offset) {
          propertized_text_length = offset - it->offset;
        }
        // Allocate the text that will has a property applied to it as
        // a seperate string that can be fed to SDL_ttf for rendering.
        char *propertized_text = allocate_string_span
          (line_text, offset_within_line, propertized_text_length);
        if (propertized_text) {
          // Handle empty lines propertized (insert space).
          if (propertized_text[0] == '\0') {
            free(propertized_text);
            propertized_text = malloc(2);
            assert(propertized_text && "Could not display empty string");
            propertized_text[0] = ' ';
            propertized_text[1] = '\0';
          } else if (propertized_text[0] == '\n') {
            propertized_text[0] = ' ';
          }
          // Get colors for propertized text from text property.
          SDL_Color prop_fg;
          SDL_Color prop_bg;
          gui_color_to_sdl(&prop_fg, &it->fg);
          gui_color_to_sdl(&prop_bg, &it->bg);
          // Render propertized text.
          // Use FreeType subpixel LCD rendering, if possible.
#         if SDL_TTF_VERSION_ATLEAST(2,20,0) && !_WIN32
          SDL_Surface *propertized_text_surface = TTF_RenderUTF8_LCD
            (font, propertized_text, prop_fg, prop_bg);
#         else
          SDL_Surface *propertized_text_surface = TTF_RenderUTF8_Shaded
            (font, propertized_text, prop_fg, prop_bg);
#         endif
          if (propertized_text_surface) {
            SDL_BlitSurface(propertized_text_surface, NULL,
                            text_surface, &property_draw_position);
            SDL_FreeSurface(propertized_text_surface);
          }
          free(propertized_text);
        }
      }
      free(line_text);

    at_end_of_line:

      destination.y += line_height;
      destination.h -= line_height;
      destination.x = 0;
      last_newline_offset = offset;

      // No more room to draw text in output rectangle, stop now.
      if (destination.h <= 0) break;
    }
    if (*string == '\0') break;
    string += 1;
    offset += 1;
  }
  // dstrect stores the position that text_surface will be copied into surface.
  SDL_RECT_EMPTY(dstrect);
  rect_copy_position(&dstrect, rect);
  SDL_BlitSurface(text_surface, &srcrect, surface, &dstrect);
  SDL_FreeSurface(text_surface);
}

void draw_gui(GUIContext *ctx) {
  if (!gwindow || !grender || !ctx || !created_gui_marker) {
    return;
  }

  // Update window title if one is provided.
  if (ctx->title) {
    printf("GFX::SDL: Setting window title to \"%s\"\n", ctx->title);
    SDL_SetWindowTitle(gwindow, ctx->title);
    ctx->title = NULL;
  }

  // Update default text property from context.
  gui_color_to_sdl(&fg, &ctx->default_property.fg);
  gui_color_to_sdl(&bg, &ctx->default_property.bg);

  // Clear screen to background color.
  draw_bg();

  // Get size of screen (needed to support resizing).
  int width = 0;
  int height = 0;
  SDL_GetWindowSize(gwindow, &width, &height);

  SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat
    (0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
  if (!surface) {
    fprintf(stderr,
            "GFX::SDL:ERROR: Could not create surface for window.\n"
            "              : %s\n",
            SDL_GetError());
    return;
  }

  // Calculate height of head/foot lines using line count.
  // TODO: Handle wrapping of long lines.
  size_t footline_line_count = count_lines(ctx->footline.string);
  size_t footline_height = font_height * footline_line_count;

  // TODO: Handle corner case when there is not enough room for contents.
  size_t contents_height = height - footline_height;

  // Simple vertical layout, for now.
  SDL_Rect rect_contents;
  SDL_Rect rect_footline;

  rect_contents.x = 0;
  rect_contents.y = 0;
  rect_contents.w = width;
  rect_contents.h = contents_height;

  rect_footline.x = 0;
  rect_footline.y = rect_contents.h + rect_contents.y;
  rect_footline.w = width;
  rect_footline.h = footline_height;

  // Draw context string contents onto screen surface within calculated rectangle,
  // taking in to account text properties of GUIStrings.
  // TODO: Make new linked list sorted by Z order. For now, just go in order.
  for (GUIWindow *window = ctx->windows; window; window = window->next) {
    // Skip windows that can't be displayed.
    if (window->posx >= 100 || window->posy >= 100
        || window->sizex == 0 || window->sizey == 0
        || window->sizex > 100 || window->sizey > 100
        ) {
      continue;
    }
    SDL_Rect rect_window;
    // Treat posx and posy as integer percent between 0-100 inclusive
    rect_window.x = (int)(((float)(window->posx) / 100.0f) * rect_contents.w);
    rect_window.y = (int)(((float)(window->posy) / 100.0f) * rect_contents.h);
    rect_window.w = (int)(((float)(window->sizex) / 100.0f) * rect_contents.w);
    rect_window.h = (int)(((float)(window->sizey) / 100.0f) * rect_contents.h);
    draw_gui_string_into_surface_within_rect(window->contents, surface, &rect_window, ctx->cr_char);
  }
  draw_gui_string_into_surface_within_rect(ctx->footline, surface, &rect_footline, ctx->cr_char);

  if (ctx->reading) {
    SDL_Rect rect_popup;
    rect_popup.w = width / 2 + width / 4;
    rect_popup.h = height / 2 + height / 4;
    rect_popup.x = width / 2 - (rect_popup.w / 2);
    rect_popup.y = height / 2 - (rect_popup.h / 2);

    size_t border_thickness = 4;
    // Ensure thickness is even.
    border_thickness &= ~1;
    SDL_Rect rect_popup_outline = rect_popup;
    rect_popup_outline.h += border_thickness;
    rect_popup_outline.w += border_thickness;
    rect_popup_outline.x -= border_thickness / 2;
    rect_popup_outline.y -= border_thickness / 2;
    SDL_FillRect(surface, &rect_popup_outline, 0xffffffff);
    SDL_FillRect(surface, &rect_popup, 0);
    draw_gui_string_into_surface_within_rect(ctx->popup, surface, &rect_popup, ctx->cr_char);
  }

  // Copy screen surface into a texture.
  texture = SDL_CreateTextureFromSurface(grender, surface);
  SDL_FreeSurface(surface);
  if (!texture) {
    printf("GFX::SDL:ERROR: Could not create texture from surface.\n"
           "              : %s\n"
           , SDL_GetError());
  }
  // Render texture.
  SDL_RenderCopy(grender, texture, NULL, NULL);
  SDL_DestroyTexture(texture);
  SDL_RenderPresent(grender);
}

HOTFUNCTION
int handle_event(SDL_Event *event) {
  if (!event) { return 0; }
  static GUIModifierKey mod;
  switch(event->type) {
  default:
    //printf("GFX::SDL: Unhandled event: %u\n", event.type);
    return 1;
  case SDL_KEYDOWN:
    if ((mod = is_modifier(event->key.keysym.sym)) != GUI_MODKEY_MAX) {
      handle_modifier_dn(mod);
    } else {
      const size_t max_size = 32;
      char *string = malloc(max_size);
      if (!string) { break; }
      static const size_t keycode_size = sizeof(SDL_Keycode);
      switch (event->key.keysym.sym) {
      default:
        memcpy(string, &event->key.keysym.sym, keycode_size);
        string[keycode_size] = '\0';
        break;
      case SDLK_RETURN:
      case SDLK_RETURN2:
        strncpy(string, LITE_KEYSTRING_RETURN, max_size);
        break;
      case SDLK_BACKSPACE:
        strncpy(string, LITE_KEYSTRING_BACKSPACE, max_size);
        break;
      case SDLK_TAB:
        strncpy(string, LITE_KEYSTRING_TAB, max_size);
        break;
      case SDLK_CAPSLOCK:
        strncpy(string, LITE_KEYSTRING_CAPSLOCK, max_size);
        break;
      case SDLK_ESCAPE:
        strncpy(string, LITE_KEYSTRING_ESCAPE, max_size);
        break;
      case SDLK_INSERT:
        strncpy(string, LITE_KEYSTRING_INSERT, max_size);
        break;
      case SDLK_DELETE:
        strncpy(string, LITE_KEYSTRING_DELETE, max_size);
        break;
      case SDLK_HOME:
        strncpy(string, LITE_KEYSTRING_HOME, max_size);
        break;
      case SDLK_END:
        strncpy(string, LITE_KEYSTRING_END, max_size);
        break;
      case SDLK_PAGEUP:
        strncpy(string, LITE_KEYSTRING_PAGE_UP, max_size);
        break;
      case SDLK_PAGEDOWN:
        strncpy(string, LITE_KEYSTRING_PAGE_DOWN, max_size);
        break;
      case SDLK_LEFT:
        strncpy(string, LITE_KEYSTRING_LEFT_ARROW, max_size);
        break;
      case SDLK_RIGHT:
        strncpy(string, LITE_KEYSTRING_RIGHT_ARROW, max_size);
        break;
      case SDLK_UP:
        strncpy(string, LITE_KEYSTRING_UP_ARROW, max_size);
        break;
      case SDLK_DOWN:
        strncpy(string, LITE_KEYSTRING_DOWN_ARROW, max_size);
        break;
      case SDLK_SCROLLLOCK:
        strncpy(string, LITE_KEYSTRING_SCROLL_LOCK, max_size);
        break;
      case SDLK_PAUSE:
        strncpy(string, LITE_KEYSTRING_PAUSE, max_size);
        break;
      case SDLK_PRINTSCREEN:
        strncpy(string, LITE_KEYSTRING_PRINT_SCREEN, max_size);
        break;
      case SDLK_NUMLOCKCLEAR:
        strncpy(string, LITE_KEYSTRING_NUMPAD_LOCK, max_size);
        break;
      case SDLK_F1:
        strncpy(string, LITE_KEYSTRING_F1, max_size);
        break;
      case SDLK_F2:
        strncpy(string, LITE_KEYSTRING_F2, max_size);
        break;
      case SDLK_F3:
        strncpy(string, LITE_KEYSTRING_F3, max_size);
        break;
      case SDLK_F4:
        strncpy(string, LITE_KEYSTRING_F4, max_size);
        break;
      case SDLK_F5:
        strncpy(string, LITE_KEYSTRING_F5, max_size);
        break;
      case SDLK_F6:
        strncpy(string, LITE_KEYSTRING_F6, max_size);
        break;
      case SDLK_F7:
        strncpy(string, LITE_KEYSTRING_F7, max_size);
        break;
      case SDLK_F8:
        strncpy(string, LITE_KEYSTRING_F8, max_size);
        break;
      case SDLK_F9:
        strncpy(string, LITE_KEYSTRING_F9, max_size);
        break;
      case SDLK_F10:
        strncpy(string, LITE_KEYSTRING_F10, max_size);
        break;
      case SDLK_F11:
        strncpy(string, LITE_KEYSTRING_F11, max_size);
        break;
      case SDLK_F12:
        strncpy(string, LITE_KEYSTRING_F12, max_size);
        break;
      case SDLK_F13:
        strncpy(string, LITE_KEYSTRING_F13, max_size);
        break;
      case SDLK_F14:
        strncpy(string, LITE_KEYSTRING_F14, max_size);
        break;
      case SDLK_F15:
        strncpy(string, LITE_KEYSTRING_F15, max_size);
        break;
      case SDLK_F16:
        strncpy(string, LITE_KEYSTRING_F16, max_size);
        break;
      case SDLK_F17:
        strncpy(string, LITE_KEYSTRING_F17, max_size);
        break;
      case SDLK_F18:
        strncpy(string, LITE_KEYSTRING_F18, max_size);
        break;
      case SDLK_F19:
        strncpy(string, LITE_KEYSTRING_F19, max_size);
        break;
      case SDLK_F20:
        strncpy(string, LITE_KEYSTRING_F20, max_size);
        break;
      case SDLK_F21:
        strncpy(string, LITE_KEYSTRING_F21, max_size);
        break;
      case SDLK_F22:
        strncpy(string, LITE_KEYSTRING_F22, max_size);
        break;
      case SDLK_F23:
        strncpy(string, LITE_KEYSTRING_F23, max_size);
        break;
      case SDLK_F24:
        strncpy(string, LITE_KEYSTRING_F24, max_size);
        break;
      case SDLK_KP_0:
        strncpy(string, LITE_KEYSTRING_NUMPAD_0, max_size);
        break;
      case SDLK_KP_1:
        strncpy(string, LITE_KEYSTRING_NUMPAD_1, max_size);
        break;
      case SDLK_KP_2:
        strncpy(string, LITE_KEYSTRING_NUMPAD_2, max_size);
        break;
      case SDLK_KP_3:
        strncpy(string, LITE_KEYSTRING_NUMPAD_3, max_size);
        break;
      case SDLK_KP_4:
        strncpy(string, LITE_KEYSTRING_NUMPAD_4, max_size);
        break;
      case SDLK_KP_5:
        strncpy(string, LITE_KEYSTRING_NUMPAD_5, max_size);
        break;
      case SDLK_KP_6:
        strncpy(string, LITE_KEYSTRING_NUMPAD_6, max_size);
        break;
      case SDLK_KP_7:
        strncpy(string, LITE_KEYSTRING_NUMPAD_7, max_size);
        break;
      case SDLK_KP_8:
        strncpy(string, LITE_KEYSTRING_NUMPAD_8, max_size);
        break;
      case SDLK_KP_9:
        strncpy(string, LITE_KEYSTRING_NUMPAD_0, max_size);
        break;
      case SDLK_KP_00:
        strncpy(string, LITE_KEYSTRING_NUMPAD_00, max_size);
        break;
      case SDLK_KP_000:
        strncpy(string, LITE_KEYSTRING_NUMPAD_000, max_size);
        break;
      case SDLK_KP_PERIOD:
        strncpy(string, LITE_KEYSTRING_NUMPAD_DOT, max_size);
        break;
      case SDLK_KP_ENTER:
        strncpy(string, LITE_KEYSTRING_NUMPAD_RETURN, max_size);
        break;
      case SDLK_KP_PLUS:
        strncpy(string, LITE_KEYSTRING_NUMPAD_PLUS, max_size);
        break;
      case SDLK_KP_MINUS:
        strncpy(string, LITE_KEYSTRING_NUMPAD_MINUS, max_size);
        break;
      case SDLK_KP_MULTIPLY:
        strncpy(string, LITE_KEYSTRING_NUMPAD_MULTIPLY, max_size);
        break;
      case SDLK_KP_DIVIDE:
        strncpy(string, LITE_KEYSTRING_NUMPAD_DIVIDE, max_size);
        break;
      case SDLK_KP_EQUALS:
        strncpy(string, LITE_KEYSTRING_NUMPAD_EQUALS, max_size);
        break;
      case SDLK_KP_GREATER:
        strncpy(string, LITE_KEYSTRING_NUMPAD_GREATER, max_size);
        break;
      case SDLK_KP_LESS:
        strncpy(string, LITE_KEYSTRING_NUMPAD_LESS, max_size);
        break;
      case SDLK_KP_AMPERSAND:
        strncpy(string, LITE_KEYSTRING_NUMPAD_AMPERSAND, max_size);
        break;
      case SDLK_KP_DBLAMPERSAND:
        strncpy(string, LITE_KEYSTRING_NUMPAD_DOUBLE_AMPERSAND, max_size);
        break;
      case SDLK_KP_VERTICALBAR:
        strncpy(string, LITE_KEYSTRING_NUMPAD_PIPE, max_size);
        break;
      case SDLK_KP_DBLVERTICALBAR:
        strncpy(string, LITE_KEYSTRING_NUMPAD_DOUBLE_PIPE, max_size);
        break;
      case SDLK_KP_XOR:
        strncpy(string, LITE_KEYSTRING_NUMPAD_XOR, max_size);
        break;
      case SDLK_KP_LEFTPAREN:
        strncpy(string, LITE_KEYSTRING_NUMPAD_LEFT_PARENTHESIS, max_size);
        break;
      case SDLK_KP_RIGHTPAREN:
        strncpy(string, LITE_KEYSTRING_NUMPAD_RIGHT_PARENTHESIS, max_size);
        break;
      case SDLK_KP_LEFTBRACE:
        strncpy(string, LITE_KEYSTRING_NUMPAD_LEFT_BRACE, max_size);
        break;
      case SDLK_KP_RIGHTBRACE:
        strncpy(string, LITE_KEYSTRING_NUMPAD_RIGHT_BRACE, max_size);
        break;
      case SDLK_KP_PERCENT:
        strncpy(string, LITE_KEYSTRING_NUMPAD_PERCENT, max_size);
        break;
      case SDLK_KP_POWER:
        strncpy(string, LITE_KEYSTRING_NUMPAD_POWER, max_size);
        break;
      case SDLK_KP_SPACE:
        strncpy(string, LITE_KEYSTRING_NUMPAD_SPACE, max_size);
        break;
      case SDLK_KP_BACKSPACE:
        strncpy(string, LITE_KEYSTRING_NUMPAD_BACKSPACE, max_size);
        break;
      case SDLK_KP_TAB:
        strncpy(string, LITE_KEYSTRING_NUMPAD_TAB, max_size);
        break;
      case SDLK_KP_PLUSMINUS:
        strncpy(string, LITE_KEYSTRING_NUMPAD_PLUS_MINUS, max_size);
        break;
      case SDLK_KP_EXCLAM:
        strncpy(string, LITE_KEYSTRING_NUMPAD_EXCLAMATION, max_size);
        break;
      case SDLK_KP_COLON:
        strncpy(string, LITE_KEYSTRING_NUMPAD_COLON, max_size);
        break;
      case SDLK_KP_COMMA:
        strncpy(string, LITE_KEYSTRING_NUMPAD_COMMA, max_size);
        break;
      case SDLK_KP_AT:
        strncpy(string, LITE_KEYSTRING_NUMPAD_AT, max_size);
        break;
      case SDLK_KP_OCTAL:
        strncpy(string, LITE_KEYSTRING_NUMPAD_OCTAL, max_size);
        break;
      case SDLK_KP_DECIMAL:
        strncpy(string, LITE_KEYSTRING_NUMPAD_DECIMAL, max_size);
        break;
      case SDLK_KP_HEXADECIMAL:
        strncpy(string, LITE_KEYSTRING_NUMPAD_HEXADECIMAL, max_size);
        break;
      case SDLK_KP_A:
        strncpy(string, LITE_KEYSTRING_NUMPAD_A, max_size);
        break;
      case SDLK_KP_B:
        strncpy(string, LITE_KEYSTRING_NUMPAD_B, max_size);
        break;
      case SDLK_KP_C:
        strncpy(string, LITE_KEYSTRING_NUMPAD_C, max_size);
        break;
      case SDLK_KP_D:
        strncpy(string, LITE_KEYSTRING_NUMPAD_D, max_size);
        break;
      case SDLK_KP_E:
        strncpy(string, LITE_KEYSTRING_NUMPAD_E, max_size);
        break;
      case SDLK_KP_F:
        strncpy(string, LITE_KEYSTRING_NUMPAD_F, max_size);
        break;
      }
      string[max_size - 1] = '\0';
      handle_keydown(string);
      free(string);
    }
    return 1;
  case SDL_KEYUP:
    if ((mod = is_modifier(event->key.keysym.sym)) != GUI_MODKEY_MAX) {
      handle_modifier_up(mod);
    }
    return 1;
  case SDL_QUIT:
    return 0;
  }
  return 0;
}

HOTFUNCTION
int handle_events() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (!handle_event(&event)) return 0;
  }
  return 1;
}

// Returns a boolean-like integer value (0 is false).
int do_gui(GUIContext *ctx) {
  if (!ctx) {
    fprintf(stderr, "GFX::SDL:ERROR: Can not render NULL graphical context.\n");
    return 0;
  }
  int status = handle_events();
  if (status == 0) {
    printf("GFX::SDL: Quitting.\n");
    return 0;
  }
  draw_gui(ctx);
  // Wait for next user input before returning. This cuts down CPU
  // usage dramatically.
  SDL_Event event;
  SDL_WaitEvent(&event);
  return handle_event(&event);
}

void destroy_gui() {
  if (created_gui_marker) {
    if (grender) {
      SDL_DestroyRenderer(grender);
      grender = NULL;
    }
    if (gwindow) {
      SDL_DestroyWindow(gwindow);
      gwindow = NULL;
    }
    if (font) {
      TTF_CloseFont(font);
      font = NULL;
    }
    TTF_Quit();
    SDL_Quit();
    created_gui_marker = 0;
  }
}

void window_size(size_t *width, size_t *height) {
  SDL_GetWindowSize(gwindow, (int *)width, (int *)height);
}

void window_size_row_col(size_t *rows, size_t *cols) {
  size_t char_w = 0;
  size_t char_h = 0;
  TTF_SizeText(font, "M", &char_w, &char_h);
  window_size(cols, rows);
  *rows /= char_h;
  *cols /= char_w;
}


void change_window_size(size_t width, size_t height) {
  SDL_SetWindowSize(gwindow, width, height);
}

// TODO: Add window_mode() query to get current window mode using
//       SDL_GetWindowFlags().
void change_window_mode(enum GFXWindowMode mode) {
  uint32_t flags = 0;
  switch (mode) {
  default:
  case GFX_WINDOW_MODE_WINDOWED:
    flags = 0;
    break;
  case GFX_WINDOW_MODE_FULLSCREEN:
    flags = SDL_WINDOW_FULLSCREEN_DESKTOP;
    break;
  }
  SDL_SetWindowFullscreen(gwindow, flags);
}

void change_window_visibility(enum GFXWindowVisible visible) {
  if (visible == GFX_WINDOW_VISIBLE) {
    SDL_ShowWindow(gwindow);
  } else if (visible == GFX_WINDOW_INVISIBLE) {
    SDL_HideWindow(gwindow);
  }
}

void set_clipboard_utf8(const char *data) {
  SDL_SetClipboardText(data);
}

char *get_clipboard_utf8() {
  char *tmp = SDL_GetClipboardText();
  size_t len = strlen(tmp) + 1;
  char *out = calloc(1, len);
  memcpy(out, tmp, len);
  SDL_free(tmp);
  return out;
}

char has_clipboard_utf8() {
  return SDL_HasClipboardText() == SDL_TRUE;
}
