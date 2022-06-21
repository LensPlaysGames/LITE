#include <environment.h>

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
      symbol_t *docstring = value.docstring ? value.docstring : cdr(bind).docstring;
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

Atom default_shift_conversion_alist() {
  // Only the first character of the string is used for now.
  Atom shift_map_alist = make_alist(make_string("q"), make_string("Q"));
  alist_set(&shift_map_alist, make_string("w"), make_string("W"));
  alist_set(&shift_map_alist, make_string("e"), make_string("E"));
  alist_set(&shift_map_alist, make_string("r"), make_string("R"));
  alist_set(&shift_map_alist, make_string("t"), make_string("T"));
  alist_set(&shift_map_alist, make_string("y"), make_string("Y"));
  alist_set(&shift_map_alist, make_string("u"), make_string("U"));
  alist_set(&shift_map_alist, make_string("i"), make_string("I"));
  alist_set(&shift_map_alist, make_string("o"), make_string("O"));
  alist_set(&shift_map_alist, make_string("p"), make_string("P"));
  alist_set(&shift_map_alist, make_string("["), make_string("{"));
  alist_set(&shift_map_alist, make_string("]"), make_string("}"));
  alist_set(&shift_map_alist, make_string("\\"), make_string("|"));
  alist_set(&shift_map_alist, make_string("a"), make_string("A"));
  alist_set(&shift_map_alist, make_string("s"), make_string("S"));
  alist_set(&shift_map_alist, make_string("d"), make_string("D"));
  alist_set(&shift_map_alist, make_string("f"), make_string("F"));
  alist_set(&shift_map_alist, make_string("g"), make_string("G"));
  alist_set(&shift_map_alist, make_string("h"), make_string("H"));
  alist_set(&shift_map_alist, make_string("j"), make_string("J"));
  alist_set(&shift_map_alist, make_string("k"), make_string("K"));
  alist_set(&shift_map_alist, make_string("l"), make_string("L"));
  alist_set(&shift_map_alist, make_string(";"), make_string(":"));
  alist_set(&shift_map_alist, make_string("'"), make_string("\""));
  alist_set(&shift_map_alist, make_string("z"), make_string("Z"));
  alist_set(&shift_map_alist, make_string("x"), make_string("X"));
  alist_set(&shift_map_alist, make_string("c"), make_string("C"));
  alist_set(&shift_map_alist, make_string("v"), make_string("V"));
  alist_set(&shift_map_alist, make_string("b"), make_string("B"));
  alist_set(&shift_map_alist, make_string("n"), make_string("N"));
  alist_set(&shift_map_alist, make_string("m"), make_string("M"));
  alist_set(&shift_map_alist, make_string(","), make_string("<"));
  alist_set(&shift_map_alist, make_string("."), make_string(">"));
  alist_set(&shift_map_alist, make_string("/"), make_string("?"));
  alist_set(&shift_map_alist, make_string("`"), make_string("~"));
  alist_set(&shift_map_alist, make_string("1"), make_string("!"));
  alist_set(&shift_map_alist, make_string("2"), make_string("@"));
  alist_set(&shift_map_alist, make_string("3"), make_string("#"));
  alist_set(&shift_map_alist, make_string("4"), make_string("$"));
  alist_set(&shift_map_alist, make_string("5"), make_string("%"));
  alist_set(&shift_map_alist, make_string("6"), make_string("^"));
  alist_set(&shift_map_alist, make_string("7"), make_string("&"));
  alist_set(&shift_map_alist, make_string("8"), make_string("*"));
  alist_set(&shift_map_alist, make_string("9"), make_string("("));
  alist_set(&shift_map_alist, make_string("0"), make_string(")"));
  alist_set(&shift_map_alist, make_string("-"), make_string("_"));
  alist_set(&shift_map_alist, make_string("="), make_string("+"));
  return shift_map_alist;
}

Atom default_keymap() {
  // Only the first character of the string is used for now.
  Atom keymap = make_empty_alist();
  alist_set(&keymap, make_string("CTRL"), nil);
  alist_set(&keymap, make_string("LEFT-CONTROL"), make_string("CTRL"));
  alist_set(&keymap, make_string("RIGHT-CONTROL"), make_string("CTRL"));
  alist_set(&keymap, make_string("SHFT"), default_shift_conversion_alist());
  alist_set(&keymap, make_string("LEFT-SHIFT"), make_string("SHFT"));
  alist_set(&keymap, make_string("RIGHT-SHIFT"), make_string("SHFT"));
  return keymap;
}

