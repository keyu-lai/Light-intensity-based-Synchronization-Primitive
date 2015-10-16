#include <linux/syscalls.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/light.h>
#include <linux/rwsem.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <asm/atomic.h>
#include <linux/slab.h>

#define MAX_INTENSITY 32768 * 100

struct event {
	int eid;
	int req_intensity;
	int frequency;
	wait_queue_head_t *waiting_tasks; /* this contains its own lock */
	struct list_head event_list;
	atomic_t run_flag;
	atomic_t ref_count; /* given that light_evt_create and light_evt_destroy will be holding
						the eventlist_lock, I'm not sure this has to be an atomic_t. A
						regular int might work. But I'm not sure yet ... this will 
						become clear as we work on it */
};

static struct light_intensity light_sensor = {0};
static DEFINE_RWLOCK(light_rwlock);

static struct event event_list_head = {
	.eid = 0,
	.req_intensity = 0,
	.frequency = 0,
	.waiting_tasks = NULL,
	.event_list = LIST_HEAD_INIT(event_list_head.event_list),
	.run_flag = {0},
	.ref_count = {0}
};
static DECLARE_RWSEM(eventlist_lock); /* we could also use a simple mutex...? */
static atomic_t eid = {0};

static struct event *create_event_descriptor(int req_intensity, int frequency) {
	

	struct event *new_event = kmalloc(sizeof(struct event), GFP_KERNEL);
	if (new_event == NULL)
		return NULL;
	new_event->eid = atomic_add_return(1, &eid);
	new_event->req_intensity = req_intensity;
	new_event->frequency = frequency;
	new_event->waiting_tasks = kmalloc(sizeof(wait_queue_head_t), GFP_KERNEL);
	if (new_event->waiting_tasks == NULL) {
		kfree(new_event);
		return NULL;
	}
	init_waitqueue_head(new_event->waiting_tasks);
	atomic_set(&new_event->run_flag, 0);
	atomic_set(&new_event->ref_count, 1);
	return new_event;
}

SYSCALL_DEFINE1(set_light_intensity, struct light_intensity __user *, user_light_intensity)
{

	if (user_light_intensity == NULL)
		return -EINVAL;
	write_lock(&light_rwlock);
	if (copy_from_user(&light_sensor, user_light_intensity, sizeof(struct light_intensity)) != 0)
		return -EINVAL;
	write_unlock(&light_rwlock);
	return 0;
}

SYSCALL_DEFINE1(get_light_intensity, struct light_intensity __user *, user_light_intensity)
{
	if (user_light_intensity == NULL)
		return -EINVAL;
	read_lock(&light_rwlock);
	if (copy_to_user(user_light_intensity, &light_sensor, sizeof(struct light_intensity)) != 0)
		return -EINVAL;
	read_unlock(&light_rwlock);
	return 0;
}

SYSCALL_DEFINE1(light_evt_create, struct event_requirements __user *, intensity_params)
{
	struct event *tmp;
	struct event_requirements request;
	int req_intensity;
	int frequency;

	if (copy_from_user(&request, intensity_params, sizeof(struct event_requirements)) != 0)
		return -EFAULT; 

	req_intensity = request.req_intensity;
	frequency = request.frequency;

	if (frequency <= 0 || req_intensity < 0 || req_intensity > MAX_INTENSITY)
		return -EINVAL;
	if (frequency > WINDOW)
		frequency = WINDOW;

	down_write(&eventlist_lock);

	tmp = create_event_descriptor(req_intensity, frequency);
	if (tmp == NULL) {
		up_write(&eventlist_lock);
		return -ENOMEM;
	}
		
	/* Seems like this process will not take a long time, we can consider using a spin lock.
	 * We will figure it out when we finish all the system calls.
	 */
	//down_write(&eventlist_lock);
	//printk("3333\n");
	list_add_tail(&tmp->event_list, &event_list_head.event_list);
	up_write(&eventlist_lock);
	//printk("4444\n");
	return tmp->eid;
}

