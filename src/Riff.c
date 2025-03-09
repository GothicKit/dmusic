// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"

#include <string.h>

#define DmRiff_HEADER_SIZE sizeof(uint32_t) * 3

bool DmRiff_init(DmRiff* slf, void const* buf, size_t len) {
	if (slf == NULL || buf == NULL || len < DmRiff_HEADER_SIZE) {
		return false;
	}

	uint32_t const* rd = buf;
	slf->id = *(rd++);
	slf->len = *(rd++);
	slf->typ = *(rd++);

	if (len - 2 * sizeof(uint32_t) < slf->len) {
		return false;
	}

	slf->mem = (uint8_t const*) rd;
	slf->pos = 0;
	slf->len = slf->len - sizeof(uint32_t);

	return true;
}

bool DmRiff_is(DmRiff const* slf, uint32_t id, uint32_t typ) {
	if (slf == NULL) {
		return false;
	}

	return slf->id == id && slf->typ == typ;
}

bool DmRiff_readChunk(DmRiff* slf, DmRiff* out) {
	if (slf == NULL || out == NULL) {
		return false;
	}

	// Cover odd case where ISFT reads to end of file,
	// but chunk reports over-read
	if (slf->pos > slf->len) {
		return false;
	}
	size_t remaining = slf->len - slf->pos;
	if (remaining < sizeof(uint32_t) * 2) {
		return false;
	}

	out->pos = 0;
	out->typ = 0;

	DmRiff_readDword(slf, &out->id);
	DmRiff_readDword(slf, &out->len);

	if (out->id == DM_FOURCC_RIFF || out->id == DM_FOURCC_LIST) {
		DmRiff_readDword(slf, &out->typ);
		out->len -= sizeof(uint32_t);
	}

	out->mem = slf->mem + slf->pos;
	slf->pos += out->len + (out->len % 2);
	return true;
}

uint32_t DmRiff_chunks(DmRiff* slf) {
	if (slf == NULL) {
		return 0;
	}

	DmRiff tmp;
	uint32_t pos = slf->pos;
	uint32_t len = 0;

	slf->pos = 0;

	while (DmRiff_readChunk(slf, &tmp)) {
		len += 1;
	}

	slf->pos = pos;

	return len;
}

void DmRiff_read(DmRiff* slf, void* buf, size_t len) {
	if (slf == NULL || buf == NULL) {
		Dm_report(DmLogLevel_ERROR, "DmRiff: Internal error: DmRiff_read called with `NULL` pointer");
		return;
	}

	if (slf->pos + len > slf->len) {
		memset(buf, 0, len);
		Dm_report(DmLogLevel_ERROR,
		          "DmRiff: Tried to read %d bytes from chunk %.4s:[%.4s] but only %d bytes are available",
		          len,
		          &slf->id,
		          &slf->typ,
		          slf->len - slf->pos);
		return;
	}

	memcpy(buf, slf->mem + slf->pos, len);
	slf->pos += len;
}

void DmRiff_readByte(DmRiff* slf, uint8_t* buf) {
	DmRiff_read(slf, buf, sizeof *buf);
}

void DmRiff_readWord(DmRiff* slf, uint16_t* buf) {
	DmRiff_read(slf, buf, sizeof *buf);
}

void DmRiff_readShort(DmRiff* slf, int16_t* buf) {
	DmRiff_read(slf, buf, sizeof *buf);
}

void DmRiff_readInt(DmRiff* slf, int32_t* buf) {
	DmRiff_read(slf, buf, sizeof *buf);
}

void DmRiff_readDword(DmRiff* slf, uint32_t* buf) {
	DmRiff_read(slf, buf, sizeof *buf);
}

void DmRiff_readDouble(DmRiff* slf, double* buf) {
	DmRiff_read(slf, buf, sizeof *buf);
}

char const* DmRiff_readString(DmRiff* slf) {
	if (slf == NULL) {
		return "";
	}

	char const* str = (char const*) slf->mem + slf->pos;
	slf->pos += strlen(str) + 1;

	return str;
}

uint16_t* DmRiff_readStringUtf(DmRiff* slf) {
	if (slf == NULL) {
		return NULL;
	}

	uint16_t* mem = (uint16_t*) slf->mem;
	uint16_t* str = (uint16_t*) slf->mem + slf->pos;

	while (mem[slf->pos] != 0) {
		slf->pos += 1;
	}

	slf->pos += 1;
	return str;
}

void DmRiff_reportDone(DmRiff* slf) {
	if (slf == NULL) {
		return;
	}

	if (slf->pos == slf->len) {
		return;
	}

	Dm_report(DmLogLevel_WARN,
	          "DmRiff: Chunk %.4s:[%.4s] not fully parsed, %d bytes remaining",
	          &slf->id,
	          &slf->typ,
	          slf->len - slf->pos);
}
