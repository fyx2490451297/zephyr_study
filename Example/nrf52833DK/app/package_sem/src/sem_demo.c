#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sem_demo, LOG_LEVEL_INF);

/* --------------------------------------------------------------------------
 * Semaphore learning notes
 * --------------------------------------------------------------------------
 * A semaphore in Zephyr is a synchronization primitive used to represent
 * available "permits" or "events".
 *
 * The three core concepts are:
 *   1. initial count:
 *      how many permits exist when the semaphore is created.
 *   2. limit:
 *      the maximum count the semaphore is allowed to reach.
 *   3. take / give:
 *      - k_sem_take(): wait for and consume one permit
 *      - k_sem_give(): add one permit and wake a waiting thread if needed
 *
 * In this demo:
 *   - initial count is 0, so the consumer starts blocked
 *   - limit is 1, so the semaphore behaves like a simple event flag
 *   - the producer gives the semaphore every 3 seconds
 *   - the consumer takes the semaphore and prints a message
 *
 * This is a good first demo because it clearly shows:
 *   - blocking behavior
 *   - wake-up behavior
 *   - the difference between "waiting" and "signaling"
 * --------------------------------------------------------------------------
 */
K_SEM_DEFINE(sem_demo_sem, 0, 1);

static void sem_consumer_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		/* Wait forever until the producer gives the semaphore.
		 *
		 * K_FOREVER means this thread will block and yield the CPU.
		 * The scheduler can then run other threads while we are waiting.
		 */
		LOG_INF("consumer: waiting for semaphore...");
		k_sem_take(&sem_demo_sem, K_FOREVER);

		/* Once k_sem_take() returns, we know one permit was consumed.
		 * At this point the consumer can safely continue its work.
		 */
		LOG_INF("consumer: semaphore acquired, doing work");

		/* Simulate some real work so you can see the log ordering clearly. */
		k_sleep(K_MSEC(500));
	}
}

static void sem_producer_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	int tick = 0;

	while (1) {
		/* Sleep first so the consumer has time to block on the semaphore.
		 * This makes the behavior easy to observe in the logs.
		 */
		k_sleep(K_SECONDS(3));

		tick++;

		/* Give the semaphore once.
		 *
		 * If the consumer is blocked in k_sem_take(), it will be woken up.
		 * If no thread is waiting, the semaphore count becomes 1.
		 * Because the limit is 1, repeated gives do not accumulate beyond 1.
		 */
		LOG_INF("producer: giving semaphore (%d)", tick);
		k_sem_give(&sem_demo_sem);
	}
}

/* K_THREAD_DEFINE creates and starts a thread automatically.
 *
 * The thread starts as soon as the kernel finishes initialization.
 * This is convenient for learning demos because you do not need to call
 * k_thread_create() manually in main().
 */
K_THREAD_DEFINE(sem_consumer_tid, 512, sem_consumer_thread,
		NULL, NULL, NULL, 7, 0, 0);

K_THREAD_DEFINE(sem_producer_tid, 512, sem_producer_thread,
		NULL, NULL, NULL, 8, 0, 0);
