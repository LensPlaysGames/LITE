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

#define ARG_ERR(args) do {                             \
    MAKE_ERROR(arg_error, ERROR_ARGUMENTS,             \
              (args), "Invalid arguments",             \
              "Consult the docstring of the builtin"); \
    return arg_error;                                  \
  } while (0)

#define NO_ARGS(builtin_args) do { \
    if (!nilp(builtin_args)) {     \
      ARG_ERR((builtin_args));     \
    }                              \
  } while (0)

#define ONE_ARG(builtin_args) do {                        \
    if (nilp(builtin_args) || !nilp(cdr(builtin_args))) { \
      ARG_ERR((builtin_args));                            \
    }                                                     \
  } while (0)

#define TWO_ARGS(builtin_args) do {       \
    if (nilp(builtin_args)                \
        || nilp(cdr(builtin_args))        \
        || !nilp(cdr(cdr(builtin_args)))) \
      {                                   \
        ARG_ERR((builtin_args));          \
      }                                   \
  } while (0)

#define THREE_ARGS(builtin_args) do {          \
    if (nilp(builtin_args)                     \
        || nilp(cdr(builtin_args))             \
        || nilp(cdr(cdr(builtin_args)))        \
        || !nilp(cdr(cdr(cdr(builtin_args))))) \
      {                                        \
        ARG_ERR((builtin_args));               \
      }                                        \
  } while (0)

#define FOUR_ARGS(builtin_args) do {                 \
    if (nilp(builtin_args)                           \
        || nilp(cdr(builtin_args))                   \
        || nilp(cdr(cdr(builtin_args)))              \
        || nilp(cdr(cdr(cdr(builtin_args))))         \
        || !nilp(cdr(cdr(cdr(cdr(builtin_args))))))  \
      {                                              \
        ARG_ERR((builtin_args));                     \
      }                                              \
  } while (0)

const char *const builtin_quit_lisp_name = "QUIT-LISP";
const char *const builtin_quit_lisp_docstring =
  "(quit-lisp)\n"
  "\n"
  "Stop all evaluation of LISP, and exit all prompts.\n"
  "If you did something on accident, call this.";
Error builtin_quit_lisp(Atom arguments, Atom *result) {
  NO_ARGS(arguments);
  user_quit = 1;
  *result = nil;
  return ok;
}

const char *const builtin_docstring_name = "DOCSTRING";
const char *const builtin_docstring_docstring =
  "(docstring SYMBOL)\n"
  "\n"
  "Return the docstring of SYMBOL within ENVIRONMENT as a string.";
Error builtin_docstring(Atom arguments, Atom *result) {
  TWO_ARGS(arguments);

  Atom environment = car(arguments);
  Atom atom = car(cdr(arguments));

  if (!envp(environment)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               environment,
               "Expected environment to be of environment type. Likely internal error (sorry).",
               NULL);
    return err_type;
  }

  if (symbolp(atom)) {
    Atom symbol_in = atom;
    Error err = env_get(environment, symbol_in, &atom);
    if (err.type) {
      return err;
    }
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
        } else {
          // Could not allocate buffer for new docstring, so just use
          // the regular docstring.
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
  } else {
    if (atom.docstring) {
      *result = make_string(atom.docstring);
    } else {
      *result = nil;
    }
  }
  return ok;
}

static Error typep(Atom arguments, enum AtomType type, Atom *result) {
  ONE_ARG(arguments);
  *result = car(arguments).type == type ? make_sym("T") : nil;
  return ok;
}

const char *const builtin_nilp_name = "NILP";
const char *const builtin_nilp_docstring =
  "(nilp ARG)\n"
  "\n"
  "Return 'T' iff ARG has a type of 'NIL', otherwise return nil.";
Error builtin_nilp(Atom arguments, Atom *result) {
  // Special argument checking because nil is a valid argument.
  if (!nilp(cdr(arguments))) {
    ARG_ERR(arguments);
  }
  *result = car(arguments).type == ATOM_TYPE_NIL ? make_sym("T") : nil;
  return ok;
}

const char *const builtin_pairp_name = "PAIRP";
const char *const builtin_pairp_docstring =
  "(pairp ARG)\n"
  "\n"
  "Return 'T' iff ARG has a type of 'PAIR', otherwise return nil.";
Error builtin_pairp(Atom arguments, Atom *result) {
  return typep(arguments, ATOM_TYPE_PAIR, result);
}

const char *const builtin_symbolp_name = "SYMBOLP";
const char *const builtin_symbolp_docstring =
  "(symbolp ARG)\n"
  "\n"
  "Return 'T' iff ARG has a type of 'SYMBOL', otherwise return nil.";
Error builtin_symbolp(Atom arguments, Atom *result) {
  return typep(arguments, ATOM_TYPE_SYMBOL, result);
}

const char *const builtin_integerp_name = "INTEGERP";
const char *const builtin_integerp_docstring =
  "(integerp ARG)\n"
  "\n"
  "Return 'T' iff ARG has a type of 'INTEGER', otherwise return nil.";
Error builtin_integerp(Atom arguments, Atom *result) {
  return typep(arguments, ATOM_TYPE_INTEGER, result);
}

const char *const builtin_builtinp_name = "BUILTINP";
const char *const builtin_builtinp_docstring =
  "(builtinp ARG)\n"
  "\n"
  "Return 'T' iff ARG has a type of 'BUILTIN', otherwise return nil.";
Error builtin_builtinp(Atom arguments, Atom *result) {
  return typep(arguments, ATOM_TYPE_BUILTIN, result);
}

const char *const builtin_closurep_name = "CLOSUREP";
const char *const builtin_closurep_docstring =
  "(closurep ARG)\n"
  "\n"
  "Return 'T' iff ARG has a type of 'CLOSURE', otherwise return nil.";
Error builtin_closurep(Atom arguments, Atom *result) {
  return typep(arguments, ATOM_TYPE_CLOSURE, result);
}

const char *const builtin_macrop_name = "MACROP";
const char *const builtin_macrop_docstring =
  "(macrop ARG)\n"
  "\n"
  "Return 'T' iff ARG has a type of 'MACRO', otherwise return nil.";
Error builtin_macrop(Atom arguments, Atom *result) {
  return typep(arguments, ATOM_TYPE_MACRO, result);
}

const char *const builtin_stringp_name = "STRINGP";
const char *const builtin_stringp_docstring =
  "(stringp ARG)\n"
  "\n"
  "Return 'T' iff ARG has a type of 'STRING', otherwise return nil.";
Error builtin_stringp(Atom arguments, Atom *result) {
  return typep(arguments, ATOM_TYPE_STRING, result);
}

const char *const builtin_bufferp_name = "BUFFERP";
const char *const builtin_bufferp_docstring =
  "(bufferp ARG)\n"
  "\n"
  "Return 'T' iff ARG has a type of 'BUFFER', otherwise return nil.";
Error builtin_bufferp(Atom arguments, Atom *result) {
  return typep(arguments, ATOM_TYPE_BUFFER, result);
}

const char *const builtin_envp_name = "ENVP";
const char *const builtin_envp_docstring =
  "(envp ARG)\n"
  "\n"
  "Return 'T' iff ARG has a type of 'ENVIRONMENT', otherwise return nil.";
Error builtin_envp(Atom arguments, Atom *result) {
  return typep(arguments, ATOM_TYPE_ENVIRONMENT, result);
}


const char *const builtin_not_name = "!";
const char *const builtin_not_docstring =
  "(! ARG)\n"
  "\n"
  "Given ARG is nil, return 'T', otherwise return nil.";
Error builtin_not(Atom arguments, Atom *result) {
  ONE_ARG(arguments);
  *result = nilp(car(arguments)) ? make_sym("T") : nil;
  return ok;
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
Error builtin_car(Atom arguments, Atom *result) {
  ONE_ARG(arguments);
  // (car nil) == nil
  if (nilp(car(arguments))) {
    *result = nil;
  } else if (car(arguments).type != ATOM_TYPE_PAIR) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               car(arguments),
               "CAR requires that the argument be a pair",
               NULL);
    return err_type;
  } else {
    *result = car(car(arguments));
  }
  return ok;
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
Error builtin_cdr(Atom arguments, Atom *result) {
  ONE_ARG(arguments);
  if (nilp(car(arguments))) {
    *result = nil;
  } else if (car(arguments).type != ATOM_TYPE_PAIR) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               car(arguments),
               "CDR requires that the argument be a pair",
               NULL);
    return err_type;
  } else {
    *result = cdr(car(arguments));
  }
  return ok;
}

