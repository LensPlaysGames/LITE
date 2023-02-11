#ifndef SHADE_H
#define SHADE_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <stdbool.h>

bool shader_compile(const char *const path, GLenum type, GLuint *out);
bool shader_program(GLuint *program, GLuint vert, GLuint frag);

#endif
