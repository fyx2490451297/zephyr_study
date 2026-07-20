#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(work_queue_demo, LOG_LEVEL_INF);

/* --------------------------------------------------------------------------
 * Work queue (struct k_work_q / k_work / k_work_delayable) learning notes
 * --------------------------------------------------------------------------
 * A workqueue is a dedicated thread that executes "work items" (functions)
 * pulled from an internal FIFO, one at a time, in the order they were
 * submitted. It decouples "who requests the work" from "who executes it":
 *
 *   - Any thread (or even an ISR) can submit a k_work item without blocking.
 *   - The workqueue's own thread picks up submitted items and runs their
 *     handler function in normal thread context (safe to sleep, take
 *     mutexes, call blocking APIs, etc. - which an ISR cannot do directly).
 *
 * Key objects used in this demo:
 *   1. struct k_work_q:
 *      A workqueue instance, backed by its own dedicated thread. Zephyr also
 *      provides a built-in "system workqueue" (k_sys_work_q) shared by the
 *      whole system, but here we create our OWN dedicated queue with
 *      k_work_queue_init()/k_work_queue_start() so demo work never competes
 *      with other subsystems that use the system workqueue.
 *   2. struct k_work:
 *      A single one-shot work item. k_work_init() binds it to a handler
 *      function. k_work_submit_to_queue() enqueues it for execution exactly
 *      once (submitting again while already pending is a no-op).
 *   3. struct k_work_delayable:
 *      A work item that first waits for a timeout before becoming ready to
 *      run, submitted with k_work_schedule_for_queue(). This demo's handler
 *      re-schedules itself at the end, producing periodic execution -
 *      similar to a repeating timer, but the callback body runs in a real
 *      thread context on the workqueue instead of ISR/timer context.
 *
 * In this demo:
 *   - a dedicated workqueue ("demo_work_q") runs on its own thread/priority
 *   - a trigger thread submits a one-shot k_work item every 2 seconds
 *   - a k_work_delayable item is scheduled once at startup and then
 *     re-schedules itself every 1 second, demonstrating periodic work
 * --------------------------------------------------------------------------
 */

/* 1. Dedicated workqueue thread. Its stack must be defined explicitly. */
static struct k_work_q demo_work_q;
K_THREAD_STACK_DEFINE(demo_work_q_stack, CONFIG_PACKAGE_WORK_QUEUE_DEMO_STACK_SIZE);

/* 2. One-shot work item and its handler. */
static void one_shot_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	/* This function runs on the demo_work_q thread, NOT on the thread
	 * that called k_work_submit_to_queue(). It is safe to do blocking
	 * work here (e.g. k_sleep, i2c transfers, etc.).
	 */
	LOG_INF("one_shot_work: handling submitted work item");
}

static struct k_work one_shot_work;

/* 3. Periodic (delayable) work item and its handler. */
static void periodic_work_handler(struct k_work *work)
{
	/* k_work_delayable embeds a struct k_work, so we can recover the
	 * outer k_work_delayable pointer with CONTAINER_OF.
	 */
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);

	LOG_INF("periodic_work: tick");

	/* Re-submit ourselves after 1 second to simulate a periodic task.
	 * Using the work item's own queue keeps it running on demo_work_q.
	 */
	k_work_schedule_for_queue(&demo_work_q, dwork, K_SECONDS(1));
}

static K_WORK_DELAYABLE_DEFINE(periodic_work, periodic_work_handler);

static void work_queue_trigger_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		k_sleep(K_SECONDS(2));

		/* k_work_submit_to_queue() never blocks the caller. It just
		 * links the work item onto the target queue's internal FIFO
		 * and wakes the queue's thread if it is idle.
		 *
		 * If the item is already pending (queued or running), this
		 * call is simply ignored - a k_work item can only be queued
		 * once at a time.
		 */
		LOG_INF("trigger: submitting one-shot work item");
		k_work_submit_to_queue(&demo_work_q, &one_shot_work);
	}
}

K_THREAD_STACK_DEFINE(trigger_thread_stack, CONFIG_PACKAGE_WORK_QUEUE_DEMO_STACK_SIZE);
static struct k_thread trigger_thread_data;

static int work_queue_demo_init(void)
{
	/* Start the dedicated workqueue thread. */
	k_work_queue_init(&demo_work_q);
	k_work_queue_start(&demo_work_q, demo_work_q_stack,
			    K_THREAD_STACK_SIZEOF(demo_work_q_stack),
			    CONFIG_PACKAGE_WORK_QUEUE_DEMO_PRIORITY, NULL);
	k_thread_name_set(&demo_work_q.thread, "demo_work_q");

	/* Bind the one-shot work item to its handler. */
	k_work_init(&one_shot_work, one_shot_work_handler);

	/* Kick off the periodic work once; from then on it reschedules itself. */
	k_work_schedule_for_queue(&demo_work_q, &periodic_work, K_SECONDS(1));

	/* Start the trigger thread that periodically submits the one-shot work. */
	k_thread_create(&trigger_thread_data, trigger_thread_stack,
			 K_THREAD_STACK_SIZEOF(trigger_thread_stack),
			 work_queue_trigger_thread, NULL, NULL, NULL,
			 CONFIG_PACKAGE_WORK_QUEUE_DEMO_TRIGGER_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(&trigger_thread_data, "work_q_trigger");

	return 0;
}

/* Run after the kernel is up so the workqueue and trigger thread start
 * automatically without any manual call from main().
 */
SYS_INIT(work_queue_demo_init, APPLICATION, CONFIG_PACKAGE_WORK_QUEUE_DEMO_INIT_PRIORITY);
