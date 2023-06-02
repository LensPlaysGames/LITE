cmake_minimum_required(VERSION 3.14)
set(LITE_VERSION 0.0.1)
set(LITE_LANGUAGES C)

# Use `ccache` if it is installed in system's PATH.
find_program(CCACHE_PROGRAM ccache)
# Only use ccache for debug modes, that way release modes will always
# rebuild. This is helpful because ccache can cache a debug build and
# use it in release builds, which is not ideal at all.
if(CCACHE_PROGRAM AND CMAKE_BUILD_TYPE STREQUAL "Debug")
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

# Export compilation database in JSON format.
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

# Output executable files to `/bin` directory.
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin)


project(LITE VERSION ${LITE_VERSION} LANGUAGES ${LITE_LANGUAGES})

option(
  LITE_GFX
  "Should LITE be built with graphical abilities (GUI)?"
  ON
)

option(
  TREE_SITTER
  "Should LITE be built with libtreesitter?"
  ON
)

# TODO I remember hearing /something/ about not needing braces here?
if (${LITE_GFX})
  add_subdirectory(gfx)
endif()

add_executable(
  LITE
  $<$<BOOL:${LITE_GFX}>:src/api.c>
  src/builtins.c
  src/buffer.c
  src/error.c
  src/environment.c
  src/evaluation.c
  src/file_io.c
  $<$<BOOL:${LITE_GFX}>:src/gfx.c>
  src/main.c
  src/repl.c
  src/rope.c
  src/parser.c
  src/types.c
  src/utility.c
)
target_compile_definitions(
  LITE
  PRIVATE
  $<$<BOOL:${LITE_GFX}>:LITE_GFX>
  LITE_DBG # This enables the debug/... variable interaction.
)
target_include_directories(
  LITE
  PRIVATE
  src/
)
if (LITE_GFX)
  target_include_directories(
    LITE
    PRIVATE
    gfx/
  )
  target_link_libraries(
    LITE
    PRIVATE
    ${LITE_GFX_LIBRARY}
  )
endif()

if (NOT MSVC)
  target_compile_options(
    LITE
    PRIVATE
    -Wall -Wextra
    -Wshadow -Wconversion

    -Werror=return-type -Werror=implicit-function-declaration
    -Werror=implicit-int -Werror=pointer-arith
    -Werror=incompatible-pointer-types

    -Wno-unused-parameter -Wno-unused-function
    -Wno-format-zero-length -Wno-comment
    -Wno-cast-function-type

    -fdiagnostics-color=always
  )
endif()

if (${TREE_SITTER})
  if (NOT TS_DIR)
    message(FATAL_ERROR "[31mTS_DIR *must* be set to the filepath to the tree sitter repository[m
https://github.com/tree-sitter/tree-sitter.git")
  endif()

  # Statically build libtree-sitter into the executable
  target_sources(
    LITE
    PRIVATE
    ${TS_DIR}/lib/src/lib.c
  )
  target_include_directories(
    LITE
    PRIVATE
    ${TS_DIR}/lib/src
    ${TS_DIR}/lib/include
  )
  target_compile_definitions(
    LITE
    PRIVATE
    TREE_SITTER
  )
endif()


# Doxygen Documentation Target
find_program(DOXYGEN_PROGRAM doxygen)
if(DOXYGEN_PROGRAM)
  add_custom_target(
    docs
    COMMAND doxygen Doxyfile
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/docs
  )
endif()

if (WIN32)
  file(TO_CMAKE_PATH $ENV{APPDATA} appdata)
  install(
    TARGETS LITE
    RUNTIME DESTINATION "${appdata}/lite"
  )
  install(
    DIRECTORY gfx/fonts
    DESTINATION "${appdata}/lite/gfx"
  )
  install(
    DIRECTORY lisp
    DESTINATION "${appdata}/lite"
  )
else ()
  install(
    TARGETS LITE
    RUNTIME DESTINATION "$ENV{HOME}/lite"
  )
  install(
    DIRECTORY gfx/fonts
    DESTINATION "$ENV{HOME}/lite/gfx"
  )
  install(
    DIRECTORY lisp
    DESTINATION "$ENV{HOME}/lite"
  )
endif()
