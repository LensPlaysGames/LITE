#include <api.h>
#include <gui.h>
#include <keystrings.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <freetype/freetype.h>

#include <hb.h>
#include <hb-ft.h>

#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <shade.h>

static size_t count_lines(char *str) {
  if (!str) return 0;
  size_t line_count = 0;
  char *string_iterator = str;
  do {
    line_count += 1;
    string_iterator = strchr(string_iterator + 1, '\n');
  } while (string_iterator);
  return line_count;
}

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
static Glyph *glyph_map_populate(GlyphMap *map, Glyph *entry, uint32_t codepoint) {
  if (!map) return NULL;
  // Populate entry using freetype, draw into glyph atlas.
  FT_Error ft_err = FT_Load_Char(map->face, codepoint, FT_LOAD_DEFAULT);
  if (ft_err) {
    fprintf(stderr, "FreeType could not load char (error %d) corresponding to codepoint 0x%"PRIx32"\n", ft_err, codepoint);
    return NULL;
  }

  // TODO: SDF, LCD
  ft_err = FT_Render_Glyph(map->face->glyph, FT_RENDER_MODE_NORMAL);
  if (ft_err) {
    fprintf(stderr, "FreeType could not render glyph (error %d) corresponding to codepoint 0x%"PRIx32"\n", ft_err, codepoint);
    return NULL;
  }

  map->glyph_count += 1;
  if (map->glyph_count >= map->glyph_capacity) {
    fprintf(stderr, "TODO: Expand glyph map.");
    return NULL;
  }

  entry->populated = true;
  entry->codepoint = codepoint;
  entry->glyph_index = map->face->glyph->glyph_index;
  entry->x = map->atlas_draw_offset + 1;
  entry->bmp_w = map->face->glyph->bitmap.width;
  entry->bmp_h = map->face->glyph->bitmap.rows;
  entry->bmp_x = map->face->glyph->bitmap_left;
  entry->bmp_y = map->face->glyph->bitmap_top;
  entry->uvx = (float)entry->x / map->atlas_width;
  entry->uvy = (float)entry->bmp_h / map->atlas_height;
  entry->uvx_max = entry->uvx + ((double)entry->bmp_w / map->atlas_width);
  entry->uvy_max = 0;
  if (entry->uvx_max < entry->uvx) {
    fprintf(stderr, "ERROR: uvx_max lte uvx: %f difference\n", ((float)entry->bmp_w / map->atlas_width));
    return NULL;
  }
  if (entry->uvy_max > entry->uvy) {
    fprintf(stderr, "ERROR: uvy_max gte uvy\n");
    return NULL;
  }
  if (map->atlas_draw_offset + entry->bmp_w >= map->atlas_width) {
    fprintf(stderr, "TODO: Expand glyph atlas texture width-wise.");
    return NULL;
  }
  if (entry->bmp_h > map->atlas_height) {
    fprintf(stderr, "TODO: Expand glyph atlas height-wise.");
    return NULL;
  }
  glBindTexture(GL_TEXTURE_2D, map->atlas);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexSubImage2D(GL_TEXTURE_2D,
    0,
    entry->x,
    0,
    entry->bmp_w,
    entry->bmp_h,
    GL_RED,
    GL_UNSIGNED_BYTE,
    map->face->glyph->bitmap.buffer
  );
  glGenerateMipmap(GL_TEXTURE_2D);

  map->atlas_draw_offset += entry->bmp_w + 2;

  return entry;
}

Glyph *glyph_map_find_or_add(GlyphMap *map, uint32_t codepoint) {
  Glyph *entry = glyph_map_entry(*map, codepoint);
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

  char * filepath;

  // Hash map containing codepoint -> glyph rendering data.
  GlyphMap glyph_map;
  FT_Face ft_face;
  hb_face_t *hb_face;
  hb_font_t *hb_font;
} GLFace;

struct {
  GLColor bg;

  GLFWwindow *window;
  size_t width;
  size_t height;

  Renderer rend;

  FT_Library ft;

  GLfloat scale;

  GLFace face;
} g;