const char *const builtin_cons_name = "CONS";
const char *const builtin_cons_docstring =
  "(cons LEFT RIGHT)\n"
  "\n"
  "Return a new pair, with LEFT and RIGHT on each side, respectively.";
Error builtin_cons(Atom arguments, Atom *result) {
  TWO_ARGS(arguments);
  *result = cons(car(arguments), car(cdr(arguments)));
  return ok;
}

const char *const builtin_setcar_name = "SETCAR";
const char *const builtin_setcar_docstring =
  "(setcar PAIR VALUE)\n"
  "\n"
  "Set the left side of PAIR to the given VALUE.";
Error builtin_setcar(Atom arguments, Atom *result) {
  TWO_ARGS(arguments);
  if (!pairp(car(arguments))) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               car(arguments),
               "SETCAR requires that the first argument be a pair",
               NULL);
    return err_type;
  }
  car(car(arguments)) = car(cdr(arguments));
  *result = nil;
  return ok;
}

const char *const builtin_setcdr_name = "SETCDR";
const char *const builtin_setcdr_docstring =
  "(setcdr PAIR VALUE)\n"
  "\n"
  "Set the right side of PAIR to the given VALUE.";
Error builtin_setcdr(Atom arguments, Atom *result) {
  TWO_ARGS(arguments);
  if (!pairp(car(arguments))) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               car(arguments),
               "SETCDR requires that the argument be a pair",
               NULL);
    return err_type;
  }
  cdr(car(arguments)) = car(cdr(arguments));
  *result = nil;
  return ok;
}

const char *const builtin_member_name = "MEMBER";
const char *const builtin_member_docstring =
  "(member ELEMENT LIST)\n"
  "\n"
  "Return non-nil iff ELEMENT is an element of LIST.";
Error builtin_member(Atom arguments, Atom *result) {
  TWO_ARGS(arguments);
  Atom key = car(arguments);
  Atom list = car(cdr(arguments));
  *result = nil;
  for (; !nilp(list); list = cdr(list)) {
    Atom equal = compare_atoms(key, car(list));
    if (!nilp(equal)) { *result = equal; }
  }
  return ok;
}

const char *const builtin_length_name = "LENGTH";
const char *const builtin_length_docstring =
  "(length SEQUENCE)\n"
  "\n"
  "Return the length of SEQUENCE, if it is a string, symbol, or list.\n"
  "Otherwise, return nil.\n";
Error builtin_length(Atom arguments, Atom *result) {
  ONE_ARG(arguments);
  Atom arg = car(arguments);
  size_t size = 0;
  if (pairp(arg)) {
    for (; !nilp(arg); arg = cdr(arg)) {
      ++size;
    }
  } else if (symbolp(arg) || stringp(arg)) {
    size = strlen(arg.value.symbol);
  } else {
    // TODO: Type error?
    *result = nil;
    return ok;
  }
  *result = make_int((integer_t)size);
  return ok;
}

const char *const builtin_add_name = "+";
const char *const builtin_add_docstring =
  "(+ A B)\n"
  "\n"
  "Add two integer numbers A and B together, and return the computed result.";
Error builtin_add(Atom arguments, Atom *result) {
  TWO_ARGS(arguments);
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (!integerp(lhs) || !integerp(rhs)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "+ requires that both of the given arguments be integers",
               NULL);
    return err_type;
  }
  *result = make_int(lhs.value.integer + rhs.value.integer);
  return ok;
}

const char *const builtin_subtract_name = "-";
const char *const builtin_subtract_docstring =
  "(- A B)\n"
  "\n"
  "Subtract integer B from integer A and return the computed result.";
Error builtin_subtract(Atom arguments, Atom *result) {
  TWO_ARGS(arguments);
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (!integerp(lhs) || !integerp(rhs)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "- requires that both of the given arguments be integers",
               NULL);
    return err_type;
  }
  *result = make_int(lhs.value.integer - rhs.value.integer);
  return ok;
}

const char *const builtin_multiply_name = "*";
const char *const builtin_multiply_docstring =
  "(* A B)\n"
  "\n"
  "Multiply integer numbers A and B together and return the computed result.";
Error builtin_multiply(Atom arguments, Atom *result) {
  TWO_ARGS(arguments);
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (!integerp(lhs) || !integerp(rhs)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "* requires that both of the given arguments be integers",
               NULL);
    return err_type;
  }
  *result = make_int(lhs.value.integer * rhs.value.integer);
  return ok;
}

const char *const builtin_divide_name = "/";
const char *const builtin_divide_docstring =
  "(/ A B)\n"
  "\n"
  "Divide integer B out of integer A and return the computed result.\n"
  "`(/ 6 3)` == \"6 / 3\" == 2";
Error builtin_divide(Atom arguments, Atom *result) {
  TWO_ARGS(arguments);
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (!integerp(lhs) || !integerp(rhs)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "/ requires that both of the given arguments be integers",
               NULL);
    return err_type;
  }
  if (rhs.value.integer == 0) {
    MAKE_ERROR(div_by_zero_err, ERROR_GENERIC,
               arguments,
               "Never, in any circumstances, may one divide by zero.",
               NULL);
    return div_by_zero_err;
  }
  *result = make_int(lhs.value.integer / rhs.value.integer);
  return ok;
}

const char *const builtin_remainder_name = "%";
const char *const builtin_remainder_docstring =
  "(% N M)\n"
  "\n"
  "Return the remainder left when N is divided by M.";
Error builtin_remainder(Atom arguments, Atom *result) {
  TWO_ARGS(arguments);
  Atom n = car(arguments);
  Atom m = car(cdr(arguments));
  if (!integerp(n) || !integerp(m)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "% requires that both of the given arguments be integers",
               NULL);
    return err_type;
  }
  *result = make_int(n.value.integer % m.value.integer);
  return ok;
}

const char *const builtin_bitand_name = "BITAND";
const char *const builtin_bitand_docstring =
  "(bitand LHS RHS)\n"
  "\n"
  "Given two integers, return their bitwise and as if they were two's-complement.";
Error builtin_bitand(Atom arguments, Atom *result) {
  TWO_ARGS(arguments);
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (!integerp(lhs) || !integerp(rhs)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BITAND requires that both of the given arguments be integers",
               NULL);
    return err_type;
  }
  *result = make_int(lhs.value.integer & rhs.value.integer);
  return ok;
}

const char *const builtin_bitor_name = "BITOR";
const char *const builtin_bitor_docstring =
  "(bitor LHS RHS)\n"
  "\n"
  "Given two integers, return their bitwise or as if they were two's-complement.";
Error builtin_bitor(Atom arguments, Atom *result) {
  TWO_ARGS(arguments);
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (!integerp(lhs) || !integerp(rhs)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BITOR requires that both of the given arguments be integers",
               NULL);
    return err_type;
  }
  *result = make_int(lhs.value.integer | rhs.value.integer);
  return ok;
}

const char *const builtin_bitxor_name = "BITXOR";
const char *const builtin_bitxor_docstring =
  "(bitxor LHS RHS)\n"
  "\n"
  "Given two integers, return their bitwise exlusive or as if they were two's-complement.";
Error builtin_bitxor(Atom arguments, Atom *result) {
  TWO_ARGS(arguments);
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (!integerp(lhs) || !integerp(rhs)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BITXOR requires that both of the given arguments be integers",
               NULL);
    return err_type;
  }
  *result = make_int(lhs.value.integer ^ rhs.value.integer);
  return ok;
}

const char *const builtin_bitnot_name = "BITNOT";
const char *const builtin_bitnot_docstring =
  "(bitnot OPERAND)\n"
  "\n"
  "Given an integer, return the bitwise NOT as if it was two's complement.";
Error builtin_bitnot(Atom arguments, Atom *result) {
  ONE_ARG(arguments);
  Atom op = car(arguments);
  if (!integerp(op)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BITNOT requires that both of the given arguments be integers",
               NULL);
    return err_type;
  }
  *result = make_int(~op.value.integer);
  return ok;
}

const char *const builtin_bitshl_name = "BITSHL";
const char *const builtin_bitshl_docstring =
  "(bitshl LHS RHS)\n"
  "\n"
  "Given two integers, return LHS shifted to the left by RHS bits.";
