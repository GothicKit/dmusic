// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"

void DmMessage_copy(DmMessage* slf, DmMessage* cpy, int64_t time) {
	if (slf == NULL || cpy == NULL) {
		return;
	}

	memcpy(cpy, slf, sizeof *slf);

	if (slf->type == DmMessage_BAND) {
		cpy->band.band = DmBand_retain(slf->band.band);
	} else if (slf->type == DmMessage_STYLE) {
		cpy->style.style = DmStyle_retain(slf->style.style);
	}

	if (time >= 0) {
		cpy->time = time;
	}
}

void DmMessage_free(DmMessage* slf) {
	if (slf == NULL) {
		return;
	}

	if (slf->type == DmMessage_BAND) {
		DmBand_release(slf->band.band);
	} else if (slf->type == DmMessage_STYLE) {
		DmStyle_release(slf->style.style);
	}
}

enum {
	DmInt_MESSAGE_QUEUE_GROWTH = 100,
};

static DmResult DmMessageQueue_growBlocks(DmMessageQueue* slf) {
	struct DmMessageQueueBlock* new_block =
	    Dm_alloc(sizeof *slf->blocks + sizeof(DmMessageQueueItem) * DmInt_MESSAGE_QUEUE_GROWTH);
	if (new_block == NULL) {
		return DmResult_MEMORY_EXHAUSTED;
	}

	new_block->next = slf->blocks;
	slf->blocks = new_block;

	DmMessageQueueItem* items = (DmMessageQueueItem*) (new_block + 1);
	for (size_t i = 0; i < DmInt_MESSAGE_QUEUE_GROWTH; ++i) {
		items[i].next = slf->free;
		slf->free = &items[i];
	}

	return DmResult_SUCCESS;
}

static DmResult DmMessageQueue_growQueue(DmMessageQueue* slf) {
	size_t new_capacity =
	    slf->queue_capacity != 0 ? slf->queue_capacity + DmInt_MESSAGE_QUEUE_GROWTH : DmInt_MESSAGE_QUEUE_GROWTH;
	DmMessageQueueItem** new_queue = Dm_alloc(sizeof(DmMessageQueueItem*) * new_capacity);
	if (new_queue == NULL) {
		return DmResult_MEMORY_EXHAUSTED;
	}

	if (slf->queue != NULL) {
		memcpy(new_queue, slf->queue, sizeof(DmMessageQueueItem*) * slf->queue_length);
		Dm_free(slf->queue);
	}

	slf->queue_capacity = new_capacity;
	slf->queue = new_queue;

	return DmResult_SUCCESS;
}

static int DmMessage_sort(DmMessage* a, DmMessage* b) {
	if (a->time < b->time) {
		return -1;
	}

	if (a->time > b->time) {
		return 1;
	}

	if (a->type < b->type) {
		return -1;
	}

	if (a->type > b->type) {
		return 1;
	}

	return 0;
}

static void DmMessageQueue_heapInsert(DmMessageQueue* slf) {
	size_t slf_i = slf->queue_length - 1;

	while (slf_i > 0) {
		size_t parent_i = (slf_i - 1) / 2;

		int sort = DmMessage_sort(&slf->queue[slf_i]->data, &slf->queue[parent_i]->data);
		if (sort >= 0) {
			break;
		}

		DmMessageQueueItem* oth = slf->queue[slf_i];
		slf->queue[slf_i] = slf->queue[parent_i];
		slf->queue[parent_i] = oth;
		slf_i = parent_i;
	}
}

static void DmMessageQueue_heapRemove(DmMessageQueue* slf) {
	slf->queue[0] = slf->queue[slf->queue_length];

	size_t slf_i = 0;
	while (slf_i < slf->queue_length - 1) {
		size_t child_1 = 2 * slf_i + 1;
		size_t child_2 = 2 * slf_i + 2;

		int sort_1 = -2;
		int sort_2 = -2;

		if (child_1 < slf->queue_length) {
			sort_1 = DmMessage_sort(&slf->queue[slf_i]->data, &slf->queue[child_1]->data);
		}

		if (child_2 < slf->queue_length) {
			sort_2 = DmMessage_sort(&slf->queue[slf_i]->data, &slf->queue[child_2]->data);
		}

		if ((sort_1 == -2 && sort_2 == -2) || (sort_1 <= 0 && sort_2 <= 0))  {
			break;
		}

		size_t swap;
		if (sort_1 == -2 && sort_2 > 0) {
			swap = child_2;
		} else if (sort_2 == -1 && sort_1 > 0) {
			swap = child_1;
		} else {
			int sort_1_2 = DmMessage_sort(&slf->queue[child_1]->data, &slf->queue[child_2]->data);
			if (sort_1_2 < 0) {
				swap = child_1;
			} else {
				swap = child_2;
			}
		}

		DmMessageQueueItem* oth = slf->queue[slf_i];
		slf->queue[slf_i] = slf->queue[swap];
		slf->queue[swap] = oth;
		slf_i = swap;
	}
}

