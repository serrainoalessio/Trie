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

#include <string.h> // memcpy
#include "trie.h" // trie data type

// All the utils functions are defined here
#include "trie_mutex.c" // It is not a good practice to include *.c files
#include "trie_childs.c" // Each files includes all the necessary
#include "trie_utils.c" // Include this at last

//  ===================
//  ==== TRIE INIT ====
//  ===================

void trie_init(trie_ptr_t t) {
    if (t == NULL)
        return; // Invalid ptr

    // Mutex objects must be initialized
    trie_init_mutex(&(t->lock));

    t->childs.child_num = 0;
    t->childs.child_alloc = 0;
    t->childs.childs = NULL; // This is gonna be changed when adding some data
    t->childs.firsts = NULL;

    t->data.data = NULL; // This does not makes sense for a non-empty node
    t->data.len = 0; // This marks an empty trie
    t->data.end = 0;
    t->data.dealloc = 0;
}

//  ====================
//  ==== TRIE CLEAR ====
//  ====================

void trie_clear(trie_ptr_t t) {
    int i;
    if (t == NULL)
        return; // Invalid ptr
    trie_writelock(&(t->lock));
    // Now have to destroy each child
    for (i = 0; i < trie_get_child_num(t); i++)
        trie_clear(trie_get_child(t, i));
    trie_destroy_node_without_child(t);
}

#ifndef NDEBUG // Debugging
#include <stdio.h>

static inline
void __print_trie_helper(struct _trie * parent, int n_child, const int depth) {
    int i;
    struct _trie * t = trie_get_child(parent, n_child);

    // Locks mutex for reading
    trie_readlock(&(t->lock));

    for (i = 0; i < depth-1; i++)
        printf(" | ");
    if (depth != 0)
        printf(" +-");
    printf("%c", trie_get_first(parent, n_child));
    for (i = 0; i < t->data.len; i++)
        printf("%c", t->data.data[i]);
    if (trie_data_end(t))
        printf("*");
    printf("\n");

    for (i = 0; i < trie_get_child_num(t); i++) {
        if (i < trie_get_child_num(t) - 1)
            assert(trie_get_first(t, i) < trie_get_first(t, i + 1)); // Checks first ordering
        __print_trie_helper(t, i, depth + 1);
    }

    // Unlocks mutex
    trie_unlock(&(t->lock));
}

void print_trie(trie_ptr_t t) {
    int i, j;

    if (t == NULL) // Not actually a trie
        return;

    trie_readlock(&(t->lock));

    for (j = 0; j < t->data.len; j++)
        printf("%c", t->data.data[j]);
    if (trie_data_end(t))
        printf("*");
    printf("\n");
    for (i = 0; i < trie_get_child_num(t); i++) {
        if (i < trie_get_child_num(t) - 1)
            assert(trie_get_first(t, i) < trie_get_first(t, i + 1)); // Checks first ordering
        __print_trie_helper(t, i, 1);
    }

    trie_unlock(&(t->lock));
}
#else // Not debugging
#    define print_trie(trie) // Do nothing macro!
#endif

// =====================
// ====== TRIE ADD =====
// =====================

static inline
void trie_fill_root_node(trie_ptr_t t, const DATA_t * arr, int len) {
    trie_attach_new_data(t, arr, len); // Copy data
    trie_init_childs(&(t->childs)); // Inits root node (it should be already initialized)
    trie_alloc_childs(&(t->childs)); // Allocs two children
    trie_unlock(&(t->lock)); // Not needed anymore
}

