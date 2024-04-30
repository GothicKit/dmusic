// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "Thread.h"

#ifndef C11THREADS_WIN32
/* ---- thread management ---- */

int thrd_create(thrd_t* thr, thrd_start_t func, void* arg) {
	int res = pthread_create(thr, 0, (void* (*) (void*) ) func, arg);
	if (res == 0) {
		return thrd_success;
	}
	return res == ENOMEM ? thrd_nomem : thrd_error;
}

void thrd_exit(int res) {
	pthread_exit((void*) (intptr_t) res);
}

int thrd_join(thrd_t thr, int* res) {
	void* retval;

	if (pthread_join(thr, &retval) != 0) {
		return thrd_error;
	}
	if (res) {
		*res = (int) (intptr_t) retval;
	}
	return thrd_success;
}

int thrd_detach(thrd_t thr) {
	return pthread_detach(thr) == 0 ? thrd_success : thrd_error;
}

thrd_t thrd_current(void) {
	return pthread_self();
}

int thrd_equal(thrd_t a, thrd_t b) {
	return pthread_equal(a, b);
}

int thrd_sleep(const struct timespec* ts_in, struct timespec* rem_out) {
	if (nanosleep(ts_in, rem_out) < 0) {
		if (errno == EINTR) return -1;
		return -2;
	}
	return 0;
}

void thrd_yield(void) {
	sched_yield();
}

/* ---- mutexes ---- */

int mtx_init(mtx_t* mtx, int type) {
	int res;
	pthread_mutexattr_t attr;

	pthread_mutexattr_init(&attr);

	if (type & mtx_timed) {
	#ifdef PTHREAD_MUTEX_TIMED_NP
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_TIMED_NP);
	#else
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
	#endif
	}
	if (type & mtx_recursive) {
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	}

	res = pthread_mutex_init(mtx, &attr) == 0 ? thrd_success : thrd_error;
	pthread_mutexattr_destroy(&attr);
	return res;
}

void mtx_destroy(mtx_t* mtx) {
	pthread_mutex_destroy(mtx);
}

int mtx_lock(mtx_t* mtx) {
	int res = pthread_mutex_lock(mtx);
	return res == 0 ? thrd_success : thrd_error;
}

int mtx_trylock(mtx_t* mtx) {
	int res = pthread_mutex_trylock(mtx);
	if (res == EBUSY) {
		return thrd_busy;
	}
	return res == 0 ? thrd_success : thrd_error;
}

int mtx_timedlock(mtx_t* mtx, const struct timespec* ts) {
	int res = 0;
	#ifdef C11THREADS_NO_TIMED_MUTEX
	/* fake a timedlock by polling trylock in a loop and waiting for a bit */
	struct timeval now;
	struct timespec sleeptime;

	sleeptime.tv_sec = 0;
	sleeptime.tv_nsec = C11THREADS_TIMEDLOCK_POLL_INTERVAL;

	while ((res = pthread_mutex_trylock(mtx)) == EBUSY) {
		gettimeofday(&now, NULL);

		if (now.tv_sec > ts->tv_sec || (now.tv_sec == ts->tv_sec && (now.tv_usec * 1000) >= ts->tv_nsec)) {
			return thrd_timedout;
		}

		nanosleep(&sleeptime, NULL);
	}
	#else
	if ((res = pthread_mutex_timedlock(mtx, ts)) == ETIMEDOUT) {
		return thrd_timedout;
	}
	#endif
	return res == 0 ? thrd_success : thrd_error;
}

int mtx_unlock(mtx_t* mtx) {
	return pthread_mutex_unlock(mtx) == 0 ? thrd_success : thrd_error;
}

/* ---- condition variables ---- */

int cnd_init(cnd_t* cond) {
	return pthread_cond_init(cond, 0) == 0 ? thrd_success : thrd_error;
}

void cnd_destroy(cnd_t* cond) {
	pthread_cond_destroy(cond);
}

int cnd_signal(cnd_t* cond) {
	return pthread_cond_signal(cond) == 0 ? thrd_success : thrd_error;
}

int cnd_broadcast(cnd_t* cond) {
	return pthread_cond_broadcast(cond) == 0 ? thrd_success : thrd_error;
}

int cnd_wait(cnd_t* cond, mtx_t* mtx) {
	return pthread_cond_wait(cond, mtx) == 0 ? thrd_success : thrd_error;
}

int cnd_timedwait(cnd_t* cond, mtx_t* mtx, const struct timespec* ts) {
	int res;

	if ((res = pthread_cond_timedwait(cond, mtx, ts)) != 0) {
		return res == ETIMEDOUT ? thrd_timedout : thrd_error;
	}
	return thrd_success;
}

/* ---- thread-specific data ---- */

int tss_create(tss_t* key, tss_dtor_t dtor) {
	return pthread_key_create(key, dtor) == 0 ? thrd_success : thrd_error;
}

void tss_delete(tss_t key) {
	pthread_key_delete(key);
}

int tss_set(tss_t key, void* val) {
	return pthread_setspecific(key, val) == 0 ? thrd_success : thrd_error;
}

void* tss_get(tss_t key) {
	return pthread_getspecific(key);
}

/* ---- misc ---- */

void call_once(once_flag* flag, void (*func)(void)) {
	pthread_once(flag, func);
}
#endif
