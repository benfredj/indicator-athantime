/*


*/

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <libappindicator/app-indicator.h>
#include <pango/pango.h>
//#include <gio/gio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <itl/prayer.h>

#include <gst/gst.h>
//to play athan sound file

#include <time.h>		/* for time_t */

#include <itl/hijri.h>
#include <sqlite3.h>
//#include <libnotify/notify.h>

#define TRACE(...) if (trace) { printf( __VA_ARGS__); fflush(stdout); } else {}
#define MORNING 0
#define EVENING 1
#define SLEEPING 2
Method *calcMethod;

Location *loc;

struct config configstruct;
bool trace = true;
Prayer ptList[6];
gint next_prayer_id;
GDate *currentDate;
Date *prayerDate;
static gchar *next_prayer_string;
sqlite3 *dhikrdb;
sqlite3 *citiesdb;
int nbAthkar = 0;
gboolean showingAthkar = 0;
int difference_prev_prayer = 0;
int waitSpecialAthkar = 0;

/*gchar *names[] = {
  "\xD8\xA7\xD9\x84\xD8\xB5\xD8\xA8\xD8\xAD",	//sobh
  "\xD8\xA7\xD9\x84\xD8\xB4\xD8\xB1\xD9\x88\xD9\x82",	//
  "\xD8\xA7\xD9\x84\xD8\xB8\xD9\x87\xD8\xB1",	//
  "\xD8\xA7\xD9\x84\xD8\xB9\xD8\xB5\xD8\xB1",	//
  "\xD8\xA7\xD9\x84\xD9\x85\xD8\xBA\xD8\xB1\xD8\xA8",	//
  "\xD8\xA7\xD9\x84\xD8\xB9\xD8\xB4\xD8\xA7\xD8\xA1"	//isha
};*/
gchar *names[] = {
  "الصبح",			//sobh
  "الشروق",		//
  "الظهر",			//
  "العصر",			//
  "المغرب",		//
  "العشاء"		//isha
};

/* update period in seconds */
int period = 59;
int stopAthkar = 0;
int dhikrPeriod = 1;
gboolean first_run = TRUE;

GSettings *settings;

AppIndicator *indicator;
GtkWidget *indicator_menu;
//GtkWidget *interfaces_menu;

GtkWidget *athantimes_items[6];
GtkWidget *stopathan_item;
GtkWidget *hijri_item;
GtkWidget *hijriConversion_item;
GtkWidget *adhkar_subitems;
GtkWidget *adhkar_item;
GtkWidget *dhikr_item;
GtkWidget *morningAthkar_item;
GtkWidget *eveningAthkar_item;
GtkWidget *sleepingAthkar_item;
GtkWidget *stopAthkar_item;
GtkWidget *settings_item;
GtkWidget *quit_item;
#define FILENAME ".athantime.conf"
#define MAXBUF 1024
#define DELIM "="

struct config
{
  double lat;
  double lon;
  int countryId;
  int cityId;
  //char city[MAXBUF];
  double height;
  int correctiond;
  int method;
  char athan[MAXBUF];
  gboolean noAthan;
  char notificationfile[MAXBUF];
  gboolean noNotify;
  int beforeSobh;
  int afterSobh;
  int beforeDohr;
  int afterDohr;
  int beforeAsr;
  int afterAsr;
  int beforeMaghrib;
  int afterMaghrib;
  int beforeIsha;
  int afterIsha;
  gboolean morningAthkar;
  int morningAthkarTime;
  gboolean eveningAthkar;
  int eveningAthkarTime;
  gboolean sleepingAthkar;
  int sleepingAthkarTime;
  int athkarPeriod;
  int specialAthkarDuration;
  char *beforeTitleHtml;
  char *afterTitleHtml;
  char *afterAthkarHtml;
  char *dhikrSeperator;
};

//char* beforeTitleHtml="<!DOCTYPE html><html><head><meta charset='utf-8' /></head><body style='direction: rtl;'><table style='width:100%'><tr><td style='width:35px'><img src='/usr/share/indicator-athantime/indicator-athantime2.png' /></td><td style='text-align:center;font-weight:bold;'>";
//char* afterTitleHtml="</td></tr><tr><td colspan='2'>";
//char* afterAthkarHtml="</td></tr></table></body></html>";
//char* dhikrSeperator="<hr/>";
gboolean show_dhikr (gpointer data);
static void fill_combo_cities ();
static void fill_combo_countries();

sDate hijridate;
void
getCurrentHijriDate ()
{
  time_t mytime;
  struct tm *t_ptr;
  int day, month, year;
  time (&mytime);

  t_ptr = localtime (&mytime);
  /* Set current time values */
  day = t_ptr->tm_mday;
  month = t_ptr->tm_mon + 1;
  year = t_ptr->tm_year + 1900;

  /* umm_alqura code */

  G2H (&hijridate, day, month, year);
  //printf("Umm-AlQura Date:     %d/%d/%d", mydate.day, mydate.month, mydate.year);

}

void incrementTime(int *h, int* m, int incM){
	//printf("%d:%d + %d = ",*h, *m, incM);
  *h +=(*m+incM)/60;
	*m = (*m+incM)%60;
	*h %= 24;
  //printf("%d:%d\n", *h, *m);
}

int writeConfigInFile(char *filename){
	FILE *file = fopen(filename, "w");
	if(!file) return 1;
	fprintf(file, "# Latitude in decimal degree \nlat=%f\n#\n# Longitude in decimal degree\nlon=%f\n#\n# country id\ncountry=%d\n#\n# city id\ncity=%d\n# \n# Height above Sea level in meters\nheight=%f\n# \n# Time Zone, difference from GMT\ncorrectiond=%d\n# \n# specify the prayer calculation method:\n# 0: none. Set to default or 0\n# 1: Egyptian General Authority of Survey\n# 2: University of Islamic Sciences, Karachi (Shaf'i)\n# 3: University of Islamic Sciences, Karachi (Hanafi)\n# 4: Islamic Society of North America\n# 5: Muslim World League (MWL)\n# 6: Umm Al-Qurra, Saudi Arabia\n# 7: Fixed Ishaa Interval (always 90)\n# 8: Egyptian General Authority of Survey (Egypt)\nmethod=%d\n# \n# full path of the athan sound file (OGG file)\nathan=%s\n# \n# full path of the notification sound file (OGG file) (before and after athan)\nnotification.file=%s\n# \n# notification time before and after each athan in minutes\n# set to 0 to disable the notification\nnotification time before sobh=%d\nnotification time after sobh=%d\nnotification time before dohr=%d\nnotification time after dohr=%d\nnotification time before asr=%d\nnotification time after asr=%d\nnotification time before maghrib=%d\nnotification time after maghrib=%d\nnotification time before isha=%d\nnotification time after isha=%d\n# \n# athkar\n# show morning athkar? 1:true, 0:false\nshow morning athkar=%d\n# when to show the morning athkar after Sobh (in minutes)\nmorning athkar minutes after Sobh=%d\n# show evening athkar? 1:true, 0:false\nshow evening athkar=%d\n# when to show the evening athkar after Asr (in minutes)\nevening athkar minutes after Asr=%d\n# show sleeping athkar? 1:true, 0:false\nshow sleeping athkar=%d\n# when to show the sleeping athkar after Isha (in minutes)\nsleeping athkar minutes after Isha=%d\n# how many minutes needed before showing next normal dhikr (in minutes)\nother athkars period in minutes=%d\n# Spacial Athkar (morning, evening and sleeping) showing duration (in minutes)\nSpecial Athkar showing duration=%d\n# show athkar randomly or consecutively\n# show athkar randomly=?\n# \n# athkar window layout (change carefully)\nbeforeTitleHtml=%s\nafterTitleHtml=%s\nafterAthkarHtml=%s\ndhikrSeperator=%s\n# ", 
	configstruct.lat,
  configstruct.lon,
  configstruct.countryId,
  configstruct.cityId,
  configstruct.height,
  configstruct.correctiond,
  configstruct.method,
  configstruct.athan,
  configstruct.notificationfile,
  configstruct.beforeSobh,
  configstruct.afterSobh,
  configstruct.beforeDohr,
  configstruct.afterDohr,
  configstruct.beforeAsr,
  configstruct.afterAsr,
  configstruct.beforeMaghrib,
  configstruct.afterMaghrib,
  configstruct.beforeIsha,
  configstruct.afterIsha,
  configstruct.morningAthkar,
  configstruct.morningAthkarTime,
  configstruct.eveningAthkar,
  configstruct.eveningAthkarTime,
  configstruct.sleepingAthkar,
  configstruct.sleepingAthkarTime,
  configstruct.athkarPeriod,
  configstruct.specialAthkarDuration,
  configstruct.beforeTitleHtml,
  configstruct.afterTitleHtml,
  configstruct.afterAthkarHtml,
  configstruct.dhikrSeperator
);
fclose(file);
return 0;
}
char configfilefullpath[MAXBUF];
void
get_config (char *filename)
{
  //char tempstr[MAXBUF];
  configstruct.lat = 0;
  configstruct.countryId = 199;//saudi arabia
  configstruct.cityId = 8154;//Makkah
  configstruct.noAthan = 1;
  configstruct.noNotify = 1;
  configstruct.beforeSobh = 0;
  configstruct.afterSobh = 0;
  configstruct.beforeDohr = 0;
  configstruct.afterDohr = 0;
  configstruct.beforeAsr = 0;
  configstruct.afterAsr = 0;
  configstruct.beforeMaghrib = 0;
  configstruct.afterMaghrib = 0;
  configstruct.beforeIsha = 0;
  configstruct.afterIsha = 0;
  configstruct.morningAthkar = 0;
  configstruct.eveningAthkar = 0;
  configstruct.sleepingAthkar = 0;
  configstruct.morningAthkarTime = 0;
  configstruct.eveningAthkarTime = 0;
  configstruct.sleepingAthkarTime = 0;
  configstruct.athkarPeriod = 0;
  configstruct.specialAthkarDuration = 0;
  configstruct.beforeTitleHtml = NULL;
  configstruct.afterTitleHtml = NULL;
  configstruct.afterAthkarHtml = NULL;
  configstruct.dhikrSeperator = NULL;

  struct passwd *pw = getpwuid (getuid ());

  TRACE ("Current working dir: %s\n", pw->pw_dir);
  sprintf (configfilefullpath, "%s/%s", pw->pw_dir, filename);

  TRACE ("Openning config file: %s\n", configfilefullpath);
  FILE *file = fopen (configfilefullpath, "r");

  if (file != NULL)
    {
      char line[MAXBUF];
      int i = 0;

      while (fgets (line, sizeof (line), file) != NULL)
	{
	  if (line[0] == '#')
	    continue;
	  char *cfline;
	  cfline = strstr ((char *) line, DELIM);
	  cfline = cfline + strlen (DELIM);

	  if (i == 0)
	    {
	      configstruct.lat = atof (cfline);
	    }
	  else if (i == 1)
	    {
	      configstruct.lon = atof (cfline);
	    }
	  else if (i == 2)
	    {
	      //strncpy (configstruct.countryId, cfline, strlen (cfline) - 1);
	      configstruct.countryId = atoi (cfline);
	    }
	  else if (i == 3)
	    {
	      //strncpy (configstruct.cityId, cfline, strlen (cfline) - 1);
	      configstruct.cityId = atoi (cfline);
	    }
	  else if (i == 4)
	    {
	      configstruct.height = atof (cfline);
	    }
	  else if (i == 5)
	    {
	      configstruct.correctiond = atoi (cfline);
	    }
	  else if (i == 6)
	    {
	      configstruct.method = atoi (cfline);
	    }/*
	 else if (i == 6)
	    {
	      configstruct.ramadanIshaaIncrements = atoi (cfline);
	      if(configstruct.ramadanIshaaIncrements<0 || configstruct.ramadanIshaaIncrements>120)
			configstruct.ramadanIshaaIncrements = 0;
	    }*/
	  else if (i == 7)
	    {
	      strncpy (configstruct.athan, cfline, strlen (cfline) - 1);
	    }
	  else if (i == 8)
	    {
	      strncpy (configstruct.notificationfile, cfline,
		       strlen (cfline) - 1);
	    }
	  else if (i == 9)
	    {
	      configstruct.beforeSobh = atoi (cfline);
	      if (configstruct.beforeSobh < 0
		  || configstruct.beforeSobh > 1440 /* one day */ )
		configstruct.beforeSobh = 0;
	    }
	  else if (i == 10)
	    {
	      configstruct.afterSobh = atoi (cfline);
	      if (configstruct.afterSobh < 0
		  || configstruct.afterSobh > 1440 /* one day */ )
		configstruct.afterSobh = 0;
	    }
	  else if (i == 11)
	    {
	      configstruct.beforeDohr = atoi (cfline);
	      if (configstruct.beforeDohr < 0
		  || configstruct.beforeDohr > 1440 /* one day */ )
		configstruct.beforeDohr = 0;
	    }
	  else if (i == 12)
	    {
	      configstruct.afterDohr = atoi (cfline);
	      if (configstruct.afterDohr < 0
		  || configstruct.afterDohr > 1440 /* one day */ )
		configstruct.afterDohr = 0;
	    }
	  else if (i == 13)
	    {
	      configstruct.beforeAsr = atoi (cfline);
	      if (configstruct.beforeAsr < 0
		  || configstruct.beforeAsr > 1440 /* one day */ )
		configstruct.beforeAsr = 0;
	    }
	  else if (i == 14)
	    {
	      configstruct.afterAsr = atoi (cfline);
	      if (configstruct.afterAsr < 0
		  || configstruct.afterAsr > 1440 /* one day */ )
		configstruct.afterAsr = 0;
	    }
	  else if (i == 15)
	    {
	      configstruct.beforeMaghrib = atoi (cfline);
	      if (configstruct.beforeMaghrib < 0
		  || configstruct.beforeMaghrib > 1440 /* one day */ )
		configstruct.beforeMaghrib = 0;
	    }
	  else if (i == 16)
	    {
	      configstruct.afterMaghrib = atoi (cfline);
	      if (configstruct.afterMaghrib < 0
		  || configstruct.afterMaghrib > 1440 /* one day */ )
		configstruct.afterMaghrib = 0;
	    }
	  else if (i == 17)
	    {
	      configstruct.beforeIsha = atoi (cfline);
	      if (configstruct.beforeIsha < 0
		  || configstruct.beforeIsha > 1440 /* one day */ )
		configstruct.beforeIsha = 0;
	    }
	  else if (i == 18)
	    {
	      configstruct.afterIsha = atoi (cfline);
	      if (configstruct.afterIsha < 0
		  || configstruct.afterIsha > 1440 /* one day */ )
		configstruct.afterIsha = 0;
	    }
	  else if (i == 19)
	    {
	      configstruct.morningAthkar = atoi (cfline);
	     
	      if (configstruct.morningAthkar < 0
		  || configstruct.morningAthkar > 1){
		  //TRACE("getCOnfig: configstruct.morningAthkar=%d\n",configstruct.morningAthkar);
		    configstruct.morningAthkar = 0;
		   }
	    }
	      
	  else if (i == 20)
	    {
	      configstruct.morningAthkarTime = atoi (cfline);
	      if (configstruct.morningAthkarTime < 0
		  || configstruct.morningAthkarTime > 300){
		    configstruct.morningAthkarTime = 0;
		    configstruct.morningAthkar = 0;
		    //TRACE("getCOnfig 20: configstruct.morningAthkarTime=#%d#\n",configstruct.morningAthkarTime);
		    //TRACE("getCOnfig 20: configstruct.morningAthkar=%d\n",configstruct.morningAthkar);
	      }
	    }
	  else if (i == 21)
	    {
	      configstruct.eveningAthkar = atoi (cfline);
	      if (configstruct.eveningAthkar < 0
		  || configstruct.eveningAthkar > 1)
		configstruct.eveningAthkar = 0;
	    }
	  else if (i == 22)
	    {
	      configstruct.eveningAthkarTime = atoi (cfline);
	      if (configstruct.eveningAthkarTime < 0
		  || configstruct.eveningAthkarTime > 300){
		    configstruct.eveningAthkarTime = 0;
		    configstruct.eveningAthkar = 0;
		  }
	    }
	  else if (i == 23)
	    {
	      configstruct.sleepingAthkar = atoi (cfline);
	      if (configstruct.sleepingAthkar < 0
		  || configstruct.sleepingAthkar > 1)
		configstruct.sleepingAthkar = 0;
	    }
	  else if (i == 24)
	    {
	      configstruct.sleepingAthkarTime = atoi (cfline);
	      if (configstruct.sleepingAthkarTime < 0
		  || configstruct.sleepingAthkarTime > 500){
		    configstruct.sleepingAthkarTime = 0;
		    configstruct.sleepingAthkar = 0;
	      }
	    }
	  else if (i == 25)
	    {
	      configstruct.athkarPeriod = atoi (cfline);
	      if (configstruct.athkarPeriod < 0
		  || configstruct.athkarPeriod > 500)
		configstruct.athkarPeriod = 0;
	    }
	  else if (i == 26)
	    {
	      configstruct.specialAthkarDuration = atoi (cfline);
	      if (configstruct.specialAthkarDuration < 0
		  || configstruct.specialAthkarDuration > 120)
		configstruct.specialAthkarDuration = 0;
	    }
	  else if (i == 27)
	    {
	      //configstruct.beforeTitleHtml = g_malloc (sizeof (gchar) * strlen (cfline));
	      configstruct.beforeTitleHtml =
		calloc (sizeof (char), strlen (cfline));
	      strncpy (configstruct.beforeTitleHtml, cfline,
		       strlen (cfline) - 1);
	    }
	  else if (i == 28)
	    {
	      //configstruct.afterTitleHtml = g_malloc (sizeof (gchar) * strlen (cfline));
	      configstruct.afterTitleHtml =
		calloc (sizeof (char), strlen (cfline));
	      strncpy (configstruct.afterTitleHtml, cfline,
		       strlen (cfline) - 1);
	    }
	  else if (i == 29)
	    {
	      //configstruct.afterAthkarHtml = g_malloc (sizeof (gchar) * strlen (cfline));
	      configstruct.afterAthkarHtml =
		calloc (sizeof (char), strlen (cfline));
	      strncpy (configstruct.afterAthkarHtml, cfline,
		       strlen (cfline) - 1);
	    }
	  else if (i == 30)
	    {
	      //configstruct.dhikrSeperator = g_malloc (sizeof (gchar) * strlen (cfline));
	      configstruct.dhikrSeperator =
		calloc (sizeof (char), strlen (cfline));
	      strncpy (configstruct.dhikrSeperator, cfline,
		       strlen (cfline) - 1);
	    }
	  i++;
	}			// End while
      fclose (file);
    }
  {

  }
  //return configstruct;
}
void getCoordinateOfCity(){
	if(configstruct.lat!=0 && configstruct.lon!=0) return;
	if(configstruct.countryId==0 || configstruct.cityId==0){
		configstruct.countryId=199; //saudi arabia
		configstruct.cityId=8154; //makkah
		//configstruct.correctiond = 3; //GMT + 3
	}
	
  char sql[MAXBUF];
  sqlite3_stmt *stmt;
  
  sprintf(sql,
	"SELECT latitude,longitude,timeZone from citiestable WHERE countryNO='%d' AND cityNO='%d'",
	configstruct.countryId, configstruct.cityId);
  TRACE("getCoordinateOfCity: sql-> %s\n",sql);

  sqlite3_prepare_v2 (citiesdb, sql, strlen (sql) + 1, &stmt, NULL);

  while (1)
    {
      int s;

      s = sqlite3_step (stmt);
      if (s == SQLITE_ROW)
		{

		  configstruct.lat = atof((const char *) sqlite3_column_text (stmt, 0))/10000;
		  configstruct.lon = atof((const char *) sqlite3_column_text (stmt, 1))/10000;
		  configstruct.correctiond = atoi((const char *) sqlite3_column_text (stmt, 2))/100;
		TRACE("getCoordinateOfCity: lat=%f, lon=%f, diffGMT:%d\n",configstruct.lat, configstruct.lon, configstruct.correctiond);
		}
      else if (s == SQLITE_DONE)
		{
		  TRACE ("selection of location done.\n");
		  break;
		}
      else
		{
		  TRACE ("selection of location Failed.\n");
		  return;
		}
    }
  return;
  
}
void
update_date (void)
{
  GTimeVal *curtime = g_malloc (sizeof (GTimeVal));

  currentDate = g_date_new ();
  g_get_current_time (curtime);
  g_date_set_time_val (currentDate, curtime);
  g_free (curtime);

  /* Setting current day */
  prayerDate = g_malloc (sizeof (Date));
  prayerDate->day = g_date_get_day (currentDate);
  prayerDate->month = g_date_get_month (currentDate);
  prayerDate->year = g_date_get_year (currentDate);
  //update_date_label();
  g_free (currentDate);
  
  getCurrentHijriDate ();
  char currenthijridate[20];
  sprintf (currenthijridate, "%d/%d/%d هـ", hijridate.year,
	   hijridate.month, hijridate.day);
  g_object_set (hijri_item, "label", currenthijridate, NULL);
  
}


