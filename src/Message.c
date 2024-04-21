// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"

void DmMessage_copy(DmMessage* slf, DmMessage* cpy) {
	if (slf == NULL || cpy == NULL) {
		return;
	}

	memcpy(cpy, slf, sizeof *slf);

	if (slf->type == DmMessage_BAND) {
		cpy->band.band = DmBand_retain(slf->band.band);
	} else if (slf->type == DmMessage_STYLE) {
		cpy->style.style = DmStyle_retain(slf->style.style);
	}
}

void DmMessage_free(DmMessage* slf) {
	if (slf == NULL) {
		return;
	}

	if (slf->type == DmMessage_BAND) {
		DmBand_release(slf->band.band);
	} else if (slf->type == DmMessage_STYLE) {
		DmStyle_release(slf->style.style);
	}
}
