#ifndef LITE_EVALUATION_H
#define LITE_EVALUATION_H

#include <error.h>
#include <types.h>

Error evaluate_expression(Atom expr, Atom environment, Atom *result);

#endif /* LITE_EVALUATION_H */
