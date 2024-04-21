// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"

static DmResult DmTempoTrack_parse(DmMessageList* slf, DmRiff* rif) {
	uint32_t item_size = 0;
	DmRiff_readDword(rif, &item_size);

	DmMessage msg;
	uint32_t item_count = (rif->len - rif->pos) / item_size;
	for (uint32_t i = 0; i < item_count; ++i) {
		uint32_t end_position = rif->pos + item_size;

		memset(&msg, 0, sizeof msg);
		msg.type = DmMessage_TEMPO;

		DmRiff_readDword(rif, &msg.time);
		DmRiff_readDouble(rif, &msg.tempo.tempo);

		DmResult rv = DmMessageList_add(slf, msg);
		if (rv != DmResult_SUCCESS) {
			return rv;
		}

		rif->pos = end_position;
	}

	return DmResult_SUCCESS;
}

static void DmCommandItem_parse(DmMessage_Command* slf, DmRiff* rif) {
	memset(slf, 0, sizeof *slf);
	slf->type = DmMessage_COMMAND;

	DmRiff_readDword(rif, &slf->time);
	DmRiff_readWord(rif, &slf->measure);
	DmRiff_readByte(rif, &slf->beat);

	uint8_t tmp = 0;
	DmRiff_readByte(rif, &tmp);
	slf->command = tmp;

	DmRiff_readByte(rif, &slf->groove_level);
	DmRiff_readByte(rif, &slf->groove_range);

	DmRiff_readByte(rif, &tmp);
	slf->repeat_mode = tmp;

	// Groove range values above 100 are not allowed. Set it to 0 instead.
	if (slf->groove_range > 100) {
		slf->groove_range = 0;
	}

	// In DirectMusic versions before DirectX 8, the repetition mode was always `RANDOM`, thus
	// this byte is just padding. Just set it to `RANDOM` if it exceeds the maximum allowed value.
	if (slf->repeat_mode > DmPatternSelect_RANDOM_ROW) {
		slf->repeat_mode = DmPatternSelect_RANDOM;
	}
}

static DmResult DmCommandTrack_parse(DmMessageList* slf, DmRiff* rif) {
	uint32_t item_size = 0;
	DmRiff_readDword(rif, &item_size);

	DmMessage msg;
	uint32_t item_count = (rif->len - rif->pos) / item_size;
	for (uint32_t i = 0; i < item_count; ++i) {
		uint32_t end_position = rif->pos + item_size;

		DmCommandItem_parse(&msg.command, rif);

		DmResult rv = DmMessageList_add(slf, msg);
		if (rv != DmResult_SUCCESS) {
			return rv;
		}

		rif->pos = end_position;
	}

	return DmResult_SUCCESS;
}

static void DmChord_parse(DmMessage_Chord* slf, DmRiff* rif) {
	memset(slf, 0, sizeof *slf);
	slf->type = DmMessage_CHORD;

	uint32_t item_size = 0;
	DmRiff_readDword(rif, &item_size);

	// First, parse DMUS_IO_CHORD.
	{
		uint32_t end_position = rif->pos + item_size;

		DmRiff_read(rif, slf->name, sizeof slf->name);
		DmRiff_readDword(rif, &slf->time);
		DmRiff_readWord(rif, &slf->measure);
		DmRiff_readByte(rif, &slf->beat);

		uint8_t silent = 0;
		DmRiff_readByte(rif, &silent);
		slf->silent = silent == 1;

		rif->pos = end_position;
	}

	// Second, parse a list of sub-chords (4 max.)
	DmRiff_readDword(rif, &slf->subchord_count);

	uint32_t max_subchord_count = (sizeof slf->subchords) / (sizeof *slf->subchords);
	if (slf->subchord_count > max_subchord_count) {
		Dm_report(DmLogLevel_ERROR,
				  "DmMessage: Chord message reports too many sub-chords: got %d, expected at maximum %d",
				  slf->subchord_count,
				  max_subchord_count);
		slf->subchord_count = max_subchord_count;
	}

	DmRiff_readDword(rif, &item_size);

	for (uint32_t i = 0; i < slf->subchord_count; ++i) {
		uint32_t end_position = rif->pos + item_size;

		DmRiff_readDword(rif, &slf->subchords[i].chord_pattern);
		DmRiff_readDword(rif, &slf->subchords[i].scale_pattern);
		DmRiff_readDword(rif, &slf->subchords[i].inversion_points);
		DmRiff_readDword(rif, &slf->subchords[i].levels);
		DmRiff_readByte(rif, &slf->subchords[i].chord_root);
		DmRiff_readByte(rif, &slf->subchords[i].scale_root);

		rif->pos = end_position;
	}
}

static DmResult DmChordTrack_parse(DmMessageList* slf, DmRiff* rif) {
	uint32_t crdh = 0;

	DmMessage msg;
	DmRiff cnk;
	while (DmRiff_readChunk(rif, &cnk)) {
		if (DmRiff_is(&cnk, DM_FOURCC_CRDH, 0)) {
			DmRiff_readDword(&cnk, &crdh);
		} else if (DmRiff_is(&cnk, DM_FOURCC_CRDB, 0)) {
			DmChord_parse(&msg.chord, &cnk);

			DmResult rv = DmMessageList_add(slf, msg);
			if (rv != DmResult_SUCCESS) {
				return rv;
			}
		}

		DmRiff_reportDone(&cnk);
	}

	return DmResult_SUCCESS;
}

