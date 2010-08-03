// AEq -- Equalizer plugin for ALSA
// Copyright 2010 John Lindgren <john.lindgren@tds.net>
//
// This program is free software: you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free Software
// Foundation, version 2.
//
// This program is distributed in the hope that it will be useful, but without
// any warranty; without even the implied warranty of merchantability or fitness
// for a particular purpose.  See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// this program.  If not, see <http://www.gnu.org/licenses/>.

#include <limits.h>
#include <stdlib.h>

#include "common.h"

#include <gtk/gtk.h>

#define MAX_GAIN 10
#define STEP 1

static char config_path[PATH_MAX];
static int on;
static float bands[BANDS];

static void on_off (GtkToggleButton * tog) {
   on = gtk_toggle_button_get_active (tog);
   write_config (config_path, on, bands);
}

static GtkWidget * create_switch (void) {
   GtkWidget * tog = gtk_check_button_new_with_mnemonic ("_Enable");
   gtk_toggle_button_set_active ((GtkToggleButton *) tog, on);
   g_signal_connect (tog, "toggled", (GCallback) on_off, NULL);
   return tog;
}

static char * format_value (GtkScale * s __attribute ((unused)), double val) {
   return g_strdup_printf ("%d", -(int) val);
}

static void changed (GtkRange * slid, float * val) {
   * val = -(float) gtk_range_get_value (slid);
   write_config (config_path, on, bands);
}

static GtkWidget * create_slider (const gchar * name, float * val) {
   GtkWidget * vbox = gtk_vbox_new (0, 6);
   GtkWidget * label = gtk_label_new (name);
   gtk_label_set_angle ((GtkLabel *) label, 90);
   gtk_box_pack_start ((GtkBox *) vbox, label, 1, 0, 0);
   GtkWidget * slid = gtk_vscale_new_with_range (-MAX_GAIN, MAX_GAIN, STEP);
   gtk_scale_set_draw_value ((GtkScale *) slid, 1);
   gtk_scale_set_value_pos ((GtkScale *) slid, GTK_POS_BOTTOM);
   gtk_widget_set_size_request (slid, -1, 144);
   gtk_range_set_value ((GtkRange *) slid, * val);
   g_signal_connect (slid, "format-value", (GCallback) format_value, NULL);
   g_signal_connect (slid, "value-changed", (GCallback) changed, val);
   gtk_box_pack_start ((GtkBox *) vbox, slid, 0, 0, 0);
   return vbox;
}

static void create_window (void) {
   GtkWidget * win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
   gtk_window_set_title ((GtkWindow *) win, "AEq Equalizer");
   gtk_window_set_resizable ((GtkWindow *) win, 0);
   gtk_container_set_border_width ((GtkContainer *) win, 6);
   g_signal_connect (win, "destroy", (GCallback) gtk_main_quit, NULL);
   GtkWidget * vbox = gtk_vbox_new (0, 6);
   gtk_container_add ((GtkContainer *) win, vbox);
   gtk_box_pack_start ((GtkBox *) vbox, create_switch (), 0, 0, 0);
   GtkWidget * hbox = gtk_hbox_new (0, 6);
   gtk_box_pack_start ((GtkBox *) vbox, hbox, 0, 0, 0);
   for (int i = 0; i < BANDS; i ++)
      gtk_box_pack_start ((GtkBox *) hbox, create_slider (labels[i], bands + i),
       0, 0, 0);
   gtk_widget_show_all (win);
}

static void error_main (void) {
   GtkWidget * error = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR,
    GTK_BUTTONS_OK, "Failed to read configuration.");
   g_signal_connect (error, "destroy", (GCallback) gtk_main_quit, NULL);
   gtk_widget_show (error);
   gtk_main ();
}

int main (void) {
   gtk_init (NULL, NULL);
   if (! config_init (config_path, NULL, sizeof config_path)) {
      error_main ();
      return EXIT_FAILURE;
   }
   read_config (config_path, & on, bands);
   create_window ();
   gtk_main ();
   return 0;
}
