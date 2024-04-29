// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"
#include <math.h>
#include <stdlib.h>

enum {
	DmInt_DEFAULT_TEMPO = 100,
};

DmResult DmPerformance_create(DmPerformance** slf) {
	if (slf == NULL) {
		return DmResult_INVALID_ARGUMENT;
	}

	DmPerformance* new = *slf = Dm_alloc(sizeof *new);
	if (new == NULL) {
		return DmResult_MEMORY_EXHAUSTED;
	}

	new->reference_count = 1;
	new->tempo = DmInt_DEFAULT_TEMPO;
	new->groove = 1;
	new->time_signature.beats_per_measure = 4;
	new->time_signature.beat = 4;
	new->time_signature.grids_per_beat = 2;

	DmResult rv = DmMessageQueue_init(&new->control_queue);
	if (rv != DmResult_SUCCESS) {
		return rv;
	}

	rv = DmMessageQueue_init(&new->music_queue);
	if (rv != DmResult_SUCCESS) {
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

	DmMessageQueue_free(&slf->control_queue);
	DmMessageQueue_free(&slf->music_queue);
	DmSegment_release(slf->segment);
	DmStyle_release(slf->style);
	DmSynth_free(&slf->synth);
	Dm_free(slf);
}

// See https://documentation.help/DirectMusic/dmussegfflags.htm
static uint32_t DmPerformance_getStartTime(DmPerformance* slf, DmPlaybackFlags flags) {
	if (flags & DmPlayback_BEAT) {
		uint32_t beat_length = Dm_getBeatLength(slf->time_signature);
		uint32_t time_to_next_beat = 0;

		uint32_t offset = slf->time - slf->segment_start;

		if (offset != 0) {
			time_to_next_beat = beat_length - (offset % beat_length);
		}

		return slf->time + time_to_next_beat;
	}

	if (flags & DmPlayback_MEASURE) {
		uint32_t measure_length = Dm_getMeasureLength(slf->time_signature);
		uint32_t time_to_next_measure = 0;

		uint32_t offset = slf->time - slf->segment_start;

		if (offset != 0) {
			time_to_next_measure = measure_length - (offset % measure_length);
		}
		return slf->time + time_to_next_measure;
	}

	Dm_report(DmLogLevel_ERROR, "DmPerformance: Playback flags %d not supported", flags);
	return slf->time;
}

DmResult DmPerformance_playSegment(DmPerformance* slf, DmSegment* sgt, DmPlaybackFlags flags) {
	if (slf == NULL) {
		return DmResult_INVALID_ARGUMENT;
	}

	if (flags & DmPlayback_SECONDARY) {
		Dm_report(DmLogLevel_ERROR, "DmPerformance: Secondary segments are not yet supported");
		return DmResult_INVALID_ARGUMENT;
	}

	if (flags & DmPlayback_DEFAULT) {
		flags = (DmPlaybackFlags) sgt->resolution;
	}

	uint32_t offset = DmPerformance_getStartTime(slf, flags);

	DmMessage msg;
	msg.time = 0;
	msg.type = DmMessage_SEGMENT;
	msg.segment.segment = sgt;
	DmMessageQueue_add(&slf->control_queue, &msg, offset, DmQueueConflict_REPLACE);

	return DmResult_SUCCESS;
}

static uint32_t DmPerformance_commandToEmbellishment(DmCommandType cmd) {
	uint32_t f = 0;

	if (cmd & DmCommand_FILL) {
		f |= 1; // "fill"
	}

	if (cmd & DmCommand_INTRO) {
		f |= 2; // "intro"
	}

	if (cmd & DmCommand_BREAK) {
		f |= 4; // "break"
	}

	if (cmd & DmCommand_END) {
		f |= 8; // "end"
	}

	return f; // "normal"
}

// See: https://documentation.help/DirectMusic/howmusicvariesduringplayback.htm
static DmPattern* DmPerformance_choosePattern(DmPerformance* slf, DmCommandType cmd) {
	size_t suitable_pattern_index = 0;
	size_t suitable_pattern_count = 0;

	uint32_t embellishment = DmPerformance_commandToEmbellishment(cmd);
	for (size_t i = 0; i < slf->style->patterns.length; ++i) {
		DmPattern* pttn = &slf->style->patterns.data[i];

		// Ignore patterns outside the current groove level.
		if (slf->groove < pttn->groove_bottom || slf->groove > pttn->groove_top) {
			continue;
		}

		// Patterns with a differing embellishment are not supported
		if (pttn->embellishment != embellishment && !(pttn->embellishment & embellishment)) {
			continue;
		}

		// Fix for Gothic 2 in which some patterns are empty but have a groove range of 1-100 with no embellishment set.
		if (pttn->embellishment == DmCommand_GROOVE && pttn->length_measures == 1) {
			continue;
		}

		suitable_pattern_index = i;
		suitable_pattern_count += 1;
	}

	if (suitable_pattern_count == 0) {
		return NULL;
	}

	if (suitable_pattern_count == 1) {
		return &slf->style->patterns.data[suitable_pattern_index];
	}

	// Select a random pattern. TODO(lmichaelis): This behaviour seems to be associated with DX < 8 only,
	// newer versions should have some way of defining how to select the pattern if more than 1 choice is
	// available but I couldn't find it.
	suitable_pattern_index = (size_t) rand() % suitable_pattern_count;

	// Do the thing from above again, but this time return the selected pattern!
	// TODO(lmichaelis): Deduplicate!
	for (size_t i = 0; i < slf->style->patterns.length; ++i) {
		DmPattern* pttn = &slf->style->patterns.data[i];

		if (slf->groove < pttn->groove_bottom || slf->groove > pttn->groove_top) {
			continue;
		}

		if (pttn->embellishment != DmPerformance_commandToEmbellishment(cmd)) {
			continue;
		}

		if (suitable_pattern_index == 0) {
			return pttn;
		}

		suitable_pattern_index--;
	}

	return NULL;
}

static uint32_t DmInt_DEFAULT_SCALE_PATTERN = 0xab5ab5;
static uint32_t DmInt_FALLBACK_SCALES[12] = {
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

uint8_t bit_count(uint32_t v) {
	uint8_t count = 0;

	for (uint8_t i = 0u; i < 32; ++i) {
		count += v & 1;
		v >>= 1;
	}

	return count;
}

uint32_t fixup_scale(uint32_t scale, uint8_t scale_root) {
	// Force the scale to be exactly two octaves wide by zero-ing out the upper octave and
	// copying the lower octave into the upper one
	scale = (scale & 0x0FFF) | (scale << 12);

	// Add the root to the scale
	scale = scale >> (12 - (scale_root & 12));

	// Clean up the scale again.
	scale = (scale & 0x0FFF) | (scale << 12);

	// If there are less than 5 bits set in the scale, figure out a fallback to use instead
	// TODO: Random, but sure.
	if (bit_count(scale & 0xFFF) <= 4) {
		uint32_t best_scale = DmInt_DEFAULT_SCALE_PATTERN;
		uint32_t best_score = 0;

		for (size_t i = 0; i < 12; ++i) {
			// Determine the score by checking the number of bits which are are set in both
			uint32_t score = bit_count((DmInt_FALLBACK_SCALES[i] & scale) & 0xFFF);

			if (score > best_score) {
				best_scale = DmInt_FALLBACK_SCALES[i];
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
		Dm_report(DmLogLevel_ERROR, "DmPerformance: DmPlayMode_KEY_ROOT requested but we don't support it");
		return -1;
	}

	// Now, to the meat of the routine: Determine the actual note position based on the chord and scale patterns!
	if (!(mode & (DmPlayMode_CHORD_INTERVALS | DmPlayMode_SCALE_INTERVALS))) {
		Dm_report(DmLogLevel_ERROR,
		          "DmPerformance: DmPlayMode_CHORD_INTERVALS, nor DmPlayMode_SCALE_INTERVALS requested");
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

static float lerp(float x, float start, float end) {
	return (1 - x) * start + x * end;
}

// See https://documentation.help/DirectMusic/dmusiostylenote.htm
static uint32_t DmPerformance_convertIoTimeRange(uint8_t range) {
	uint32_t result = 0;
	if (range <= 190) {
		result = range;
	} else if (191 <= range && range <= 212) {
		result = ((range - 190) * 5) + 190;
	} else if (213 <= range && range <= 232) {
		result = ((range - 212) * 10) + 300;
	} else {
		// range > 232
		result = ((range - 232) * 50) + 500;
	}
	return result;
}

static void DmPerformance_playPattern(DmPerformance* slf, DmPattern* pttn) {
	DmSynth_reset(&slf->synth);
	DmMessageQueue_clear(&slf->music_queue);
	DmSynth_sendNoteOffEverything(&slf->synth);

	Dm_report(DmLogLevel_DEBUG, "DmPerformance: Playing pattern '%s'", pttn->info.unam);

	int variation_lock[256];
	for (size_t i = 0; i < 256; ++i) {
		variation_lock[i] = -1;
	}

	for (size_t i = 0; i < pttn->parts.length; ++i) {
		DmPartReference* pref = &pttn->parts.data[i];
		DmPart* part = DmStyle_findPart(slf->style, pref);
		if (part == NULL) {
			Dm_report(DmLogLevel_ERROR, "DmPerformance: Part reference could not be resolved!");
			continue;
		}

		// If the variation lock is nonzero, we need select the same variation for all parts
		// with the same lock ID. Otherwise, we play a variation according to the settings.
		int32_t variation = variation_lock[pref->variation_lock_id];
		if (pref->variation_lock_id == 0 || variation < 0) {
			if (pref->random_variation == 0) {
				// This pattern is supposed to play its variations in sequence
				// TODO(lmichaelis): This global counter is probably not correct. Replace it with one specific for each
				// pattern
				variation = (int32_t) slf->variation;
			} else if (pref->random_variation == 1) {
				// This pattern is supposed to play its variations in a random order
				variation = rand();
			} else {
				Dm_report(DmLogLevel_ERROR,
				          "DmPerformance: Unknown pattern variation selection: %d",
				          pref->random_variation);
			}

			variation_lock[pref->variation_lock_id] = (int) variation;
		}

		variation = 1 << ((uint32_t) variation % DmPart_getValidVariationCount(part));

		// Now we need to select the correct sub-chord to use for the pattern
		// by comparing against the sub-chord level of the pattern. By default,
		// we just use the first one.
		struct DmSubChord chord = slf->chord.subchords[0];

		for (size_t j = 0; j < slf->chord.subchord_count; ++j) {
			if (slf->chord.subchords[j].levels & (1 << pref->subchord_level)) {
				chord = slf->chord.subchords[j];
				break;
			}
		}

		// Now we are ready to create all the note on/note off messages
		// for this pattern
		for (size_t j = 0; j < part->note_count; ++j) {
			DmNote note = part->notes[j];

			// We ignore notes which do not correspond to the selected variation
			if (!(note.variation & (uint32_t) variation)) {
				continue;
			}

			DmPlayModeFlags flags =
			    note.play_mode_flags == DmPlayMode_NONE ? part->play_mode_flags : note.play_mode_flags;
			int midi = DmPerformance_musicValueToMidi(chord, flags, note.music_value);
			if (midi < 0) {
				// We were unable to convert the music value
				Dm_report(DmLogLevel_WARN, "DmPerformance: Unable to convert music value %d to MIDI", note.music_value);
				continue;
			}

			uint32_t time = Dm_getTimeOffset(note.grid_start, note.time_offset, part->time_signature);

			if (note.time_range != 0) {
				uint32_t range = DmPerformance_convertIoTimeRange(note.time_range);
				uint32_t rnd = (uint32_t) rand() % range;
				time -= range - (rnd / 2);
			}

			uint32_t velocity = note.velocity;
			if (note.velocity_range != 0) {
				uint32_t rnd = (uint32_t) rand() % note.velocity_range;
				velocity -= note.velocity_range - (rnd / 2);
			}

			uint32_t duration = note.duration;
			if (note.duration_range != 0) {
				uint32_t range = DmPerformance_convertIoTimeRange(note.duration_range);
				uint32_t rnd = (uint32_t) rand() % range;
				duration -= range - (rnd / 2);
			}

			DmMessage msg;
			msg.type = DmMessage_NOTE;
			msg.time = 0;

			msg.note.on = true;
			msg.note.note = (uint8_t) midi;
			msg.note.velocity = (uint8_t) velocity;
			msg.note.channel = pref->logical_part_id;

			DmMessageQueue_add(&slf->music_queue, &msg, slf->time + time, DmQueueConflict_APPEND);

			msg.note.on = false;
			msg.note.note = (uint8_t) midi;

			DmMessageQueue_add(&slf->music_queue, &msg, slf->time + time + duration, DmQueueConflict_APPEND);
		}

		for (size_t j = 0; j < part->curve_count; ++j) {
			DmCurve curve = part->curves[j];

			// We ignore notes which do not correspond to the selected variation
			if (!(curve.variation & (uint32_t) variation)) {
				continue;
			}

			// TODO(lmichaelis): Implement the other curve types!
			if (curve.event_type != DmCurveType_CONTROL_CHANGE && curve.event_type != DmCurveType_PITCH_BEND) {
				Dm_report(DmLogLevel_ERROR, "DmPerformance: Curve type %d is not supported", curve.event_type);
				continue;
			}

			uint32_t start_time = Dm_getTimeOffset(curve.grid_start, curve.time_offset, part->time_signature);
			uint32_t duration = curve.duration / 2;

			int16_t start = curve.start_value;
			int16_t end = curve.end_value;

			float prev_value;
			// TODO(lmichaelis): Check whether this is actually correct!
			for (uint32_t k = 0; k < (duration / DmInt_CURVE_SPACING); ++k) {
				uint32_t offset = k * DmInt_CURVE_SPACING;
				float phase = (float) offset / (float) duration;

				float value = 0;
				switch (curve.curve_shape) {
				case DmCurveShape_LINEAR:
					value = lerp(phase, start, end);
					break;
				case DmCurveShape_INSTANT:
					value = end;
					break;
				case DmCurveShape_EXP:
					value = lerp(powf(phase, 4), start, end);
					break;
				case DmCurveShape_LOG:
					value = lerp(sqrtf(phase), start, end);
					break;
				case DmCurveShape_SINE:
					value = lerp((sinf((phase - 0.5f) * (float) M_PI) + 1) * 0.5f, start, end);
					break;
				}

				DmMessage msg;
				msg.time = 0;

				if (curve.event_type == DmCurveType_CONTROL_CHANGE) {
					msg.type = DmMessage_CONTROL;
					msg.control.control = curve.cc_data;
					msg.control.channel = pref->logical_part_id;
					msg.control.value = value / 127.f;
					msg.control.reset = curve.flags & DmCurveFlags_RESET;
					msg.control.reset_value = curve.reset_value / 127.f;

					// Optimization: Don't emit a message if the value is the same as the previous one.
					if (msg.control.value == prev_value) {
						continue;
					}

					prev_value = msg.control.value;
				} else if (curve.event_type == DmCurveType_PITCH_BEND) {
					msg.type = DmMessage_PITCH_BEND;
					msg.pitch_bend.value = (int) value;
					msg.pitch_bend.channel = pref->logical_part_id;
					msg.pitch_bend.reset = curve.flags & DmCurveFlags_RESET;
					msg.pitch_bend.reset_value = (int) value;

					// Optimization: Don't emit a message if the value is the same as the previous one.
					if (msg.pitch_bend.value == (uint16_t) prev_value) {
						continue;
					}

					prev_value = (float) msg.pitch_bend.value;
				}

				DmMessageQueue_add(&slf->music_queue, &msg, slf->time + start_time + offset, DmQueueConflict_APPEND);
			}
		}
	}

	DmMessage msg;
	msg.type = DmMessage_COMMAND;
	msg.command.command = DmCommand_GROOVE;
	msg.command.groove_level = slf->groove;
	msg.command.groove_range = slf->groove_range;
	msg.command.repeat_mode = DmPatternSelect_RANDOM;
	msg.command.beat = 0;
	msg.command.measure = 0;

	uint32_t pattern_length = Dm_getMeasureLength(slf->time_signature) * pttn->length_measures;
	DmMessageQueue_add(&slf->music_queue, &msg, slf->time + pattern_length, DmQueueConflict_KEEP);

	slf->variation += 1;
}

static void DmPerformance_handleCommandMessage(DmPerformance* slf, DmMessage_Command* msg) {
	if (msg->command == DmCommand_GROOVE) {
		slf->groove = msg->groove_level;
		slf->groove_range = msg->groove_range;

		// Randomize the groove level
		if (msg->groove_range != 0) {
			int rnd = rand() % msg->groove_range;
			int range = rnd - (msg->groove_range / 2);
			slf->groove = (uint8_t) max_s32(msg->groove_level + range, 0);
		}
	} else if (msg->command == DmCommand_END_AND_INTRO) {
		Dm_report(DmLogLevel_WARN, "DmPerformance: Command message with command %d not implemented", msg->command);
	}

	DmPattern* pttn = DmPerformance_choosePattern(slf, msg->command);
	if (pttn == NULL) {
		Dm_report(DmLogLevel_WARN, "DmPerformance: No suitable pattern found. Silence ensues ...", msg->command);
		return;
	}

	DmPerformance_playPattern(slf, pttn);
}

static void DmPerformance_handleMessage(DmPerformance* slf, DmMessage* msg) {
	switch (msg->type) {
	case DmMessage_SEGMENT: {
		Dm_report(DmLogLevel_DEBUG,
		          "DmPerformance: MESSAGE time=%d msg=segment-change segment=\"%s\"",
		          slf->time,
		          msg->segment.segment->info.unam);

		// TODO(lmichaelis): The segment in this message might no longer be valid, since we
		// have called `pop` on the queue but not kept a strong reference to the message!
		DmSegment* sgt = msg->segment.segment;

		DmMessageQueue_clear(&slf->control_queue);
		DmMessageQueue_clear(&slf->music_queue);
		DmSynth_sendNoteOffEverything(&slf->synth);

		// Reset the time to combat drift
		slf->time = 0;

		for (size_t i = 0; i < sgt->messages.length; ++i) {
			DmMessage* m = &sgt->messages.data[i];

			if (msg->segment.loop == 0 && m->time < sgt->play_start) {
				continue;
			}

			if (msg->segment.loop > 0 && (m->time < sgt->loop_start || m->time > sgt->loop_end)) {
				continue;
			}

			DmMessageQueue_add(&slf->control_queue, m, slf->time + m->time, DmQueueConflict_REPLACE);
		}

		DmSegment_release(slf->segment);
		slf->segment = DmSegment_retain(sgt);
		slf->segment_start = slf->time;

		if (msg->segment.loop < sgt->repeats) {
			DmMessage m;
			m.type = DmMessage_SEGMENT;
			m.time = 0;
			m.segment.segment = sgt;
			m.segment.loop = msg->segment.loop + 1;

			DmMessageQueue_add(&slf->control_queue, &m, slf->time + slf->segment->length, DmQueueConflict_KEEP);
		}
		break;
	}
	case DmMessage_STYLE:
		// TODO(lmichaelis): The style in this message might have already been de-allocated!
		slf->style = DmStyle_retain(msg->style.style);
		slf->time_signature = slf->style->time_signature;
		Dm_report(DmLogLevel_DEBUG,
		          "DmPerformance: MESSAGE time=%d msg=style-change style=\"%s\"",
		          slf->time,
		          msg->style.style->info.unam);
		break;
	case DmMessage_BAND:
		// TODO(lmichaelis): The band in this message might have already been de-allocated!
		DmSynth_sendBandUpdate(&slf->synth, msg->band.band);
		Dm_report(DmLogLevel_DEBUG,
		          "DmPerformance: MESSAGE time=%d msg=band-change band=\"%s\"",
		          slf->time,
		          msg->band.band->info.unam);
		break;
	case DmMessage_TEMPO:
		slf->tempo = msg->tempo.tempo;
		Dm_report(DmLogLevel_DEBUG,
		          "DmPerformance: MESSAGE time=%d msg=tempo-change tempo=%f",
		          slf->time,
		          msg->tempo.tempo);
		break;
	case DmMessage_COMMAND:
		DmPerformance_handleCommandMessage(slf, &msg->command);
		Dm_report(DmLogLevel_DEBUG,
		          "DmPerformance: MESSAGE time=%d msg=command kind=%d groove=%d (+/- %d)",
		          slf->time,
		          msg->command.command,
		          msg->command.groove_level,
		          msg->command.groove_range / 2);
		break;
	case DmMessage_CHORD:
		slf->chord = msg->chord;
		Dm_report(DmLogLevel_DEBUG, "DmPerformance: MESSAGE time=%d msg=chord-change", slf->time);
		break;
	case DmMessage_NOTE:
		if (msg->note.on) {
			DmSynth_sendNoteOn(&slf->synth, msg->note.channel, msg->note.note, msg->note.velocity);
			Dm_report(DmLogLevel_TRACE,
			          "DmPerformance: MESSAGE time=%d msg=note-on note=%d channel=%d velocity=%d",
			          slf->time,
			          (int) msg->note.note,
			          msg->note.channel,
			          msg->note.velocity);
		} else {
			DmSynth_sendNoteOff(&slf->synth, msg->note.channel, msg->note.note);
			Dm_report(DmLogLevel_TRACE,
			          "DmPerformance: MESSAGE time=%d msg=note-off note=%d channel=%d",
			          slf->time,
			          (int) msg->note.note,
			          msg->note.channel);
		}
		break;
	case DmMessage_CONTROL:
		DmSynth_sendControl(&slf->synth, msg->control.channel, msg->control.control, msg->control.value);
		if (msg->control.reset) {
			DmSynth_sendControlReset(&slf->synth, msg->control.channel, msg->control.control, msg->control.reset_value);
		}

		Dm_report(DmLogLevel_TRACE,
		          "DmPerformance: MESSAGE time=%d msg=control channel=%d control=%d value=%f",
		          slf->time,
		          msg->control.channel,
		          msg->control.control,
		          msg->control.value);
		break;
	case DmMessage_PITCH_BEND:
		DmSynth_sendPitchBend(&slf->synth, msg->pitch_bend.channel, msg->pitch_bend.value);
		if (msg->pitch_bend.reset) {
			DmSynth_sendPitchBendReset(&slf->synth, msg->pitch_bend.channel, msg->pitch_bend.reset_value);
		}

		Dm_report(DmLogLevel_TRACE,
		          "DmPerformance: MESSAGE time=%d channel=%d msg=pitch-bend value=%d",
		          slf->time,
		          msg->pitch_bend.channel,
		          msg->pitch_bend.value);
		break;
	default:
		Dm_report(DmLogLevel_INFO, "DmPerformance: Message type %d not implemented", msg->type);
		break;
	}
}

static uint32_t DmPerformance_getSampleCountFromDuration(DmPerformance* slf, uint32_t duration, uint32_t sample_rate, uint8_t channels) {
	double pulses_per_sample = Dm_getTicksPerSample(slf->time_signature, slf->tempo, sample_rate) / channels;
	return (uint32_t) (duration / pulses_per_sample);
}

static uint32_t DmPerformance_getDurationFromSampleCount(DmPerformance* slf, uint32_t samples, uint32_t sample_rate, uint8_t channels) {
	double pulses_per_sample = Dm_getTicksPerSample(slf->time_signature, slf->tempo, sample_rate) / channels;
	return (uint32_t) round(pulses_per_sample * samples);
}

DmResult DmPerformance_renderPcm(DmPerformance* slf, void* buf, size_t len, DmRenderOptions opts) {
	if (slf == NULL || buf == NULL) {
		return DmResult_INVALID_ARGUMENT;
	}

	uint8_t const channels = opts & DmRender_STEREO ? 2 : 1;
	uint32_t const sample_rate = 44100;

	DmMessage msg_ctrl;
	DmMessage msg_midi;

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

		DmMessage* msg = ok_ctrl ? &msg_ctrl : &msg_midi;
		uint32_t time_offset = (uint32_t) max_s32((int) msg->time - (int) slf->time, 0);
		uint32_t offset_samples = DmPerformance_getSampleCountFromDuration(slf, time_offset, sample_rate, channels);

		if (offset_samples > len - sample) {
			// The next message does not fall into this render call (i.e. it happens after the number of
			// samples left to process)
			break;
		}

		// Eliminate crackling when rendering stereo audio. This is required so that we always output the
		// same number of samples for each channel.
		if ((opts & DmRender_STEREO)) {
			offset_samples += offset_samples % 2;
			time_offset = DmPerformance_getDurationFromSampleCount(slf, offset_samples, sample_rate, channels);
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

		DmPerformance_handleMessage(slf, msg);
	}

	// Render the remaining samples
	uint32_t remaining_samples = (uint32_t) (len - sample);
	(void) DmSynth_render(&slf->synth, buf, remaining_samples, opts);
	slf->time += DmPerformance_getDurationFromSampleCount(slf, remaining_samples, sample_rate, channels);

	return DmResult_SUCCESS;
}

DmResult DmPerformance_playTransition(DmPerformance* slf,
                                      DmSegment* sgt,
                                      DmEmbellishmentType embellishment,
                                      DmPlaybackFlags flags) {
	if (slf == NULL || sgt == NULL) {
		return DmResult_INVALID_ARGUMENT;
	}

	DmMessageQueue_clear(&slf->control_queue);

	// TODO: This is wrong!
	uint32_t start = DmPerformance_getStartTime(slf, flags);
	if (embellishment != DmEmbellishment_NONE) {
		// TODO: This won't work if there are multiple possible pattern
		DmPattern* pattern = DmPerformance_choosePattern(slf, (DmCommandType) embellishment);

		if (pattern != NULL) {
			DmMessage msg;
			msg.type = DmMessage_COMMAND;
			msg.command.command = (DmCommandType) embellishment;
			msg.command.groove_level = pattern->groove_bottom;
			msg.command.groove_range = 0;
			msg.command.measure = 0;
			msg.command.beat = 0;
			msg.command.repeat_mode = DmPatternSelect_RANDOM;

			DmMessageQueue_add(&slf->control_queue, &msg, start, DmQueueConflict_REPLACE);

			start += Dm_getMeasureLength(pattern->time_signature) * pattern->length_measures;
		}
	}

	DmMessage msg;
	msg.type = DmMessage_SEGMENT;
	msg.segment.segment = sgt;
	DmMessageQueue_add(&slf->control_queue, &msg, start, DmQueueConflict_REPLACE);

	return DmResult_SUCCESS;
}
