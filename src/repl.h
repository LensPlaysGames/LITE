#ifndef LITE_REPL_H
#define LITE_REPL_H

#define MAX_INPUT_BUFSZ 0x1000

struct Atom;
typedef struct Atom Atom;

char* readline(char *prompt);
void enter_repl(Atom environment);

#endif /* #ifndef LITE_REPL_H */
