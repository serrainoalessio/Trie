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

// Init/destroy utilities
void trie_init(trie_ptr_t t);
void trie_clear(trie_ptr_t t); // deletes every element from the trie

// Tee utils
void trie_add(trie_ptr_t t, const DATA_t * arr, int len); // adds an elemente to the trie
int trie_find(trie_ptr_t t, const DATA_t * arr, int len); // searches for an element in the trie
                                                          // returns 1 if it exist, otherwise 0
typedef struct {
    DATA_t * data;
    int len;
} trie_iterator_t;
#define trie_iterator_len(t) (t)->len
#define trie_iterator_data(t) (t)->data

void trie_init_iterator(trie_iterator_t * iterator);
void trie_destroy_iterator(trie_iterator_t * iterator);
int trie_next_iterator(trie_ptr_t t, trie_iterator_t * iterator); // 1 if success, 0 if reached the end

#endif // TRIE_H defined
