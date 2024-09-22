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

	for (size_t i = 0; i < slf->instruments_len; ++i) {
		DmInstrument_free(&slf->instruments[i]);
	}

	Dm_free(slf->instruments);
	Dm_free(slf);
}

DmDlsInstrument* DmInstrument_getDlsInstrument(DmInstrument* slf) {
	if (slf == NULL || slf->dls == NULL) {
		return NULL;
	}

	uint32_t bank = (slf->patch & 0xFF00U) >> 8;
	uint32_t patch = slf->patch & 0xFFU;

	DmDlsInstrument* ins = NULL;
	for (long long i = slf->dls->instrument_count; i >= 0; --i) {
		ins = &slf->dls->instruments[i];

		// If it's the correct instrument, return it.
		// TODO(lmichaelis): Dirty fix for drum kit problems. Instead of choosing the first valid DLS instrument, we
		//                   chose the last valid one. This acts as if later instruments override previous ones and
		//                   thus prevents problems where the same channel is re-used multiple times, specifically in
		//                   Gothic 1, which assigns a drum kit and melodic instrument to the same channel. This works
		//                   in conjunction with the TSF creation in Dm_createHydra to prevent drum kit issues in G1.
		if ((ins->bank & 127) == bank && ins->patch == patch) {
			return ins;
		}
	}

	Dm_report(DmLogLevel_WARN,
	          "DmBand: Instrument patch %d:%d not found in band '%s'",
	          bank,
	          patch,
	          slf->reference.name);
	return NULL;
}

DmResult DmBand_download(DmBand* slf, DmLoader* loader) {
	if (slf == NULL || loader == NULL) {
		return DmResult_INVALID_ARGUMENT;
	}

	Dm_report(DmLogLevel_INFO, "DmBand: Downloading instruments for band '%s'", slf->info.unam);

	DmResult rv = DmResult_SUCCESS;
	for (size_t i = 0; i < slf->instruments_len; ++i) {
		DmInstrument* instrument = &slf->instruments[i];

		// The DLS has already been downloaded. We don't need to do it again.
		if (instrument->dls != NULL) {
			continue;
		}

		// If the patch is not valid, this instrument cannot be played since we don't know
		// where to find it in the DLS collection.
		if (!(instrument->options & (DmInstrument_VALID_PATCH | DmInstrument_VALID_BANKSELECT))) {
			Dm_report(DmLogLevel_DEBUG,
			          "DmBand: Not downloading instrument '%s' without valid patch",
			          instrument->reference.name);
			continue;
		}

		// TODO(lmichaelis): The General MIDI, Roland GS and Yamaha XG collections are not supported.
		if (instrument->options & DmInstrument_PREDEFINED_COLLECTION) {
			Dm_report(DmLogLevel_INFO,
			          "DmBand: Cannot download instrument '%s': GM, GS and XG collections not available",
			          instrument->reference.name);
			continue;
		}

		rv = DmLoader_getDownloadableSound(loader, &instrument->reference, &instrument->dls);
		if (rv != DmResult_SUCCESS || instrument->dls == NULL) {
			continue;
		}

		DmDlsInstrument* dls_instrument = DmInstrument_getDlsInstrument(instrument);
		if (dls_instrument == NULL) {
			continue;
		}

		Dm_report(DmLogLevel_DEBUG,
		          "DmBand: DLS instrument '%s' assigned to channel %d for band '%s'",
		          dls_instrument->info.inam,
		          instrument->channel,
		          slf->info.unam);
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
