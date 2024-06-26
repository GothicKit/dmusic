// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

enum {
	DmInt_TICKS_PER_QUARTER_NOTE = 768,
	DmInt_SECONDS_PER_MINUTE = 60,
};

size_t max_usize(size_t a, size_t b) {
	return a >= b ? a : b;
}

int32_t max_s32(int32_t a, int32_t b) {
	return a > b ? a : b;
}

uint8_t min_u8(uint8_t a, uint8_t b) {
	return a < b ? a : b;
}

float lerp(float x, float start, float end) {
	return (1 - x) * start + x * end;
}

int32_t clamp_s32(int32_t val, int32_t min, int32_t max) {
	if (val < min) {
		return min;
	}

	if (val > max) {
		return max;
	}

	return val;
}

int32_t Dm_randRange(int32_t range) {
	uint32_t rnd = Dm_rand() % range;
	return range - (int32_t) (rnd / 2);
}

DmCommandType Dm_embellishmentToCommand(DmEmbellishmentType embellishment) {
	switch (embellishment) {
	case DmEmbellishment_NONE:
	case DmEmbellishment_GROOVE:
		return DmCommand_GROOVE;
	case DmEmbellishment_FILL:
		return DmCommand_FILL;
	case DmEmbellishment_INTRO:
		return DmCommand_INTRO;
	case DmEmbellishment_BREAK:
		return DmCommand_BREAK;
	case DmEmbellishment_END:
		return DmCommand_END;
	case DmEmbellishment_END_AND_INTRO:
		return DmCommand_END_AND_INTRO;
	}
	return DmCommand_GROOVE;
}

bool DmGuid_equals(DmGuid const* a, DmGuid const* b) {
	return memcmp(a->data, b->data, sizeof a->data) == 0;
}

size_t DmGuid_toString(DmGuid const* slf, char* out, size_t len) {
	if (slf == NULL) {
		return 0;
	}

	return snprintf(out,
	                len,
	                "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
	                slf->data[0x0],
	                slf->data[0x1],
	                slf->data[0x2],
	                slf->data[0x3],
	                slf->data[0x4],
	                slf->data[0x5],
	                slf->data[0x6],
	                slf->data[0x7],
	                slf->data[0x8],
	                slf->data[0x9],
	                slf->data[0xA],
	                slf->data[0xB],
	                slf->data[0xC],
	                slf->data[0xD],
	                slf->data[0xE],
	                slf->data[0xF]);
}

uint32_t Dm_getBeatLength(DmTimeSignature sig) {
	// Special case: If the beat 0, it indicates a 256th note instead
	if (sig.beat == 0) {
		return (DmInt_TICKS_PER_QUARTER_NOTE * 4) / 256;
	}

	return (DmInt_TICKS_PER_QUARTER_NOTE * 4) / sig.beat;
}

uint32_t Dm_getMeasureLength(DmTimeSignature sig) {
	uint32_t v = sig.beats_per_measure * Dm_getBeatLength(sig);

	if (v < 1) {
		return 1;
	}

	return v;
}

double Dm_getTicksPerSecond(DmTimeSignature time_signature, double beats_per_minute) {
	uint32_t pulses_per_beat = Dm_getBeatLength(time_signature);           // unit: music-time per beat
	double beats_per_second = beats_per_minute / DmInt_SECONDS_PER_MINUTE; // unit: 1 per second
	double pulses_per_second = pulses_per_beat * beats_per_second;         // unit: music-time per second
	return pulses_per_second;
}

double Dm_getTicksPerSample(DmTimeSignature time_signature, double beats_per_minute, uint32_t sample_rate) {
	double pulses_per_second = Dm_getTicksPerSecond(time_signature, beats_per_minute); // unit: music-time per second
	double pulses_per_sample = pulses_per_second / sample_rate;                        // unit: music-time per sample
	return pulses_per_sample;
}

uint32_t Dm_getTimeOffset(uint32_t grid_start, int32_t time_offset, DmTimeSignature sig) {
	uint32_t beat_length = Dm_getBeatLength(sig);

	uint32_t full_beat_length = (grid_start / sig.grids_per_beat) * beat_length;
	uint32_t partial_beat_length = (grid_start % sig.grids_per_beat) * (beat_length / sig.grids_per_beat);

	return (uint32_t) time_offset + full_beat_length + partial_beat_length;
}

uint32_t Dm_getSampleCountForDuration(uint32_t duration,
                                      DmTimeSignature time_signature,
                                      double tempo,
                                      uint32_t sample_rate,
                                      uint8_t channels) {
	double pulses_per_sample = Dm_getTicksPerSample(time_signature, tempo, sample_rate) / channels;
	return (uint32_t) (duration / pulses_per_sample);
}

uint32_t Dm_getDurationForSampleCount(uint32_t samples,
                                      DmTimeSignature time_signature,
                                      double tempo,
                                      uint32_t sample_rate,
                                      uint8_t channels) {
	double pulses_per_sample = Dm_getTicksPerSample(time_signature, tempo, sample_rate) / channels;
	return (uint32_t) round(pulses_per_sample * samples);
}
