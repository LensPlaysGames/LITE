#include <api.h>
#include <gui.h>
#include <utility.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_events.h>
#include <SDL_keycode.h>
#include <SDL_rect.h>
#include <SDL_render.h>
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

static inline TTF_Font *try_open_system_font(const char *path, size_t size) {
  char *working_path = NULL;
  TTF_Font *working_font = NULL;

#if defined (_WIN32) || defined (_WIN64)

  working_path = string_join("C:/Windows/fonts/", (char *)path);
  if (!working_path) { return NULL; }
  working_font = TTF_OpenFont(working_path, size);
  free(working_path);
  if (working_font) { return working_font; }

#elif defined (__unix)

  working_path = string_join("/usr/share/fonts/", (char *)path);
  if (!working_path) { return NULL; }
  working_font = TTF_OpenFont(working_path, size);
  free(working_path);
  if (working_font) { return working_font; }

  working_path = string_join("/usr/local/share/fonts/", (char *)path);
  if (!working_path) { return NULL; }
  working_font = TTF_OpenFont(working_path, size);
  free(working_path);
  if (working_font) { return working_font; }

#endif

  return NULL;
}

// Try to open a font found in the SDL2 backend directory,
// falling back to searching the system-wide font directories.
static inline TTF_Font *try_open_font(const char *name, size_t size) {
  TTF_Font *working_font = NULL;
  char *working_path = NULL;

  // Search current directory.
  working_font = TTF_OpenFont(name, size);
  if (working_font) { return working_font; }

  // Assume working directory of base of the repository.
  working_path = string_join("gfx/fonts/apache/", (char *)name);
  if (!working_path) { return NULL; }
  working_font = TTF_OpenFont(working_path, size);
  free(working_path);
  if (working_font) { return working_font; }

  // Assume working directory of bin repository subdirectory.
  working_path = string_join("../gfx/fonts/apache/", (char *)name);
  if (!working_path) { return NULL; }
  working_font = TTF_OpenFont(working_path, size);
  free(working_path);
  if (working_font) { return working_font; }

  // Assume working directory of gfx repository subdirectory.
  working_path = string_join("fonts/apache/", (char *)name);
  if (!working_path) { return NULL; }
  working_font = TTF_OpenFont(working_path, size);
  free(working_path);
  if (working_font) { return working_font; }

  // If still not found, search system-specific font directories.
  return try_open_system_font(name, size);
}

