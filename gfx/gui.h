#ifndef GUI_H
#define GUI_H

typedef struct GUIContext {
  const char *headline;
  const char *contents;
  const char *footline;
} GUIContext;

#define INPUT_PREV_SIZE 1023
typedef struct GUIInput {
  unsigned prev_counter;
  char prev[INPUT_PREV_SIZE+1];
  /* SOLUTIONS:
   *     
   * - I could have an array of chars for QWERTY characters,
   * with some chars separate for modifier keys. This doesn't
   * sound too insane, but creating a string from the given
   * input would be impossible since order isn't known.
   *
   * - How do I get user input?
   * I feel dumb dumb dumb dumb dumb dumb dumb dumb.
   * I'm stupid and useless and should give up on life.
   *
   * What does SDL provide?
   * SDL provides events that signify the user has pressed a button.
   * These events will be handled when do_gui() is called.
   * It may be smart to factor out the event handling to it's own function.
   *
   */
} GUIInput;

int create_gui();
void destroy_gui();

/// Populate an existing GUIInput structure.
void handle_input_gui(GUIInput *input);

/// Render a single frame based on graphical context.
void draw_gui(GUIContext *ctx);

void do_gui(int *open, GUIInput *input, GUIContext *ctx);

#endif /* GUI_H */
