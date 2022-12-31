/*---------------------------------------------------------*\
| Touchpad Emulator                                         |
|                                                           |
|   A Virtual Mouse for Linux Phones                        |
|                                                           |
|   This program hijacks the touchscreen input device and   |
|   exposes a virtual mouse device using uinput.  The mouse |
|   is controlled by dragging a finger on the touchscreen   |
|   as if it were a laptop touchpad.                        |
|                                                           |
|   License: GNU GPL V2                                     |
|                                                           |
|   Adam Honse (CalcProgrammer1)                            |
|   calcprogrammer1@gmail.com                               |
\*---------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <errno.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <fcntl.h>
#include <linux/uinput.h>
#include <poll.h>
#include <pthread.h>
#include <unistd.h>

/*---------------------------------------------------------*\
| Event Codes                                               |
\*---------------------------------------------------------*/
#define EVENT_TYPE      EV_ABS
#define EVENT_CODE_X    ABS_X
#define EVENT_CODE_Y    ABS_Y

/*---------------------------------------------------------*\
| Button Events                                             |
\*---------------------------------------------------------*/
enum
{
    BUTTON_EVENT_DO_NOTHING,
    BUTTON_EVENT_ENABLE_TOUCHPAD,
    BUTTON_EVENT_DISABLE_TOUCHPAD,
    BUTTON_EVENT_CLOSE,
    BUTTON_EVENT_EMIT_VOLUMEUP,
    BUTTON_EVENT_EMIT_VOLUMEDOWN
};

/*---------------------------------------------------------*\
| Global Variables                                          |
\*---------------------------------------------------------*/
int     rotation            = 0;
char    query_buf[64];

int     buttons_fd          = 0;
int     touchscreen_fd      = 0;
int     virtual_buttons_fd  = 0;
int     virtual_mouse_fd    = 0;

int     close_flag          = 0;
int     touchpad_enable     = 1;
int     keyboard_enable     = 1;
    
/*---------------------------------------------------------*\
| emit                                                      |
|                                                           |
| Emits an input event                                      |
\*---------------------------------------------------------*/

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

/*---------------------------------------------------------*\
| open_uinput                                               |
|                                                           |
| Creates the virtual mouse device                          |
\*---------------------------------------------------------*/

void open_uinput(int* fd)
{
    /*-----------------------------------------------------*\
    | If virtual mouse is already opened, return            |
    \*-----------------------------------------------------*/
    if(*fd != 0)
    {
        return;
    }

    /*-----------------------------------------------------*\
    | Open the uinput device                                |
    \*-----------------------------------------------------*/
    *fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

    /*-----------------------------------------------------*\
    | Virtual mouse provides left and right keys; x, y, and |
    | wheel axes and has direct property                    |
    \*-----------------------------------------------------*/
    ioctl(*fd, UI_SET_EVBIT,  EV_KEY);
    ioctl(*fd, UI_SET_KEYBIT, BTN_LEFT);
    ioctl(*fd, UI_SET_KEYBIT, BTN_RIGHT);

    ioctl(*fd, UI_SET_EVBIT,  EV_REL);
    ioctl(*fd, UI_SET_RELBIT, REL_X);
    ioctl(*fd, UI_SET_RELBIT, REL_Y);
    ioctl(*fd, UI_SET_RELBIT, REL_WHEEL);

    ioctl(*fd, UI_SET_PROPBIT, INPUT_PROP_DIRECT);

    /*-----------------------------------------------------*\
    | Set up virtual mouse device.  Use fake USB ID and name|
    | it "Touchpad Emulator"                                |
    \*-----------------------------------------------------*/
    struct uinput_setup usetup;

    memset(&usetup, 0, sizeof(usetup));

    usetup.id.bustype = BUS_USB;
    usetup.id.vendor  = 0x1234;
    usetup.id.product = 0x5678;
    strcpy(usetup.name, "Touchpad Emulator");

    ioctl(*fd, UI_DEV_SETUP, &usetup);

    /*-----------------------------------------------------*\
    | Create the virtual mouse.  The cursor should appear   |
    | on screen after this call.                            |
    \*-----------------------------------------------------*/
    ioctl(*fd, UI_DEV_CREATE);
}