static inline
void trie_add_helper(trie_ptr_t t, const DATA_t * arr, int len){
    int mismatch; // data counter
    int special, a_id, b_id; // identifiers
    struct _childs temp_childs; // Temporany data holder
    struct _trie * cur, * next; // current root pointer (not reallocable)

    cur = t;
    while (1) {
        assert(trie_correct_child_num(cur)); // Effectively used chidls less than allocated
        // Looks for the first mismatching character. It is right to search again if lock was not acquired
        mismatch = find_first_mismatch(arr, len, trie_data(cur), trie_data_len(cur));

        // Now parse
        if ((mismatch == trie_data_len(cur)) && (mismatch == len)) { // Reached end of data, and end of node
            if (trie_data_end(cur)) break; // Element already exists
            if (trie_upgrade_lock(&(cur->lock)) != 0) // Lock gained, do what to do
                continue;
            trie_set_data_end(cur); // simply sets the end flag, finish
            break;
        } else if ( (mismatch == trie_data_len(cur)) && trie_empty_childs(cur) ) { // Reached end of stored data
            if (trie_upgrade_lock(&(cur->lock)) != 0) // Lock gained, do what to do
                continue;
            assert(len > mismatch); // there is always a next character
            assert(trie_data_end(cur)); // Beacuse of empty childs

            if (!trie_is_root(t, cur)) // Not the root node, root node does not need alloc
                trie_alloc_childs(&(cur->childs)); // normal alloc
            trie_insert_init_child(cur, 0); // inserts and inits a child
            trie_attach_new_data(trie_get_child(cur, 0), arr + mismatch + 1, len - (mismatch + 1));
            trie_attach_first_data(cur, 0, arr[mismatch]);
            assert(trie_correct_child_num(trie_get_child(cur, 0)));
            break; // End
        } else if (mismatch == trie_data_len(cur)) { // Reached end of stored data, has childs
            // Let's start by binary searching the next character inside the childs
            assert(mismatch < len); //  there is always a next character, so can access arr[mismatch]
            a_id = trie_search_in_childs(&b_id, &(cur->childs), arr[mismatch]); // Binary search in child nodes
            if (a_id) { // Element was found, calls to add now became recursive ...
                next = trie_get_child(cur, b_id); // New data should be added here
                trie_readlock_upgrd(&(next->lock)); // Readlocks next.
                trie_unlock(&(cur->lock)); // Unlocks current. N.B. Keep order
                arr += (mismatch + 1); // Moves forward the array data
                len -= (mismatch + 1);
                cur = next;
                continue; // Continues while loop
            } else { // Element was not found, inserts a new one, b_id contains new position
                if (trie_upgrade_lock(&(cur->lock)) != 0) // Lock gained, do what to do
                    continue;
                trie_insert_init_child(cur, b_id);  // adds a child, remember b_id is its position
                trie_attach_new_data(trie_get_child(cur, b_id), arr + mismatch + 1, len - (mismatch + 1));
                trie_attach_first_data(cur, b_id, arr[mismatch]);
                assert(trie_correct_child_num(trie_get_child(cur, b_id)));
                break;
            }
        } else if (mismatch == len) {
            if (trie_upgrade_lock(&(cur->lock)) != 0) // Lock gained, do what to do
                continue;
            assert(mismatch < trie_data_len(cur));

            special = trie_is_root(t, cur) && trie_empty_childs(cur);
            // New node innherits childs, so need to save them
            if (!special) { // Not root, or root with no childs
                memcpy(&temp_childs, &(cur->childs), sizeof(temp_childs));
                trie_init_childs(&(cur->childs)); // Resets current childs
                trie_add_first_child(&(cur->childs)); // Allocs them again
            } else { // Special algirithm for first child of root node
                trie_insert_child(&(cur->childs), 0);  // adds a child, b_id must be it's position
            }
            trie_init_new_child(cur, 0); // Inits the just created child
            trie_attach_existent_data(trie_get_child(cur, 0),
                                      trie_data(cur) + mismatch + 1, trie_data_len(cur) - (mismatch + 1));
            trie_attach_first_data(cur, 0, trie_data(cur)[mismatch]);
            trie_data_len(cur) = mismatch; // shrinks current data lenght
            if (trie_data_end(cur)) { // Sets end in new child
                trie_set_data_end(trie_get_child(cur, 0));
            } else { // Data does not end there
                trie_set_data_end(cur); // Data ends before child
                trie_clear_data_end(trie_get_child(cur, 0));
            }
            // Now attaches old childs to new data
            if (!special)
                memcpy(&(trie_get_child(cur, 0)->childs), &temp_childs, sizeof(temp_childs));
            else
                trie_init_childs(&(trie_get_child(cur, 0)->childs)); // Inits to null

            assert(trie_correct_child_num(trie_get_child(cur, 0)));
            break;
        } else { // Normal case
            if (trie_upgrade_lock(&(cur->lock)) != 0) // Lock gained, do what to do
                continue;
            assert(mismatch < trie_data_len(cur));
            assert(mismatch < len);

            special = trie_is_root(t, cur) && trie_empty_childs(cur); // Use special algorithm
            if (!special) { // Normal case
                // New node innherits childs, so need to save them
                memcpy(&temp_childs, &(cur->childs), sizeof(temp_childs));
                trie_init_childs(&(cur->childs)); // Resets current childs
                trie_add_first_two_childs(&(cur->childs)); // Allocs them again
            } else { // Special algorithm
                trie_insert_child(&(cur->childs), 0);  // adds a child, b_id must be it's position
                trie_insert_child(&(cur->childs), 1);  // adds a child, b_id must be it's position
            }
            trie_init_new_child(cur, 0);
            trie_init_new_child(cur, 1); // Inits the just created childs

            // Choses which child will be the first, to keep the order
            // remember: 'mismatch' is the first mismatch
            // i.e. it is impossible cur->data[mismatch] == arr[mismatch]
            if (trie_data(cur)[mismatch] < arr[mismatch]) {
                a_id = 0;
                b_id = 1; // New data goes after (at id 1)
            } else { // cur->data[mismatch] > arr[mismatch], they can't be equal
                a_id = 1;
                b_id = 0; // New data goes before (at id 0)
            }

            // Sets old data
            trie_attach_existent_data(trie_get_child(cur, a_id),
                                      trie_data(cur) + mismatch + 1, trie_data_len(cur) - (mismatch + 1));
            trie_attach_first_data(cur, a_id, trie_data(cur)[mismatch]);
            if (!special)
                memcpy(&(trie_get_child(cur, a_id)->childs), &temp_childs, sizeof(temp_childs));
            else
                trie_init_childs(&(trie_get_child(cur, a_id)->childs)); // Inits to null
            trie_data_end(trie_get_child(cur, a_id)) = trie_data_end(cur);
            trie_data_len(cur) = mismatch; // shrinks current data lenght

           // Now new data
           trie_attach_new_data(trie_get_child(cur, b_id), arr + mismatch + 1, len - (mismatch + 1));
           trie_attach_first_data(cur, b_id, arr[mismatch]);
           trie_init_childs(&(trie_get_child(cur, b_id)->childs)); // Inits to null

           trie_clear_data_end(cur); // Now current node surely does not contain a data end

           assert(trie_correct_child_num(trie_get_child(cur, a_id)));
           assert(trie_correct_child_num(trie_get_child(cur, b_id)));
           break;
        }
        assert(0);
        __builtin_unreachable();
    } // end while

    assert(trie_correct_child_num(cur)); // Effectively used chidls less than allocated
    trie_unlock(&(cur->lock));
}

