#include <builtins.h>

#include <assert.h>
#include <buffer.h>
#include <error.h>
#include <environment.h>
#include <evaluation.h>
#include <file_io.h>
#include <keystrings.h>
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
#  include <gfx.h>
#  include <gui.h>
#endif

#define BUILTIN_ENSURE_NO_ARGUMENTS(builtin_args) do {  \
    if (!nilp(builtin_args)) {                          \
      return ERROR_ARGUMENTS;                           \
    }                                                   \
  } while (0);

#define BUILTIN_ENSURE_ONE_ARGUMENT(builtin_args) do {      \
    if (nilp(builtin_args) || !nilp(cdr(builtin_args))) {   \
      return ERROR_ARGUMENTS;                               \
    }                                                       \
  } while (0);

#define BUILTIN_ENSURE_TWO_ARGUMENTS(builtin_args) do { \
    if (nilp(builtin_args)                              \
        || nilp(cdr(builtin_args))                      \
        || !nilp(cdr(cdr(builtin_args))))               \
      {                                                 \
        return ERROR_ARGUMENTS;                         \
      }                                                 \
  } while (0);

#define BUILTIN_ENSURE_THREE_ARGUMENTS(builtin_args) do {   \
    if (nilp(builtin_args)                                  \
        || nilp(cdr(builtin_args))                          \
        || nilp(cdr(cdr(builtin_args)))                     \
        || !nilp(cdr(cdr(cdr(builtin_args)))))              \
      {                                                     \
        return ERROR_ARGUMENTS;                             \
      }                                                     \
  } while (0);

#define BUILTIN_ENSURE_FOUR_ARGUMENTS(builtin_args) do {    \
    if (nilp(builtin_args)                                  \
        || nilp(cdr(builtin_args))                          \
        || nilp(cdr(cdr(builtin_args)))                     \
        || nilp(cdr(cdr(cdr(builtin_args))))                \
        || !nilp(cdr(cdr(cdr(cdr(builtin_args))))))         \
      {                                                     \
        return ERROR_ARGUMENTS;                             \
      }                                                     \
  } while (0);

const char *const builtin_quit_lisp_name = "QUIT-LISP";
const char *const builtin_quit_lisp_docstring =
  "(quit-lisp)\n"
  "\n"
  "Stop all evaluation of LISP, and exit all prompts.\n"
  "If you did something on accident, call this.";
int builtin_quit_lisp(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_NO_ARGUMENTS(arguments);
  user_quit = 1;
  *result = nil;
  return ERROR_NONE;
}

const char *const builtin_docstring_name = "DOCSTRING";
const char *const builtin_docstring_docstring =
  "(docstring SYMBOL ENVIRONMENT)\n"
  "\n"
  "Return the docstring of SYMBOL within ENVIRONMENT as a string.";
int builtin_docstring(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_TWO_ARGUMENTS(arguments);

  Atom atom = car(arguments);
  Atom environment = car(cdr(arguments));

  if (!pairp(environment)) {
    return ERROR_TYPE;
  }

  if (symbolp(atom)) {
    Atom symbol_in = atom;
    Error err = env_get(environment, symbol_in, &atom);
    print_error(err);
    if (err.type) { return err.type; }
  }

  // If atom is of type closure, show closure signature (arguments).
  // FIXME: The docstring could be set to this value instead of
  // creating this new string each time the docstring is fetched.
  // This is absolutely horrid code... OML.
  char *docstring = NULL;
  if (closurep(atom) || macrop(atom)) {
    // Prepend docstring with closure signature.
    char *signature = atom_string(car(cdr(atom)), NULL);
    size_t siglen = 0;
    if (signature && (siglen = strlen(signature)) != 0) {
      if (atom.docstring) {
        size_t newlen = strlen(atom.docstring) + siglen + 10;
        char *newdoc = (char *)malloc(newlen);
        if (newdoc) {
          memcpy(newdoc, "ARGS: \0", 7);
          strcat(newdoc, signature);
          strcat(newdoc, "\n\n");
          strcat(newdoc, atom.docstring);
          docstring = newdoc;
        } else {
          // Could not allocate buffer for new docstring,
          // free allocated memory and use regular docstring.
          newdoc = (char *)atom.docstring;
        }
        free(signature);
        docstring = newdoc;
      } else {
        docstring = signature;
      }
    }
  }
  if (docstring) {
    *result = make_string(docstring);
    free(docstring);
    docstring = NULL;
  } else {
    if (atom.docstring) {
      *result = make_string(atom.docstring);
    } else {
      *result = nil;
    }
  }
  return ERROR_NONE;
}

int typep(Atom arguments, enum AtomType type, Atom *result) {
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  *result = car(arguments).type == type ? make_sym("T") : nil;
  return ERROR_NONE;
}

const char *const builtin_nilp_name = "NILP";
const char *const builtin_nilp_docstring =
  "(nilp ARG)\n"
  "\n"
  "Return 'T' iff ARG has a type of 'NIL', otherwise return nil.";
int builtin_nilp(Atom arguments, Atom *result) {
  if (!nilp(cdr(arguments))) {
    return ERROR_ARGUMENTS;
  }
  *result = car(arguments).type == ATOM_TYPE_NIL ? make_sym("T") : nil;
  return ERROR_NONE;
}

const char *const builtin_pairp_name = "PAIRP";
const char *const builtin_pairp_docstring =
  "(pairp ARG)\n"
  "\n"
  "Return 'T' iff ARG has a type of 'PAIR', otherwise return nil.";
int builtin_pairp(Atom arguments, Atom *result) {
  return typep(arguments, ATOM_TYPE_PAIR, result);
}

const char *const builtin_symbolp_name = "SYMBOLP";
const char *const builtin_symbolp_docstring =
  "(symbolp ARG)\n"
  "\n"
  "Return 'T' iff ARG has a type of 'SYMBOL', otherwise return nil.";
int builtin_symbolp(Atom arguments, Atom *result) {
  return typep(arguments, ATOM_TYPE_SYMBOL, result);
}

const char *const builtin_integerp_name = "INTEGERP";
const char *const builtin_integerp_docstring =
  "(integerp ARG)\n"
  "\n"
  "Return 'T' iff ARG has a type of 'INTEGER', otherwise return nil.";
int builtin_integerp(Atom arguments, Atom *result) {
  return typep(arguments, ATOM_TYPE_INTEGER, result);
}

const char *const builtin_builtinp_name = "BUILTINP";
const char *const builtin_builtinp_docstring =
  "(builtinp ARG)\n"
  "\n"
  "Return 'T' iff ARG has a type of 'BUILTIN', otherwise return nil.";
int builtin_builtinp(Atom arguments, Atom *result) {
  return typep(arguments, ATOM_TYPE_BUILTIN, result);
}

const char *const builtin_closurep_name = "CLOSUREP";
const char *const builtin_closurep_docstring =
  "(closurep ARG)\n"
  "\n"
  "Return 'T' iff ARG has a type of 'CLOSURE', otherwise return nil.";
int builtin_closurep(Atom arguments, Atom *result) {
  return typep(arguments, ATOM_TYPE_CLOSURE, result);
}

const char *const builtin_macrop_name = "MACROP";
const char *const builtin_macrop_docstring =
  "(macrop ARG)\n"
  "\n"
  "Return 'T' iff ARG has a type of 'MACRO', otherwise return nil.";
int builtin_macrop(Atom arguments, Atom *result) {
  return typep(arguments, ATOM_TYPE_MACRO, result);
}

