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
			rv = DmBand_download(&msg->band.band, loader);
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

	return rv;
}

