ogon RDP server backend protocol
=============

# Introduction

This document describes the protocol that is used between the ogon RDP server and the content
providers (also called backend).

# Protocol

[Google protocol buffers] is used for marshalling and unmarshalling the data
before. A pipe transport is used.

The following protocol is used:
* (UINT16) TYPE
* (UINT32) payloadLength
* (VOID*)  payload (encoded protobuf data)

*TODO: more on how things are working*

# Damage buffer

The damage buffer is used to transfer the framebuffer and the damage information from the backend to the
ogon RDP Server via shared memory.

Damage buffer data structure:
```C
UINT32 MAGIC;                // must be 0xCACAB0B1
UINT32 SHMID;                // shared Memory identifier
UINT32 WIDTH;                // width of framebuffer
UINT32 HEIGHT;               // height of framebuffer
UINT32 SCANLINE;             // scanline (size of a single horizontal framebuffer line)
UINT32 MAX_RECTS;            // maximum number of rectangles storable in the damage buffer
UINT32 NUM_RECTS;            // actual number of damaged rectangles
RDP_RECT RECTS[MAX_RECTS];   // RDP_RECT list
BYTE DATA[HEIGHT*SCANLINE];  // framebuffer bitmap data
```

The frame buffer bitmap data is expected to store each pixel in the ARGB32 format with alpha in the
upper 8 bits, then red, then green and blue in the lower 8 bits.

MSB             LSB
 [ A | R | G | B ]

# Protocol messages

Each message contains a header that is used to describe the kind and size of the message.

The format is the following:

* (UINT16) type: the kind of message.
* (UINT32) length: the length of the message payload (so not including the header).

So an implementation SHOULD read 6 bytes to read the common header, and then read 
commonHeader.length more bytes to have the full message.

The payload itself is encoded using google protobuf, the definition file of message
is [here](../protocols/protobuf/backend.proto).

## Messages from the ogon RDP server to the backend / content provider

### Capabilities
This message is sent by the ogon RDP as a handshake to the backend. This message allows the
backend to prepare a suitable environment for rendering the content to display. The backend
is expected to respond with a *framebuffer info* packet. 

The message contains:

* the version of the spoken protocol.
* the width, height and colorDepth (8 to 32) sent by the RDP peer.
* the keyboard features of the RDP peer (layout, type, subtype).
* and the connection id of the front connection. This can be considered as the id of the *main* seat
in shadowing scenario.


### Synchronize keyboard
This message is sent to synchronize the state of the keyboard modifiers and leds. Informations
are the same as these transported by a [RDP synchronize keyboard packet][synchronize packet] packet. For the backend,
this packet also means: *all keys up* (same meaning as in RDP).

The message contains:

* flags as in the [RDP spec][synchronize packet].
* the connection id of the front connection that want to synchronize the keyboard indicators.


### Scancode keyboard
This message is used to notify the backend that a front connection has a key pressed or released.

The message contains:

* flags as in the [RDP packet][scancode packet].
* the scancode as in the [RDP packet][scancode packet].
* as a reminder, the type of keyboard (already sent in the *capabilities* message).
* the connection id of the front connection that sends the key event.
 
### Virtual keyboard
This message is like the *scancode keyboard*, but this time a virtual keycode is sent instead of
a scancode.

The message contains:

* flags as in the [scancode RDP packet][scancode packet].
* the virtual keycode.
* the connection id of the front connection that sends the key event.

	
### Unicode keyboard
This message is used to notify the backend of a unicode keyboard event.

The message contains:

* flags as in the [RDP packet][unicode packet].
* unicode code as in the corresponding [RDP packet][unicode packet].
* the connection id of the front connection that sends the unicode event.


### Mouse event
This message is used to notify the motion or some button state changes of the mouse.
This message should be interpreted as what is reported by the corresponding [RDP packet][mouse packet].


The message contains:

* flags to be interpreted as in the [RDP spec][mouse packet].
* x, y coordinates to be interpreted as in the [RDP spec][mouse packet].
* the connection id of the front connection that reports the mouse event.

*Note:* Backends should be aware that sometime the x and y coordinates aren't correct, especially
when the event is about buttons state changes.

	
### Extended mouse
This message is used to notify the motion or some button state changes of the mouse.
This message should be interpreted as what is reported by the corresponding [RDP packet][extended mouse packet].

The message contains:

* flags to be interpreted as in the [RDP spec][mouse packet].
* x, y coordinates to be interpreted as in the [RDP spec][mouse packet].
* the connection id of the front connection that reports the mouse event.

*Note:* Backends should be aware that sometime the x and y coordinates aren't correct, especially
when the event is about wheel buttons state changes.

	
### Sync request
This kind of packet instruct the backend to update the framebuffer and the damaged
regions (in the shared memory) and then to answer with a *Sync reply* packet. The *Sync reply*
packet MUST be sent only when the shared memory has been updated. If no damage were accumulated,
the backend SHOULD wait until some part of the screen are damaged. 

The message contains the target System V shm id that contains the *damage buffer* to update

### SBP reply

