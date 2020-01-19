/*
    pkTriggerCord
    Remote control of Pentax DSLR cameras.
    Copyright (C) 2011-2019 Andras Salamon <andras.salamon@melda.info>

    based on:

    PK-Remote: Remote control of Pentax DSLR cameras.
    Copyright (C) 2008 Pontus Lidman <pontus@lysator.liu.se>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU General Public License
    and GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <getopt.h>
#include <sys/time.h>

#include "pslr.h"
#include "pslr_lens.h"
#include "pktriggercord-servermode.h"

#ifdef WIN32
#define FILE_ACCESS O_WRONLY | O_CREAT | O_TRUNC | O_BINARY
#else
#define FILE_ACCESS O_WRONLY | O_CREAT | O_TRUNC
#endif

#define GW(name) GTK_WIDGET (gtk_builder_get_object (xml, name))
#define GTA(name) GTK_TOGGLE_ACTION (gtk_builder_get_object (xml, name))

// status.bufmask
#define MAX_BUFFERS 8*sizeof(uint16_t)

static struct {
    char *autosave_path;
} plugin_config;

static gboolean added_quit(gpointer data);
static void my_atexit(void);

int common_init(void);
void init_preview_area(void);
void set_preview_icon(int n, GdkPixbuf *pBuf);

void error_message(const gchar *message);

static gboolean status_poll(gpointer data);
static void update_image_areas(int buffer, bool main);

static void init_controls(pslr_status *st_new, pslr_status *st_old);
static bool auto_save_check(int format, int buffer);
static void manage_camera_buffers(pslr_status *st_new, pslr_status *st_old);
static void manage_camera_buffers_limited();

static void which_shutter_table(pslr_status *st, pslr_rational_t **table, int *steps);
static void which_iso_table(pslr_status *st, const int **table, int *steps);
static void which_ec_table(pslr_status *st, const int **table, int *steps);
static bool is_inside(int rect_x, int rect_y, int rect_w, int rect_h, int px, int py);

static void save_buffer(int bufno, const char *filename);

// coordinates for 640 x 480 image
#define AF_FAR_LEFT   132
#define AF_LEFT       223
#define AF_CENTER     319
#define AF_RIGHT      415
#define AF_FAR_RIGHT  505

#define AF_TOP        (149+27)
#define AF_MID        (213+27)
#define AF_BOTTOM     (276+27)

#define AF_CROSS_W    9
#define AF_CROSS_H    10

#define AF_CENTER_W   15
#define AF_CENTER_H   15

#define AF_LINE_W     7
#define AF_LINE_H     21

/* Order of array corresponsds to AF point bitmask, pslr_af11_point_t */
static struct {
    int x;
    int y;
    int w;
    int h;
} af_points[] = {
    { AF_LEFT-(AF_CROSS_W/2), AF_TOP-(AF_CROSS_H/2), AF_CROSS_W, AF_CROSS_H },
    { AF_CENTER-(AF_CROSS_W/2), AF_TOP-(AF_CROSS_H/2), AF_CROSS_W, AF_CROSS_H },
    { AF_RIGHT-(AF_CROSS_W/2), AF_TOP-(AF_CROSS_H/2), AF_CROSS_W, AF_CROSS_H },
    { AF_FAR_LEFT - (AF_LINE_W/2), AF_MID - (AF_LINE_H/2) - 1, AF_LINE_W, AF_LINE_H },
    { AF_LEFT-(AF_CROSS_W/2), AF_MID-(AF_CROSS_H/2), AF_CROSS_W, AF_CROSS_H },
    { AF_CENTER-(AF_CENTER_W/2), AF_MID-(AF_CENTER_H/2) - 1, AF_CENTER_W, AF_CENTER_H },
    { AF_RIGHT-(AF_CROSS_W/2), AF_MID-(AF_CROSS_H/2), AF_CROSS_W, AF_CROSS_H },
    { AF_FAR_RIGHT - (AF_LINE_W/2), AF_MID - (AF_LINE_H/2) - 1, AF_LINE_W, AF_LINE_H },
    { AF_LEFT-(AF_CROSS_W/2), AF_BOTTOM-(AF_CROSS_H/2), AF_CROSS_W, AF_CROSS_H },
    { AF_CENTER-(AF_CROSS_W/2), AF_BOTTOM-(AF_CROSS_H/2), AF_CROSS_W, AF_CROSS_H },
    { AF_RIGHT-(AF_CROSS_W/2), AF_BOTTOM-(AF_CROSS_H/2), AF_CROSS_W, AF_CROSS_H },
};

static uint32_t focus_indicated_af_points;
static uint32_t select_indicated_af_points;
static uint32_t preselect_indicated_af_points;
static bool preselect_reselect = false;
static pslr_settings settings;

/* This is the nominator, the denominator is 10 for all confirmed
 * apertures */
static const int aperture_tbl[] = {
    10, 11, 12, 14, 16, 17, 18,
    20, 22, 24, 25, 28, 32, 35,
    40, 45, 50, 56, 63, 67, 71,
    80, 90, 95, 100, 110, 130, 140,
    160, 180, 190, 200, 220, 250, 280,
    320, 360, 400, 450, 510, 570
};

static pslr_rational_t shutter_tbl_1_3[] = {
    { 30, 1},{ 25, 1},{ 20, 1},{ 15, 1},{ 13, 1},{ 10, 1},{ 8, 1},{ 6, 1},
    { 5, 1},{ 4, 1},{ 3, 1},{ 25, 10},{ 2, 1},{ 16, 10},{ 13, 10},{ 1, 1},
    { 8, 10},{ 6, 10},{ 5, 10},{ 4, 10},{ 3, 10},{ 1, 4},{ 1, 5},{ 1, 6},
    { 1, 8},{ 1, 10},{ 1, 13},{ 1, 15},{ 1, 20},{ 1, 25},{ 1, 30},{ 1, 40},
    { 1, 50},{ 1, 60},{ 1, 80},{ 1, 100},{ 1, 125},{ 1, 160},{ 1, 200},{ 1, 250},
    { 1, 320},{ 1, 400},{ 1, 500},{ 1, 640},{ 1, 800},{ 1, 1000},{ 1, 1250},{ 1, 1600},
    { 1, 2000},{ 1, 2500},{ 1, 3200},{ 1, 4000},{1, 5000}, {1, 6400}, {1, 8000}
};

static pslr_rational_t shutter_tbl_1_2[] = {
    { 30, 1},{ 20, 1},{ 15, 1},{ 10, 1},{ 8, 1},{ 6, 1},
    { 4, 1},{ 3, 1},{ 2, 1},{ 15, 10},{ 1, 1},
    { 7, 10},{ 5, 10},{ 3, 10},{ 1, 4},{ 1, 6},
    { 1, 8},{ 1, 10},{ 1, 15},{ 1, 20},{ 1, 30},
    { 1, 45},{ 1, 60},{ 1, 90},{ 1, 125},{ 1, 180},{ 1, 250},
    { 1, 350},{ 1, 500},{ 1, 750},{ 1, 1000},{ 1, 1500},
    { 1, 2000},{ 1, 3000},{ 1, 4000}, {1, 6400}, {1, 8000}
};


static const int iso_tbl_1_3[] = {
    80, 100, 125, 160, 200, 250, 320, 400, 500, 640, 800, 1000, 1250, 1600, 2000, 2500,
    3200, 4000, 5000, 6400, 8000, 10000, 12800, 16000, 20000, 25600, 32000, 40000, 51200, 64000,
    80000, 102400
};

static const int iso_tbl_1_2[] = {
    100, 140, 200, 280, 400, 560, 800, 1100, 1600, 2200, 3200, 4500, 6400, 9000, 12800, 18000, 25600, 36000, 51200, 72000, 102400
};

static const int iso_tbl_1[] = {
    100, 200, 400, 800, 1600, 3200, 6400, 12800, 25600, 51200, 102400
};

static const int ec_tbl_1_3[] = {
    -30, -27, -23, -20, -17, -13, -10, -7, -3, 0, 3, 7, 10, 13, 17, 20, 23, 27, 30
    };

static const int ec_tbl_1_2[]= {
    -30, -25, -20, -15, -10, -5, 0, 5, 10, 15, 20, 30
    };

static pslr_handle_t camhandle;
static bool handle_af_points;
static double af_width_multiplier;
static double af_height_multiplier;
static GtkBuilder *xml;
static GtkStatusbar *statusbar;
static guint sbar_connect_ctx;
static guint sbar_download_ctx;
bool need_histogram=false;
bool fullsize_preview=false;
static GtkListStore *list_store;

bool debug = false;
bool dangerous = false;
bool dangerous_camera_connected = false;
bool in_initcontrols = false;
bool need_one_push_bracketing_cleanup = false;
static struct timeval expected_bulb_end_time = {0, 0};

static const int THUMBNAIL_WIDTH = 160;
static const int THUMBNAIL_HEIGHT = 120;
static const int HISTOGRAM_WIDTH = 640;
static const int HISTOGRAM_HEIGHT = 480;

static void wait_for_gtk_events_pending() {
    while (gtk_events_pending()) {
        gtk_main_iteration();
    }
}

void combobox_append( GtkComboBox *combobox, char **items, int item_num ) {
    GtkListStore *store = gtk_list_store_new (1, G_TYPE_STRING);
    GtkTreeIter iter;
    int i;
    DPRINT("combobox_append\n");

    for (i = 0; i<item_num; i++) {
        DPRINT("adding item %s\n", items[i] );
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, items[i], -1);
    }

    gtk_combo_box_set_model( combobox, GTK_TREE_MODEL(store));

    GtkCellRenderer * cell = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combobox ), cell, TRUE );
    gtk_cell_layout_set_attributes( GTK_CELL_LAYOUT( combobox ), cell, "text", 0, NULL );
}

int common_init(void) {
    GtkWidget *widget;

    atexit(my_atexit);

    gtk_init(0, 0);

    gtk_quit_add(0, added_quit, 0);

    DPRINT("Create gtk xml\n");

    GError* error = NULL;
    if (debug) {
        xml = gtk_builder_new ();
        if (!gtk_builder_add_from_file (xml, "pktriggercord.ui", &error))  {
            error = NULL;
            if ( !gtk_builder_add_from_file (xml, PKTDATADIR "/pktriggercord.ui", &error))  {
                g_warning ("Couldn't load builder file: %s", error->message);
                g_error_free (error);
                return -1;
            }
        }
    } else {
        xml = gtk_builder_new ();
        if (!gtk_builder_add_from_file (xml, PKTDATADIR "/pktriggercord.ui", &error))  {
            error = NULL;
            if ( !gtk_builder_add_from_file (xml, "pktriggercord.ui", &error))  {
                g_warning ("Couldn't load builder file: %s", error->message);
                g_error_free (error);
                return -1;
            }
        }
    }

    init_preview_area();

    widget = GW("mainwindow");

    gchar titlebuf[128];
    snprintf(titlebuf, sizeof(titlebuf), "pkTriggerCord %s", VERSION);
    gtk_window_set_title( (GtkWindow *)widget, titlebuf);

    gtk_builder_connect_signals( xml, NULL );

    statusbar = GTK_STATUSBAR(GW("statusbar1"));
    sbar_connect_ctx = gtk_statusbar_get_context_id(statusbar, "connect");
    sbar_download_ctx = gtk_statusbar_get_context_id(statusbar, "download");

    gtk_statusbar_push(statusbar, sbar_connect_ctx, "No camera connected.");

    gdk_window_set_events(gtk_widget_get_window(widget), GDK_ALL_EVENTS_MASK);

    GtkComboBox *pw = (GtkComboBox*)GW("file_format_combo");

    int numfileformats = sizeof(file_formats) / sizeof (file_formats[0]);
    char **fileformatnames = malloc( numfileformats * sizeof(char*) );
    int i;
    for (i = 0; i<numfileformats; i++) {
        fileformatnames[i] = malloc( strlen(file_formats[i].file_format_name)+1);
        memset( fileformatnames[i], '\0', strlen(file_formats[i].file_format_name)+1);
        strncpy( fileformatnames[i], file_formats[i].file_format_name, strlen( file_formats[i].file_format_name ));
    }

    combobox_append( pw, fileformatnames, numfileformats );

    init_controls(NULL, NULL);

    g_timeout_add(1000, status_poll, 0);

    gtk_widget_show(widget);

    gtk_main();
    return 0;
}

