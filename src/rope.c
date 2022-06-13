#include <rope.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t rope_length(Rope *rope) {
  return rope ? rope->weight : 0;
}

char rope_index(Rope *rope, size_t index) {
  if (!rope) {
    return 0;
  }
  size_t current_index = index + 1;
  Rope *current_rope = rope;
  while (current_rope && !current_rope->string) {
    if (current_index > current_rope->weight) {
      current_index -= current_rope->weight;
      current_rope = current_rope->right;
    } else {
      current_rope = current_rope->left;
    }
  }
  if (!current_rope) {
    return 0;
  }
  return current_rope->string[current_index];
}

Rope *rope_create(const char *str) {
  if (!str) {
    return NULL;
  }

  // Copy input string to heap.
  size_t len = strlen(str);
  char *newstr = malloc(len);
  if (!newstr) {
    return NULL;
  }
  strncpy(newstr, str, len);

  // Allocate new rope nodes on heap.
  Rope *rope = malloc(sizeof(Rope));
  if (!rope) {
    return NULL;
  }
  Rope *left = malloc(sizeof(Rope));
  if (!left) {
    return NULL;
  }

  // Initialize nodes.
  left->weight = len;
  left->string = newstr;
  left->right = NULL;
  left->left = NULL;
  
  rope->weight = len;
  rope->string = NULL;
  rope->right = NULL;
  rope->left = left;

  return rope;
}

/// Return the sum of the weights of all nodes in any subtree.
size_t rope_sum(Rope *rope) {
  if (!rope) {
    return 0;
  }
  if (rope->string) {
    return rope->weight;
  }
  size_t sum = 0;
  if (rope->left) {
    sum += rope_sum(rope->left);
  }
  if (rope->right) {
    sum += rope_sum(rope->left);
  }
  return sum;
}

// Re-calculates weight based on what the current values are.
void rope_update_weights(Rope *rope) {
  if (!rope) {
    return;
  }
  if (rope->left) {
    rope_update_weights(rope->left);
    rope->weight = rope_sum(rope->left);
  }
  if (rope->right) {
    rope_update_weights(rope->right);
  }
}

Rope *rope_copy(Rope *original){
  if (!original) {
    return NULL;
  }
  Rope *rope = malloc(sizeof(Rope));
  if (!rope) { return NULL; }
  rope->weight = original->weight;
  rope->string = original->string ? strdup(original->string) : NULL;
  rope->left = rope_copy(original->left);
  rope->right = rope_copy(original->right);
  return rope;
}

/// Return a new rope with string inserted at index,
/// or NULL if the operation is not able to be completed.
// FIXME: This function became very monolithic over time.
Rope *rope_insert(Rope *rope, size_t index, char *str) {
  if (!rope || !str) {
    return NULL;
  }
  size_t contents_length = strlen(str);
  char *contents = malloc(contents_length > 0 ? contents_length + 1 : 8);
  strncpy(contents, str, contents_length);
  contents[contents_length] = '\0';
  if (index >= rope->weight) {
    // Append
    Rope *new_rope = malloc(sizeof(Rope));
    if (!new_rope) {
      return NULL;
    }
    Rope *new_contents = malloc(sizeof(Rope));
    if (!new_contents) {
      free(new_rope);
      return NULL;
    }

    new_contents->weight = contents_length;
    new_contents->string = contents;
    new_contents->right = NULL;
    new_contents->left = NULL;

    rope->right = new_contents;

    new_rope->weight = rope->weight + new_contents->weight;
    new_rope->string = NULL;
    new_rope->right = NULL;
    new_rope->left = rope;

    return new_rope;
  } else {
    // Insert
    size_t current_index = index + 1;
    Rope *current_rope = rope;
    while (!current_rope->string) {
      if (current_index > current_rope->weight) {
        current_index -= current_rope->weight;
        current_rope = current_rope->right;
      } else {
        // Update weights as we find the string to change.
        current_rope->weight += contents_length;
        current_rope = current_rope->left;
      }
    }

    if (current_index <= 1) {
      // INPUT: "This is a rope. | Appended."
      //
      //     <root> (27)-x
      //    /
      //   <node> (15)----------,
      //  /                      \
      // "This is a rope." (15)   " | Appended." (12)
      //
      // Attempting to prepend "| Prepended | ".
      //
      // OUTPUT: "| Prepended | This is a rope. | Appended."
      //
      //       <root> (41)-x
      //      /
      //     <node> (29)------------,
      //    /                        \
      //   <current_rope> (14)-,      " | Appended." (12)
      //  /                     \
      // "| Prepended | " (14)   "This is a rope." (15)
      // new_contents            rope_copy(current_rope)

      Rope *new_contents = malloc(sizeof(Rope));
      if (!new_contents) {
        return NULL;
      }

      new_contents->weight = contents_length;
      new_contents->string = contents;
      new_contents->right = NULL;
      new_contents->left = NULL;

      current_rope->right = rope_copy(current_rope);
      current_rope->left = new_contents;
      current_rope->weight = new_contents->weight;
      current_rope->string = NULL;

      rope_update_weights(current_rope);

      return rope;
    }

    // INPUT: "This is a rope. | Appended."
    //
    //     <root> (27)-x
    //    /
    //   <node> (15)---------,
    //  /                     \
    // "This is a rope." (15)  " | Appended." (12)
    //
    // Attempting to insert "| Inserted | " at index of eight.
    //
    // OUTPUT: "This is | Inserted | a rope. | Appended."
    //
    //         <root> (40)-x
    //        /
    //       <node> (28)---------,
    //      /                     \
    //     <current_rope> (21)-,   " | Appended." (12)
    //    /                     \
    //   <new_left> (8)-,        "a rope."
    //  /                \       new_right (7)
    // "This is "         "| Inserted | "
    // new_left_left (8)  new_contents (13)

    Rope *new_left_left = malloc(sizeof(Rope));
    if (!new_left_left) {
      return NULL;
    }
    Rope *new_left = malloc(sizeof(Rope));
    if (!new_left) {
      free(new_left_left);
      return NULL;
    }
    Rope *new_contents = malloc(sizeof(Rope));
    if (!new_contents) {
      free(new_left_left);
      free(new_left);
      return NULL;
    }
    Rope *new_right = malloc(sizeof(Rope));
    if (!new_right) {
      free(new_left_left);
      free(new_left);
      free(new_contents);
      return NULL;
    }

    size_t new_left_left_len = current_index + 1;
    char *new_left_left_str = malloc(new_left_left_len);
    strncpy(new_left_left_str
            , current_rope->string
            , new_left_left_len
            );
    new_left_left_str[new_left_left_len - 1] = '\0';

    new_left_left->string = new_left_left_str;
    new_left_left->weight = new_left_left_len - 1;
    new_left_left->left = NULL;
    new_left_left->right = NULL;

    new_contents->string = contents;
    new_contents->weight = contents_length - 1;
    new_contents->left = NULL;
    new_contents->right = NULL;

    new_left->string = NULL;
    new_left->weight = new_left_left->weight;
    new_left->left = new_left_left;
    new_left->right = new_contents;

    size_t new_right_len = current_rope->weight - current_index;
    char *new_right_str = malloc(new_right_len+1);
    strncpy(new_right_str
            , current_rope->string + current_index // skip left_left
            , new_right_len
            );
    new_right_str[new_right_len] = '\0';

    new_right->string = new_right_str;
    new_right->weight = new_right_len;
    new_right->left = NULL;
    new_right->right = NULL;

    free(current_rope->string);
    current_rope->string = NULL;
    current_rope->left = new_left;
    current_rope->right = new_right;

    rope_update_weights(current_rope);
  }
  return rope;
}

