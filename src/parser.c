#include <parser.h>

#include <assert.h>
#include <ctype.h>
#include <error.h>
#include <types.h>
#include <stdlib.h>
#include <string.h>

// Whitespace characters that may safely be skipped by the lexer.
const char *lite_ws = " \t\r\n\f\v";
// Delimiting characters separate different atoms.
// NOTE: lite_delimiters is suffixed with lite_ws.
const char *lite_delimiters = "()\" \t\r\n\f\v";
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

/// Write to RESULT if an integer, nil, or a symbol can be parsed.
/// Otherwise, return an ERROR detailing why it could not be done.
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
    MAKE_ERROR(err, ERROR_MEMORY, nil,
               "Could not allocate buffer to read symbol.",
               NULL);
    return err;
  }
  p = buffer;
  while (beg != end) {
    *p++ = toupper(*beg);
    ++beg;
  }
  *p = '\0';
  if (strlen(buffer) == 0 || buffer[0] == ' ' || buffer[0] == '\n' || buffer[0] == '\r') {
    MAKE_ERROR(err, ERROR_SYNTAX, nil,
               "Zero-length symbol is not allowed.",
               NULL);
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

Error parse_string(const char *beg, const char **end, Atom *result) {
  *result = nil;
  *end = beg;
  Error err = ok;
  if (beg[0] != '"') {
    PREP_ERROR(err, ERROR_SYNTAX, nil,
               "A string is expected to begin with a double quote.",
               "This is likely an internal error. Consider making an issue on GitHub.");
    return err;
  }
  // Opening quote is eaten here.
  const char *p = beg + 1;
  // Find end double quote.
  while (*p) {
    if (*p == '"') {
      // After there have been two characters, check for escape sequence!
      if (p - 2 > beg && *(p - 2) == '\\' && *(p - 1) == '\\') {
        p++;
        continue;
      }
      break;
    }
    p++;
  }
  if (!*p) {
    PREP_ERROR(err, ERROR_SYNTAX, nil,
               "Expected a closing double quote.",
               NULL);
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
  while (*p && written_offset <= string_length) {
    //printf("%zu: \"%.*s\" -> \"%s\"\n", written_offset, (int)string_length, beg + 1, contents);
    write_this_iteration = 1;
    if (prev_was_backslash) {
      write_this_iteration = 0;
      if (prev_was_backslash >= 2) {
        if (*p == '\\') {
          // "\\\" -> "\"
          // Write the backslash that "falls off the end".
          contents[written_offset] = '\\';
          written_offset += 1;
        } else {
          switch (*p) {
          default:
            // Unrecognized escape sequence, print single backslash as
            // well as the current character.
            contents[written_offset] = '\\';
            written_offset += 1;
            contents[written_offset] = '\\';
            written_offset += 1;
            write_this_iteration = 1;
            break;
          case '_':
            // "\\_" -> ""
            // The unescape escape character, mostly for strings that
            // need a backslash at the end.
            break;
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
          case '"':
            // "\\"" -> '"'
            contents[written_offset] = '"';
            written_offset += 1;
            break;
          }
          prev_was_backslash = 0;
        }
      }
    }
    if (*p == '\\') {
      prev_was_backslash += 1;
      // skip backslash, do not write this iteration.
      write_this_iteration = 0;
    } else {
      if (prev_was_backslash == 1) {
        contents[written_offset] = '\\';
        written_offset += 1;
        write_this_iteration = 1;
      }
      prev_was_backslash = 0;
    }
    if (write_this_iteration) {
      if (*p == '"') { break; }
      contents[written_offset] = *p;
      written_offset += 1;
    }
    p++;
  }
  prev_was_backslash -= 1;
  while (--prev_was_backslash >= 0 && written_offset < string_length) {
    contents[written_offset] = '\\';
    written_offset += 1;
  }
  contents[written_offset] = '\0';
  contents[string_length] = '\0';
  //printf("Parsed string contents: \"%s\"\n", contents);
  // Make LISP String Atom from C-style string.
  *result = make_string(contents);
  free(contents);
  return ok;
}

typedef struct ParserStack {
  struct ParserStack *parent;
  const char* begin;
  Atom *working_list;
} ParserStack;

ParserStack *parser_make_frame
(ParserStack *parent, const char* beg, Atom *working_list) {
  ParserStack *frame = malloc(sizeof(ParserStack));
  assert(frame && "Failed to allocate memory in parser for new stack frame.");
  frame->parent = parent;
  frame->begin = beg;
  frame->working_list = working_list;
  return frame;
}

void parser_print_stackframe(ParserStack *stack, int depth) {
  if (!stack) { return; }
  do {
    int d = depth;
    while (--depth > 0) { putchar(' '); }
    printf("%.8s\n", stack->begin);
    depth = d;
    while (--depth > 0) { putchar(' '); }
    printf("list:   ");
    print_atom(*stack->working_list);
    putchar('\n');
    depth = d + 2;
    stack = stack->parent;
  } while (stack);
}

// (defs . body)

/// Eat the next LISP object from source.
Error parse_expr(const char *source, const char **end, Atom *result) {
  Error err = ok;
  if (!source || !end || !*end || !result) {
    PREP_ERROR(err, ERROR_ARGUMENTS, nil,
               "Can not parse expression when any given argument is NULL.",
               NULL);
    return err;
  }

  ParserStack *stack = NULL;

  *result = nil;
  Atom *working_result = result;
  Atom *list = NULL;
  Atom *working_list = NULL;
  char list_improper = 0;
  char *symbol = NULL;
  const char *token = NULL;
  *end = source;
  for (;;) {
    err = lex(*end, &token, end);
    if (err.type) { return err; }
    switch (token[0]) {
    default:
      err = parse_simple(token, *end, working_result);
      if (err.type) { return err; }
      if (!list) { return ok; }
      break;
    case '"':
      err = parse_string(*end, end, working_result);
      if (err.type) { return err; }
      if (!list) { return ok; }
      break;
    case '\'':
      *working_result = cons(make_sym("QUOTE"), cons(nil, nil));
      working_result = &car(cdr(*working_result));
      continue;
    case '`':
      *working_result = cons(make_sym("QUASIQUOTE"), cons(nil, nil));
      working_result = &car(cdr(*working_result));
      continue;
    case ',':
      symbol = token[1] == '@' ? "UNQUOTE-SPLICING" : "UNQUOTE";
      *working_result = cons(make_sym(symbol), cons(nil, nil));
      working_result = &car(cdr(*working_result));
      continue;
    case ')':
      if (!stack) { return ok; }
      parser_print_stackframe(stack,0);
      PREP_ERROR(err, ERROR_SYNTAX, *result,
                 "Extraneous closing parenthesis.",
                 NULL);
      return err;
    case '(':
      if (list) {
        stack = parser_make_frame(stack, token, working_list);
        list = NULL;
      }
      break;
    }

    // LIST PARSING

    // Check for end of list. If found, check parser stack to ensure nested list handling.
    // TODO: Handle improper list '.' operator properly.
    const char *end_copy = *end;

    if (list_improper) {
      list_improper = 0;
      err = lex(end_copy, &token, &end_copy);
      if (err.type) { return err; }
      if (token[0] == ')') {
        *end += 1;
        if (!stack) { return ok; }
        working_list = stack->working_list;
        working_result = working_list;
        stack = stack->parent;
      } else {
        PREP_ERROR(err, ERROR_SYNTAX, *result,
                   "There may only be one list item given after '.'"
                   , NULL);
        return err;
      }
    }

    for (;;) {
      err = lex(end_copy, &token, &end_copy);
      if (err.type) { return err; }
      if (token[0] == '.') {
        working_result = working_list;
        *end = end_copy;
        list_improper = 1;
        break;
      }
      else if (token[0] == ')') {
        *end += 1;
        if (!stack) { return ok; }
        working_list = stack->working_list;
        working_result = working_list;
        stack = stack->parent;
      } else { break; }
    }
    if (!list) {
      // Handle new list.
      *working_result = cons(nil, nil);
      list = working_result;
      working_list = &cdr(*list);
      // Evaluate the left side of the list.
      working_result = &car(*list);
      continue;
    }
    if (!list_improper) {
      // Make space for another element, then evaluate into that element.
      *working_list = cons(nil, nil);       // Expand end of list.
      working_result = &car(*working_list); // Set working_result for next iteration.
      working_list = &cdr(*working_list);   // Set working_list for next iteration.
    }
  }
  return ok;
}