static int get_jpeg_property_shift() {
    return (pslr_get_model_jpeg_property_levels( camhandle )-1) / 2;
}

void shutter_speed_table_init(pslr_status *st) {
    // check valid shutter speeds for the camera
    int max_valid_shutter_speed_index=0;
    int i;
    pslr_rational_t *tbl;
    int steps;
    which_shutter_table( st, &tbl, &steps);
    int fastest_shutter_speed = pslr_get_model_fastest_shutter_speed(camhandle);
    for (i=0;  i<steps; i++) {
        if ( tbl[i].nom == 1 &&
                tbl[i].denom <= fastest_shutter_speed) {
            max_valid_shutter_speed_index = i;
        }
    }
    if ( tbl[max_valid_shutter_speed_index].denom != fastest_shutter_speed ) {
        // not an exact match
        ++max_valid_shutter_speed_index;
        tbl[max_valid_shutter_speed_index].denom = fastest_shutter_speed;
    }
    GtkWidget *pw;
    pw = GW("shutter_scale");
    //printf("range 0-%f\n", (float) sizeof(shutter_tbl)/sizeof(shutter_tbl[0]));
    gtk_range_set_range(GTK_RANGE(pw), 0.0, max_valid_shutter_speed_index);
    gtk_range_set_increments(GTK_RANGE(pw), 1.0, 1.0);
}

void iso_speed_table_init(pslr_status *st) {
    DPRINT("iso_speed_table_init\n");
    GtkWidget *pw;
    pw = GW("iso_scale");

    const int *tbl = 0;
    int steps = 0;
    which_iso_table(st, &tbl, &steps);
    int min_iso_index=0;
    int max_iso_index=steps-1;
    int i;

    // cannot determine if base or extended iso is set.
    // use extended iso range
    for (i=0;  i<steps; i++) {
        if ( tbl[i] < pslr_get_model_extended_iso_min(camhandle)) {
            min_iso_index = i+1;
        }

        if ( tbl[i] <= pslr_get_model_extended_iso_max(camhandle)) {
            max_iso_index = i;
        }
    }


    GtkAdjustment *adj = gtk_range_get_adjustment (GTK_RANGE(pw));
    gdouble current_scale_iso_min = gtk_adjustment_get_lower(adj);
    gdouble current_scale_iso_max = gtk_adjustment_get_upper(adj);
    DPRINT("iso_speed_table_init %f - %f\n", current_scale_iso_min, current_scale_iso_max);
    if ((gdouble)(min_iso_index) != current_scale_iso_min || (gdouble)(max_iso_index) != current_scale_iso_max) {
        gtk_range_set_range(GTK_RANGE(pw), (gdouble)(min_iso_index), (gdouble) (max_iso_index));
    }
}

void camera_specific_init() {
    bool has_jpeg_hue = pslr_get_model_has_jpeg_hue( camhandle );
    if ( has_jpeg_hue ) {
        gtk_range_set_range( GTK_RANGE(GW("jpeg_hue_scale")), -get_jpeg_property_shift(), get_jpeg_property_shift());
    }
    gtk_range_set_range( GTK_RANGE(GW("jpeg_sharpness_scale")), -get_jpeg_property_shift(), get_jpeg_property_shift());
    gtk_range_set_range( GTK_RANGE(GW("jpeg_saturation_scale")), -get_jpeg_property_shift(), get_jpeg_property_shift());
    gtk_range_set_range( GTK_RANGE(GW("jpeg_contrast_scale")), -get_jpeg_property_shift(), get_jpeg_property_shift());
    int *resolutions = pslr_get_model_jpeg_resolutions( camhandle );
    int resindex=0;
    char **str_resolutions = malloc( MAX_RESOLUTION_SIZE * sizeof( char * ));
    while ( resindex < MAX_RESOLUTION_SIZE ) {
        str_resolutions[resindex] = malloc( 10 );
        sprintf( str_resolutions[resindex], "%dM", resolutions[resindex]);
        ++resindex;
    }

    combobox_append( GTK_COMBO_BOX(GW("jpeg_resolution_combo")), str_resolutions, MAX_RESOLUTION_SIZE );

    int starindex= pslr_get_model_max_jpeg_stars( camhandle) ;
    const char ch[] = "*********";
    char **str_jpegstars = malloc( starindex * sizeof( char * ));
    int num_stars = starindex;
    int index=0;
    while ( starindex > 0 ) {
        str_jpegstars[index] = malloc(10);
        sprintf( str_jpegstars[index], "%.*s", starindex,ch);
        --starindex;
        ++index;
    }

    combobox_append(  GTK_COMBO_BOX(GW("jpeg_quality_combo")), str_jpegstars, num_stars );

    int max_supported_image_tone = pslr_get_model_max_supported_image_tone( camhandle )+1;
    DPRINT("max image tone:%d\n", max_supported_image_tone);
    GtkComboBox *pw = (GtkComboBox*)GW("jpeg_image_tone_combo");

    char **imagetones = malloc( max_supported_image_tone * sizeof(char*) );
    int i;
    for (i = 0; i<max_supported_image_tone; i++) {
        DPRINT("get tone %s\n", get_pslr_jpeg_image_tone_str( i ) );
        imagetones[i] = malloc( strlen( get_pslr_jpeg_image_tone_str( i ))+1);
        memset(imagetones[i], '\0', strlen( get_pslr_jpeg_image_tone_str( i ))+1);
        strncpy( imagetones[i], get_pslr_jpeg_image_tone_str( i ), strlen( get_pslr_jpeg_image_tone_str( i ) ));
    }
    combobox_append( pw, imagetones, max_supported_image_tone );

    gtk_widget_set_sensitive( GTK_WIDGET(pw), max_supported_image_tone > -1 );
    handle_af_points = pslr_get_model_af_point_num(camhandle) == 11;

    pslr_get_settings_json(camhandle, &settings);

    if (pslr_get_model_bufmask_single(camhandle) && settings.one_push_bracketing.pslr_setting_status == PSLR_SETTING_STATUS_READ && settings.one_push_bracketing.value) {
        pslr_write_setting_by_name(camhandle, "one_push_bracketing", 0);
        settings.one_push_bracketing.value=false;
        need_one_push_bracketing_cleanup = true;
    }
}

static void init_aperture_scale(pslr_status *st_new) {
    GtkWidget *pw = GW("aperture_scale");
    int min_ap=-1, max_ap=-1, current_ap = -1;
    int i;
    if (st_new) {
        for (i=0; i<sizeof(aperture_tbl)/sizeof(aperture_tbl[0]); i++) {
            if (st_new->lens_min_aperture.nom == aperture_tbl[i]) {
                min_ap = i;
            }
            if (st_new->lens_max_aperture.nom == aperture_tbl[i]) {
                max_ap = i;
            }
            if (st_new->set_aperture.nom == aperture_tbl[i]) {
                current_ap = i;
            }
        }
        gtk_range_set_increments(GTK_RANGE(pw), 1.0, 1.0);
        if (min_ap >= 0 && max_ap >= 0 && current_ap >= 0) {
            //printf("range %f-%f (%f)\n", (float) min_ap, (float)max_ap, (float)current_ap);
            gtk_range_set_range(GTK_RANGE(pw), min_ap, max_ap);
            gtk_range_set_value(GTK_RANGE(pw), current_ap);
            gtk_widget_set_sensitive(pw, TRUE);
        }
    }
    if (st_new == NULL || min_ap < 0 || max_ap < 0 || current_ap < 0) {
        gtk_widget_set_sensitive(pw, FALSE);
    } else {
        gtk_widget_set_sensitive(pw, TRUE);
    }
}

static void init_shutter_scale(pslr_status *st_new) {
    GtkWidget *pw = GW("shutter_scale");
    int i;
    if (st_new) {
        int idx = -1;
        pslr_rational_t *tbl = 0;
        int steps = 0;
        which_shutter_table( st_new, &tbl, &steps );
        for (i=0; i<steps; i++) {
            if (st_new->set_shutter_speed.nom == tbl[i].nom
                    && st_new->set_shutter_speed.denom == tbl[i].denom) {
                idx = i;
            }
        }
        if (idx >= 0) {
            gtk_range_set_value(GTK_RANGE(pw), idx);
        }
    }
    gtk_widget_set_sensitive(pw, st_new != NULL);
}

static void init_bulb_value(pslr_status *st_new) {
    GtkWidget *pw = GW("bulb_exp_value");
    gtk_widget_set_sensitive(pw, st_new != NULL);
}

static void init_iso_scale(pslr_status *st_new) {
    GtkWidget *pw = GW("iso_scale");
    int current_iso = -1;
    int i;
    if (st_new) {
        DPRINT("init_controls iso %d\n", st_new->fixed_iso);
        const int *tbl = 0;
        int steps = 0;
        which_iso_table(st_new, &tbl, &steps);
        for (i=0; i<steps; i++) {
            if (tbl[i] >= st_new->fixed_iso) {
                current_iso = i;
                break;
            }
        }
        DPRINT("init_controls current_iso: %d\n", current_iso);
        if (current_iso >= 0) {
            //printf("set value for ISO slider, new = %d old = %d\n",
            //       st_new->set_iso, st_old ? st_old->set_iso : -1);
            gtk_range_set_value(GTK_RANGE(pw), current_iso);
        }
    }
    gtk_widget_set_sensitive(pw, st_new != NULL);
}

static void init_ec_scale(pslr_status *st_new, pslr_status *st_old) {
    GtkWidget *pw = GW("ec_scale");
    int i;
    if (st_new) {
        const int *tbl;
        int steps;
        which_ec_table( st_new, &tbl, &steps);
        int idx = -1;
        for (i=0; i<steps; i++) {
            if (tbl[i] == st_new->ec.nom) {
                idx = i;
            }
        }
        if (!st_old || st_old->custom_ev_steps != st_new->custom_ev_steps) {
            gtk_range_set_range(GTK_RANGE(pw), 0.0, steps-1);
        }
        if (!st_old || st_old->ec.nom != st_new->ec.nom
                || st_old->ec.denom != st_new->ec.denom) {
            gtk_range_set_value(GTK_RANGE(pw), idx);
        }
    }
    gtk_widget_set_sensitive(pw, st_new != NULL);
}