Rope *rope_append(Rope *rope, char *string) {
  return rope_insert(rope, SIZE_MAX, string);
}

Rope *rope_prepend(Rope *rope, char *string) {
  return rope_insert(rope, 0, string);
}

/// Return a new rope with string inserted at index,
/// or NULL if the operation is not able to be completed.
Rope *rope_insert_char(Rope *rope, size_t index, char c) {
  if (!rope) {
    return NULL;
  }
  if (index >= rope->weight) {
    // Append
    // TODO: Find right-most string-containing rope-node,
    // allocate a new buffer with size for the new char
    // as well as a null terminator, then replace string.
  } else {
    // Insert
    // TODO: Find string-containing rope-node by index (see above),
    // allocate a new buffer with size for the new char
    // as well as a null terminator, then replace string.
  }
  return rope;
}

// Either create a new string or append to existing.
char *rope_string(Rope *parent, Rope *rope, char *string) {
  if (!rope) {
    return NULL;
  }
  if (!string) {
    string = malloc(8);
    string[0] = '\0';
  }
  if (rope->string) {
    size_t len = strlen(string);
    size_t to_add = strlen(rope->string);
    string = realloc(string, len+to_add+1);
    if (!string) { return NULL; }
    strncat(string, rope->string, to_add);
    string[len+to_add] = '\0';
  } else {
    if (rope->left) {
      string = rope_string(rope, rope->left, string);
    }
    if (rope->right) {
      string = rope_string(rope, rope->right, string);
    }
  }
  return string;
}

void rope_free(Rope *rope) {
  if (rope->string) {
    free(rope->string);
  }
  if (rope->left) {
    rope_free(rope->left);
  }
  if (rope->right) {
    rope_free(rope->right);
  }
  free(rope);
}

void rope_print(Rope *rope, size_t given_depth) {
  if (!rope) {
    return;
  }
  size_t depth = given_depth;
  const char *depth_string = "  ";
  while (depth) {
    printf("%s", depth_string);
    depth -= 1;
  }
  if (rope->string) {
    printf("\"%s\" (%zu)\n"
           , rope->string
           , rope->weight
           );
    return;
  }
  printf("<node> (%zu)\n", rope->weight);
  depth = given_depth;
  while (depth) {
    printf("%s", depth_string);
    depth -= 1;
  }
  printf("l:\n");
  rope_print(rope->left, given_depth + 1);
  depth = given_depth;
  while (depth) {
    printf("%s", depth_string);
    depth -= 1;
  }
  printf("r:\n");
  rope_print(rope->right, given_depth + 1);
  depth = given_depth;
  while (depth) {
    printf("%s", depth_string);
    depth -= 1;
  }
  printf("END\n");
}
