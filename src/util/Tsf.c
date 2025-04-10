// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"

#include <tsf.h>

DmArray_DEFINE(SFModulatorList, struct tsf_hydra_imod);
DmArray_IMPLEMENT(SFModulatorList, struct tsf_hydra_imod, NULL);
DmArray_DEFINE(SFGeneratorList, struct tsf_hydra_igen);
DmArray_IMPLEMENT(SFGeneratorList, struct tsf_hydra_igen, NULL);

enum {
	kStartLoopOffset = 2,
	kEndLoopOffset = 3,
	kModLfoToPitch = 5,
	kVibLfoToPitch = 6,
	kModEnvToPitch = 7,
	kInitialFilterFc = 8,
	kInitialFilterQ = 9,
	kModLfoToVolume = 13,
	kChorusSend = 16,
	kReverbSend = 16,
	kPan = 17,
	kDelayModLFO = 21,
	kFreqModLFO = 22,
	kDelayVibLFO = 23,
	kFreqVibLFO = 24,
	kDelayModEnv = 25,
	kAttackModEnv = 26,
	kHoldModEnv = 27,
	kDecayModEnv = 28,
	kSustainModEnv = 29,
	kReleaseModEnv = 30,
	kDelayVolEnv = 33,
	kAttackVolEnv = 34,
	kHoldVolEnv = 35,
	kDecayVolEnv = 36,
	kSustainVolEnv = 37,
	kReleaseVolEnv = 38,
	kInstrument = 41,
	kKeyRange = 43,
	kVelRange = 44,
	kStartLoopOffsetCoarse = 45,
	kVelocity = 47,
	kInitialAttenuation = 48,
	kEndLoopOffsetCoarse = 50,
	kFineTune = 52,
	kSampleID = 53,
	kSampleModes = 54,
	kScaleTuning = 56,
	kExclusiveClass = 57,
	kOverridingRootKey = 58,

	kNoteOnVelocity = 2,
	kNoteOnKey = 3,

	kSamplePadding = 46,
	kNone = 0,
	kLinear = 0,
};

