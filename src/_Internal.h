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

typedef enum DmResolveFlags {
	DmResolve_AFTER_PREPARE_TIME = 1 << 10,
	DmResolve_AFTER_QUEUE_TIME = 1 << 21,
	DmResolve_AFTER_LATENCY_TIME = 1 << 22,
	DmResolve_GRID = 1 << 11,
	DmResolve_BEAT = 1 << 12,
	DmResolve_MEASURE = 1 << 13,
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
	mtx_t lock;

	bool autodownload;
	DmResolverList resolvers;

	DmStyleCache style_cache;
	DmDlsCache dls_cache;
};

typedef enum DmInstrumentFlags {
	/// \brief The `patch` member is valid
	DmInstrument_VALID_PATCH = (1 << 0),

	/// \brief The `patch` member contains a valid bank and preset select
	DmInstrument_VALID_BANKSELECT = (1 << 1),

	/// \brief The `assign_patch` member is valid
	DmInstrument_VALID_ASSIGN_PATCH = (1 << 3),

	/// \brief The `note_ranges` member is valid
	DmInstrument_VALID_NOTE_RANGES = (1 << 4),

	/// \brief The `pan` member is valid
	DmInstrument_VALID_PAN = (1 << 5),

	/// \brief The `volume` member is valid
	DmInstrument_VALID_VOLUME = (1 << 6),

	/// \brief The `transpose` member is valid
	DmInstrument_VALID_TRANSPOSE = (1 << 7),

	/// \brief The `channel_priority` member is valid
	DmInstrument_VALID_CHANNEL_PRIORITY = (1 << 11),

	/// \brief The instrument is from the General MIDI collection
	DmInstrument_GENERAL_MIDI = (1 << 8),

	/// \brief The instrument is from the Roland GS collection
	DmInstrument_ROLAND_GS = (1 << 9),

	/// \brief The instrument is from the Yamaha XG collection
	DmInstrument_YAMAHA_XG = (1 << 10),

	/// \brief The instrument is from any of the predefined collections
	DmInstrument_PREDEFINED_COLLECTION = DmInstrument_GENERAL_MIDI | DmInstrument_ROLAND_GS | DmInstrument_YAMAHA_XG,

	/// \brief The General MIDI collection should be loaded in software
	///        even if the hardware supports it natively.
	DmInstrument_USE_DEFAULT_GM_SET = (1 << 12),
} DmInstrumentOptions;

typedef struct DmInstrument {
	/// \brief The bank and patch number of the instrument in the referenced DLS file.
	/// \note Original name: `dwPatch`
	/// \see #reference
	uint32_t patch;

	uint32_t assign_patch;
	uint32_t note_ranges[4];

	/// \brief The performance channel the instrument plays on.
	/// \note Original name: `dwPChannel`
	uint32_t channel;

	/// \brief Flag set identifying valid fields and general options of the instrument.
	///
	/// Before using the value of any field of the instrument (except #reference and #dls), check this
	/// field identifying whether each member is valid.
	///
	/// \note Original name: `dwFlags`
	DmInstrumentOptions options;

	/// \brief The left-right pan of the instrument.
	///
	/// The valid range for this field is 0-127 where 0 indicates full left pan and 127 indicates full right pan.
	/// The value range constrained is enforced upon parsing.
	///
	/// \note Original name: `bPan`
	uint8_t pan;

	/// \brief The volume of the instrument.
	///
	/// The valid range for this field is 0-127 where 0 indicates silence and 127 indicates maximum volume.
	/// The value range constrained is enforced upon parsing.
	///
	/// \note Original name: `bVolume`
	uint8_t volume;

	/// \brief The number of semitones to transpose all notes played by the instrument by.
	/// \note Original name: `nTranspose`
	int16_t transpose;

	/// \brief The priority of the instrument over other instruments if no additional
	///        voices can be allocated by the synthesizer.
	///
	/// > The number of notes that can be played simultaneously is limited by the number of voices available on the
	/// port.
	///   A voice is a set of resources dedicated to the synthesis of a single note or waveform being played on a
	///   channel. In the event that more notes are playing than there are available voices, one or more notes must
	///   be suppressed by the synthesizer. The choice is determined by the priority of the voice currently playing the
	///   note, which is based on the priority of the channel. By default, channels are ranked according to their index
	///   value, except that channel 10, the MIDI percussion channel, is ranked highest.
	/// _— Microsoft DirectX Documentation (https://documentation.help/DirectMusic/channels.htm)_
	///
	/// \note Original name: `dwChannelPriority`
	uint32_t channel_priority;

	DmReference reference;

	/// \brief A pointer to a loaded DLS file containing the instrument samples.
	DmDls* dls;
} DmInstrument;

