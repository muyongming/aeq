// AEq -- Equalizer plugin for ALSA
// Copyright 2010 John Lindgren <john.lindgren@aol.com>
//
// Implementation of a 10 band time domain graphic equalizer using IIR filters
// Copyright 2001 Anders Johansson <ajh@atri.curtin.edu.au>
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

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/inotify.h>

#include <alsa/asoundlib.h>
#include <alsa/pcm_external.h>

#include "common.h"

#define MAX_CHANS 10
// Q value for band-pass filters 1.2247 = (3/2)^(1/2)
// Gives 4 dB suppression at Fc*2 and Fc/2
#define Q 1.224745f

typedef struct {
   int initted;
   int notify;
   int on;
   int chans, rate;
   float a[BANDS][2]; // A weights
   float b[BANDS][2]; // B weights
   float wq[MAX_CHANS][BANDS][2]; // Circular buffer for W data
   float gv[MAX_CHANS][BANDS]; // Gain factor for each channel and band
   int K; // Number of used EQ bands
   float atten[MAX_CHANS];
} AEQState;

// 2nd order band-pass filter design
static void bp2 (float * a, float * b, float fc) {
   float th = 2 * (float)M_PI * fc;
   float C = (1 - tanf (th * Q / 2)) / (1 + tanf (th * Q / 2));
   a[0] = (1 + C) * cosf (th);
   a[1] = -C;
   b[0] = (1 - C) / 2;
   b[1] = -1.005f;
}

static void set_format (AEQState * s, int chans, int rate) {
   s->chans = chans;
   s->rate = rate;
   // Calculate number of active filters
   s->K = BANDS;
   while (s->K > 0 && freqs[s->K - 1] > (float) rate / (2.005f * Q))
      s->K --;
   // Generate filter taps
   for (int k = 0; k < s->K; k ++)
      bp2 (s->a[k], s->b[k], freqs[k] / (float) rate);
   memset (& s->wq, 0, sizeof s->wq);
}

static void set_bands (AEQState * s, int on, const float bands[BANDS + 1]) {
   s->on = on;
   for (int i = 0; i < MAX_CHANS; i ++) {
      for (int k = 0; k < BANDS; k ++)
         s->gv[i][k] = powf (10, (bands[PREAMP_BAND] + bands[k]) / 20) - 1;
      s->atten[i] = 1;
   }
}

static void equalize (AEQState * s, int chan, const int16_t * in, int in_step,
 int16_t * out, int out_step, int samps) {
   // Gain factor
   const float * g = s->gv[chan];
   const int16_t * end = in + in_step * samps;
   float atten = s->atten[chan];
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
      // Attenuate to prevent clipping
      f *= atten;
      if (f > INT16_MAX) {
         atten *= INT16_MAX / f;
         f = INT16_MAX;
      } else if (f < INT16_MIN) {
         atten *= INT16_MIN / f;
         f = INT16_MIN;
      }
      * out = f;
      out += out_step;
      // Gradually restore volume (10 dB over 10 seconds at 44.1 kHz)
      atten = fminf (1, atten * 1.0000068f);
   }
   s->atten[chan] = atten;
}

static int notify_new (const char * path) {
   int n = inotify_init1 (IN_CLOEXEC | IN_NONBLOCK);
   if (n < 0) {
      FAIL ("start", "inotify");
      return -1;
   }
   if (inotify_add_watch (n, path, IN_CLOSE_WRITE) < 0) {
      FAIL ("set inotify watch on", path);
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
         FAIL ("read from", "inotify watch");
      return 0;
   }
   return (readed > 0);
}

static snd_pcm_sframes_t aeq_transfer (snd_pcm_extplug_t * p,
 const snd_pcm_channel_area_t * out_areas, snd_pcm_uframes_t out_off,
 const snd_pcm_channel_area_t * in_areas, snd_pcm_uframes_t in_off,
 snd_pcm_uframes_t frames) {
   AEQState * s = p->private_data;
   if (! s->initted) {
      ERROR ("Not initialized.\n");
      return 0;
   }
   if (notify_changed (s->notify)) {
      int on;
      float bands[BANDS + 1];
      read_config (CONFIG_PATH, & on, bands);
      set_bands (s, on, bands);
   }
   if (s->on) {
      for (int i = 0; i < s->chans; i ++) {
         const int16_t * in = (int16_t *) in_areas[i].addr +
          (in_areas[i].first + in_areas[i].step * in_off) / 16;
         int in_step = in_areas[i].step / 16;
         int16_t * out = (int16_t *) out_areas[i].addr +
          (out_areas[i].first + out_areas[i].step * out_off) / 16;
         int out_step = out_areas[i].step / 16;
         equalize (s, i, in, in_step, out, out_step, frames);
      }
   } else {
      snd_pcm_areas_copy (out_areas, out_off, in_areas, in_off, s->chans,
       frames, SND_PCM_FORMAT_S16);
   }
   return frames;
}

static void aeq_free (snd_pcm_extplug_t * p) {
   AEQState * s = p->private_data;
   memset (s, 0, sizeof (AEQState));
   free (s);
   memset (p, 0, sizeof (snd_pcm_extplug_t));
   free (p);
}

static int aeq_init (snd_pcm_extplug_t * p) {
   AEQState * s = p->private_data;
   if (s->initted)
      return 0;
   if (p->channels > MAX_CHANS) {
      ERROR ("Too many channels.\n");
      return -EINVAL;
   }
   set_format (s, p->channels, p->rate);
   if ((s->notify = notify_new (CONFIG_PATH)) < 0) {
      memset (s, 0, sizeof (AEQState));
      return -EIO;
   }
   int on;
   float bands[BANDS + 1];
   read_config (CONFIG_PATH, & on, bands);
   set_bands (s, on, bands);
   s->initted = 1;
   return 0;
}

static int aeq_close (snd_pcm_extplug_t * p) {
   AEQState * s = p->private_data;
   if (s->initted)
      close (s->notify);
   aeq_free (p);
   return 0;
}

static const snd_pcm_extplug_callback_t aeq_callback = {
   .transfer = aeq_transfer,
   .init = aeq_init,
   .close = aeq_close
};

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
   snd_pcm_extplug_t * p = calloc (1, sizeof (snd_pcm_extplug_t));
   if (! p)
      return NULL;
   AEQState * s = calloc (1, sizeof (AEQState));
   if (! s) {
      free (p);
      return NULL;
   }
   p->version = SND_PCM_EXTPLUG_VERSION;
   p->name = "AEq Equalizer";
   p->callback = & aeq_callback;
   p->private_data = s;
   return p;
}

SND_PCM_PLUGIN_DEFINE_FUNC (aeq) {
   snd_config_t * slave = get_slave (conf);
   if (! slave)
      return -EINVAL;
   snd_pcm_extplug_t * p = aeq_new ();
   if (! p)
      return -ENOMEM;
   int err = snd_pcm_extplug_create (p, name, root, slave, stream, mode);
   if (err < 0) {
      aeq_free (p);
      return err;
   }
   snd_pcm_extplug_set_param (p, SND_PCM_EXTPLUG_HW_FORMAT, SND_PCM_FORMAT_S16);
   snd_pcm_extplug_set_slave_param (p, SND_PCM_EXTPLUG_HW_FORMAT, SND_PCM_FORMAT_S16);
   * pcmp = p->pcm;
   return 0;
}

SND_PCM_PLUGIN_SYMBOL (aeq);