static void DmSynth_insertGenerators(SFGeneratorList* gens, DmDlsArticulator* art) {
	for (size_t k = 0; k < art->connection_count; ++k) {
		struct DmDlsArticulatorConnection* con = &art->connections[k];
		struct tsf_hydra_igen gen;

		// Ignore scaled controlled source articulators because they can't become generators anyway
		if (con->control != DmDlsArticulatorSource_NONE) {
			continue;
		}

		// Special articulator checks:

		// Mod LFO to pitch
		if (con->destination == DmDlsArticulatorDestination_PITCH && con->source == DmDlsArticulatorSource_LFO) {
			gen.genOper = kModLfoToPitch;
			gen.genAmount.shortAmount = clamp_s32(con->scale >> 16, -1200, 1200);
			SFGeneratorList_add(gens, gen);
			continue;
		}

		// Mod LFO to Gain
		if (con->destination == DmDlsArticulatorDestination_ATTENUATION && con->source == DmDlsArticulatorSource_LFO) {
			gen.genOper = kModLfoToVolume;
			gen.genAmount.shortAmount = clamp_s32(con->scale >> 16, 0, 120);
			SFGeneratorList_add(gens, gen);
			continue;
		}

		// Vib LFO to pitch
		if (con->destination == DmDlsArticulatorDestination_PITCH && con->source == DmDlsArticulatorSource_VIBRATO) {
			gen.genOper = kVibLfoToPitch;
			gen.genAmount.shortAmount = clamp_s32(con->scale >> 16, -1200, 1200);
			SFGeneratorList_add(gens, gen);
			continue;
		}

		// Mod EG to pitch
		if (con->destination == DmDlsArticulatorDestination_PITCH && con->source == DmDlsArticulatorSource_EG2) {
			gen.genOper = kModEnvToPitch;
			gen.genAmount.shortAmount = clamp_s32(con->scale >> 16, -1200, 1200);
			SFGeneratorList_add(gens, gen);
			continue;
		}

		// Key Number to Pitch
		if (con->destination == DmDlsArticulatorDestination_PITCH && con->source == DmDlsArticulatorSource_KEY_NUMBER) {
			gen.genOper = kScaleTuning;
			gen.genAmount.shortAmount = con->scale / 12800 * 100;
			SFGeneratorList_add(gens, gen);
			continue;
		}

		// Ignore other scaled source connection blocks because they'll resolve to modulators instead.
		if (con->source != DmDlsArticulatorSource_NONE) {
			continue;
		}

		switch (con->destination) {
		case DmDlsArticulatorDestination_PITCH:
			// Special case: We have to use the `Fine Tune` destination already to apply the sample-inherent
			//               tuning, so we have to use a modulator here instead.
			continue;
		case DmDlsArticulatorDestination_PAN:
			gen.genOper = kPan;
			gen.genAmount.shortAmount = (tsf_s16) (con->scale >> 16);
			break;
		case DmDlsArticulatorDestination_CHORUS:
			gen.genOper = kChorusSend;
			gen.genAmount.shortAmount = (tsf_s16) (con->scale >> 16);
			break;
		case DmDlsArticulatorDestination_REVERB:
			gen.genOper = kReverbSend;
			gen.genAmount.shortAmount = (tsf_s16) (con->scale >> 16);
			break;
		case DmDlsArticulatorDestination_EG1_ATTACK_TIME:
			gen.genOper = kAttackVolEnv;
			gen.genAmount.shortAmount = con->scale >> 16;
			break;
		case DmDlsArticulatorDestination_EG2_ATTACK_TIME:
			gen.genOper = kAttackModEnv;
			gen.genAmount.shortAmount = con->scale >> 16;
			break;
		case DmDlsArticulatorDestination_EG1_DECAY_TIME:
			gen.genOper = kDecayVolEnv;
			gen.genAmount.shortAmount = con->scale >> 16;
			break;
		case DmDlsArticulatorDestination_EG2_DECAY_TIME:
			gen.genOper = kDecayModEnv;
			gen.genAmount.shortAmount = con->scale >> 16;
			break;
		case DmDlsArticulatorDestination_EG1_RELEASE_TIME:
			gen.genOper = kReleaseVolEnv;
			gen.genAmount.shortAmount = con->scale >> 16;
			break;
		case DmDlsArticulatorDestination_EG2_RELEASE_TIME:
			gen.genOper = kReleaseModEnv;
			gen.genAmount.shortAmount = con->scale >> 16;
			break;
		case DmDlsArticulatorDestination_EG1_DELAY_TIME:
			gen.genOper = kDelayVolEnv;
			gen.genAmount.shortAmount = con->scale >> 16;
			break;
		case DmDlsArticulatorDestination_EG2_DELAY_TIME:
			gen.genOper = kDelayModEnv;
			gen.genAmount.shortAmount = con->scale >> 16;
			break;
		case DmDlsArticulatorDestination_EG1_HOLD_TIME:
			gen.genOper = kHoldVolEnv;
			gen.genAmount.shortAmount = con->scale >> 16;
			break;
		case DmDlsArticulatorDestination_EG2_HOLD_TIME:
			gen.genOper = kHoldModEnv;
			gen.genAmount.shortAmount = con->scale >> 16;
			break;
		case DmDlsArticulatorDestination_EG1_SUSTAIN_LEVEL: {
			gen.genOper = kSustainVolEnv;
			gen.genAmount.shortAmount = (tsf_s16) (1000 - clamp_s32(con->scale >> 16, 0, 1000));
			break;
		}
		case DmDlsArticulatorDestination_EG2_SUSTAIN_LEVEL: {
			gen.genOper = kSustainModEnv;
			gen.genAmount.shortAmount = (tsf_s16) (1000 - clamp_s32(con->scale >> 16, 0, 1000));
			break;
		}
		case DmDlsArticulatorDestination_FILTER_CUTOFF:
			gen.genOper = kInitialFilterFc;
			gen.genAmount.shortAmount = con->scale >> 16;
			break;
		case DmDlsArticulatorDestination_FILTER_Q:
			gen.genOper = kInitialFilterQ;
			gen.genAmount.shortAmount = con->scale >> 16;
			break;
		case DmDlsArticulatorDestination_LFO_FREQUENCY:
			gen.genOper = kFreqModLFO;
			gen.genAmount.shortAmount = con->scale >> 16;
			break;
		case DmDlsArticulatorDestination_LFO_START_DELAY:
			gen.genOper = kDelayModLFO;
			gen.genAmount.shortAmount = con->scale >> 16;
			break;
		case DmDlsArticulatorDestination_VIB_FREQUENCY:
			gen.genOper = kFreqVibLFO;
			gen.genAmount.shortAmount = con->scale >> 16;
			break;
		case DmDlsArticulatorDestination_VIB_START_DELAY:
			gen.genOper = kDelayVibLFO;
			gen.genAmount.shortAmount = con->scale >> 16;
			break;
		default:
			Dm_report(DmLogLevel_DEBUG, "DmSynth: Unknown Instrument Generator: %d", con->destination);
			continue;
		}

		SFGeneratorList_add(gens, gen);
	}
}