static DmResult DmBandItem_parse(DmMessage_Band* slf, DmRiff* rif) {
	memset(slf, 0, sizeof *slf);
	slf->type = DmMessage_BAND;

	DmRiff cnk;
	while (DmRiff_readChunk(rif, &cnk)) {
		if (DmRiff_is(&cnk, DM_FOURCC_BDIH, 0)) {
			DmRiff_readDword(&cnk, &slf->time);
		} else if (DmRiff_is(&cnk, DM_FOURCC_RIFF, DM_FOURCC_DMBD)) {
			DmBand_init(&slf->band);
			DmResult rv = DmBand_parse(&slf->band, &cnk);
			if (rv != DmResult_SUCCESS) {
				DmBand_free(&slf->band);
				return rv;
			}
		}

		DmRiff_reportDone(&cnk);
	}

	return DmResult_SUCCESS;
}

static DmResult DmBandList_parse(DmMessageList* slf, DmRiff* rif) {
	DmMessage msg;
	DmRiff cnk;
	while (DmRiff_readChunk(rif, &cnk)) {
		if (DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_LBND)) {
			DmResult rv = DmBandItem_parse(&msg.band, &cnk);
			if (rv != DmResult_SUCCESS) {
				return rv;
			}

			rv = DmMessageList_add(slf, msg);
			if (rv != DmResult_SUCCESS) {
				return rv;
			}
		}

		DmRiff_reportDone(&cnk);
	}

	return DmResult_SUCCESS;
}


static DmResult DmBandTrack_parse(DmMessageList* slf, DmRiff* rif) {
	DmRiff cnk;
	while (DmRiff_readChunk(rif, &cnk)) {
		if (DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_LBDL)) {
			DmResult rv = DmBandList_parse(slf, &cnk);
			if (rv != DmResult_SUCCESS) {
				return rv;
			}
		}

		DmRiff_reportDone(&cnk);
	}

	return DmResult_SUCCESS;
}

static void DmStyleItem_parse(DmMessage_Style* slf, DmRiff* rif) {
	memset(slf, 0, sizeof *slf);
	slf->type = DmMessage_STYLE;

	DmRiff cnk;
	while (DmRiff_readChunk(rif, &cnk)) {
		if (DmRiff_is(&cnk, DM_FOURCC_STMP, 0)) {
			DmRiff_readDword(&cnk, &slf->time);
		} else if (DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_DMRF)) {
			DmReference_parse(&slf->reference, &cnk);
		}

		DmRiff_reportDone(&cnk);
	}
}

static DmResult DmStyleTrack_parse(DmMessageList* slf, DmRiff* rif) {
	DmMessage msg;
	DmRiff cnk;
	while (DmRiff_readChunk(rif, &cnk)) {
		if (DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_STRF)) {
			DmStyleItem_parse(&msg.style, &cnk);

			DmResult rv = DmMessageList_add(slf, msg);
			if (rv != DmResult_SUCCESS) {
				return rv;
			}
		}

		DmRiff_reportDone(&cnk);
	}

	return DmResult_SUCCESS;
}

static DmResult DmTrack_parse(DmMessageList* slf, DmRiff* rif) {
	DmGuid class_id;
	uint32_t position;
	uint32_t group;
	uint32_t chunk_id;
	uint32_t chunk_type;

	DmRiff cnk;
	while (DmRiff_readChunk(rif, &cnk)) {
		DmResult rv = DmResult_SUCCESS;
		if (DmRiff_is(&cnk, DM_FOURCC_TRKH, 0)) {
			DmGuid_parse(&class_id, &cnk);
			DmRiff_readDword(&cnk, &position);
			DmRiff_readDword(&cnk, &group);
			DmRiff_readDword(&cnk, &chunk_id);
			DmRiff_readDword(&cnk, &chunk_type);
		} else if (DmRiff_is(&cnk, DM_FOURCC_TETR, 0)) {
			rv = DmTempoTrack_parse(slf, &cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_CMND, 0)) {
			rv = DmCommandTrack_parse(slf, &cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_CORD)) {
			rv = DmChordTrack_parse(slf, &cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_STTR)) {
			rv = DmStyleTrack_parse(slf, &cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_RIFF, DM_FOURCC_DMBT)) {
			rv = DmBandTrack_parse(slf, &cnk);
		}

		if (rv != DmResult_SUCCESS) {
			return rv;
		}

		DmRiff_reportDone(&cnk);
	}

	return DmResult_SUCCESS;
}

DmResult DmTrackList_parse(DmMessageList* slf, DmRiff* rif) {
	DmRiff cnk;
	while (DmRiff_readChunk(rif, &cnk)) {
		if (DmRiff_is(&cnk, DM_FOURCC_RIFF, DM_FOURCC_DMTK)) {
			DmResult rv = DmTrack_parse(slf, &cnk);
			if (rv != DmResult_SUCCESS) {
				return rv;
			}
		}

		DmRiff_reportDone(&cnk);
	}

	return DmResult_SUCCESS;
}
