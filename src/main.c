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

void help(int argc, char **argv) {
  printf("USAGE: `%s [filepath to open] [flags/options] [ -- filepaths to evaluate ]`\n"
         "Flags:\n"
         "    --script ... evaluate the given files and then exit, printing\n"
         "                 nothing except what is done from LITE LISP.\n",
         argv[0]);
  exit(0);
}

// Find and keep track of recognized arguments.
static int arg_script_index          = -1;
static int arg_eval_delimiter_index  = -1;
static int arg_eval_index            = -1;
static int arg_file_index            = -1;

void handle_arguments(int argc, char **argv) {
  if (argc == 1) {
    return;
  }
  for (int i = 1; i < argc; ++i) {
    if ((strcmp(argv[i], "help") == 0)
        || (strcmp(argv[i], "-help") == 0)
        || (strcmp(argv[i], "--help") == 0)
        ) {
      help(argc, argv);
      exit(0);
    }
    if (strcmp(argv[i], "--script") == 0) {
      if (arg_script_index != -1) {
        printf("LITE does not accept multiple `--script` arguments.\n");
        exit(1);
      }
      arg_script_index = i;
      strict_output = true;
      continue;
    }
    if (strcmp(argv[i], "--") == 0) {
      if (i + 1 >= argc) {
        continue;
      }
      arg_eval_delimiter_index = i;
      arg_eval_index = i + 1;
      return;
    }
    if (arg_file_index != -1) {
      fprintf(stderr, "WARN: Multiple filepaths given, using latest\n");
    }
    arg_file_index = i;
  }
  return;
}

int args_count = 0;
char **args_vector = NULL;

int main(int argc, char **argv) {
  args_count = argc;
  args_vector = argv;

  Error err = ok;
  Atom result = nil;
  handle_arguments(argc, argv);

  Atom initial_buffer = nil;
  if (arg_file_index == -1) {
    // TODO: Don't panic when default file isn't found. Also maybe
    // have default file included XD.
    initial_buffer = initialize_buffer_or_panic("LITE_SHINES_UPON_US.txt");
  } else {
    initial_buffer = initialize_buffer_or_panic(argv[arg_file_index]);
  }
  env_set(*genv(), make_sym("CURRENT-BUFFER"), initial_buffer);

# ifdef LITE_GFX
  Atom popup_buffer = initialize_buffer_or_panic(".popup");
  env_set(*genv(), make_sym("POPUP-BUFFER"), popup_buffer);

  int gui_status = create_gui();
  if (gui_status != CREATE_GUI_OK && gui_status != CREATE_GUI_ALREADY_CREATED) {
    return 420;
  }
# endif

  // Attempt to load the standard library at a relative path from the
  // executable. This has proven the most reliable, given the
  // executable of LITE hasn't been moved.
  // "<argv[0]>/../../<std_path>"
  const char *const std_path = "lisp/std.lt";
  char *concat_path = string_trijoin(argv[0], "/../../", std_path);
  err = evaluate_file(*genv(), concat_path, &result);
  if (err.type == ERROR_FILE) {
    // Try loading the path directly. For some reason, this is needed
    // on Windows when LITE is in PATH.

    err = evaluate_file(*genv(), std_path, &result);
    if (err.type == ERROR_FILE) {
      fprintf(stderr,
              "[WARN]: LITE could not load the standard library,\n"
              "and a large portion of the functionality will be missing.\n"
              "  Path: %s\n", concat_path);
    }
  }
  if (err.type) {
    print_error(err);
    return 7;
  }
  free(concat_path);

  // Evaluate all the arguments as file paths, except for detected
  // valid arguments.
  if (arg_eval_index != -1) {
    for (int i = arg_eval_index; i < argc; ++i) {
      err = evaluate_file(*genv(), argv[i], &result);
      if (err.type) {
        printf("%s: ", argv[i]);
        print_error(err);
        err = ok;
      }
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