static void DmSynth_insertModulators(SFModulatorList* mods, DmDlsArticulator* art) {
	for (size_t k = 0; k < art->connection_count; ++k) {
		struct DmDlsArticulatorConnection* con = &art->connections[k];
		struct tsf_hydra_imod mod;

		// Ignore scaled controlled source articulators because SF2 support none of those allowed in DLS
		if (con->control != DmDlsArticulatorSource_NONE) {
			Dm_report(DmLogLevel_DEBUG, "DmDls: File uses scaled controlled source articulators which SF2 does not support");
			continue;
		}

		// Pitch (scaled connection block special case)
		if (con->source == DmDlsArticulatorSource_NONE && con->destination == DmDlsArticulatorDestination_PITCH) {
			mod.modSrcOper = kNone;
			mod.modTransOper = 0;
			mod.modDestOper = kFineTune;
			mod.modAmount = -(con->scale >> 16);
			mod.modAmtSrcOper = kNone;
			SFModulatorList_add(mods, mod);
			continue;
		}

		// TODO: For all of these, also convert the transforms, including bipolarity and inversion, to SF2.

		// Vol EG Key to Decay
		if (con->destination == DmDlsArticulatorDestination_EG1_DECAY_TIME &&
		    con->source == DmDlsArticulatorSource_KEY_NUMBER) {
			mod.modSrcOper = kNoteOnKey;
			mod.modDestOper = kDecayVolEnv;
			mod.modAmtSrcOper = 0;
			mod.modTransOper = kLinear;
			mod.modAmount = con->scale >> 16;
			SFModulatorList_add(mods, mod);
			continue;
		}

		// Mod EG Key to Decay
		if (con->destination == DmDlsArticulatorDestination_EG2_DECAY_TIME &&
		    con->source == DmDlsArticulatorSource_KEY_NUMBER) {
			mod.modSrcOper = kNoteOnKey;
			mod.modDestOper = kDecayModEnv;
			mod.modAmtSrcOper = 0;
			mod.modTransOper = kLinear;
			mod.modAmount = con->scale >> 16;
			SFModulatorList_add(mods, mod);
			continue;
		}

		// Vol EG Velocity to Attack
		if (con->destination == DmDlsArticulatorDestination_EG1_ATTACK_TIME &&
		    con->source == DmDlsArticulatorSource_KEY_ON_VELOCITY) {
			mod.modSrcOper = kNoteOnVelocity;
			mod.modDestOper = kAttackVolEnv;
			mod.modAmtSrcOper = 0;
			mod.modTransOper = kLinear;
			mod.modAmount = con->scale >> 16;
			SFModulatorList_add(mods, mod);
			continue;
		}

		// Mod EG Velocity to Attack
		if (con->destination == DmDlsArticulatorDestination_EG2_ATTACK_TIME &&
		    con->source == DmDlsArticulatorSource_KEY_ON_VELOCITY) {
			mod.modSrcOper = kNoteOnVelocity;
			mod.modDestOper = kAttackModEnv;
			mod.modAmtSrcOper = 0;
			mod.modTransOper = kLinear;
			mod.modAmount = con->scale >> 16;
			SFModulatorList_add(mods, mod);
			continue;
		}

		// Vol EG Key to Hold
		if (con->destination == DmDlsArticulatorDestination_EG1_HOLD_TIME &&
		    con->source == DmDlsArticulatorSource_KEY_NUMBER) {
			mod.modSrcOper = kNoteOnKey;
			mod.modDestOper = kHoldVolEnv;
			mod.modAmtSrcOper = 0;
			mod.modTransOper = kLinear;
			mod.modAmount = con->scale >> 16;
			SFModulatorList_add(mods, mod);
			continue;
		}

		// Mod EG Key to Hold
		if (con->destination == DmDlsArticulatorDestination_EG2_HOLD_TIME &&
		    con->source == DmDlsArticulatorSource_KEY_NUMBER) {
			mod.modSrcOper = kNoteOnKey;
			mod.modDestOper = kHoldModEnv;
			mod.modAmtSrcOper = 0;
			mod.modTransOper = kLinear;
			mod.modAmount = con->scale >> 16;
			SFModulatorList_add(mods, mod);
			continue;
		}

		Dm_report(DmLogLevel_DEBUG,
			   "DmDls: Unsupported scaled source connection block; source=%d, destination=%d",
			   con->source,
			   con->destination);
	}
}