const char *const builtin_stringp_name = "STRINGP";
const char *const builtin_stringp_docstring =
  "(stringp ARG)\n"
  "\n"
  "Return 'T' iff ARG has a type of 'STRING', otherwise return nil.";
int builtin_stringp(Atom arguments, Atom *result) {
  return typep(arguments, ATOM_TYPE_STRING, result);
}

const char *const builtin_bufferp_name = "BUFFERP";
const char *const builtin_bufferp_docstring =
  "(bufferp ARG)\n"
  "\n"
  "Return 'T' iff ARG has a type of 'BUFFER', otherwise return nil.";
int builtin_bufferp(Atom arguments, Atom *result) {
  return typep(arguments, ATOM_TYPE_BUFFER, result);
}

const char *const builtin_not_name = "!";
const char *const builtin_not_docstring =
  "(! ARG)\n"
  "\n"
  "Given ARG is nil, return 'T', otherwise return nil.";
int builtin_not(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  *result = nilp(car(arguments)) ? make_sym("T") : nil;
  return ERROR_NONE;
}

const char *const builtin_car_name = "CAR";
const char *const builtin_car_docstring =
  "(car ARG)\n"
  "\n"
  "Given ARG is a pair, return the value on the left side. Otherwise,\n"
  "return nil.\n"
  "CAR stands for \"Contents of the Address part of Register N\".\n"
  "This was in reference to the machine instructions used to implement\n"
  "LISP originally, in the 1950s.";
int builtin_car(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  if (nilp(car(arguments))) {
    *result = nil;
  } else if (car(arguments).type != ATOM_TYPE_PAIR) {
    return ERROR_TYPE;
  } else {
    *result = car(car(arguments));
  }
  return ERROR_NONE;
}

const char *const builtin_cdr_name = "CDR";
const char *const builtin_cdr_docstring =
  "(cdr ARG)\n"
  "\n"
  "Given ARG is a pair, return the value on the right side. Otherwise,\n"
  "return nil.\n"
  "CDR stands for \"Contents of the Decrement part of the Register N\".\n"
  "This was in reference to the machine instructions used to implement\n"
  "LISP originally, in the 1950s.";
int builtin_cdr(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  if (nilp(car(arguments))) {
    *result = nil;
  } else if (car(arguments).type != ATOM_TYPE_PAIR) {
    return ERROR_TYPE;
  } else {
    *result = cdr(car(arguments));
  }
  return ERROR_NONE;
}

const char *const builtin_cons_name = "CONS";
const char *const builtin_cons_docstring =
  "(cons LEFT RIGHT)\n"
  "\n"
  "Return a new pair, with LEFT and RIGHT on each side, respectively.";
int builtin_cons(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_TWO_ARGUMENTS(arguments);
  *result = cons(car(arguments), car(cdr(arguments)));
  return ERROR_NONE;
}

const char *const builtin_setcar_name = "SETCAR";
const char *const builtin_setcar_docstring =
  "(setcar PAIR VALUE)\n"
  "\n"
  "Set the left side of PAIR to the given VALUE.";
int builtin_setcar(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_TWO_ARGUMENTS(arguments);
  if (!pairp(car(arguments))) {
    return ERROR_TYPE;
  }
  car(car(arguments)) = car(cdr(arguments));
  *result = nil;
  return ERROR_NONE;
}

const char *const builtin_setcdr_name = "SETCDR";
const char *const builtin_setcdr_docstring =
  "(setcdr PAIR VALUE)\n"
  "\n"
  "Set the right side of PAIR to the given VALUE.";
int builtin_setcdr(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_TWO_ARGUMENTS(arguments);
  if (!pairp(car(arguments))) {
    return ERROR_TYPE;
  }
  cdr(car(arguments)) = car(cdr(arguments));
  *result = nil;
  return ERROR_NONE;
}

const char *const builtin_member_name = "MEMBER";
const char *const builtin_member_docstring =
  "(member ELEMENT LIST)\n"
  "\n"
  "Return non-nil iff ELEMENT is an element of LIST.";
int builtin_member(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_TWO_ARGUMENTS(arguments);
  Atom key = car(arguments);
  Atom list = car(cdr(arguments));
  *result = nil;
  for (; !nilp(list); list = cdr(list)) {
    Atom equal = compare_atoms(key, car(list));
    if (!nilp(equal)) { *result = equal; }
  }
  return ERROR_NONE;
}

const char *const builtin_length_name = "LENGTH";
const char *const builtin_length_docstring =
  "(length SEQUENCE)\n"
  "\n"
  "Return the length of SEQUENCE, if it is a string, symbol, or list.\n"
  "Otherwise, return nil.\n";
int builtin_length(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  Atom arg = car(arguments);
  size_t size = 0;
  if (pairp(arg)) {
    for (; !nilp(arg); arg = cdr(arg)) {
      ++size;
    }
  } else if (symbolp(arg) || stringp(arg)) {
    size = strlen(arg.value.symbol);
  } else {
    *result = nil;
    return ERROR_NONE;
  }
  *result = make_int(size);
  return ERROR_NONE;
}

const char *const builtin_add_name = "+";
const char *const builtin_add_docstring =
  "(+ A B)\n"
  "\n"
  "Add two integer numbers A and B together, and return the computed result.";
int builtin_add(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_TWO_ARGUMENTS(arguments);
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (lhs.type != ATOM_TYPE_INTEGER || rhs.type != ATOM_TYPE_INTEGER) {
    return ERROR_TYPE;
  }
  *result = make_int(lhs.value.integer + rhs.value.integer);
  return ERROR_NONE;
}

const char *const builtin_subtract_name = "-";
const char *const builtin_subtract_docstring =
  "(- A B)\n"
  "\n"
  "Subtract integer B from integer A and return the computed result.";
int builtin_subtract(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_TWO_ARGUMENTS(arguments);
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (lhs.type != ATOM_TYPE_INTEGER || rhs.type != ATOM_TYPE_INTEGER) {
    return ERROR_TYPE;
  }
  *result = make_int(lhs.value.integer - rhs.value.integer);
  return ERROR_NONE;
}

const char *const builtin_multiply_name = "*";
const char *const builtin_multiply_docstring =
  "(* A B)\n"
  "\n"
  "Multiply integer numbers A and B together and return the computed result.";
int builtin_multiply(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_TWO_ARGUMENTS(arguments);
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (lhs.type != ATOM_TYPE_INTEGER || rhs.type != ATOM_TYPE_INTEGER) {
    return ERROR_TYPE;
  }
  *result = make_int(lhs.value.integer * rhs.value.integer);
  return ERROR_NONE;
}

const char *const builtin_divide_name = "/";
const char *const builtin_divide_docstring =
  "(/ A B)\n"
  "\n"
  "Divide integer B out of integer A and return the computed result.\n"
  "`(/ 6 3)` == \"6 / 3\" == 2";
int builtin_divide(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_TWO_ARGUMENTS(arguments);
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (lhs.type != ATOM_TYPE_INTEGER || rhs.type != ATOM_TYPE_INTEGER) {
    return ERROR_TYPE;
  }
  if (rhs.value.integer == 0) {
    return ERROR_ARGUMENTS;
  }
  *result = make_int(lhs.value.integer / rhs.value.integer);
  return ERROR_NONE;
}

const char *const builtin_remainder_name = "%";
const char *const builtin_remainder_docstring =
  "(% N M)\n"
  "\n"
  "Return the remainder left when N is divided by M.";
