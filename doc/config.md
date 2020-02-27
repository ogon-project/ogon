# File Format

ogon uses a "ini" file based config file format.
Configuration "blocks" are indicated by [&lt;blockname&gt;]. For the ogon Session Manager
the valid blocks are "global" and "user_&lt;username&gt;". Other blocks can be used but are ignored
by the ogon Session Manager.

Lines staring with a `#` (hash) are treated as comments and are ignored.

Each line contains a property composed of a path, type and value in the format
&lt;path&gt;_&lt;type&gt;=&lt;value&gt;

Example:
```ini
session_yres_number=768
```

A path may contain multiple "scopes" separated with an `_` (underscore).

The following top level scopes are currently used:

* auth - related to authentication
* module
* permission
* session
* environment

When a path consists of multiple scopes, the later parts are related to the top level.
For example if the path `module_xsession_startwm` is used to specify the startwm script,
for a module defined as xsession.

The valid types are:

* string
* number
* bool: can be true or false

Properties need to be unique per block. If a duplicated property exists (even with different settings!) within a block the ini file is considered invalid and can't be parsed.

```ini
[global]
session_xres_number=1024
#invalid
session_xres_number=1024
```

# Properties

## ssl_certificate_string

is the filename with path to the certificate.

Default: /etc/ssl/certs/ogon.crt


## ssl_key_string

is the filename with path to the key file.

Default: /etc/ssl/private/ogon.key


## tcp_keepalive_params_string

is the parameters for TCP keepalive configuration used by the RDP listener. It's a string separated by a coma
containing the maximum idle time, followed by the maximum number of keep alive packets.
Negative values means that this parameter will not be set.

Default: -1,-1

## auth_module_string

Is the authentication module loaded and used for authentication. default: PAM

## auth_greeter_string

Specifies the name of a module config which gets started as greeter. standard: greeter
The used module needs to be defined and setup properly.

## session_reconnect_bool

If true, a disconnected Session can be reconnected, otherwise every new connection gets a new session

## session_reconnect_fromSameClient_bool
If true, a reconnect only from the same client (hostname) will be made, otherwise a new session is created.

## session_timeout_number
Time in minutes, after a disconnected session will be closed.
0 means it will be closed immediately.
-1 means it will never be closed.

## session_singleSession_bool

If true only 1 session per user is permitted.
Exception: If `session_reconnect_fromSameClient` is true, one session per user and host combination is permitted.

## environment_filter_string

Only specific environment settings will be used for a session.

For example:
```
TEST;PATH
```

Only the environment variable TEST and PATH will get into a user session.

## environment_add_string
Environment variables can be added to a user environment.

For example:
```
TEST:value;TEST1:value1
```

So env variable TEST will be value and TEST1 will be value1.
If a environment variable contains multiple `:`, like PATH for example, they should be set unaltered.
```ini
environment_add_string=PATH:/usr/local/bin:/usr/bin:/bin:/usr/local/games:/usr/games
```

## permission_level_string

Is the permission level for OTSAPI.
Possible values:

* FULL: has full access on the OTSAPI for all session.
* USER: has the permission to logon, connect and query_information
* GUEST: has the permission to logon

## session_maxXRes_number

Maximal width (x resolution) allowed for a session.

## session_maxYRes_number

The maximum height (y resolution) allowed for a session.


## session_xres_number

If nothing else is specified or set by a rdp connection this is the default width
a session is started with.

## session_yres_number

If nothing else is specified or set by a rdp connection this is the default height
a session is started with.

## session_colordepth_number

If nothing else is specified or set by a rdp connection this is the default height
a session is started with.

## session_remotecontrol_ask_bool

If set to true, the user is asked before a shadwing of his session is started, otherwise
the shadowing starts immediately (default behaviour).

# Module specific properties

Module configs starts with module followed by the module ConfigName.
The only required setting is the modulename. The modulename is the name of the module which gets started for this module config. For example "QT"

Minimal required structure: ```module_<ConfigName>_<ModuleName>_string```

## general

### module_string