static void init_jpeg_scales(pslr_status *st_new) {
    GtkWidget *pw;
    /* JPEG contrast */
    pw = GW("jpeg_contrast_scale");
    if (st_new) {
        gtk_range_set_value(GTK_RANGE(pw), (gdouble)st_new->jpeg_contrast - get_jpeg_property_shift());
    }

    gtk_widget_set_sensitive(pw, st_new != NULL);
    /* JPEG hue */
//    DPRINT("init controls hue\n");
    pw = GW("jpeg_hue_scale");
    bool sensitive_hue = st_new;
    if (st_new) {
        gtk_range_set_value(GTK_RANGE(pw), (gdouble)st_new->jpeg_hue - get_jpeg_property_shift());
        bool has_jpeg_hue = pslr_get_model_has_jpeg_hue( camhandle );
//  DPRINT("has_jpeg_hue %d\n",has_jpeg_hue);
        sensitive_hue &= has_jpeg_hue;
    }
    gtk_widget_set_sensitive(pw, sensitive_hue);

    /* JPEG saturation */
    pw = GW("jpeg_saturation_scale");
    if (st_new) {
        gtk_range_set_value(GTK_RANGE(pw), (gdouble)st_new->jpeg_saturation - get_jpeg_property_shift());
    }

    gtk_widget_set_sensitive(pw, st_new != NULL);
    /* JPEG sharpness */
    pw = GW("jpeg_sharpness_scale");
    if (st_new) {
        gtk_range_set_value(GTK_RANGE(pw), (gdouble)st_new->jpeg_sharpness - get_jpeg_property_shift());
    }

    gtk_widget_set_sensitive(pw, st_new != NULL);
    /* JPEG quality */

    pw = GW("jpeg_quality_combo");
    if (st_new) {
        GtkTreeModel *jpeg_quality_model = gtk_combo_box_get_model(GTK_COMBO_BOX(pw));
        gint jpeg_quality_num = gtk_tree_model_iter_n_children( jpeg_quality_model, NULL );
        ipslr_handle_t *p = (ipslr_handle_t *)camhandle;
        int hw_jpeg_quality = get_hw_jpeg_quality(p->model, st_new->jpeg_quality);
        if ( st_new->jpeg_quality >= jpeg_quality_num ) {
            hw_jpeg_quality = 0;
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(pw), hw_jpeg_quality);
    }

    gtk_widget_set_sensitive(pw, st_new != NULL);
    /* JPEG resolution */
    pw = GW("jpeg_resolution_combo");
    if (st_new) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(pw), st_new->jpeg_resolution);
    }

    gtk_widget_set_sensitive(pw, st_new != NULL);
    /* Image tone */
    pw = GW("jpeg_image_tone_combo");
    if (st_new ) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(pw), st_new->jpeg_image_tone);
    }
    gtk_widget_set_sensitive(pw, st_new != NULL);
}

static void init_user_mode_combo(pslr_status *st_new, pslr_status *st_old) {
    GtkWidget *pw = GW("user_mode_combo");
    if (st_new) {
        if (!st_old || st_old->exposure_mode != st_new->exposure_mode) {
            gtk_combo_box_set_active(GTK_COMBO_BOX(pw), st_new->exposure_mode);
        }
    }
    gtk_widget_set_sensitive(pw, st_new != NULL && st_new->user_mode_flag);
}

static void init_file_format_combo(pslr_status *st_new) {
    GtkWidget *pw = GW("file_format_combo");
    if (st_new) {
        int val = get_user_file_format(st_new);
        gtk_combo_box_set_active(GTK_COMBO_BOX(pw), val);
    }
    gtk_widget_set_sensitive(pw, st_new != NULL);
}

static void init_buttons(pslr_status *st_new) {
    gtk_widget_set_sensitive( GW("shutter_button"), st_new != NULL && (!pslr_get_model_bufmask_single(camhandle) || !st_new->bufmask) );
    gtk_widget_set_sensitive( GW("focus_button"), st_new != NULL);
    gtk_widget_set_sensitive( GW("status_button"), st_new != NULL);
    gtk_widget_set_sensitive( GW("status_hex_button"), st_new != NULL);
    gtk_widget_set_sensitive( GW("settings_button"), st_new != NULL);
    gtk_widget_set_sensitive( GW("green_button"), st_new != NULL);

    GtkWidget *pw = GW("ae_lock_button");
    if (st_new) {
        bool lock;
        lock = (st_new->light_meter_flags & PSLR_LIGHT_METER_AE_LOCK) != 0;
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pw), lock);
    }
    gtk_widget_set_sensitive(pw, st_new != NULL);
}

static void init_controls(pslr_status *st_new, pslr_status *st_old) {
    in_initcontrols = true;
    DPRINT("start initcontrols\n");
    init_aperture_scale(st_new);
    init_shutter_scale(st_new);
    init_bulb_value(st_new);
    init_iso_scale(st_new);
    init_ec_scale(st_new, st_old);
    init_jpeg_scales(st_new);
    init_user_mode_combo(st_new, st_old);
    init_file_format_combo(st_new);
    init_buttons(st_new);
    in_initcontrols = false;
    DPRINT("end initcontrols\n");
}

static pslr_status cam_status[2];
static pslr_status *status_new = NULL;
static pslr_status *status_old = NULL;

static void update_status_pointers(void) {
    pslr_status *tmp = status_new;
    status_new = status_old;
    status_old = tmp;

    if (status_new == NULL) {
        if (status_old == NULL) {
            status_new = &cam_status[0];
        } else {
            status_new = &cam_status[1];
        }
    }
}


static void connect_camera(void) {
    int ret;
    /* Try to reconnect */
    gtk_statusbar_pop(statusbar, sbar_connect_ctx);
    gtk_statusbar_push(statusbar, sbar_connect_ctx, "Connecting...");

    /* Allow the message to be seen */
    wait_for_gtk_events_pending();

    /* Connect */
    ret = pslr_connect(camhandle);
    DPRINT("ret: %d\n", ret);
    if ( ret == -1 ) {
        gtk_statusbar_pop(statusbar, sbar_connect_ctx);
        gtk_statusbar_push(statusbar, sbar_connect_ctx, "Unknown Pentax camera found.");
        camhandle = NULL;
    } else if ( ret != 0 ) {
        gtk_statusbar_pop(statusbar, sbar_connect_ctx);
        gtk_statusbar_push(statusbar, sbar_connect_ctx, "Cannot connect to Pentax camera.");
        camhandle = NULL;
    }
}

static void update_widgets_after_connect(void) {
    gchar buf[256];
    if (camhandle) {
        DPRINT("before camera_specific_init\n");
        camera_specific_init();
        DPRINT("after camera_specific_init\n");
        const char *name;
        name = pslr_camera_name(camhandle);
        snprintf(buf, sizeof(buf), "Connected: %s", name);
        buf[sizeof(buf)-1] = '\0';
        gtk_statusbar_pop(statusbar, sbar_connect_ctx);
        gtk_statusbar_push(statusbar, sbar_connect_ctx, buf);
    } else {
        gtk_statusbar_pop(statusbar, sbar_connect_ctx);
        gtk_statusbar_push(statusbar, sbar_connect_ctx, "No camera connected.");
    }
}

static void update_aperture_label(void) {
    gchar buf[256];
    GtkLabel *aperture_label = GTK_LABEL(GW("label_aperture"));
    if (status_new && status_new->current_aperture.denom) {
        float aperture = (float)status_new->current_aperture.nom / (float)status_new->current_aperture.denom;
        sprintf(buf, "f/%.1f", aperture);
        gtk_label_set_text(aperture_label, buf);
    }
}

static void update_shutter_speed_widgets(void) {
    gchar buf[256];
    if (status_new && (status_new->exposure_mode == PSLR_GUI_EXPOSURE_MODE_B)) {
        sprintf(buf, "BULB");
        gtk_label_set_text(GTK_LABEL(GW("label_shutter")), buf);
        /* inhibit shutter exposure slide */
        gtk_widget_set_visible( GW("shutter_scale"), FALSE);
        gtk_widget_set_visible( GW("shutter_scale_label"), FALSE);
        gtk_widget_set_visible( GW("bulb_exp_value"), TRUE);
        gtk_widget_set_visible( GW("bulb_exp_value_label"), TRUE);
    } else if (status_new && status_new->current_shutter_speed.denom) {
        if (status_new->current_shutter_speed.denom == 1) {
            sprintf(buf, "%ds", status_new->current_shutter_speed.nom);
        } else if (status_new->current_shutter_speed.nom == 1) {
            sprintf(buf, "1/%ds", status_new->current_shutter_speed.denom);
        } else {
            sprintf(buf, "%.1fs", (float)status_new->current_shutter_speed.nom / (float) status_new->current_shutter_speed.denom);
        }
        gtk_label_set_text(GTK_LABEL(GW("label_shutter")), buf);
        gtk_widget_set_visible( GW("shutter_scale"), TRUE);
        gtk_widget_set_visible( GW("shutter_scale_label"), TRUE);
        gtk_widget_set_visible( GW("bulb_exp_value"), FALSE);
        gtk_widget_set_visible( GW("bulb_exp_value_label"), FALSE);
    }
}

static void update_iso_label(void) {
    gchar buf[256];
    if (status_new) {
        sprintf(buf, "ISO %d", status_new->current_iso);
        gtk_label_set_text(GTK_LABEL(GW("label_iso")), buf);
    }
}

static void update_ev_label(void) {
    gchar buf[256];
    if (status_new && status_new->current_aperture.denom
            && status_new->current_shutter_speed.denom) {
        float ev, a, s;
        a = (float)status_new->current_aperture.nom/(float)status_new->current_aperture.denom;
        s = (float)status_new->current_shutter_speed.nom/(float)status_new->current_shutter_speed.denom;
        ev = log(a*a/s)/M_LN2 - log(status_new->current_iso/100)/M_LN2;
        sprintf(buf, "<span size=\"xx-large\">EV %.2f</span>", ev);
        gtk_label_set_markup(GTK_LABEL(GW("label_ev")), buf);
    }
}

static void update_zoom_label(void) {
    gchar buf[256];
    if (status_new && status_new->zoom.denom) {
        sprintf(buf, "%d mm", status_new->zoom.nom / status_new->zoom.denom);
        gtk_label_set_text(GTK_LABEL(GW("label_zoom")), buf);
    }
}

static void update_focus_label(void) {
    gchar buf[256];
    if (status_new) {
        sprintf(buf, "focus: %d", status_new->focus);
        gtk_label_set_text(GTK_LABEL(GW("label_focus")), buf);
    }
}

static void update_lens_label(void) {
    gchar buf[256];
    if (status_new) {
        sprintf(buf, "%s", get_lens_name(status_new->lens_id1, status_new->lens_id2));
        gtk_label_set_text(GTK_LABEL(GW("label_lens")), buf);
    }
}

static void update_af_points(void) {
    if (handle_af_points) {
        GtkWidget *pw = GW("main_drawing_area");
        GdkWindow *window = gtk_widget_get_window(pw);
        GtkAllocation allocation;
        gtk_widget_get_allocation (pw, &allocation);
        if (status_new) {
            if (!status_old || status_old->focused_af_point != status_new->focused_af_point) {
                /* Change in focused AF points */
                focus_indicated_af_points |= status_new->focused_af_point;
                gdk_window_invalidate_rect(window, &allocation, FALSE);
            } else {
                /* Same as previous check, stop indicating */
                focus_indicated_af_points = 0;
                gdk_window_invalidate_rect(window, &allocation, FALSE);
            }

            if (!status_old || status_old->selected_af_point != status_new->selected_af_point) {
                /* Change in selected AF points */
                select_indicated_af_points = status_new->selected_af_point;
                gdk_window_invalidate_rect(window, &allocation, FALSE);
            } else {
                /* Same as previous check, stop indicating */
                select_indicated_af_points = 0;
                gdk_window_invalidate_rect(window, &allocation, FALSE);
            }
            preselect_indicated_af_points = 0;
            preselect_reselect = false;
        }
    }
}

