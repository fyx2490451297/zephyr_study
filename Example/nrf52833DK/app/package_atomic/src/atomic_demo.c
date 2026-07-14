#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(atomic_demo, LOG_LEVEL_INF);

/* --------------------------------------------------------------------------
 * Atomic operations (atomic_t) learning notes
 * --------------------------------------------------------------------------
 * Zephyr's atomic API provides lock-free read-modify-write operations on an
 * `atomic_t` (a machine word, typically `long`). They are useful when two or
 * more threads (or a thread and an ISR) need to update shared state without
 * paying the cost of a mutex/semaphore, and without risking data races.
 *
 * Core concepts:
 *   1. atomic_t is just a word-sized integer, but every access must go
 *      through the atomic_* API. Reading or writing it directly with plain
 *      C operators (`=`, `++`) is not safe across threads.
 *
 *   2. Read-modify-write operations (atomic_inc, atomic_dec, atomic_add,
 *      atomic_sub, atomic_or, atomic_and, atomic_xor) apply the operation as
 *      a single indivisible step, even if two threads call them at the same
 *      instant on the same variable.
 *
 *   3. atomic_cas() (compare-and-swap) is the building block for lock-free
 *      algorithms: it only writes the new value if the current value still
 *      matches the expected one, and reports whether the swap happened.
 *
 *   4. Bit helpers (atomic_set_bit, atomic_clear_bit, atomic_test_bit) treat
 *      an atomic_t (or an ATOMIC_DEFINE array) as a bitmask that can be
 *      updated concurrently, e.g. for lock-free flag/event bits.
 *
 * Key API used in this demo:
 *   - atomic_inc(&v) / atomic_dec(&v)      : increment/decrement, returns old value
 *   - atomic_get(&v) / atomic_set(&v, val) : plain read / unconditional write
 *   - atomic_cas(&v, old, new)             : compare-and-swap, returns true on success
 *   - atomic_set_bit(&v, bit) / atomic_clear_bit(&v, bit)
 *   - atomic_test_bit(&v, bit)
 *
 * In this demo:
 *   - An incrementer thread bumps a shared counter and sets bits in a shared
 *     bitmask every 500 ms, without holding any lock.
 *   - A decrementer thread decrements the same counter and clears bits in
 *     the same bitmask every 700 ms, also without any lock.
 *   - Both threads race on the same atomic_t variables, but since every
 *     update goes through the atomic_* API, the counter and bitmask are
 *     always left in a consistent state (no lost updates, no torn reads).
 *   - A periodic atomic_cas() demonstrates resetting the counter back to 0
 *     only if it has drifted to a specific "high water mark" value.
 *
 * This is a good follow-up demo after mutexes/semaphores because it shows
 * that not every shared-state problem needs a lock: for single-word
 * counters and flags, atomic operations are simpler and cheaper.
 * --------------------------------------------------------------------------
 */

/* Shared counter, updated concurrently by both worker threads without a lock. */
static atomic_t atomic_demo_counter = ATOMIC_INIT(0);

/* Shared bitmask, used to demonstrate lock-free bit manipulation. */
static atomic_t atomic_demo_flags = ATOMIC_INIT(0);

/* Bit positions used in atomic_demo_flags. */
#define ATOMIC_DEMO_BIT_INCREMENTER 0
#define ATOMIC_DEMO_BIT_DECREMENTER 1

/* Counter value at which the incrementer resets the counter via atomic_cas(). */
#define ATOMIC_DEMO_HIGH_WATER_MARK 5

/* --------------------------------------------------------------------------
 * Incrementer thread
 *
 * Every 500 ms:
 *   - atomically increments the shared counter
 *   - sets its "activity" bit in the shared bitmask
 *   - if the counter reached the high water mark, atomically resets it to 0
 *     using atomic_cas(), only succeeding if no other thread changed it in
 *     the meantime.
 * --------------------------------------------------------------------------
 */
static void atomic_incrementer_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		/* atomic_inc() returns the value *before* the increment. */
		atomic_val_t old_val = atomic_inc(&atomic_demo_counter);

		atomic_set_bit(&atomic_demo_flags, ATOMIC_DEMO_BIT_INCREMENTER);

		LOG_INF("incrementer: counter %d -> %d  flags=0x%x",
			(int)old_val, (int)old_val + 1,
			(unsigned int)atomic_get(&atomic_demo_flags));

		/* Demonstrate compare-and-swap: only reset the counter if it is
		 * still exactly at the high water mark, i.e. no one else raced
		 * ahead of us between the log above and this check.
		 */
		if (atomic_cas(&atomic_demo_counter, ATOMIC_DEMO_HIGH_WATER_MARK, 0)) {
			LOG_INF("incrementer: high water mark reached, counter reset to 0");
		}

		k_sleep(K_MSEC(500));
	}
}

/* --------------------------------------------------------------------------
 * Decrementer thread
 *
 * Every 700 ms:
 *   - atomically decrements the shared counter
 *   - sets its "activity" bit in the shared bitmask
 *   - clears the incrementer's bit to show bit manipulation is also
 *     lock-free and independent of the counter updates above.
 * --------------------------------------------------------------------------
 */
static void atomic_decrementer_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		/* atomic_dec() returns the value *before* the decrement. */
		atomic_val_t old_val = atomic_dec(&atomic_demo_counter);

		atomic_set_bit(&atomic_demo_flags, ATOMIC_DEMO_BIT_DECREMENTER);

		LOG_INF("decrementer: counter %d -> %d  flags=0x%x",
			(int)old_val, (int)old_val - 1,
			(unsigned int)atomic_get(&atomic_demo_flags));

		if (atomic_test_bit(&atomic_demo_flags, ATOMIC_DEMO_BIT_INCREMENTER)) {
			atomic_clear_bit(&atomic_demo_flags, ATOMIC_DEMO_BIT_INCREMENTER);
		}

		k_sleep(K_MSEC(700));
	}
}

/* K_THREAD_DEFINE creates and auto-starts both threads at boot.
 *
 * Both threads share the same priority (CONFIG_PACKAGE_ATOMIC_DEMO_*_PRIORITY,
 * default 8) so the scheduler interleaves them, exercising the lock-free
 * atomic updates under real concurrency.
 *
 * Stack size is shared and controlled via Kconfig.
 */
K_THREAD_DEFINE(atomic_incrementer_tid,
		CONFIG_PACKAGE_ATOMIC_DEMO_STACK_SIZE,
		atomic_incrementer_thread,
		NULL, NULL, NULL,
		CONFIG_PACKAGE_ATOMIC_DEMO_INCREMENTER_PRIORITY, 0, 0);

K_THREAD_DEFINE(atomic_decrementer_tid,
		CONFIG_PACKAGE_ATOMIC_DEMO_STACK_SIZE,
		atomic_decrementer_thread,
		NULL, NULL, NULL,
		CONFIG_PACKAGE_ATOMIC_DEMO_DECREMENTER_PRIORITY, 0, 0);
