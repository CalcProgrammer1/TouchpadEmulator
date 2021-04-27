//  Touchpad Emulator
// Turn touchscreen into touchpad

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <linux/uinput.h>
#include <stdlib.h>
#include <unistd.h>

#define EVENT_TYPE	EV_ABS
#define EVENT_CODE_X	ABS_X
#define EVENT_CODE_Y	ABS_Y

void emit(int fd, int type, int code, int val)
{
	struct input_event ie;

	ie.type = type;
	ie.code = code;
	ie.value = val;
	ie.time.tv_sec = 0;
	ie.time.tv_usec = 0;

	write(fd, &ie, sizeof(ie));
}

int main()
{
	struct uinput_setup usetup;
	int i = 50;

	int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

	ioctl(fd, UI_SET_EVBIT, EV_KEY);
	ioctl(fd, UI_SET_KEYBIT, BTN_LEFT);
	ioctl(fd, UI_SET_KEYBIT, BTN_RIGHT);

	ioctl(fd, UI_SET_EVBIT, EV_REL);
	ioctl(fd, UI_SET_RELBIT, REL_X);
	ioctl(fd, UI_SET_RELBIT, REL_Y);

	memset(&usetup, 0, sizeof(usetup));

	usetup.id.bustype = BUS_USB;
	usetup.id.vendor  = 0x1234;
	usetup.id.product = 0x5678;
	strcpy(usetup.name, "Touchpad Emulator");

	ioctl(fd, UI_DEV_SETUP, &usetup);
	ioctl(fd, UI_DEV_CREATE);

	sleep(1);

	int touchscreen_fd = open("/dev/input/event2", O_RDONLY|O_NONBLOCK);

	ioctl(touchscreen_fd, EVIOCGRAB, 1);

	int prev_x = 0;
	int prev_y = 0;

	int init_prev_x = 0;
	int init_prev_y = 0;

	int fingers = 0;

	struct timeval time_active;
	struct timeval two_finger_time_active;

	while(1)
	{
		struct input_event touchscreen_event;

		//Read the touchscreen
		int ret = read(touchscreen_fd, &touchscreen_event, sizeof(touchscreen_event));

		if(ret > 0)
		{
			if(touchscreen_event.type == EV_KEY && touchscreen_event.value == 1 && touchscreen_event.code == BTN_TOUCH)
			{
				time_active = touchscreen_event.time;
				init_prev_x = 1;
				init_prev_y = 1;
				printf("key press\r\n");
			}
			if(touchscreen_event.type == EV_KEY && touchscreen_event.value == 0 && touchscreen_event.code == BTN_TOUCH)
			{
				printf("key release\r\n");
				struct timeval ret_time;
				timersub(&touchscreen_event.time, &time_active, &ret_time);
				if(ret_time.tv_sec == 0 && ret_time.tv_usec < 500000)
				{
					printf("click\r\n");
					emit(fd, EV_KEY, BTN_LEFT, 1);
					emit(fd, EV_SYN, SYN_REPORT, 0);
					emit(fd, EV_KEY, BTN_LEFT, 0);
				}
			}
			if(touchscreen_event.type == EV_ABS && touchscreen_event.code == ABS_MT_TRACKING_ID && touchscreen_event.value >= 0)
			{
				fingers++;

				if(fingers == 2)
				{
					two_finger_time_active = touchscreen_event.time;
				}

				printf("finger pressed, %d fingers on screen\r\n", fingers);
			}
			if(touchscreen_event.type == EV_ABS && touchscreen_event.code == ABS_MT_TRACKING_ID && touchscreen_event.value == -1)
			{
				if(fingers == 2)
				{
					struct timeval ret_time;
					timersub(&touchscreen_event.time, &two_finger_time_active, &ret_time);

					if(ret_time.tv_sec == 0 && ret_time.tv_usec < 500000)
					{
						printf("right click\r\n");
						emit(fd, EV_KEY, BTN_RIGHT, 1);
						emit(fd, EV_SYN, SYN_REPORT, 0);
						emit(fd, EV_KEY, BTN_RIGHT, 0);
					}
				}

				fingers--;

				if(fingers < 0)
				{
					fingers = 0;
				}

				printf("finger released, %d fingers on screen\r\n", fingers);
			}
			if(touchscreen_event.type == EVENT_TYPE && touchscreen_event.code == EVENT_CODE_X)
			{
				if(!init_prev_x)
				emit(fd, EV_REL, REL_X, touchscreen_event.value - prev_x);
				prev_x = touchscreen_event.value;
				init_prev_x = 0;
				//printf("touch x: %d\r\n", touchscreen_event.value);
			}
			if(touchscreen_event.type == EVENT_TYPE && touchscreen_event.code == EVENT_CODE_Y)
			{
				if(!init_prev_y)
				emit(fd, EV_REL, REL_Y, touchscreen_event.value - prev_y);
				prev_y = touchscreen_event.value;
				init_prev_y = 0;
			}
			if(touchscreen_event.type == EV_SYN && touchscreen_event.code == SYN_REPORT)
			{
				emit(fd, EV_SYN, SYN_REPORT, 0);
			}
		}
	}

	sleep(1);

	ioctl(fd, UI_DEV_DESTROY);
	close(fd);

	return 0;
}
