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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define BANDS 10

#define ERROR(...) fprintf (stderr, "AEq: " __VA_ARGS__)
#define FAIL(a, f) ERROR ("Failed to %s %s: %s.\n", a, f, strerror (errno))

static const float freqs[BANDS] = {31.25, 62.5, 125, 250, 500, 1000, 2000, 4000,
 8000, 16000};
static const char * const labels[BANDS] = {"31.25 Hz", "62.5 Hz", "125 Hz",
 "250 Hz", "500 Hz", "1 kHz", "2 kHz", "4 kHz", "8 kHz", "16 kHz"};

static void read_config (const char * path, int * on, float bands[BANDS]) {
   * on = 0;
   memset (bands, 0, sizeof bands);
   FILE * file = fopen (path, "r");
   if (! file) {
      FAIL ("read from", path);
      return;
   }
   char line[256];
   if (! fgets (line, sizeof line, file) || ! sscanf (line, "%d", on)) {
ERR:
      ERROR ("Syntax error in %s.\n", path);
      fclose (file);
      return;
   }
   for (int k = 0; k < BANDS; k ++) {
      if (! fgets (line, sizeof line, file) || ! sscanf (line, "%f", bands + k))
         goto ERR;
   }
   fclose (file);
}

static void write_config (const char * path, int on, const float bands[BANDS]) {
   FILE * file = fopen (path, "w");
   if (! file) {
      FAIL ("write to", path);
      return;
   }
   fprintf (file, "%d # On/off\n", on);
   for (int k = 0; k < BANDS; k ++)
      fprintf (file, "%.1f # %s\n", bands[k], labels[k]);
   fclose (file);
}

static int folder_init (const char * path) {
   if (! mkdir (path, S_IRUSR | S_IXUSR | S_IWUSR | S_IRGRP | S_IXGRP) || errno
    == EEXIST)
      return 1;
   FAIL ("create", path);
   return 0;
}

static int file_exists (const char * path) {
   struct stat s;
   if (! lstat (path, & s))
      return 1;
   if (errno == ENOENT)
      return 0;
   FAIL ("stat", path);
   return 0;
}

static int config_init (char * path, char * pre, int size) {
   snprintf (path, size, "%s/.config", getenv ("HOME"));
   if (! folder_init (path))
      return 0;
   snprintf (path + strlen (path), size - strlen (path), "/aeq");
   if (! folder_init (path))
      return 0;
   if (pre) {
      snprintf (pre, size, "%s/presets", path);
      if (! folder_init (path))
         return 0;
   }
   snprintf (path + strlen (path), size - strlen (path), "/config");
   if (! file_exists (path)) {
      float bands[BANDS];
      memset (bands, 0, sizeof bands);
      write_config (path, 0, bands);
   }
   return 1;
}
