# Trie
Trie data structure, full support for multithread (compile with -lphtread).
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

## Feedback

Bugs and feedbacks to serrainoalessio (at) gmail (dot) com
