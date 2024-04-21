// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#pragma once
#include "dmusic.h"

#include "_Dls.h"
#include "_Riff.h"
#include "util/Array.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <uchar.h>

typedef enum DmPlaybackFlags {
	DmPlayback_REFTIME = 1 << 6,
	DmPlayback_SECONDARY = 1 << 7,
	DmPlayback_QUEUE = 1 << 8,
	DmPlayback_CONTROL = 1 << 9,
	DmPlayback_AFTER_PREPARE_TIME = 1 << 10,
	DmPlayback_GRID = 1 << 11,
	DmPlayback_BEAT = 1 << 12,
	DmPlayback_MEASURE = 1 << 13,
	DmPlayback_DEFAULT = 1 << 14,
	DmPlayback_NOINVALIDATE = 1 << 15,
	DmPlayback_ALIGN = 1 << 16,
	DmPlayback_VALID_START_BEAT = 1 << 17,
	DmPlayback_VALID_START_GRID = 1 << 18,
	DmPlayback_VALID_START_TICK = 1 << 19,
	DmPlayback_AUTOTRANSITION = 1 << 20,
	DmPlayback_AFTER_QUEUE_TIME = 1 << 21,
	DmPlayback_AFTER_LATENCY_TIME = 1 << 22,
	DmPlayback_SEGMENT_END = 1 << 23,
	DmPlayback_MARKER = 1 << 24,
	DmPlayback_TIMESIG_ALWAYS = 1 << 25,
	DmPlayback_USE_AUDIOPATH = 1 << 26,
	DmPlayback_VALID_START_MEASURE = 1 << 27,
	DmPlayback_INVALIDATE_PRI = 1 << 28
} DmPlaybackFlags;

typedef enum DmResolveFlags {
	DmResolve_AFTER_PREPARE_TIME = DmPlayback_AFTER_PREPARE_TIME,
	DmResolve_AFTER_QUEUE_TIME = DmPlayback_AFTER_QUEUE_TIME,
	DmResolve_AFTER_LATENCY_TIME = DmPlayback_AFTER_LATENCY_TIME,
	DmResolve_GRID = DmPlayback_GRID,
	DmResolve_BEAT = DmPlayback_BEAT,
	DmResolve_MEASURE = DmPlayback_MEASURE,
	DmResolve_MARKER = DmPlayback_MARKER,
	DmResolve_SEGMENT_END = DmPlayback_SEGMENT_END,
} DmResolveFlags;

typedef enum DmCommandType {
	DmCommand_GROOVE = 0,
	DmCommand_FILL = 1,
	DmCommand_INTRO = 2,
	DmCommand_BREAK = 3,
	DmCommand_END = 4,
	DmCommand_END_AND_INTRO = 5
} DmCommandType;

typedef enum DmPatternSelectMode {
	DmPatternSelect_RANDOM = 0,
	DmPatternSelect_REPEAT = 1,
	DmPatternSelect_SEQUENTIAL = 2,
	DmPatternSelect_RANDOM_START = 3,
	DmPatternSelect_NO_REPEAT = 4,
	DmPatternSelect_RANDOM_ROW = 5
} DmPatternSelectMode;

typedef struct DmTimeSignature {
	uint8_t beats_per_measure;
	uint8_t beat;
	uint16_t grids_per_beat;
} DmTimeSignature;

typedef struct DmResolver {
	DmLoaderResolverCallback* resolve;
	void* context;
} DmResolver;

struct DmStyle;

DmArray_DEFINE(DmResolverList, DmResolver);
DmArray_DEFINE(DmDlsCache, DmDls*);
DmArray_DEFINE(DmStyleCache, struct DmStyle*);

struct DmLoader {
	_Atomic size_t reference_count;
	bool autodownload;
	DmResolverList resolvers;

	DmStyleCache style_cache;
	DmDlsCache dls_cache;
};

