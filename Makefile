default:			TouchpadEmulator

TouchpadEmulator:	TouchpadEmulator.c
					gcc -Wall $(shell pkg-config --cflags dbus-1 dbus-glib-1) TouchpadEmulator.c -ldbus-1 -ldbus-glib-1 -lpthread -o TouchpadEmulator

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