void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
  if (width < 0 || height < 0) return;
  g.width  = (size_t)width;
  g.height = (size_t)height;
  glViewport(0, 0, g.width, g.height);
}
// Returns GUI_MODKEY_MAX for any key that is not a modifier,
// otherwise returns the corresponding GUI_MODKEY_* enum.
static GUIModifierKey is_modifier(int key) {
  switch (key) {
  default: return GUI_MODKEY_MAX;
  case GLFW_KEY_LEFT_SUPER:    return GUI_MODKEY_LSUPER;
  case GLFW_KEY_RIGHT_SUPER:   return GUI_MODKEY_RSUPER;
  case GLFW_KEY_LEFT_CONTROL:  return GUI_MODKEY_LCTRL;
  case GLFW_KEY_RIGHT_CONTROL: return GUI_MODKEY_RCTRL;
  case GLFW_KEY_LEFT_ALT:      return GUI_MODKEY_LALT;
  case GLFW_KEY_RIGHT_ALT:     return GUI_MODKEY_RALT;
  case GLFW_KEY_LEFT_SHIFT:    return GUI_MODKEY_LSHIFT;
  case GLFW_KEY_RIGHT_SHIFT:   return GUI_MODKEY_RSHIFT;
  }
}
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  GUIModifierKey mod;
  if ((mod = is_modifier(key)) != GUI_MODKEY_MAX) {
    if (action == GLFW_PRESS)
      handle_modifier_dn(mod);
    else if (action == GLFW_RELEASE)
      handle_modifier_up(mod);
    return;
  }

  // GLFW is stupid and sends uppercase letters no matter what for some ungodly reason.
  if ((mods & GLFW_MOD_SHIFT) == 0 && isupper(key))
    key = tolower(key);

  # define max_size 32
  char string[max_size] = {0};
  if (action == GLFW_PRESS) {
    switch (key) {
    case GLFW_KEY_ENTER:
      strncpy(string, LITE_KEYSTRING_RETURN, max_size);
      break;
    case GLFW_KEY_BACKSPACE:
      strncpy(string, LITE_KEYSTRING_BACKSPACE, max_size);
      break;
    case GLFW_KEY_TAB:
      strncpy(string, LITE_KEYSTRING_TAB, max_size);
      break;
    case GLFW_KEY_CAPS_LOCK:
      strncpy(string, LITE_KEYSTRING_CAPSLOCK, max_size);
      break;
    case GLFW_KEY_ESCAPE:
      strncpy(string, LITE_KEYSTRING_ESCAPE, max_size);
      break;
    case GLFW_KEY_INSERT:
      strncpy(string, LITE_KEYSTRING_INSERT, max_size);
      break;
    case GLFW_KEY_DELETE:
      strncpy(string, LITE_KEYSTRING_DELETE, max_size);
      break;
    case GLFW_KEY_HOME:
      strncpy(string, LITE_KEYSTRING_HOME, max_size);
      break;
    case GLFW_KEY_END:
      strncpy(string, LITE_KEYSTRING_END, max_size);
      break;
    case GLFW_KEY_PAGE_UP:
      strncpy(string, LITE_KEYSTRING_PAGE_UP, max_size);
      break;
    case GLFW_KEY_PAGE_DOWN:
      strncpy(string, LITE_KEYSTRING_PAGE_DOWN, max_size);
      break;
    case GLFW_KEY_LEFT:
      strncpy(string, LITE_KEYSTRING_LEFT_ARROW, max_size);
      break;
    case GLFW_KEY_RIGHT:
      strncpy(string, LITE_KEYSTRING_RIGHT_ARROW, max_size);
      break;
    case GLFW_KEY_UP:
      strncpy(string, LITE_KEYSTRING_UP_ARROW, max_size);
      break;
    case GLFW_KEY_DOWN:
      strncpy(string, LITE_KEYSTRING_DOWN_ARROW, max_size);
      break;
    case GLFW_KEY_SCROLL_LOCK:
      strncpy(string, LITE_KEYSTRING_SCROLL_LOCK, max_size);
      break;
    case GLFW_KEY_PAUSE:
      strncpy(string, LITE_KEYSTRING_PAUSE, max_size);
      break;
    case GLFW_KEY_PRINT_SCREEN:
      strncpy(string, LITE_KEYSTRING_PRINT_SCREEN, max_size);
      break;
    case GLFW_KEY_NUM_LOCK:
      strncpy(string, LITE_KEYSTRING_NUMPAD_LOCK, max_size);
      break;
    case GLFW_KEY_F1:
      strncpy(string, LITE_KEYSTRING_F1, max_size);
      break;
    case GLFW_KEY_F2:
      strncpy(string, LITE_KEYSTRING_F2, max_size);
      break;
    case GLFW_KEY_F3:
      strncpy(string, LITE_KEYSTRING_F3, max_size);
      break;
    case GLFW_KEY_F4:
      strncpy(string, LITE_KEYSTRING_F4, max_size);
      break;
    case GLFW_KEY_F5:
      strncpy(string, LITE_KEYSTRING_F5, max_size);
      break;
    case GLFW_KEY_F6:
      strncpy(string, LITE_KEYSTRING_F6, max_size);
      break;
    case GLFW_KEY_F7:
      strncpy(string, LITE_KEYSTRING_F7, max_size);
      break;
    case GLFW_KEY_F8:
      strncpy(string, LITE_KEYSTRING_F8, max_size);
      break;
    case GLFW_KEY_F9:
      strncpy(string, LITE_KEYSTRING_F9, max_size);
      break;
    case GLFW_KEY_F10:
      strncpy(string, LITE_KEYSTRING_F10, max_size);
      break;
    case GLFW_KEY_F11:
      strncpy(string, LITE_KEYSTRING_F11, max_size);
      break;
    case GLFW_KEY_F12:
      strncpy(string, LITE_KEYSTRING_F12, max_size);
      break;
    case GLFW_KEY_F13:
      strncpy(string, LITE_KEYSTRING_F13, max_size);
      break;
    case GLFW_KEY_F14:
      strncpy(string, LITE_KEYSTRING_F14, max_size);
      break;
    case GLFW_KEY_F15:
      strncpy(string, LITE_KEYSTRING_F15, max_size);
      break;
    case GLFW_KEY_F16:
      strncpy(string, LITE_KEYSTRING_F16, max_size);
      break;
    case GLFW_KEY_F17:
      strncpy(string, LITE_KEYSTRING_F17, max_size);
      break;
    case GLFW_KEY_F18:
      strncpy(string, LITE_KEYSTRING_F18, max_size);
      break;
    case GLFW_KEY_F19:
      strncpy(string, LITE_KEYSTRING_F19, max_size);
      break;
    case GLFW_KEY_F20:
      strncpy(string, LITE_KEYSTRING_F20, max_size);
      break;
    case GLFW_KEY_F21:
      strncpy(string, LITE_KEYSTRING_F21, max_size);
      break;
    case GLFW_KEY_F22:
      strncpy(string, LITE_KEYSTRING_F22, max_size);
      break;
    case GLFW_KEY_F23:
      strncpy(string, LITE_KEYSTRING_F23, max_size);
      break;
    case GLFW_KEY_F24:
      strncpy(string, LITE_KEYSTRING_F24, max_size);
      break;
    case GLFW_KEY_KP_0:
      strncpy(string, LITE_KEYSTRING_NUMPAD_0, max_size);
      break;
    case GLFW_KEY_KP_1:
      strncpy(string, LITE_KEYSTRING_NUMPAD_1, max_size);
      break;
    case GLFW_KEY_KP_2:
      strncpy(string, LITE_KEYSTRING_NUMPAD_2, max_size);
      break;
    case GLFW_KEY_KP_3:
      strncpy(string, LITE_KEYSTRING_NUMPAD_3, max_size);
      break;
    case GLFW_KEY_KP_4:
      strncpy(string, LITE_KEYSTRING_NUMPAD_4, max_size);
      break;
    case GLFW_KEY_KP_5:
      strncpy(string, LITE_KEYSTRING_NUMPAD_5, max_size);
      break;
    case GLFW_KEY_KP_6:
      strncpy(string, LITE_KEYSTRING_NUMPAD_6, max_size);
      break;
    case GLFW_KEY_KP_7:
      strncpy(string, LITE_KEYSTRING_NUMPAD_7, max_size);
      break;
    case GLFW_KEY_KP_8:
      strncpy(string, LITE_KEYSTRING_NUMPAD_8, max_size);
      break;
    case GLFW_KEY_KP_9:
      strncpy(string, LITE_KEYSTRING_NUMPAD_0, max_size);
      break;
      // Unclear whether this means the decimal key (as in the radix)
      // or if it means the full stop key/dot '.'.
    case GLFW_KEY_KP_DECIMAL:
      strncpy(string, LITE_KEYSTRING_NUMPAD_DOT, max_size);
      break;
    case GLFW_KEY_KP_ENTER:
      strncpy(string, LITE_KEYSTRING_NUMPAD_RETURN, max_size);
      break;
    case GLFW_KEY_KP_ADD:
      strncpy(string, LITE_KEYSTRING_NUMPAD_PLUS, max_size);
      break;
    case GLFW_KEY_KP_SUBTRACT:
      strncpy(string, LITE_KEYSTRING_NUMPAD_MINUS, max_size);
      break;
    case GLFW_KEY_KP_MULTIPLY:
      strncpy(string, LITE_KEYSTRING_NUMPAD_MULTIPLY, max_size);
      break;
    case GLFW_KEY_KP_DIVIDE:
      strncpy(string, LITE_KEYSTRING_NUMPAD_DIVIDE, max_size);
      break;
    case GLFW_KEY_KP_EQUAL:
      strncpy(string, LITE_KEYSTRING_NUMPAD_EQUALS, max_size);
      break;
    default:
      memcpy(string, &key, sizeof(key));
      string[sizeof(key)] = '\0';
      break;
    }
    string[max_size - 1] = '\0';
    handle_keydown(string);
  }
