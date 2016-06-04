# indicator-athantime
This is an Linux indicator (tested in Ubuntu) that shows the athan times (salat times for muslims).
مؤشر لنظام لينكس يعرض أوقات الأذان (أوقات الصلاة)

It depends on the libraries: -litl -lm -lgtk-3 -lgdk-3 -latk-1.0 -lgio-2.0 -lpangocairo-1.0 -lgdk_pixbuf-2.0 -lcairo-gobject -lpango-1.0 -lcairo -lgobject-2.0 -lglib-2.0 -lappindicator3 -ldbusmenu-glib -lgstreamer-1.0 -lsqlite3 -lwebkitgtk-3.0

Keywords:
- 
أذان, صلاة, مواقيت, اوقات, اذان, صلاه,وقت,مؤذن,الصلاة,الصلاه, مسلم, مسلمين, اسلام,إسلام, لينكس, يوبنتو
salat, prayer, salats, prayers, adhan, athan, muslim, muslims, islam, linux, ubuntu, c language

Features:
- 
+ shows athan times
+ shows current hijri date (based on the Umm Alqura Calendar)
+ plays athan sound
+ plays notification sound before and after athan
+ shows adhkar in HTML mode

How to use:
- 
make

sudo make install


Set your preferences:
- 
edit the file $HOME/.athantime.conf

add the application (/usr/bin/indicator-athantime) to your startup applications

Adhkar.db is an sqlite3 file. The adhkar are written in HTML.You can add others and customize the styles of the existing ones.


TODO:
- 
- ...


Acknowledgment:
- 
The work was partially inspired from the Minbar application



By: 
- 
Ouissem Ben Fredj

Email: Ouissem  __ D O T __  BenFredj __  A T __ gmail __ D O T __ com
