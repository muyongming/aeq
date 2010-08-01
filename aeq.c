// AEq -- Equalizer plugin for ALSA
// Copyright 2010 John Lindgren <john.lindgren@tds.net>
//
// Implementation of a 10 band time domain graphic equalizer using IIR filters
// Copyright 2001 Anders Johansson <ajh@atri.curtin.edu.au>
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
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>

#include <alsa/asoundlib.h>
#include <alsa/pcm_external.h>

#define ERROR(...) fprintf (stderr, "AEq: " __VA_ARGS__)

#define BANDS 10
#define MAX_CHANS 10

// Q value for band-pass filters 1.2247 = (3/2)^(1/2)
// Gives 4 dB suppression at Fc*2 and Fc/2
#define Q 1.224745

typedef struct {
   int initted;
   char path[PATH_MAX];
   int notify;
   int on;
   int chans, rate;
   float a[BANDS][2]; // A weights
   float b[BANDS][2]; // B weights
   float wq[MAX_CHANS][BANDS][2]; // Circular buffer for W data
   float gv[MAX_CHANS][BANDS]; // Gain factor for each channel and band
   int K; // Number of used eq bands
} AEQState;

static const float freqs[BANDS] = {31.25, 62.5, 125, 250, 500, 1000, 2000, 4000,
 8000, 16000};
static const char * const labels[BANDS] = {"31.25 Hz", "62.5 Hz", "125 Hz",
 "250 Hz", "500 Hz", "1 kHz", "2 kHz", "4 kHz", "8 kHz", "16 kHz"};

// 2nd order band-pass filter design
static void bp2 (float * a, float * b, float fc, float q) {
   float th = 2 * M_PI * fc;
   float C = (1 - tanf (th * q / 2)) / (1 + tanf (th * q / 2));
   a[0] = (1 + C) * cosf (th);
   a[1] = -C;
   b[0] = (1 - C) / 2;
   b[1] = -1.005;
}

static void set_format (AEQState * s, int chans, int rate) {
   s->chans = chans;
   s->rate = rate;
   // Calculate number of active filters
   s->K = BANDS;
   while (freqs[s->K - 1] > (float) rate / 2.2)
      s->K --;
   // Generate filter taps
   for (int k = 0; k < s->K; k ++)
      bp2 (s->a[k], s->b[k], freqs[k] / (float) rate, Q);
   memset (& s->wq, 0, sizeof s->wq);
}

static void set_bands (AEQState * s, int on, const float bands[BANDS]) {
   s->on = on;
   for (int i = 0; i < MAX_CHANS; i ++) {
      for (int k = 0; k < BANDS; k ++)
         s->gv[i][k] = powf (10, bands[k] / 20) - 1;
   }
}

static void equalize (AEQState * s, int chan, const float * in, int in_step,
 float * out, int out_step, int samps) {
   // Gain factor
   const float * g = s->gv[chan];
   const float * end = in + in_step * samps;
   while (in < end) {
      float f = * in;
      in += in_step;
      for (int k = 0; k < s->K; k ++) {
         // Pointer to circular buffer wq
         float * wq = s->wq[chan][k];
         // Calculate output from AR part of current filter
         float w = f * s->b[k][0] + wq[0] * s->a[k][0] + wq[1] * s->a[k][1];
         // Calculate output from MA part of current filter
         f += (w + wq[1] * s->b[k][1]) * g[k];
         // Update circular buffer
         wq[1] = wq[0];
         wq[0] = w;
      }
      * out = f;
      out += out_step;
   }
}

static void read_config (const char * path, int * on, float bands[BANDS]) {
   * on = 0;
   memset (bands, 0, sizeof bands);
   FILE * file = fopen (path, "r");
   if (! file) {
      ERROR ("Failed to open %s for reading: %s\n", path, strerror (errno));
      return;
   }
   char line[256];
   if (! fgets (line, sizeof line, file) || ! sscanf (line, "%d", on))
      goto ERR;
   for (int k = 0; k < BANDS; k ++) {
      if (! fgets (line, sizeof line, file) || ! sscanf (line, "%f", bands + k))
         goto ERR;
   }
   fclose (file);
   return;
ERR:
   ERROR ("Syntax error in %s.\n", path);
   fclose (file);
}

