// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"
#include <math.h>

#include <tsf.h>

static double dlsTimeCentsToSeconds(int32_t tc) {
	return exp2((double) tc / (1200.0 * 65536.0));
}

static int16_t sf2SecondsToTimeCents(double secs) {
	return (int16_t) (1200.0 * log2(secs));
}

enum {
	kInitialFilterFc = 8,
	kPan = 17,
	kAttackModEnv = 26,
	kDecayModEnv = 28,
	kSustainModEnv = 29,
	kReleaseModEnv = 30,
	kAttackVolEnv = 34,
	kDecayVolEnv = 36,
	kSustainVolEnv = 37,
	kReleaseVolEnv = 38,
	kInstrument = 41,
	kKeyRange = 43,
	kVelRange = 44,
	kInitialAttenuation = 48,
	kSampleID = 53,
	kSampleModes = 54,

	kSamplePadding = 46,
};

static size_t DmSynth_insertGeneratorArticulators(struct tsf_hydra_igen* gens, DmDlsArticulator* art) {
	size_t count = 0;

	for (size_t k = 0; k < art->connection_count; ++k) {
		struct DmDlsArticulatorConnection* con = &art->connections[k];
		struct tsf_hydra_igen* gen = &gens[count];

		bool is_modulator = con->source != DmDlsArticulatorSource_NONE ||
		    con->transform != DmDlsArticulatorTransform_NONE || con->control != 0;
		if (is_modulator) {
			Dm_report(DmLogLevel_WARN, "DmSynth: DLS Modulators are not supported");
			continue;
		}

		if (art->level != 1) {
			Dm_report(DmLogLevel_WARN, "DmSynth: DLS Level 2 articulators are not implemented, expect weird results");
		}

		switch (con->destination) {
		case DmDlsArticulatorDestination_PAN:
			gen->genOper = kPan;
			gen->genAmount.shortAmount = (tsf_s16) (con->scale / 65535);
			break;
		case DmDlsArticulatorDestination_EG1_ATTACK_TIME:
			gen->genOper = kAttackVolEnv;
			gen->genAmount.shortAmount = sf2SecondsToTimeCents(dlsTimeCentsToSeconds(con->scale));
			break;
		case DmDlsArticulatorDestination_EG2_ATTACK_TIME:
			gen->genOper = kAttackModEnv;
			gen->genAmount.shortAmount = sf2SecondsToTimeCents(dlsTimeCentsToSeconds(con->scale));
			break;
		case DmDlsArticulatorDestination_EG1_DECAY_TIME:
			gen->genOper = kDecayVolEnv;
			gen->genAmount.shortAmount = sf2SecondsToTimeCents(dlsTimeCentsToSeconds(con->scale));
			break;
		case DmDlsArticulatorDestination_EG2_DECAY_TIME:
			gen->genOper = kDecayModEnv;
			gen->genAmount.shortAmount = sf2SecondsToTimeCents(dlsTimeCentsToSeconds(con->scale));
			break;
		case DmDlsArticulatorDestination_EG1_RELEASE_TIME:
			gen->genOper = kReleaseVolEnv;
			gen->genAmount.shortAmount = sf2SecondsToTimeCents(dlsTimeCentsToSeconds(con->scale));
			break;
		case DmDlsArticulatorDestination_EG2_RELEASE_TIME:
			gen->genOper = kReleaseModEnv;
			gen->genAmount.shortAmount = sf2SecondsToTimeCents(dlsTimeCentsToSeconds(con->scale));
			break;
		case DmDlsArticulatorDestination_EG1_SUSTAIN_LEVEL: {
			// SF2 Spec:
			//     This is the decrease in level, expressed in centibels, to which the Volume Envelope
			//     value ramps during the decay phase. For the Volume Envelope, the sustain level is
			//     best expressed in centibels of attenuation from full scale. A value of 0 indicates the
			//     sustain level is full level; this implies a zero duration of decay phase regardless of
			//     decay time. A positive value indicates a decay to the corresponding level. Values
			//     less than zero are to be interpreted as zero; conventionally 1000 indicates full
			//     attenuation. For example, a sustain level which corresponds to an absolute value
			//     12dB below of peak would be 120.
			//
			// Thus, since the DLS value is in 0.1% steps and 100% indicates "no attenuation", we can simply
			// convert the DLS value into percent and set the SF2 sustainVolEnv to `1000 * (1 - dls)`.
			int16_t clamped = (int16_t) clamp_s32(con->scale, 0, 1000);

			gen->genOper = kSustainVolEnv;
			gen->genAmount.shortAmount = (tsf_s16) (1000.f * (1.f - (clamped / 1000.f)));
			break;
		}
		case DmDlsArticulatorDestination_EG2_SUSTAIN_LEVEL: {
			// SF2 Spec:
			//     This is the decrease in level, expressed in 0.1% units, to which the Modulation
			//     Envelope value ramps during the decay phase. For the Modulation Envelope, the
			//     sustain level is properly expressed in percent of full scale. Because the volume
			//     envelope sustain level is expressed as an attenuation from full scale, the sustain level
			//     is analogously expressed as a decrease from full scale. A value of 0 indicates the
			//     sustain level is full level; this implies a zero duration of decay phase regardless of
			//     decay time. A positive value indicates a decay to the corresponding level. Values
			//     less than zero are to be interpreted as zero; values above 1000 are to be interpreted as
			//     1000. For example, a sustain level which corresponds to an absolute value 40% of
			//     peak would be 600.
			//
			// Thus, we just need to invert the DLS value.
			int16_t clamped = (int16_t) clamp_s32(con->scale, 0, 1000);

			gen->genOper = kSustainModEnv;
			gen->genAmount.shortAmount = 1000 - clamped;
			break;
		}
		case DmDlsArticulatorDestination_FILTER_CUTOFF:
			// SF2 Spec:
			//     This is the cutoff and resonant frequency of the lowpass filter in absolute cent units.
			//     The lowpass filter is defined as a second order resonant pole pair whose pole
			//     frequency in Hz is defined by the Initial Filter Cutoff parameter. When the cutoff
			//     frequency exceeds 20kHz and the Q (resonance) of the filter is zero, the filter does
			//     not affect the signal.
			gen->genOper = kInitialFilterFc;
			gen->genAmount.shortAmount = sf2SecondsToTimeCents(dlsTimeCentsToSeconds(con->scale));
			break;
		default:
			Dm_report(DmLogLevel_WARN, "DmSynth: Unknown Instrument Generator: %d", con->destination);
			continue;
		}

		count += 1;
	}

	return count;
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

	// 6. Count the number of instrument zones and generators required and allocate them
	res->ibagNum = 1; // One for the sentinel
	res->igenNum = 1; // One for the sentinel
	res->shdrNum = 1; // One for the sentinel

	for (size_t i = 0; i < dls->instrument_count; ++i) {
		DmDlsInstrument* ins = &dls->instruments[i];

		// -> We need one zone for each instrument region
		res->ibagNum += ins->region_count;

		// -> We need one sample for each instrument region
		res->shdrNum += ins->region_count;

		// -> We need 6 generators for the implicit 'kKeyRange', 'kVelRange', 'kAttackVolEnv',
		//   'kInitialAttenuation', 'kSampleModes' and 'kSampleID' for each zone
		res->igenNum += ins->region_count * 6;

		// -> We need one generator for each instrument-level articulator connection
		for (size_t a = 0; a < ins->articulator_count; ++a) {
			res->igenNum += ins->articulators[a].connection_count * ins->region_count;
		}

		// -> We need one generator for each region-level articulator connection
		for (size_t r = 0; r < ins->region_count; ++r) {
			DmDlsRegion* reg = &ins->regions[r];

			for (size_t a = 0; a < reg->articulator_count; ++a) {
				res->igenNum += reg->articulators[a].connection_count;
			}
		}
	}

	res->ibags = Dm_alloc(sizeof(struct tsf_hydra_ibag) * res->ibagNum);
	res->igens = Dm_alloc(sizeof(struct tsf_hydra_igen) * res->igenNum);
	res->shdrs = Dm_alloc(sizeof(struct tsf_hydra_shdr) * res->shdrNum);

	// 7. Count the number of instrument modulators required and allocate them
	// -> We need one sentinel (modulators are not supported)
	res->imodNum = 1; // One for the sentinel
	res->imods = Dm_alloc(sizeof(struct tsf_hydra_imod) * res->imodNum);

	bool ok =
	    res->phdrs && res->pbags && res->pgens && res->pmods && res->insts && res->ibags && res->igens && res->imods;
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

	// Fill the hydra with useful data
	uint32_t pgen_ndx = 0;
	uint32_t pmod_ndx = 0;
	uint32_t ibag_ndx = 0;
	uint32_t igen_ndx = 0;
	uint32_t imod_ndx = 0;
	uint32_t shdr_ndx = 0;
	for (size_t i = 0; i < dls->instrument_count; ++i) {
		DmDlsInstrument* ins = &dls->instruments[i];
		uint32_t bank = ins->bank;

		// Ignore drum kits for now.
		if (ins->bank & DmDls_DRUM_KIT) {
			bank = 999;
		}

		strncpy(hydra->phdrs[i].presetName, ins->info.inam, 19);
		hydra->phdrs[i].bank = bank;
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

			hydra->ibags[ibag_ndx].instGenNdx = igen_ndx;
			hydra->ibags[ibag_ndx].instModNdx = imod_ndx;
			ibag_ndx++;

			hydra->igens[igen_ndx].genOper = kKeyRange;
			hydra->igens[igen_ndx].genAmount.range.hi = (tsf_u8) reg->range_high;
			hydra->igens[igen_ndx].genAmount.range.lo = (tsf_u8) reg->range_low;
			igen_ndx++;

			hydra->igens[igen_ndx].genOper = kVelRange;
			uint8_t vel_hi = reg->velocity_high;
			uint8_t vel_lo = reg->velocity_low;

			if (vel_hi <= vel_lo) {
				vel_hi = 127;
				vel_lo = 0;
			}

			hydra->igens[igen_ndx].genAmount.range.hi = vel_hi;
			hydra->igens[igen_ndx].genAmount.range.lo = vel_lo;
			igen_ndx++;

			hydra->igens[igen_ndx].genOper = kAttackVolEnv;
			hydra->igens[igen_ndx].genAmount.shortAmount = sf2SecondsToTimeCents(0.1);
			igen_ndx++;

			hydra->igens[igen_ndx].genOper = kInitialAttenuation;
			hydra->igens[igen_ndx].genAmount.shortAmount = (tsf_s16) reg->sample.attenuation;
			igen_ndx++;

			// Articulators
			for (size_t a = 0; a < ins->articulator_count; ++a) {
				igen_ndx += DmSynth_insertGeneratorArticulators(hydra->igens + igen_ndx, &ins->articulators[a]);
			}

			for (size_t a = 0; a < reg->articulator_count; ++a) {
				igen_ndx += DmSynth_insertGeneratorArticulators(hydra->igens + igen_ndx, &reg->articulators[a]);
			}

			hydra->igens[igen_ndx].genOper = kSampleModes;
			hydra->igens[igen_ndx].genAmount.wordAmount = reg->sample.looping == true ? 1 : 0;
			igen_ndx++;

			hydra->igens[igen_ndx].genOper = kSampleID;
			hydra->igens[igen_ndx].genAmount.wordAmount = shdr_ndx;
			igen_ndx++;

			// Additional sample configuration.
			struct tsf_hydra_shdr* hdr = &hydra->shdrs[shdr_ndx];
			*hdr = default_shdrs[reg->link_table_index];
			shdr_ndx++;

			if (reg->sample.looping) {
				uint32_t offset = hdr->start;
				hdr->startLoop = offset + reg->sample.loop_start;
				hdr->endLoop = offset + reg->sample.loop_start + reg->sample.loop_length;

				// NOTE: Fix for sound cutting off too early. When using TSF.
				if (hdr->endLoop > hdr->end) {
					hdr->endLoop = hdr->end;
				}
			} else {
				hdr->startLoop = 0;
				hdr->endLoop = 0;
			}

			hdr->originalPitch = (tsf_u8) reg->sample.unity_note;
			hdr->pitchCorrection = (tsf_s8) reg->sample.fine_tune;
		}
	}

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

	strncpy(hydra->insts[hydra->instNum - 1].instName, "EOI", 19);
	hydra->insts[hydra->instNum - 1].instBagNdx = hydra->ibagNum - 1;

	hydra->ibags[hydra->ibagNum - 1].instGenNdx = hydra->igenNum - 1;
	hydra->ibags[hydra->ibagNum - 1].instModNdx = hydra->imodNum - 1;

	hydra->igens[hydra->igenNum - 1].genOper = 0;
	hydra->igens[hydra->igenNum - 1].genAmount.shortAmount = 0;

	hydra->imods[hydra->imodNum - 1].modSrcOper = 0;
	hydra->imods[hydra->imodNum - 1].modDestOper = 0;
	hydra->imods[hydra->imodNum - 1].modTransOper = 0;
	hydra->imods[hydra->imodNum - 1].modAmount = 0;
	hydra->imods[hydra->imodNum - 1].modAmtSrcOper = 0;

	Dm_free(default_shdrs);
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
