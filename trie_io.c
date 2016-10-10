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

#include <stdio.h> // fread, fwrite
#include <limits.h> // INT_MIN
#include "trie.h"

// This source uses functions from:
//    trie_utils.c, trie_childs.c, trie_mutex.c

/*
   Data format for each node:
   === NODE: ===
              4 bytes (*)            n bytes (depends by DATA_t, data_len)          4 bytes       n_1 bytes        n_2 bytes     ...     n_l bytes
       [ data lenght & data_end] [ data stored, without any coding/compression ] [ child num ] [ CHILD NODE 1 ] [ CHILD NODE 2 ] ... [ LAST CHILD NODE ]
   Root node is stored in the same way

   This definition is recursive, each child node uses the same format format as the parent.
   Last step of the definition is the one with no more child nodes, the size of that node is (4 + 4 + data_len*data_size) bytes

   (*) format: NOTE: It won't be used trie_data_len(node) data, instead will be used trie_data_len(node) + 1. This is actually the lenght of the data.
               If data end flag is true for this node then the lenght will be negative, otherwise it will be positive.
               It might exists a zero-lenght node, root node when empy string is stored. If node is 0-lengt and end-flag is true
               it is stored in memory -2^31 (INT_MIN), otherwise if node is 0-lenght and end-flag is false it is stored 0

   Before root node there is a Magic number, undef the correspunding macro to disable
*/

#define MAGIC_NUMBER

//   =================
//   ===   WRITE   ===
//   =================

static inline // inlines when possible
void __trie_fwrite_node(FILE * fp, struct _trie * parent, int n_child) {
    struct _trie * t = trie_get_child(parent, n_child);
    int i, tmp_len;

    // Locks mutex for reading
    trie_readlock(&(t->lock));

    // === Stores data ===
    tmp_len = trie_data_len(t) + 1; // Do not read this before readlock
    assert(tmp_len > 0); // implies tmp_len != 0
    if (trie_data_end(t)) // tmp_len is always != from zero
        tmp_len = -tmp_len; // uses the negative size
    fwrite(&tmp_len, sizeof(tmp_len), 1, fp); // Writes lenght
    fwrite(&(trie_get_first(parent, n_child)), sizeof(trie_get_first(parent, n_child)), 1, fp); // Writes first chunk of data
    fwrite(trie_data(t), sizeof*(trie_data(t)), trie_data_len(t), fp); // Writes the rest of the data, lenght is always data_len(...)

    // === Now stores childs ===
    fwrite(&trie_get_child_num(t), sizeof(trie_get_child_num(t)), 1, fp); // First stores child num
    for (i = 0; i < trie_get_child_num(t); i++) { // Now for each child
#ifndef NDEBUG // if Debugging
        if (i < trie_get_child_num(t) - 1) // except for the last
            assert(trie_get_first(t, i) < trie_get_first(t, i + 1)); // Checks 'firsts' data order
#endif // End debug section
        __trie_fwrite_node(fp, t, i); // t is new parent, i means i-th child
    }

    // Unlocks mutex
    trie_unlock(&(t->lock));
}

void trie_fwrite(FILE * fp, trie_ptr_t t) {
    int i, tmp_len;

    if (t == NULL) // Not actually a trie
        return;

    trie_readlock(&(t->lock));

    // === Stores data ===
    tmp_len = trie_data_len(t); // Do not read this before readlock
    // + 1 is missing because of first data is not stored elsewhere!
    assert(tmp_len >= 0); // Data lenght may not be negative
    if (trie_data_end(t) && (tmp_len != 0)) // Normal case
        tmp_len = -tmp_len; // uses the negative size
    else if (trie_data_end(t) && (tmp_len == 0))
        tmp_len = INT_MIN; // -2^31
    fwrite(&tmp_len, sizeof(tmp_len), 1, fp); // Writes lenght
    fwrite(trie_data(t), sizeof*(trie_data(t)), trie_data_len(t), fp); // Writes the whole data

    // === Now stores childs ===   (exactly the same as above)
    fwrite(&trie_get_child_num(t), sizeof(trie_get_child_num(t)), 1, fp); // First stores child num
    for (i = 0; i < trie_get_child_num(t); i++) { // Now for each child
#ifndef NDEBUG // if Debugging
        if (i < trie_get_child_num(t) - 1) // except for the last
            assert(trie_get_first(t, i) < trie_get_first(t, i + 1)); // Checks 'firsts' data order
#endif // End debug section
        __trie_fwrite_node(fp, t, i); // t is new parent, i means i-th child
    }

    trie_unlock(&(t->lock));
}

//   ================
//   ===   READ   ===
//   ================

