# Tested Desktops
There are a few limitations concerning different desktop types when using ogon.
This file is about documenting the approved desktop types and maybe the changes required to make them work.

## Basics
Ogon is using session files (usually located at /usr/share/xsession/*.desktop) to start sessions.
The desktop type can be defined globally (by editing ogon.ini)
```ini
environment_add_string=WTSAPI_LIBRARY:/usr/lib/x86_64-linux-gnu/ogon1/libogon-otsapi.so;OGON_X11_DESKTOP:xfce
```
or on a per user basis by creating a file:
```console
echo "xfce" > ~/.config/ogon/desktop
```

## XFCE desktop (xfce.desktop, Tested on Ubuntu 20.04)
Install packages:
```console
apt install xfce4
```

## Ubuntu desktop (ubuntu.desktop, Tested on Ubuntu 20.04)
Install packages:
```console
apt install ubuntu-session
```
Running ubuntu desktop requires a little more work:
Make sure to set variables XDG_SESSION_TYPE and GDK_BACKEND to "x11":
```ini
environment_add_string=WTSAPI_LIBRARY:/usr/lib/x86_64-linux-gnu/ogon1/libogon-otsapi.so;OGON_X11_DESKTOP:ubuntu;XDG_SESSION_TYPE:x11;GDK_BACKEND:x11
```
Issues:
Some gnome applications may have a double title bar, similar to [this issue!](https://github.com/neutrinolabs/xrdp/issues/1642).
