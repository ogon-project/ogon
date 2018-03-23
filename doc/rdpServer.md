ogon RDP Server
==================

# Introduction

This document describes the current c ogon RDP Server

# Purpose

The ogon RDP Server handles all rdp specific stuff. It encodes the picture and all virtual channels into the rdp
stream. The FreeRDP library is used to do most of the rdp specific

# Architecture

## Connectivity

The ogon Session Manager and the ogon RDP Server communicate using the Internal Communication Protocol (ICP).
The ICP uses as base the Protocol Buffer RPC (PBRCP) which uses a pipe as transport.

The ogon RDP Server implements a rpc-server, which runs his own thread. Once a rpc request is received the request
gets decoded and translated to an internal message, which is posted to the corresponding rdp-client thread. The request
is handled and the answer is written back to the ogon session-mananger.

## Connection Handling

Each connection is handled by a separate thread.

## Channels

The channels are forwarded to the OTSAPI client by a pipe. Each received message is put in the pipe to the client and
all data read from the pipe is sent to the rdp client.

The only exception from the rule is the Graphics Pipeline Extension (so-called GFX) channel. 
This channel is used if the client supports it and is executed as thread in the ogon RDP Server. 

## Encoders

### Bitmap Encoder

Is found in the `encoder.h` and `encoder.c`  file.

### RFX Encoder

Supported are:
* Legacy RemoteFX over surface commands
* Legacy RemoteFX over the Graphics Pipeline Extension
* Progressive RemoteFX over the Graphics Pipeline Extension

### OpenH264

It is supported if the openh264 library is available on the installed system.
If it is not already installed, a download script (/sbin/ogon-get-openh264-codec) is 
shipped to help download the openh264 library.

### NSCodec

NSCodec is not supported.

## Bandwidth Management

The ogon RDP Server supports, under certain circumstances, bandwidth adjustments depending on the bandwidth available.
 To achieve this, the h264 codec is has to be used and the client needs to supports the necessary rdp commands.
 So either freerdp or a Microsoft rdp client (mstsc) which supports at least RDP protocol 8.0 has to be used, otherwise
 the dynamic compression adjustment cannot be made.

## Debug overlay.

 It is possible to enable a debug overlay to get information about the current connection, like encoder type, datarate
 and so on. Please read the [config document](config.md) about how to enable this overlay.
