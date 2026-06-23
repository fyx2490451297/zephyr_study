#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(kmsq_demo, LOG_LEVEL_INF);

/* --------------------------------------------------------------------------
 * Message Queue (k_msgq) learning notes
 * --------------------------------------------------------------------------
 * A message queue in Zephyr is a FIFO buffer that allows threads to exchange
 * fixed-size data items safely across thread boundaries.
 *
 * Core concepts:
 *   1. Item size (msg_size):
 *      Every message stored in the queue has exactly this many bytes.
 *      The queue copies the data in and out by value, so the sender does not
 *      need to keep the original buffer alive after k_msgq_put() returns.
 *
 *   2. Maximum items (max_msgs):
 *      How many messages the queue can hold at the same time.
 *      If the queue is full, k_msgq_put() will block (or return -ENOMSG if
 *      called with K_NO_WAIT) until the consumer makes room.
 *
 *   3. Alignment:
 *      The queue's internal buffer is aligned to match the message struct.
 *      K_MSGQ_DEFINE handles this automatically.
 *
 * Key API:
 *   - k_msgq_put(&q, &msg, timeout) : enqueue one message (copy-in)
 *   - k_msgq_get(&q, &msg, timeout) : dequeue one message (copy-out)
 *   - k_msgq_purge(&q)              : discard all pending messages
 *   - k_msgq_num_used_get(&q)       : query how many messages are waiting
 *
 * Timeout values:
 *   - K_FOREVER  – block until the operation can complete
 *   - K_NO_WAIT  – return immediately with an error if it would block
 *   - K_MSEC(n)  – block at most n milliseconds
 *
 * In this demo:
 *   - The producer enqueues a new message every 2 seconds.
 *   - The queue holds up to 4 messages, so bursts are absorbed gracefully.
 *   - The consumer dequeues and logs each message, then sleeps 3 seconds
 *     to simulate slower processing.  This lets the queue fill up so you
 *     can observe blocking on the producer side once the queue is full.
 *
 * This is a good follow-up demo after semaphores because it shows:
 *   - how to pass data (not just a signal) between threads
 *   - FIFO ordering guarantees
 *   - backpressure when the consumer is slower than the producer
 * --------------------------------------------------------------------------
 */

/* Define the message structure that is exchanged between threads.
 *
 * Using a struct (rather than a raw integer) is the typical pattern because
 * real applications usually need to pass multiple related fields at once.
 */
struct kmsq_msg {
	uint32_t id;      /* monotonically increasing sequence number       */
	uint32_t value;   /* arbitrary payload, here simulated sensor data  */
};

/* K_MSGQ_DEFINE(name, msg_size, max_msgs, align)
 *
 * Statically allocates the queue and its internal ring-buffer storage.
 * - msg_size  = sizeof(struct kmsq_msg)  so each slot is exactly one item
 * - max_msgs  = 4  so up to four items can be buffered between threads
 * - align     = 4  matches the natural alignment of the struct (uint32_t)
 */
K_MSGQ_DEFINE(kmsq_demo_queue, sizeof(struct kmsq_msg), 4, 4);

/* --------------------------------------------------------------------------
 * Producer thread
 *
 * Generates a new message every 2 seconds and enqueues it.
 * If the queue is full (consumer fell behind), k_msgq_put() blocks here
 * until the consumer dequeues at least one item.
 * --------------------------------------------------------------------------
 */
static void kmsq_producer_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	uint32_t seq = 0;

	while (1) {
		/* Build the outgoing message on the stack.
		 *
		 * The queue makes a copy, so this local variable can be reused
		 * safely in the next iteration without any shared-memory concern.
		 */
		struct kmsq_msg msg = {
			.id    = seq,
			.value = 1000U + (seq % 100U), /* simulated sensor reading */
		};

		/* k_msgq_put() copies msg into the queue's ring buffer.
		 *
		 * K_FOREVER: if the queue is full, this call blocks until the
		 * consumer calls k_msgq_get() and frees a slot.
		 *
		 * num_used_get() is called only for logging; it is not required
		 * for correct operation.
		 */
		LOG_INF("producer: enqueue id=%u value=%u  (used=%u/4)",
			msg.id, msg.value,
			k_msgq_num_used_get(&kmsq_demo_queue));

		k_msgq_put(&kmsq_demo_queue, &msg, K_FOREVER);

		seq++;

		/* Produce one message every 2 seconds. */
		k_sleep(K_SECONDS(2));
	}
}

/* --------------------------------------------------------------------------
 * Consumer thread
 *
 * Waits for a message to arrive in the queue, then processes it.
 * The consumer sleeps 3 seconds after each message to simulate a slower
 * worker, which will cause the queue to fill up and block the producer.
 * --------------------------------------------------------------------------
 */
static void kmsq_consumer_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	struct kmsq_msg msg;

	while (1) {
		/* k_msgq_get() copies one item from the queue into our local msg.
		 *
		 * K_FOREVER: block until at least one message is available.
		 * When this call returns 0, msg holds a valid copy of the data.
		 */
		k_msgq_get(&kmsq_demo_queue, &msg, K_FOREVER);

		LOG_INF("consumer: dequeue id=%u value=%u  (used=%u/4)",
			msg.id, msg.value,
			k_msgq_num_used_get(&kmsq_demo_queue));

		/* Simulate slow processing (3 s) so the queue gradually fills up,
		 * letting you observe the producer blocking once all 4 slots are used.
		 */
		k_sleep(K_SECONDS(3));
	}
}

/* K_THREAD_DEFINE creates and auto-starts both threads at boot.
 *
 * Producer priority CONFIG_PACKAGE_KMSQ_DEMO_PRODUCER_PRIORITY (default 8).
 * Consumer priority CONFIG_PACKAGE_KMSQ_DEMO_CONSUMER_PRIORITY (default 9,
 * slightly lower so the producer runs first when both are ready, which makes
 * the fill-up behaviour easier to observe).
 *
 * Stack size is shared and controlled via Kconfig.
 */
K_THREAD_DEFINE(kmsq_producer_tid,
		CONFIG_PACKAGE_KMSQ_DEMO_STACK_SIZE,
		kmsq_producer_thread,
		NULL, NULL, NULL,
		CONFIG_PACKAGE_KMSQ_DEMO_PRODUCER_PRIORITY, 0, 0);

K_THREAD_DEFINE(kmsq_consumer_tid,
		CONFIG_PACKAGE_KMSQ_DEMO_STACK_SIZE,
		kmsq_consumer_thread,
		NULL, NULL, NULL,
		CONFIG_PACKAGE_KMSQ_DEMO_CONSUMER_PRIORITY, 0, 0);