Tells which module is started as user session. The Value has to be an existing ConfigName.
```ini
module_string=xsession
```
### ogon_forceWeakRdpKey_bool

If found and set to true it forces ogon to generate a temporary 512 bit rsa key in memory that will only be used for RDP security (required to support legacy rdp clients like rdesktop 1.4.x).

## permission_groups_whiteList_string

List of groups which are allowed to log in.
If * is present in the whiteList then all groups are allowed to log in as long as they are not
blacklisted.

Default: * - allows all groups to log in

## permission_groups_blackList_string

List of groups which are NOT allowed to log in.
If * is present in the blacklist all groups that are not white listed are forbidden to log in.

Default: '' - no blacklisted groups

### ogon_showDebugInfo_bool

If found and set to true some debug information is embedded directly in the top of the frame buffer.

### ogon_disableGraphicsPipeline_bool

If found and set to true the RDP Graphics pipeline extension is disabled.

### ogon_disableGraphicsPipelineH264_bool

If found and set to true the RDP Graphics pipeline will not use the H.264 codec, even if supported by the client.  
Note: OpenH264 Video Codec provided by Cisco Systems, Inc.

### ogon_enableFullAVC444_bool

If found and set to true a AVC444-capable client will receive an additional chroma frame for each single
standard H.264 frame. This mode reduces blur effects and improves the readability for certain text colors.
Note: This setting should only be enabled for highspeed LAN connections.

### ogon_restrictAVC444_bool

Restrict the usage of AVC444 to clients that announce the RDPGFX_CAPS_FLAG_AVC_THINCLIENT flag.

Default: false

### ogon_bitrate_number

Is the bitrate which should be used (only applies to H.264 for now).
If 0 (default) the bitrate will be dynamically managed by the bandwidth management.

For example:
0 for auto
4000000 for 4 Mb


## Module X11

module_xsession_modulename_string=X11

Defines which module is used when referencing a module with xsession. Default: X11.

### module_xsession_startwm_string

Window manager command/script to be launched. If this command/script stops the session is stopped.
If a script is used the last command is usually to exec (like to start the window manager)
that keeps running. The default is to use the script ogonXsession which tries
to launch the xsession with one of the system's Xsession scripts.

To select a desktop session the variable OGON_X11_DESKTOP (for example set with environment_add_string=OGON_X11_DESKTOP:fvwm for a user) or the file ${HOME}/.config/ogon/desktop can be used. If both are set the variable takes
precedence and the file is ignored. Both the variable and the file must contain
the name of a valid session from */usr/share/xsession* without the .desktop extension.

For example if /usr/share/xsession contains a session file with the name
awesome.desktop and ${HOME}/.config/ogon/desktop contained *awesome* the
corresponding session would get started for a user.

Unless the variable OGON_X11_NODBUS is spezified in the environment the default scripts
starts a session dbus and exports the corresponding variables.

### module_xsession_stopwm_string

A script to be launched if a xsession gets stopped. This happens before the started
X11 processes get stopped/killed.


### module_xsession_startvc_string

Start script for all virtual channels which should be launched for xsession.

### module_xsession_displayOffset_number

Specifies the first display to use. Default is 10.

### module_xsession_dpi_number

Specifies the dpi used in starting up the xserver. Default is 96.

### module_xsession_noKPCursor_bool

If set to true the keypad cursor keys are disabled and mapped to the regular cursor keys.

### module_xsession_fontPath_string

Fontpath to start the xserver with.

### module_xsession_uselauncher_bool

If true the ogon-process-launcher is used to start modules.

### module_xsession_launcherdebugfile_string

If specified the launcher will log into this file, otherwise no debug output will be generated.

## Module Greeter

module_greeter_modulename_string=Qt

### module_greeter_cmd_string

The command line the greeter is started with:

For example:
```ini
module_greeter_cmd_string=ogon-qt-greeter --noeffects
```



# Example(s):
```ini
auth_module_string = PAM
auth_greeter_string = stdgreeter
module_string = xsession
module_stdgreeter_modulename_string = Qt
module_stdgreeter_cmd_string = ogon-qt-greeter
```