void trie_add(trie_ptr_t t, const DATA_t * arr, int len) {
    int upgrade_res;

    if ((t == NULL) || (arr == NULL))
        return; // Invalid ptr

    trie_readlock_upgrd(&(t->lock)); // locks root trie read mutex, upgadable
    while (1) {
        if (trie_is_empty(t)) {
            upgrade_res = trie_upgrade_lock(&(t->lock)); // tries lock upgrading
            if (upgrade_res == 0) { // lock gained, none has written
                trie_fill_root_node(t, arr, len); // Fills root and releases mutex
                break; // Finish
            } else { // Lock not gained, someone got it
                continue; // So go back and checks again the condition
            }
        } else { // Trie not empty (general case)
            trie_add_helper(t, arr, len);
            break; // Finish
        }
    }
    // print_trie(t); // debug purpose
}

// =====================
// ==== TRIE REMOVE ====
// =====================

void trie_remove(trie_ptr_t t, const DATA_t * arr, int len) {
    int mismatch; // data counter
    int found, pos; // Data search index
    struct _trie * cur, * next; // current root pointer (not reallocable)
    struct _trie * prev; // Previous used node
    
    int newlen; // Temporany new data lenght
    DATA_t * newdata; // Temporany data holder

    // Basic checking
    if (t == NULL || arr == NULL)
        return; // No data to delete!

    trie_readlock_upgrd(&(t->lock)); // locks root trie read mutex, upgadable
    if (trie_is_empty(t)) { // No data to delete
        trie_unlock(&(t->lock));
        return;
    }

    cur = prev = t;
    while (1) {
        assert(trie_correct_child_num(cur)); // Effectively used chidls less than allocated
        // Looks for the first mismatching character
        mismatch = find_first_mismatch(arr, len, trie_data(cur), trie_data_len(cur));

        // Now parse
        if ((mismatch == trie_data_len(cur)) && (mismatch == len)) { // Reached end of data, and end of node
            if (trie_data_end(cur) == 0) {
                assert(!trie_empty_childs(cur)); // Should be at least one child
                break; // Data does not exist, do not remove nothing
            }
            
            if (trie_upgrade_lock(&(cur->lock)) != 0) // Lock gained, now cur is readlocked
                continue; // Needs to read again the data

            if (trie_get_child_num(cur) >= 2) { // More than two childs, cannot remove node
                trie_clear_data_end(cur); // simply clears the end flag, finish
                trie_unlock(&(cur->lock)); // Unlocks current node
            } else if (trie_get_child_num(cur) == 1) { // Must merge the only child
                next = trie_get_child(cur, 0); // Sets next to the only child
                // need a readlock on the child
                trie_readlock(&(next->lock)); // Gains readlock
                newlen = trie_data_len(cur) + trie_data_len(next) + 1; // +1 is for the first character
                
                if (!(cur->data.dealloc) && !(next->data.dealloc) &&
                        trie_data(cur) + trie_data_len(cur) + 1 == trie_data(next) ) {
                    // Do nothing!, data is already where it should be
                    assert(trie_data(cur)[trie_data_len(cur)] == trie_get_first(cur, 0));
                } else { // Data was allocated, copies inside this buffer
                    if (cur->data.dealloc) { // Reallocs current data
                        trie_data(cur) = realloc((DATA_t*)trie_data(cur), newlen*sizeof*trie_data(cur));
                    } else { // Data for current node is not allocated, must malloc all the data
                        newdata = malloc(newlen*sizeof*trie_data(cur)); // Allocs a new chunk of data
                        assert(newdata); // This is a bit like attach data, but this adds some extra bytes at the end
                        cur->data.dealloc = 1; // needs freeing
                        memcpy(newdata, trie_data(cur), trie_data_len(cur)*sizeof*trie_data(cur)); // Copies old data
                        trie_data(cur) = newdata; // Links new data
                    }
                    memcpy((DATA_t*)trie_data(cur) + trie_data_len(cur), &trie_get_first(cur, 0),
                            sizeof*trie_data(cur)); // Copies the first byte
                    memcpy((DATA_t*)trie_data(cur) + trie_data_len(cur) + 1, trie_data(next), // Copies the rest
                            trie_data_len(next)*sizeof*trie_data(cur));
                }
                trie_data_len(next) = newlen;
                trie_data(next) = trie_data(cur); // attaches current data to child
                next->data.dealloc = cur->data.dealloc; // innherits dealloc property
                // now links the child node to the parent
                trie_get_child(prev, pos) = next;

                // Now destroys current node (and frees the readlock)
                trie_destroy_node_without_child(cur); // Destroys all allocs for the current node
                if (!trie_is_root(t, cur)) 
                    free(cur);

                trie_unlock(&(next->lock)); // Lock no more needed
            } else { // No childs
                // Now destroys the current node. no child is allocated.
                trie_destroy_node_without_child(cur); // Destroys all allocs for the current node
                if (!trie_is_root(t, cur)) { // Root node, uses a special algorithm 
                    // If not the root can unlink from the prior
                    free(cur); // cur was allocated with malloc
                    trie_remove_child(&(prev->childs), pos);
                } else { // cur is the root node, needs to reset back to void root
                    trie_get_childs(cur) = NULL;
                    trie_get_child_num(cur) = 0;
                    cur->childs.child_alloc = 0;
                }
            }

            break;
        } else if ( (mismatch == trie_data_len(cur)) && trie_empty_childs(cur) ) { // Reached end of stored data
            assert(len > mismatch); // there is always a next character
            assert(trie_data_end(cur)); // Beacuse of empty childs
            break; // Element does not exist
        } else if (mismatch == trie_data_len(cur)) { // Reached end of stored data, has childs
            // Let's start by binary searching the next character inside the childs
            assert(mismatch < len); //  there is always a next character, so can access arr[mismatch]
            found = trie_search_in_childs(&pos, &(cur->childs), arr[mismatch]); // Binary search in child nodes
            if (found) { // Element was found, calls to add now became recursive ...
                next = trie_get_child(cur, pos); // Moves to the next node
                if (!trie_is_root(t, cur)) // True every time except the first here
                    trie_unlock(&(prev->lock)); // Unlocks previous. N.B. Keep order
                else {} // If trie root node do not unlocks anything!
                trie_readlock_upgrd(&(next->lock)); // Readlocks next, with an upgradable lock
                arr += (mismatch + 1); // Moves forward the array data
                len -= (mismatch + 1);
                prev = cur; // Stores the current node
                cur = next; // and moves to the nexe
                continue; // Continues while loop
            } else { // Element was not found, inserts a new one
                break; // Element does not exists
            }
        } else if (mismatch == len) {
            assert(mismatch < trie_data_len(cur));
            break; // Element does not exists
        } else { // Normal case
            assert(mismatch < trie_data_len(cur));
            assert(mismatch < len);
            break; // Element does not exists
        }
        assert(0);
        __builtin_unreachable();
    } // end while

    assert(trie_correct_child_num(prev)); // Effectively used chidls less than allocated
    trie_unlock(&(prev->lock));
}

