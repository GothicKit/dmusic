// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"

static void DmDls_parseWaveSample(DmDlsWaveSample* slf, DmRiff* rif) {
	uint32_t size = 0;
	DmRiff_readDword(rif, &size);
	DmRiff_readWord(rif, &slf->unity_note);
	DmRiff_readWord(rif, &slf->fine_tune);
	DmRiff_readInt(rif, &slf->attenuation);
	DmRiff_readDword(rif, &slf->flags);

	uint32_t sample_loops = 0;
	DmRiff_readDword(rif, &sample_loops);

	if (sample_loops > 1) {
		Dm_report(DmLogLevel_ERROR, "DmDls: Wave sample reports more than 1 loop; ignoring excess");
		sample_loops = 1;
	} else if (sample_loops == 0) {
		slf->looping = false;
		return;
	}

	slf->looping = true;
	DmRiff_readDword(rif, &size);
	DmRiff_readDword(rif, &slf->loop_type);
	DmRiff_readDword(rif, &slf->loop_start);
	DmRiff_readDword(rif, &slf->loop_length);
}

static DmResult DmDls_parseArticulator(DmDlsArticulator* slf, DmRiff* rif) {
	uint32_t struct_size = 0;
	DmRiff_readDword(rif, &struct_size);

	DmRiff_readDword(rif, &slf->connection_count);
	slf->connections = Dm_alloc(slf->connection_count * sizeof(*slf->connections));
	if (slf->connections == NULL) {
		return DmResult_MEMORY_EXHAUSTED;
	}

	uint16_t tmp = 0;
	for (size_t i = 0; i < slf->connection_count; ++i) {
		DmRiff_readWord(rif, &tmp);
		slf->connections[i].source = tmp;

		DmRiff_readWord(rif, &slf->connections[i].control);

		DmRiff_readWord(rif, &tmp);
		slf->connections[i].destination = tmp;

		DmRiff_readWord(rif, &tmp);
		slf->connections[i].transform = tmp;

		DmRiff_readInt(rif, &slf->connections[i].scale);
	}

	return DmResult_SUCCESS;
}

static DmResult DmDls_parseArticulatorList(DmDlsArticulator* lst, DmRiff* rif, size_t len) {
	DmRiff cnk;
	for (size_t i = 0; i < len; ++i) {
		if (!DmRiff_readChunk(rif, &cnk)) {
			return DmResult_FILE_CORRUPT;
		}

		bool level1 = DmRiff_is(&cnk, DM_FOURCC_ART1, 0);
		bool level2 = DmRiff_is(&cnk, DM_FOURCC_ART2, 0);
		if (!level1 && !level2) {
			return DmResult_FILE_CORRUPT;
		}

		DmDlsArticulator_init(&lst[i]);
		lst[i].level = level2 ? 2 : 1;

		DmResult rv = DmDls_parseArticulator(&lst[i], &cnk);
		if (rv != DmResult_SUCCESS) {
			return rv;
		}

		DmRiff_reportDone(&cnk);
	}

	return DmResult_SUCCESS;
}

static DmResult DmDls_parseRegion(DmDlsRegion* slf, DmRiff* rif) {
	DmRiff cnk;
	while (DmRiff_readChunk(rif, &cnk)) {
		if (DmRiff_is(&cnk, DM_FOURCC_RGNH, 0)) {
			DmRiff_readWord(&cnk, &slf->range_low);
			DmRiff_readWord(&cnk, &slf->range_high);
			DmRiff_readWord(&cnk, &slf->velocity_low);
			DmRiff_readWord(&cnk, &slf->velocity_high);

			uint16_t options = 0;
			DmRiff_readWord(&cnk, &options);
			slf->flags = options;

			DmRiff_readWord(&cnk, &slf->key_group);
		} else if (DmRiff_is(&cnk, DM_FOURCC_WSMP, 0)) {
			DmDls_parseWaveSample(&slf->sample, &cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_WLNK, 0)) {
			uint16_t options = 0;
			DmRiff_readWord(&cnk, &options);
			slf->link_flags = options;

			DmRiff_readWord(&cnk, &slf->link_phase_group);
			DmRiff_readDword(&cnk, &slf->link_channel);
			DmRiff_readDword(&cnk, &slf->link_table_index);
		} else if (DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_LART) || DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_LAR2)) {
			// We can only accept either lart or lar2, not both. lar2 takes precedence, though.
			if (slf->articulator_count != 0 && !DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_LAR2)) {
				continue;
			}

			// If we're overriding lart, we need to free all the exising articulators
			if (slf->articulators != NULL) {
				for (size_t i = 0; i < slf->articulator_count; ++i) {
					DmDlsArticulator_free(&slf->articulators[i]);
				}

				Dm_free(slf->articulators);
			}

			slf->articulator_count = DmRiff_chunks(&cnk);
			slf->articulators = Dm_alloc(slf->articulator_count * sizeof(DmDlsArticulator));

			if (slf->articulators == NULL) {
				return DmResult_MEMORY_EXHAUSTED;
			}

			DmResult rv = DmDls_parseArticulatorList(slf->articulators, &cnk, slf->articulator_count);
			if (rv != DmResult_SUCCESS) {
				return rv;
			}
		}

		DmRiff_reportDone(&cnk);
	}

	return DmResult_SUCCESS;
}

