# Trie
Implementation of a Trie data structure (https://en.wikipedia.org/wiki/Trie), with full support for multithread (compile with -lphtread). Two or more thread may add or search data at the same time without any conflict.

Future versions will have a switch for pthread.

## Usage:

    trie_t my_trie; // Creates a trie
    trie_init(&my_trie); // Initializes the trie
    
    //        trie     string        lenght
    trie_add(&trie, "Hello World!", strlen("Hello World")); // Adds some data
    found = trie_find(&trie, "Hello World!", strlen("Hello World")); // Searches data added, returns int
    if (found)
        // Data was found!
    
    trie_iterator_t iter; // Iterator for the trie
    tire_init_iterator(&iter); // Inits the iterator
    
    while (trie_next_iterator(&trie, &iter)) {  // Gets the next element
        printf("%.*s", trie_iterator_len(&iter), // lenght of current item
                       trie_iterator_data(&data)); // Underlayng data
    } // trie next_iterator_returns 0 when no other data is aviable
    
    tire_destroy_iterator(&iter); // Destroys iterator
    trie_clear(&trie); // Destroys all the data
    
See the test main file provided for an example of implementation

And remember to include "trie.h".

## TODO
    IMPORTANT: a function to remove data
    IMPORTANT: a backward iterator
    IMPORTANT: C++ binding

## Feedback

Bugs and feedbacks to serrainoalessio (at) gmail (dot) com