SYSCALL_DEFINE1(light_evt_wait, int, event_id)
{
	struct event *tmp;

	/* Since wait_queue has its own lock, we don't need to acquire write lock. */
	down_read(&eventlist_lock);
	list_for_each_entry(tmp, &event_list_head.event_list, event_list) {
		if (tmp->eid == event_id) {
			up_read(&eventlist_lock);
			atomic_add(1, &tmp->ref_count);
			wait_event(*tmp->waiting_tasks, atomic_read(&tmp->run_flag));
			break;
		}
	}

	/* No such event. */
	if (tmp == &event_list_head) {
		up_read(&eventlist_lock);
		return -EINVAL;
	}
	return 0;
}

SYSCALL_DEFINE1(light_evt_signal, struct light_intensity __user *, user_light_intensity)
{
	static int light_readings[WINDOW];
	static int sorted_indices[WINDOW];
	static int next_reading = 0; 
	static int buffer_full = 0; 
	/* readings_buffer_lock locks both arrays and both index vars */
	/* do not use any of these 4 static vars above without acquiring lock */
	static DECLARE_RWSEM(readings_buffer_lock); 
	int i, added_light, removed_light;
	struct event *tmp;
	 
	if (copy_from_user(&added_light, &user_light_intensity->cur_intensity, sizeof(int)) != 0)
		return -EFAULT;

	/* I'm worried that this insertion sort is too complicated.
		It's a nice optimization, but we could just sort the indices
		with qsort */
	down_write(&readings_buffer_lock);
	removed_light = light_readings[next_reading];
	light_readings[next_reading++] = added_light;
	if (next_reading == WINDOW)
		next_reading = 0;
	if (!buffer_full && !next_reading)
		buffer_full = 1;

	if (!buffer_full) {
		/* insert sort */
		i = next_reading - 1;
		while (i > 0 && added_light > sorted_indices[i - 1]) {
			sorted_indices[i] = sorted_indices[i - 1];
			--i;
		}
		sorted_indices[i] = added_light;
		up_write(&readings_buffer_lock);
		return 0;
	}

	/* insert sort */
	for (i = 0; i < WINDOW; ++i) {
		if (sorted_indices[i] == removed_light)
			break;
	}
	while (i > 0 && added_light > sorted_indices[i - 1]) {
		sorted_indices[i] = sorted_indices[i - 1];
		--i;
	}
	sorted_indices[i] = added_light;
	while (i < WINDOW - 1 && added_light < sorted_indices[i + 1]) {
		sorted_indices[i] = sorted_indices[i + 1];
		++i;
	}
	sorted_indices[i] = added_light;

	/* Do we need a write lock? Because wait_queue has its own lock. */
	downgrade_write(&readings_buffer_lock);
	down_read(&eventlist_lock);
	list_for_each_entry(tmp, &event_list_head.event_list, event_list) {		
		if (tmp->eid != 0) {
			if (tmp->frequency == 0 || sorted_indices[tmp->frequency - 1] >= 
										tmp->req_intensity - NOISE) {
				atomic_set(&tmp->run_flag, 1);			
				atomic_set(&tmp->ref_count, 0);
				wake_up_all(tmp->waiting_tasks);
			}
			else
				atomic_set(&tmp->run_flag, 0); 		
		}
	}
	up_read(&eventlist_lock);
	up_read(&readings_buffer_lock);
	return 0;
}


SYSCALL_DEFINE1(light_evt_destroy, int, event_id)
{
	/* TODO: change this to use the correct behavior from
		Piazza, which is to destroy the event immediately 
		no matter how many are waiting, and notify them
		somehow 
	*/

	struct event *tmp;

	down_read(&eventlist_lock);
	list_for_each_entry(tmp, &event_list_head.event_list, event_list) {
		if (tmp->eid == event_id)
			break;
	}
	up_read(&eventlist_lock);

	/* No such event. */
	if (tmp == &event_list_head)
		return -EINVAL;

	if (atomic_read(&tmp->ref_count) == 0) {
		down_write(&eventlist_lock);
		list_del(&tmp->event_list);
		up_write(&eventlist_lock);
		kfree(tmp->waiting_tasks);
		kfree(tmp);
		/* TODO: free event itself and its wait queue */
		atomic_set(&tmp->run_flag, 1);			
		wake_up_all(tmp->waiting_tasks);
	}
	return 0;
}


