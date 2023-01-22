/**
 * ogon - Free Remote Desktop Services
 * pbRPC: Simple Protocol Buffers based RPC
 * Transport Protocol Abstraction
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

#ifndef OGON_RDPSRV_ICP_PBRPCTRANSPORT_H_
#define OGON_RDPSRV_ICP_PBRPCTRANSPORT_H_

#include <winpr/wtypes.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct pbrpc_transport_context pbRPCTransportContext;

typedef int (*pTransport_open)(pbRPCTransportContext* context, int timeout);
typedef int (*pTransport_close)(pbRPCTransportContext* context);
typedef int (*pTransport_write)(pbRPCTransportContext* context, char* data, unsigned int datalen);
typedef int (*pTransport_read)(pbRPCTransportContext* context, char* data, unsigned int datalen);
typedef HANDLE (*pTransport_get_fds)(pbRPCTransportContext* context);

struct pbrpc_transport_context
{
	pTransport_open open;
	pTransport_close close;
	pTransport_read read;
	pTransport_write write;
	pTransport_get_fds get_fds;
};

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* OGON_RDPSRV_ICP_PBRPCTRANSPORT_H_ */
