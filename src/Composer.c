// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"

DmResult Dm_composeTransition(DmStyle* sty,
                              DmBand* bnd,
                              DmMessage_Chord* chord,
                              DmSegment* sgt,
                              DmEmbellishmentType embellishment,
                              DmSegment** out) {
	DmResult rv = DmSegment_create(out);
	if (rv != DmResult_SUCCESS) {
		return rv;
	}

	DmSegment* trans = *out;
	trans->repeats = 1;
	trans->length = 0;
	trans->play_start = 0;
	trans->loop_start = 0;
	trans->loop_end = trans->length;
	trans->downloaded = true;
	trans->info.unam = "Composed Transition";

	// NOTE: We only support transitions of length 1 (measure)

	DmMessage msg;
	msg.time = 0;

	if (embellishment != DmEmbellishment_NONE) {
		msg.type = DmMessage_TEMPO;
		msg.tempo.tempo = sty->tempo;
		rv = DmMessageList_add(&trans->messages, msg);
		if (rv != DmResult_SUCCESS) {
			return rv;
		}

		msg.type = DmMessage_BAND;
		msg.band.band = DmBand_retain(bnd);
		rv = DmMessageList_add(&trans->messages, msg);
		if (rv != DmResult_SUCCESS) {
			return rv;
		}

		msg.type = DmMessage_STYLE;
		msg.style.style = DmStyle_retain(sty);
		rv = DmMessageList_add(&trans->messages, msg);
		if (rv != DmResult_SUCCESS) {
			return rv;
		}

		msg.type = DmMessage_CHORD;
		msg.chord = *chord;
		rv = DmMessageList_add(&trans->messages, msg);
		if (rv != DmResult_SUCCESS) {
			return rv;
		}

		if (embellishment == DmEmbellishment_END_AND_INTRO) {
			// Complex "extro" plus "intro" transitoon
			msg.type = DmMessage_COMMAND;
			msg.command.command = DmCommand_END;
			msg.command.groove_level = 1;
			msg.command.groove_range = 0;
			msg.command.repeat_mode = DmPatternSelect_NO_REPEAT;
			msg.command.beat = 0;
			msg.command.measure = 0;
			rv = DmMessageList_add(&trans->messages, msg);
			if (rv != DmResult_SUCCESS) {
				return rv;
			}

			trans->length = Dm_getMeasureLength(sty->time_signature);

			// TODO(lmichaelis): implement "end-and-intro" transitions
			Dm_report(DmLogLevel_WARN,
			          "DmPerformance: Complex END_AND_INTRO transition is not yet supported. Only playing END");
		} else {
			// Basic "extro"-style transition
			msg.type = DmMessage_COMMAND;
			msg.command.command = Dm_embellishmentToCommand(embellishment);
			msg.command.groove_level = 1;
			msg.command.groove_range = 0;
			msg.command.repeat_mode = DmPatternSelect_NO_REPEAT;
			msg.command.beat = 0;
			msg.command.measure = 0;
			rv = DmMessageList_add(&trans->messages, msg);
			if (rv != DmResult_SUCCESS) {
				return rv;
			}

			trans->length = Dm_getMeasureLength(sty->time_signature);
		}
	}

	msg.type = DmMessage_SEGMENT;
	msg.time = trans->length;
	msg.segment.segment = DmSegment_retain(sgt);
	msg.segment.loop = 0;
	rv = DmMessageList_add(&trans->messages, msg);
	if (rv != DmResult_SUCCESS) {
		return rv;
	}

	return DmResult_SUCCESS;
}
