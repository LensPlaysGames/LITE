#ifndef LITE_UTILITY_H
#define LITE_UTILITY_H

#if defined (__GNUC__) || defined (__clang__)
#  define HOTFUNCTION __attribute__((hot))
#elif defined (_MSC_VER)
#  define HOTFUNCTION
#endif

/** Exit the program entirely and safely.
 * Cleans up as much memory as possible before calling `exit()`.
 *
 * If the following debug flags are non-nil in the global LISP
 * environment, extra information will be printed to standard output.
 * - `DEBUG/MEMORY`
 * .
 *
 * @param[in] code The exit code that is passed to `exit()`.
 */
void exit_safe(int code);

/** Allocate the given string on the heap and return it's address.
 *
 * @param[in] string A null-terminated string that will be copied onto
 *                   the heap.
 *
 * @return A heap-allocated copy of the passed string, or NULL if the
 *         action can not be completed.
 */
char *allocate_string(const char *const string);

/** Concatenate the two given strings into a new string.
 *
 * @param[in] a Prefix string
 * @param[in] b Suffix string
 *
 * @return A heap-allocated string with the contents of A followed by B.
 */
char *string_join(const char *const a, const char *const b);

#endif /* LITE_UTILITY_H */
