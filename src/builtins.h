#ifndef LITE_BUILTINS_H
#define LITE_BUILTINS_H

struct Error;

#define builtin(name)                                  \
  extern const char *const builtin_##name##_name;      \
  extern const char *const builtin_##name##_docstring; \
  struct Error builtin_##name(Atom arguments, Atom *result)


struct Atom;
typedef struct Atom Atom;

builtin(quit_lisp);

builtin(docstring);

// TYPES

builtin(nilp);
builtin(pairp);
builtin(symbolp);
builtin(integerp);
builtin(builtinp);
builtin(closurep);
builtin(macrop);
builtin(stringp);
builtin(bufferp);
builtin(envp);

// PAIRS

builtin(cons);
builtin(car);
builtin(cdr);
builtin(setcar);
builtin(setcdr);

// LISTS

builtin(member);
builtin(length);

// LOGICAL

builtin(not);
builtin(eq);

builtin(numeq);
builtin(numnoteq);
builtin(numlt);
builtin(numlt_or_eq);
builtin(numgt);
builtin(numgt_or_eq);

// MATHEMATICAL

builtin(add);
builtin(subtract);
builtin(multiply);
builtin(divide);
builtin(remainder);

// BITWISE

builtin(bitand);
builtin(bitor);
builtin(bitxor);
builtin(bitnot);
builtin(bitshl);
builtin(bitshr);

// BUFFERS

builtin(buffer_toggle_mark);
builtin(buffer_set_mark_activation);
builtin(buffer_set_mark);
builtin(buffer_mark);
builtin(buffer_mark_activated);
builtin(buffer_region);
builtin(buffer_region_length);
builtin(buffer_region_length);
builtin(buffer_region_length);
builtin(open_buffer);
builtin(buffer_path);
builtin(buffer_table);
builtin(buffer_insert);
builtin(buffer_remove);
builtin(buffer_remove_forward);

builtin(buffer_undo);
builtin(buffer_redo);

builtin(buffer_set_point);
builtin(buffer_point);

builtin(buffer_index);
builtin(buffer_string);
builtin(buffer_lines);
builtin(buffer_line);
builtin(buffer_current_line);

builtin(buffer_seek_byte);
builtin(buffer_seek_past_byte);
builtin(buffer_seek_substring);

builtin(save);

// STRINGS

builtin(to_string);

builtin(string_length);
builtin(string_concat);
builtin(string_index);


// OTHER

builtin(copy);

builtin(evaluate_string);
builtin(evaluate_file);
builtin(apply);

builtin(symbol_table);
builtin(print);
builtin(prins);

builtin(read_prompted);
builtin(finish_read);

builtin(clipboard_cut);
builtin(clipboard_copy);
builtin(clipboard_paste);

builtin(set_carriage_return_character);

builtin(change_font);
builtin(change_font_size);

builtin(window_size);
builtin(change_window_size);
builtin(change_window_mode);

// TODO: These should be implemented in LISP; they have no need for
// being written in C.
builtin(scroll_up);
builtin(scroll_down);
builtin(scroll_left);
builtin(scroll_right);

#undef builtin

#endif /* LITE_BUILTINS_H */