static gboolean status_poll(gpointer data) {
    int ret;
    static bool status_poll_inhibit = false;

    DPRINT("start status_poll\n");
    if (status_poll_inhibit) {
        return TRUE;
    }

    /* Do not recursively status poll */
    status_poll_inhibit = true;

    if (!camhandle) {
        if ( dangerous_camera_connected ) {
            DPRINT("dangerous camera connected\n");
            status_poll_inhibit = false;
            return TRUE;
        }
        camhandle = pslr_init( NULL, NULL );
        if (camhandle) {
            connect_camera();
        }

        update_widgets_after_connect();
        status_poll_inhibit = false;
        DPRINT("end status_poll\n");
        return TRUE;
    }

    update_status_pointers();

    ret = pslr_get_status(camhandle, status_new);
    shutter_speed_table_init( status_new );
    iso_speed_table_init( status_new );
    if (ret != PSLR_OK) {
        if (ret == PSLR_DEVICE_ERROR) {
            /* Camera disconnected */
            camhandle = NULL;
        }
        DPRINT("pslr_get_status: %d\n", ret);
        status_new = NULL;
    }

    update_aperture_label();
    update_shutter_speed_widgets();
    update_iso_label();
    update_ev_label();
    update_zoom_label();
    update_focus_label();
    update_lens_label();

    /* Other controls */
    init_controls(status_new, status_old);

    update_af_points();

    /* Camera buffer checks */
    manage_camera_buffers(status_new, status_old);
    DPRINT("end status_poll\n");

    status_poll_inhibit = false;
    return TRUE;
}

static void clear_preview_icons() {
    int i;
    for (i=0; i < MAX_BUFFERS; i++) {
        set_preview_icon(i, NULL);
    }
}

static int find_new_pictures(pslr_status *st_new, pslr_status *st_old) {
    int new_pictures;
    if (st_old) {
        new_pictures = (st_new->bufmask ^ st_old->bufmask) & st_new->bufmask;
    } else {
        new_pictures = st_new->bufmask;
    }
    return new_pictures;
}

static int find_newest_picture(int new_pictures) {
    int newest_picture;
    for (newest_picture=MAX_BUFFERS; newest_picture>=0; --newest_picture) {
        if (new_pictures & (1<<newest_picture)) {
            break;
        }
    }
    return newest_picture;
}

static int auto_save_pictures(pslr_status *st_new, int new_pictures) {
    int i;
    bool deleted;
    int format = get_user_file_format(st_new);


    /* auto-save check buffers */
    for (i=0; i<MAX_BUFFERS; i++) {
        if (new_pictures & (1<<i)) {
            deleted = auto_save_check(format, i);
            if (deleted) {
                new_pictures &= ~(1<<i);
            }
        }
    }
    return new_pictures;
}

static void update_thumbnails(int new_pictures, int newest_picture) {
    int i;
    for (i=0; i<MAX_BUFFERS; i++) {
        if (i!=newest_picture && new_pictures & (1<<i)) {
            update_image_areas(i, false);
        }
    }
}

static void select_thumbnail(int thumbnail_index) {
    GtkIconView *pw = GTK_ICON_VIEW(GW("preview_icon_view"));

    GtkTreePath *path;
    path = gtk_tree_path_new_from_indices (thumbnail_index, -1);
    gtk_icon_view_unselect_all( pw );
    gtk_icon_view_select_path( pw, path );
    gtk_tree_path_free (path);
}

static void manage_camera_buffers(pslr_status *st_new, pslr_status *st_old) {
    uint32_t new_pictures;
    int newest_picture;

    if (!st_new) {
        clear_preview_icons();
        return;
    }

    if (st_old && st_new->bufmask == st_old->bufmask) {
        return;
    }
    new_pictures = find_new_pictures(st_new, st_old);
    if (!new_pictures) {
        return;
    }

    newest_picture = find_newest_picture(new_pictures);
    if (newest_picture >= 0) {
        update_image_areas(newest_picture, true);
    }

    new_pictures = auto_save_pictures(st_new, new_pictures);
    update_thumbnails(new_pictures, newest_picture);
    select_thumbnail(newest_picture);
}

static void manage_camera_buffers_limited() {
    update_image_areas(0, true);
}

G_MODULE_EXPORT void auto_save_folder_button_clicked_cb(GtkAction *action) {
    GtkWidget *pw;
    char *filename;
    int res;

    pw = GW("auto_save_folder_dialog");
    res = gtk_dialog_run(GTK_DIALOG(pw));
    DPRINT("Run folder dialog -> %d\n", res);
    gtk_widget_hide(pw);

    if (res == 1) {
        filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (pw));
        DPRINT("Selected path: %s\n", filename);
        if (plugin_config.autosave_path) {
            g_free(plugin_config.autosave_path);
        }
        plugin_config.autosave_path = g_strdup(filename);
        pw = GW("auto_folder_entry");
        gtk_entry_set_text(GTK_ENTRY(pw), plugin_config.autosave_path);
    } else {
        DPRINT("Cancelled.\n");
    }
}

G_MODULE_EXPORT void auto_folder_entry_changed_cb(GtkAction *action) {
    GtkEntry *widget = GTK_ENTRY(GW("auto_folder_entry"));
    DPRINT("Auto folder changed to %s\n", gtk_entry_get_text(widget));
    if (plugin_config.autosave_path) {
        g_free(plugin_config.autosave_path);
    }
    plugin_config.autosave_path = g_strdup(gtk_entry_get_text(widget));
}

static bool auto_save_check(int format, int buffer) {
    GtkWidget *pw;
    gboolean autosave;
    gboolean autodelete;
    const gchar *filebase;
    gint counter;
    int ret;
    GtkSpinButton *spin;
    char *old_path = NULL;
    GtkProgressBar *pbar;
    bool deleted = false;
    char filename[256];

    pw = GW("auto_save_check");
    autosave = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pw));

    if (!autosave) {
        return false;
    }

    pbar = GTK_PROGRESS_BAR(GW("download_progress"));

    pw = GW("auto_delete_check");
    autodelete = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pw));

    spin = GTK_SPIN_BUTTON(GW("auto_name_spin"));
    counter = gtk_spin_button_get_value_as_int(spin);
    DPRINT("Counter = %d\n", counter);
    pw = GW("auto_name_entry");
    filebase = gtk_entry_get_text(GTK_ENTRY(pw));

    /* Change to auto-save path */
    if (plugin_config.autosave_path) {
        old_path = getcwd(NULL, 0);
        if (chdir(plugin_config.autosave_path) == -1) {
            char msg[256];

            snprintf(msg, sizeof(msg), "Could not save in folder %s: %s",
                     plugin_config.autosave_path, strerror(errno));
            error_message(msg);
            free(old_path);
            return false;
        }
    }

    gtk_statusbar_push(statusbar, sbar_download_ctx, "Auto-saving");
    wait_for_gtk_events_pending();

    snprintf(filename, sizeof(filename), "%s-%04d.%s", filebase, counter, file_formats[format].extension);
    DPRINT("Save buffer %d\n", buffer);
    gtk_progress_bar_set_text(pbar, filename);
    save_buffer(buffer, filename);
    gtk_progress_bar_set_text(pbar, NULL);

    if (autodelete) {
        int retry;
        pslr_status st;
        /* Init bufmask to 1's so that we don't see buffer as deleted
         * if we never got a good status. */
        st.bufmask = ~0;
        DPRINT("Delete buffer %d\n", buffer);
        for (retry = 0; retry < 5; retry++)  {
            ret = pslr_delete_buffer(camhandle, buffer);
            if (ret == PSLR_OK) {
                break;
            }
            DPRINT("Could not delete buffer %d: %d\n", buffer, ret);
            usleep(100000);
        }
        for (retry=0; retry<5; retry++) {
            pslr_get_status(camhandle, &st);
            if ((st.bufmask & (1<<buffer))==0) {
                break;
            }
            DPRINT("Buffer not gone - wait\n");
        }
        set_preview_icon(buffer, NULL);
        if ((st.bufmask & (1<<buffer)) == 0) {
            deleted = true;
        }
    }
    counter++;
    DPRINT("Set counter -> %d\n", counter);
    gtk_spin_button_set_value(spin, counter);

    if (old_path) {
        ssize_t r = chdir(old_path);
        if (r != 0) {
            fprintf(stderr, "chdir(old_path) failed");
        }

        free(old_path);
    }

    gtk_statusbar_pop(statusbar, sbar_download_ctx);

    //printf("auto_save_check done\n");
    return deleted;
}

static GdkPixbuf *pMainPixbuf = NULL;
uint8_t *pLastPreviewImage;
uint32_t lastPreviewImageSize;

//static GdkPixbuf *pThumbPixbuf[MAX_BUFFERS];

// updates the thumbnails and optionally the main preview
static void update_image_areas(int buffer, bool main) {
    GError *pError;
    int r;
    GdkPixbuf *pixBuf;

    DPRINT("update_image_areas\n");
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    double bulb_remain_sec = timeval_diff_sec(&expected_bulb_end_time, &current_time);
    GtkButton *shutterButton = (GtkButton *)GW("shutter_button");
    while (bulb_remain_sec>0) {
        static gchar bulb_message[100];
        sprintf (bulb_message, "BULB -> wait : %.0f seconds", bulb_remain_sec);
        gtk_button_set_label(shutterButton, bulb_message);
        sleep_sec(1);
        wait_for_gtk_events_pending();
        gettimeofday(&current_time, NULL);
        bulb_remain_sec = timeval_diff_sec(&expected_bulb_end_time, &current_time);
    }
    gtk_button_set_label(shutterButton, "Take picture");

    gtk_statusbar_push(statusbar, sbar_download_ctx, "Getting preview ");
    wait_for_gtk_events_pending();

    pError = NULL;
    DPRINT("Trying to read buffer %d %d\n", buffer, main);
    if (fullsize_preview || pslr_get_model_bufmask_single(camhandle)) {
        r = pslr_get_buffer(camhandle, buffer, PSLR_BUF_JPEG_MAX, 0, &pLastPreviewImage, &lastPreviewImageSize);
    } else {
        r = pslr_get_buffer(camhandle, buffer, PSLR_BUF_PREVIEW, 4, &pLastPreviewImage, &lastPreviewImageSize);
    }
    if (r != PSLR_OK) {
        printf("Could not get buffer data\n");
        goto the_end;
    }

    GInputStream *ginput = g_memory_input_stream_new_from_data (pLastPreviewImage, lastPreviewImageSize, NULL);
    pixBuf = gdk_pixbuf_new_from_stream( ginput, NULL, &pError);
    if (!pixBuf) {
        printf("No pixbuf from loader.\n");
        goto the_end;
    }
    g_object_ref(pixBuf);
    pError = NULL;
    if (main) {
        DPRINT("Setting pMainPixbuf\n");
        pMainPixbuf = pixBuf;
        GtkAllocation allocation;
        GtkWidget *pw;
        pw = GW("main_drawing_area");
        gtk_widget_get_allocation( pw, &allocation);
        gdk_window_invalidate_rect(gtk_widget_get_window(pw), &allocation, FALSE);
    }

    GdkPixbuf *scaledThumb = gdk_pixbuf_scale_simple( pixBuf, THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT, GDK_INTERP_BILINEAR);
    set_preview_icon(buffer, scaledThumb);

the_end:
    gtk_statusbar_pop(statusbar, sbar_download_ctx);
}

G_MODULE_EXPORT void menu_quit_activate_cb(GtkAction *action, gpointer user_data) {
    DPRINT("menu quit.\n");
    gtk_main_quit();
}

G_MODULE_EXPORT void menu_about_activate_cb(GtkAction *action, gpointer user_data) {
    GtkWidget *pw;
    DPRINT("menu about.\n");
    pw = GW("about_dialog");
    gtk_about_dialog_set_version( GTK_ABOUT_DIALOG(pw), VERSION);
    gtk_about_dialog_set_copyright( GTK_ABOUT_DIALOG(pw), copyright());
    gtk_dialog_run(GTK_DIALOG(pw));
    gtk_widget_hide(pw);
}