void open_virtual_buttons(int* fd)
{
    /*-----------------------------------------------------*\
    | If virtual buttons is already opened, return          |
    \*-----------------------------------------------------*/
    if(*fd != 0)
    {
        return;
    }

    /*-----------------------------------------------------*\
    | Open the uinput device                                |
    \*-----------------------------------------------------*/
    *fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

    /*-----------------------------------------------------*\
    | Virtual buttons provides volume up and down keys      |
    \*-----------------------------------------------------*/
    ioctl(*fd, UI_SET_EVBIT,  EV_KEY);
    ioctl(*fd, UI_SET_KEYBIT, KEY_VOLUMEUP);
    ioctl(*fd, UI_SET_KEYBIT, KEY_VOLUMEDOWN);

    /*-----------------------------------------------------*\
    | Set up virtual buttons device.  Use fake USB ID and   |
    |name it "Touchpad Emulator Buttons"                    |
    \*-----------------------------------------------------*/
    struct uinput_setup usetup;

    memset(&usetup, 0, sizeof(usetup));

    usetup.id.bustype = BUS_USB;
    usetup.id.vendor  = 0x1234;
    usetup.id.product = 0x5678;
    strcpy(usetup.name, "Touchpad Emulator Buttons");

    ioctl(*fd, UI_DEV_SETUP, &usetup);

    /*-----------------------------------------------------*\
    | Create the virtual buttons                            |
    \*-----------------------------------------------------*/
    ioctl(*fd, UI_DEV_CREATE);
}

/*---------------------------------------------------------*\
| close_uinput                                              |
|                                                           |
| Closes the virtual mouse device                           |
\*---------------------------------------------------------*/

void close_uinput(int* fd)
{
    /*-----------------------------------------------------*\
    | Destroy the virtual mouse.  The cursor should         |
    | disappear from the screen after this call if no other |
    | mice are present.                                     |
    \*-----------------------------------------------------*/
    ioctl(*fd, UI_DEV_DESTROY);
    close(*fd);

    *fd = 0;
}

/*---------------------------------------------------------*\
| query_accelerometer_orientation                           |
|                                                           |
| Query DBus for the AccelerometerOrientation property of   |
| SensorProxy                                               |
\*---------------------------------------------------------*/

char* query_accelerometer_orientation()
{
    DBusMessageIter     args;
    DBusMessageIter     args_variant;
    DBusConnection*     conn;
    DBusError           err;
    DBusMessage*        msg;
    DBusPendingCall*    pending;
    char*               stat;

    // initialiset the errors
    dbus_error_init(&err);

    // connect to the system bus and check for errors
    conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);

    if(dbus_error_is_set(&err))
    {
        dbus_error_free(&err);
    }

    if(NULL == conn)
    {
        exit(1);
    }

    // create a new method call and check for errors
    msg = dbus_message_new_method_call("net.hadess.SensorProxy",
                                       "/net/hadess/SensorProxy",
                                       "org.freedesktop.DBus.Properties",
                                       "Get");

    if(NULL == msg)
    {
        exit(1);
    }

    // append arguments
    dbus_message_iter_init_append(msg, &args);

    char* arg1 = "net.hadess.SensorProxy";
    char* arg2 = "AccelerometerOrientation";
    if(!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &arg1))
    {
        exit(1);
    }

    if(!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &arg2))
    {
          exit(1);
    }

    // send message and get a handle for a reply
    if(!dbus_connection_send_with_reply (conn, msg, &pending, -1))
    {
        exit(1);
    }
       
    if(NULL == pending)
    {
        exit(1);
    }

    dbus_connection_flush(conn);

    // free message
    dbus_message_unref(msg);

    // block until we recieve a reply
    dbus_pending_call_block(pending);

    // get the reply message
    msg = dbus_pending_call_steal_reply(pending);

    if(NULL == msg)
    {
        exit(1);
    }

    // free the pending message handle
    dbus_pending_call_unref(pending);

    // read the parameters
    if (!dbus_message_iter_init(msg, &args))
    {
        fprintf(stderr, "Message has no arguments!\n");
    }
    else if(DBUS_TYPE_VARIANT != dbus_message_iter_get_arg_type(&args))
    {
        fprintf(stderr, "Argument is not variant! It is: %d\n", dbus_message_iter_get_arg_type(&args));
    }
    else
    {
        dbus_message_iter_recurse(&args, &args_variant);
        
        if(dbus_message_iter_get_arg_type(&args_variant) == DBUS_TYPE_STRING)
        {
            dbus_message_iter_get_basic(&args_variant, &stat);
        }
    }

    // copy reply
    strncpy(query_buf, stat, 64);

    // free reply
    dbus_message_unref(msg);

    return(query_buf);
}

