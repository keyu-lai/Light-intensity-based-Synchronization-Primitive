#ifndef __LIGHT_EVENT_H
#define __LIGHT_EVENT_H

/*
 *Define time interval (ms)
 */
#define TIME_INTERVAL  200
#define NOISE 10
#define WINDOW 20


/*
 * The data structure for passing light intensity data to the
 * kernel and storing the data in the kernel.
 */
struct light_intensity {
	int cur_intensity; /* scaled intensity as read from the light sensor */
}; 

/*
 * Defines a light event.
 *
 * Event is defined by a required intensity and frequency.
 */
struct event_requirements {
    int req_intensity; /* scaled value of light intensity in centi-lux */
    int frequency;     /* number of samples with 
    					intensity-noise > req_intensity */
};
     
/*
 * Set current ambient intensity in the kernel.
 *
 * The parameter user_light_intensity is the pointer to the address
 * where the sensor data is stored in user space. Follow system call
 * convention to return 0 on success and the appropriate error value
 * on failure.
 *
 * syscall number 378
 */
int set_light_intensity(struct light_intensity * user_light_intensity);

/*
 * Retrive the scaled intensity set in the kernel.
 *
 * The same convention as the previous system call but
 * you are reading the value that was just set. 
 * Handle error cases appropriately and return values according to convention.
 * The calling process should provide memory in userspace to return the intensity.
 *
 * syscall number 379
 */
int get_light_intensity(struct light_intensity * user_light_intensity);


/*
 * Create an event based on light intensity.
 *
 * If frequency exceeds WINDOW, cap it at WINDOW.
 * Return an event_id on success and the appropriate error on failure.
 *
 * system call number 380
 */
 int light_evt_create(struct event_requirements * intensity_params);
 

/*
 * Block a process on an event.
 *
 * It takes the event_id as parameter. The event_id requires verification.
 * Return 0 on success and the appropriate error on failure.
 *
 * system call number 381
 */
 int light_evt_wait(int event_id);
 
 
/*
 * The light_evt_signal system call.
 *
 * Takes sensor data from user, stores the data in the kernel,
 * and notifies all open events whose
 * baseline is surpassed.  All processes waiting on a given event 
 * are unblocked.
 *
 * Return 0 success and the appropriate error on failure.
 *
 * system call number 382
 */
 int light_evt_signal(struct light_intensity * user_light_intensity);


/*
 * Destroy an event using the event_id.
 *
 * Return 0 on success and the appropriate error on failure.
 *
 * system call number 383
 */
 int light_evt_destroy(int event_id);

 #endif