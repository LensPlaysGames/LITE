#ifndef SHADE_H
#define SHADE_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <stdbool.h>

/** Compile a shader from the given source
  *
  * NOTE: given path may be NULL; it is used only for diagnostic purposes.
  */
bool shader_compile_src(const char *const path, const char *const source, GLenum type, GLuint *out);

bool shader_compile(const char *const path, GLenum type, GLuint *out);

/** Create a new program, attaching and linking given shaders.
  *
  * NOTE: DELETES any shaders given.
  */
bool shader_program(GLuint *program, GLuint vert, GLuint frag);

#endif
