# Dependencies:
# |-- cglm
# |-- freetype
# |-- glad
# |-- glfw
# |-- harfbuzz
# `-- OpenGL 3.3 or greater
# Meson (python build system)

# If trouble with submodules, or you just want to do it manually:
#   $ cd libs
#   $ git clone https://github.com/glfw/glfw
#   $ git clone https://github.com/recp/cglm
#   $ git clone https://github.com/freetype/freetype

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
set(FT_DISABLE_ZLIB TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI TRUE CACHE BOOL "" FORCE)
add_subdirectory(libs/freetype)

add_library(GL_LIB STATIC src/main.c src/shade.c)
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
)
target_link_libraries(
  GL_LIB
  PRIVATE
  OpenGL::GL
  glad
  glfw
  cglm
  freetype
)

set(HARFBUZZ_BUILD_SRC_PATH ${CMAKE_CURRENT_BINARY_DIR}/harfbuzz/src/ext_harfbuzz-build/src CACHE STRING "" FORCE)
if(WIN32)
  if (MSVC)
    set(HARFBUZZ_DYLIB_PATH ${HARFBUZZ_BUILD_SRC_PATH}/harfbuzz.dll CACHE STRING "" FORCE)
    set(HARFBUZZ_DYLIB_LINK_PATH ${HARFBUZZ_BUILD_SRC_PATH}/harfbuzz.lib CACHE STRING "" FORCE)
  else()
    set(HARFBUZZ_DYLIB_PATH ${HARFBUZZ_BUILD_SRC_PATH}/libharfbuzz-0.dll CACHE STRING "" FORCE)
    set(HARFBUZZ_DYLIB_LINK_PATH ${HARFBUZZ_BUILD_SRC_PATH}/libharfbuzz.dll.a CACHE STRING "" FORCE)
  endif()
elseif(APPLE)
  set(HARFBUZZ_DYLIB_PATH ${HARFBUZZ_BUILD_SRC_PATH}/libharfbuzz-0.dylib CACHE STRING "" FORCE)
else()
  set(HARFBUZZ_DYLIB_PATH ${HARFBUZZ_BUILD_SRC_PATH}/libharfbuzz-0.so CACHE STRING "" FORCE)
endif()

include(ExternalProject)
ExternalProject_Add(
  ext_harfbuzz
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/harfbuzz
  GIT_REPOSITORY https://github.com/harfbuzz/harfbuzz.git
  GIT_TAG main # works with 7.0.0
  CONFIGURE_COMMAND meson setup . ../ext_harfbuzz -Dfreetype=disabled -Dtests=disabled -Ddocs=disabled -Dintrospection=disabled
  USES_TERMINAL_CONFIGURE true
  BUILD_COMMAND meson compile
  BUILD_BYPRODUCTS ${HARFBUZZ_DYLIB_PATH} ${HARFBUZZ_DYLIB_LINK_PATH}
  USES_TERMINAL_BUILD true
  INSTALL_COMMAND ${CMAKE_COMMAND} -E copy ${HARFBUZZ_DYLIB_PATH} ${EXECUTABLE_OUTPUT_PATH}
  TEST_COMMAND ""
)

add_library(harfbuzz SHARED IMPORTED)
add_dependencies(harfbuzz ext_harfbuzz)
if (EXISTS ${CMAKE_CURRENT_BINARY_DIR}/harfbuzz/src/ext_harfbuzz/src)
  target_include_directories(
    harfbuzz
    INTERFACE
    ${CMAKE_CURRENT_BINARY_DIR}/harfbuzz/src/ext_harfbuzz/src
  )
endif()
if(WIN32)
  set_target_properties(
    harfbuzz PROPERTIES
    IMPORTED_IMPLIB ${HARFBUZZ_DYLIB_LINK_PATH}
    IMPORTED_LOCATION ${HARFBUZZ_DYLIB_PATH}
  )
else()
  set_target_properties(
    harfbuzz PROPERTIES
    IMPORTED_LOCATION ${HARFBUZZ_DYLIB_PATH}
  )
