// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"

static void* DmLoader_resolveName(DmLoader* slf, char const* name, size_t* length);

DmResult DmLoader_create(DmLoader** slf, DmLoaderOptions opt) {
	if (slf == NULL) {
		return DmResult_INVALID_ARGUMENT;
	}

	DmLoader* new = *slf = Dm_alloc(sizeof *new);

	if (new == NULL) {
		return DmResult_MEMORY_EXHAUSTED;
	}

	new->reference_count = 1;
	new->autodownload = opt& DmLoader_DOWNLOAD;

	if (mtx_init(&new->cache_lock, mtx_recursive) != thrd_success) {
		Dm_free(new);
		return DmResult_INTERNAL_ERROR;
	}

	DmResolverList_init(&new->resolvers);
	DmStyleCache_init(&new->style_cache);
	DmDlsCache_init(&new->dls_cache);

	return DmResult_SUCCESS;
}

DmLoader* DmLoader_retain(DmLoader* slf) {
	if (slf == NULL) {
		return NULL;
	}

	(void) atomic_fetch_add(&slf->reference_count, 1);
	return slf;
}

void DmLoader_release(DmLoader* slf) {
	if (slf == NULL) {
		return;
	}

	size_t refs = atomic_fetch_sub(&slf->reference_count, 1) - 1;
	if (refs != 0) {
		return;
	}

	mtx_destroy(&slf->cache_lock);
	DmStyleCache_free(&slf->style_cache);
	DmDlsCache_free(&slf->dls_cache);
	DmResolverList_free(&slf->resolvers);
	Dm_free(slf);
}

DmResult DmLoader_addResolver(DmLoader* slf, DmLoaderResolverCallback* resolve, void* ctx) {
	if (slf == NULL || resolve == NULL) {
		return DmResult_INVALID_ARGUMENT;
	}

	DmResolver resolver;
	resolver.context = ctx;
	resolver.resolve = resolve;

	if (mtx_lock(&slf->cache_lock) != thrd_success) {
		return DmResult_INTERNAL_ERROR;
	}

	DmResult rv = DmResolverList_add(&slf->resolvers, resolver);

	(void) mtx_unlock(&slf->cache_lock);
	return rv;
}

void* DmLoader_resolveName(DmLoader* slf, const char* name, size_t* length) {
	if (mtx_lock(&slf->cache_lock) != thrd_success) {
		return NULL;
	}

	void* bytes = NULL;
	for (size_t i = 0U; i < slf->resolvers.length; ++i) {
		DmResolver* resolver = &slf->resolvers.data[i];
		bytes = resolver->resolve(resolver->context, name, length);
		if (bytes != NULL) {
			break;
		}
	}

	(void) mtx_unlock(&slf->cache_lock);
	return bytes;
}

DmResult DmLoader_getSegment(DmLoader* slf, char const* name, DmSegment** segment) {
	if (slf == NULL || name == NULL || segment == NULL) {
		return DmResult_INVALID_ARGUMENT;
	}

	size_t length = 0;
	void* bytes = DmLoader_resolveName(slf, name, &length);
	if (bytes == NULL) {
		Dm_report(DmLogLevel_DEBUG, "DmLoader: Segment '%s' not found", name);
		return DmResult_NOT_FOUND;
	}

	DmResult rv = DmSegment_create(segment);
	if (rv != DmResult_SUCCESS) {
		Dm_free(bytes);
		return rv;
	}

	Dm_report(DmLogLevel_DEBUG, "DmLoader: Loading segment '%s'", name);

	rv = DmSegment_parse(*segment, bytes, length);
	if (rv != DmResult_SUCCESS) {
		DmSegment_release(*segment);
		return rv;
	}

	if (!slf->autodownload) {
		return DmResult_SUCCESS;
	}

	rv = DmSegment_download(*segment, slf);
	if (rv != DmResult_SUCCESS) {
		Dm_report(DmLogLevel_ERROR, "DmLoader: Automatic download of segment '%s' failed", name);
		DmSegment_release(*segment);
		return rv;
	}

	Dm_report(DmLogLevel_INFO, "DmLoader: Automatic download of segment '%s' succeeded", name);
	return DmResult_SUCCESS;
}

