// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"

DmResult DmDls_create(DmDls** slf) {
	if (slf == NULL) {
		return DmResult_INVALID_ARGUMENT;
	}

	DmDls* new = *slf = Dm_alloc(sizeof *new);
	if (new == NULL) {
		return DmResult_MEMORY_EXHAUSTED;
	}

	new->reference_count = 1;
	return DmResult_SUCCESS;
}

DmDls* DmDls_retain(DmDls* slf) {
	if (slf == NULL) {
		return NULL;
	}

	(void) atomic_fetch_add(&slf->reference_count, 1);
	return slf;
}

size_t DmDls_release(DmDls* slf) {
	if (slf == NULL) {
		return 0;
	}

	size_t refs = atomic_fetch_sub(&slf->reference_count, 1) - 1;
	if (refs > 0) {
		return refs;
	}

	for (uint32_t i = 0; i < slf->instrument_count; ++i) {
		DmDlsInstrument_free(&slf->instruments[i]);
	}

	Dm_free(slf->instruments);
	Dm_free(slf->pool_table);
	Dm_free(slf->wave_table);
	Dm_free(slf->backing_memory);
	Dm_free(slf);
	return 0;
}

void DmDlsInstrument_init(DmDlsInstrument* slf) {
	if (slf == NULL) {
		return;
	}

	memset(slf, 0, sizeof *slf);
}

void DmDlsInstrument_free(DmDlsInstrument* slf) {
	if (slf == NULL) {
		return;
	}

	for (size_t i = 0; i < slf->region_count; ++i) {
		DmDlsRegion_free(&slf->regions[i]);
	}

	Dm_free(slf->regions);
	slf->regions = NULL;
	slf->region_count = 0;

	for (size_t i = 0; i < slf->articulator_count; ++i) {
		DmDlsArticulator_free(&slf->articulators[i]);
	}

	Dm_free(slf->articulators);
	slf->articulators = NULL;
	slf->articulator_count = 0;
}

void DmDlsRegion_init(DmDlsRegion* slf) {
	if (slf == NULL) {
		return;
	}

	memset(slf, 0, sizeof *slf);
}

void DmDlsRegion_free(DmDlsRegion* slf) {
	if (slf == NULL) {
		return;
	}

	for (size_t i = 0; i < slf->articulator_count; ++i) {
		DmDlsArticulator_free(&slf->articulators[i]);
	}

	Dm_free(slf->articulators);
	slf->articulators = NULL;
	slf->articulator_count = 0;
}

void DmDlsArticulator_init(DmDlsArticulator* slf) {
	if (slf == NULL) {
		return;
	}

	memset(slf, 0, sizeof *slf);
}

void DmDlsArticulator_free(DmDlsArticulator* slf) {
	if (slf == NULL) {
		return;
	}

	Dm_free(slf->connections);
	slf->connections = NULL;
	slf->connection_count = 0;
}

static size_t DmDlsWave_decodeShort(DmDlsWave const* slf, float* out, size_t len) {
	uint32_t size = slf->pcm_size / 2;
	if (out == NULL) {
		return size;
	}

	int16_t const* raw = (int16_t const*) slf->pcm;
	size_t i = 0;
	for (i = 0; i < size && i < len; ++i) {
		out[i] = (float) raw[i] / INT16_MAX;
	}

	return i;
}

static int signed_4bit(int v) {
	return v & 0x8 ? v - 16 : v;
}

static int clamp_16bit(int v) {
	if (v < INT16_MIN) {
		return INT16_MIN;
	}

	if (v > INT16_MAX) {
		return INT16_MAX;
	}

	return v;
}

// TODO(lmichaelis): These are the 'built in' set of 7 predictor value pairs; additional values can be added
//  				 to this table by including them as metadata chunks in the WAVE header
static int16_t ADPCM_ADAPT_TABLE[16] = {230, 230, 230, 230, 307, 409, 512, 614, 768, 614, 512, 409, 307, 230, 230, 230};

#define DmInt_read(src, tgt)                                                                                           \
	memcpy((tgt), (src), sizeof(*(tgt)));                                                                              \
	(src) += sizeof(*(tgt))

// See https://wiki.multimedia.cx/index.php/Microsoft_ADPCM
static uint8_t const* DmDls_decodeAdpcmBlock(uint8_t const* adpcm, float* pcm, uint32_t block_size, int16_t const* coeff1, int16_t const* coeff2) {
	uint8_t block_predictor;
	DmInt_read(adpcm, &block_predictor);

	int16_t delta_;
	DmInt_read(adpcm, &delta_);
	int delta = delta_;

	int16_t sample_a;
	DmInt_read(adpcm, &sample_a);

	int16_t sample_b;
	DmInt_read(adpcm, &sample_b);

	*pcm++ = (float) sample_b / (float) INT16_MAX;
	*pcm++ = (float) sample_a / (float) INT16_MAX;

	int coeff_1 = coeff1[block_predictor];
	int coeff_2 = coeff2[block_predictor];

	uint32_t remaining = block_size - 7 /* header */;
	for (uint32_t i = 0; i < remaining; ++i) {
		int8_t b;
		DmInt_read(adpcm, &b);

		// High Nibble
		int nibble = signed_4bit((b & 0xF0) >> 4);
		int predictor = (coeff_1 * sample_a + coeff_2 * sample_b) / 256;
		predictor += nibble * delta;
		predictor = clamp_16bit(predictor);
		*pcm++ = (float) predictor / (float) INT16_MAX;
		sample_b = sample_a;
		sample_a = (int16_t) (predictor);
		delta = max_s32((ADPCM_ADAPT_TABLE[(b & 0xF0) >> 4] * delta) / 256, 16);

		// Low Nibble
		nibble = signed_4bit((b & 0x0F) >> 0);
		predictor = (coeff_1 * sample_a + coeff_2 * sample_b) / 256;
		predictor += nibble * delta;
		predictor = clamp_16bit(predictor);
		*pcm++ = (float) predictor / (float) INT16_MAX;
		sample_b = sample_a;
		sample_a = (int16_t) (predictor);
		delta = max_s32((ADPCM_ADAPT_TABLE[b & 0x0F] * delta) / 256, 16);
	}

	return adpcm;
}

static size_t DmDls_decodeAdpcm(DmDlsWave const* slf, float* out, size_t len) {
	if (slf->channels != 1) {
		Dm_report(DmLogLevel_ERROR,
		          "DmDls: Attempted to decode ADPCM with %d channels; only mono is supported!",
		          slf->channels);
		return 0;
	}

	uint32_t block_count = slf->pcm_size / slf->block_align;
	uint32_t frames_per_block =
	    (uint32_t) (slf->block_align - 6 * slf->channels) * 2 /* two frames per channel from the header */;
	uint32_t size = frames_per_block * block_count;

	if (out == NULL || len == 0) {
		return size;
	}

	uint8_t const* adpcm = slf->pcm;
	size_t offset = 0;
	for (size_t i = 0; i < block_count; ++i) {
		if (len < offset + frames_per_block) {
			break;
		}

		adpcm = DmDls_decodeAdpcmBlock(adpcm, out + offset, slf->block_align, slf->coefficient_table_0 , slf->coefficient_table_1);
		offset += frames_per_block;
	}

	return offset;
}

size_t DmDls_decodeSamples(DmDlsWave const* slf, float* out, size_t len) {
	switch (slf->format) {
	case DmDlsWaveFormat_PCM:
		return DmDlsWave_decodeShort(slf, out, len);
	case DmDlsWaveFormat_ADPCM:
		return DmDls_decodeAdpcm(slf, out, len);
	default:
		return 0;
	}
}
