#include <utility.h>

#include <assert.h>
#include <environment.h>
#include <stdlib.h>
#include <types.h>

#ifdef LITE_GFX
#include <gui.h>
#endif

void exit_lite(int code) {
# ifdef LITE_GFX
  destroy_gui();
# endif
  int debug_memory = env_non_nil(genv(), make_sym("DEBUG/MEMORY"));
  // Garbage collection with no marking means free everything.
  gcol();
  // Buffers aren't garbage collected, for now. They build up as the
  // program runs and are freed all at once right at the end.
  free_buffer_table();
  if (debug_memory) {
    print_gcol_data();
  }
  exit(code);
  assert(0 && "UNREACHABLE: exit_lite() must never return.");
}
