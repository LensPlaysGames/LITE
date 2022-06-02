#ifndef LITE_PARSER_H
#define LITE_PARSER_H

#include <error.h>

struct Atom;
typedef struct Atom Atom;

Error parse_simple(const char *beg, const char *end, Atom *result);
Error parse_list(const char *beg, const char **end, Atom *result);
/// Eat the next LISP object from source.
Error parse_expr(const char *source, const char **end, Atom *result);

#endif /* #ifdef LITE_PARSER_H */
