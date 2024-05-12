// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"
#include <math.h>
#include <stdlib.h>

enum {
	DmInt_DEFAULT_TEMPO = 100,
	DmInt_DEFAULT_SAMPLE_RATE = 44100,
	DmInt_DEFAULT_SCALE_PATTERN = 0xab5ab5,
};

DmResult DmPerformance_create(DmPerformance** slf, uint32_t rate) {
	if (slf == NULL) {
		return DmResult_INVALID_ARGUMENT;
	}

	DmPerformance* new = *slf = Dm_alloc(sizeof *new);
	if (new == NULL) {
		return DmResult_MEMORY_EXHAUSTED;
	}

	new->sample_rate = rate == 0 ? DmInt_DEFAULT_SAMPLE_RATE : rate;
	new->reference_count = 1;
	new->tempo = DmInt_DEFAULT_TEMPO;
	new->groove = 1;
	new->time_signature.beats_per_measure = 4;
	new->time_signature.beat = 4;
	new->time_signature.grids_per_beat = 2;

	DmSynth_init(&new->synth, new->sample_rate);

	if (mtx_init(&new->lock, mtx_plain) != thrd_success) {
		Dm_free(new);
		return DmResult_MUTEX_ERROR;
	}

	DmResult rv = DmMessageQueue_init(&new->control_queue);
	if (rv != DmResult_SUCCESS) {
		mtx_destroy(&new->lock);
		Dm_free(new);
		return rv;
	}

	rv = DmMessageQueue_init(&new->music_queue);
	if (rv != DmResult_SUCCESS) {
		mtx_destroy(&new->lock);
		DmMessageQueue_free(&new->control_queue);
		Dm_free(new);
		return rv;
	}

	return DmResult_SUCCESS;
}

DmPerformance* DmPerformance_retain(DmPerformance* slf) {
	if (slf == NULL) {
		return NULL;
	}

	(void) atomic_fetch_add(&slf->reference_count, 1);
	return slf;
}

void DmPerformance_release(DmPerformance* slf) {
	if (slf == NULL) {
		return;
	}

	size_t refs = atomic_fetch_sub(&slf->reference_count, 1) - 1;
	if (refs > 0) {
		return;
	}

	mtx_destroy(&slf->lock);
	DmMessageQueue_free(&slf->control_queue);
	DmMessageQueue_free(&slf->music_queue);
	DmSegment_release(slf->segment);
	DmStyle_release(slf->style);
	DmBand_release(slf->band);
	DmSynth_free(&slf->synth);
	Dm_free(slf);
}

static uint32_t Dm_getBoundaryOffset(DmPerformance* slf, DmTiming timing) {
	uint32_t timing_unit_length = 0;

	switch (timing) {
	case DmTiming_INSTANT:
		return slf->time;
	case DmTiming_GRID:
		timing_unit_length = Dm_getBeatLength(slf->time_signature) / slf->time_signature.grids_per_beat;
		break;
	case DmTiming_BEAT:
		timing_unit_length = Dm_getBeatLength(slf->time_signature);
		break;
	case DmTiming_MEASURE:
		timing_unit_length = Dm_getMeasureLength(slf->time_signature);
		break;
	}

	uint32_t delay = 0;
	uint32_t offset = slf->time - slf->segment_start;

	if (offset != 0) {
		delay = timing_unit_length - (offset % timing_unit_length);
	}

	return slf->time + delay;
}

DmResult DmPerformance_playSegment(DmPerformance* slf, DmSegment* sgt, DmTiming timing) {
	if (slf == NULL) {
		return DmResult_INVALID_ARGUMENT;
	}

	if (sgt != NULL && !sgt->downloaded) {
		Dm_report(DmLogLevel_ERROR, "DmPerformance: You must download the segment before playing it");
		return DmResult_INVALID_ARGUMENT;
	}

	uint32_t offset = Dm_getBoundaryOffset(slf, timing);

	DmMessage msg;
	msg.time = 0;
	msg.type = DmMessage_SEGMENT;
	msg.segment.segment = DmSegment_retain(sgt);
	msg.segment.loop = 0;

	if (mtx_lock(&slf->lock) != thrd_success) {
		return DmResult_MUTEX_ERROR;
	}

	DmMessageQueue_add(&slf->control_queue, &msg, offset, DmQueueConflict_REPLACE);

	(void) mtx_unlock(&slf->lock);
	return DmResult_SUCCESS;
}

