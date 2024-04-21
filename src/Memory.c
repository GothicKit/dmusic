// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"

#include <stdlib.h>
#include <string.h>

static void* DmInt_defaultAlloc(void* ctx, size_t len);
static void DmInt_defaultFree(void* ctx, void* ptr);

static _Atomic size_t DmGlob_allocCount = 0;
static void* DmGlob_allocContext = NULL;
static DmMemoryAlloc* DmGlob_alloc = DmInt_defaultAlloc;
static DmMemoryFree* DmGlob_free = DmInt_defaultFree;

DmResult Dm_setHeapAllocator(DmMemoryAlloc* alloc, DmMemoryFree* free, void* ctx) {
	if (DmGlob_allocCount != 0) {
		return DmResult_INVALID_STATE;
	}

	if (alloc == NULL || free == NULL) {
		return DmResult_INVALID_ARGUMENT;
	}

	DmGlob_allocContext = ctx;
	DmGlob_alloc = alloc;
	DmGlob_free = free;

	return DmResult_SUCCESS;
}

void* Dm_alloc(size_t len) {
	void* mem = DmGlob_alloc(DmGlob_allocContext, len);
	if (mem == NULL) {
		return NULL;
	}

	atomic_fetch_add(&DmGlob_allocCount, 1);
	return memset(mem, 0, len);
}

void Dm_free(void* ptr) {
	DmGlob_free(DmGlob_allocContext, ptr);
}

static void* DmInt_defaultAlloc(void* ctx, size_t len) {
	(void) ctx;
	return malloc(len);
}

static void DmInt_defaultFree(void* ctx, void* ptr) {
	(void) ctx;
	free(ptr);
}
