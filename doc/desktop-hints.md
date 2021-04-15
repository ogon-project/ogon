# Tested Desktops
There are a few limitations concerning different desktop types when using ogon.
This file is about documenting the approved desktop types and maybe the changes required to make them work.

## Basics
Ogon is using session files (usually located at /usr/share/xsession/*.desktop) to start sessions.
The desktop type can be defined globally (by setting the environment variable OGON_X11_DESKTOP in ogon.ini)
```ini
environment_add_string=WTSAPI_LIBRARY:/usr/lib/x86_64-linux-gnu/ogon1/libogon-otsapi.so;OGON_X11_DESKTOP:xfce
```
or on a per user basis by creating a file:
```console
echo "xfce" > ~/.config/ogon/desktop
```

## LXDE desktop (LXDE.desktop, Tested on Ubuntu 20.04)
Install packages:
```console
apt install lxde # OGON_X11_DESKTOP=LXDE
```

## MATE desktop (mate.desktop, Tested on Ubuntu 20.04)
Install packages:
```console
apt install mate-desktop-environment # OGON_X11_DESKTOP=mate
```
Known issues:
* Panel app IndicatorAppletcomplete won't start (just disable it).

## XFCE desktop (xfce.desktop, Tested on Ubuntu 20.04)
Install packages:
```console
apt install xfce4 # OGON_X11_DESKTOP=xfce
```

## xubuntu desktop (xubuntu.desktop, Tested on Ubuntu 20.04)
Install packages:
```console
apt install xubuntu-desktop # OGON_X11_DESKTOP=xubuntu
```

## Ubuntu desktop (ubuntu.desktop, Tested on Ubuntu 20.04)
Install packages:
```console
apt install ubuntu-session # OGON_X11_DESKTOP=ubuntu
```
Running ubuntu desktop requires a little more work:
Make sure to set variables XDG_SESSION_TYPE and GDK_BACKEND to "x11":
```ini
environment_add_string=WTSAPI_LIBRARY:/usr/lib/x86_64-linux-gnu/ogon1/libogon-otsapi.so;OGON_X11_DESKTOP:ubuntu;XDG_SESSION_TYPE:x11;GDK_BACKEND:x11
```
Known issues:
* Some gnome applications may have a double title bar, similar to [this issue](https://github.com/neutrinolabs/xrdp/issues/1642).