This kind of packet is sent by the ogon RDP server when it receives an SBP packet on its ICP channel from the
ogon Session Manager. The payload of the packet is opaque to ogon RDP server, it just forwards it to the backend.
An example of such packet is the result of the greeter authentication.


### Immediate sync request
This kind of packet is just like a *Sync request* but this time the ogon RDP server asks to immediately answer
with a *Sync reply* packet. Most of the time this packet is used to exit the situation when a *Sync request* 
has already been sent, and there's no damage available. In that situation, the backend _MUST_ reply with a *Sync reply*.
	
### New seat
This message is used to notify the arrival of a front connection wired on the backend.
This kind of message is sent when a new spy shadows a connection. The content provider
SHOULD store the keyboard features to correctly interpret further keyboard events.
The type of this packet is 110.

The message contains:

* the connection id of the new front connection.
* keyboard features (layout, type, subtype) of the remote peer as in the [RDP spec][MS-RDPBCGR].


### Seat removed
This message is sent when a front connection disconnects from this backend. It can be 
because the spy has disconnected, or pressed the escape sequence. 

The message contains the connection id of the removed front connection.


### Message

This message is sent by the ogon RDP server when the OTSAPI function WTSSendMessage was invoked.
This message is also used if shadowing asks for permission to shadow the current session.

The format of the message is:

* (UINT32) messageId: an id to identify the message afterwards.
* (UINT32) messageType: The type of the message, 1 for a custom message.
* (UINT32) style: defines the content and behavior of the message to display. The styles are taken from the Windows
                    [MessageBox] style definition.
* (UINT32) timeout: 0 if no timeout is used. Otherwise the timeout in seconds. After the timeout occurs IDTIMEOUT is
                    returned.
* (UINT32) parameter_num: number of parameters included in parameters.
* repeated string parameters: List of all parameters.

MessageTypes so far:
 * MESSAGE_CUSTOM_TYPE (1): custom message
    First parameter is the message header.
    Second parameter is the displayed message.

 * MESSAGE_REQUEST_REMOTE_CONTROL (2): Asks permission for remote control.
    First parameter is the username who asks for remote control.


### Get version
This message is sent by the ogon RDP server when it wants to request the version of the backend protocol.


This message has no other fields.


## Messages from the backend to the ogon RDP server

### Set pointer shape
This message is used to set the client pointer shape on the RDP peer. This message
is the equivalent of the RDP [new pointer update][new pointer update] packet.


### Framebuffer infos
This packet is sent by the backend either as an answer to a *capabilities* packet from the ogon RDP server, or
to notify the server that the resolution of the screen has changed (for example when
you're using xrandr in a Xrds session).
	
The message contains:

* the version of the protocol that the backend wants to talk.
* properties of the shared framebuffer: width, height, scanline, bitsPerPixel, bytesPerPixel.
* the UID of the process running the content provider / backend. This
information is given so that ogon can restrict the access to the shared memory
segments that will be used.

	
### Beep
This message is used to do a beep on the RDP client.

The message contain the same information as the equivalent [RDP PDU][play sound].

### Set system pointer
This kind packet is sent by the backend to set the kind of client-side pointer. It's
the equivalent of the [corresponding RDP][set system pointer] packet.

The message contains the type of system pointer as in the [corresponding RDP][set system pointer] packet.
Values for type are:

* `0x00000000` (SYSPTR_NULL): no client side pointer.
* `0x00007F00` (SYSPTR_DEFAULT): default system pointer.



### SBP request
This kind of packet is used by the backend to query the ogon Session Manager directly, using
the SBP protocol. The backend forges a protocol buffer encoded packet and sends it
using this kind of packet. The meaning of the data payload is opaque to the ogon RDP server it just forwards
the data blob to the ogon Session Manager using the ICP channel.



### Framebuffer sync reply
This packet is sent by the backend to express the ogon RDP server that the *damage buffer* in the
shared memory segment has been updated and can be safely accessed. It's an answer to a
*Sync request* or an *Immediate Sync request* packet. 


### Message reply
This message is answered by the backend when ogon RDP server has requested to show a user message.

The definition of the return value is used from the Windows [MessageBox].

### Get version
This message is answered by the backend when the ogon RDP server has requested version with a
*get version request* packet




[MS-RDPBCGR]: http://msdn.microsoft.com/en-us/library/cc240445.aspx
[scancode packet]: http://msdn.microsoft.com/en-us/library/cc240584.aspx
[unicode packet]: http://msdn.microsoft.com/en-us/library/cc240585.aspx
[synchronize packet]: http://msdn.microsoft.com/en-us/library/cc240588.aspx
[mouse packet]: http://msdn.microsoft.com/en-us/library/cc240586.aspx
[extended mouse packet]: http://msdn.microsoft.com/en-us/library/cc240587.aspx
[play sound]: http://msdn.microsoft.com/en-us/library/cc240633.aspx
[set system pointer]: http://msdn.microsoft.com/en-us/library/cc240617.aspx
[new pointer update]: http://msdn.microsoft.com/en-us/library/cc240619.aspx
[MessageBox]:https://msdn.microsoft.com/en-us/library/ms645505%28v=vs.85%29.aspx
[Google protocol buffers]:https://developers.google.com/protocol-buffers/
