ogon RDP server backend protocol
=============

# Introduction

This document describes the protocol that is used between the ogon RDP server and the ogon Session Manager.

# Protocol

[Google protocol buffers] is used for serialization. Since protobuf doesn't provide a *compatible* RPC implementation for
different languages which also support bidirectional RPC (over one socket) this needs to be implemented.

The protocol itself is kept simple. The wire format consists of two fields:
* 32 bit length (in network byte order)
* serialized pbRPC message

The pbRPC message is the basic transmission unit for RPC messages (in both directions) and includes:
* Tag - to uniquely identify a request/response
* Response - a field to set if the message is a response to a call
* Status - status of the call (set to SUCCESS in case of a request)
* Message type - type of the containing request/response
* Payload - serialized protocol buffer message containing the request/response itself

With the fields response and message type one can identify the type of a message that is contained in payload and
if its a request or response (if response is set to true it's a response)
The tag field must be used to identify the corresponding response for a request.
One basic rule for pbrpc is that *every* message must be responded to.


## Messages from the ogon RDP server to the ogon Session Manager

### DisconnectUserSession

This message is sent if the rdp client got or will get disconnected.

The message contains:

* connectionId - the connection identifier.

The response:

* disconnected - true if the disconnect was successful, otherwise false.


### LogonUser

This message is sent if a new rdp client connection has connected to the ogon RDP Server. With this message the
new client authenticates itself and gets a user session or a greeter if the logon was not successful.

The message contains:

* connectionId - the connection identifier.
* username - the username
* password = the password
* domain - the domain
* width - the requested desktop width.
* height - the requested desktop height.
* colorDepth - the requested colorDepth
* clientHostName - the client hostname of the rdp-connection.
* clientAddress - the client address of the rdp-connection.
* clientBuildNumber - the build number of the client.
* clientProductId - the product id of the client.
* clientHardwareId - the hardware id of the client.
* clientProtocolType - the used protocol type to communicate with the client.


The response:

* serviceEndpoint - is a pipename where to connect, otherwise null on error.
* maxHeight - maximal height which is allowed for that session.
* maxWidth - maximal width which is allowed for that session.
* backendCookie - the cookie to identify the backend.
* ogonCookie - the cookie to identify the ogon session.


### PropertyBool

This message is sent if a boolean configuration value has to be queried from the ogon Session Manager.

The message contains:

* connectionId - the connection identifier.
* path - path and name of the value to query.

The response:

* value - the value of the requested configuration value, if success is true.
* success - true if value was found, otherwise false, also on error.


### PropertyNumber

This message is sent if a numeric configuration value has to be queried from the ogon Session Manager.

The message contains:

* connectionId - the connection identifier.
* path - path and name of the value to query.

The response:

* value - the value of the requested configuration value, if success is true.
* success - true if value was found, otherwise false, also on error.

### PropertyString

This message is sent if a string configuration value has to be queried from the ogon Session Manager.

The message contains:

* connectionId - the connection identifier.
* path - path and name of the value to query.

The response:

* value - the value of the requested configuration value, if success is true.
* success - true if value was found, otherwise false, also on error.


### RemoteControlEnded

This message is sent if if a spy (shadowing session) disconnects, or gets disconnected.

The message contains:

* spyId - the connection identifier of the spy.
* spiedId - the connection identifier of the spied (shadowed) session.

The response:

* success - true if value was found, otherwise false, also on error.

### PropertyBulk

(since protocol version 1.1)
This message is sent to retrieve a set of properties from the Session Manager.

The message contains:

* connectionId - the connection identifier.
* properties - an array of PropertyReq, each one containing the path and the type of the property

The response:

* results - an array of PropertyValue in the same order than the request. Each PropertyValue contains if
the property was successfully found and the retrieved value


## Messages from the ogon Session Manager to the ogon RDP server.

### ping

This message is sent to check the ogon RDP Server for response.

The message contains nothing.

The response:

* pong - true or false.


### switchTo

This message is sent if the backend should be switched to another backend.

The message contains:

* connectionId - the connection identifier.
* serviceEndpoint - Pipename of the new backend.
* maxHeight - Maximal supported Height of the new backend.
* maxWidth - Maximal supported Width of the new backend.
* backendCookie - the cookie of the new backend.
* ogonCookie - ogon authentication cookie.

The response:

* success - true if switch was success, otherwise false.


### logoffUserSession

This message is sent if the backend should be switched to another backend.

The message contains:

* connectionId - the connection identifier.

The response:

* loggedoff - true if logoff was successful, otherwise false.


### otsapiVirtualChannelOpen

This message is sent if a virtual channel needs to be opened.

The message contains:

* connectionId - the connection identifier.
* virtualName - name of the virtual channel to open.
* dynamicChannel - if its a dynamic channel or not
* flags - flags of the virtual channel to open.

The response:

* connectionString - Pipename of the created virtual channel.
* instance - instance count of the virtual channel. This is required to be closed properly.


### otsapiVirtualChannelClose

This message is sent if a virtual channel needs to be closed.

The message contains:

* connectionId - the connection identifier.
* virtualName - name of the virtual channel to open.
* instance - instance of the opened virtual channel. This value is returned as result from an open call.

The response:

* success - True if close was a success otherwise false.


### otsapiVirtualChannelClose

This message is sent if a virtual channel needs to be closed.

The message contains:

* connectionId - the connection identifier.
* virtualName - name of the virtual channel to open.
* instance - instance of the opened virtual channel. This value is returned as result from an open call.

The response:

* success - True if close was a success otherwise false.



### otsapiStartRemoteControl

This message is sent if a shadowing / remote control is requested over the OTSAPI.

The message contains:

* connectionId - the connection identifier.
* targetConnectionId - the connectionId of the session which should be shadowed.
* hotKeyVk - Virtual Key Code of the hotkey to stop shadowing.
* hotKeyModifiers - The virtual hotkey modifier which has to be present if the hotkey is pressed.
                    - REMOTECONTROL_KBDSHIFT_HOTKEY: SHIFT key
                    - REMOTECONTROL_KBDCTRL_HOTKEY: CTRL key
                    - REMOTECONTROL_KBDALT_HOTKEY: ALT key
* flags - flags to define behaviour of the shadowing session:
                    - REMOTECONTROL_FLAG_DISABLE_KEYBOARD: keyboard input disabled
                    - REMOTECONTROL_FLAG_DISABLE_MOUSE: mouse input disabled
                    - REMOTECONTROL_FLAG_DISABLE_INPUT: all input disabled
The response:

* success - True if shadowing was a success otherwise false.


### otsapiStopRemoteControl

This message is sent if a shadowing / remote control has to be stopped.

The message contains:

* connectionId - the connection identifier.

The response:

* success - True if stopping the shadowing was a success otherwise false.


### message

This message is sent if a user interaction is required. This message travels over the ogon RDP Server to the backend.
Where it is displayed and the response (or a timeout) is sent back as response.

The message contains:

* connectionId - the connection identifier.
* messageType: The type of the message, 1 for a custom message.
* style: defines the content and behavior of the message to display. The styles are taken from the Windows
                    [MessageBox] style definition.
* timeout: 0 if no timeout is used. Otherwise the timeout in seconds. After the timeout occurs IDTIMEOUT is
                    returned.
* parameter_num: number of parameters included in parameters.
* parameters: List of all parameters.

MessageTypes so far:
 * MESSAGE_CUSTOM_TYPE (1): custom message
    First parameter is the message header.
    Second parameter is the displayed message.

 * MESSAGE_REQUEST_REMOTE_CONTROL (2): Asks for allowance for remote control.
    First parameter is the username who asks for remote control.


The response:

* response - The returncodes used are taken from the Windows [MessageBox] return code definition.



[Google protocol buffers]:https://developers.google.com/protocol-buffers/
[MessageBox]:https://msdn.microsoft.com/en-us/library/ms645505%28v=vs.85%29.aspx