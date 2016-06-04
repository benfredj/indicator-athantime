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
#include <libnotify/notify.h>

#define TRACE(...) if (trace) { printf( __VA_ARGS__); fflush(stdout); } else {}
#define MORNING 0
#define EVENING 1
#define SLEEPING 2
struct config configstruct;
bool trace = false;
Prayer ptList[6];
gint next_prayer_id;
GDate *currentDate;
Date *prayerDate;
static gchar *next_prayer_string;
sqlite3 *dhikrdb;
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
  "الصبح",	//sobh
  "الشروق",	//
  "الظهر",	//
  "العصر",	//
  "المغرب",	//
  "العشاء"	//isha
};

/* update period in seconds */
int period = 59;
int dhikrPeriod=1;
gboolean first_run = TRUE;

GSettings *settings;

AppIndicator *indicator;
GtkWidget *indicator_menu;
//GtkWidget *interfaces_menu;

GtkWidget *athantimes_items[6];
GtkWidget *stopathan_item;
GtkWidget *hijri_item;
GtkWidget *adhkar_subitems;
GtkWidget *adhkar_item;
GtkWidget *dhikr_item;
GtkWidget *morningAthkar_item;
GtkWidget *eveningAthkar_item;
GtkWidget *sleepingAthkar_item;
GtkWidget *quit_item;
#define FILENAME ".athantime.conf"
#define MAXBUF 1024
#define DELIM "="

struct config
{
  double lat;
  double lon;
  char city[MAXBUF];
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
  char* beforeTitleHtml;
  char* afterTitleHtml;
  char* afterAthkarHtml;
  char* dhikrSeperator;
};

//char* beforeTitleHtml="<!DOCTYPE html><html><head><meta charset='utf-8' /></head><body style='direction: rtl;'><table style='width:100%'><tr><td style='width:35px'><img src='/usr/share/indicator-athantime/indicator-athantime2.png' /></td><td style='text-align:center;font-weight:bold;'>";
//char* afterTitleHtml="</td></tr><tr><td colspan='2'>";
//char* afterAthkarHtml="</td></tr></table></body></html>";
//char* dhikrSeperator="<hr/>";
gboolean show_dhikr (gpointer data);

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