GMainLoop *loop;

GstElement *pipeline, *source, *demuxer, *decoder, *conv, *sink;
GstBus *bus;
guint bus_watch_id;

void
stop_athan_callback ()
{
  //TRACE ("stopping Athan 0\n");
  // clean up nicely
  if (GST_IS_ELEMENT (pipeline))
    {
      TRACE ("stopping sound\n");
      gst_element_set_state (pipeline, GST_STATE_NULL);
      gst_object_unref (GST_OBJECT (pipeline));
      g_source_remove (bus_watch_id);
      g_main_loop_quit (loop);
      g_main_loop_unref (loop);
    }
}

static gboolean
bus_call (GstBus * bus, GstMessage * msg, gpointer data)
{
  GMainLoop *loop = (GMainLoop *) data;

  switch (GST_MESSAGE_TYPE (msg))
    {

    case GST_MESSAGE_EOS:
      TRACE ("End of stream\n");
      g_main_loop_quit (loop);
      break;

    case GST_MESSAGE_ERROR:
      {
	gchar *debug;
	GError *error;

	gst_message_parse_error (msg, &error, &debug);
	g_free (debug);

	g_printerr ("Error: %s\n", error->message);
	g_error_free (error);

	g_main_loop_quit (loop);
	break;
      }
    default:
      break;
    }

  return TRUE;
}


static void
on_pad_added (GstElement * element, GstPad * pad, gpointer data)
{
  GstPad *sinkpad;
  GstElement *decoder = (GstElement *) data;

  // We can now link this pad with the vorbis-decoder sink pad
  TRACE ("Dynamic pad created, linking demuxer/decoder\n");

  sinkpad = gst_element_get_static_pad (decoder, "sink");

  gst_pad_link (pad, sinkpad);

  gst_object_unref (sinkpad);
}

void
play_soundfile (char *filename)
{

  loop = g_main_loop_new (NULL, FALSE);

  /* Create gstreamer elements */
  pipeline = gst_pipeline_new ("audio-player");
  source = gst_element_factory_make ("filesrc", "file-source");
  demuxer = gst_element_factory_make ("oggdemux", "ogg-demuxer");
  decoder = gst_element_factory_make ("vorbisdec", "vorbis-decoder");
  conv = gst_element_factory_make ("audioconvert", "converter");
  sink = gst_element_factory_make ("autoaudiosink", "audio-output");

  if (!pipeline || !source || !demuxer || !decoder || !conv || !sink)
    {
      g_printerr ("One element could not be created. Exiting.\n");
      return;
    }

  /* Set up the pipeline */

  /* we set the input filename to the source element */
  g_object_set (G_OBJECT (source), "location", filename, NULL);

  /* we add a message handler */
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  bus_watch_id = gst_bus_add_watch (bus, bus_call, loop);
  gst_object_unref (bus);

  /* we add all elements into the pipeline */
  /* file-source | ogg-demuxer | vorbis-decoder | converter | alsa-output */
  gst_bin_add_many (GST_BIN (pipeline),
		    source, demuxer, decoder, conv, sink, NULL);

  /* we link the elements together */
  /* file-source -> ogg-demuxer ~> vorbis-decoder -> converter -> alsa-output */
  gst_element_link (source, demuxer);
  gst_element_link_many (decoder, conv, sink, NULL);
  g_signal_connect (demuxer, "pad-added", G_CALLBACK (on_pad_added), decoder);

  /* note that the demuxer will be linked to the decoder dynamically.
     The reason is that Ogg may contain various streams (for example
     audio and video). The source pad(s) will be created at run time,
     by the demuxer when it detects the amount and nature of streams.
     Therefore we connect a callback function which will be executed
     when the "pad-added" is emitted. */


  /* Set the pipeline to "playing" state */
  TRACE ("gstreamer: Now playing: %s\n", filename);
  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  g_main_loop_run (loop);

  /* Out of the main loop, clean up nicely */
  TRACE ("gstreamer: Returned, stopping playback\n");
  stop_athan_callback ();
}

// Taken from minbar
void
next_prayer (void)
{
  /* current time */
  time_t result;
  struct tm *curtime;
  result = time (NULL);
  curtime = localtime (&result);

  int i;
  for (i = 0; i < 6; i++)
    {
      if (i == 1)
	{
	  continue;
	}			/* skip shorouk */
      next_prayer_id = i;
      if (ptList[i].hour > curtime->tm_hour ||
	  (ptList[i].hour == curtime->tm_hour &&
	   ptList[i].minute >= curtime->tm_min))
	{
	  return;
	}
    }

  next_prayer_id = 0;
}

void
set_opac (GtkWidget * widget, gpointer data)
{
  //GdkRGBA col = { 255, 255, 255 };
  //gtk_widget_override_background_color(widget, GTK_STATE_PRELIGHT, &col);
  gtk_widget_set_opacity (widget, 0.5);
}

void
reset_opac (GtkWidget * widget, gpointer data)
{
  //GdkRGBA col = { 255, 255, 255 };
  //gtk_widget_override_background_color(widget, GTK_STATE_PRELIGHT, &col);
  gtk_widget_set_opacity (widget, 1);
}




/*
 * //using notification
static int show_dhikr_callback(void *data, int argc, char **argv, char **azColName){
   
	notify_init ("أذكار");
	//NotifyNotification * dhikr = notify_notification_new (argv[0], argv[1], "dialog-information");
	NotifyNotification * dhikr = notify_notification_new (argv[0], argv[1], "indicator-athantime");
	notify_notification_set_timeout(dhikr, strlen(argv[1])*300); // 3* strlen(dhikr)/10 seconds. Note that the timeout may be ignored by the notification server. 
	//notify_notification_set_timeout(dhikr, strlen(argv[1])*100+10000); // 10 seconds + strlen(dhikr)/10 seconds
	notify_notification_show (dhikr, NULL);
	g_object_unref(G_OBJECT(dhikr));
	notify_uninit();
	do_dialog(argv[0], argv[1]);
   TRACE("Dhikr shown, len=%d\n", (int)strlen(argv[1]));
   //int i;
   //fprintf(stderr, "%s: ", (const char*)data);
   //for(i=0; i<argc; i++){
   //   printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
   //}
   //printf("\n");
   return 0;
}
*/
GtkWidget *athanfile = NULL;
GtkWidget *notificationfile = NULL;
GtkWidget *calculationMethod = NULL;
GtkWidget *configWindow = NULL;
GtkWidget* countryCombo=NULL;
GtkWidget* cityCombo=NULL;
GtkWidget *cityLat = NULL;
GtkWidget *cityLon = NULL;
GtkWidget *cityTimeDiff = NULL;
GtkWidget *citySealevel = NULL;
GtkWidget *beforesobhCombo = NULL;
GtkWidget *beforedohrCombo = NULL;
GtkWidget *beforeasrCombo = NULL;
GtkWidget *beforemaghribCombo = NULL;
GtkWidget *beforeishaaCombo = NULL;
GtkWidget *aftersobhCombo = NULL;
GtkWidget *afterdohrCombo = NULL;
GtkWidget *afterasrCombo = NULL;
GtkWidget *aftermaghribCombo = NULL;
GtkWidget *afterishaaCombo = NULL;
GtkWidget *sleepingAthkarCombo = NULL;
GtkWidget *eveningAthkarCombo = NULL;
GtkWidget *morningAthkarCombo = NULL;
GtkWidget *specialAthkarDurationCombo = NULL;
GtkWidget *otherAthkarperiodCombo = NULL;
GtkTextBuffer *beforeTitleHtmlBuffer = NULL;
GtkTextBuffer *afterTitleHtmlBuffer = NULL;
GtkTextBuffer *afterAthkarHtmlBuffer = NULL;
GtkTextBuffer *dhikrSeperatorBuffer = NULL;


