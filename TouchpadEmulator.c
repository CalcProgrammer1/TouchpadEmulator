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
#include <linux/input.h>
#include <linux/uinput.h>
#include <poll.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

/*---------------------------------------------------------*\
| Event Codes                                               |
\*---------------------------------------------------------*/
#define ABS_SLIDER              34
#define EVENT_TYPE              EV_ABS
#define EVENT_CODE_X            ABS_X
#define EVENT_CODE_ALT_X        ABS_MT_POSITION_X
#define EVENT_CODE_Y            ABS_Y
#define EVENT_CODE_ALT_Y        ABS_MT_POSITION_Y
#define EVENT_CODE_SLIDER       ABS_SLIDER

/*---------------------------------------------------------*\
| Macros (adapted from evtest.c)                            |
\*---------------------------------------------------------*/
#define NBITS(x)                ((((x) - 1) / __BITS_PER_LONG) + 1)
#define OFF(x)                  ((x) % __BITS_PER_LONG)
#define BIT(x)                  (1UL << OFF(x))
#define LONG(x)                 ((x) / __BITS_PER_LONG)
#define test_bit(bit, array)    ((array[LONG(bit)] >> OFF(bit)) & 1)

/*---------------------------------------------------------*\
| Button Events                                             |
\*---------------------------------------------------------*/
enum
{
    BUTTON_EVENT_DO_NOTHING,
    BUTTON_EVENT_ENABLE_TOUCHPAD,
    BUTTON_EVENT_DISABLE_TOUCHPAD_TOGGLE_KEYBOARD,
    BUTTON_EVENT_CLOSE,
    BUTTON_EVENT_EMIT_VOLUMEUP,
    BUTTON_EVENT_EMIT_VOLUMEDOWN,
    BUTTON_EVENT_CHANGE_ORIENTATION,
    BUTTON_EVENT_DISABLE_TOUCHPAD_ENABLE_KEYBOARD,
    BUTTON_EVENT_DISABLE_TOUCHPAD_DISABLE_KEYBOARD,
};

/*---------------------------------------------------------*\
| Global Variables                                          |
\*---------------------------------------------------------*/
char    query_buf[64]       = "";
int     rotation            = 0;

int     button_0_fd         = 0;
int     button_1_fd         = 0;
int     slider_fd           = 0;
int     touchscreen_fd      = 0;
int     virtual_buttons_fd  = 0;
int     virtual_mouse_fd    = 0;

int     close_flag          = 0;
int     touchpad_enable     = 1;
int     keyboard_enable     = 1;

int     dragging            = 0;
int     check_for_dragging  = 0;

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
| disable_keyboard                                          |
|                                                           |
| Disable the on screen keyboard                            |
\*---------------------------------------------------------*/

void disable_keyboard()
{
    system("gsettings set org.gnome.desktop.a11y.applications screen-keyboard-enabled false");
    keyboard_enable = 0;
}

/*---------------------------------------------------------*\
| enable_keyboard                                           |
|                                                           |
| Enable the on screen keyboard                             |
\*---------------------------------------------------------*/