// ===================
// ==== TRIE FIND ====
// ===================

int trie_find(trie_ptr_t t, const DATA_t * arr, int len) {
    int mismatch; // data counter
    int retval;
    int a_id, b_id; // identifiers
    struct _trie * cur, * next; // current root pointer (not reallocable)

    if (t == NULL)
        return 0; // Invalid ptr

    trie_readlock(&(t->lock)); // locks root trie read mutex
    if (trie_is_empty(t)) {
        retval = 0; // Empty trie
    } else { // Trie not empty (general case)
        cur = t;
        while (1) {
            // Looks for the first mismatching character
            mismatch = find_first_mismatch(arr, len, trie_data(cur), trie_data_len(cur));

            // Now parse
            if ((mismatch == trie_data_len(cur)) && (mismatch == len)) { // Reached end of data, and end of node
                retval = trie_data_end(cur);
                break;
            } else if ( (mismatch == trie_data_len(cur)) && trie_empty_childs(cur) ) { // Reached end of stored data
                assert(len > mismatch); // there is always a next character
                assert(trie_data_end(cur)); // Because of empty childs

                retval = 0; // Not found
                break; // End
            } else if (mismatch == trie_data_len(cur)) { // Reached end of stored data, has childs
                // Let's start by binary searching the next character inside the childs
                assert(mismatch < len); //  there is always a next character, so can access arr[mismatch]
                a_id = trie_search_in_childs(&b_id, &(cur->childs), arr[mismatch]); // Binary search in child nodes
                if (a_id) { // Element was found, calls to add now became recursive ...
                    next = trie_get_child(cur, b_id); // New data should be added here
                    trie_readlock(&(next->lock)); // Readlocks next.
                    trie_unlock(&(cur->lock)); // Unlocks current. N.B. Keep order
                    arr += (mismatch + 1); // Moves forward the array data
                    len -= (mismatch + 1);
                    cur = next;
                    continue; // Continues while loop
                } else { // Element was not found, inserts a new one
                    retval = 0;
                    break;
                }
            } else if (mismatch == len) { // Middle of the data, data is longer
                assert(mismatch < trie_data_len(cur));

                retval = 0;
                break;
            } else { // Both mismatches in the middle of the data
                assert(mismatch < trie_data_len(cur));
                assert(mismatch < len);

                retval = 0;
                break;
            }
            assert(0);
            __builtin_unreachable();
        } // end while
    }

    trie_unlock(&(cur->lock));
    return retval;
}