static DmResult DmDls_parseInstrumentRegionList(DmDlsInstrument* slf, DmRiff* rif) {
	slf->regions = Dm_alloc(slf->region_count * sizeof(DmDlsRegion));
	if (slf->regions == NULL) {
		return DmResult_MEMORY_EXHAUSTED;
	}

	DmRiff cnk;
	for (size_t i = 0; i < slf->region_count; ++i) {
		if (!DmRiff_readChunk(rif, &cnk)) {
			return DmResult_FILE_CORRUPT;
		}

		if (!DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_RGN_) && !DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_RGN2)) {
            DmRiff_reportDone(&cnk);
            continue;
		}

		DmDlsRegion_init(&slf->regions[i]);
		DmResult rv = DmDls_parseRegion(&slf->regions[i], &cnk);
		if (rv != DmResult_SUCCESS) {
			return rv;
		}

		DmRiff_reportDone(&cnk);
	}

	return DmResult_SUCCESS;
}

static DmResult DmDls_parseInstrument(DmDlsInstrument* slf, DmRiff* rif) {
	DmRiff cnk;
	while (DmRiff_readChunk(rif, &cnk)) {
		DmResult rv = DmResult_SUCCESS;

		if (DmRiff_is(&cnk, DM_FOURCC_INSH, 0)) {
			DmRiff_readDword(&cnk, &slf->region_count);
			DmRiff_readDword(&cnk, &slf->bank);
			DmRiff_readDword(&cnk, &slf->patch);
		} else if (DmRiff_is(&cnk, DM_FOURCC_DLID, 0)) {
			DmGuid_parse(&slf->guid, &cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_INFO)) {
			DmInfo_parse(&slf->info, &cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_LRGN)) {
			rv = DmDls_parseInstrumentRegionList(slf, &cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_LART) || DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_LAR2)) {
			// We can only accept either lart or lar2, not both. lar2 takes precedence, though.
			if (slf->articulator_count != 0 && !DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_LAR2)) {
				continue;
			}

			// If we're overriding lart, we need to free all the exising articulators
			if (slf->articulators != NULL) {
				for (size_t i = 0; i < slf->articulator_count; ++i) {
					DmDlsArticulator_free(&slf->articulators[i]);
				}

				Dm_free(slf->articulators);
			}
			slf->articulator_count = DmRiff_chunks(&cnk);
			slf->articulators = Dm_alloc(slf->articulator_count * sizeof(DmDlsArticulator));

			if (slf->articulators == NULL) {
				return DmResult_MEMORY_EXHAUSTED;
			}

			rv = DmDls_parseArticulatorList(slf->articulators, &cnk, slf->articulator_count);
		}

		if (rv != DmResult_SUCCESS) {
			return rv;
		}

		DmRiff_reportDone(&cnk);
	}

	return DmResult_SUCCESS;
}

static DmResult DmDls_parseInstrumentList(DmDls* slf, DmRiff* rif) {
	slf->instruments = Dm_alloc(slf->instrument_count * sizeof(DmDlsInstrument));
	if (slf->instruments == NULL) {
		return DmResult_MEMORY_EXHAUSTED;
	}

	DmRiff cnk;
	for (size_t i = 0; i < slf->instrument_count; ++i) {
		if (!DmRiff_readChunk(rif, &cnk)) {
			return DmResult_FILE_CORRUPT;
		}

		if (!DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_INS_)) {
			return DmResult_FILE_CORRUPT;
		}

		DmResult rv = DmDls_parseInstrument(&slf->instruments[i], &cnk);
		if (rv != DmResult_SUCCESS) {
			return rv;
		}

		DmRiff_reportDone(&cnk);
	}

	return DmResult_SUCCESS;
}

static void DmDls_parseWaveTableItemFormat(DmDlsWave* slf, DmRiff* rif) {
	uint16_t tmp = 0;
	DmRiff_readWord(rif, &tmp);
	slf->format = tmp;

	DmRiff_readWord(rif, &slf->channels);
	DmRiff_readDword(rif, &slf->samples_per_second);
	DmRiff_readDword(rif, &slf->avg_bytes_per_second);
	DmRiff_readWord(rif, &slf->block_align);

	if (slf->format == DmDlsWaveFormat_PCM) {
		DmRiff_readWord(rif, &slf->bits_per_sample);
		DmRiff_read(rif, &tmp, 2); // Padding
	} else if (slf->format == DmDlsWaveFormat_ADPCM) {
		uint16_t size = 0;
		DmRiff_readWord(rif, &slf->bits_per_sample);
		DmRiff_readWord(rif, &size);
		DmRiff_readWord(rif, &slf->samples_per_block);
		DmRiff_readWord(rif, &size);

		if (size != 7) {
			Dm_report(DmLogLevel_ERROR, "DmDls: Invalid ADPCM coefficient count: %d", size);
			return;
		}

		for (uint16_t i = 0; i < size * 2; ++i) {
			DmRiff_readWord(rif, slf->coefficient_table + i);
		}
	} else {
		Dm_report(DmLogLevel_ERROR, "DmDls: Unknown Wave Format: %d", slf->format);
	}
}

