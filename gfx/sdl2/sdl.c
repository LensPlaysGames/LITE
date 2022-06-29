#include <api.h>
#include <gui.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_ttf.h>

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
  strncpy(out, string + offset, length);
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

int create_gui() {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    printf("GFX::SDL:ERROR: Failed to initialize SDL.\n");
    return 1;
  }
  gwindow = SDL_CreateWindow
    ("GUI BASIC"
     , SDL_WINDOWPOS_UNDEFINED
     , SDL_WINDOWPOS_UNDEFINED
     , 640, 480
     , SDL_WINDOW_SHOWN
     | SDL_WINDOW_RESIZABLE
     | SDL_WINDOW_ALLOW_HIGHDPI
     | SDL_WINDOW_INPUT_FOCUS
     );
  if (!gwindow) {
    printf("GFX::SDL:ERROR: Could not create SDL window.\n");
    return 1;
  }
  grender = SDL_CreateRenderer(gwindow, -1, SDL_RENDERER_SOFTWARE);
  if (!grender) {
    printf("GFX::SDL:ERROR: Could not create SDL renderer.\n");
    return 1;
  }
  printf("GFX::SDL: SDL Initialized\n");
  if (TTF_Init() != 0) {
    printf("GFX::SDL:ERROR: Failed to initialize SDL_ttf.\n");
    return 1;
  }
  // First, assume working directory of base of the repository.
  font = TTF_OpenFont("gfx/sdl2/DroidSans.ttf", 18);
  // Next, assume working directory of bin repository subdirectory.
  if (!font) { font = TTF_OpenFont("../gfx/sdl2/DroidSans.ttf", 18); }
  // Next, search current directory.
  if (!font) { font = TTF_OpenFont("DroidSans.ttf", 18); }
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
  if (!string.properties) {
    text_surface = TTF_RenderUTF8_Shaded_Wrapped(font, string.string, fg, bg, rect->w);
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
    SDL_Rect destination = *rect;
    while (1) {
      if (*string_contents == '\r'
          || *string_contents == '\n'
          || *string_contents == '\0')
        {
          size_t start_of_line_offset = (last_newline_offset == 0 ? 0 : last_newline_offset + 1);
          destination.x = 0;
          GUIStringProperty *it = string.properties;
          uint8_t prop_count = 0;
          uint8_t props_in_line = 0;
          size_t line_height = font_height;
          // null-terminated list of offset (index + 1) into linked list.
          uint8_t properties[256];
          while (it) {
            prop_count += 1;
            if (it->offset < offset
                && it->offset + it->length > last_newline_offset) {
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
            if (!line_text) { return; }
            SDL_Surface *line_text_surface = TTF_RenderUTF8_Shaded
              (font, line_text, fg, bg);
            free(line_text);
              // TODO: Handle memory allocation failure during GUI redraw.
            if (line_text_surface) {
              SDL_BlitSurface(line_text_surface, NULL, text_surface, &destination);
              SDL_FreeSurface(line_text_surface);
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
                  // TODO: Handle memory allocation failure during GUI redraw.
                  if (prepended_text && prepended_text[0] != '\0') {
                    SDL_Surface *prepended_text_surface = TTF_RenderUTF8_Shaded
                      (font, prepended_text, fg, bg);
                    free(prepended_text);
                    // TODO: Handle memory allocation failure during GUI redraw.
                    if (prepended_text_surface) {
                      SDL_BlitSurface(prepended_text_surface, NULL, text_surface, &destination);
                      horizontal_offset += bytes_to_render;
                      destination.x += prepended_text_surface->w;
                      SDL_FreeSurface(prepended_text_surface);
                    }
                  }
                }

                char *propertized_text = allocate_string_span
                  (string.string, it->offset, it->length);
                if (!propertized_text) {
                  // TODO: Handle memory allocation failure during GUI redraw.
                  return;
                }
                if (propertized_text[0] != '\n'
                    && propertized_text[0] != '\r'
                    && propertized_text[0] != '\0')
                  {
                    SDL_Color prop_fg;
                    gui_color_to_sdl(&prop_fg, &it->fg);
                    SDL_Color prop_bg;
                    gui_color_to_sdl(&prop_bg, &it->bg);
                    SDL_Surface *propertized_text_surface = TTF_RenderUTF8_Shaded
                      (font, propertized_text, prop_fg, prop_bg);
                    free(propertized_text);
                    if (!propertized_text_surface) {
                      // TODO: Handle memory allocation failure during GUI redraw.
                      return;
                    }
                    SDL_BlitSurface(propertized_text_surface, NULL, text_surface, &destination);
                    horizontal_offset += it->length;
                    destination.x += propertized_text_surface->w;
                    SDL_FreeSurface(propertized_text_surface);
                  }

                prop_index += 1;
                it = it->next;
              }
            }
            if (last_newline_offset + horizontal_offset < offset) {
              size_t bytes_to_render = offset - horizontal_offset - (last_newline_offset == 0 ? 0 : last_newline_offset + 1);
              char *appended_text = allocate_string_span
                (string.string
                 , (last_newline_offset == 0 ? 0 : last_newline_offset + 1) + horizontal_offset
                 , bytes_to_render);
              // TODO: Handle memory allocation failure during GUI redraw.
              if (appended_text && appended_text[0] != '\0') {
                SDL_Surface *appended_text_surface = TTF_RenderUTF8_Shaded
                  (font, appended_text, fg, bg);
                free(appended_text);
                // TODO: Handle memory allocation failure during GUI redraw.
                if (appended_text_surface) {
                  SDL_BlitSurface(appended_text_surface, NULL, text_surface, &destination);
                  horizontal_offset += bytes_to_render;
                  destination.x += appended_text_surface->w;
                  SDL_FreeSurface(appended_text_surface);
                }
              }
            }
          }
          destination.y += line_height;
          destination.h -= line_height;
          last_newline_offset = offset;
        }
      if (*string_contents == '\0') {
        break;
      }
      string_contents += 1;
      offset += 1;
    }
  }
  SDL_BlitSurface(text_surface, NULL, surface, rect);
  SDL_FreeSurface(text_surface);
}

