# Dependencies:
# |-- cglm
# |-- freetype
# |-- glad
# |-- glfw
# |-- harfbuzz
# `-- OpenGL 3.3 or greater

# If trouble with submodules, or you just want to do it manually:
#   $ cd libs
#   $ git clone https://github.com/glfw/glfw
#   $ git clone https://github.com/recp/cglm
#   $ git clone https://github.com/freetype/freetype
#   $ git clone https://github.com/harfbuzz/harfbuzz

# GLAD
add_library(
  glad
  libs/glad/src/glad.c
)
target_include_directories(
  glad
  PUBLIC
  libs/glad/include
)

# GLFW
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(libs/glfw)

# CGLM
set(CGLM_STATIC ON CACHE BOOL "" FORCE)
add_subdirectory(libs/cglm)

# OPENGL
find_package(OpenGL REQUIRED)

# FREETYPE (Font handling, Codepoint -> Bitmap, basic layout)
add_subdirectory(libs/freetype)

# HARFBUZZ (Advanced layout, codepoint -> font glyph index)
set(HB_HAVE_FREETYPE on)
add_subdirectory(libs/harfbuzz)
target_compile_options(harfbuzz PRIVATE -w)
if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
  target_compile_options(harfbuzz PRIVATE -Wno-error=unused-variable)
endif()


add_library(GL_LIB STATIC src/main.c src/shade.c)
target_compile_definitions(GL_LIB PUBLIC HB_TINY)
target_include_directories(
  GL_LIB
  PRIVATE
  src
  .
  ..
  ${CMAKE_SOURCE_DIR}/src
  libs/glad/include
  libs/glfw/include
  libs/cglm/include
  libs/freetype/include
  libs/harfbuzz/src
)
target_link_libraries(
  GL_LIB
  PRIVATE
  OpenGL::GL
  glad
  glfw
  cglm
  freetype
  harfbuzz
)

if (BIDI)
  # GNU FRIBIDI (Logical to Visual reordering for bidirectional text display)
  include(ExternalProject)
  ExternalProject_Add(
    ext_fribidi
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/fribidi
    GIT_REPOSITORY https://github.com/fribidi/fribidi.git
    CONFIGURE_COMMAND meson setup . ../ext_fribidi -Dbin=false -Ddocs=false -Dtests=false
    BUILD_COMMAND meson compile
    INSTALL_COMMAND ""
    TEST_COMMAND ""
  )

  add_library(fribidi SHARED IMPORTED)
  target_include_directories(
    fribidi
    INTERFACE
    ${CMAKE_BINARY_DIR}/fribidi/src/ext_fribidi/lib
    ${CMAKE_BINARY_DIR}/fribidi/src/ext_fribidi-build/lib
    ${CMAKE_BINARY_DIR}/fribidi/src/ext_fribidi-build/gen.tab
  )
  if(WIN32)
    set_target_properties(
      fribidi PROPERTIES
      IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/fribidi/src/ext_fribidi-build/lib/libfribidi-0.dll.a
      IMPORTED_IMPLIB ${CMAKE_BINARY_DIR}/fribidi/src/ext_fribidi-build/lib/libfribidi-0.dll
    )
  elseif(APPLE)
    set_target_properties(
      fribidi PROPERTIES
      IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/fribidi/src/ext_fribidi-build/lib/libfribidi-0.dylib
    )
  else()
    set_target_properties(
      fribidi PROPERTIES
      IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/fribidi/src/ext_fribidi-build/lib/libfribidi-0.so
    )
  endif()

  target_compile_definitions(GL_LIB PUBLIC BIDI)
  target_link_libraries(GL_LIB fribidi)
endif()


set(LITE_GFX_LIBRARY GL_LIB CACHE INTERNAL "" FORCE)