Error builtin_bitshl(Atom arguments, Atom *result) {
  TWO_ARGS(arguments);
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (!integerp(lhs) || !integerp(rhs)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BITSHL requires that both of the given arguments be integers",
               NULL);
    return err_type;
  }
  *result = make_int(lhs.value.integer << rhs.value.integer);
  return ok;
}

const char *const builtin_bitshr_name = "BITSHR";
const char *const builtin_bitshr_docstring =
  "(bitshr LHS RHS)\n"
  "\n"
  "Given two integers, return LHS shifted to the right by RHS bits.";
Error builtin_bitshr(Atom arguments, Atom *result) {
  TWO_ARGS(arguments);
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (!integerp(lhs) || !integerp(rhs)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BITSHR requires that both of the given arguments be integers",
               NULL);
    return err_type;
  }
  *result = make_int(lhs.value.integer >> rhs.value.integer);
  return ok;
}

const char *const builtin_buffer_toggle_mark_name = "BUFFER-TOGGLE-MARK";
const char *const builtin_buffer_toggle_mark_docstring =
  "(buffer-toggle-mark BUFFER)\n\nToggle mark activation on the given buffer.";
Error builtin_buffer_toggle_mark(Atom arguments, Atom *result) {
  ONE_ARG(arguments);
  Atom buffer = car(arguments);
  if (!bufferp(buffer)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BUFFER-TOGGLE-MARK requires a single buffer argument",
               NULL);
    return err_type;
  }
  buffer_toggle_mark(buffer.value.buffer);
  *result = nil;
  if (buffer_mark_active(*buffer.value.buffer)) {
    *result = make_sym("T");
  }
  return ok;
}

const char *const builtin_buffer_set_mark_activation_name = "BUFFER-SET-MARK-ACTIVATION";
const char *const builtin_buffer_set_mark_activation_docstring =
  "(buffer-set-mark-activation BUFFER STATE)"
  "\n"
  "Set activation state of mark in BUFFER based on STATE being non-nil or not.";
Error builtin_buffer_set_mark_activation(Atom arguments, Atom *result) {
  // TODO: Can you actually pass nil, or does TWO_ARGS see that as an error?
  TWO_ARGS(arguments);
  Atom buffer = car(arguments);
  Atom state = car(cdr(arguments));
  if (!bufferp(buffer)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BUFFER-SET-MARK-ACTIVATION requires the first argument to be a buffer",
               NULL);
    return err_type;
  }
  Error err = buffer_set_mark_activation(buffer.value.buffer, !nilp(state));
  if (err.type) {
    return err;
  }
  *result = state;
  return ok;
}

const char *const builtin_buffer_set_mark_name = "BUFFER-SET-MARK";
const char *const builtin_buffer_set_mark_docstring =
  "(buffer-set-mark BUFFER MARK)\n\nSet marked byte in BUFFER to MARK.";
Error builtin_buffer_set_mark(Atom arguments, Atom *result) {
  TWO_ARGS(arguments);
  Atom buffer = car(arguments);
  Atom mark = car(cdr(arguments));
  if (!bufferp(buffer)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BUFFER-SET-MARK requires the first argument be a buffer",
               NULL);
    return err_type;
  }
  if (!integerp(mark)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BUFFER-SET-MARK requires the second argument be an integer",
               NULL);
    return err_type;
  }
  buffer_set_mark(buffer.value.buffer, (size_t)mark.value.integer);
  *result = nil;
  if (buffer_mark_active(*buffer.value.buffer)) {
    *result = make_sym("T");
  }
  return ok;
}

const char *const builtin_buffer_mark_name = "BUFFER-MARK";
const char *const builtin_buffer_mark_docstring =
  "(buffer-mark BUFFER)\n\nReturn byte offset of mark in BUFFER.";
Error builtin_buffer_mark(Atom arguments, Atom *result) {
  ONE_ARG(arguments);
  Atom buffer = car(arguments);
  if (!bufferp(buffer)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BUFFER-MARK requires a single buffer argument",
               NULL);
    return err_type;
  }
  *result = make_int((integer_t)buffer_mark(*buffer.value.buffer));
  return ok;
}

const char *const builtin_buffer_mark_activated_name = "BUFFER-MARK-ACTIVE";
const char *const builtin_buffer_mark_activated_docstring =
  "(buffer-mark-active BUFFER)\n"
  "\n"
  "Return T iff mark is active in BUFFER. Otherwise, return nil.";
Error builtin_buffer_mark_activated(Atom arguments, Atom *result) {
  ONE_ARG(arguments);
  Atom buffer = car(arguments);
  if (!bufferp(buffer)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BUFFER-MARK-ACTIVE requires a single buffer argument",
               NULL);
    return err_type;
  }
  *result = nil;
  if (buffer_mark_active(*buffer.value.buffer)) {
    *result = make_sym("T");
  }
  return ok;
}

const char *const builtin_buffer_region_name = "BUFFER-REGION";
const char *const builtin_buffer_region_docstring =
  "(buffer-region BUFFER)\n\nReturn the region between mark and point in BUFFER.";
Error builtin_buffer_region(Atom arguments, Atom *result) {
  ONE_ARG(arguments);
  Atom buffer = car(arguments);
  if (!bufferp(buffer)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BUFFER-REGION requires a single buffer argument",
               NULL);
    return err_type;
  }
  char *region = buffer_region(*buffer.value.buffer);
  if (!region) {
    MAKE_ERROR(err_type, ERROR_GENERIC,
               arguments,
               "buffer_region() returned NULL",
               NULL);
    return err_type;
  }
  *result = make_string(region);
  free(region);
  return ok;
}

const char *const builtin_buffer_region_length_name = "BUFFER-REGION-LENGTH";
const char *const builtin_buffer_region_length_docstring =
  "(buffer-region-length BUFFER)\n"
  "\n"
  "Get byte difference between mark and point within BUFFER; "
  "the byte length of the selected region";
Error builtin_buffer_region_length(Atom arguments, Atom *result) {
  ONE_ARG(arguments);
  Atom buffer = car(arguments);
  if (!bufferp(buffer)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BUFFER-REGION-LENGTH requires a single buffer argument",
               NULL);
    return err_type;
  }
  *result = make_int((integer_t)buffer_region_length(*buffer.value.buffer));
  return ok;
}

const char *const builtin_open_buffer_name = "OPEN-BUFFER";
const char *const builtin_open_buffer_docstring =
  "(open-buffer PATH)\n\nReturn a buffer visiting PATH.\n"
  "The following places are searched for files.\n"
  "1. If file exists at PATH, open buffer at PATH.\n"
  "2. Try PATH relative to current buffer path.\n"
  "3. Try PATH relative to current working directory.";
Error builtin_open_buffer(Atom arguments, Atom *result) {
  ONE_ARG(arguments);
  Atom path = car(arguments);
  if (!stringp(path)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "OPEN-BUFFER requires a single string argument; a filepath",
               NULL);
    return err_type;
  }
  if (file_exists(path.value.symbol)) {
    // TODO: A buffer-local environment should go in each call to
    // make_buffer, or something?
    *result = make_buffer(env_create(nil, 0), (char *)path.value.symbol);
    return ok;
  }

  char *working_path = NULL;

  // Attempt to load relative to path of current buffer.
  Atom current_buffer = nil;
  Error err = env_get(*genv(), make_sym("CURRENT-BUFFER"), &current_buffer);
  if (!err.type && bufferp(current_buffer)) {
    working_path = string_trijoin(current_buffer.value.buffer->path, "/../", path.value.symbol);
    if (file_exists(working_path)) {
      *result = make_buffer(env_create(nil, 0), working_path);
      free(working_path);
      return ok;
    }
    free(working_path);
    working_path = string_trijoin(current_buffer.value.buffer->path, "/", path.value.symbol);
    if (file_exists(working_path)) {
      *result = make_buffer(env_create(nil, 0), working_path);
      free(working_path);
      return ok;
    }
  }

  char *cwd = get_working_dir();
  if (cwd) {
    working_path = string_trijoin(cwd, "/", path.value.symbol);
    free(cwd);
    if (file_exists(working_path)) {
      *result = make_buffer(env_create(nil, 0), working_path);
      free(working_path);
      return ok;
    }
  }

  // Attempt to load from lite application data directory.
  char *litedir = getlitedir();
  if (litedir) {
    working_path = string_trijoin(litedir, "/", path.value.symbol);
    free(litedir);
    if (file_exists(working_path)) {
      *result = make_buffer(env_create(nil, 0), working_path);
      free(working_path);
      return ok;
    }
  }

  free(working_path);

  // If no existing file was found, just make a buffer at a new file.
  *result = make_buffer(env_create(nil, 0), (char *)path.value.symbol);
  return ok;
}

