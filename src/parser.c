#include <parser.h>

#include <ctype.h>
#include <error.h>
#include <types.h>
#include <stdlib.h>
#include <string.h>

// Whitespace characters that may safely be skipped by the lexer.
const char *lite_ws = " \t\r\n\e\f\v";
// Delimiting characters separate different atoms.
// NOTE: lite_delimiters is suffixed with lite_ws.
const char *lite_delimiters = "()\" \t\r\n\e\f\v";
// Characters that may come before an atom but not after.
const char *lite_prefixes = "()\'`";

# define seek_past(str, chars)  (str += strspn((str),  (chars)))
# define seek_until(str, chars) (str += strcspn((str), (chars)))

/// Given a SOURCE, get the next token, and point to it with BEG and END.
Error lex(const char *source, const char **beg, const char **end) {
  // Eat all preceding whitespace.
  seek_past(source, lite_ws);
  if (source[0] == '\0') {
    *beg = NULL;
    *end = NULL;
    MAKE_ERROR(err, ERROR_SYNTAX, nil, "Can not lex empty input.", NULL);
    return err;
  }
  while (source[0] == ';') {
    // Eat line following comment delimiter.
    source = strchr(source, '\n');
    if (source == NULL) {
      // Nothing in source left except comment(s).
      *beg = NULL;
      *end = NULL;
      return ok;
    }
    // Eat preceding whitespace before delimiter check.
    seek_past(source, lite_ws);
  }
  *beg = source;
  if (strchr(lite_prefixes, source[0]) != NULL) {
    *end = source + 1;
  } else if (source[0] == ',') {
    *end = source + (source[1] == '@' ? 2 : 1);
  } else {
    *end = source;
    seek_until(*end, lite_delimiters);
  }
  return ok;
}
# undef seek_past
# undef seek_until

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
  if (strlen(buffer) == 0 || buffer[0] == ' ' || buffer[0] == '\n' || buffer[0] == '\r') {
    MAKE_ERROR(err, ERROR_SYNTAX, nil
               , "Zero-length symbol is not allowed."
               , NULL);
    return err;
  }
  if (strcmp(buffer, "NIL") == 0) {
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
    err = lex(*end, &token, end);
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
  *result = nil;
  *end = beg;
  Error err = ok;
  if (beg[0] != '"') {
    PREP_ERROR(err, ERROR_SYNTAX, nil
               , "A string is expected to begin with a double quote."
               , "This is likely an internal error. Consider making an issue on GitHub.")
    return err;
  }
  // Opening quote is eaten here.
  const char *p = beg + 1;
  // Find end double quote.
  while (*p && *p != '"') { p++; }
  if (!*p) {
    PREP_ERROR(err, ERROR_SYNTAX, nil
               , "Expected a closing double quote."
               , NULL)
    return err;
  }
  // Closing quote is eaten here.
  *end = p + 1;
  size_t string_length = *end - beg - 2;
  // Allocate memory for string contents.
  char *contents = malloc(string_length + 1);
  if (!contents) {
    PREP_ERROR(err, ERROR_MEMORY, nil,
               "Could not allocate memory for new string contents.",
               NULL);
    return err;
  }
  memset(contents, 0, string_length);
  // Copy into allocated memory, handling escape sequences.
  // TODO: It would be cool to handle unicode characters with '\uXXXX',
  // as well as hex values with '\xXXXX'.
  size_t written_offset = 0;
  char prev_was_backslash = 0;
  char write_this_iteration = 1;
  p = beg + 1;
  while (*p && *p != '"' && written_offset <= string_length) {
    //printf("%zu: \"%.*s\" -> \"%s\"\n", written_offset, (int)string_length, beg + 1, contents);
    write_this_iteration = 1;
    if (prev_was_backslash) {
      write_this_iteration = 0;
      if (prev_was_backslash >= 2) {
        switch (*p) {
        case 'n':
          // "\\n" -> 0xa ('\n')
          contents[written_offset] = '\n';
          written_offset += 1;
          break;
        case 'r':
          // "\\r" -> 0xd ('\r')
          contents[written_offset] = '\r';
          written_offset += 1;
          break;
        default:
          // Unrecognized escape sequence, print literally.
          contents[written_offset] = '\\';
          written_offset += 1;
          contents[written_offset] = '\\';
          written_offset += 1;
          write_this_iteration = 1;
          break;
        }
      }
    }
    if (*p == '\\') {
      prev_was_backslash += 1;
      // skip backslash, do not write this iteration.
      write_this_iteration = 0;
    } else {
      prev_was_backslash = 0;
    }
    if (write_this_iteration) {
      contents[written_offset] = *p;
      written_offset += 1;
    }
    p++;
  }
  contents[written_offset] = '\0';
  contents[string_length] = '\0';
  //printf("Parsed string contents: \"%s\"\n", contents);
  // Make LISP String Atom from C-style string.
  *result = make_string(contents);
  free(contents);
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
