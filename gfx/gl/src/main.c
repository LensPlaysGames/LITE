#include <gui.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <freetype/freetype.h>

#include <hb.h>
#include <hb-ft.h>

#include <inttypes.h>
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
    r->vertex_capacity = ((1 << 20) / sizeof(Vertex));
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


typedef struct Glyph {
  uint32_t codepoint;
  uint32_t glyph_index;
  uint32_t hash;
  // X offset into glyph atlas to get to this glyph's bitmap.
  size_t x;
  // Size of the bitmap within glyph atlas
  uint32_t bmp_w; //> bitmap width
  uint32_t bmp_h; //> bitmap height
  // Offset within the bitmap within glyph atlas to draw
  int64_t bmp_x; //> bitmap x offset
  int64_t bmp_y; //> bitmap y offset
  // X offset into glyph atlas as a float between 0 and 1
  float uvx;
  // Y offset into glyph atlas as a float between 0 and 1
  float uvy;
  // X max offset into glyph atlas as a float between 0 and 1
  float uvx_max;
  // Y max offset into glyph atlas as a float between 0 and 1
  float uvy_max;
  /// Equal to `true` iff this glyph structure is valid.
  bool populated;
} Glyph;

/// Open addressed hash map of UTF32 codepoint -> Glyph
typedef struct GlyphMap {
  FT_Face face; //> Face that atlas is rendered with.

  GLuint atlas; //> Glyph atlas
  size_t atlas_width;
  size_t atlas_height;
  size_t atlas_draw_offset; //> Offset into atlas texture to begin drawing new glyphs.

  size_t glyph_capacity;
  size_t glyph_count;
  Glyph *glyphs;
} GlyphMap;

// https://crypto.stackexchange.com/a/16231
// some arbitrary permutation of 32-bit values
uint32_t hash_perm32(uint32_t x) {
  // Repeat for n from 12 down to 1
  int n = 12;
  do x = ((x>>8)^x)*0x6B+n;
  while( --n!=0 );
  return x;
}

#ifndef glyph_map_hash
#define glyph_map_hash hash_perm32
#endif

// By default, make an 8 megabyte glyph atlas.
#ifndef GLYPH_ATLAS_WIDTH
# define GLYPH_ATLAS_WIDTH (2 << 13)
#endif
#ifndef GLYPH_ATLAS_HEIGHT
// Hopefully one glyph is never taller than this, otherwise it will
// cause a major slowdown in having to reallocate a new buffer and
// copy data...
# define GLYPH_ATLAS_HEIGHT (2 << 8)
#endif

/// Set min/mag texture filters based on whether or not we want to use
/// anti aliasing, and whether or not mipmaps should be used for the
/// texture. Operates on currently bound texture.
static void antialiasing(GLboolean enable, GLboolean use_mipmaps) {
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  if (enable) {
    if (use_mipmaps) glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    else glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  } else {
    if (use_mipmaps) glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    else glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  }
}