const char *const builtin_buffer_path_name = "BUFFER-PATH";
const char *const builtin_buffer_path_docstring =
  "(buffer-path BUFFER)\n\nReturn buffer's path as a string.";
Error builtin_buffer_path(Atom arguments, Atom *result) {
  ONE_ARG(arguments);
  Atom buffer = car(arguments);
  if (!bufferp(buffer)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BUFFER-PATH requires a single buffer argument",
               NULL);
    return err_type;
  }
  *result = make_string(buffer.value.buffer->path);
  return ok;
}

const char *const builtin_buffer_table_name = "BUF";
const char *const builtin_buffer_table_docstring =
  "(buf)\n\nReturn the LISP buffer table.";
Error builtin_buffer_table(Atom arguments, Atom *result) {
  NO_ARGS(arguments);
  *result = *buf_table();
  return ok;
}

const char *const builtin_buffer_insert_name = "BUFFER-INSERT";
const char *const builtin_buffer_insert_docstring =
  "(buffer-insert BUFFER STRING) \n"
  "\n"
  "Insert STRING into BUFFER at point.";
Error builtin_buffer_insert(Atom arguments, Atom *result) {
  TWO_ARGS(arguments);
  Atom buffer = car(arguments);
  Atom string = car(cdr(arguments));
  if (!bufferp(buffer)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BUFFER-INSERT requires that the first argument be a buffer",
               NULL);
    return err_type;
  }
  if (!stringp(string)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BUFFER-INSERT requires that the second argument be a string",
               NULL);
    return err_type;
  }
  Error err = buffer_insert(buffer.value.buffer, (char *)string.value.symbol);
  if (err.type) {
    return err;
  }
  *result = buffer;
  return ok;
}

const char *const builtin_buffer_remove_name = "BUFFER-REMOVE";
const char *const builtin_buffer_remove_docstring =
  "(buffer-remove BUFFER COUNT) \n"
  "\n"
  "Backspace COUNT bytes from BUFFER at point.";
Error builtin_buffer_remove(Atom arguments, Atom *result) {
  TWO_ARGS(arguments);
  Atom buffer = car(arguments);
  Atom count = car(cdr(arguments));
  if (!bufferp(buffer)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BUFFER-REMOVE requires that the first argument be a buffer",
               NULL);
    return err_type;
  }
  if (!integerp(count)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BUFFER-REMOVE requires that the second argument be an integer",
               NULL);
    return err_type;
  }
  Error err = buffer_remove_bytes(buffer.value.buffer, (size_t)count.value.integer);
  if (err.type) {
    return err;
  }
  *result = buffer;
  return ok;
}

const char *const builtin_buffer_remove_forward_name = "BUFFER-REMOVE-FORWARD";
const char *const builtin_buffer_remove_forward_docstring =
  "(buffer-remove-forward BUFFER COUNT) \n"
  "\n"
  "Remove COUNT bytes from BUFFER following point.";
Error builtin_buffer_remove_forward(Atom arguments, Atom *result) {
  TWO_ARGS(arguments);
  Atom buffer = car(arguments);
  Atom count = car(cdr(arguments));
  if (!bufferp(buffer)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BUFFER-REMOVE-FORWARD requires that the first argument be a buffer",
               NULL);
    return err_type;
  }
  if (!integerp(count)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BUFFER-REMOVE-FORWARD requires that the second argument be an integer",
               NULL);
    return err_type;
  }
  Error err = buffer_remove_bytes_forward(buffer.value.buffer, (size_t)count.value.integer);
  if (err.type) {
    return err;
  }
  *result = buffer;
  return ok;
}

const char *const builtin_buffer_undo_name = "BUFFER-UNDO";
const char *const builtin_buffer_undo_docstring =
  "(buffer-undo BUFFER)";
Error builtin_buffer_undo(Atom arguments, Atom *result) {
  (void)result;
  ONE_ARG(arguments);
  Atom buffer = car(arguments);
  if (!bufferp(buffer)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BUFFER-UNDO requires a single buffer argument",
               NULL);
    return err_type;
  }
  Error err = buffer_undo(buffer.value.buffer);
  if (err.type) {
    return err;
  }
  return ok;
}
const char *const builtin_buffer_redo_name = "BUFFER-REDO";
const char *const builtin_buffer_redo_docstring =
  "(buffer-redo BUFFER)";
Error builtin_buffer_redo(Atom arguments, Atom *result) {
  (void)result;
  ONE_ARG(arguments);
  Atom buffer = car(arguments);
  if (!bufferp(buffer)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BUFFER-REDO requires a single buffer argument",
               NULL);
    return err_type;
  }
  Error err = buffer_redo(buffer.value.buffer);
  if (err.type) {
    return err;
  }
  return ok;
}

const char *const builtin_buffer_set_point_name = "BUFFER-SET-POINT";
const char *const builtin_buffer_set_point_docstring =
  "(buffer-set-point BUFFER POINT) \n"
  "\n"
  "Set byte offset of cursor within BUFFER to POINT.";
Error builtin_buffer_set_point(Atom arguments, Atom *result) {
  TWO_ARGS(arguments);
  Atom buffer = car(arguments);
  Atom point = car(cdr(arguments));
  if (!bufferp(buffer)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BUFFER-SET-POINT requires that the first argument be a buffer",
               NULL);
    return err_type;
  }
  if (!integerp(point)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BUFFER-SET-POINT requires that the second argument be an integer",
               NULL);
    return err_type;
  }
  size_t new_point_byte = 0;
  if (point.value.integer > 0) {
    if (point.value.integer > (integer_t)buffer.value.buffer->rope->weight) {
      new_point_byte = buffer.value.buffer->rope->weight;
    } else {
      new_point_byte = (size_t)point.value.integer;
    }
  }
  buffer.value.buffer->point_byte = new_point_byte;
  *result = buffer;
  return ok;
}

const char *const builtin_buffer_point_name = "BUFFER-POINT";
const char *const builtin_buffer_point_docstring =
  "(buffer-point BUFFER) \n"
  "\n"
  "Get byte offset of cursor (point) within BUFFER.";
Error builtin_buffer_point (Atom arguments, Atom *result) {
  ONE_ARG(arguments);
  Atom buffer = car(arguments);
  if (!bufferp(buffer)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BUFFER-POINT requires a single buffer argument",
               NULL);
    return err_type;
  }
  *result = make_int((integer_t)buffer.value.buffer->point_byte);
  return ok;
}

const char *const builtin_buffer_index_name = "BUFFER-INDEX";
const char *const builtin_buffer_index_docstring =
  "(buffer-index BUFFER INDEX)\n"
  "\n"
  "Get character from BUFFER at INDEX";
Error builtin_buffer_index (Atom arguments, Atom *result) {
  TWO_ARGS(arguments);
  Atom buffer = car(arguments);
  Atom index = car(cdr(arguments));
  if (!bufferp(buffer)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BUFFER-INDEX requires that the first argument be a buffer",
               NULL);
    return err_type;
  }
  if (!integerp(index)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BUFFER-INDEX requires that the second argument be an integer",
               NULL);
    return err_type;
  }
  char one_byte_string[2];
  one_byte_string[0] = rope_index(buffer.value.buffer->rope
                                  , (size_t)index.value.integer);
  one_byte_string[1] = '\0';
  *result = make_string(&one_byte_string[0]);
  return ok;
}

const char *const builtin_buffer_string_name = "BUFFER-STRING";
const char *const builtin_buffer_string_docstring =
  "(buffer-string BUFFER)\n"
  "\n"
  "Get the contents of BUFFER as a string. "
  "Be careful with large files.";
Error builtin_buffer_string(Atom arguments, Atom *result) {
  ONE_ARG(arguments);
  Atom buffer = car(arguments);
  if (!bufferp(buffer)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BUFFER-STRING requires a single buffer argument",
               NULL);
    return err_type;
  }
  char *contents = buffer_string(*buffer.value.buffer);
  if (!contents) {
    MAKE_ERROR(err, ERROR_GENERIC,
               nil, "buffer_string() returned NULL!",
               NULL);
    return err;
  }
  *result = make_string(contents);
  free(contents);
  return ok;
}

