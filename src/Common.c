// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"

bool DmGuid_equals(DmGuid const* a, DmGuid const* b) {
	return memcmp(a->data, b->data, sizeof a->data) == 0;
}