int builtin_remainder(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_TWO_ARGUMENTS(arguments);
  Atom n = car(arguments);
  Atom m = car(cdr(arguments));
  if (!integerp(n) || !integerp(m)) {
    return ERROR_TYPE;
  }
  *result = make_int(n.value.integer % m.value.integer);
  return ERROR_NONE;
}

const char *const builtin_buffer_toggle_mark_name = "BUFFER-TOGGLE-MARK";
const char *const builtin_buffer_toggle_mark_docstring =
  "(buffer-toggle-mark BUFFER)\n\nToggle mark activation on the given buffer.";
int builtin_buffer_toggle_mark(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  Atom buffer = car(arguments);
  if (!bufferp(buffer) || !buffer.value.buffer) {
    return ERROR_TYPE;
  }
  buffer_toggle_mark(buffer.value.buffer);
  *result = nil;
  if (buffer_mark_active(*buffer.value.buffer)) {
    *result = make_sym("T");
  }
  return ERROR_NONE;
}

const char *const builtin_buffer_set_mark_activation_name = "BUFFER-SET-MARK-ACTIVATION";
const char *const builtin_buffer_set_mark_activation_docstring =
  "(buffer-set-mark-activation BUFFER STATE)"
  "\n"
  "Set activation state of mark in BUFFER based on STATE being non-nil or not.";
int builtin_buffer_set_mark_activation(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_TWO_ARGUMENTS(arguments);
  Atom buffer = car(arguments);
  Atom state = car(cdr(arguments));
  if (!bufferp(buffer) || !buffer.value.buffer) {
    return ERROR_TYPE;
  }
  Error err = buffer_set_mark_activation(buffer.value.buffer, !nilp(state));
  if (err.type) {
    print_error(err);
    return err.type;
  }
  *result = state;
  return ERROR_NONE;
}

const char *const builtin_buffer_set_mark_name = "BUFFER-SET-MARK";
const char *const builtin_buffer_set_mark_docstring =
  "(buffer-set-mark BUFFER MARK)\n\nSet marked byte in BUFFER to MARK.";
int builtin_buffer_set_mark(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_TWO_ARGUMENTS(arguments);
  Atom buffer = car(arguments);
  Atom mark = car(cdr(arguments));
  if (!bufferp(buffer) || !buffer.value.buffer || !integerp(mark)) {
    return ERROR_TYPE;
  }
  buffer_set_mark(buffer.value.buffer, mark.value.integer);
  *result = nil;
  if (buffer_mark_active(*buffer.value.buffer)) {
    *result = make_sym("T");
  }
  return ERROR_NONE;
}

const char *const builtin_buffer_mark_name = "BUFFER-MARK";
const char *const builtin_buffer_mark_docstring =
  "(buffer-mark BUFFER)\n\nReturn byte offset of mark in BUFFER.";
int builtin_buffer_mark(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  Atom buffer = car(arguments);
  if (!bufferp(buffer) || !buffer.value.buffer) {
    return ERROR_TYPE;
  }
  *result = make_int(buffer_mark(*buffer.value.buffer));
  return ERROR_NONE;
}

const char *const builtin_buffer_mark_activated_name = "BUFFER-MARK-ACTIVE";
const char *const builtin_buffer_mark_activated_docstring =
  "(buffer-mark-active BUFFER)\n"
  "\n"
  "Return T iff mark is active in BUFFER. Otherwise, return nil.";
int builtin_buffer_mark_activated(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  Atom buffer = car(arguments);
  if (!bufferp(buffer) || !buffer.value.buffer) {
    return ERROR_TYPE;
  }
  *result = nil;
  if (buffer_mark_active(*buffer.value.buffer)) {
    *result = make_sym("T");
  }
  return ERROR_NONE;
}

const char *const builtin_buffer_region_name = "BUFFER-REGION";
const char *const builtin_buffer_region_docstring =
  "(buffer-region BUFFER)\n\nReturn the region between mark and point in BUFFER.";
int builtin_buffer_region(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  Atom buffer = car(arguments);
  if (!bufferp(buffer) || !buffer.value.buffer) {
    return ERROR_TYPE;
  }
  char *region = buffer_region(*buffer.value.buffer);
  if (!region) { return ERROR_GENERIC; }
  *result = make_string(region);
  free(region);
  return ERROR_NONE;
}

const char *const builtin_buffer_region_length_name = "BUFFER-REGION-LENGTH";
const char *const builtin_buffer_region_length_docstring =
  "(buffer-region-length BUFFER)\n"
  "\n"
  "Get byte difference between mark and point within BUFFER; "
  "the byte length of the selected region";
int builtin_buffer_region_length(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  Atom buffer = car(arguments);
  if (!bufferp(buffer) || !buffer.value.buffer) {
    return ERROR_TYPE;
  }
  *result = make_int(buffer_region_length(*buffer.value.buffer));
  return ERROR_NONE;
}

const char *const builtin_open_buffer_name = "OPEN-BUFFER";
const char *const builtin_open_buffer_docstring =
  "(open-buffer PATH)\n\nReturn a buffer visiting PATH.";
int builtin_open_buffer(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  Atom path = car(arguments);
  if (!stringp(path) || !path.value.symbol) {
    return ERROR_TYPE;
  }
  if (file_exists(path.value.symbol)) {
    // TODO: A buffer-local environment should go in each call to
    // make_buffer, or something?
    *result = make_buffer(env_create(nil), (char *)path.value.symbol);
    return ERROR_NONE;
  }

  char *working_path = NULL;
  if (args_vector) {
    working_path = string_trijoin(args_vector[0], "/../../", path.value.symbol);
    if (file_exists(working_path)) {
      *result = make_buffer(env_create(nil), working_path);
      free(working_path);
      return ERROR_NONE;
    }
    free(working_path);
  }

  // Attempt to load relative to path of current buffer.
  Atom current_buffer = nil;
  Error err = env_get(*genv(), make_sym("CURRENT-BUFFER"), &current_buffer);
  if (!err.type && bufferp(current_buffer) && current_buffer.value.buffer) {
    working_path = string_trijoin(current_buffer.value.buffer->path, "/../", path.value.symbol);
    if (file_exists(working_path)) {
      *result = make_buffer(env_create(nil), working_path);
      free(working_path);
      return ERROR_NONE;
    }
    free(working_path);
    working_path = string_trijoin(current_buffer.value.buffer->path, "/", path.value.symbol);
    if (file_exists(working_path)) {
      *result = make_buffer(env_create(nil), working_path);
      free(working_path);
      return ERROR_NONE;
    }
  }
  free(working_path);

  // If no existing file was found, just make a buffer at a new file.
  *result = make_buffer(env_create(nil), (char *)path.value.symbol);
  return ERROR_NONE;
}

const char *const builtin_buffer_path_name = "BUFFER-PATH";
const char *const builtin_buffer_path_docstring =
  "(buffer-path BUFFER)\n\nReturn buffer's path as a string.";
int builtin_buffer_path(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  Atom buffer = car(arguments);
  if (!bufferp(buffer)) {
    return ERROR_TYPE;
  }
  *result = make_string(buffer.value.buffer->path);
  return ERROR_NONE;
}

const char *const builtin_buffer_table_name = "BUF";
const char *const builtin_buffer_table_docstring =
  "(buf)\n\nReturn the LISP buffer table.";
int builtin_buffer_table(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_NO_ARGUMENTS(arguments);
  *result = *buf_table();
  return ERROR_NONE;
}