const char *const builtin_buffer_lines_name = "BUFFER-LINES";
const char *const builtin_buffer_lines_docstring =
  "(buffer-lines BUFFER START-LINE LINE-COUNT)\n"
  "\n"
  "Get LINE-COUNT lines starting at START-LINE within BUFFER.";
Error builtin_buffer_lines (Atom arguments, Atom *result) {
  THREE_ARGS(arguments);
  Atom buffer = car(arguments);
  if (!bufferp(buffer)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BUFFER-LINES requires that the first argument be a buffer",
               NULL);
    return err_type;
  }
  Atom start_line = car(cdr(arguments));
  if (!integerp(start_line)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BUFFER-LINES requires that the second argument be an integer",
               NULL);
    return err_type;
  }
  Atom line_count = car(cdr(cdr(arguments)));
  if (!integerp(line_count)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BUFFER-LINES requires that the third argument be an integer",
               NULL);
    return err_type;
  }
  char *lines = buffer_lines
    (*buffer.value.buffer
     , (size_t)start_line.value.integer
     , (size_t)line_count.value.integer);
  *result = make_string(lines);
  free(lines);
  return ok;
}

const char *const builtin_buffer_line_name = "BUFFER-LINE";
const char *const builtin_buffer_line_docstring =
  "(buffer-line BUFFER LINE-NUMBER)\n"
  "\n"
  "Get line LINE-NUMBER from BUFFER contents as string.";
Error builtin_buffer_line  (Atom arguments, Atom *result) {
  TWO_ARGS(arguments);
  Atom buffer = car(arguments);
  if (!bufferp(buffer)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BUFFER-LINE requires that the first argument be a buffer",
               NULL);
    return err_type;
  }
  Atom line_count = car(cdr(arguments));
  if (!integerp(line_count)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BUFFER-LINE requires that the second argument be an integer",
               NULL);
    return err_type;
  }
  char *line = buffer_line(*buffer.value.buffer, (size_t)line_count.value.integer);
  *result = make_string(line);
  free(line);
  return ok;
}

const char *const builtin_buffer_current_line_name = "BUFFER-CURRENT-LINE";
const char *const builtin_buffer_current_line_docstring =
  "(buffer-current-line BUFFER)\n"
  "\n"
  "Get line surrounding point in BUFFER as string.";
Error builtin_buffer_current_line(Atom arguments, Atom *result) {
  ONE_ARG(arguments);
  Atom buffer = car(arguments);
  if (!bufferp(buffer)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "BUFFER-CURRENT-LINE requires a single buffer argument",
               NULL);
    return err_type;
  }
  char *line = buffer_current_line(*buffer.value.buffer);
  *result = make_string(line);
  free(line);
  return ok;
}

const char *const builtin_numeq_name = "=";
const char *const builtin_numeq_docstring =
  "(= ARG1 ARG2)\n"
  "\n"
  "Return 'T' iff the two given arguments have the same integer value.";
Error builtin_numeq(Atom arguments, Atom *result) {
  TWO_ARGS(arguments);
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (!integerp(lhs) || !integerp(rhs)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "= requires that both of the given arguments be integers",
               "Use 'eq' to compare equality of atoms regardless of type.");
    return err_type;
  }
  *result = lhs.value.integer == rhs.value.integer ? make_sym("T") : nil;
  return ok;
}

const char *const builtin_numnoteq_name = "!=";
const char *const builtin_numnoteq_docstring =
  "(!= ARG1 ARG2) \n"
  "\n"
  "Return 'T' iff the two given arguments *do not* have the same integer value.";
Error builtin_numnoteq(Atom arguments, Atom *result) {
  TWO_ARGS(arguments);
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (!integerp(lhs) || !integerp(rhs)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "!= requires that both of the given arguments be integers",
               "Use 'eq' to compare equality of atoms regardless of type.");
    return err_type;
  }
  *result = lhs.value.integer != rhs.value.integer ? make_sym("T") : nil;
  return ok;
}

const char *const builtin_numlt_name = "<";
const char *const builtin_numlt_docstring =
  "(< INT-A INT-B)\n"
  "\n"
  "Return 'T' iff integer A is less than integer B.";
Error builtin_numlt(Atom arguments, Atom *result) {
  TWO_ARGS(arguments);
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (!integerp(lhs) || !integerp(rhs)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "< requires that both of the given arguments be integers",
               NULL);
    return err_type;
  }
  *result = lhs.value.integer < rhs.value.integer ? make_sym("T") : nil;
  return ok;
}

const char *const builtin_numlt_or_eq_name = "<=";
const char *const builtin_numlt_or_eq_docstring =
  "(<= INT-A INT-B)\n"
  "\n"
  "Return 'T' iff integer A is less than or equal to integer B.";
Error builtin_numlt_or_eq(Atom arguments, Atom *result) {
  TWO_ARGS(arguments);
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (!integerp(lhs) || !integerp(rhs)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "<= requires that both of the given arguments be integers",
               NULL);
    return err_type;
  }
  *result = lhs.value.integer <= rhs.value.integer ? make_sym("T") : nil;
  return ok;
}

const char *const builtin_numgt_name = ">";
const char *const builtin_numgt_docstring =
  "(> INT-A INT-B)\n"
  "\n"
  "Return 'T' iff integer A is greater than integer B.";
Error builtin_numgt(Atom arguments, Atom *result) {
  TWO_ARGS(arguments);
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (!integerp(lhs) || !integerp(rhs)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "> requires that both of the given arguments be integers",
               NULL);
    return err_type;
  }
  *result = lhs.value.integer > rhs.value.integer ? make_sym("T") : nil;
  return ok;
}

const char *const builtin_numgt_or_eq_name = ">=";
const char *const builtin_numgt_or_eq_docstring =
  "(>= INT-A INT-B)\n"
  "\n"
  "Return 'T' iff integer A is greater than or equal to integer B.";
Error builtin_numgt_or_eq(Atom arguments, Atom *result) {
  TWO_ARGS(arguments);
  Atom lhs = car(arguments);
  Atom rhs = car(cdr(arguments));
  if (!integerp(lhs) || !integerp(rhs)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               ">= requires that both of the given arguments be integers",
               NULL);
    return err_type;
  }
  *result = lhs.value.integer >= rhs.value.integer ? make_sym("T") : nil;
  return ok;
}

const char *const builtin_buffer_seek_byte_name = "BUFFER-SEEK-BYTE";
const char *const builtin_buffer_seek_byte_docstring =
  "(buffer-seek-byte BUFFER BYTES DIRECTION)\n"
  "\n"
  "Move buffer point to the next byte that is within the control string.\n"
  "If no matching bytes are found, don't move point_byte.\n"
  "Returns the amount of bytes buffer's point was moved.";
Error builtin_buffer_seek_byte(Atom arguments, Atom *result) {
  THREE_ARGS(arguments);
  Atom buffer = car(arguments);
  Atom bytes = car(cdr(arguments));
  Atom direction = car(cdr(cdr(arguments)));
  if (!bufferp(buffer)) {
    MAKE_ERROR(err, ERROR_TYPE,
               arguments,
               "BUFFER-SEEK-BYTE requires that the first argument be a buffer",
               NULL);
    return err;
  }
  if (!stringp(bytes)) {
    MAKE_ERROR(err, ERROR_TYPE,
               arguments,
               "BUFFER-SEEK-BYTE requires that the second argument be a string",
               NULL);
    return err;
  }
  if (!integerp(direction)) {
    MAKE_ERROR(err, ERROR_TYPE,
               arguments,
               "BUFFER-SEEK-BYTE requires that the third argument be an integer",
               NULL);
    return err;
  }
  size_t bytes_moved = buffer_seek_until_byte
    (buffer.value.buffer,
     (char *)bytes.value.symbol,
     (char)direction.value.integer);
  *result = make_int((integer_t)bytes_moved);
  return ok;
}

const char *const builtin_buffer_seek_past_byte_name = "BUFFER-SEEK-PAST-BYTE";
const char *const builtin_buffer_seek_past_byte_docstring =
  "(buffer-seek-past-byte BUFFER BYTES DIRECTION)\n"
  "\n"
  "Move BUFFER point to the next byte that is NOT within the control string.\n"
  "Returns the amount of bytes BUFFER's point was moved.\n"
  "When DIRECTION is negative, seek backwards.";
