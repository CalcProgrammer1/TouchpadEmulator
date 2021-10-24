default:			TouchpadEmulator

TouchpadEmulator:	TouchpadEmulator.c
					gcc -Wall $(shell pkg-config --cflags dbus-1 dbus-glib-1) TouchpadEmulator.c -ldbus-1 -ldbus-glib-1 -lpthread -o TouchpadEmulator

clean:
					git clean -dfx

install:
					install -d $(DESTDIR)/usr/bin
					install -m 755 TouchpadEmulator $(DESTDIR)/usr/bin
					install -m 755 LaunchTouchpadEmulator.sh $(DESTDIR)/usr/bin
					install -d $(DESTDIR)/usr/share/applications
					install -m 755 TouchpadEmulator.desktop $(DESTDIR)/usr/share/applications
					install -d $(DESTDIR)/usr/share/icons
					install -m 644 TouchpadEmulator.png $(DESTDIR)/usr/share/icons

uninstall:
					rm $(DESTDIR)/usr/bin/TouchpadEmulator
					rm $(DESTDIR)/usr/bin/LaunchTouchpadEmulator.sh
					rm $(DESTDIR)/usr/share/applications/TouchpadEmulator.desktop
					rm $(DESTDIR)/usr/share/icons/TouchpadEmulator.png
