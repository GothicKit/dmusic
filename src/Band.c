// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"

DmResult DmBand_create(DmBand** slf) {
	if (slf == NULL) {
		return DmResult_INVALID_ARGUMENT;
	}

	DmBand* new = *slf = Dm_alloc(sizeof *new);
	if (new == NULL) {
		return DmResult_MEMORY_EXHAUSTED;
	}

	new->reference_count = 1;
	return DmResult_SUCCESS;
}

DmBand* DmBand_retain(DmBand* slf) {
	if (slf == NULL) {
		return NULL;
	}

	(void) atomic_fetch_add(&slf->reference_count, 1);
	return slf;
}

void DmBand_release(DmBand* slf) {
	if (slf == NULL) {
		return;
	}

	size_t count = atomic_fetch_sub(&slf->reference_count, 1) - 1;
	if (count > 0) {
		return;
	}

	for (size_t i = 0; i < slf->instrument_count; ++i) {
		DmInstrument_free(&slf->instruments[i]);
	}

	Dm_free(slf->instruments);
	Dm_free(slf);
}

DmResult DmBand_download(DmBand* slf, DmLoader* loader) {
	if (slf == NULL || loader == NULL) {
		return DmResult_INVALID_ARGUMENT;
	}

	DmResult rv = DmResult_SUCCESS;
	for (size_t i = 0; i < slf->instrument_count; ++i) {
		DmInstrument* instrument = &slf->instruments[i];

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
