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

#define TRACE(...) if (trace) { printf( __VA_ARGS__); fflush(stdout); } else {}
struct config configstruct;
bool trace = false;
Prayer ptList[6];
gint next_prayer_id;
GDate *currentDate;
Date *prayerDate;
static gchar *next_prayer_string;

gchar *names[] = {
	"\xD8\xA7\xD9\x84\xD8\xB5\xD8\xA8\xD8\xAD",
	"\xD8\xA7\xD9\x84\xD8\xB4\xD8\xB1\xD9\x88\xD9\x82",
	"\xD8\xA7\xD9\x84\xD8\xB8\xD9\x87\xD8\xB1",
	"\xD8\xA7\xD9\x84\xD8\xB9\xD8\xB5\xD8\xB1",
	"\xD8\xA7\xD9\x84\xD9\x85\xD8\xBA\xD8\xB1\xD8\xA8",
	"\xD8\xA7\xD9\x84\xD8\xB9\xD8\xB4\xD8\xA7\xD8\xA1"
};
/* update period in seconds */
int period = 59;
int noAthan = 1;
gboolean first_run = TRUE;

GSettings *settings;

AppIndicator *indicator;
GtkWidget *indicator_menu;
GtkWidget *interfaces_menu;

GtkWidget * athantimes_items[6];
GtkWidget *stopathan_item;
GtkWidget *quit_item;
#define FILENAME ".athantime.conf"
#define MAXBUF 1024
#define DELIM "="
char tempstr[MAXBUF];
struct config
{
   double lat;
   double lon;
   char name[MAXBUF];
   double height;
   int correctiond;
   int method;
   char athan[MAXBUF];
};
void get_config(char *filename)
{
        
		configstruct.lat = 0;
		
		struct passwd *pw = getpwuid(getuid());
		
		TRACE("Current working dir: %s\n", pw->pw_dir);
		sprintf(tempstr,"%s/%s", pw->pw_dir, filename);
		
		TRACE("Openning config file: %s\n", tempstr);
		FILE *file = fopen (tempstr, "r");
		
        if (file != NULL)
        {
                char line[MAXBUF];
                int i = 0;

                while(fgets(line, sizeof(line), file) != NULL)
                {
                        if(line[0]=='#') continue;
                        char *cfline;
                        cfline = strstr((char *)line,DELIM);
                        cfline = cfline + strlen(DELIM);
   
                        if (i == 0){
                                memcpy(tempstr,cfline,strlen(cfline));
                                configstruct.lat = atof(tempstr);
                                //printf("%s",configstruct.imgserver);
                        } else if (i == 1){
                                memcpy(tempstr,cfline,strlen(cfline));
                                configstruct.lon = atof(tempstr);
                                //printf("%s",configstruct.ccserver);
                        } else if (i == 2){
                                memcpy(configstruct.name,cfline,strlen(cfline));
                                 configstruct.name[strlen( configstruct.name)-1] = '\0';
                                //printf("%s",configstruct.port);
                        } else if (i == 3){
                                memcpy(tempstr,cfline,strlen(cfline));
                                configstruct.height = atof(tempstr);
                                //printf("%s",configstruct.imagename);
                        } else if (i == 4){
                                memcpy(tempstr,cfline,strlen(cfline));
                                configstruct.correctiond = atoi(tempstr);
                                //printf("%s",configstruct.getcmd);
                        } else if (i == 5){
                                memcpy(tempstr,cfline,strlen(cfline));
                                configstruct.method = atoi(tempstr);
                                //printf("%s",configstruct.getcmd);
                        } else if (i == 6){
                                memcpy(configstruct.athan,cfline,strlen(cfline));
                                configstruct.athan[strlen( configstruct.athan)-1] = '\0';
                                //printf("%s",configstruct.getcmd);
                        }             
                        i++;
                } // End while
                fclose(file);
        }{
			
		}
        //return configstruct;
}

// Taken from minbar
void 
next_prayer(void)
{   
    /* current time */
    time_t result;
    struct tm * curtime;
    result      = time(NULL);
    curtime     = localtime(&result);

    int i;
    for (i = 0; i < 6; i++)
    {
        if ( i == 1 ) { continue ;} /* skip shorouk */
        next_prayer_id = i;
        if(ptList[i].hour > curtime->tm_hour || 
            (ptList[i].hour == curtime->tm_hour && 
            ptList[i].minute >= curtime->tm_min))
        {
            return;
        }
    }

    next_prayer_id = 0; 
}

