// AEq -- Equalizer plugin for ALSA
// Copyright 2010-2016 John Lindgren <john.lindgren@aol.com>
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

#ifndef AEQ_COMMON_H
#define AEQ_COMMON_H

#include <errno.h>
#include <stdio.h>
#include <string.h>

#define BANDS 10
#define PREAMP_BAND 10
#define CONFIG_PATH "/etc/aeq/config"

#define ERROR(...) fprintf (stderr, "AEq: " __VA_ARGS__)
#define FAIL(a, f) ERROR ("Failed to %s %s: %s.\n", a, f, strerror (errno))

extern const float freqs[BANDS];
extern const char * const labels[BANDS + 1]; /* last one is preamp */

void read_config (const char * path, int * on, float bands[BANDS + 1]);
void write_config (const char * path, int on, const float bands[BANDS + 1]);

#endif // AEQ_COMMON_H