DmResult DmMessageQueue_init(DmMessageQueue* slf) {
	if (slf == NULL) {
		return DmResult_INVALID_ARGUMENT;
	}

	DmResult rv = DmMessageQueue_growBlocks(slf);
	if (rv != DmResult_SUCCESS) {
		return rv;
	}

	rv = DmMessageQueue_growQueue(slf);
	if (rv != DmResult_SUCCESS) {
		return rv;
	}

	return DmResult_SUCCESS;
}

void DmMessageQueue_free(DmMessageQueue* slf) {
	if (slf == NULL) {
		return;
	}

	for (size_t i = 0; i < slf->queue_length; ++i) {
		DmMessage_free(&slf->queue[i]->data);
	}

	Dm_free(slf->queue);

	while (slf->blocks != NULL) {
		struct DmMessageQueueBlock* next = slf->blocks->next;
		Dm_free(slf->blocks);
		slf->blocks = next;
	}
}

static DmMessageQueueItem* DmMessageQueue_getAt(DmMessageQueue* slf, size_t time, DmMessageType type) {
	for (size_t i = 0; i < slf->queue_length; ++i) {
		int64_t delta = (int64_t) slf->queue[i]->data.time - (int64_t)time;
		int64_t d = delta < 0 ? (delta * -1) : delta;
		if (d < 10 && slf->queue[i]->data.type == type) {
			return slf->queue[i];
		}
	}

	return NULL;
}

DmResult DmMessageQueue_add(DmMessageQueue* slf, DmMessage* msg, uint32_t time, DmQueueConflictResolution cr) {
	if (slf == NULL || msg == NULL) {
		return DmResult_INVALID_ARGUMENT;
	}

	if (cr != DmQueueConflict_APPEND) {
		DmMessageQueueItem* itm = DmMessageQueue_getAt(slf, time, msg->type);
		if (itm != NULL) {
			if (cr == DmQueueConflict_KEEP) {
				return DmResult_SUCCESS;
			}

			DmMessage_free(&itm->data);
			DmMessage_copy(msg, &itm->data, time);
			return DmResult_SUCCESS;
		}
	}

	if (slf->queue_capacity == slf->queue_length) {
		DmResult rv = DmMessageQueue_growQueue(slf);
		if (rv != DmResult_SUCCESS) {
			return rv;
		}
	}

	if (slf->free == NULL) {
		DmResult rv = DmMessageQueue_growBlocks(slf);
		if (rv != DmResult_SUCCESS) {
			return rv;
		}
	}

	DmMessageQueueItem* itm = slf->free;
	slf->free = slf->free->next;

	DmMessage_copy(msg, &itm->data, time);
	itm->next = NULL;

	slf->queue[slf->queue_length++] = itm;
	DmMessageQueue_heapInsert(slf);
	return DmResult_SUCCESS;
}

bool DmMessageQueue_get(DmMessageQueue* slf, DmMessage* msg) {
	if (slf == NULL || msg == NULL) {
		return false;
	}

	if (slf->queue_length == 0) {
		return false;
	}

	memcpy(msg, &slf->queue[0]->data, sizeof *msg);
	return true;
}

void DmMessageQueue_pop(DmMessageQueue* slf) {
	if (slf == NULL || slf->queue_length == 0) {
		return;
	}

	DmMessage_free(&slf->queue[0]->data);
	slf->queue[0]->next = slf->free;
	slf->free = slf->queue[0];
	slf->queue_length -= 1;

	DmMessageQueue_heapRemove(slf);
}

void DmMessageQueue_clear(DmMessageQueue* slf) {
	if (slf == NULL || slf->queue_length == 0) {
		return;
	}

	for (size_t i = 0; i < slf->queue_length; ++i) {
		DmMessage_free(&slf->queue[i]->data);
		slf->queue[i]->next = slf->free;
		slf->free = slf->queue[i];
	}

	slf->queue_length = 0;
}

