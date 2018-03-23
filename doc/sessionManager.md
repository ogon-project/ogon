ogon Session Manager
======================

# Introduction

This document describes the current c++ ogon Session Manager.

# Purpose

The ogon Session Manager handles (start, stop) sessions and keeps track of them.

# Architecture

## Connectivity

The ogon Session Manager and the ogon RDP Server communicate using the Internal Communication Protocol (ICP).
The ICP uses as base the Protocol Buffer RPC (PBRCP) which uses a pipe as transport.

The OTSAPI uses a thrift multi-threaded server for rpc request handling. A TLS secured tcp connection is used as transport
between the rpc client and server. A single thread per client connection is used.

The ogon Session Manager to backend protocol (SBP) is routed through the ogon RDP Server. For that a special ICP call is defined,
which handles all SBP calls. Also the answer (result) is routed back to the ogon Session Manager.

## Session Handling

The session handling is done by a synchronized session store. To synchronize the session state itself, session state modifications
are only made in a worker thread per session. This session worker thread handles so-called tasks. These tasks are executed
one after another and only these tasks modify the session state. If one task needs to modify another session's state, a task
is generated for this action. After the other session has finished the task execution, the result can be handled. So
tasks need to be carefully developed, to prevent deadlock situations.
Also calls from the OTSAPI need to create tasks if a session-state is modified.

## Backend Launching

For definition of backends look at the [architecture document](architecture.md).

Each backend-module can be loaded directly into the ogon Session Manager or can be loaded through the backend-launcher.
If the backend-launcher is used, the ogon Session Manager communicates to the launcher with the Module protocol
(protocol buffers). Standard streams (stdin, stdout) are used as transport. Also, if the module is leaking memory or doing other bad
stuff, the ogon Session Manager is not affected.


## ogon Terminal Server API (OTSAPI)

The OTSAPI allows client programs to communicate with the ogon Session Manager. The OTSAPI is designed to be API compatible
with the Microsoft [WTSAPI], nevertheless some addidtions where necessary.
The OTSAPI is needed for virtual channel communication and for managing sessions (listing, disconnecting, shadowing, ...).
The additions (WTSLogoffUser, WTSLogonUser) are necessary to get and an authentication token, where the Mirosoft [WTSAPI]
uses the Windows single sign-on token to authenticate the user.

Implemented API functions so far:

WTSStartRemoteControlSession
WTSStopRemoteControlSession
WTSOpenServer
WTSCloseServer
WTSEnumerateSessions
WTSQuerySessionInformation
WTSSendMessage
WTSDisconnectSession
WTSLogoffSession
WTSVirtualChannelOpenEx
WTSVirtualChannelOpen
WTSVirtualChannelClose
WTSVirtualChannelRead
WTSVirtualChannelWrite
WTSVirtualChannelQuery
WTSFreeMemory

Additions:
WTSLogoffUser
WTSLogonUser






[WTSAPI]:https://msdn.microsoft.com/en-us/library/aa383464%28v=vs.85%29.aspx