const char *const builtin_buffer_insert_name = "BUFFER-INSERT";
const char *const builtin_buffer_insert_docstring =
  "(buffer-insert BUFFER STRING) \n"
  "\n"
  "Insert STRING into BUFFER at point.";
int builtin_buffer_insert(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_TWO_ARGUMENTS(arguments);
  Atom buffer = car(arguments);
  Atom string = car(cdr(arguments));
  if (!bufferp(buffer) || !stringp(string)) {
    return ERROR_TYPE;
  }
  Error err = buffer_insert
    (buffer.value.buffer, (char *)string.value.symbol);
  if (err.type) {
    print_error(err);
    return err.type;
  }
  *result = buffer;
  return ERROR_NONE;
}

const char *const builtin_buffer_remove_name = "BUFFER-REMOVE";
const char *const builtin_buffer_remove_docstring =
  "(buffer-remove BUFFER COUNT) \n"
  "\n"
  "Backspace COUNT bytes from BUFFER at point.";
int builtin_buffer_remove(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_TWO_ARGUMENTS(arguments);
  Atom buffer = car(arguments);
  Atom count = car(cdr(arguments));
  if (!bufferp(buffer) || !integerp(count)) {
    return ERROR_TYPE;
  }
  Error err = buffer_remove_bytes(buffer.value.buffer
                                  , count.value.integer);
  if (err.type) {
    print_error(err);
    return err.type;
  }
  *result = buffer;
  return ERROR_NONE;
}

const char *const builtin_buffer_remove_forward_name = "BUFFER-REMOVE-FORWARD";
const char *const builtin_buffer_remove_forward_docstring =
  "(buffer-remove-forward BUFFER COUNT) \n"
  "\n"
  "Remove COUNT bytes from BUFFER following point.";
int builtin_buffer_remove_forward(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_TWO_ARGUMENTS(arguments);
  Atom buffer = car(arguments);
  Atom count = car(cdr(arguments));
  if (!bufferp(buffer) || !integerp(count)) {
    return ERROR_TYPE;
  }
  Error err = buffer_remove_bytes_forward(buffer.value.buffer
                                          , count.value.integer);
  if (err.type) {
    print_error(err);
    return err.type;
  }
  *result = buffer;
  return ERROR_NONE;
}

const char *const builtin_buffer_set_point_name = "BUFFER-SET-POINT";
const char *const builtin_buffer_set_point_docstring =
  "(buffer-set-point BUFFER POINT) \n"
  "\n"
  "Set byte offset of cursor within BUFFER to POINT.";
int builtin_buffer_set_point(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_TWO_ARGUMENTS(arguments);
  Atom buffer = car(arguments);
  Atom point = car(cdr(arguments));
  if (!bufferp(buffer) || !integerp(point)) {
    return ERROR_TYPE;
  }
  size_t new_point_byte = 0;
  if (point.value.integer > 0) {
    if (point.value.integer > (integer_t)buffer.value.buffer->rope->weight) {
      new_point_byte = buffer.value.buffer->rope->weight;
    } else {
      new_point_byte = point.value.integer;
    }
  }
  buffer.value.buffer->point_byte = new_point_byte;
  *result = buffer;
  return ERROR_NONE;
}

const char *const builtin_buffer_point_name = "BUFFER-POINT";
const char *const builtin_buffer_point_docstring =
  "(buffer-point BUFFER) \n"
  "\n"
  "Get byte offset of cursor (point) within BUFFER.";
int builtin_buffer_point (Atom arguments, Atom *result) {
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  Atom buffer = car(arguments);
  if (!bufferp(buffer)) {
    return ERROR_TYPE;
  }
  *result = make_int(buffer.value.buffer->point_byte);
  return ERROR_NONE;
}

const char *const builtin_buffer_index_name = "BUFFER-INDEX";
const char *const builtin_buffer_index_docstring =
  "(buffer-index BUFFER INDEX)\n"
  "\n"
  "Get character from BUFFER at INDEX";
int builtin_buffer_index (Atom arguments, Atom *result) {
  BUILTIN_ENSURE_TWO_ARGUMENTS(arguments);
  Atom buffer = car(arguments);
  Atom index = car(cdr(arguments));
  if (!bufferp(buffer) || !integerp(index)) {
    return ERROR_TYPE;
  }
  char one_byte_string[2];
  one_byte_string[0] = rope_index(buffer.value.buffer->rope
                                  , index.value.integer);
  one_byte_string[1] = '\0';
  *result = make_string(&one_byte_string[0]);
  return ERROR_NONE;
}

const char *const builtin_buffer_string_name = "BUFFER-STRING";
const char *const builtin_buffer_string_docstring =
  "(buffer-string BUFFER)\n"
  "\n"
  "Get the contents of BUFFER as a string. "
  "Be careful with large files.";
int builtin_buffer_string(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  Atom buffer = car(arguments);
  if (!bufferp(buffer) || !buffer.value.buffer) {
    return ERROR_TYPE;
  }
  char *contents = buffer_string(*buffer.value.buffer);
  if (!contents) { return ERROR_GENERIC; }
  *result = make_string(contents);
  free(contents);
  return ERROR_NONE;
}

const char *const builtin_buffer_lines_name = "BUFFER-LINES";
const char *const builtin_buffer_lines_docstring =
  "(buffer-lines BUFFER START-LINE LINE-COUNT)\n"
  "\n"
  "Get LINE-COUNT lines starting at START-LINE within BUFFER.";
int builtin_buffer_lines (Atom arguments, Atom *result) {
  BUILTIN_ENSURE_THREE_ARGUMENTS(arguments);
  Atom buffer = car(arguments);
  if (!bufferp(buffer) || !buffer.value.buffer) {
    return ERROR_TYPE;
  }
  Atom start_line = car(cdr(arguments));
  if (!integerp(start_line)) {
    return ERROR_TYPE;
  }
  Atom line_count = car(cdr(cdr(arguments)));
  if (!integerp(line_count)) {
    return ERROR_TYPE;
  }
  char *lines = buffer_lines
    (*buffer.value.buffer
     , start_line.value.integer
     , line_count.value.integer);
  *result = make_string(lines);
  free(lines);
  return ERROR_NONE;
}

const char *const builtin_buffer_line_name = "BUFFER-LINE";
const char *const builtin_buffer_line_docstring =
  "(buffer-line BUFFER LINE-NUMBER)\n"
  "\n"
  "Get line LINE-NUMBER from BUFFER contents as string.";
int builtin_buffer_line  (Atom arguments, Atom *result) {
  BUILTIN_ENSURE_TWO_ARGUMENTS(arguments);
  Atom buffer = car(arguments);
  if (!bufferp(buffer) || !buffer.value.buffer) {
    return ERROR_TYPE;
  }
  Atom line_count = car(cdr(arguments));
  if (!integerp(line_count)) {
    return ERROR_TYPE;
  }
  char *line = buffer_line(*buffer.value.buffer
                           , line_count.value.integer);
  *result = make_string(line);
  free(line);
  return ERROR_NONE;
}

const char *const builtin_buffer_current_line_name = "BUFFER-CURRENT-LINE";
const char *const builtin_buffer_current_line_docstring =
  "(buffer-current-line BUFFER)\n"
  "\n"
  "Get line surrounding point in BUFFER as string.";