Error builtin_buffer_seek_past_byte(Atom arguments, Atom *result) {
  THREE_ARGS(arguments);
  Atom buffer = car(arguments);
  Atom bytes = car(cdr(arguments));
  Atom direction = car(cdr(cdr(arguments)));
  if (!bufferp(buffer)) {
    MAKE_ERROR(err, ERROR_TYPE,
               arguments,
               "BUFFER-SEEK-PAST-BYTE requires that the first argument be a buffer",
               NULL);
    return err;
  }
  if (!stringp(bytes)) {
    MAKE_ERROR(err, ERROR_TYPE,
               arguments,
               "BUFFER-SEEK-PAST-BYTE requires that the second argument be a string",
               NULL);
    return err;
  }
  if (!integerp(direction)) {
    MAKE_ERROR(err, ERROR_TYPE,
               arguments,
               "BUFFER-SEEK-PAST-BYTE requires that the third argument be an integer",
               NULL);
    return err;
  }
  size_t bytes_moved = buffer_seek_while_byte
    (buffer.value.buffer,
     (char *)bytes.value.symbol,
     (char)direction.value.integer);
  *result = make_int((integer_t)bytes_moved);
  return ok;
}

const char *const builtin_buffer_seek_substring_name = "BUFFER-SEEK-SUBSTRING";
const char *const builtin_buffer_seek_substring_docstring =
  "(buffer-seek-substring BUFFER SUBSTRING DIRECTION)\n"
  "\n"
  "Move buffer point to the beginning of the given substring, if it exists.\n"
  "If no matching substring is found, don't move point_byte.\n"
  "When direction is negative, search backwards. Otherwise, search forwards.";
Error builtin_buffer_seek_substring(Atom arguments, Atom *result) {
  THREE_ARGS(arguments);
  Atom buffer = car(arguments);
  Atom substring = car(cdr(arguments));
  Atom direction = car(cdr(cdr(arguments)));
  if (!bufferp(buffer)) {
    MAKE_ERROR(err, ERROR_TYPE,
               arguments,
               "BUFFER-SEEK-SUBSTRING requires that the first argument be a buffer",
               NULL);
    return err;
  }
  if (!stringp(substring)) {
    MAKE_ERROR(err, ERROR_TYPE,
               arguments,
               "BUFFER-SEEK-SUBSTRING requires that the second argument be a string",
               NULL);
    return err;
  }
  if (!integerp(direction)) {
    MAKE_ERROR(err, ERROR_TYPE,
               arguments,
               "BUFFER-SEEK-SUBSTRING requires that the third argument be an integer",
               NULL);
    return err;
  }
  size_t bytes_moved = buffer_seek_until_substr
    (buffer.value.buffer,
     (char *)substring.value.symbol,
     (char)direction.value.integer);
  *result = make_int((integer_t)bytes_moved);
  return ok;
}

Error copy_impl(Atom *copy, Atom *result) {
  assert(ATOM_TYPE_MAX == 10 && "Exhaustive handling of atom types in copy_impl()");
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
    // NOTE: symbols do not allow duplicates. This will return the same
    // symbol, but it sets type info and such, so we use the helper
    // still.
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
    // NOTE: buffers do not allow duplicates.
    *result = make_buffer(copy->value.buffer->environment,
                          copy->value.buffer->path);
    break;
  case ATOM_TYPE_ENVIRONMENT:
    *result = *copy;
    break;
  default:
    break;
  }
  return ok;
}

const char *const builtin_copy_name = "COPY";
const char *const builtin_copy_docstring =
  "(copy ATOM)\n"
  "\n"
  "Return a deep copy of ATOM.";
Error builtin_copy(Atom arguments, Atom *result) {
  ONE_ARG(arguments);
  return copy_impl(&car(arguments), result);
}

const char *const builtin_string_length_name = "STRING-LENGTH";
const char *const builtin_string_length_docstring =
  "(string-length STRING)\n"
  "\n"
  "Return the length of STRING.";
Error builtin_string_length(Atom arguments, Atom *result) {
  ONE_ARG(arguments);
  Atom string = car(arguments);
  if (!stringp(string)) {
    MAKE_ERROR(err, ERROR_TYPE,
               arguments,
               "STRING-LENGTH requires a single string argument",
               NULL);
    return err;
  }
  size_t length = strlen(string.value.symbol);
  *result = make_int((integer_t)length);
  return ok;
}

const char *const builtin_string_concat_name = "STRING-CONCAT";
const char *const builtin_string_concat_docstring =
  "(string-concat STRING-A STRING-B)\n"
  "\n"
  "Return a new string consisting of STRING-A followed by STRING-B.";
Error builtin_string_concat(Atom arguments, Atom *result) {
  TWO_ARGS(arguments);
  Atom string_a = car(arguments);
  Atom string_b = car(cdr(arguments));
  if (!stringp(string_a) || !stringp(string_b)) {
    MAKE_ERROR(err, ERROR_TYPE,
               arguments,
               "STRING-CONCAT requires that both arguments be a string",
               NULL);
    return err;
  }
  size_t string_length = strlen(string_a.value.symbol) + strlen(string_b.value.symbol);
  char *string = malloc(string_length + 1);
  snprintf(string, string_length + 1, "%s%s", string_a.value.symbol, string_b.value.symbol);
  string[string_length] = '\0';
  *result = make_string(string);
  free(string);
  return ok;
}

const char *const builtin_evaluate_string_name = "EVALUATE-STRING";
const char *const builtin_evaluate_string_docstring =
  "(evaluate-string STRING)\n"
  "\n"
  "Evaluate STRING as a LITE LISP expression.";
Error builtin_evaluate_string(Atom arguments, Atom *result) {
  ONE_ARG(arguments);
  Atom input = car(arguments);
  if (!stringp(input)) {
    MAKE_ERROR(err, ERROR_TYPE,
               arguments,
               "EVALUATE-STRING requires a single string argument",
               NULL);
    return err;
  }
  Atom expr = nil;
  Error err = parse_expr(input.value.symbol,
                         (const char**)&input.value.symbol,
                         &expr);
  if (err.type) {
    return err;
  }
  err = evaluate_expression(expr, *genv(), result);
  if (err.type) {
    return err;
  }
  return ok;
}

const char *const builtin_evaluate_file_name = "EVALUATE-FILE";
const char *const builtin_evaluate_file_docstring =
  "(evaluate-file FILEPATH)\n"
  "\n"
  "Evaluate file at FILEPATH as LITE LISP source code.\n"
  "1. If file exists at PATH, evaluate that.\n"
  "2. Try PATH relative to current buffer and current buffer parent.\n"
  "3. Try PATH relative to current working directory.";
