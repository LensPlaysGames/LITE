#ifndef LITE_PARSER_H
#define LITE_PARSER_H

struct Atom;
typedef struct Atom Atom;

int parse_simple(const char *beg, const char *end, Atom *result);
int parse_list(const char *beg, const char **end, Atom *result);
/// Eat the next LISP object from source.
int parse_expr(const char *source, const char **end, Atom *result);

#endif /* #ifdef LITE_PARSER_H */
