ogon Session Manager to backend protocol
=============

# Introduction

This document describes the protocol that is used between the ogon Session Manager and a backend.
The SBP uses the ICP connection for transport and uses the same wire format. The MSGTYPE has to be 200 or greater to
 be treated as SBP Message from the ogon RDP Server and to be forwarded to the backend.

# Protocol

For the packing and the wire format please have a look at the ICP protocol definition.
The messages can go both directions, but at the moment only calls from the backend to the ogon Session Manager are implemented
so far.

## ogon backend to ogon Session Manager messages

### AuthenticateUser

This message is sent if a backend needs to authenticate a user. This is obviously used for greeters, so the entered
authentication informatio can be transmitted to the ogon Session Manager.

The message contains:

* sessionId - the sessionId to identify the session.
* username - the username
* password - the password
* domain - the domain

The response:

* authStatus - result of the authentication:
    	- AUTH_SUCCESSFUL: Authentication was succesful.
    	- AUTH_BAD_CREDENTIAL: bad credentials.
    	- AUTH_WRONG_SESSION_STATE: session was in wrong state.
    	- AUTH_UNKOWN_ERROR : unknown error.


### EndSession

This message is sent if a backend wants to terminate. This is only used in the greeter so far, if cancel gets pressed.

The message contains:

* sessionId - the sessionId to identify the session.

The response:

* success - ture if successfully otherwise false.




