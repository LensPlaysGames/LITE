#include <parser.h>

#include <ctype.h>
#include <error.h>
#include <types.h>
#include <stdlib.h>
#include <string.h>

/// Given a SOURCE, get the next token, and point to it with BEG and END.
int lex(const char *source, const char **beg, const char **end) {
  const char *ws = " \t\n";
  const char *delimiter = "()\" \t\n";
  const char *prefix = "()\'`";
  // TODO: Move whitespace + comment skip to helper function.
  // Eat all preceding whitespace.
  source += strspn(source, ws);
  if (source[0] == '\0') {
    *beg = NULL;
    *end = NULL;
    return ERROR_SYNTAX;
  }
  while (source[0] == ';') {
    // Eat line following comment delimiter.
    source = strchr(source, '\n');
    // Nothing in source left except comment(s).
    if (source == NULL) {
      *beg = NULL;
      *end = NULL;
      return ERROR_NONE;
    }
    // Eat preceding whitespace before delimiter check.
    source += strspn(source, ws);
  }
  *beg = source;
  if (strchr(prefix, source[0]) != NULL) {
    *end = source + 1;
  } else if (source[0] == ',') {
    *end = source + (source[1] == '@' ? 2 : 1);
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
  if (!buffer) {
    return ERROR_MEMORY;
  }
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
  Atom list = nil;
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
      if (nilp(list)) {
        return ERROR_SYNTAX;
      }
      err = parse_expr(*end, end, &item);
      if (err) { return err; }
      cdr(list) = item;
      // Closing ')'
      err = lex(*end, &token, end);
      if (!err && token[0] != ')') {
        err = ERROR_SYNTAX;
      }
      return err;
    }
    err = parse_expr(token, end, &item);
    if (err) { return err; }
    if (nilp(list)) {
      // First item in list.
      *result = cons(item, nil);
      list = *result;
    } else {
      cdr(list) = cons(item, nil);
      list = cdr(list);
    }
  }
}

int parse_string(const char *beg, const char **end, Atom *result) {
  Atom string;
  string.type = ATOM_TYPE_STRING;
  *result = nil;
  *end = beg;
  if (beg[0] != '"') {
    return ERROR_SYNTAX;
  }
  // Find end double quote.
  // Opening quote is eaten here.
  const char *p = beg + 1;
  while (*p && *p != '"') {
    p++;
  }
  if (!*p) {
    return ERROR_SYNTAX;
  }
  // Closing quote is eaten here.
  *end = p + 1;
  size_t string_length = *end - beg - 2;
  char *name = malloc(string_length + 1);
  if (!name) {
    return ERROR_MEMORY;
  }
  memcpy(name, beg + 1, string_length);
  name[string_length] = '\0';
  string.value.symbol = name;
  *result = string;
  return ERROR_NONE;
}

/// Eat the next LISP object from source.
int parse_expr(const char *source, const char **end, Atom *result) {
  const char *token;
  enum Error err = lex(source, &token, end);
  if (err) { return err; }
  if (token[0] == '(') {
    return parse_list(*end, end, result);
  } else if (token[0] == ')') {
    return ERROR_SYNTAX;
  } else if (token[0] == '"') {
    return parse_string(*end, end, result);
  } else if (token[0] == '\'') {
    *result = cons(make_sym("QUOTE"), cons(nil, nil));
    return parse_expr(*end, end, &car(cdr(*result)));
  } else if (token[0] == '`') {
    *result = cons(make_sym("QUASIQUOTE"), cons(nil, nil));
    return parse_expr(*end, end, &car(cdr(*result)));
  } else if (token[0] == ',') {
    const char *symbol = token[1] == '@' ? "UNQUOTE-SPLICING" : "UNQUOTE";
    *result = cons(make_sym(symbol), cons(nil, nil));
    return parse_expr(*end, end, &car(cdr(*result)));
  } else {
    return parse_simple(token, *end, result);
  }
}
