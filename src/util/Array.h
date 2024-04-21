// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#pragma once
#include "dmusic.h"

#include <stddef.h>
#include <string.h>

#define DmArray_DEFINE(Name, Type)                                                                                     \
	typedef struct {                                                                                                   \
		Type* data;                                                                                                    \
		size_t length;                                                                                                 \
		size_t capacity;                                                                                               \
	} Name;                                                                                                            \
                                                                                                                       \
	DMINT void Name##_init(Name* slf);                                                                                 \
	DMINT void Name##_free(Name* slf);                                                                                 \
	DMINT DmResult Name##_add(Name* slf, Type val);                                                                    \
	DMINT Type Name##_get(Name const* slf, size_t i)

#define DmArray_IMPLEMENT(Name, Type, Delete)                                                                          \
	void Name##_init(Name* slf) {                                                                                      \
		if (slf == NULL) {                                                                                             \
			return;                                                                                                    \
		}                                                                                                              \
                                                                                                                       \
		slf->data = NULL;                                                                                              \
		slf->length = 0;                                                                                               \
		slf->capacity = 0;                                                                                             \
	}                                                                                                                  \
                                                                                                                       \
	void Name##_free(Name* slf) {                                                                                      \
		if (slf == NULL) {                                                                                             \
			return;                                                                                                    \
		}                                                                                                              \
                                                                                                                       \
		for (size_t i = 0; i < slf->length; ++i) {                                                                     \
			Type* itm = slf->data + i;                                                                                 \
			Delete;                                                                                                    \
			(void) itm;                                                                                                \
		}                                                                                                              \
                                                                                                                       \
		Dm_free(slf->data);                                                                                            \
		slf->data = NULL;                                                                                              \
		slf->length = 0;                                                                                               \
		slf->capacity = 0;                                                                                             \
	}                                                                                                                  \
                                                                                                                       \
	DmResult Name##_add(Name* slf, Type val) {                                                                         \
		if (slf == NULL) {                                                                                             \
			return DmResult_INVALID_ARGUMENT;                                                                          \
		}                                                                                                              \
                                                                                                                       \
		if (slf->data == NULL || slf->length + 1 > slf->capacity) {                                                    \
			size_t newSize = slf->capacity == 0 ? 10 : slf->capacity * 2;                                              \
			Type* newData = Dm_alloc(sizeof(Type) * newSize);                                                          \
			if (newData == NULL) {                                                                                     \
				return DmResult_MEMORY_EXHAUSTED;                                                                      \
			}                                                                                                          \
                                                                                                                       \
			if (slf->data != NULL) {                                                                                   \
				memcpy(newData, slf->data, slf->length * sizeof(Type));                                                \
			}                                                                                                          \
                                                                                                                       \
			Dm_free(slf->data);                                                                                        \
			slf->data = newData;                                                                                       \
			slf->capacity = newSize;                                                                                   \
		}                                                                                                              \
                                                                                                                       \
		slf->data[slf->length++] = val;                                                                                \
		return DmResult_SUCCESS;                                                                                       \
	}                                                                                                                  \
                                                                                                                       \
	Type Name##_get(Name const* slf, size_t i) {                                                                       \
		return slf->data[i];                                                                                           \
	}                                                                                                                  \
                                                                                                                       \
	struct DmArray_dummy##Name {                                                                                       \
		int _;                                                                                                         \
	}