void enable_keyboard()
{
    system("gsettings set org.gnome.desktop.a11y.applications screen-keyboard-enabled true");
    keyboard_enable = 1;
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

    /*-----------------------------------------------------*\
    | Initialize the errors                                 |
    \*-----------------------------------------------------*/
    dbus_error_init(&err);

    /*-----------------------------------------------------*\
    | Connect to the system bus and check for errors        |
    \*-----------------------------------------------------*/
    conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);

    if(dbus_error_is_set(&err))
    {
        dbus_error_free(&err);
    }

    if(NULL == conn)
    {
        exit(1);
    }

    /*-----------------------------------------------------*\
    | Create a new method call and check for errors         |
    \*-----------------------------------------------------*/
    msg = dbus_message_new_method_call("net.hadess.SensorProxy",
                                       "/net/hadess/SensorProxy",
                                       "org.freedesktop.DBus.Properties",
                                       "Get");

    if(NULL == msg)
    {
        exit(1);
    }

    /*-----------------------------------------------------*\
    | Append arguments                                      |
    \*-----------------------------------------------------*/
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

    /*-----------------------------------------------------*\
    | Send message and get a handle for a reply             |
    \*-----------------------------------------------------*/
    if(!dbus_connection_send_with_reply (conn, msg, &pending, -1))
    {
        exit(1);
    }
       
    if(NULL == pending)
    {
        exit(1);
    }

    dbus_connection_flush(conn);

    /*-----------------------------------------------------*\
    | Free message                                          |
    \*-----------------------------------------------------*/
    dbus_message_unref(msg);

    /*-----------------------------------------------------*\
    | Block until we recieve a reply                        |
    \*-----------------------------------------------------*/
    dbus_pending_call_block(pending);

    /*-----------------------------------------------------*\
    | Get the reply message                                 |
    \*-----------------------------------------------------*/
    msg = dbus_pending_call_steal_reply(pending);

    if(NULL == msg)
    {
        exit(1);
    }

    /*-----------------------------------------------------*\
    | Free the pending message handle                       |
    \*-----------------------------------------------------*/
    dbus_pending_call_unref(pending);

    /*-----------------------------------------------------*\
    | Read the parameters                                   |
    \*-----------------------------------------------------*/
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

            /*---------------------------------------------*\
            | Copy reply                                    |
            \*---------------------------------------------*/
            strncpy(query_buf, stat, 64);
        }
    }

    /*-----------------------------------------------------*\
    | Free reply                                            |
    \*-----------------------------------------------------*/
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
    else if(strncmp(orientation, "normal", 64) == 0)
    {
        return(0);
    }
    else
    {
        return(-1);
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

/*---------------------------------------------------------*\
| scan_and_open_auto                                        |
|                                                           |
| Scan for and open devices based on capabilities           |
\*---------------------------------------------------------*/

bool scan_and_open_auto()
{
    int     event_id            = 0;
    bool    button_0_found      = false;
    bool    button_1_found      = false;
    bool    touchscreen_found   = false;

    /*-----------------------------------------------------*\
    | Default all file descriptors to -1 (invalid)          |
    \*-----------------------------------------------------*/
    touchscreen_fd  = -1;
    button_0_fd     = -1;
    button_1_fd     = -1;
    slider_fd       = -1;

    while(1)
    {
        char input_dev_buf[1024];

        /*-------------------------------------------------*\
        | Create the input event path                       |
        \*-------------------------------------------------*/
        snprintf(input_dev_buf, 1024, "/dev/input/event%d", event_id);

        /*-------------------------------------------------*\
        | Open the input event path                         |
        \*-------------------------------------------------*/
        int input_fd = open(input_dev_buf, O_RDONLY|O_NONBLOCK);

        if(input_fd < 0)
        {
            break;
        }

        /*-------------------------------------------------*\
        | Print device name                                 |
        \*-------------------------------------------------*/
        ioctl(input_fd, EVIOCGNAME(sizeof(input_dev_buf)), input_dev_buf);

        printf("Input Device %d: %s\r\n", event_id, input_dev_buf);

        /*-------------------------------------------------*\
        | Get list of capabilities                          |
        \*-------------------------------------------------*/
        unsigned long capabilities[EV_MAX][NBITS(KEY_MAX)];

        ioctl(input_fd, EVIOCGBIT(0, EV_MAX), capabilities[0]);

        for(unsigned int type = 0; type < EV_MAX; type++)
        {
            if(test_bit(type, capabilities[0]) && type != EV_REP)
            {
                if(type == EV_SYN)
                {
                    continue;
                }

                ioctl(input_fd, EVIOCGBIT(type, KEY_MAX), capabilities[type]);
            }
        }

        /*-------------------------------------------------*\
        | Check if this device is Volume Up                 |
        \*-------------------------------------------------*/
        if(!button_0_found
        && test_bit(EV_SYN,             capabilities[0])
        && test_bit(EV_KEY,             capabilities[0])
        && test_bit(KEY_VOLUMEUP,       capabilities[EV_KEY]))
        {
            printf(" Device is Volume Up Key\r\n");
            button_0_fd         = input_fd;
            button_0_found      = true;
        }

        /*-------------------------------------------------*\
        | Check if this device is Volume Down               |
        \*-------------------------------------------------*/
        if(!button_1_found
        && test_bit(EV_SYN,             capabilities[0])
        && test_bit(EV_KEY,             capabilities[0])
        && test_bit(KEY_VOLUMEDOWN,     capabilities[EV_KEY]))
        {
            printf(" Device is Volume Down Key\r\n");
            button_1_fd         = input_fd;
            button_1_found      = true;
        }

        /*-------------------------------------------------*\
        | Check if this device is Touchscreen               |
        \*-------------------------------------------------*/
        if(!touchscreen_found
        && test_bit(EV_SYN,             capabilities[0])
        && test_bit(EV_KEY,             capabilities[0])
        && test_bit(BTN_TOUCH,          capabilities[EV_KEY])
        && test_bit(EV_ABS,             capabilities[0])
        && test_bit(ABS_MT_SLOT,        capabilities[EV_ABS])
        && ((test_bit(ABS_X,             capabilities[EV_ABS])
          && test_bit(ABS_Y,             capabilities[EV_ABS]))
         || (test_bit(ABS_MT_POSITION_X,  capabilities[EV_ABS])
          && test_bit(ABS_MT_POSITION_Y,  capabilities[EV_ABS]))))
        {
            printf(" Device is Touchscreen\r\n");
            touchscreen_fd      = input_fd;
            touchscreen_found   = true;
        }

        /*-------------------------------------------------*\
        | If this device was not used, close the device     |
        \*-------------------------------------------------*/
        if(touchscreen_fd != input_fd
        && button_0_fd    != input_fd
        && button_1_fd    != input_fd)
        {
            close(input_fd);
        }

        /*-------------------------------------------------*\
        | Move on to the next event                         |
        \*-------------------------------------------------*/
        event_id++;
    }

    /*-----------------------------------------------------*\
    | If both volume up and down are on the same device,    |
    | set the second button fd to invalid                   |
    \*-----------------------------------------------------*/
    if(button_0_fd == button_1_fd)
    {
        button_1_fd = -1;
    }

    /*-----------------------------------------------------*\
    | Return true if touchscreen, volume up, and volume     |
    | down were all found                                   |
    \*-----------------------------------------------------*/
    if(touchscreen_found && button_0_found && button_1_found)
    {
        return true;
    }
    
    close(button_0_fd);
    close(button_1_fd);
    close(touchscreen_fd);

    return false;
}

/*---------------------------------------------------------*\
| scan_and_open_devices                                     |
|                                                           |
| Scan for and open devices matching the given names        |
\*---------------------------------------------------------*/

bool scan_and_open_devices(char* touchscreen_device, char* button_0_device, char* button_1_device, char* slider_device)
{
    char*   device_name[4];
    bool    device_required[4];
    int     device_id[4];
    int     event_id        = 0;
    
    device_name[0] = touchscreen_device;
    device_name[1] = button_0_device;
    device_name[2] = button_1_device;
    device_name[3] = slider_device;

    for(int i = 0; i < 4; i++)
    {
        device_required[i] = false;
        device_id[i]       = -1;

        if(strlen(device_name[i]) > 0)
        {
            device_required[i] = true;
        }
    }
    
    bool all_found = false;

    while(!all_found)
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
        | Check if this input matches any of our devices    |
        \*-------------------------------------------------*/
        for(int i = 0; i < 4; i++)
        {
            if(device_required[i] && (strncmp(input_dev_buf, device_name[i], strlen(device_name[i])) == 0))
            {
                printf("Opened device %s\r\n", device_name[i]);
                device_id[i] = event_id;
            }
        }

        all_found = true;

        for(int i = 0; i < 4; i++)
        {
            if(device_required[i] && device_id[i] == -1)
            {
                all_found = false;
            }
        }

        /*-------------------------------------------------*\
        | Move on to the next event                         |
        \*-------------------------------------------------*/
        event_id++;
    }

    if(!all_found)
    {
        return false;
    }

    /*-----------------------------------------------------*\
    | Open the touchscreen device                           |
    \*-----------------------------------------------------*/
    char touchscreen_dev_path[1024];

    snprintf(touchscreen_dev_path, 1024, "/dev/input/event%d", device_id[0]);

    touchscreen_fd = open(touchscreen_dev_path, O_RDONLY|O_NONBLOCK);

    /*-----------------------------------------------------*\
    | Open the button 0 device                              |
    \*-----------------------------------------------------*/
    char button_0_dev_path[1024];

    snprintf(button_0_dev_path, 1024, "/dev/input/event%d", device_id[1]);

    button_0_fd = open(button_0_dev_path, O_RDONLY|O_NONBLOCK);

    /*-----------------------------------------------------*\
    | Open the button 1 device                              |
    \*-----------------------------------------------------*/
    char button_1_dev_path[1024];

    snprintf(button_1_dev_path, 1024, "/dev/input/event%d", device_id[2]);

    button_1_fd = open(button_1_dev_path, O_RDONLY|O_NONBLOCK);
    
    /*-----------------------------------------------------*\
    | Open the slider device                                |
    \*-----------------------------------------------------*/
    char slider_dev_path[1024];

    snprintf(slider_dev_path, 1024, "/dev/input/event%d", device_id[3]);

    slider_fd = open(slider_dev_path, O_RDONLY|O_NONBLOCK);
    
    return true;
}