static DmResult
Dm_createHydraSamplesForDls(DmDls* dls, float** pcm, int32_t* pcm_len, struct tsf_hydra_shdr** cfg, int32_t* cfg_len) {
	// 1. Count the number of PCM samples actually required after decoding.
	size_t sample_count = 0;
	for (uint32_t i = 0; i < dls->wave_table_size; ++i) {
		DmDlsWave* wav = &dls->wave_table[i];
		sample_count += DmDls_decodeSamples(wav, NULL, 0);

		// There are 46 0-samples after each "real" sample
		sample_count += kSamplePadding;
	}

	// 2. Decode all required samples
	size_t sample_headers_length = dls->wave_table_size + 1; // One for the sentinel
	struct tsf_hydra_shdr* sample_headers = Dm_alloc(sizeof(struct tsf_hydra_shdr) * sample_headers_length);
	if (sample_headers == NULL) {
		return DmResult_MEMORY_EXHAUSTED;
	}

	float* samples = Dm_alloc(sizeof(float) * sample_count);
	if (samples == NULL) {
		return DmResult_MEMORY_EXHAUSTED;
	}

	size_t sample_offset = 0;
	for (uint32_t i = 0; i < dls->wave_table_size; ++i) {
		DmDlsWave* wav = &dls->wave_table[i];

		strncpy(sample_headers[i].sampleName, wav->info.inam, 19);

		sample_headers[i].start = (uint32_t) sample_offset;
		sample_headers[i].startLoop = (uint32_t) sample_offset;
		sample_headers[i].endLoop = (uint32_t) sample_offset;
		sample_headers[i].sampleRate = wav->samples_per_second;
		sample_headers[i].sampleType = 1; // SFSampleLink::monoSample
		sample_offset += DmDls_decodeSamples(wav, samples + sample_offset, sample_count - sample_offset);
		sample_headers[i].end = (uint32_t) sample_offset;

		// There are 46 0-samples after each "real" sample
		sample_offset += kSamplePadding;
	}

	strncpy(sample_headers[sample_headers_length - 1].sampleName, "EOS", 19);

	*pcm = samples;
	*pcm_len = sample_count;
	*cfg = sample_headers;
	*cfg_len = (int) sample_headers_length;
	return DmResult_SUCCESS;
}

