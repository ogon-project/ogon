/**
 * ogon - Free Remote Desktop Services
 * Backend Library
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * Bernhard Miklautz <bernhard.miklautz@thincast.com>
 * David Fort <contact@hardening-consulting.com>
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
 * Under the GNU Affero General Public License version 3 section 7 the
 * copyright holders grant the additional permissions set forth in the
 * ogon Library AGPL Exceptions version 1 as published by
 * Thincast Technologies GmbH.
 *
 * For more information see the file LICENSE in the distribution of this file.
 */

#ifndef _OGON_BACKEND_PROTOCOL_H_
#define _OGON_BACKEND_PROTOCOL_H_

#include <ogon/backend.h>
#include "backend.pb-c.h"

#include <winpr/stream.h>
#include <freerdp/api.h>


#define RDS_ORDER_HEADER_LENGTH	6

/** @brief all protobuf messages in a union */
typedef union _ogon_protobuf_message {
	Ogon__Backend__Capabilities capabilities;
	Ogon__Backend__KeyboardSync keyboardSync;
	Ogon__Backend__KeyboardScanCode keyboardScanCode;
	Ogon__Backend__KeyboardVirtualScanCode keyboardVirtualScanCode;
	Ogon__Backend__KeyboardUnicode keyboardUnicode;
	Ogon__Backend__MouseEvent mouseEvent;
	Ogon__Backend__MouseExtendedEvent mouseExtendedEvent;
	Ogon__Backend__SyncRequest syncRequest;
	Ogon__Backend__ImmediateSyncRequest immediateSyncRequest;
	Ogon__Backend__SbpReply sbpReply;
	Ogon__Backend__SeatNew seatNew;
	Ogon__Backend__SeatRemoved seatRemoved;
	Ogon__Backend__Message message;
	Ogon__Backend__VersionReply versionReply;
	Ogon__Backend__FramebufferInfos framebufferInfos;
	Ogon__Backend__SetPointerShape setPointerShape;
	Ogon__Backend__Beep beep;
	Ogon__Backend__SetSystemPointer setSystemPointer;
	Ogon__Backend__SbpRequest sbpRequest;
	Ogon__Backend__SyncReply syncReply;
	Ogon__Backend__MessageReply messageReply;
} ogon_protobuf_message;


#endif /* _OGON_BACKEND_PROTOCOL_H_ */
