#include <parser.h>

#include <ctype.h>
#include <error.h>
#include <types.h>
#include <stdlib.h>
#include <string.h>

/// Given a SOURCE, get the next token, and point to it with BEG and END.
int lex(const char *source, const char **beg, const char **end) {
  const char *ws = " \t\n";
  const char *delimiter = "() \t\n";
  const char *prefix = "() \t\n";
  // Eat all preceding whitespace.
  source += strspn(source, ws);
  if (source[0] == '\0') {
    *beg = NULL;
    *end = NULL;
    return ERROR_SYNTAX;
  }
  *beg = source;
  if (strchr(prefix, source[0]) != NULL) {
    *end = source + 1;
  } else {
    *end = source + strcspn(source, delimiter);
  }
  return ERROR_NONE;
}

int parse_simple(const char *beg, const char *end, Atom *result) {
  char *buffer;
  char *p;
  // INTEGER
  // FIXME: strtoll ONLY WORKS WHEN integer_t IS OF TYPE long long.
  integer_t value = (integer_t)strtoll(beg, &p, 10);
  if (p == end) {
    result->type = ATOM_TYPE_INTEGER;
    result->value.integer = value;
    return ERROR_NONE;
  }
  // NIL or SYMBOL
  buffer = malloc(end - beg + 1);
  p = buffer;
  while (beg != end) {
    // A comma?!?!?!
    *p++ = toupper(*beg), ++beg;
  }
  *p = '\0';
  if (memcmp(buffer, "NIL", 3) == 0) {
    *result = nil;
  } else {
    *result = make_sym(buffer);
  }
  free(buffer);
  return ERROR_NONE;
}

int parse_list(const char *beg, const char **end, Atom *result) {
  Atom p;
  p = nil;
  *result = nil;
  *end = beg;
  for (;;) {
    const char *token;
    Atom item;
    enum Error err;
    err = lex(*end, &token, end);
    if (err) { return err; }
    // End of list.
    if (token[0] == ')') {
      return ERROR_NONE;
    }
    // Improper list.
    if (token[0] == '.' && *end - token == 1) {
      if (nilp(p)) {
        return ERROR_SYNTAX;
      }
      err = parse_expr(*end, end, &item);
      if (err) { return err; }
      cdr(p) = item;
      // Closing ')'
      err = lex(*end, &token, end);
      if (!err && token[0] != ')') {
        err = ERROR_SYNTAX;
      }
      return err;
    }
    err = parse_expr(token, end, &item);
    if (err) { return err; }
    if (nilp(p)) {
      // First item in list.
      *result = cons(item, nil);
      p = *result;
    } else {
      cdr(p) = cons(item, nil);
      p = cdr(p);
    }
  }
}

/// Eat the next LISP object from source.
int parse_expr(const char *source, const char **end, Atom *result) {
  const char *token;
  enum Error err = lex(source, &token, end);
  if (err) { return err; }
  if (token[0] == '(') {
    return parse_list(*end, end, result);
  }
  else if (token[0] == ')') {
    return ERROR_SYNTAX;
  } else {
    return parse_simple(token, *end, result);
  }
}
