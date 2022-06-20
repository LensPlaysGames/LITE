# The implementation is expected to provide all
# functions defined in `gui.h`, as well as set
# `LITE_GFX_LIBRARY` to the CMake library it produces.

set(
  LITE_GFX_IMPLEMENTATION_SUBDIR
  "sdl2"
  CACHE
  PATH
  "The path to the subdirectory that contains the CMakeLists.txt
for the actual implementation of LITE_GFX."
)

add_subdirectory(${LITE_GFX_IMPLEMENTATION_SUBDIR})
