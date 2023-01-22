/**
 * ogon - Free Remote Desktop Services
 * RDP Server
 * State Machine
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * Bernhard Miklautz <bernhard.miklautz@thincast.com>
 *
 * This file may be used under the terms of the GNU Affero General
 * Public License version 3 as published by the Free Software Foundation
 * and appearing in the file LICENSE-AGPL included in the distribution
 * of this file.
 *
 * Under the GNU Affero General Public License version 3 section 7 the
 * copyright holders grant the additional permissions set forth in the
 * ogon Core AGPL Exceptions version 1 as published by
 * Thincast Technologies GmbH.
 *
 * For more information see the file LICENSE in the distribution of this file.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../common/global.h"
#include "state.h"

#define TAG OGON_TAG("core.state")

#ifdef WITH_DEBUG_STATE
#define DEBUG_STATE(fmt, ...) WLog_DBG(TAG, fmt, ## __VA_ARGS__)
#else
#define DEBUG_STATE(fmt, ...) do { } while (0)
#endif

struct _ogon_state_machine {
	ogon_state currentState;
	BOOL resizePending;
	BOOL outputSuppressed;
	BOOL waitingForBandwidth;
	BOOL waitingGraphicsPipeline;
	BOOL handleFrame;
};

#ifdef WITH_DEBUG_STATE
static const char *get_state_name(ogon_state state) {
	switch(state) {
		case OGON_STATE_INVALID:
			return "INVALID";
		case OGON_STATE_WAITING_BACKEND:
			return "WAITING_BACKEND";
		case OGON_STATE_WAITING_TIMER:
			return "WAITING_TIMER";
		case OGON_STATE_WAITING_SYNC_REPLY:
			return "WAITING_SYNC_REPLY";
		case OGON_STATE_WAITING_ACTIVE_OUTPUT:
			return "WAITING_ACTIVE_OUTPUT";
		case OGON_STATE_WAITING_FRAME_SENT:
			return "WAITING_FRAME_SENT";
		case OGON_STATE_WAITING_ACK:
			return "WAITING_ACK";
		case OGON_STATE_EVENTLOOP_MOVE:
			return "EVENTLOOP_MOVE";
		case OGON_STATE_WAITING_RESIZE:
			return "WAITING_RESIZE";
		default:
			return "UNKNOWN STATE!";
	}
}

static const char *get_event_name(ogon_event event) {
	switch(event) {
		case OGON_EVENT_INVALID:
			return "INVALID";
		case OGON_EVENT_BACKEND_ATTACHED:
			return "BACKEND_ATTACHED";
		case OGON_EVENT_BACKEND_SYNC_REPLY_RECEIVED:
			return "BACKEND_SYNC_REPLY_RECEIVED";
		case OGON_EVENT_BACKEND_SYNC_REQUESTED:
			return "BACKEND_SYNC_REQUESTED";
		case OGON_EVENT_BACKEND_REWIRE_ORIGINAL:
			return "BACKEND_REWIRE_ORIGINAL";
		case OGON_EVENT_BACKEND_TRIGGER_REWIRE:
			return "BACKEND_TRIGGER_REWIRE";
		case OGON_EVENT_BACKEND_SWITCH:
			return "BACKEND_SWITCH";
		case OGON_EVENT_FRONTEND_ENABLE_OUTPUT:
			return "FRONTEND_ENABLE_OUTPUT";
		case OGON_EVENT_FRONTEND_DISABLE_OUTPUT:
			return "FRONTEND_DISABLE_OUTPUT";
		case OGON_EVENT_FRONTEND_FRAME_ACK_SEND:
			return "FRONTEND_FRAME_ACK_SEND";
		case OGON_EVENT_FRONTEND_FRAME_ACK_RECEIVED:
			return "FRONTENhandle_D_FRAME_ACK_RECEIVED";
		case OGON_EVENT_FRONTEND_FRAME_SENT:
			return "FRONTEND_FRAME_SENT";
		case OGON_EVENT_FRONTEND_IMMEDIATE_REQUEST:
			return "FRONTEND_IMMEDIATE_REQUEST";
		case OGON_EVENT_FRONTEND_NEW_SHADOWING:
			return "FRONTEND_NEW_SHADOWING";
		case OGON_EVENT_FRONTEND_REWIRE_ERROR:
			return "FRONTEND_REWIRE_ERROR";
		case OGON_EVENT_FRONTEND_TRIGGER_RESIZE:
			return "FRONTEND_TRIGGER_RESIZE";
		case OGON_EVENT_FRONTEND_RESIZED:
			return "FRONTEND_RESIZED";
		case OGON_EVENT_FRAME_TIMER:
			return "FRAME_TIMER";
		case OGON_EVENT_FRONTEND_WAITING_GFX:
			return "FRONTEND_WAITING_GFX";
		case OGON_EVENT_FRONTEND_STOP_WAITING_GFX:
			return "FRONTEND_STOP_WAITING_GFX";

		default:
			return "UNKNOWN EVENT!";
	}
}
#endif

ogon_state_machine *ogon_state_new() {
	ogon_state_machine *stateMachine = calloc(1, sizeof(ogon_state_machine));
	if (!stateMachine) {
		return stateMachine;
	}
	stateMachine->currentState = OGON_STATE_WAITING_BACKEND;
	return stateMachine;
}

void ogon_state_free(ogon_state_machine *stateMachine) {
	free(stateMachine);
}

static inline void ogon_state_set_wait_frame_sent(ogon_state_machine *stateMachine) {
	stateMachine->currentState = OGON_STATE_WAITING_FRAME_SENT;
}

static inline void ogon_state_set_waiting_active_output(ogon_state_machine *stateMachine) {
	stateMachine->currentState = OGON_STATE_WAITING_ACTIVE_OUTPUT;
}

static inline void ogon_state_set_waiting_timer(ogon_state_machine *stateMachine) {
	stateMachine->currentState = OGON_STATE_WAITING_TIMER;
}

static inline void ogon_state_set_waiting_sync_reply(ogon_state_machine *stateMachine) {
	/* In case resize is pending an we are waiting for a timer keep that state */
	if (stateMachine->resizePending && stateMachine->currentState == OGON_STATE_WAITING_TIMER) {
		return;
	}
	stateMachine->currentState = OGON_STATE_WAITING_SYNC_REPLY;
}