void cb_config_window_destroy(){
	  if (gtk_widget_get_visible (configWindow))
    {
      gtk_widget_hide (configWindow);
    }
}
void cb_config_window_ok(){
	
	int i;
	
	configstruct.lat = atof (gtk_entry_get_text(GTK_ENTRY(cityLat)));
	configstruct.lon = atof (gtk_entry_get_text(GTK_ENTRY(cityLon)));
	configstruct.countryId = atoi (gtk_combo_box_get_active_id(GTK_COMBO_BOX(countryCombo)));
	configstruct.cityId = atoi (gtk_combo_box_get_active_id(GTK_COMBO_BOX(cityCombo)));
	configstruct.height = atof (gtk_entry_get_text(GTK_ENTRY(citySealevel)));
	configstruct.correctiond = atoi (gtk_entry_get_text(GTK_ENTRY(cityTimeDiff)));
	configstruct.method = atoi (gtk_combo_box_get_active_id(GTK_COMBO_BOX(calculationMethod)));
	strncpy (configstruct.athan, gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(athanfile)), strlen (gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(athanfile))));
	strncpy (configstruct.notificationfile, gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(notificationfile)),strlen (gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(notificationfile))));
	configstruct.beforeSobh = atoi (gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(beforesobhCombo)));
	configstruct.afterSobh = atoi (gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(aftersobhCombo)));
	configstruct.beforeDohr = atoi (gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(beforedohrCombo)));
	configstruct.afterDohr = atoi (gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(afterdohrCombo)));
	configstruct.beforeAsr = atoi (gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(beforeasrCombo)));
	configstruct.afterAsr = atoi (gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(afterasrCombo)));
	configstruct.beforeMaghrib = atoi (gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(beforemaghribCombo)));
	configstruct.afterMaghrib = atoi (gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(aftermaghribCombo)));
	configstruct.beforeIsha = atoi (gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(beforeishaaCombo)));
	configstruct.afterIsha = atoi (gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(afterishaaCombo)));
	
	configstruct.morningAthkar = atoi (gtk_combo_box_get_active_id(GTK_COMBO_BOX(morningAthkarCombo)));
	if(configstruct.morningAthkar == -1) configstruct.morningAthkar=0; else configstruct.morningAthkar=1;
	configstruct.morningAthkarTime = atoi (gtk_combo_box_get_active_id(GTK_COMBO_BOX(morningAthkarCombo)));
	
	configstruct.eveningAthkar = atoi (gtk_combo_box_get_active_id(GTK_COMBO_BOX(eveningAthkarCombo)));
	if(configstruct.eveningAthkar == -1) configstruct.eveningAthkar=0; else configstruct.eveningAthkar=1;
	configstruct.eveningAthkarTime = atoi (gtk_combo_box_get_active_id(GTK_COMBO_BOX(eveningAthkarCombo)));
	
	configstruct.sleepingAthkar = atoi (gtk_combo_box_get_active_id(GTK_COMBO_BOX(sleepingAthkarCombo)));
	if(configstruct.sleepingAthkar == -1) configstruct.sleepingAthkar=0; else configstruct.sleepingAthkar=1;
	configstruct.sleepingAthkarTime = atoi (gtk_combo_box_get_active_id(GTK_COMBO_BOX(sleepingAthkarCombo)));
	
	configstruct.athkarPeriod = atoi (gtk_combo_box_get_active_id(GTK_COMBO_BOX(otherAthkarperiodCombo)));
	configstruct.specialAthkarDuration = atoi (gtk_combo_box_get_active_id(GTK_COMBO_BOX(specialAthkarDurationCombo)));
	
	GtkTextIter iterStart, iterEnd;
	gtk_text_buffer_get_start_iter (beforeTitleHtmlBuffer, &iterStart);
	gtk_text_buffer_get_end_iter (beforeTitleHtmlBuffer, &iterEnd);
	char *html = gtk_text_buffer_get_text(beforeTitleHtmlBuffer,&iterStart, &iterEnd, FALSE);
	int s = strlen(html);
	if(configstruct.beforeTitleHtml) free(configstruct.beforeTitleHtml);
	configstruct.beforeTitleHtml =calloc (sizeof (char), s);
	strncpy (configstruct.beforeTitleHtml, html, s);
	for(i=0; i<s; i++) if(configstruct.beforeTitleHtml[i]=='\n' || configstruct.beforeTitleHtml[i]=='\r')  configstruct.beforeTitleHtml[i]=' ';
	
	gtk_text_buffer_get_start_iter (afterTitleHtmlBuffer, &iterStart);
	gtk_text_buffer_get_end_iter (afterTitleHtmlBuffer, &iterEnd);
	html = gtk_text_buffer_get_text(afterTitleHtmlBuffer,&iterStart, &iterEnd, FALSE);
	s = strlen(html);
	if(configstruct.afterTitleHtml) free(configstruct.afterTitleHtml);
	configstruct.afterTitleHtml =calloc (sizeof (char), s);
	strncpy (configstruct.afterTitleHtml, html,s);
	for(i=0; i<s; i++) if(configstruct.afterTitleHtml[i]=='\n' || configstruct.afterTitleHtml[i]=='\r')  configstruct.afterTitleHtml[i]=' ';
	
	gtk_text_buffer_get_start_iter (afterAthkarHtmlBuffer, &iterStart);
	gtk_text_buffer_get_end_iter (afterAthkarHtmlBuffer, &iterEnd);
	html = gtk_text_buffer_get_text(afterAthkarHtmlBuffer,&iterStart, &iterEnd, FALSE);
	s = strlen(html);
	if(configstruct.afterAthkarHtml) free(configstruct.afterAthkarHtml);
	configstruct.afterAthkarHtml =calloc (sizeof (char), s);
	strncpy (configstruct.afterAthkarHtml, html,s);
	for(i=0; i<s; i++) if(configstruct.afterAthkarHtml[i]=='\n' || configstruct.afterAthkarHtml[i]=='\r')  configstruct.afterAthkarHtml[i]=' ';
	
	gtk_text_buffer_get_start_iter (dhikrSeperatorBuffer, &iterStart);
	gtk_text_buffer_get_end_iter (dhikrSeperatorBuffer, &iterEnd);
	html = gtk_text_buffer_get_text(dhikrSeperatorBuffer,&iterStart, &iterEnd, FALSE);
	s = strlen(html);
	if(configstruct.dhikrSeperator) free(configstruct.dhikrSeperator);
	configstruct.dhikrSeperator =calloc (sizeof (char), s);
	strncpy (configstruct.dhikrSeperator, html,s);
	for(i=0; i<s; i++) if(configstruct.dhikrSeperator[i]=='\n' || configstruct.dhikrSeperator[i]=='\r')  configstruct.dhikrSeperator[i]=' ';


	if (gtk_widget_get_visible (configWindow))
    {
      gtk_widget_hide (configWindow);
    }
    writeConfigInFile(configfilefullpath);
}
void cb_reset_city(GtkComboBox *widget, gpointer configWindow){
	gtk_combo_box_set_active (GTK_COMBO_BOX (cityCombo), -1);
}
void cb_fill_city_details(GtkComboBox *widget, gpointer configWindow){
	char sql[MAXBUF];
  sqlite3_stmt *stmt;
  const char * cityNO=NULL;
  const char* countryNO=NULL;
  const char* lat=NULL;
  const char* lon=NULL;
  const char* timezone=NULL;
  
  //cityNO = gtk_combo_box_text_get_active_text (cityCombo);
  //countryNO = gtk_combo_box_text_get_active_text (countryCombo);
  cityNO = gtk_combo_box_get_active_id   (GTK_COMBO_BOX(cityCombo));
  countryNO = gtk_combo_box_get_active_id (GTK_COMBO_BOX(countryCombo));
  
  sprintf(sql,"SELECT latitude,longitude,timeZone from citiestable WHERE countryNO='%d' AND cityNO='%d'",
			atoi(countryNO), atoi(cityNO));
  TRACE("%s\n",sql);

  sqlite3_prepare_v2 (citiesdb, sql, strlen (sql) + 1, &stmt, NULL);

  while (1)
    {
      int s;

      s = sqlite3_step (stmt);
      if (s == SQLITE_ROW)
		{
			char temp[20];
		  lat = (const char *) sqlite3_column_text (stmt, 0);
		  lon = (const char *) sqlite3_column_text (stmt, 1);
		  timezone = (const char *) sqlite3_column_text (stmt, 2);
		  sprintf(temp, "%f", atof(lat)/10000);
		  gtk_entry_set_text(GTK_ENTRY(cityLat), temp);
		  sprintf(temp, "%f", atof(lon)/10000);
		  gtk_entry_set_text(GTK_ENTRY(cityLon), temp);
		  sprintf(temp, "%f", atof(timezone)/100);
		  gtk_entry_set_text(GTK_ENTRY(cityTimeDiff), temp);
		}
      else if (s == SQLITE_DONE)
		{
		  TRACE ("selection of cities done.\n");
		  break;
		}
      else
		{
		  TRACE ("fill combo cities Failed.\n");
		  return;
		}
    }
  return;
}

void cb_fill_cities(GtkComboBox *widget, gpointer configWindow){
	char sql[MAXBUF];
  sqlite3_stmt *stmt;
  const char * cityNO=NULL;
  const char* cityName=NULL;
  const char* countryNO=NULL;
  int row = 0, cityComboId=0;

	countryNO = gtk_combo_box_get_active_id (GTK_COMBO_BOX(countryCombo));
  sprintf(sql,"SELECT cityNO, cityName FROM citiestable WHERE countryNO='%s'",
			countryNO);
  TRACE("cb_fill_cities: sql: %s\n",sql);

  sqlite3_prepare_v2 (citiesdb, sql, strlen (sql) + 1, &stmt, NULL);
	gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT (cityCombo));
  while (1)
    {
      int s;

      s = sqlite3_step (stmt);
      if (s == SQLITE_ROW)
		{

		  cityNO = (const char *) sqlite3_column_text (stmt, 0);
		  cityName = (const char *) sqlite3_column_text (stmt, 1);

		  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (cityCombo), cityNO, cityName);
		  if(configstruct.cityId == atoi(cityNO)){
			  cityComboId = row;
			  gtk_combo_box_set_active (GTK_COMBO_BOX (cityCombo), cityComboId);
			  
		  }
			
		  row++;
		  // TRACE ("country combo filled up\n", row);
		}
      else if (s == SQLITE_DONE)
		{
		  TRACE ("cb_fill_cities: selection of cities done.\n");
		  break;
		}
      else
		{
		  TRACE ("cb_fill_cities: fill combo cities Failed.\n");
		  return;
		}
    }
    
  return;
  
  
}