static uint8_t bit_count(uint32_t v) {
	uint8_t count = 0;

	for (uint8_t i = 0u; i < 32; ++i) {
		count += v & 1;
		v >>= 1;
	}

	return count;
}

static uint32_t fixup_scale(uint32_t scale, uint8_t scale_root) {
	uint32_t const FALLBACK_SCALES[12] = {
	    0xab5ab5,
	    0x6ad6ad,
	    0x5ab5ab,
	    0xad5ad5,
	    0x6b56b5,
	    0x5ad5ad,
	    0x56b56b,
	    0xd5ad5a,
	    0xb56b56,
	    0xd6ad6a,
	    0xb5ab5a,
	    0xad6ad6,
	};

	// Force the scale to be exactly two octaves wide by zero-ing out the upper octave and
	// copying the lower octave into the upper one
	scale = (scale & 0x0FFF) | (scale << 12);

	// Add the root to the scale
	scale = scale >> (12 - (scale_root & 12));

	// Clean up the scale again.
	scale = (scale & 0x0FFF) | (scale << 12);

	// If there are less than 5 bits set in the scale, figure out a fallback to use instead
	if (bit_count(scale & 0xFFF) <= 4) {
		uint32_t best_scale = FALLBACK_SCALES[0];
		uint32_t best_score = 0;

		for (size_t i = 0; i < 12; ++i) {
			// Determine the score by checking the number of bits which are set in both
			uint32_t score = bit_count((FALLBACK_SCALES[i] & scale) & 0xFFF);

			if (score > best_score) {
				best_scale = FALLBACK_SCALES[i];
				best_score = score;
			}
		}

		scale = best_scale;
	}

	// Copy the second octave of the scale to the third, but only if the third octave is empty
	if (!(scale & 0xFF000000)) {
		scale |= (scale & 0xFFF000) << 12;
	}

	return scale;
}

static int DmPerformance_musicValueToMidi(struct DmSubChord chord, DmPlayModeFlags mode, uint16_t value) {
	uint32_t offset = 0;

	// Make sure the octave is not negative. If it is, transpose it up, and save the note offset.
	// TODO: Not sure what this actually does
	while (value >= 0xE000) {
		value += 0x1000;
		offset -= 12;
	}

	// Make sure that we can add 7 to the scale offset without overflowing. If we cannot, trim off the excess
	// bytes and move the note offset one octave lower
	uint16_t music_tmp = (value & 0x00F0) + 0x0070;
	if (music_tmp & 0x0F00) {
		value = (value & 0xFF0F) | (music_tmp & 0x00F0);
		offset -= 12;
	}

	// Determine the root of the note
	uint16_t root = 0;

	if (mode & DmPlayMode_CHORD_ROOT) {
		root = chord.chord_root;
	} else if (mode & DmPlayMode_KEY_ROOT) {
		Dm_report(DmLogLevel_DEBUG, "DmPerformance: DmPlayMode_KEY_ROOT requested but we don't support it");
		return -1;
	}

	// Now, to the meat of the routine: Determine the actual note position based on the chord and scale patterns!
	if (!(mode & (DmPlayMode_CHORD_INTERVALS | DmPlayMode_SCALE_INTERVALS))) {
		Dm_report(DmLogLevel_DEBUG,
		          "DmPerformance: Neither DmPlayMode_CHORD_INTERVALS, nor DmPlayMode_SCALE_INTERVALS requested");
		return -1;
	}

	// Make sure we actually have a scale to play from and fix it up (?)
	// TODO: Why do we need to fixup the scale?
	uint32_t scale_pattern = chord.scale_pattern ? chord.scale_pattern : DmInt_DEFAULT_SCALE_PATTERN;
	scale_pattern = fixup_scale(scale_pattern, chord.scale_root);

	uint32_t chord_pattern = chord.chord_pattern;
	if (chord_pattern == 0) {
		chord_pattern = 1;
	}

	uint16_t chord_position = (value & 0x0f00) >> 8;
	uint16_t scale_position = (value & 0x0070) >> 4; // Make sure scale position < 8

	int16_t note_accidentals = value & 0x000f;
	if (note_accidentals > 8) {
		note_accidentals -= 16;
	}

	int note_value = 0;
	int note_offset = 0;
	uint32_t note_pattern = 0;
	uint16_t note_position = 0;

	uint16_t root_octave = root % 12;
	uint16_t chord_bits = bit_count(chord_pattern);

	if ((mode & DmPlayMode_CHORD_INTERVALS) && scale_position == 0 && (chord_position < chord_bits)) {
		note_offset = root + note_accidentals;
		note_pattern = chord_pattern;
		note_position = chord_position;
	} else if ((mode & DmPlayMode_CHORD_INTERVALS) && (chord_position < chord_bits)) {
		note_pattern = chord_pattern;
		note_position = chord_position;

		// Skip to the first note in the chord
		if (note_pattern != 0) {
			while ((note_pattern & 1) == 0) {
				note_pattern >>= 1;
				note_value += 1;
			}
		}

		if (note_position > 0) {
			do {
				note_pattern >>= 1;
				note_value += 1;

				if (note_pattern & 1) {
					note_position -= 1;
				}

				if (note_pattern == 0) {
					note_value += note_position;
					break;
				}
			} while (note_position > 0);
		}

		note_value += root_octave;
		note_offset = note_accidentals + root - root_octave;

		note_pattern = scale_pattern >> (note_value % 12);
		note_position = scale_position;
	} else if (mode & DmPlayMode_SCALE_INTERVALS) {
		note_value = root_octave;
		note_offset = note_accidentals + root - root_octave;

		note_pattern = scale_pattern >> root_octave;
		note_position = chord_position * 2 + scale_position;
	} else {
		return -1;
	}

	note_position += 1; // the actual position of the note (1-indexed)
	for (; note_position > 0; note_pattern >>= 1) {
		note_value += 1;

		if (note_pattern & 1) {
			note_position -= 1;
		}

		if (note_pattern == 0) {
			note_value += note_position;
			break;
		}
	}

	note_value -= 1; // The loop counts one too many semitones (?)
	note_value = note_value + note_offset;

	// Take the note down an octave it the root is < 12
	note_value += offset;
	if (mode & DmPlayMode_CHORD_ROOT) {
		note_value = ((short) ((value >> 12) & 0xF) * 12) + note_value - 12;
	} else {
		note_value = ((short) ((value >> 12) & 0xF) * 12) + note_value;
	}

	return note_value;
}