/*---------------------------------------------------------*\
| rotation_from_accelerometer_orientation                   |
|                                                           |
| Determine the orientation angle from SensorProxy's        |
| AccelerometerOrientation property                         |
\*---------------------------------------------------------*/

int rotation_from_accelerometer_orientation(const char* orientation)
{
    if(strncmp(orientation, "right-up", 64) == 0)
    {
        return(90);
    }
    else if(strncmp(orientation, "bottom-up", 64) == 0)
    {
        return(180);
    }
    else if(strncmp(orientation, "left-up", 64) == 0)
    {
        return(270);
    }
    else //orientation == "normal"
    {
        return(0);
    }
}

/*---------------------------------------------------------*\
| monitor_rotation                                          |
|                                                           |
| Thread for monitoring rotation                            |
\*---------------------------------------------------------*/

void *monitor_rotation(void *vargp)
{
    while(1)
    {
        sleep(1);
        const char* orientation = query_accelerometer_orientation();
        rotation = rotation_from_accelerometer_orientation(orientation);   
    }
}

bool scan_and_open_devices(char* touchscreen_device, char* buttons_device)
{
    int     event_id        = 0;
    int     buttons_id      = -1;
    int     touchscreen_id  = -1;

    while((touchscreen_id == -1) || (buttons_id == -1))
    {
        /*-------------------------------------------------*\
        | Create the input event name path                  |
        \*-------------------------------------------------*/
        char input_dev_buf[1024];

        snprintf(input_dev_buf, 1024, "/sys/class/input/event%d/device/name", event_id);

        /*-------------------------------------------------*\
        | Open the input event path to get the name         |
        \*-------------------------------------------------*/
        int input_name_fd = open(input_dev_buf, O_RDONLY|O_NONBLOCK);

        if(input_name_fd < 0)
        {
            break;
        }

        memset(input_dev_buf, 0, 1024);
        read(input_name_fd, input_dev_buf, 1024);
        close(input_name_fd);

        printf("Input Device %d: %s", event_id, input_dev_buf);

        /*-------------------------------------------------*\
        | Check if this input is the touchscreen            |
        \*-------------------------------------------------*/
        if(strncmp(input_dev_buf, touchscreen_device, strlen(touchscreen_device)) == 0)
        {
            touchscreen_id = event_id;
        }
        
        /*-------------------------------------------------*\
        | Check if this input is the buttons                |
        \*-------------------------------------------------*/
        if(strncmp(input_dev_buf, buttons_device, strlen(buttons_device)) == 0)
        {
            buttons_id = event_id;
        }

        /*-------------------------------------------------*\
        | Move on to the next event                         |
        \*-------------------------------------------------*/
        event_id++;
    }

    if((touchscreen_id == -1) || (buttons_id == -1))
    {
        return false;
    }

    /*-----------------------------------------------------*\
    | Open the touchscreen device                           |
    \*-----------------------------------------------------*/
    char touchscreen_dev_path[1024];

    snprintf(touchscreen_dev_path, 1024, "/dev/input/event%d", touchscreen_id);

    touchscreen_fd = open(touchscreen_dev_path, O_RDONLY|O_NONBLOCK);

    /*-----------------------------------------------------*\
    | Open the buttons device                               |
    \*-----------------------------------------------------*/
    char buttons_dev_path[1024];

    snprintf(buttons_dev_path, 1024, "/dev/input/event%d", buttons_id);

    buttons_fd = open(buttons_dev_path, O_RDONLY|O_NONBLOCK);
    
    return true;
}

