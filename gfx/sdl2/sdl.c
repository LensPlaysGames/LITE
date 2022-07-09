#include <api.h>
#include <gui.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_events.h>
#include <SDL_rect.h>
#include <SDL_render.h>
#include <SDL_surface.h>
#include <SDL_ttf.h>
#include <SDL_video.h>
#include <SDL_version.h>

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

static inline char *string_join(char *a, char *b) {
  if (!a || !b) { return NULL; }
  size_t a_len = strlen(a);
  size_t b_len = strlen(b);
  size_t new_len = a_len + b_len;
  char *out = malloc(new_len + 1);
  if (!out) { return NULL; }
  memcpy(out, a, a_len);
  memcpy(out + a_len, b, b_len);
  out[new_len] = '\0';
  return out;
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

// Uncomment the following definition for lots of debug output.
// #define DEBUG_TEXT_PROPERTIES

static inline void draw_gui_string_into_surface_within_rect
(GUIString string
 , SDL_Surface *surface
 , SDL_Rect *rect
 )
{
  if (!string.string || string.string[0] == '\0' || !surface || !rect) {
    return;
  }
  SDL_Surface *text_surface = NULL;
  // srcrect is the rectangle within text_surface that will be copied from.
  SDL_RECT_EMPTY(srcrect);
  if (!string.properties) {
#if SDL_TTF_VERSION_ATLEAST(2,0,18)
    text_surface = TTF_RenderUTF8_Shaded_Wrapped
      (font, string.string, fg, bg, rect->w);
#else
    text_surface = TTF_RenderUTF8_Blended_Wrapped
      (font, string.string, fg, rect->w);
#endif
    if (!text_surface) { return; }
  } else {
    // First line is blank for some reason.
    srcrect.y += font_height;
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
    SDL_Rect destination = *rect;
    SDL_Rect destination_copy = destination;
    while (1) {
      if (*string_contents == '\r'
          || *string_contents == '\n'
          || *string_contents == '\0')
        {
          size_t start_of_line_offset =
            (last_newline_offset == 0 ? 0 : last_newline_offset + 1);
          GUIStringProperty *it = string.properties;
          uint8_t prop_count = 0;
          uint8_t props_in_line = 0;
          size_t line_height = font_height;
          // zero-terminated list of count (index + 1) into linked list.
          uint8_t properties[256];
          while (it) {
            prop_count += 1;
            // Validate text property fields.
            if (it->length == 0) { continue; }
            // Somewhat protect against infinite looping.
            // TODO: It would be best to create a new list and check
            // that each new property has not already been visited, but
            // we trust LITE frontend more than that right now.
            if (it->next == it) { break; }
            // Ensure text property is within `start_of_line_offset` through `offset`.
            if (it->offset <= offset
                && it->offset + it->length > start_of_line_offset)
              {
                // TODO: Update font_height iff property height is larger.
                properties[props_in_line] = prop_count;
                props_in_line += 1;
              }
            it = it->next;
          }
          properties[props_in_line] = 0;

          if (!props_in_line || properties[0] == 0) {
            // no properties, render entire line using defaults.

            size_t bytes_to_render = offset - start_of_line_offset;
            char *line_text = allocate_string_span
              (string.string, start_of_line_offset, bytes_to_render);
            if (line_text) {
              if (line_text[0] != '\0') {
                SDL_Surface *line_text_surface =
                  TTF_RenderUTF8_Shaded(font, line_text, fg, bg);
                if (line_text_surface) {
                  // TODO: Handle BlitSurface failure everywhere (0 == success).
                  SDL_BlitSurface(line_text_surface, NULL, text_surface
                                  , rect_copy(&destination_copy,&destination));
                  SDL_FreeSurface(line_text_surface);
                }
              }
              free(line_text);
            }
          } else {
            // properties present, render portions of line at a time
            // and copy into the line surface.

            size_t horizontal_offset = 0;
            size_t prop_index = 0;
            prop_count = 0;
            it = string.properties;
            while (it) {
              if (prop_index + 1 == properties[prop_count]) {
                prop_count += 1;
                if (last_newline_offset + horizontal_offset < it->offset) {
                  size_t bytes_to_render = it->offset - horizontal_offset - start_of_line_offset;
                  char *prepended_text = allocate_string_span
                    (string.string, start_of_line_offset, bytes_to_render);
                  if (prepended_text) {
                    if (prepended_text[0] != '\0') {
#ifdef DEBUG_TEXT_PROPERTIES
                      printf("Prepended:      \"%s\"\n", prepended_text);
#endif
                      SDL_Surface *prepended_text_surface = TTF_RenderUTF8_Shaded
                        (font, prepended_text, fg, bg);
                      // TODO: Handle memory allocation failure during GUI redraw.
                      if (prepended_text_surface) {
                        SDL_BlitSurface(prepended_text_surface, NULL, text_surface
                                        , rect_copy(&destination_copy,&destination));
                        horizontal_offset += bytes_to_render;
                        destination.x += prepended_text_surface->w;
                        SDL_FreeSurface(prepended_text_surface);
                      }
                    }
                    free(prepended_text);
                  }
                }
                char *propertized_text = allocate_string_span
                  (string.string, it->offset, it->length);
                if (propertized_text) {
                  // Correctly display newline/end of string by inserting space.
                  if (propertized_text[0] == '\n'
                      || propertized_text[0] == '\r'
                      || propertized_text[0] == '\0')
                    {
                      propertized_text[0] = ' ';
                      propertized_text[1] = '\0';
                    }
                  SDL_Color prop_fg;
                  gui_color_to_sdl(&prop_fg, &it->fg);
                  SDL_Color prop_bg;
                  gui_color_to_sdl(&prop_bg, &it->bg);
#ifdef DEBUG_TEXT_PROPERTIES
                  printf("Propertized:    \"%s\"\n", propertized_text);
#endif
                  SDL_Surface *propertized_text_surface = TTF_RenderUTF8_Shaded
                    (font, propertized_text, prop_fg, prop_bg);
                  if (propertized_text_surface) {
                    SDL_BlitSurface(propertized_text_surface, NULL, text_surface
                                    , rect_copy(&destination_copy,&destination));
                    horizontal_offset += it->length;
                    destination.x += propertized_text_surface->w;
                    SDL_FreeSurface(propertized_text_surface);
                  }
                  free(propertized_text);
                }
              }
              prop_index += 1;
              it = it->next;
            }
            if (last_newline_offset + horizontal_offset < offset) {
              size_t bytes_to_render = offset - horizontal_offset - start_of_line_offset;
              char *appended_text = allocate_string_span
                (string.string
                 , start_of_line_offset + horizontal_offset
                 , bytes_to_render);
              // TODO: Handle memory allocation failure during GUI redraw.
              if (appended_text) {
                if (appended_text[0] != '\0') {
#ifdef DEBUG_TEXT_PROPERTIES
                  printf("Appended:       \"%s\"\n", appended_text);
#endif
                  SDL_Surface *appended_text_surface = TTF_RenderUTF8_Shaded
                    (font, appended_text, fg, bg);
                  if (appended_text_surface) {
                    SDL_BlitSurface(appended_text_surface, NULL, text_surface
                                    , rect_copy(&destination_copy,&destination));
                    horizontal_offset += bytes_to_render;
                    destination.x += appended_text_surface->w;
                    SDL_FreeSurface(appended_text_surface);
                  }
                }
                free(appended_text);
              }
            }
#ifdef DEBUG_TEXT_PROPERTIES
            printf("Handled line with text properties\n\n");
#endif
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
  rect_copy_size(&srcrect, rect);
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

int handle_events() {
  SDL_Event event;
  GUIModifierKey mod;
  while (SDL_PollEvent(&event)) {
    switch(event.type) {
    default:
      //printf("GFX::SDL: Unhandled event: %u\n", event.type);
      continue;
    case SDL_KEYDOWN:
      if ((mod = is_modifier(event.key.keysym.sym)) != GUI_MODKEY_MAX) {
        handle_modifier_dn(mod);
      } else {
        uint64_t c = event.key.keysym.sym;
        handle_character_dn(c);
      }
      break;
    case SDL_KEYUP:
      if ((mod = is_modifier(event.key.keysym.sym)) != GUI_MODKEY_MAX) {
        handle_modifier_up(mod);
      }
      break;
    case SDL_QUIT:
      return 0;
    }
  }
  return 1;
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
  return 1;
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
