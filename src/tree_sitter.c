#include <tree_sitter.h>

#include <utility.h>

#include <assert.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef LITE_GFX
#include <api.h>
#include <gfx.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#elif defined(__unix__)
#include <dlfcn.h>
#endif

/// Define `SO_EXIT` preprocessor directive to make errors exit the program
#if defined(SO_EXIT)
#define so_return_error exit(1)
#else
#define so_return_error return NULL
#endif

void *so_load(const char *library_path) {
  void *out = NULL;
#if defined(_WIN32)
  // Use WINAPI to load library
  HMODULE lib = LoadLibraryA(TEXT(library_path));
  if (lib == NULL) {
    fprintf(stdout, "Could not load tree sitter grammar library at \"%s\"\n", library_path);
    so_return_error;
  }
  out = lib;
#elif defined(__unix__)
  out = dlopen(library_path, RTLD_NOW);
  if (out == NULL) {
    fprintf(stdout, "Error from dlopen(\"%s\"):\n  \"%s\"\n", library_path, dlerror());
    so_return_error;
  }
  // Clear any existing error.
  dlerror();
#else
# error "Can not support dynamic loading on unrecognized system"
#endif
  return out;
}

void *so_get(void *data, const char *symbol) {
  void *out = NULL;
#if defined(_WIN32)
  // Get Tree Sitter language function address
  out = (void *)GetProcAddress(data, TEXT(symbol));
  if (out == NULL) {
    fprintf(stdout, "Could not load symbol \"%s\" from dynamically loaded library.\n", symbol);
    so_return_error;
  }
#elif defined(__unix__)
  out = (void *)dlsym(data, symbol);
  char *error = dlerror();
  if (error) {
    fprintf(stdout,
            "Could not load symbol \"%s\" from dynamically loaded library.\n"
            "  \"%s\"\n",
            symbol,
            error);
    so_return_error;
  }
#else
# error "Can not support dynamic loading on unrecognized system"
#endif
  return out;
}

void so_delete(void *data) {
#if defined(_WIN32)
  FreeLibrary(data);
#elif defined(__unix__)
  dlclose(data);
#else
# error "Can not support dynamic loading on unrecognized system"
#endif
}

void *so_load_ts(const char *language) {
  void *out = NULL;
  // Construct library path
#if defined(_WIN32)
  // ~/.tree_sitter/bin/libtree-sitter-<language>.dll
  const char *appdata_path = getenv("APPDATA");
  char *tree_sitter_bin = string_join(appdata_path, "/.tree_sitter/bin/");
  char *tmp = string_join("libtree-sitter-", language);
  char *library_path = string_trijoin(tree_sitter_bin, tmp, ".dll");
  free(tree_sitter_bin);
  free(tmp);
#elif defined(__APPLE__)
  // libtree-sitter-<language>.dylib
  // TODO: Tree sitter bin on MacOS?
  char *library_path = string_trijoin("libtree-sitter-", language, ".dylib");
#elif defined(__linux__)
  // ~/.tree_sitter/bin/libtree-sitter-<language>.so
  const char *home_path = getenv("HOME");
  char *tree_sitter_bin = string_join(home_path, "/.tree_sitter/bin/");
  char *tmp = string_trijoin("libtree-sitter-", language, ".so");
  char *library_path = string_join(tree_sitter_bin, tmp);
  free(tree_sitter_bin);
  free(tmp);

#else
# error "Can not support dynamic loading on unrecognized system"
#endif
  // Load library at path
  out = so_load(library_path);
  free(library_path);
  return out;
}

void *so_get_ts(void *data, const char *language) {
  void *out = NULL;
  // Construct function name
  char *lang_symbol = string_join("tree_sitter_", language);
  // Load symbol from library
  out = so_get(data, lang_symbol);
  free(lang_symbol);
  return out;
}