// ============================
// ===    TRIE GET SUFFIX   ===
// ============================

int trie_get_suffix(trie_ptr_t t, const DATA_t * arr, int len, trie_arr_t * suffix) {
    int mismatch; // data counter
    int retval;
    int a_id, b_id; // identifiers
    struct _trie * cur, * next; // current root pointer (not reallocable)

    if (t == NULL)
        return TRIE_NO_SUFFIX_FOUND; // Invalid ptr

    trie_readlock(&(t->lock)); // locks root trie read mutex
    if (trie_is_empty(t)) {
        retval = TRIE_NO_SUFFIX_FOUND; // Empty trie
    } else { // Trie not empty (general case)
        cur = t;
        while (1) {
            // Looks for the first mismatching character
            mismatch = find_first_mismatch(arr, len, trie_data(cur), trie_data_len(cur));

            // Now parse
            if ((mismatch == trie_data_len(cur)) && (mismatch == len)) { // Reached end of data, and end of node
                if (trie_data_end(cur)) { // Data ends here
                    // Asserts there are no childs
                    if (trie_empty_childs(cur)) { // Suffix has no lenght
                        if (suffix != NULL)
                                trie_arr_len(suffix) = 0;
                        retval = TRIE_SUFFIX_FOUND; 
                    } else { // Not univocal way of interpretation
                        retval = TRIE_NO_SUFFIX_FOUND;
                    }
                } else { // Date does not ends here.
                    assert(!trie_empty_childs(cur)); // Because of non ending data
                    assert(trie_get_child_num(cur) != 1); // A non-ending node may not have one child
                    retval = TRIE_MULTIPLE_SUFFIX;
                }
                break;
            } else if ( (mismatch == trie_data_len(cur)) // Reached end of stored data
                        && trie_empty_childs(cur) ) { // And has no childs
                assert(len > mismatch); // there is always a next character
                assert(trie_data_end(cur)); // Because of empty childs
                retval = TRIE_NO_SUFFIX_FOUND; // Not found
                break; // End
            } else if (mismatch == trie_data_len(cur)) { // Reached end of stored data, has childs
                // Let's start by binary searching the next character inside the childs
                assert(mismatch < len); //  there is always a next character, so can access arr[mismatch]
                a_id = trie_search_in_childs(&b_id, &(cur->childs), arr[mismatch]); // Binary search in child nodes
                if (a_id) { // Element was found, calls to add now became recursive ...
                    next = trie_get_child(cur, b_id); // New data should be added here
                    trie_readlock(&(next->lock)); // Readlocks next.
                    trie_unlock(&(cur->lock)); // Unlocks current. N.B. Keep order
                    arr += (mismatch + 1); // Moves forward the array data
                    len -= (mismatch + 1);
                    cur = next;
                    continue; // Continues while loop
                } else { // Element was not found, inserts a new one
                    retval = TRIE_NO_SUFFIX_FOUND;
                    break;
                }
            } else if (mismatch == len) { // Middle of the data, data is longer than search
                assert(mismatch < trie_data_len(cur)); // Less, not less or equal
                if (trie_data_end(cur)) { // Data ends here!
                    if (suffix != NULL) { // if suffix == NULL, do not copy data, only returns the value
                        trie_arr_len(suffix) = trie_data_len(cur) - mismatch; // stores the remaining part of the data
                        trie_arr_data(suffix) = realloc(trie_arr_data(suffix), trie_arr_len(suffix)*sizeof*trie_arr_data(suffix));
                        memcpy(trie_arr_data(suffix), trie_data(cur) + mismatch, trie_arr_len(suffix)*sizeof*trie_data(cur)); // Copies the rest of the data
                    }
                    retval = TRIE_SUFFIX_FOUND;
                } else { // Data does not end with this node
                    assert(!trie_empty_childs(cur)); // Because of non end data
                    assert(trie_get_child_num(cur) != 1); // A non-ending node may not have one child
                    retval = TRIE_MULTIPLE_SUFFIX;
                }
                break;
            } else { // Both mismatches in middle, it is not possible to have a suffix
                assert(mismatch < trie_data_len(cur));
                assert(mismatch < len);
                retval = TRIE_NO_SUFFIX_FOUND;
                break;
            }
            assert(0);
            __builtin_unreachable();
        } // end while
    }

    trie_unlock(&(cur->lock));
    return retval;
}


// ==========================
// ===   TRIE ITERATORS   ===
// ==========================

