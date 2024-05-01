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

void DmSynth_init(DmSynth* slf, uint32_t sample_rate) {
	if (slf == NULL) {
		return;
	}

	memset(slf, 0, sizeof *slf);

	slf->rate = sample_rate;
	DmSynthFontArray_init(&slf->fonts);
}

void DmSynth_free(DmSynth* slf) {
	if (slf == NULL) {
		return;
	}

	Dm_free(slf->channels);
	DmSynthFontArray_free(&slf->fonts);
}

void DmSynth_reset(DmSynth* slf) {
	if (slf == NULL) {
		return;
	}

	for (uint32_t i = 0; i < slf->channels_len; ++i) {
		DmSynthChannel* chan = &slf->channels[i];
		if (chan->font == NULL) {
			continue;
		}

		tsf_channel_set_volume(chan->font->syn, chan->channel, chan->reset_volume);
		tsf_channel_set_pan(chan->font->syn, chan->channel, chan->reset_pan);
		tsf_channel_set_pitchwheel(chan->font->syn, chan->channel, chan->reset_pitch);
	}
}

static DmSynthFont* DmSynth_getFont(DmSynth* slf, DmInstrument* ins) {
	for (size_t i = 0; i < slf->fonts.length; ++i) {
		if (slf->fonts.data[i].dls == ins->dls) {
			return &slf->fonts.data[i];
		}
	}

	return NULL;
}

static DmResult DmSynth_updateFonts(DmSynth* slf, DmBand* band) {
	DmResult rv = DmResult_SUCCESS;
	for (size_t i = 0; i < band->instruments_len; ++i) {
		DmInstrument* ins = &band->instruments[i];
		if (ins->dls == NULL) {
			continue;
		}

		DmSynthFont* fnt = DmSynth_getFont(slf, ins);

		// The instrument font does not yet exist. Create it anew!
		if (fnt == NULL) {
			DmSynthFont new_fnt;
			new_fnt.dls = ins->dls;

			rv = DmSynth_createTsfForDls(ins->dls, &new_fnt.syn);
			if (rv != DmResult_SUCCESS) {
				continue;
			}

			tsf_set_output(new_fnt.syn, TSF_STEREO_INTERLEAVED, slf->rate, 0);

			rv = DmSynthFontArray_add(&slf->fonts, new_fnt);
			if (rv != DmResult_SUCCESS) {
				return rv;
			}
		}
	}

	return DmResult_SUCCESS;
}

