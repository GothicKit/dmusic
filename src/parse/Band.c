// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"

static void DmInstrument_parse(DmInstrument* slf, DmRiff* rif) {
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
			DmRiff_readDword(&cnk, &slf->flags);
			DmRiff_readByte(&cnk, &slf->pan);
			DmRiff_readByte(&cnk, &slf->volume);
			DmRiff_readShort(&cnk, &slf->transpose);
			DmRiff_readDword(&cnk, &slf->channel_priority);
		} else if (DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_DMRF)) {
			DmReference_parse(&slf->reference, &cnk);
		}

		DmRiff_reportDone(&cnk);
	}
}

static DmResult DmInstrumentList_parse(DmInstrumentList* slf, DmRiff* rif) {
	DmInstrument instrument;

	DmRiff cnk;
	while (DmRiff_readChunk(rif, &cnk)) {
		if (DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_LBIN)) {
			memset(&instrument, 0, sizeof instrument);

			DmInstrument_parse(&instrument, &cnk);

			// DmArray_add can result in a memory allocation failure which we need to catch.
			DmResult rv = DmInstrumentList_add(slf, instrument);
			if (rv != DmResult_SUCCESS) {
				return rv;
			}
		}

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
			DmResult rv = DmInstrumentList_parse(&slf->instruments, &cnk);
			if (rv != DmResult_SUCCESS) {
				return rv;
			}
		}

		DmRiff_reportDone(&cnk);
	}

	return DmResult_SUCCESS;
}