#define DmInt_CURVE_SPACING 5

static DmResult DmPattern_generateNoteMessages(DmPart* part,
                                               struct DmSubChord chord,
                                               uint32_t time,
                                               uint32_t variation,
                                               uint32_t channel,
                                               DmMessageQueue* out) {
	// Now we are ready to create all the note on/note off messages
	// for this pattern
	for (size_t j = 0; j < part->note_count; ++j) {
		DmNote note = part->notes[j];

		// We ignore notes which do not correspond to the selected variation
		if (!(note.variation & variation)) {
			continue;
		}

		DmPlayModeFlags flags = note.play_mode_flags == DmPlayMode_NONE ? part->play_mode_flags : note.play_mode_flags;

		int midi = note.music_value;
		if (flags != DmPlayMode_FIXED) {
			midi = DmPerformance_musicValueToMidi(chord, flags, note.music_value);
		}

		if (midi < 0) {
			// We were unable to convert the music value
			Dm_report(DmLogLevel_WARN, "DmPerformance: Unable to convert music value %d to MIDI", note.music_value);
			continue;
		}

		uint32_t offset = Dm_getTimeOffset(note.grid_start, note.time_offset, part->time_signature);

		if (note.time_range != 0) {
			offset += Dm_randRange(note.time_range);
		}

		uint32_t duration = note.duration;
		if (note.duration_range != 0) {
			offset += Dm_randRange(note.duration_range);
		}

		uint32_t velocity = note.velocity;
		if (note.velocity_range != 0) {
			offset += Dm_randRange(note.velocity_range);
		}

		DmMessage msg;
		msg.type = DmMessage_NOTE;
		msg.time = 0;

		msg.note.on = true;
		msg.note.note = (uint8_t) midi;
		msg.note.velocity = (uint8_t) velocity;
		msg.note.channel = channel;

		DmResult rv = DmMessageQueue_add(out, &msg, time + offset, DmQueueConflict_APPEND);
		if (rv != DmResult_SUCCESS) {
			return rv;
		}

		msg.note.on = false;
		msg.note.note = (uint8_t) midi;

		rv = DmMessageQueue_add(out, &msg, time + offset + duration, DmQueueConflict_APPEND);
		if (rv != DmResult_SUCCESS) {
			return rv;
		}
	}

	return DmResult_SUCCESS;
}