void glyph_map_init(GlyphMap *map, FT_Face face) {
  if (!map) return;

  map->face = face;

  // Megabyte of glyphs...
  map->glyph_capacity = ((1 << 20) / sizeof(*map->glyphs));
  map->glyph_count = 0;
  map->glyphs = calloc(map->glyph_capacity, sizeof(*map->glyphs));

  map->atlas_width = GLYPH_ATLAS_WIDTH;
  map->atlas_height = GLYPH_ATLAS_HEIGHT;
  map->atlas_draw_offset = 0;
  glGenTextures(1, &map->atlas);
  glBindTexture(GL_TEXTURE_2D, map->atlas);
  antialiasing(GL_TRUE, GL_TRUE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(
    GL_TEXTURE_2D,
    0,
    GL_RED,
    map->atlas_width,
    map->atlas_height,
    0,
    GL_RED,
    GL_UNSIGNED_BYTE,
    NULL
  );
}

/// Return matching or free entry.
Glyph *glyph_map_entry(GlyphMap map, uint32_t codepoint) {
  uint32_t hash = glyph_map_hash(codepoint);
  size_t index = hash % map.glyph_capacity;
  Glyph *entry = map.glyphs + index;
  // Start search at hash index.
  for (size_t i = index; i < map.glyph_capacity; ++i) {
    // Free entry
    if (!entry->populated) {
      entry->hash = hash;
      return entry;
    }
    // Matching entry
    if (entry->codepoint == codepoint) {
      entry->hash = hash;
      return entry;
    }
  }
  // Start search from beginning.
  for (size_t i = 0; i < index; ++i) {
    // Free entry
    if (!entry->populated) {
      entry->hash = hash;
      return entry;
    }
    // Matching entry
    if (entry->codepoint == codepoint) {
      entry->hash = hash;
      return entry;
    }
  }
  return NULL;
}

/// Populate `entry` with data corresponding to `codepoint`.
static Glyph *glyph_map_populate(GlyphMap map, Glyph *entry, uint32_t codepoint) {
  // Populate entry using freetype, draw into glyph atlas.
  FT_Error ft_err = FT_Load_Char(map.face, codepoint, FT_LOAD_DEFAULT);
  if (ft_err) {
    fprintf(stderr, "FreeType could not load char (error %d) corresponding to codepoint 0x%"PRIx32"\n", ft_err, codepoint);
    return NULL;
  }

  // TODO: SDF, LCD
  ft_err = FT_Render_Glyph(map.face->glyph, FT_RENDER_MODE_NORMAL);
  if (ft_err) {
    fprintf(stderr, "FreeType could not render glyph (error %d) corresponding to codepoint 0x%"PRIx32"\n", ft_err, codepoint);
    return NULL;
  }

  map.glyph_count += 1;
  if (map.glyph_count >= map.glyph_capacity) {
    fprintf(stderr, "TODO: Expand glyph map.");
    return NULL;
  }

  entry->codepoint = codepoint;
  entry->glyph_index = map.face->glyph->glyph_index;
  entry->x = map.atlas_draw_offset + 1;
  entry->bmp_w = map.face->glyph->bitmap.width;
  entry->bmp_h = map.face->glyph->bitmap.rows;
  entry->bmp_x = map.face->glyph->bitmap_left;
  entry->bmp_y = map.face->glyph->bitmap_top;
  entry->uvx = (float)entry->x / map.atlas_width;
  entry->uvy = (float)entry->bmp_h / map.atlas_height;
  entry->uvx_max = entry->uvx + ((float)entry->bmp_w / map.atlas_width);
  entry->uvy_max = 0;
  if (entry->uvx_max < entry->uvx) {
    fprintf(stderr, "ERROR: uvx_max lte uvx: %f difference\n", ((float)entry->bmp_w / map.atlas_width));
    return NULL;
  }
  if (entry->uvy_max > entry->uvy) {
    fprintf(stderr, "ERROR: uvy_max gte uvy\n");
    return NULL;
  }
  if (map.atlas_draw_offset + entry->bmp_w >= map.atlas_width) {
    fprintf(stderr, "TODO: Expand glyph atlas texture width-wise.");
    return NULL;
  }
  if (entry->bmp_h > map.atlas_height) {
    fprintf(stderr, "TODO: Expand glyph atlas height-wise.");
    return NULL;
  }
  glBindTexture(GL_TEXTURE_2D, map.atlas);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexSubImage2D(
                  GL_TEXTURE_2D,
                  0,
                  entry->x,
                  0,
                  entry->bmp_w,
                  entry->bmp_h,
                  GL_RED,
                  GL_UNSIGNED_BYTE,
                  map.face->glyph->bitmap.buffer
                  );
  glGenerateMipmap(GL_TEXTURE_2D);

  map.atlas_draw_offset += entry->bmp_w + 2;

  return entry;
}

Glyph *glyph_map_find_or_add(GlyphMap map, uint32_t codepoint) {
  Glyph *entry = glyph_map_entry(map, codepoint);
  if (!entry) return NULL;
  if (entry->populated) return entry;
  else return glyph_map_populate(map, entry, codepoint);
}

/// Freetype is used to get glyph metrics and cache them in the glyph
/// map, as well as render the glyph and put the contents into the
/// glyph atlas found at `texture`.
/// Text renderer uses glyph metrics in order to be able to draw the
/// glyph, and uses the harfbuzz font to put the glyphs into position.

typedef struct GLFace {
  // TODO: Allow multiple sets of these members corresponding to multiple font sizes.

  // Hash map containing codepoint -> glyph rendering data.
  GlyphMap glyph_map;
  FT_Face ft_face;
  hb_font_t *hb_font;
} GLFace;

struct {
  GLColor bg;

  GLFWwindow *window;
  size_t width;
  size_t height;

  Renderer rend;

  FT_Library ft;

  GLFace face;
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

  FT_Error ft_err = FT_Init_FreeType(&g.ft);
  if (ft_err) {
    fprintf(stderr, "Could not initialize freetype, sorry\n");
    return CREATE_GUI_ERR;
  }
#ifdef _WIN32
  const char *const facepath = "C:/Windows/Fonts/arial.ttf";
#elif defined(__unix__)
  // FIXME: idk lol, I'm not a unix guru
  const char *const facepath = "/usr/share/fonts/Iosevka.ttf";
#endif
  ft_err = FT_New_Face(g.ft, facepath, 0, &g.face.ft_face);
  if (ft_err) {
    fprintf(stderr, "Could not initialize freetype face from font file at %s, sorry\n", facepath);
    return CREATE_GUI_ERR;
  }
  FT_Set_Pixel_Sizes(g.face.ft_face, 0, 512);

  glyph_map_init(&g.face.glyph_map, g.face.ft_face);

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

  r_clear(&g.rend);

  Glyph *glyph = glyph_map_find_or_add(g.face.glyph_map, 'E');
  Vertex tl = (Vertex){ .position = {-0.5,  0.5}, .uv = { glyph->uvx,     glyph->uvy_max}, .color = { 1.0,  1.0,  1.0,  1.0} };
  Vertex tr = (Vertex){ .position = { 0.5,  0.5}, .uv = { glyph->uvx_max, glyph->uvy_max}, .color = { 1.0,  1.0,  1.0,  1.0} };
  Vertex bl = (Vertex){ .position = {-0.5, -0.5}, .uv = { glyph->uvx,     glyph->uvy},     .color = { 1.0,  1.0,  1.0,  1.0} };
  Vertex br = (Vertex){ .position = { 0.5, -0.5}, .uv = { glyph->uvx_max, glyph->uvy},     .color = { 1.0,  1.0,  1.0,  1.0} };
  r_quad(&g.rend, tl, tr, bl, br);

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

