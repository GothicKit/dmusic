// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"

extern DmResult DmSynth_createTsfForInstrument(DmInstrument* slf, tsf** out);

static size_t max(size_t a, size_t b) {
	return a >= b ? a : b;
}

void DmSynth_init(DmSynth* slf) {
	if (slf == NULL) {
		return;
	}

	memset(slf, 0, sizeof *slf);
}

static void DmSynth_freeChannels(DmSynth* slf) {
	for (size_t i = 0; i < slf->channel_count; ++i) {
		tsf_close(slf->channels[i]);
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

// TODO(lmichaelis): Technically, we shoud change as little as possible to accomoate the new band.
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
		slf->channel_count = max(band->instruments[i].channel, slf->channel_count);
	}

	// Allocate all synths
	slf->channel_count += 1;
	slf->channels = Dm_alloc(sizeof(tsf*) * slf->channel_count);

	for (size_t i = 0; i < band->instrument_count; ++i) {
		DmInstrument* ins = &band->instruments[i];

		tsf* tsf = NULL;
		DmResult rv = DmSynth_createTsfForInstrument(ins, &tsf);
		if (rv != DmResult_SUCCESS) {
			slf->channels[ins->channel] = NULL;
			continue;
		}

		slf->channels[ins->channel] = tsf;

		float pan = (ins->flags & DmInstrument_PAN) ? (float) ins->pan / 127.F : 0.5f;
		float vol = (ins->flags & DmInstrument_VOLUME) ? (float) ins->volume / 127.F : 1.f;

		bool res = tsf_channel_set_pan(tsf, 0, pan);
		if (!res) {
			Dm_report(DmLogLevel_ERROR, "DmSynth: tsf_channel_set_pan encountered an error.");
		}

		res = tsf_channel_set_volume(tsf, 0, vol);
		if (!res) {
			Dm_report(DmLogLevel_ERROR, "DmSynth: tsf_channel_set_volume encountered an error.");
		}
	}
}

#define DmInt_MIDI_CC_VOLUME 7
#define DmInt_MIDI_CC_PAN 10
#define DmInt_MIDI_CC_EXPRESSION 11

void DmSynth_sendControl(DmSynth* slf, uint32_t channel, uint8_t control, float value) {
	if (slf == NULL || channel >= slf->channel_count) {
		return;
	}

	if (slf->channels[channel] == NULL) {
		return;
	}

	if (control == DmInt_MIDI_CC_VOLUME || control == DmInt_MIDI_CC_EXPRESSION) {
		tsf_channel_set_volume(slf->channels[channel], 0, value);
	}else if (control == DmInt_MIDI_CC_PAN) {
		tsf_channel_set_pan(slf->channels[channel], 0, value);
	} else {
		Dm_report(DmLogLevel_WARN, "DmSynth: Control change %d is unknown.", control);
	}
}

void DmSynth_sendPitchBend(DmSynth* slf, uint32_t channel, int bend) {
	if (slf == NULL || channel >= slf->channel_count) {
		return;
	}

	if (slf->channels[channel] == NULL) {
		return;
	}

	tsf_channel_set_pitchwheel(slf->channels[channel], 0, bend);
}

void DmSynth_sendNoteOn(DmSynth* slf, uint32_t channel, uint8_t note, uint8_t velocity) {
	if (slf == NULL || channel >= slf->channel_count) {
		return;
	}

	if (slf->channels[channel] == NULL) {
		return;
	}

	bool res = tsf_channel_note_on(slf->channels[channel], 0, note, ((float) velocity + 0.5f) / 127.f);
	if (!res) {
		Dm_report(DmLogLevel_ERROR, "DmSynth: DmSynth_sendNoteOn encountered an error.");
	}
}

void DmSynth_sendNoteOff(DmSynth* slf, uint32_t channel, uint8_t note) {
	if (slf == NULL || channel >= slf->channel_count) {
		return;
	}

	if (slf->channels[channel] == NULL) {
		return;
	}

	tsf_note_off(slf->channels[channel], 0, note);
}

void DmSynth_sendNoteOffAll(DmSynth* slf, uint32_t channel) {
	if (slf == NULL || channel >= slf->channel_count) {
		return;
	}

	if (slf->channels[channel] == NULL) {
		return;
	}

	tsf_channel_note_off_all(slf->channels[channel], 0);
}

void DmSynth_sendNoteOffEverything(DmSynth* slf) {
	if (slf == NULL) {
		return;
	}

	for (uint32_t i = 0; i < slf->channel_count; ++i) {
		if (slf->channels[i] == NULL) {
			continue;
		}

		DmSynth_sendNoteOffAll(slf, i);
	}
}

size_t DmSynth_render(DmSynth* slf, void* buf, size_t len, DmRenderOptions fmt) {
	for (size_t i = 0; i < slf->channel_count; ++i) {
		if (slf->channels[i] == NULL) {
			continue;
		}

		int channels = (fmt & DmRender_STEREO) ? 2 : 1;
		if (fmt & DmRender_STEREO) {
			tsf_set_output(slf->channels[i], TSF_STEREO_INTERLEAVED, 44100, 0);
		} else {
			tsf_set_output(slf->channels[i], TSF_MONO, 44100, 0);
		}

		if (fmt & DmRender_FLOAT) {
			tsf_render_float(slf->channels[i], buf, (int) len / channels, true);
		} else {
			tsf_render_short(slf->channels[i], buf, (int) len / channels, true);
		}
	}

	return fmt & DmRender_FLOAT ? len * 4 : len * 2;
}
