#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(kmutex_demo, LOG_LEVEL_INF);

/* --------------------------------------------------------------------------
 * Mutex (k_mutex) learning notes
 * --------------------------------------------------------------------------
 * A mutex (mutual exclusion lock) in Zephyr protects a shared resource so
 * that only ONE thread can access it at a time.
 *
 * Core concepts:
 *   1. Lock / Unlock:
 *      - k_mutex_lock()   : acquire the mutex; blocks if already held
 *      - k_mutex_unlock() : release the mutex; wakes the next waiter
 *
 *   2. Ownership:
 *      A mutex is owned by the thread that locked it.  Only the owner may
 *      unlock it.  This is different from a semaphore, which any thread can
 *      give regardless of who took it.
 *
 *   3. Recursive locking:
 *      Zephyr mutexes are RECURSIVE.  The same thread may lock the same
 *      mutex multiple times without deadlocking itself, as long as it calls
 *      k_mutex_unlock() the same number of times to fully release it.
 *
 *   4. Priority inheritance:
 *      Zephyr mutexes implement priority inheritance automatically.
 *      Scenario: a high-priority thread (Thread A) waits for a mutex held
 *      by a low-priority thread (Thread B).  The kernel temporarily raises
 *      Thread B's priority to match Thread A so that Thread B finishes its
 *      critical section quickly and is not preempted by medium-priority
 *      threads.  This prevents "priority inversion".
 *
 * Mutex vs Semaphore (count=1):
 *   - Semaphore: any thread can give; no ownership concept; use for
 *     signaling events between threads.
 *   - Mutex: only the owner can unlock; built-in priority inheritance; use
 *     for protecting shared data/resources.
 *
 * In this demo:
 *   - A shared counter (shared_counter) represents a resource that must not
 *     be modified by two threads simultaneously.
 *   - Thread A (high priority) and Thread B (low priority) both increment
 *     the counter inside a critical section guarded by the mutex.
 *   - Each thread holds the mutex for 500 ms to make contention visible in
 *     the logs.
 *   - The logs will clearly show that the two threads never overlap inside
 *     the critical section.
 *
 * Without the mutex, both threads could read-modify-write the counter at
 * the same time, producing lost updates (a classic data race).
 * --------------------------------------------------------------------------
 */

/* K_MUTEX_DEFINE statically creates and initialises the mutex.
 *
 * No parameters are needed beyond the name: Zephyr mutexes are always
 * unlocked at creation and support recursive locking + priority inheritance
 * out of the box.
 */
K_MUTEX_DEFINE(counter_mutex);

/* Shared resource --------------------------------------------------------- */

/* This counter is accessed by both threads.
 * It MUST be modified only while holding counter_mutex.
 */
static uint32_t shared_counter;

/* --------------------------------------------------------------------------
 * Thread A — high priority (6)
 *
 * Locks the mutex, increments the shared counter, holds the lock for a
 * while to make contention easy to observe, then unlocks.
 * --------------------------------------------------------------------------
 */
static void kmutex_thread_a(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		/* k_mutex_lock() acquires the mutex.
		 *
		 * K_FOREVER: block until the mutex is available.
		 * If Thread B currently holds it, Thread A waits here and the
		 * scheduler runs Thread B (possibly at an elevated priority due
		 * to priority inheritance) until Thread B calls k_mutex_unlock().
		 *
		 * Return value 0 means the lock was acquired successfully.
		 */
		k_mutex_lock(&counter_mutex, K_FOREVER);

		/* --- Critical section begin ---
		 *
		 * Only one thread can be inside this region at any time.
		 * Thread B is guaranteed NOT to be modifying shared_counter
		 * while we are here.
		 */
		LOG_INF("Thread A: locked mutex, counter=%u", shared_counter);

		shared_counter++;

		/* Hold the lock for 500 ms to simulate real work and make
		 * contention clearly visible in the logs.
		 */
		k_sleep(K_MSEC(500));

		LOG_INF("Thread A: unlocking mutex, counter=%u", shared_counter);
		/* --- Critical section end --- */

		/* k_mutex_unlock() releases ownership.
		 *
		 * If Thread B is waiting on k_mutex_lock(), the kernel will
		 * wake it up and transfer ownership.
		 */
		k_mutex_unlock(&counter_mutex);

		/* Sleep outside the lock so Thread B gets a fair chance to run. */
		k_sleep(K_MSEC(200));
	}
}

/* --------------------------------------------------------------------------
 * Thread B — low priority (8)
 *
 * Same pattern as Thread A.  Because its priority is lower, it will often
 * find the mutex already held by Thread A and have to wait.
 *
 * When Thread A (priority 6) is waiting for the mutex held by Thread B
 * (priority 8), the kernel raises Thread B temporarily to priority 6 so it
 * is not starved by other priority-7 threads.  This is priority inheritance.
 * --------------------------------------------------------------------------
 */
static void kmutex_thread_b(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		/* Thread B also waits forever for the mutex. */
		k_mutex_lock(&counter_mutex, K_FOREVER);

		/* --- Critical section begin --- */
		LOG_INF("Thread B: locked mutex, counter=%u", shared_counter);

		shared_counter++;

		k_sleep(K_MSEC(500));

		LOG_INF("Thread B: unlocking mutex, counter=%u", shared_counter);
		/* --- Critical section end --- */

		k_mutex_unlock(&counter_mutex);

		k_sleep(K_MSEC(200));
	}
}

/* K_THREAD_DEFINE creates and auto-starts both threads at boot.
 *
 * Thread A has priority CONFIG_PACKAGE_KMUTEX_DEMO_THREAD_A_PRIORITY (default 6,
 * higher priority), Thread B has CONFIG_PACKAGE_KMUTEX_DEMO_THREAD_B_PRIORITY
 * (default 8, lower priority).
 *
 * This priority gap makes it possible to observe priority inheritance:
 * when Thread A waits for the mutex held by Thread B, Thread B is
 * temporarily promoted to Thread A's priority by the kernel.
 *
 * Stack size is controlled via Kconfig.
 */
K_THREAD_DEFINE(kmutex_thread_a_tid,
		CONFIG_PACKAGE_KMUTEX_DEMO_STACK_SIZE,
		kmutex_thread_a,
		NULL, NULL, NULL,
		CONFIG_PACKAGE_KMUTEX_DEMO_THREAD_A_PRIORITY, 0, 0);

K_THREAD_DEFINE(kmutex_thread_b_tid,
		CONFIG_PACKAGE_KMUTEX_DEMO_STACK_SIZE,
		kmutex_thread_b,
		NULL, NULL, NULL,
		CONFIG_PACKAGE_KMUTEX_DEMO_THREAD_B_PRIORITY, 0, 0);
