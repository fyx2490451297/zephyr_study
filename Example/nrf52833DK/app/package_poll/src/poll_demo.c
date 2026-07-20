#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(poll_demo, LOG_LEVEL_INF);

/* --------------------------------------------------------------------------
 * Poll (struct k_poll_event / k_poll_signal / k_poll()) learning notes
 * --------------------------------------------------------------------------
 * k_poll() lets a single thread block on SEVERAL, POTENTIALLY DIFFERENT
 * kernel objects at the same time, and wake up as soon as ANY one of them
 * becomes ready. This is something none of the single-object primitives
 * (semaphore, FIFO, event) can do by themselves:
 *   - k_sem_take() can only wait on one semaphore.
 *   - k_event only multiplexes bits within ONE event object.
 *   - k_poll() can watch a semaphore, a FIFO/queue, a message queue, AND a
 *     dedicated "poll signal" object all in the same call.
 *
 * Key objects used in this demo:
 *   1. struct k_poll_event:
 *      One entry in the array passed to k_poll(). Each entry is initialized
 *      with a type (what kind of object/condition to watch), a mode
 *      (K_POLL_MODE_NOTIFY_ONLY is the only mode currently supported), and
 *      a pointer to the actual kernel object to watch.
 *   2. struct k_poll_signal:
 *      A lightweight, application-defined "event" object with no built-in
 *      meaning - unlike a semaphore, it carries an arbitrary signed 32-bit
 *      "result" value set by the raiser and read by the poller. It must be
 *      reset with k_poll_signal_reset() before it can be waited on again.
 *   3. k_poll():
 *      Blocks the calling thread until at least one watched event's
 *      condition is satisfied (or the timeout expires). After it returns,
 *      the caller must inspect event.state on each entry to find out which
 *      object(s) became ready, then reset that entry's state and/or the
 *      underlying object.
 *
 * In this demo:
 *   - a semaphore ("sem_source") is given every 2 seconds by one thread
 *   - a poll signal ("signal_source") is raised every 3 seconds by another
 *     thread, carrying an incrementing counter as its "result" value
 *   - the poller thread calls k_poll() once with BOTH watched together and
 *     reports which one triggered (and the signal's payload, if any)
 * --------------------------------------------------------------------------
 */

/* Semaphore watched by the poller. limit=1: behaves like a simple flag. */
K_SEM_DEFINE(sem_source, 0, 1);

/* Poll signal watched by the poller. Carries an arbitrary result value. */
K_POLL_SIGNAL_DEFINE(signal_source);

static void poll_poller_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	/* Describe what we want to watch. Each k_poll_event entry is bound to
	 * exactly one kernel object and one "ready" condition type:
	 *   - K_POLL_TYPE_SEM_AVAILABLE fires when the semaphore's count > 0
	 *   - K_POLL_TYPE_SIGNAL fires when k_poll_signal_raise() is called
	 */
	struct k_poll_event events[2];

	while (1) {
		/* k_poll_event_init() must be called again before every
		 * k_poll() call for entries whose state was consumed, since
		 * k_poll() mutates the .state field in place.
		 */
		k_poll_event_init(&events[0], K_POLL_TYPE_SEM_AVAILABLE,
				   K_POLL_MODE_NOTIFY_ONLY, &sem_source);
		k_poll_event_init(&events[1], K_POLL_TYPE_SIGNAL,
				   K_POLL_MODE_NOTIFY_ONLY, &signal_source);

		LOG_INF("poller: waiting on semaphore OR signal...");

		/* Blocks until ANY of the events[] entries becomes ready, or
		 * the timeout (K_FOREVER here) expires. Returns 0 on success.
		 */
		int ret = k_poll(events, ARRAY_SIZE(events), K_FOREVER);

		if (ret != 0) {
			LOG_WRN("poller: k_poll() returned %d", ret);
			continue;
		}

		/* Inspect each entry's state to find out which object(s)
		 * actually triggered this wake-up. Both could be ready at
		 * once if their timing coincides.
		 */
		if (events[0].state == K_POLL_STATE_SEM_AVAILABLE) {
			/* Consume the permit so the semaphore count goes back to 0. */
			k_sem_take(&sem_source, K_NO_WAIT);
			LOG_INF("poller: woken by SEMAPHORE (count consumed)");
		}

		if (events[1].state == K_POLL_STATE_SIGNALED) {
			unsigned int signaled;
			int result;

			/* Read the payload the raiser attached to the signal. */
			k_poll_signal_check(&signal_source, &signaled, &result);
			LOG_INF("poller: woken by SIGNAL (result=%d)", result);

			/* A poll signal stays "signaled" until explicitly reset. */
			k_poll_signal_reset(&signal_source);
		}
	}
}

static void poll_sem_source_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		k_sleep(K_SECONDS(2));

		LOG_INF("sem source: giving semaphore");
		k_sem_give(&sem_source);
	}
}

static void poll_signal_source_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	int counter = 0;

	while (1) {
		k_sleep(K_SECONDS(3));

		counter++;

		/* k_poll_signal_raise() sets the signal to "signaled" and
		 * stores the given result value, then wakes any thread
		 * blocked in k_poll() on this signal.
		 */
		LOG_INF("signal source: raising signal (result=%d)", counter);
		k_poll_signal_raise(&signal_source, counter);
	}
}

/* K_THREAD_DEFINE creates and starts a thread automatically.
 *
 * The thread starts as soon as the kernel finishes initialization.
 * This is convenient for learning demos because you do not need to call
 * k_thread_create() manually in main().
 */
K_THREAD_DEFINE(poll_poller_tid,
		CONFIG_PACKAGE_POLL_DEMO_STACK_SIZE,
		poll_poller_thread,
		NULL, NULL, NULL,
		CONFIG_PACKAGE_POLL_DEMO_POLLER_PRIORITY, 0, 0);

K_THREAD_DEFINE(poll_sem_source_tid,
		CONFIG_PACKAGE_POLL_DEMO_STACK_SIZE,
		poll_sem_source_thread,
		NULL, NULL, NULL,
		CONFIG_PACKAGE_POLL_DEMO_SOURCE_PRIORITY, 0, 0);

K_THREAD_DEFINE(poll_signal_source_tid,
		CONFIG_PACKAGE_POLL_DEMO_STACK_SIZE,
		poll_signal_source_thread,
		NULL, NULL, NULL,
		CONFIG_PACKAGE_POLL_DEMO_SOURCE_PRIORITY, 0, 0);