DmResult DmLoader_getDownloadableSound(DmLoader* slf, DmReference const* ref, DmDls** snd) {
	if (slf == NULL || ref == NULL || snd == NULL) {
		return DmResult_INVALID_ARGUMENT;
	}

	// See if we have the requested item in the cache.
	if (mtx_lock(&slf->cache_lock) != thrd_success) {
		return DmResult_INTERNAL_ERROR;
	}

	for (size_t i = 0; i < slf->dls_cache.length; ++i) {
		if (!DmGuid_equals(&ref->guid, &slf->dls_cache.data[i]->guid)) {
			continue;
		}

		*snd = DmDls_retain(slf->dls_cache.data[i]);

		(void) mtx_unlock(&slf->cache_lock);
		return DmResult_SUCCESS;
	}

	(void) mtx_unlock(&slf->cache_lock);

	// Resolve and parse the DLS
	size_t length = 0;
	void* bytes = DmLoader_resolveName(slf, ref->file, &length);
	if (bytes == NULL) {
		Dm_report(DmLogLevel_DEBUG, "DmLoader: DLS collection '%s' not found", ref->name);
		return DmResult_NOT_FOUND;
	}

	DmResult rv = DmDls_create(snd);
	if (rv != DmResult_SUCCESS) {
		Dm_free(bytes);
		return rv;
	}

	Dm_report(DmLogLevel_DEBUG, "DmLoader: Loading DLS collection '%s'", ref->file);

	rv = DmDls_parse(*snd, bytes, length);
	if (rv != DmResult_SUCCESS) {
		DmDls_release(*snd);
		return rv;
	}

	// Add the new item to the cache
	if (mtx_lock(&slf->cache_lock) != thrd_success) {
		return DmResult_INTERNAL_ERROR;
	}

	rv = DmDlsCache_add(&slf->dls_cache, DmDls_retain(*snd));

	(void) mtx_unlock(&slf->cache_lock);
	return rv;
}

DmResult DmLoader_getStyle(DmLoader* slf, DmReference const* ref, DmStyle** sty) {
	if (slf == NULL || ref == NULL || sty == NULL) {
		return DmResult_INVALID_ARGUMENT;
	}

	// See if we have the requested item in the cache.
	if (mtx_lock(&slf->cache_lock) != thrd_success) {
		return DmResult_INTERNAL_ERROR;
	}

	for (size_t i = 0; i < slf->style_cache.length; ++i) {
		if (!DmGuid_equals(&ref->guid, &slf->style_cache.data[i]->guid)) {
			continue;
		}

		*sty = DmStyle_retain(slf->style_cache.data[i]);

		(void) mtx_unlock(&slf->cache_lock);
		return DmResult_SUCCESS;
	}

	(void) mtx_unlock(&slf->cache_lock);

	// Resolve and parse the DLS
	size_t length = 0;
	void* bytes = DmLoader_resolveName(slf, ref->file, &length);
	if (bytes == NULL) {
		Dm_report(DmLogLevel_DEBUG, "DmLoader: Style '%s' not found", ref->name);
		return DmResult_NOT_FOUND;
	}

	DmResult rv = DmStyle_create(sty);
	if (rv != DmResult_SUCCESS) {
		Dm_free(bytes);
		return rv;
	}

	Dm_report(DmLogLevel_DEBUG, "DmLoader: Loading style '%s'", ref->file);

	rv = DmStyle_parse(*sty, bytes, length);
	if (rv != DmResult_SUCCESS) {
		DmStyle_release(*sty);
		return rv;
	}

	// Add the new item to the cache
	if (mtx_lock(&slf->cache_lock) != thrd_success) {
		return DmResult_INTERNAL_ERROR;
	}

	rv = DmStyleCache_add(&slf->style_cache, DmStyle_retain(*sty));

	(void) mtx_unlock(&slf->cache_lock);
	return rv;
}