static void write_config (const char * path, int on, const float bands[BANDS]) {
   FILE * file = fopen (path, "w");
   if (! file) {
      ERROR ("Failed to open %s for writing: %s\n", path, strerror (errno));
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
   ERROR ("Failed to create %s: %s.\n", path, strerror (errno));
   return 0;
}

static int file_exists (const char * path) {
   struct stat s;
   if (! lstat (path, & s))
      return 1;
   if (errno == ENOENT)
      return 0;
   ERROR ("Failed to stat %s: %s.\n", path, strerror (errno));
   return 0;
}

static int config_init (char * path, int psize) {
   snprintf (path, psize, "%s/.config", getenv ("HOME"));
   if (! folder_init (path))
      return 0;
   snprintf (path + strlen (path), psize - strlen (path), "/aeq");
   if (! folder_init (path))
      return 0;
   snprintf (path + strlen (path), psize - strlen (path), "/config");
   if (! file_exists (path)) {
      float bands[BANDS];
      memset (bands, 0, sizeof bands);
      write_config (path, 0, bands);
   }
   return 1;
}

static int notify_new (const char * path) {
   int n = inotify_init1 (IN_CLOEXEC | IN_NONBLOCK);
   if (n < 0) {
      ERROR ("Failed to start inotify: %s.\n", strerror (errno));
      return -1;
   }
   if (inotify_add_watch (n, path, IN_CLOSE_WRITE) < 0) {
      ERROR ("Failed to set inotify watch on %s: %s.\n", path, strerror (errno));
      close (n);
      return -1;
   }
   return n;
}

static int notify_changed (int n) {
   struct inotify_event e;
   int readed = read (n, & e, sizeof e);
   if (readed < 0) {
      if (errno != EAGAIN)
         ERROR ("Failed to read from inotify watch: %s.\n", strerror (errno));
      return 0;
   }
   return (readed > 0);
}

static snd_pcm_sframes_t aeq_transfer (snd_pcm_extplug_t * plug,
 const snd_pcm_channel_area_t * out_areas, snd_pcm_uframes_t out_off,
 const snd_pcm_channel_area_t * in_areas, snd_pcm_uframes_t in_off,
 snd_pcm_uframes_t frames) {
   AEQState * s = plug->private_data;
   if (notify_changed (s->notify)) {
      int on;
      float bands[BANDS];
      read_config (s->path, & on, bands);
      set_bands (s, on, bands);
   }
   if (s->on) {
      for (int i = 0; i < s->chans; i ++) {
         const float * in = (float *) in_areas[i].addr + (in_areas[i].first +
          in_areas[i].step * in_off) / (8 * sizeof (float));
         int in_step = in_areas[i].step / (8 * sizeof (float));
         float * out = (float *) out_areas[i].addr + (out_areas[i].first +
          out_areas[i].step * out_off) / (8 * sizeof (float));
         int out_step = out_areas[i].step / (8 * sizeof (float));
         equalize (s, i, in, in_step, out, out_step, frames);
      }
   } else
      snd_pcm_areas_copy (out_areas, out_off, in_areas, in_off, s->chans,
       frames, SND_PCM_FORMAT_FLOAT);
   return frames;
}

static int aeq_init (snd_pcm_extplug_t * plug) {
   AEQState * s = plug->private_data;
   if (s->initted) /* Fix me: Why does this happen? */
      return 0;
   if (plug->channels > MAX_CHANS)
      return -EINVAL;
   set_format (s, plug->channels, plug->rate);
   if (! config_init (s->path, sizeof s->path))
      return -EIO;
   if ((s->notify = notify_new (s->path)) < 0)
      return -EIO;
   int on;
   float bands[BANDS];
   read_config (s->path, & on, bands);
   set_bands (s, on, bands);
   s->initted = 1;
   return 0;
}

static int aeq_close (snd_pcm_extplug_t * plug) {
   AEQState * s = plug->private_data;
   close (s->notify);
   free (s);
   free (plug);
   return 0;
}

static const snd_pcm_extplug_callback_t aeq_callback = {
 .transfer = aeq_transfer,
 .init = aeq_init,
 .close = aeq_close};

static snd_config_t * get_slave (snd_config_t * self) {
   snd_config_iterator_t i, next;
   snd_config_for_each (i, next, self) {
      snd_config_t * n = snd_config_iterator_entry (i);
      const char * id;
      if (snd_config_get_id (n, & id) < 0)
         continue;
      if (! strcmp (id, "slave"))
         return n;
   }
   ERROR ("No slave PCM.\n");
   return NULL;
}

static snd_pcm_extplug_t * aeq_new (void) {
   snd_pcm_extplug_t * plug = calloc (1, sizeof (snd_pcm_extplug_t));
   if (! plug)
      return NULL;
   AEQState * s = calloc (1, sizeof (AEQState));
   if (! s) {
      free (plug);
      return NULL;
   }
   plug->version = SND_PCM_EXTPLUG_VERSION;
   plug->name = "AEq Equalizer";
   plug->callback = & aeq_callback;
   plug->private_data = s;
   return plug;
}

SND_PCM_PLUGIN_DEFINE_FUNC (aeq) {
   snd_config_t * slave = get_slave (conf);
   if (! slave)
      return -EINVAL;
   snd_pcm_extplug_t * plug = aeq_new ();
   if (! plug)
      return -ENOMEM;
   int err = snd_pcm_extplug_create (plug, name, root, slave, stream, mode);
   if (err < 0) {
      aeq_close (plug);
      return err;
   }
   snd_pcm_extplug_set_param (plug, SND_PCM_EXTPLUG_HW_FORMAT,
    SND_PCM_FORMAT_FLOAT);
   snd_pcm_extplug_set_slave_param (plug, SND_PCM_EXTPLUG_HW_FORMAT,
    SND_PCM_FORMAT_FLOAT);
   * pcmp = plug->pcm;
   return 0;
}

SND_PCM_PLUGIN_SYMBOL (aeq);
