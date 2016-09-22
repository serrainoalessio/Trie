#ifndef TRIE_H
#define TRIE_H

typedef unsigned char DATA_t; // You may change this at your option

#include <pthread.h> // mutex
#include <stdint.h> // uint8_t
#define USE_NOT_UPGRADABLE_MUTEX

struct _trie; // Struct trie prototype

struct _rwlock {
    pthread_rwlock_t rwlock; // mutex for this object
#ifndef USE_NOT_UPGRADABLE_MUTEX
    pthread_rwlock_t write_peeding; // util R/W mutex for who asks to write
    pthread_mutex_t upgrade; // rwlock mutex
#endif
};

struct _childs {
    int child_num; // number of child nodes
    int child_alloc; // Nmber of elements dynamically allocated
    struct _trie ** childs; // Dynamic array of pointer to childs
    DATA_t * firsts; // Array of first objects
};

struct _data {
    int len; // lenght of data, excluding first. first is stored elsewhere
    const DATA_t * data; // array of data

    // data flags
    uint8_t end: 1; // true if reached end of data
    uint8_t dealloc: 1; // true only if data is an allocated ptr
};

struct _trie {
    struct _rwlock lock; // compact way of keeping lock stuff
    struct _data data; // compact way of keeping data
    struct _childs childs; // again a compact way to write
};
typedef struct _trie trie_t;
typedef struct _trie * trie_ptr_t;

// data representing an array for the trie
typedef struct {
    DATA_t * data;
    int len;
} trie_arr_t;
#define trie_arr_len(t) (t)->len
#define trie_arr_data(t) (t)->data

// Init/destroy utilities
void trie_init(trie_ptr_t t);
void trie_clear(trie_ptr_t t); // deletes every element from the trie

void trie_arr_init(trie_arr_t * arr); // Inits a trie array
void trie_arr_clear(trie_arr_t * arr); // Clears a trie array

// Trie utils
void trie_add(trie_ptr_t t, const DATA_t * arr, int len); // adds an elemente to the trie
int trie_find(trie_ptr_t t, const DATA_t * arr, int len); // searches for an element in the trie
                                                          // returns 1 if it exist, otherwise 0
#define TRIE_SUFFIX_FOUND     0 // Normal return value
#define TRIE_NO_SUFFIX_FOUND  1 // Base for the suffix was not found
#define TRIE_MULTIPLE_SUFFIX -1 // Found more than one suffix
int trie_get_suffix(trie_ptr_t t, const DATA_t * arr, int len, trie_arr_t * suffix);

typedef trie_arr_t trie_iterator_t;
#define trie_iterator_len(t) (t)->len
#define trie_iterator_data(t) (t)->data

void trie_iterator_init(trie_iterator_t * iterator);
void trie_iterator_clear(trie_iterator_t * iterator);
int trie_iterator_next(trie_ptr_t t, trie_iterator_t * iterator); // 1 if success, 0 if reached the end

#endif // TRIE_H defined
