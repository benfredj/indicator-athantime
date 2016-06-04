all: indicator-athantime
CC = gcc
CFLAGS = -Wall -g -O2 `pkg-config --cflags gtk+-3.0` `pkg-config --cflags appindicator3-0.1` `pkg-config --cflags  gstreamer-1.0`  -I/usr/include/itl `pkg-config --cflags sqlite3` `pkg-config --cflags webkitgtk-3.0`
LIBS = -litl -lm `pkg-config --libs gtk+-3.0` `pkg-config --libs  appindicator3-0.1` `pkg-config --libs  gstreamer-1.0` `pkg-config --libs sqlite3` `pkg-config --libs webkitgtk-3.0`

	
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
	cp indicator-athantime.svg /usr/share/icons/hicolor/scalable/apps
	gtk-update-icon-cache -f /usr/share/icons/hicolor/
	cp athan.ogg /usr/share/indicator-athantime
	cp adhkar.db /usr/share/indicator-athantime
	cp Rooster_Short_Crow.ogg /usr/share/indicator-athantime
	cp Warbling_Vireo-Mike_Koenig-89869915.ogg /usr/share/indicator-athantime
	cp killdeer_song-Mike_Koenig-1144525481.ogg /usr/share/indicator-athantime

uninstall:
	rm /usr/bin/indicator-athantime
	rm -rf /usr/share/indicator-athantime
	rm /usr/share/icons/hicolor/scalable/apps/indicator-athantime.svg
	gtk-update-icon-cache -f /usr/share/icons/hicolor/
	
	
clean:
	rm -f *.o *~  indicator-athantime