void
initConfigWindow ()
{
  int i=0;
  
  if (!configWindow)
    {
      configWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_type_hint (GTK_WINDOW (configWindow),
				GDK_WINDOW_TYPE_HINT_MENU);
      gtk_window_set_keep_above (GTK_WINDOW (configWindow), TRUE);
      gtk_window_set_default_size (GTK_WINDOW (configWindow), 500, 500);
      gtk_window_set_title (GTK_WINDOW (configWindow),
			    "برنامج أوقات الأذان: التقويم");
      gtk_window_set_resizable (GTK_WINDOW (configWindow), TRUE);
      g_signal_connect (G_OBJECT (configWindow), "destroy",
			G_CALLBACK (cb_config_window_destroy), &configWindow);
			
      GtkWidget *page1label =	gtk_label_new ("المدينة");
      GtkWidget *gridpage1 = gtk_grid_new ();
      gtk_grid_set_row_spacing (GTK_GRID (gridpage1), 5);
      gtk_grid_set_column_spacing (GTK_GRID (gridpage1), 5);
      GtkWidget *countryLabel =	gtk_label_new ("البلد");
      
      gtk_grid_attach (GTK_GRID (gridpage1), countryLabel, 0, 0, 1, 1);
      countryCombo = gtk_combo_box_text_new ();
      g_signal_connect (G_OBJECT (countryCombo), "changed",
			G_CALLBACK (cb_fill_cities), &configWindow);
			
		//gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (countryCombo), "199", "السعودية");
		//gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (countryCombo), "230", "تونس");
      fill_combo_countries();
      
      //gtk_combo_box_set_active (GTK_COMBO_BOX (countryCombo), 0);
      gtk_grid_attach (GTK_GRID (gridpage1), countryCombo, 0, 1, 1,1);
      
      GtkWidget *cityLabel =	gtk_label_new ("المدينة");
      gtk_grid_attach (GTK_GRID (gridpage1), cityLabel, 1, 0, 1, 1);
      cityCombo = gtk_combo_box_text_new ();
      fill_combo_cities ();
      
      
      g_signal_connect (G_OBJECT (cityCombo), "changed",
			G_CALLBACK (cb_fill_city_details), &configWindow);
      gtk_grid_attach (GTK_GRID (gridpage1), cityCombo, 1, 1, 1,1);
      GtkWidget *latLabel =	gtk_label_new ("خط العرض:");
      //gtk_misc_set_alignment(GTK_MISC(latLabel),0,0);      
      //gtk_label_set_xalign (GTK_LABEL(latLabel),0);
      //gtk_label_set_yalign (GTK_LABEL(latLabel),0);
      
      cityLat = gtk_entry_new();
     
      GtkWidget *lonLabel =	gtk_label_new ("خط الطول:");
      gtk_misc_set_alignment(GTK_MISC(lonLabel),0,0);
      cityLon = gtk_entry_new();
      
      GtkWidget *timezoneLabel =	gtk_label_new ("المنطقة الزمنية:");
      gtk_misc_set_alignment(GTK_MISC(timezoneLabel),0,0);
      cityTimeDiff = gtk_entry_new();
      
      GtkWidget *sealevelLabel =	gtk_label_new ("الارتفاع عن مستوى البحر:");
      gtk_misc_set_alignment(GTK_MISC(sealevelLabel),0,0);
      citySealevel = gtk_entry_new();
      
      cb_fill_city_details(NULL, NULL);
       //g_signal_connect (G_OBJECT (cityLat), "changed", G_CALLBACK (cb_reset_city), &configWindow);
       //g_signal_connect (G_OBJECT (cityLon), "changed", G_CALLBACK (cb_reset_city), &configWindow);
       //g_signal_connect (G_OBJECT (cityTimeDiff), "changed", G_CALLBACK (cb_reset_city), &configWindow);
      
      gtk_grid_attach (GTK_GRID (gridpage1), latLabel, 0, 2, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage1), cityLat, 1, 2, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage1), lonLabel, 0, 3, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage1), cityLon, 1, 3, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage1), timezoneLabel, 0, 4, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage1), cityTimeDiff, 1, 4, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage1), sealevelLabel, 0, 5, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage1), citySealevel, 1, 5, 1, 1);
      
      
      GtkWidget *page2label =	gtk_label_new ("الأذان");
      GtkWidget *gridpage2 = gtk_grid_new ();
      gtk_grid_set_row_spacing (GTK_GRID (gridpage2), 5);
      gtk_grid_set_column_spacing (GTK_GRID (gridpage2), 5);
      GtkWidget *adhanLabel =	gtk_label_new ("طريقة حساب أوقات الصلاة:");
      gtk_misc_set_alignment(GTK_MISC(adhanLabel),0,0);
      calculationMethod = gtk_combo_box_text_new ();
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT(calculationMethod), "0", "إفتراضي");
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT(calculationMethod), "1", "الهيئة المصرية العامة للمساحة");
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT(calculationMethod), "2", "جامعة العلوم الإسلامية بكراتشي (شافعي)");
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT(calculationMethod), "3", "جامعة العلوم الإسلامية بكراتشي (حنفي)");
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT(calculationMethod), "4", "الجمعية الإسلامية لأمريكا الشمالية");
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT(calculationMethod), "5", "رابطة العالم الإسلامي ");
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT(calculationMethod), "6", "أم القرى");
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT(calculationMethod), "7", "العشاء بعد الغروب ب90 دق");
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT(calculationMethod), "8", "الهيئة المصرية العامة للمساحة(مصر)");
      gtk_combo_box_set_active (GTK_COMBO_BOX (calculationMethod), configstruct.method);
      GtkWidget *athanfileLabel =	gtk_label_new ("ملف الأذان:");      
      gtk_misc_set_alignment(GTK_MISC(athanfileLabel),0,0);
      athanfile = gtk_file_chooser_button_new ("تحديد ملف الأذان",
                                          GTK_FILE_CHOOSER_ACTION_OPEN);
      GtkFileFilter *filter = gtk_file_filter_new ();
      gtk_file_filter_add_pattern (filter, "*.ogg");
      gtk_file_chooser_set_filter (GTK_FILE_CHOOSER(athanfile), filter);
      gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(athanfile), configstruct.athan);
      GtkWidget *notificationfileLabel =	gtk_label_new ("ملف التنبيه:");     
      gtk_misc_set_alignment(GTK_MISC(notificationfileLabel),0,0);
      notificationfile = gtk_file_chooser_button_new ("تحديد ملف التنبيه",
                                          GTK_FILE_CHOOSER_ACTION_OPEN);
      gtk_file_chooser_set_filter (GTK_FILE_CHOOSER(notificationfile), filter);
      gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(notificationfile), configstruct.notificationfile);
      gtk_grid_attach (GTK_GRID (gridpage2), adhanLabel, 0, 0, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage2), calculationMethod, 1, 0, 2, 1);
      gtk_grid_attach (GTK_GRID (gridpage2), athanfileLabel, 0, 1, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage2), athanfile, 1, 1, 2, 1);
      gtk_grid_attach (GTK_GRID (gridpage2), notificationfileLabel, 0, 2, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage2), notificationfile, 1, 2, 2, 1);
      
      GtkWidget *notifyLabel =	gtk_label_new ("تنبيه");    
      gtk_misc_set_alignment(GTK_MISC(notifyLabel),0.5,0);
      GtkWidget *notifyBeforeLabel =	gtk_label_new ("قبل");
      GtkWidget *notifyafterLabel =	gtk_label_new ("بعد");
      GtkWidget *sobhLabel =	gtk_label_new ("الصبح:");    
      gtk_misc_set_alignment(GTK_MISC(sobhLabel),0,0);
      GtkWidget *dhorLabel =	gtk_label_new ("الظهر:");    
      gtk_misc_set_alignment(GTK_MISC(dhorLabel),0,0);
      GtkWidget *asrLabel =	gtk_label_new ("العصر:");    
      gtk_misc_set_alignment(GTK_MISC(asrLabel),0,0);
      GtkWidget *maghribLabel =	gtk_label_new ("المغرب:");    
      gtk_misc_set_alignment(GTK_MISC(maghribLabel),0,0);
      GtkWidget *ishaaLabel =	gtk_label_new ("العشاء:");    
      gtk_misc_set_alignment(GTK_MISC(ishaaLabel),0,0);
      beforesobhCombo = gtk_combo_box_text_new ();
      beforedohrCombo = gtk_combo_box_text_new ();
      beforeasrCombo = gtk_combo_box_text_new ();
      beforemaghribCombo = gtk_combo_box_text_new ();
      beforeishaaCombo = gtk_combo_box_text_new ();
      aftersobhCombo = gtk_combo_box_text_new ();
      afterdohrCombo = gtk_combo_box_text_new ();
      afterasrCombo = gtk_combo_box_text_new ();
      aftermaghribCombo = gtk_combo_box_text_new ();
      afterishaaCombo = gtk_combo_box_text_new ();
      
      char temp[3];
      for(i=0; i<60; i++){
		  sprintf(temp,"%d",i);
		gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT(beforesobhCombo), temp, temp);
		gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT(beforedohrCombo), temp, temp);
		gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT(beforeasrCombo), temp, temp);
		gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT(beforemaghribCombo), temp, temp);
		gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT(beforeishaaCombo), temp, temp);
		gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT(aftersobhCombo), temp, temp);
		gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT(afterdohrCombo), temp, temp);
		gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT(afterasrCombo), temp, temp);
		gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT(aftermaghribCombo), temp, temp);
		gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT(afterishaaCombo), temp, temp);
      }
      gtk_combo_box_set_active (GTK_COMBO_BOX (beforesobhCombo), configstruct.beforeSobh);
      gtk_combo_box_set_active (GTK_COMBO_BOX (beforedohrCombo), configstruct.beforeDohr);
      gtk_combo_box_set_active (GTK_COMBO_BOX (beforeasrCombo), configstruct.beforeAsr);
      gtk_combo_box_set_active (GTK_COMBO_BOX (beforemaghribCombo), configstruct.beforeMaghrib);
      gtk_combo_box_set_active (GTK_COMBO_BOX (beforeishaaCombo), configstruct.beforeIsha);
      gtk_combo_box_set_active (GTK_COMBO_BOX (aftersobhCombo), configstruct.afterSobh);
      gtk_combo_box_set_active (GTK_COMBO_BOX (afterdohrCombo), configstruct.afterDohr);
      gtk_combo_box_set_active (GTK_COMBO_BOX (afterasrCombo), configstruct.afterAsr);
      gtk_combo_box_set_active (GTK_COMBO_BOX (aftermaghribCombo), configstruct.afterMaghrib);
      gtk_combo_box_set_active (GTK_COMBO_BOX (afterishaaCombo), configstruct.afterIsha);
      gtk_grid_attach (GTK_GRID (gridpage2), notifyLabel, 0, 3, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage2), notifyBeforeLabel, 1, 3, 1, 1);	
      gtk_grid_attach (GTK_GRID (gridpage2), notifyafterLabel, 2, 3, 1, 1);	
      gtk_grid_attach (GTK_GRID (gridpage2), sobhLabel, 0, 4, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage2), dhorLabel, 0, 5, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage2), asrLabel, 0, 6, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage2), maghribLabel, 0, 7, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage2), ishaaLabel, 0, 8, 1, 1);	
      gtk_grid_attach (GTK_GRID (gridpage2), beforesobhCombo, 1, 4, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage2), beforedohrCombo, 1, 5, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage2), beforeasrCombo, 1, 6, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage2), beforemaghribCombo, 1, 7, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage2), beforeishaaCombo, 1, 8, 1, 1);	
      gtk_grid_attach (GTK_GRID (gridpage2), aftersobhCombo, 2, 4, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage2), afterdohrCombo, 2, 5, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage2), afterasrCombo, 2, 6, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage2), aftermaghribCombo, 2,7, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage2), afterishaaCombo, 2, 8, 1, 1);
      	 
      	 
      GtkWidget *page3label =	gtk_label_new ("الأذكار");
      GtkWidget *gridpage3 = gtk_grid_new ();
      gtk_grid_set_row_spacing (GTK_GRID (gridpage3), 5);
      gtk_grid_set_column_spacing (GTK_GRID (gridpage3), 5);
      //GtkWidget *athkarLabel =	gtk_label_new ("الأذكار");
      //gtk_grid_attach (GTK_GRID (gridpage3), athkarLabel, 0, 0, 1, 1);
      
      //GtkWidget *showSleepingAthkarCheck = gtk_check_button_new_with_label ("إظهار أذكار النوم؟");
      //GtkWidget *showEveningAthkarCheck = gtk_check_button_new_with_label ("إظهار أذكار المساء؟");
      //GtkWidget *showMorningAthkarCheck = gtk_check_button_new_with_label ("إظهار أذكار الصباح؟");
      GtkWidget *sleepingAthkarLabel =	gtk_label_new ("بعد كم دقيقة من العشاء تظهر أذكار النوم؟");    
      gtk_misc_set_alignment(GTK_MISC(sleepingAthkarLabel),0,0);
      GtkWidget *eveningAthkarLabel =	gtk_label_new ("بعد كم دقيقة من العصر تظهر أذكار المساء؟");    
      gtk_misc_set_alignment(GTK_MISC(eveningAthkarLabel),0,0);
      GtkWidget *morningAthkarLabel =	gtk_label_new ("بعد كم دقيقة من الصبح تظهر أذكار الصباح؟");    
      gtk_misc_set_alignment(GTK_MISC(morningAthkarLabel),0,0);
      GtkWidget *specialAthkarDurationLabel =	gtk_label_new ("فترة ظهور أذكار النوم والمساء والصباح:");    
      gtk_misc_set_alignment(GTK_MISC(specialAthkarDurationLabel),0,0);
      GtkWidget *athkarPeriodLabel =	gtk_label_new ("فترة ظهور الأذكار الأخرى:");    
      gtk_misc_set_alignment(GTK_MISC(athkarPeriodLabel),0,0);
      //gtk_label_set_xalign (GTK_LABEL(athkarPeriodLabel),0);
      //gtk_label_set_yalign (GTK_LABEL(athkarPeriodLabel),0);
      
      gtk_grid_attach (GTK_GRID (gridpage3), sleepingAthkarLabel, 0, 0, 2, 1);
      gtk_grid_attach (GTK_GRID (gridpage3), eveningAthkarLabel, 0, 1, 2, 1);
      gtk_grid_attach (GTK_GRID (gridpage3), morningAthkarLabel, 0, 2, 2, 1);
      
      
      sleepingAthkarCombo = gtk_combo_box_text_new ();
      /*GtkWidget *sleepingAthkarEntry = gtk_entry_new();
      numberDetails.min = -1;
      numberDetails.max = 120;
      g_signal_connect (G_OBJECT (sleepingAthkarEntry), "changed",
			G_CALLBACK (cb_config_fill_number), &numberDetails);
		*/	
      eveningAthkarCombo = gtk_combo_box_text_new ();
      morningAthkarCombo = gtk_combo_box_text_new ();
      specialAthkarDurationCombo = gtk_combo_box_text_new ();
      otherAthkarperiodCombo = gtk_combo_box_text_new ();
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT(sleepingAthkarCombo), "-1", "لا تظهر");
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT(eveningAthkarCombo), "-1", "لا تظهر");
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT(morningAthkarCombo), "-1", "لا تظهر");
		
      for(i=0; i<=60; i++){
		  sprintf(temp,"%d",i);
		gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT(sleepingAthkarCombo), temp, temp);
		gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT(eveningAthkarCombo), temp, temp);
		gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT(morningAthkarCombo), temp, temp);
		gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT(specialAthkarDurationCombo), temp, temp);
		gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT(otherAthkarperiodCombo), temp, temp);
      }
      //TRACE("InitConfig: configstruct.sleepingAthkar=%d\n",configstruct.sleepingAthkar);
      if(configstruct.sleepingAthkar == 0)
		gtk_combo_box_set_active (GTK_COMBO_BOX (sleepingAthkarCombo), 0);
	  else
	    gtk_combo_box_set_active (GTK_COMBO_BOX (sleepingAthkarCombo), configstruct.sleepingAthkarTime+1);
      if(configstruct.eveningAthkar == 0)
		gtk_combo_box_set_active (GTK_COMBO_BOX (eveningAthkarCombo), 0);
	  else
        gtk_combo_box_set_active (GTK_COMBO_BOX (eveningAthkarCombo), configstruct.eveningAthkarTime+1);
      if(configstruct.morningAthkar == 0)
		gtk_combo_box_set_active (GTK_COMBO_BOX (morningAthkarCombo), 0);
	  else
        gtk_combo_box_set_active (GTK_COMBO_BOX (morningAthkarCombo), configstruct.morningAthkarTime+1);
      gtk_combo_box_set_active (GTK_COMBO_BOX (specialAthkarDurationCombo), configstruct.specialAthkarDuration);
      gtk_combo_box_set_active (GTK_COMBO_BOX (otherAthkarperiodCombo), configstruct.athkarPeriod);
      
      gtk_grid_attach (GTK_GRID (gridpage3), sleepingAthkarCombo, 2, 0, 1, 1);
      //gtk_grid_attach (GTK_GRID (gridpage3), sleepingAthkarEntry, 2, 0, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage3), eveningAthkarCombo, 2, 1, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage3), morningAthkarCombo, 2, 2, 1, 1);
      
      gtk_grid_attach (GTK_GRID (gridpage3), specialAthkarDurationLabel, 0, 3, 2, 1);
      gtk_grid_attach (GTK_GRID (gridpage3), specialAthkarDurationCombo, 2, 3, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage3), athkarPeriodLabel, 0, 4, 2, 1);
      gtk_grid_attach (GTK_GRID (gridpage3), otherAthkarperiodCombo, 2, 4, 1, 1);
      
      
      GtkWidget *page4label =	gtk_label_new ("شاشة الأذكار");
      GtkWidget *gridpage4 = gtk_grid_new ();
      gtk_grid_set_row_spacing (GTK_GRID (gridpage4), 5);
      gtk_grid_set_column_spacing (GTK_GRID (gridpage4), 5);
      
      GtkWidget *beforeTitleHtmlView;      
      GtkWidget *beforeTitleHtmlLabel =	gtk_label_new ("كود HTML قبل العنوان:");
      beforeTitleHtmlView = gtk_text_view_new ();
      beforeTitleHtmlBuffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (beforeTitleHtmlView));
      gtk_text_buffer_set_text (beforeTitleHtmlBuffer, configstruct.beforeTitleHtml, -1);
      gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (beforeTitleHtmlView), GTK_WRAP_WORD);
      GtkWidget *sw1 = gtk_scrolled_window_new (NULL, NULL);

      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw1),
				      GTK_POLICY_AUTOMATIC,
				      GTK_POLICY_AUTOMATIC);
	  gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(sw1), 100);
	  gtk_scrolled_window_set_min_content_width(GTK_SCROLLED_WINDOW(sw1), 300);
      gtk_container_add (GTK_CONTAINER (sw1), beforeTitleHtmlView);

      GtkWidget *afterTitleHtmlView;
      GtkWidget *afterTitleHtmlLabel =	gtk_label_new ("كود HTML بعد العنوان:");
      afterTitleHtmlView = gtk_text_view_new ();
      afterTitleHtmlBuffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (afterTitleHtmlView));
      gtk_text_buffer_set_text (afterTitleHtmlBuffer, configstruct.afterTitleHtml, -1);
      gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (afterTitleHtmlView), GTK_WRAP_WORD);
      GtkWidget *sw2 = gtk_scrolled_window_new (NULL, NULL);

      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw2),
				      GTK_POLICY_AUTOMATIC,
				      GTK_POLICY_AUTOMATIC);
	  gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(sw2), 50);
	  gtk_scrolled_window_set_min_content_width(GTK_SCROLLED_WINDOW(sw2), 300);
      gtk_container_add (GTK_CONTAINER (sw2), afterTitleHtmlView);
      
      GtkWidget *afterAthkarHtmlView;
      GtkWidget *afterAthkarHtmlLabel =	gtk_label_new ("كود HTML بعد الأذكار:");
      afterAthkarHtmlView = gtk_text_view_new ();
      afterAthkarHtmlBuffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (afterAthkarHtmlView));
      gtk_text_buffer_set_text (afterAthkarHtmlBuffer, configstruct.afterAthkarHtml, -1);
      gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (afterAthkarHtmlView), GTK_WRAP_WORD);
      GtkWidget *sw3 = gtk_scrolled_window_new (NULL, NULL);

      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw3),
				      GTK_POLICY_AUTOMATIC,
				      GTK_POLICY_AUTOMATIC);
	  gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(sw3), 50);
	  gtk_scrolled_window_set_min_content_width(GTK_SCROLLED_WINDOW(sw3), 300);
      gtk_container_add (GTK_CONTAINER (sw3), afterAthkarHtmlView);
      
      
      GtkWidget *dhikrSeperatorView;
      GtkWidget *dhikrSeperatorLabel =	gtk_label_new ("كود HTML بين الأذكار:");
      dhikrSeperatorView = gtk_text_view_new ();
      dhikrSeperatorBuffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dhikrSeperatorView));
      gtk_text_buffer_set_text (dhikrSeperatorBuffer, configstruct.dhikrSeperator, -1);
      gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (dhikrSeperatorView), GTK_WRAP_WORD);
      GtkWidget *sw4 = gtk_scrolled_window_new (NULL, NULL);

      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw4),
				      GTK_POLICY_AUTOMATIC,
				      GTK_POLICY_AUTOMATIC);
	  gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(sw4), 50);
	  gtk_scrolled_window_set_min_content_width(GTK_SCROLLED_WINDOW(sw4), 300);
      gtk_container_add (GTK_CONTAINER (sw4), dhikrSeperatorView);
      
      
      /*gtk_grid_attach (GTK_GRID (gridpage4), beforeTitleHtmlLabel, 0, 0, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage4), sw1, 0, 1, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage4), afterTitleHtmlLabel, 0, 2, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage4), sw2, 0, 3, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage4), afterAthkarHtmlLabel, 0, 4, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage4), sw3, 0, 5, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage4), dhikrSeperatorLabel, 0, 6, 1, 1);
      gtk_grid_attach (GTK_GRID (gridpage4), sw4, 0, 7, 1, 1);
      */
      GtkWidget * vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
      gtk_box_pack_start (GTK_BOX(vbox), beforeTitleHtmlLabel, TRUE, TRUE, 5);
      gtk_box_pack_start (GTK_BOX(vbox), sw1, TRUE, TRUE, 5);
      gtk_box_pack_start (GTK_BOX(vbox), afterTitleHtmlLabel, TRUE, TRUE, 5);
      gtk_box_pack_start (GTK_BOX(vbox), sw2, TRUE, TRUE, 5);
      gtk_box_pack_start (GTK_BOX(vbox), afterAthkarHtmlLabel, TRUE, TRUE, 5);
      gtk_box_pack_start (GTK_BOX(vbox), sw3, TRUE, TRUE, 5);
      gtk_box_pack_start (GTK_BOX(vbox), dhikrSeperatorLabel, TRUE, TRUE, 5);
      gtk_box_pack_start (GTK_BOX(vbox), sw4, TRUE, TRUE, 5);
		
      GtkWidget *notebook = gtk_notebook_new ();
      gtk_notebook_append_page (GTK_NOTEBOOK(notebook),gridpage1,page1label);
      gtk_notebook_append_page (GTK_NOTEBOOK(notebook),gridpage2,page2label);
      gtk_notebook_append_page (GTK_NOTEBOOK(notebook),gridpage3,page3label);
      gtk_notebook_append_page (GTK_NOTEBOOK(notebook),vbox,page4label);
      
      GtkWidget * vboxMain = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
      GtkWidget * vboxButtons = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
      GtkWidget * buttonOk = gtk_button_new_with_label ("موافق");
      GtkWidget * buttonCancel = gtk_button_new_with_label ("إلغاء");
      g_signal_connect (G_OBJECT (buttonCancel), "clicked",
			G_CALLBACK (cb_config_window_destroy), &configWindow);
			
      g_signal_connect (G_OBJECT (buttonOk), "clicked",
			G_CALLBACK (cb_config_window_ok), &configWindow);
      gtk_box_pack_start (GTK_BOX(vboxButtons), buttonOk, TRUE, TRUE, 5);
      gtk_box_pack_start (GTK_BOX(vboxButtons), buttonCancel, TRUE, TRUE, 5);
      
      gtk_box_pack_start (GTK_BOX(vboxMain), notebook, TRUE, TRUE, 5);
      gtk_box_pack_start (GTK_BOX(vboxMain), vboxButtons, TRUE, TRUE, 5);
      
      gtk_container_add (GTK_CONTAINER (configWindow), GTK_WIDGET(vboxMain));

    }

}

