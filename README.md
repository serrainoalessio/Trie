# Trie
Implementation of a Trie data structure (https://en.wikipedia.org/wiki/Trie), with full support for multithread (compile with -lphtread). Two or more thread may add or search data at the same time without any conflict.

Future versions will have a switch for pthread.

## Do I need a trie?
Trie is an efficient way to store and manage arrays of object. Tries stores many array of object, not a single one, so, for example, a dictionary is an array of array of characters.
Tries DO NOT SAVE data with the order provided by the user. Insted they keep all the data with alphabetical order, so objects must be sortable. The order in wich the user adds or removes the data is absolutly ininfluent, so you won't provide a "position" for the new object.

### You should definetiely use a Trie if you answer yes to ALL the following questions
- You need a fast data structure wich provides addition, deleting, and finding elements.
- Have to store many arrays of objects (not a single one array)
- You can always sort every SINGLE array of object
- Overall order is not important, or you need alphabetical order.
  - i.e. for each object it is only important if it is in the trie or not, but not if it was added later or before another given object
  - SINGLE array must be sortable, but it is not important the whole structure with all arrays stores them in order

A classical example is if you need to store a dictionary. You have to store many words (arrays of characters), and for each word you can always sort the letters in the word (repetitions are not important). Overall order in the dictionary is not important, in the sense the only important order is one, the alphabetical order. You only need to know if a word exists or not in the dictionary, and not where the word is stored in the memory. So a trie would be a good choice.

## Usage:

    trie_t my_trie; // Creates a trie
    trie_init(&my_trie); // Initializes the trie
    
    //        trie     string        lenght
    trie_add(&trie, "Hello World!", strlen("Hello World")); // Adds some data
    found = trie_find(&trie, "Hello World!", strlen("Hello World")); // Searches data added, returns int
    if (found) {
        // Data was found!
    }
    trie_remove(&trie, "Hello World", strlen("Hello World")); // Removes data
    
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
    IMPORTANT: C++ binding, with templates

## Feedback

Bugs and feedbacks to serrainoalessio (at) gmail (dot) com