# undef max_size
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
  int w;
  int h;
  glfwGetFramebufferSize(g.window, &w, &h);
  if (w < 0 || h < 0) {
    fprintf(stderr, "Could not get size of framebuffer\n");
    return CREATE_GUI_ERR;
  }
  g.width = (size_t)w;
  g.height = (size_t)h;
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
  const char *const facepath = "C:/Windows/Fonts/DejaVuSansMono.ttf";
#elif defined(__unix__)
  // TODO: Unix kind of sucks at a unified font interface, so we'll
  // have to use one packaged with LITE instead of defaulting to a
  // known system font...
  const char *const facepath = "/usr/share/fonts/DroidSansMono.ttf";
#endif
  g.face.filepath = strdup(facepath);
  if (!g.face.filepath) {
    fprintf(stderr, "Could not duplicate facepath\n");
    return CREATE_GUI_ERR;
  }
  ft_err = FT_New_Face(g.ft, facepath, 0, &g.face.ft_face);
  if (ft_err) {
    fprintf(stderr, "Could not initialize freetype face from font file at %s, sorry\n", facepath);
    return CREATE_GUI_ERR;
  }
  size_t font_height = GLYPH_ATLAS_HEIGHT;
  FT_Set_Pixel_Sizes(g.face.ft_face, 0, font_height);

  hb_blob_t *hb_blob = hb_blob_create_from_file(g.face.filepath);
  if (!hb_blob) {
    fprintf(stderr, "Could not create harfbuzz blob from font file\n");
    return CREATE_GUI_ERR;
  }
  g.face.hb_face = hb_face_create(hb_blob, 0);
  if (!g.face.hb_face) {
    fprintf(stderr, "Could not create harfbuzz face from font blob\n");
    return CREATE_GUI_ERR;
  }
  hb_blob_destroy(hb_blob);
  g.face.hb_font = hb_font_create(g.face.hb_face);
  if (!g.face.hb_font) {
    fprintf(stderr, "Could not create harfbuzz font from face\n");
    return CREATE_GUI_ERR;
  }
  hb_font_set_scale(g.face.hb_font, font_height * 64, font_height * 64);

  glyph_map_init(&g.face.glyph_map, g.face.ft_face);

  // TODO: Get rid of this. I *guess* it could be argued that rendering
  // the glyphs at a high resolution and then scaling them is a good
  // thing. In practice, it just makes it really hard to know when to
  // apply the scaling, as there are already a million god-damned
  // invisible unit conversions that make me want to bawl my eyes out
  // so yeah.
  g.scale = 1.0 / 19.6;

  return CREATE_GUI_OK;
}

