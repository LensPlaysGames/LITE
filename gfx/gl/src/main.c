#include <gui.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <shade.h>

typedef struct GLColor {
  float r;
  float g;
  float b;
  float a;
} GLColor, GLColour;

#define glcolor_from_gui(c) (GLColor){ .r = c.r / UINT8_MAX, .g = c.g / UINT8_MAX, .b = c.b / UINT8_MAX, .a = c.a / UINT8_MAX }
#define guicolor_from_gl(c) (GUIColor){ .r = c.r * UINT8_MAX, .g = c.g * UINT8_MAX, .b = c.b * UINT8_MAX, .a = c.a * UINT8_MAX }


typedef struct vec2 {
  GLfloat x;
  GLfloat y;
} vec2;

typedef struct vec3 {
  GLfloat x;
  GLfloat y;
  GLfloat z;
} vec3;

typedef struct vec4 {
  GLfloat x;
  GLfloat y;
  GLfloat z;
  GLfloat w;
} vec4;

typedef struct Vertex {
  vec2 position;
  vec2 uv;
  vec4 color;
} Vertex;

typedef struct Renderer {
  size_t vertex_capacity;
  size_t vertex_count;
  Vertex *vertices;


  /** Vertex Array Object
   * Allows quick binding/unbinding of different VBOs without
   * creating/destroying them all the time.
   */
  GLuint vao;
  /** Vertex Buffer Object
   * Stores vertex attribute configuration.
   */
  GLuint vbo;
  /// Shader program
  GLuint shader;
} Renderer;

static void r_vertex(Renderer *r, Vertex v) {
  if (!r->vertices) {
    r->vertex_count = 0;
    // Megabyte of vertices.
    r->vertex_capacity = ((2 << 16) / sizeof(Vertex));
    r->vertices = malloc(sizeof(*r->vertices) * r->vertex_capacity);
  }
  if (r->vertex_count + 1 >= r->vertex_capacity) {
    // TODO: Expand, try again.
    return;
  }
  r->vertices[r->vertex_count++] = v;
}

static void r_tri(Renderer *r, Vertex a, Vertex b, Vertex c) {
  r_vertex(r, a);
  r_vertex(r, b);
  r_vertex(r, c);
}

static void r_quad(Renderer *r, Vertex tl, Vertex tr, Vertex bl, Vertex br) {
  r_tri(r, bl, tl, br);
  r_tri(r, br, tl, tr);
}

static void r_update(Renderer *r) {
  glBindVertexArray(r->vao);
  glBindBuffer(GL_ARRAY_BUFFER, r->vbo);
  glBufferSubData(GL_ARRAY_BUFFER, 0, r->vertex_count * sizeof(Vertex), r->vertices);
}

static void r_clear(Renderer *r) {
  r->vertex_count = 0;
}

struct {
  GLColor bg;

  GLFWwindow *window;
  size_t width;
  size_t height;

  Renderer rend;
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

  glGenVertexArrays(1, &g.rend.vao);
  glGenBuffers(1, &g.rend.vbo);

  g.rend.vertex_count = 0;
  g.rend.vertex_capacity = ((2 << 16) / sizeof(Vertex));
  g.rend.vertices = malloc(sizeof(*g.rend.vertices) * g.rend.vertex_capacity);

  glBindVertexArray(g.rend.vao);
  glBindBuffer(GL_ARRAY_BUFFER, g.rend.vbo);
  glBufferData(GL_ARRAY_BUFFER, g.rend.vertex_capacity * sizeof(Vertex), g.rend.vertices, GL_DYNAMIC_DRAW);

  // The layout of the data that the shaders are expecting is specified here.
  // This must match the `layout`s at the top of the vertex shader.
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(0,            //> Which attribute to configure
    2,                                //> Number of elements in attribute
    GL_FLOAT,                         //> Type of each element
    GL_FALSE,                         //> Normalise
    sizeof(Vertex),                   //> Stride
    (void*)offsetof(Vertex, position) //> Offset
  );
  glVertexAttribPointer(1,
    2,
    GL_FLOAT,
    GL_FALSE,
    sizeof(Vertex),
    (void*)offsetof(Vertex, uv)
  );
  glVertexAttribPointer(2,
    4,
    GL_FLOAT,
    GL_FALSE,
    sizeof(Vertex),
    (void*)offsetof(Vertex, color)
  );

  GLuint frag;
  GLuint vert;
  bool success;
  // TODO: Look in lite directory for shaders.
  success = shader_compile("gfx/gl/shaders/vert.glsl", GL_VERTEX_SHADER, &vert);
  if (!success) return CREATE_GUI_ERR;
  success = shader_compile("gfx/gl/shaders/frag.glsl", GL_FRAGMENT_SHADER, &frag);
  if (!success) return CREATE_GUI_ERR;
  success = shader_program(&g.rend.shader, vert, frag);
  if (!success) return CREATE_GUI_ERR;

  // http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-10-transparency/
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

  return CREATE_GUI_OK;
}
static void fini_opengl()  {
  glDeleteProgram(g.rend.shader);
  glDeleteBuffers(1, &g.rend.vbo);
  glDeleteVertexArrays(1, &g.rend.vao);
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

  glBindVertexArray(g.rend.vao);
  glUseProgram(g.rend.shader);

  Vertex bl  = (Vertex){ .position = {-0.5, -0.5}, .uv = { 0.0,  0.0}, .color = { 1.0,  0.0,  0.0,  1.0} };
  Vertex top = (Vertex){ .position = { 0.0,  0.5}, .uv = { 0.0,  0.0}, .color = { 1.0,  0.0,  0.0,  1.0} };
  Vertex br  = (Vertex){ .position = { 0.5, -0.5}, .uv = { 0.0,  0.0}, .color = { 1.0,  0.0,  0.0,  1.0} };

  r_clear(&g.rend);
  r_tri(&g.rend, bl, top, br);
  r_update(&g.rend);

  // Use shader program.
  glUseProgram(g.rend.shader);
  // Binding the VAO "remembers" the bound buffers and vertex
  // attributes and such, which is way nicer than having to set all
  // that state manually, and also a great hardware speedup.
  glBindVertexArray(g.rend.vao);
  // Draw amount of triangles that are present in the current frame...
  glDrawArrays(GL_TRIANGLES, 0, g.rend.vertex_count);

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

