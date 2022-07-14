#ifndef LITE_BUILTINS_H
#define LITE_BUILTINS_H

struct Atom;
typedef struct Atom Atom;

typedef const char symbol_t;

// TYPES

extern symbol_t         *builtin_nilp_docstring;
int builtin_nilp        (Atom arguments, Atom *result);
extern symbol_t         *builtin_pairp_docstring;
int builtin_pairp       (Atom arguments, Atom *result);
extern symbol_t         *builtin_symbolp_docstring;
int builtin_symbolp     (Atom arguments, Atom *result);
extern symbol_t         *builtin_integerp_docstring;
int builtin_integerp    (Atom arguments, Atom *result);
extern symbol_t         *builtin_builtinp_docstring;
int builtin_builtinp    (Atom arguments, Atom *result);
extern symbol_t         *builtin_closurep_docstring;
int builtin_closurep    (Atom arguments, Atom *result);
extern symbol_t         *builtin_macrop_docstring;
int builtin_macrop      (Atom arguments, Atom *result);
extern symbol_t         *builtin_stringp_docstring;
int builtin_stringp     (Atom arguments, Atom *result);
extern symbol_t         *builtin_bufferp_docstring;
int builtin_bufferp     (Atom arguments, Atom *result);

// PAIRS

extern symbol_t         *builtin_cons_docstring;
int builtin_cons        (Atom arguments, Atom *result);
extern symbol_t         *builtin_car_docstring;
int builtin_car         (Atom arguments, Atom *result);
extern symbol_t         *builtin_cdr_docstring;
int builtin_cdr         (Atom arguments, Atom *result);
extern symbol_t         *builtin_setcar_docstring;
int builtin_setcar      (Atom arguments, Atom *result);

// LISTS

extern symbol_t         *builtin_member_docstring;
int builtin_member      (Atom arguments, Atom *result);

// LOGICAL

extern symbol_t         *builtin_not_docstring;
int builtin_not         (Atom arguments, Atom *result);
extern symbol_t         *builtin_eq_docstring;
int builtin_eq          (Atom arguments, Atom *result);

extern symbol_t         *builtin_numeq_docstring;
int builtin_numeq       (Atom arguments, Atom *result);
extern symbol_t         *builtin_numnoteq_docstring;
int builtin_numnoteq    (Atom arguments, Atom *result);
extern symbol_t         *builtin_numlt_docstring;
int builtin_numlt       (Atom arguments, Atom *result);
extern symbol_t         *builtin_numlt_or_eq_docstring;
int builtin_numlt_or_eq (Atom arguments, Atom *result);
extern symbol_t         *builtin_numgt_docstring;
int builtin_numgt       (Atom arguments, Atom *result);
extern symbol_t         *builtin_numgt_or_eq_docstring;
int builtin_numgt_or_eq (Atom arguments, Atom *result);

// MATHEMATICAL

extern symbol_t         *builtin_add_docstring;
int builtin_add         (Atom arguments, Atom *result);
extern symbol_t         *builtin_subtract_docstring;
int builtin_subtract    (Atom arguments, Atom *result);
extern symbol_t         *builtin_multiply_docstring;
int builtin_multiply    (Atom arguments, Atom *result);
extern symbol_t         *builtin_divide_docstring;
int builtin_divide      (Atom arguments, Atom *result);

// BUFFERS

extern symbol_t          *builtin_open_buffer_docstring;
int builtin_open_buffer  (Atom arguments, Atom *result);

extern symbol_t          *builtin_buffer_table_docstring;
int builtin_buffer_table (Atom arguments, Atom *result);
extern symbol_t          *builtin_buffer_insert_docstring;
int builtin_buffer_insert(Atom arguments, Atom *result);
extern symbol_t          *builtin_buffer_remove_docstring;
int builtin_buffer_remove(Atom arguments, Atom *result);
extern symbol_t          *builtin_buffer_remove_forward_docstring;
int builtin_buffer_remove_forward(Atom arguments, Atom *result);

extern symbol_t          *builtin_buffer_set_point_docstring;
int builtin_buffer_set_point(Atom arguments, Atom *result);
extern symbol_t          *builtin_buffer_point_docstring;
int builtin_buffer_point (Atom arguments, Atom *result);

extern symbol_t          *builtin_buffer_index_docstring;
int builtin_buffer_index (Atom arguments, Atom *result);
extern symbol_t          *builtin_buffer_string_docstring;
int builtin_buffer_string(Atom arguments, Atom *result);
extern symbol_t          *builtin_buffer_lines_docstring;
int builtin_buffer_lines (Atom arguments, Atom *result);
extern symbol_t          *builtin_buffer_line_docstring;
int builtin_buffer_line  (Atom arguments, Atom *result);
extern symbol_t          *builtin_buffer_current_line_docstring;
int builtin_buffer_current_line(Atom arguments, Atom *result);

extern symbol_t          *builtin_buffer_seek_byte_docstring;
int builtin_buffer_seek_byte(Atom arguments, Atom *result);
extern symbol_t          *builtin_buffer_seek_substring_docstring;
int builtin_buffer_seek_substring(Atom arguments, Atom *result);

extern symbol_t         *builtin_save_docstring;
int builtin_save        (Atom arguments, Atom *result);

// STRINGS

extern symbol_t         *builtin_string_length_docstring;
int builtin_string_length(Atom arguments, Atom *result);

// OTHER

extern symbol_t         *builtin_copy_docstring;
int builtin_copy        (Atom arguments, Atom *result);
extern symbol_t         *builtin_evaluate_docstring;
int builtin_evaluate    (Atom arguments, Atom *result);
extern symbol_t         *builtin_evaluate_string_docstring;
int builtin_evaluate_string(Atom arguments, Atom *result);
extern symbol_t         *builtin_evaluate_file_docstring;
int builtin_evaluate_file(Atom arguments, Atom *result);
extern symbol_t         *builtin_apply_docstring;
int builtin_apply       (Atom arguments, Atom *result);
extern symbol_t         *builtin_symbol_table_docstring;
int builtin_symbol_table(Atom arguments, Atom *result);
extern symbol_t         *builtin_print_docstring;
int builtin_print       (Atom arguments, Atom *result);
extern symbol_t         *builtin_read_prompted_docstring;
int builtin_read_prompted(Atom arguments, Atom *result);
extern symbol_t         *builtin_finish_read_docstring;
int builtin_finish_read (Atom arguments, Atom *result);

#endif /* LITE_BUILTINS_H */