static DmResult Dm_createHydraSkeleton(DmDls* dls, struct tsf_hydra* res) {
	// 1. Count the number of presets required and allocate them
	// -> We need one for each instrument
	res->phdrNum = dls->instrument_count + 1; // One for the sentinel
	res->phdrs = Dm_alloc(sizeof(struct tsf_hydra_phdr) * res->phdrNum);

	// 2. Count the number of preset zones required and allocate them
	// -> We need one for each instrument
	res->pbagNum = dls->instrument_count + 1; // One for the sentinel
	res->pbags = Dm_alloc(sizeof(struct tsf_hydra_pbag) * res->pbagNum);

	// 3. Count the number of preset generators required and allocate them
	// -> We need one for each instrument to indicate the instrument index
	res->pgenNum = dls->instrument_count + 1; // One for the sentinel
	res->pgens = Dm_alloc(sizeof(struct tsf_hydra_pgen) * res->pgenNum);

	// 4. Count the number of preset modulators required and allocate them
	// -> We need one sentinel (modulators are not supported)
	res->pmodNum = 1; // One for the sentinel
	res->pmods = Dm_alloc(sizeof(struct tsf_hydra_pmod) * res->pmodNum);

	// 5. Count the number of instruments required and allocate them
	// -> We need one for each instrument
	res->instNum = dls->instrument_count + 1; // One for the sentinel
	res->insts = Dm_alloc(sizeof(struct tsf_hydra_inst) * res->instNum);

	// 6. Count the number of instrument zones and allocate them
	res->ibagNum = 1; // One for the sentinel

	for (size_t i = 0; i < dls->instrument_count; ++i) {
		DmDlsInstrument* ins = &dls->instruments[i];

		// -> We need one zone for each instrument region
		res->ibagNum += ins->region_count;
	}

	res->ibags = Dm_alloc(sizeof(struct tsf_hydra_ibag) * res->ibagNum);

	// 7. Calculate the number of sample headers required and allocate them
	res->shdrNum = dls->wave_table_size + 1; // One for the sentinel
	res->shdrs = Dm_alloc(sizeof(struct tsf_hydra_shdr) * res->shdrNum);

	bool ok =
	    res->phdrs && res->pbags && res->pgens && res->pmods && res->insts && res->ibags && res->shdrNum;
	return ok ? DmResult_SUCCESS : DmResult_MEMORY_EXHAUSTED;
}

