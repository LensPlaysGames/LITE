#include <utility.h>

#include <assert.h>
#include <environment.h>
#include <stdlib.h>
#include <string.h>
#include <types.h>

#ifdef LITE_GFX
#include <gui.h>
#endif

void exit_lite(int code) {
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

char *allocate_string(const char *string) {
  if (!string) { return NULL; }
  size_t string_length = strlen(string);
  if (!string_length) { return NULL; }
  char *out = malloc(string_length + 1);
  if (!out) { return NULL; }
  memcpy(out, string, string_length);
  out[string_length] = '\0';
  return out;
}