int builtin_buffer_current_line(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  Atom buffer = car(arguments);
  if (!bufferp(buffer) || !buffer.value.buffer) {
    return ERROR_TYPE;
  }
  char *line = buffer_current_line(*buffer.value.buffer);
  *result = make_string(line);
  free(line);
  return ERROR_NONE;
}

const char *const builtin_numeq_name = "=";
const char *const builtin_numeq_docstring =
  "(= ARG1 ARG2)\n"
  "\n"
  "Return 'T' iff the two given arguments have the same integer value.";
int builtin_numeq(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_TWO_ARGUMENTS(arguments);
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (lhs.type != ATOM_TYPE_INTEGER || rhs.type != ATOM_TYPE_INTEGER) {
    return ERROR_TYPE;
  }
  *result = lhs.value.integer == rhs.value.integer ? make_sym("T") : nil;
  return ERROR_NONE;
}

const char *const builtin_numnoteq_name = "!=";
const char *const builtin_numnoteq_docstring =
  "(!= ARG1 ARG2) \n"
  "\n"
  "Return 'T' iff the two given arguments *do not* have the same integer value.";
int builtin_numnoteq(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_TWO_ARGUMENTS(arguments);
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (lhs.type != ATOM_TYPE_INTEGER || rhs.type != ATOM_TYPE_INTEGER) {
    return ERROR_TYPE;
  }
  *result = lhs.value.integer != rhs.value.integer ? make_sym("T") : nil;
  return ERROR_NONE;
}

const char *const builtin_numlt_name = "<";
const char *const builtin_numlt_docstring =
  "(< INT-A INT-B)\n"
  "\n"
  "Return 'T' iff integer A is less than integer B.";
int builtin_numlt(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_TWO_ARGUMENTS(arguments);
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (lhs.type != ATOM_TYPE_INTEGER || rhs.type != ATOM_TYPE_INTEGER) {
    return ERROR_TYPE;
  }
  *result = lhs.value.integer < rhs.value.integer ? make_sym("T") : nil;
  return ERROR_NONE;
}

const char *const builtin_numlt_or_eq_name = "<=";
const char *const builtin_numlt_or_eq_docstring =
  "(<= INT-A INT-B)\n"
  "\n"
  "Return 'T' iff integer A is less than or equal to integer B.";
int builtin_numlt_or_eq(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_TWO_ARGUMENTS(arguments);
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (lhs.type != ATOM_TYPE_INTEGER || rhs.type != ATOM_TYPE_INTEGER) {
    return ERROR_TYPE;
  }
  *result = lhs.value.integer <= rhs.value.integer ? make_sym("T") : nil;
  return ERROR_NONE;
}

const char *const builtin_numgt_name = ">";
const char *const builtin_numgt_docstring =
  "(> INT-A INT-B)\n"
  "\n"
  "Return 'T' iff integer A is greater than integer B.";
int builtin_numgt(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_TWO_ARGUMENTS(arguments);
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (lhs.type != ATOM_TYPE_INTEGER || rhs.type != ATOM_TYPE_INTEGER) {
    return ERROR_TYPE;
  }
  *result = lhs.value.integer > rhs.value.integer ? make_sym("T") : nil;
  return ERROR_NONE;
}

const char *const builtin_numgt_or_eq_name = ">=";
const char *const builtin_numgt_or_eq_docstring =
  "(>= INT-A INT-B)\n"
  "\n"
  "Return 'T' iff integer A is greater than or equal to integer B.";
int builtin_numgt_or_eq(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_TWO_ARGUMENTS(arguments);
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (lhs.type != ATOM_TYPE_INTEGER || rhs.type != ATOM_TYPE_INTEGER) {
    return ERROR_TYPE;
  }
  *result = lhs.value.integer >= rhs.value.integer ? make_sym("T") : nil;
  return ERROR_NONE;
}

const char *const builtin_buffer_seek_byte_name = "BUFFER-SEEK-BYTE";
const char *const builtin_buffer_seek_byte_docstring =
  "(buffer-seek-byte buffer bytes direction)\n"
  "\n"
  "Move buffer point to the next byte that is within the control string.\n"
  "If no matching bytes are found, don't move point_byte.\n"
  "Returns the amount of bytes buffer's point was moved.";
int builtin_buffer_seek_byte(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_THREE_ARGUMENTS(arguments);
  Atom buffer = car(arguments);
  Atom bytes = car(cdr(arguments));
  Atom direction = car(cdr(cdr(arguments)));
  if (!bufferp(buffer) || !stringp(bytes) || !integerp(direction)) {
    return ERROR_TYPE;
  }
  size_t bytes_moved = buffer_seek_until_byte
    (buffer.value.buffer,
     (char *)bytes.value.symbol,
     direction.value.integer);
  *result = make_int(bytes_moved);
  return ERROR_NONE;
}

const char *const builtin_buffer_seek_past_byte_name = "BUFFER-SEEK-PAST-BYTE";
const char *const builtin_buffer_seek_past_byte_docstring =
  "(buffer-seek-past-byte buffer bytes direction)\n"
  "\n"
  "Move BUFFER point to the next byte that is NOT within the control string.\n"
  "Returns the amount of bytes BUFFER's point was moved.\n"
  "When DIRECTION is negative, seek backwards.";
int builtin_buffer_seek_past_byte(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_THREE_ARGUMENTS(arguments);
  Atom buffer = car(arguments);
  Atom bytes = car(cdr(arguments));
  Atom direction = car(cdr(cdr(arguments)));
  if (!bufferp(buffer) || !stringp(bytes) || !integerp(direction)) {
    return ERROR_TYPE;
  }
  size_t bytes_moved = buffer_seek_while_byte
    (buffer.value.buffer,
     (char *)bytes.value.symbol,
     direction.value.integer);
  *result = make_int(bytes_moved);
  return ERROR_NONE;
}

const char *const builtin_buffer_seek_substring_name = "BUFFER-SEEK-SUBSTRING";
const char *const builtin_buffer_seek_substring_docstring =
  "(buffer-seek-substring buffer substring direction)\n"
  "\n"
  "Move buffer point to the beginning of the given substring, if it exists.\n"
  "If no matching substring is found, don't move point_byte.\n"
  "When direction is negative, search backwards. Otherwise, search forwards.";
int builtin_buffer_seek_substring(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_THREE_ARGUMENTS(arguments);
  Atom buffer = car(arguments);
  Atom substring = car(cdr(arguments));
  Atom direction = car(cdr(cdr(arguments)));
  if (!bufferp(buffer) || !stringp(substring) || !integerp(direction)) {
    return ERROR_TYPE;
  }
  size_t bytes_moved = buffer_seek_until_substr
    (buffer.value.buffer,
     (char *)substring.value.symbol,
     direction.value.integer);
  *result = make_int(bytes_moved);
  return ERROR_NONE;
}

int copy_impl(Atom *copy, Atom *result) {
  assert(ATOM_TYPE_MAX == 9);
  switch (copy->type) {
  case ATOM_TYPE_NIL:
    *result = nil;
    break;
  case ATOM_TYPE_CLOSURE:
  case ATOM_TYPE_MACRO:
  case ATOM_TYPE_PAIR:
    *result = cons(nil, nil);
    copy_impl(&copy->value.pair->atom[0], &result->value.pair->atom[0]);
    copy_impl(&copy->value.pair->atom[1], &result->value.pair->atom[1]);
    break;
  case ATOM_TYPE_SYMBOL:
    *result = make_sym(copy->value.symbol);
    break;
  case ATOM_TYPE_INTEGER:
    *result = make_int(copy->value.integer);
    break;
  case ATOM_TYPE_BUILTIN:
    *result = make_builtin(copy->value.builtin.function,
                           copy->value.builtin.name,
                           copy->docstring);
    break;
  case ATOM_TYPE_STRING:
    *result = make_string(copy->value.symbol);
    break;
  case ATOM_TYPE_BUFFER:
    *result = make_buffer(copy->value.buffer->environment,
                          copy->value.buffer->path);
    break;
  default:
    break;
  }
  return ERROR_NONE;
}

