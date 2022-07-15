#include <environment.h>

#ifdef LITE_GFX
#  include <api.h>
#  include <gui.h>
#endif /* #ifdef LITE_GFX */

#include <builtins.h>
#include <error.h>
#include <types.h>

Atom env_create(Atom parent) {
  return cons(parent, nil);
}

Error env_set(Atom environment, Atom symbol, Atom value) {
  Atom bindings = cdr(environment);
  while (!nilp(bindings)) {
    Atom bind = car(bindings);
    if (car(bind).value.symbol == symbol.value.symbol) {
      // Update docstring, or use the old one.
      char *docstring = value.docstring ? value.docstring : cdr(bind).docstring;
      cdr(bind) = value;
      cdr(bind).docstring = docstring;
      return ok;
    }
    bindings = cdr(bindings);
  }
  bindings = cons(symbol, value);
  cdr(environment) = cons(bindings, cdr(environment));
  return ok;
}

Error env_get(Atom environment, Atom symbol, Atom *result) {
  Atom parent = car(environment);
  Atom bindings = cdr(environment);
#ifdef LITE_GFX
  // Handle reading/popup-buffer.
  if (gui_ctx() && gui_ctx()->reading
      && symbol.value.symbol == make_sym("CURRENT-BUFFER").value.symbol)
    {
      symbol = make_sym("POPUP-BUFFER");
    }
#endif /* #ifdef LITE_GFX */
  while (!nilp(bindings)) {
    Atom bind = car(bindings);
    if (car(bind).value.symbol == symbol.value.symbol) {
      *result = cdr(bind);
      return ok;
    }
    bindings = cdr(bindings);
  }
  if (nilp(parent)) {
    MAKE_ERROR(err, ERROR_NOT_BOUND
               , symbol
               // FIXME: Which environment?
               , "Symbol is not bound in environment."
               , NULL);
    return err;
  }
  return env_get(parent, symbol, result);
}

int env_non_nil(Atom environment, Atom symbol) {
  Atom bind = nil;
  Error err = env_get(environment, symbol, &bind);
  if (err.type) {
    return 0;
  }
  return !nilp(bind);
}

int boundp(Atom environment, Atom symbol) {
  Atom bind = nil;
  return !(env_get(environment, symbol, &bind).type);
}