static void resize_preview_icons() {
    GtkWidget *pw;
    bool chk_preview;
    bool chk_histogram;

    chk_preview = gtk_toggle_action_get_active(GTA("menu_buffer_window"));

    chk_histogram = gtk_toggle_action_get_active(GTA("menu_histogram_window"));

    gtk_widget_set_size_request(GW("preview_icon_view"), chk_preview ? (chk_histogram ? 400 : 200) : 0, 10 );

    pw = GW("preview_icon_scrollwindow");
    if ( chk_preview ) {
        gtk_widget_set_size_request(pw, chk_histogram ? 440 : 220, 10 );
        gtk_widget_show( pw );
    } else {
        gtk_widget_set_size_request(pw, 0, 10 );
        gtk_widget_hide( pw );
    }
}

G_MODULE_EXPORT void menu_buffer_window_toggled_cb(GtkAction *action, gpointer user_data) {
    resize_preview_icons();
}

G_MODULE_EXPORT void menu_settings_window_toggled_cb(GtkAction *action, gpointer user_data) {
    guint checked;
    GtkWidget *pw;
    pw = GW("settings_window");
    checked = gtk_toggle_action_get_active(GTA("menu_settings_window"));
    DPRINT("settings window %d.\n", checked);
    if (checked) {
        gtk_widget_show(pw);
    } else {
        gtk_widget_hide(pw);
    }
}

G_MODULE_EXPORT int mainwindow_expose(GtkAction *action, gpointer userData) {
    GtkWidget *pw;
    GdkGC *gc_focus, *gc_sel, *gc_presel;
    GdkColor red = { 0, 65535, 0, 0 };
    GdkColor green = { 0, 0, 65535, 0 };
    GdkColor yellow = { 0, 65535, 65535, 0 };
    int i;
    gboolean fill;

    DPRINT("mainwindow_expose\n");
    pw = GW("main_drawing_area");
    GtkStyle *style=gtk_widget_get_style(pw);

    double ratio = 1.0;
    af_width_multiplier = 1.0;
    af_height_multiplier = 1.0;
    if (pMainPixbuf != NULL) {
        DPRINT("pMainPixbuf drawing\n");
        GdkPixbuf *pMainToRender;
        int pixbufWidth = gdk_pixbuf_get_width(pMainPixbuf);
        int pixbufHeight = gdk_pixbuf_get_height(pMainPixbuf);
        DPRINT("Preview image size: %d x %d\n", pixbufWidth, pixbufHeight);
        GtkAllocation allocation;
        gtk_widget_get_allocation( pw, &allocation);
        int areaWidth = allocation.width;
        int areaHeight = allocation.height;
        DPRINT("Preview area size: %d x %d\n", areaWidth, areaHeight);
        double ratioWidth = 1.0 * areaWidth / pixbufWidth;
        double ratioHeight = 1.0 * areaHeight / pixbufHeight;
        ratio = ratioWidth < ratioHeight ? ratioWidth : ratioHeight;
        ratio = ratio > 1 ? 1 : ratio;
        DPRINT("Scaling ratio: %f\n, ratio");
        if (ratio < 1) {
            pMainToRender = gdk_pixbuf_scale_simple( pMainPixbuf, pixbufWidth * ratio, pixbufHeight * ratio, GDK_INTERP_BILINEAR);
            af_width_multiplier = ratio * pixbufWidth / 640;
            af_height_multiplier = ratio * pixbufHeight / 480;
        } else {
            pMainToRender = pMainPixbuf;
        }
        gdk_draw_pixbuf(gtk_widget_get_window(pw), style->fg_gc[gtk_widget_get_state(pw)], pMainToRender, 0, 0, 0, 0, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
    }

    gc_focus = gdk_gc_new(gtk_widget_get_window(pw));
    gc_sel = gdk_gc_new(gtk_widget_get_window(pw));
    gc_presel = gdk_gc_new(gtk_widget_get_window(pw));
    gdk_gc_copy(gc_focus, style->fg_gc[gtk_widget_get_state (pw)]);
    gdk_gc_copy(gc_sel, style->fg_gc[gtk_widget_get_state (pw)]);
    gdk_gc_copy(gc_presel, style->fg_gc[gtk_widget_get_state (pw)]);

    gdk_gc_set_rgb_fg_color(gc_focus, &red);
    gdk_gc_set_rgb_fg_color(gc_sel, &green);
    gdk_gc_set_rgb_fg_color(gc_presel, &yellow);

    if (handle_af_points) {
        for (i=0; i<sizeof(af_points)/sizeof(af_points[0]); i++) {
            GdkGC *the_gc;
            the_gc = select_indicated_af_points & (1<<i) ? gc_sel : gc_focus;
            if (preselect_indicated_af_points & (1<<i)) {
                /* The user clicked an AF point and this should be
                 * indicated. If it's the same as the current AF point
                 * selected in the camera (indicated bu
                 * preselect_reselect), it should be green. If not, it
                 * means we're waiting for camera confirmation and it
                 * should be yellow. */
                if (preselect_reselect) {
                    the_gc = gc_sel;
                } else {
                    the_gc = gc_presel;
                }
            }
            fill = focus_indicated_af_points & (1<<i) ? TRUE : FALSE;
            gdk_draw_rectangle(gtk_widget_get_window(pw), the_gc, fill,
                               af_points[i].x * af_width_multiplier, af_points[i].y * af_height_multiplier,
                               af_points[i].w * af_width_multiplier, af_points[i].h * af_height_multiplier);
        }
    }

    gdk_gc_destroy(gc_focus);
    gdk_gc_destroy(gc_sel);
    gdk_gc_destroy(gc_presel);

    return FALSE;
}

G_MODULE_EXPORT int main_drawing_area_expose_event_cb(GtkAction *action, gpointer userData) {
    return mainwindow_expose( action, userData );
}

G_MODULE_EXPORT gboolean main_drawing_area_button_press_event_cb(GtkAction *action, GdkEventButton *event, gpointer user_data) {
    int x = rint(event->x);
    int y = rint(event->y);
    int i;
    int ret;
    GtkWidget *pw;

    DPRINT("main_drawing_area_button_press_event_cb");
    /* Don't care about clicks on AF points if no camera is connected. */
    if (!camhandle) {
        return TRUE;
    }

    if (handle_af_points) {
        /* Click one at a time please */
        if (preselect_indicated_af_points) {
            return TRUE;
        }

        for (i=0; i<sizeof(af_points)/sizeof(af_points[0]); i++) {
            if (is_inside(af_points[i].x * af_width_multiplier,
                          af_points[i].y * af_height_multiplier,
                          af_points[i].w * af_width_multiplier,
                          af_points[i].h * af_height_multiplier,
                          x, y)) {
                //printf("click: af point %d\n", i);

                preselect_indicated_af_points = 1<<i;
                if (status_new && status_new->selected_af_point == preselect_indicated_af_points) {
                    preselect_reselect = true;
                } else {
                    preselect_reselect = false;
                }
                pw = GW("main_drawing_area");
                GtkAllocation allocation;
                gtk_widget_get_allocation( pw, &allocation);
                gdk_window_invalidate_rect(gtk_widget_get_window(pw), &allocation, FALSE);
                wait_for_gtk_events_pending();
                if (status_new && status_new->af_point_select == PSLR_AF_POINT_SEL_SELECT) {
                    ret = pslr_select_af_point(camhandle, 1 << i);
                    if (ret != PSLR_OK) {
                        DPRINT("Could not select AF point %d\n", i);
                    }
                } else {
                    gtk_statusbar_push(statusbar, sbar_download_ctx, "Cannot select AF point in this AF mode.");
                    wait_for_gtk_events_pending();
                    sleep_sec(3);
                    gtk_statusbar_pop(statusbar, sbar_download_ctx);
                }
                break;
            }
        }
    }
    return TRUE;
}

static bool is_inside(int rect_x, int rect_y, int rect_w, int rect_h, int px, int py) {
    return px >= rect_x && py >= rect_y && px < rect_x + rect_w && py < rect_y + rect_h;
}

gboolean bufferwindow_expose_event_cb(GtkWidget *widget, GdkEventExpose *pEvent, gpointer userData) {
    DPRINT("bufferwindow expose\n");
    return FALSE;
}

GdkPixmap *calculate_histogram( GdkPixbuf *input, int hist_w, int hist_h ) {
    guchar *pixels;
    uint32_t histogram[256][3];
    int x, y;
    uint32_t scale;
    int pitch;
    int wx1, wy1, wx2, wy2;
    GdkGC *gc;
    int input_width, input_height;
    GdkColor cWhite = { 0, 65535, 65535, 65535 };

    if ( !input ) {
        return NULL;
    }

    const GdkColor hist_colors[] = {
        { 0, 65535, 0, 0 },
        { 0, 0, 65535, 0 },
        { 0, 0, 0, 65535 }
    };

    g_assert (gdk_pixbuf_get_colorspace (input) == GDK_COLORSPACE_RGB);
    g_assert (gdk_pixbuf_get_bits_per_sample (input) == 8);
    g_assert (!gdk_pixbuf_get_has_alpha (input));
    g_assert (gdk_pixbuf_get_n_channels(input) == 3);

    input_width = gdk_pixbuf_get_width(input);
    input_height = gdk_pixbuf_get_height(input);
    DPRINT("input: %d x %d\n", input_width, input_height);

    pixels = gdk_pixbuf_get_pixels(input);
    pitch = gdk_pixbuf_get_rowstride(input);

    memset(histogram, 0, sizeof(histogram));

    for (y=9.0/160*input_height; y<(151.0/160)*input_height; y++) {
        for (x=0; x<input_width; x++) {
            uint8_t r = pixels[y*pitch+x*3+0];
            uint8_t g = pixels[y*pitch+x*3+1];
            uint8_t b = pixels[y*pitch+x*3+2];
            histogram[r][0]++;
            histogram[g][1]++;
            histogram[b][2]++;
        }
    }

    scale = 0;
    for (y=0; y<3; y++) {
        for (x=0; x<256; x++) {
            if (histogram[x][y] > scale) {
                scale = histogram[x][y];
            }
        }
    }
    // draw onto
    GdkPixmap *output = gdk_pixmap_new( NULL, hist_w, 3*hist_h, 24);
    gc = gdk_gc_new(output);
    gdk_gc_set_rgb_fg_color(gc, &cWhite);
    gdk_draw_rectangle(output, gc, TRUE, 0, 0, hist_w, 3*hist_h);

    for (y=0; y<3; y++) {
        gdk_gc_set_rgb_fg_color(gc, &hist_colors[y]);
        for (x=0; x<256; x++) {
            wx1 = hist_w*x / 256;
            wx2 = hist_w*(x+1) / 256;
            int yval = histogram[x][y] * hist_h / scale;
            wy1 = (hist_h * y) + hist_h - yval;
            wy2 = (hist_h * y) + hist_h;
            gdk_draw_rectangle(output, gc, TRUE, wx1, wy1, wx2-wx1, wy2-wy1);
        }
    }

    return output;
}

G_MODULE_EXPORT void shutter_press(GtkAction *action) {
    int r;
    static gboolean is_bulbing_on = FALSE;
    int shutter_speed;
    const gchar * bulb_exp_str = NULL;
    pslr_status status;

    GtkWidget *widget = GW("shutter_button");
    if (is_bulbing_on == TRUE) {
        is_bulbing_on = FALSE;
        gtk_button_set_label((GtkButton *)widget, "Take picture");
        /* end current bulb shooting */
        pslr_bulb(camhandle, false);
        if (pslr_get_model_only_limited(camhandle)) {
            manage_camera_buffers_limited();
        }
        return;
    }
    DPRINT("Shutter press.\n");
    pslr_get_status(camhandle, &status);
    if (status.exposure_mode == PSLR_GUI_EXPOSURE_MODE_B) {
        GtkWidget * pw;
        pw = GW("bulb_exp_value");
        bulb_exp_str = gtk_entry_get_text(GTK_ENTRY(pw));
        if (bulb_exp_str == NULL) {
            return;
        }
        shutter_speed = atoi(bulb_exp_str);
        if (shutter_speed <= 0) {
            return;
        }
        if (pslr_get_model_old_bulb_mode(camhandle)) {
            is_bulbing_on = TRUE;
            pslr_bulb(camhandle, true);
            pslr_shutter(camhandle);
            while (shutter_speed > 0 && is_bulbing_on == TRUE) {
                static gchar bulb_message[100];
                sprintf (bulb_message, "BULB -> wait : %d seconds", shutter_speed);
                gtk_button_set_label((GtkButton *)widget, bulb_message);
                sleep_sec(1);
                shutter_speed--;
                wait_for_gtk_events_pending();
            }
            if (is_bulbing_on == TRUE) {
                pslr_bulb(camhandle, false);
                is_bulbing_on = FALSE;
                gtk_button_set_label((GtkButton *)widget, "Take picture");
            }
        } else {
            if (pslr_has_setting_by_name(camhandle, "bulb_timer")) {
                pslr_write_setting_by_name(camhandle, "bulb_timer", 1);
            } else if (pslr_has_setting_by_name(camhandle, "astrotracer")) {
                pslr_write_setting_by_name(camhandle, "astrotracer", 1);
            } else {
                fprintf(stderr, "New bulb mode is not supported for this camera model\n");
                return;
            }
            if (pslr_has_setting_by_name(camhandle, "bulb_timer_sec")) {
                pslr_write_setting_by_name(camhandle, "bulb_timer_sec", shutter_speed);
            } else if (pslr_has_setting_by_name(camhandle, "astrotracer_timer_sec")) {
                pslr_write_setting_by_name(camhandle, "astrotracer_timer_sec", shutter_speed);
            } else {
                fprintf(stderr, "New bulb mode is not supported for this camera model\n");
                return;
            }
            gettimeofday(&expected_bulb_end_time, NULL);
            expected_bulb_end_time.tv_sec += shutter_speed;
            pslr_shutter(camhandle);
        }
    } else {
        r = pslr_shutter(camhandle);
        if (r != PSLR_OK) {
            DPRINT("shutter error\n");
            return;
        }
    }

    if (pslr_get_model_only_limited(camhandle)) {
        manage_camera_buffers_limited();
    }
}

G_MODULE_EXPORT void focus_button_clicked_cb(GtkAction *action) {
    DPRINT("Focus");
    int ret;
    ret = pslr_focus(camhandle);
    if (ret != PSLR_OK) {
        DPRINT("Focus failed: %d\n", ret);
    }
}

G_MODULE_EXPORT void status_button_clicked_cb(GtkAction *action) {
    DPRINT("Status");
    GtkWidget *pw;
    pslr_status st;

    pslr_get_status(camhandle, &st);

    char *collected_status = collect_status_info(  camhandle, st );
    GtkLabel *label = GTK_LABEL(GW("status_label"));

    char *markup = g_markup_printf_escaped ("<tt>%s</tt>", collected_status);
    gtk_label_set_markup ( label, markup);
    g_free (markup);
    free( collected_status );

    pw = GW("statuswindow");
    gtk_window_set_title( (GtkWindow *)pw, "Status Info");
    gtk_window_present(GTK_WINDOW(pw));
}

G_MODULE_EXPORT void status_hex_button_clicked_cb(GtkAction *action) {
    DPRINT("Status hex");
    GtkWidget *pw;

    int status_bufsize = pslr_get_model_status_buffer_size( camhandle );
    uint8_t status_buffer[MAX_STATUS_BUF_SIZE];
    pslr_get_status_buffer(camhandle, status_buffer);
    char *collected_status_hex = shexdump( status_buffer, status_bufsize > 0 ? status_bufsize : MAX_STATUS_BUF_SIZE);
    GtkLabel *label = GTK_LABEL(GW("status_label"));

    char *markup = g_markup_printf_escaped ("<tt>%s</tt>", collected_status_hex);
    gtk_label_set_markup ( label, markup);
    g_free (markup);
    free( collected_status_hex );

    pw = GW("statuswindow");
    gtk_window_set_title( (GtkWindow *)pw, "Status Hexdump");
    gtk_window_present(GTK_WINDOW(pw));
}

G_MODULE_EXPORT void settings_button_clicked_cb(GtkAction *action) {
    DPRINT("Settings");
    GtkWidget *pw;

    char *collected_settings = collect_settings_info(  camhandle, settings );
    GtkLabel *label = GTK_LABEL(GW("status_label"));

    char *markup = g_markup_printf_escaped ("<tt>%s</tt>", collected_settings);
    gtk_label_set_markup ( label, markup);
    g_free (markup);
    free( collected_settings );

    pw = GW("statuswindow");
    gtk_window_set_title( (GtkWindow *)pw, "Settings Info");
    gtk_window_present(GTK_WINDOW(pw));
}


G_MODULE_EXPORT void green_button_clicked_cb(GtkAction *action) {
    DPRINT("Green btn");
    int ret;
    ret = pslr_green_button( camhandle );
    if (ret != PSLR_OK) {
        DPRINT("Green button failed: %d\n", ret);
        gtk_statusbar_push(statusbar, sbar_connect_ctx, "Error: green button failed.");
    }
}

G_MODULE_EXPORT void ae_lock_button_toggled_cb(GtkAction *action) {
    DPRINT("AE Lock");
    gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(GW("ae_lock_button")));
    DPRINT("ACTIVE: %d\n", active);
    int ret;
    gboolean locked;
    if (status_new == NULL) {
        return;
    }
    locked = (status_new->light_meter_flags & PSLR_LIGHT_METER_AE_LOCK) != 0;
    if (locked != active) {
        ret = pslr_ae_lock(camhandle, active);
        if (ret != PSLR_OK) {
            DPRINT("AE lock failed: %d\n", ret);
        }
    }
}

