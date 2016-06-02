# indicator-athantime
This is an Linux indicator (tested in Ubuntu) that shows the athan times (salat times for muslims).
مؤشر لنظام لينكس يعرض أوقات الأذان (أوقات الصلاة)

It depends on the libraries: -litl -lm -lgtk-3 -lgdk-3 -latk-1.0 -lgio-2.0 -lpangocairo-1.0 -lgdk_pixbuf-2.0 -lcairo-gobject -lpango-1.0 -lcairo -lgobject-2.0 -lglib-2.0 -lappindicator3 -ldbusmenu-glib -lgstreamer-1.0 -lsqlite3


Features:
- 
+ shows athan times
+ shows current hijri date (based on the Umm Alqura Calendar)
+ plays athan sound
+ plays notification sound before and after athan
+ shows adhkar

How to use:
- 
make

sudo make install


Set your preferences:
- 
edit the file $HOME/.athantime.conf

add the application (/usr/bin/indicator-athantime) to your startup applications


TODO:
- 
- ...


Thanks:
- 
The work was inspired from the Minbar application



By: 
- 
Ouissem Ben Fredj

Email: Ouissem  __ D O T __  BenFredj __  A T __ gmail __ D O T __ com
