cmake_minimum_required(VERSION 3.20)
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

add_executable(
  LITE
  src/builtins.c
  src/error.c
  src/environment.c
  src/evaluation.c
  src/main.c
  src/repl.c
  src/parser.c
  src/types.c
)
target_include_directories(
  LITE
  PRIVATE src/
)
