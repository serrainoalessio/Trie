#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>

#include "trie.h"

#define MAX_LEN 12 // Maximum lenght of each string
#define THREAD_NUM 32 // Threads adding
#define REPS 500 // Number of strings each thread adds
#define SUFFIX_REPS 200 // Number of suffix test strings

pthread_mutex_t lock;
pthread_t tid[THREAD_NUM]; // Starts eight threads

static const char vowels[] = {'a', 'e', 'i', 'o', 'u'};
static const char consonants[] = {'b', 'c', 'd', 'f', 'g', 'h', 'j',
                                 'k', 'l', 'm', 'n', 'p', 'q', 'r',
                                 's', 't', 'v', 'w', 'x', 'y', 'z'};
static inline int is_vowel(char letter) {
    int i;
    for (i = 0; i < (int)(sizeof(vowels)/sizeof(*vowels)); i++)
        if (vowels[i] == letter)
            return 1;
    return 0;
}

static inline int my_rand(void) {
    int retval;
    pthread_mutex_lock(&lock);
    retval = rand();
    pthread_mutex_unlock(&lock);
    return retval;
}

static inline int extract_rand_len(void) {
    int retval;
    retval = my_rand() % MAX_LEN + my_rand() % MAX_LEN;
    if (retval > MAX_LEN)
        retval = MAX_LEN*2 - 1 - retval; // max len = MAX_LEN
    return retval;
}

#define EXTRACT_VOWEL    -1
#define EXTRACT_CONSONANT 1
#define EXTRACT_LETTER    0
static inline char extract_rand_letter(int what) {
    switch (what) {
        case EXTRACT_LETTER:
            return 'a' + my_rand() % 26;
        case EXTRACT_VOWEL:
            return vowels[my_rand() % sizeof(vowels)/sizeof(*vowels)];
        case EXTRACT_CONSONANT:
            return consonants[my_rand() % sizeof(consonants)/sizeof(*consonants)];
        default:
            return '\0';
    }
}

// Extracts a string of exactly 'len' letters (without null terminator)
static inline void get_rand_string(DATA_t * string, int len) {
    int vowel_last, type_to_extract;
    float switch_probab; // switching wovel-consonant
    int j; // counter

    if (len != 0) {
        string[0] = extract_rand_letter(EXTRACT_LETTER); // from entire alphabet
        vowel_last = is_vowel(string[0]);

        switch_probab = 0.50;
        for (j = 1; j < len; j++) {
            // Chooses the type of next character
            if (((float)my_rand() / RAND_MAX) < switch_probab) { // Change
               if (vowel_last) { // Last was a wovel, extracts a consonant
                   type_to_extract = EXTRACT_CONSONANT;
                   vowel_last = 0;
               } else {
                   type_to_extract = EXTRACT_VOWEL;
                   vowel_last = 1;
               }
               switch_probab = 0.50; // Resets change probability
            } else { // not change
                if (vowel_last) { // Last was a wovel, extracts a consonant
                    type_to_extract = EXTRACT_VOWEL;
                    vowel_last = 1;
                } else {
                    type_to_extract = EXTRACT_CONSONANT;
                    vowel_last = 0;
                }
                switch_probab = switch_probab*2 - pow(switch_probab, 2); // increases probability of a change
            }

            string[j] = extract_rand_letter(type_to_extract); // extracts random letter
        }
    }
}

static inline int parity(const DATA_t * string, int len) {
    int i, parity;

    parity = 0;
    for (i = 0; i < len; i++)
        parity += string[i];

    return parity % 2;
}

#define PARITY_EVEN 0
#define PARITY_ODD  1
static inline void fix_parity(DATA_t * string, int len, int parity_type) {
    int rand_bit, rand_dir;
    int parity_search;

    if (len == 0) // Lenght 0 string is even
        return;

    if (parity_type == PARITY_EVEN)
        parity_search = 1;
    else if (parity_type == PARITY_ODD)
        parity_search = 0;

    if (parity(string, len) == parity_search) {

        // Modify a random bit to invert parity
        rand_bit = my_rand() % len;
        rand_dir = my_rand() % 2;

        if (rand_dir)
            string[rand_bit]++;
        else
            string[rand_bit]--;

        if (string[rand_bit] < 'a')
            string[rand_bit] = 'z';
        else if (string[rand_bit] > 'z')
            string[rand_bit] = 'a';
    }
}

