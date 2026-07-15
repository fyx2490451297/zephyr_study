#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/spinlock.h>

LOG_MODULE_REGISTER(spinlock_demo, LOG_LEVEL_INF);

/* --------------------------------------------------------------------------
 * Spinlock (struct k_spinlock) learning notes
 * --------------------------------------------------------------------------
 * A spinlock is the lowest-level mutual-exclusion primitive in Zephyr. It
 * protects a critical section by disabling interrupts (on single-core
 * builds) or busy-waiting on another CPU's lock (on SMP builds), so the
 * critical section must be very short.
 *
 * Core concepts:
 *   1. Unlike k_mutex, a spinlock is *not* owned by a thread and does not
 *      support priority inheritance or recursive locking. It is meant for
 *      very short critical sections only.
 *
 *   2. A spinlock may be safely taken from both thread context and
 *      interrupt (ISR) context. A mutex or semaphore with K_FOREVER cannot
 *      be taken from an ISR, but a spinlock can, because locking briefly
 *      disables interrupts instead of blocking a thread.
 *
 *   3. k_spin_lock() returns a `k_spinlock_key_t`, which records the
 *      previous interrupt state so k_spin_unlock() can restore it exactly,
 *      even with nested lock/unlock pairs on different spinlocks.
 *
 *   4. Because interrupts are disabled while the lock is held, the code
 *      inside the critical section must never block (no k_sleep(),
 *      no k_sem_take() with a timeout, no logging that itself blocks).
 *
 * Key API:
 *   - struct k_spinlock lock = {}          : zero-initialize a static spinlock
 *   - k_spin_lock(&lock)                     : disable interrupts, return key
 *   - k_spin_unlock(&lock, key)               : restore interrupts to prior state
 *
 * In this demo:
 *   - A shared `spinlock_demo_stats` struct (event count + running sum) is
 *     updated from three different contexts: a writer thread, a reader
 *     thread, and a periodic timer ISR (k_timer expiry callback).
 *   - The writer thread appends "events" every 300 ms.
 *   - The timer ISR fires every 250 ms and also appends an "event", proving
 *     the spinlock is safe to take from interrupt context.
 *   - The reader thread wakes up every 1 second, atomically snapshots and
 *     resets the stats under the same spinlock, and logs the result.
 *   - All three contexts race on the same struct, but since every access
 *     happens inside the spinlock's critical section, the count and sum
 *     always stay consistent (no lost updates, no torn reads).
 *
 * This is a good follow-up demo after mutexes/semaphores/atomics because it
 * shows the primitive one layer below them: the tool the kernel itself uses
 * to protect data that ISRs also need to touch.
 * --------------------------------------------------------------------------
 */

/* Shared data protected entirely by spinlock_demo_lock below. Never access
 * these fields without holding the lock first.
 */
struct spinlock_demo_stats {
	uint32_t event_count; /* number of events recorded since last reader read */
	uint32_t sum;          /* running sum of event payloads                   */
};

static struct spinlock_demo_stats spinlock_demo_stats;

/* Statically allocates and zero-initializes the spinlock. A `struct
 * k_spinlock` needs no explicit initializer; it is valid in its
 * zero-initialized (unlocked) state.
 */
static struct k_spinlock spinlock_demo_lock;

/* --------------------------------------------------------------------------
 * Timer ISR callback
 *
 * Fires every 250 ms in interrupt context. Demonstrates that the same
 * spinlock protecting the writer/reader threads can also be safely taken
 * here, which would not be possible with a mutex.
 * --------------------------------------------------------------------------
 */
static void spinlock_demo_timer_expiry(struct k_timer *timer)
{
	ARG_UNUSED(timer);

	k_spinlock_key_t key = k_spin_lock(&spinlock_demo_lock);

	spinlock_demo_stats.event_count++;
	spinlock_demo_stats.sum += 1U; /* ISR contributes a fixed payload of 1 */

	k_spin_unlock(&spinlock_demo_lock, key);
}

static K_TIMER_DEFINE(spinlock_demo_timer, spinlock_demo_timer_expiry, NULL);

/* --------------------------------------------------------------------------
 * Writer thread
 *
 * Every 300 ms, takes the spinlock and appends one "event" with a growing
 * payload value to the shared stats struct.
 * --------------------------------------------------------------------------
 */
static void spinlock_writer_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	uint32_t payload = 0;

	while (1) {
		payload++;

		/* Critical section: keep it short, no blocking calls inside. */
		k_spinlock_key_t key = k_spin_lock(&spinlock_demo_lock);

		spinlock_demo_stats.event_count++;
		spinlock_demo_stats.sum += payload;

		k_spin_unlock(&spinlock_demo_lock, key);

		LOG_INF("writer: recorded event payload=%u", payload);

		k_sleep(K_MSEC(300));
	}
}

/* --------------------------------------------------------------------------
 * Reader thread
 *
 * Every 1 second, atomically snapshots and resets the shared stats, then
 * logs the aggregated result outside the critical section (logging itself
 * may block, so it must never happen while the spinlock is held).
 * --------------------------------------------------------------------------
 */
static void spinlock_reader_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		k_sleep(K_SECONDS(1));

		struct spinlock_demo_stats snapshot;

		/* Critical section: copy out and reset, then unlock immediately. */
		k_spinlock_key_t key = k_spin_lock(&spinlock_demo_lock);

		snapshot = spinlock_demo_stats;
		spinlock_demo_stats.event_count = 0;
		spinlock_demo_stats.sum = 0;

		k_spin_unlock(&spinlock_demo_lock, key);

		LOG_INF("reader: %u events in the last second, sum=%u",
			snapshot.event_count, snapshot.sum);
	}
}

/* K_THREAD_DEFINE creates and auto-starts both threads at boot.
 *
 * Both threads share the same priority (CONFIG_PACKAGE_SPINLOCK_DEMO_*_PRIORITY,
 * default 8) so the scheduler interleaves them, and the periodic timer ISR
 * races with both, exercising the spinlock from all three contexts.
 *
 * Stack size is shared and controlled via Kconfig.
 */
K_THREAD_DEFINE(spinlock_writer_tid,
		CONFIG_PACKAGE_SPINLOCK_DEMO_STACK_SIZE,
		spinlock_writer_thread,
		NULL, NULL, NULL,
		CONFIG_PACKAGE_SPINLOCK_DEMO_WRITER_PRIORITY, 0, 0);

K_THREAD_DEFINE(spinlock_reader_tid,
		CONFIG_PACKAGE_SPINLOCK_DEMO_STACK_SIZE,
		spinlock_reader_thread,
		NULL, NULL, NULL,
		CONFIG_PACKAGE_SPINLOCK_DEMO_READER_PRIORITY, 0, 0);

/* --------------------------------------------------------------------------
 * Module init
 *
 * Starts the periodic timer that drives the ISR-context spinlock updates.
 * The worker threads are already auto-started by K_THREAD_DEFINE above.
 * --------------------------------------------------------------------------
 */
static int spinlock_demo_init(void)
{
	k_timer_start(&spinlock_demo_timer, K_MSEC(250), K_MSEC(250));

	return 0;
}

SYS_INIT(spinlock_demo_init, APPLICATION, CONFIG_PACKAGE_SPINLOCK_DEMO_INIT_PRIORITY);