void
get_config (char *filename)
{
  char tempstr[MAXBUF];
  configstruct.lat = 0;
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

  struct passwd *pw = getpwuid (getuid ());

  TRACE ("Current working dir: %s\n", pw->pw_dir);
  sprintf (tempstr, "%s/%s", pw->pw_dir, filename);

  TRACE ("Openning config file: %s\n", tempstr);
  FILE *file = fopen (tempstr, "r");

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
	      strncpy (configstruct.city, cfline, strlen (cfline)-1);
	    }
	  else if (i == 3)
	    {
	      configstruct.height = atof (cfline);
	    }
	  else if (i == 4)
	    {
	      configstruct.correctiond = atoi (cfline);
	    }
	  else if (i == 5)
	    {
	      configstruct.method = atoi (cfline);
	    }
	  else if (i == 6)
	    {
	      strncpy (configstruct.athan, cfline, strlen (cfline)-1);
	    }
	  else if (i == 7)
	    {
	      strncpy (configstruct.notificationfile, cfline, strlen (cfline)-1);
	    }
	  else if (i == 8)
	    {
	      configstruct.beforeSobh = atoi (cfline);
	      if (configstruct.beforeSobh < 0
		  || configstruct.beforeSobh > 1440 /* one day */ )
		configstruct.beforeSobh = 0;
	    }
	  else if (i == 9)
	    {
	      configstruct.afterSobh = atoi (cfline);
	      if (configstruct.afterSobh < 0
		  || configstruct.afterSobh > 1440 /* one day */ )
		configstruct.afterSobh = 0;
	    }
	  else if (i == 10)
	    {
	      configstruct.beforeDohr = atoi (cfline);
	      if (configstruct.beforeDohr < 0
		  || configstruct.beforeDohr > 1440 /* one day */ )
		configstruct.beforeDohr = 0;
	    }
	  else if (i == 11)
	    {
	      configstruct.afterDohr = atoi (cfline);
	      if (configstruct.afterDohr < 0
		  || configstruct.afterDohr > 1440 /* one day */ )
		configstruct.afterDohr = 0;
	    }
	  else if (i == 12)
	    {
	      configstruct.beforeAsr = atoi (cfline);
	      if (configstruct.beforeAsr < 0
		  || configstruct.beforeAsr > 1440 /* one day */ )
		configstruct.beforeAsr = 0;
	    }
	  else if (i == 13)
	    {
	      configstruct.afterAsr = atoi (cfline);
	      if (configstruct.afterAsr < 0
		  || configstruct.afterAsr > 1440 /* one day */ )
		configstruct.afterAsr = 0;
	    }
	  else if (i == 14)
	    {
	      configstruct.beforeMaghrib = atoi (cfline);
	      if (configstruct.beforeMaghrib < 0
		  || configstruct.beforeMaghrib > 1440 /* one day */ )
		configstruct.beforeMaghrib = 0;
	    }
	  else if (i == 15)
	    {
	      configstruct.afterMaghrib = atoi (cfline);
	      if (configstruct.afterMaghrib < 0
		  || configstruct.afterMaghrib > 1440 /* one day */ )
		configstruct.afterMaghrib = 0;
	    }
	  else if (i == 16)
	    {
	      configstruct.beforeIsha = atoi (cfline);
	      if (configstruct.beforeIsha < 0
		  || configstruct.beforeIsha > 1440 /* one day */ )
		configstruct.beforeIsha = 0;
	    }
	  else if (i == 17)
	    {
	      configstruct.afterIsha = atoi (cfline);
	      if (configstruct.afterIsha < 0
		  || configstruct.afterIsha > 1440 /* one day */ )
		configstruct.afterIsha = 0;
	    }
	  else if (i == 18)
	    {
	      configstruct.morningAthkar = atoi (cfline);
	      if (configstruct.morningAthkar < 0
		  || configstruct.morningAthkar > 1)
		configstruct.morningAthkar = 0;
	    }
	  else if (i == 19)
	    {
	      configstruct.morningAthkarTime = atoi (cfline);
	      if (configstruct.morningAthkarTime < 0
		  || configstruct.morningAthkarTime > 300)
		configstruct.morningAthkarTime = 0;
	    }
	  else if (i == 20)
	    {
	      configstruct.eveningAthkar = atoi (cfline);
	      if (configstruct.eveningAthkar < 0
		  || configstruct.eveningAthkar > 1)
		configstruct.eveningAthkar = 0;
	    }
	  else if (i == 21)
	    {
	      configstruct.eveningAthkarTime = atoi (cfline);
	      if (configstruct.eveningAthkarTime < 0
		  || configstruct.eveningAthkarTime > 300)
		configstruct.eveningAthkarTime = 0;
	    }
	  else if (i == 22)
	    {
	      configstruct.sleepingAthkar = atoi (cfline);
	      if (configstruct.sleepingAthkar < 0
		  || configstruct.sleepingAthkar > 1)
		configstruct.sleepingAthkar = 0;
	    }
	  else if (i == 23)
	    {
	      configstruct.sleepingAthkarTime = atoi (cfline);
	      if (configstruct.sleepingAthkarTime < 0
		  || configstruct.sleepingAthkarTime > 500)
		configstruct.sleepingAthkarTime = 0;
	    }
	  else if (i == 24)
	    {
	      configstruct.athkarPeriod = atoi (cfline);
	      if (configstruct.athkarPeriod < 0
		  || configstruct.athkarPeriod > 500)
		configstruct.athkarPeriod = 0;
	    }
	  else if (i == 25)
	    {
	      configstruct.specialAthkarDuration = atoi (cfline);
	      if (configstruct.specialAthkarDuration < 0
		  || configstruct.specialAthkarDuration > 120)
		configstruct.specialAthkarDuration = 0;
	    }
	  else if (i == 26)
	    {
			//configstruct.beforeTitleHtml = g_malloc (sizeof (gchar) * strlen (cfline));
			configstruct.beforeTitleHtml = calloc (sizeof (char), strlen (cfline));
	      strncpy (configstruct.beforeTitleHtml, cfline, strlen (cfline)-1);
	    }
	  else if (i == 27)
	    {
			//configstruct.afterTitleHtml = g_malloc (sizeof (gchar) * strlen (cfline));
			configstruct.afterTitleHtml = calloc (sizeof (char), strlen (cfline));
	      strncpy (configstruct.afterTitleHtml, cfline, strlen (cfline)-1);
	    }
	  else if (i == 28)
	    {
			//configstruct.afterAthkarHtml = g_malloc (sizeof (gchar) * strlen (cfline));
			configstruct.afterAthkarHtml = calloc (sizeof (char), strlen (cfline));
	      strncpy (configstruct.afterAthkarHtml, cfline, strlen (cfline)-1);
	    }
	  else if (i == 29)
	    {
			//configstruct.dhikrSeperator = g_malloc (sizeof (gchar) * strlen (cfline));
			configstruct.dhikrSeperator = calloc (sizeof (char), strlen (cfline));
	      strncpy (configstruct.dhikrSeperator, cfline, strlen (cfline)-1);
	    }
	  i++;
	}			// End while
      fclose (file);
    }
  {

  }
  //return configstruct;
}

