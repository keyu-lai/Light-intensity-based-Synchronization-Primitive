#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include "light.h"

int main (void)
{
	struct light_intensity buf;
	struct event_requirements event;
	int eid;
	int i;
	/* Write your test program here */
	while (0) {
		syscall(379, &buf);
		printf("%d\n", buf.cur_intensity);
		usleep(300000);
	}
	event.req_intensity = 300000;
	event.frequency = 12;
	eid = syscall(380, &event);
	printf("%d\n", eid);
	/*event.frequency = 15;
	eid = syscall(380, &event);
	printf("%d\n", eid);
	event.frequency = 12;
	eid = syscall(380, &event);
	printf("%d\n", eid);
	eid = syscall(380, &event);
	printf("%d\n", eid);
	buf.cur_intensity = 10000;
	for (i = 0; i < 21; ++i) {
		buf.cur_intensity = 10000+i;
		syscall(382, &buf);
	}*/
	i = syscall(381, eid);
	printf("%d\n", i);
	return 0;
}
