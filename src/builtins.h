#ifndef LITE_BUILTINS_H
#define LITE_BUILTINS_H

struct Atom;
typedef struct Atom Atom;

// TYPES

extern const char *const builtin_nilp_name;
extern const char *const builtin_nilp_docstring;
int builtin_nilp(Atom arguments, Atom *result);
extern const char *const builtin_pairp_name;
extern const char *const builtin_pairp_docstring;
int builtin_pairp(Atom arguments, Atom *result);
extern const char *const builtin_symbolp_name;
extern const char *const builtin_symbolp_docstring;
int builtin_symbolp(Atom arguments, Atom *result);
extern const char *const builtin_integerp_name;
extern const char *const builtin_integerp_docstring;
int builtin_integerp(Atom arguments, Atom *result);
extern const char *const builtin_builtinp_name;
extern const char *const builtin_builtinp_docstring;
int builtin_builtinp(Atom arguments, Atom *result);
extern const char *const builtin_closurep_name;
extern const char *const builtin_closurep_docstring;
int builtin_closurep(Atom arguments, Atom *result);
extern const char *const builtin_macrop_name;
extern const char *const builtin_macrop_docstring;
int builtin_macrop(Atom arguments, Atom *result);
extern const char *const builtin_stringp_name;
extern const char *const builtin_stringp_docstring;
int builtin_stringp(Atom arguments, Atom *result);
extern const char *const builtin_bufferp_name;
extern const char *const builtin_bufferp_docstring;
int builtin_bufferp(Atom arguments, Atom *result);

// PAIRS

extern const char *const builtin_cons_name;
extern const char *const builtin_cons_docstring;
int builtin_cons(Atom arguments, Atom *result);
extern const char *const builtin_car_name;
extern const char *const builtin_car_docstring;
int builtin_car(Atom arguments, Atom *result);
extern const char *const builtin_cdr_name;
extern const char *const builtin_cdr_docstring;
int builtin_cdr(Atom arguments, Atom *result);
extern const char *const builtin_setcar_name;
extern const char *const builtin_setcar_docstring;
int builtin_setcar(Atom arguments, Atom *result);
extern const char *const builtin_setcdr_name;
extern const char *const builtin_setcdr_docstring;
int builtin_setcdr(Atom arguments, Atom *result);

// LISTS

extern const char *const builtin_member_name;
extern const char *const builtin_member_docstring;
int builtin_member(Atom arguments, Atom *result);

// LOGICAL

extern const char *const builtin_not_name;
extern const char *const builtin_not_docstring;
int builtin_not(Atom arguments, Atom *result);
extern const char *const builtin_eq_name;
extern const char *const builtin_eq_docstring;
int builtin_eq(Atom arguments, Atom *result);

extern const char *const builtin_numeq_name;
extern const char *const builtin_numeq_docstring;
int builtin_numeq(Atom arguments, Atom *result);
extern const char *const builtin_numnoteq_name;
extern const char *const builtin_numnoteq_docstring;
int builtin_numnoteq(Atom arguments, Atom *result);
extern const char *const builtin_numlt_name;
extern const char *const builtin_numlt_docstring;
int builtin_numlt(Atom arguments, Atom *result);
extern const char *const builtin_numlt_or_eq_name;
extern const char *const builtin_numlt_or_eq_docstring;
int builtin_numlt_or_eq(Atom arguments, Atom *result);
extern const char *const builtin_numgt_name;
extern const char *const builtin_numgt_docstring;
int builtin_numgt(Atom arguments, Atom *result);
extern const char *const builtin_numgt_or_eq_name;
extern const char *const builtin_numgt_or_eq_docstring;
int builtin_numgt_or_eq(Atom arguments, Atom *result);

// MATHEMATICAL

extern const char *const builtin_add_name;
extern const char *const builtin_add_docstring;
int builtin_add(Atom arguments, Atom *result);
extern const char *const builtin_subtract_name;
extern const char *const builtin_subtract_docstring;
int builtin_subtract(Atom arguments, Atom *result);
extern const char *const builtin_multiply_name;
extern const char *const builtin_multiply_docstring;
int builtin_multiply(Atom arguments, Atom *result);
extern const char *const builtin_divide_name;
extern const char *const builtin_divide_docstring;
int builtin_divide(Atom arguments, Atom *result);
extern const char *const builtin_remainder_name;
extern const char *const builtin_remainder_docstring;
int builtin_remainder(Atom arguments, Atom *result);

// BUFFERS

extern const char *const builtin_buffer_toggle_mark_name;
extern const char *const builtin_buffer_toggle_mark_docstring;
int builtin_buffer_toggle_mark(Atom arguments, Atom *result);

extern const char *const builtin_buffer_set_mark_name;
extern const char *const builtin_buffer_set_mark_docstring;
int builtin_buffer_set_mark(Atom arguments, Atom *result);

extern const char *const builtin_buffer_mark_name;
extern const char *const builtin_buffer_mark_docstring;
int builtin_buffer_mark(Atom arguments, Atom *result);

extern const char *const builtin_buffer_mark_activated_name;
extern const char *const builtin_buffer_mark_activated_docstring;
int builtin_buffer_mark_activated(Atom arguments, Atom *result);

extern const char *const builtin_buffer_region_name;
extern const char *const builtin_buffer_region_docstring;
int builtin_buffer_region(Atom arguments, Atom *result);

extern const char *const builtin_buffer_region_length_name;
extern const char *const builtin_buffer_region_length_docstring;
int builtin_buffer_region_length(Atom arguments, Atom *result);