void destroy_gui() {
  if (created) {
    fini_opengl();
    fini_glfw();

    if (g.face.hb_font) hb_font_destroy(g.face.hb_font);
    if (g.face.hb_face) hb_face_destroy(g.face.hb_face);
    if (g.face.ft_face) FT_Done_Face(g.face.ft_face);
    if (g.face.filepath) free(g.face.filepath);
    // TODO: g.face.glyph_map

    created = 0;
  }
}

int handle_events() {
  glfwPollEvents();
  return glfwWindowShouldClose(g.window);
}

/// `x` and `y` are in pixels, whereas the returned values are in NDC (-1 to 1, origin bottom left).
static vec2 pixel_to_screen_coordinates(size_t x, size_t y) {
  vec2 out = (vec2){
    out.x = (2 * ((float)x / g.width)) - 1.0f,
    out.y = (2 * ((float)y / g.height)) - 1.0f
  };
  if (out.x > 1.0f) out.x = 1.0f;
  if (out.y > 1.0f) out.y = 1.0f;
  if (out.x < -1.0f) out.x = -1.0f;
  if (out.y < -1.0f) out.y = -1.0f;
  return out;
}

/// This function has lost me my sanity, my sleep at nights, my non-
/// white hair, and a leg. I do not recommend *ever* attempting to do
/// *anything* like this.
static void draw_glyph(vec2 draw_position, vec4 color, uint32_t codepoint) {
  // TODO: Ensure draw_position_max is also oob.
  if (draw_position.x >= g.width || draw_position.y >= g.height) {
    //fprintf(stderr, "Draw position oob\n");
    return;
  }

  // glyph bitmap is upside down in glyph atlas. uvy is larger than
  // uvy_max in order to flip it right way up... This is mainly due
  // to FreeType using top left as (0, 0) origin, where OpenGL uses
  // bottom left as origin.

  // draw_position is the pixel position we should draw the glyph at.

  Glyph *glyph = glyph_map_find_or_add(&g.face.glyph_map, codepoint);
  float glyph_width = glyph->bmp_w;
  float glyph_height = glyph->bmp_h;
  vec2 uv = {glyph->uvx,glyph->uvy}; //> bottom left
  vec2 uvmax = {glyph->uvx_max,glyph->uvy_max}; //> top right

  //fprintf(stderr, "'%c':  w:%.2f  h:%.2f  x:%"PRId64"  y:%"PRId64"\n", codepoint, glyph_width, glyph_height, glyph->bmp_x, glyph->bmp_y);
  // FIXME: I think this may be wrong, as it causes characters to grow and shrink oddly...
  draw_position.x += g.scale * glyph->bmp_x;

  // bmp_y == top side bearing (distance from baseline to top of glyph) in unscaled bitmap pixels
  // glyph_height == vertical size of glyph in unscaled bitmap pixels
  // If we were drawing from the top left, we would just start drawing
  // at bmp_y, but because OpenGL is bottom left in origin, we have to
  // set the draw position to this number *subtract* the height of the
  // glyph we are going to draw, leaving us at the bottom left corner.
  // FIXME: I think something may be wrong with this calculation, or
  // something. While it does align glyphs like 'y', it causes a lot of
  // things to just be misaligned (like `most`).
  double diff_y_height = glyph->bmp_y - glyph_height;
  //fprintf(stderr, "%c: y-height=%f\n", codepoint, diff_y_height);
  draw_position.y += g.scale * diff_y_height;

  { // Truncate draw positions off left and bottom sides
    if (draw_position.x < 0) {
      if (draw_position.x + (g.scale * glyph_width) < 0) {
        //fprintf(stderr, "Draw position off left side, not drawing\n");
        return;
      }
      //fprintf(stderr, "Reducing glyph width\n");
      // Reduce glyph width
      glyph_width -= (-draw_position.x / g.scale);
      // Recalculate starting uv with new width
      uv.x += (-draw_position.x / g.scale) / GLYPH_ATLAS_WIDTH; //> FIXME: Use `map->atlas_width`; need `GlyphMap *map` parameter.
      draw_position.x = 0;
    }
    if (draw_position.y < 0) {
      if (draw_position.y + (g.scale * glyph_height) < 0) {
        //fprintf(stderr, "Draw position off top side, not drawing\n");
        return;
      }
      glyph_height -= (-draw_position.y / g.scale);
      uv.y -= (-draw_position.y / g.scale) / GLYPH_ATLAS_HEIGHT; //> FIXME: Use `map->atlas_height`; need `GlyphMap *map` parameter.
      draw_position.y = 0;
    }
  }

  // Draw position past right/top edge means that there isn't anything to actually draw.
  if (draw_position.x >= g.width || draw_position.y >= g.height) {
    //fprintf(stderr, "Draw position oob 2: (%f %f)\n", draw_position.x, draw_position.y);
    return;
  }

  vec2 draw_position_max = (vec2){
    .x = draw_position.x + (g.scale * glyph_width),
    .y = draw_position.y + (g.scale * glyph_height),
  };

  // If right and top bounds are less than the minimum left and bottom
  // bounds (0), then that means the glyph is off of the screen entirely.
  if (draw_position_max.x < 0 || draw_position_max.y < 0) return;

  { // Truncate draw positions off right and top sides
    if (draw_position_max.x > g.width) {
      float diff = draw_position_max.x - g.width;
      glyph_width -= diff / g.scale;
      // Use new glyph width to calculate maximum texture uv coordinate.
      // FIXME: Use `map->atlas_width`; need `GlyphMap *map` parameter.
      uvmax.x -= ((diff / g.scale) / GLYPH_ATLAS_WIDTH);
      // Update maximum drawing position to be within bounds.
      // NOTE: Not really needed since pixel_to_screen_coordinates truncates.
      draw_position_max.x = g.width;
    }
    if (draw_position_max.y > g.height) {
      float diff = draw_position_max.y - g.height;
      glyph_height -= diff / g.scale;
      // Use new glyph width to calculate maximum texture uv coordinate.
      // FIXME: Use `map->atlas_width`; need `GlyphMap *map` parameter.
      uvmax.y -= ((diff / g.scale) / GLYPH_ATLAS_HEIGHT);
      // Update maximum drawing position to be within bounds.
      // NOTE: Not really needed since pixel_to_screen_coordinates truncates.
      draw_position_max.y = g.height;
    }
  }

  vec2 screen_position = pixel_to_screen_coordinates(draw_position.x, draw_position.y);
  vec2 screen_position_max = pixel_to_screen_coordinates(draw_position_max.x, draw_position_max.y);

  const Vertex tl = (Vertex){
    .position = screen_position.x, screen_position_max.y,
    .uv = { uv.x, uvmax.y },
    .color = color
  };
  const Vertex tr = (Vertex){
    .position = screen_position_max.x, screen_position_max.y,
    .uv = { uvmax.x, uvmax.y },
    .color = color
  };
  const Vertex bl = (Vertex){
    .position = screen_position.x, screen_position.y,
    .uv = { uv.x, uv.y },
    .color = color
  };
  const Vertex br = (Vertex){
    .position = screen_position_max.x, screen_position.y,
    .uv = { uvmax.x, uv.y },
    .color = color
  };

  r_quad(&g.rend, tl, tr, bl, br);
}