// We export this function for the tools.
static DmResult Dm_createHydra(DmDls* dls, struct tsf_hydra* hydra, float** pcm, int32_t* pcm_len) {
	DmResult rv = Dm_createHydraSkeleton(dls, hydra);
	if (rv != DmResult_SUCCESS) {
		return rv;
	}

	// Decode all PCM and create the sample headers.
	struct tsf_hydra_shdr* default_shdrs = NULL;
	int32_t default_shdrs_len = 0;
	rv = Dm_createHydraSamplesForDls(dls, pcm, pcm_len, &default_shdrs, &default_shdrs_len);
	if (rv != DmResult_SUCCESS) {
		return rv;
	}

	struct tsf_hydra_igen gen;
	SFModulatorList imod;
	SFModulatorList_init(&imod);

	struct tsf_hydra_imod mod;
	SFGeneratorList igen;
	SFGeneratorList_init(&igen);

	// Fill the hydra with useful data
	uint32_t pgen_ndx = 0;
	uint32_t pmod_ndx = 0;
	uint32_t ibag_ndx = 0;
	for (size_t i = 0; i < dls->instrument_count; ++i) {
		DmDlsInstrument* ins = &dls->instruments[i];
		uint32_t bank = ins->bank;

		strncpy(hydra->phdrs[i].presetName, ins->info.inam, 19);
		// NOTE: Drum kits always go into MIDI Bank 128
		hydra->phdrs[i].bank = bank & DmDls_DRUM_KIT ? 128 : bank;
		hydra->phdrs[i].preset = ins->patch;
		hydra->phdrs[i].genre = 0;
		hydra->phdrs[i].morphology = 0;
		hydra->phdrs[i].library = 0;
		hydra->phdrs[i].presetBagNdx = i;

		hydra->pbags[i].genNdx = pgen_ndx;
		hydra->pbags[i].modNdx = pmod_ndx;

		hydra->pgens[pgen_ndx].genOper = kInstrument;
		hydra->pgens[pgen_ndx].genAmount.wordAmount = i;
		pgen_ndx++;

		strncpy(hydra->insts[i].instName, ins->info.inam, 19);
		hydra->insts[i].instBagNdx = ibag_ndx;

		for (size_t r = 0; r < ins->region_count; ++r) {
			DmDlsRegion* reg = &ins->regions[r];

			hydra->ibags[ibag_ndx].instGenNdx = igen.length;
			hydra->ibags[ibag_ndx].instModNdx = imod.length;
			ibag_ndx++;

			// Key Range Generator
			gen.genOper = kKeyRange;
			gen.genAmount.range.hi = (tsf_u8) reg->range_high;
			gen.genAmount.range.lo = (tsf_u8) reg->range_low;
			SFGeneratorList_add(&igen, gen);

			// Velocity Range Generator
			gen.genOper = kVelRange;
			gen.genAmount.range.hi = (tsf_u8) reg->velocity_high;
			gen.genAmount.range.lo = (tsf_u8) reg->velocity_low;

			if (reg->velocity_high <= reg->velocity_low) {
				gen.genAmount.range.hi = 127;
				gen.genAmount.range.lo = 0;
			}
			SFGeneratorList_add(&igen, gen);

			// Exclusive Class Generator
			if (reg->key_group != 0) {
				gen.genOper = kExclusiveClass;
				gen.genAmount.wordAmount = reg->key_group;
				SFGeneratorList_add(&igen, gen);
			}

			// Overriding Root Key Generator
			gen.genOper = kOverridingRootKey;
			gen.genAmount.wordAmount = reg->sample.unity_note;
			SFGeneratorList_add(&igen, gen);

			// Fine Tune Generator
			gen.genOper = kFineTune;
			gen.genAmount.shortAmount = reg->sample.fine_tune >> 16;
			SFGeneratorList_add(&igen, gen);

			// Initial Attenuation Generator
			gen.genOper = kInitialAttenuation;
			gen.genAmount.wordAmount = -(reg->sample.gain >> 16);
			SFGeneratorList_add(&igen, gen);

			// Sample Loops
			uint16_t loop = 0;  // i.e. "no loop"
			if (reg->sample.looping) {
				uint16_t coarse = reg->sample.loop_start / 32768;
				uint16_t fine = reg->sample.loop_start % 32768;

				gen.genOper = kStartLoopOffset;
				gen.genAmount.wordAmount = fine;
				SFGeneratorList_add(&igen, gen);

				gen.genOper = kStartLoopOffsetCoarse;
				gen.genAmount.wordAmount = coarse;
				SFGeneratorList_add(&igen, gen);

				coarse = (reg->sample.loop_start + reg->sample.loop_length) / 32768;
				fine = (reg->sample.loop_start + reg->sample.loop_length) % 32768;

				gen.genOper = kEndLoopOffset;
				gen.genAmount.wordAmount = fine;
				SFGeneratorList_add(&igen, gen);

				gen.genOper = kEndLoopOffsetCoarse;
				gen.genAmount.wordAmount = coarse;
				SFGeneratorList_add(&igen, gen);

				loop = reg->sample.loop_with_release ? 3u : 1u;
			}

			gen.genOper = kSampleModes;
			gen.genAmount.wordAmount = loop;
			SFGeneratorList_add(&igen, gen);

			// Articulators
			if (reg->articulator_count == 0) {
				// Add only instrument articulators
				for (size_t a = 0; a < ins->articulator_count; ++a) {
					DmSynth_insertGenerators(&igen, &ins->articulators[a]);
					DmSynth_insertModulators(&imod, &ins->articulators[a]);
				}
			} else {
				// Add only region articulators
				for (size_t a = 0; a < reg->articulator_count; ++a) {
					DmSynth_insertGenerators(&igen, &reg->articulators[a]);
					DmSynth_insertModulators(&imod, &ins->articulators[a]);
				}
			}


			gen.genOper = kSampleID;
			gen.genAmount.wordAmount = reg->link_table_index;
			SFGeneratorList_add(&igen, gen);
		}
	}

	// Sentinel Generator
	gen.genOper = 0;
	gen.genAmount.wordAmount = 0;
	SFGeneratorList_add(&igen, gen);

	// Sentinel Modulator
	mod.modAmount = 0;
	mod.modAmtSrcOper = 0;
	mod.modDestOper = 0;
	mod.modSrcOper = 0;
	mod.modTransOper = 0;
	SFModulatorList_add(&imod, mod);

	// Populate the sentinel values of the hydra
	strncpy(hydra->phdrs[hydra->phdrNum - 1].presetName, "EOP", 19);
	hydra->phdrs[hydra->phdrNum - 1].bank = 0;
	hydra->phdrs[hydra->phdrNum - 1].preset = 0;
	hydra->phdrs[hydra->phdrNum - 1].genre = 0;
	hydra->phdrs[hydra->phdrNum - 1].morphology = 0;
	hydra->phdrs[hydra->phdrNum - 1].library = 0;
	hydra->phdrs[hydra->phdrNum - 1].presetBagNdx = hydra->pbagNum - 1;

	hydra->pbags[hydra->pbagNum - 1].genNdx = hydra->pgenNum - 1;
	hydra->pbags[hydra->pbagNum - 1].modNdx = hydra->pmodNum - 1;

	hydra->pgens[hydra->pgenNum - 1].genOper = 0;
	hydra->pgens[hydra->pgenNum - 1].genAmount.shortAmount = 0;

	hydra->pmods[hydra->pmodNum - 1].modSrcOper = 0;
	hydra->pmods[hydra->pmodNum - 1].modDestOper = 0;
	hydra->pmods[hydra->pmodNum - 1].modTransOper = 0;
	hydra->pmods[hydra->pmodNum - 1].modAmount = 0;
	hydra->pmods[hydra->pmodNum - 1].modAmtSrcOper = 0;

	hydra->igens = igen.data;
	hydra->igenNum = igen.length;

	hydra->imods = imod.data;
	hydra->imodNum = imod.length;

	strncpy(hydra->insts[hydra->instNum - 1].instName, "EOI", 19);
	hydra->insts[hydra->instNum - 1].instBagNdx = hydra->ibagNum - 1;

	hydra->ibags[hydra->ibagNum - 1].instGenNdx = hydra->igenNum - 1;
	hydra->ibags[hydra->ibagNum - 1].instModNdx = hydra->imodNum - 1;

	hydra->shdrNum = default_shdrs_len;
	hydra->shdrs = default_shdrs;

	return DmResult_SUCCESS;
}