void settings_callback(){
	
	initConfigWindow ();
	if (!gtk_widget_get_visible (configWindow))
    {
      gtk_widget_show_all (configWindow);
    }
}
GtkWidget *dateWindow = NULL;
GtkWidget *calendar;
GtkWidget *HijriDateLabelValue;
GtkWidget *salatTimesLabel;
void
calendar_day_selected (GtkWidget * widget)
{
  char hiriDate[256];
  char salatTimes[256];
  
  guint day, month, year;
  sDate UmAlQuraDate;
  sDate cHijriDate;
  Date *prayerDate1 = NULL;
  Prayer ptList1[6];

  gtk_calendar_get_date (GTK_CALENDAR (calendar), &year, &month, &day);
  TRACE ("calendar --> date selected: %d/%d/%d\n",day, month+1, year);
  G2H (&UmAlQuraDate, day, month+1, year);
  h_date(&cHijriDate, day, month+1, year);
  sprintf (hiriDate, "أم القرى: %02d/%02d/%d\n هجري  : %02d/%02d/%d", 
  	UmAlQuraDate.day, UmAlQuraDate.month, UmAlQuraDate.year, 
	cHijriDate.day, cHijriDate.month, cHijriDate.year);
  gtk_label_set_text (GTK_LABEL (HijriDateLabelValue), hiriDate);


  prayerDate1 = g_malloc (sizeof (Date));
  prayerDate1->day = day;
  prayerDate1->month = month+1;
  prayerDate1->year = year;
  getPrayerTimes (loc, calcMethod, prayerDate1, ptList1);

  sprintf (salatTimes,//"——\t——\t——\t——\t——\t\n%s\t%s\t%s\t%s\t%s\n%02d:%02d\t%02d:%02d\t%02d:%02d\t%02d:%02d\t%02d:%02d",
  //"———————————————\n%s\t%s\t%s\t%s\t%s\n%02d:%02d\t%02d:%02d\t%02d:%02d\t%02d:%02d\t%02d:%02d",
  "%s\t%s\t%s\t%s\t%s\n%02d:%02d\t%02d:%02d\t%02d:%02d\t%02d:%02d\t%02d:%02d",
	   "الصبح", "الظهر", "العصر", "المغرب",
	   "العشاء", ptList1[0].hour, ptList1[0].minute,
	   ptList1[2].hour, ptList1[2].minute, ptList1[3].hour,
	   ptList1[3].minute, ptList1[4].hour, ptList1[4].minute,
	   ptList1[5].hour, ptList1[5].minute);
  //char* salatTimes;
  //gtk_label_set_markup (GTK_LABEL (salatTimesLabel), salatTimes);
  //g_free(salatTimes);
  gtk_label_set_text (GTK_LABEL (salatTimesLabel), salatTimes);
  free (prayerDate1);
}

static void
calendar_select_today (GtkWidget * button, gpointer * calendar_data)
{
  time_t mytime;
  struct tm *t_ptr;

  /* Get current dte structure */
  time (&mytime);
  t_ptr = localtime (&mytime);

  gtk_calendar_select_month (GTK_CALENDAR (calendar), t_ptr->tm_mon,
			     t_ptr->tm_year + 1900);
  gtk_calendar_select_day (GTK_CALENDAR (calendar), t_ptr->tm_mday);
  gtk_calendar_mark_day (GTK_CALENDAR (calendar), t_ptr->tm_mday);
}

void
initDateWindow ()
{

  if (!dateWindow)
    {
      dateWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_type_hint (GTK_WINDOW (dateWindow),
				GDK_WINDOW_TYPE_HINT_MENU);
      gtk_window_set_keep_above (GTK_WINDOW (dateWindow), TRUE);
      gtk_window_set_title (GTK_WINDOW (dateWindow),
			    "برنامج أوقات الأذان: التقويم");
      gtk_window_set_resizable (GTK_WINDOW (dateWindow), FALSE);
      g_signal_connect (G_OBJECT (dateWindow), "destroy",
			G_CALLBACK (gtk_widget_destroyed), &dateWindow);


      GtkWidget *grid = gtk_grid_new ();
      gtk_container_add (GTK_CONTAINER (dateWindow), grid);

      gtk_grid_set_row_spacing (GTK_GRID (grid), 5);
      gtk_grid_set_column_spacing (GTK_GRID (grid), 5);
      calendar = gtk_calendar_new ();
      GtkWidget *todayButton = gtk_button_new_with_label ("اليوم");

      GtkWidget *HijriDateLabel =
	gtk_label_new ("التاريخ الهجري: ");
      gtk_label_set_max_width_chars (GTK_LABEL (HijriDateLabel),
				     strlen
				     ("التاريخ الهجري: "));
      HijriDateLabelValue = gtk_label_new ("");
      gtk_label_set_selectable (GTK_LABEL (HijriDateLabelValue), TRUE);
      salatTimesLabel = gtk_label_new ("");
      gtk_label_set_selectable (GTK_LABEL (salatTimesLabel), TRUE);

      gtk_grid_attach (GTK_GRID (grid), calendar, 0, 0, 2, 1);
      gtk_grid_attach (GTK_GRID (grid), todayButton, 0, 1, 2, 1);
      gtk_grid_attach (GTK_GRID (grid), HijriDateLabel, 0, 2, 1, 1);
      gtk_grid_attach (GTK_GRID (grid), HijriDateLabelValue, 1, 2, 1, 1);
      gtk_grid_attach (GTK_GRID (grid), salatTimesLabel, 0, 3, 2, 1);
      g_signal_connect (calendar, "day_selected",
			G_CALLBACK (calendar_day_selected), calendar);
      g_signal_connect (todayButton,
			"clicked", G_CALLBACK (calendar_select_today), NULL);

    }

}

GtkWidget *dhikrWindow = NULL;
//GtkWidget *label = NULL;
//GtkWidget *frame = NULL;
GtkWidget *sw = NULL;		//scrolledwindow
//GtkWidget *contents = NULL;
//GtkTextBuffer *buffer = NULL;
GtkWidget *webView = NULL;

gboolean
reset_contextmenu (WebKitWebView * web_view,
		   GtkWidget * default_menu,
		   WebKitHitTestResult * hit_test_result,
		   gboolean triggered_with_keyboard, gpointer user_data)
{
  return TRUE;
}

