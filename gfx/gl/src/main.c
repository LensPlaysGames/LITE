#include <gui.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct GLColor {
  float r;
  float g;
  float b;
  float a;
} GLColor, GLColour;

#define glcolor_from_gui(c) (GLColor){ .r = c.r / UINT8_MAX, .g = c.g / UINT8_MAX, .b = c.b / UINT8_MAX, .a = c.a / UINT8_MAX }
#define guicolor_from_gl(c) (GUIColor){ .r = c.r * UINT8_MAX, .g = c.g * UINT8_MAX, .b = c.b * UINT8_MAX, .a = c.a * UINT8_MAX }


struct {
  GLColor bg;

  GLFWwindow *window;
  size_t width;
  size_t height;
} g;


void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
  if (width < 0 || height < 0) return;
  g.width  = (size_t)width;
  g.height = (size_t)height;
  glViewport(0, 0, g.width, g.height);
}
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  // TODO: Lots of keystring stuff...
  if (action == GLFW_PRESS) {
    switch (key) {
    default: return;
    }
  }
}


static int init_glfw() {
  if (glfwInit() == GLFW_FALSE) {
    fprintf(stderr, "Could not initialise GLFW\n");
    return CREATE_GUI_ERR;
  }
#ifndef DISABLE_TRANSPARENCY
  glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
#endif

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  g.width  = 640;
  g.height = 480;
  g.window = glfwCreateWindow(g.width, g.height, "LITE GL", NULL, NULL);
  if (!g.window) {
    fprintf(stderr, "Could not create GLFW window");
    return CREATE_GUI_ERR;
  }
  glfwSetFramebufferSizeCallback(g.window, framebuffer_size_callback);
  glfwSetKeyCallback(g.window, key_callback);
  glfwMakeContextCurrent(g.window);
  return CREATE_GUI_OK;
}
static void fini_glfw() {
  if (g.window) glfwDestroyWindow(g.window);
  glfwTerminate();
}

static int init_opengl() {
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    fprintf(stderr, "Could not load OpenGL (GLAD failure)");
    return CREATE_GUI_ERR;
  }

  glViewport(0, 0, g.width, g.height);

  // TODO: Generate VAO/VBO, create basic vertex renderer

  // http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-10-transparency/
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

  return CREATE_GUI_OK;
}
static void fini_opengl()  {
  ;
}



static char created = 0;
int create_gui() {
  if (created) return CREATE_GUI_ALREADY_CREATED;
  created = 1;

  g.bg = (GLColor){ .r = (float)22 / UINT8_MAX, .g = (float)23 / UINT8_MAX, .b = (float)24 / UINT8_MAX, .a = ((float)UINT8_MAX / 2) / UINT8_MAX };

  int status = init_glfw();
  if (status != CREATE_GUI_OK) return status;

  status = init_opengl();
  if (status != CREATE_GUI_OK) return status;

  return CREATE_GUI_OK;
}

void destroy_gui() {
  if (created) {
    fini_opengl();
    fini_glfw();
    created = 0;
  }
}

int handle_events() {
  glfwPollEvents();
  return 1;
}

void draw_gui(GUIContext *ctx) {
  if (!ctx) return;
  if (ctx->title) {
    glfwSetWindowTitle(g.window, ctx->title);
    ctx->title = NULL;
  }

  glClearColor(g.bg.r, g.bg.g, g.bg.b, g.bg.a);
  glClear(GL_COLOR_BUFFER_BIT);

  glfwSwapBuffers(g.window);
}

int do_gui(GUIContext *ctx) {
  if (!ctx) return 0;

  handle_events();
  if (glfwWindowShouldClose(g.window)) return 0;

  draw_gui(ctx);

  glfwWaitEvents();
  if (glfwWindowShouldClose(g.window)) return 0;
  return 1;
}



int change_font(const char *path, size_t size) {
  if (!path) return 1;
  // TODO
  return 1;
}

int change_font_size(size_t size) {
  // TODO
  return 1;
}

void window_size(size_t *width, size_t *height) {
  if (!width || !height) return;
  *width = g.width;
  *height = g.height;
}

void window_size_row_col(size_t *rows, size_t *cols) {
  if (!rows || !cols) return;
  // TODO: measure
  *rows = 1;
  *cols = 1;
}

void change_window_size(size_t width, size_t height) {
  g.width = width;
  g.height = height;

  // TODO: glfwSetFramebufferSize is in screen coordinates...
  // Need to get pixel coordinates and screen coordinates, compare to
  // get ratio (separately for each coordinate), then multiply input by
  // that in order to set proper size...
  //glfwSetFramebufferSize(g.window, g.width, g.height);

  //glViewport(0, 0, g.width, g.height);
}

void set_clipboard_utf8(const char *data) {
  if (!data) return;
  glfwSetClipboardString(g.window, data);
}

char *get_clipboard_utf8() {
  const char *clip = glfwGetClipboardString(g.window);
  if (!clip) return NULL;
  size_t len = strlen(clip);
  char *buf = malloc(len + 1);
  strncpy(buf, clip, len + 1);
  buf[len] = '\0';
  return buf;
}

char has_clipboard_utf8() {
  return glfwGetClipboardString(g.window) != 0;
}

void change_window_mode(enum GFXWindowMode mode) {
  if (mode == GFX_WINDOW_MODE_WINDOWED) glfwRestoreWindow(g.window);
  else if (mode == GFX_WINDOW_MODE_FULLSCREEN) glfwMaximizeWindow(g.window);
}

void change_window_visibility(enum GFXWindowVisible visible) {
  if (visible == GFX_WINDOW_VISIBLE) glfwShowWindow(g.window);
  else glfwHideWindow(g.window);
}

