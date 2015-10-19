#ifndef __LIGHT_EVENT_H
#define __LIGHT_EVENT_H

/*
 * Define time interval (ms)
 */
#define TIME_INTERVAL  200
#define NOISE 10
#define WINDOW 20

struct event {
	int eid;
	int req_intensity;
	int frequency;
	/* this contains its own lock */
	wait_queue_head_t *waiting_tasks;
	struct list_head event_list;
	atomic_t run_flag;
	atomic_t ref_count;
};

struct light_intensity {
	int cur_intensity;
};

struct event_requirements {
	int req_intensity;
	int frequency;
};

int set_light_intensity(struct light_intensity *user_light_intensity);
int get_light_intensity(struct light_intensity *user_light_intensity);
int light_evt_create(struct event_requirements *intensity_params);
int light_evt_wait(int event_id);
int light_evt_signal(struct light_intensity *user_light_intensity);
int light_evt_destroy(int event_id);

#endif

