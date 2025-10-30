#ifndef BM_LOCK_H
#define BM_LOCK_H

#if defined(__APPLE__)
// ==================================================
// macOS / iOS
// ==================================================
#include <os/lock.h>

typedef os_unfair_lock BMLock;

#define BMLock_init(l)        (*(l) = OS_UNFAIR_LOCK_INIT)
#define BMLock_lock(l)        os_unfair_lock_lock(l)
#define BMLock_unlock(l)      os_unfair_lock_unlock(l)
#define BMLock_trylock(l)     os_unfair_lock_trylock(l)




#elif defined(__linux__)
// ==================================================
// Linux / Android
// ==================================================
#include <pthread.h>

typedef pthread_spinlock_t BMLock;

#define BMLock_init(l)        pthread_spin_init((l), PTHREAD_PROCESS_PRIVATE)
#define BMLock_lock(l)        pthread_spin_lock((l))
#define BMLock_unlock(l)      pthread_spin_unlock((l))
#define BMLock_trylock(l)     (pthread_spin_trylock((l)) == 0)




#elif defined(_WIN32)
// ==================================================
// Windows
// ==================================================
#include <windows.h>

typedef CRITICAL_SECTION BMLock;

#define BMLock_init(l)        InitializeCriticalSection((l))
#define BMLock_lock(l)        EnterCriticalSection((l))
#define BMLock_unlock(l)      LeaveCriticalSection((l))
#define BMLock_trylock(l)     TryEnterCriticalSection((l))




#else
// ==================================================
// Generic POSIX Fallback
// ==================================================
#include <pthread.h>

typedef pthread_mutex_t BMLock;

#define BMLock_init(l)        pthread_mutex_init((l), NULL)
#define BMLock_lock(l)        pthread_mutex_lock((l))
#define BMLock_unlock(l)      pthread_mutex_unlock((l))
#define BMLock_trylock(l)     (pthread_mutex_trylock((l)) == 0)

#endif

#endif /* BM_LOCK_H */