static float DmCurve_lerp(DmCurve* curve, float phase) {
	switch (curve->curve_shape) {
	case DmCurveShape_LINEAR:
		return lerp(phase, curve->start_value, curve->end_value);
	case DmCurveShape_INSTANT:
		return curve->end_value;
	case DmCurveShape_EXP:
		return lerp(powf(phase, 4), curve->start_value, curve->end_value);
	case DmCurveShape_LOG:
		return lerp(sqrtf(phase), curve->start_value, curve->end_value);
	case DmCurveShape_SINE: {
		phase = (sinf((phase - 0.5f) * (float) M_PI) + 1) * 0.5f;
		return lerp(phase, curve->start_value, curve->end_value);
	}
	}

	return curve->start_value;
}

static DmResult
DmPattern_generateControlChangeCurve(DmCurve* curve, uint32_t time, uint32_t channel, DmMessageQueue* out) {
	// Some MIDI control curves have invalid ranges (e.g. -22000 to 127)
	bool start_in_range = curve->start_value >= 0 && curve->start_value <= 127;
	bool end_in_range = curve->end_value >= 0 && curve->end_value <= 127;
	if (!(start_in_range && end_in_range)) {
		Dm_report(DmLogLevel_DEBUG, "DmPerformance: Curve is out-of-range");
		return DmResult_SUCCESS;
	}

	float prev = (float) curve->start_value - 1;
	for (uint32_t k = 0; k < (curve->duration / DmInt_CURVE_SPACING); ++k) {
		uint32_t offset = k * DmInt_CURVE_SPACING;
		float phase = (float) offset / (float) curve->duration;
		float value = DmCurve_lerp(curve, phase);

		DmMessage msg;
		msg.time = 0;

		msg.type = DmMessage_CONTROL;
		msg.control.control = curve->cc_data;
		msg.control.channel = channel;
		msg.control.value = value / 127.f;
		msg.control.reset = curve->flags & DmCurveFlags_RESET;
		msg.control.reset_value = curve->reset_value / 127.f;

		// Optimization: Don't emit a message if the value is the same as the previous one.
		if (msg.control.value == prev) {
			continue;
		}

		DmResult rv = DmMessageQueue_add(out, &msg, time + offset, DmQueueConflict_APPEND);
		if (rv != DmResult_SUCCESS) {
			return rv;
		}

		prev = msg.control.value;
	}

	return DmResult_SUCCESS;
}

static DmResult DmPattern_generatePitchBendCurve(DmCurve* curve, uint32_t time, uint32_t channel, DmMessageQueue* out) {
	int prev = curve->start_value;
	for (uint32_t k = 0; k < (curve->duration / DmInt_CURVE_SPACING); ++k) {
		uint32_t offset = k * DmInt_CURVE_SPACING;
		float phase = (float) offset / (float) curve->duration;
		float value = DmCurve_lerp(curve, phase);

		DmMessage msg;
		msg.time = 0;

		msg.type = DmMessage_PITCH_BEND;
		msg.pitch_bend.value = (int) value;
		msg.pitch_bend.channel = channel;
		msg.pitch_bend.reset = curve->flags & DmCurveFlags_RESET;
		msg.pitch_bend.reset_value = (int) value;

		// Optimization: Don't emit a message if the value is the same as the previous one.
		if (msg.pitch_bend.value == prev) {
			continue;
		}

		DmResult rv = DmMessageQueue_add(out, &msg, time + offset, DmQueueConflict_APPEND);
		if (rv != DmResult_SUCCESS) {
			return rv;
		}

		prev = msg.pitch_bend.value;
	}

	return DmResult_SUCCESS;
}

