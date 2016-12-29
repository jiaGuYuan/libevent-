/*
 * Copyright (c) 2008-2012 Niels Provos and Nick Mathewson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef EVENT2_THREAD_H_INCLUDED_
#define EVENT2_THREAD_H_INCLUDED_

/** @file event2/thread.h

  Functions for multi-threaded applications using Libevent.

  When using a multi-threaded application in which multiple threads
  add and delete events from a single event base, Libevent needs to
  lock its data structures.

  Like the memory-management function hooks, all of the threading functions
  _must_ be set up before an event_base is created if you want the base to
  use them.

  Most programs will either be using Windows threads or Posix threads.  You
  can configure Libevent to use one of these event_use_windows_threads() or
  event_use_pthreads() respectively.  If you're using another threading
  library, you'll need to configure threading functions manually using
  evthread_set_lock_callbacks() and evthread_set_condition_callbacks().

 */

#include <event2/visibility.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <event2/event-config.h>

/**
   @name Flags passed to lock functions

   @{
*/
/** A flag passed to a locking callback when the lock was allocated as a
 * read-write lock, and we want to acquire or release the lock for writing.
 仅用于读写锁：为写操作请求或者释放锁*/
#define EVTHREAD_WRITE	0x04
/** A flag passed to a locking callback when the lock was allocated as a
 * read-write lock, and we want to acquire or release the lock for reading.
 仅用于读写锁：为读操作请求或者释放锁*/
#define EVTHREAD_READ	0x08
/** A flag passed to a locking callback when we don't want to block waiting
 * for the lock; if we can't get the lock immediately, we will instead
 * return nonzero from the locking callback.
 仅用于锁定：仅在可以立刻锁定的时候才请求锁*/
#define EVTHREAD_TRY    0x10
/**@}*/

#if !defined(EVENT__DISABLE_THREAD_SUPPORT) || defined(EVENT_IN_DOXYGEN_)

#define EVTHREAD_LOCK_API_VERSION 1

/**
   @name Types of locks

   @{*/
/** A recursive lock is one that can be acquired multiple times at once by the
 * same thread.  No other process can allocate the lock until the thread that
 * has been holding it has unlocked it as many times as it locked it.
 不会阻塞已经持有它的线程的锁。 一旦持有它的线程进行原来锁定次数的解锁，
 其他线程立刻就可以请求它了。*/
#define EVTHREAD_LOCKTYPE_RECURSIVE 1
/* A read-write lock is one that allows multiple simultaneous readers, but
 * where any one writer excludes all other writers and readers.
 可以让多个线程同时因为读而持有它， 但是任何时刻只有一个线程因为写而持有它。
 写操作排斥所有读操作。*/
#define EVTHREAD_LOCKTYPE_READWRITE 2
/**@}*/

/** This structure describes the interface a threading library uses for
 * locking.   It's used to tell evthread_set_lock_callbacks() how to use
 * locking on this platform.
 */
 //描述的锁回调函数及其能力。
struct evthread_lock_callbacks {
	/** 当前版本的锁API.  设置为 EVTHREAD_LOCK_API_VERSION */
	int lock_api_version;
	/** 这个版本的锁支持API锁定吗? 设置为 EVTHREAD_LOCKTYPE_RECURSIVE and EVTHREAD_LOCKTYPE_READWRITE.
	    (注意:注意RECURSIVE锁是目前强制使用的,READWRITE锁目前不使用) **/
	unsigned supported_locktypes;
	/** 函数分配和初始化“locktype”类型的新锁。返回NULL失败 */
	void *(*alloc)(unsigned locktype);
	/** 函数释放指定类型锁持有的所有资源. */
	void (*free)(void *lock, unsigned locktype);
	/** 以指定模式请求锁.成功，返回0;失败，返回非0 */
	int (*lock)(unsigned mode, void *lock);
	/** unlock函数必须试图解锁，成功则返回0，否则返回非零。 */
	int (*unlock)(unsigned mode, void *lock);
};

/** Sets a group of functions that Libevent should use for locking.
 * For full information on the required callback API, see the
 * documentation for the individual members of evthread_lock_callbacks.
 *
 * Note that if you're using Windows or the Pthreads threading library, you
 * probably shouldn't call this function; instead, use
 * evthread_use_windows_threads() or evthread_use_posix_threads() if you can.
 */
EVENT2_EXPORT_SYMBOL
int evthread_set_lock_callbacks(const struct evthread_lock_callbacks *);

#define EVTHREAD_CONDITION_API_VERSION 1

struct timeval;

/** This structure describes the interface a threading library uses for
 * condition variables.  It's used to tell evthread_set_condition_callbacks
 * how to use locking on this platform.
 */
 //描述了与条件变量相关的回调函数
