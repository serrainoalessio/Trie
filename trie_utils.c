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
#include <assert.h> // assert

static inline
int find_first_mismatch(const DATA_t * arr1, int len1,
                        const DATA_t * arr2, int len2 ) {
    int i, min_compar;
    if (len1 <= len2)
        min_compar = len1;
    else
        min_compar = len2;

    for (i = 0; i < min_compar; i++)
        if (arr1[i] != arr2[i])
            break;
    // i may reach either an internal mismatch, or min_compar
    return i;
}

static inline
void trie_attach_new_data(struct _trie * t, const DATA_t * arr, int len) {
    DATA_t * alloc_arr;
    alloc_arr = malloc(len*sizeof(*(t->data.data))); // data after allocation is static, so alloc exactly the needed
    assert(alloc_arr);
    memcpy(alloc_arr, arr, len*sizeof(*(t->data.data)));
    t->data.data = alloc_arr;
    t->data.len = len;
    t->data.end = 1; // Data ends here
    t->data.dealloc = 1; // Data is a new alloc, so must free it
}

static inline
void trie_attach_existent_data(struct _trie * t, const DATA_t * arr, int len) {
    t->data.len = len;
    t->data.data = (DATA_t *)arr; // Simply attaches the pointer
    t->data.dealloc = 0; // Data is not allocated
}

static inline
void trie_attach_first_data(struct _trie * t, int pos, DATA_t new_data) {
    assert(pos < t->childs.child_num);
    t->childs.firsts[pos] = new_data;
}

#define trie_set_data_end(t)   t->data.end = 1
#define trie_clear_data_end(t) t->data.end = 0
#define trie_data_end(t)       t->data.end
#define trie_data(t)           t->data.data
#define trie_data_len(t)       t->data.len
#define trie_get_childs(t)     t->childs.childs
#define trie_get_firsts(t)     t->childs.firsts
#define trie_get_child_num(t)  t->childs.child_num
#define trie_empty_childs(t)   (trie_get_child_num(t) == 0)
#define trie_get_first(t, pos) trie_get_firsts(t)[pos]
#define trie_get_child(t, pos) trie_get_childs(t)[pos]
#define trie_is_root(t, node)  (t == node)

static inline
void trie_init_new_child(struct _trie * t, int pos) { // Inits a new empty child, without data
    trie_get_child(t, pos) = malloc(sizeof**(trie_get_childs(t))); // Mallocs space for the child
    assert(trie_get_child(t, pos));
    trie_init_childs(&(trie_get_child(t, pos)->childs)); // Inits childs of the child
    trie_init_mutex(&(trie_get_child(t, pos)->lock)); // Inits mutex

    // Inits data, it might be not necessary if attaches data.
    trie_get_child(t, pos)->data.len = 0;
    trie_get_child(t, pos)->data.data = NULL;
    trie_get_child(t, pos)->data.end = 0;
    trie_get_child(t, pos)->data.dealloc = 0;
}

#define trie_correct_child_num(t) (trie_get_child_num(t) <= t->childs.child_alloc)
#define trie_insert_init_child(t, pos)                  \
                trie_insert_child(&(t->childs), pos);   \
                trie_init_new_child(t, pos)
#define trie_is_empty(t) (trie_get_childs(t) == NULL)

static inline
void trie_destroy_data(struct _trie * const t) {
    if (t->data.dealloc)
        free((DATA_t*)t->data.data);
}

void trie_init_iterator(trie_iterator_t * iterator) {
    iterator->data = NULL;
    iterator->len = 0;
}

void trie_destroy_iterator(trie_iterator_t * iterator) {
    free(iterator->data);
    iterator->data = NULL;
    iterator->len = 0;
}
// Reallocs to new data len, and increases data len, then copies current node data
#define iterator_substitute_end(iterator, offset, new_data, new_data_len)                           \
    iterator->data = realloc(iterator->data, ((offset) + (new_data_len))*sizeof*(iterator->data));  \
    iterator->len = (offset) + (new_data_len);                                                      \
    memcpy(iterator->data + (offset), new_data, (new_data_len)*sizeof*(iterator->data))