TreeSitterLanguage ts_langs[TS_LANG_MAX];
TreeSitterLanguage *ts_langs_find_lang(const char *lang) {
  for (int i = 0; i < TS_LANG_MAX; ++i) {
    if (ts_langs[i].used && ts_langs[i].lang && strcmp(lang, ts_langs[i].lang) == 0) {
      return ts_langs + i;
    }
  }
  return NULL;
}
TreeSitterLanguage *ts_langs_find_free(void) {
  for (int i = 0; i < TS_LANG_MAX; ++i) {
    if (!ts_langs[i].used) {
      return ts_langs + i;
    }
  }
  assert(0 && "Sorry, we don't support over TS_LANG_MAX tree sitter languages.");
  return NULL;
}
TreeSitterLanguage *ts_langs_new(const char *lang_string) {
  TreeSitterLanguage *lang = NULL;
  if ((lang = ts_langs_find_lang(lang_string))) {
    return lang;
  }
  lang = ts_langs_find_free();
  lang->used = 1;
  // FIXME: More error checks
  lang->lang = strdup(lang_string);
  lang->library_handle = so_load_ts(lang_string);
  lang->lang_func = (TSLanguage *(*)(void))so_get_ts(lang->library_handle, lang_string);
  lang->parser = ts_parser_new();
  lang->query_count = 0;
  lang->queries = NULL;
  ts_parser_set_language(lang->parser, lang->lang_func());
  return lang;
}
Error ts_langs_update_queries(const char *lang_string, struct Atom queries) {
  TreeSitterLanguage *lang = ts_langs_new(lang_string);
  // Free existing queries, if any.
  if (lang->query_count) {
    for (size_t i = 0; i < lang->query_count; ++i) {
      TreeSitterQuery ts_query = lang->queries[i];
      ts_query_delete(ts_query.query);
    }
    lang->query_count = 0;
    free(lang->queries);;
    lang->queries = NULL;
  }
  // Count queries
  for (Atom query_it = queries; pairp(query_it); query_it = cdr(query_it)) {
    lang->query_count += 1;
  }
  // If there are no queries, there is nothing to do.
  if (lang->query_count == 0) {
    MAKE_ERROR(err, ERROR_GENERIC, queries,
               "ts_langs_update_queries(): No queries supplied.",
               NULL);
    return err;
  }

  lang->queries = calloc(lang->query_count, sizeof *lang->queries);

  size_t i = 0;
  for (Atom query_it = queries; !nilp(query_it); query_it = cdr(query_it), ++i) {
    if (i >= lang->query_count) {
      MAKE_ERROR(err, ERROR_GENERIC, queries,
                 "Trouble updating tree sitter queries for %s: too many supplied (likely internal error)",
                 "Report this bug to the LITE developers with as much context and information as possible.");
      return err;
    }
    TreeSitterQuery *ts_it = lang->queries + i;
    Atom query = car(query_it);
    if (!pairp(query)) {
      // invalid query
      printf("ERROR in tree-sitter query specification: Query must be a list\n");
      continue;
    }
    Atom query_string = car(query);
    if (!stringp(query_string)) {
      // invalid query
      printf("ERROR in tree-sitter query specification: Query must be a string\n");
      continue;
    }

    if (!pairp(cdr(query))) {
      // invalid query
      printf("ERROR in tree-sitter query specification: Invalid amount of elements in query specification\n");
      continue;
    }

    Atom query_colors = car(cdr(query));
    if (!pairp(query_colors)) {
      // invalid query
      printf("ERROR in tree-sitter query specification: Invalid fg/bg color pair\n");
      continue;
    }

    // ((fg_r . (fg_g . (fg_b . (fg_a . nil)))) . ((bg_r . (bg_g . (bg_b . (bg_a . nil)))) . nil))

    // car: (fg_r . (fg_g . (fg_b . (fg_a . nil))))
    //   car: fg_r
    //   cdr: (fg_g . (fg_b . (fg_a . nil)))
    //     car: fg_g
    //     cdr: (fg_b . (fg_a . nil))
    //       car: fg_b
    //       cdr: (fg_a . nil)
    //         car: fg_a
    //         cdr: nil
    if (!pairp(car(query_colors))
        || !pairp(cdr(car(query_colors)))
        || !pairp(cdr(cdr(car(query_colors))))
        || !pairp(cdr(cdr(cdr(car(query_colors)))))
        || !integerp(car(car(query_colors)))
        || !integerp(car(cdr(car(query_colors))))
        || !integerp(car(cdr(cdr(car(query_colors)))))
        || !integerp(car(cdr(cdr(cdr(car(query_colors))))))
      ) {
      // invalid query
      printf("ERROR in tree-sitter query specification: Invalid foreground color\n");
      continue;
    }
    Atom query_fg = car(query_colors);
    Atom query_fg_r = car(query_fg);
    Atom query_fg_g = car(cdr(query_fg));
    Atom query_fg_b = car(cdr(cdr(query_fg)));
    Atom query_fg_a = car(cdr(cdr(cdr(query_fg))));


    // cdr: ((bg_r . (bg_g . (bg_b . (bg_a . nil)))))
    //   car: (bg_r . (bg_g . (bg_b . (bg_a . nil))))
    //     car: bg_r
    //     cdr: (bg_g . (bg_b . (bg_a . nil)))
    //       car: bg_g
    //       cdr: (bg_b . (bg_a . nil))
    //         car: bg_b
    //         cdr: (bg_a . nil)
    //           car: bg_a
    //           cdr: nil
    //   cdr: nil
    if (!pairp(car(cdr(query_colors)))
        || !pairp(cdr(car(cdr(query_colors))))
        || !pairp(cdr(cdr(car(cdr(query_colors)))))
        || !pairp(cdr(cdr(cdr(car(cdr(query_colors))))))
        || !integerp(car(car(cdr(query_colors))))
        || !integerp(car(cdr(car(cdr(query_colors)))))
        || !integerp(car(cdr(cdr(car(cdr(query_colors))))))
        || !integerp(car(cdr(cdr(cdr(car(cdr(query_colors)))))))
      ) {
      // invalid query
      printf("ERROR in tree-sitter query specification: Invalid background color\n");
      pretty_print_atom(cdr(query_colors));putchar('\n');
      continue;
    }
    Atom query_bg = car(cdr(query_colors));
    Atom query_bg_r = car(query_bg);
    Atom query_bg_g = car(cdr(query_bg));
    Atom query_bg_b = car(cdr(cdr(query_bg)));
    Atom query_bg_a = car(cdr(cdr(cdr(query_bg))));

    RGBA fg = RGBA_VALUE(query_fg_r.value.integer,
                         query_fg_g.value.integer,
                         query_fg_b.value.integer,
                         query_fg_a.value.integer);

    RGBA bg = RGBA_VALUE(query_bg_r.value.integer,
                         query_bg_g.value.integer,
                         query_bg_b.value.integer,
                         query_bg_a.value.integer);

    uint32_t query_error_offset = 0;
    TSQueryError query_error = TSQueryErrorNone;
    TSQuery *new_query = ts_query_new
                           (ts_parser_language(lang->parser),
                            query_string.value.symbol,
                            (uint32_t)strlen(query_string.value.symbol),
                            &query_error_offset, &query_error);

    if (query_error != TSQueryErrorNone) {
      fprintf(stderr, "Error in query at offset %"PRIu32"\n%s\n", query_error_offset, query_string.value.symbol);
      MAKE_ERROR(err, ERROR_GENERIC, query_string,
                 "Error in tree-sitter query",
                 NULL);
      return err;
    }

    ts_it->query = new_query;
    ts_it->fg = fg;
    ts_it->bg = bg;
  }
  return ok;
}
void ts_langs_delete_one(TreeSitterLanguage *lang) {
  if (!lang || !lang->used) return;
  ts_parser_delete(lang->parser);
  if (lang->library_handle) {
    so_delete(lang->library_handle);
  }
  free(lang->lang);
  lang->used = 0;
}
void ts_langs_delete(const char *lang_string) {
  ts_langs_delete_one(ts_langs_find_lang(lang_string));
}
void ts_langs_delete_all() {
  for (int i = 0; i < TS_LANG_MAX; ++i) {
    ts_langs_delete_one(ts_langs + i);
  }
}