endif()
target_link_libraries(GL_LIB PUBLIC harfbuzz)

# GNU FRIBIDI (Logical to Visual reordering for bidirectional text display)
set(FRIBIDI_BUILD_LIB_PATH ${CMAKE_CURRENT_BINARY_DIR}/fribidi/src/ext_fribidi-build/lib)
if(WIN32)
  if (MSVC)
    set(FRIBIDI_DYLIB_PATH ${FRIBIDI_BUILD_LIB_PATH}/fribidi-0.dll CACHE STRING "" FORCE)
    set(FRIBIDI_DYLIB_LINK_PATH ${FRIBIDI_BUILD_LIB_PATH}/fribidi.lib CACHE STRING "" FORCE)
  else()
    set(FRIBIDI_DYLIB_PATH ${FRIBIDI_BUILD_LIB_PATH}/libfribidi-0.dll CACHE STRING "" FORCE)
    set(FRIBIDI_DYLIB_LINK_PATH ${FRIBIDI_BUILD_LIB_PATH}/libfribidi.dll.a  CACHE STRING "" FORCE)
  endif()
elseif(APPLE)
  set(FRIBIDI_DYLIB_PATH ${FRIBIDI_BUILD_LIB_PATH}/libfribidi-0.dylib CACHE STRING "" FORCE)
else()
  set(FRIBIDI_DYLIB_PATH ${FRIBIDI_BUILD_LIB_PATH}/libfribidi-0.so CACHE STRING "" FORCE)
endif()

ExternalProject_Add(
  ext_fribidi
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/fribidi
  GIT_REPOSITORY https://github.com/fribidi/fribidi.git
  CONFIGURE_COMMAND meson setup . ../ext_fribidi -Dbin=false -Ddocs=false -Dtests=false
  USES_TERMINAL_CONFIGURE true
  BUILD_COMMAND meson compile
  BUILD_BYPRODUCTS ${FRIBIDI_DYLIB_PATH} ${FRIBIDI_DYLIB_LINK_PATH}
  USES_TERMINAL_BUILD true
  INSTALL_COMMAND ${CMAKE_COMMAND} -E copy ${FRIBIDI_DYLIB_PATH} ${EXECUTABLE_OUTPUT_PATH}
  TEST_COMMAND ""
)

add_library(fribidi SHARED IMPORTED)
add_dependencies(fribidi ext_fribidi)
if (EXISTS ${CMAKE_CURRENT_BINARY_DIR}/fribidi/src/ext_fribidi/lib)
  target_include_directories(
    fribidi
    INTERFACE
    ${CMAKE_CURRENT_BINARY_DIR}/fribidi/src/ext_fribidi/lib
  )
endif()
if (EXISTS ${CMAKE_CURRENT_BINARY_DIR}/fribidi/src/ext_fribidi-build/gen.tab)
  target_include_directories(
    fribidi
    INTERFACE
      ${CMAKE_CURRENT_BINARY_DIR}/fribidi/src/ext_fribidi-build/gen.tab
  )
endif()
if (EXISTS ${CMAKE_CURRENT_BINARY_DIR}/fribidi/src/ext_fribidi-build/lib)
  target_include_directories(
    fribidi
    INTERFACE
    ${CMAKE_CURRENT_BINARY_DIR}/fribidi/src/ext_fribidi-build/lib
)
endif()
if(WIN32)
  set_target_properties(
    fribidi PROPERTIES
    IMPORTED_IMPLIB ${FRIBIDI_DYLIB_LINK_PATH}
    IMPORTED_LOCATION ${FRIBIDI_DYLIB_PATH}
  )
else()
  set_target_properties(
    fribidi PROPERTIES
    IMPORTED_LOCATION ${FRIBIDI_DYLIB_PATH}
  )
endif()
target_link_libraries(GL_LIB PUBLIC fribidi)

set(LITE_GFX_LIBRARY GL_LIB CACHE INTERNAL "" FORCE)
