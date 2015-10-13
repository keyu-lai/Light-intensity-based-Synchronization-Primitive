#include <linux/light.h>
#include <linux/rwsem.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <asm/atomic.h>
#include <linux/slab.h>

struct event {
	int req_intensity;
	int frequency;
	wait_queue_head_t *waiting_tasks; /* this contains its own lock */
	struct list_head event_list;
	atomic_t ref_count; /* given that light_evt_create and light_evt_destroy will be holding
						the eventlist_lock, I'm not sure this has to be an atomic_t. A
						regular int might work. But I'm not sure yet ... this will 
						become clear as we work on it */
};
struct list_head event_list;
static DECLARE_RWSEM(eventlist_lock); /* we could also use a simple mutex...? */

static int light_readings[WINDOW];
static int next_reading = 0;
static DECLARE_RWSEM(readings_buffer_lock); /* we could also use a simple mutex...? */


static struct event *create_event_descriptor(int req_intensity, int frequency) {
	struct event *new_event = kmalloc(sizeof(struct event), GFP_KERNEL);
	if (new_event == NULL)
		return NULL;
	new_event->req_intensity = req_intensity;
	new_event->frequency = frequency;
	new_event->waiting_tasks = kmalloc(sizeof(wait_queue_head_t), GFP_KERNEL);
	if (new_event->waiting_tasks == NULL) {
		kfree(new_event);
		return NULL;
	}
	init_waitqueue_head(new_event->waiting_tasks);
	atomic_set(&new_event->ref_count, 0);
	return new_event;
}
