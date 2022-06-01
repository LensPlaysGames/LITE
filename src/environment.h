#ifndef LITE_ENVIRONMENT_H
#define LITE_ENVIRONMENT_H

struct Atom;
typedef struct Atom Atom;

Atom env_create(Atom parent);
int env_set(Atom environment, Atom symbol, Atom value);
int env_get(Atom environment, Atom symbol, Atom *result);
// Return 0 if evaluated symbol is equal to NIL or not bound, otherwise return 1.
int env_non_nil(Atom environment, Atom symbol);

Atom default_environment();

#endif /* LITE_ENVIRONMENT_H */