void __trie_fread_node(FILE * fp, struct _trie * parent, int n_child) {
    struct _trie * t;
    int i, tmp_len;

    // === Allocates a new node ===
    trie_init_new_child(parent, n_child);    
    t = trie_get_child(parent, n_child);
    
    // === Reads data ===
    assert(fread(&tmp_len, sizeof(tmp_len), 1, fp) == 1); // Reads data lenght
    assert(tmp_len != INT_MIN && tmp_len != 0); // Invalid in this context
    if (tmp_len < 0) { // Data ends here 
        trie_set_data_end(t);
        trie_data_len(t) = -tmp_len - 1; // Uses positive lenght
    } else { // Data does not ends here
        trie_clear_data_end(t);
        trie_data_len(t) = tmp_len - 1;
    }
    
    assert(trie_data_len(t) >= 0);
    t->data.dealloc = 1; // This chunk needs to be deallocated
    trie_data(t) = malloc(trie_data_len(t)*sizeof*trie_data(t)); // Allocs enough data
    fread(&(trie_get_first(parent, n_child)), sizeof(trie_get_first(parent, n_child)), 1, fp); // Reads first chunk of data
    fread((DATA_t*)trie_data(t), sizeof*(trie_data(t)), trie_data_len(t), fp); // Reads the rest of the data, lenght is always data_len(...)

    // === Reads childs ===
    fread(&trie_get_child_num(t), sizeof(trie_get_child_num(t)), 1, fp); // First stores child num
    assert(trie_get_child_num(t) >= 0);
    trie_add_first_n_childs(&(t->childs), trie_get_child_num(t));
    for (i = 0; i < trie_get_child_num(t); i++) // Now for each child
        __trie_fread_node(fp, t, i); // t is new parent, i means i-th child
}

void trie_fread(FILE * fp, trie_ptr_t t) {
    int i, tmp_len;

    if (t == NULL)
        return;

    // ==== First erases the trie ====
    trie_clear(t);
    trie_init(t);

    // === Reads data ===
    fread(&tmp_len, sizeof(tmp_len), 1, fp); // Reads data lenght
    if (tmp_len < 0) { // Data ends here 
        trie_set_data_end(t);
        if (tmp_len != INT_MIN)
            trie_data_len(t) = -tmp_len; // Uses positive lenght
        else
            trie_data_len(t) = 0;
    } else { // tmp_len >= 0, Data does not ends here
        trie_clear_data_end(t);
        trie_data_len(t) = tmp_len;
    }
    
    assert(trie_data_len(t) >= 0);
    t->data.dealloc = 1; // This chunk needs to be deallocated
    trie_data(t) = malloc(trie_data_len(t)*sizeof*trie_data(t)); // Allocs enough data
    fread((DATA_t*)trie_data(t), sizeof*(trie_data(t)), trie_data_len(t), fp); // Reads the rest of the data, lenght is always data_len(...)

    // === Reads childs === (exactly as above)
    fread(&trie_get_child_num(t), sizeof(trie_get_child_num(t)), 1, fp); // First stores child num
    assert(trie_get_child_num(t) >= 0);
    if (trie_get_child_num(t) != 0) { // Normal case
        trie_add_first_n_childs(&(t->childs), trie_get_child_num(t));
        for (i = 0; i < trie_get_child_num(t); i++) // Now for each child
            __trie_fread_node(fp, t, i); // t is new parent, i means i-th child
    } else { // Empty childs, it might means empty trie or not
        if (trie_data_len(t) == 0 && ! trie_data_end(t)) { // Empty trie
            trie_get_childs(t) = NULL; // No children for the root node
            free((DATA_t*)trie_data(t));
            t->data.dealloc = 0; // Already freed
        } else {
            trie_init_childs(&(t->childs)); // Inits root node (it should be already initialized)
            trie_alloc_childs(&(t->childs)); // Allocs two children for the root node
        }
    }
}

// Merges a read trie
void trie_fread_merge(FILE * fp, trie_ptr_t t) {
    trie_t trie; // Temporany
    trie_iterator_t iter;
    
    trie_init(&trie);
    trie_iterator_init(&iter);

    // The only safe way to do so is to read the whole input, then add everything
    trie_fread(fp, &trie);

    while (trie_iterator_next(&trie, &iter)) {
        trie_add(t, trie_iterator_data(&iter), trie_iterator_data_len(&iter));

        // Now searches the data inside the trie (for debug)
        assert(trie_find(t, trie_iterator_data(&iter), trie_iterator_data_len(&iter)));
    }

    trie_iterator_clear(&iter);
    trie_clear(&trie);    
}