Atom default_environment() {
  Atom environment = env_create(nil);
  env_set(environment, make_sym("T"),        make_sym("T"));
  env_set(environment, make_sym("CAR"),      make_builtin(builtin_car,          builtin_car_docstring));
  env_set(environment, make_sym("CDR"),      make_builtin(builtin_cdr,          builtin_cdr_docstring));
  env_set(environment, make_sym("CONS"),     make_builtin(builtin_cons,         builtin_cons_docstring));
  env_set(environment, make_sym("SETCAR"),   make_builtin(builtin_setcar,       builtin_setcar_docstring));
  env_set(environment, make_sym("NILP"),     make_builtin(builtin_nilp,         builtin_nilp_docstring));
  env_set(environment, make_sym("PAIRP"),    make_builtin(builtin_pairp,        builtin_pairp_docstring));
  env_set(environment, make_sym("SYMBOLP"),  make_builtin(builtin_symbolp,      builtin_symbolp_docstring));
  env_set(environment, make_sym("INTEGERP"), make_builtin(builtin_integerp,     builtin_integerp_docstring));
  env_set(environment, make_sym("BUILTINP"), make_builtin(builtin_builtinp,     builtin_builtinp_docstring));
  env_set(environment, make_sym("CLOSUREP"), make_builtin(builtin_closurep,     builtin_closurep_docstring));
  env_set(environment, make_sym("MACROP"),   make_builtin(builtin_macrop,       builtin_macrop_docstring));
  env_set(environment, make_sym("STRINGP"),  make_builtin(builtin_stringp,      builtin_stringp_docstring));
  env_set(environment, make_sym("BUFFERP"),  make_builtin(builtin_bufferp,      builtin_bufferp_docstring));
  env_set(environment, make_sym("+"),        make_builtin(builtin_add,          builtin_add_docstring));
  env_set(environment, make_sym("-"),        make_builtin(builtin_subtract,     builtin_subtract_docstring));
  env_set(environment, make_sym("*"),        make_builtin(builtin_multiply,     builtin_multiply_docstring));
  env_set(environment, make_sym("/"),        make_builtin(builtin_divide,       builtin_divide_docstring));
  env_set(environment, make_sym("%"),        make_builtin(builtin_remainder,    builtin_remainder_docstring));
  env_set(environment, make_sym("!"),        make_builtin(builtin_not,          builtin_not_docstring));
  env_set(environment, make_sym("="),        make_builtin(builtin_numeq,        builtin_numeq_docstring));
  env_set(environment, make_sym("!="),       make_builtin(builtin_numnoteq,     builtin_numnoteq_docstring));
  env_set(environment, make_sym("<"),        make_builtin(builtin_numlt,        builtin_numlt_docstring));
  env_set(environment, make_sym("<="),       make_builtin(builtin_numlt_or_eq,  builtin_numlt_or_eq_docstring));
  env_set(environment, make_sym(">"),        make_builtin(builtin_numgt,        builtin_numgt_docstring));
  env_set(environment, make_sym(">="),       make_builtin(builtin_numgt_or_eq,  builtin_numgt_or_eq_docstring));
  env_set(environment, make_sym("EQ"),       make_builtin(builtin_eq,           builtin_eq_docstring));
  env_set(environment, make_sym("COPY"),     make_builtin(builtin_copy,         builtin_copy_docstring));
  env_set(environment, make_sym("SAVE"),     make_builtin(builtin_save,         builtin_save_docstring));
  env_set(environment, make_sym("APPLY"),    make_builtin(builtin_apply,        builtin_apply_docstring));
  env_set(environment, make_sym("PRINT"),    make_builtin(builtin_print,        builtin_print_docstring));
  env_set(environment, make_sym("SYM"),      make_builtin(builtin_symbol_table, builtin_symbol_table_docstring));

  env_set(environment, make_sym("BUF"),
          make_builtin(builtin_buffer_table,            builtin_buffer_table_docstring));
  env_set(environment, make_sym("OPEN-BUFFER")
          , make_builtin(builtin_open_buffer,           builtin_open_buffer_docstring));
  env_set(environment, make_sym("BUFFER-INSERT")
          , make_builtin(builtin_buffer_insert,         builtin_buffer_insert_docstring));
  env_set(environment, make_sym("BUFFER-REMOVE")
          , make_builtin(builtin_buffer_remove,         builtin_buffer_remove_docstring));
  env_set(environment, make_sym("BUFFER-REMOVE-FORWARD")
          , make_builtin(builtin_buffer_remove_forward, builtin_buffer_remove_forward_docstring));
  env_set(environment, make_sym("BUFFER-STRING")
          , make_builtin(builtin_buffer_string,         builtin_buffer_string_docstring));
  env_set(environment, make_sym("BUFFER-LINES")
          , make_builtin(builtin_buffer_lines,          builtin_buffer_lines_docstring));
  env_set(environment, make_sym("BUFFER-LINE")
          , make_builtin(builtin_buffer_line,           builtin_buffer_line_docstring));
  env_set(environment, make_sym("BUFFER-CURRENT-LINE")
          , make_builtin(builtin_buffer_current_line,   builtin_buffer_current_line_docstring));
  env_set(environment, make_sym("BUFFER-SET-POINT")
          , make_builtin(builtin_buffer_set_point,      builtin_buffer_set_point_docstring));
  env_set(environment, make_sym("BUFFER-POINT")
          , make_builtin(builtin_buffer_point,          builtin_buffer_point_docstring));
  env_set(environment, make_sym("BUFFER-INDEX")
          , make_builtin(builtin_buffer_index,          builtin_buffer_index_docstring));
  env_set(environment, make_sym("BUFFER-SEEK-BYTE")
          , make_builtin(builtin_buffer_seek_byte,      builtin_buffer_seek_byte_docstring));
  env_set(environment, make_sym("BUFFER-SEEK-SUBSTRING")
          , make_builtin(builtin_buffer_seek_substring, builtin_buffer_seek_substring_docstring));

  env_set(environment, make_sym("READ-PROMPTED")
          , make_builtin(builtin_read_prompted,         builtin_read_prompted_docstring));
  env_set(environment, make_sym("FINISH-READ")
          , make_builtin(builtin_finish_read,           builtin_finish_read_docstring));

  env_set(environment, make_sym("STRING-LENGTH")
          , make_builtin(builtin_string_length,         builtin_string_length_docstring));

  env_set(environment, make_sym("EVALUATE")
          , make_builtin(builtin_evaluate,              builtin_evaluate_docstring));
  env_set(environment, make_sym("EVALUATE-STRING")
          , make_builtin(builtin_evaluate_string,       builtin_evaluate_string_docstring));
  env_set(environment, make_sym("EVALUATE-FILE")
          , make_builtin(builtin_evaluate_file,         builtin_evaluate_file_docstring));

  env_set(environment, make_sym("MEMBER"), make_builtin(builtin_member, builtin_member_docstring));

  env_set(environment, make_sym("WHILE-RECURSE-LIMIT"), make_int_with_docstring
          (10000
           , "This is the maximum amount of times a while loop may loop.\n"
           "\n"
           "Used to prevent infinite loops."));

  // FIXME: It's always best to keep docstrings out of here and where they belong.
  env_set(environment, make_sym("GARBAGE-COLLECTOR-EVALUATION-ITERATIONS-THRESHOLD"),
          make_int_with_docstring
          (100000, "This number corresponds to the amount of evaluation operations before \
running the garbage collector.\nSmaller numbers mean memory is freed more often. \
The default value of '100000' means memory is freed in around twenty megabyte chunks."));

  env_set(environment, make_sym("GARBAGE-COLLECTOR-PAIR-ALLOCATIONS-THRESHOLD"),
          make_int_with_docstring
          (290500, "This number corresponds to the amount of pairs able to be allocated \
before running the garbage collector.\nSmaller numbers mean memory is freed more often, \
but too small causes problems."));

  env_set(environment, make_sym("DEBUG/ENVIRONMENT"), nil_with_docstring
          ("When non-nil, display debug information concerning the current \
LISP evaluation environment, including the symbol table."));

  env_set(environment, make_sym("DEBUG/EVALUATE"), nil_with_docstring
          ("When non-nil, display debug information concerning the evaluation of expressions."));

  env_set(environment, make_sym("DEBUG/KEYBINDING"), nil_with_docstring
          ("When non-nil, display debug information concerning keybindings, \
including information about keymaps on every button press."));

  env_set(environment, make_sym("DEBUG/MACRO"), nil_with_docstring
          ("When non-nil, display debug information concerning macros, \
including what each expansion step looks like."));

  env_set(environment, make_sym("DEBUG/MEMORY"), nil_with_docstring
          ("When non-nil, display debug information concerning allocated memory, \
including when garbage collections happen and data upon program exit."));

  env_set(environment, make_sym("DEBUG/WHILE"), nil_with_docstring
          ("When non-nil, display debug information concerning 'WHILE', \
including data output at each iteration of the loop."));

  return environment;
}

static Atom global_environment = { ATOM_TYPE_NIL, { 0 }, NULL, NULL };
Atom *genv() {
  if (nilp(global_environment)) {
    printf("Recreating global environment from defaults...\n");
    global_environment = default_environment();
  }
  return &global_environment;
}