#ifdef LITE_GFX

#define GUI_COLOR_FROM_RGBA(name, rgba) GUIColor (name); (name).r = RGBA_R(rgba); (name).g = RGBA_G(rgba); (name).b = RGBA_B(rgba); (name).a = RGBA_A(rgba)

void add_property_from_query_matches(GUIWindow *window, TSNode root, size_t offset, size_t narrow_begin, size_t narrow_end, TSQuery *ts_query, RGBA fg, RGBA bg) {
  if (!ts_query) return;

  TSQueryCursor *query_cursor = ts_query_cursor_new();
  ts_query_cursor_exec(query_cursor, ts_query, root);

  GUI_COLOR_FROM_RGBA(fg_color, fg);
  GUI_COLOR_FROM_RGBA(bg_color, bg);

  // Foreach match...
  TSQueryMatch match;
  while (ts_query_cursor_next_match(query_cursor, &match)) {
    // Foreach capture...
    for (uint16_t i = 0; i < match.capture_count; ++i) {
      TSQueryCapture capture = match.captures[i];

      size_t start = ts_node_start_byte(capture.node) + offset;
      // NOTE: This assumes captures are in order, from beginning to end.
      if (start > narrow_end) break;

      size_t end = ts_node_end_byte(capture.node) + offset;
      size_t length = end - start;
      if (start < narrow_begin && end < narrow_begin) continue;

      GUIStringProperty *new_property = malloc(sizeof *new_property);
      new_property->offset = start;
      new_property->length = length;
      new_property->fg = fg_color;
      new_property->bg = bg_color;

      // Add new property to list.
      add_property(&window->contents, new_property);
    }
  }

  ts_query_cursor_delete(query_cursor);
}

#endif // LITE_GFX
