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

static const int LOW_INTENSITY = 500;
static const int MEDIUM_INTENSITY = 5000;
static const int HIGH_INTENSITY = 20000;
static const int NO_INT_CAT = 3;
static const int NUM_OF_CHILDREN = 10;
static const int FREQUENCY = 7;

static void create_child(int,int);
	
int main (void)
{
	int i=0, j=0, iindex=0, eindex = 0;
	int earr[NO_INT_CAT], iarr[NO_INT_CAT];
	/* Write your test program here */
	iarr[iindex++] = LOW_INTENSITY;
	iarr[iindex++] = MEDIUM_INTENSITY;
	iarr[iindex++] = HIGH_INTENSITY;
	
	for (i = 0; i < iindex;i++) {
		struct event_requirements event = {iarr[i],FREQUENCY};
		printf("Create event for %d !\n",iarr[i]);
		int eid = syscall(__NR_light_evt_create,&event);
		if(eid == 0)
			printf("Error !!\n");
		
		if (eid <= 0) {
			printf("error %s\n",strerror(errno));
			exit(EXIT_FAILURE);
		}
		
		earr[eindex++] = eid;
	}
	
	for(i=0;i < eindex;i++) 
		for(j=0; j < NUM_OF_CHILDREN;j++)
			create_child(earr[i],iarr[i]);
	
	int sleep_time = 5, des_int = 60, time_pass = 0; //in milisecond
	
	while (1) {		
		usleep(sleep_time * 1000000);
		time_pass += sleep_time;

		if (time_pass >= des_int) {
			for (i=0; i < eindex; i++) {
				//destory all event
				printf("Destroying event %d \n",earr[i]);
				syscall(__NR_light_evt_destroy ,earr[i]);
				printf("Destroyed event %d \n",earr[i]);
			}
				
			time_pass = 0;
			break;
		}
	}
	
	//wait for all children to exit
	while (wait(0) > 0) {
		printf("wait for child to end !\n");
	};
	
	return 0;
}

static void create_child(int event_id, int light_intensity) {
	pid_t cid;
	
	cid = fork();
	if (cid < 0) {
		printf("error %s \n",strerror(errno));
		exit(EXIT_FAILURE);
	}
	else if(cid == 0) {
		pid_t cur_pid = getpid();
		printf("%d is waiting for %d with  intensity %d \n", cur_pid, event_id,light_intensity);
		
		int status = syscall(__NR_light_evt_wait, event_id);
		
		if (status == 0) {
			if (light_intensity >= HIGH_INTENSITY)
				printf("%d detected a high intensity event\n",cur_pid);
		
			if (light_intensity >= MEDIUM_INTENSITY && light_intensity < HIGH_INTENSITY)
				printf("%d detected a medium intensity event\n",cur_pid);
		
			if (light_intensity < MEDIUM_INTENSITY)
				printf("%d detected a low intensity event\n",cur_pid);
			
			printf("%d quits because it finishes waiting %d event \n",cur_pid, event_id);
		}
		
		if (status < 0) {
			if (errno == EINTR)
				printf("%d quits because event %d is destroyed\n",cur_pid, event_id);
			else
				printf("error %s\n",strerror(errno));

		}
			
		exit(0);
	}
		
}