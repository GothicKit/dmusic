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

	for (size_t i = 0; i< slf->region_count; ++i) {
		DmDlsRegion_free(&slf->regions[i]);
	}

	Dm_free(slf->regions);
	slf->regions = NULL;
	slf->region_count = 0;

	for (size_t i = 0; i< slf->articulator_count; ++i) {
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

	for (size_t i = 0; i< slf->articulator_count; ++i) {
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
