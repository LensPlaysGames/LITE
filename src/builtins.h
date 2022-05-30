#ifndef LITE_BUILTINS_H
#define LITE_BUILTINS_H

struct Atom;
typedef struct Atom Atom;

int builtin_car         (Atom arguments, Atom *result);
int builtin_cdr         (Atom arguments, Atom *result);
int builtin_cons        (Atom arguments, Atom *result);
int builtin_add         (Atom arguments, Atom *result);
int builtin_subtract    (Atom arguments, Atom *result);
int builtin_multiply    (Atom arguments, Atom *result);
int builtin_divide      (Atom arguments, Atom *result);
int builtin_numeq       (Atom arguments, Atom *result);
int builtin_numlt       (Atom arguments, Atom *result);
int builtin_numlt_or_eq (Atom arguments, Atom *result);
int builtin_numgt       (Atom arguments, Atom *result);
int builtin_numgt_or_eq (Atom arguments, Atom *result);
int builtin_apply       (Atom arguments, Atom *result);
int builtin_pairp       (Atom arguments, Atom *result);
int builtin_eq          (Atom arguments, Atom *result);

#endif /* LITE_BUILTINS_H */
