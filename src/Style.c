// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"

#include <stdlib.h>

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

static uint32_t Dm_toEmbellishmentFlagset(DmCommandType cmd) {
	uint32_t f = 0;

	if (cmd & DmCommand_FILL) {
		f |= 1;
	}

	if (cmd & DmCommand_INTRO) {
		f |= 2;
	}

	if (cmd & DmCommand_BREAK) {
		f |= 4;
	}

	if (cmd & DmCommand_END) {
		f |= 8;
	}

	return f;
}

// See: https://documentation.help/DirectMusic/howmusicvariesduringplayback.htm
DmPattern* DmStyle_getRandomPattern(DmStyle* slf, uint32_t groove, DmCommandType cmd) {
	uint32_t embellishment = Dm_toEmbellishmentFlagset(cmd);

	// Select a random pattern according to the current groove level.
	// TODO(lmichaelis): This behaviour seems to be associated with DX < 8 only, newer versions should
	//                   have some way of defining how to select the pattern if more than 1 choice is available
	//                   but I couldn't find it.

	int64_t index = Dm_rand() % (uint32_t) slf->patterns.length;
	do {
		for (size_t i = 0; i < slf->patterns.length; ++i) {
			DmPattern* pttn = &slf->patterns.data[i];

			// Ignore patterns outside the current groove level.
			if (groove < pttn->groove_bottom || groove > pttn->groove_top) {
				continue;
			}

			// Patterns with a differing embellishment are not supported
			if (pttn->embellishment != embellishment && !(pttn->embellishment & embellishment)) {
				continue;
			}

			// Fix for Gothic 2 in which some patterns are empty but have a groove range of 1-100 with no embellishment
			// set.
			if (pttn->embellishment == DmCommand_GROOVE && pttn->length_measures == 1) {
				continue;
			}

			if (index == 0) {
				return pttn;
			}

			index -= 1;
		}
	} while (index >= 0);

	return NULL;
}

void DmPart_init(DmPart* slf) {
	if (slf == NULL) {
		return;
	}

	memset(slf, 0, sizeof *slf);
}

void DmPart_free(DmPart* slf) {
	if (slf == NULL) {
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
		return;
	}

	memset(slf, 0, sizeof *slf);
}

void DmPartReference_free(DmPartReference* slf) {
	(void) slf;
}

void DmPattern_init(DmPattern* slf) {
	if (slf == NULL) {
		return;
	}

	memset(slf, 0, sizeof *slf);
	DmPartReferenceList_init(&slf->parts);
}

void DmPattern_free(DmPattern* slf) {
	if (slf == NULL) {
		return;
	}

	DmPartReferenceList_free(&slf->parts);
	Dm_free(slf->rhythm);
	slf->rhythm = NULL;
	slf->rhythm_len = 0;
}
