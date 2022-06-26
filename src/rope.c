#include <rope.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline void rope_set_string(Rope *rope, char *string) {
  if (!rope) { return; }
  char *old_string = rope->string;
  rope->string = string;
  if (old_string) {
    free(old_string);
  }
}

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

Rope *rope_from_buffer(uint8_t *bytes, size_t length) {
  if (!bytes || length == 0) {
    return NULL;
  }

  // FIXME: This doesn't feel right, but I can't write all the
  // buffer + length versions functions right now...
  // Replace null terminators with space for now.
  // Otherwise `rope_string()` seriously breaks (among other things).
  uint8_t *it = bytes;
  size_t count = 0;
  while (count < length) {
    if (*it == '\0') {
      *it = ' ';
    }
    it += 1;
    count += 1;
  }

  // Copy input string to heap.
  char *newstr = malloc(length + 1);
  if (!newstr) {
    return NULL;
  }
  memcpy(newstr, bytes, length);
  newstr[length] = '\0';

  // Allocate new rope nodes on heap.
  Rope *rope = malloc(sizeof(Rope));
  if (!rope) {
    free(newstr);
    return NULL;
  }
  Rope *left = malloc(sizeof(Rope));
  if (!left) {
    free(newstr);
    free(rope);
    return NULL;
  }

  // Initialize nodes.
  left->weight = length;
  left->string = newstr;
  left->right = NULL;
  left->left = NULL;

  rope->weight = length;
  rope->string = NULL;
  rope->right = NULL;
  rope->left = left;

  return rope;
}

Rope *rope_create(const char *str) {
  if (!str) {
    return NULL;
  }

  // Copy input string to heap.
  size_t len = strlen(str);
  char *newstr = malloc(len + 1);
  if (!newstr) {
    return NULL;
  }
  strncpy(newstr, str, len);
  newstr[len] = '\0';

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
    sum += rope_sum(rope->right);
  }
  return sum;
}

// Re-calculates weight based on what the current values are.
void rope_update_weights(Rope *rope) {
  if (!rope || rope->string) {
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
  if (contents_length == 1) {
    return rope_insert_byte(rope, index, str[0]);
  }
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
      rope_set_string(current_rope, NULL);

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

    rope_set_string(current_rope, NULL);
    current_rope->left = new_left;
    current_rope->right = new_right;

    rope_update_weights(current_rope);
  }
  return rope;
}

Rope *rope_prepend(Rope *rope, char *string) {
  return rope_insert(rope, 0, string);
}

Rope *rope_append(Rope *rope, char *string) {
  return rope_insert(rope, SIZE_MAX, string);
}

