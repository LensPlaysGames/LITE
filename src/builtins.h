#ifndef LITE_BUILTINS_H
#define LITE_BUILTINS_H

struct Atom;
typedef struct Atom Atom;

typedef const char symbol_t;

// PAIRS

extern symbol_t         *builtin_cons_docstring;
int builtin_cons        (Atom arguments, Atom *result);
extern symbol_t         *builtin_car_docstring;
int builtin_car         (Atom arguments, Atom *result);
extern symbol_t         *builtin_cdr_docstring;
int builtin_cdr         (Atom arguments, Atom *result);
extern symbol_t         *builtin_pairp_docstring;
int builtin_pairp       (Atom arguments, Atom *result);

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

// OTHER

extern symbol_t         *builtin_apply_docstring;
int builtin_apply       (Atom arguments, Atom *result);
extern symbol_t         *builtin_symbol_table_docstring;
int builtin_symbol_table(Atom arguments, Atom *result);
extern symbol_t         *builtin_print_docstring;
int builtin_print       (Atom arguments, Atom *result);

#endif /* LITE_BUILTINS_H */