#include <fribidi.h>

// TODO: LTR vs RTL base direction when reordering
/// Reorder input as UTF8 BIDI text into utf32 codepoints. Update length with amount of elements in output string.
/// Caller responsible for freeing returnvalue when non-null.
static uint32_t *bidi_reorder_utf8_to_utf32(const char *const input, size_t *length) {
  // Convert utf8 to utf32
  FriBidiChar *fri_input = malloc(sizeof(FriBidiChar) * *length);
  FriBidiStrIndex fri_length = fribidi_charset_to_unicode(
    FRIBIDI_CHAR_SET_UTF8, // character set to convert from
    input,                 // input string encoded in char_set
    *length,               // input string length
    fri_input              // output Unicode string
  );
  *length = fri_length;
  // Reorder utf32
  FriBidiChar *fri_output = malloc(sizeof(FriBidiChar) * fri_length);
  FriBidiParType base_type = FRIBIDI_TYPE_ON;
  fribidi_log2vis(
    fri_input,  //> Unicode input string
    fri_length, //> Length of input string
    &base_type, //> Input and output base direction (default)
    fri_output, //> Unicode output string
    NULL, //> map of positions in logical string to visual string
    NULL, //> map of positions in visual string to logical string
    NULL  //> even numbers indicate LTR characters, odd levels indicate RTL characters
  );
  free(fri_input);
  return fri_output;

  /*
  printf("fri output: ");
  for (size_t i = 0; i < fri_length; ++i) {
    printf("0x%x ", fri_output[i]);
  }
  printf("\n");
  */
}

