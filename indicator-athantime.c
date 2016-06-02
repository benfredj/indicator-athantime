/*


*/

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include <gtk/gtk.h>
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
gboolean first_run = TRUE;

GSettings *settings;

AppIndicator *indicator;
GtkWidget *indicator_menu;
GtkWidget *interfaces_menu;

GtkWidget *athantimes_items[6];
GtkWidget *stopathan_item;
GtkWidget *hijri_item;
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
};

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
	      memcpy (tempstr, cfline, strlen (cfline));
	      configstruct.lat = atof (tempstr);
	      //printf("%s",configstruct.imgserver);
	    }
	  else if (i == 1)
	    {
	      memcpy (tempstr, cfline, strlen (cfline));
	      configstruct.lon = atof (tempstr);
	      //printf("%s",configstruct.ccserver);
	    }
	  else if (i == 2)
	    {
	      memcpy (configstruct.city, cfline, strlen (cfline));
	      configstruct.city[strlen (configstruct.city) - 1] = '\0';
	      //printf("%s",configstruct.port);
	    }
	  else if (i == 3)
	    {
	      memcpy (tempstr, cfline, strlen (cfline));
	      configstruct.height = atof (tempstr);
	      //printf("%s",configstruct.imagename);
	    }
	  else if (i == 4)
	    {
	      memcpy (tempstr, cfline, strlen (cfline));
	      configstruct.correctiond = atoi (tempstr);
	      //printf("%s",configstruct.getcmd);
	    }
	  else if (i == 5)
	    {
	      memcpy (tempstr, cfline, strlen (cfline));
	      configstruct.method = atoi (tempstr);
	      //printf("%s",configstruct.getcmd);
	    }
	  else if (i == 6)
	    {
	      memcpy (configstruct.athan, cfline, strlen (cfline));
	      configstruct.athan[strlen (configstruct.athan) - 1] = '\0';
	    }
	  else if (i == 7)
	    {
	      memcpy (configstruct.notificationfile, cfline, strlen (cfline));
	      configstruct.
		notificationfile[strlen (configstruct.notificationfile) - 1] =
		'\0';
	    }
	  else if (i == 8)
	    {
	      memcpy (tempstr, cfline, strlen (cfline));
	      configstruct.beforeSobh = atoi (tempstr);
	      if (configstruct.beforeSobh < 0
		  || configstruct.beforeSobh > 1440 /* one day */ )
		configstruct.beforeSobh = 0;
	    }
	  else if (i == 9)
	    {
	      memcpy (tempstr, cfline, strlen (cfline));
	      configstruct.afterSobh = atoi (tempstr);
	      if (configstruct.afterSobh < 0
		  || configstruct.afterSobh > 1440 /* one day */ )
		configstruct.afterSobh = 0;
	    }
	  else if (i == 10)
	    {
	      memcpy (tempstr, cfline, strlen (cfline));
	      configstruct.beforeDohr = atoi (tempstr);
	      if (configstruct.beforeDohr < 0
		  || configstruct.beforeDohr > 1440 /* one day */ )
		configstruct.beforeDohr = 0;
	    }
	  else if (i == 11)
	    {
	      memcpy (tempstr, cfline, strlen (cfline));
	      configstruct.afterDohr = atoi (tempstr);
	      if (configstruct.afterDohr < 0
		  || configstruct.afterDohr > 1440 /* one day */ )
		configstruct.afterDohr = 0;
	    }
	  else if (i == 12)
	    {
	      memcpy (tempstr, cfline, strlen (cfline));
	      configstruct.beforeAsr = atoi (tempstr);
	      if (configstruct.beforeAsr < 0
		  || configstruct.beforeAsr > 1440 /* one day */ )
		configstruct.beforeAsr = 0;
	    }
	  else if (i == 13)
	    {
	      memcpy (tempstr, cfline, strlen (cfline));
	      configstruct.afterAsr = atoi (tempstr);
	      if (configstruct.afterAsr < 0
		  || configstruct.afterAsr > 1440 /* one day */ )
		configstruct.afterAsr = 0;
	    }
	  else if (i == 14)
	    {
	      memcpy (tempstr, cfline, strlen (cfline));
	      configstruct.beforeMaghrib = atoi (tempstr);
	      if (configstruct.beforeMaghrib < 0
		  || configstruct.beforeMaghrib > 1440 /* one day */ )
		configstruct.beforeMaghrib = 0;
	    }
	  else if (i == 15)
	    {
	      memcpy (tempstr, cfline, strlen (cfline));
	      configstruct.afterMaghrib = atoi (tempstr);
	      if (configstruct.afterMaghrib < 0
		  || configstruct.afterMaghrib > 1440 /* one day */ )
		configstruct.afterMaghrib = 0;
	    }
	  else if (i == 16)
	    {
	      memcpy (tempstr, cfline, strlen (cfline));
	      configstruct.beforeIsha = atoi (tempstr);
	      if (configstruct.beforeIsha < 0
		  || configstruct.beforeIsha > 1440 /* one day */ )
		configstruct.beforeIsha = 0;
	    }
	  else if (i == 17)
	    {
	      memcpy (tempstr, cfline, strlen (cfline));
	      configstruct.afterIsha = atoi (tempstr);
	      if (configstruct.afterIsha < 0
		  || configstruct.afterIsha > 1440 /* one day */ )
		configstruct.afterIsha = 0;
	    }
	  else if (i == 18)
	    {
	      memcpy (tempstr, cfline, strlen (cfline));
	      configstruct.morningAthkar = atoi (tempstr);
	      if (configstruct.morningAthkar < 0
		  || configstruct.morningAthkar > 1)
		configstruct.morningAthkar = 0;
	    }
	  else if (i == 19)
	    {
	      memcpy (tempstr, cfline, strlen (cfline));
	      configstruct.morningAthkarTime = atoi (tempstr);
	      if (configstruct.morningAthkarTime < 0
		  || configstruct.morningAthkarTime > 300)
		configstruct.morningAthkarTime = 0;
	    }
	  else if (i == 20)
	    {
	      memcpy (tempstr, cfline, strlen (cfline));
	      configstruct.eveningAthkar = atoi (tempstr);
	      if (configstruct.eveningAthkar < 0
		  || configstruct.eveningAthkar > 1)
		configstruct.eveningAthkar = 0;
	    }
	  else if (i == 21)
	    {
	      memcpy (tempstr, cfline, strlen (cfline));
	      configstruct.eveningAthkarTime = atoi (tempstr);
	      if (configstruct.eveningAthkarTime < 0
		  || configstruct.eveningAthkarTime > 300)
		configstruct.eveningAthkarTime = 0;
	    }
	  else if (i == 22)
	    {
	      memcpy (tempstr, cfline, strlen (cfline));
	      configstruct.sleepingAthkar = atoi (tempstr);
	      if (configstruct.sleepingAthkar < 0
		  || configstruct.sleepingAthkar > 1)
		configstruct.sleepingAthkar = 0;
	    }
	  else if (i == 23)
	    {
	      memcpy (tempstr, cfline, strlen (cfline));
	      configstruct.sleepingAthkarTime = atoi (tempstr);
	      if (configstruct.sleepingAthkarTime < 0
		  || configstruct.sleepingAthkarTime > 500)
		configstruct.sleepingAthkarTime = 0;
	    }
	  else if (i == 24)
	    {
	      memcpy (tempstr, cfline, strlen (cfline));
	      configstruct.athkarPeriod = atoi (tempstr);
	      if (configstruct.athkarPeriod < 0
		  || configstruct.athkarPeriod > 500)
		configstruct.athkarPeriod = 0;
	    }
	  else if (i == 25)
	    {
	      memcpy (tempstr, cfline, strlen (cfline));
	      configstruct.specialAthkarDuration = atoi (tempstr);
	      if (configstruct.specialAthkarDuration < 0
		  || configstruct.specialAthkarDuration > 120)
		configstruct.specialAthkarDuration = 0;
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

  //GdkColor col = {0, 27000, 30000, 35000};   
  GdkRGBA col = { 255, 255, 255 };
  //gtk_widget_override_background_color(widget, GTK_STATE_PRELIGHT, &col);
  gtk_widget_set_opacity (widget, 0.5);
}