typedef enum DmInstrumentFlags {
	DmInstrument_PATCH = (1 << 0),
	DmInstrument_BANK_SELECT = (1 << 1),
	DmInstrument_ASSIGN_PATCH = (1 << 3),
	DmInstrument_NOTE_RANGES = (1 << 4),
	DmInstrument_PAN = (1 << 5),
	DmInstrument_VOLUME = (1 << 6),
	DmInstrument_TRANSPOSE = (1 << 7),
	DmInstrument_GM = (1 << 8),
	DmInstrument_GS = (1 << 9),
	DmInstrument_XG = (1 << 10),
	DmInstrument_CHANNEL_PRIORITY = (1 << 11),
	DmInstrument_USE_DEFAULT_GM_SET = (1 << 12),
} DmInstrumentFlags;

typedef struct DmInstrument {
	uint32_t patch;
	uint32_t assign_patch;
	uint32_t note_ranges[4];
	uint32_t channel;
	DmInstrumentFlags flags;
	uint8_t pan;
	uint8_t volume;
	int16_t transpose;
	uint32_t channel_priority;
	DmReference reference;
	DmDls* dls;
} DmInstrument;

typedef struct DmBand {
	_Atomic size_t reference_count;

	DmGuid guid;
	DmUnfo info;

	size_t instrument_count;
	DmInstrument* instruments;
} DmBand;

typedef enum DmPlayModeFlags {
	DmPlayMode_KEY_ROOT = 1,
	DmPlayMode_CHORD_ROOT = 2,
	DmPlayMode_SCALE_INTERVALS = 4,
	DmPlayMode_CHORD_INTERVALS = 8,
	DmPlayMode_NONE = 16,
} DmPlayModeFlags;

typedef struct DmNote {
	uint32_t grid_start;
	uint32_t variation;
	uint32_t duration;
	int16_t time_offset;
	uint16_t music_value;
	uint8_t velocity;
	uint8_t time_range;
	uint8_t duration_range;
	uint8_t velocity_range;
	uint8_t inversion_id;
	DmPlayModeFlags play_mode_flags;
} DmNote;

typedef struct DmPart {
	DmUnfo info;

	DmTimeSignature time_signature;
	uint32_t variation_choices[32];

	DmGuid part_id;
	uint16_t length_measures;
	DmPlayModeFlags play_mode_flags;
	uint8_t invert_upper;
	uint8_t invert_lower;

	uint32_t note_count;
	DmNote* notes;
} DmPart;

typedef struct DmPartReference {
	DmGuid part_id;
	DmUnfo info;

	uint16_t logical_part_id;
	uint8_t variation_lock_id;
	uint8_t subchord_level;
	uint8_t priority;
	uint8_t random_variation;
} DmPartReference;

DmArray_DEFINE(DmPartReferenceList, DmPartReference);

typedef struct DmPattern {
	DmUnfo info;

	DmTimeSignature time_signature;
	uint8_t groove_bottom;
	uint8_t groove_top;
	uint16_t embellishment;
	uint16_t length_measures;

	uint32_t rhythm_len;
	uint32_t* rhythm;

	DmPartReferenceList parts;
} DmPattern;

DmArray_DEFINE(DmPartList, DmPart);
DmArray_DEFINE(DmPatternList, DmPattern);
DmArray_DEFINE(DmBandList, DmBand*);

typedef struct DmStyle {
	_Atomic size_t reference_count;
	void* backing_memory;

	DmGuid guid;
	DmUnfo info;
	DmVersion version;
	DmTimeSignature time_signature;
	double tempo;

	DmBandList bands;
	DmPartList parts;
	DmPatternList patterns;
} DmStyle;

typedef enum DmMessageType {
	DmMessage_COMMAND,
	DmMessage_TEMPO,
	DmMessage_CHORD,
	DmMessage_BAND,
	DmMessage_STYLE,
} DmMessageType;

typedef struct DmMessage_Tempo {
	DmMessageType type;
	uint32_t time;
	double tempo;
} DmMessage_Tempo;

typedef struct DmMessage_Command {
	DmMessageType type;
	uint32_t time;
	uint16_t measure;
	uint8_t beat;
	DmCommandType command;
	uint8_t groove_level;
	uint8_t groove_range;
	DmPatternSelectMode repeat_mode;
} DmMessage_Command;

