// AEq -- Equalizer plugin for ALSA
// Copyright 2010 John Lindgren <john.lindgren@aol.com>
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions, and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions, and the following disclaimer in the documentation
//    provided with the distribution.
//
// This software is provided "as is" and without any warranty, express or
// implied. In no event shall the authors be liable for any damages arising from
// the use of this software.

#include "common.h"

#include <stdlib.h>
#include <sys/stat.h>

const float freqs[BANDS] = {31.25, 62.5, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
const char * const labels[BANDS] = {"31.25 Hz", "62.5 Hz", "125 Hz", "250 Hz",
 "500 Hz", "1 kHz", "2 kHz", "4 kHz", "8 kHz", "16 kHz"};

void read_config (const char * path, int * on, float bands[BANDS]) {
   * on = 0;
   memset (bands, 0, BANDS * sizeof bands[0]);
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

void write_config (const char * path, int on, const float bands[BANDS]) {
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
   if (! mkdir (path, 0777) || errno == EEXIST)
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

int config_init (char * path, char * pre, int size) {
   snprintf (path, size, "%s/.config", getenv ("HOME"));
   if (! folder_init (path))
      return 0;
   snprintf (path + strlen (path), size - strlen (path), "/aeq");
   if (! folder_init (path))
      return 0;
   if (pre) {
      snprintf (pre, size, "%s/presets", path);
      if (! folder_init (pre))
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