/*---------------------------------------------------------*\
| process_button_event                                      |
|                                                           |
| Process a button event                                    |
\*---------------------------------------------------------*/

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

                disable_keyboard();
            }
            break;

        case BUTTON_EVENT_DISABLE_TOUCHPAD_TOGGLE_KEYBOARD:
            if(!touchpad_enable)
            {
                if(keyboard_enable)
                {
                    disable_keyboard();
                }
                else
                {
                    enable_keyboard();
                }
            }
            
            ioctl(touchscreen_fd, EVIOCGRAB, 0);
            touchpad_enable = 0;
            close_uinput(&virtual_mouse_fd);
            break;

        case BUTTON_EVENT_DISABLE_TOUCHPAD_DISABLE_KEYBOARD:
            if(!touchpad_enable)
            {
                disable_keyboard();
            }
            
            ioctl(touchscreen_fd, EVIOCGRAB, 0);
            touchpad_enable = 0;
            close_uinput(&virtual_mouse_fd);
            break;

        case BUTTON_EVENT_DISABLE_TOUCHPAD_ENABLE_KEYBOARD:
            if(!touchpad_enable)
            {
                enable_keyboard();
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

        case BUTTON_EVENT_CHANGE_ORIENTATION:
            rotation += 90;

            if(rotation > 270)
            {
                rotation = 0;
            }
            break;
    }
}

