#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>

#define DEVICE_NAME "/dev/btn_d0"
#define CMD_LED_ON	1
#define CMD_LED_OFF 	0

int main(void)
{
	int btn_led_fd;
	char buttons[8] = {'0', '0', '0', '0', '0', '0', '0', '0'};
	btn_led_fd = open(DEVICE_NAME, O_RDWR);
	if (btn_led_fd < 0) {
		perror("open device buttons");
		exit(1);
	}

	while(1)
	{
		int i;
		if (read(btn_led_fd, buttons, sizeof buttons) < 0) {
			perror("read buttons:");
			exit(1);
		}
		for (i = 0 ; i < sizeof(buttons) / sizeof(buttons[0]); i++)
		{
			printf ("%c\t",buttons[i]);
		}
		printf("\n");
	}

	close(btn_led_fd);
	return 0;
}