static inline // Gets first useful iterator. This function always succedes
void trie_get_first_iterator(trie_ptr_t t, trie_iterator_t * iterator, int offset) {
    struct _trie * cur, *next;
    if (offset > trie_iterator_data_len(iterator)) {
        printf("Error here\n");
        fflush(stdout);
    }
    assert(offset <= trie_iterator_data_len(iterator));

    cur = t; // Asserts t was already readlocked
    while (1) {
        trie_iterator_substitute_end(iterator, offset, trie_data(cur), trie_data_len(cur)); // Substitutes last data
        if (trie_data_end(cur) || trie_empty_childs(cur)) { // Actually, the second implies the first
            trie_unlock(&(cur->lock));
            break;
        }
        offset += trie_data_len(cur); // Writes next data after the first
        trie_iterator_substitute_end(iterator, offset, &(trie_get_first(cur, 0)), 1); // adds the first character
        offset++; // First data inside child. If a child exists must exists also first data
        next = trie_get_child(cur, 0); // Goes in the first child
        trie_readlock(&(next->lock)); // Readlocks next.
        trie_unlock(&(cur->lock)); // Unlocks current. N.B. Keep order
        cur = next;
    }

    assert(trie_iterator_data_len(iterator) <= iterator->alloc); // Debug only, asserts not writing after allocated memory
}

static inline // This is recursive, inlines only what can
int trie_next_iterator_helper(trie_ptr_t t, trie_iterator_t * iterator, int cur_offset) {
    struct _trie * next;
    int mismatch, res;
    int pos, found;

    // As usual searches for the first mismathc
    mismatch = find_first_mismatch(trie_iterator_data(iterator) + cur_offset, trie_iterator_data_len(iterator) - cur_offset, trie_data(t), trie_data_len(t));
    if ((mismatch == trie_data_len(t)) && (mismatch + cur_offset == trie_iterator_data_len(iterator))) { // Reached end of data, and end of node
        if (!trie_empty_childs(t)) {
            next = trie_get_child(t, 0);
            trie_iterator_substitute_end(iterator, cur_offset + trie_data_len(t), &(trie_get_first(t, 0)), 1); // adds the first character
            trie_readlock(&(next->lock));
            trie_get_first_iterator(next, iterator, cur_offset + trie_data_len(t) + 1);
            return 1; // Success, data modified
        } else {
            return 0; // No childs!
        }
    } else if ( (mismatch == trie_data_len(t)) && trie_empty_childs(t) ) { // Reached end of stored data
        assert(trie_iterator_data_len(iterator) > mismatch + cur_offset); // there is always a next character
        assert(trie_data_end(t)); // Because of empty childs
        return 0; // Nothig to do
    } else if (mismatch == trie_data_len(t)) { // Reached end of stored data, has childs
        // Let's start by binary searching the next character inside the childs
        assert(mismatch + cur_offset < trie_iterator_data_len(iterator)); //  there is always a next character, so can access arr[mismatch]
        found = trie_search_in_childs(&pos, &(t->childs), trie_iterator_data(iterator)[mismatch + cur_offset]); // Binary search in child nodes
        if (found) { // Element was found, calls to add now became recursive ...
            next = trie_get_child(t, pos);
            trie_readlock(&(next->lock));
            res = trie_next_iterator_helper(next, iterator, cur_offset + trie_data_len(t) + 1); // Plus first data
            trie_unlock(&(next->lock));
            if (res == 0) // Nothing was done
                pos++; // pos++ is the first child aftr, then go on
            else // Adds the first not added character
                return res;
        }
        // Element was not found, pos contains the next
        if (pos == trie_get_child_num(t)) // If not reached the end
            return 0; // End reached nothing can be done

        next = trie_get_child(t, pos); // Gets next child
        trie_iterator_substitute_end(iterator, cur_offset + trie_data_len(t), // adds the first character after matching chars
                                     &(trie_get_first(t, pos)), 1);
        trie_readlock(&(next->lock));
        trie_get_first_iterator(next, iterator, cur_offset + trie_data_len(t) + 1);
        return 1;
    } else if (mismatch + cur_offset == trie_iterator_data_len(iterator)) { // End of the data, current data stored is longer
        assert(mismatch < trie_data_len(t));
        // Copies current data in the iterator, then gets first useful data
        trie_iterator_substitute_end(iterator, cur_offset + mismatch, trie_data(t) + mismatch, trie_data_len(t) - mismatch);
        if (!trie_empty_childs(t)) {
            next = trie_get_child(t, 0);
            trie_iterator_substitute_end(iterator, cur_offset + trie_data_len(t), // adds the first character
                                         &(trie_get_first(t, pos + found?0:1 )), 1); // if found first at step before adds 0 
            trie_readlock(&(next->lock));                                    // (i.e. use that character) else moves to the next
            trie_get_first_iterator(next, iterator, cur_offset + trie_data_len(t) + 1);
        }
        return 1;
    } else { // mismatch in the middle of the data
        assert(mismatch < trie_data_len(t));
        assert(mismatch + cur_offset < trie_iterator_data_len(iterator));

        if (trie_iterator_data(iterator)[mismatch + cur_offset] > trie_data(t)[mismatch]) { // iterator goes after
            return 0; // Nothing to do in this node
        } else { // cur->data[mismatch] > arr[mismatch], they can't be equal
            // Iterator goes before, do as the previous case
            trie_iterator_substitute_end(iterator, cur_offset + mismatch, trie_data(t) + mismatch, trie_data_len(t) - mismatch);
            if (!trie_empty_childs(t)) {
                next = trie_get_child(t, 0);
                trie_iterator_substitute_end(iterator, cur_offset + trie_data_len(t), // adds the first character
                                         &(trie_get_first(t, pos + found?0:1 )), 1); // Same as before for found
                trie_readlock(&(next->lock));
                trie_get_first_iterator(next, iterator, cur_offset + trie_data_len(t) + 1);
            }
            return 1;
        }
    }
}