static DmResult DmPattern_generateCurveMessages(DmPart* part,
                                                uint32_t time,
                                                uint32_t variation,
                                                uint32_t channel,
                                                DmMessageQueue* out) {
	for (size_t j = 0; j < part->curve_count; ++j) {
		DmCurve curve = part->curves[j];

		// We ignore notes which do not correspond to the selected variation
		if (!(curve.variation & variation)) {
			continue;
		}

		uint32_t start = Dm_getTimeOffset(curve.grid_start, curve.time_offset, part->time_signature);

		DmResult rv = DmResult_SUCCESS;
		switch (curve.event_type) {
		case DmCurveType_PITCH_BEND:
			rv = DmPattern_generatePitchBendCurve(&curve, time + start, channel, out);
			break;
		case DmCurveType_CONTROL_CHANGE:
			rv = DmPattern_generateControlChangeCurve(&curve, time + start, channel, out);
			break;
		case DmCurveType_MONO_AFTERTOUCH:
		case DmCurveType_POLY_AFTERTOUCH:
			Dm_report(DmLogLevel_WARN,
			          "DmPerformance: Curve type %d not implemented (midi channel pressure)",
			          curve.event_type);
			break;
		}

		if (rv != DmResult_SUCCESS) {
			return rv;
		}
	}

	return DmResult_SUCCESS;
}

static DmResult DmPattern_generateMessages(DmPattern* slf,
                                           DmStyle* sty,
                                           DmMessage_Chord* chord,
                                           uint32_t time,
                                           uint32_t seq,
                                           DmMessageQueue* out) {
	int64_t variation[UINT8_MAX + 1];
	memset(variation, -1, sizeof variation);

	// 1. Select the variation IDs for each part
	for (size_t i = 0; i < slf->parts.length; ++i) {
		DmPartReference* pref = &slf->parts.data[i];
		DmPart* part = DmStyle_findPart(sty, pref);

		if (part == NULL) {
			Dm_report(DmLogLevel_WARN, "DmPerformance: Part reference could not be resolved!");
			continue;
		}

		// If this part is locked, or we have not yet select a variation for its lock id,
		// select a new variation to play.
		if (pref->variation_lock_id == 0 || variation[pref->variation_lock_id] == -1) {
			switch (pref->random_variation) {
			case DmVariation_SEQUENTIAL:
				variation[pref->variation_lock_id] = seq;
				break;
			case DmVariation_RANDOM:
				variation[pref->variation_lock_id] = Dm_rand();
				break;
			case DmVariation_RANDOM_START:
				// TODO(lmichaelis): Implement this correctly. To do that, we need to store the previous
				//                   variation id for each pattern somewhere and add the seq to it.
				variation[pref->variation_lock_id] = seq;
				break;
			case DmVariation_NO_REPEAT:
				// TODO(lmichaelis): Implement this correctly. To do that, we need to store the previous
				//                   variation id for each pattern somewhere and compare it to the next value
				variation[pref->variation_lock_id] = Dm_rand();
				break;
			}
		}

		uint32_t variation_id = (uint32_t) variation[pref->variation_lock_id];
		variation_id = 1 << (variation_id % DmPart_getValidVariationCount(part));

		// Now we need to select the correct sub-chord to use for the pattern
		// by comparing against the sub-chord level of the pattern. By default,
		// we just use the first one.
		struct DmSubChord level = chord->subchords[0];
		for (size_t j = 0; j < chord->subchord_count; ++j) {
			if (chord->subchords[j].levels & (1 << pref->subchord_level)) {
				level = chord->subchords[j];
				break;
			}
		}

		// Now we can create the actual messages for the pattern.
		DmResult rv = DmPattern_generateNoteMessages(part, level, time, variation_id, pref->logical_part_id, out);
		if (rv != DmResult_SUCCESS) {
			return rv;
		}

		rv = DmPattern_generateCurveMessages(part, time, variation_id, pref->logical_part_id, out);
		if (rv != DmResult_SUCCESS) {
			return rv;
		}
	}

	return DmResult_SUCCESS;
}

static DmResult DmPerformance_playPattern(DmPerformance* slf, DmPattern* pttn) {
	Dm_report(DmLogLevel_INFO,
	          "DmPerformance: Playing pattern '%s' (measure %d, length %d)",
	          pttn->info.unam,
	          slf->time / Dm_getMeasureLength(slf->time_signature) + 1,
	          pttn->length_measures);

	// Stop any already playing pattern.
	DmMessageQueue_clear(&slf->music_queue);
	DmSynth_sendNoteOffEverything(&slf->synth);
	DmSynth_reset(&slf->synth);

	// Generate the new pattern's messages
	DmResult rv =
	    DmPattern_generateMessages(pttn, slf->style, &slf->chord, slf->time, slf->variation, &slf->music_queue);
	if (rv != DmResult_SUCCESS) {
		return rv;
	}

	// Schedule a new pattern to be played after this one if one is not already scheduled
	DmMessage msg;
	msg.type = DmMessage_COMMAND;
	msg.command.command = DmCommand_GROOVE;
	msg.command.groove_level = slf->groove;
	msg.command.groove_range = slf->groove_range;
	msg.command.repeat_mode = DmPatternSelect_RANDOM;
	msg.command.beat = 0;
	msg.command.measure = 0;

	uint32_t pattern_length = Dm_getMeasureLength(slf->time_signature) * pttn->length_measures;

	// Will keep any existing command message and discard the new one.
	rv = DmMessageQueue_add(&slf->music_queue, &msg, slf->time + pattern_length, DmQueueConflict_KEEP);
	if (rv != DmResult_SUCCESS) {
		return rv;
	}

	slf->variation += 1;
	return DmResult_SUCCESS;
}