static DmResult DmSynth_assignInstrumentChannels(DmSynth* slf, DmBand* band) {
	// Calculate the number of required performance channels
	size_t channel_count = 0;
	for (size_t i = 0; i < band->instruments_len; ++i) {
		channel_count = max_usize(band->instruments[i].channel, channel_count);
	}
	channel_count += 1;

	// Increase the size of the channel array (if required)
	if (channel_count > slf->channels_len) {
		DmSynthChannel* new_channels = Dm_alloc(sizeof(DmSynthChannel) * channel_count);
		if (new_channels == NULL) {
			return DmResult_MEMORY_EXHAUSTED;
		}

		if (slf->channels != NULL) {
			memcpy(new_channels, slf->channels, sizeof(DmSynthChannel) * slf->channels_len);
			Dm_free(slf->channels);
		}

		slf->channels = new_channels;
		slf->channels_len = channel_count;
	}

	// Clear existing channels.
	memset(slf->channels, 0, sizeof(DmSynthChannel*) * slf->channels_len);

	// Assign the instrument to each channel.
	for (size_t i = 0; i < band->instruments_len; ++i) {
		DmInstrument* ins = &band->instruments[i];
		if (ins->dls == NULL) {
			continue;
		}

		DmSynthFont* fnt = DmSynth_getFont(slf, ins);
		slf->channels[ins->channel].font = fnt;
		slf->channels[ins->channel].channel = ins->channel;
		slf->channels[ins->channel].reset_volume = DmInt_VOLUME_MAX;
		slf->channels[ins->channel].reset_pan = DmInt_PAN_CENTER;
		slf->channels[ins->channel].reset_pitch = DmInt_PITCH_BEND_NEUTRAL;
		slf->channels[ins->channel].transpose = 0;

		if (fnt == NULL) {
			continue;
		}

		uint32_t bank = (ins->patch & 0xFF00U) >> 8;
		uint32_t patch = ins->patch & 0xFFU;

		tsf_channel_set_bank_preset(fnt->syn, ins->channel, bank, patch);

		// Set the instrument's properties
		if (ins->options & DmInstrument_VALID_PAN) {
			tsf_channel_set_pan(fnt->syn, ins->channel, (float) ins->pan / (float) DmInt_MIDI_MAX);
			slf->channels[ins->channel].reset_pan = (float) ins->pan / (float) DmInt_MIDI_MAX;
		}

		if (ins->options & DmInstrument_VALID_VOLUME) {
			tsf_channel_set_volume(fnt->syn, ins->channel, (float) ins->volume / DmInt_MIDI_MAX);
			slf->channels[ins->channel].reset_volume = (float) ins->volume / (float) DmInt_MIDI_MAX;
		}

		if (ins->options & DmInstrument_VALID_TRANSPOSE) {
			slf->channels[ins->channel].transpose = ins->transpose;
		}
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

	DmResult rv = DmSynth_updateFonts(slf, band);
	if (rv != DmResult_SUCCESS) {
		Dm_free(slf->channels);
		slf->channels = NULL;
		slf->channels_len = 0;
		return;
	}

	rv = DmSynth_assignInstrumentChannels(slf, band);
	if (rv != DmResult_SUCCESS) {
		return;
	}
}

void DmSynth_sendControl(DmSynth* slf, uint32_t channel, uint8_t control, float value) {
	if (slf == NULL || channel >= slf->channels_len) {
		return;
	}

	DmSynthChannel* chan = &slf->channels[channel];
	if (chan->font == NULL) {
		return;
	}

	if (control == DmInt_MIDI_CC_VOLUME || control == DmInt_MIDI_CC_EXPRESSION) {
		tsf_channel_set_volume(chan->font->syn, chan->channel, value);
	} else if (control == DmInt_MIDI_CC_PAN) {
		tsf_channel_set_pan(chan->font->syn, chan->channel, value);
	} else {
		Dm_report(DmLogLevel_WARN, "DmSynth: Control change %d is unknown.", control);
	}
}

void DmSynth_sendControlReset(DmSynth* slf, uint32_t channel, uint8_t control, float reset) {
	if (slf == NULL || channel >= slf->channels_len) {
		return;
	}

	DmSynthChannel* chan = &slf->channels[channel];
	if (chan->font == NULL) {
		return;
	}

	if (control == DmInt_MIDI_CC_VOLUME || control == DmInt_MIDI_CC_EXPRESSION) {
		chan->reset_volume = reset;
	} else if (control == DmInt_MIDI_CC_PAN) {
		chan->reset_pan = reset;
	} else {
		Dm_report(DmLogLevel_WARN, "DmSynth: Control change %d is unknown.", control);
	}
}

void DmSynth_sendPitchBend(DmSynth* slf, uint32_t channel, int bend) {
	if (slf == NULL || channel >= slf->channels_len) {
		return;
	}

	DmSynthChannel* chan = &slf->channels[channel];
	if (chan->font == NULL) {
		return;
	}

	tsf_channel_set_pitchwheel(chan->font->syn, chan->channel, bend);
}

void DmSynth_sendPitchBendReset(DmSynth* slf, uint32_t channel, int reset) {
	if (slf == NULL || channel >= slf->channels_len) {
		return;
	}

	DmSynthChannel* chan = &slf->channels[channel];
	if (chan->font == NULL) {
		return;
	}

	chan->reset_pitch = reset;
}

void DmSynth_sendNoteOn(DmSynth* slf, uint32_t channel, uint8_t note, uint8_t velocity) {
	if (slf == NULL || channel >= slf->channels_len) {
		return;
	}

	DmSynthChannel* chan = &slf->channels[channel];
	if (chan->font == NULL) {
		return;
	}

	bool res =
	    tsf_channel_note_on(chan->font->syn, chan->channel, note + chan->transpose, (float) velocity / DmInt_MIDI_MAX);
	if (!res) {
		Dm_report(DmLogLevel_ERROR, "DmSynth: DmSynth_sendNoteOn encountered an error.");
	}
}

void DmSynth_sendNoteOff(DmSynth* slf, uint32_t channel, uint8_t note) {
	if (slf == NULL || channel >= slf->channels_len) {
		return;
	}

	DmSynthChannel* chan = &slf->channels[channel];
	if (chan->font == NULL) {
		return;
	}

	tsf_channel_note_off(chan->font->syn, chan->channel, note);
}

void DmSynth_sendNoteOffAll(DmSynth* slf, uint32_t channel) {
	if (slf == NULL || channel >= slf->channels_len) {
		return;
	}

	DmSynthChannel* chan = &slf->channels[channel];
	if (chan->font == NULL) {
		return;
	}

	tsf_channel_note_off_all(chan->font->syn, chan->channel);
}

void DmSynth_sendNoteOffEverything(DmSynth* slf) {
	if (slf == NULL) {
		return;
	}

	for (uint32_t i = 0; i < slf->channels_len; ++i) {
		DmSynth_sendNoteOffAll(slf, i);
	}
}

void DmSynth_setVolume(DmSynth* slf, float vol) {
	if (slf == NULL) {
		return;
	}

	for (size_t i = 0; i < slf->fonts.length; ++i) {
		tsf_set_volume(slf->fonts.data[i].syn, vol);
	}
}

size_t DmSynth_render(DmSynth* slf, void* buf, size_t len, DmRenderOptions fmt) {
	int channels = (fmt & DmRender_STEREO) ? 2 : 1;
	for (size_t i = 0; i < slf->fonts.length; ++i) {
		DmSynthFont* fnt = &slf->fonts.data[i];

		if (fmt & DmRender_STEREO) {
			fnt->syn->outputmode = TSF_STEREO_INTERLEAVED;
		} else {
			fnt->syn->outputmode = TSF_MONO;
		}

		if (fmt & DmRender_FLOAT) {
			tsf_render_float(fnt->syn, buf, (int) len / channels, i != 0, 1);
		} else {
			tsf_render_short(fnt->syn, buf, (int) len / channels, i != 0, 1);
		}
	}

	return fmt & DmRender_FLOAT ? len * 4 : len * 2;
}
