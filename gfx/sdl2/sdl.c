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

void draw_gui(GUIContext *ctx) {
  draw_bg();
  if (ctx->title) {
    SDL_SetWindowTitle(gwindow, ctx->title);
    ctx->title = NULL;
  }
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
  size_t headline_line_count = count_lines(ctx->headline);
  size_t footline_line_count = count_lines(ctx->footline);

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

  if (ctx->headline && ctx->headline[0] != '\0') {
    SDL_Surface *headline = NULL;
    headline = TTF_RenderUTF8_Blended_Wrapped(font, ctx->headline, fg, width);
    if (headline) {
      SDL_BlitSurface(headline, NULL, surface, &rect_headline);
      SDL_FreeSurface(headline);
    }
  }
  if (ctx->contents && ctx->contents[0] != '\0') {
    SDL_Surface *contents = NULL;
    // TODO: Display each line on a newline!
    // TODO: Wrapped text vs unwrapped text.
    //       SDL_ttf V2.0.18 introduced _Wrapped functions to all
    //       of these functions, which is very, very handy!
    contents = TTF_RenderUTF8_Blended_Wrapped(font, ctx->contents, fg, width);
    if (contents) {
      SDL_BlitSurface(contents, NULL, surface, &rect_contents);
      SDL_FreeSurface(contents);
    }
  }
  if (ctx->footline && ctx->footline[0] != '\0') {
    SDL_Surface *footline = NULL;
    footline = TTF_RenderUTF8_Blended_Wrapped(font, ctx->footline, fg, width);
    if (footline) {
      SDL_BlitSurface(footline, NULL, surface, &rect_footline);
      SDL_FreeSurface(footline);
    }
  }
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

// Returns a boolean-like integer value (0 is false).
void do_gui(int *open, GUIContext *ctx) {
  if (!open || *open == 0 || !ctx) {
    return;
  }
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
      *open = 0;
      return;
    }
  }
  draw_gui(ctx);
}

void destroy_gui() {
  SDL_DestroyRenderer(grender);
  SDL_DestroyWindow(gwindow);
  TTF_CloseFont(font);
  TTF_Quit();
  SDL_Quit();
}
