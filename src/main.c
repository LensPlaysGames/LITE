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
#  include <gui.h>
#endif

int main(int argc, char **argv) {
  Error err = ok;
  Atom result = nil;

  // Find and keep track of recognized arguments.
  int arg_script_index = -1;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--script") == 0) {
      if (arg_script_index != -1) {
        printf("LITE does not accept multiple `--script` arguments.\n");
        exit(1);
      }
      arg_script_index = i;
      break;
    }
  }

  // Only initialize buffers before evaluating arguments as filepaths
  // when NOT in script mode. This is needed due to get_file() and
  // friends printing to standard out, which breaks end-to-end tests.
  if (arg_script_index == -1) {
    Atom initial_buffer = initialize_buffer_or_panic("LITE_SHINES_UPON_US.txt");
    env_set(*genv(), make_sym("CURRENT-BUFFER"), initial_buffer);

#   ifdef LITE_GFX
    Atom popup_buffer = initialize_buffer_or_panic(".popup");
    env_set(*genv(), make_sym("POPUP-BUFFER"), popup_buffer);

    int status = create_gui();
    if (status != CREATE_GUI_OK && status != CREATE_GUI_ALREADY_CREATED) {
      return 420;
    }
#   endif

  }

  // Evaluate all the arguments as file paths, except for detected
  // valid arguments.
  for (int i = 1; i < argc; ++i) {
    if (i == arg_script_index) { continue; }
    err = evaluate_file(*genv(), argv[i], &result);
    if (err.type) {
      printf("%s: ", argv[i]);
      print_error(err);
      err = ok;
    }
  }

  if (arg_script_index != -1) {
    exit_safe(0);
  }

  const char *home_path_var = "HOME";
  const char *home_path = getenv(home_path_var);
  if (!home_path) {
    home_path_var = "APPDATA";
    home_path = getenv(home_path_var);
  }
  if (!home_path) {
    home_path_var = "USERPROFILE";
    home_path = getenv(home_path_var);
  }
  if (!home_path) {
    home_path_var = "HOMEPATH";
    home_path = getenv(home_path_var);
  }
  if (home_path) {
    env_set(*genv(), make_sym("HOMEPATH"), make_string((char *)home_path));
    char *lite_path = string_join(home_path, "/.lite");
    if (lite_path) {
      env_set(*genv(), make_sym("LITEPATH"), make_string(lite_path));
      err = evaluate_file(*genv(), lite_path, &result);
      free(lite_path);
      if (err.type == ERROR_FILE) {
        printf(".lite could not be loaded from \"%s\" (%s)\n",
               home_path, home_path_var);
      } else if (err.type) {
        printf(".lite ");
        print_error(err);
      } else {
        printf(".lite successfully loaded from \"%s\" (%s)\n",
               home_path, home_path_var);
      }
    } else {
      printf("Could not create path: allocation failure.\n");
    }
  } else {
    printf("LITE will attempt to load \".lite\" from the path at"
           "the HOME environment variable, but it is not set.");
  }

  int status = 0;
  printf("|> LITE will guide the way through the darkness.\n");
# ifdef LITE_GFX
  status = enter_lite_gui();
# else
  enter_repl(*genv());
# endif
  exit_safe(status);

  return status;
}
