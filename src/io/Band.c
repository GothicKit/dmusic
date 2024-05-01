// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"

enum {
	DmBand_MAX_PAN = 127,
	DmBand_MAX_VOLUME = 127,
};

static void DmBand_parseInstrument(DmInstrument* slf, DmRiff* rif) {
	DmRiff cnk;
	while (DmRiff_readChunk(rif, &cnk)) {
		if (DmRiff_is(&cnk, DM_FOURCC_BINS, 0)) {
			DmRiff_readDword(&cnk, &slf->patch);
			DmRiff_readDword(&cnk, &slf->assign_patch);
			DmRiff_readDword(&cnk, &slf->note_ranges[0]);
			DmRiff_readDword(&cnk, &slf->note_ranges[1]);
			DmRiff_readDword(&cnk, &slf->note_ranges[2]);
			DmRiff_readDword(&cnk, &slf->note_ranges[3]);
			DmRiff_readDword(&cnk, &slf->channel);
			DmRiff_readDword(&cnk, &slf->options);
			DmRiff_readByte(&cnk, &slf->pan);
			DmRiff_readByte(&cnk, &slf->volume);
			DmRiff_readShort(&cnk, &slf->transpose);
			DmRiff_readDword(&cnk, &slf->channel_priority);

			slf->pan = min_u8(slf->pan, DmBand_MAX_PAN);
			slf->volume = min_u8(slf->volume, DmBand_MAX_VOLUME);
		} else if (DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_DMRF)) {
			DmReference_parse(&slf->reference, &cnk);
		}

		DmRiff_reportDone(&cnk);
	}
}

static DmResult DmBand_parseInstrumentList(DmBand* slf, DmRiff* rif) {
	slf->instruments_len = DmRiff_chunks(rif);
	slf->instruments = Dm_alloc(slf->instruments_len * sizeof(DmInstrument));
	if (slf->instruments == NULL) {
		return DmResult_MEMORY_EXHAUSTED;
	}

	DmRiff cnk;
	for (size_t i = 0; i < slf->instruments_len; ++i) {
		if (!DmRiff_readChunk(rif, &cnk)) {
			return DmResult_FILE_CORRUPT;
		}

		if (!DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_LBIN)) {
			return DmResult_FILE_CORRUPT;
		}

		DmBand_parseInstrument(&slf->instruments[i], &cnk);
		DmRiff_reportDone(&cnk);
	}

	return DmResult_SUCCESS;
}

DmResult DmBand_parse(DmBand* slf, DmRiff* rif) {
	DmRiff cnk;
	while (DmRiff_readChunk(rif, &cnk)) {
		if (DmRiff_is(&cnk, DM_FOURCC_GUID, 0)) {
			DmGuid_parse(&slf->guid, &cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_UNFO)) {
			DmUnfo_parse(&slf->info, &cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_LBIL)) {
			DmResult rv = DmBand_parseInstrumentList(slf, &cnk);
			if (rv != DmResult_SUCCESS) {
				return rv;
			}
		}

		DmRiff_reportDone(&cnk);
	}

	return DmResult_SUCCESS;
}