void
reset_opac (GtkWidget * widget, gpointer data)
{

  //GdkColor col = {0, 27000, 30000, 35000};   
  GdkRGBA col = { 255, 255, 255 };
  //gtk_widget_override_background_color(widget, GTK_STATE_PRELIGHT, &col);
  gtk_widget_set_opacity (widget, 1);
}

static GtkWidget *dhikrWindow = NULL;
GtkWidget *label = NULL;
//GtkWidget *frame = NULL;
GtkWidget *sw = NULL;
GtkWidget *contents = NULL;
GtkTextBuffer *buffer = NULL;


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

void
initAthkarWindow (int heightWindow)
{
  GdkGeometry geometry;
  GdkWindowHints geometry_mask;

  if (!dhikrWindow)
    {
      dhikrWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_window_set_resizable (GTK_WINDOW (dhikrWindow), FALSE);

      gtk_window_set_keep_above (GTK_WINDOW (dhikrWindow), TRUE);
      gtk_window_set_type_hint (GTK_WINDOW (dhikrWindow),
				GDK_WINDOW_TYPE_HINT_MENU);


      sw = gtk_scrolled_window_new (NULL, NULL);

      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
				      GTK_POLICY_AUTOMATIC,
				      GTK_POLICY_AUTOMATIC);

      gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
					   GTK_SHADOW_IN);
      gtk_container_add (GTK_CONTAINER (dhikrWindow), sw);
      contents = gtk_text_view_new ();
      gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (contents), GTK_WRAP_WORD);
      gtk_text_view_set_overwrite (GTK_TEXT_VIEW (contents), FALSE);
      gtk_text_view_set_editable (GTK_TEXT_VIEW (contents), FALSE);
      //gtk_widget_grab_focus (contents);

      gtk_container_add (GTK_CONTAINER (sw), contents);


      g_signal_connect (dhikrWindow, "destroy",
			G_CALLBACK (gtk_widget_destroyed), &dhikrWindow);
      g_signal_connect (G_OBJECT (dhikrWindow), "enter-notify-event",
			G_CALLBACK (set_opac), NULL);
      //g_signal_connect(G_OBJECT(contents), "enter-notify-event", G_CALLBACK(set_opac), NULL);
      g_signal_connect (G_OBJECT (dhikrWindow), "leave-notify-event",
			G_CALLBACK (reset_opac), NULL);
      //g_signal_connect(G_OBJECT(contents), "leave-notify-event", G_CALLBACK(reset_opac), NULL);
      gtk_container_set_border_width (GTK_CONTAINER (dhikrWindow), 8);

    }
  gtk_window_set_default_size (GTK_WINDOW (dhikrWindow), 100, heightWindow);
  geometry_mask = GDK_HINT_MIN_SIZE | GDK_HINT_BASE_SIZE | GDK_HINT_MAX_SIZE;

  geometry.min_width = 300;
  geometry.min_height = heightWindow;
  geometry.max_width = 300;
  geometry.max_height = heightWindow;
  geometry.base_width = 0;
  geometry.base_height = 0;
  geometry.win_gravity = GDK_GRAVITY_SOUTH_EAST;

  gtk_window_set_geometry_hints (GTK_WINDOW (dhikrWindow),
				 NULL, &geometry, geometry_mask);


  /* Obtaining the buffer associated with the widget. */
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (contents));
}

