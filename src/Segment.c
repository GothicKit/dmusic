// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"

DmResult DmSegment_create(DmSegment** slf) {
	if (slf == NULL) {
		return DmResult_INVALID_ARGUMENT;
	}

	DmSegment* new = *slf = Dm_alloc(sizeof *new);
	if (new == NULL) {
		return DmResult_MEMORY_EXHAUSTED;
	}

	new->reference_count = 1;

	DmMessageList_init(&new->messages);

	return DmResult_SUCCESS;
}

DmSegment* DmSegment_retain(DmSegment* slf) {
	if (slf == NULL) {
		return NULL;
	}

	(void) atomic_fetch_add(&slf->reference_count, 1);
	return slf;
}

void DmSegment_release(DmSegment* slf) {
	if (slf == NULL) {
		return;
	}

	size_t refs = atomic_fetch_sub(&slf->reference_count, 1) - 1;
	if (refs > 0) {
		return;
	}

	DmMessageList_free(&slf->messages);
	Dm_free(slf->backing_memory);
	Dm_free(slf);
}

DmResult DmSegment_download(DmSegment* slf, DmLoader* loader) {
	if (slf == NULL || loader == NULL) {
		return DmResult_INVALID_ARGUMENT;
	}

	DmResult rv = DmResult_SUCCESS;
	for (size_t i = 0; i < slf->messages.length; ++i) {
		DmMessage* msg = slf->messages.data + i;

		if (msg->type == DmMessage_BAND) {
			rv = DmBand_download(msg->band.band, loader);
		} else if (msg->type == DmMessage_STYLE) {
			rv = DmLoader_getStyle(loader, &msg->style.reference, &msg->style.style);

			if (rv != DmResult_SUCCESS) {
				break;
			}

			rv = DmStyle_download(msg->style.style, loader);
		}

		if (rv != DmResult_SUCCESS) {
			break;
		}
	}

	slf->downloaded = true;
	return rv;
}

DmGuid const* DmSegment_getGuid(DmSegment const* slf) {
	if (slf == NULL) {
		return NULL;
	}

	return &slf->guid;
}

char const* DmSegment_getName(DmSegment const* slf) {
	if (slf == NULL) {
		return NULL;
	}

	return slf->info.unam;
}

double DmSegment_getLength(DmSegment const* slf) {
	if (slf == NULL) {
		return 0;
	}

	// NOTE: This assumes that the tempo messages are ordered from earliest to latest
	DmTimeSignature signature = {4, 4, 4};
	uint32_t offset = 0;
	double tempo = 100.;
	double duration = 0;

	for (unsigned i = 0; i < slf->messages.length; ++i) {
		DmMessage* msg = &slf->messages.data[i];
		if (msg->type != DmMessage_TEMPO) {
			continue;
		}

		uint32_t ticks = msg->time - offset;
		tempo = msg->tempo.tempo;

		duration += ticks / Dm_getTicksPerSecond(signature, tempo);
		offset = msg->time;
	}

	uint32_t ticks = slf->length - offset;
	duration += ticks / Dm_getTicksPerSecond(signature, tempo);

	return duration;
}