// Taken from minbar
void 
update_date(void)
{
    GTimeVal * curtime  = g_malloc(sizeof(GTimeVal));

    currentDate         = g_date_new();
    g_get_current_time(curtime);
    g_date_set_time_val(currentDate, curtime);
    g_free(curtime);

    /* Setting current day */
    prayerDate      = g_malloc(sizeof(Date));
    prayerDate->day     = g_date_get_day(currentDate);
    prayerDate->month   = g_date_get_month(currentDate);
    prayerDate->year    = g_date_get_year(currentDate);
    //update_date_label();
    g_free(currentDate);
}


GMainLoop *loop;

GstElement *pipeline, *source, *demuxer, *decoder, *conv, *sink;
GstBus *bus;
guint bus_watch_id;

void stop_athan_callback()
{
	//TRACE ("stopping Athan 0\n");
	// clean up nicely
	if(GST_IS_ELEMENT (pipeline))
	{	
		TRACE ("stopping Athan\n");
		gst_element_set_state (pipeline, GST_STATE_NULL);
		gst_object_unref (GST_OBJECT (pipeline));
		g_source_remove (bus_watch_id);
		g_main_loop_quit (loop);
		g_main_loop_unref (loop);
	}
}

static gboolean
bus_call (GstBus     *bus,
          GstMessage *msg,
          gpointer    data)
{
  GMainLoop *loop = (GMainLoop *) data;

  switch (GST_MESSAGE_TYPE (msg)) {

    case GST_MESSAGE_EOS:
      TRACE ("End of stream\n");
      g_main_loop_quit (loop);
      break;

    case GST_MESSAGE_ERROR: {
      gchar  *debug;
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
on_pad_added (GstElement *element,
              GstPad     *pad,
              gpointer    data)
{
  GstPad *sinkpad;
  GstElement *decoder = (GstElement *) data;

  // We can now link this pad with the vorbis-decoder sink pad
  TRACE ("Dynamic pad created, linking demuxer/decoder\n");

  sinkpad = gst_element_get_static_pad (decoder, "sink");

  gst_pad_link (pad, sinkpad);

  gst_object_unref (sinkpad);
}

void play_athan(){

	if(noAthan == 1) return;
	stop_athan_callback();
  loop = g_main_loop_new (NULL, FALSE);


  /* Create gstreamer elements */
  pipeline = gst_pipeline_new ("audio-player");
  source   = gst_element_factory_make ("filesrc",       "file-source");
  demuxer  = gst_element_factory_make ("oggdemux",      "ogg-demuxer");
  decoder  = gst_element_factory_make ("vorbisdec",     "vorbis-decoder");
  conv     = gst_element_factory_make ("audioconvert",  "converter");
  sink     = gst_element_factory_make ("autoaudiosink", "audio-output");

  if (!pipeline || !source || !demuxer || !decoder || !conv || !sink) {
    g_printerr ("One element could not be created. Exiting.\n");
    return;
  }

  /* Set up the pipeline */

  /* we set the input filename to the source element */
  g_object_set (G_OBJECT (source), "location", configstruct.athan, NULL);

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
     when the "pad-added" is emitted.*/


  /* Set the pipeline to "playing" state*/
  TRACE ("gstreamer: Now playing: %s\n", configstruct.athan);
  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  g_main_loop_run (loop);
  
  /* Out of the main loop, clean up nicely */
  TRACE ("gstreamer: Returned, stopping playback\n");
  stop_athan_callback();
}
// Taken and modified from minbar
void 
update_remaining(void)
{
    /* converts times to minutes */
    int next_minutes = ptList[next_prayer_id].minute + ptList[next_prayer_id].hour*60;
    time_t  result;
    struct  tm * curtime;

    result  = time(NULL);
    curtime = localtime(&result);
    int cur_minutes = curtime->tm_min + curtime->tm_hour * 60; 
    if(ptList[next_prayer_id].hour < curtime->tm_hour)
    {
        /* salat is on next day (subh, and even Isha sometimes) after midnight */
        next_minutes += 60*24;
    }

    int difference = next_minutes - cur_minutes;
    int hours = difference / 60;
    int minutes = difference % 60;

    if (difference == 0)
    {
        g_snprintf(next_prayer_string, 400,
          ("\xD8\xAD\xD8\xA7\xD9\x86\x20\xD9\x88\xD9\x82\xD8\xAA\x20\xD8\xB5\xD9\x84\xD8\xA7\xD8\xA9 %s"),  // _("Time for %s"), 
            names[next_prayer_id]);
            
            
          play_athan();
			
    }
    else
    {
        g_snprintf(next_prayer_string, 400, "%s %d:%02d-", names[next_prayer_id], hours, minutes);
    }
}

gboolean
update_data(gpointer data)
{
    next_prayer();
    update_remaining();
    //menu_item_set_label(GTK_MENU_ITEM(menu), next_prayer_string);

    gchar *label_guide = "100000000000.000000000";   //maximum length label text, doesn't really work atm
    app_indicator_set_label(indicator, next_prayer_string, label_guide);

    return TRUE;
}

int main (int argc, char **argv)
{
    int i;
    if (argc > 1 && strcmp("--trace", argv[1]) == 0)
    {
        trace = true;
        argc--;
        argv++;
        TRACE("Tracing is on\n");
        fflush(stdout);
    }

    gtk_init (&argc, &argv);

	// initialize GStreamer
	gst_init (&argc, &argv);
    
       
    get_config(FILENAME); // sets up configstruct
    if(configstruct.lat == 0){
		
        printf("An error occured while reading the config file %s\n", FILENAME);
        fflush(stdout);
		 return 0;
	 }
        // Struct members
        TRACE("Reading config file:\n");
        TRACE("lat: %f\n",configstruct.lat);
        TRACE("lon: %f\n",configstruct.lon);
        TRACE("name: %s\n",configstruct.name);
        TRACE("height: %f\n",configstruct.height);
        TRACE("correctiond: %d\n",configstruct.correctiond);
        TRACE("method: %d\n",configstruct.method);
        TRACE("athan file: %s\n",configstruct.athan);
        
		FILE *file = fopen (configstruct.athan, "r");		
        if (file == NULL){
			TRACE("Athan sound file not set or corrupted!\n");
			noAthan = 1;
		}else{			
			noAthan = 0;
		}

    indicator_menu = gtk_menu_new();

    //add interfaces menu
	
	 for (i=0; i<6; i++) {
       athantimes_items[i] = gtk_image_menu_item_new_with_label("");
		gtk_menu_shell_append(GTK_MENU_SHELL(indicator_menu), athantimes_items[i]);
    }
	
    //separator
    GtkWidget *sep = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(indicator_menu), sep);
    
    stopathan_item = gtk_menu_item_new_with_label("\xD8\xA3\xD9\x88\xD9\x82\xD9\x81\x20\xD8\xA7\xD9\x84\xD8\xA3\xD8\xB0\xD8\xA7\xD9\x86"); //stop athan item
    gtk_menu_shell_append(GTK_MENU_SHELL(indicator_menu), stopathan_item);
    g_signal_connect(stopathan_item, "activate", G_CALLBACK (stop_athan_callback), NULL);
    
    //quit item
    quit_item = gtk_menu_item_new_with_label("\xD8\xA3\xD8\xBA\xD9\x84\xD9\x82"); //quit item
    gtk_menu_shell_append(GTK_MENU_SHELL(indicator_menu), quit_item);
    g_signal_connect(quit_item, "activate", G_CALLBACK (gtk_main_quit), NULL);

    gtk_widget_show_all(indicator_menu);

    //create app indicator
    indicator = app_indicator_new ("athantimes", "minbar", APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
    app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
    app_indicator_set_label(indicator, "athantimes", "athantimes");
    app_indicator_set_menu(indicator, GTK_MENU (indicator_menu));

    Method *calcMethod = g_malloc(sizeof(Method));
    getMethod(configstruct.method, calcMethod);

    Location *loc = g_malloc(sizeof(Location));

    loc->degreeLong = configstruct.lon;
    loc->degreeLat = configstruct.lat;
    loc->gmtDiff = configstruct.correctiond;
    loc->dst = 0;
    loc->seaLevel = configstruct.height;
    loc->pressure = 1010;
    loc->temperature = 10;


    update_date();

    // Prayer time
    getPrayerTimes(loc, calcMethod, prayerDate, ptList);

    gchar *prayer_time_text[6];
    for (i=0; i<6; i++) {
        prayer_time_text[i] = g_malloc(100);
        g_snprintf(prayer_time_text[i], 100, "%s %d:%02d", names[i], ptList[i].hour, ptList[i].minute); 

        //gtk_label_set_text(GTK_LABEL(prayer_times_label[i]), prayer_time_text[i]);
        gtk_menu_item_set_label(GTK_MENU_ITEM(athantimes_items[i]), prayer_time_text[i]);
    }
        
     // Next prayer
    next_prayer_string = g_malloc(sizeof(gchar) * 400);
    update_data(NULL);
    

    // update period in milliseconds
    g_timeout_add(1000*period, update_data, NULL);

    gtk_main ();


    return 0;
}