static void DmPerformance_handleCommandMessage(DmPerformance* slf, DmMessage_Command* msg) {
	if (msg->command == DmCommand_GROOVE) {
		slf->groove = msg->groove_level;
		slf->groove_range = msg->groove_range;

		// Randomize the groove level
		if (msg->groove_range != 0) {
			int32_t new_groove = slf->groove + Dm_randRange(msg->groove_range);
			slf->groove = max_s32(new_groove, 0);
		}
	} else if (msg->command == DmCommand_END_AND_INTRO) {
		Dm_report(DmLogLevel_WARN, "DmPerformance: Command message with command %d not implemented", msg->command);
	}

	DmPattern* pttn = DmStyle_getRandomPattern(slf->style, slf->groove, msg->command);
	if (pttn == NULL) {
		Dm_report(DmLogLevel_INFO, "DmPerformance: No suitable pattern found. Silence ensues ...", msg->command);
		return;
	}

	DmPerformance_playPattern(slf, pttn);
}

static void DmPerformance_handleSegmentMessage(DmPerformance* slf, DmMessage_SegmentChange* msg) {
	DmSegment* sgt = msg->segment;
	DmSegment_release(slf->segment);

	// Get rid of the currently playing segment.
	DmMessageQueue_clear(&slf->control_queue);
	DmMessageQueue_clear(&slf->music_queue);
	DmSynth_sendNoteOffEverything(&slf->synth);

	// If a `NULL`-segment is provided, simply stop the playing segment!
	if (sgt == NULL) {
		DmStyle_release(slf->style);
		DmBand_release(slf->band);

		slf->time = 0;
		slf->style = NULL;
		slf->segment = NULL;
		slf->band = NULL;
		return;
	}

	Dm_report(DmLogLevel_INFO,
	          "DmPerformance: Playing segment \"%s\" (repeat %d/%d)",
	          msg->segment->info.unam,
	          msg->loop + 1,
	          msg->segment->repeats);

	// Reset the time to combat drift
	slf->time = 0;

	// Import all the new segment's messages
	// NOTE: If we have a different `play_start` or `loop_start`, we need to discard messages
	//       before it and make sure to re-align them at time 0.
	uint32_t start = msg->loop != 0 ? sgt->loop_start : sgt->play_start;
	uint32_t end = (msg->loop != 0 && sgt->loop_end != 0) ? sgt->loop_end : sgt->length;

	for (size_t i = 0; i < sgt->messages.length; ++i) {
		DmMessage* m = &sgt->messages.data[i];

		// Messages occur before the indicated start offset or after the end offset are cut out.
		if (m->time < start || m->time > end) {
			continue;
		}

		// The message starts at the indicated message's time but aligned at zero based on the start offset.
		uint32_t mt = slf->time + m->time - start;

		DmMessageQueue_add(&slf->control_queue, m, mt, DmQueueConflict_REPLACE);
	}

	// If we don't yet have a command, add it!
	DmMessage cmd;
	cmd.type = DmMessage_COMMAND;
	cmd.time = 0;
	cmd.command.command = DmCommand_GROOVE;
	cmd.command.groove_level = 1;
	DmMessageQueue_add(&slf->control_queue, &cmd, 0, DmQueueConflict_KEEP);

	slf->segment = DmSegment_retain(sgt);
	slf->segment_start = slf->time;

	// If required, schedule to loop this segment.
	if (msg->loop < sgt->repeats) {
		DmMessage m;
		m.type = DmMessage_SEGMENT;
		m.time = 0;
		m.segment.segment = DmSegment_retain(sgt);
		m.segment.loop = msg->loop + 1;

		DmMessageQueue_add(&slf->control_queue, &m, slf->time + slf->segment->length, DmQueueConflict_KEEP);
	}
}

