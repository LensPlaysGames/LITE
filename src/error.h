#ifndef LITE_ERROR_H
#define LITE_ERROR_H

#include <stdio.h>

enum Error {
  ERROR_NONE = 0,
  ERROR_TODO,
  ERROR_SYNTAX,
  ERROR_NOT_BOUND,
  ERROR_ARGUMENTS,
  ERROR_TYPE,
};

void print_error(enum Error e);

#endif /* LITE_ERROR_H */
