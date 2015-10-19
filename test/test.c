#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include "light.h"

#define __NR_get_light_intensity 379
#define __NR_light_evt_create 380
#define __NR_light_evt_wait 381
#define __NR_light_evt_signal 382
#define __NR_light_evt_destroy 383

static const int LOW_INTENSITY = 5000;
static const int MEDIUM_INTENSITY = 20000;
static const int HIGH_INTENSITY = 100000;
static const int NO_INT_CAT = 3;
static const int NUM_OF_CHILDREN = 10;
static const int FREQUENCY = 7;

static void create_child(int event_id, int light_intensity)
{
	pid_t cid;

	cid = fork();
	if (cid < 0) {
		printf("error: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (cid > 0)
		return;

	pid_t cur_pid = getpid();

	int status = syscall(__NR_light_evt_wait, event_id);

	if (status == 0) {
		if (light_intensity >= HIGH_INTENSITY)
			printf("%d detected a high intensity event\n", cur_pid);

		if (light_intensity >= MEDIUM_INTENSITY &&
			light_intensity < HIGH_INTENSITY)
			printf("%d detected a medium intensity event\n",
				cur_pid);

		if (light_intensity < MEDIUM_INTENSITY)
			printf("%d detected a low intensity event\n", cur_pid);
	}

	if (status < 0)
		printf("error: %s\n", strerror(errno));

	exit(EXIT_SUCCESS);
}

int main (void)
{
	int i = 0, j = 0, iindex = 0, eindex = 0;
	int earr[NO_INT_CAT], iarr[NO_INT_CAT];
	int des_int = 60, time_pass = 0; /* in miliseconds */

	iarr[iindex++] = LOW_INTENSITY;
	iarr[iindex++] = MEDIUM_INTENSITY;
	iarr[iindex++] = HIGH_INTENSITY;

	for (i = 0; i < iindex; i++) {
		struct event_requirements event = {iarr[i], FREQUENCY};
		int eid = syscall(__NR_light_evt_create, &event);

		if (eid < 0) {
			printf("error: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		earr[eindex++] = eid;
	}

	for (i = 0; i < eindex; i++)
		for (j = 0; j < NUM_OF_CHILDREN; j++)
			create_child(earr[i], iarr[i]);

	while (1) {
		usleep(1000000);
		time_pass += 1;

		if (time_pass >= des_int)
			break;
	}
	/* destroy all events */
	for (i = 0; i < eindex; i++)
		syscall(__NR_light_evt_destroy, earr[i]);

	/* wait for all children to exit */
	while (wait(0) > 0)
		;

	return 0;
}

