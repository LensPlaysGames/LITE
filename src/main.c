#include <buffer.h>
#include <ctype.h>
#include <environment.h>
#include <evaluation.h>
#include <error.h>
#include <file_io.h>
#include <stdbool.h>
#include <stdio.h>
#include <parser.h>
#include <repl.h>
#include <rope.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <types.h>
#include <utility.h>

#ifdef LITE_GFX
#  include <api.h>
#endif

int main(int argc, char **argv) {
  printf("|> LITE will guide the way through the darkness.\n");
  // Treat every given argument as a file to load, for now.
  if (argc > 1) {
    Atom result = nil;
    Error err = ok;
    for (int i = 1; i < argc; ++i) {
      err = evaluate_file(*genv(), argv[i], &result);
      if (err.type) {
        printf("%s: ", argv[i]);
        print_error(err);
        err = ok;
      }
    }
  }

  Atom initial_buffer = make_buffer
    (env_create(nil), allocate_string("LITE_SHINES_UPON_US.txt"));
  if (nilp(initial_buffer)) {
    return 1;
  }
  env_set(*genv(), make_sym("CURRENT-BUFFER"), initial_buffer);

  Atom popup_buffer = make_buffer
    (env_create(nil), allocate_string(".popup"));
  if (nilp(popup_buffer)) {
    return 1;
  }
  env_set(*genv(), make_sym("POPUP-BUFFER"), popup_buffer);

  int status = 0;

#ifdef LITE_GFX
  status = enter_lite_gui();
#else
  enter_repl(*genv());
#endif

  exit_safe(status);
  return status;
}