// Taken from minbar
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
GtkWidget *dhikrWindow = NULL;
//GtkWidget *label = NULL;
//GtkWidget *frame = NULL;
GtkWidget *sw = NULL; //scrolledwindow
//GtkWidget *contents = NULL;
//GtkTextBuffer *buffer = NULL;
GtkWidget *webView = NULL;

gboolean
reset_contextmenu (WebKitWebView       *web_view,
               GtkWidget           *default_menu,
               WebKitHitTestResult *hit_test_result,
               gboolean             triggered_with_keyboard,
               gpointer             user_data){
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
		gtk_window_set_title (GTK_WINDOW (dhikrWindow), "برنامج أوقات الأذان: أذكار");
      //gtk_window_set_resizable (GTK_WINDOW (dhikrWindow), FALSE);

      gtk_window_set_keep_above (GTK_WINDOW (dhikrWindow), TRUE);
      gtk_window_set_type_hint (GTK_WINDOW (dhikrWindow),
				GDK_WINDOW_TYPE_HINT_MENU);


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
		
		webView = webkit_web_view_new();
		//g_object_ref(G_OBJECT(webView));
		//g_object_ref_sink(G_OBJECT (webView));
		//g_assert_cmpint(G_OBJECT(webView)->ref_count, ==, 1);
    // This crashed with the original version
    //g_object_unref(webView);
    
      webkit_web_view_set_editable (WEBKIT_WEB_VIEW(webView), FALSE);
      gtk_container_add(GTK_CONTAINER(sw), GTK_WIDGET(webView));
       gtk_container_add (GTK_CONTAINER (dhikrWindow), sw);

      g_signal_connect (G_OBJECT (dhikrWindow), "destroy", G_CALLBACK (gtk_widget_destroyed), &dhikrWindow);
      g_signal_connect (G_OBJECT (dhikrWindow), "enter-notify-event", G_CALLBACK (set_opac), NULL);
      g_signal_connect (G_OBJECT (dhikrWindow), "leave-notify-event", G_CALLBACK (reset_opac), NULL);
      g_signal_connect (G_OBJECT (webView), "context-menu", G_CALLBACK (reset_contextmenu), NULL);
      gtk_container_set_border_width (GTK_CONTAINER (dhikrWindow), 8);

    }
  gtk_window_set_default_size (GTK_WINDOW (dhikrWindow), 300, heightWindow);
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
	//			 NULL, &geometry, geometry_mask);


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

	webkit_web_view_load_string(webView, nass, "text/html", "utf-8", "file://");
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
  webkit_web_view_load_string(webView, html, "text/html", "utf-8", "file://");
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
	size_t htmlSize=10;
	
	htmlSize+=strlen(configstruct.beforeTitleHtml)+strlen(configstruct.afterTitleHtml)+strlen(configstruct.dhikrSeperator)+strlen(configstruct.afterAthkarHtml);
	
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
		 
		  nass = (const char *)sqlite3_column_text (stmt, 1);
		  //printf ("%d: %s\n", row, text);
			if(row == 0){
				 title = (const char *)sqlite3_column_text (stmt, 0);
				htmlSize += strlen(title);
			}
			htmlSize += strlen(nass)+strlen(configstruct.dhikrSeperator);
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
    html = calloc(sizeof (gchar), htmlSize);
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
			if(row == 0){
				 title = (const char *)sqlite3_column_text (stmt, 0);
				 html[0] = '\0';
				 strcat(html, configstruct.beforeTitleHtml);
				 strcat(html, title);
				 strcat(html, configstruct.afterTitleHtml);
			}
			 nass = (const char *)sqlite3_column_text (stmt, 1);
			strcat(html, configstruct.dhikrSeperator);
			strcat(html, nass);
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
    strcat(html, configstruct.afterAthkarHtml);

  initAthkarWindow (250);

  //gtk_window_set_title (GTK_WINDOW (dhikrWindow), title);
  //gtk_window_set_title (GTK_WINDOW (dhikrWindow), "برنامج أوقات الأذان: أذكار");

	webkit_web_view_load_string(WEBKIT_WEB_VIEW(webView), html, "text/html", "utf-8", "file://");
	free(html);
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
	  if (configstruct.sleepingAthkar == 1
	      && difference_prev_prayer == configstruct.sleepingAthkarTime)
	    {
	      waitSpecialAthkar = configstruct.sleepingAthkarTime;
	      show_athkar (SLEEPING);
	    }
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
  next_prayer ();
  update_remaining ();
  //menu_item_set_label(GTK_MENU_ITEM(menu), next_prayer_string);

  getCurrentHijriDate ();
  char currenthijridate[20];
  sprintf (currenthijridate, "%d/%d/%d هـ", hijridate.year,
	   hijridate.month, hijridate.day);
  g_object_set (hijri_item, "label", currenthijridate, NULL);

	show_dhikr (NULL);
  return TRUE;
}