void
initAthkarWindow (int heightWindow)
{
  //GdkGeometry geometry;
  //GdkWindowHints geometry_mask;

  if (!dhikrWindow)
    {
      dhikrWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_title (GTK_WINDOW (dhikrWindow),
			    "برنامج أوقات الأذان: أذكار");
      //gtk_window_set_resizable (GTK_WINDOW (dhikrWindow), FALSE);

      gtk_window_set_keep_above (GTK_WINDOW (dhikrWindow), TRUE);
      gtk_window_set_type_hint (GTK_WINDOW (dhikrWindow),
				GDK_WINDOW_TYPE_HINT_MENU);
      gtk_window_set_accept_focus (GTK_WINDOW (dhikrWindow), TRUE);

      sw = gtk_scrolled_window_new (NULL, NULL);

      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
				      GTK_POLICY_AUTOMATIC,
				      GTK_POLICY_AUTOMATIC);

      //gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);


      //contents = gtk_text_view_new ();
      //gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (contents), GTK_WRAP_WORD);
      //gtk_text_view_set_overwrite (GTK_TEXT_VIEW (contents), FALSE);
      //gtk_text_view_set_editable (GTK_TEXT_VIEW (contents), FALSE);
      //gtk_widget_grab_focus (contents);
      //gtk_container_add (GTK_CONTAINER (sw), contents);

      webView = webkit_web_view_new ();
      //g_object_ref(G_OBJECT(webView));
      //g_object_ref_sink(G_OBJECT (webView));
      //g_assert_cmpint(G_OBJECT(webView)->ref_count, ==, 1);
      // This crashed with the original version
      //g_object_unref(webView);

      webkit_web_view_set_editable (WEBKIT_WEB_VIEW (webView), FALSE);
      gtk_container_add (GTK_CONTAINER (sw), GTK_WIDGET (webView));
      
      GtkWidget * vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
      gtk_box_set_homogeneous(GTK_BOX(vbox), FALSE);
      gtk_container_add (GTK_CONTAINER (dhikrWindow), vbox);
    
      GtkAdjustment *adjustment = gtk_adjustment_new (0.0, 0.0, 100.0, 1.0, 100.0, 0.0);
      GtkWidget *spinb = gtk_spin_button_new (adjustment, 1.0, 0);
      gtk_spin_button_set_numeric (GTK_SPIN_BUTTON(spinb), TRUE);
                  
      gtk_box_pack_start (GTK_BOX(vbox), sw, TRUE, TRUE, 1);
      gtk_box_pack_start (GTK_BOX(vbox), spinb, FALSE, TRUE, 1);
      
      //gtk_container_add (GTK_CONTAINER (dhikrWindow), sw);

      g_signal_connect (G_OBJECT (dhikrWindow), "destroy",
			G_CALLBACK (gtk_widget_destroyed), &dhikrWindow);
      g_signal_connect (G_OBJECT (dhikrWindow), "enter-notify-event",
			G_CALLBACK (set_opac), NULL);
      g_signal_connect (G_OBJECT (dhikrWindow), "leave-notify-event",
			G_CALLBACK (reset_opac), NULL);
      g_signal_connect (G_OBJECT (webView), "context-menu",
			G_CALLBACK (reset_contextmenu), NULL);
      gtk_container_set_border_width (GTK_CONTAINER (dhikrWindow), 8);

    }
  gtk_window_set_default_size (GTK_WINDOW (dhikrWindow), 200, heightWindow);
  //g_assert_cmpint(G_OBJECT(webView)->ref_count, ==, 1);
  //geometry_mask = GDK_HINT_MIN_SIZE | GDK_HINT_BASE_SIZE | GDK_HINT_MAX_SIZE;

  //geometry.min_width = 300;
  //geometry.min_height = heightWindow;
  //geometry.max_width = 300;
  //geometry.max_height = heightWindow;
  //geometry.base_width = 0;
  //geometry.base_height = 0;
  //geometry.win_gravity = GDK_GRAVITY_SOUTH_EAST;

  //gtk_window_set_geometry_hints (GTK_WINDOW (dhikrWindow),
  //                       NULL, &geometry, geometry_mask);


  /* Obtaining the buffer associated with the widget. */
  //buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (contents));
}

/*
static int
show_athkar_window (const char *title, const char *nass)
{
  //TRACE("show_athkar_window(%s, %s, %d)\n", title, nass, count);
  TRACE ("show_athkar_window(s, s)\n");
  GdkGeometry geometry;
  GdkWindowHints geometry_mask;
  char *sepString;

  initAthkarWindow (200);

  //gtk_window_set_title (GTK_WINDOW (dhikrWindow), title);
  gtk_window_set_title (GTK_WINDOW (dhikrWindow), "برنامج أوقات الأذان: أذكار");

	webkit_web_view_load_html(webView, nass, "text/html", "utf-8", "file://");
  free(nass);
    

  if (!gtk_widget_get_visible (dhikrWindow))
    {
      gtk_widget_show_all (dhikrWindow);
    }
  return 0;
}
*/
/*
static int
show_dhikr_callback (void *data, int argc, char **argv, char **azColName)
{
  char *title = argv[0];
  char *nass = argv[1];
  char *html = NULL;
  size_t htmlSize = 10;
  TRACE ("show_dhikr_callback: processing\n");
	size_t lenNass = strlen (nass);
	htmlSize += strlen(configstruct.beforeTitleHtml)+strlen(configstruct.afterTitleHtml)+strlen(configstruct.dhikrSeperator)+strlen(configstruct.afterAthkarHtml)+strlen(title)+lenNass;
	TRACE ("show_dhikr_callback: size nass:%d\n", (int)lenNass);
	//html = malloc(htmlSize);
	html = g_malloc (sizeof (gchar) * htmlSize);
	//TRACE ("show_dhikr_callback: size nass:%d\n", (int)lenNass);
	
	sprintf(html, "%s%s%s%s%s%s",configstruct.beforeTitleHtml, title, configstruct.afterTitleHtml, configstruct.dhikrSeperator, nass, configstruct.afterAthkarHtml);
  //TRACE ("show_dhikr_callback: size nass:%d\n", (int)lenNass);
  
  int heightWindow = 150;
  if (lenNass < 400)
    heightWindow = 150;
  //else if(lenNass<400) heightWindow  = 200;
  else
    heightWindow = 200;

  initAthkarWindow (heightWindow);

  //gtk_window_set_title (GTK_WINDOW (dhikrWindow), title);
	gtk_window_set_title (GTK_WINDOW (dhikrWindow), "برنامج أوقات الأذان: أذكار");
  //gtk_text_buffer_set_text (buffer, nass, -1);
  //TRACE("HTML dhikr: %s\n",html);
  webkit_web_view_load_html(webView, html, "text/html", "utf-8", "file://");
  free(html);
  

  if (!gtk_widget_get_visible (dhikrWindow))
    {
      gtk_widget_show_all (dhikrWindow);
    }
  TRACE ("Dhikr shown successfully\n");
  return 0;
}
*/
static void
fill_combo_countries ()
{

  char *sql = NULL;
  sqlite3_stmt *stmt;
  const char * countryNO=NULL;
  const char* countryName=NULL;
  int row = 0, countryComboId=0;

		sql =	"SELECT countryNO, countryName from countriestable";

  sqlite3_prepare_v2 (citiesdb, sql, strlen (sql) + 1, &stmt, NULL);
		  TRACE("fill_combo_countries: configstruct.countryId=%d\n", configstruct.countryId);

  while (1)
    {
      int s;

      s = sqlite3_step (stmt);
      if (s == SQLITE_ROW)
		{

		  countryNO = (const char *) sqlite3_column_text (stmt, 0);
		  countryName = (const char *) sqlite3_column_text (stmt, 1);

		  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (countryCombo), countryNO, countryName);
		  if(configstruct.countryId == atoi(countryNO)){
			  TRACE ("fill_combo_countries: activate country %d.\n", configstruct.countryId);
			  countryComboId = row;
			  gtk_combo_box_set_active (GTK_COMBO_BOX (countryCombo), countryComboId);
			  
		  }
			
		  row++;
		  // TRACE ("country combo filled up\n", row);
		}
      else if (s == SQLITE_DONE)
		{
		  TRACE ("fill_combo_countries: selection of countries done.\n");
		  break;
		}
      else
		{
		  TRACE ("fill_combo_countries: fill combo countries Failed.\n");
		  return;
		}
    }
    
  return;
}
static void
fill_combo_cities ()
{

  char sql[MAXBUF];
  sqlite3_stmt *stmt;
  const char * cityNO=NULL;
  const char* cityName=NULL;
  int row = 0, cityComboId=0;
  const char* countryNO=NULL;
	//countryNO = gtk_combo_box_get_active_id (GTK_COMBO_BOX(countryCombo));
      //sql =	"SELECT cityNO, cityName from citiestable WHERE countryNO='199'";//saudi arabia
		//sql =	"SELECT cityNO, cityName from citiestable WHERE countryNO='230'";//tunisia
		sprintf(sql,"SELECT cityNO, cityName FROM citiestable WHERE countryNO='%d'",
			configstruct.countryId);

  sqlite3_prepare_v2 (citiesdb, sql, strlen (sql) + 1, &stmt, NULL);

  while (1)
    {
      int s;

      s = sqlite3_step (stmt);
      if (s == SQLITE_ROW)
		{

		  cityNO = (const char *) sqlite3_column_text (stmt, 0);
		  cityName = (const char *) sqlite3_column_text (stmt, 1);
		  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT(cityCombo), cityNO, cityName);
		  if(configstruct.cityId == atoi(cityNO)){
			  cityComboId = row;
			  gtk_combo_box_set_active (GTK_COMBO_BOX (cityCombo), cityComboId);
			  
		  }
			
		  row++;
		  // TRACE ("Special Athkar shown: %d\n", row);
		}
      else if (s == SQLITE_DONE)
		{
		  TRACE ("fill_combo_cities: selection of cities done.\n");
		  break;
		}
      else
		{
		  TRACE ("fill_combo_cities: fill combo cities Failed.\n");
		  return;
		}
    }
    
  return;
}

static void
show_athkar_by_string_type (char * stringTypeAthkar)
{

  char sql[MAXBUF];
  char *html = NULL;
  const char *title = NULL;
  const char *nass = NULL;
  sqlite3_stmt *stmt;
  int row = 0;
  size_t htmlSize = 100;

  htmlSize +=
    strlen (configstruct.beforeTitleHtml) +
    strlen (configstruct.afterTitleHtml) +
    strlen (configstruct.dhikrSeperator) +
    strlen (configstruct.afterAthkarHtml)+
    strlen (stringTypeAthkar);

    
  sprintf(sql,"SELECT title,nass FROM athkar WHERE type='%s'",stringTypeAthkar);
  TRACE ("show_athkar_by_string_type:%s\n", sql);
  
  sqlite3_prepare_v2 (dhikrdb, sql, strlen (sql) + 1, &stmt, NULL);
  
  title = stringTypeAthkar;
  
  while (1)
    {
      int s;

      s = sqlite3_step (stmt);
      if (s == SQLITE_ROW)
	{

	  nass = (const char *) sqlite3_column_text (stmt, 1);
	  htmlSize += strlen (nass) + strlen (configstruct.dhikrSeperator);
	  row++;
	  // TRACE ("Special Athkar shown: %d\n", row);
	}
      else if (s == SQLITE_DONE)
	{
	  TRACE ("selection of athkar done.\n");
	  break;
	}
      else
	{
	  TRACE ("Show athkar Failed.\n");
	  return;
	}
    }
  html = calloc (htmlSize, sizeof (gchar));
  sqlite3_reset (stmt);
  row = 0;
  while (1)
    {
      int s;

      s = sqlite3_step (stmt);
      if (s == SQLITE_ROW)
	{

	  if (row == 0)
	    {
	      //title = (const char *) sqlite3_column_text (stmt, 0);
	      html[0] = '\0';
	      strcat (html, configstruct.beforeTitleHtml);
	      strcat (html, title);
	      strcat (html, configstruct.afterTitleHtml);
	    }
	  nass = (const char *) sqlite3_column_text (stmt, 1);
	  strcat (html, configstruct.dhikrSeperator);
	  strcat (html, nass);
	  row++;
	  TRACE ("Special Athkar shown: %d\n", row);
	}
      else if (s == SQLITE_DONE)
	{
	  TRACE ("selection of athkar done.\n");
	  break;
	}
      else
	{
	  TRACE ("Show athkar Failed.\n");
	  return;
	}
    }
  strcat (html, configstruct.afterAthkarHtml);

  initAthkarWindow (200);

  webkit_web_view_load_html (WEBKIT_WEB_VIEW (webView), html, "text/html",
			       "utf-8", "file://");
  free (html);
  html = NULL;

  if (!gtk_widget_get_visible (dhikrWindow))
    {
      gtk_widget_show_all (dhikrWindow);
    }
  TRACE ("Show athkar done.\n");
  return;
}
void
cb_show_athkar_by_string_type (GtkMenuItem * menu_item, gpointer stringTypeAthkar)
{
  TRACE ("cb_show_athkar_by_string_type: entering\n");
  dhikrPeriod = 1;
  show_athkar_by_string_type ((char *)stringTypeAthkar);
}