typedef struct DmMessage_Chord {
	DmMessageType type;
	uint32_t time;

	char16_t name[16];
	uint16_t measure;
	uint8_t beat;
	bool silent;

	uint32_t subchord_count;
	struct {
		uint32_t chord_pattern;
		uint32_t scale_pattern;
		uint32_t inversion_points;
		uint32_t levels;
		uint8_t chord_root;
		uint8_t scale_root;
	} subchords[4];
} DmMessage_Chord;

typedef struct DmMessage_Band {
	DmMessageType type;
	uint32_t time;
	DmBand* band;
} DmMessage_Band;

typedef struct DmMessage_Style {
	DmMessageType type;
	uint32_t time;
	DmReference reference;
	DmStyle* style;
} DmMessage_Style;

typedef union DmMessage {
	struct {
		DmMessageType type;
		uint32_t time;
	};

	DmMessage_Tempo tempo;
	DmMessage_Command command;
	DmMessage_Chord chord;
	DmMessage_Band band;
	DmMessage_Style style;
} DmMessage;

DmArray_DEFINE(DmMessageList, DmMessage);

struct DmSegment {
	_Atomic size_t reference_count;
	void* backing_memory;

	/// \brief Number of repetitions.
	uint32_t repeats;

	/// \brief Length of the segment in music time.
	uint32_t length;

	/// \brief Start of playback, normally 0, in music time.
	uint32_t play_start;

	/// \brief Start of the looping portion, normally 0, in music time.
	uint32_t loop_start;

	/// \brief End of the looping portion in music time.
	/// \note Must be greater than #play_start, or zero to loop the entire segment.
	uint32_t loop_end;

	/// \brief Default resolution.
	DmResolveFlags resolution;

	DmGuid guid;
	DmUnfo info;
	DmVersion version;

	DmMessageList messages;
};

DMINT void* Dm_alloc(size_t len);
DMINT void Dm_free(void* ptr);
DMINT void Dm_report(DmLogLevel lvl, char const* fmt, ...);

DMINT bool DmGuid_equals(DmGuid const* a, DmGuid const* b);
DMINT void DmTimeSignature_parse(DmTimeSignature* slf, DmRiff* rif);

DMINT DmResult DmLoader_getStyle(DmLoader* slf, DmReference const* ref, DmStyle** sty);
DMINT DmResult DmLoader_getDownloadableSound(DmLoader* slf, DmReference const* ref, DmDls** snd);

DMINT DmResult DmSegment_create(DmSegment** slf);
DMINT DmResult DmSegment_parse(DmSegment* slf, void* buf, size_t len);

DMINT void DmMessage_copy(DmMessage* slf, DmMessage* cpy);
DMINT void DmMessage_free(DmMessage* slf);

DMINT DmResult DmBand_create(DmBand** slf);
DMINT DmBand* DmBand_retain(DmBand* slf);
DMINT void DmBand_release(DmBand* slf);
DMINT DmResult DmBand_parse(DmBand* slf, DmRiff* rif);
DMINT DmResult DmBand_download(DmBand* slf, DmLoader* loader);
DMINT void DmInstrument_free(DmInstrument* slf);

DMINT DmResult DmStyle_create(DmStyle** slf);
DMINT DmStyle* DmStyle_retain(DmStyle* slf);
DMINT void DmStyle_release(DmStyle* slf);
DMINT DmResult DmStyle_parse(DmStyle* slf, void* buf, size_t len);
DMINT DmResult DmStyle_download(DmStyle* slf, DmLoader* loader);

DMINT void DmPart_init(DmPart* slf);
DMINT void DmPart_free(DmPart* slf);

DMINT void DmPartReference_init(DmPartReference* slf);
DMINT void DmPartReference_free(DmPartReference* slf);

DMINT void DmPattern_init(DmPattern* slf);
DMINT void DmPattern_free(DmPattern* slf);