static void DmPerformance_handleMessage(DmPerformance* slf, DmMessage* msg) {
	switch (msg->type) {
	case DmMessage_SEGMENT:
		if (msg->segment.segment) {
			Dm_report(DmLogLevel_TRACE,
			          "DmPerformance(Message): time=%d type=segment-change name=\"%s\"",
			          slf->time,
			          msg->segment.segment->info.unam);
		}

		DmPerformance_handleSegmentMessage(slf, &msg->segment);
		break;
	case DmMessage_STYLE:
		Dm_report(DmLogLevel_TRACE,
		          "DmPerformance(Message): time=%d type=style-change name=\"%s\"",
		          slf->time,
		          msg->style.style->info.unam);

		DmStyle_release(slf->style);
		slf->style = DmStyle_retain(msg->style.style);
		slf->time_signature = slf->style->time_signature;
		break;
	case DmMessage_BAND:
		Dm_report(DmLogLevel_TRACE,
		          "DmPerformance(Message): time=%d type=band-change name=\"%s\"",
		          slf->time,
		          msg->band.band->info.unam);

		DmBand_release(slf->band);
		slf->band = DmBand_retain(msg->band.band);
		DmSynth_sendBandUpdate(&slf->synth, msg->band.band);
		break;
	case DmMessage_TEMPO:
		Dm_report(DmLogLevel_TRACE,
		          "DmPerformance(Message): time=%d type=tempo-change value=%f",
		          slf->time,
		          msg->tempo.tempo);

		slf->tempo = msg->tempo.tempo;
		break;
	case DmMessage_COMMAND:
		Dm_report(DmLogLevel_TRACE,
		          "DmPerformance(Message): time=%d type=command-change value=%d groove=%d groove-range=%d",
		          slf->time,
		          msg->command.command,
		          msg->command.groove_level,
		          msg->command.groove_range);

		DmPerformance_handleCommandMessage(slf, &msg->command);
		break;
	case DmMessage_CHORD:
		Dm_report(DmLogLevel_TRACE,
		          "DmPerformance(Message): time=%d type=chord-change name=\"%s\"",
		          slf->time,
		          msg->chord.name);

		slf->chord = msg->chord;
		break;
	case DmMessage_NOTE:
		Dm_report(DmLogLevel_TRACE,
		          "DmPerformance(Message): time=%d type=note-%s channel=%d value=%d velocity=%d",
		          slf->time,
		          msg->note.on ? "on" : "off",
		          msg->note.channel,
		          msg->note.note,
		          msg->note.velocity);

		if (msg->note.on) {
			DmSynth_sendNoteOn(&slf->synth, msg->note.channel, msg->note.note, msg->note.velocity);
		} else {
			DmSynth_sendNoteOff(&slf->synth, msg->note.channel, msg->note.note);
		}
		break;
	case DmMessage_CONTROL:
		Dm_report(DmLogLevel_TRACE,
		          "DmPerformance(Message): time=%d type=control-change channel=%d control=%d value=%f",
		          slf->time,
		          msg->control.channel,
		          msg->control.control,
		          msg->control.value);

		DmSynth_sendControl(&slf->synth, msg->control.channel, msg->control.control, msg->control.value);

		if (msg->control.reset) {
			DmSynth_sendControlReset(&slf->synth, msg->control.channel, msg->control.control, msg->control.reset_value);
		} else {
			DmSynth_sendControlReset(&slf->synth, msg->control.channel, msg->control.control, msg->control.value);
		}

		break;
	case DmMessage_PITCH_BEND:
		Dm_report(DmLogLevel_TRACE,
		          "DmPerformance(Message): time=%d type=pitch-bend channel=%d value=%d",
		          slf->time,
		          msg->pitch_bend.channel,
		          msg->pitch_bend.value);

		DmSynth_sendPitchBend(&slf->synth, msg->pitch_bend.channel, msg->pitch_bend.value);
		if (msg->pitch_bend.reset) {
			DmSynth_sendPitchBendReset(&slf->synth, msg->pitch_bend.channel, msg->pitch_bend.reset_value);
		}

		break;
	default:
		Dm_report(DmLogLevel_ERROR, "DmPerformance: Message type %d not implemented", msg->type);
		break;
	}
}