void show_random_dhikr(){
	char sql[MAXBUF];
  const char *title;
  const char *nass;
  char *html = NULL;
  size_t lenNass = 0, htmlSize=10;
	int heightWindow = 150;
	sqlite3_stmt *stmt;
	
	
    htmlSize += strlen(configstruct.beforeTitleHtml)+strlen(configstruct.afterTitleHtml)+strlen(configstruct.dhikrSeperator)+strlen(configstruct.afterAthkarHtml);
    
  sprintf (sql, "SELECT title,nass from athkar WHERE id=%d",rand () % nbAthkar);

	sqlite3_prepare_v2 (dhikrdb, sql, strlen (sql) + 1, &stmt, NULL);
	while (1)
    {
      int s;

      s = sqlite3_step (stmt);
      if (s == SQLITE_ROW)
		{
		 
			nass = (const char *)sqlite3_column_text (stmt, 1);
			title = (const char *)sqlite3_column_text (stmt, 0);
			lenNass = strlen (nass);
			htmlSize += strlen(title)+lenNass;
			
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
	TRACE ("show_random_dhikr: size nass:%d\n", (int)lenNass);
	
	html = g_malloc (sizeof (gchar) * htmlSize);
	
	sprintf(html, "%s%s%s%s%s%s",configstruct.beforeTitleHtml, title, configstruct.afterTitleHtml, configstruct.dhikrSeperator, nass, configstruct.afterAthkarHtml);
  
  
  if (lenNass < 400)
    heightWindow = 200;
  else
    heightWindow = 250;

  initAthkarWindow (heightWindow);

  webkit_web_view_load_string(WEBKIT_WEB_VIEW(webView), html, "text/html", "utf-8", "file://");
  free(html);
  

  if (!gtk_widget_get_visible (dhikrWindow))
    {
      gtk_widget_show_all (dhikrWindow);
    }
  TRACE ("Dhikr shown successfully\n");
}

gboolean
show_dhikr (gpointer data)
{
	TRACE ("show_dhikr: entering\n");
	
	if(dhikrPeriod<configstruct.athkarPeriod){
		dhikrPeriod++;
		return 0;
	}else
		dhikrPeriod=1;
		
	if (difference_prev_prayer>0 && difference_prev_prayer <
      (waitSpecialAthkar + configstruct.specialAthkarDuration)
      && waitSpecialAthkar > 0)
    {
      TRACE ("show_dhikr: skipped\n");
      return 0;
    }
    TRACE ("show_dhikr: processing\n");
    
    show_random_dhikr();

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


      sql = "SELECT count(*) from athkar WHERE title!='أذكار الصباح' AND title!='أذكار المساء' AND title!='أذكار النوم' LIMIT 1";
      //sql = "SELECT count(*) from athkar";

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
}

void cb_show_dhikr(GtkMenuItem *menu_item, gpointer user_data){
	TRACE ("cb_show_dhikr: entering\n");
	dhikrPeriod = 1;
	show_random_dhikr();
}
void cb_show_sleeping_athkar(GtkMenuItem *menu_item, gpointer user_data){
	TRACE ("cb_show_sleeping_athkar: entering\n");
	dhikrPeriod = 1;
	show_athkar(SLEEPING);
}
void cb_show_evening_athkar(GtkMenuItem *menu_item, gpointer user_data){
	TRACE ("cb_show_evening_athkar: entering\n");
	dhikrPeriod = 1;
	show_athkar(EVENING);
}
void cb_show_morning_athkar(GtkMenuItem *menu_item, gpointer user_data){
	TRACE ("cb_show_morning_athkar: entering\n");
	dhikrPeriod = 1;
	show_athkar(MORNING);
}

int
main (int argc, char **argv)
{
  int i;
  //int rc;
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
  if (configstruct.lat == 0)
    {

      printf ("An error occured while reading the config file %s\n",
	      FILENAME);
      fflush (stdout);
      return 0;
    }
  // Struct members
  TRACE ("Reading config file:\n");
  TRACE ("lat: %f\n", configstruct.lat);
  TRACE ("lon: %f\n", configstruct.lon);
  TRACE ("city: %s\n", configstruct.city);
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

  //separator
  GtkWidget *sep = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (indicator_menu), sep);
  
	adhkar_item = gtk_menu_item_new_with_label("الأذكار");
	gtk_menu_shell_append(GTK_MENU_SHELL(indicator_menu), adhkar_item);
	adhkar_subitems = gtk_menu_new();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM(adhkar_item), adhkar_subitems);
	sleepingAthkar_item = gtk_menu_item_new_with_label("أذكار النوم");
	gtk_menu_shell_append(GTK_MENU_SHELL(adhkar_subitems), sleepingAthkar_item);
	g_signal_connect(sleepingAthkar_item, "activate", G_CALLBACK (cb_show_sleeping_athkar), NULL);
	eveningAthkar_item = gtk_menu_item_new_with_label("أذكار المساء");
	gtk_menu_shell_append(GTK_MENU_SHELL(adhkar_subitems), eveningAthkar_item);
	g_signal_connect(eveningAthkar_item, "activate", G_CALLBACK (cb_show_evening_athkar), NULL);
	morningAthkar_item = gtk_menu_item_new_with_label("أذكار الصباح");
	gtk_menu_shell_append(GTK_MENU_SHELL(adhkar_subitems), morningAthkar_item);
	g_signal_connect(morningAthkar_item, "activate", G_CALLBACK (cb_show_morning_athkar), NULL);
	dhikr_item = gtk_menu_item_new_with_label("أظهر ذكرا");
	gtk_menu_shell_append(GTK_MENU_SHELL(adhkar_subitems), dhikr_item);
	g_signal_connect(dhikr_item, "activate", G_CALLBACK (cb_show_dhikr), NULL);

  //separator
  sep = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (indicator_menu), sep);
  
  for (i = 0; i < 6; i++)
    {
      athantimes_items[i] = gtk_menu_item_new_with_label ("");
      gtk_menu_shell_append (GTK_MENU_SHELL (indicator_menu),
			     athantimes_items[i]);
    }

  //separator
  sep = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (indicator_menu), sep);

  stopathan_item = gtk_menu_item_new_with_label ("أوقف الأذان");	//stop athan item
  gtk_menu_shell_append (GTK_MENU_SHELL (indicator_menu), stopathan_item);
  g_signal_connect (stopathan_item, "activate",
		    G_CALLBACK (stop_athan_callback), NULL);

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

  Method *calcMethod = g_malloc (sizeof (Method));
  getMethod (configstruct.method, calcMethod);

  Location *loc = g_malloc (sizeof (Location));

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
      g_snprintf (prayer_time_text[i], 100, "%s: %d:%02d", names[i],
		  ptList[i].hour, ptList[i].minute);

      //gtk_label_set_text(GTK_LABEL(prayer_times_label[i]), prayer_time_text[i]);
      //gtk_menu_item_set_label(GTK_MENU_ITEM(athantimes_items[i]), prayer_time_text[i]);
      g_object_set (athantimes_items[i], "label", prayer_time_text[i], NULL);
    }

  // Next prayer
  next_prayer_string = g_malloc (sizeof (gchar) * 400);
  update_data (NULL);


  // update period in milliseconds
  g_timeout_add (1000 * period, update_data, NULL);

  //if (configstruct.athkarPeriod > 0)
    //g_timeout_add (1000 * 60 * configstruct.athkarPeriod, show_dhikr, NULL);
    //show_dhikr (NULL);

  gtk_main ();


  return 0;
}