const char *const builtin_copy_name = "COPY";
const char *const builtin_copy_docstring =
  "(copy ATOM)\n"
  "\n"
  "Return a deep copy of ATOM.";
int builtin_copy(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  return copy_impl(&car(arguments), result);
}

const char *const builtin_string_length_name = "STRING-LENGTH";
const char *const builtin_string_length_docstring =
  "(string-length string)\n"
  "\n"
  "Return the length of STRING.";
int builtin_string_length(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  Atom string = car(arguments);
  if (!stringp(string)) { return ERROR_TYPE; }
  size_t length = strlen(string.value.symbol);
  *result = make_int(length);
  return ERROR_NONE;
}

const char *const builtin_string_concat_name = "STRING-CONCAT";
const char *const builtin_string_concat_docstring =
  "(string-concat STRING-A STRING-B)\n"
  "\n"
  "Return a new string consisting of STRING-A followed by STRING-B.";
int builtin_string_concat(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_TWO_ARGUMENTS(arguments);
  Atom string_a = car(arguments);
  Atom string_b = car(cdr(arguments));
  if (!stringp(string_a) || !stringp(string_b)) {
    return ERROR_TYPE;
  }
  size_t string_length = strlen(string_a.value.symbol) + strlen(string_b.value.symbol);
  char *string = malloc(string_length + 1);
  snprintf(string, string_length + 1, "%s%s", string_a.value.symbol, string_b.value.symbol);
  string[string_length] = '\0';
  *result = make_string(string);
  free(string);
  return ERROR_NONE;
}

const char *const builtin_evaluate_string_name = "EVALUATE-STRING";
const char *const builtin_evaluate_string_docstring =
  "(evaluate-string STRING)\n"
  "\n"
  "Evaluate STRING as a LITE LISP expression.";
int builtin_evaluate_string(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  Atom input = car(arguments);
  if (!stringp(input) || !input.value.symbol) {
    return ERROR_TYPE;
  }
  Atom expr = nil;
  Error err = parse_expr(input.value.symbol,
                         (const char**)&input.value.symbol,
                         &expr);
  if (err.type) {
    printf("EVALUATE-STRING PARSING ");
    print_error(err);
    return err.type;
  }
  err = evaluate_expression(expr, *genv(), result);
  if (err.type) {
    printf("EVALUATE-STRING EVALUATION ");
    print_error(err);
    return err.type;
  }
  return ERROR_NONE;
}

const char *const builtin_evaluate_file_name = "EVALUATE-FILE";
const char *const builtin_evaluate_file_docstring =
  "(evaluate-file FILEPATH)\n"
  "\n"
  "Evaluate file at FILEPATH as LITE LISP source code.";
int builtin_evaluate_file(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);

  Atom filepath = car(arguments);
  if (!stringp(filepath)) { return ERROR_TYPE; }

  Error err = evaluate_file(*genv(), filepath.value.symbol, result);
  if (err.type == ERROR_FILE && args_vector) {
    // Attempt to load relative to "argv[0]/../../"
    char *path = string_trijoin(args_vector[0], "/../../", filepath.value.symbol);
    err = evaluate_file(*genv(), path, result);
    free(path);
  }
  if (err.type == ERROR_FILE) {
    // Attempt to load relative to path of current buffer.
    Atom current_buffer;
    Error err = env_get(*genv(), make_sym("CURRENT-BUFFER"), &current_buffer);
    // Treat current buffer path as file.
    if (bufferp(current_buffer) && current_buffer.value.buffer) {
      char *path = string_trijoin(current_buffer.value.buffer->path, "/../", filepath.value.symbol);
      err = evaluate_file(*genv(), path, result);
      free(path);
      if (err.type == ERROR_FILE) {
        // Treat current buffer path as directory (no trailing separator).
        char *path = string_trijoin(current_buffer.value.buffer->path, "/", filepath.value.symbol);
        err = evaluate_file(*genv(), path, result);
        free(path);
      }
    }
  }
  if (err.type) {
    print_error(err);
    return err.type;
  }
  return ERROR_NONE;
}

const char *const builtin_save_name = "SAVE";
const char *const builtin_save_docstring =
  "(save BUFFER)\n"
  "\n"
  "Save the given BUFFER to a file.";
int builtin_save(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  Atom buffer = car(arguments);
  if (!bufferp(buffer) || !buffer.value.buffer) {
    return ERROR_TYPE;
  }
  Error err = buffer_save(*buffer.value.buffer);
  if (err.type) {
    print_error(err);
    return err.type;
  }
  *result = make_sym("T");
  return ERROR_NONE;
}

const char *const builtin_apply_name = "APPLY";
const char *const builtin_apply_docstring =
  "(apply FUNCTION ARGUMENTS)\n"
  "\n"
  "Call FUNCTION with the given ARGUMENTS and return the result.";
int builtin_apply(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_TWO_ARGUMENTS(arguments);
  Atom function;
  function = car(arguments);
  arguments = car(cdr(arguments));
  if (!listp(arguments)) {
    return ERROR_SYNTAX;
  }
  if (function.type == ATOM_TYPE_BUILTIN) {
    return (*function.value.builtin.function)(arguments, result);
  } else if (function.type != ATOM_TYPE_CLOSURE) {
    printf("APPLY: Given function is not a BuiltIn or a closure.\n");
    return ERROR_TYPE;
  }
  // Handle closure.
  // Layout: FUNCTION ARGUMENTS . BODY
  Atom environment = env_create(car(function));
  Atom argument_names = car(cdr(function));
  Atom body = cdr(cdr(function));
  // Bind arguments into local environment.
  while (!nilp(argument_names)) {
    // Handle variadic arguments.
    if (argument_names.type == ATOM_TYPE_SYMBOL) {
      env_set(environment, argument_names, arguments);
      arguments = nil;
      break;
    }
    if (nilp(arguments)) {
      return ERROR_ARGUMENTS;
    }
    env_set(environment, car(argument_names), car(arguments));
    argument_names = cdr(argument_names);
    arguments = cdr(arguments);
  }
  if (!nilp(arguments)) {
    return ERROR_ARGUMENTS;
  }
  // Evaluate body of closure.
  while (!nilp(body)) {
    Error err = evaluate_expression(car(body), environment, result);
    if (err.type) { return err.type; }
    if (user_quit) {
      break;
    }
    body = cdr(body);
  }
  return ERROR_NONE;
}

const char *const builtin_eq_name = "EQ";
const char *const builtin_eq_docstring =
  "(eq A B)\n"
  "\n"
  "Return 'T' iff A and B refer to the same Atomic LISP object.";
int builtin_eq(Atom arguments, Atom *result) {
  assert(ATOM_TYPE_MAX == 9);
  BUILTIN_ENSURE_TWO_ARGUMENTS(arguments);
  *result = compare_atoms(car(arguments), car(cdr(arguments)));
  return ERROR_NONE;
}

