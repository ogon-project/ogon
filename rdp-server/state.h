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

#ifndef OGON_RDPSRV_STATE_H_
#define OGON_RDPSRV_STATE_H_

/* @brief states of the frame emitting automata
 */
enum _ogon_state {
	OGON_STATE_INVALID                   = 0,
	OGON_STATE_WAITING_BACKEND           = 1,
	OGON_STATE_WAITING_TIMER             = 2,
	OGON_STATE_WAITING_SYNC_REPLY        = 3,
	OGON_STATE_WAITING_ACTIVE_OUTPUT     = 4,
	OGON_STATE_WAITING_FRAME_SENT        = 5,
	OGON_STATE_WAITING_ACK               = 6,
	OGON_STATE_EVENTLOOP_MOVE            = 7,
	OGON_STATE_WAITING_RESIZE            = 8,
};
typedef enum _ogon_state ogon_state;

enum _ogon_event {
	OGON_EVENT_INVALID, /* Invalid Event */
	OGON_EVENT_BACKEND_ATTACHED, /* Backend was sucessfully attached */
	OGON_EVENT_BACKEND_SYNC_REPLY_RECEIVED, /* Sync reply received from backend */
	OGON_EVENT_BACKEND_SYNC_REQUESTED, /* requested sync from backend*/
	OGON_EVENT_BACKEND_REWIRE_ORIGINAL, /* rewire original backend */
	OGON_EVENT_BACKEND_TRIGGER_REWIRE, /* trigger the backend rewiring */
	OGON_EVENT_BACKEND_SWITCH, /* switch to new backend*/
	OGON_EVENT_FRONTEND_ENABLE_OUTPUT, /* Enable output */
	OGON_EVENT_FRONTEND_DISABLE_OUTPUT, /* Disable output */
	OGON_EVENT_FRONTEND_FRAME_ACK_SEND, /* Received frame ack */
	OGON_EVENT_FRONTEND_FRAME_ACK_RECEIVED, /* Received frame ack */
	OGON_EVENT_FRONTEND_FRAME_SENT, /* Received frame sent to client */
	OGON_EVENT_FRONTEND_IMMEDIATE_REQUEST, /* Received frame ack */
	OGON_EVENT_FRONTEND_NEW_SHADOWING , /* Got new shadowing frontend*/
	OGON_EVENT_FRONTEND_REWIRE_ERROR , /* Problem rewiring the backend */
	OGON_EVENT_FRONTEND_TRIGGER_RESIZE , /* Problem rewiring the backend */
	OGON_EVENT_FRONTEND_RESIZED , /* Finished the frontend resize */
	OGON_EVENT_FRAME_TIMER , /* Frame timer happend */
	OGON_EVENT_FRONTEND_WAITING_GFX, /* waiting for gfx channel */
	OGON_EVENT_FRONTEND_STOP_WAITING_GFX, /* stop waiting for gfx channel */
	OGON_EVENT_FRONTEND_BANDWIDTH_FAIL, /* bandwidth exceeded */
	OGON_EVENT_FRONTEND_BANDWIDTH_GOOD, /* bandwidth available */
};
typedef enum _ogon_event ogon_event;

struct _ogon_state_machine;
typedef struct _ogon_state_machine ogon_state_machine;

ogon_state_machine *ogon_state_new();
void ogon_state_free(ogon_state_machine *stateMachine);
void ogon_state_set_event(ogon_state_machine *stateMachine, ogon_event event);
ogon_state ogon_state_get(ogon_state_machine *stateMachine);
void ogon_state_prepare_shadowing(ogon_state_machine *src, ogon_state_machine *dst);
BOOL ogon_state_should_create_frame(ogon_state_machine *stateMachine);

#endif /* OGON_RDPSRV_STATE_H_ */
