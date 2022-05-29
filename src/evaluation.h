#ifndef LITE_EVALUATION_H
#define LITE_EVALUATION_H

struct Atom;
typedef struct Atom Atom;

int apply(Atom function, Atom arguments, Atom *result);
int evaluate_expr(Atom expr, Atom environment, Atom *result);

#endif /* LITE_EVALUATION_H */
