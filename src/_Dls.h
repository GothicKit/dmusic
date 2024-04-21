// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#pragma once
#include "_Riff.h"

typedef enum DmDlsRegionFlags {
	DmDlsRegion_NONEXCLUSIVE = 1 << 0,
} DmDlsRegionFlags;

typedef enum DmDlsWaveSampleFlags {
	DmDlsWave_NO_TRUNCATION = 0x11,
	DmDlsWave_NO_COMPRESSION = 0x21,
} DmDlsWaveSampleFlags;

typedef enum DmDlsLoopType {
	DmDlsLoop_FORWARD = 0,
} DmDlsLoopType;

typedef enum DmDlsWaveLinkFlags {
	DmDlsWaveLink_MASTER_PHASE = 1 << 0,
} DmDlsWaveLinkFlags;

typedef enum DmDlsArticulatorSource {
	DmDlsArticulatorSource_NONE = 0,
	DmDlsArticulatorSource_LFO = 1,
	DmDlsArticulatorSource_KEY_ON_VELOCITY = 2,
	DmDlsArticulatorSource_KEY_NUMBER = 3,
	DmDlsArticulatorSource_EG1 = 4,
	DmDlsArticulatorSource_EG2 = 5,
	DmDlsArticulatorSource_PITCH_WEEL = 6,
	DmDlsArticulatorSource_CC1 = 0x81,
	DmDlsArticulatorSource_CC7 = 0x87,
	DmDlsArticulatorSource_CC10 = 0x8a,
	DmDlsArticulatorSource_CC11 = 0x8b,
	DmDlsArticulatorSource_RPN0 = 0x100,
	DmDlsArticulatorSource_RPN1 = 0x101,
	DmDlsArticulatorSource_RPN2 = 0x102,
} DmDlsArticulatorSource;

typedef enum DmDlsArticulatorDestination {
	DmDlsArticulatorDestination_NONE = 0,
	DmDlsArticulatorDestination_ATTENUATION = 1,
	DmDlsArticulatorDestination_PITCH = 3,
	DmDlsArticulatorDestination_PAN = 4,
	DmDlsArticulatorDestination_LFO_FREQUENCY = 0x104,
	DmDlsArticulatorDestination_LFO_START_DELAY = 0x105,
	DmDlsArticulatorDestination_EG1_ATTACK_TIME = 0x206,
	DmDlsArticulatorDestination_EG1_DECAY_TIME = 0x207,
	DmDlsArticulatorDestination_EG1_RELEASE_TIME = 0x209,
	DmDlsArticulatorDestination_EG1_SUSTAIN_LEVEL = 0x20a,
	DmDlsArticulatorDestination_EG2_ATTACK_TIME = 0x30a,
	DmDlsArticulatorDestination_EG2_DECAY_TIME = 0x30b,
	DmDlsArticulatorDestination_EG2_RELEASE_TIME = 0x30d,
	DmDlsArticulatorDestination_EG2_SUSTAIN_LEVEL = 0x30e,
} DmDlsArticulatorDestination;

typedef enum DmDlsArticulatorTransform {
	DmDlsArticulatorTransform_NONE = 0,
	DmDlsArticulatorTransform_CONCAVE = 1,
} DmDlsArticulatorTransform;

typedef struct DmDlsWaveSample {
	uint16_t unity_note;
	uint16_t fine_tune;
	int32_t attenuation;
	DmDlsWaveSampleFlags flags;

	bool looping;
	DmDlsLoopType loop_type;
	uint32_t loop_start;
	uint32_t loop_length;
} DmDlsWaveSample;

typedef struct DmDlsArticulator {
	uint32_t connection_count;
	struct {
		DmDlsArticulatorSource source;
		uint16_t control;
		DmDlsArticulatorDestination destination;
		DmDlsArticulatorTransform transform;
		int32_t scale;
	}* connections;
} DmDlsArticulator;

typedef struct DmDlsRegion {
	uint16_t range_low;
	uint16_t range_high;
	uint16_t velocity_low;
	uint16_t velocity_high;
	DmDlsRegionFlags flags;
	uint16_t key_group;

	DmDlsWaveSample sample;

	DmDlsWaveLinkFlags link_flags;
	uint16_t link_phase_group;
	uint32_t link_channel;
	uint32_t link_table_index;

	uint32_t articulator_count;
	DmDlsArticulator* articulators;
} DmDlsRegion;

typedef struct DmDlsInstrument {
	DmGuid guid;
	DmInfo info;

	uint32_t bank;
	uint32_t instrument;

	uint32_t region_count;
	DmDlsRegion* regions;

	uint32_t articulator_count;
	DmDlsArticulator* articulators;
} DmDlsInstrument;

typedef enum DmDlsWaveFormat {
	DmDlsWaveFormat_PCM = 1,
	DmDlsWaveFormat_ADPCM = 2,
} DmDlsWaveFormat;

typedef struct DmDlsWave {
	DmInfo info;

	DmDlsWaveFormat format;
	uint16_t channels;
	uint32_t samples_per_second;
	uint32_t avg_bytes_per_second;
	uint16_t block_align;
	uint16_t bits_per_sample;

	// ADPCM only:
	uint16_t samples_per_block;
	uint16_t coefficient_table[14];

	DmDlsWaveSample sample;

	uint32_t data_length;
	uint8_t const* data;
} DmDlsWave;

typedef struct DmDls {
	_Atomic size_t reference_count;
	void* backing_memory;

	DmGuid guid;
	DmVersion version;
	DmInfo info;

	uint32_t instrument_count;
	DmDlsInstrument* instruments;

	uint32_t pool_table_size;
	uint32_t* pool_table;

	uint32_t wave_table_size;
	DmDlsWave* wave_table;
} DmDls;

DMINT DmResult DmDls_create(DmDls** slf);
DMAPI DmDls* DmDls_retain(DmDls* slf);
DMINT size_t DmDls_release(DmDls*);
DMINT DmResult DmDls_parse(DmDls* slf, void* buf, size_t len);

DMINT void DmDlsInstrument_init(DmDlsInstrument* slf);
DMINT void DmDlsInstrument_free(DmDlsInstrument* slf);

DMINT void DmDlsRegion_init(DmDlsRegion* slf);
DMINT void DmDlsRegion_free(DmDlsRegion* slf);

DMINT void DmDlsArticulator_init(DmDlsArticulator* slf);
DMINT void DmDlsArticulator_free(DmDlsArticulator* slf);