static void DmDls_parseWaveTableItem(DmDlsWave* slf, DmRiff* rif) {
	DmRiff cnk;
	while (DmRiff_readChunk(rif, &cnk)) {
		if (DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_INFO)) {
			DmInfo_parse(&slf->info, &cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_GUID, 0)) {
			DmGuid_parse(&slf->guid, &cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_DATA, 0)) {
			slf->pcm_size = cnk.len;
			slf->pcm = cnk.mem;
			cnk.pos = cnk.len;
		} else if (DmRiff_is(&cnk, DM_FOURCC_WSMP, 0)) {
			DmDls_parseWaveSample(&slf->sample, &cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_WAVH, 0)) {
			continue; // TODO(lmichaelis): The purpose of the 'wavh'-chunk is unknown.
		} else if (DmRiff_is(&cnk, DM_FOURCC_WAVU, 0)) {
			continue; // TODO(lmichaelis): The purpose of the 'wavu'-chunk is unknown.
		} else if (DmRiff_is(&cnk, DM_FOURCC_SMPL, 0)) {
			continue; // TODO(lmichaelis): The purpose of the 'smpl'-chunk is unknown.
		} else if (DmRiff_is(&cnk, DM_FOURCC_FMT_, 0)) {
			DmDls_parseWaveTableItemFormat(slf, &cnk);
		}

		DmRiff_reportDone(&cnk);
	}
}

static DmResult DmDls_parsePoolTable(DmDls* slf, DmRiff* rif) {
	uint32_t size = 0;
	DmRiff_readDword(rif, &size); // Structure size; ignored.
	DmRiff_readDword(rif, &slf->pool_table_size);

	slf->pool_table = Dm_alloc(slf->pool_table_size * sizeof(uint32_t));
	if (slf->pool_table == NULL) {
		return DmResult_MEMORY_EXHAUSTED;
	}

	for (uint32_t i = 0; i < slf->pool_table_size; ++i) {
		DmRiff_readDword(rif, slf->pool_table + i);
	}

	return DmResult_SUCCESS;
}

static DmResult DmDls_parseWaveTable(DmDls* slf, DmRiff* rif) {
	slf->wave_table_size = DmRiff_chunks(rif);
	slf->wave_table = Dm_alloc(slf->wave_table_size * sizeof(DmDlsWave));

	if (slf->wave_table == NULL) {
		return DmResult_MEMORY_EXHAUSTED;
	}

	DmRiff cnk;
	for (size_t i = 0; i < slf->wave_table_size; ++i) {
		if (!DmRiff_readChunk(rif, &cnk)) {
			return DmResult_FILE_CORRUPT;
		}

		if (!DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_WAVE)) {
			return DmResult_FILE_CORRUPT;
		}

		DmDls_parseWaveTableItem(&slf->wave_table[i], &cnk);
		DmRiff_reportDone(&cnk);
	}

	return DmResult_SUCCESS;
}

DmResult DmDls_parse(DmDls* slf, void* buf, size_t len) {
	DmRiff rif;
	if (!DmRiff_init(&rif, buf, len)) {
		Dm_free(buf);
		return DmResult_FILE_CORRUPT;
	}

	slf->backing_memory = buf;

	DmRiff cnk;
	while (DmRiff_readChunk(&rif, &cnk)) {
		DmResult rv = DmResult_SUCCESS;

		if (DmRiff_is(&cnk, DM_FOURCC_DLID, 0)) {
			DmGuid_parse(&slf->guid, &cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_VERS, 0)) {
			DmVersion_parse(&slf->version, &cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_COLH, 0)) {
			DmRiff_readDword(&cnk, &slf->instrument_count);
		} else if (DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_INFO)) {
			DmInfo_parse(&slf->info, &cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_PTBL, 0)) {
			rv = DmDls_parsePoolTable(slf, &cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_LINS)) {
			rv = DmDls_parseInstrumentList(slf, &cnk);
		} else if (DmRiff_is(&cnk, DM_FOURCC_LIST, DM_FOURCC_WVPL)) {
			rv = DmDls_parseWaveTable(slf, &cnk);
		}

		if (rv != DmResult_SUCCESS) {
			return rv;
		}

		DmRiff_reportDone(&cnk);
	}

	return DmResult_SUCCESS;
}