const char *const builtin_symbol_table_name = "SYM";
const char *const builtin_symbol_table_docstring =
  "(sym)\n"
  "\n"
  "Return a copy of the global symbol table at the time of calling.";
int builtin_symbol_table(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_NO_ARGUMENTS(arguments);
  *result = symbol_table();
  return ERROR_NONE;
}

const char *const builtin_print_name = "PRINT";
const char *const builtin_print_docstring =
  "(print ARG)\n"
  "\n"
  "Print the given ARG to standard out.";
int builtin_print(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  print_atom(car(arguments));
  fputc('\n', stdout);
  fflush(stdout);
  *result = nil;
  return ERROR_NONE;
}

const char *const builtin_prins_name = "PRINS";
const char *const builtin_prins_docstring =
  "(prins STRING)\n"
  "\n"
  "Print the given STRING to standard out.\n"
  "When FLUSH is non-nil, flush standard out after writing.";
int builtin_prins(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  Atom string = car(arguments);
  if (!stringp(string)) {
    return ERROR_TYPE;
  }
  fputs(string.value.symbol, stdout);
  fflush(stdout);
  *result = nil;
  return ERROR_NONE;
}

const char *const builtin_read_prompted_name = "READ-PROMPTED";
const char *const builtin_read_prompted_docstring =
  "(read-prompted PROMPT)\n"
  "\n"
  "Show the user a PROMPT and return user response as a string.";
// TODO: There has to be better ways to do this...
int builtin_read_prompted(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  Atom prompt = car(arguments);
  if (!stringp(prompt)) {
    return ERROR_TYPE;
  }
  size_t prompt_length = strlen(prompt.value.symbol);

#ifdef LITE_GFX

  // Release all modifiers.
  handle_modifier_up(GUI_MODKEY_MAX);

  // Bind return to 'finish-read', but save old binding of return so it can be restored.
  // TODO: Just make a named keymap and add it to keymaps list in the global position.
  Atom keymap = nil;
  env_get(*genv(), make_sym("KEYMAP"), &keymap);
  Atom original_return_binding = alist_get(keymap, make_string(LITE_KEYSTRING_RETURN));
  alist_set(&keymap, make_string(LITE_KEYSTRING_RETURN), cons(make_sym("FINISH-READ"), nil));
  env_set(*genv(), make_sym("KEYMAP"), keymap);
  env_set(*genv(), make_sym("CURRENT-KEYMAP"), keymap);

  Atom popup_buffer = make_buffer(env_create(nil), ".popup");
  if (!bufferp(popup_buffer) || !popup_buffer.value.buffer) {
    return ERROR_GENERIC;
  }

  // Clear popup buffer.
  popup_buffer.value.buffer->point_byte = 0;
  buffer_remove_bytes_forward(popup_buffer.value.buffer, SIZE_MAX);

  // Insert prompt.
  // TODO+FIXME: Make prompt not editable, somehow :p.
  buffer_insert(popup_buffer.value.buffer, (char *)prompt.value.symbol);

  // Re-enter main loop until input is complete.
  GUIContext *ctx = gui_ctx();
  ctx->reading = 1;
  int open = 1;
  while (open && ctx->reading) {
    open = gui_loop();
  }
  if (!open) {
    exit_safe(0);
  }

  // Remove prompt.
  popup_buffer.value.buffer->point_byte = 0;
  buffer_remove_bytes_forward(popup_buffer.value.buffer, prompt_length);

  // Convert popup buffer contents into a string.
  char *string = buffer_string(*popup_buffer.value.buffer);
  *result = make_string(string);
  free(string);

  // Restore keymap.
  env_get(*genv(), make_sym("KEYMAP"), &keymap);
  alist_set(&keymap, make_string(LITE_KEYSTRING_RETURN), original_return_binding);
  env_set(*genv(), make_sym("KEYMAP"), keymap);

#else /* #ifdef LITE_GFX */

  char *input = readline((char *)prompt.value.symbol);
  if (!input) {
    return ERROR_MEMORY;
  }
  size_t input_length = strlen(input);
  // Trim newline byte(s) from end of input, if present.
  if (input[input_length - 1] == '\n') {
    input_length -= 1;
  }
  if (input[input_length - 1] == '\r') {
    input_length -= 1;
  }
  input[input_length] = '\0';
  *result = make_string(input);
  print_atom(*result);
  free(input);

#endif /* #ifdef LITE_GFX */

  return ERROR_NONE;
}

const char *const builtin_finish_read_name = "FINISH-READ";
const char *const builtin_finish_read_docstring =
  "(finish-read)\n"
  "\n"
  "When reading, complete the read and return the string.";
int builtin_finish_read(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_NO_ARGUMENTS(arguments);
#ifdef LITE_GFX
  gui_ctx()->reading = 0;
#endif /* #ifdef LITE_GFX */
  *result = nil;
  return ERROR_NONE;
}

const char *const builtin_change_font_name = "CHANGE-FONT";
const char *const builtin_change_font_docstring =
  "(change-font FONT-FILENAME POINT-SIZE)\n"
  "\n"
  "Attempt to change font to FONT-FILENAME with POINT-SIZE.";
int builtin_change_font(Atom arguments, Atom *result) {
#ifdef LITE_GFX
  BUILTIN_ENSURE_TWO_ARGUMENTS(arguments);
  Atom font_path = car(arguments);
  if (!stringp(font_path)) {
    return ERROR_TYPE;
  }
  Atom font_size = car(cdr(arguments));
  if (!integerp(font_size)) {
    return ERROR_TYPE;
  }
  *result = nil;
  if (change_font(font_path.value.symbol, font_size.value.integer) == 0) {
    *result = make_sym("T");
  }
#endif
  return ERROR_NONE;
}

const char *const builtin_change_font_size_name = "CHANGE-FONT-SIZE";
const char *const builtin_change_font_size_docstring =
  "(change-font-size POINT-SIZE)\n"
  "\n"
  "Attempt to change the current font's size to POINT-SIZE.";
int builtin_change_font_size(Atom arguments, Atom *result) {
#ifdef LITE_GFX
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  Atom font_size = car(arguments);
  if (!integerp(font_size)) {
    return ERROR_TYPE;
  }
  *result = nil;
  if (change_font_size(font_size.value.integer) == 0) {
    *result = make_sym("T");
  }
#endif
  return ERROR_NONE;
}

const char *const builtin_window_size_name = "WINDOW-SIZE";
const char *const builtin_window_size_docstring =
  "(window-size)\n"
  "\n"
  "Return a pair containing the graphical window's size.";
int builtin_window_size(Atom arguments, Atom *result) {
#ifdef LITE_GFX
  BUILTIN_ENSURE_NO_ARGUMENTS(arguments);
  size_t width = 0;
  size_t height = 0;
  window_size(&width, &height);
  *result = cons(make_int(width), make_int(height));
#endif
  return ERROR_NONE;
}

const char *const builtin_change_window_size_name = "CHANGE-WINDOW-SIZE";
const char *const builtin_change_window_size_docstring =
  "(change-window-size WIDTH HEIGHT)\n"
  "\n"
  "Attempt to change size of window to WIDTH by HEIGHT.";
int builtin_change_window_size(Atom arguments, Atom *result) {
#ifdef LITE_GFX
  BUILTIN_ENSURE_TWO_ARGUMENTS(arguments);
  Atom width = car(arguments);
  Atom height = car(cdr(arguments));
  if (!integerp(width) || !integerp(height)) {
    return ERROR_TYPE;
  }
  *result = make_sym("T");
  change_window_size(width.value.integer, height.value.integer);
#endif
  return ERROR_NONE;
}