void process_button_event(int event)
{
    switch(event)
    {
        case BUTTON_EVENT_ENABLE_TOUCHPAD:
            if(!touchpad_enable)
            {
                ioctl(touchscreen_fd, EVIOCGRAB, 1);
                touchpad_enable = 1;
                open_uinput(&virtual_mouse_fd);

                system("gsettings set org.gnome.desktop.a11y.applications screen-keyboard-enabled false");
                keyboard_enable = 0;
            }
            break;

        case BUTTON_EVENT_DISABLE_TOUCHPAD:
            if(!touchpad_enable)
            {
                if(keyboard_enable)
                {
                    system("gsettings set org.gnome.desktop.a11y.applications screen-keyboard-enabled false");
                    keyboard_enable = 0;
                }
                else
                {
                    system("gsettings set org.gnome.desktop.a11y.applications screen-keyboard-enabled true");
                    keyboard_enable = 1;
                }
            }
            
            ioctl(touchscreen_fd, EVIOCGRAB, 0);
            touchpad_enable = 0;
            close_uinput(&virtual_mouse_fd);
            break;

        case BUTTON_EVENT_CLOSE:
            close_flag = 1;
            break;
        
        case BUTTON_EVENT_EMIT_VOLUMEUP:
            emit(virtual_buttons_fd, EV_KEY, KEY_VOLUMEUP, 1);
            emit(virtual_buttons_fd, EV_SYN, SYN_REPORT,   0);
            emit(virtual_buttons_fd, EV_KEY, KEY_VOLUMEUP, 0);
            emit(virtual_buttons_fd, EV_SYN, SYN_REPORT,   0);
            break;
            
        case BUTTON_EVENT_EMIT_VOLUMEDOWN:
            emit(virtual_buttons_fd, EV_KEY, KEY_VOLUMEDOWN, 1);
            emit(virtual_buttons_fd, EV_SYN, SYN_REPORT,     0);
            emit(virtual_buttons_fd, EV_KEY, KEY_VOLUMEDOWN, 0);
            emit(virtual_buttons_fd, EV_SYN, SYN_REPORT,     0);
            break;
    }
}

/*---------------------------------------------------------*\
| main                                                      |
|                                                           |
| Main function                                             |
\*---------------------------------------------------------*/