static int
show_athkar_window (const char *title, const char *nass, int count)
{
  //TRACE("show_athkar_window(%s, %s, %d)\n", title, nass, count);
  TRACE ("show_athkar_window(s, s, %d)\n", count);
  GdkGeometry geometry;
  GdkWindowHints geometry_mask;
  char *sepString;

  initAthkarWindow (200);

  gtk_window_set_title (GTK_WINDOW (dhikrWindow), title);
  GtkTextIter iter;


  if (!count)
    {
      gtk_text_buffer_set_text (buffer, nass, -1);
    }
  else
    {
      sepString = "\n\n+++\n\n";
      gtk_text_buffer_get_end_iter (buffer, &iter);
      gtk_text_buffer_insert (buffer, &iter, sepString, -1);
      gtk_text_buffer_get_end_iter (buffer, &iter);

      gtk_text_buffer_insert (buffer, &iter, nass, -1);
    }

  if (!gtk_widget_get_visible (dhikrWindow))
    {
      gtk_widget_show_all (dhikrWindow);
    }
  return 0;
}

static int
show_dhikr_callback (void *data, int argc, char **argv, char **azColName)
{

  GdkGeometry geometry;
  GdkWindowHints geometry_mask;
  char *title = argv[0];
  char *nass = argv[1];


  size_t lenNass = strlen (nass);
  int heightWindow = 100;
  if (lenNass < 400)
    heightWindow = 100;
  //else if(lenNass<400) heightWindow  = 200;
  else
    heightWindow = 200;

  initAthkarWindow (heightWindow);

  gtk_window_set_title (GTK_WINDOW (dhikrWindow), title);

  gtk_text_buffer_set_text (buffer, nass, -1);

  if (!gtk_widget_get_visible (dhikrWindow))
    {
      gtk_widget_show_all (dhikrWindow);
    }
  TRACE ("Dhikr shown successfully\n");
  return 0;
}

