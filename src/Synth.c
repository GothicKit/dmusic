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

void DmSynth_init(DmSynth* slf) {
	if (slf == NULL) {
		return;
	}

	memset(slf, 0, sizeof *slf);
	DmSynthInstrumentArray_init(&slf->instruments);
	slf->volume = 1;
}

void DmSynth_free(DmSynth* slf) {
	if (slf == NULL) {
		return;
	}

	Dm_free(slf->channels);
	DmSynthInstrumentArray_free(&slf->instruments);
}

void DmSynth_reset(DmSynth* slf) {
	if (slf == NULL) {
		return;
	}

	for (uint32_t i = 0; i < slf->instruments.length; ++i) {
		DmSynthInstrument* ins = &slf->instruments.data[i];

		ins->volume = ins->volume_reset;
		tsf_channel_set_pan(ins->synth, 0, ins->pan_reset);
		tsf_channel_set_pitchwheel(ins->synth, 0, ins->pitch_bend_reset);
	}
}

static DmSynthInstrument* DmSynth_getInstrument(DmSynth* slf, DmInstrument* ins) {
	for (size_t i = 0; i < slf->instruments.length; ++i) {
		uint32_t bank = (ins->patch & 0xFF00U) >> 8;
		uint32_t patch = ins->patch & 0xFFU;
		if (slf->instruments.data[i].dls == ins->dls_collection && slf->instruments.data[i].bank == bank &&
		    slf->instruments.data[i].patch == patch) {
			return &slf->instruments.data[i];
		}
	}

	return NULL;
}

static DmResult DmSynth_loadInstruments(DmSynth* slf, DmBand* band) {
	DmResult rv = DmResult_SUCCESS;
	for (size_t i = 0; i < band->instrument_count; ++i) {
		DmInstrument* ins = &band->instruments[i];
		if (ins->dls == NULL) {
			continue;
		}

		DmSynthInstrument* sin = DmSynth_getInstrument(slf, ins);

		// The instrument does not yet exist. Create it anew!
		if (sin == NULL) {
			DmSynthInstrument new_ins;
			rv = DmSynth_createTsfForInstrument(ins, &new_ins.synth);
			if (rv != DmResult_SUCCESS) {
				continue;
			}

			new_ins.bank = (ins->patch & 0xFF00U) >> 8;
			new_ins.patch = ins->patch & 0xFFU;
			new_ins.dls = ins->dls_collection;

			rv = DmSynthInstrumentArray_add(&slf->instruments, new_ins);
			if (rv != DmResult_SUCCESS) {
				return rv;
			}

			sin = &slf->instruments.data[slf->instruments.length - 1];
		}

		// Reset the instrument's properties

		float pan = (ins->flags & DmInstrument_PAN) ? (float) ins->pan / DmInt_MIDI_MAX : DmInt_PAN_CENTER;
		float vol = (ins->flags & DmInstrument_VOLUME) ? (float) ins->volume / DmInt_MIDI_MAX : DmInt_VOLUME_MAX;

		(void) tsf_channel_set_pan(sin->synth, 0, pan);
		sin->pitch_bend_reset = DmInt_PITCH_BEND_NEUTRAL;
		sin->volume = vol;
		sin->volume_reset = vol;
		sin->pan_reset = pan;
	}

	return rv;
}

static DmResult DmSynth_assignInstrumentChannels(DmSynth* slf, DmBand* band) {
	// Calculate the number of required performance channels
	size_t channel_count = 0;
	for (size_t i = 0; i < band->instrument_count; ++i) {
		channel_count = max_usize(band->instruments[i].channel, channel_count);
	}
	channel_count += 1;

	// Increase the size of the channel array (if required)
	if (channel_count > slf->channel_count) {
		DmSynthInstrument** new_channels = Dm_alloc(sizeof(DmSynthInstrument*) * channel_count);
		if (new_channels == NULL) {
			return DmResult_MEMORY_EXHAUSTED;
		}

		if (slf->channels != NULL) {
			memcpy(new_channels, slf->channels, sizeof(DmSynthInstrument*) * slf->channel_count);
			Dm_free(slf->channels);
		}

		slf->channels = new_channels;
		slf->channel_count = channel_count;
	}

	// Clear existing channels.
	memset(slf->channels, 0, sizeof(DmSynthInstrument*) * slf->channel_count);

	// Assign the instrument to each channel.
	for (size_t i = 0; i < band->instrument_count; ++i) {
		DmInstrument* ins = &band->instruments[i];
		if (ins->dls == NULL) {
			continue;
		}

		slf->channels[ins->channel] = DmSynth_getInstrument(slf, ins);
	}

	return DmResult_SUCCESS;
}

