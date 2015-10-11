#include <linux/syscalls.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/light.h>


static struct light_intensity intensity = {0};

SYSCALL_DEFINE1(set_light_intensity, struct light_intensity __user *, user_light_intensity)
{
	if (user_light_intensity == NULL)
		return -EINVAL;
	if (copy_from_user(&intensity, user_light_intensity, sizeof(struct light_intensity)) != 0)
		return -EINVAL;
	return 0;
}

SYSCALL_DEFINE1(get_light_intensity, struct light_intensity __user *, user_light_intensity)
{
	if (user_light_intensity == NULL)
		return -EINVAL;
	if (copy_to_user(user_light_intensity, &intensity, sizeof(struct light_intensity)) != 0)
		return -EINVAL;
	return 0;
}