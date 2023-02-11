#include <shade.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

bool shader_compile(const char *const path, GLenum type, GLuint *out) {
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

  // Compile shader from buffer
  *out = glCreateShader(type);
  glShaderSource(*out, 1, (const char *const *)&buffer, NULL);
  glCompileShader(*out);
  int success;
  glGetShaderiv(*out, GL_COMPILE_STATUS, &success);
  if (!success) {
    char err_log[512];
    glGetShaderInfoLog(*out, 512, NULL, err_log);
    fprintf(stderr, "Failed to compile GLSL shader at %s\n  %s\n", path, err_log);
    return false;
  }
  free(buffer);
  return true;
}

/** Create a new program, attaching and linking given shaders.
  *
  * NOTE: Deletes both VERT and FRAG shaders given before returning.
  */
bool shader_program(GLuint *program, GLuint vert, GLuint frag) {
  *program = glCreateProgram();
  glAttachShader(*program, vert);
  glAttachShader(*program, frag);
  glLinkProgram(*program);
  int success;
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
