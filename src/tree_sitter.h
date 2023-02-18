#ifdef TREE_SITTER

#ifndef LITE_TREE_SITTER_H
#define LITE_TREE_SITTER_H

#include <error.h>

#include <tree_sitter/api.h>

typedef uint32_t RGBA;
#define RGBA_R(a) ((uint8_t)((((RGBA)a) & (0xff000000)) >> 24))
#define RGBA_G(a) ((uint8_t)((((RGBA)a) & (0x00ff0000)) >> 16))
#define RGBA_B(a) ((uint8_t)((((RGBA)a) & (0x0000ff00)) >>  8))
#define RGBA_A(a) ((uint8_t)((((RGBA)a) & (0x000000ff)) >>  0))
#define RGBA_VALUE(r, g, b, a) (RGBA)(((r) << 24) | ((g) << 16) | ((b) << 8) | (a))

typedef struct TreeSitterQuery {
  TSQuery *query;
  RGBA fg;
  RGBA bg;
} TreeSitterQuery;

// TODO: Also store queries here, and only update these structures from
// LISP variables when user calls something like `tree-sitter-update`
// or something. This will allow us to properly cache everything and
// only do data traversal in the redraw.
typedef struct TreeSitterLanguage {
  /// A string representing the language name that this parser parses.
  char *lang;

  int used;

  void *library_handle;
  TSLanguage *(*lang_func)(void);

  /// A parser with language set. Be sure to call ts_parser_reset before each use.
  TSParser *parser;
  /// When non-null, a parsed tree. Can be used if contents haven't been modified.
  TSTree *tree;

  /// Dynamic array of Tree Sitter Queries that should be run
  size_t query_count;
  TreeSitterQuery *queries;

} TreeSitterLanguage;

// TODO: Make this a hash table from language string to TreeSitterLanguage.
#define TREE_SITTER_LANGUAGE_MAXIMUM 256
#define TS_LANG_MAX TREE_SITTER_LANGUAGE_MAXIMUM
extern TreeSitterLanguage ts_langs[TS_LANG_MAX];

Error ts_langs_update_queries(const char *lang_string, struct Atom queries);

#endif /* LITE_TREE_SITTER_H */

#endif /* #ifdef TREE_SITTER */
