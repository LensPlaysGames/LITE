#include <environment.h>

#include <assert.h>
#include <stdlib.h>

#ifdef LITE_GFX
#  include <api.h>
#  include <gui.h>
#endif /* #ifdef LITE_GFX */

#include <builtins.h>
#include <error.h>
#include <types.h>

char user_quit = 0;

Atom env_create(Atom parent, size_t initial_capacity) {
  Environment *env = calloc(1, sizeof(*env));
  if (!env) {
    fprintf(stderr, "env_create() could not allocate new environment.");
    exit(9);
  }
  env->parent = parent;
  env->data_count = 0;
  env->data_capacity = initial_capacity;
  env->data = calloc(1, initial_capacity * sizeof(*env->data));
  if (!env->data) {
    fprintf(stderr, "env_create() could not allocate new hash table.");
    exit(9);
  }

  Atom out;
  out.type = ATOM_TYPE_ENVIRONMENT;
  out.galloc = 0;
  out.docstring = NULL;
  out.value.env = env;

  gcol_generic_allocation(&out, out.value.env->data);
  gcol_generic_allocation(&out, out.value.env);
  return out;
}

static size_t knuth_multiplicative(unsigned char *symbol_pointer) {
  return ((size_t)symbol_pointer) * 2654435761;
}

static size_t hash64shift(char *symbol_pointer) {
  size_t key = (size_t)symbol_pointer;
  key = (~key) + (key << 21); // key = (key << 21) - key - 1;
  key = key ^ (key >> 24);
  key = (key + (key << 3)) + (key << 8); // key * 265
  key = key ^ (key >> 14);
  key = (key + (key << 2)) + (key << 4); // key * 21
  key = key ^ (key >> 28);
  key = key + (key << 31);
  return key;
}

static size_t env_hash(Environment table, char *symbol_pointer) {
  size_t hash = hash64shift(symbol_pointer);
  //printf("Hash of %s is %zu  pointer=%p\n", symbol_pointer, hash, (void*)symbol_pointer);
  return hash & (table.data_capacity - 1);
}

static void env_expand(Environment *table);

/// Return the entry for the given key string.
// TODO: I haven't tested this myself, as I'm not *that* proficient at
// writing hash tables, but there are (apparently) better ways to do
// collision avoidance and such.
// https://www.sebastiansylvan.com/post/robin-hood-hashing-should-be-your-default-hash-table-implementation/
static EnvironmentValue *env_entry(Environment *env, char *symbol_pointer) {
  if (!symbol_pointer) {
    fprintf(stderr, "Can not get environment entry for NULL key!\n");
    return NULL;
  }
  size_t index = env_hash(*env, symbol_pointer);
  EnvironmentValue *entry;

  size_t index_it = index;
  while (index_it < env->data_capacity) {
    entry = env->data + index_it;
    if (entry->key == symbol_pointer) {
      return entry;
    }
    if (entry->key == NULL) {
      return entry;
    }
    ++index_it;
  }

  index_it = 0;
  while (index_it < index) {
    entry = env->data + index_it;
    if (entry->key == symbol_pointer) {
      return entry;
    }
    if (entry->key == NULL) {
      return entry;
    }
    ++index_it;
  }

  // Expand and try again.
  // FIXME: It's probably best to expand BEFORE the hash table is
  // completely full...
  env_expand(env);
  return env_entry(env, symbol_pointer);
}

static void env_insert(Environment *env, char *symbol_pointer, Atom to_insert) {
  EnvironmentValue *entry = env_entry(env, symbol_pointer);
  entry->key = symbol_pointer;
  entry->value = to_insert;
}

static void env_expand(Environment *table) {
  // Create a new, larger hash table.
  size_t old_capacity = table->data_capacity;
  size_t new_capacity = table->data_capacity << 1;
  EnvironmentValue *new_data = calloc(1, new_capacity * sizeof(*new_data));
  if (!new_data) {
    fprintf(stderr, "env_create() could not allocate new hash table.");
    exit(9);
  }

  EnvironmentValue *old_data = table->data;
  table->data = new_data;
  table->data_capacity = new_capacity;

  // Rehash all values from old table into new table. This is needed
  // because the index where the symbol is stored is a function of the
  // capacity of the table: when the capacity changes, so does the
  // mapping of hashes to indices. There aren't really many ways to
  // avoid this. One would be to not actually get rid of the old table,
  // and have it implemented as a linked list of tables, but this very
  // quickly slows down lookup and insertion by quite a bit. This
  // rehashing method, while expensive, only occurs during expansion,
  // which very rarely occurs in the first place.
  EnvironmentValue *entry = old_data;
  // For every value bound in old table, insert binding into new table.
  for (size_t i = 0; i < old_capacity; ++i, ++entry) {
    if (entry->key) {
      env_insert(table, entry->key, entry->value);
    }
  }

  free(old_data);

  // Debug output.
  size_t old_size = old_capacity * sizeof(*table->data);
  size_t new_size = old_size << 1;
  if (!strict_output) {
    printf("Environment size expanded from %zu to %zu entries (%zu to %zu bytes)\n",
           old_capacity, new_capacity, old_size, new_size);
  }
}