static float line_height_in_pixels(FT_Face f) {
  return g.scale * (f->size->metrics.height / 64.0);
}

// TODO: LTR vs RTL
static hb_buffer_t *hb_xt_bf_create_utf32(const uint32_t *const utf32, const size_t length) {
  hb_buffer_t *buf = hb_buffer_create();
  hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
  hb_buffer_set_script(buf, HB_SCRIPT_UNKNOWN);
  hb_buffer_set_language(buf, hb_language_get_default());
  hb_buffer_add_utf32(buf, utf32, length, 0, length);
  if (!g.face.hb_font) {
    fprintf(stderr, "Can not shape harfbuzz buffer with NULL harfbuzz font\n");
    return NULL;
  }
  hb_shape(g.face.hb_font, buf, NULL, 0);
  return buf;
}

/// Return size of shaped harfbuzz buffer in screen pixels
static vec2 measure_shaped_hb_buffer(const size_t length, const uint32_t *const codepoints, hb_buffer_t *const buf) {
  unsigned int glyph_count = 0;
  hb_glyph_position_t *glyph_pos = hb_buffer_get_glyph_positions(buf, &glyph_count);
  vec2 pos = {0};
  GLfloat least_x = 0;
  GLfloat most_x = 0;
  GLfloat least_y = 0;
  GLfloat most_y = 0;
  for (unsigned int i = 0; i < glyph_count; ++i) {
    static const double divisor = 64.0;
    pos.x += g.scale * glyph_pos[i].x_advance / divisor;
    pos.y += g.scale * glyph_pos[i].y_advance / divisor;
    if (pos.x < least_x) least_x = pos.x;
    if (pos.x > most_x) most_x = pos.x;
    if (pos.y < least_y) least_y = pos.y;
    if (pos.y > most_y) most_y = pos.y;
    Glyph *glyph = glyph_map_find_or_add(&g.face.glyph_map, codepoints[i]);
    vec2 draw_pos = pos;
    draw_pos.x += g.scale * glyph_pos[i].x_offset / divisor;
    draw_pos.y += g.scale * glyph_pos[i].y_offset / divisor;
    if (draw_pos.x < least_x) least_x = draw_pos.x;
    if (draw_pos.x > most_x) most_x = draw_pos.x;
    if (draw_pos.y < least_y) least_y = draw_pos.y;
    if (draw_pos.y > most_y) most_y = draw_pos.y;
    vec2 draw_pos_max = (vec2){
      .x = draw_pos.x + (g.scale * glyph->bmp_w),
      .y = draw_pos.y + (g.scale * glyph->bmp_h),
    };
    if (draw_pos_max.x < least_x) least_x = draw_pos_max.x;
    if (draw_pos_max.x > most_x) most_x = draw_pos_max.x;
    if (draw_pos_max.y < least_y) least_y = draw_pos_max.y;
    if (draw_pos_max.y > most_y) most_y = draw_pos_max.y;
  }
  return (vec2){
    .x = most_x - least_x,
    .y = most_y - least_y,
  };
}

