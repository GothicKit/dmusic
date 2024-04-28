// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"

enum {
	DmInt_MIDI_CC_VOLUME = 7,
	DmInt_MIDI_CC_PAN = 10,
	DmInt_MIDI_CC_EXPRESSION = 11,

	DmInt_MIDI_MAX = 127,
	DmInt_PITCH_BEND_NEUTRAL = 8192
};

#define DmInt_PAN_CENTER 0.5F
#define DmInt_VOLUME_MAX 1.0F

extern DmResult DmSynth_createTsfForInstrument(DmInstrument* slf, tsf** out);

void DmSynth_init(DmSynth* slf) {
	if (slf == NULL) {
		return;
	}

	memset(slf, 0, sizeof *slf);
}

static void DmSynth_freeChannels(DmSynth* slf) {
	for (size_t i = 0; i < slf->channel_count; ++i) {
		tsf_close(slf->channels[i].synth);
	}

	Dm_free(slf->channels);

	slf->channels = NULL;
	slf->channel_count = 0;
}

void DmSynth_free(DmSynth* slf) {
	if (slf == NULL) {
		return;
	}

	DmSynth_freeChannels(slf);
}

void DmSynth_reset(DmSynth* slf) {
	if (slf == NULL) {
		return;
	}

	for (uint32_t i = 0; i < slf->channel_count; ++i) {
		if (slf->channels[i].synth == NULL) {
			continue;
		}

		tsf_channel_set_volume(slf->channels[i].synth, 0, slf->channels[i].volume_reset);
		tsf_channel_set_pan(slf->channels[i].synth, 0, slf->channels[i].pan_reset);
		tsf_channel_set_pitchwheel(slf->channels[i].synth, 0, slf->channels[i].pitch_bend_reset);
	}
}

// TODO(lmichaelis): Technically, we should change as little as possible to accommodate the new band.
//                   For example: if only the pan of an instrument changes, we should also only update that
//                   instead of reloading the entire instrument list and re-creating all TSFs.
// See also: https://documentation.help/DirectMusic/usingbands.htm
void DmSynth_sendBandUpdate(DmSynth* slf, DmBand* band) {
	if (slf == NULL) {
		return;
	}

	// Optimization: Don't re-alloc the entire thing if we're getting the same band
	if (band == slf->band) {
		return;
	}

	slf->band = band;
	DmSynth_freeChannels(slf);

	if (band == NULL) {
		return;
	}

	// Calculate the number of required performance channels
	for (size_t i = 0; i < band->instrument_count; ++i) {
		slf->channel_count = max_usize(band->instruments[i].channel, slf->channel_count);
	}

	// Allocate all synths
	slf->channel_count += 1;
	slf->channels = Dm_alloc(sizeof(DmSynthChannel) * slf->channel_count);

	for (size_t i = 0; i < band->instrument_count; ++i) {
		DmInstrument* ins = &band->instruments[i];

		tsf* tsf = NULL;
		DmResult rv = DmSynth_createTsfForInstrument(ins, &tsf);
		if (rv != DmResult_SUCCESS) {
			slf->channels[ins->channel].synth = NULL;
			continue;
		}

		float pan = (ins->flags & DmInstrument_PAN) ? (float) ins->pan / DmInt_MIDI_MAX : DmInt_PAN_CENTER;
		float vol = (ins->flags & DmInstrument_VOLUME) ? (float) ins->volume / DmInt_MIDI_MAX : DmInt_VOLUME_MAX;

		bool res = tsf_channel_set_pan(tsf, 0, pan);
		if (!res) {
			Dm_report(DmLogLevel_ERROR, "DmSynth: tsf_channel_set_pan encountered an error.");
		}

		res = tsf_channel_set_volume(tsf, 0, vol);
		if (!res) {
			Dm_report(DmLogLevel_ERROR, "DmSynth: tsf_channel_set_volume encountered an error.");
		}

		slf->channels[ins->channel].synth = tsf;
		slf->channels[ins->channel].pitch_bend_reset = DmInt_PITCH_BEND_NEUTRAL;
		slf->channels[ins->channel].volume_reset = vol;
		slf->channels[ins->channel].pan_reset = pan;
	}
}

