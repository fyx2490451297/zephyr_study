#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(fifo_demo, LOG_LEVEL_INF);

/* --------------------------------------------------------------------------
 * FIFO (struct k_fifo) learning notes
 * --------------------------------------------------------------------------
 * A k_fifo is a queue of variable-size data items shared between threads.
 * Unlike a semaphore (which only signals "an event happened"), a FIFO also
 * carries actual data from the producer to the consumer.
 *
 * Key characteristics:
 *   1. Item format:
 *      Every item pushed into a k_fifo MUST reserve its first machine word
 *      for the kernel's internal single-linked-list pointer. The easiest
 *      way to guarantee this is to put a "void *fifo_reserved;" field (or a
 *      sys_snode_t) as the FIRST member of the item struct.
 *   2. put / get:
 *      - k_fifo_put(): push one item onto the tail of the queue (never blocks)
 *      - k_fifo_get(): pop one item from the head of the queue (can block)
 *   3. Ordering:
 *      Items come out in first-in-first-out order, one at a time.
 *   4. Ownership:
 *      k_fifo itself does not manage memory. The producer must allocate the
 *      item (here we use a static ring/pool for simplicity) and the
 *      consumer is responsible for the item once it is popped.
 *
 * In this demo:
 *   - the producer builds a small "message" item every 2 seconds and pushes
 *     it into the FIFO with k_fifo_put()
 *   - the consumer blocks on k_fifo_get(..., K_FOREVER) and prints every
 *     message it receives, in the same order it was produced
 *
 * This demo highlights the difference from a semaphore: the consumer does
 * not just wake up, it actually receives the data that was produced.
 * --------------------------------------------------------------------------
 */

/* A FIFO data item. The first member must be reserved for the kernel's
 * internal linked-list bookkeeping; user data follows after it.
 */
struct fifo_demo_item {
	void *fifo_reserved; /* required by k_fifo, must be first member */
	int id;
	int value;
};

/* Simple static item pool so the demo does not need a heap allocator.
 * Real applications typically use k_malloc()/k_free() or a k_mem_slab
 * instead of a plain static array.
 */
#define FIFO_DEMO_POOL_SIZE 4
static struct fifo_demo_item item_pool[FIFO_DEMO_POOL_SIZE];

K_FIFO_DEFINE(fifo_demo_fifo);

static void fifo_consumer_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		/* Block forever until the producer pushes an item.
		 *
		 * k_fifo_get() returns a pointer to the item that was put in,
		 * with the same address the producer used in k_fifo_put().
		 */
		LOG_INF("consumer: waiting for fifo item...");
		struct fifo_demo_item *item = k_fifo_get(&fifo_demo_fifo, K_FOREVER);

		/* Once k_fifo_get() returns, we own this item exclusively.
		 * Read the payload before the producer reuses the slot.
		 */
		LOG_INF("consumer: received item id=%d value=%d", item->id, item->value);

		/* Simulate some real work so you can see the log ordering clearly. */
		k_sleep(K_MSEC(300));
	}
}

static void fifo_producer_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	int tick = 0;

	while (1) {
		/* Sleep first so the consumer has time to block on the fifo.
		 * This makes the behavior easy to observe in the logs.
		 */
		k_sleep(K_SECONDS(2));

		/* Pick a slot from the static pool in round-robin order.
		 * NOTE: this simple demo assumes the consumer finishes with a
		 * slot before it is reused FIFO_DEMO_POOL_SIZE items later.
		 */
		struct fifo_demo_item *item = &item_pool[tick % FIFO_DEMO_POOL_SIZE];
		item->id = tick;
		item->value = tick * 10;
		tick++;

		/* Push the item onto the fifo.
		 *
		 * k_fifo_put() never blocks: it just links the item onto the
		 * queue and wakes up a thread blocked in k_fifo_get(), if any.
		 */
		LOG_INF("producer: pushing item id=%d value=%d", item->id, item->value);
		k_fifo_put(&fifo_demo_fifo, item);
	}
}

/* K_THREAD_DEFINE creates and starts a thread automatically.
 *
 * The thread starts as soon as the kernel finishes initialization.
 * This is convenient for learning demos because you do not need to call
 * k_thread_create() manually in main().
 */
K_THREAD_DEFINE(fifo_consumer_tid,
		CONFIG_PACKAGE_FIFO_DEMO_STACK_SIZE,
		fifo_consumer_thread,
		NULL, NULL, NULL,
		CONFIG_PACKAGE_FIFO_DEMO_CONSUMER_PRIORITY, 0, 0);

K_THREAD_DEFINE(fifo_producer_tid,
		CONFIG_PACKAGE_FIFO_DEMO_STACK_SIZE,
		fifo_producer_thread,
		NULL, NULL, NULL,
		CONFIG_PACKAGE_FIFO_DEMO_PRODUCER_PRIORITY, 0, 0);