static void draw_shaped_hb_buffer(const size_t length, const uint32_t *const codepoints, hb_buffer_t *const buf, const vec2 starting_position, const vec4 fg_color, const vec4 bg_color) {
  unsigned int glyph_count = 0;
  hb_glyph_position_t *glyph_pos = hb_buffer_get_glyph_positions(buf, &glyph_count);
  const double divisor = 64.0;
  vec2 pos = starting_position;
  /*if (bg_color.w) {
    for (unsigned int i = 0; i < glyph_count && i < length; ++i) {
      vec2 draw_pos = pos;
      draw_pos.x += g.scale * glyph_pos[i].x_offset / divisor;
      draw_pos.y += g.scale * glyph_pos[i].y_offset / divisor;
      vec2 draw_cursor_advance = {
        g.scale * glyph_pos[i].x_advance / divisor,
        g.scale * glyph_pos[i].y_advance / divisor
      };
      // TODO: Pass draw_cursor_advance.y when vertical
      // TODO: Should just calculate full bounding box, then draw bg of that, rather than drawing background of each glyph, right?
      draw_codepoint_background(codepoints[i], draw_pos, bg_color, pos, draw_cursor_advance.x);
      pos.x += draw_cursor_advance.x;
      pos.y += draw_cursor_advance.y;
    }
  }*/
  if (fg_color.w) {
    pos = starting_position;
    for (unsigned int i = 0; i < glyph_count && i < length; ++i) {
      vec2 draw_pos = {
        pos.x + ((glyph_pos[i].x_offset / divisor) * g.scale),
        pos.y + ((glyph_pos[i].y_offset / divisor) * g.scale),
      };
      draw_glyph(draw_pos, fg_color, codepoints[i]);
      pos.x += (glyph_pos[i].x_advance / divisor) * g.scale;
      pos.y += (glyph_pos[i].y_advance / divisor) * g.scale;
    }
  }
}


static void render_utf32(const uint32_t *const input, size_t input_length, vec2 starting_position, vec4 fg_color, vec4 bg_color) {
  hb_buffer_t *hb_buf = hb_xt_bf_create_utf32(input, input_length);
  if (!hb_buf) {
    fprintf(stderr, "Harfbuzz could not create buffer from reordered utf32 codepoints\n");
    return;
  }
  draw_shaped_hb_buffer(input_length, input, hb_buf, starting_position, fg_color, bg_color);
  hb_buffer_destroy(hb_buf);
}

static void render_utf8(const char *const input, size_t input_length, vec2 starting_position, vec4 fg_color, vec4 bg_color) {
  uint32_t *utf32 = bidi_reorder_utf8_to_utf32(input, &input_length);
  if (!utf32) {
    fprintf(stderr, "BIDI could not reorder utf8 to utf32, sorry\n");
    return;
  }
  render_utf32(utf32, input_length, starting_position, fg_color, bg_color);
  free(utf32);
}

static void render_lines_utf8(const char *const input, size_t input_length, vec2 starting_position, vec4 fg_color, vec4 bg_color) {
  // Get to end of line.
  const char *string = input;
  size_t offset = 0;
  size_t rendered_offset = 0;
  size_t last_newline_offset = -1;
  vec2 destination = starting_position;
  char cr_skip = 0;
  for (;;) {

    char c = *string;
    if (c == '\r' && *(string + 1) == '\n') {
      cr_skip = 1;
    } else if (c == '\n') {
      size_t start_of_line_offset = last_newline_offset + 1; // TODO: + horizontal_offset

      // Current line hidden entirely by horizontal scrolling
      if (start_of_line_offset > offset) break;

      // Calculate amount of bytes within the current line.
      size_t bytes_to_render = offset - last_newline_offset;
      // Exclude newline (LF or CRLF)
      bytes_to_render -= 1 + cr_skip;
      if (input[start_of_line_offset] != '\0') {
        // Render entire line using defaults.
        render_utf8(input + start_of_line_offset, bytes_to_render, destination, fg_color, bg_color);
      }

      cr_skip = 0;
      destination.y -= line_height_in_pixels(g.face.ft_face); // TODO: Better face selection
      last_newline_offset = offset;
      // TODO: Use bounding rectangle parameter to properly calculate out-of-bounds.
      // Stop if drawing offscreen entirely.
      if (destination.y <= 0 - line_height_in_pixels(g.face.ft_face)) break;
    }
    string++;
    offset++;
    if (offset >= input_length) break;
  }
  // Draw from last_newline_offset to offset (handle bytes at end of string with no newline or null terminator)
  if (last_newline_offset != offset) {
    size_t start_of_line_offset = last_newline_offset + 1; // TODO: + horizontal_offset
    // Hidden entirely by horizontal scrolling
    if (start_of_line_offset > offset) return;
    // Calculate amount of bytes that have yet to be drawn.
    size_t bytes_to_render = offset - last_newline_offset;
    // Exclude newline (LF or CRLF)
    bytes_to_render -= 1 + cr_skip;
    if (input[start_of_line_offset] != '\0') {
      // Render entire line using defaults.
      render_utf8(input + start_of_line_offset, bytes_to_render, destination, fg_color, bg_color);
    }
  }
}

