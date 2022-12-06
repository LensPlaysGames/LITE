#include <environment.h>

#ifdef LITE_GFX
#  include <api.h>
#  include <gui.h>
#endif /* #ifdef LITE_GFX */

#include <builtins.h>
#include <error.h>
#include <types.h>

char user_quit = 0;

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
  for (;;) {
    while (!nilp(bindings)) {
      Atom *bind = &car(bindings);
      if (bind->value.pair->atom[0].value.symbol == symbol.value.symbol) {
        *result = bind->value.pair->atom[1];
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
    bindings = cdr(parent);
    parent = car(parent);
  }
  MAKE_ERROR(err, ERROR_NOT_BOUND
             , symbol
             // FIXME: Which environment?
             , "Symbol is not bound in environment."
             , NULL);
  return err;
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
  env_set(environment, make_sym("T"), make_sym("T"));
  env_set(environment, make_sym((char *)builtin_quit_lisp_name),
          make_builtin(builtin_quit_lisp,
                       (char *)builtin_quit_lisp_name,
                       (char *)builtin_quit_lisp_docstring));
  env_set(environment, make_sym((char *)builtin_docstring_name),
          make_builtin(builtin_docstring,
                       (char *)builtin_docstring_name,
                       (char *)builtin_docstring_docstring));
  env_set(environment, make_sym((char *)builtin_car_name),
          make_builtin(builtin_car,
                       (char *)builtin_car_name,
                       (char *)builtin_car_docstring));
  env_set(environment, make_sym((char *)builtin_cdr_name),
          make_builtin(builtin_cdr,
                       (char *)builtin_cdr_name,
                       (char *)builtin_cdr_docstring));
  env_set(environment, make_sym((char *)builtin_cons_name),
          make_builtin(builtin_cons,
                       (char *)builtin_cons_name,
                       (char *)builtin_cons_docstring));
  env_set(environment, make_sym((char *)builtin_setcar_name),
          make_builtin(builtin_setcar,
                       (char *)builtin_setcar_name,
                       (char *)builtin_setcar_docstring));
  env_set(environment, make_sym((char *)builtin_setcdr_name),
          make_builtin(builtin_setcdr,
                       (char *)builtin_setcdr_name,
                       (char *)builtin_setcdr_docstring));
  env_set(environment, make_sym((char *)builtin_nilp_name),
          make_builtin(builtin_nilp,
                       (char *)builtin_nilp_name,
                       (char *)builtin_nilp_docstring));
  env_set(environment, make_sym((char *)builtin_pairp_name),
          make_builtin(builtin_pairp,
                       (char *)builtin_pairp_name,
                       (char *)builtin_pairp_docstring));
  env_set(environment, make_sym((char *)builtin_symbolp_name),
          make_builtin(builtin_symbolp,
                       (char *)builtin_symbolp_name,
                       (char *)builtin_symbolp_docstring));
  env_set(environment, make_sym((char *)builtin_integerp_name),
          make_builtin(builtin_integerp,
                       (char *)builtin_integerp_name,
                       (char *)builtin_integerp_docstring));
  env_set(environment, make_sym((char *)builtin_builtinp_name),
          make_builtin(builtin_builtinp,
                       (char *)builtin_builtinp_name,
                       (char *)builtin_builtinp_docstring));
  env_set(environment, make_sym((char *)builtin_closurep_name),
          make_builtin(builtin_closurep,
                       (char *)builtin_closurep_name,
                       (char *)builtin_closurep_docstring));
  env_set(environment, make_sym((char *)builtin_macrop_name),
          make_builtin(builtin_macrop,
                       (char *)builtin_macrop_name,
                       (char *)builtin_macrop_docstring));
  env_set(environment, make_sym((char *)builtin_stringp_name),
          make_builtin(builtin_stringp,
                       (char *)builtin_stringp_name,
                       (char *)builtin_stringp_docstring));
  env_set(environment, make_sym((char *)builtin_bufferp_name),
          make_builtin(builtin_bufferp,
                       (char *)builtin_bufferp_name,
                       (char *)builtin_bufferp_docstring));
  env_set(environment, make_sym((char *)builtin_add_name),
          make_builtin(builtin_add,
                       (char *)builtin_add_name,
                       (char *)builtin_add_docstring));
  env_set(environment, make_sym((char *)builtin_subtract_name),
          make_builtin(builtin_subtract,
                       (char *)builtin_subtract_name,
                       (char *)builtin_subtract_docstring));
  env_set(environment, make_sym((char *)builtin_multiply_name),
          make_builtin(builtin_multiply,
                       (char *)builtin_multiply_name,
                       (char *)builtin_multiply_docstring));
  env_set(environment, make_sym((char *)builtin_divide_name),
          make_builtin(builtin_divide,
                       (char *)builtin_divide_name,
                       (char *)builtin_divide_docstring));
  env_set(environment, make_sym((char *)builtin_remainder_name),
          make_builtin(builtin_remainder,
                       (char *)builtin_remainder_name,
                       (char *)builtin_remainder_docstring));
  env_set(environment, make_sym((char *)builtin_not_name),
          make_builtin(builtin_not,
                       (char *)builtin_not_name,
                       (char *)builtin_not_docstring));
  env_set(environment, make_sym((char *)builtin_numeq_name),
          make_builtin(builtin_numeq,
                       (char *)builtin_numeq_name,
                       (char *)builtin_numeq_docstring));
  env_set(environment, make_sym((char *)builtin_numnoteq_name),
          make_builtin(builtin_numnoteq,
                       (char *)builtin_numnoteq_name,
                       (char *)builtin_numnoteq_docstring));
  env_set(environment, make_sym((char *)builtin_numlt_name),
          make_builtin(builtin_numlt,
                       (char *)builtin_numlt_name,
                       (char *)builtin_numlt_docstring));
  env_set(environment, make_sym((char *)builtin_numlt_or_eq_name),
          make_builtin(builtin_numlt_or_eq,
                       (char *)builtin_numlt_or_eq_name,
                       (char *)builtin_numlt_or_eq_docstring));
  env_set(environment, make_sym((char *)builtin_numgt_name),
          make_builtin(builtin_numgt,
                       (char *)builtin_numgt_name,
                       (char *)builtin_numgt_docstring));
  env_set(environment, make_sym((char *)builtin_numgt_or_eq_name),
          make_builtin(builtin_numgt_or_eq,
                       (char *)builtin_numgt_or_eq_name,
                       (char *)builtin_numgt_or_eq_docstring));
  env_set(environment, make_sym((char *)builtin_eq_name),
          make_builtin(builtin_eq,
                       (char *)builtin_eq_name,
                       (char *)builtin_eq_docstring));
  env_set(environment, make_sym((char *)builtin_copy_name),
          make_builtin(builtin_copy,
                       (char *)builtin_copy_name,
                       (char *)builtin_copy_docstring));
  env_set(environment, make_sym((char *)builtin_save_name),
          make_builtin(builtin_save,
                       (char *)builtin_save_name,
                       (char *)builtin_save_docstring));
  env_set(environment, make_sym((char *)builtin_apply_name),
          make_builtin(builtin_apply,
                       (char *)builtin_apply_name,
                       (char *)builtin_apply_docstring));
  env_set(environment, make_sym((char *)builtin_print_name),
          make_builtin(builtin_print,
                       (char *)builtin_print_name,
                       (char *)builtin_print_docstring));
    env_set(environment, make_sym((char *)builtin_prins_name),
          make_builtin(builtin_prins,
                       (char *)builtin_prins_name,
                       (char *)builtin_prins_docstring));
  env_set(environment, make_sym((char *)builtin_symbol_table_name),
          make_builtin(builtin_symbol_table,
                       (char *)builtin_symbol_table_name,
                       (char *)builtin_symbol_table_docstring));

  env_set(environment, make_sym((char *)builtin_buffer_toggle_mark_name),
          make_builtin(builtin_buffer_toggle_mark,
                       (char *)builtin_buffer_toggle_mark_name,
                       (char *)builtin_buffer_toggle_mark_docstring));
  env_set(environment, make_sym((char *)builtin_buffer_set_mark_activation_name),
          make_builtin(builtin_buffer_set_mark_activation,
                       (char *)builtin_buffer_set_mark_activation_name,
                       (char *)builtin_buffer_set_mark_activation_docstring));
  env_set(environment, make_sym((char *)builtin_buffer_set_mark_name),
          make_builtin(builtin_buffer_set_mark,
                       (char *)builtin_buffer_set_mark_name,
                       (char *)builtin_buffer_set_mark_docstring));
  env_set(environment, make_sym((char *)builtin_buffer_mark_name),
          make_builtin(builtin_buffer_mark,
                       (char *)builtin_buffer_mark_name,
                       (char *)builtin_buffer_mark_docstring));
  env_set(environment, make_sym((char *)builtin_buffer_mark_activated_name),
          make_builtin(builtin_buffer_mark_activated,
                       (char *)builtin_buffer_mark_activated_name,
                       (char *)builtin_buffer_mark_activated_docstring));
  env_set(environment, make_sym((char *)builtin_buffer_region_name),
          make_builtin(builtin_buffer_region,
                       (char *)builtin_buffer_region_name,
                       (char *)builtin_buffer_region_docstring));
  env_set(environment, make_sym((char *)builtin_buffer_region_length_name),
          make_builtin(builtin_buffer_region_length,
                       (char *)builtin_buffer_region_length_name,
                       (char *)builtin_buffer_region_length_docstring));

  env_set(environment, make_sym((char *)builtin_buffer_table_name),
          make_builtin(builtin_buffer_table,
                       (char *)builtin_buffer_table_name,
                       (char *)builtin_buffer_table_docstring));
  env_set(environment, make_sym((char *)builtin_buffer_path_name),
          make_builtin(builtin_buffer_path,
                       (char *)builtin_buffer_path_name,
                       (char *)builtin_buffer_path_docstring));
  env_set(environment, make_sym((char *)builtin_open_buffer_name),
          make_builtin(builtin_open_buffer,
                       (char *)builtin_open_buffer_name,
                       (char *)builtin_open_buffer_docstring));
  env_set(environment, make_sym((char *)builtin_buffer_insert_name),
          make_builtin(builtin_buffer_insert,
                       (char *)builtin_buffer_insert_name,
                       (char *)builtin_buffer_insert_docstring));
  env_set(environment, make_sym((char *)builtin_buffer_remove_name),
          make_builtin(builtin_buffer_remove,
                       (char *)builtin_buffer_remove_name,
                       (char *)builtin_buffer_remove_docstring));
  env_set(environment, make_sym((char *)builtin_buffer_remove_forward_name),
          make_builtin(builtin_buffer_remove_forward,
                       (char *)builtin_buffer_remove_forward_name,
                       (char *)builtin_buffer_remove_forward_docstring));
  env_set(environment, make_sym((char *)builtin_buffer_string_name),
          make_builtin(builtin_buffer_string,
                       (char *)builtin_buffer_string_name,
                       (char *)builtin_buffer_string_docstring));
  env_set(environment, make_sym((char *)builtin_buffer_lines_name),
          make_builtin(builtin_buffer_lines,
                       (char *)builtin_buffer_lines_name,
                       (char *)builtin_buffer_lines_docstring));
  env_set(environment, make_sym((char *)builtin_buffer_line_name),
          make_builtin(builtin_buffer_line,
                       (char *)builtin_buffer_line_name,
                       (char *)builtin_buffer_line_docstring));
  env_set(environment, make_sym((char *)builtin_buffer_current_line_name),
          make_builtin(builtin_buffer_current_line,
                       (char *)builtin_buffer_current_line_name,
                       (char *)builtin_buffer_current_line_docstring));
  env_set(environment, make_sym((char *)builtin_buffer_set_point_name),
          make_builtin(builtin_buffer_set_point,
                       (char *)builtin_buffer_set_point_name,
                       (char *)builtin_buffer_set_point_docstring));
  env_set(environment, make_sym((char *)builtin_buffer_point_name),
          make_builtin(builtin_buffer_point,
                       (char *)builtin_buffer_point_name,
                       (char *)builtin_buffer_point_docstring));
  env_set(environment, make_sym((char *)builtin_buffer_index_name),
          make_builtin(builtin_buffer_index,
                       (char *)builtin_buffer_index_name,
                       (char *)builtin_buffer_index_docstring));
  env_set(environment, make_sym((char *)builtin_buffer_seek_byte_name),
          make_builtin(builtin_buffer_seek_byte,
                       (char *)builtin_buffer_seek_byte_name,
                       (char *)builtin_buffer_seek_byte_docstring));
  env_set(environment, make_sym((char *)builtin_buffer_seek_past_byte_name),
          make_builtin(builtin_buffer_seek_past_byte,
                       (char *)builtin_buffer_seek_past_byte_name,
                       (char *)builtin_buffer_seek_past_byte_docstring));
  env_set(environment, make_sym((char *)builtin_buffer_seek_substring_name),
          make_builtin(builtin_buffer_seek_substring,
                       (char *)builtin_buffer_seek_substring_name,
                       (char *)builtin_buffer_seek_substring_docstring));

  env_set(environment, make_sym((char *)builtin_read_prompted_name),
          make_builtin(builtin_read_prompted,
                       (char *)builtin_read_prompted_name,
                       (char *)builtin_read_prompted_docstring));
  env_set(environment, make_sym((char *)builtin_finish_read_name),
          make_builtin(builtin_finish_read,
                       (char *)builtin_finish_read_name,
                       (char *)builtin_finish_read_docstring));

  env_set(environment, make_sym((char *)builtin_string_length_name),
          make_builtin(builtin_string_length,
                       (char *)builtin_string_length_name,
                       (char *)builtin_string_length_docstring));
    env_set(environment, make_sym((char *)builtin_string_concat_name),
          make_builtin(builtin_string_concat,
                       (char *)builtin_string_concat_name,
                       (char *)builtin_string_concat_docstring));

  env_set(environment, make_sym((char *)builtin_evaluate_string_name),
          make_builtin(builtin_evaluate_string,
                       (char *)builtin_evaluate_string_name,
                       (char *)builtin_evaluate_string_docstring));
  env_set(environment, make_sym((char *)builtin_evaluate_file_name),
          make_builtin(builtin_evaluate_file,
                       (char *)builtin_evaluate_file_name,
                       (char *)builtin_evaluate_file_docstring));

  env_set(environment, make_sym((char *)builtin_member_name),
          make_builtin(builtin_member,
                       (char *)builtin_member_name,
                       (char *)builtin_member_docstring));

  env_set(environment, make_sym((char *)builtin_clipboard_cut_name),
          make_builtin(builtin_clipboard_cut,
                       (char *)builtin_clipboard_cut_name,
                       (char *)builtin_clipboard_cut_docstring));
  env_set(environment, make_sym((char *)builtin_clipboard_copy_name),
          make_builtin(builtin_clipboard_copy,
                       (char *)builtin_clipboard_copy_name,
                       (char *)builtin_clipboard_copy_docstring));
  env_set(environment, make_sym((char *)builtin_clipboard_paste_name),
          make_builtin(builtin_clipboard_paste,
                       (char *)builtin_clipboard_paste_name,
                       (char *)builtin_clipboard_paste_docstring));

  env_set(environment, make_sym((char *)builtin_change_font_name),
          make_builtin(builtin_change_font,
                       (char *)builtin_change_font_name,
                       (char *)builtin_change_font_docstring));
  env_set(environment, make_sym((char *)builtin_change_font_size_name),
          make_builtin(builtin_change_font_size,
                       (char *)builtin_change_font_size_name,
                       (char *)builtin_change_font_size_docstring));

  env_set(environment, make_sym((char *)builtin_window_size_name),
          make_builtin(builtin_window_size,
                       (char *)builtin_window_size_name,
                       (char *)builtin_window_size_docstring));
  env_set(environment, make_sym((char *)builtin_change_window_size_name),
          make_builtin(builtin_change_window_size,
                       (char *)builtin_change_window_size_name,
                       (char *)builtin_change_window_size_docstring));
  env_set(environment, make_sym((char *)builtin_change_window_mode_name),
          make_builtin(builtin_change_window_mode,
                       (char *)builtin_change_window_mode_name,
                       (char *)builtin_change_window_mode_docstring));

  env_set(environment, make_sym((char *)builtin_make_gui_property_name),
          make_builtin(builtin_make_gui_property,
                       (char *)builtin_make_gui_property_name,
                       (char *)builtin_make_gui_property_docstring));
  env_set(environment, make_sym((char *)builtin_set_gui_property_position_name),
          make_builtin(builtin_set_gui_property_position,
                       (char *)builtin_set_gui_property_position_name,
                       (char *)builtin_set_gui_property_position_docstring));
  env_set(environment, make_sym((char *)builtin_set_gui_property_color_name),
          make_builtin(builtin_set_gui_property_color,
                       (char *)builtin_set_gui_property_color_name,
                       (char *)builtin_set_gui_property_color_docstring));

  env_set(environment, make_sym((char *)builtin_scroll_up_name),
          make_builtin(builtin_scroll_up,
                       (char *)builtin_scroll_up_name,
                       (char *)builtin_scroll_up_docstring));
  env_set(environment, make_sym((char *)builtin_scroll_down_name),
          make_builtin(builtin_scroll_down,
                       (char *)builtin_scroll_down_name,
                       (char *)builtin_scroll_down_docstring));

  env_set(environment, make_sym((char *)"WHILE-RECURSE-LIMIT"), make_int_with_docstring
          (10000,
           "This is the maximum amount of times a while loop may loop.\n"
           "\n"
           "Used to prevent infinite loops."));

  env_set(environment, make_sym("GARBAGE-COLLECTOR-EVALUATION-ITERATIONS-THRESHOLD"),
          make_int_with_docstring
          (100000, "This number corresponds to the amount of evaluation operations before \
running the garbage collector.\nSmaller numbers mean memory is freed more often. \
The default value of '100000' means memory is freed in around twenty megabyte chunks."));

  env_set(environment, make_sym("GARBAGE-COLLECTOR-PAIR-ALLOCATIONS-THRESHOLD"),
          make_int_with_docstring
          ((integer_t)290500, "This number corresponds to the amount of pairs able to be allocated \
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
    //printf("Recreating global environment from defaults...\n");
    global_environment = default_environment();
  }
  return &global_environment;
}
