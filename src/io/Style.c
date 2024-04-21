// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"

static void DmStyle_parsePartReference(DmPartReference* slf, DmRiff* rif) {
	DmRiff cnk;
	while (DmRiff_readChunk(rif, &cnk)) {
		if (DmRiff_is(&cnk, DM_FOURCC_PRFC, 0)) {
			DmGuid_parse(&slf->part_id, &cnk);
			DmRiff_readWord(&cnk, &slf->logical_part_id);
			DmRiff_readByte(&cnk, &slf->variation_lock_id);
			DmRiff_readByte(&cnk, &slf->subchord_level);
			DmRiff_readByte(&cnk, &slf->priority);
			DmRiff_readByte(&cnk, &slf->random_variation);
		} else if (DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_UNFO)) {
			DmUnfo_parse(&slf->info, &cnk);
		}

		DmRiff_reportDone(&cnk);
	}
}

static DmResult DmStyle_parsePattern(DmPattern* slf, DmRiff* rif) {
	DmRiff cnk;
	while (DmRiff_readChunk(rif, &cnk)) {
		if (DmRiff_is(&cnk, DM_FOURCC_PTNH, 0)) {
			DmTimeSignature_parse(&slf->time_signature, &cnk);
			DmRiff_readByte(&cnk, &slf->groove_bottom);
			DmRiff_readByte(&cnk, &slf->groove_top);
			DmRiff_readWord(&cnk, &slf->embellishment);
			DmRiff_readWord(&cnk, &slf->length_measures);
		} else if (DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_UNFO)) {
			DmUnfo_parse(&slf->info, &cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_RHTM, 0)) {
			slf->rhythm_len = cnk.len / 4;
			slf->rhythm = Dm_alloc(cnk.len);
			if (slf->rhythm == NULL) {
				return DmResult_MEMORY_EXHAUSTED;
			}

			for (size_t i = 0; i < slf->rhythm_len; ++i) {
				DmRiff_readDword(&cnk, &slf->rhythm[i]);
			}
		} else if (DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_PREF)) {
			DmPartReference pref;
			DmPartReference_init(&pref);
			DmStyle_parsePartReference(&pref, &cnk);

			DmResult rv = DmPartReferenceList_add(&slf->parts, pref);
			if (rv != DmResult_SUCCESS) {
				return rv;
			}
		}

		DmRiff_reportDone(&cnk);
	}

	return DmResult_SUCCESS;
}

static DmResult DmStyle_parsePartNotes(DmPart* part, DmRiff* rif) {
	uint32_t item_size = 0;
	DmRiff_readDword(rif, &item_size);

	part->note_count = (rif->len - rif->pos) / item_size;
	part->notes = Dm_alloc(part->note_count * sizeof(DmNote));

	if (part->notes == NULL) {
		return DmResult_MEMORY_EXHAUSTED;
	}

	for (uint32_t i = 0; i < part->note_count; ++i) {
		uint32_t end_position = rif->pos + item_size;

		DmRiff_readDword(rif, &part->notes[i].grid_start);
		DmRiff_readDword(rif, &part->notes[i].variation);
		DmRiff_readDword(rif, &part->notes[i].duration);
		DmRiff_readShort(rif, &part->notes[i].time_offset);
		DmRiff_readWord(rif, &part->notes[i].music_value);
		DmRiff_readByte(rif, &part->notes[i].velocity);
		DmRiff_readByte(rif, &part->notes[i].time_range);
		DmRiff_readByte(rif, &part->notes[i].duration_range);
		DmRiff_readByte(rif, &part->notes[i].velocity_range);
		DmRiff_readByte(rif, &part->notes[i].inversion_id);

		uint8_t flags = 0;
		DmRiff_readByte(rif, &flags);
		part->notes[i].play_mode_flags = flags;

		rif->pos = end_position;
	}

	return DmResult_SUCCESS;
}

static DmResult DmStyle_parsePart(DmPart* slf, DmRiff* rif) {
	DmRiff cnk;
	while (DmRiff_readChunk(rif, &cnk)) {
		if (DmRiff_is(&cnk, DM_FOURCC_PRTH, 0)) {
			DmTimeSignature_parse(&slf->time_signature, &cnk);

			for (size_t i = 0; i < 32; ++i) {
				DmRiff_readDword(&cnk, slf->variation_choices + i);
			}

			DmGuid_parse(&slf->part_id, &cnk);
			DmRiff_readWord(&cnk, &slf->length_measures);

			uint8_t play_mode_flags = 0;
			DmRiff_readByte(&cnk, &play_mode_flags);
			slf->play_mode_flags = play_mode_flags;

			DmRiff_readByte(&cnk, &slf->invert_upper);
			DmRiff_readByte(&cnk, &slf->invert_lower);
			DmRiff_read(&cnk, &play_mode_flags, 1); // padding
		} else if (DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_UNFO)) {
			DmUnfo_parse(&slf->info, &cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_NOTE, 0)) {
			DmResult rv = DmStyle_parsePartNotes(slf, &cnk);
			if (rv != DmResult_SUCCESS) {
				return rv;
			}
		}

		DmRiff_reportDone(&cnk);
	}

	return DmResult_SUCCESS;
}

DmResult DmStyle_parse(DmStyle* slf, void* buf, size_t len) {
	DmRiff rif;
	if (!DmRiff_init(&rif, buf, len)) {
		Dm_free(buf);
		return DmResult_FILE_CORRUPT;
	}

	slf->backing_memory = buf;

	DmRiff cnk;
	while (DmRiff_readChunk(&rif, &cnk)) {
		if (DmRiff_is(&cnk, DM_FOURCC_STYH, 0)) {
			DmTimeSignature_parse(&slf->time_signature, &cnk);
			DmRiff_readDouble(&cnk, &slf->tempo);
		} else if (DmRiff_is(&cnk, DM_FOURCC_GUID, 0)) {
			DmGuid_parse(&slf->guid, &cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_VERS, 0)) {
			DmVersion_parse(&slf->version, &cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_UNFO)) {
			DmUnfo_parse(&slf->info, &cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_RIFF, DM_FOURCC_DMBD)) {
			DmBand* band = NULL;
			DmResult rv = DmBand_create(&band);
			if (rv != DmResult_SUCCESS) {
				return rv;
			}

			rv = DmBand_parse(band, &cnk);
			if (rv != DmResult_SUCCESS) {
				DmBand_release(band);
				return rv;
			}

			rv = DmBandList_add(&slf->bands, band);
			if (rv != DmResult_SUCCESS) {
				DmBand_release(band);
				return rv;
			}
		} else if (DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_PART)) {
			DmPart part;
			DmPart_init(&part);

			DmResult rv = DmStyle_parsePart(&part, &cnk);
			if (rv != DmResult_SUCCESS) {
				DmPart_free(&part);
				return rv;
			}

			rv = DmPartList_add(&slf->parts, part);
			if (rv != DmResult_SUCCESS) {
				return rv;
			}
		} else if (DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_PTTN)) {
			DmPattern pttn;
			DmPattern_init(&pttn);

			DmResult rv = DmStyle_parsePattern(&pttn, &cnk);
			if (rv != DmResult_SUCCESS) {
				DmPattern_free(&pttn);
				return rv;
			}

			rv = DmPatternList_add(&slf->patterns, pttn);
			if (rv != DmResult_SUCCESS) {
				return rv;
			}
		}

		DmRiff_reportDone(&cnk);
	}

	return DmResult_SUCCESS;
}

