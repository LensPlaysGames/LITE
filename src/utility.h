#ifndef LITE_UTILITY_H
#define LITE_UTILITY_H

/** Exit the LITE program entirely.
 * Attempts to clean up as much memory as possible before calling `exit()`.
 *
 * If the following debug flags are non-nil in the global LISP environment,
 * extra information will be printed to standard output.
 * - `DEBUG/MEMORY`
 * .
 *
 * @param code The exit code that is passed to `exit()`.
 */
void exit_lite(int code);

#endif /* LITE_UTILITY_H */
