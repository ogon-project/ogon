Implementing a ogon backend
=============

# Introduction

This document gives instructions and tips to write a backend for ogon. We don't give
full protocol structure here, refer to the corresponding document for a verbose documentation.


# Messages and frame generation

For the different messages and the message protocol please read the [backendProtocol](backendProtocol.md) document.


## Messages workflow

Once ogon has connected on the unix socket or named pipe of the backend, it sends a first
message to obtain the version of the backend. The backend MUST answer with
a _VersionInfo_ message. Major version numbers should match or ogon will
close the connection

ogon will then send a _capabilities_ message containing the screen resolution, keyboard
informations, and the connection id of the main seat. The backend MUST answer this message
with a _framebufferInfo_ message, this allows ogon to know the format and size that will 
be used for the shared framebuffer between ogon and the backend.

## Frame generation workflow

When ogon is ready for generating a frame it sends a synchronize request to the backend. The
backend should fill the shared memory with the changes that have occurred locally:
the damaged rectangles + updating the framebuffer. When this is done, it sends a synchronize
reply to notify ogon that the shared memory has been updated and can be safely read. 

If the backend has no local changes it can delay the shared memory update and synchronize reply until an update
has occurred.

The backend should not update the shared memory until it has received a sync request. ogon will not try
to read or update the shared memory until it has received a sync reply.


## Multiseat and inputs treatment

ogon will send messages for input signals: synchronize, keyboard and mouse. Each message contains
the information about the input event and the id of the front connection that this message comes from. 
Backends that don't handle multi-seat can just go and ignore that field. Multi-seat aware backends
can treat this field to maintain the state of each of the seat. ogon will announce the arrival 
and removal of front seat with appropriate messages. 


## The resize case

A resize can occur initiated by either the backend itself (playing with xrandr under
Xrds for example) or instructed by the remote RDP peer (on reconnection mostly).

A common case of resize is when the backend supports reconnection, and the front peer comes
with a new resolution. In this case, at connection the backend will receive a _capabilities_
message with a resolution that is different from the current one. The backend MUST adjust to this
new resolution. 

When the backend wants to initiate a resize, it sends a _framebufferInfo_ message with
the new resolution (and stride, etc...) set. The backend must remember the id of the previous
shmId, and it should ignore _syncRequests_ for this shm id. When ogon has handled the
resolution change client side (because changing resolution implies doing a reactivation 
sequence), it will use another shmId. This is guarantee to prevent a race between the 
backend requesting resize, and ogon asking for a sync request for the previous resolution.  

