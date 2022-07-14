# Load CMake modules from current directory.
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}")

set(SDL2_BUILDING_LIBRARY TRUE)
find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)

add_library(
  SDL_LIB STATIC
  sdl.c
)
target_include_directories(
  SDL_LIB
  PRIVATE
  .
  ..
  ${CMAKE_SOURCE_DIR}/src
  ${SDL2_DIR}/include
  ${SDL2_INCLUDE_DIRS}
  ${SDL2_TTF_INCLUDE_DIRS}
)
target_link_libraries(
  SDL_LIB
  PRIVATE
  ${SDL2_LIBRARIES}
  ${SDL2_TTF_LIBRARIES}
)

# Copy SDL2 dynamically linked libraries (.dll)
if (CMAKE_HOST_WIN32)
  add_custom_command(
    TARGET SDL_LIB POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_LIST_DIR}/SDL2.dll"
    ${EXECUTABLE_OUTPUT_PATH})
  add_custom_command(
    TARGET SDL_LIB POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_LIST_DIR}/SDL2_ttf.dll"
    ${EXECUTABLE_OUTPUT_PATH})
endif ()

set(LITE_GFX_LIBRARY SDL_LIB CACHE INTERNAL "" FORCE)
