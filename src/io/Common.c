// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"

#include <wchar.h>

void DmGuid_parse(DmGuid* slf, DmRiff* rif) {
	DmRiff_read(rif, slf->data, sizeof slf->data);
}

char* Dm_utf16ToUtf8Inline(char* out, uint16_t const* u16) {
	size_t len = 0;
	while (u16[len] != 0) {
		len += 1;
	}

	mbstate_t state;
	memset(&state, 0, sizeof state);

	size_t i = 0;
	size_t j = 0;
	for (; i < len; ++i) {
		j += wcrtomb(out + j, u16[i], &state);
	}

	out[j] = '\0';
	return out;
}

void DmUnfo_parse(DmUnfo* slf, DmRiff* rif) {
	DmRiff cnk;
	while (DmRiff_readChunk(rif, &cnk)) {
		if (DmRiff_is(&cnk, DM_FOURCC_UNAM, 0)) {
			uint16_t* raw = DmRiff_readStringUtf(&cnk);
			slf->unam = Dm_utf16ToUtf8Inline((char*) raw, raw);
		} else {
			DmRiff_reportDone(&cnk);
		}
	}
}

void DmInfo_parse(DmInfo* slf, DmRiff* rif) {
	DmRiff cnk;
	while (DmRiff_readChunk(rif, &cnk)) {
		if (DmRiff_is(&cnk, DM_FOURCC_INAM, 0)) {
			slf->inam = DmRiff_readString(&cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_ICMT, 0)) {
			slf->icmt = DmRiff_readString(&cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_ICOP, 0)) {
			slf->icop = DmRiff_readString(&cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_IENG, 0)) {
			slf->ieng = DmRiff_readString(&cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_ISBJ, 0)) {
			slf->isbj = DmRiff_readString(&cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_ISFT, 0)) {
			slf->isft = DmRiff_readString(&cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_DATE, 0)) {
			slf->date = DmRiff_readString(&cnk);
		} else {
			DmRiff_reportDone(&cnk);
		}
	}
}

void DmVersion_parse(DmVersion* slf, DmRiff* rif) {
	DmRiff_readDword(rif, &slf->ms);
	DmRiff_readDword(rif, &slf->ls);
}

void DmReference_parse(DmReference* slf, DmRiff* rif) {
	DmRiff cnk;
	while (DmRiff_readChunk(rif, &cnk)) {
		if (DmRiff_is(&cnk, DM_FOURCC_REFH, 0)) {
			DmGuid_parse(&slf->class_id, &cnk);
			DmRiff_readDword(&cnk, &slf->valid_data);
		} else if (DmRiff_is(&cnk, DM_FOURCC_GUID, 0)) {
			DmGuid_parse(&slf->guid, &cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_NAME, 0)) {
			uint16_t* raw = DmRiff_readStringUtf(&cnk);
			slf->name = Dm_utf16ToUtf8Inline((char*) raw, raw);
			continue; // Ignore following bytes
		} else if (DmRiff_is(&cnk, DM_FOURCC_FILE, 0)) {
			uint16_t* raw = DmRiff_readStringUtf(&cnk);
			slf->file = Dm_utf16ToUtf8Inline((char*) raw, raw);
			continue; // Ignore following bytes
		} else if (DmRiff_is(&cnk, DM_FOURCC_VERS, 0)) {
			DmVersion_parse(&slf->version, &cnk);
		}

		DmRiff_reportDone(&cnk);
	}
}

void DmTimeSignature_parse(DmTimeSignature* slf, DmRiff* rif) {
	DmRiff_readByte(rif, &slf->beats_per_measure);
	DmRiff_readByte(rif, &slf->beat);
	DmRiff_readWord(rif, &slf->grids_per_beat);
}
