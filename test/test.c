#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include "light.h"

int main (void)
{
	struct light_intensity buf;
	/* Write your test program here */
	while (1) {
		syscall(379, &buf);
		printf("%d\n", buf.cur_intensity);
		usleep(300000);
	}
	return 0;
}