static void my_atexit(void) {
    DPRINT("atexit\n");
}

static gboolean added_quit(gpointer data) {
    DPRINT("added_quit\n");
    if (camhandle) {

        if (need_one_push_bracketing_cleanup) {
            pslr_write_setting_by_name(camhandle, "one_push_bracketing", 1);
        }

        pslr_disconnect(camhandle);
        pslr_shutdown(camhandle);
        camhandle = 0;
    }
    return FALSE;
}

G_MODULE_EXPORT void plugin_quit(GtkAction *action) {
    DPRINT("Quit plugin.\n");
    gtk_main_quit();
}

G_MODULE_EXPORT gboolean settings_window_delete_event_cb(GtkAction *action, gpointer user_data) {
    GtkToggleAction *pw;
    DPRINT("Hide settings window.\n");
    pw = GTA ("menu_settings_window");
    gtk_widget_hide(GW("settings_window"));
    gtk_toggle_action_set_active(pw, FALSE);
    return TRUE;
}

G_MODULE_EXPORT gboolean statuswindow_delete_event_cb(GtkAction *action, gpointer user_data) {
    DPRINT("Hide statuswindow.\n");
    gtk_widget_hide(GW("statuswindow"));
    return TRUE;
}


void error_message(const gchar *message) {
    GtkWidget *pw;

    pw = GW("error_message");
    gtk_label_set_markup(GTK_LABEL(pw), message);
    pw = GW("errordialog");
    gtk_dialog_run(GTK_DIALOG(pw));
    //gtk_widget_show(pw);
}


G_MODULE_EXPORT gboolean error_dialog_close(GtkAction *action) {
    DPRINT("close event.\n");
    gtk_widget_hide(GW("errordialog"));
    return FALSE;
}

void init_preview_area(void) {
    GtkWidget *pw;
    GtkTreeIter iter;
    gint i;

    DPRINT("init_preview_area\n");

    pw = GW("preview_icon_view");

    list_store = gtk_list_store_new (3,
                                     GDK_TYPE_PIXBUF, // thumbnail
                                     GDK_TYPE_PIXMAP, // histogram
                                     GDK_TYPE_PIXBUF  // visible icon
                                    );

    for (i = 0; i < MAX_BUFFERS; i++) {
        /* Add a new row to the model */
        gtk_list_store_append (list_store, &iter);
        gtk_list_store_set (list_store, &iter,
                            0, NULL, 1, NULL, 2, NULL,
                            -1);
    }

    gtk_icon_view_set_model(GTK_ICON_VIEW(pw), GTK_TREE_MODEL(list_store));
}

GdkPixbuf *merge_preview_icons( GdkPixbuf *thumb, GdkPixmap *histogram ) {
    GdkPixbuf *output;
    if ( need_histogram ) {
        GdkPixmap *mMerged = gdk_pixmap_new(NULL, 2*HISTOGRAM_WIDTH, HISTOGRAM_HEIGHT, 24);
        GdkGC *gc = gdk_gc_new( mMerged );
        GdkPixbuf *scaledThumb = gdk_pixbuf_scale_simple( thumb, HISTOGRAM_WIDTH, HISTOGRAM_HEIGHT, GDK_INTERP_BILINEAR);
        gdk_draw_pixbuf( mMerged, gc, scaledThumb, 0, 0, 0, 0, HISTOGRAM_WIDTH, HISTOGRAM_HEIGHT, GDK_RGB_DITHER_NONE, 0, 0);
        gdk_draw_drawable( mMerged, gc, histogram, 0, 0, HISTOGRAM_WIDTH, 0, HISTOGRAM_WIDTH, HISTOGRAM_HEIGHT);
        GdkPixbuf *pMerged = gdk_pixbuf_get_from_drawable( NULL, mMerged, GDK_COLORSPACE_RGB, 0, 0, 0, 0, 2*HISTOGRAM_WIDTH, HISTOGRAM_HEIGHT );
        output = gdk_pixbuf_scale_simple( pMerged, 2*THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT, GDK_INTERP_BILINEAR);
    } else {
        output = thumb;
    }
    return output;
}

G_MODULE_EXPORT void menu_histogram_window_toggled_cb(GtkAction *action, gpointer user_data) {
    GtkTreePath *path;
    GtkTreeIter iter;
    GdkPixbuf *thumb;
    GdkPixmap *hist;
    int i;

    resize_preview_icons();

    DPRINT("before need_histogram\n");
    need_histogram =  gtk_toggle_action_get_active(GTA("menu_histogram_window"));
    DPRINT("after need_histogram %d\n", need_histogram);

    for (i = 0; i < MAX_BUFFERS; i++) {
        path = gtk_tree_path_new_from_indices (i, -1);
        gtk_tree_model_get_iter (GTK_TREE_MODEL (list_store), &iter, path);
        gtk_tree_path_free (path);
        gtk_tree_model_get( GTK_TREE_MODEL (list_store), &iter, 0, &thumb, 1, &hist, -1 );
        if ( thumb ) {
            GdkPixbuf *pMerged = merge_preview_icons( thumb, hist );
            gtk_list_store_set (list_store, &iter, 0, thumb, 1, hist, 2, pMerged, -1);
        }
    }
}

G_MODULE_EXPORT void menu_fullsize_preview_toggled_cb(GtkAction *action, gpointer user_data) {
    fullsize_preview =  gtk_toggle_action_get_active(GTA("menu_fullsize_preview"));
    DPRINT("menu_fullsize_preview %d\n", fullsize_preview);
}

