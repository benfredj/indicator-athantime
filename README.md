# indicator-athantime
This is a Ubuntu indicator that shows the athan times.

It depends on the libraries: -litl -lm `pkg-config --libs gtk+-3.0` `pkg-config --libs  appindicator3-0.1` `pkg-config --libs  gstreamer-1.0`

Features:
- 
+ shows athan times
+ shows current hijri date (based on the Umm Alqura Calendar)
+ play athan sound
+ play notification sound before and after athan

How to use:
- 
make

sudo make install


Set your preferences:
- 
edit the file $HOME/.athantime.conf

add the application (/usr/bin/indicator-athantime) to your startup application


TODO:
- 
- customer icon
- ...


Thanks:
- 
The work was inspired from the Minbar application



By: 
- 
Ouissem Ben Fredj

Email: Ouissem  __ D O T __  BenFredj __  A T __ gmail __ D O T __ com
