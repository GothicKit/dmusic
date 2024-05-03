// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
//
// Adapted from c11threads originally authored by
//    John Tsiombikas <nuclear@member.fsf.org> and
//    Oliver Old <oliver.old@outlook.com>
// and placed into the public domain.
//
// https://github.com/jtsiomb/c11threads
#pragma once
#include "dmusic.h"

/* If you wish to use this with pthread-win32 (i.e. use the POSIX threads wrapper
 * instead of the native win32 API implementation of C11 threads), then just
 * define C11THREADS_PTHREAD_WIN32 before including this header file.
 */
#if defined(_WIN32) && !defined(C11THREADS_PTHREAD_WIN32)
	#define C11THREADS_WIN32
#endif

#include <time.h>

#ifndef TIME_UTC
	#define TIME_UTC 1
#endif

typedef int (*thrd_start_t)(void*);
typedef void (*tss_dtor_t)(void*);

enum {
	mtx_plain = 0,
	mtx_recursive = 1,
	mtx_timed = 2,
};

enum { thrd_success, thrd_timedout, thrd_busy, thrd_error, thrd_nomem };

#ifndef C11THREADS_WIN32
    /* C11 threads over POSIX threads as thin static inline wrapper functions */
	#include <stdint.h>
	#include <errno.h>
	#include <pthread.h>
	#include <sched.h> /* for sched_yield */
	#include <sys/time.h>

	#ifndef thread_local
		#define thread_local _Thread_local
	#endif

	#define ONCE_FLAG_INIT PTHREAD_ONCE_INIT
	#define TSS_DTOR_ITERATIONS PTHREAD_DESTRUCTOR_ITERATIONS

	#ifdef __APPLE__
	    /* Darwin doesn't implement timed mutexes currently */
		#define C11THREADS_NO_TIMED_MUTEX
		#include <Availability.h>
		#ifndef __MAC_10_15
			#define C11THREADS_NO_TIMESPEC_GET
		#endif
	#elif __STDC_VERSION__ < 201112L
		#define C11THREADS_NO_TIMESPEC_GET
	#endif

	#ifdef C11THREADS_NO_TIMED_MUTEX
		#define C11THREADS_TIMEDLOCK_POLL_INTERVAL 5000000 /* 5 ms */
	#endif

/* types */
typedef pthread_t thrd_t;
typedef pthread_mutex_t mtx_t;
typedef pthread_cond_t cnd_t;
typedef pthread_key_t tss_t;
typedef pthread_once_t once_flag;

/* ---- thread management ---- */

DMINT int thrd_create(thrd_t* thr, thrd_start_t func, void* arg);
DMINT void thrd_exit(int res);

DMINT int thrd_join(thrd_t thr, int* res);
DMINT int thrd_detach(thrd_t thr);
DMINT thrd_t thrd_current(void);
DMINT int thrd_equal(thrd_t a, thrd_t b);
DMINT int thrd_sleep(const struct timespec* ts_in, struct timespec* rem_out);
DMINT void thrd_yield(void);

/* ---- mutexes ---- */

DMINT int mtx_init(mtx_t* mtx, int type);
DMINT void mtx_destroy(mtx_t* mtx);
DMINT int mtx_lock(mtx_t* mtx);
DMINT int mtx_trylock(mtx_t* mtx);
DMINT int mtx_timedlock(mtx_t* mtx, const struct timespec* ts);
DMINT int mtx_unlock(mtx_t* mtx);

/* ---- condition variables ---- */

DMINT int cnd_init(cnd_t* cond);
DMINT void cnd_destroy(cnd_t* cond);
DMINT int cnd_signal(cnd_t* cond);
DMINT int cnd_broadcast(cnd_t* cond);
DMINT int cnd_wait(cnd_t* cond, mtx_t* mtx);
DMINT int cnd_timedwait(cnd_t* cond, mtx_t* mtx, const struct timespec* ts);

/* ---- thread-specific data ---- */

DMINT int tss_create(tss_t* key, tss_dtor_t dtor);
DMINT void tss_delete(tss_t key);
DMINT int tss_set(tss_t key, void* val);
DMINT void* tss_get(tss_t key);

/* ---- misc ---- */

DMINT void call_once(once_flag* flag, void (*func)(void));

#else /* C11THREADS_WIN32 */

/* C11 threads implementation using native Win32 API calls (see c11threads_win32.c) */

	#ifndef thread_local
		#ifdef _MSC_VER
			#define thread_local __declspec(thread)
		#else
			#define thread_local _Thread_local
		#endif
	#endif

	#define ONCE_FLAG_INIT                                                                                             \
		{ 0 }
	#define TSS_DTOR_ITERATIONS 4

	#ifndef _UCRT
		#define C11THREADS_NO_TIMESPEC_GET
	#endif

	#ifdef _MSC_VER
		#define C11THREADS_MSVC_NORETURN __declspec(noreturn)
		#define C11THREADS_GNUC_NORETURN
	#elif defined(__GNUC__)
		#define C11THREADS_MSVC_NORETURN
		#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 5)
			#define C11THREADS_GNUC_NORETURN __attribute__((noreturn))
		#else
			#define C11THREADS_GNUC_NORETURN
		#endif
	#else
		#define C11THREADS_MSVC_NORETURN
		#define C11THREADS_GNUC_NORETURN
	#endif