int trie_iterator_next(trie_ptr_t t, trie_iterator_t * iterator) {
    int res;
    
    if (t == NULL || iterator == NULL) // Invalid pointers
        return 0;

    trie_readlock(&(t->lock)); // Readlocks next.
    if (trie_is_empty(t)) {
        trie_iterator_clear(iterator);
        trie_unlock(&(t->lock)); // Readlocks next.
        res = 0;
    } else if (trie_iterator_first_iterator(iterator)) { // First time here, gets the first
        trie_iterator_use_iterator(iterator); // This way does not execute this code next time
        trie_get_first_iterator(t, iterator, 0); // Auto unlocks
        res = 1; // Found
    } else { // Normal
        res = trie_next_iterator_helper(t, iterator, 0);
        trie_unlock(&(t->lock)); // Readlocks next.
    }

    if (res == 0) // Reached last element
        trie_iterator_clear(iterator);
    return res;
}

// ===============================
// ==== TRIE SUFFFIX ITERATOR ====
// ===============================

int trie_suffix_iterator_next(trie_ptr_t t, trie_arr_t trie_data, trie_iterator_t * iterator) {
    int mismatch; // data counter
    int retval; // Temporany (return value)
    int tmp_len; // Temporany
    int a_id, b_id; // identifiers
    struct _trie * cur, * next; // current root pointer (not reallocable)
    
    if (t == NULL || iterator == NULL) // Invalid pointers
        return 0;

    // First of all reachs the end of the data

    if (t == NULL)
        return 0; // Invalid ptr

    trie_readlock(&(t->lock)); // locks root trie read mutex
    if (trie_is_empty(t)) {
        retval = 0; // Empty trie
    } else { // Trie not empty (general case)
        cur = t;
        while (1) {
            // Looks for the first mismatching character
            mismatch = find_first_mismatch(trie_data.data, trie_data.len, trie_data(cur), trie_data_len(cur));

            // Now parse
            if ((mismatch == trie_data_len(cur)) && (mismatch == trie_data.len)) { // Reached end of data, and end of node
                // Starts from the next node
                if (trie_get_child_num(cur) == 0) { // No childs
                    assert(trie_data_end(cur)); // Always true when there are no childs
                    retval = 0; // 0 means this is the last iterator (or an error occurred)
                    trie_iterator_clear(iterator); // No more iterators
                    break; // nothing to do
                }

                if (trie_iterator_first_iterator(iterator)) { // First time here, gets the first
                    trie_iterator_use_iterator(iterator); // This way does not execute this code next time
                    if (!trie_data_end(cur)) { // If data does not ends here first iterator is inside first child
                        trie_readlock(&(trie_get_child(cur, 0)->lock)); // Readlocks next first child
                        // Copies the first byte of next node in the iterator
                        trie_iterator_substitute_end(iterator, 0, &trie_get_first(cur, 0), 1); // only one character
                        trie_get_first_iterator(trie_get_child(cur, 0), iterator, 1); // Sets 1 offset
                        // Prior function call auto unlocks mutex
                    } else {} // First iterator is the void iterator. Iterator is already void, so do nothing
                    retval = 1; // Found
                } else { // Normal
                    // Now searches in childs from wich to start
                    if (trie_iterator_data_len(iterator) == 0)
                        a_id = b_id = 0; // Starts from the first child, forces copying the first byte
                    else // Does a binary search for the first data inside childs
                        a_id = trie_search_in_childs(&b_id, &(cur->childs), trie_iterator_data(iterator)[0]);
                    
                    if (b_id == trie_get_child_num(cur)) { // Reached the end, sets the iterator to null
                        retval = 0; // This forces clearing iterator (next)
                    } else { // Normal case
                        trie_readlock(&(trie_get_child(cur, b_id)->lock)); // Readlocks next first child
                        if (!a_id) // Not found, need to copy the first character
                            trie_iterator_substitute_end(iterator, 0, &trie_get_first(cur, b_id), 1); // only one character
                        retval = trie_next_iterator_helper(trie_get_child(cur, b_id), iterator, 1); // Gets next iterator from that root
                        trie_unlock(&(trie_get_child(cur, b_id)->lock)); // Readlocks next.
                    }
                }
                
                if (retval == 0) // Reached last element
                    trie_iterator_clear(iterator);

                break;
            } else if ( (mismatch == trie_data_len(cur)) && trie_empty_childs(cur) ) { // Reached end of stored data
                assert(trie_data.len > mismatch); // there is always a next character
                assert(trie_data_end(cur)); // Because of empty childs
                retval = 0; // Not found
                break; // End
            } else if (mismatch == trie_data_len(cur)) { // Reached end of stored data, has childs
                // Let's start by binary searching the next character inside the childs
                assert(mismatch < trie_data.len); //  there is always a next character, so can access arr[mismatch]
                a_id = trie_search_in_childs(&b_id, &(cur->childs), trie_data.data[mismatch]); // Binary search in child nodes
                if (a_id) { // Element was found, calls to add now became recursive ...
                    next = trie_get_child(cur, b_id); // New data should be added here
                    trie_readlock(&(next->lock)); // Readlocks next.
                    trie_unlock(&(cur->lock)); // Unlocks current. N.B. Keep order
                    trie_data.data += (mismatch + 1); // Moves forward the array data, skipping first
                    trie_data.len -= (mismatch + 1); // Lenght of remaining data reduces
                    cur = next;
                    continue; // Continues while loop
                } else { // Element was not found, inserts a new one
                    retval = 0; break;
                }
            } else if (mismatch == trie_data.len) { // Middle of the data, data stored is longer
                // Suffix will be made with a part of the current data, and the childs data
                assert(mismatch < trie_data_len(cur));
                if (trie_get_child_num(cur) == 0) assert(trie_data_end(cur)); // Debug purpose
                tmp_len = trie_data_len(cur) - mismatch;
                
                if (!trie_iterator_first_iterator(iterator)) { // Compares the beginning of the data
                    if (trie_iterator_data_len(iterator) < tmp_len) // If iterator data is less
                        tmp_len = trie_iterator_data_len(iterator); // Must choose the shorter one
                    retval = memcmp(trie_iterator_data(iterator), trie_data(cur) + mismatch,
                                    tmp_len*sizeof(*trie_data(cur))); // Uses memcpy as comparator
                    // Restores tmp_len
                    tmp_len = trie_data_len(cur) - mismatch;
                    
                    if ((retval == 0) && (trie_iterator_data_len(iterator) < tmp_len)) { // Exact match
                        retval = -1; // Shorter data comes before, use the first algorithm
                    } else if (retval > 0) { // got over the rest of the data, skips
                        trie_iterator_clear(iterator); // This sets as last iterator (i.e. the NULL iterator)
                        retval = 0; break; // Returns a not found, as usual
                    }
                }

                // if never got here, or first useful data here is after the iterator, then must choose the
                if (trie_iterator_first_iterator(iterator) || (retval < 0)) { //      first possible iterator
                    if (trie_iterator_first_iterator(iterator)) // First time here, must use the iterator
                        trie_iterator_use_iterator(iterator); // This way does not execute this code next time
                    // Adds at the beginning the remaining part of the data (beginning of the first possible iter)
                    trie_iterator_substitute_end(iterator, 0, trie_data(cur) + mismatch, tmp_len);
                    
                    if (!trie_data_end(cur)) { // Data does not ends with this node
                        assert(trie_get_child_num(cur) >= 2); // Must have at least two childs
                        trie_readlock(&(trie_get_child(cur, 0)->lock)); // Readlocks next first child
                        trie_iterator_substitute_end(iterator, tmp_len, &trie_get_first(cur, 0), 1); // only one character
                        trie_get_first_iterator(trie_get_child(cur, 0), iterator, tmp_len + 1); // Auto unlocks
                    } else {} // Data ends with this node, this is the first iterator (the shorter one)
                    retval = 1; // In either case it is found an iterator
                } else { // Normal, retval == 0 (i.e. beginning of the data matches)
                    // Now searches in childs from wich to start
                    if (trie_iterator_data_len(iterator) == tmp_len) // No data after this node
                        a_id = b_id = 0; // Starts from the first child (NOTE: it might not exist, see after)
                    else // Does a binary search for the first data inside childs
                        a_id = trie_search_in_childs(&b_id, &(cur->childs), trie_iterator_data(iterator)[tmp_len]);
                    
                    if (b_id == trie_get_child_num(cur)) { // Reached the end, sets the iterator to null
                        retval = 0; // This forces clearing iterator (next)
                    } else { // Normal case
                        trie_readlock(&(trie_get_child(cur, b_id)->lock)); // Readlocks the child we are going to use
                        if (!a_id) // If not found needs to substitute
                            trie_iterator_substitute_end(iterator, tmp_len, &trie_get_first(cur, b_id), 1); // only one character
                        retval = trie_next_iterator_helper(trie_get_child(cur, b_id), iterator, tmp_len + 1); // Gets next iterator from that root
                        trie_unlock(&(trie_get_child(cur, b_id)->lock)); // Readlocks next.
                    }
                }
                
                if (retval == 0) // Reached last element
                    trie_iterator_clear(iterator);
                
                break;
            } else { // Both mismatches in the middle of the data, part of the input data does not matches, no iterator is possible
                assert(mismatch < trie_data_len(cur));
                assert(mismatch < trie_data.len);
                retval = 0; break;
            }
            assert(0);
            __builtin_unreachable();
        } // end while
    }

    trie_unlock(&(cur->lock));
    return retval;
}