Error builtin_evaluate_file(Atom arguments, Atom *result) {
  ONE_ARG(arguments);

  Atom filepath = car(arguments);
  if (!stringp(filepath)) {
    MAKE_ERROR(err, ERROR_TYPE,
               arguments,
               "EVALUATE-FILE requires a single string argument",
               NULL);
    return err;
  }

  Error err = evaluate_file(*genv(), filepath.value.symbol, result);

  if (err.type == ERROR_FILE) {
    // Attempt to load relative to path of current buffer.
    Atom current_buffer;
    err = env_get(*genv(), make_sym("CURRENT-BUFFER"), &current_buffer);
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
  if (err.type == ERROR_FILE) {
    // Attempt to load from lite application data directory.
    char *litedir = getlitedir();
    if (litedir) {
      char *path = string_trijoin(litedir, "/", filepath.value.symbol);
      free(litedir);
      err = evaluate_file(*genv(), path, result);
      free(path);
    }
  }

  // TODO: Attempt to load relative to "argv[0]" and it's parents? iff
  //       argv[0] contains a path separating character.

  if (err.type) {
    return err;
  }
  return ok;
}

const char *const builtin_save_name = "SAVE";
const char *const builtin_save_docstring =
  "(save BUFFER)\n"
  "\n"
  "Save the given BUFFER to a file.";
Error builtin_save(Atom arguments, Atom *result) {
  ONE_ARG(arguments);
  Atom buffer = car(arguments);
  if (!bufferp(buffer)) {
    MAKE_ERROR(err, ERROR_TYPE,
               arguments,
               "SAVE requires a single buffer argument",
               NULL);
    return err;
  }
  Error err = buffer_save(*buffer.value.buffer);
  if (err.type) {
    return err;
  }
  *result = make_sym("T");
  return ok;
}

const char *const builtin_apply_name = "APPLY";
const char *const builtin_apply_docstring =
  "(apply FUNCTION ARGUMENTS)\n"
  "\n"
  "Call FUNCTION with the given ARGUMENTS and return the result.";
Error builtin_apply(Atom arguments, Atom *result) {
  TWO_ARGS(arguments);
  Atom function;
  function = car(arguments);
  arguments = car(cdr(arguments));
  // I think is to prevent against improper list argument? I'm not
  // sure, it was a long time ago at this point.
  if (!listp(arguments)) {
    MAKE_ERROR(err, ERROR_SYNTAX,
               arguments,
               "APPLY requires that arguments be a properly formed list",
               NULL);
    return err;
  }
  if (function.type == ATOM_TYPE_BUILTIN) {
    return (*function.value.builtin.function)(arguments, result);
  } else if (function.type != ATOM_TYPE_CLOSURE) {
    MAKE_ERROR(err, ERROR_TYPE,
               arguments,
               "APPLY requires the first argument be a builtin or closure",
               NULL);
    return err;
  }
  // Handle closure.
  // Layout: FUNCTION ARGUMENTS . BODY
  Atom environment = env_create(car(function), 2 << 2);
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
      // TODO: Better error.
      ARG_ERR(arguments);
    }
    env_set(environment, car(argument_names), car(arguments));
    argument_names = cdr(argument_names);
    arguments = cdr(arguments);
  }
  if (!nilp(arguments)) {
    // TODO: Better error.
    ARG_ERR(arguments);
  }
  // Evaluate body of closure.
  while (!nilp(body)) {
    Error err = evaluate_expression(car(body), environment, result);
    if (err.type) { return err; }
    if (user_quit) {
      break;
    }
    body = cdr(body);
  }
  return ok;
}

const char *const builtin_eq_name = "EQ";
const char *const builtin_eq_docstring =
  "(eq A B)\n"
  "\n"
  "Return 'T' iff A and B refer to the same Atomic LISP object.";
Error builtin_eq(Atom arguments, Atom *result) {
  TWO_ARGS(arguments);
  *result = compare_atoms(car(arguments), car(cdr(arguments)));
  return ok;
}

const char *const builtin_symbol_table_name = "SYM";
const char *const builtin_symbol_table_docstring =
  "(sym)\n"
  "\n"
  "Return a copy of the global symbol table at the time of calling.";
Error builtin_symbol_table(Atom arguments, Atom *result) {
  NO_ARGS(arguments);
  *result = symbol_table();
  return ok;
}

const char *const builtin_print_name = "PRINT";
const char *const builtin_print_docstring =
  "(print ARG)\n"
  "\n"
  "Print the given ARG to standard out followed by newline.";
Error builtin_print(Atom arguments, Atom *result) {
  ONE_ARG(arguments);
  print_atom(car(arguments));
  fputc('\n', stdout);
  fflush(stdout);
  *result = nil;
  return ok;
}

const char *const builtin_prins_name = "PRINS";
const char *const builtin_prins_docstring =
  "(prins STRING)\n"
  "\n"
  "Print the given STRING (or symbol) to standard out.";
Error builtin_prins(Atom arguments, Atom *result) {
  ONE_ARG(arguments);
  Atom string = car(arguments);
  if (!stringp(string) && !symbolp(string)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "PRINS requires a single string or symbol argument",
               NULL);
    return err_type;
  }
  fputs(string.value.symbol, stdout);
  fflush(stdout);
  *result = nil;
  return ok;
}

const char *const builtin_read_prompted_name = "READ-PROMPTED";
const char *const builtin_read_prompted_docstring =
  "(read-prompted PROMPT)\n"
  "\n"
  "Show the user a PROMPT and return user response as a string.";
// TODO: There has to be better ways to do this...
Error builtin_read_prompted(Atom arguments, Atom *result) {
  ONE_ARG(arguments);
  Atom prompt = car(arguments);
  if (!stringp(prompt)) {
    MAKE_ERROR(err, ERROR_TYPE,
               arguments,
               "READ-PROMPTED requires a single string argument (a prompt)",
               NULL);
    return err;
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

  Atom popup_buffer = make_buffer(env_create(nil, 0), ".popup");
  if (popup_buffer.value.buffer && !bufferp(popup_buffer)) {
    MAKE_ERROR(err, ERROR_GENERIC,
               nil, "make_buffer() didn't return a buffer!",
               NULL);
    return err;
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

  return ok;
}

const char *const builtin_finish_read_name = "FINISH-READ";
const char *const builtin_finish_read_docstring =
  "(finish-read)\n"
  "\n"
  "When reading, complete the read and return the string.";
Error builtin_finish_read(Atom arguments, Atom *result) {
  NO_ARGS(arguments);
#ifdef LITE_GFX
  gui_ctx()->reading = 0;
#endif /* #ifdef LITE_GFX */
  *result = nil;
  return ok;
}

const char *const builtin_change_font_name = "CHANGE-FONT";
const char *const builtin_change_font_docstring =
  "(change-font FONT-FILENAME POINT-SIZE)\n"
  "\n"
  "Attempt to change font to FONT-FILENAME with POINT-SIZE.";
Error builtin_change_font(Atom arguments, Atom *result) {
#ifdef LITE_GFX
  TWO_ARGS(arguments);
  Atom font_path = car(arguments);
  if (!stringp(font_path)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "CHANGE-FONT requires that the first argument be a string",
               NULL);
    return err_type;
  }
  Atom font_size = car(cdr(arguments));
  if (!integerp(font_size)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "CHANGE-FONT requires that the second argument be an integer",
               NULL);
    return err_type;
  }
  *result = nil;
  if (change_font(font_path.value.symbol, (size_t)font_size.value.integer) == 0) {
    *result = make_sym("T");
  }
#endif
  return ok;
}

const char *const builtin_change_font_size_name = "CHANGE-FONT-SIZE";
const char *const builtin_change_font_size_docstring =
  "(change-font-size POINT-SIZE)\n"
  "\n"
  "Attempt to change the current font's size to POINT-SIZE.";
Error builtin_change_font_size(Atom arguments, Atom *result) {
#ifdef LITE_GFX
  ONE_ARG(arguments);
  Atom font_size = car(arguments);
  if (!integerp(font_size)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "CHANGE-FONT-SIZE requires a single integer argument",
               NULL);
    return err_type;
  }
  *result = nil;
  if (change_font_size((size_t)font_size.value.integer) == 0) {
    *result = make_sym("T");
  }
#endif
  return ok;
}

const char *const builtin_window_size_name = "WINDOW-SIZE";
const char *const builtin_window_size_docstring =
  "(window-size)\n"
  "\n"
  "Return a pair containing the graphical window's size.";
Error builtin_window_size(Atom arguments, Atom *result) {
#ifdef LITE_GFX
  NO_ARGS(arguments);
  size_t width = 0;
  size_t height = 0;
  window_size(&width, &height);
  *result = cons(make_int((integer_t)width), make_int((integer_t)height));
#endif
  return ok;
}

const char *const builtin_change_window_size_name = "CHANGE-WINDOW-SIZE";
const char *const builtin_change_window_size_docstring =
  "(change-window-size WIDTH HEIGHT)\n"
  "\n"
  "Attempt to change size of window to WIDTH by HEIGHT.";
Error builtin_change_window_size(Atom arguments, Atom *result) {
#ifdef LITE_GFX
  TWO_ARGS(arguments);
  Atom width = car(arguments);
  Atom height = car(cdr(arguments));
  if (!integerp(width) || !integerp(height)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "CHANGE-WINDOW-SIZE requires that both of the given arguments be integers",
               NULL);
    return err_type;
  }
  *result = make_sym("T");
  change_window_size((size_t)width.value.integer, (size_t)height.value.integer);
#endif
  return ok;
}

// TODO: Accept 'windowed, 'fullscreen, 'maximized symbols as arguments.
const char *const builtin_change_window_mode_name = "CHANGE-WINDOW-MODE";
const char *const builtin_change_window_mode_docstring =
  "(change-window-mode N)\n"
  "\n"
  "If N is 1, set window mode to fullscreen. Otherwise, set it to windowed.";
