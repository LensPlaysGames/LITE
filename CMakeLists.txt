cmake_minimum_required(VERSION 3.14)
set(LITE_VERSION 0.0.1)
set(LITE_LANGUAGES C)

# Use `ccache` if it is installed in system's PATH.
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
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
  OFF
)

if (${LITE_GFX})
  add_subdirectory(gfx)
endif ()

add_executable(
  LITE
  $<$<BOOL:${LITE_GFX}>:src/api.c>
  src/builtins.c
  src/buffer.c
  src/error.c
  src/environment.c
  src/evaluation.c
  src/file_io.c
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
    -Wall
    -Wextra
    -pedantic
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
