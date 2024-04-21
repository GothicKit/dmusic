// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#pragma once
#include <stdbool.h>
#include <uchar.h>

#define DM_FOURCC(a, b, c, d) (((d) << 24U) | ((c) << 16U) | ((b) << 8U) | (a))
#define DM_FOURCC_RIFF DM_FOURCC('R', 'I', 'F', 'F')
#define DM_FOURCC_LIST DM_FOURCC('L', 'I', 'S', 'T')
#define DM_FOURCC_UNFO DM_FOURCC('U', 'N', 'F', 'O')
#define DM_FOURCC_UNAM DM_FOURCC('U', 'N', 'A', 'M')
#define DM_FOURCC_INFO DM_FOURCC('I', 'N', 'F', 'O')
#define DM_FOURCC_INAM DM_FOURCC('I', 'N', 'A', 'M')
#define DM_FOURCC_ICMT DM_FOURCC('I', 'C', 'M', 'T')
#define DM_FOURCC_ICOP DM_FOURCC('I', 'C', 'O', 'P')
#define DM_FOURCC_IENG DM_FOURCC('I', 'E', 'N', 'G')
#define DM_FOURCC_ISBJ DM_FOURCC('I', 'S', 'B', 'J')
#define DM_FOURCC_ISFT DM_FOURCC('I', 'S', 'F', 'T')
#define DM_FOURCC_DATE DM_FOURCC('D', 'A', 'T', 'E')
#define DM_FOURCC_DMRF DM_FOURCC('D', 'M', 'R', 'F')
#define DM_FOURCC_GUID DM_FOURCC('g', 'u', 'i', 'd')
#define DM_FOURCC_VERS DM_FOURCC('v', 'e', 'r', 's')
#define DM_FOURCC_REFH DM_FOURCC('r', 'e', 'f', 'h')
#define DM_FOURCC_NAME DM_FOURCC('n', 'a', 'm', 'e')
#define DM_FOURCC_FILE DM_FOURCC('f', 'i', 'l', 'e')
#define DM_FOURCC_SEGH DM_FOURCC('s', 'e', 'g', 'h')
#define DM_FOURCC_TRKL DM_FOURCC('t', 'r', 'k', 'l')
#define DM_FOURCC_DMTK DM_FOURCC('D', 'M', 'T', 'K')
#define DM_FOURCC_TRKH DM_FOURCC('t', 'r', 'k', 'h')
#define DM_FOURCC_TETR DM_FOURCC('t', 'e', 't', 'r')
#define DM_FOURCC_CMND DM_FOURCC('c', 'm', 'n', 'd')
#define DM_FOURCC_STTR DM_FOURCC('s', 't', 't', 'r')
#define DM_FOURCC_STRF DM_FOURCC('s', 't', 'r', 'f')
#define DM_FOURCC_STMP DM_FOURCC('s', 't', 'm', 'p')
#define DM_FOURCC_CORD DM_FOURCC('c', 'o', 'r', 'd')
#define DM_FOURCC_CRDH DM_FOURCC('c', 'r', 'd', 'h')
#define DM_FOURCC_CRDB DM_FOURCC('c', 'r', 'd', 'b')
#define DM_FOURCC_DMBT DM_FOURCC('D', 'M', 'B', 'T')
#define DM_FOURCC_LBDL DM_FOURCC('l', 'b', 'd', 'l')
#define DM_FOURCC_LBND DM_FOURCC('l', 'b', 'n', 'd')
#define DM_FOURCC_BDIH DM_FOURCC('b', 'd', 'i', 'h')
#define DM_FOURCC_DMBD DM_FOURCC('D', 'M', 'B', 'D')
#define DM_FOURCC_LBIL DM_FOURCC('l', 'b', 'i', 'l')
#define DM_FOURCC_LBIN DM_FOURCC('l', 'b', 'i', 'n')
#define DM_FOURCC_BINS DM_FOURCC('b', 'i', 'n', 's')
#define DM_FOURCC_DLID DM_FOURCC('d', 'l', 'i', 'd')
#define DM_FOURCC_COLH DM_FOURCC('c', 'o', 'l', 'h')
#define DM_FOURCC_PTBL DM_FOURCC('p', 't', 'b', 'l')
#define DM_FOURCC_LINS DM_FOURCC('l', 'i', 'n', 's')
#define DM_FOURCC_INS_ DM_FOURCC('i', 'n', 's', ' ')
#define DM_FOURCC_INSH DM_FOURCC('i', 'n', 's', 'h')
#define DM_FOURCC_LRGN DM_FOURCC('l', 'r', 'g', 'n')
#define DM_FOURCC_RGN_ DM_FOURCC('r', 'g', 'n', ' ')
#define DM_FOURCC_RGNH DM_FOURCC('r', 'g', 'n', 'h')
#define DM_FOURCC_WSMP DM_FOURCC('w', 's', 'm', 'p')
#define DM_FOURCC_WLNK DM_FOURCC('w', 'l', 'n', 'k')
#define DM_FOURCC_LART DM_FOURCC('l', 'a', 'r', 't')
#define DM_FOURCC_ART1 DM_FOURCC('a', 'r', 't', '1')
#define DM_FOURCC_WVPL DM_FOURCC('w', 'v', 'p', 'l')
#define DM_FOURCC_WAVE DM_FOURCC('w', 'a', 'v', 'e')
#define DM_FOURCC_FMT_ DM_FOURCC('f', 'm', 't', ' ')
#define DM_FOURCC_DATA DM_FOURCC('d', 'a', 't', 'a')
#define DM_FOURCC_WAVU DM_FOURCC('w', 'a', 'v', 'u')
#define DM_FOURCC_SMPL DM_FOURCC('s', 'm', 'p', 'l')
#define DM_FOURCC_STYH DM_FOURCC('s', 't', 'y', 'h')
#define DM_FOURCC_PART DM_FOURCC('p', 'a', 'r', 't')
#define DM_FOURCC_PRTH DM_FOURCC('p', 'r', 't', 'h')
#define DM_FOURCC_NOTE DM_FOURCC('n', 'o', 't', 'e')
#define DM_FOURCC_PTTN DM_FOURCC('p', 't', 't', 'n')
#define DM_FOURCC_PTNH DM_FOURCC('p', 't', 'n', 'h')
#define DM_FOURCC_RHTM DM_FOURCC('r', 'h', 't', 'm')
#define DM_FOURCC_PREF DM_FOURCC('p', 'r', 'e', 'f')
#define DM_FOURCC_PRFC DM_FOURCC('p', 'r', 'f', 'c')