/// \brief A DirectMusic band containing a set of instruments to use for playing MIDI notes.
typedef struct DmBand {
	_Atomic size_t reference_count;

	/// \brief The GUID uniquely identifying the band.
	DmGuid guid;

	/// \brief Human-readable information about the band.
	DmUnfo info;

	/// \brief The number of instruments available in the band.
	/// \see #instruments
	size_t instruments_len;

	/// \brief The list of instruments available in the band.
	/// \see #instruments_len
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
	uint32_t time_range;
	uint32_t duration_range;
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

typedef enum DmVariationType {
	/// \brief Play matching variations sequentially, in the order loaded, starting with the first.
	DmVariation_SEQUENTIAL = 0,

	/// \brief Select a random matching variation.
	DmVariation_RANDOM = 1,

	/// \brief Play matching variations sequentially, in the order loaded, starting at a random point in the sequence.
	DmVariation_RANDOM_START = 2,

	/// \brief Play randomly, but do not play the same variation twice.
	DmVariation_NO_REPEAT = 3,

	/// \brief Play randomly, but do not repeat any variation until all have played.
	DmVariation_RANDOM_ROW = 3,
} DmVariationType;

typedef struct DmPartReference {
	DmGuid part_id;
	DmUnfo info;

	uint16_t logical_part_id;
	uint8_t variation_lock_id;
	uint8_t subchord_level;
	uint8_t priority;
	DmVariationType random_variation;
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
	DmMessage_SIGNATURE,
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

	char name[32];
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

typedef struct DmMessage_TimeSignature {
	DmMessageType type;
	uint32_t time;

	DmTimeSignature signature;
} DmMessage_TimeSignature;

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
	DmMessage_TimeSignature signature;
	DmMessage_PitchBend pitch_bend;
} DmMessage;

DmArray_DEFINE(DmMessageList, DmMessage);

typedef struct DmSynthFont {
	DmDls* dls;
	tsf* syn;
} DmSynthFont;

typedef struct DmSynthChannel {
	DmSynthFont* font;
	int32_t channel;
	int32_t transpose;

	float reset_volume;
	float reset_pan;
	int reset_pitch;
} DmSynthChannel;

DmArray_DEFINE(DmSynthFontArray, DmSynthFont);

typedef struct DmSynth {
	uint32_t rate;
	float volume;
	DmSynthFontArray fonts;

	size_t channels_len;
	DmSynthChannel* channels;
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
	mtx_t lock;

	DmMessageQueue control_queue;
	DmMessageQueue music_queue;

	DmSegment* segment;
	uint32_t segment_start;
	DmStyle* style;
	DmBand* band;
	DmSynth synth;

	uint32_t sample_rate;
	uint32_t variation;
	uint32_t time;
	uint8_t groove;
	uint8_t groove_range;
	double tempo;
	DmMessage_Chord chord;
	DmTimeSignature time_signature;
};

/// \brief Allocate \p len bytes on the heap.
///
/// If set, this function will automatically choose a user-provided allocator over a the default one.
///
/// \note This function is thread-safe.
/// \param len The number of bytes to allocate.
/// \return A pointer to the first allocated byte or `NULL` if allocation failed.
/// \see Dm_free
/// \see Dm_setHeapAllocator
DMINT void* Dm_alloc(size_t len);

/// \brief Free a heap-allocated pointer previously allocated by #Dm_alloc
///
/// If set, this function will automatically choose a user-provided allocator over a the default one.
///
/// \note This function is thread-safe.
/// \param ptr A pointer to the memory to de-allocate or `NULL`.
/// \see Dm_alloc
/// \see Dm_setHeapAllocator
DMINT void Dm_free(void* ptr);

/// \brief Generate a log message at the given level.
/// \invariant \p fmt may not be `NULL`.
/// \param lvl The level of the log message to generate.
/// \param fmt The log message as a `printf`-style format string.
/// \param ... The values for the format string.
/// \see Dm_setLogger
/// \see Dm_setLoggerDefault
/// \see Dm_setLoggerLevel
DMINT void Dm_report(DmLogLevel lvl, char const* fmt, ...);

/// \brief Generate a random number in the range 0 to UINT32_MAX.
/// \return A random number.
/// \see Dm_setRandomNumberGenerator
DMINT uint32_t Dm_rand(void);

DMINT size_t max_usize(size_t a, size_t b);
DMINT int32_t max_s32(int32_t a, int32_t b);
DMINT uint8_t min_u8(uint8_t a, uint8_t b);
DMINT float lerp(float x, float start, float end);
DMINT int32_t clamp_s32(int32_t val, int32_t min, int32_t max);
DMINT int32_t Dm_randRange(int32_t range);
DMINT DmCommandType Dm_embellishmentToCommand(DmEmbellishmentType embellishment);
DMINT bool DmGuid_equals(DmGuid const* a, DmGuid const* b);
DMINT void DmTimeSignature_parse(DmTimeSignature* slf, DmRiff* rif);

DMINT uint32_t Dm_getBeatLength(DmTimeSignature sig);
DMINT uint32_t Dm_getMeasureLength(DmTimeSignature sig);
DMINT double Dm_getTicksPerSecond(DmTimeSignature time_signature, double beats_per_minute);
DMINT double Dm_getTicksPerSample(DmTimeSignature time_signature, double beats_per_minute, uint32_t sample_rate);
DMINT uint32_t Dm_getTimeOffset(uint32_t grid_start, int32_t time_offset, DmTimeSignature sig);
DMINT uint32_t Dm_getSampleCountForDuration(uint32_t duration,
                                            DmTimeSignature time_signature,
                                            double tempo,
                                            uint32_t sample_rate,
                                            uint8_t channels);
DMINT uint32_t Dm_getDurationForSampleCount(uint32_t samples,
                                            DmTimeSignature time_signature,
                                            double tempo,
                                            uint32_t sample_rate,
                                            uint8_t channels);

DMINT DmResult DmLoader_getStyle(DmLoader* slf, DmReference const* ref, DmStyle** sty);
DMINT DmResult DmLoader_getDownloadableSound(DmLoader* slf, DmReference const* ref, DmDls** snd);

DMINT DmResult DmSegment_create(DmSegment** slf);
DMINT DmResult DmSegment_parse(DmSegment* slf, void* buf, size_t len);

DMINT void DmMessage_copy(DmMessage* slf, DmMessage* cpy, int64_t time);
DMINT void DmMessage_free(DmMessage* slf);
DMINT DmResult DmMessageQueue_init(DmMessageQueue* slf);
DMINT void DmMessageQueue_free(DmMessageQueue* slf);
DMINT DmResult DmMessageQueue_add(DmMessageQueue* slf, DmMessage* msg, uint32_t time, DmQueueConflictResolution cr);
DMINT bool DmMessageQueue_get(DmMessageQueue* slf, DmMessage* msg);
DMINT void DmMessageQueue_pop(DmMessageQueue* slf);
DMINT void DmMessageQueue_clear(DmMessageQueue* slf);

DMINT DmResult DmBand_create(DmBand** slf);
DMINT DmBand* DmBand_retain(DmBand* slf);
DMINT void DmBand_release(DmBand* slf);
DMINT DmResult DmBand_parse(DmBand* slf, DmRiff* rif);
DMINT DmResult DmBand_download(DmBand* slf, DmLoader* loader);
DMINT DmDlsInstrument* DmInstrument_getDlsInstrument(DmInstrument* slf);
DMINT void DmInstrument_free(DmInstrument* slf);

DMINT DmResult DmStyle_create(DmStyle** slf);
DMINT DmStyle* DmStyle_retain(DmStyle* slf);
DMINT void DmStyle_release(DmStyle* slf);
DMINT DmResult DmStyle_parse(DmStyle* slf, void* buf, size_t len);
DMINT DmResult DmStyle_download(DmStyle* slf, DmLoader* loader);
DMINT DmPart* DmStyle_findPart(DmStyle* slf, DmPartReference* pref);
DMINT DmPattern* DmStyle_getRandomPattern(DmStyle* slf, uint32_t groove, DmCommandType cmd);

DMINT void DmPart_init(DmPart* slf);
DMINT void DmPart_free(DmPart* slf);
DMINT uint32_t DmPart_getValidVariationCount(DmPart* slf);

DMINT void DmPartReference_init(DmPartReference* slf);
DMINT void DmPartReference_free(DmPartReference* slf);

DMINT void DmPattern_init(DmPattern* slf);
DMINT void DmPattern_free(DmPattern* slf);

DMINT void DmSynth_init(DmSynth* slf, uint32_t sample_rate);
DMINT void DmSynth_free(DmSynth* slf);
DMINT void DmSynth_reset(DmSynth* slf);

DMINT void DmSynth_setVolume(DmSynth* slf, float vol);
DMINT DmResult DmSynth_createTsfForDls(DmDls* dls, tsf** out);
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

DMINT DmResult Dm_composeTransition(DmStyle* sty,
                                    DmBand* bnd,
                                    DmMessage_Chord* chord,
                                    DmSegment* sgt,
                                    DmEmbellishmentType embellishment,
                                    DmSegment** out);