static int created_gui_marker = 0;
int create_gui() {
  if (created_gui_marker != 0) { return 3; }
  created_gui_marker = 1;
  if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
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
  printf("GFX::SDL: SDL Initialized\n");
  if (TTF_Init() != 0) {
    printf("GFX::SDL:ERROR: Failed to initialize SDL_ttf.\n");
    return 1;
  }
  // TODO: Do not hard-code font size!
  // Attempt to load font(s) included with LITE distribution.
  const size_t font_size = 18;
  if (!font) { font = try_open_font("DroidSansMono.ttf"      , font_size); }
  if (!font) { font = try_open_font("RobotoMono-Regular.ttf" , font_size); }
  if (!font) { font = try_open_font("DroidSans.ttf"          , font_size); }
  if (!font) { font = try_open_font("Tinos-Regular.ttf"      , font_size); }
  // At this point, hail mary for a system font.
  if (!font) { font = try_open_font("UbuntuMono-R.ttf"       , font_size); }
  if (!font) { font = try_open_font("Arial.ttf"              , font_size); }
  // Finally, if no font was found anywhere, error out.
  if (!font) {
    printf("GFX::SDL: SDL_ttf could not open a font.\n"
           "        : %s\n"
           , TTF_GetError()
           );
    return 1;
  }
  font_height = TTF_FontHeight(font);
  if (!font_height) {
    printf("GFX::SDL: SDL_ttf could not get height of font.\n"
           "        : %s\n"
           , TTF_GetError()
           );
    return 1;
  }
  printf("GFX::SDL: SDL_ttf Initialized\n");
  return 0;
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

typedef struct LinkedRectangle {
  SDL_Rect rect;
  struct LinkedRectangle *next;
} LinkedRectangle;

LinkedRectangle *make_linked_rect(LinkedRectangle *parent, SDL_Rect rect) {
  LinkedRectangle *linked_rect = malloc(sizeof(LinkedRectangle));
  linked_rect->rect.x = rect.x;
  linked_rect->rect.y = rect.y;
  linked_rect->rect.w = rect.w;
  linked_rect->rect.h = rect.h;
  linked_rect->next = parent;
  return linked_rect;
}

static inline void draw_gui_string_into_surface_within_rect
(GUIString string, SDL_Surface *surface, SDL_Rect *rect) {
  if (!string.string || string.string[0] == '\0' || !surface || !rect) {
    return;
  }
  SDL_Surface *text_surface = NULL;
  // srcrect is the rectangle within text_surface that will be copied from.
  SDL_RECT_EMPTY(srcrect);
  rect_copy_size(&srcrect, rect);
  if (!string.properties) {
#if SDL_TTF_VERSION_ATLEAST(2,20,0)
    text_surface = TTF_RenderUTF8_LCD_Wrapped
      (font, string.string, fg, bg, rect->w);
#elif SDL_TTF_VERSION_ATLEAST(2,0,18)
    text_surface = TTF_RenderUTF8_Shaded_Wrapped
      (font, string.string, fg, bg, rect->w);
#else
    text_surface = TTF_RenderUTF8_Blended_Wrapped
      (font, string.string, fg, rect->w);
#endif
    if (!text_surface) { return; }
  } else {
    text_surface = SDL_CreateRGBSurfaceWithFormat
      (0, rect->w, rect->h, 32, SDL_PIXELFORMAT_RGBA32);
    if (!text_surface) { return; }
    // Iterator for string.
    char *string_contents = string.string;
    // Byte offset of `string_contents` iterator into string.
    size_t offset = 0;
    // Byte offset of newline previous to `offset` in string.
    size_t last_newline_offset = 0;
    // Destination within text_surface to render text into.
    // SDL_BlitSurface overrides the destination rect w and h,
    // so we must use a copy to prevent clobbering our data.
    SDL_Rect destination = srcrect;
    SDL_Rect destination_copy = destination;
    while (1) {
      if (*string_contents == '\n' || *string_contents == '\0') {
        // Byte offset of start of line we are currently at the end of.
        size_t start_of_line_offset =
          last_newline_offset == 0 ? 0 : last_newline_offset + 1;
        // Iterator for GUIString properties linked list.
        GUIStringProperty *it = string.properties;
        uint8_t prop_count = 0;
        uint8_t props_in_line = 0;
        size_t line_height = font_height;
        // Ensure that `line_height` is equal to the max size of any
        // property within the current line.
        while (it) {
          prop_count += 1;
          // Somewhat protect against infinite looping.
          // TODO: It may be best to create a new list and check that
          // each new property has not already been visited, but we
          // trust LITE frontend more than that right now.
          if (it->next == it) { break; }
          // Validate text property fields.
          if (it->length == 0) {
            it = it->next;
            continue;
          }
          // Ensure text property is within `start_of_line_offset` through `offset`.
          if (it->offset <= offset
              && it->offset + it->length > start_of_line_offset)
            {
              // TODO: Update line_height iff property height is larger.
              props_in_line += 1;
            }
          it = it->next;
        }
        // Render entire line using defaults.
        size_t bytes_to_render = offset - start_of_line_offset;
        char *line_text = allocate_string_span
          (string.string, start_of_line_offset, bytes_to_render);
        if (line_text) {
          if (line_text[0] != '\0') {
            // Use FreeType subpixel LCD rendering, if possible.
#           if SDL_TTF_VERSION_ATLEAST(2,20,0)
            SDL_Surface *line_text_surface =
              TTF_RenderUTF8_LCD(font, line_text, fg, bg);
#           else
            SDL_Surface *line_text_surface =
              TTF_RenderUTF8_Shaded(font, line_text, fg, bg);
#           endif
            if (line_text_surface) {
              // TODO: Handle BlitSurface failure everywhere (0 == success).
              SDL_BlitSurface(line_text_surface, NULL, text_surface
                              , rect_copy(&destination_copy,&destination));
              SDL_FreeSurface(line_text_surface);
            }
          }
          free(line_text);
        }
        if (props_in_line) {
          // Properties present, render text properties over default line.
          // This linked list will store the draw position of every character.
          // This should definitely be optimized, as we only need to save
          // the positions of the text properties offset's we need to draw.
          LinkedRectangle *character_rects = make_linked_rect(NULL, destination);
          // Calculate amount of bytes within the current line.
          size_t bytes_to_render = offset - start_of_line_offset;
          // Allocate a copy of a string span (the current line).
          char *line_contents = allocate_string_span
            (string.string, start_of_line_offset, bytes_to_render);
          if (line_contents) {
            // Create a buffer to use as a temporary string for sizing.
            char *string_for_sizing = malloc(bytes_to_render + 1);
            if (string_for_sizing) {
              string_for_sizing[0] = '\0';
              SDL_RECT_EMPTY(char_rect);
              // Save the draw position of every character.
              // TODO: Only save draw positions we actually need, or
              // maybe get the ones we need as we need them.
              for (size_t i = 0; i < bytes_to_render; ++i) {
                string_for_sizing[i] = line_contents[i];
                string_for_sizing[i + 1] = '\0';
                TTF_SizeUTF8(font, string_for_sizing, &char_rect.x, &char_rect.y);
                character_rects = make_linked_rect(character_rects, char_rect);
              }
              // Render text properties over current line.
              it = string.properties;
              while (it) {
                if (it->offset <= offset
                    && it->offset + it->length > start_of_line_offset) {
                  // How far the text property starts within the current line (index).
                  size_t offset_within_line = 0;
                  if (it->offset > start_of_line_offset) {
                    offset_within_line = it->offset - start_of_line_offset;
                  }
                  // Get draw position at the byte offset within line.
                  LinkedRectangle *rect_it = character_rects;
                  size_t rect_offset = bytes_to_render - offset_within_line;
                  if (offset_within_line == 0) {
                    while (rect_it->next) {
                      rect_it = rect_it->next;
                    }
                  } else {
                    while (rect_it && rect_offset) {
                      rect_it = rect_it->next;
                      rect_offset -= 1;
                    }
                  }
                  if (rect_it) {
                    // If we were able to find a draw position, calculate the length
                    // of the propertized text (in bytes). We can't just use the
                    // iterator's length because of multi-line text properties.
                    size_t propertized_text_length = it->length;
                    if (it->offset < start_of_line_offset) {
                      propertized_text_length -= start_of_line_offset - it->offset;
                    }
                    // Allocate the text that will has a property applied to it as
                    // a seperate string that can be fed to SDL_ttf for rendering.
                    char *propertized_text = allocate_string_span
                      (line_contents, offset_within_line, propertized_text_length);
                    if (propertized_text) {
                      // Handle empty lines propertized (insert space).
                      if (propertized_text[0] ==  '\0') {
                        free(propertized_text);
                        propertized_text = malloc(2);
                        if (propertized_text) {
                          propertized_text[0] = ' ';
                          propertized_text[1] = '\0';
                        }
                      } else if (propertized_text[0] ==  '\n') {
                        propertized_text[0] = ' ';
                      }
                      // Get colors for propertized text from text property.
                      SDL_Color prop_fg;
                      SDL_Color prop_bg;
                      gui_color_to_sdl(&prop_fg, &it->fg);
                      gui_color_to_sdl(&prop_bg, &it->bg);
                      // Render propertized text.
                      // Use FreeType subpixel LCD rendering, if possible.
#                     if SDL_TTF_VERSION_ATLEAST(2,20,0)
                      SDL_Surface *propertized_text_surface = TTF_RenderUTF8_LCD
                        (font, propertized_text, prop_fg, prop_bg);
#                     else
                      SDL_Surface *propertized_text_surface = TTF_RenderUTF8_Shaded
                        (font, propertized_text, prop_fg, prop_bg);
#                     endif
                      if (propertized_text_surface) {
                        SDL_RECT_EMPTY(property_draw_position);
                        property_draw_position.x = rect_it->rect.x;
                        property_draw_position.w = destination.w - property_draw_position.x;
                        property_draw_position.y = destination.y;
                        SDL_BlitSurface(propertized_text_surface, NULL,
                                        text_surface, &property_draw_position);
                        SDL_FreeSurface(propertized_text_surface);
                      }
                      free(propertized_text);
                    }
                  }
                }
                it = it->next;
              }
            }
            free(line_contents);
          }
        }
        destination.y += line_height;
        destination.h -= line_height;
        destination.x = 0;
        last_newline_offset = offset;
        // No more room to draw text in output rectangle, stop now.
        if (destination.h <= 0) {
          break;
        }
      }
      if (*string_contents == '\0') {
        break;
      }
      string_contents += 1;
      offset += 1;
    }
  }
  // dstrect stores the position that text_surface will be copied into surface.
  SDL_RECT_EMPTY(dstrect);
  rect_copy_position(&dstrect, rect);
  SDL_BlitSurface(text_surface, &srcrect, surface, &dstrect);
  SDL_FreeSurface(text_surface);
}

void draw_gui(GUIContext *ctx) {
  if (!gwindow || !grender || !ctx) {
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
    printf("GFX::SDL:ERROR: Could not create surface for window.\n"
           "              : %s\n"
           , SDL_GetError());
    return;
  }

  // Calculate height of head/foot lines using line count.
  // TODO: Handle wrapping of long lines.
  size_t headline_line_count = count_lines(ctx->headline.string);
  size_t footline_line_count = count_lines(ctx->footline.string);
  size_t headline_height = font_height * headline_line_count;
  size_t footline_height = font_height * footline_line_count;

  // TODO: Handle corner case when there is not enough room for contents.
  size_t contents_height = height - headline_height - footline_height;

  // Simple vertical layout, for now.
  SDL_Rect rect_headline;
  SDL_Rect rect_contents;
  SDL_Rect rect_footline;

  rect_headline.x = 0;
  rect_headline.y = 0;
  rect_headline.w = width;
  rect_headline.h = headline_height;

  rect_contents.x = 0;
  rect_contents.y = rect_headline.h + rect_headline.y;
  rect_contents.w = width;
  rect_contents.h = contents_height;

  rect_footline.x = 0;
  rect_footline.y = rect_contents.h + rect_contents.y;
  rect_footline.w = width;
  rect_footline.h = footline_height;

  // Draw context string contents onto screen surface within calculated rectangle,
  // taking in to account text properties of GUIStrings.
  draw_gui_string_into_surface_within_rect(ctx->headline, surface, &rect_headline);
  draw_gui_string_into_surface_within_rect(ctx->contents, surface, &rect_contents);
  draw_gui_string_into_surface_within_rect(ctx->footline, surface, &rect_footline);

  if (ctx->reading) {
    SDL_RECT_EMPTY(rect_popup);
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
    draw_gui_string_into_surface_within_rect(ctx->popup, surface, &rect_popup);
  }

  // Copy screen surface into a texture.
  texture = SDL_CreateTextureFromSurface(grender, surface);
  SDL_FreeSurface(surface);
  if (texture) {
    // Render texture.
    SDL_RenderCopy(grender, texture, NULL, NULL);
    SDL_DestroyTexture(texture);
  } else {
    printf("GFX::SDL:ERROR: Could not create texture from surface.\n"
           "              : %s\n"
           , SDL_GetError());
  }
  SDL_RenderPresent(grender);
}

HOTFUNCTION
int handle_event(SDL_Event *event) {
  if (!event) { return 0; }
  GUIModifierKey mod;
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
        strncpy(string, "<return>"                       , max_size);
        break;
      case SDLK_BACKSPACE:
        strncpy(string, "<backspace>"                    , max_size);
        break;
      case SDLK_TAB:
        strncpy(string, "<tab>"                          , max_size);
        break;
      case SDLK_CAPSLOCK:
        strncpy(string, "<capslock>"                     , max_size);
        break;
      case SDLK_ESCAPE:
        strncpy(string, "<escape>"                       , max_size);
        break;
      case SDLK_INSERT:
        strncpy(string, "<insert>"                       , max_size);
        break;
      case SDLK_DELETE:
        strncpy(string, "<delete>"                       , max_size);
        break;
      case SDLK_HOME:
        strncpy(string, "<home>"                         , max_size);
        break;
      case SDLK_END:
        strncpy(string, "<end>"                          , max_size);
        break;
      case SDLK_PAGEUP:
        strncpy(string, "<page-up>"                      , max_size);
        break;
      case SDLK_PAGEDOWN:
        strncpy(string, "<page-down>"                    , max_size);
        break;
      case SDLK_LEFT:
        strncpy(string, "<left-arrow>"                   , max_size);
        break;
      case SDLK_RIGHT:
        strncpy(string, "<right-arrow>"                  , max_size);
        break;
      case SDLK_UP:
        strncpy(string, "<up-arrow>"                     , max_size);
        break;
      case SDLK_DOWN:
        strncpy(string, "<down-arrow>"                   , max_size);
        break;
      case SDLK_SCROLLLOCK:
        strncpy(string, "<scroll-lock>"                  , max_size);
        break;
      case SDLK_PAUSE:
        strncpy(string, "<pause>"                        , max_size);
        break;
      case SDLK_PRINTSCREEN:
        strncpy(string, "<print-screen>"                 , max_size);
        break;
      case SDLK_NUMLOCKCLEAR:
        strncpy(string, "<numpad:lock>"                  , max_size);
        break;
      case SDLK_F1:
        strncpy(string, "<f1>"                           , max_size);
        break;
      case SDLK_F2:
        strncpy(string, "<f2>"                           , max_size);
        break;
      case SDLK_F3:
        strncpy(string, "<f3>"                           , max_size);
        break;
      case SDLK_F4:
        strncpy(string, "<f4>"                           , max_size);
        break;
      case SDLK_F5:
        strncpy(string, "<f5>"                           , max_size);
        break;
      case SDLK_F6:
        strncpy(string, "<f6>"                           , max_size);
        break;
      case SDLK_F7:
        strncpy(string, "<f7>"                           , max_size);
        break;
      case SDLK_F8:
        strncpy(string, "<f8>"                           , max_size);
        break;
      case SDLK_F9:
        strncpy(string, "<f9>"                           , max_size);
        break;
      case SDLK_F10:
        strncpy(string, "<f10>"                          , max_size);
        break;
      case SDLK_F11:
        strncpy(string, "<f11>"                          , max_size);
        break;
      case SDLK_F12:
        strncpy(string, "<f12>"                          , max_size);
        break;
      case SDLK_F13:
        strncpy(string, "<f13>"                          , max_size);
        break;
      case SDLK_F14:
        strncpy(string, "<f14>"                          , max_size);
        break;
      case SDLK_F15:
        strncpy(string, "<f15>"                          , max_size);
        break;
      case SDLK_F16:
        strncpy(string, "<f16>"                          , max_size);
        break;
      case SDLK_F17:
        strncpy(string, "<f17>"                          , max_size);
        break;
      case SDLK_F18:
        strncpy(string, "<f18>"                          , max_size);
        break;
      case SDLK_F19:
        strncpy(string, "<f19>"                          , max_size);
        break;
      case SDLK_F20:
        strncpy(string, "<f20>"                          , max_size);
        break;
      case SDLK_F21:
        strncpy(string, "<f21>"                          , max_size);
        break;
      case SDLK_F22:
        strncpy(string, "<f22>"                          , max_size);
        break;
      case SDLK_F23:
        strncpy(string, "<f23>"                          , max_size);
        break;
      case SDLK_F24:
        strncpy(string, "<f24>"                          , max_size);
        break;
      case SDLK_KP_0:
        strncpy(string, "<numpad:0>"                     , max_size);
        break;
      case SDLK_KP_1:
        strncpy(string, "<numpad:1>"                     , max_size);
        break;
      case SDLK_KP_2:
        strncpy(string, "<numpad:2>"                     , max_size);
        break;
      case SDLK_KP_3:
        strncpy(string, "<numpad:3>"                     , max_size);
        break;
      case SDLK_KP_4:
        strncpy(string, "<numpad:4>"                     , max_size);
        break;
      case SDLK_KP_5:
        strncpy(string, "<numpad:5>"                     , max_size);
        break;
      case SDLK_KP_6:
        strncpy(string, "<numpad:6>"                     , max_size);
        break;
      case SDLK_KP_7:
        strncpy(string, "<numpad:7>"                     , max_size);
        break;
      case SDLK_KP_8:
        strncpy(string, "<numpad:8>"                     , max_size);
        break;
      case SDLK_KP_9:
        strncpy(string, "<numpad:9>"                     , max_size);
        break;
      case SDLK_KP_00:
        strncpy(string, "<numpad:00>"                    , max_size);
        break;
      case SDLK_KP_000:
        strncpy(string, "<numpad:000>"                   , max_size);
        break;
      case SDLK_KP_PERIOD:
        strncpy(string, "<numpad:.>"                     , max_size);
        break;
      case SDLK_KP_ENTER:
        strncpy(string, "<numpad:return>"                , max_size);
        break;
      case SDLK_KP_PLUS:
        strncpy(string, "<numpad:plus>"                  , max_size);
        break;
      case SDLK_KP_MINUS:
        strncpy(string, "<numpad:minus>"                 , max_size);
        break;
      case SDLK_KP_MULTIPLY:
        strncpy(string, "<numpad:multiply>"              , max_size);
        break;
      case SDLK_KP_DIVIDE:
        strncpy(string, "<numpad:divide>"                , max_size);
        break;
      case SDLK_KP_EQUALS:
        strncpy(string, "<numpad:equals>"                , max_size);
        break;
      case SDLK_KP_GREATER:
        strncpy(string, "<numpad:greater>"               , max_size);
        break;
      case SDLK_KP_LESS:
        strncpy(string, "<numpad:less>"                  , max_size);
        break;
      case SDLK_KP_AMPERSAND:
        strncpy(string, "<numpad:&>"                     , max_size);
        break;
      case SDLK_KP_DBLAMPERSAND:
        strncpy(string, "<numpad:&&>"                    , max_size);
        break;
      case SDLK_KP_VERTICALBAR:
        strncpy(string, "<numpad:|>"                     , max_size);
        break;
      case SDLK_KP_DBLVERTICALBAR:
        strncpy(string, "<numpad:||>"                    , max_size);
        break;
      case SDLK_KP_XOR:
        strncpy(string, "<numpad:xor>"                   , max_size);
        break;
      case SDLK_KP_LEFTPAREN:
        strncpy(string, "<numpad:(>"                     , max_size);
        break;
      case SDLK_KP_RIGHTPAREN:
        strncpy(string, "<numpad:)>"                     , max_size);
        break;
      case SDLK_KP_LEFTBRACE:
        strncpy(string, "<numpad:{>"                     , max_size);
        break;
      case SDLK_KP_RIGHTBRACE:
        strncpy(string, "<numpad:}>"                     , max_size);
        break;
      case SDLK_KP_PERCENT:
        strncpy(string, "<numpad:%>"                     , max_size);
        break;
      case SDLK_KP_POWER:
        strncpy(string, "<numpad:power>"                 , max_size);
        break;
      case SDLK_KP_SPACE:
        strncpy(string, "<numpad: >"                     , max_size);
        break;
      case SDLK_KP_BACKSPACE:
        strncpy(string, "<numpad:backspace>"             , max_size);
        break;
      case SDLK_KP_TAB:
        strncpy(string, "<numpad:tab>"                   , max_size);
        break;
      case SDLK_KP_PLUSMINUS:
        strncpy(string, "<numpad:plusminus>"             , max_size);
        break;
      case SDLK_KP_EXCLAM:
        strncpy(string, "<numpad:!>"                     , max_size);
        break;
      case SDLK_KP_COLON:
        strncpy(string, "<numpad::>"                     , max_size);
        break;
      case SDLK_KP_COMMA:
        strncpy(string, "<numpad:,>"                     , max_size);
        break;
      case SDLK_KP_AT:
        strncpy(string, "<numpad:@>"                     , max_size);
        break;
      case SDLK_KP_OCTAL:
        strncpy(string, "<numpad:octal>"                 , max_size);
        break;
      case SDLK_KP_DECIMAL:
        strncpy(string, "<numpad:decimal>"               , max_size);
        break;
      case SDLK_KP_HEXADECIMAL:
        strncpy(string, "<numpad:hexadecimal>"           , max_size);
        break;
      case SDLK_KP_A:
        strncpy(string, "<numpad:A>"                     , max_size);
        break;
      case SDLK_KP_B:
        strncpy(string, "<numpad:B>"                     , max_size);
        break;
      case SDLK_KP_C:
        strncpy(string, "<numpad:C>"                     , max_size);
        break;
      case SDLK_KP_D:
        strncpy(string, "<numpad:D>"                     , max_size);
        break;
      case SDLK_KP_E:
        strncpy(string, "<numpad:E>"                     , max_size);
        break;
      case SDLK_KP_F:
        strncpy(string, "<numpad:F>"                     , max_size);
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
  int open = 1;
  while (open && SDL_PollEvent(&event)) {
    open = handle_event(&event);
  }
  return open;
}

// Returns a boolean-like integer value (0 is false).
int do_gui(GUIContext *ctx) {
  if (!ctx) {
    printf("GFX::SDL:ERROR: Can not render NULL graphical context.\n");
    return 0;
  }
  int status = handle_events();
  if (status == 0) {
    printf("GFX::SDL: Quitting.\n");
    return 0;
  }
  draw_gui(ctx);
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
