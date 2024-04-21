// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"

DmResult DmSegment_parse(DmSegment* slf, void* buf, size_t len) {
	DmRiff rif;
	if (!DmRiff_init(&rif, buf, len)) {
		Dm_free(buf);
		return DmResult_FILE_CORRUPT;
	}

	slf->backing_memory = buf;

	DmRiff cnk;
	while (DmRiff_readChunk(&rif, &cnk)) {
		if (DmRiff_is(&cnk, DM_FOURCC_SEGH, 0)) {
			DmRiff_readDword(&cnk, &slf->repeats);
			DmRiff_readDword(&cnk, &slf->length);
			DmRiff_readDword(&cnk, &slf->play_start);
			DmRiff_readDword(&cnk, &slf->loop_start);
			DmRiff_readDword(&cnk, &slf->loop_end);
			DmRiff_readDword(&cnk, &slf->resolution);
		} else if (DmRiff_is(&cnk, DM_FOURCC_GUID, 0)) {
			DmGuid_parse(&slf->guid, &cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_VERS, 0)) {
			DmVersion_parse(&slf->version, &cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_UNFO)) {
			DmUnfo_parse(&slf->info, &cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_TRKL)) {
			DmTrackList_parse(&slf->messages, &cnk);
		}

		DmRiff_reportDone(&cnk);
	}

	return DmResult_SUCCESS;
}
