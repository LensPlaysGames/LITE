// This file will be compiled into a library
// that is then linked with, after compilation.

#include <api.h>
#include <gui.h>
#include <stdint.h>
#include <stdlib.h>


#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_ttf.h>

#include <stdio.h>

SDL_Color bg = { 18, 18, 18, UINT8_MAX };
SDL_Color fg = { UINT8_MAX, UINT8_MAX, UINT8_MAX, UINT8_MAX };

SDL_Window *gwindow = NULL;
SDL_Renderer *grender = NULL;
SDL_Texture *texture = NULL;

TTF_Font *font = NULL;

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
  printf("GFX::SDL: SDL_ttf Initialized\n");
  return 0;
}

static inline void draw_bg() {
  SDL_SetRenderDrawColor(grender, bg.r, bg.g, bg.b, bg.a);
  SDL_RenderClear(grender);
}

void draw_gui(GUIContext *ctx) {
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

  // TODO: Align on character grid for current font size.
  SDL_Rect rect_headline;
  rect_headline.x = 0;
  rect_headline.y = 0;
  rect_headline.w = width;
  rect_headline.h = height * 0.05;
  SDL_Rect rect_contents;
  rect_contents.x = 0;
  rect_contents.y = rect_headline.h + rect_headline.y;
  rect_contents.w = width;
  rect_contents.h = height * 0.90;
  SDL_Rect rect_footline;
  rect_footline.x = 0;
  rect_footline.y = rect_contents.h + rect_contents.y;
  rect_footline.w = width;
  rect_footline.h = height * 0.05;

  if (ctx->headline) {
    SDL_Surface *headline = NULL;
    headline = TTF_RenderUTF8_Blended(font, ctx->headline, fg);
    if (!headline) {
      printf("GFX::SDL: Could not render headline text.\n");
      return;
    }
    SDL_BlitSurface(headline, NULL, surface, &rect_headline);
    SDL_FreeSurface(headline);
  }
  if (ctx->contents) {
    SDL_Surface *contents = NULL;
    // TODO: Display each line on a newline!
    // TODO: Wrapped text vs unwrapped text.
    //       SDL_ttf V2.0.18 introduced _Wrapped functions to all
    //       of these functions, which is very, very handy!
    contents = TTF_RenderUTF8_Blended(font, ctx->contents, fg);
    if (!contents) {
      printf("GFX::SDL: Could not render contents text.\n");
      return;
    }
    SDL_BlitSurface(contents, NULL, surface, &rect_contents);
    SDL_FreeSurface(contents);
  }
  if (ctx->footline) {
    SDL_Surface *footline = NULL;
    footline = TTF_RenderUTF8_Blended(font, ctx->footline, fg);
    if (!footline) {
      printf("GFX::SDL: Could not render footline text.\n");
      return;
    }
    SDL_BlitSurface(footline, NULL, surface, &rect_footline);
    SDL_FreeSurface(footline);
  }
  texture = SDL_CreateTextureFromSurface(grender, surface);
  SDL_FreeSurface(surface);
  if (!texture) {
    printf("GFX::SDL:ERROR: Could not create texture from surface.\n"
           "              : %s\n"
           , SDL_GetError()
           );
    return;
  }
  SDL_RenderCopy(grender, texture, NULL, NULL);
  SDL_RenderPresent(grender);
}

// Returns GUI_MODKEY_MAX for any key that is not a modifier,
// otherwise returns the corresponding GUI_MODKEY_* enum.
GUIModifierKey is_modifier(SDL_KeyCode key) {
  switch (key) {
  default:
    return GUI_MODKEY_MAX;
    break;
  case SDLK_LCTRL:
    return GUI_MODKEY_LCTRL;
    break;
  case SDLK_RCTRL:
    return GUI_MODKEY_RCTRL;
    break;
  case SDLK_LALT:
    return GUI_MODKEY_LALT;
    break;
  case SDLK_RALT:
    return GUI_MODKEY_RALT;
    break;
  case SDLK_LSHIFT:
    return GUI_MODKEY_LSHIFT;
    break;
  case SDLK_RSHIFT:
    return GUI_MODKEY_RSHIFT;
    break;
  }
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
        handle_character_dn(event.key.keysym.sym);
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
      printf("GFX::SDL: Quitting\n");
      destroy_gui();
      exit(0);
    }
  }
  draw_gui(ctx);
  *open = 1;
}

void destroy_gui() {
  SDL_DestroyRenderer(grender);
  SDL_DestroyWindow(gwindow);
  if (font) {
    TTF_CloseFont(font);
  }
  TTF_Quit();
  SDL_Quit();
}
