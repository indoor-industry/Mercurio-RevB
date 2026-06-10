#pragma once

#include "driver/i2s_std.h"
#include "shared_types.h"

/* Read one chunk of n mono 32-bit samples from an open RX channel and
 * return its RMS level (0..1); optionally also report the absolute peak.
 * Shared by the microphone self-test and the live detail-screen meter. */
double mic_read_chunk(i2s_chan_handle_t rx, int32_t *buf, int n, double *peak_out);

void test_microphone(test_entry_t *t);

/* Active mic test: measure the ambient noise floor, then prompt the user
 * to make noise and compare the peak level against that floor. */
void mic_run_test(test_entry_t *t);

void test_speaker(test_entry_t *t);

/* Play a tone (or, for freq<0, a 440->2000Hz sweep) for `ms` milliseconds
 * through the MAX98357A speaker amp. */
void play_tone(float freq, int ms);
