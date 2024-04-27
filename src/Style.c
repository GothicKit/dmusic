// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"

DmResult DmStyle_create(DmStyle** slf) {
	if (slf == NULL) {
		return DmResult_INVALID_ARGUMENT;
	}

	DmStyle* new = *slf = Dm_alloc(sizeof *new);
	if (new == NULL) {
		return DmResult_MEMORY_EXHAUSTED;
	}

	new->reference_count = 1;

	DmBandList_init(&new->bands);
	DmPartList_init(&new->parts);
	DmPatternList_init(&new->patterns);

	return DmResult_SUCCESS;
}

DmStyle* DmStyle_retain(DmStyle* slf) {
	if (slf == NULL) {
		return NULL;
	}

	(void) atomic_fetch_add(&slf->reference_count, 1);
	return slf;
}

void DmStyle_release(DmStyle* slf) {
	if (slf == NULL) {
		return;
	}

	size_t refs = atomic_fetch_sub(&slf->reference_count, 1) - 1;
	if (refs > 0) {
		return;
	}

	DmPatternList_free(&slf->patterns);
	DmBandList_free(&slf->bands);
	DmPartList_free(&slf->parts);
	Dm_free(slf->backing_memory);
	Dm_free(slf);
}

DmResult DmStyle_download(DmStyle* slf, DmLoader* loader) {
	if (slf == NULL || loader == NULL) {
		return DmResult_INVALID_ARGUMENT;
	}

	DmResult rv = DmResult_SUCCESS;
	for (size_t i = 0; i < slf->bands.length; ++i) {
		rv = DmBand_download(slf->bands.data[i], loader);
		if (rv != DmResult_SUCCESS) {
			break;
		}
	}

	return rv;
}

DmPart* DmStyle_findPart(DmStyle* slf, DmPartReference* pref) {
	if (slf == NULL || pref == NULL) {
		return NULL;
	}

	for (size_t i = 0; i < slf->parts.length; ++i) {
		if (DmGuid_equals(&slf->parts.data[i].part_id, &pref->part_id)) {
			return &slf->parts.data[i];
		}
	}

	return NULL;
}

void DmPart_init(DmPart* slf) {
	if (slf == NULL) {
		Dm_report(DmLogLevel_ERROR, "DmPart: Internal error: DmPart_init called with a `NULL` pointer");
		return;
	}

	memset(slf, 0, sizeof *slf);
}

void DmPart_free(DmPart* slf) {
	if (slf == NULL) {
		Dm_report(DmLogLevel_ERROR, "DmPart: Internal error: DmPart_free called with a `NULL` pointer");
		return;
	}

	Dm_free(slf->notes);
	Dm_free(slf->curves);
}

uint32_t DmPart_getValidVariationCount(DmPart* slf) {
	if (slf == NULL) {
		return 0;
	}

	uint32_t i = 0;
	for (; i < 32; ++i) {
		if ((slf->variation_choices[i] & 0x0FFFFFFF) == 0) {
			break;
		}
	}

	return i;
}

void DmPartReference_init(DmPartReference* slf) {
	if (slf == NULL) {
		Dm_report(DmLogLevel_ERROR,
		          "DmPartReference: Internal error: DmPartReference_init called with a `NULL` pointer");
		return;
	}

	memset(slf, 0, sizeof *slf);
}

void DmPartReference_free(DmPartReference* slf) {
	(void) slf;
}

void DmPattern_init(DmPattern* slf) {
	if (slf == NULL) {
		Dm_report(DmLogLevel_ERROR, "DmPattern: Internal error: DmPattern_init called with a `NULL` pointer");
		return;
	}

	memset(slf, 0, sizeof *slf);
	DmPartReferenceList_init(&slf->parts);
}

void DmPattern_free(DmPattern* slf) {
	if (slf == NULL) {
		Dm_report(DmLogLevel_ERROR, "DmPattern: Internal error: DmPattern_free called with a `NULL` pointer");
		return;
	}

	DmPartReferenceList_free(&slf->parts);
	Dm_free(slf->rhythm);
	slf->rhythm = NULL;
	slf->rhythm_len = 0;
}
