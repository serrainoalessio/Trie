/*
    Multithread Trie library, fast implementation of trie data structure
    Copyright (C) 2016  Alessio Serraino

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see http://www.gnu.org/licenses/ .
*/

#include <stdlib.h> // malloc
#include <string.h> // memmove
#include <assert.h> // debugging
#include <math.h> // ceil, may compile with -ffast-math
#include <limits.h> // INT_MAX

// Child relocation options
#define CHILD_RELOC_FACTOR 1.618 // A good choiche is between 1.5 and 2
#define CHILD_RELOC_MAX ((int)sizeof(DATA_t)*8) // Maximum number of preventive allocs,
// It might depend on what you are doing: for example if you are storing english word lowercase
// setting it to 26 would be a good option (26 chars in alphabet)
// Keep in mind that phisical maximum number of child is sizeof(DATA_t)*8
// Set to INT_MAX to disable

#include <stdio.h>

// This function only allocs space, it does not initializes new child
static inline
void trie_add_first_two_childs(struct _childs * const childs) {
    childs->child_alloc = 2; // For the first allocs two childrens
    assert(childs->childs == NULL && childs->firsts == NULL); // First time here
    childs->childs = malloc((childs->child_alloc)*sizeof*(childs->childs));
    childs->firsts = malloc((childs->child_alloc)*sizeof*(childs->firsts));
    assert(childs->childs != NULL && childs->firsts != NULL);
    childs->child_num = 2;
}

// As the function above, it does not initialize new child
static inline
void trie_add_first_child(struct _childs * const childs) {
    trie_add_first_two_childs(childs); // allocs twice
    childs->child_num = 1;
}

static inline
void trie_alloc_childs(struct _childs * const childs) {
    trie_add_first_two_childs(childs); // allocs twice
    childs->child_num = 0;
}

static inline // resets a childs structure
void trie_init_childs(struct _childs * const childs) {
    childs->child_alloc = 0;
    childs->child_num = 0;
    childs->childs = NULL;
    childs->firsts = NULL;
//    trie_add_first_two_childs(childs);
//    childs->child_num = 0; // No child used
}

static inline
void trie_destroy_childs(struct _childs * const childs) {
    int i;
    for (i = 0; i < childs->child_num; i++)
        free(childs->childs[i]);
    free(childs->childs);
    free(childs->firsts);
    // This may not be necessary, but for a well done work resets also them
    childs->child_alloc = 0;
    childs->child_num = 0;
    childs->childs = NULL;
    childs->firsts = NULL;
}

static inline
int trie_search_in_childs(int * const res, const struct _childs * const childs, const DATA_t to_search) {
    const DATA_t * begin, * end; // Data is searched into an array of DATA_t
    const DATA_t * mid;

    // This function does not need to lock mutexes
    // because it actually doesn't read childs

    begin = childs->firsts; // Begin of the array
    end = childs->firsts + childs->child_num; // End of the array

    while (begin < end) {
        mid = begin + (end - begin) / 2; // Gets the middle point
        if (*mid > to_search) { // to_search may be before
            end = mid;
            continue; // next loop
        } else if (*mid < to_search) { // to_search may be after
            begin = mid + 1; // Begin has been compared, skip to the next one
            continue; // next loop
        } else { // exact match
            *res = (mid - childs->firsts);
            return 1; // Element found
        }
    }
    assert(begin == end); // They should be equals

    *res = (begin - childs->firsts); // Place where the element should be saved
    return 0; // Not found
}

#ifndef CHILD_RELOC_MAX
#    define CHILD_RELOC_MAX INT_MAX
#endif
static inline
void trie_insert_child(struct _childs * const childs, const int new_pos) {
    assert(new_pos <= childs->child_num); // May add in the middle or at the end

    // There are two cases: relocation needed, or relocation not needed
    if (childs->child_num >= childs->child_alloc) { // Needs relocation
        if ((CHILD_RELOC_MAX != INT_MAX) && (childs->child_alloc >= CHILD_RELOC_MAX)) {
            childs->child_alloc++; // Reached maximum, allocs one more
        } else {
            childs->child_alloc = ceil((childs->child_alloc)*CHILD_RELOC_FACTOR); // Increases alloc space
            if (childs->child_alloc > CHILD_RELOC_MAX) // If reached maximum
                childs->child_alloc = CHILD_RELOC_MAX; // Sets to the maximum
        }
        childs->childs = realloc(childs->childs, sizeof*(childs->childs)*(childs->child_alloc)); // Actually allocs
        childs->firsts = realloc(childs->firsts, sizeof*(childs->firsts)*(childs->child_alloc)); // Actually allocs
    } // else reallocation is not needed

    childs->child_num++; // Increases the number of children
    assert(childs->child_num <= childs->child_alloc); // At least one more should be allocated!

    // Now move forward by one position all the data after new_pos
    memmove((childs->childs) + new_pos + 1, (childs->childs) + new_pos, ((childs->child_num) - new_pos - 1)*sizeof*(childs->childs));
    memmove((childs->firsts )+ new_pos + 1, (childs->firsts) + new_pos, ((childs->child_num) - new_pos - 1)*sizeof*(childs->firsts));
}

static inline
void trie_remove_child(struct _childs * const childs, const int old_pos) { // This never reduces the size of the childs
    if (childs->child_num == 0) return; // Should not happen
    childs->child_num--; // One removed
    memmove((childs->childs) + old_pos, (childs->childs) + old_pos + 1, ((childs->child_num) - old_pos)*sizeof*(childs->childs));
    memmove((childs->firsts )+ old_pos, (childs->firsts) + old_pos + 1, ((childs->child_num) - old_pos)*sizeof*(childs->firsts));
}
