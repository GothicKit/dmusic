// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#pragma once
#include "dmusic.h"

#include "_Dls.h"
#include "_Riff.h"
#include "thread/Thread.h"
#include "util/Array.h"

#include <tsf.h>

#include <stdatomic.h>
#include <stdbool.h>
#include <uchar.h>

typedef enum DmResolveFlags {
	DmResolve_AFTER_PREPARE_TIME = 1 << 10,
	DmResolve_AFTER_QUEUE_TIME = 1 << 21,
	DmResolve_AFTER_LATENCY_TIME = 1 << 22,
	DmResolve_GRID = 1 << 11,
	DmResolve_BEAT = 1 << 12,
	DmResolve_MEASURE =1 << 13,
	DmResolve_MARKER = 1 << 24,
	DmResolve_SEGMENT_END = 1 << 23,
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
	mtx_t cache_lock;

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

	DmDls* dls_collection;
	DmDlsInstrument* dls;
} DmInstrument;

typedef struct DmBand {
	_Atomic size_t reference_count;

	DmGuid guid;
	DmUnfo info;

	size_t instrument_count;
	DmInstrument* instruments;
} DmBand;

typedef enum DmPlayModeFlags {
	DmPlayMode_FIXED = 0, // Special case: music value == midi value
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

typedef enum DmCurveType {
	DmCurveType_PITCH_BEND = 0x03,
	DmCurveType_CONTROL_CHANGE = 0x04,
	DmCurveType_MONO_AFTERTOUCH = 0x05,
	DmCurveType_POLY_AFTERTOUCH = 0x06,
} DmCurveType;

typedef enum DmCurveShape {
	DmCurveShape_LINEAR = 0,
	DmCurveShape_INSTANT = 1,
	DmCurveShape_EXP = 2,
	DmCurveShape_LOG = 3,
	DmCurveShape_SINE = 4
} DmCurveShape;

typedef enum DmCurveFlags {
	DmCurveFlags_RESET = 1,
} DmCurveFlags;

typedef struct DmCurve {
	uint32_t grid_start;
	uint32_t variation;
	uint32_t duration;
	uint32_t reset_duration;
	short time_offset;
	short start_value;
	short end_value;
	short reset_value;
	DmCurveType event_type;
	DmCurveShape curve_shape;
	uint8_t cc_data;
	uint8_t flags;
} DmCurve;

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

	uint32_t curve_count;
	DmCurve* curves;
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

// NOTE: Ordered by priority
typedef enum DmMessageType {
	DmMessage_NOTE = 0,
	DmMessage_CONTROL,
	DmMessage_PITCH_BEND,

	DmMessage_SEGMENT,
	DmMessage_STYLE,
	DmMessage_BAND,
	DmMessage_TEMPO,
	DmMessage_CHORD,
	DmMessage_COMMAND,
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
	struct DmSubChord {
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

typedef struct DmMessage_SegmentChange {
	DmMessageType type;
	uint32_t time;
	DmSegment* segment;
	uint32_t loop;
} DmMessage_SegmentChange;

typedef struct DmMessage_Note {
	DmMessageType type;
	uint32_t time;

	bool on;
	uint8_t note;
	uint8_t velocity;
	uint32_t channel;
} DmMessage_Note;

typedef struct DmMessage_Control {
	DmMessageType type;
	uint32_t time;

	uint8_t control;
	float value;
	uint32_t channel;

	bool reset;
	float reset_value;
} DmMessage_Control;

typedef struct DmMessage_PitchBend {
	DmMessageType type;
	uint32_t time;

	uint32_t channel;
	int value;

	bool reset;
	int reset_value;
} DmMessage_PitchBend;

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
	DmMessage_SegmentChange segment;
	DmMessage_Note note;
	DmMessage_Control control;
	DmMessage_PitchBend pitch_bend;
} DmMessage;

DmArray_DEFINE(DmMessageList, DmMessage);

typedef struct DmSynthChannel {
	tsf* synth;
	float volume;
	float volume_reset;
	float pan_reset;
	int pitch_bend_reset;
} DmSynthChannel;

typedef struct DmSynth {
	size_t channel_count;
	DmSynthChannel* channels;
	DmBand* band;
	float volume;
} DmSynth;

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

	bool downloaded;
};

typedef enum DmQueueConflictResolution {
	DmQueueConflict_KEEP,
	DmQueueConflict_REPLACE,
	DmQueueConflict_APPEND,
} DmQueueConflictResolution;

typedef struct DmMessageQueueItem {
	struct DmMessageQueueItem* next;
	DmMessage data;
} DmMessageQueueItem;

typedef struct DmMessageQueue {
	size_t queue_length;
	size_t queue_capacity;
	DmMessageQueueItem** queue;

	DmMessageQueueItem* free;
	struct DmMessageQueueBlock {
		struct DmMessageQueueBlock* next;
	}* blocks;
} DmMessageQueue;

struct DmPerformance {
	_Atomic size_t reference_count;
	mtx_t mod_lock;

	DmMessageQueue control_queue;
	DmMessageQueue music_queue;

	DmSegment* segment;
	uint32_t segment_start;
	DmStyle* style;
	DmBand* band;
	DmSynth synth;

	uint32_t variation;
	uint32_t time;
	uint8_t groove;
	uint8_t groove_range;
	double tempo;
	DmMessage_Chord chord;
	DmTimeSignature time_signature;

	int pitch_bend_reset;
	float volume_reset;
};

DMINT void* Dm_alloc(size_t len);
DMINT void Dm_free(void* ptr);
DMINT void Dm_report(DmLogLevel lvl, char const* fmt, ...);

DMINT size_t max_usize(size_t a, size_t b);
DMINT int32_t max_s32(int32_t a, int32_t b);
DMINT float clamp_f32(float val, float min, float max);
DMINT DmCommandType Dm_embellishmentToCommand(DmEmbellishmentType embellishment);
DMINT bool DmGuid_equals(DmGuid const* a, DmGuid const* b);
DMINT void DmTimeSignature_parse(DmTimeSignature* slf, DmRiff* rif);

DMINT uint32_t Dm_getBeatLength(DmTimeSignature sig);
DMINT uint32_t Dm_getMeasureLength(DmTimeSignature sig);
DMINT double Dm_getTicksPerSample(DmTimeSignature time_signature, double beats_per_minute, uint32_t sample_rate);
DMINT uint32_t Dm_getTimeOffset(uint32_t grid_start, int32_t time_offset, DmTimeSignature sig);

DMINT DmResult DmLoader_getStyle(DmLoader* slf, DmReference const* ref, DmStyle** sty);
DMINT DmResult DmLoader_getDownloadableSound(DmLoader* slf, DmReference const* ref, DmDls** snd);

DMINT DmResult DmSegment_create(DmSegment** slf);
DMINT DmResult DmSegment_parse(DmSegment* slf, void* buf, size_t len);

DMINT void DmMessage_copy(DmMessage* slf, DmMessage* cpy, uint32_t time);
DMINT void DmMessage_free(DmMessage* slf);
DMINT DmResult DmMessageQueue_init(DmMessageQueue* slf);
DMINT void DmMessageQueue_free(DmMessageQueue* slf);
DMINT void DmMessageQueue_add(DmMessageQueue* slf, DmMessage* msg, uint32_t time, DmQueueConflictResolution cr);
DMINT bool DmMessageQueue_get(DmMessageQueue* slf, DmMessage* msg);
DMINT void DmMessageQueue_pop(DmMessageQueue* slf);
DMINT void DmMessageQueue_clear(DmMessageQueue* slf);

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
DMINT DmPart* DmStyle_findPart(DmStyle* slf, DmPartReference* pref);

DMINT void DmPart_init(DmPart* slf);
DMINT void DmPart_free(DmPart* slf);
DMINT uint32_t DmPart_getValidVariationCount(DmPart* slf);

DMINT void DmPartReference_init(DmPartReference* slf);
DMINT void DmPartReference_free(DmPartReference* slf);

DMINT void DmPattern_init(DmPattern* slf);
DMINT void DmPattern_free(DmPattern* slf);

DMINT void DmSynth_init(DmSynth* slf);
DMINT void DmSynth_free(DmSynth* slf);
DMINT void DmSynth_reset(DmSynth* slf);

DMINT void DmSynth_setVolume(DmSynth* slf, float vol);
DMINT DmResult DmSynth_createTsfForInstrument(DmInstrument* slf, tsf** out);
DMINT void DmSynth_sendBandUpdate(DmSynth* slf, DmBand* band);
DMINT void DmSynth_sendControl(DmSynth* slf, uint32_t channel, uint8_t control, float value);
DMINT void DmSynth_sendControlReset(DmSynth* slf, uint32_t channel, uint8_t control, float reset);
DMINT void DmSynth_sendPitchBend(DmSynth* slf, uint32_t channel, int bend);
DMINT void DmSynth_sendPitchBendReset(DmSynth* slf, uint32_t channel, int reset);
DMINT void DmSynth_sendNoteOn(DmSynth* slf, uint32_t channel, uint8_t note, uint8_t velocity);
DMINT void DmSynth_sendNoteOff(DmSynth* slf, uint32_t channel, uint8_t note);
DMINT void DmSynth_sendNoteOffAll(DmSynth* slf, uint32_t channel);
DMINT void DmSynth_sendNoteOffEverything(DmSynth* slf);
DMINT size_t DmSynth_render(DmSynth* slf, void* buf, size_t len, DmRenderOptions fmt);