Error env_set(Atom environment, Atom symbol, Atom value) {
  env_insert(environment.value.env, symbol.value.symbol, value);
  //printf("Set %s to ", symbol.value.symbol);
  //print_atom(value);
  //putchar('\n');
  return ok;
}

void env_free(Environment env) {
  free(env.data);
}

Atom env_get_containing(Atom environment, Atom symbol) {
# ifdef LITE_GFX
  // Handle reading/popup-buffer.
  if (gui_ctx() && gui_ctx()->reading
      && symbol.value.symbol == make_sym("CURRENT-BUFFER").value.symbol
      ) {
    symbol = make_sym("POPUP-BUFFER");
  }
# endif /* #ifdef LITE_GFX */

  EnvironmentValue *entry = env_entry(environment.value.env, symbol.value.symbol);

  // No key means not bound.
  if (!entry->key) {
    // Attempt to check parent environment, if it exists.
    if (!nilp(environment.value.env->parent)) {
      return env_get_containing(environment.value.env->parent, symbol);
    }
    // Symbol not bound, no environment contains it, so return nil.
    return nil;
  }

  return environment;
}

Error env_get(Atom environment, Atom symbol, Atom *result) {
# ifdef LITE_GFX
  // Handle reading/popup-buffer.
  if (gui_ctx() && gui_ctx()->reading
      && symbol.value.symbol == make_sym("CURRENT-BUFFER").value.symbol
      ) {
    symbol = make_sym("POPUP-BUFFER");
  }
# endif /* #ifdef LITE_GFX */

  EnvironmentValue *entry = env_entry(environment.value.env, symbol.value.symbol);

  // No key means not bound.
  if (!entry->key) {
    // If there is a parent environment, search that.
    if (!nilp(environment.value.env->parent)) {
      return env_get(environment.value.env->parent, symbol, result);
    }

    // Otherwise, no binding and no parent means symbol not bound.
    *result = nil;
    MAKE_ERROR(err, ERROR_NOT_BOUND,
               symbol,
               "Symbol not bound in any environment.",
               NULL);
    return err;
  }

  // Got binding, return value.
  *result = entry->value;

  return ok;
}

int env_non_nil(Atom environment, Atom symbol) {
  Atom bind = nil;
  Error err = env_get(environment, symbol, &bind);
  if (err.type) {
    return 0;
  }
  return !nilp(bind);
}

int boundp(Atom environment, Atom symbol) {
  if (!envp(environment) || !symbolp(symbol)) {
    return 0;
  }
  EnvironmentValue *entry = env_entry(environment.value.env, symbol.value.symbol);
  // Key must not be NULL for a binding to be valid.
  return entry->key != NULL;
}

