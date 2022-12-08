#include <utility.h>

#include <assert.h>
#include <environment.h>
#include <stdlib.h>
#include <string.h>
#include <types.h>

#ifdef LITE_GFX
#include <gui.h>
#endif

void exit_safe(int code) {
# ifdef LITE_GFX
  destroy_gui();
# endif
  int debug_memory = env_non_nil(*genv(), make_sym("DEBUG/MEMORY"));
  // Garbage collection with no marking means free everything.
  gcol();
  if (debug_memory) {
    print_gcol_data();
  }
  exit(code);
  assert(0 && "UNREACHABLE: exit_lite() must never return.");
}

char *allocate_string(const char *const string) {
  if (!string) { return NULL; }
  size_t string_length = strlen(string);
  if (!string_length) { return NULL; }
  char *out = malloc(string_length + 1);
  if (!out) { return NULL; }
  memcpy(out, string, string_length);
  out[string_length] = '\0';
  return out;
}

char *string_join(const char *const a, const char *const b) {
  if (!a || !b) { return NULL; }
  size_t a_len = strlen(a);
  size_t b_len = strlen(b);
  size_t new_len = a_len + b_len;
  char *out = malloc(new_len + 1);
  if (!out) { return NULL; }
  memcpy(out, a, a_len);
  memcpy(out + a_len, b, b_len);
  out[new_len] = '\0';
  return out;
}

char *string_trijoin(const char *const a, const char *const b, const char *const c) {
  if (!a || !b || !c) { return NULL; }
  char *tmp = string_join(a, b);
  char *out = string_join(tmp, c);
  free(tmp);
  return out;
}