// TODO(lmichaelis): Technically, we should change as little as possible to accommodate the new band.
//                   For example: if only the pan of an instrument changes, we should also only update that
//                   instead of reloading the entire instrument list and re-creating all TSFs.
// See also: https://documentation.help/DirectMusic/usingbands.htm
void DmSynth_sendBandUpdate(DmSynth* slf, DmBand* band) {
	if (slf == NULL || band == NULL) {
		return;
	}

	DmResult rv = DmSynth_loadInstruments(slf, band);
	if (rv != DmResult_SUCCESS) {
		return;
	}

	rv = DmSynth_assignInstrumentChannels(slf, band);
	if (rv != DmResult_SUCCESS) {
		return;
	}
}

void DmSynth_sendControl(DmSynth* slf, uint32_t channel, uint8_t control, float value) {
	if (slf == NULL || channel >= slf->channel_count) {
		return;
	}

	if (slf->channels[channel] == NULL) {
		return;
	}

	if (control == DmInt_MIDI_CC_VOLUME || control == DmInt_MIDI_CC_EXPRESSION) {
		slf->channels[channel]->volume = value;
	} else if (control == DmInt_MIDI_CC_PAN) {
		tsf_channel_set_pan(slf->channels[channel]->synth, 0, value);
	} else {
		Dm_report(DmLogLevel_WARN, "DmSynth: Control change %d is unknown.", control);
	}
}

void DmSynth_sendControlReset(DmSynth* slf, uint32_t channel, uint8_t control, float reset) {
	if (slf == NULL || channel >= slf->channel_count) {
		return;
	}

	if (slf->channels[channel] == NULL) {
		return;
	}

	if (control == DmInt_MIDI_CC_VOLUME || control == DmInt_MIDI_CC_EXPRESSION) {
		slf->channels[channel]->volume_reset = reset;
	} else if (control == DmInt_MIDI_CC_PAN) {
		slf->channels[channel]->pan_reset = reset;
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

	tsf_channel_set_pitchwheel(slf->channels[channel]->synth, 0, bend);
}

void DmSynth_sendPitchBendReset(DmSynth* slf, uint32_t channel, int reset) {
	if (slf == NULL || channel >= slf->channel_count) {
		return;
	}

	if (slf->channels[channel] == NULL) {
		return;
	}

	slf->channels[channel]->pitch_bend_reset = reset;
}

void DmSynth_sendNoteOn(DmSynth* slf, uint32_t channel, uint8_t note, uint8_t velocity) {
	if (slf == NULL || channel >= slf->channel_count) {
		return;
	}

	if (slf->channels[channel] == NULL) {
		return;
	}

	bool res = tsf_channel_note_on(slf->channels[channel]->synth, 0, note, (float) velocity / DmInt_MIDI_MAX);
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

	tsf_note_off(slf->channels[channel]->synth, 0, note);
}

void DmSynth_sendNoteOffAll(DmSynth* slf, uint32_t channel) {
	if (slf == NULL || channel >= slf->channel_count) {
		return;
	}

	if (slf->channels[channel] == NULL) {
		return;
	}

	tsf_channel_note_off_all(slf->channels[channel]->synth, 0);
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

void DmSynth_setVolume(DmSynth* slf, float vol) {
	if (slf == NULL) {
		return;
	}

	slf->volume = clamp_f32(vol, 0, 1);
}

size_t DmSynth_render(DmSynth* slf, void* buf, size_t len, DmRenderOptions fmt) {
	for (size_t i = 0; i < slf->instruments.length; ++i) {
		DmSynthInstrument* ins = &slf->instruments.data[i];

		int channels = (fmt & DmRender_STEREO) ? 2 : 1;
		if (fmt & DmRender_STEREO) {
			tsf_set_output(ins->synth, TSF_STEREO_INTERLEAVED, 44100, 0);
		} else {
			tsf_set_output(ins->synth, TSF_MONO, 44100, 0);
		}

		float vol = ins->volume * slf->volume;

		if (fmt & DmRender_FLOAT) {
			tsf_render_float(ins->synth, buf, (int) len / channels, i != 0, vol);
		} else {
			tsf_render_short(ins->synth, buf, (int) len / channels, i != 0, vol);
		}
	}

	return fmt & DmRender_FLOAT ? len * 4 : len * 2;
}
