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

/** Allocate the given string on the heap and return it's address.
 *
 * @param string A null-terminated string that will be copied onto the
 *               heap.
 *
 * @return A heap-allocated copy of the passed string, or NULL if the
 *         action can not be completed.
 */
char *allocate_string(const char *string);

#endif /* LITE_UTILITY_H */
