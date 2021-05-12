//  Touchpad Emulator
// Turn touchscreen into touchpad

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <linux/uinput.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>

#define EVENT_TYPE	EV_ABS
#define EVENT_CODE_X	ABS_X
#define EVENT_CODE_Y	ABS_Y

void emit(int fd, int type, int code, int val)
{
	struct input_event ie;

	ie.type = type;
	ie.code = code;
	ie.value = val;
	ie.input_event_sec = 0;
	ie.input_event_usec = 0;

	write(fd, &ie, sizeof(ie));
}

int main(int argc, char* argv[])
{
	if(argc != 3)
	{
		return 0;
	}

	struct uinput_setup usetup;
	int rotation = 0;
	int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

	ioctl(fd, UI_SET_EVBIT, EV_KEY);
	ioctl(fd, UI_SET_KEYBIT, BTN_LEFT);
	ioctl(fd, UI_SET_KEYBIT, BTN_RIGHT);

	ioctl(fd, UI_SET_EVBIT, EV_REL);
	ioctl(fd, UI_SET_RELBIT, REL_X);
	ioctl(fd, UI_SET_RELBIT, REL_Y);
	ioctl(fd, UI_SET_RELBIT, REL_WHEEL);

	ioctl(fd, UI_SET_PROPBIT, INPUT_PROP_DIRECT);

	memset(&usetup, 0, sizeof(usetup));

	usetup.id.bustype = BUS_USB;
	usetup.id.vendor  = 0x1234;
	usetup.id.product = 0x5678;
	strcpy(usetup.name, "Touchpad Emulator");

	ioctl(fd, UI_DEV_SETUP, &usetup);
	ioctl(fd, UI_DEV_CREATE);

	sleep(1);

	int touchscreen_fd = open(argv[1], O_RDONLY|O_NONBLOCK);

	ioctl(touchscreen_fd, EVIOCGRAB, 1);

	int max_x[6];
	int max_y[6];

	ioctl(touchscreen_fd, EVIOCGABS(ABS_MT_POSITION_X), max_x);
	ioctl(touchscreen_fd, EVIOCGABS(ABS_MT_POSITION_Y), max_y);

	printf("max x:%d max y:%d\r\n", max_x[2], max_y[2]);

	int buttons_fd = open(argv[2], O_RDONLY|O_NONBLOCK);

	ioctl(buttons_fd, EVIOCGRAB, 1);

	int prev_x = 0;
	int prev_y = 0;
	int prev_wheel_x = 0;
	int prev_wheel_y = 0;

	int init_prev_x = 0;
	int init_prev_y = 0;
	int init_prev_wheel_x = 0;
	int init_prev_wheel_y = 0;
	
	int fingers = 0;
	int dragging = 0;

	struct timeval time_active;
	struct timeval time_release;
	struct timeval time_button;
	struct timeval two_finger_time_active;

	int close_flag = 0;
	int touchpad_enable = 1;
	
	int en_key_val = 0;
	int dis_key_val = 0;
	
	struct pollfd fds[2];
	
	fds[0].fd = touchscreen_fd;
	fds[1].fd = buttons_fd;
	
	fds[0].events = POLLIN;
	fds[1].events = POLLIN;
	
	while(!close_flag)
	{
		int ret = poll(fds, 2, 5000);
		
		if(ret <= 0) continue;
		
		struct input_event touchscreen_event;

		//Read the touchscreen
		ret = read(touchscreen_fd, &touchscreen_event, sizeof(touchscreen_event));

		if(ret > 0 && touchpad_enable)
		{
			if(touchscreen_event.type == EV_KEY && touchscreen_event.value == 1 && touchscreen_event.code == BTN_TOUCH)
			{
				struct timeval cur_time;
				cur_time.tv_sec = touchscreen_event.input_event_sec;
				cur_time.tv_usec = touchscreen_event.input_event_usec;
				time_active = cur_time;
				struct timeval ret_time;
				timersub(&cur_time, &time_release, &ret_time);
				if(ret_time.tv_sec == 0 && ret_time.tv_usec < 150000)
				{
					//printf("drag started\r\n");
					dragging = 1;
					emit(fd, EV_KEY, BTN_LEFT, 1);
					emit(fd, EV_SYN, SYN_REPORT, 0);
				}
				init_prev_x = 1;
				init_prev_y = 1;
				//printf("key press\r\n");
			}
			if(touchscreen_event.type == EV_KEY && touchscreen_event.value == 0 && touchscreen_event.code == BTN_TOUCH)
			{
				struct timeval cur_time;
				cur_time.tv_sec = touchscreen_event.input_event_sec;
				cur_time.tv_usec = touchscreen_event.input_event_usec;
				time_release = cur_time;
				//printf("key release\r\n");
				struct timeval ret_time;
				timersub(&cur_time, &time_active, &ret_time);
				if(ret_time.tv_sec == 0 && ret_time.tv_usec < 150000)
				{
					//printf("click\r\n");
					emit(fd, EV_KEY, BTN_LEFT, 1);
					emit(fd, EV_SYN, SYN_REPORT, 0);
					emit(fd, EV_KEY, BTN_LEFT, 0);
				}

				if(dragging)
				{
					//printf("drag stopped\r\n");
					emit(fd, EV_KEY, BTN_LEFT, 0);
					dragging = 0;
				}
			}
			if(touchscreen_event.type == EV_ABS && touchscreen_event.code == ABS_MT_TRACKING_ID && touchscreen_event.value >= 0)
			{
				fingers++;

				if(fingers == 2)
				{
					two_finger_time_active.tv_sec = touchscreen_event.input_event_sec;
					two_finger_time_active.tv_usec = touchscreen_event.input_event_usec;
					init_prev_wheel_x = 1;
					init_prev_wheel_y = 1;
				}

				//printf("finger pressed, %d fingers on screen\r\n", fingers);
			}
			if(touchscreen_event.type == EV_ABS && touchscreen_event.code == ABS_MT_TRACKING_ID && touchscreen_event.value == -1)
			{
				if(fingers == 2)
				{
					struct timeval cur_time;
					cur_time.tv_sec = touchscreen_event.input_event_sec;
					cur_time.tv_usec = touchscreen_event.input_event_usec;
					struct timeval ret_time;
					timersub(&cur_time, &two_finger_time_active, &ret_time);

					if(ret_time.tv_sec == 0 && ret_time.tv_usec < 150000)
					{
						//printf("right click\r\n");
						emit(fd, EV_KEY, BTN_RIGHT, 1);
						emit(fd, EV_SYN, SYN_REPORT, 0);
						emit(fd, EV_KEY, BTN_RIGHT, 0);
					}
					
					init_prev_x = 1;
					init_prev_y = 1;
				}

				fingers--;

				if(fingers < 0)
				{
					fingers = 0;
				}

				//printf("finger released, %d fingers on screen\r\n", fingers);
			}
			if(touchscreen_event.type == EVENT_TYPE && touchscreen_event.code == EVENT_CODE_X)
			{
				if(rotation == 90 || rotation == 180)
				{
					touchscreen_event.value = max_x[2] - touchscreen_event.value;
				}
				
				if(fingers == 1)
				{
					if(!init_prev_x)
					{
						if(rotation == 0 || rotation == 180)
						{
							emit(fd, EV_REL, REL_X, touchscreen_event.value - prev_x);
						}
						else if(rotation == 90 || rotation == 270)
						{
							emit(fd, EV_REL, REL_Y, touchscreen_event.value - prev_x);
						}
					}
						
					prev_x = touchscreen_event.value;
					init_prev_x = 0;
				}
				else if(fingers == 2)
				{
					if(init_prev_wheel_x)
					{
						prev_wheel_x = touchscreen_event.value;
						init_prev_wheel_x = 0;
					}
					else
					{
						if(rotation == 90 || rotation == 270)
						{
							int accumulator_wheel_x = touchscreen_event.value;
							
							if(abs(accumulator_wheel_x - prev_wheel_x) > 15)
							{
								emit(fd, EV_REL, REL_WHEEL, (accumulator_wheel_x - prev_wheel_x) / 10);
								prev_wheel_x = accumulator_wheel_x;
							}
						}
					}
				}	

			}
			if(touchscreen_event.type == EVENT_TYPE && touchscreen_event.code == EVENT_CODE_Y)
			{
				if(rotation == 180 || rotation == 270)
				{
					touchscreen_event.value = max_y[2] - touchscreen_event.value;
				}

				if(fingers == 1)
				{
					if(!init_prev_y)
					{
						if(rotation == 0 || rotation == 180)
						{
							emit(fd, EV_REL, REL_Y, touchscreen_event.value - prev_y);
						}
						else if(rotation == 90 || rotation == 270)
						{
							emit(fd, EV_REL, REL_X, touchscreen_event.value - prev_y);
						}
					}
	
					prev_y = touchscreen_event.value;
					init_prev_y = 0;
				}
				else if(fingers == 2)
				{
					if(init_prev_wheel_y)
					{
						prev_wheel_y = touchscreen_event.value;
						init_prev_wheel_y = 0;
					}
					else
					{
						if(rotation == 0 || rotation == 180)
						{
							int accumulator_wheel_y = touchscreen_event.value;
							
							if(abs(accumulator_wheel_y - prev_wheel_y) > 15)
							{
								emit(fd, EV_REL, REL_WHEEL, (accumulator_wheel_y - prev_wheel_y) / 10);
								prev_wheel_y = accumulator_wheel_y;
							}
						}
					}
				}
					
			}
			if(touchscreen_event.type == EV_SYN && touchscreen_event.code == SYN_REPORT)
			{
				emit(fd, EV_SYN, SYN_REPORT, 0);
			}
		}
		
		ret = read(buttons_fd, &touchscreen_event, sizeof(touchscreen_event));
		
		if(ret > 0)
		{
			if(touchscreen_event.type == EV_KEY && touchscreen_event.code == KEY_VOLUMEUP)
			{
				if(touchscreen_event.value == 1)
				{
					time_button.tv_sec = touchscreen_event.input_event_sec;
					time_button.tv_usec = touchscreen_event.input_event_usec;
				}
				
				en_key_val = touchscreen_event.value;
			}
			if(touchscreen_event.type == EV_KEY && touchscreen_event.code == KEY_VOLUMEDOWN)
			{
				if(touchscreen_event.value == 1)
				{
					time_button.tv_sec = touchscreen_event.input_event_sec;
					time_button.tv_usec = touchscreen_event.input_event_usec;
				}
				
				dis_key_val = touchscreen_event.value;
			}
			if(touchscreen_event.type == EV_SYN && touchscreen_event.code == SYN_REPORT)
			{
				if(!en_key_val || !dis_key_val)
				{
					struct timeval cur_time;
					cur_time.tv_sec = touchscreen_event.input_event_sec;
					cur_time.tv_usec = touchscreen_event.input_event_usec;
					struct timeval ret_time;
					timersub(&cur_time, &time_button, &ret_time);

					if(ret_time.tv_sec > 1)
					{
						close_flag = 1;
					}
				}
				if(en_key_val)
				{
					if(touchpad_enable)
					{
						rotation += 90;
						
						if(rotation > 270)
						{
							rotation = 0;
						}
					}
					else
					{
						ioctl(touchscreen_fd, EVIOCGRAB, 1);
						touchpad_enable = 1;
					}
				}
				else if(dis_key_val)
				{
					ioctl(touchscreen_fd, EVIOCGRAB, 0);
					touchpad_enable = 0;
				}
			}
		}
	}

	sleep(1);

	ioctl(fd, UI_DEV_DESTROY);
	close(fd);

	return 0;
}