void draw_gui(GUIContext *ctx) {
  if (!created || !ctx) return;
  if (ctx->title) {
    glfwSetWindowTitle(g.window, ctx->title);
    ctx->title = NULL;
  }

  glClearColor(g.bg.r, g.bg.g, g.bg.b, g.bg.a);
  glClear(GL_COLOR_BUFFER_BIT);

  glBindVertexArray(g.rend.vao);
  glUseProgram(g.rend.shader);

  r_clear(&g.rend);

  size_t footline_line_count = count_lines(ctx->footline.string);
  size_t footline_height = line_height_in_pixels(g.face.ft_face) * footline_line_count;

  size_t contents_height = g.height - footline_height;

  // TODO: Use above data about layout to feed proper bounds to rendering functions...

  vec4 fg_color = {1.0, 1.0, 1.0, 1.0};
  vec4 bg_color = {0.0, 0.0, 0.0, 0.0};
  GUIWindow *window = ctx->windows;
  while (window) {
    const char *utf8 = window->contents.string;
    vec2 draw_position = {
      (window->posx / 100.0) * g.width,
      ((1 + g.height) - ((window->posy / 100.0) * g.height)) - line_height_in_pixels(g.face.ft_face),
    };
    render_lines_utf8(utf8, strlen(utf8), draw_position, fg_color, bg_color);
    window = window->next;
  }

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
  if (!size) return 1;
  if (size > GLYPH_ATLAS_HEIGHT) {
    fprintf(stderr, "TODO: Can not set size to %zu as it is greater than glyph atlas height %u; expand glyph atlas height.\n", size, GLYPH_ATLAS_HEIGHT);
    return 1;
  }
  // TODO: This sucks. Should create new font from current one, set new
  // size, allocate new glyphmap, etc, then use that font. Save old one
  // in case we need to switch back to it.
  FT_Error ft_err = FT_Set_Pixel_Sizes(g.face.ft_face, 0, size);
  if (ft_err) {
    fprintf(stderr, "[GFX]: An error occured when trying to set pixel size of face using FT_Set_Pixel_Sizes");
    return 1;
  }
  hb_font_set_scale(g.face.hb_font, size * 64, size * 64);

  g.face.glyph_map.glyph_count = 0;
  memset(g.face.glyph_map.glyphs, 0, sizeof(*g.face.glyph_map.glyphs) * g.face.glyph_map.glyph_capacity);

  return 0;
}

void window_size(size_t *width, size_t *height) {
  if (!width || !height) return;
  *width = g.width;
  *height = g.height;
}

void window_size_row_col(size_t *rows, size_t *cols) {
  if (!rows || !cols) return;
  uint32_t M = 'M';
  hb_buffer_t *hb_buf = hb_xt_bf_create_utf32(&M, 1);
  if (!hb_buf) {
    fprintf(stderr, "Harfbuzz could not create buffer from utf32 codepoints to get size of 'M'\n");
    return;
  }
  vec2 emsize = measure_shaped_hb_buffer(1, &M, hb_buf);
  // Prevent divide by zero
  if (emsize.x == 0 || emsize.y == 0) return;
  hb_buffer_destroy(hb_buf);
  window_size(cols, rows);
  *cols /= emsize.x;
  *rows /= emsize.y;
}

void change_window_size(size_t width, size_t height) {
  g.width = width;
  g.height = height;

  // TODO: glfwSetFramebufferSize is in screen coordinates...
  // Need to get pixel coordinates and screen coordinates, compare to
  // get ratio (separately for each coordinate), then multiply input by
  // that in order to set proper size...
  int fb_width;
  int fb_height;
  glfwGetFramebufferSize(g.window, &fb_width, &fb_height);
  // Prevent divide by zero
  if (!fb_width || !fb_height) return;

  int win_width;
  int win_height;
  glfwGetWindowSize(g.window, &win_width, &win_height);

  // for every 1 fb_width there are win_to_fb_w win_widths...
  double win_to_fb_w = (double)win_width / fb_width;
  double win_to_fb_h = (double)win_height / fb_height;

  glfwSetWindowSize(g.window, g.width * win_to_fb_w, g.height * win_to_fb_h);

  glViewport(0, 0, g.width, g.height);
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