// We export this function for the tools.
static void Dm_freeHydra(struct tsf_hydra* hydra) {
	Dm_free(hydra->phdrs);
	Dm_free(hydra->pbags);
	Dm_free(hydra->pgens);
	Dm_free(hydra->pmods);
	Dm_free(hydra->insts);
	Dm_free(hydra->ibags);
	Dm_free(hydra->igens);
	Dm_free(hydra->imods);
	Dm_free(hydra->shdrs);
}

DmResult DmSynth_createTsfForDls(DmDls* dls, tsf** out) {
	if (dls == NULL) {
		return DmResult_INVALID_ARGUMENT;
	}

	// Initialize the hydra by allocation all required memory
	struct tsf_hydra hydra;
	float* pcm = NULL;
	int32_t pcm_len = 0;

	DmResult rv = Dm_createHydra(dls, &hydra, &pcm, &pcm_len);
	if (rv != DmResult_SUCCESS) {
		return rv;
	}

	// Finally, create the tsf
	tsf* res = *out = Dm_alloc(sizeof(tsf));
	if (res == NULL) {
		return DmResult_MEMORY_EXHAUSTED;
	}

	if (!tsf_load_presets(res, &hydra, pcm_len)) {
		Dm_report(DmLogLevel_ERROR, "DmSynth: Failed to load tsf presets");
		return DmResult_MEMORY_EXHAUSTED;
	}

	res->fontSamples = pcm;

	// Lastly, free up all the hydra stuff
	Dm_freeHydra(&hydra);

	return DmResult_SUCCESS;
}
