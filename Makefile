all: indicator-athantime
CC = gcc
CFLAGS = -O2 `pkg-config --cflags gtk+-2.0` `pkg-config --cflags appindicator-0.1` `pkg-config --cflags  gstreamer-0.10`  -I/usr/include/itl
LIBS = -litl -lm `pkg-config --libs gtk+-2.0` `pkg-config --libs  appindicator-0.1` `pkg-config --libs  gstreamer-0.10`

	
indicator-athantime: umm_alqura.o indicator-athantime.c
	@echo "==> Building indicator..."
	$(CC) $(CFLAGS) umm_alqura.o indicator-athantime.c -o indicator-athantime $(LIBS)
	
umm_alqura.o: umm_alqura.c
	@echo "==> Building umm_alqura component..."
	$(CC) -O2 -I/usr/include/itl -c umm_alqura.c -o umm_alqura.o
	
install:
	install --mode=755 indicator-athantime  /usr/bin/
	cp .athantime.conf $(HOME)
	mkdir -p /usr/share/indicator-athantime
	cp athan.ogg /usr/share/indicator-athantime

uninstall:
	rm /usr/bin/indicator-athantime
	
clean:
	rm -f *.o *~  indicator-athantime