/*---------------------------------------------------------*\
| drag_timeout                                              |
|                                                           |
| Handle the hold-to-drag timer timeout                     |
\*---------------------------------------------------------*/

void drag_timeout(union sigval val)
{
    if(check_for_dragging)
    {
        dragging = 1;
        check_for_dragging = 0;
        emit(virtual_mouse_fd, EV_KEY, BTN_LEFT,   1);
        emit(virtual_mouse_fd, EV_SYN, SYN_REPORT, 0);
    }
}

/*---------------------------------------------------------*\
| main                                                      |
|                                                           |
| Main function                                             |
\*---------------------------------------------------------*/

int main(int argc, char* argv[])
{
    bool opened         = false;

    /*-----------------------------------------------------*\
    | Open touchscreen and button devices by name           |
    \*-----------------------------------------------------*/
    // PINE64 PinePhone
    opened |= scan_and_open_devices("Goodix Capacitive TouchScreen", "1c21800.lradc", "", "");
    
    // PINE64 PinePhone Pro
    opened |= scan_and_open_devices("Goodix Capacitive TouchScreen", "adc-keys", "", "");
    
    // OnePlus 6T
    opened |= scan_and_open_devices("Synaptics S3706B", "" /*"Volume keys"*/, "", "Alert slider");

    // Xiaomi Pad 5 Pro
    opened |= scan_and_open_devices("NVTCapacitiveTouchScreen", "gpio-keys", "pm8941_resin", "");

    // Google Pixel 3a
    opened |= scan_and_open_devices("Synaptics S3706B", "gpio-keys", "pm8941_resin", "");

    // Xiaomi Poco F1
    opened |= scan_and_open_devices("nvt-ts", "gpio-keys", "pm8941_resin", "");

    // LG Google Nexus 5
    opened |= scan_and_open_devices("Synaptics PLG218", "gpio-keys", "", "");

    /*-----------------------------------------------------*\
    | If probing known device event names failed, try to    |
    | detect touchscreen and buttons devices automatically  |
    | based on input capabilities                           |
    \*-----------------------------------------------------*/
    if(!opened)
    {
        opened |= scan_and_open_auto();
    }

    if(!opened)
    {
        printf("No supported set of input devices found, exiting.\r\n");
        exit(1);
    }

    /*-----------------------------------------------------*\
    | Set up default button behaviors                       |
    \*-----------------------------------------------------*/
    int button_0_long_hold_event   = BUTTON_EVENT_CLOSE;
    int button_0_short_hold_event  = BUTTON_EVENT_ENABLE_TOUCHPAD;
    int button_0_click_event       = BUTTON_EVENT_EMIT_VOLUMEUP;
    int button_1_long_hold_event   = BUTTON_EVENT_CLOSE;
    int button_1_short_hold_event  = BUTTON_EVENT_DISABLE_TOUCHPAD_TOGGLE_KEYBOARD;
    int button_1_click_event       = BUTTON_EVENT_EMIT_VOLUMEDOWN;

    /*-----------------------------------------------------*\
    | If rotation is passed on command line, use fixed      |
    | rotation value                                        |
    \*-----------------------------------------------------*/    
    bool rotation_override = false;

    if(argc > 1)
    {
        if(strncmp(argv[1], "0", 1) == 0)
        {
            rotation = 0;
            rotation_override = true;
        }
        else if(strncmp(argv[1], "90", 2) == 0)
        {
            rotation = 90;
            rotation_override = true;
        }
        else if(strncmp(argv[1], "180", 3) == 0)
        {
            rotation = 180;
            rotation_override = true;
        }
        else if(strncmp(argv[1], "270", 3) == 0)
        {
            rotation = 270;
            rotation_override = true;
        }
    }

    /*-----------------------------------------------------*\
    | Otherwise, query rotation from accelerometer and      |
    | start rotation monitor thread                         |
    \*-----------------------------------------------------*/
    if(!rotation_override)
    {
        /*-------------------------------------------------*\
        | Query accelerometer orientation to initialize     |
        | rotation                                          |
        \*-------------------------------------------------*/
        const char* orientation = query_accelerometer_orientation();
        rotation = rotation_from_accelerometer_orientation(orientation);

        if(rotation >= 0)
        {
            /*---------------------------------------------*\
            | Start rotation monitor thread                 |
            \*---------------------------------------------*/
            printf("Automatic orientation detection enabled.\r\n");
            pthread_t thread_id;
            pthread_create(&thread_id, NULL, monitor_rotation, NULL);
        }
        else
        {
            /*---------------------------------------------*\
            | Enable manual rotation if automatic rotation  |
            | could not be enabled                          |
            \*---------------------------------------------*/
            printf("Orientation could not be determined from accelerometer, defaulting to 0 degrees.\r\n");
            printf("Long-press Volume Up button to change orientations manually.\r\n");
            button_0_long_hold_event = BUTTON_EVENT_CHANGE_ORIENTATION;
            rotation                 = 0;
        }
    }

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
    ioctl(button_0_fd, EVIOCGRAB, 1);
    ioctl(button_1_fd, EVIOCGRAB, 1);
  
    /*-----------------------------------------------------*\
    | Open the slider device and grab exclusive access      |
    \*-----------------------------------------------------*/
    ioctl(slider_fd, EVIOCGRAB, 1);

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
    
    int touch_active        = 0;
    int fingers             = 0;

    int active_mt_slot      = 0;
    int check_for_click     = 0;
    int check_for_tap_drag  = 0;

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
    touchpad_enable         = 0;
    keyboard_enable         = 1;
    
    /*-----------------------------------------------------*\
    | Set up file descriptor polling structures             |
    \*-----------------------------------------------------*/
    struct pollfd fds[4];
    
    fds[0].fd               = touchscreen_fd;
    fds[1].fd               = button_0_fd;
    fds[2].fd               = button_1_fd;
    fds[3].fd               = slider_fd;
    
    fds[0].events           = POLLIN;
    fds[1].events           = POLLIN;
    fds[2].events           = POLLIN;
    fds[3].events           = POLLIN;

    /*-----------------------------------------------------*\
    | Create a timer to handle hold-to-drag                 |
    \*-----------------------------------------------------*/
    timer_t timer;
    
    struct sigevent ev;
    ev.sigev_notify                 = SIGEV_THREAD;
    ev.sigev_signo                  = 0;
    ev.sigev_value.sival_ptr        = NULL;
    ev.sigev_notify_function        = &drag_timeout;
    ev.sigev_notify_attributes      = 0;

    timer_create(CLOCK_MONOTONIC, &ev, &timer);

    struct itimerspec itime_start;
    itime_start.it_value.tv_sec     = 1;
    itime_start.it_value.tv_nsec    = 0;
    itime_start.it_interval.tv_sec  = 0;
    itime_start.it_interval.tv_nsec = 0;

    struct itimerspec itime_stop;
    itime_stop.it_value.tv_sec      = 0;
    itime_stop.it_value.tv_nsec     = 0;
    itime_stop.it_interval.tv_sec   = 0;
    itime_stop.it_interval.tv_nsec  = 0;

    /*-----------------------------------------------------*\
    | Determine initial state                               |
    |   If slider is used, initialize based on slider       |
    |   position, otherwise initialize to touchpad mode     |
    \*-----------------------------------------------------*/
    if(slider_fd >= 0)
    {
        struct input_absinfo absinfo;
        ioctl(slider_fd, EVIOCGABS(ABS_SLIDER), &absinfo);

        switch(absinfo.value)
        {
            case 0:
                process_button_event(BUTTON_EVENT_ENABLE_TOUCHPAD);
                break;
                
            case 1:
                process_button_event(BUTTON_EVENT_DISABLE_TOUCHPAD_DISABLE_KEYBOARD);
                break;
                
            case 2:
                process_button_event(BUTTON_EVENT_DISABLE_TOUCHPAD_ENABLE_KEYBOARD);
                break;
        }
    }
    else
    {
        disable_keyboard();
        touchpad_enable = 1;
    }

    /*-----------------------------------------------------*\
    | Main loop                                             |
    \*-----------------------------------------------------*/
    while(!close_flag)
    {
        /*-------------------------------------------------*\
        | Poll until an input event occurs                  |
        \*-------------------------------------------------*/
        int ret = poll(fds, 4, 5000);
        
        if(ret <= 0) continue;

        if(!touchpad_enable)
        {
            fingers = 0;
        }

        /*-------------------------------------------------*\
        | Read the touchscreen event                        |
        \*-------------------------------------------------*/
        struct input_event touchscreen_event;

        ret = read(touchscreen_fd, &touchscreen_event, sizeof(touchscreen_event));

        if(ret > 0 && touchpad_enable)
        {
            /*---------------------------------------------*\
            | Touchscreen pressed                           |
            \*---------------------------------------------*/
            if(touchscreen_event.type == EV_KEY && touchscreen_event.value == 1 && touchscreen_event.code == BTN_TOUCH)
            {
                /*-----------------------------------------*\
                | Set touch active flag                     |
                \*-----------------------------------------*/
                touch_active = 1;

                /*-----------------------------------------*\
                | Record the activated time                 |
                \*-----------------------------------------*/
                struct timeval cur_time;
                cur_time.tv_sec = touchscreen_event.input_event_sec;
                cur_time.tv_usec = touchscreen_event.input_event_usec;
                time_active = cur_time;

                /*-----------------------------------------*\
                | If there has been less than 150000 usec   |
                | since the last tap, activate dragging     |
                \*-----------------------------------------*/
                struct timeval ret_time;
                timersub(&cur_time, &time_release, &ret_time);

                if(check_for_tap_drag && ret_time.tv_sec == 0 && ret_time.tv_usec < 150000)
                {
                    dragging = 1;
                    check_for_tap_drag = 0;
                    emit(virtual_mouse_fd, EV_KEY, BTN_LEFT,   1);
                    emit(virtual_mouse_fd, EV_SYN, SYN_REPORT, 0);
                }

                /*-----------------------------------------*\
                | Otherwise, start a 1 second timer.  If no |
                | movement has occurred when the timer      |
                | expires, activate dragging                |
                \*-----------------------------------------*/
                else if(fingers <= 1)
                {
                    check_for_dragging = 1;
                    timer_settime(timer, 0, &itime_start, NULL);
                }

                /*-----------------------------------------*\
                | Set the initialize previous x and y flags |
                \*-----------------------------------------*/
                init_prev_x = 1;
                init_prev_y = 1;

                check_for_click = 1;
                check_for_tap_drag = 1;
            }

            /*---------------------------------------------*\
            | Touchscreen released                          |
            \*---------------------------------------------*/
            if(touchscreen_event.type == EV_KEY && touchscreen_event.value == 0 && touchscreen_event.code == BTN_TOUCH)
            {
                /*-----------------------------------------*\
                | Clear touch active flag                   |
                \*-----------------------------------------*/
                touch_active = 0;

                /*-----------------------------------------*\
                | Record the released time                  |
                \*-----------------------------------------*/
                struct timeval cur_time;
                cur_time.tv_sec = touchscreen_event.input_event_sec;
                cur_time.tv_usec = touchscreen_event.input_event_usec;
                time_release = cur_time;
                
                /*-----------------------------------------*\
                | If there has been less than 150000 usec   |
                | since touch was activated, produce click  |
                \*-----------------------------------------*/
                struct timeval ret_time;
                timersub(&cur_time, &time_active, &ret_time);

                if(check_for_click == 1 && ret_time.tv_sec == 0 && ret_time.tv_usec < 150000)
                {
                    check_for_click = 0;
                    emit(virtual_mouse_fd, EV_KEY, BTN_LEFT,   1);
                    emit(virtual_mouse_fd, EV_SYN, SYN_REPORT, 0);
                    emit(virtual_mouse_fd, EV_KEY, BTN_LEFT,   0);
                }

                /*-----------------------------------------*\
                | If dragging is active, release button and |
                | stop dragging                             |
                \*-----------------------------------------*/
                if(dragging)
                {
                    emit(virtual_mouse_fd, EV_KEY, BTN_LEFT, 0);
                    dragging = 0;
                }

                /*-----------------------------------------*\
                | If touch has been released, cancel hold   |
                | to drag check                             |
                \*-----------------------------------------*/
                check_for_dragging = 0;
                timer_settime(timer, 0, &itime_stop, NULL);
            }
            
            /*---------------------------------------------*\
            | Finger pressed                                |
            \*---------------------------------------------*/
            if(touchscreen_event.type == EV_ABS && touchscreen_event.code == ABS_MT_TRACKING_ID && touchscreen_event.value >= 0)
            {
                /*-----------------------------------------*\
                | Increment finger count                    |
                \*-----------------------------------------*/
                fingers++;

                /*-----------------------------------------*\
                | If more than one finger touched since     |
                | touch activated, cancel hold to drag check|
                \*-----------------------------------------*/
                if(fingers > 1)
                {
                    check_for_dragging  = 0;
                    timer_settime(timer, 0, &itime_stop, NULL);

                    check_for_click     = 0;
                    check_for_tap_drag  = 0;
                }

                /*-----------------------------------------*\
                | If there are two fingers active, record   |
                | two finger active time and set previous   |
                | wheel x and y initialization flags        |
                \*-----------------------------------------*/
                if(fingers == 2)
                {
                    two_finger_time_active.tv_sec = touchscreen_event.input_event_sec;
                    two_finger_time_active.tv_usec = touchscreen_event.input_event_usec;
                    init_prev_wheel_x = 1;
                    init_prev_wheel_y = 1;
                }
            }
            
            /*---------------------------------------------*\
            | Finger released                               |
            \*---------------------------------------------*/
            if(touchscreen_event.type == EV_ABS && touchscreen_event.code == ABS_MT_TRACKING_ID && touchscreen_event.value == -1)
            {
                if(fingers == 2)
                {
                    /*-------------------------------------*\
                    | Read the two finger released time     |
                    \*-------------------------------------*/
                    struct timeval cur_time;
                    cur_time.tv_sec = touchscreen_event.input_event_sec;
                    cur_time.tv_usec = touchscreen_event.input_event_usec;

                    /*-------------------------------------*\
                    | If there has been less than 150000    |
                    | usec since two fingers were activated,|
                    | produce right click                   |
                    \*-------------------------------------*/
                    struct timeval ret_time;
                    timersub(&cur_time, &two_finger_time_active, &ret_time);

                    if(ret_time.tv_sec == 0 && ret_time.tv_usec < 150000)
                    {
                        emit(virtual_mouse_fd, EV_KEY, BTN_RIGHT,  1);
                        emit(virtual_mouse_fd, EV_SYN, SYN_REPORT, 0);
                        emit(virtual_mouse_fd, EV_KEY, BTN_RIGHT,  0);
                    }
                    
                    /*-------------------------------------*\
                    | Set the initialize previous x and y   |
                    | flags                                 |
                    \*-------------------------------------*/
                    init_prev_x = 1;
                    init_prev_y = 1;
                }

                /*-----------------------------------------*\
                | If number of fingers has changed since    |
                | touch activated, cancel hold to drag check|
                \*-----------------------------------------*/
                check_for_dragging = 0;
                timer_settime(timer, 0, &itime_stop, NULL);

                if(fingers > 1)
                {
                    check_for_click = 0;
                    check_for_tap_drag = 0;
                }

                /*-----------------------------------------*\
                | Decrement finger count                    |
                \*-----------------------------------------*/
                fingers--;

                /*-----------------------------------------*\
                | Sanity check, fingers on screen cannot be |
                | less than zero                            |
                \*-----------------------------------------*/
                if(fingers < 0)
                {
                    fingers = 0;
                }
            }
            
            /*---------------------------------------------*\
            | X-position of touch                           |
            \*---------------------------------------------*/
            if(touchscreen_event.type == EVENT_TYPE && (touchscreen_event.code == EVENT_CODE_X || touchscreen_event.code == EVENT_CODE_ALT_X))
            {
                if(!(active_mt_slot > 0 && touchscreen_event.code == EVENT_CODE_ALT_X))
                {
                    /*-------------------------------------*\
                    | If X position has changed since touch |
                    | activated, cancel hold to drag check  |
                    \*-------------------------------------*/
                    if(touch_active && (!init_prev_x && touchscreen_event.value != prev_x))
                    {
                        check_for_dragging = 0;
                        timer_settime(timer, 0, &itime_stop, NULL); 

                        check_for_click    = 0;
                        check_for_tap_drag = 0;
                    }
                    
                    /*-------------------------------------*\
                    | Handle orientations where X axis is   |
                    | mirrored                              |
                    \*-------------------------------------*/
                    if(rotation == 90 || rotation == 180)
                    {
                        touchscreen_event.value = max_x[2] - touchscreen_event.value;
                    }
                    
                    if(touch_active)
                    {
                        /*---------------------------------*\
                        | If one finger is on the screen,   |
                        | move the mouse cursor             |
                        \*---------------------------------*/
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

                        /*---------------------------------*\
                        | Otherwise, if two fingers are on  |
                        | the screen, move the scroll wheel |
                        \*---------------------------------*/
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
                }
            }
            
            /*---------------------------------------------*\
            | Y-position of touch                           |
            \*---------------------------------------------*/
            if(touchscreen_event.type == EVENT_TYPE && (touchscreen_event.code == EVENT_CODE_Y || touchscreen_event.code == EVENT_CODE_ALT_Y))
            {
                if(!(active_mt_slot > 0 && touchscreen_event.code == EVENT_CODE_ALT_Y))
                {
                    /*-------------------------------------*\
                    | If Y position has changed since touch |
                    | activated, cancel hold to drag check  |
                    \*-------------------------------------*/
                    if(touch_active && (!init_prev_y && touchscreen_event.value != prev_y))
                    {
                        check_for_dragging = 0;
                        timer_settime(timer, 0, &itime_stop, NULL); 

                        check_for_click    = 0;
                        check_for_tap_drag = 0;                 
                    }
                    
                    /*-------------------------------------*\
                    | Handle orientations where Y axis is   |
                    | mirrored                              |
                    \*-------------------------------------*/
                    if(rotation == 180 || rotation == 270)
                    {
                        touchscreen_event.value = max_y[2] - touchscreen_event.value;
                    }

                    if(touch_active)
                    {
                        /*---------------------------------*\
                        | If one finger is on the screen,   |
                        | move the mouse cursor             |
                        \*---------------------------------*/
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

                        /*---------------------------------*\
                        | Otherwise, if two fingers are on  |
                        | the screen, move the scroll wheel |
                        \*---------------------------------*/
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
                }
            }

            /*---------------------------------------------*\
            | Sync event                                    |
            \*---------------------------------------------*/
            if(touchscreen_event.type == EV_SYN && touchscreen_event.code == SYN_REPORT)
            {
                emit(virtual_mouse_fd, EV_SYN, SYN_REPORT, 0);
            }

            /*---------------------------------------------*\
            | Slot event                                    |
            \*---------------------------------------------*/
            if(touchscreen_event.type == EVENT_TYPE && touchscreen_event.code == ABS_MT_SLOT)
            {
                active_mt_slot = touchscreen_event.value;
            }
        }

        /*-------------------------------------------------*\
        | Read the buttons event                            |
        \*-------------------------------------------------*/
        struct input_event buttons_event;

        ret = read(button_0_fd, &buttons_event, sizeof(buttons_event));
        
        if(ret <= 0)
        {
            ret = read(button_1_fd, &buttons_event, sizeof(buttons_event));
        }

        if(ret > 0)
        {
            /*---------------------------------------------*\
            | Handle volume up key events                   |
            \*---------------------------------------------*/
            if(buttons_event.type == EV_KEY && buttons_event.code == KEY_VOLUMEUP)
            {
                if(buttons_event.value == 1)
                {
                    time_button.tv_sec = buttons_event.input_event_sec;
                    time_button.tv_usec = buttons_event.input_event_usec;
                }
                else if(buttons_event.value == 0)
                {
                    struct timeval cur_time;
                    cur_time.tv_sec = buttons_event.input_event_sec;
                    cur_time.tv_usec = buttons_event.input_event_usec;
                    struct timeval ret_time;
                    timersub(&cur_time, &time_button, &ret_time);

                    unsigned int usec = (ret_time.tv_sec * 1000000) + ret_time.tv_usec;
                    if(usec > 4000000)
                    {
                        process_button_event(button_0_long_hold_event);
                    }
                    else if(usec > 500000)
                    {
                    	process_button_event(button_0_short_hold_event);
                    }
                    else
                    {
                        process_button_event(button_0_click_event);
                    }
                }
            }

            /*---------------------------------------------*\
            | Handle volume down key events                 |
            \*---------------------------------------------*/
            if(buttons_event.type == EV_KEY && buttons_event.code == KEY_VOLUMEDOWN)
            {
                if(buttons_event.value == 1)
                {
                    time_button.tv_sec = buttons_event.input_event_sec;
                    time_button.tv_usec = buttons_event.input_event_usec;
                }
                else if(buttons_event.value == 0)
                {
                    struct timeval cur_time;
                    cur_time.tv_sec = buttons_event.input_event_sec;
                    cur_time.tv_usec = buttons_event.input_event_usec;
                    struct timeval ret_time;
                    timersub(&cur_time, &time_button, &ret_time);

                    unsigned int usec = (ret_time.tv_sec * 1000000) + ret_time.tv_usec;
                    if(usec > 4000000)
                    {
                        process_button_event(button_1_long_hold_event);
                    }
                    else if(usec > 500000)
                    {
                    	process_button_event(button_1_short_hold_event);
                    }
                    else
                    {
                        process_button_event(button_1_click_event);
                    }
                }
            }
        }

        /*-------------------------------------------------*\
        | Read the slider event                             |
        \*-------------------------------------------------*/
        struct input_event slider_event;

        ret = read(slider_fd, &slider_event, sizeof(slider_event));
        
        if(ret > 0)
        {
            /*---------------------------------------------*\
            | Handle slider events                          |
            \*---------------------------------------------*/
            if((slider_event.type == EV_ABS) && (slider_event.code == EVENT_CODE_SLIDER))
            {
                switch(slider_event.value)
                {
                    case 0:
                        process_button_event(BUTTON_EVENT_ENABLE_TOUCHPAD);
                        break;
                        
                    case 1:
                        process_button_event(BUTTON_EVENT_DISABLE_TOUCHPAD_DISABLE_KEYBOARD);
                        break;
                        
                    case 2:
                        process_button_event(BUTTON_EVENT_DISABLE_TOUCHPAD_ENABLE_KEYBOARD);
                        break;
                }
            }
        }
    }

    sleep(1);

    /*-----------------------------------------------------*\
    | Close the virtual mouse                               |
    \*-----------------------------------------------------*/
    close_uinput(&virtual_mouse_fd);
    
    /*-----------------------------------------------------*\
    | Enable the on-screen keyboard                         |
    \*-----------------------------------------------------*/
    enable_keyboard();

    return 0;
}
