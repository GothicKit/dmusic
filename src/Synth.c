// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"

void DmSynth_init(DmSynth* slf) {
	if (slf == NULL) {
		return;
	}

	memset(slf, 0, sizeof *slf);
}

void DmSynth_free(DmSynth* slf) {
	if (slf == NULL) {
		return;
	}
}

void DmSynth_sendBandUpdate(DmSynth* slf, DmBand* band) {

}

void DmSynth_sendControl(DmSynth* slf, uint32_t channel, uint8_t control, uint32_t value) {

}

void DmSynth_sendNoteOn(DmSynth* slf, uint32_t channel, uint8_t note, uint8_t velocity) {

}

void DmSynth_sendNoteOff(DmSynth* slf, uint32_t channel, uint8_t note){

}

void DmSynth_sendNoteOffAll(DmSynth* slf, uint32_t channel) {

}

void DmSynth_render(DmSynth* slf, void* buf, size_t len, DmSynthFormat fmt) {

}
