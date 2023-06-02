#include <shade.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

bool shader_compile_src(const char *const path, const char *const source, GLenum type, GLuint *out) {
  // Compile shader from buffer
  *out = glCreateShader(type);
  glShaderSource(*out, 1, (const char *const *)&source, NULL);
  glCompileShader(*out);
  int success;
  glGetShaderiv(*out, GL_COMPILE_STATUS, &success);
  if (!success) {
    char err_log[512];
    glGetShaderInfoLog(*out, 512, NULL, err_log);
    fprintf(stderr, "Failed to compile GLSL shader%s%s\n  %s\n", path ? " at " : "", path ? path : "", err_log);
    return false;
  }
  return true;
}

bool shader_compile(const char *const path, GLenum type, GLuint *out) {
  if (!path) {
    fprintf(stderr, "Refusing NULL path in shader_compile()\n");
    return false;
  }

  FILE *f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "Could not open shader at %s\n", path);
    return false;
  }

  // Get size in bytes
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);

  // Read file into buffer
  char *buffer = malloc(size + 1);
  fread(buffer, 1, size, f);
  buffer[size] = '\0';
  fclose(f);

  bool status = shader_compile_src(path, buffer, type, out);

  free(buffer);
  return status;
}

bool shader_program(GLuint *program, GLuint vert, GLuint frag) {
  *program = glCreateProgram();
  glAttachShader(*program, vert);
  glAttachShader(*program, frag);
  glLinkProgram(*program);
  GLint success;
  glGetProgramiv(*program, GL_LINK_STATUS, &success);
  if(!success) {
    char err_log[512];
    glGetProgramInfoLog(*program, 512, NULL, err_log);
    fprintf(stderr, "Failed to link shader program\n  %s\n", err_log);
    return false;
  }
  glDeleteShader(vert);
  glDeleteShader(frag);
  return true;
}
