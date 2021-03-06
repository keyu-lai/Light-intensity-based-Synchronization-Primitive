/*
 * Columbia University
 * COMS W4118 Fall 2015
 * Homework 3
 *
 */
#include <bionic/errno.h> /* Google does things a little different...*/
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <hardware/hardware.h>
#include <hardware/sensors.h> /* <-- This is a good place to look! */
#include <errno.h>
#include <sys/stat.h>
#include "../flo-kernel/include/linux/akm8975.h" 
#include "light.h"

#define EMULATOR 0
#define DEVICE 1

#define LIGHT_INTENSITY_SENSOR 5

#define __NR_set_light_intensity 378
#define __NR_light_evt_signal 382

/* set to 1 for a bit of debug output */
#if 1
	#define dbg(fmt, ...) printf("Accelerometer: " fmt, ## __VA_ARGS__)
#else
	#define dbg(fmt, ...)
#endif

static int effective_sensor;
static int cur_device;

/* helper functions which you should use */
static int open_sensors(struct sensors_module_t **hw_module,
			struct sensors_poll_device_t **poll_device);
static void enumerate_sensors(const struct sensors_module_t *sensors);
static int poll_sensor_data_emulator(void);
static int poll_sensor_data(struct sensors_poll_device_t *sensors_device);

/* entry point: fill in daemon implementation
   where indicated */

void daemon_mode()
{
	/* Fill in daemon implementation here */
	pid_t pid, sid;

	pid = fork();
	if (pid < 0) {
		printf("error: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (pid > 0)
		exit(EXIT_SUCCESS);

	/* Change the file mode mask */
	umask(0);
	/* Create a new SID for the child process */
	sid = setsid();
	if (sid < 0) {
		printf("error: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	/* Change the current working directory */
	if ((chdir("/")) < 0) {
		/* Log any failure here */
		printf("error: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	/* Close out the standard file descriptors */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
}

int main(int argc, char **argv)
{
	effective_sensor = -1;
	cur_device = -1;
	if (argc != 2) {
		printf("Invalid arguments - use ./light_d [-e] [-d]\n");
		return EXIT_FAILURE;
	}

	if (strcmp(argv[1], "-e") == 0)
		cur_device = EMULATOR;
	else if (strcmp(argv[1], "-d") == 0)
		cur_device = DEVICE;
	else {
		printf ("Invalid arguments - use ./light_d [-e] [-d]\n");
		return EXIT_FAILURE;
	}

	struct sensors_module_t *sensors_module = NULL;
	struct sensors_poll_device_t *sensors_device = NULL;
	
	daemon_mode();
	/* Implement your code to make this process a daemon in daemon_mode function */

	printf("Opening sensors...\n");
	if (open_sensors(&sensors_module,
			 &sensors_device) < 0) {
		printf("open_sensors failed\n");
		return EXIT_FAILURE;
	}
	enumerate_sensors(sensors_module);

	/* Fill in daemon implementation around here */
	#define TIME_INTERVAL  1000
	printf("turn me into a daemon!\n");
	while (1) {
		poll_sensor_data(sensors_device);
		usleep(TIME_INTERVAL);
	}

	return EXIT_SUCCESS;
}

/*
 *You should send the intensity value to the kernel as soon as you read it
 * from the function below
 */

static int poll_sensor_data(struct sensors_poll_device_t *sensors_device)
{   
	const size_t numEventMax = 16;
	const size_t minBufferSize = numEventMax;
	sensors_event_t buffer[minBufferSize];
	ssize_t count = sensors_device->poll(sensors_device,
		buffer, minBufferSize);
	float cur_intensity = 0;
	struct light_intensity lig;
	int i;

	if (cur_device == DEVICE) {

		for (i = 0; i < count; ++i) {
			if (buffer[i].sensor != effective_sensor)
				continue;

			cur_intensity = buffer[i].light;
			lig.cur_intensity = cur_intensity * 100;
			if (syscall(__NR_set_light_intensity, &lig)) {
				printf("error: %s\n", strerror(errno));
				return EXIT_FAILURE;
			}
			if (syscall(__NR_light_evt_signal, &lig)) {
				printf("error: %s\n", strerror(errno));
				return EXIT_FAILURE;
			}
		}
	}

	else if (cur_device == EMULATOR) {

		/* Same thing again here - pretty bad hack for the emulator */
		/* Didn't know that the sensor simulator had only temperature but not light */		
		/* cur_intensity has a floating point value that you would have fed to */
		/* light_sensor binary */
		cur_intensity = poll_sensor_data_emulator();
		lig.cur_intensity = cur_intensity * 100;
		if (syscall(__NR_set_light_intensity, &lig)) {
			printf("error: %s\n", strerror(errno));
			return EXIT_FAILURE;
		}
		if (syscall(__NR_light_evt_signal, &lig)) {
			printf("error: %s\n", strerror(errno));
			return EXIT_FAILURE;
		}
	}

	return 0;
}



/*  DO NOT MODIFY BELOW THIS LINE  */
/*---------------------------------*/



static int poll_sensor_data_emulator(void)
{
	float cur_intensity;
	FILE *fp = fopen("/data/misc/intensity", "r");

	if (!fp)
		return 0;

	fscanf(fp, "%f", &cur_intensity);
	fclose(fp);
	return cur_intensity;
}




static int open_sensors(struct sensors_module_t **mSensorModule,
			struct sensors_poll_device_t **mSensorDevice)
{
   
	int err = hw_get_module(SENSORS_HARDWARE_MODULE_ID,
				     (hw_module_t const**)mSensorModule);

	if (err) {
		printf("couldn't load %s module (%s)",
			SENSORS_HARDWARE_MODULE_ID, strerror(-err));
	}

	if (!*mSensorModule)
		return -1;

	err = sensors_open(&((*mSensorModule)->common), mSensorDevice);

	if (err) {
		printf("couldn't open device for module %s (%s)",
			SENSORS_HARDWARE_MODULE_ID, strerror(-err));
	}

	if (!*mSensorDevice)
		return -1;

	const struct sensor_t *list;
	ssize_t count = (*mSensorModule)->get_sensors_list(*mSensorModule, &list);
	size_t i;
	for (i=0 ; i<(size_t)count ; i++)
		(*mSensorDevice)->activate(*mSensorDevice, list[i].handle, 1);

	return 0;
}

static void enumerate_sensors(const struct sensors_module_t *sensors)
{
	int nr, s;
	const struct sensor_t *slist = NULL;
	if (!sensors)
		printf("going to fail\n");

	nr = sensors->get_sensors_list((struct sensors_module_t *)sensors,
					&slist);
	if (nr < 1 || slist == NULL) {
		printf("no sensors!\n");
		return;
	}

	for (s = 0; s < nr; s++) {
		printf("%s (%s) v%d\n\tHandle:%d, type:%d, max:%0.2f, "
			"resolution:%0.2f \n", slist[s].name, slist[s].vendor,
			slist[s].version, slist[s].handle, slist[s].type,
			slist[s].maxRange, slist[s].resolution);

		/* Awful hack to make it work on emulator */
		if (slist[s].type == LIGHT_INTENSITY_SENSOR && slist[s].handle == LIGHT_INTENSITY_SENSOR)
			effective_sensor = LIGHT_INTENSITY_SENSOR; /*the sensor ID*/

		}
}