void initAthkarMenus(){
  sqlite3_stmt *stmt;
  const char *type = NULL;
  char *type2;
  
  char *sql = "SELECT DISTINCT type FROM athkar";

  sqlite3_prepare_v2 (dhikrdb, sql, strlen (sql) + 1, &stmt, NULL);
    
  while (1)
    {
      int s;

      s = sqlite3_step (stmt);
      if (s == SQLITE_ROW)
	{
       type = (const char *)sqlite3_column_text (stmt, 0);
       type2 = calloc(strlen(type)+1, sizeof(gchar));
       strcpy(type2, type);
	   GtkWidget* item = gtk_menu_item_new_with_label (type2);
       gtk_menu_shell_append (GTK_MENU_SHELL (adhkar_subitems), item);
       g_signal_connect (item, "activate",
		    G_CALLBACK (cb_show_athkar_by_string_type), (gpointer)type2);
		    
	}
      else if (s == SQLITE_DONE)
	{
	  TRACE ("selection of athkar done.\n");
	  break;
	}
      else
	{
	  TRACE ("Show athkar Failed.\n");
	  return;
	}
    }
}
static void
show_athkar (int AthkarType)
{

  //char *zErrMsg = 0;
  //int rc;
  char *sql = NULL;
  char *html = NULL;
  const char *title = NULL;
  const char *nass = NULL;
  sqlite3_stmt *stmt;
  int row = 0;
  size_t htmlSize = 100;

  htmlSize +=
    strlen (configstruct.beforeTitleHtml) +
    strlen (configstruct.afterTitleHtml) +
    strlen (configstruct.dhikrSeperator) +
    strlen (configstruct.afterAthkarHtml);

  switch (AthkarType)
    {
    case MORNING:
      TRACE ("Morning Athkar to be shown\n");
      sql =
	"SELECT title,nass from athkar WHERE title='أذكار الصباح'";
      break;
    case EVENING:
      TRACE ("Evening Athkar to be shown\n");
      sql =
	"SELECT title,nass from athkar WHERE title='أذكار المساء'";
      break;
    case SLEEPING:
      TRACE ("Sleeping Athkar to be shown\n");
      sql =
	"SELECT title,nass from athkar WHERE title='أذكار النوم'";
      break;
    }


  sqlite3_prepare_v2 (dhikrdb, sql, strlen (sql) + 1, &stmt, NULL);

  while (1)
    {
      int s;

      s = sqlite3_step (stmt);
      if (s == SQLITE_ROW)
	{

	  nass = (const char *) sqlite3_column_text (stmt, 1);
	  //printf ("%d: %s\n", row, text);
	  if (row == 0)
	    {
	      title = (const char *) sqlite3_column_text (stmt, 0);
	      htmlSize += strlen (title);
	    }
	  htmlSize += strlen (nass) + strlen (configstruct.dhikrSeperator);
	  row++;
	  // TRACE ("Special Athkar shown: %d\n", row);
	}
      else if (s == SQLITE_DONE)
	{
	  TRACE ("selection of athkar done.\n");
	  break;
	}
      else
	{
	  TRACE ("Show athkar Failed.\n");
	  return;
	}
    }
  //html = malloc(htmlSize);
  html = calloc (sizeof (gchar), htmlSize);
  //html = g_malloc (sizeof (gchar) * htmlSize);
  sqlite3_reset (stmt);
  row = 0;
  while (1)
    {
      int s;

      s = sqlite3_step (stmt);
      if (s == SQLITE_ROW)
	{
	  //title = sqlite3_column_text (stmt, 0);

	  //printf ("%d: %s\n", row, text);
	  if (row == 0)
	    {
	      title = (const char *) sqlite3_column_text (stmt, 0);
	      html[0] = '\0';
	      strcat (html, configstruct.beforeTitleHtml);
	      strcat (html, title);
	      strcat (html, configstruct.afterTitleHtml);
	    }
	  nass = (const char *) sqlite3_column_text (stmt, 1);
	  strcat (html, configstruct.dhikrSeperator);
	  strcat (html, nass);
	  row++;
	  TRACE ("Special Athkar shown: %d\n", row);
	}
      else if (s == SQLITE_DONE)
	{
	  TRACE ("selection of athkar done.\n");
	  break;
	}
      else
	{
	  TRACE ("Show athkar Failed.\n");
	  return;
	}
    }
  strcat (html, configstruct.afterAthkarHtml);

  initAthkarWindow (200);

  //gtk_window_set_title (GTK_WINDOW (dhikrWindow), title);
  //gtk_window_set_title (GTK_WINDOW (dhikrWindow), "برنامج أوقات الأذان: أذكار");

  webkit_web_view_load_html (WEBKIT_WEB_VIEW (webView), html, "text/html",
			       "utf-8", "file://");
  free (html);
  html = NULL;

  if (!gtk_widget_get_visible (dhikrWindow))
    {
      gtk_widget_show_all (dhikrWindow);
    }
  TRACE ("Show athkar done.\n");
  return;
}

// Taken and modified from minbar
void
update_remaining (void)
{
  /* converts times to minutes */
  int next_minutes =
    ptList[next_prayer_id].minute + ptList[next_prayer_id].hour * 60;
  time_t result;
  struct tm *curtime;
  int prev_prayer_id;
  gchar *label_guide = "100000000000.000000000";	//maximum length label text, doesn't really work atm

  result = time (NULL);
  curtime = localtime (&result);
  int cur_minutes = curtime->tm_min + curtime->tm_hour * 60;
  if (ptList[next_prayer_id].hour < curtime->tm_hour)
    {
      /* salat is on next day (subh, and even Isha sometimes) after midnight */
      next_minutes += 60 * 24;
    }
  int difference = next_minutes - cur_minutes;
  int hours = difference / 60;
  int minutes = difference % 60;
  if (difference == 0)
    {
      g_snprintf (next_prayer_string, 400, ("حان موعد صلاة %s"),	// _("Time for %s"), 
		  names[next_prayer_id]);
      app_indicator_set_label (indicator, next_prayer_string, label_guide);

      if (!configstruct.noAthan)
	{
	  stop_athan_callback ();
	  play_soundfile (configstruct.athan);
	}

    }
  else
    {
      g_snprintf (next_prayer_string, 400, "%s %d:%02d-",
		  names[next_prayer_id], hours, minutes);
      app_indicator_set_label (indicator, next_prayer_string, label_guide);
    }

  if (!next_prayer_id)
    prev_prayer_id = 5;
  else if (next_prayer_id == 2)
    prev_prayer_id = 0;
  else
    prev_prayer_id = next_prayer_id - 1;
  int prev_minutes =
    ptList[prev_prayer_id].minute + ptList[prev_prayer_id].hour * 60;

  if (ptList[prev_prayer_id].hour > curtime->tm_hour)
    {
      /* salat is on prev day (subh, and even Isha sometimes) before midnight */
      cur_minutes += 60 * 24;
    }
  difference_prev_prayer = cur_minutes - prev_minutes;

  TRACE ("next hour: %d, cur hour: %d, diff:%d\n",
	 ptList[prev_prayer_id].hour, curtime->tm_hour,
	 difference_prev_prayer);

  TRACE ("difference prev prayer=%d\n", difference_prev_prayer);
  if (!configstruct.noNotify && difference_prev_prayer >= 0)
    {
      TRACE ("difference prev prayer=%d, salat id:%d\n",
	     difference_prev_prayer, prev_prayer_id);
      switch (prev_prayer_id)
	{
	case 0:		//sobh  

	  if (difference_prev_prayer == configstruct.afterSobh)
	    play_soundfile (configstruct.notificationfile);
	  else if (difference_prev_prayer < configstruct.afterSobh)
	    {
	      g_snprintf (next_prayer_string, 400,
			  "%s منذ %d دقيقة",
			  names[prev_prayer_id], difference_prev_prayer);
	      app_indicator_set_label (indicator, next_prayer_string,
				       label_guide);
	    }

	  if (configstruct.morningAthkar == 1
	      && difference_prev_prayer == configstruct.morningAthkarTime)
	    {
	      waitSpecialAthkar = configstruct.morningAthkarTime;
	      show_athkar (MORNING);
	    }
	  break;
	case 2:		//dohr
	  if (difference_prev_prayer == configstruct.afterDohr)
	    play_soundfile (configstruct.notificationfile);
	  else if (difference_prev_prayer < configstruct.afterDohr)
	    {
	      g_snprintf (next_prayer_string, 400,
			  "%s منذ %d دقيقة",
			  names[prev_prayer_id], difference_prev_prayer);
	      app_indicator_set_label (indicator, next_prayer_string,
				       label_guide);
	    }
	  waitSpecialAthkar = 0;

	  break;
	case 3:		//asr
	  if (difference_prev_prayer == configstruct.afterAsr)
	    play_soundfile (configstruct.notificationfile);
	  else if (difference_prev_prayer < configstruct.afterAsr)
	    {
	      g_snprintf (next_prayer_string, 400,
			  "%s منذ %d دقيقة",
			  names[prev_prayer_id], difference_prev_prayer);
	      app_indicator_set_label (indicator, next_prayer_string,
				       label_guide);
	    }

	  if (configstruct.eveningAthkar == 1
	      && difference_prev_prayer == configstruct.eveningAthkarTime)
	    {
	      waitSpecialAthkar = configstruct.eveningAthkarTime;
	      show_athkar (EVENING);
	    }
	  break;
	case 4:		//maghrib
	  if (difference_prev_prayer == configstruct.afterMaghrib)
	    play_soundfile (configstruct.notificationfile);
	  else if (difference_prev_prayer < configstruct.afterMaghrib)
	    {
	      g_snprintf (next_prayer_string, 400,
			  "%s منذ %d دقيقة",
			  names[prev_prayer_id], difference_prev_prayer);
	      app_indicator_set_label (indicator, next_prayer_string,
				       label_guide);
	    }
	  //waitSpecialAthkar = 0;
	  /*if (configstruct.sleepingAthkar == 1
	      && difference_prev_prayer == configstruct.sleepingAthkarTime)
	    {
	      waitSpecialAthkar = configstruct.sleepingAthkarTime;
	      show_athkar (SLEEPING);
	    }*/
	  break;
	case 5:		//isha
	  //TRACE ("notify after isha?\n");
	  if (difference_prev_prayer == configstruct.afterIsha)
	    play_soundfile (configstruct.notificationfile);
	  else if (difference_prev_prayer < configstruct.afterIsha)
	    {
	      TRACE ("notify after isha?yes\n");
	      g_snprintf (next_prayer_string, 400,
			  "%s منذ %d دقيقة",
			  names[prev_prayer_id], difference_prev_prayer);
	      app_indicator_set_label (indicator, next_prayer_string,
				       label_guide);
	    }

	  if (configstruct.sleepingAthkar == 1
	      && difference_prev_prayer == configstruct.sleepingAthkarTime)
	    {
	      waitSpecialAthkar = configstruct.sleepingAthkarTime;
	      show_athkar (SLEEPING);
	    }
	  break;

	}
    }


  TRACE ("difference next prayer=%d\n", difference);
  if (!configstruct.noNotify && difference)
    {
      TRACE ("difference next prayer=%d\n", difference);
      switch (next_prayer_id)
	{
	case 0:		//sobh                                  
	  if (difference == configstruct.beforeSobh)
	    play_soundfile (configstruct.notificationfile);
	  break;
	case 2:		//dohr
	  if (difference == configstruct.beforeDohr)
	    play_soundfile (configstruct.notificationfile);
	  break;
	case 3:		//asr
	  if (difference == configstruct.beforeAsr)
	    play_soundfile (configstruct.notificationfile);
	  break;
	case 4:		//maghrib
	  if (difference == configstruct.beforeMaghrib)
	    play_soundfile (configstruct.notificationfile);
	  break;
	case 5:		//isha
	  if (difference == configstruct.beforeIsha)
	    play_soundfile (configstruct.notificationfile);
	  break;

	}
    }
}

gboolean
update_data (gpointer data)
{
	int i=0;
    
  getMethod (configstruct.method, calcMethod);
  if(configstruct.method == 6) //Makkah NOTE: fajr was 19 degrees before 1430 hijri
    calcMethod->fajrAng = 18.5;

  loc->degreeLong = configstruct.lon;
  loc->degreeLat = configstruct.lat;
  loc->gmtDiff = configstruct.correctiond;
  loc->dst = 0;
  loc->seaLevel = configstruct.height;
  loc->pressure = 1010;
  loc->temperature = 10;


  update_date ();

  // Prayer time
  getPrayerTimes (loc, calcMethod, prayerDate, ptList);

  gchar *prayer_time_text[6];
  for (i = 0; i < 6; i++)
    {
      prayer_time_text[i] = g_malloc (100);
      //Makkah NOTE: ishaa adhan is after 120 mn of maghrib (instead of 90) with umm alqura method
      if(configstruct.method == 6 && i==5 && hijridate.month==9){
		  incrementTime(&(ptList[i].hour), &(ptList[i].minute), 30);
	  }
		
      g_snprintf (prayer_time_text[i], 100, "%s: %d:%02d", names[i],
		  ptList[i].hour, ptList[i].minute);

      //gtk_label_set_text(GTK_LABEL(prayer_times_label[i]), prayer_time_text[i]);
      //gtk_menu_item_set_label(GTK_MENU_ITEM(athantimes_items[i]), prayer_time_text[i]);
      g_object_set (athantimes_items[i], "label", prayer_time_text[i], NULL);
    }


  next_prayer ();
  update_remaining ();
  //menu_item_set_label(GTK_MENU_ITEM(menu), next_prayer_string);

  

  show_dhikr (NULL);
  return TRUE;
}

void
show_random_dhikr ()
{
  char sql[MAXBUF];
  const char *title;
  const char *nass;
  char *html = NULL;
  size_t lenNass = 0, htmlSize = 100;
  int heightWindow = 150;
  sqlite3_stmt *stmt;


  htmlSize +=
    strlen (configstruct.beforeTitleHtml) +
    strlen (configstruct.afterTitleHtml) +
    strlen (configstruct.dhikrSeperator) +
    strlen (configstruct.afterAthkarHtml);

  sprintf (sql, "SELECT title,nass from athkar LIMIT %d,%d",
	   (rand () % nbAthkar), (rand () % nbAthkar));
	TRACE ("show_random_dhikr SQL=%s.\n", sql);
	
  sqlite3_prepare_v2 (dhikrdb, sql, strlen (sql) + 1, &stmt, NULL);
  while (1)
    {
      int s;

      s = sqlite3_step (stmt);
      if (s == SQLITE_ROW)
	{

	  nass = (const char *) sqlite3_column_text (stmt, 1);
	  title = (const char *) sqlite3_column_text (stmt, 0);
	  if(nass==NULL || title==NULL){
		TRACE ("show_random_dhikr Failed, title or nass is null.\n");
		return;
	  }
	  lenNass = strlen (nass);
	  htmlSize += strlen (title) + lenNass;
		if(!lenNass || !strlen (title)){
			TRACE ("show_random_dhikr Failed, title or nass is empty.\n");
			return;
		}
	  
	  break;
	}
      else if (s == SQLITE_DONE)
	{
	  TRACE ("show_random_dhikr done.\n");
	  break;
	}
      else
	{
	  TRACE ("show_random_dhikr Failed.\n");
	  return;
	}
    }
  TRACE ("show_random_dhikr: size nass:%d\n", (int) lenNass);

  html = g_malloc (sizeof (gchar) * htmlSize);

  sprintf (html, "%s%s%s%s%s%s", configstruct.beforeTitleHtml, title,
	   configstruct.afterTitleHtml, configstruct.dhikrSeperator, nass,
	   configstruct.afterAthkarHtml);


  //if (lenNass < 400)
    heightWindow = 200;
  //else
    //heightWindow = 250;

  initAthkarWindow (heightWindow);

  webkit_web_view_load_html (WEBKIT_WEB_VIEW (webView), html, "text/html",
			       "utf-8", "file://");
  free (html);


  if (!gtk_widget_get_visible (dhikrWindow))
    {
      gtk_widget_show_all (dhikrWindow);
    }
  TRACE ("Dhikr shown successfully\n");
}

