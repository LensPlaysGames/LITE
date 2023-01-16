#ifndef LITE_ENVIRONMENT_H
#define LITE_ENVIRONMENT_H

#include <types.h>
#include <error.h>

/// Create an environment without registering it in garbage collection
/// system.
Atom env_create_nofree(Atom parent, size_t initial_size);
/// Create an environment and register it's allocated data in the
/// garbage collection system.
Atom env_create(Atom parent, size_t initial_size);
/// Bind SYMBOL to VALUE in ENVIRONMENT.
Error env_set(Atom environment, Atom symbol, Atom value);
/// Return VALUE bound to SYMBOL in ENVIRONMENT.
Error env_get(Atom environment, Atom symbol, Atom *result);

/// Get the containing environment where SYMBOL is bound, or nil if unbound.
Atom env_get_containing(Atom environment, Atom symbol);

/// Return 0 if symbol is not bound in environment, otherwise return 1.
int boundp(Atom environment, Atom symbol);

/// Return 0 if evaluated symbol is equal to NIL or not bound, otherwise return 1.
int env_non_nil(Atom environment, Atom symbol);

Atom default_environment(void);

/// Return the global environment, creating it if it doesn't exist.
Atom *genv(void);

extern char user_quit;

#endif /* LITE_ENVIRONMENT_H */
