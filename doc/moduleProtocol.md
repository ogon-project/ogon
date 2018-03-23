ogon module protocol
=============

# Introduction

This document describes the protocol that is used between the ogon Session Manager and the backend-launcher.

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



# Protocol messages

Each message requires an answer.

## Messages from the ogon Session Manager to the backend-launcher.

### ModuleStart

This message is sent to start a specific backend.

The message contains:

* sessionId - id of the started session.
* userName - Username for which user the session should be started.
* userDomain - Userdomain for which user the session should be started.
* baseConfigPath - is the configuration path for the module. For example "module.xsession".
* envBlock - the envBlock which should be used to start the backend.
* moduleFileName - name of the Module which should be started.
* remoteIp - the remote IP address of the client.

The response:

* pipeName - the pipename where to connect to the backend. If NULL there was an error.

### ModuleStart

This message is sent to stop a started backend.

The message contains nothing.

The response:

* success: 0 on success otherwise an error code.

### ModuleGetCustomInfo

This message is sent to request the custom info of a module.

The message contains nothing.

The response:

* customInfo: the custom info of the module. For example the X11 module returns the displaynumber, the Qt module
    returns the processID.

### ModuleConnect

This message is sent if a client connects to an already started module. This gives the module the chance to execute
some code on connection time. For example the X11 module executes the virtual channel script at that point of time.

The message contains nothing.

The response:

* success: 0 on success otherwise an error code.

### ModuleDisconnect

This message is sent if a client disconnect from an already started module. This gives the module the chance to execute
some code if a client disconnects.

The message contains nothing.

The response:

* success: 0 on success otherwise an error code.

### ModuleExit

This message is sent if the module is no longer needed and the backend-launcher can exit. This gives the backend-launcher
the chance to exit gracefully, before it gets killed after a timeout from the ogon Session Manager.

The message contains nothing.

The response:

* success: true if no error occurred, otherwise false.


## Messages from the ogon backend-launcher to the ogon Session Manager.

A module may need access to the configuration. For that a module can request this configuration from the ogon Session Manager
while a call is served for the ogon Session Manager.

### PropertyBool

This message is sent if a bool property is requested from the configuration.

Th message contains:

* sessionId: the sessionId of the session for which the configuration value is requested.
* path: the name (path) of the configuration which should be received.

The response:

* success: true if a config value has been found, otherwise false.
* value: the value

### PropertyNumber

This message is sent if a number property is requested from the configuration.

Th message contains:

* sessionId: the sessionId of the session for which the configuration value is requested.
* path: the name (path) of the configuration which should be received.

The response:

* success: true if a config value has been found, otherwise false.
* value: the value

### PropertyString

This message is sent if a string property is requested from the configuration.

Th message contains:

* sessionId: the sessionId of the session for which the configuration value is requested.
* path: the name (path) of the configuration which should be received.

The response:

* success: true if a config value has been found, otherwise false.
* value: the value
