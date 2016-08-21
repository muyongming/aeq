// AEq -- Equalizer plugin for ALSA
// Copyright 2010-2011 John Lindgren <john.lindgren@tds.net>
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

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#define MAX_GAIN 10
#define STEP 1

static char config_path[PATH_MAX];
static char preset_path[sizeof config_path];
static int on;
static float bands[BANDS];
static GtkWidget * toggle;
static GtkWidget * sliders[BANDS];
static int toggle_sig;
static int slider_sigs[BANDS];

static void update (void) {
   g_signal_handler_block (toggle, toggle_sig);
   gtk_toggle_button_set_active ((GtkToggleButton *) toggle, on);
   g_signal_handler_unblock (toggle, toggle_sig);
   for (int i = 0; i < BANDS; i ++) {
      g_signal_handler_block (sliders[i], slider_sigs[i]);
      gtk_range_set_value ((GtkRange *) sliders[i], -bands[i]);
      g_signal_handler_unblock (sliders[i], slider_sigs[i]);
   }
}

static void changed (void) {
   on = gtk_toggle_button_get_active ((GtkToggleButton *) toggle);
   for (int i = 0; i < BANDS; i ++)
      bands[i] = -(float) gtk_range_get_value ((GtkRange *) sliders[i]);
   write_config (config_path, on, bands);
}

static GtkWidget * create_toggle (void) {
   toggle = gtk_check_button_new_with_mnemonic ("_Enable");
   toggle_sig = g_signal_connect (toggle, "toggled", (GCallback) changed, NULL);
   return toggle;
}

static char * format_value (GtkScale * s __attribute ((unused)), double val) {
   return g_strdup_printf ("%d", -(int) val);
}

static GtkWidget * create_slider (int i) {
   GtkWidget * vbox = gtk_vbox_new (0, 6);
   GtkWidget * label = gtk_label_new (labels[i]);
   gtk_label_set_angle ((GtkLabel *) label, 90);
   gtk_box_pack_start ((GtkBox *) vbox, label, 1, 0, 0);
   sliders[i] = gtk_vscale_new_with_range (-MAX_GAIN, MAX_GAIN, STEP);
   gtk_scale_set_draw_value ((GtkScale *) sliders[i], 1);
   gtk_scale_set_value_pos ((GtkScale *) sliders[i], GTK_POS_BOTTOM);
   gtk_widget_set_size_request (sliders[i], -1, 144);
   g_signal_connect (sliders[i], "format-value", (GCallback) format_value, NULL);
   slider_sigs[i] = g_signal_connect (sliders[i], "value-changed", (GCallback)
    changed, NULL);
   gtk_box_pack_start ((GtkBox *) vbox, sliders[i], 0, 0, 0);
   return vbox;
}

static void open_preset (GtkDialog * win, gint resp) {
   if (resp != GTK_RESPONSE_OK)
      return;
   char * name = gtk_file_chooser_get_filename ((GtkFileChooser *) win);
   if (! name)
      return;
   read_config (name, & on, bands);
   g_free (name);
   gtk_widget_destroy ((GtkWidget *) win);
   update ();
   changed ();
}

static void open_window (void) {
   GtkWidget * win = gtk_file_chooser_dialog_new ("Open Preset", NULL,
    GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_OPEN, GTK_RESPONSE_OK, NULL);
   gtk_file_chooser_set_current_folder ((GtkFileChooser *) win, preset_path);
   g_signal_connect (win, "response", (GCallback) open_preset, NULL);
   gtk_widget_show (win);
}

static void save_preset (GtkDialog * win, gint resp) {
   if (resp != GTK_RESPONSE_OK)
      return;
   char * name = gtk_file_chooser_get_filename ((GtkFileChooser *) win);
   if (! name)
      return;
   write_config (name, on, bands);
   g_free (name);
   gtk_widget_destroy ((GtkWidget *) win);
}

static void save_window (void) {
   GtkWidget * win = gtk_file_chooser_dialog_new ("Save Preset", NULL,
    GTK_FILE_CHOOSER_ACTION_SAVE, GTK_STOCK_SAVE, GTK_RESPONSE_OK, NULL);
   gtk_file_chooser_set_current_folder ((GtkFileChooser *) win, preset_path);
   g_signal_connect (win, "response", (GCallback) save_preset, NULL);
   gtk_widget_show (win);
}

static GtkWidget * create_buttons (void) {
   GtkWidget * hbox = gtk_hbox_new (0, 6);
   gtk_box_pack_start ((GtkBox *) hbox, gtk_label_new ("Presets:"), 0, 0, 0);
   GtkWidget * save = gtk_button_new_from_stock (GTK_STOCK_SAVE);
   g_signal_connect (save, "clicked", (GCallback) save_window, NULL);
   gtk_box_pack_end ((GtkBox *) hbox, save, 0, 0, 0);
   GtkWidget * open = gtk_button_new_from_stock (GTK_STOCK_OPEN);
   g_signal_connect (open, "clicked", (GCallback) open_window, NULL);
   gtk_box_pack_end ((GtkBox *) hbox, open, 0, 0, 0);
   return hbox;
}

static int keypress (GtkWidget * widget, GdkEventKey * event) {
   if (event->keyval == GDK_Escape) {
      gtk_widget_destroy (widget);
      return 1;
   }
   return 0;
}

static void create_window (void) {
   GtkWidget * win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
   gtk_window_set_title ((GtkWindow *) win, "AEq Equalizer");
   gtk_window_set_resizable ((GtkWindow *) win, 0);
   gtk_container_set_border_width ((GtkContainer *) win, 6);
   g_signal_connect (win, "key-press-event", (GCallback) keypress, NULL);
   g_signal_connect (win, "destroy", (GCallback) gtk_main_quit, NULL);
   GtkWidget * vbox = gtk_vbox_new (0, 6);
   gtk_container_add ((GtkContainer *) win, vbox);
   gtk_box_pack_start ((GtkBox *) vbox, create_toggle (), 0, 0, 0);
   GtkWidget * hbox = gtk_hbox_new (0, 6);
   gtk_box_pack_start ((GtkBox *) vbox, hbox, 0, 0, 0);
   for (int i = 0; i < BANDS; i ++)
      gtk_box_pack_start ((GtkBox *) hbox, create_slider (i), 0, 0, 0);
   gtk_box_pack_start ((GtkBox *) vbox, create_buttons (), 0, 0, 0);
   update ();
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
   if (! config_init (config_path, preset_path, sizeof config_path)) {
      error_main ();
      return EXIT_FAILURE;
   }
   read_config (config_path, & on, bands);
   create_window ();
   gtk_main ();
   return 0;
}