DATA_t * data_added; // Big array of each data
int * data_added_len;
pthread_mutex_t * data_added_len_mutex;

static inline void store_string(DATA_t * data, int datalen, int thread_num) {
    memcpy(data_added + thread_num*REPS*MAX_LEN + data_added_len[thread_num]*MAX_LEN,
            data, datalen*sizeof(*data));
    if (datalen < MAX_LEN)
        data_added[thread_num*REPS*MAX_LEN + data_added_len[thread_num]*MAX_LEN + datalen] = '\0';
    pthread_mutex_lock(&data_added_len_mutex[thread_num]);
    data_added_len[thread_num]++;
    pthread_mutex_unlock(&data_added_len_mutex[thread_num]);
}

static inline void get_rand_added_string(DATA_t * data, int * datalen) {
    int i, j, k;

    while (1) {
        i = my_rand() % THREAD_NUM; // Random thread
        pthread_mutex_lock(&data_added_len_mutex[i]);
        if (data_added_len[i] == 0) // No data added, must choose a different thread
            pthread_mutex_unlock(&data_added_len_mutex[i]);
        else
            break;
    }
    j = my_rand();
    j = j % data_added_len[i]; // Random string

    for (k = 0; k < MAX_LEN; k++)
        if (data_added[i*REPS*MAX_LEN + j*MAX_LEN + k] == '\0')
            break;
    // k contains lenght
    *datalen = k;
    memcpy(data, data_added + i*REPS*MAX_LEN + j*MAX_LEN, k*sizeof(*data));
    pthread_mutex_unlock(&data_added_len_mutex[i]);
}

static void print_trie(trie_ptr_t ptr);
static inline void dump(trie_ptr_t ptr, int my_tid, const char * msg) {
    flockfile(stdout);
    printf("Thread #%d says: %s\n", my_tid + 2, msg);
    print_trie(ptr);
    printf("\n");
    funlockfile(stdout);
    fflush(stdout);
}

static inline void print_trie(trie_ptr_t ptr) {
    int res;
    trie_iterator_t iter;
    trie_iterator_init(&iter);

    while (trie_iterator_next(ptr, &iter)) {
        printf("%.*s", trie_iterator_data_len(&iter), (char*)trie_iterator_data(&iter));
        printf(" (%d)\n", trie_iterator_data_len(&iter));

        // Now searches the data inside the trie (for debug)
        res = trie_find(ptr, trie_iterator_data(&iter),
                             trie_iterator_data_len(&iter));
        if (res == 0) {
            char msg[32 + MAX_LEN];
            sprintf(msg, "Cannot find this added string: %.*s\n",
                trie_iterator_data_len(&iter), (char*)trie_iterator_data(&iter));
            dump(ptr, -1, msg); // This function should be called by main thread
            assert(0);
        }
    }

    trie_iterator_clear(&iter);
}