void draw_gui(GUIContext *ctx) {
  if (ctx->title) {
    SDL_SetWindowTitle(gwindow, ctx->title);
    ctx->title = NULL;
  }

  // Update default text property from context.
  gui_color_to_sdl(&fg, &ctx->default_property.fg);
  gui_color_to_sdl(&bg, &ctx->default_property.bg);

  // Clear screen to background color.
  draw_bg();

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

  // TODO: Handle wrapping of long lines.
  size_t headline_line_count = count_lines(ctx->headline.string);
  size_t footline_line_count = count_lines(ctx->footline.string);

  size_t headline_height = font_height * headline_line_count;
  size_t footline_height = font_height * footline_line_count;

  // TODO: Handle corner case when there is not enough room for contents.
  size_t contents_height = height - headline_height - footline_height;

  SDL_Rect rect_headline;
  rect_headline.x = 0;
  rect_headline.y = 0;
  rect_headline.w = width;
  rect_headline.h = headline_height;
  SDL_Rect rect_contents;
  rect_contents.x = 0;
  rect_contents.y = rect_headline.h + rect_headline.y;
  rect_contents.w = width;
  rect_contents.h = contents_height;
  SDL_Rect rect_footline;
  rect_footline.x = 0;
  rect_footline.y = rect_contents.h + rect_contents.y;
  rect_footline.w = width;
  rect_footline.h = footline_height;

  draw_gui_string_into_surface_within_rect(ctx->headline, surface, &rect_headline);
  draw_gui_string_into_surface_within_rect(ctx->contents, surface, &rect_contents);
  draw_gui_string_into_surface_within_rect(ctx->footline, surface, &rect_footline);

  texture = SDL_CreateTextureFromSurface(grender, surface);
  SDL_FreeSurface(surface);
  if (texture) {
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
      break;
    case SDL_KEYDOWN:
      if ((mod = is_modifier(event.key.keysym.sym)) != GUI_MODKEY_MAX) {
        handle_modifier_dn(mod);
      } else {
        // TODO: Convert `SDLK_` keycodes to UTF8.
        // Maybe look into SDL TEXTINPUT event?
        uint64_t c = event.key.keysym.sym;
        handle_character_dn(c);
      }
      break;
    case SDL_KEYUP:
      if ((mod = is_modifier(event.key.keysym.sym)) != GUI_MODKEY_MAX) {
        handle_modifier_up(mod);
      } else {
        handle_character_up(event.key.keysym.sym);
      }
      break;
    case SDL_QUIT:
      return 0;
    }
  }
  return 1;
}

// Returns a boolean-like integer value (0 is false).
void do_gui(int *open, GUIContext *ctx) {
  if (!open || *open == 0 || !ctx) {
    return;
  }
  *open = handle_events();
  if (*open == 0) { return; }
  draw_gui(ctx);
}

void destroy_gui() {
  SDL_DestroyRenderer(grender);
  SDL_DestroyWindow(gwindow);
  TTF_CloseFont(font);
  TTF_Quit();
  SDL_Quit();
}
