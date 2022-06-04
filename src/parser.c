#include <parser.h>

#include <ctype.h>
#include <error.h>
#include <types.h>
#include <stdlib.h>
#include <string.h>

/// Given a SOURCE, get the next token, and point to it with BEG and END.
Error lex(const char *source, const char **beg, const char **end) {
  const char *ws = " \t\n";
  const char *delimiter = "()\" \t\n";
  const char *prefix = "()\'`";
  // Eat all preceding whitespace.
  source += strspn(source, ws);
  if (source[0] == '\0') {
    *beg = NULL;
    *end = NULL;
    MAKE_ERROR(err, ERROR_SYNTAX, nil, "Can not lex empty input.", NULL);
    return err;
  }
  while (source[0] == ';') {
    // Eat line following comment delimiter.
    source = strchr(source, '\n');
    // Nothing in source left except comment(s).
    if (source == NULL) {
      *beg = NULL;
      *end = NULL;
      return ok;
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
  return ok;
}

Error parse_simple(const char *beg, const char *end, Atom *result) {
  char *buffer;
  char *p;
  // INTEGER
  // FIXME: strtoll ONLY WORKS WHEN integer_t IS OF TYPE long long.
  integer_t value = (integer_t)strtoll(beg, &p, 10);
  if (p == end) {
    result->type = ATOM_TYPE_INTEGER;
    result->value.integer = value;
    result->docstring = NULL;
    result->galloc = NULL;
    return ok;
  }
  // NIL or SYMBOL
  buffer = malloc(end - beg + 1);
  if (!buffer) {
    MAKE_ERROR(err, ERROR_MEMORY, nil
               , "Could not allocate buffer to read symbol."
               , NULL);
    return err;
  }
  p = buffer;
  while (beg != end) {
    *p++ = toupper(*beg);
    ++beg;
  }
  *p = '\0';
  if (memcmp(buffer, "NIL", 3) == 0) {
    *result = nil;
  } else {
    *result = make_sym(buffer);
  }
  free(buffer);
  return ok;
}

Error parse_list(const char *beg, const char **end, Atom *result) {
  Atom list = nil;
  *result = nil;
  *end = beg;
  Error err;
  for (;;) {
    const char *token;
    Atom item;
    Error err = lex(*end, &token, end);
    if (err.type) { return err; }
    // End of list.
    if (token[0] == ')') {
      return ok;
    }
    // Improper list.
    if (token[0] == '.' && *end - token == 1) {
      if (nilp(list)) {
        PREP_ERROR(err, ERROR_SYNTAX, nil
                   , "There must be at least one object on the left side \
of the `.` improper list operator."
                   , NULL);
        return err;
      }
      err = parse_expr(*end, end, &item);
      if (err.type) { return err; }
      cdr(list) = item;
      // Closing ')'
      err = lex(*end, &token, end);
      if (!err.type && token[0] != ')') {
        PREP_ERROR(err, ERROR_SYNTAX, nil
                   , "Expected a closing parenthesis following a list."
                   , NULL);
      }
      return err;
    }
    err = parse_expr(token, end, &item);
    if (err.type) { return err; }
    if (nilp(list)) {
      // Create list with one item.
      *result = cons(item, nil);
      list = *result;
    } else {
      cdr(list) = cons(item, nil);
      list = cdr(list);
    }
  }
  return ok;
}

Error parse_string(const char *beg, const char **end, Atom *result) {
  Atom string = nil;
  string.type = ATOM_TYPE_STRING;
  *result = nil;
  *end = beg;
  Error err = ok;
  if (beg[0] != '"') {
    PREP_ERROR(err, ERROR_SYNTAX, nil
               , "A string is expected to begin with a double quote."
               , "This is likely an internal error. Consider making an issue on GitHub.")
    return err;
  }
  // Find end double quote.
  // Opening quote is eaten here.
  const char *p = beg + 1;
  while (*p && *p != '"') {
    p++;
  }
  if (!*p) {
    PREP_ERROR(err, ERROR_SYNTAX, nil
               , "Expected a closing double quote."
               , NULL)
    return err;
  }
  // Closing quote is eaten here.
  *end = p + 1;
  size_t string_length = *end - beg - 2;
  string.docstring = NULL;
  // Allocate memory for string contents.
  char *contents = malloc(string_length + 1);
  memcpy(contents, beg + 1, string_length);
  contents[string_length] = '\0';
  string.value.symbol = contents;
  *result = string;
  return ok;
}

/// Eat the next LISP object from source.
Error parse_expr(const char *source, const char **end, Atom *result) {
  const char *token;
  Error err = lex(source, &token, end);
  if (err.type) { return err; }
  if (token[0] == '(') {
    return parse_list(*end, end, result);
  } else if (token[0] == ')') {
    PREP_ERROR(err, ERROR_SYNTAX, nil
               , "Extraneous closing parenthesis.", NULL);
    return err;
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