extern const char *const builtin_open_buffer_name;
extern const char *const builtin_open_buffer_docstring;
int builtin_open_buffer(Atom arguments, Atom *result);

extern const char *const builtin_buffer_path_name;
extern const char *const builtin_buffer_path_docstring;
int builtin_buffer_path(Atom arguments, Atom *result);

extern const char *const builtin_buffer_table_name;
extern const char *const builtin_buffer_table_docstring;
int builtin_buffer_table(Atom arguments, Atom *result);
extern const char *const builtin_buffer_insert_name;
extern const char *const builtin_buffer_insert_docstring;
int builtin_buffer_insert(Atom arguments, Atom *result);
extern const char *const builtin_buffer_remove_name;
extern const char *const builtin_buffer_remove_docstring;
int builtin_buffer_remove(Atom arguments, Atom *result);
extern const char *const builtin_buffer_remove_forward_name;
extern const char *const builtin_buffer_remove_forward_docstring;
int builtin_buffer_remove_forward(Atom arguments, Atom *result);

extern const char *const builtin_buffer_set_point_name;
extern const char *const builtin_buffer_set_point_docstring;
int builtin_buffer_set_point(Atom arguments, Atom *result);
extern const char *const builtin_buffer_point_name;
extern const char *const builtin_buffer_point_docstring;
int builtin_buffer_point(Atom arguments, Atom *result);

extern const char *const builtin_buffer_index_name;
extern const char *const builtin_buffer_index_docstring;
int builtin_buffer_index(Atom arguments, Atom *result);
extern const char *const builtin_buffer_string_name;
extern const char *const builtin_buffer_string_docstring;
int builtin_buffer_string(Atom arguments, Atom *result);
extern const char *const builtin_buffer_lines_name;
extern const char *const builtin_buffer_lines_docstring;
int builtin_buffer_lines(Atom arguments, Atom *result);
extern const char *const builtin_buffer_line_name;
extern const char *const builtin_buffer_line_docstring;
int builtin_buffer_line(Atom arguments, Atom *result);
extern const char *const builtin_buffer_current_line_name;
extern const char *const builtin_buffer_current_line_docstring;
int builtin_buffer_current_line(Atom arguments, Atom *result);

extern const char *const builtin_buffer_seek_byte_name;
extern const char *const builtin_buffer_seek_byte_docstring;
int builtin_buffer_seek_byte(Atom arguments, Atom *result);
extern const char *const builtin_buffer_seek_substring_name;
extern const char *const builtin_buffer_seek_substring_docstring;
int builtin_buffer_seek_substring(Atom arguments, Atom *result);

extern const char *const builtin_save_name;
extern const char *const builtin_save_docstring;
int builtin_save(Atom arguments, Atom *result);

// STRINGS

extern const char *const builtin_string_length_name;
extern const char *const builtin_string_length_docstring;
int builtin_string_length(Atom arguments, Atom *result);

// OTHER

extern const char *const builtin_copy_name;
extern const char *const builtin_copy_docstring;
int builtin_copy(Atom arguments, Atom *result);
extern const char *const builtin_evaluate_string_name;
extern const char *const builtin_evaluate_string_docstring;
int builtin_evaluate_string(Atom arguments, Atom *result);
extern const char *const builtin_evaluate_file_name;
extern const char *const builtin_evaluate_file_docstring;
int builtin_evaluate_file(Atom arguments, Atom *result);
extern const char *const builtin_apply_name;
extern const char *const builtin_apply_docstring;
int builtin_apply(Atom arguments, Atom *result);
extern const char *const builtin_symbol_table_name;
extern const char *const builtin_symbol_table_docstring;
int builtin_symbol_table(Atom arguments, Atom *result);
extern const char *const builtin_print_name;
extern const char *const builtin_print_docstring;
int builtin_print(Atom arguments, Atom *result);
extern const char *const builtin_read_prompted_name;
extern const char *const builtin_read_prompted_docstring;
int builtin_read_prompted(Atom arguments, Atom *result);
extern const char *const builtin_finish_read_name;
extern const char *const builtin_finish_read_docstring;
int builtin_finish_read(Atom arguments, Atom *result);

extern const char *const builtin_clipboard_cut_name;
extern const char *const builtin_clipboard_cut_docstring;
int builtin_clipboard_cut(Atom arguments, Atom *result);
extern const char *const builtin_clipboard_copy_name;
extern const char *const builtin_clipboard_copy_docstring;
int builtin_clipboard_copy(Atom arguments, Atom *result);
extern const char *const builtin_clipboard_paste_name;
extern const char *const builtin_clipboard_paste_docstring;
int builtin_clipboard_paste(Atom arguments, Atom *result);

#ifdef LITE_GFX

extern const char *const builtin_change_font_name;
extern const char *const builtin_change_font_docstring;
int builtin_change_font(Atom arguments, Atom *result);
extern const char *const builtin_change_font_size_name;
extern const char *const builtin_change_font_size_docstring;
int builtin_change_font_size(Atom arguments, Atom *result);

extern const char *const builtin_window_size_name;
extern const char *const builtin_window_size_docstring;
int builtin_window_size(Atom arguments, Atom *result);
extern const char *const builtin_change_window_size_name;
extern const char *const builtin_change_window_size_docstring;
int builtin_change_window_size(Atom arguments, Atom *result);
extern const char *const builtin_change_window_mode_name;
extern const char *const builtin_change_window_mode_docstring;
int builtin_change_window_mode(Atom arguments, Atom *result);

#endif /* #ifdef LITE_GFX */

#endif /* LITE_BUILTINS_H */
