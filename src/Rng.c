// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"

#include <stdlib.h>

static uint32_t DmInt_defaultRng(void* ctx);

static DmRng* DmGlob_rngCallback = DmInt_defaultRng;
static void* DmGlob_rngContext = NULL;

uint32_t Dm_rand(void) {
	return DmGlob_rngCallback(DmGlob_rngContext);
}

void Dm_setRandomNumberGenerator(DmRng* rng, void* ctx) {
	if (rng == NULL) {
		DmGlob_rngCallback = DmInt_defaultRng;
		DmGlob_rngContext = NULL;
		return;
	}

	DmGlob_rngCallback = rng;
	DmGlob_rngContext = ctx;
}

static uint32_t DmInt_defaultRng(void* ctx) {
	(void) ctx;
	return rand();
}