void set_preview_icon(int n, GdkPixbuf *pBuf) {
    GtkTreePath *path;
    GtkTreeIter iter;
    DPRINT("set_preview_icon\n");
    path = gtk_tree_path_new_from_indices (n, -1);
    gtk_tree_model_get_iter (GTK_TREE_MODEL (list_store),
                             &iter,
                             path);
    gtk_tree_path_free (path);
    GdkPixmap *gpm = calculate_histogram( pBuf, HISTOGRAM_WIDTH, HISTOGRAM_HEIGHT/3 );

    GdkPixbuf *pMerged = merge_preview_icons( pBuf, gpm );
    gtk_list_store_set (list_store, &iter, 0, pBuf, 1, gpm, 2, pMerged, -1);
}

G_MODULE_EXPORT gchar* shutter_scale_format_value_cb(GtkAction *action, gdouble value) {
    int idx = rint(value);
    pslr_rational_t *tbl = 0;
    int steps = 0;
    if (!status_new) {
        return g_strdup_printf("(%f)", value);
    }
    which_shutter_table(status_new, &tbl, &steps);

    if (idx >= 0 && idx < steps) {
        int n = tbl[idx].nom;
        int d = tbl[idx].denom;
        if (n == 1) {
            return g_strdup_printf("1/%d", d);
        } else if (d == 1) {
            return g_strdup_printf("%d\"", n);
        } else {
            return g_strdup_printf("%.1f\"", (float)n/(float)d);
        }
    } else {
        return g_strdup_printf("(%f)", value);
    }
}

G_MODULE_EXPORT gchar* aperture_scale_format_value_cb(GtkAction *action, gdouble value) {
    int idx = rint(value);
    //printf("aperture value: %f\n", value);
    if (idx < sizeof(aperture_tbl)/sizeof(aperture_tbl[0])) {
        return g_strdup_printf("f/%.1f", aperture_tbl[idx]/10.0);
    } else {
        return g_strdup_printf("(%f)", value);
    }
}

G_MODULE_EXPORT gchar* iso_scale_format_value_cb(GtkAction *action, gdouble value) {
    DPRINT("iso_scale_format_value_cb\n");
    int i = rint(value);
    DPRINT("iso index %f %d\n", value, i);
    const int *tbl = 0;
    int steps = 0;
    if (status_new) {
        which_iso_table(status_new, &tbl, &steps);
        if (i >= 0 && i < steps) {
            DPRINT("printable iso: %d\n", tbl[i]);
            return g_strdup_printf("%d", tbl[i]);
        }
    }
    DPRINT("printable iso: (%d)\n", i);
    return g_strdup_printf("(%d)", i);
}

G_MODULE_EXPORT gchar* ec_scale_format_value_cb(GtkAction *action, gdouble value) {
    int i = rint(value);
    const int *tbl;
    int steps;
    if (status_new) {
        which_ec_table(status_new, &tbl, &steps);
        if (i >= 0 && i < steps) {
            return g_strdup_printf("%.1f", tbl[i]/10.0);
        }
    }
    return g_strdup_printf("(%f)", value);
}

G_MODULE_EXPORT void aperture_scale_value_changed_cb(GtkAction *action, gpointer user_data) {
//    DPRINT("APERTURE CHANGE\n");
    gdouble a;
    pslr_rational_t value;
    int idx;
    int ret;

    if ( in_initcontrols ) {
        return;
    }

    if (!status_new) {
        return;
    }
    a = gtk_range_get_value(GTK_RANGE(GW("aperture_scale")));
    idx = rint(a);
    assert(idx >= 0);
    assert(idx < sizeof(aperture_tbl)/sizeof(aperture_tbl[0]));
    value.nom = aperture_tbl[idx];
    value.denom = 10;
    DPRINT("aperture->%d/%d\n", value.nom, value.denom);
    ret = pslr_set_aperture(camhandle, value);
    if (ret != PSLR_OK) {
        DPRINT("Set aperture failed: %d\n", ret);
    }
}

G_MODULE_EXPORT void shutter_scale_value_changed_cb(GtkAction *action, gpointer user_data) {
    gdouble a;
    pslr_rational_t value;
    int ret;
    int idx;
    pslr_rational_t *tbl;
    int steps;

    if (!status_new) {
        return;
    }
    a = gtk_range_get_value(GTK_RANGE(GW("shutter_scale")));
    idx = rint(a);
    which_shutter_table(status_new, &tbl, &steps);
    assert(idx >= 0);
    assert(idx < steps);
    value = tbl[idx];
    DPRINT("shutter->%d/%d\n", value.nom, value.denom);
    ret = pslr_set_shutter(camhandle, value);
    if (ret != PSLR_OK) {
        DPRINT("Set shutter failed: %d\n", ret);
    }
}

G_MODULE_EXPORT void iso_scale_value_changed_cb(GtkAction *action, gpointer user_data) {
    int idx;
    int ret;
    const int *tbl;
    int steps;

    if (!status_new) {
        return;
    }

    idx = rint(gtk_range_get_value(GTK_RANGE(GW("iso_scale"))));
    which_iso_table(status_new, &tbl, &steps);
    assert(idx >= 0);
    assert(idx <= steps);
    DPRINT("cam iso = %d\n", status_new->fixed_iso);
    DPRINT("iso->%d\n", tbl[idx]);
    /* If the known camera iso is same as new slider value, then the
     * slider change came from the camera (via init_controls), not
     * user change, and we should NOT send any new value to the
     * camera; for example the Fn menu will be exited if we do. */
    if (status_new->fixed_iso != tbl[idx]) {
        ret = pslr_set_iso(camhandle, tbl[idx], 0, 0);
        if (ret != PSLR_OK) {
            DPRINT("Set ISO failed: %d\n", ret);
        }
    }
}

G_MODULE_EXPORT void ec_scale_value_changed_cb(GtkAction *action, gpointer user_data) {
    gdouble a;
    pslr_rational_t new_ec;
    int ret;
    int idx;
    const int *tbl;
    int steps;
    if (!status_new) {
        return;
    }

    a = gtk_range_get_value(GTK_RANGE(GW("ec_scale")));
    which_ec_table(status_new, &tbl, &steps);
    idx = rint(a);
    DPRINT("EC->%d\n", idx);
    assert(idx >= 0);
    assert(idx < steps);
    new_ec.nom = tbl[idx];
    new_ec.denom = 10;
    DPRINT("new_ec: %d / %d\n", new_ec.nom, new_ec.denom);
    if (status_new == NULL) {
        return;
    }
    if (status_new->ec.nom != new_ec.nom || status_new->ec.denom != new_ec.denom) {
        ret = pslr_set_ec(camhandle, new_ec);
        if (ret != PSLR_OK) {
            DPRINT("Set EC failed: %d\n", ret);
        }
    }
    DPRINT("End of Set EC\n");
}

G_MODULE_EXPORT void jpeg_resolution_combo_changed_cb(GtkAction *action, gpointer user_data) {
    DPRINT("jpeg res active start\n");
    if ( !status_new ) {
        DPRINT("jpeg res active !status_new\n");
        return;
    }

    int ret;

    int idx = gtk_combo_box_get_active(GTK_COMBO_BOX(GW("jpeg_resolution_combo")));
    DPRINT("jpeg res active->%d\n", idx);
    int *resolutions = pslr_get_model_jpeg_resolutions( camhandle );
    int megapixel = resolutions[idx];
    DPRINT("jpeg res active->%d\n", megapixel);
    /* Prevent menu exit (see comment for iso_scale_value_changed_cb) */
    if (status_new == NULL || pslr_get_jpeg_resolution(camhandle, status_new->jpeg_resolution) != megapixel) {
        ret = pslr_set_jpeg_resolution(camhandle, megapixel);
        if (ret != PSLR_OK) {
            DPRINT("Set JPEG resolution failed.\n");
        }
    }
}

G_MODULE_EXPORT void jpeg_quality_combo_changed_cb(GtkAction *action, gpointer user_data) {
    DPRINT("start jpeg_quality_combo_changed_cb\n");
    int idx = gtk_combo_box_get_active(GTK_COMBO_BOX(GW("jpeg_quality_combo")));
    int val=pslr_get_model_max_jpeg_stars( camhandle)-idx;
    int ret;

    /* Prevent menu exit (see comment for iso_scale_value_changed_cb) */
    if (status_new == NULL || status_new->jpeg_quality != val) {
        ret = pslr_set_jpeg_stars(camhandle, val);
        if (ret != PSLR_OK) {
            DPRINT("Set JPEG quality failed.\n");
        }
    }
}

G_MODULE_EXPORT void jpeg_image_tone_combo_changed_cb(GtkAction *action, gpointer user_data) {
    pslr_jpeg_image_tone_t val = gtk_combo_box_get_active(GTK_COMBO_BOX(GW("jpeg_image_tone_combo")));
    int ret;
    DPRINT("jpeg image_tone active->%d %d\n", val, PSLR_JPEG_IMAGE_TONE_MAX);
    assert( (int)val >= -1);
    assert( (int)val < PSLR_JPEG_IMAGE_TONE_MAX);
    /* Prevent menu exit (see comment for iso_scale_value_changed_cb) */
    if ( val != -1 && (status_new == NULL || status_new->jpeg_image_tone != val) ) {
        ret = pslr_set_jpeg_image_tone(camhandle, val);
        if (ret != PSLR_OK) {
            DPRINT("Set JPEG image tone failed.\n");
        }
    }
}

G_MODULE_EXPORT void jpeg_sharpness_scale_value_changed_cb(GtkAction *action, gpointer user_data) {
    if ( in_initcontrols ) {
        return;
    }
    DPRINT("before get sharpness\n");
    int value = rint(gtk_range_get_value(GTK_RANGE(GW("jpeg_sharpness_scale"))));
    DPRINT("after get sharpness\n");
    int ret;
    assert(value >= -get_jpeg_property_shift());
    assert(value <= get_jpeg_property_shift());
    ret = pslr_set_jpeg_sharpness(camhandle, value);
    if (ret != PSLR_OK) {
        DPRINT("Set JPEG sharpness failed.\n");
    }
}

G_MODULE_EXPORT void jpeg_contrast_scale_value_changed_cb(GtkAction *action, gpointer user_data) {
    if ( in_initcontrols ) {
        return;
    }
    DPRINT("before get contrast\n");
    int value = rint(gtk_range_get_value(GTK_RANGE(GW("jpeg_contrast_scale"))));
    DPRINT("after get contrast %d\n",value);
    int ret;
    assert(value >= -get_jpeg_property_shift());
    assert(value <= get_jpeg_property_shift());
    ret = pslr_set_jpeg_contrast(camhandle, value);
    if (ret != PSLR_OK) {
        DPRINT("Set JPEG contrast failed.\n");
    }
}

G_MODULE_EXPORT void jpeg_hue_scale_value_changed_cb(GtkAction *action, gpointer user_data) {
    if ( in_initcontrols ) {
        return;
    }
    int value = rint(gtk_range_get_value(GTK_RANGE(GW("jpeg_hue_scale"))));
    int ret;
    assert(value >= -get_jpeg_property_shift());
    assert(value <= get_jpeg_property_shift());
    ret = pslr_set_jpeg_hue(camhandle, value);
    if (ret != PSLR_OK) {
        DPRINT("Set JPEG hue failed.\n");
    }
    DPRINT("end jpeg_hue_scale_value_changed_cb\n");
}