#define defbuiltin(name) do {                                   \
  env_set(environment, make_sym((char *)builtin_##name##_name), \
          make_builtin(builtin_##name,                          \
                       (char *)builtin_##name##_name,           \
                       (char *)builtin_##name##_docstring));    \
  } while (0)

Atom default_environment(void) {
  Atom environment = env_create(nil, 2 << 7);
  env_set(environment, make_sym("T"), make_sym("T"));

  defbuiltin(quit_lisp);
  defbuiltin(docstring);

  defbuiltin(car);
  defbuiltin(cdr);
  defbuiltin(cons);
  defbuiltin(setcar);
  defbuiltin(setcdr);

  defbuiltin(nilp);
  defbuiltin(pairp);
  defbuiltin(symbolp);
  defbuiltin(integerp);
  defbuiltin(builtinp);
  defbuiltin(closurep);
  defbuiltin(macrop);
  defbuiltin(stringp);
  defbuiltin(bufferp);

  defbuiltin(add);
  defbuiltin(subtract);
  defbuiltin(multiply);
  defbuiltin(divide);
  defbuiltin(remainder);


  defbuiltin(not);
  defbuiltin(eq);
  defbuiltin(numeq);
  defbuiltin(numnoteq);
  defbuiltin(numlt);
  defbuiltin(numlt_or_eq);
  defbuiltin(numgt);
  defbuiltin(numgt_or_eq);

  defbuiltin(copy);
  defbuiltin(apply);

  defbuiltin(print);
  defbuiltin(prins);

  defbuiltin(symbol_table);

  defbuiltin(buffer_toggle_mark);
  defbuiltin(buffer_set_mark_activation);
  defbuiltin(buffer_set_mark);
  defbuiltin(buffer_mark);
  defbuiltin(buffer_mark_activated);
  defbuiltin(buffer_region);
  defbuiltin(buffer_region_length);
  defbuiltin(buffer_table);
  defbuiltin(buffer_path);
  defbuiltin(open_buffer);
  defbuiltin(buffer_insert);
  defbuiltin(buffer_remove);
  defbuiltin(buffer_remove_forward);
  defbuiltin(buffer_undo);
  defbuiltin(buffer_redo);
  defbuiltin(buffer_string);
  defbuiltin(buffer_lines);
  defbuiltin(buffer_line);
  defbuiltin(buffer_current_line);
  defbuiltin(buffer_set_point);
  defbuiltin(buffer_point);
  defbuiltin(buffer_index);
  defbuiltin(buffer_seek_byte);
  defbuiltin(buffer_seek_past_byte);
  defbuiltin(buffer_seek_substring);
  defbuiltin(save);

  defbuiltin(read_prompted);
  defbuiltin(finish_read);

  defbuiltin(string_length);
  defbuiltin(string_concat);

  defbuiltin(evaluate_string);
  defbuiltin(evaluate_file);

  defbuiltin(member);
  defbuiltin(length);

  defbuiltin(clipboard_cut);
  defbuiltin(clipboard_copy);
  defbuiltin(clipboard_paste);

  defbuiltin(set_carriage_return_character);

  defbuiltin(change_font);
  defbuiltin(change_font_size);

  defbuiltin(window_size);
  defbuiltin(change_window_size);
  defbuiltin(change_window_mode);

  // TODO: These should be implemented in LISP!!!
  defbuiltin(scroll_up);
  defbuiltin(scroll_down);
  defbuiltin(scroll_left);
  defbuiltin(scroll_right);


  env_set(environment, make_sym((char *)"PLATFORM"),
#         if defined(__FreeBSD__)
          make_sym((char *)"FREE-BSD")
#         elif defined(__linux__)
          make_sym((char *)"LINUX")
#         elif defined(__unix__)
          make_sym((char *)"UNIX")
#         elif defined(_WIN32)
          make_sym((char *)"WINDOWS")
#         elif defined(__APPLE__)
          make_sym((char *)"APPLE")
#         endif
          );

  env_set(environment, make_sym((char *)"WHILE-RECURSE-LIMIT"), make_int_with_docstring
          (10000,
           "This is the maximum amount of times a while loop may loop.\n"
           "\n"
           "Used to prevent infinite loops."));

  env_set(environment, make_sym("GARBAGE-COLLECTOR-EVALUATION-ITERATIONS-THRESHOLD"),
          make_int_with_docstring
          (100000, "This number corresponds to the amount of evaluation operations before \
running the garbage collector.\nSmaller numbers mean memory is freed more often. \
The default value of '100000' means memory is freed in around twenty megabyte chunks."));

  env_set(environment, make_sym("GARBAGE-COLLECTOR-PAIR-ALLOCATIONS-THRESHOLD"),
          make_int_with_docstring
          ((integer_t)290500, "This number corresponds to the amount of pairs able to be allocated \
before running the garbage collector.\nSmaller numbers mean memory is freed more often, \
but too small causes problems."));

  env_set(environment, make_sym("DEBUG/ENVIRONMENT"), nil_with_docstring
          ("When non-nil, display debug information concerning the current \
LISP evaluation environment, including the symbol table."));

  env_set(environment, make_sym("DEBUG/EVALUATE"), nil_with_docstring
          ("When non-nil, display debug information concerning the evaluation of expressions."));

  env_set(environment, make_sym("DEBUG/KEYBINDING"), nil_with_docstring
          ("When non-nil, display debug information concerning keybindings, \
including information about keymaps on every button press."));

  env_set(environment, make_sym("DEBUG/MACRO"), nil_with_docstring
          ("When non-nil, display debug information concerning macros, \
including what each expansion step looks like."));

  env_set(environment, make_sym("DEBUG/MEMORY"), nil_with_docstring
          ("When non-nil, display debug information concerning allocated memory, \
including when garbage collections happen and data upon program exit."));

  env_set(environment, make_sym("DEBUG/WHILE"), nil_with_docstring
          ("When non-nil, display debug information concerning 'WHILE', \
including data output at each iteration of the loop."));

  return environment;
}

#undef defbuiltin


static Atom global_environment = { ATOM_TYPE_NIL, { 0 }, NULL, NULL };
Atom *genv(void) {
  if (nilp(global_environment)) {
    //printf("Recreating global environment from defaults...\n");
    global_environment = default_environment();
  }
  return &global_environment;
}