void DmSynth_sendControl(DmSynth* slf, uint32_t channel, uint8_t control, float value) {
	if (slf == NULL || channel >= slf->channel_count) {
		return;
	}

	if (slf->channels[channel].synth == NULL) {
		return;
	}

	if (control == DmInt_MIDI_CC_VOLUME || control == DmInt_MIDI_CC_EXPRESSION) {
		tsf_channel_set_volume(slf->channels[channel].synth, 0, value);
	} else if (control == DmInt_MIDI_CC_PAN) {
		tsf_channel_set_pan(slf->channels[channel].synth, 0, value);
	} else {
		Dm_report(DmLogLevel_WARN, "DmSynth: Control change %d is unknown.", control);
	}
}

void DmSynth_sendControlReset(DmSynth* slf, uint32_t channel, uint8_t control, float reset) {
	if (slf == NULL || channel >= slf->channel_count) {
		return;
	}

	if (slf->channels[channel].synth == NULL) {
		return;
	}

	if (control == DmInt_MIDI_CC_VOLUME || control == DmInt_MIDI_CC_EXPRESSION) {
		slf->channels[channel].volume_reset = reset;
	} else if (control == DmInt_MIDI_CC_PAN) {
		slf->channels[channel].pan_reset = reset;
	} else {
		Dm_report(DmLogLevel_WARN, "DmSynth: Control change %d is unknown.", control);
	}
}

void DmSynth_sendPitchBend(DmSynth* slf, uint32_t channel, int bend) {
	if (slf == NULL || channel >= slf->channel_count) {
		return;
	}

	if (slf->channels[channel].synth == NULL) {
		return;
	}

	tsf_channel_set_pitchwheel(slf->channels[channel].synth, 0, bend);
}

void DmSynth_sendPitchBendReset(DmSynth* slf, uint32_t channel, int reset) {
	if (slf == NULL || channel >= slf->channel_count) {
		return;
	}

	if (slf->channels[channel].synth == NULL) {
		return;
	}

	slf->channels[channel].pitch_bend_reset = reset;
}

void DmSynth_sendNoteOn(DmSynth* slf, uint32_t channel, uint8_t note, uint8_t velocity) {
	if (slf == NULL || channel >= slf->channel_count) {
		return;
	}

	if (slf->channels[channel].synth == NULL) {
		return;
	}

	bool res = tsf_channel_note_on(slf->channels[channel].synth, 0, note, ((float) velocity + 0.5f) / DmInt_MIDI_MAX);
	if (!res) {
		Dm_report(DmLogLevel_ERROR, "DmSynth: DmSynth_sendNoteOn encountered an error.");
	}
}

void DmSynth_sendNoteOff(DmSynth* slf, uint32_t channel, uint8_t note) {
	if (slf == NULL || channel >= slf->channel_count) {
		return;
	}

	if (slf->channels[channel].synth == NULL) {
		return;
	}

	tsf_note_off(slf->channels[channel].synth, 0, note);
}

void DmSynth_sendNoteOffAll(DmSynth* slf, uint32_t channel) {
	if (slf == NULL || channel >= slf->channel_count) {
		return;
	}

	if (slf->channels[channel].synth == NULL) {
		return;
	}

	tsf_channel_note_off_all(slf->channels[channel].synth, 0);
}

void DmSynth_sendNoteOffEverything(DmSynth* slf) {
	if (slf == NULL) {
		return;
	}

	for (uint32_t i = 0; i < slf->channel_count; ++i) {
		if (slf->channels[i].synth == NULL) {
			continue;
		}

		DmSynth_sendNoteOffAll(slf, i);
	}
}

size_t DmSynth_render(DmSynth* slf, void* buf, size_t len, DmRenderOptions fmt) {
	for (size_t i = 0; i < slf->channel_count; ++i) {
		if (slf->channels[i].synth == NULL) {
			continue;
		}

		int channels = (fmt & DmRender_STEREO) ? 2 : 1;
		if (fmt & DmRender_STEREO) {
			tsf_set_output(slf->channels[i].synth, TSF_STEREO_INTERLEAVED, 44100, 0);
		} else {
			tsf_set_output(slf->channels[i].synth, TSF_MONO, 44100, 0);
		}

		if (fmt & DmRender_FLOAT) {
			tsf_render_float(slf->channels[i].synth, buf, (int) len / channels, true);
		} else {
			tsf_render_short(slf->channels[i].synth, buf, (int) len / channels, true);
		}
	}

	return fmt & DmRender_FLOAT ? len * 4 : len * 2;
}
