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

#include <pthread.h>
#include <errno.h>
#include <assert.h>

// Errors may be EINVAL, EDEADLK, EAGAIN
static inline
void trie_readlock(struct _rwlock * rw) {
    int res;
#ifndef USE_NOT_UPGRADABLE_MUTEX
    res = pthread_rwlock_rdlock(&(rw->write_peeding));
    assert(res == 0);
#endif
    res = pthread_rwlock_rdlock(&(rw->rwlock));
    if (res != 0)
        assert(res == 0);
    (void)res; // Uses res, suppress warning (optimization should remove res)
}

static inline
void trie_writelock(struct _rwlock * rw) {
    int res;
#ifndef USE_NOT_UPGRADABLE_MUTEX
    res = pthread_rwlock_wrlock(&(rw->write_peeding)); // waits each read lock is released
    assert(res == 0);
#endif
    res = pthread_rwlock_wrlock(&(rw->rwlock));
    assert(res == 0);
    (void)res; // Uses res, suppress warning (optimization should remove res)
}

static inline
void trie_readlock_upgrd(struct _rwlock * rw) {
#ifndef USE_NOT_UPGRADABLE_MUTEX
    trie_readlock(rw);
#else // Upgradable readlock is a Writelock
    trie_writelock(rw);
#endif
}

static inline // Returns 1 if success, or zero if another thread gained the lock
int trie_upgrade_lock(struct _rwlock * rw) {
    int retval;
    int res;
#ifndef USE_NOT_UPGRADABLE_MUTEX
    int lock_result; // Needed to know if mutex has been locked or not
    lock_result = pthread_mutex_trylock(&(rw->upgrade)); // Tries to lock the mutex
    assert(lock_result == 0 || lock_result == EBUSY);
    res = pthread_rwlock_unlock(&(rw->rwlock)); // Releases the lock
    assert(res == 0);
    if (lock_result == 0) { // Mutex locked successfully
        res = pthread_rwlock_wrlock(&(rw->rwlock)); // waits everyone finishes to read
        retval = 0; // Success
    } else if (lock_result == EBUSY) { // another thread is writing
        res = pthread_mutex_lock(&(rw->upgrade)); // waits mutex unlock
        assert(res == 0);
        res = pthread_rwlock_rdlock(&(rw->rwlock)); // gains again readlock
        retval = 1; // Fail, write gained by another thread
    } else {
        return -1; // Error, should not happen
    }
    assert(res == 0);
    res = pthread_mutex_unlock(&(rw->upgrade)); // Not needed anymore
    assert(res == 0);

#else // defined USE_NOT_UPGRADABLE_MUTEX, mutex is already a writelock
    (void)rw; // Suppresses warning unused
    retval = 0; // Always success, expect to call upgrade lock on writelocks
#endif
    (void)res; // Uses res, suppress warning (optimization should remove res)
    return retval;
}

static inline
void trie_unlock(struct _rwlock * rw) {
    int res;
    res = pthread_rwlock_unlock(&(rw->rwlock));
    assert(res == 0);
#ifndef USE_NOT_UPGRADABLE_MUTEX
    res = pthread_rwlock_unlock(&(rw->write_peeding));
    assert(res == 0);
#endif
    (void)res; // Uses res, suppress warning (optimization should remove res)
}

static inline
void trie_init_mutex(struct _rwlock * rw) {
#ifndef USE_NOT_UPGRADABLE_MUTEX
    pthread_mutex_init(&(rw->upgrade), NULL);
    pthread_rwlock_init(&(rw->write_peeding), NULL);
#endif
    pthread_rwlock_init(&(rw->rwlock), NULL);
}

static inline
void trie_destroy_mutex(struct _rwlock * rw) {
    pthread_rwlock_destroy(&(rw->rwlock));
#ifndef USE_NOT_UPGRADABLE_MUTEX
    pthread_rwlock_destroy(&(rw->write_peeding));
    pthread_mutex_destroy(&(rw->upgrade));
#endif
}
