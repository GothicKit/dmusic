// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"

void DmBand_init(DmBand* slf) {
	if (slf == NULL) {
		Dm_report(DmLogLevel_ERROR, "DmBand: Internal error: DmBand_init called with a `NULL` pointer");
		return;
	}

	memset(slf, 0, sizeof *slf);
	DmInstrumentList_init(&slf->instruments);
}

void DmBand_free(DmBand* slf) {
	if (slf == NULL) {
		Dm_report(DmLogLevel_WARN, "DmBand: Internal error: DmBand_free called with a `NULL` pointer");
		return;
	}

	DmInstrumentList_free(&slf->instruments);
}

DmResult DmBand_download(DmBand* slf, DmLoader* loader) {
	if (slf == NULL || loader == NULL) {
		return DmResult_INVALID_ARGUMENT;
	}

	DmResult rv = DmResult_SUCCESS;
	for (size_t i = 0; i < slf->instruments.length; ++i) {
		DmInstrument* instrument = slf->instruments.data + i;

		// The DLS has already been downloaded. We don't need to do it again.
		if (instrument->dls != NULL) {
			continue;
		}

		// If the patch is not valid, this instrument cannot be played since we don't know
		// where to find it in the DLS collection.
		if (!(instrument->flags & DmInstrument_PATCH)) {
			Dm_report(DmLogLevel_TRACE, "DmBand: Not downloading instrument without valid patch.");
			continue;
		}

		// TODO(lmichaelis): The General MIDI and Roland GS collections are not supported.
		if (instrument->flags & (DmInstrument_GS | DmInstrument_GM)) {
			Dm_report(DmLogLevel_INFO, "DmBand: Cannot download instrument: GS and GM collection not available");
			continue;
		}

		rv = DmLoader_getDownloadableSound(loader, &instrument->reference, &instrument->dls);
		if (rv != DmResult_SUCCESS) {
			break;
		}
	}

	return rv;
}

void DmInstrument_free(DmInstrument* slf) {
	if (slf == NULL || slf->dls == NULL) {
		return;
	}

	DmDls_release(slf->dls);
	slf->dls = NULL;
}
