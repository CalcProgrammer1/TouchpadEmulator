INC=-I/usr/include/dbus-1.0 -I/usr/include/glib-2.0 -I/usr/lib/dbus-1.0/include -I/usr/lib/glib-2.0/include -I/usr/lib/x86_64-linux-gnu/dbus-1.0/include -I/usr/lib/x86_64-linux-gnu/glib-2.0/include

default:			TouchpadEmulator

TouchpadEmulator:	TouchpadEmulator.c
					gcc -Wall $(INC) TouchpadEmulator.c -ldbus-1 -ldbus-glib-1 -lpthread -o TouchpadEmulator

clean:
					git clean -dfx

install:
					install -d /usr/bin
					install -m 755 TouchpadEmulator /usr/bin
					install -m 755 LaunchTouchpadEmulator.sh /usr/bin
					install -d /usr/share/applications
					install -m 755 TouchpadEmulator.desktop /usr/share/applications
					install -d /usr/share/icons
					install -m 644 TouchpadEmulator.png /usr/share/icons

uninstall:
					rm /usr/bin/TouchpadEmulator
					rm /usr/bin/LaunchTouchpadEmulator.sh
					rm /usr/share/applications/TouchpadEmulator.desktop
					rm /usr/share/icons/TouchpadEmulator.png