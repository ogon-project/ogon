Implementing a ogon Session Manager
=============

# Introduction

This document gives instructions and tips to write a ogon Session Manager for a ogon RDP Server. We don't give
full protocols structure here, refer to the corresponding documents for the full specifications.

# Architecture

## ogon RDP Server and the ogon Session Manager

The ogon RDP Server is connected to the ogon Session Manager using a named pipe / unix socket. The protocol spoken
is protocol buffer based. 
At the lower layer, the protocol is called pbrpc for ProtocolBuffer RPC. The connection between the
ogon RDP Server and the ogon Session Manager is bidirectionnal: Either the ogon RDP Server or the ogon Session Manager can initiate
a request. The protocol is also asynchronous: many requests can be sent by a peer before
the reply has been received.

On top of pbrcp, the ogon calls are encoded using a protocol buffer based protocol, its name is ICP
(Internal Communication Protocol). The ICP protocol contains all the messages that can be exchanged between 
ogon RDP Server and the ogon Session Manager.
 
In some case you need to communicate directly from a ogon backend to the ogon Session Manager. This 
is the case for example when you press "Login" in the greeter application: the greeter needs to talk 
directly to the ogon Session Manager. So in the ICP protocol there's a special message to pass a message through 
the ogon RDP Server to the ogon Session Manager. The ogon RDP Server will not try to interpret that message and will pass the payload 
directly the ogon Session Manager. This kind of message is interesting if you want to
develop a custom application with a custom ogon Session Manager.


## Peripheral programs and the ogon Session Manager

The ogon Session Manager shipped with ogon can also be contacted by external programs using
a thrift connection. This API is used by cli programs to do operations or by WTS library to 
handle channels 
 
## ogon Terminal Server API (OTSAPI)

The OTSAPI allows client programs to communicate with the ogon Session Manager. The OTSAPI is designed to be API compatible
with the Microsoft [WTSAPI], nevertheless some addidtions where necessary.
The OTSAPI is needed for virtual channel communication and for managing sessions (listing, disconnecting, shadowing, ...).
The additions (WTSLogoffUser, WTSLogonUser) are necessary to get and an authentication token, where the Microsoft [WTSAPI]
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