static inline void ogon_state_set_waiting_ack(ogon_state_machine *stateMachine) {
	stateMachine->currentState = OGON_STATE_WAITING_ACK;
}

static inline void ogon_state_set_eventloop_move(ogon_state_machine *stateMachine) {
	stateMachine->currentState = OGON_STATE_EVENTLOOP_MOVE;
}

static inline void ogon_state_set_waiting_backend(ogon_state_machine *stateMachine) {
	stateMachine->currentState = OGON_STATE_WAITING_BACKEND;
}

void ogon_state_set_event(ogon_state_machine *stateMachine, ogon_event event) {
#ifdef WITH_DEBUG_STATE
	ogon_state before = stateMachine->currentState;
#endif
	DEBUG_STATE("Event %s", get_event_name(event));

	switch(event){

	case OGON_EVENT_BACKEND_SYNC_REPLY_RECEIVED:
		if (stateMachine->outputSuppressed || stateMachine->waitingGraphicsPipeline) {
			ogon_state_set_waiting_active_output(stateMachine);
		} else {
			ogon_state_set_wait_frame_sent(stateMachine);
		}
		break;

	case OGON_EVENT_FRONTEND_DISABLE_OUTPUT:
		stateMachine->outputSuppressed = TRUE;
		break;

	case OGON_EVENT_FRONTEND_ENABLE_OUTPUT:
		if (!stateMachine->waitingGraphicsPipeline && stateMachine->currentState == OGON_STATE_WAITING_ACTIVE_OUTPUT) {
			stateMachine->handleFrame = TRUE;
			ogon_state_set_waiting_timer(stateMachine);
		}
		stateMachine->outputSuppressed = FALSE;
		break;

	case OGON_EVENT_FRONTEND_WAITING_GFX:
		stateMachine->waitingGraphicsPipeline = TRUE;
		break;

	case OGON_EVENT_FRONTEND_STOP_WAITING_GFX:
		if (!stateMachine->outputSuppressed && stateMachine->currentState == OGON_STATE_WAITING_ACTIVE_OUTPUT) {
			stateMachine->handleFrame = TRUE;
			ogon_state_set_waiting_timer(stateMachine);
		}
		stateMachine->waitingGraphicsPipeline = FALSE;
		break;

	case OGON_EVENT_FRONTEND_BANDWIDTH_FAIL:
		stateMachine->waitingForBandwidth = TRUE;
		break;

	case OGON_EVENT_FRONTEND_BANDWIDTH_GOOD:
		stateMachine->waitingForBandwidth = FALSE;
		break;

	case OGON_EVENT_FRONTEND_FRAME_ACK_RECEIVED:
		ogon_state_set_waiting_timer(stateMachine);
		break;

	case OGON_EVENT_BACKEND_SYNC_REQUESTED:
		stateMachine->handleFrame = FALSE;
		ogon_state_set_waiting_sync_reply(stateMachine);
		break;

	case OGON_EVENT_FRONTEND_FRAME_ACK_SEND:
		ogon_state_set_waiting_ack(stateMachine);
		break;

	case OGON_EVENT_FRONTEND_FRAME_SENT:
		ogon_state_set_waiting_timer(stateMachine);
		break;

	case OGON_EVENT_FRONTEND_IMMEDIATE_REQUEST:
		stateMachine->handleFrame = TRUE;
		ogon_state_set_waiting_sync_reply(stateMachine);
		break;

	case OGON_EVENT_BACKEND_TRIGGER_REWIRE:
		stateMachine->handleFrame = TRUE;
		ogon_state_set_eventloop_move(stateMachine);
		break;

	case OGON_EVENT_FRONTEND_NEW_SHADOWING:
		stateMachine->handleFrame = TRUE;
		ogon_state_set_waiting_timer(stateMachine);
		break;

	case OGON_EVENT_BACKEND_REWIRE_ORIGINAL:
		stateMachine->handleFrame = TRUE;
		ogon_state_set_waiting_timer(stateMachine);
		break;

	case OGON_EVENT_FRONTEND_REWIRE_ERROR:
		ogon_state_set_waiting_timer(stateMachine);
		break;

	case OGON_EVENT_BACKEND_SWITCH:
		stateMachine->handleFrame = TRUE;
		ogon_state_set_waiting_backend(stateMachine);
		break;

	case OGON_EVENT_FRONTEND_TRIGGER_RESIZE:
		stateMachine->resizePending = TRUE;
		break;

	case OGON_EVENT_BACKEND_ATTACHED:
		stateMachine->handleFrame = TRUE;
		ogon_state_set_waiting_timer(stateMachine);
		break;

	case OGON_EVENT_FRONTEND_RESIZED:
		stateMachine->resizePending = FALSE;
		break;

	case OGON_EVENT_FRAME_TIMER:
		stateMachine->handleFrame = TRUE;
		break;

	default:
		WLog_DBG(TAG, "Unhandled event! (%d)", event);

	}

	DEBUG_STATE("Transition from %s -> %s (resize pending: %s)", get_state_name(before),
		get_state_name(stateMachine->currentState),
		stateMachine->resizePending ? "YES" : "NO");
}

ogon_state ogon_state_get(ogon_state_machine *stateMachine) {
	if (stateMachine->resizePending) {
		return OGON_STATE_WAITING_RESIZE;
	}
	return stateMachine->currentState;
}

void ogon_state_prepare_shadowing(ogon_state_machine *spy, ogon_state_machine *src) {
	spy->handleFrame = src->handleFrame;
}

BOOL ogon_state_should_create_frame(ogon_state_machine *stateMachine) {
	DEBUG_STATE("Should handle frame? - handleFrame: %"PRId32" - state: %s",
		stateMachine->handleFrame, get_state_name(stateMachine->currentState));

	if ((stateMachine->currentState == OGON_STATE_WAITING_TIMER ||
		stateMachine->currentState == OGON_STATE_WAITING_SYNC_REPLY)
		&& stateMachine->handleFrame && !stateMachine->resizePending
		&& !stateMachine->outputSuppressed && !stateMachine->waitingForBandwidth)
	{
		return TRUE;
	}

	return FALSE;
}
