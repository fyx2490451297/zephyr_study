#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(event_demo, LOG_LEVEL_INF);

/* --------------------------------------------------------------------------
 * Event (struct k_event) learning notes
 * --------------------------------------------------------------------------
 * A k_event is a synchronization object that holds a 32-bit set of "event
 * bits". Unlike a semaphore (a single counter) or a FIFO (an ordered data
 * queue), an event lets a thread wait for a combination of independent
 * conditions, each represented by one bit.
 *
 * Key characteristics:
 *   1. Bits:
 *      Any subset of the 32 bits can be used as an application-defined
 *      condition. Multiple producers can set different bits independently.
 *   2. post / set:
 *      - k_event_post(): OR the given bits into the current event state and
 *        wake up any waiter whose condition is now satisfied. Bits that
 *        were already set stay set (accumulates).
 *      - k_event_set(): replace the event state with exactly the given bits
 *        (bits not in the mask are cleared).
 *   3. wait / wait_all:
 *      - k_event_wait(): wake up as soon as ANY of the requested bits are set
 *        ("OR" wait). Returns the bits that were set at wake-up time.
 *      - k_event_wait_all(): wake up only once ALL of the requested bits are
 *        set ("AND" wait).
 *      Both can automatically clear the consumed bits before returning if
 *      the caller passes reset=true.
 *
 * In this demo:
 *   - bit 0 (TEMPERATURE_READY) is posted by "sensor A" every 1 second
 *   - bit 1 (HUMIDITY_READY)    is posted by "sensor B" every 1.5 seconds
 *   - the waiter thread calls k_event_wait_all() for both bits together,
 *     so it only wakes up once BOTH sensors have produced fresh data,
 *     demonstrating the "AND" join behavior that a semaphore cannot express
 *     with a single object.
 * --------------------------------------------------------------------------
 */

/* Application-defined event bits. Any bit position 0-31 can be used. */
#define EVENT_TEMPERATURE_READY BIT(0)
#define EVENT_HUMIDITY_READY    BIT(1)
#define EVENT_ALL_SENSORS_READY (EVENT_TEMPERATURE_READY | EVENT_HUMIDITY_READY)

K_EVENT_DEFINE(sensor_event);

static void event_waiter_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		/* Block until BOTH the temperature and humidity bits are set.
		 *
		 * "reset=true" clears exactly EVENT_ALL_SENSORS_READY from the
		 * event state right before returning, so the next round starts
		 * from a clean state without racing the posters.
		 */
		LOG_INF("waiter: waiting for all sensors...");
		uint32_t events = k_event_wait_all(&sensor_event, EVENT_ALL_SENSORS_READY,
						    true, K_FOREVER);

		LOG_INF("waiter: all sensors ready! events=0x%02x", events);

		/* Simulate some real work so you can see the log ordering clearly. */
		k_sleep(K_MSEC(200));
	}
}

static void event_temperature_poster_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		k_sleep(K_SECONDS(1));

		/* k_event_post() ORs this bit into the event state and wakes
		 * any waiter whose full condition is now satisfied. It never
		 * blocks the poster.
		 */
		LOG_INF("sensor A: posting EVENT_TEMPERATURE_READY");
		k_event_post(&sensor_event, EVENT_TEMPERATURE_READY);
	}
}

static void event_humidity_poster_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		k_sleep(K_MSEC(1500));

		LOG_INF("sensor B: posting EVENT_HUMIDITY_READY");
		k_event_post(&sensor_event, EVENT_HUMIDITY_READY);
	}
}

/* K_THREAD_DEFINE creates and starts a thread automatically.
 *
 * The thread starts as soon as the kernel finishes initialization.
 * This is convenient for learning demos because you do not need to call
 * k_thread_create() manually in main().
 */
K_THREAD_DEFINE(event_waiter_tid,
		CONFIG_PACKAGE_EVENT_DEMO_STACK_SIZE,
		event_waiter_thread,
		NULL, NULL, NULL,
		CONFIG_PACKAGE_EVENT_DEMO_WAITER_PRIORITY, 0, 0);

K_THREAD_DEFINE(event_temperature_poster_tid,
		CONFIG_PACKAGE_EVENT_DEMO_STACK_SIZE,
		event_temperature_poster_thread,
		NULL, NULL, NULL,
		CONFIG_PACKAGE_EVENT_DEMO_POSTER_PRIORITY, 0, 0);

K_THREAD_DEFINE(event_humidity_poster_tid,
		CONFIG_PACKAGE_EVENT_DEMO_STACK_SIZE,
		event_humidity_poster_thread,
		NULL, NULL, NULL,
		CONFIG_PACKAGE_EVENT_DEMO_POSTER_PRIORITY, 0, 0);
