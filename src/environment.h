#ifndef LITE_ENVIRONMENT_H
#define LITE_ENVIRONMENT_H

struct Atom;
typedef struct Atom Atom;

Atom env_create(Atom parent);
int env_set(Atom environment, Atom symbol, Atom value);
int env_get(Atom environment, Atom symbol, Atom *result);

Atom default_environment();

#endif /* LITE_ENVIRONMENT_H */