/* types */
typedef unsigned long thrd_t;
typedef struct {
	void* debug_info;
	long lock_count;
	long recursion_count;
	void* owning_thread;
	void* lock_semaphore;
	void* spin_count;
} mtx_t;
typedef void* cnd_t;
typedef unsigned long tss_t;
typedef void* once_flag;
struct _c11threads_win32_timespec32_t {
	long tv_sec;
	long tv_nsec;
};
struct _c11threads_win32_timespec64_t {
	#ifdef _MSC_VER
	__int64 tv_sec;
	#else
	long long tv_sec;
	#endif
	long tv_nsec;
};
	#if !defined(_UCRT) && !defined(_TIMESPEC_DEFINED)
		#ifdef _USE_32BIT_TIME_T
struct timespec {
	long tv_sec;
	long tv_nsec;
};
		#elif !defined(_USE_32BIT_TIME_T)
struct timespec {
	__int64 tv_sec;
	long tv_nsec;
};
		#endif /* !defined(_USE_32BIT_TIME_T) */
	#endif     /* !defined(_UCRT) && !defined(_TIMESPEC_DEFINED) */

/* Thread functions. */

DMINT int thrd_create(thrd_t* thr, thrd_start_t func, void* arg);
/* Win32: Threads not created with thrd_create() need to call this to clean up TSS. */
DMINT C11THREADS_MSVC_NORETURN void thrd_exit(int res) C11THREADS_GNUC_NORETURN;
DMINT int thrd_join(thrd_t thr, int* res);
DMINT int thrd_detach(thrd_t thr);
DMINT thrd_t thrd_current(void);
DMINT int thrd_equal(thrd_t a, thrd_t b);
DMINT int thrd_sleep(const struct timespec* ts_in, struct timespec* rem_out);
DMINT void thrd_yield(void);

/* Mutex functions. */

DMINT int mtx_init(mtx_t* mtx, int type);
DMINT void mtx_destroy(mtx_t* mtx);
DMINT int mtx_lock(mtx_t* mtx);
DMINT int mtx_trylock(mtx_t* mtx);
DMINT int mtx_timedlock(mtx_t* mtx, const struct timespec* ts);
DMINT int mtx_unlock(mtx_t* mtx);

/* Condition variable functions. */

DMINT int cnd_init(cnd_t* cond);
DMINT void cnd_destroy(cnd_t* cond);
DMINT int cnd_signal(cnd_t* cond);
DMINT int cnd_broadcast(cnd_t* cond);
DMINT int cnd_wait(cnd_t* cond, mtx_t* mtx);
DMINT int cnd_timedwait(cnd_t* cond, mtx_t* mtx, const struct timespec* ts);

/* Thread-specific storage functions. */

DMINT int tss_create(tss_t* key, tss_dtor_t dtor);
DMINT void tss_delete(tss_t key);
DMINT int tss_set(tss_t key, void* val);
DMINT void* tss_get(tss_t key);

/* One-time callable function. */

DMINT void call_once(once_flag* flag, void (*func)(void));

/* Special Win32 functions. */
/* Win32: Free resources associated with this library. */
DMINT void c11threads_win32_destroy(void);
/* Win32: Register current Win32 thread in c11threads to allow for proper thrd_join(). */
DMINT int c11threads_win32_thrd_self_register(void);
/* Win32: Register Win32 thread by ID in c11threads to allow for proper thrd_join(). */
DMINT int c11threads_win32_thrd_register(unsigned long win32_thread_id);

	#ifdef _MSC_VER
		#pragma warning(push)
		#pragma warning(disable : 4127) /* Warning C4127: conditional expression is constant */
	#endif

/* ---- thread management ---- */

DMINT int _c11threads_win32_thrd_sleep32(const struct _c11threads_win32_timespec32_t* ts_in,
                                   struct _c11threads_win32_timespec32_t* rem_out);
DMINT int _c11threads_win32_thrd_sleep64(const struct _c11threads_win32_timespec64_t* ts_in,
                                   struct _c11threads_win32_timespec64_t* rem_out);

/* ---- mutexes ---- */

DMINT int _c11threads_win32_mtx_timedlock32(mtx_t* mtx, const struct _c11threads_win32_timespec32_t* ts);
DMINT int _c11threads_win32_mtx_timedlock64(mtx_t* mtx, const struct _c11threads_win32_timespec64_t* ts);

/* ---- condition variables ---- */

DMINT int _c11threads_win32_cnd_timedwait32(cnd_t* cond, mtx_t* mtx, const struct _c11threads_win32_timespec32_t* ts);
DMINT int _c11threads_win32_cnd_timedwait64(cnd_t* cond, mtx_t* mtx, const struct _c11threads_win32_timespec64_t* ts);

/* ---- misc ---- */
#endif