struct evthread_condition_callbacks {
	/** 当前版本的条件API. 设置为 EVTHREAD_CONDITION_API_VERSION */
	int condition_api_version;
	/** 函数分配和初始化一个新的条件变量。 成功:返回条件变量
	失败:返回NULL。 'condtype' 参数为0	 */
	void *(*alloc_condition)(unsigned condtype);
	/** 释放条件变量持有的存储器和资源. */
	void (*free_condition)(void *cond);
	/** Function to signal a condition variable.  If 'broadcast' is 1, all
	 * threads waiting on 'cond' should be woken; otherwise, only on one
	 * thread is worken.  Should return 0 on success, -1 on failure.
	 * This function will only be called while holding the associated
	 * lock for the condition.
	 */
	int (*signal_condition)(void *cond, int broadcast);
	/** Function to wait for a condition variable.  The lock 'lock'
	 * will be held when this function is called; should be released
	 * while waiting for the condition to be come signalled, and
	 * should be held again when this function returns.
	 * If timeout is provided, it is interval of seconds to wait for
	 * the event to become signalled; if it is NULL, the function
	 * should wait indefinitely.
	 *
	 
	 * The function should return -1 on error; 0 if the condition
	 * was signalled, or 1 on a timeout.
	 这个函数将等待条件变量.
	 wait_condition 函数要求三个参数：一个由alloc_condition分配的条件变量，
	 一个由你提供的evthread_lock_callbacks.alloc函数分配的锁，以及一个可选的超时值。 
	 调用本函数时，必须已经持有参数指定的锁； 本函数应该释放指定的锁，等待条件变量成为授信状态，
	 或者直到指定的超时时间已经流逝（可选）。
	 wait_condition应该在错误时返回-1，条件变量授信时返回0，超时时返回1。
	 返回之前， 函数应该确定其再次持有锁。最后,signal_condition函数应该唤醒等待该条件变量的
	 某个线程 (broadcast参数为false时)， 或者唤醒等待条件变量的所有线程(broadcast参数为true时)。只有在持有与条件变量相关的锁的时候，才能够进行这些操作
	 */
	int (*wait_condition)(void *cond, void *lock,
	    const struct timeval *timeout);
};

/** Sets a group of functions that Libevent should use for condition variables.
 * For full information on the required callback API, see the
 * documentation for the individual members of evthread_condition_callbacks.
 *
 * Note that if you're using Windows or the Pthreads threading library, you
 * probably shouldn't call this function; instead, use
 * evthread_use_windows_threads() or evthread_use_pthreads() if you can.
 */
EVENT2_EXPORT_SYMBOL
int evthread_set_condition_callbacks(
	const struct evthread_condition_callbacks *);

/**
   Sets the function for determining the thread id.

   @param base the event base for which to set the id function
   @param id_fn the identify function Libevent should invoke to
     determine the identity of a thread.
*/
EVENT2_EXPORT_SYMBOL
void evthread_set_id_callback(
    unsigned long (*id_fn)(void));

#if (defined(_WIN32) && !defined(EVENT__DISABLE_THREAD_SUPPORT)) || defined(EVENT_IN_DOXYGEN_)
/** Sets up Libevent for use with Windows builtin locking and thread ID
    functions.  Unavailable if Libevent is not built for Windows.

    @return 0 on success, -1 on failure. */
EVENT2_EXPORT_SYMBOL
int evthread_use_windows_threads(void);
/**
   Defined if Libevent was built with support for evthread_use_windows_threads()
*/
#define EVTHREAD_USE_WINDOWS_THREADS_IMPLEMENTED 1
#endif

#if defined(EVENT__HAVE_PTHREADS) || defined(EVENT_IN_DOXYGEN_)
/** Sets up Libevent for use with Pthreads locking and thread ID functions.
    Unavailable if Libevent is not build for use with pthreads.  Requires
    libraries to link against Libevent_pthreads as well as Libevent.

    @return 0 on success, -1 on failure. */
EVENT2_EXPORT_SYMBOL
int evthread_use_pthreads(void);
/** Defined if Libevent was built with support for evthread_use_pthreads() */
#define EVTHREAD_USE_PTHREADS_IMPLEMENTED 1

#endif

/** Enable debugging wrappers around the current lock callbacks.  If Libevent
 * makes one of several common locking errors, exit with an assertion failure.
 *
 * If you're going to call this function, you must do so before any locks are
 * allocated.
 **/
EVENT2_EXPORT_SYMBOL
void evthread_enable_lock_debugging(void);

/* Old (misspelled) version: This is deprecated; use
 * evthread_enable_log_debugging instead. */
EVENT2_EXPORT_SYMBOL
void evthread_enable_lock_debuging(void);

#endif /* EVENT__DISABLE_THREAD_SUPPORT */

struct event_base;
/** Make sure it's safe to tell an event base to wake up from another thread
    or a signal handler.

    You shouldn't need to call this by hand; configuring the base with thread
    support should be necessary and sufficient.

    @return 0 on success, -1 on failure.
 */
EVENT2_EXPORT_SYMBOL
int evthread_make_base_notifiable(struct event_base *base);

#ifdef __cplusplus
}
#endif

#endif /* EVENT2_THREAD_H_INCLUDED_ */