DmResult DmPerformance_renderPcm(DmPerformance* slf, void* buf, size_t len, DmRenderOptions opts) {
	if (slf == NULL || buf == NULL) {
		return DmResult_INVALID_ARGUMENT;
	}

	if ((opts & DmRender_STEREO) && (len % 2 != 0)) {
		return DmResult_INVALID_ARGUMENT;
	}

	uint8_t const channels = opts & DmRender_STEREO ? 2 : 1;

	DmMessage msg_ctrl;
	DmMessage msg_midi;

	if (mtx_lock(&slf->lock) != thrd_success) {
		return DmResult_MUTEX_ERROR;
	}

	size_t sample = 0;
	while (sample < len) {

		bool ok_ctrl = DmMessageQueue_get(&slf->control_queue, &msg_ctrl);
		bool ok_midi = DmMessageQueue_get(&slf->music_queue, &msg_midi);

		if (!ok_ctrl && !ok_midi) {
			// No more messages to process.
			break;
		}

		if (ok_ctrl && ok_midi) {
			// Both queues have a message, choose the one that happens
			// earlier while preferring control messages
			ok_ctrl = msg_ctrl.time <= msg_midi.time;
			ok_midi = msg_midi.time < msg_ctrl.time;
		}

		DmMessage msg;
		DmMessage_copy(ok_ctrl ? &msg_ctrl : &msg_midi, &msg, -1);

		uint32_t time_offset = (uint32_t) max_s32((int) msg.time - (int) slf->time, 0);
		uint32_t offset_samples =
		    Dm_getSampleCountForDuration(time_offset, slf->time_signature, slf->tempo, slf->sample_rate, channels);

		if (offset_samples > len - sample) {
			// The next message does not fall into this render call (i.e. it happens after the number of
			// samples left to process)
			break;
		}

		// Eliminate crackling when rendering stereo audio. This is required so that we always output the
		// same number of samples for each channel.
		if ((opts & DmRender_STEREO)) {
			offset_samples += offset_samples % 2;
			time_offset = Dm_getDurationForSampleCount(offset_samples,
			                                           slf->time_signature,
			                                           slf->tempo,
			                                           slf->sample_rate,
			                                           channels);
		}

		// Render the samples from now until the message occurs and advance the buffer pointer
		// and time and sample counters.
		if (offset_samples > 0) {
			size_t bytes_rendered = DmSynth_render(&slf->synth, buf, offset_samples, opts);
			buf = (uint8_t*) buf + bytes_rendered;
		}

		sample += offset_samples;
		slf->time += time_offset;

		// Handle the next message
		if (ok_ctrl) {
			DmMessageQueue_pop(&slf->control_queue);
		} else {
			DmMessageQueue_pop(&slf->music_queue);
		}

		DmPerformance_handleMessage(slf, &msg);
		DmMessage_free(&msg);
	}

	(void) mtx_unlock(&slf->lock);

	// Render the remaining samples
	uint32_t remaining_samples = (uint32_t) (len - sample);
	(void) DmSynth_render(&slf->synth, buf, remaining_samples, opts);
	slf->time +=
	    Dm_getDurationForSampleCount(remaining_samples, slf->time_signature, slf->tempo, slf->sample_rate, channels);

	return DmResult_SUCCESS;
}

DmResult
DmPerformance_playTransition(DmPerformance* slf, DmSegment* sgt, DmEmbellishmentType embellishment, DmTiming timing) {
	if (slf == NULL || sgt == NULL) {
		return DmResult_INVALID_ARGUMENT;
	}

	// If no segment is currently playing, simply start playing the
	// new segment without a transition.
	if (slf->segment == NULL) {
		return DmPerformance_playSegment(slf, sgt, timing);
	}

	if (!sgt->downloaded) {
		return DmResult_INVALID_ARGUMENT;
	}

	DmSegment* transition = NULL;
	DmResult rv = Dm_composeTransition(slf->style, slf->band, &slf->chord, sgt, embellishment, &transition);
	if (rv != DmResult_SUCCESS) {
		return rv;
	}

	rv = DmPerformance_playSegment(slf, transition, timing);
	if (rv != DmResult_SUCCESS) {
		return rv;
	}

	DmSegment_release(transition);
	return DmResult_SUCCESS;
}

void DmPerformance_setVolume(DmPerformance* slf, float vol) {
	if (slf == NULL) {
		return;
	}

	DmSynth_setVolume(&slf->synth, vol);
}