Rope *rope_insert_byte(Rope *rope, size_t index, char c) {
  // TODO: Split string-containing nodes that grow too large.
  if (!rope) { return NULL; }
  if (index == 0) {
    // Prepend byte to beginning.

    // Find left-most string-containing node.
    Rope *current_rope = rope;
    while (!current_rope->string) {
      if (current_rope->left) {
        current_rope = current_rope->left;
      } else if (current_rope->right) {
        current_rope = current_rope->right;
      } else {
        return NULL;
      }
    }
    // Re-allocate string.
    char *newstr = malloc(current_rope->weight + 2);
    if (!newstr) { return NULL; }
    strncpy(newstr + 1, current_rope->string, current_rope->weight);
    newstr[0] = c;
    newstr[current_rope->weight + 1] = '\0';
    current_rope->weight += 1;
    rope_set_string(current_rope, newstr);
    rope_update_weights(rope);
    return rope;

  } else if (index >= rope->weight) {
    // Append byte to end.

    // Find right-most string-containing node.
    Rope *current_rope = rope;
    while (!current_rope->string) {
      if (current_rope->right) {
        current_rope = current_rope->right;
      } else if (current_rope->left) {
        current_rope = current_rope->left;
      } else {
        return NULL;
      }
    }

    char *newstr = malloc(current_rope->weight + 2);
    if (!newstr) { return NULL; }
    strncpy(newstr, current_rope->string, current_rope->weight);
    newstr[current_rope->weight] = c;
    newstr[current_rope->weight + 1] = '\0';
    current_rope->weight += 1;
    rope_set_string(current_rope, newstr);
    rope_update_weights(rope);
    return rope;
  }

  // Insert
  size_t current_index = index + 1;
  Rope *current_rope = rope;
  while (!current_rope->string) {
    if (current_index > current_rope->weight) {
      current_index -= current_rope->weight;
      current_rope = current_rope->right;
    } else {
      current_rope = current_rope->left;
    }
  }

  if (current_index - 1 == 0) {
    // Prepend
    char *newstr = malloc(current_rope->weight + 2);
    if (!newstr) { return NULL; }
    newstr[0] = c;
    strncpy(newstr + 1
            , current_rope->string
            , current_rope->weight);
    newstr[current_rope->weight + 1] = '\0';
    current_rope->weight += 1;
    rope_set_string(current_rope, newstr);
    rope_update_weights(rope);
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
  // Attempting to insert '|' at index of 23.
  //
  // OUTPUT: "This is a rope. | Append|ed."
  //
  //         <root> (28)-x
  //        /
  //       <node> (15)-----------,
  //      /                       \
  //     "This is a rope." (15)    " | Append|ed." (13)

  char *newstr = malloc(current_rope->weight + 2);
  if (!newstr) { return NULL; }
  strncpy(newstr, current_rope->string, current_index);
  strncpy(newstr + current_index + 1
          , current_rope->string + current_index
          , current_rope->weight - current_index);
  newstr[current_index] = c;
  newstr[current_rope->weight + 1] = '\0';
  current_rope->weight += 1;
  rope_set_string(current_rope, newstr);
  rope_update_weights(rope);
  return rope;
}

Rope *rope_prepend_byte(Rope *rope, char c) {
  return rope_insert_byte(rope, 0, c);
}

Rope *rope_append_byte(Rope *rope, char c) {
  return rope_insert_byte(rope, SIZE_MAX, c);
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
    size_t to_add = rope->weight; // strlen(rope->string);
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

Rope *rope_remove_from_beginning(Rope *rope, size_t length) {
  // Find left-most string-containing node.
  int left_right = 0; // 0 if child is left, 1 if child is right of parent.
  Rope *parent_rope = NULL;
  Rope *current_rope = rope;
  while (!current_rope->string) {
    parent_rope = current_rope;
    if (current_rope->left) {
      current_rope = current_rope->left;
      left_right = 0;
    } else if (current_rope->right) {
      current_rope = current_rope->right;
      left_right = 1;
    } else {
      return NULL;
    }
  }

  if (length >= current_rope->weight) {
    // Remove node and call self with amount - current_rope->weight.

    // No parent rope means given rope is a string node, and we
    // can't really remove a node from itself.
    if (!parent_rope) {
      return NULL;
    }

    size_t current_rope_weight = current_rope->weight;

    // Shuffle nodes around.
    // TODO: Less code duplication with Rope **.
    if (left_right == 0) {
      // `current_rope` is equal to `parent_rope->left`.
      if (!parent_rope->right) {
        rope_free(rope);
        return rope_create("");
      }
      Rope *removed = parent_rope->left;
      *parent_rope = *parent_rope->right;
      rope_free(removed);
    } else {
      // `current_rope` is equal to `parent_rope->right`.
      if (!parent_rope->left) {
        rope_free(rope);
        return rope_create("");
      }
      Rope *removed = parent_rope->right;
      *parent_rope = *parent_rope->left;
      rope_free(removed);
    }

    rope_update_weights(rope);

    // If node contained perfect length, we are all done.
    if (length - current_rope_weight == 0) {
      return rope;
    }
    // Else, we must continue removing.
    return rope_remove_from_beginning(rope, length - current_rope_weight);
  }

  // Make smaller string from right side of current string.
  size_t newstr_len = current_rope->weight - length;
  char *newstr = malloc(newstr_len + 1);
  strncpy(newstr, current_rope->string + length, newstr_len);
  newstr[newstr_len] = '\0';
  current_rope->weight = newstr_len;
  rope_set_string(current_rope, newstr);
  rope_update_weights(rope);
  return rope;
}

Rope *rope_remove_from_end(Rope *rope, size_t length) {
  if (!rope || !rope->weight) {
    return NULL;
  }
  // Find right-most string-containing node.
  int left_right = 0; // 0 if child is left, 1 if child is right of parent.
  Rope *parent_rope = NULL;
  Rope *current_rope = rope;
  while (!current_rope->string) {
    parent_rope = current_rope;
    if (current_rope->right) {
      current_rope = current_rope->right;
      left_right = 1;
    } else if (current_rope->left) {
      current_rope = current_rope->left;
      left_right = 0;
    } else {
      return NULL;
    }
  }

  if (length >= current_rope->weight) {
    // Remove node and call self with amount - current_rope->weight.

    if (!parent_rope) {
      return NULL;
    }

    size_t current_rope_weight = current_rope->weight;

    if (left_right == 0) {
      if (!parent_rope->right) {
        rope_free(rope);
        return rope_create("");
      }
      Rope *removed = parent_rope->left;
      *parent_rope = *parent_rope->right;
      rope_free(removed);
    } else {
      if (!parent_rope->left) {
        rope_free(rope);
        return rope_create("");
      }
      Rope *removed = parent_rope->right;
      *parent_rope = *parent_rope->left;
      rope_free(removed);
    }

    if (length - current_rope_weight == 0) {
      return rope;
    }
    return rope_remove_from_end(rope, length - current_rope_weight);
  }

  // Re-allocate smaller string for node.
  size_t newstr_len = current_rope->weight - length;
  char *newstr = malloc(newstr_len + 1);
  strncpy(newstr, current_rope->string, newstr_len);
  newstr[newstr_len] = '\0';
  current_rope->weight = newstr_len;
  rope_set_string(current_rope, newstr);
  rope_update_weights(rope);
  return rope;
}

Rope *rope_remove_span(Rope *rope, size_t offset, size_t length) {
  if (!rope) {
    return NULL;
  }

  if (offset == 0) {
    return rope_remove_from_beginning(rope, length);
  } else if (offset >= rope->weight) {
    return rope_remove_from_end(rope, length);
  } else {
    // Remove from middle.

    size_t initial_index = offset + 1;
    size_t current_index = initial_index;
    int left_right = 0; // 0 if child is left, 1 if child is right of parent.
    Rope *parent_rope = NULL;
    Rope *current_rope = rope;
    while (!current_rope->string) {
      parent_rope = current_rope;
      if (current_index > current_rope->weight) {
        current_index -= current_rope->weight;
        current_rope = current_rope->right;
        left_right = 1;
      } else {
        current_rope = current_rope->left;
        left_right = 0;
      }
    }
    size_t current_offset = current_index - 1;
    if (current_offset == 0) {
      // Remove from beginning of `current_rope`.
      if (length >= current_rope->weight) {
        // Remove node and call self with amount - current_rope->weight.

        // No parent rope means given rope is a string node, and we
        // can't really remove a node from itself.
        if (!parent_rope) {
          return NULL;
        }

        size_t current_rope_weight = current_rope->weight;

        // Shuffle nodes around.
        // TODO: Less code duplication with Rope **.
        if (left_right == 0) {
          // `current_rope` is equal to `parent_rope->left`.
          if (!parent_rope->right) {
            rope_free(rope);
            return rope_create("");
          }
          Rope *removed = parent_rope->left;
          *parent_rope = *parent_rope->right;
          rope_free(removed);
        } else {
          // `current_rope` is equal to `parent_rope->right`.
          if (!parent_rope->left) {
            rope_free(rope);
            return rope_create("");
          }
          Rope *removed = parent_rope->right;
          *parent_rope = *parent_rope->left;
          rope_free(removed);
        }

        // If node contained perfect length, we are all done.
        if (length - current_rope_weight == 0) {
          rope_update_weights(rope);
          return rope;
        }
        // Else, we must continue removing.
        return rope_remove_span(rope, offset, length - current_rope_weight);
      }

      // Make smaller string from right side of current string.
      size_t newstr_len = current_rope->weight - length;
      char *newstr = malloc(newstr_len + 1);
      if (!newstr) { return NULL; }
      strncpy(newstr, current_rope->string + length, newstr_len);
      newstr[newstr_len] = '\0';
      current_rope->weight = newstr_len;
      rope_set_string(current_rope, newstr);
      rope_update_weights(rope);
      return rope;

    } else if (length >= current_rope->weight - current_index) {
      // Remove everything after current_index, then continue removing recursively.
      current_index -= 1;
      size_t rope_weight = current_rope->weight;
      size_t newstr_len = current_index;
      char *newstr = malloc(newstr_len + 1);
      if (!newstr) { return NULL; }
      strncpy(newstr, current_rope->string, newstr_len);
      newstr[newstr_len] = '\0';
      current_rope->weight = newstr_len;
      rope_set_string(current_rope, newstr);
      rope_update_weights(rope);

      // Perfect match for removal length and length of node.
      size_t remove_length = rope_weight - current_index;
      if (remove_length == 0) {
        return rope;
      }
      return rope_remove_span(rope, offset, length - remove_length);

    } else {
      // Remove a span inside of string of current_rope.
      size_t newstr_len = current_rope->weight - length;
      char *newstr = malloc(newstr_len + 1);
      if (!newstr) { return NULL; }
      strncpy(newstr, current_rope->string, current_index);
      strncpy(newstr + current_index
              , current_rope->string + current_index + length
              , newstr_len - current_index + 1);
      newstr[newstr_len] = '\0';
      current_rope->weight = newstr_len;
      rope_set_string(current_rope, newstr);
    }
    rope_update_weights(rope);
    return rope;
  }
  return NULL;
}

char *rope_lines(Rope *rope, size_t start_line, size_t count) {
  if (!rope || count == 0) { return NULL; }
  char *string = rope_string(NULL, rope, NULL);
  if (!string) { return NULL; }
  char *it = string;
  size_t line_count = 0;
  // Advance to `start_line`.
  while (*it != '\0' && line_count < start_line) {
    if (*it == '\n') {
      line_count += 1;
    }
    it++;
  }
  if (*it == '\0') {
    return NULL;
  }
  // Record beginning of `start_line`, iterate to end of `count` lines.
  char *beg = it;
  size_t gathered_lines = 0;
  while (*it != '\0') {
    if (*it == '\n') {
      gathered_lines += 1;
      if (gathered_lines >= count) {
        break;
      }
    }
    it++;
  }
  size_t length = it - beg;
  char *lines = malloc(length + 1);
  if (!lines) { return NULL; }
  memcpy(lines, beg, length);
  lines[length] = '\0';
  free(string);
  return lines;
}

char *rope_line(Rope *rope, size_t line) {
  return rope_lines(rope, line, 1);
}