Atom default_environment() {
  Atom environment = env_create(nil);
  Atom keymap = default_keymap();
  env_set(environment, make_sym("KEYMAP"),   keymap);
  env_set(environment, make_sym("T"),        make_sym("T"));
  env_set(environment, make_sym("CAR"),      make_builtin(builtin_car,          builtin_car_docstring));
  env_set(environment, make_sym("CDR"),      make_builtin(builtin_cdr,          builtin_cdr_docstring));
  env_set(environment, make_sym("CONS"),     make_builtin(builtin_cons,         builtin_cons_docstring));
  env_set(environment, make_sym("NILP"),     make_builtin(builtin_nilp,         builtin_nilp_docstring));
  env_set(environment, make_sym("PAIRP"),    make_builtin(builtin_pairp,        builtin_pairp_docstring));
  env_set(environment, make_sym("SYMBOLP"),  make_builtin(builtin_symbolp,      builtin_symbolp_docstring));
  env_set(environment, make_sym("INTEGERP"), make_builtin(builtin_integerp,     builtin_integerp_docstring));
  env_set(environment, make_sym("BUILTINP"), make_builtin(builtin_builtinp,     builtin_builtinp_docstring));
  env_set(environment, make_sym("CLOSUREP"), make_builtin(builtin_closurep,     builtin_closurep_docstring));
  env_set(environment, make_sym("MACROP"),   make_builtin(builtin_macrop,       builtin_macrop_docstring));
  env_set(environment, make_sym("STRINGP"),  make_builtin(builtin_stringp,      builtin_stringp_docstring));
  env_set(environment, make_sym("+"),        make_builtin(builtin_add,          builtin_add_docstring));
  env_set(environment, make_sym("-"),        make_builtin(builtin_subtract,     builtin_subtract_docstring));
  env_set(environment, make_sym("*"),        make_builtin(builtin_multiply,     builtin_multiply_docstring));
  env_set(environment, make_sym("/"),        make_builtin(builtin_divide,       builtin_divide_docstring));
  env_set(environment, make_sym("!"),        make_builtin(builtin_not,          builtin_not_docstring));
  env_set(environment, make_sym("="),        make_builtin(builtin_numeq,        builtin_numeq_docstring));
  env_set(environment, make_sym("!="),       make_builtin(builtin_numnoteq,     builtin_numnoteq_docstring));
  env_set(environment, make_sym("<"),        make_builtin(builtin_numlt,        builtin_numlt_docstring));
  env_set(environment, make_sym("<="),       make_builtin(builtin_numlt_or_eq,  builtin_numlt_or_eq_docstring));
  env_set(environment, make_sym(">"),        make_builtin(builtin_numgt,        builtin_numgt_docstring));
  env_set(environment, make_sym(">="),       make_builtin(builtin_numgt_or_eq,  builtin_numgt_or_eq_docstring));
  env_set(environment, make_sym("EQ"),       make_builtin(builtin_eq,           builtin_eq_docstring));
  env_set(environment, make_sym("SAVE"),     make_builtin(builtin_save,         builtin_save_docstring));
  env_set(environment, make_sym("APPLY"),    make_builtin(builtin_apply,        builtin_apply_docstring));
  env_set(environment, make_sym("PRINT"),    make_builtin(builtin_print,        builtin_print_docstring));
  env_set(environment, make_sym("SYM"),      make_builtin(builtin_symbol_table, builtin_symbol_table_docstring));
  env_set(environment, make_sym("BUF"),      make_builtin(builtin_buffer_table, builtin_buffer_table_docstring));

  // FIXME: It's always best to keep docstrings out of here and where they belong.
  env_set(environment, make_sym("GARBAGE-COLLECTOR-ITERATIONS-THRESHOLD"), make_int_with_docstring
          (100000, "This number corresponds to the amount of evaluation operations before \
running the garbage collector.\nSmaller numbers mean memory is freed more often."));

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

  env_set(environment, make_sym("WHILE-RECURSE-LIMIT"), make_int_with_docstring
          (10000
           , "This is the maximum amount of times a while loop may loop.\n"
           "\n"
           "Used to prevent infinite loops."));

  env_set(environment, make_sym("SHIFT-CONVERSION-ALIST"), default_shift_conversion_alist());

  return environment;
}
