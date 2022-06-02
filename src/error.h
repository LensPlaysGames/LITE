#ifndef LITE_ERROR_H
#define LITE_ERROR_H

#include <stdio.h>

#include <types.h>

typedef struct Error {
  enum ErrorType {
    ERROR_NONE = 0,
    ERROR_TODO,
    ERROR_SYNTAX,
    ERROR_NOT_BOUND,
    ERROR_ARGUMENTS,
    ERROR_TYPE,
    ERROR_MEMORY,
  } type;
  const char *message;
  const char *suggestion;
  Atom ref;
} Error;

#define MAKE_ERROR(n, t, refer, msg, sugg)      \
  Error (n);                                    \
  (n).type = (t);                               \
  (n).message = (msg);                          \
  (n).suggestion = (sugg);                      \
  (n).ref = (refer);

#define PREP_ERROR(err, t, refer, msg, sugg)    \
  (err).type = (t);                             \
  (err).message = (msg);                        \
  (err).suggestion = (sugg);                    \
  (err).ref = (refer);

void print_error(Error e);

extern Error ok;

#endif /* LITE_ERROR_H */