static void
show_athkar (int AthkarType)
{

  char *zErrMsg = 0;
  int rc;
  char *sql = NULL;
  const char *title = NULL;
  const char *nass = NULL;
  sqlite3_stmt *stmt;
  int row = 0;

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
	  //int bytes;
	  //const unsigned char * text;
	  //bytes = sqlite3_column_bytes(stmt, 0);
	  title = sqlite3_column_text (stmt, 0);
	  nass = sqlite3_column_text (stmt, 1);
	  //printf ("%d: %s\n", row, text);

	  show_athkar_window (title, nass, row);

	  row++;
	  TRACE ("Special Athkar shown: %d\n", row);
	}
      else if (s == SQLITE_DONE)
	{
	  TRACE ("Show athkar done.\n");
	  break;
	}
      else
	{
	  TRACE ("Show athkar Failed.\n");
	  return;
	}
    }
  sqlite3_reset (stmt);

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
	  waitSpecialAthkar = 0;
	  break;
	case 5:		//isha
	  TRACE ("notify after isha?\n");
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

  return TRUE;
}

gboolean
show_dhikr (gpointer data)
{
  char *zErrMsg = 0;
  int rc;
  char sql[MAXBUF];

	if (difference_prev_prayer>0 && difference_prev_prayer <
      (waitSpecialAthkar + configstruct.specialAthkarDuration)
      && waitSpecialAthkar > 0)
    {
      TRACE ("show_dhikr_callback: skipped\n");
      return 0;
    }
    
  //sprintf(sql,"SELECT title,nass from athkar WHERE id=%d AND title!='أذكار الصباح' AND title!='أذكار المساء' AND title!='أذكار النوم' LIMIT 1", rand()%nbAthkar);
  sprintf (sql, "SELECT title,nass from athkar WHERE id=%d",
	   rand () % nbAthkar);

  /* Execute SQL statement */
  rc =
    sqlite3_exec (dhikrdb, sql, show_dhikr_callback, (void *) data, &zErrMsg);
  if (rc != SQLITE_OK)
    {
      TRACE ("SQL error: %s\n", zErrMsg);
      sqlite3_free (zErrMsg);
    }				/*else{
				   TRACE("Dhikr selected successfully\n");
				   } */

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



int
main (int argc, char **argv)
{
  int i;
  int rc;
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

  if (configstruct.athkarPeriod > 0)
    g_timeout_add (1000 * 60 * configstruct.athkarPeriod, show_dhikr, NULL);

  gtk_main ();


  return 0;
}