static inline void print_trie_starting_with(trie_ptr_t ptr) {
    int nfound, res, datalen = (int)ceil(log(MAX_LEN));
    trie_iterator_t iter, iterck; // Iterator and iterator check
    trie_arr_t arr;
    DATA_t str_to_search[MAX_LEN];
    // Now extracts a random smaller string and lists all the data starting with that
    
    if ( (datalen != 0) &&
         (my_rand() % 10 == 0) )
        datalen--; // Randomly uses a shorter string
    
    get_rand_string(str_to_search, datalen);
    
    arr.data = str_to_search;
    arr.len = datalen;
    arr.alloc = datalen; // Formal, won't be used
    trie_iterator_init(&iter);
    
    trie_iterator_init(&iterck);
    trie_iterator_data(&iterck) = malloc(datalen*sizeof*str_to_search); // Must be allocated with malloc
    memcpy(trie_iterator_data(&iterck), str_to_search, datalen*sizeof*str_to_search); // Dumps the buffer
    trie_iterator_data_len(&iterck) = iterck.alloc = datalen; // Lenght and allocated data have the same lenght

    nfound = 0; // Number of elements found
    while (trie_suffix_iterator_next(ptr, arr, &iter)) {
        if (nfound++ == 0) // Executes only the first time
            printf("   === All data starting with %.*s\n", datalen, str_to_search); 
        printf("%.*s%.*s", datalen, str_to_search,
                           trie_iterator_data_len(&iter), (char*)trie_iterator_data(&iter));
        printf(" (%d)\n", datalen + trie_iterator_data_len(&iter));

        // Now searches the data inside the trie (for debug)
        // NOTE: the iterator contains only the suffix, so copies the whole data
        memcpy(str_to_search + datalen, trie_iterator_data(&iter),
               trie_iterator_data_len(&iter)*sizeof*str_to_search);
        res = trie_find(ptr, str_to_search, datalen + trie_iterator_data_len(&iter));
        if (res == 0) {
            char msg[32 + MAX_LEN];
            sprintf(msg, "Cannot find this added string: %.*s%.*s\n", datalen, str_to_search,
                trie_iterator_data_len(&iter), (char*)trie_iterator_data(&iter));
            dump(ptr, -1, msg); // This function should be called by main thread
            assert(0);
        }
        
        if ((nfound != 1) || (trie_iterator_data_len(&iter) != 0))
            trie_iterator_next(ptr, &iterck); // Gets next iterator

        if (memcmp(trie_iterator_data(&iterck) + datalen, trie_iterator_data(&iter),
                   trie_iterator_data_len(&iter)*sizeof*trie_iterator_data(&iter)) != 0) {
            printf("   === ERROR Skipping data ===\n");
            printf(" Looking for %.*s (%d)\n", trie_iterator_data_len(&iterck), trie_iterator_data(&iterck),
                                               trie_iterator_data_len(&iterck) );
            assert(0);
        }
    }

    trie_iterator_clear(&iter);
    trie_iterator_clear(&iterck);
    if (nfound != 0)
        printf("found: %d\n", nfound);
//    if (nfound >= 2) // Do not print nothing if nothing was found
//        printf("   === End of all strings starting with %.*s\n", datalen, str_to_search);
}

int my_get_tid(void) {
    pthread_t me = pthread_self();
    int i;

    for (i = 0; i < THREAD_NUM; i++)
        if (pthread_equal(tid[i], me))
            return i;
    return -1; // main thread (or bug)
}

void * add_data(void * ptr_void) {
    trie_ptr_t ptr = ptr_void;
    int my_tid = my_get_tid();

    int rand_len;
    DATA_t rand_data[MAX_LEN];
    int i, res;

    flockfile(stdout);
    printf("Entering in thread %d\n", my_tid + 2);
    fflush(stdout);
    funlockfile(stdout);

    for (i = 0; i < REPS; i++) {
        rand_len = extract_rand_len();
        get_rand_string(rand_data, rand_len);

        // Ensure to insert only even strings (this way searching odd returns 0)
        fix_parity(rand_data, rand_len, PARITY_EVEN);

        flockfile(stdout); // Debug functionality
        printf("Thread #%d says: adding string: ", my_tid + 2);
        printf("%.*s\n", rand_len, (char*)rand_data);
        fflush(stdout);
        funlockfile(stdout);

        trie_add(ptr, rand_data, rand_len); // adds random data to the structure
        store_string(rand_data, rand_len, my_tid);

        res = trie_find(ptr, rand_data, rand_len);
        if (res == 0) {
            char msg[32 + MAX_LEN];
            sprintf(msg, "Cannot find just added string: %.*s\n", rand_len, (char*)rand_data);
            dump(ptr, my_tid, msg);
            assert(0);
        }

        get_rand_added_string(rand_data, &rand_len);
        res = trie_find(ptr, rand_data, rand_len);
        if (res == 0) {
            char msg[32 + MAX_LEN];
            sprintf(msg, "Cannot find this added string: %.*s\n", rand_len, (char*)rand_data);
            dump(ptr, my_tid, msg);
            assert(0);
        }

        // Now extract inexistent data, to slow down a little bit the proces
        rand_len = extract_rand_len();
        if (rand_len == 0)
            rand_len++;
        get_rand_string(rand_data, rand_len);
        fix_parity(rand_data, rand_len, PARITY_ODD);

        res = trie_find(ptr, rand_data, rand_len);
        assert(!res);
    }

    pthread_exit(0);
    __builtin_unreachable();
}

