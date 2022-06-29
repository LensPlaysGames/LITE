#include <builtins.h>

#include <buffer.h>
#include <error.h>
#include <environment.h>
#include <evaluation.h>
#include <parser.h>
#include <rope.h>
#include <string.h>
#include <types.h>

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
        return 0;                                           \
      }                                                     \
  } while (0);

int typep(Atom arguments, enum AtomType type, Atom *result) {
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  *result = car(arguments).type == type ? make_sym("T") : nil;
  return ERROR_NONE;
}

symbol_t *builtin_nilp_docstring =
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

symbol_t *builtin_pairp_docstring =
  "(pairp ARG)\n"
  "\n"
  "Return 'T' iff ARG has a type of 'PAIR', otherwise return nil.";
int builtin_pairp(Atom arguments, Atom *result) {
  return typep(arguments, ATOM_TYPE_PAIR, result);
}

symbol_t *builtin_symbolp_docstring =
  "(symbolp ARG)\n"
  "\n"
  "Return 'T' iff ARG has a type of 'SYMBOL', otherwise return nil.";
int builtin_symbolp(Atom arguments, Atom *result) {
  return typep(arguments, ATOM_TYPE_SYMBOL, result);
}

symbol_t *builtin_integerp_docstring =
  "(integerp ARG)\n"
  "\n"
  "Return 'T' iff ARG has a type of 'INTEGER', otherwise return nil.";
int builtin_integerp(Atom arguments, Atom *result) {
  return typep(arguments, ATOM_TYPE_INTEGER, result);
}

symbol_t *builtin_builtinp_docstring =
  "(builtinp ARG)\n"
  "\n"
  "Return 'T' iff ARG has a type of 'BUILTIN', otherwise return nil.";
int builtin_builtinp(Atom arguments, Atom *result) {
  return typep(arguments, ATOM_TYPE_BUILTIN, result);
}

symbol_t *builtin_closurep_docstring =
  "(closurep ARG)\n"
  "\n"
  "Return 'T' iff ARG has a type of 'CLOSURE', otherwise return nil.";
int builtin_closurep(Atom arguments, Atom *result) {
  return typep(arguments, ATOM_TYPE_CLOSURE, result);
}

symbol_t *builtin_macrop_docstring =
  "(macrop ARG)\n"
  "\n"
  "Return 'T' iff ARG has a type of 'MACRO', otherwise return nil.";
int builtin_macrop(Atom arguments, Atom *result) {
  return typep(arguments, ATOM_TYPE_MACRO, result);
}

symbol_t *builtin_stringp_docstring =
  "(stringp ARG)\n"
  "\n"
  "Return 'T' iff ARG has a type of 'STRING', otherwise return nil.";
int builtin_stringp(Atom arguments, Atom *result) {
  return typep(arguments, ATOM_TYPE_STRING, result);
}

symbol_t *builtin_bufferp_docstring =
  "(bufferp ARG)\n"
  "\n"
  "Return 'T' iff ARG has a type of 'BUFFER', otherwise return nil.";
int builtin_bufferp(Atom arguments, Atom *result) {
  return typep(arguments, ATOM_TYPE_BUFFER, result);
}

symbol_t *builtin_not_docstring =
  "(! ARG)\n"
  "\n"
  "Given ARG is nil, return 'T', otherwise return nil.";
int builtin_not(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  *result = nilp(car(arguments)) ? make_sym("T") : nil;
  return ERROR_NONE;
}

symbol_t *builtin_car_docstring =
  "(car ARG)\n"
  "\n"
  "Given ARG is a pair, return the value on the left side.\n"
  "Otherwise, return nil.";
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

symbol_t *builtin_cdr_docstring =
  "(cdr ARG)\n"
  "\n"
  "Given ARG is a pair, return the value on the right side.\n"
  "Otherwise, return nil.";
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

symbol_t *builtin_cons_docstring =
  "(cons LEFT RIGHT)\n"
  "\n"
  "Return a new pair, with LEFT and RIGHT on each side, respectively.";
int builtin_cons(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_TWO_ARGUMENTS(arguments);
  *result = cons(car(arguments), car(cdr(arguments)));
  return ERROR_NONE;
}

symbol_t *builtin_add_docstring =
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

symbol_t *builtin_subtract_docstring =
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

symbol_t *builtin_multiply_docstring =
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

symbol_t *builtin_divide_docstring =
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

symbol_t *builtin_open_buffer_docstring =
  "(open-buffer PATH)\n\nReturn a buffer visiting PATH.";
int builtin_open_buffer(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  Atom path = car(arguments);
  if (!stringp(path) || !path.value.symbol) {
    return ERROR_TYPE;
  }
  // TODO: Buffer-local environment should go here.
  *result = make_buffer(env_create(nil), (char *)path.value.symbol);
  return ERROR_NONE;
}

symbol_t *builtin_buffer_table_docstring =
  "(buf)\n\nReturn the LISP buffer table.";
int builtin_buffer_table(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_NO_ARGUMENTS(arguments);
  *result = *buf_table();
  return ERROR_NONE;
}

symbol_t *builtin_buffer_insert_docstring =
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
  Error err = buffer_insert(buffer.value.buffer
                            , (char *)string.value.symbol);
  if (err.type) {
    print_error(err);
    return err.type;
  }
  *result = buffer;
  return ERROR_NONE;
}