// TODO: Accept 'windowed, 'fullscreen, 'maximized symbols as arguments.
const char *const builtin_change_window_mode_name = "CHANGE-WINDOW-MODE";
const char *const builtin_change_window_mode_docstring =
  "(change-window-mode N)\n"
  "\n"
  "If N is 1, set window mode to fullscreen. Otherwise, set it to windowed.";
int builtin_change_window_mode(Atom arguments, Atom *result) {
#ifdef LITE_GFX
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  Atom n = car(arguments);
  if (!integerp(n)) {
    return ERROR_TYPE;
  }
  enum GFXWindowMode mode = GFX_WINDOW_MODE_WINDOWED;
  if (n.value.integer == 1) {
    mode = GFX_WINDOW_MODE_FULLSCREEN;
  }
  change_window_mode(mode);
#endif
  return ERROR_NONE;
}

static Atom get_active_window() {
  Atom window_list = nil;
  Error err = env_get(*genv(), make_sym("WINDOWS"), &window_list);
  if (err.type) {
    print_error(err);
    return nil;
  }

  Atom active_window_index = nil;
  err = env_get(*genv(), make_sym("ACTIVE-WINDOW-INDEX"), &active_window_index);
  if (err.type) {
    active_window_index = make_int(0);
  }

  Atom active_window = list_get_safe(window_list, active_window_index.value.integer);
  return active_window;
}

const char *const builtin_scroll_down_name = "SCROLL-DOWN";
const char *const builtin_scroll_down_docstring =
  "(scroll-down)\n"
  "\n"
  "";
int builtin_scroll_down(Atom arguments, Atom *result) {
  (void)result;
#ifdef LITE_GFX
  // Use the default offset of one, unless a positive integer value was
  // given.
  size_t offset = 1;
  if (!nilp(arguments)) {
    if (!nilp(cdr(arguments))) {
      // Too many arguments (more than one).
      return ERROR_ARGUMENTS;
    }
    if (!integerp(car(arguments))) {
      // Given argument must be an integer.
      return ERROR_TYPE;
    }
    // Only use argument to set offset if it is positive.
    if (car(arguments).value.integer > 0) {
      offset = car(arguments).value.integer;
    }
  }
  Atom active_window = get_active_window();
  // Prevent unsigned integer overflow.
  Atom scrollxy = list_get(active_window, 3);
  integer_t old_vertical_offset = cdr(scrollxy).value.integer;
  cdr(scrollxy).value.integer += offset;
  if (cdr(scrollxy).value.integer < old_vertical_offset) {
    // Restore vertical offset to what it was before overflow.
    cdr(scrollxy).value.integer = old_vertical_offset;
  }
#else
  (void)arguments;
#endif /* #ifdef LITE_GFX */
  return ERROR_NONE;
}

const char *const builtin_scroll_up_name = "SCROLL-UP";
const char *const builtin_scroll_up_docstring =
  "(scroll-up)\n"
  "\n"
  "";
int builtin_scroll_up(Atom arguments, Atom *result) {
  (void)result;
#ifdef LITE_GFX
  // Use the default offset of one, unless a positive integer value was
  // given.
  integer_t offset = 1;
  if (!nilp(arguments)) {
    if (!integerp(car(arguments))) {
      return ERROR_TYPE;
    }
    if (car(arguments).value.integer > 0) {
      offset = car(arguments).value.integer;
    }
  }
  Atom active_window = get_active_window();
  // Prevent unsigned integer underflow.
  Atom scrollxy = list_get(active_window, 3);
  if (offset > cdr(scrollxy).value.integer) {
    offset = cdr(scrollxy).value.integer;
  }
  cdr(scrollxy).value.integer -= offset;
#else
  (void)arguments;
#endif /* #ifdef LITE_GFX */
  return ERROR_NONE;

}


Atom terrible_copy_paste_implementation = { .type      = ATOM_TYPE_STRING,
                                            .value     = { .symbol = "Paste and ye shall recieve." },
                                            .galloc    = NULL,
                                            .docstring = NULL };

const char *const builtin_clipboard_cut_name = "CLIPBOARD-CUT";
const char *const builtin_clipboard_cut_docstring =
  "(clipboard-cut BUFFER)\n"
  "\n"
  "Cut the selected region from BUFFER, if active.\n"
  "The selected region will be removed and placed in the copy and paste buffer.";
int builtin_clipboard_cut(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  *result = nil;
  Atom buffer = car(arguments);
  if (!bufferp(buffer) || !buffer.value.buffer) {
    return ERROR_TYPE;
  }
  if (buffer_mark_active(*buffer.value.buffer)) {
    char *region = buffer_region(*buffer.value.buffer);
    Error err = buffer_remove_region(buffer.value.buffer);
    if (err.type) {
      print_error(err);
      return err.type;
    }
    // TODO: This is a terrible copy and paste implementation!
    terrible_copy_paste_implementation = make_string(region);
#   ifdef LITE_GFX
    set_clipboard_utf8(region);
#   endif
    free(region);
    *result = make_sym("T");
  }
  return ERROR_NONE;
}
const char *const builtin_clipboard_copy_name = "CLIPBOARD-COPY";
const char *const builtin_clipboard_copy_docstring =
  "(clipboard-copy BUFFER)\n"
  "\n"
  "Copy the selected region from BUFFER, if active.";
int builtin_clipboard_copy(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  *result = nil;
  Atom buffer = car(arguments);
  if (!bufferp(buffer) || !buffer.value.buffer) {
    return ERROR_TYPE;
  }
  if (buffer_mark_active(*buffer.value.buffer)) {
    char *region = buffer_region(*buffer.value.buffer);
    // TODO: This is a terrible copy and paste implementation!
    terrible_copy_paste_implementation = make_string(region);
#   ifdef LITE_GFX
    set_clipboard_utf8(region);
#   endif
    free(region);
    *result = make_sym("T");
  }

  return ERROR_NONE;
}
const char *const builtin_clipboard_paste_name = "CLIPBOARD-PASTE";
const char *const builtin_clipboard_paste_docstring =
  "(clipboard-paste BUFFER)\n"
  "\n"
  "Insert the most-recently clipboarded string into the current buffer at point.";
int builtin_clipboard_paste(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  Atom buffer = car(arguments);
  if (!bufferp(buffer) || !buffer.value.buffer) {
    return ERROR_TYPE;
  }
  *result = nil;
  char *to_insert = NULL;
# ifdef LITE_GFX
  char needs_freed = 0;
  if (has_clipboard_utf8()) {
    to_insert = get_clipboard_utf8();
    needs_freed = true;
  } else {
# endif
    if (!stringp(terrible_copy_paste_implementation)) {
      return ERROR_TYPE;
    }
    to_insert = terrible_copy_paste_implementation.value.symbol;
# ifdef LITE_GFX
  }
# endif
  *result = make_sym("T");
  buffer_insert(buffer.value.buffer, to_insert);
# ifdef LITE_GFX
  if (needs_freed) {
    free(to_insert);
  }
# endif
  return ERROR_NONE;
}


//const char *const builtin__name = "LISP-SYMBOL";
//const char *const builtin__docstring =
//  "(lisp-symbol)\n"
//  "\n"
//  "";
//int builtin_(Atom arguments, Atom *result) {
//  return ERROR_TODO;
//}
