#ifndef LITE_ENVIRONMENT_H
#define LITE_ENVIRONMENT_H

#include <error.h>

struct Atom;
typedef struct Atom Atom;

Atom env_create(Atom parent);
Error env_set(Atom environment, Atom symbol, Atom value);
Error env_get(Atom environment, Atom symbol, Atom *result);

/// Return 0 if symbol is not bound in environment, otherwise return 1.
int boundp(Atom environment, Atom symbol);

/// Return 0 if evaluated symbol is equal to NIL or not bound, otherwise return 1.
int env_non_nil(Atom environment, Atom symbol);

Atom default_environment();

/// Return the global environment, creating it if it doesn't exist.
Atom *genv();

extern char user_quit;

#endif /* LITE_ENVIRONMENT_H */
