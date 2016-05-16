all: indicator-athantime
CC = gcc
CFLAGS = -g -O2 `pkg-config --cflags gtk+-2.0` `pkg-config --cflags appindicator-0.1` `pkg-config --cflags  gstreamer-0.10`  -I/usr/include/itl
LIBS = -litl -lm `pkg-config --libs gtk+-2.0` `pkg-config --libs  appindicator-0.1` `pkg-config --libs  gstreamer-0.10`


demo_prayer: demo_prayer.c
	@echo "==> Building demo..."
	$(CC) $(CFLAGS) demo_prayer.c -o demo_prayer $(LIBS)
	
indicator-athantime: indicator-athantime.c
	@echo "==> Building indicator..."
	$(CC) $(CFLAGS) indicator-athantime.c -o indicator-athantime $(LIBS)

install:
	install --mode=755 indicator-athantime  /usr/bin/
	cp .athantime.conf $(HOME)

uninstall:
	rm /usr/bin/indicator-athantime
	
clean:
	rm -f *.o *~ demo_prayer indicator-athantime