gboolean
show_dhikr (gpointer data)
{
	
  if(stopAthkar) return TRUE;
  
  TRACE ("show_dhikr: entering\n");

  if (dhikrPeriod < configstruct.athkarPeriod)
    {
      dhikrPeriod++;
      return 0;
    }
  else
    dhikrPeriod = 1;

  if (difference_prev_prayer > 0 && difference_prev_prayer <
      (waitSpecialAthkar + configstruct.specialAthkarDuration)
      && waitSpecialAthkar > 0)
    {
      TRACE ("show_dhikr: skipped\n");
      return 0;
    }
  TRACE ("show_dhikr: processing\n");

  show_random_dhikr ();

  return TRUE;
}

static int
count_athkar_callback (void *data, int argc, char **argv, char **azColName)
{

  nbAthkar = atoi (argv[0]);
  TRACE ("Athkar count=%d\n", nbAthkar);
  return 0;
}

void
initializeAthkarDatabase ()
{
  char *zErrMsg = 0;
  int rc;
  char *sql;
  time_t t;


  rc = sqlite3_open ("/usr/share/indicator-athantime/adhkar.db", &dhikrdb);
  if (rc)
    {
      TRACE ("Can't open Athkar database: %s\n", sqlite3_errmsg (dhikrdb));

      configstruct.morningAthkar = 0;
      configstruct.eveningAthkar = 0;
      configstruct.sleepingAthkar = 0;
      configstruct.athkarPeriod = 0;

    }
  else
    {
      srand ((unsigned) time (&t));
      TRACE ("Opened Athkar database successfully\n");


      //sql ="SELECT count(*) from athkar WHERE title!='أذكار الصباح' AND title!='أذكار المساء' AND title!='أذكار النوم' LIMIT 1";
      sql = "SELECT count(*) from athkar";

      /* Execute SQL statement */
      rc = sqlite3_exec (dhikrdb, sql, count_athkar_callback, NULL, &zErrMsg);
      if (rc != SQLITE_OK)
		{
		  fprintf (stderr, "SQL error: %s\n", zErrMsg);
		  sqlite3_free (zErrMsg);
		}
      else
		{
		  TRACE ("Athkar counted successfully\n");
		}
    }
    rc = sqlite3_open ("/usr/share/indicator-athantime/countries.db", &citiesdb);
  if (rc)
    {
      TRACE ("Can't open countries database: %s\n", sqlite3_errmsg (citiesdb));
    }
}

void
cb_show_dhikr (GtkMenuItem * menu_item, gpointer user_data)
{
  TRACE ("cb_show_dhikr: entering\n");
  dhikrPeriod = 1;
  stopAthkar = 0;
  show_random_dhikr ();
}

void
cb_show_sleeping_athkar (GtkMenuItem * menu_item, gpointer user_data)
{
  TRACE ("cb_show_sleeping_athkar: entering\n");
  dhikrPeriod = 1;
  stopAthkar = 0;
  show_athkar (SLEEPING);
}

void
cb_show_evening_athkar (GtkMenuItem * menu_item, gpointer user_data)
{
  TRACE ("cb_show_evening_athkar: entering\n");
  dhikrPeriod = 1;
  stopAthkar = 0;
  show_athkar (EVENING);
}

void
cb_show_morning_athkar (GtkMenuItem * menu_item, gpointer user_data)
{
  TRACE ("cb_show_morning_athkar: entering\n");
  dhikrPeriod = 1;
  show_athkar (MORNING);
}
void 
cb_stop_athkar(GtkMenuItem * menu_item, gpointer user_data)
{
	stopAthkar = 1 - stopAthkar;
	if(stopAthkar){
		if (dhikrWindow && gtk_widget_get_visible (dhikrWindow))
		{
		  gtk_widget_hide (dhikrWindow);
		}
		stopAthkar_item = gtk_menu_item_new_with_label ("شغل الأذكار");
	}else{
		stopAthkar_item = gtk_menu_item_new_with_label ("أوقف الأذكار");
	}
	
}


void
cb_show_hijriConversionWindow (GtkMenuItem * menu_item, gpointer user_data)
{
  initDateWindow ();
  calendar_select_today (NULL, NULL);
  if (!gtk_widget_get_visible (dateWindow))
    {
      gtk_widget_show_all (dateWindow);
    }
  TRACE ("Hijri Conversion window shown successfully\n");
}

int
main (int argc, char **argv)
{
  int i;
  //int rc;
  GtkWidget *sep;
  if (argc > 1 && strcmp ("--trace", argv[1]) == 0)
    {
      trace = true;
      argc--;
      argv++;
      TRACE ("Tracing is on\n");
      fflush (stdout);
    }

  gtk_init (&argc, &argv);

  // initialize GStreamer
  gst_init (&argc, &argv);


  get_config (FILENAME);	// sets up configstruct
  /*if (configstruct.lat == 0) //fixme: how to check if an error occured while reading the config file
    {

      printf ("An error occured while reading the config file %s\n",
	      FILENAME);
      fflush (stdout);
      return 0;
    }*/
  // Struct members
  TRACE ("Reading config file:\n");
  TRACE ("lat: %f\n", configstruct.lat);
  TRACE ("lon: %f\n", configstruct.lon);
  TRACE ("height: %f\n", configstruct.height);
  TRACE ("correctiond: %d\n", configstruct.correctiond);
  TRACE ("method: %d\n", configstruct.method);
  TRACE ("athan file: %s\n", configstruct.athan);
  TRACE ("notification file: %s\n", configstruct.notificationfile);
  TRACE ("before sobh: %d\n", configstruct.beforeSobh);
  TRACE ("after sobh: %d\n", configstruct.afterSobh);
  TRACE ("before dohr: %d\n", configstruct.beforeDohr);
  TRACE ("after dohr: %d\n", configstruct.afterDohr);
  TRACE ("before asr: %d\n", configstruct.beforeAsr);
  TRACE ("after asr: %d\n", configstruct.afterAsr);
  TRACE ("before maghrib: %d\n", configstruct.beforeMaghrib);
  TRACE ("after maghrib: %d\n", configstruct.afterMaghrib);
  TRACE ("before isha: %d\n", configstruct.beforeIsha);
  TRACE ("After isha: %d\n", configstruct.afterIsha);
  TRACE ("morning athkar: %s\n",
	 configstruct.morningAthkar == 0 ? "false" : "true");
  TRACE ("morning athkar time after sobh: %d\n",
	 configstruct.morningAthkarTime);
  TRACE ("Evening athkar: %s\n",
	 configstruct.eveningAthkar == 0 ? "false" : "true");
  TRACE ("Evening athkar time after Asr: %d\n",
	 configstruct.eveningAthkarTime);
  TRACE ("sleeping athkar: %s\n",
	 configstruct.sleepingAthkar == 0 ? "false" : "true");
  TRACE ("sleeping athkar time after isha: %d\n",
	 configstruct.sleepingAthkarTime);
  TRACE ("Athkar period: %d\n", configstruct.athkarPeriod);
  TRACE ("beforeTitleHtml: %s\n", configstruct.beforeTitleHtml);
  TRACE ("afterTitleHtml: %s\n", configstruct.afterTitleHtml);
  TRACE ("afterAthkarHtml: %s\n", configstruct.afterAthkarHtml);
  TRACE ("dhikrSeperator: %s\n", configstruct.dhikrSeperator);

//writeConfigInFile(".athantime2.conf");
  FILE *file = fopen (configstruct.athan, "r");
  if (file == NULL)
    {
      TRACE ("Athan sound file not set or corrupted: %s!\n",
	     configstruct.athan);
      configstruct.noAthan = 1;
    }
  else
    {
      configstruct.noAthan = 0;
      fclose (file);
    }

  file = fopen (configstruct.notificationfile, "r");
  if (file == NULL)
    {
      TRACE ("Notification sound file not set or corrupted: %s!\n",
	     configstruct.notificationfile);
      configstruct.noNotify = 1;
    }
  else
    {
      configstruct.noNotify = 0;
      fclose (file);
    }
  initializeAthkarDatabase ();

  indicator_menu = gtk_menu_new ();

  getCurrentHijriDate ();
  char currenthijridate[20];
  sprintf (currenthijridate, "%d/%d/%d هـ", hijridate.year,
	   hijridate.month, hijridate.day);
  hijri_item = gtk_menu_item_new_with_label (currenthijridate);	//hijri date item
  gtk_menu_shell_append (GTK_MENU_SHELL (indicator_menu), hijri_item);
  
  GtkWidget *salattimes_item = gtk_menu_item_new_with_label ("أوقات الصلاة");
  gtk_menu_shell_append (GTK_MENU_SHELL (indicator_menu), salattimes_item);
  GtkWidget *salattimes_subitems = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (salattimes_item), salattimes_subitems);
  
  for (i = 0; i < 6; i++)
    {
      athantimes_items[i] = gtk_menu_item_new_with_label ("");
      gtk_menu_shell_append (GTK_MENU_SHELL (salattimes_subitems),
			     athantimes_items[i]);
    }
    
  adhkar_item = gtk_menu_item_new_with_label ("الأذكار");
  gtk_menu_shell_append (GTK_MENU_SHELL (indicator_menu), adhkar_item);
  adhkar_subitems = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (adhkar_item), adhkar_subitems);
  /*
  sleepingAthkar_item =
    gtk_menu_item_new_with_label ("أذكار النوم");
  gtk_menu_shell_append (GTK_MENU_SHELL (adhkar_subitems),
			 sleepingAthkar_item);
  g_signal_connect (sleepingAthkar_item, "activate",
		    G_CALLBACK (cb_show_sleeping_athkar), NULL);
  eveningAthkar_item =
    gtk_menu_item_new_with_label ("أذكار المساء");
  gtk_menu_shell_append (GTK_MENU_SHELL (adhkar_subitems),
			 eveningAthkar_item);
  g_signal_connect (eveningAthkar_item, "activate",
		    G_CALLBACK (cb_show_evening_athkar), NULL);
  morningAthkar_item =
    gtk_menu_item_new_with_label ("أذكار الصباح");
  gtk_menu_shell_append (GTK_MENU_SHELL (adhkar_subitems),
			 morningAthkar_item);
  g_signal_connect (morningAthkar_item, "activate",
		    G_CALLBACK (cb_show_morning_athkar), NULL);
*/		    
  dhikr_item = gtk_menu_item_new_with_label ("أظهر ذكرا");
  gtk_menu_shell_append (GTK_MENU_SHELL (adhkar_subitems), dhikr_item);
  g_signal_connect (dhikr_item, "activate", G_CALLBACK (cb_show_dhikr), NULL);
  
  stopAthkar_item = gtk_menu_item_new_with_label ("أوقف الأذكار");
  gtk_menu_shell_append (GTK_MENU_SHELL (adhkar_subitems), stopAthkar_item);
  g_signal_connect (stopAthkar_item, "activate", G_CALLBACK (cb_stop_athkar), NULL);
  
  sep = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (adhkar_subitems), sep);
  
  initAthkarMenus();
  
  
  hijriConversion_item = gtk_menu_item_new_with_label ("التقويم");	//calendar item
  gtk_menu_shell_append (GTK_MENU_SHELL (indicator_menu),
			 hijriConversion_item);

  //separator
  //sep = gtk_separator_menu_item_new ();
  //gtk_menu_shell_append (GTK_MENU_SHELL (indicator_menu), sep);  

  //separator
  //sep = gtk_separator_menu_item_new ();
  //gtk_menu_shell_append (GTK_MENU_SHELL (indicator_menu), sep);

  

  //separator
  //sep = gtk_separator_menu_item_new ();
  //gtk_menu_shell_append (GTK_MENU_SHELL (indicator_menu), sep);

  g_signal_connect (hijriConversion_item, "activate",
		    G_CALLBACK (cb_show_hijriConversionWindow), NULL);

  stopathan_item = gtk_menu_item_new_with_label ("أوقف الأذان");	//stop athan item
  gtk_menu_shell_append (GTK_MENU_SHELL (indicator_menu), stopathan_item);
  g_signal_connect (stopathan_item, "activate",
		    G_CALLBACK (stop_athan_callback), NULL);

  //sep = gtk_separator_menu_item_new ();
  //gtk_menu_shell_append (GTK_MENU_SHELL (indicator_menu), sep);

  settings_item = gtk_menu_item_new_with_label ("الإعدادات"); //settings item
  gtk_menu_shell_append (GTK_MENU_SHELL (indicator_menu), settings_item);
  g_signal_connect (settings_item, "activate",
		    G_CALLBACK (settings_callback), NULL);

  //quit item
  //quit_item = gtk_menu_item_new_with_label("\xD8\xA3\xD8\xBA\xD9\x84\xD9\x82"); //quit item
  //gtk_menu_shell_append(GTK_MENU_SHELL(indicator_menu), quit_item);
  //g_signal_connect(quit_item, "activate", G_CALLBACK (gtk_main_quit), NULL);

  gtk_widget_show_all (indicator_menu);

  //create app indicator
  indicator =
    app_indicator_new ("athantimes", "indicator-athantime",
		       APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
  app_indicator_set_status (indicator, APP_INDICATOR_STATUS_ACTIVE);
  app_indicator_set_label (indicator, "athantimes", "athantimes");
  app_indicator_set_menu (indicator, GTK_MENU (indicator_menu));

  calcMethod = g_malloc (sizeof (Method));
  
  loc = g_malloc (sizeof (Location));

  // Next prayer
  next_prayer_string = g_malloc (sizeof (gchar) * 400);
  getCoordinateOfCity();
  update_data (NULL);


  // update period in milliseconds
  g_timeout_add (1000 * period, update_data, NULL);

  //if (configstruct.athkarPeriod > 0)
  //g_timeout_add (1000 * 60 * configstruct.athkarPeriod, show_dhikr, NULL);
  //show_dhikr (NULL);

  gtk_main ();


  return 0;
}