symbol_t *builtin_buffer_remove_docstring =
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

symbol_t *builtin_buffer_remove_forward_docstring =
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

symbol_t *builtin_buffer_set_point_docstring =
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
    if (point.value.integer > buffer.value.buffer->rope->weight) {
      new_point_byte = buffer.value.buffer->rope->weight;
    } else {
      new_point_byte = point.value.integer;
    }
  }
  buffer.value.buffer->point_byte = new_point_byte;
  *result = buffer;
  return ERROR_NONE;
}

symbol_t *builtin_buffer_point_docstring =
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

symbol_t *builtin_buffer_index_docstring =
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

symbol_t *builtin_buffer_string_docstring =
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
  *result = make_string(buffer_string(*buffer.value.buffer));
  return ERROR_NONE;
}

symbol_t *builtin_buffer_lines_docstring =
  "(buffer-lines BUFFER START-LINE LINE-COUNT)\n"
  "\n"
  "Get LINE-COUNT lines";
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
  *result = make_string
    (buffer_lines(*buffer.value.buffer
                  , start_line.value.integer
                  , line_count.value.integer));
  return ERROR_NONE;
}

symbol_t *builtin_buffer_line_docstring =
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
  *result = make_string(buffer_line(*buffer.value.buffer
                                    , line_count.value.integer));
  return ERROR_NONE;
}

symbol_t *builtin_buffer_current_line_docstring =
  "(buffer-current-line BUFFER)\n"
  "\n"
  "Get line surrounding point in BUFFER as string.";
int builtin_buffer_current_line(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  Atom buffer = car(arguments);
  if (!bufferp(buffer) || !buffer.value.buffer) {
    return ERROR_TYPE;
  }
  *result = make_string(buffer_current_line(*buffer.value.buffer));
  return ERROR_NONE;
}

symbol_t *builtin_numeq_docstring =
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

symbol_t *builtin_numnoteq_docstring =
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

symbol_t *builtin_numlt_docstring =
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

symbol_t *builtin_numlt_or_eq_docstring =
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

symbol_t *builtin_numgt_docstring =
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

symbol_t *builtin_numgt_or_eq_docstring =
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

symbol_t *builtin_evaluate_string_docstring =
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
  Error err = parse_expr(input.value.symbol, &input.value.symbol, &expr);
  if (err.type) {
    printf("EVALUATE-STRING PARSING ");
    print_error(err);
    return err.type;
  }
  err = evaluate_expression(expr, genv(), result);
  if (err.type) {
    printf("EVALUATE-STRING EVALUATION ");
    print_error(err);
    return err.type;
  }
  return ERROR_NONE;
}

symbol_t *builtin_save_docstring =
  "(save BUFFER)\n"
  "\n"
  "Save the given BUFFER to a file.";
int builtin_save(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  Atom buffer = car(arguments);
  if (!bufferp(buffer)) {
    return ERROR_TYPE;
  }
  Error err = buffer_save(*buffer.value.buffer);
  if (err.type) {
    print_error(err);
    return err.type;
  }
  return ERROR_NONE;
}

symbol_t *builtin_apply_docstring =
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
    return (*function.value.builtin)(arguments, result);
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
    body = cdr(body);
  }
  return ERROR_NONE;
}

symbol_t *builtin_eq_docstring =
  "(eq A B)\n"
  "\n"
  "Return 'T' iff A and B refer to the same Atomic LISP object.";
int builtin_eq(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_TWO_ARGUMENTS(arguments);
  Atom a = car(arguments);
  Atom b = car(cdr(arguments));
  int equal = 0;
  if (a.type == b.type) {
    switch (a.type) {
    case ATOM_TYPE_NIL:
      equal = 1;
      break;
    case ATOM_TYPE_PAIR:
    case ATOM_TYPE_CLOSURE:
    case ATOM_TYPE_MACRO:
      equal = (a.value.pair == b.value.pair);
      break;
    case ATOM_TYPE_SYMBOL:
      equal = (a.value.symbol == b.value.symbol);
      break;
    case ATOM_TYPE_STRING:
      equal = (strcmp(a.value.symbol, b.value.symbol) == 0);
      break;
    case ATOM_TYPE_INTEGER:
      equal = (a.value.integer == b.value.integer);
      break;
    case ATOM_TYPE_BUILTIN:
      equal = (a.value.builtin == b.value.builtin);
      break;
    default:
      equal = 0;
      break;
    }
  }
  *result = equal ? make_sym("T") : nil;
  return ERROR_NONE;
}

symbol_t *builtin_symbol_table_docstring =
  "(sym)\n\nReturn the LISP symbol table.";
int builtin_symbol_table(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_NO_ARGUMENTS(arguments);
  *result = *sym_table();
  return ERROR_NONE;
}

symbol_t *builtin_print_docstring =
  "(print ARG)\n\nPrint the given ARG to standard out, prettily.";
int builtin_print(Atom arguments, Atom *result) {
  BUILTIN_ENSURE_ONE_ARGUMENT(arguments);
  pretty_print_atom(car(arguments));
  putchar('\n');
  *result = nil;
  return ERROR_NONE;
}