Error builtin_change_window_mode(Atom arguments, Atom *result) {
  (void)result;
#ifdef LITE_GFX
  ONE_ARG(arguments);
  Atom n = car(arguments);
  if (!integerp(n)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "CHANGE-WINDOW-MODE requires a single integer argument",
               NULL);
    return err_type;
  }
  enum GFXWindowMode mode = GFX_WINDOW_MODE_WINDOWED;
  if (n.value.integer == 1) {
    mode = GFX_WINDOW_MODE_FULLSCREEN;
  }
  change_window_mode(mode);
#endif
  return ok;
}

static Atom get_active_window(void) {
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

  Atom active_window = list_get_safe(window_list, (int)active_window_index.value.integer);
  return active_window;
}

const char *const builtin_scroll_up_name = "SCROLL-UP";
const char *const builtin_scroll_up_docstring =
  "(scroll-up)\n"
  "\n"
  "";
Error builtin_scroll_up(Atom arguments, Atom *result) {
  (void)result;
#ifdef LITE_GFX
  // Use the default offset of one, unless a positive integer value was
  // given.
  integer_t offset = 1;
  if (!nilp(arguments)) {
    if (!integerp(car(arguments))) {
      MAKE_ERROR(err_type, ERROR_TYPE,
                 arguments,
                 "SCROLL-UP may be given a single *integer* argument",
                 NULL);
      return err_type;
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
  return ok;
}

const char *const builtin_scroll_down_name = "SCROLL-DOWN";
const char *const builtin_scroll_down_docstring =
  "(scroll-down)\n"
  "\n"
  "";
Error builtin_scroll_down(Atom arguments, Atom *result) {
  (void)result;
#ifdef LITE_GFX
  // Use the default offset of one, unless a positive integer value was
  // given.
  integer_t offset = 1;
  if (!nilp(arguments)) {
    if (!nilp(cdr(arguments))) {
      // Too many arguments (more than one).
      ARG_ERR(arguments);
    }
    if (!integerp(car(arguments))) {
      // Given argument must be an integer.
      MAKE_ERROR(err_type, ERROR_TYPE,
                 arguments,
                 "SCROLL-DOWN may be given a single *integer* argument",
                 NULL);
      return err_type;
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
  return ok;
}

const char *const builtin_scroll_left_name = "SCROLL-LEFT";
const char *const builtin_scroll_left_docstring =
  "(scroll-left)\n"
  "\n"
  "";
Error builtin_scroll_left(Atom arguments, Atom *result) {
  NO_ARGS(arguments);
  (void)result;
#ifdef LITE_GFX
  // Use the default offset of one, unless a positive integer value was
  // given.
  integer_t offset = 1;
  if (!nilp(arguments)) {
    if (!integerp(car(arguments))) {
      MAKE_ERROR(err_type, ERROR_TYPE,
                 arguments,
                 "SCROLL-LEFT may be given a single *integer* argument",
                 NULL);
      return err_type;
    }
    if (car(arguments).value.integer > 0) {
      offset = car(arguments).value.integer;
    }
  }
  Atom active_window = get_active_window();
  // Prevent unsigned integer underflow.
  Atom scrollxy = list_get(active_window, 3);
  if (offset > car(scrollxy).value.integer) {
    offset = car(scrollxy).value.integer;
  }
  car(scrollxy).value.integer -= offset;
#else
  (void)arguments;
#endif /* #ifdef LITE_GFX */
  return ok;
}

const char *const builtin_scroll_right_name = "SCROLL-RIGHT";
const char *const builtin_scroll_right_docstring =
  "(scroll-right)\n"
  "\n"
  "";
Error builtin_scroll_right(Atom arguments, Atom *result) {
  NO_ARGS(arguments);
  (void)result;
#ifdef LITE_GFX
  // Use the default offset of one, unless a positive integer value was
  // given.
  integer_t offset = 1;
  if (!nilp(arguments)) {
    if (!nilp(cdr(arguments))) {
      // Too many arguments (more than one).
      ARG_ERR(arguments);
    }
    if (!integerp(car(arguments))) {
      // Given argument must be an integer.
      MAKE_ERROR(err_type, ERROR_TYPE,
                 arguments,
                 "SCROLL-RIGHT may be given a single *integer* argument",
                 NULL);
      return err_type;
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
  car(scrollxy).value.integer += offset;
  if (car(scrollxy).value.integer < old_vertical_offset) {
    // Restore vertical offset to what it was before overflow.
    car(scrollxy).value.integer = old_vertical_offset;
  }
#else
  (void)arguments;
#endif /* #ifdef LITE_GFX */
  return ok;
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
Error builtin_clipboard_cut(Atom arguments, Atom *result) {
  ONE_ARG(arguments);
  *result = nil;
  Atom buffer = car(arguments);
  if (!bufferp(buffer)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "CLIPBOARD-CUT requires a single buffer argument",
               NULL);
    return err_type;
  }
  if (buffer_mark_active(*buffer.value.buffer)) {
    // TODO: STRIP CARRIAGE RETURN FROM BEFORE EVERY NEWLINE ON WINDOWS IFF 'CLIPBOARD--STRIP-CR' IS NON-NIL.
    char *region = buffer_region(*buffer.value.buffer);
    Error err = buffer_remove_region(buffer.value.buffer);
    if (err.type) {
      return err;
    }
    // TODO: This is a terrible copy and paste implementation!
    terrible_copy_paste_implementation = make_string(region);
#   ifdef LITE_GFX
    set_clipboard_utf8(region);
#   endif
    free(region);
    *result = make_sym("T");
  }
  return ok;
}
const char *const builtin_clipboard_copy_name = "CLIPBOARD-COPY";
const char *const builtin_clipboard_copy_docstring =
  "(clipboard-copy BUFFER)\n"
  "\n"
  "Copy the selected region from BUFFER, if active.";
Error builtin_clipboard_copy(Atom arguments, Atom *result) {
  ONE_ARG(arguments);
  *result = nil;
  Atom buffer = car(arguments);
  if (!bufferp(buffer)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "CLIPBOARD-COPY requires a single buffer argument",
               NULL);
    return err_type;
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

  return ok;
}
const char *const builtin_clipboard_paste_name = "CLIPBOARD-PASTE";
const char *const builtin_clipboard_paste_docstring =
  "(clipboard-paste BUFFER)\n"
  "\n"
  "Insert the most-recently clipboarded string into the current buffer at point.";
Error builtin_clipboard_paste(Atom arguments, Atom *result) {
  ONE_ARG(arguments);
  Atom buffer = car(arguments);
  if (!bufferp(buffer)) {
    MAKE_ERROR(err_type, ERROR_TYPE,
               arguments,
               "CLIPBOARD-PASTE requires a single buffer argument",
               NULL);
    return err_type;
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
    /*
    if (!stringp(terrible_copy_paste_implementation)) {
      return ERROR_TYPE;
    }
    */
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
  return ok;
}


const char *const builtin_set_carriage_return_character_name = "SET-CARRIAGE-RETURN-CHARACTER";
const char *const builtin_set_carriage_return_character_docstring =
  "(set-carriage-return-character CHARSTRING)\n"
  "\n"
  "When non-nil, use the first character of string as the character to render a carriage return (\\\\r) as.\n"
  "When nil, do not render carriage return character (make it invisible).";
Error builtin_set_carriage_return_character(Atom arguments, Atom *result) {
  ONE_ARG(arguments);
  *result = nil;

# ifdef LITE_GFX
  if (nilp(car(arguments)) || stringp(car(arguments)) || symbolp(car(arguments))) {
    *result = make_sym("T");
    gui_ctx()->cr_char = nilp(car(arguments)) ? 0 : car(arguments).value.symbol[0];
    // Disallow control characters.
    if (gui_ctx()->cr_char < 32) {
      gui_ctx()->cr_char = 0;
    }
    return ok;
  }
  MAKE_ERROR(err, ERROR_TYPE,
             arguments,
             "Argument to set-carriage-return-character is of incorrect type!",
             "Try passing a symbol or string, like \"$\" or '$, or nil to hide carriage return.");
  return err;
# endif

  return ok;
}

/*
const char *const builtin__name = "LISP-SYMBOL";
const char *const builtin__docstring =
  "(lisp-symbol)\n"
  "\n"
  "";
Error builtin_(Atom arguments, Atom *result) {
  MAKE_ERROR(err, ERROR_TODO, arguments, "There's always more todo...", NULL);
  return err;
}
*/