int main(int argc, char* argv[])
{
    /*-----------------------------------------------------*\
    | Open touchscreen and button devices by name           |
    \*-----------------------------------------------------*/
    if(!scan_and_open_devices("Goodix Capacitive TouchScreen", "1c21800.lradc"))
    {
        if(!scan_and_open_devices("Goodix Capacitive TouchScreen", "adc-keys"))
        {
            if(!scan_and_open_devices("Synaptics S3706B", "Volume keys"))
            {
                exit(1);
            }
        }
    }

    /*-----------------------------------------------------*\
    | Query accelerometer orientation to initialize rotation|
    \*-----------------------------------------------------*/
    const char* orientation = query_accelerometer_orientation();
    rotation = rotation_from_accelerometer_orientation(orientation);

    /*-----------------------------------------------------*\
    | Start rotation monitor thread                         |
    \*-----------------------------------------------------*/
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, monitor_rotation, NULL);

    /*-----------------------------------------------------*\
    | Open the virtual mouse                                |
    \*-----------------------------------------------------*/

    open_uinput(&virtual_mouse_fd);
    open_virtual_buttons(&virtual_buttons_fd);
    
    sleep(1);

    /*-----------------------------------------------------*\
    | Open the touchscreen device and grab exclusive access |
    \*-----------------------------------------------------*/
    ioctl(touchscreen_fd, EVIOCGRAB, 1);

    int max_x[6];
    int max_y[6];

    ioctl(touchscreen_fd, EVIOCGABS(ABS_MT_POSITION_X), max_x);
    ioctl(touchscreen_fd, EVIOCGABS(ABS_MT_POSITION_Y), max_y);

    printf("Touchscreen Max X:%d, Max y:%d\r\n", max_x[2], max_y[2]);

    /*-----------------------------------------------------*\
    | Open the buttons device and grab exclusive access     |
    \*-----------------------------------------------------*/
    ioctl(buttons_fd, EVIOCGRAB, 1);

    int button_up_long_hold_event   = BUTTON_EVENT_CLOSE;
    int button_up_short_hold_event  = BUTTON_EVENT_ENABLE_TOUCHPAD;
    int button_up_click_event       = BUTTON_EVENT_EMIT_VOLUMEUP;
    int button_dn_long_hold_event   = BUTTON_EVENT_CLOSE;
    int button_dn_short_hold_event  = BUTTON_EVENT_DISABLE_TOUCHPAD;
    int button_dn_click_event       = BUTTON_EVENT_EMIT_VOLUMEDOWN;
    
    /*-----------------------------------------------------*\
    | Initialize virtual mouse pointer tracking variables   |
    \*-----------------------------------------------------*/
    int prev_x              = 0;
    int prev_y              = 0;
    int prev_wheel_x        = 0;
    int prev_wheel_y        = 0;

    int init_prev_x         = 0;
    int init_prev_y         = 0;
    int init_prev_wheel_x   = 0;
    int init_prev_wheel_y   = 0;
    
    int fingers             = 0;
    int dragging            = 0;

    /*-----------------------------------------------------*\
    | Initialize time tracking variables                    |
    \*-----------------------------------------------------*/
    struct timeval time_active;
    struct timeval time_release;
    struct timeval time_button;
    struct timeval two_finger_time_active;

    /*-----------------------------------------------------*\
    | Initialize flag variables                             |
    \*-----------------------------------------------------*/
    close_flag              = 0;
    touchpad_enable         = 1;
    keyboard_enable         = 1;
    
    /*-----------------------------------------------------*\
    | Set up file descriptor polling structures             |
    \*-----------------------------------------------------*/
    struct pollfd fds[2];
    
    fds[0].fd               = touchscreen_fd;
    fds[1].fd               = buttons_fd;
    
    fds[0].events           = POLLIN;
    fds[1].events           = POLLIN;
    
    system("gsettings set org.gnome.desktop.a11y.applications screen-keyboard-enabled false");

    /*-----------------------------------------------------*\
    | Main loop                                             |
    \*-----------------------------------------------------*/
    while(!close_flag)
    {
        /*-------------------------------------------------*\
        | Poll until an input event occurs                  |
        \*-------------------------------------------------*/
        int ret = poll(fds, 2, 5000);
        
        if(ret <= 0) continue;
        
        /*-------------------------------------------------*\
        | Read the touchscreen event                        |
        \*-------------------------------------------------*/
        struct input_event touchscreen_event;

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
                    emit(virtual_mouse_fd, EV_KEY, BTN_LEFT,   1);
                    emit(virtual_mouse_fd, EV_SYN, SYN_REPORT, 0);
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
                    emit(virtual_mouse_fd, EV_KEY, BTN_LEFT,   1);
                    emit(virtual_mouse_fd, EV_SYN, SYN_REPORT, 0);
                    emit(virtual_mouse_fd, EV_KEY, BTN_LEFT,   0);
                }

                if(dragging)
                {
                    //printf("drag stopped\r\n");
                    emit(virtual_mouse_fd, EV_KEY, BTN_LEFT, 0);
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
                        emit(virtual_mouse_fd, EV_KEY, BTN_RIGHT,  1);
                        emit(virtual_mouse_fd, EV_SYN, SYN_REPORT, 0);
                        emit(virtual_mouse_fd, EV_KEY, BTN_RIGHT,  0);
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
                            emit(virtual_mouse_fd, EV_REL, REL_X, touchscreen_event.value - prev_x);
                        }
                        else if(rotation == 90 || rotation == 270)
                        {
                            emit(virtual_mouse_fd, EV_REL, REL_Y, touchscreen_event.value - prev_x);
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
                                emit(virtual_mouse_fd, EV_REL, REL_WHEEL, (accumulator_wheel_x - prev_wheel_x) / 10);
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
                            emit(virtual_mouse_fd, EV_REL, REL_Y, touchscreen_event.value - prev_y);
                        }
                        else if(rotation == 90 || rotation == 270)
                        {
                            emit(virtual_mouse_fd, EV_REL, REL_X, touchscreen_event.value - prev_y);
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
                                emit(virtual_mouse_fd, EV_REL, REL_WHEEL, (accumulator_wheel_y - prev_wheel_y) / 10);
                                prev_wheel_y = accumulator_wheel_y;
                            }
                        }
                    }
                }
                    
            }
            if(touchscreen_event.type == EV_SYN && touchscreen_event.code == SYN_REPORT)
            {
                emit(virtual_mouse_fd, EV_SYN, SYN_REPORT, 0);
            }
        }
        
        /*-------------------------------------------------*\
        | Read the buttons event                            |
        \*-------------------------------------------------*/
        struct input_event buttons_event;

        ret = read(buttons_fd, &buttons_event, sizeof(buttons_event));
        
        if(ret > 0)
        {
            if(buttons_event.type == EV_KEY && buttons_event.code == KEY_VOLUMEUP)
            {
                if(buttons_event.value == 1)
                {
                    time_button.tv_sec = buttons_event.input_event_sec;
                    time_button.tv_usec = buttons_event.input_event_usec;
                }
                else
                {
                    struct timeval cur_time;
                    cur_time.tv_sec = buttons_event.input_event_sec;
                    cur_time.tv_usec = buttons_event.input_event_usec;
                    struct timeval ret_time;
                    timersub(&cur_time, &time_button, &ret_time);

                    unsigned int usec = (ret_time.tv_sec * 1000000) + ret_time.tv_usec;
                    if(usec > 4000000)
                    {
                        process_button_event(button_up_long_hold_event);
                    }
                    else if(usec > 500000)
                    {
                    	process_button_event(button_up_short_hold_event);
                    }
                    else
                    {
                        process_button_event(button_up_click_event);
                    }
                }
            }
            if(buttons_event.type == EV_KEY && buttons_event.code == KEY_VOLUMEDOWN)
            {
                if(buttons_event.value == 1)
                {
                    time_button.tv_sec = buttons_event.input_event_sec;
                    time_button.tv_usec = buttons_event.input_event_usec;
                }
                else
                {
                    struct timeval cur_time;
                    cur_time.tv_sec = buttons_event.input_event_sec;
                    cur_time.tv_usec = buttons_event.input_event_usec;
                    struct timeval ret_time;
                    timersub(&cur_time, &time_button, &ret_time);

                    unsigned int usec = (ret_time.tv_sec * 1000000) + ret_time.tv_usec;
                    if(usec > 4000000)
                    {
                        process_button_event(button_dn_long_hold_event);
                    }
                    else if(usec > 500000)
                    {
                    	process_button_event(button_dn_short_hold_event);
                    }
                    else
                    {
                        process_button_event(button_dn_click_event);
                    }
                }
            }
        }
    }

    sleep(1);

    /*-----------------------------------------------------*\
    | Close the virtual mouse                               |
    \*-----------------------------------------------------*/
    close_uinput(&virtual_mouse_fd);
    
    system("gsettings set org.gnome.desktop.a11y.applications screen-keyboard-enabled true");

    return 0;
}