G_MODULE_EXPORT void jpeg_saturation_scale_value_changed_cb(GtkAction *action, gpointer user_data) {
    if ( in_initcontrols ) {
        return;
    }
    DPRINT("before get saturation\n");
    int value = rint(gtk_range_get_value(GTK_RANGE(GW("jpeg_saturation_scale"))));
    DPRINT("after get saturation\n");
    int ret;
    assert(value >= -get_jpeg_property_shift());
    assert(value <= get_jpeg_property_shift());
    ret = pslr_set_jpeg_saturation(camhandle, value);
    if (ret != PSLR_OK) {
        DPRINT("Set JPEG saturation failed.\n");
    }
}

G_MODULE_EXPORT void preview_icon_view_selection_changed_cb(GtkAction *action) {
    GList *l;
    guint len;
    gboolean en;

    /* If nothing is selected, disable the action buttons. Otherwise
     * enable them. */
    GtkIconView *icon_view = GTK_ICON_VIEW(GW("preview_icon_view"));
    l = gtk_icon_view_get_selected_items(icon_view);
    len = g_list_length(l);
    g_list_foreach (l, (GFunc) gtk_tree_path_free, NULL);
    g_list_free (l);
    en = len > 0;

    gtk_widget_set_sensitive(GW("preview_save_as_button"), en);
    gtk_widget_set_sensitive(GW("preview_delete_button"), en);
}

static pslr_buffer_type get_image_type_based_on_ui() {
    GtkWidget *pw;
    int quality;
    //int resolution;
    int filefmt;
    pslr_buffer_type imagetype;

    pw = GW("jpeg_quality_combo");
    quality = gtk_combo_box_get_active(GTK_COMBO_BOX(pw));
    //pw = GW("jpeg_resolution_combo");
    //resolution = gtk_combo_box_get_active(GTK_COMBO_BOX(pw));
    pw = GW("file_format_combo");
    filefmt = gtk_combo_box_get_active(GTK_COMBO_BOX(pw));

    if (filefmt == USER_FILE_FORMAT_PEF) {
        imagetype = PSLR_BUF_PEF;
    } else if (filefmt == USER_FILE_FORMAT_DNG) {
        imagetype = PSLR_BUF_DNG;
    } else {
        imagetype = pslr_get_jpeg_buffer_type(camhandle, quality);
    }
    return imagetype;
}

static void save_buffer_single(const char *filename, const pslr_buffer_type imagetype) {
    int fd;
    if ((imagetype == PSLR_BUF_PEF || imagetype == PSLR_BUF_DNG)) {
        fprintf(stderr, "Cannot download RAW images for this model if preview is already visible");
    } else {
        fd = open(filename, FILE_ACCESS, 0664);
        if (fd == -1) {
            perror("could not open target");
            return;
        }
        write(fd, pLastPreviewImage, lastPreviewImageSize);
        wait_for_gtk_events_pending();
        close(fd);
    }
}

static void save_file_from_buffer(int fd) {
    uint8_t buf[65536];
    uint32_t length;
    uint32_t current = 0;
    GtkProgressBar *progress_bar = GTK_PROGRESS_BAR(GW("download_progress"));
    length = pslr_buffer_get_size(camhandle);
    while (true) {
        uint32_t bytes;
        bytes = pslr_buffer_read(camhandle, buf, sizeof(buf));
        //printf("Read %d bytes\n", bytes);
        if (bytes == 0) {
            break;
        }
        ssize_t written_bytes = write(fd, buf, bytes);
        if (written_bytes == 0) {
            DPRINT("write(buf): Nothing has been written to buf.\n");
        } else if (written_bytes == -1) {
            perror("write(buf)");
        } else if (written_bytes < bytes) {
            DPRINT("write(buf): only write %d bytes, should be %d bytes.\n", written_bytes, bytes);
        }

        current += bytes;
        gtk_progress_bar_set_fraction(progress_bar, (gdouble) current / (gdouble) length);
        wait_for_gtk_events_pending();
    }
}

/*
 * Save the indicated buffer using the current UI file format
 * settings.  Updates the progress bar periodically & runs the GTK
 * main loop to show it.
 */
static void save_buffer(int bufno, const char *filename) {
    int r;
    int fd;
    GtkComboBox *resolution_combo_box;
    int resolution;
    pslr_buffer_type imagetype;

    resolution_combo_box = GTK_COMBO_BOX(GW("jpeg_resolution_combo"));
    resolution = gtk_combo_box_get_active(resolution_combo_box);

    imagetype = get_image_type_based_on_ui();
    if (pslr_get_model_bufmask_single(camhandle)) {
        save_buffer_single(filename, imagetype);
        return;
    }

    DPRINT("get buffer %d type %d res %d\n", bufno, imagetype, resolution);
    r = pslr_buffer_open(camhandle, bufno, imagetype, resolution);
    if (r != PSLR_OK) {
        DPRINT("Could not open buffer: %d\n", r);
        return;
    }

    fd = open(filename, FILE_ACCESS, 0664);
    if (fd == -1) {
        perror("could not open target");
        return;
    }

    save_file_from_buffer(fd);
    close(fd);
    pslr_buffer_close(camhandle);
}

G_MODULE_EXPORT void preview_save_as_cb(GtkAction *action) {
    GtkWidget *pw, *icon_view;
    GList *l, *i;
    GtkProgressBar *pbar;
    DPRINT("preview save as\n");
    icon_view = GW("preview_icon_view");
    l = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(icon_view));
    pbar = (GtkProgressBar *) GW("download_progress");
    for (i=g_list_first(l); i; i=g_list_next(i)) {
        GtkTreePath *p;
        int d, *pi;
        int res;
        p = i->data;
        d = gtk_tree_path_get_depth(p);
        DPRINT("Tree depth = %d\n", d);
        pi = gtk_tree_path_get_indices(p);
        DPRINT("Selected item = %d\n", *pi);

        pw = GW("save_as_dialog");

        res = gtk_dialog_run(GTK_DIALOG(pw));
        DPRINT("Run dialog -> %d\n", res);
        gtk_widget_hide(pw);
        if (res > 0) {
            char *sel_filename;
            char *filename;
            sel_filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (pw));
            char *dot = strrchr(sel_filename, '.');
            if (dot) {
                filename = sel_filename;
            } else {
                filename = malloc(strlen(sel_filename)+10);
                pw = GW("file_format_combo");
                int filefmt = gtk_combo_box_get_active(GTK_COMBO_BOX(pw));
                sprintf(filename, "%s.%s", sel_filename, file_formats[filefmt].extension);
            }
            DPRINT("Save to: %s\n", filename);
            gtk_progress_bar_set_text(pbar, filename);
            save_buffer(*pi, filename);
            gtk_progress_bar_set_text(pbar, NULL);
        }
    }
    g_list_foreach (l, (GFunc) gtk_tree_path_free, NULL);
    g_list_free (l);
}

G_MODULE_EXPORT void preview_save_as_cancel( GtkAction *action ) {
}

G_MODULE_EXPORT void preview_save_as_save( GtkAction *action ) {
}

G_MODULE_EXPORT void preview_delete_button_clicked_cb(GtkAction *action) {
    GtkWidget *icon_view;
    GList *l, *i;
    int ret;
    int retry;
    pslr_status st;

    DPRINT("preview delete\n");

    icon_view = GW("preview_icon_view");
    l = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(icon_view));
    for (i=g_list_first(l); i; i=g_list_next(i)) {
        GtkTreePath *p;
        int d, *pi;
        p = i->data;
        d = gtk_tree_path_get_depth(p);
        DPRINT("Tree depth = %d\n", d);
        pi = gtk_tree_path_get_indices(p);
        DPRINT("Selected item = %d\n", *pi);

        // needed? : g_object_unref(thumbpixbufs[i])?
        set_preview_icon(*pi, NULL);

        ret = pslr_delete_buffer(camhandle, *pi);
        if (ret != PSLR_OK) {
            DPRINT("Could not delete buffer %d: %d\n", *pi, ret);
        }
        for (retry=0; retry<5; retry++) {
            pslr_get_status(camhandle, &st);
            if ((st.bufmask & (1<<*pi))==0) {
                break;
            }
            DPRINT("Buffer not gone - retry\n");
        }
    }

    g_list_foreach (l, (GFunc) gtk_tree_path_free, NULL);
    g_list_free (l);
}

G_MODULE_EXPORT void file_format_combo_changed_cb(GtkAction *action, gpointer user_data) {
    DPRINT("file_format_combo_changed_cb\n");
    int val = gtk_combo_box_get_active(GTK_COMBO_BOX(GW("file_format_combo")));
    pslr_set_user_file_format( camhandle, val );
}

G_MODULE_EXPORT void user_mode_combo_changed_cb(GtkAction *action, gpointer user_data) {
    DPRINT("user_mode_combo_changed_cb\n");
    pslr_gui_exposure_mode_t val = gtk_combo_box_get_active(GTK_COMBO_BOX(GW("user_mode_combo")));
    if (!status_new) {
        return;
    }
    assert(val >= 0);
    assert(val < PSLR_GUI_EXPOSURE_MODE_MAX);
    if (status_new == NULL || val != status_new->exposure_mode ) {
        pslr_set_exposure_mode(camhandle, val);
    }
}

static void which_iso_table(pslr_status *st, const int **table, int *steps) {
    if (st->custom_sensitivity_steps == PSLR_CUSTOM_SENSITIVITY_STEPS_1EV) {
        *table = iso_tbl_1;
        *steps = sizeof(iso_tbl_1)/sizeof(iso_tbl_1[0]);
    } else if (st->custom_ev_steps == PSLR_CUSTOM_EV_STEPS_1_2) {
        *table = iso_tbl_1_2;
        *steps = sizeof(iso_tbl_1_2)/sizeof(iso_tbl_1_2[0]);
    } else {
        *table = iso_tbl_1_3;
        *steps = sizeof(iso_tbl_1_3)/sizeof(iso_tbl_1_3[0]);
    }
    assert(*table);
    assert(*steps);
}

static void which_ec_table(pslr_status *st, const int **table, int *steps) {
    if (st->custom_ev_steps == PSLR_CUSTOM_EV_STEPS_1_2) {
        *table = ec_tbl_1_2;
        *steps = sizeof(ec_tbl_1_2)/sizeof(ec_tbl_1_2[0]);
    } else {
        *table = ec_tbl_1_3;
        *steps = sizeof(ec_tbl_1_3)/sizeof(ec_tbl_1_3[0]);
    }
    assert(*table);
    assert(*steps);
}

static void which_shutter_table(pslr_status *st, pslr_rational_t **table, int *steps) {
    if (st->custom_ev_steps == PSLR_CUSTOM_EV_STEPS_1_2) {
        *table = shutter_tbl_1_2;
        *steps = sizeof(shutter_tbl_1_2)/sizeof(shutter_tbl_1_2[0]);
    } else {
        *table = shutter_tbl_1_3;
        *steps = sizeof(shutter_tbl_1_3)/sizeof(shutter_tbl_1_3[0]);
    }
    assert(*table);
    assert(*steps);
}

static struct option const longopts[] = {
    {"debug", no_argument, NULL, 4},
    {"dangerous", no_argument, NULL, 25},
    { NULL, 0, NULL, 0}
};

void gui_getopt(int argc, char **argv) {
    int optc;
    while ((optc = getopt_long(argc, argv, "4", longopts, NULL)) != -1) {
        switch (optc) {
            case 4:
                debug = true;
                break;

            case 25:
                dangerous = true;
                break;
        }
    }
}

int main(int argc, char **argv) {
    gui_getopt(argc, argv);
    DPRINT("%s %s \n", argv[0], VERSION);
    int r;
    r = common_init();
    if (r < 0) {
        printf("Could not initialize device.\n");
    }
    return 0;
}