void * check_added_data(void * ptr_void) {
    trie_ptr_t ptr = ptr_void;
    int my_tid = my_get_tid();

    int string_lenght = 0;
    DATA_t string[MAX_LEN];
    int i, res;

    for (i = 0; i < REPS; i++) { // For each added string
        // Retrives the i-th string
        for (string_lenght = 0; string_lenght < MAX_LEN; string_lenght++)
            if (data_added[my_tid*REPS*MAX_LEN + i*MAX_LEN + string_lenght] == '\0')
                break;
        memcpy(string, data_added + my_tid*REPS*MAX_LEN + i*MAX_LEN, string_lenght*sizeof(*string));

        // Now searches the string, and asserts the string was found
        res = trie_find(ptr, string, string_lenght);
        if (res == 0) {
            char msg[32 + MAX_LEN];
            sprintf(msg, "Cannot find this added string: %.*s\n",
                            string_lenght, (char*)string);
            dump(ptr, my_tid, msg);
            assert(0);
        }
    }

    pthread_exit(0);
    __builtin_unreachable();
}

int main(int argc, char * argv[]) {
    int i, res;
    trie_t my_trie;

    (void)argc;
    (void)argv;

    srand(time(NULL));
    data_added = malloc(THREAD_NUM*REPS*MAX_LEN*sizeof(*data_added));
    data_added_len = malloc(THREAD_NUM*sizeof(*data_added_len));
    data_added_len_mutex = malloc(THREAD_NUM*sizeof(*data_added_len_mutex));

    pthread_mutex_init(&lock, NULL);
    trie_init(&my_trie);

    printf("   === begin ===\n");

    for (i = 0; i < THREAD_NUM; i++) {
        pthread_mutex_init(&data_added_len_mutex[i], NULL);
        pthread_mutex_lock(&data_added_len_mutex[i]);
        data_added_len[i] = 0;
        pthread_mutex_unlock(&data_added_len_mutex[i]);
    }

    asm volatile ("":::"memory"); // Memory barrier
                    // (avoid compiler reorcer instructions crossing the barrier)

    for (i = 0; i < THREAD_NUM; i++) {
        res = pthread_create(tid + i, NULL, add_data, &my_trie);
        assert(res == 0); // Returns 0 on success
    }
    for (i = 0; i < THREAD_NUM; i++)
        pthread_join(tid[i], NULL);
    // Join all threads
    printf("   === All data added ===\n");
    fflush(stdout);

    // File IO testing. Writes the trie to a file, then reads it again
    FILE *out = fopen("trie_out.hex", "w");
    assert(out);
    trie_fwrite(out, &my_trie);
    fclose(out);
    
    trie_clear(&my_trie); // Clears all the data
    out = fopen("trie_out.hex", "r");
    trie_fread(out, &my_trie); // Reads again the same trie
    fclose(out);

    // Now re-creates thread to check data added
    for (i = 0; i < THREAD_NUM; i++) {
        res = pthread_create(tid + i, NULL, check_added_data, &my_trie);
        assert(res == 0); // Returns 0 on success
    }
    for (i = 0; i < THREAD_NUM; i++)
        pthread_join(tid[i], NULL);

    asm volatile ("":::"memory"); // Memory barrier
    for (i = 0; i < THREAD_NUM; i++)
        pthread_mutex_destroy(&data_added_len_mutex[i]);

    printf("   === All threads joined ===\n");
    printf("   === Final data structure: ===\n");
    print_trie(&my_trie);
    printf("\n");
    fflush(stdout);
     
    for (i = 0; i < SUFFIX_REPS; i++) // Repeats many times!
        print_trie_starting_with(&my_trie); // Works out a random prefix, etc...

    printf("   === end ===\n");
    
    printf("   === Final data structure: ===\n");
    print_trie(&my_trie);
    printf("\n");
    fflush(stdout);
    
    trie_clear(&my_trie); // Frees all the memory
    pthread_mutex_destroy(&lock);

    free(data_added);
    free(data_added_len);
    free(data_added_len_mutex);

    return 0;
}