typedef struct DmRiff {
	uint8_t const* mem;
	uint32_t len;
	uint32_t pos;
	uint32_t id;
	uint32_t typ;
} DmRiff;

typedef struct DmVersion {
	uint32_t ms;
	uint32_t ls;
} DmVersion;

typedef struct DmUnfo {
	char16_t const* unam;
} DmUnfo;

typedef struct DmInfo {
	char const* inam;
	char const* icmt;
	char const* icop;
	char const* ieng;
	char const* isbj;
	char const* isft;
	char const* date;
} DmInfo;

typedef struct DmReference {
	DmGuid class_id;
	uint32_t valid_data;
	DmGuid guid;
	char16_t const* name;
	char16_t const* file;
	DmVersion version;
} DmReference;

DMINT void DmGuid_parse(DmGuid* slf, DmRiff* rif);
DMINT void DmUnfo_parse(DmUnfo* slf, DmRiff* rif);
DMINT void DmInfo_parse(DmInfo* slf, DmRiff* rif);
DMINT void DmVersion_parse(DmVersion* slf, DmRiff* rif);
DMINT void DmReference_parse(DmReference* slf, DmRiff* rif);

DMINT bool DmRiff_init(DmRiff* slf, void const* buf, size_t len);
DMINT bool DmRiff_is(DmRiff const* slf, uint32_t id, uint32_t typ);
DMINT bool DmRiff_readChunk(DmRiff* slf, DmRiff* out);
DMINT uint32_t DmRiff_chunks(DmRiff* slf);
DMINT void DmRiff_read(DmRiff* slf, void* buf, size_t len);
DMINT void DmRiff_readByte(DmRiff* slf, uint8_t* buf);
DMINT void DmRiff_readWord(DmRiff* slf, uint16_t* buf);
DMINT void DmRiff_readShort(DmRiff* slf, int16_t* buf);
DMINT void DmRiff_readInt(DmRiff* slf, int32_t* buf);
DMINT void DmRiff_readDword(DmRiff* slf, uint32_t* buf);
DMINT void DmRiff_readDouble(DmRiff* slf, double* buf);
DMINT char const* DmRiff_readString(DmRiff* slf);
DMINT char16_t const* DmRiff_readStringUtf(DmRiff* slf);
DMINT void DmRiff_reportDone(DmRiff* slf);
