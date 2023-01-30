/**
 * ogon - Free Remote Desktop Services
 * Backend Library
 * RDP scancode conversions
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * Norbert Federa <norbert.federa@thincast.com>
 *
 * This file may be used under the terms of the GNU Affero General
 * Public License version 3 as published by the Free Software Foundation
 * and appearing in the file LICENSE-AGPL included in the distribution
 * of this file.
 *
 * Under the GNU Affero General Public License version 3 section 7 the
 * copyright holders grant the additional permissions set forth in the
 * ogon Core AGPL Exceptions version 1 as published by
 * Thincast Technologies GmbH.
 *
 * Under the GNU Affero General Public License version 3 section 7 the
 * copyright holders grant the additional permissions set forth in the
 * ogon Library AGPL Exceptions version 1 as published by
 * Thincast Technologies GmbH.
 *
 * For more information see the file LICENSE in the distribution of this file.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../common/global.h"
#include <ogon/backend.h>

#define EVDEV_KEY_RESERVED 0
#define EVDEV_KEY_ESC 1
#define EVDEV_KEY_1 2
#define EVDEV_KEY_2 3
#define EVDEV_KEY_3 4
#define EVDEV_KEY_4 5
#define EVDEV_KEY_5 6
#define EVDEV_KEY_6 7
#define EVDEV_KEY_7 8
#define EVDEV_KEY_8 9
#define EVDEV_KEY_9 10
#define EVDEV_KEY_0 11
#define EVDEV_KEY_MINUS 12
#define EVDEV_KEY_EQUAL 13
#define EVDEV_KEY_BACKSPACE 14
#define EVDEV_KEY_TAB 15
#define EVDEV_KEY_Q 16
#define EVDEV_KEY_W 17
#define EVDEV_KEY_E 18
#define EVDEV_KEY_R 19
#define EVDEV_KEY_T 20
#define EVDEV_KEY_Y 21
#define EVDEV_KEY_U 22
#define EVDEV_KEY_I 23
#define EVDEV_KEY_O 24
#define EVDEV_KEY_P 25
#define EVDEV_KEY_LEFTBRACE 26
#define EVDEV_KEY_RIGHTBRACE 27
#define EVDEV_KEY_ENTER 28
#define EVDEV_KEY_LEFTCTRL 29
#define EVDEV_KEY_A 30
#define EVDEV_KEY_S 31
#define EVDEV_KEY_D 32
#define EVDEV_KEY_F 33
#define EVDEV_KEY_G 34
#define EVDEV_KEY_H 35
#define EVDEV_KEY_J 36
#define EVDEV_KEY_K 37
#define EVDEV_KEY_L 38
#define EVDEV_KEY_SEMICOLON 39
#define EVDEV_KEY_APOSTROPHE 40
#define EVDEV_KEY_GRAVE 41
#define EVDEV_KEY_LEFTSHIFT 42
#define EVDEV_KEY_BACKSLASH 43
#define EVDEV_KEY_Z 44
#define EVDEV_KEY_X 45
#define EVDEV_KEY_C 46
#define EVDEV_KEY_V 47
#define EVDEV_KEY_B 48
#define EVDEV_KEY_N 49
#define EVDEV_KEY_M 50
#define EVDEV_KEY_COMMA 51
#define EVDEV_KEY_DOT 52
#define EVDEV_KEY_SLASH 53
#define EVDEV_KEY_RIGHTSHIFT 54
#define EVDEV_KEY_KPASTERISK 55
#define EVDEV_KEY_LEFTALT 56
#define EVDEV_KEY_SPACE 57
#define EVDEV_KEY_CAPSLOCK 58
#define EVDEV_KEY_F1 59
#define EVDEV_KEY_F2 60
#define EVDEV_KEY_F3 61
#define EVDEV_KEY_F4 62
#define EVDEV_KEY_F5 63
#define EVDEV_KEY_F6 64
#define EVDEV_KEY_F7 65
#define EVDEV_KEY_F8 66
#define EVDEV_KEY_F9 67
#define EVDEV_KEY_F10 68
#define EVDEV_KEY_NUMLOCK 69
#define EVDEV_KEY_SCROLLLOCK 70
#define EVDEV_KEY_KP7 71
#define EVDEV_KEY_KP8 72
#define EVDEV_KEY_KP9 73
#define EVDEV_KEY_KPMINUS 74
#define EVDEV_KEY_KP4 75
#define EVDEV_KEY_KP5 76
#define EVDEV_KEY_KP6 77
#define EVDEV_KEY_KPPLUS 78
#define EVDEV_KEY_KP1 79
#define EVDEV_KEY_KP2 80
#define EVDEV_KEY_KP3 81
#define EVDEV_KEY_KP0 82
#define EVDEV_KEY_KPDOT 83
#define EVDEV_KEY_84 84
#define EVDEV_KEY_ZENKAKUHANKAKU 85
#define EVDEV_KEY_102ND 86
#define EVDEV_KEY_F11 87
#define EVDEV_KEY_F12 88
#define EVDEV_KEY_RO 89
#define EVDEV_KEY_KATAKANA 90
#define EVDEV_KEY_HIRAGANA 91
#define EVDEV_KEY_HENKAN 92
#define EVDEV_KEY_KATAKANAHIRAGANA 93
#define EVDEV_KEY_MUHENKAN 94
#define EVDEV_KEY_KPJPCOMMA 95
#define EVDEV_KEY_KPENTER 96
#define EVDEV_KEY_RIGHTCTRL 97
#define EVDEV_KEY_KPSLASH 98
#define EVDEV_KEY_SYSRQ 99
#define EVDEV_KEY_RIGHTALT 100
#define EVDEV_KEY_LINEFEED 101
#define EVDEV_KEY_HOME 102
#define EVDEV_KEY_UP 103
#define EVDEV_KEY_PAGEUP 104
#define EVDEV_KEY_LEFT 105
#define EVDEV_KEY_RIGHT 106
#define EVDEV_KEY_END 107
#define EVDEV_KEY_DOWN 108
#define EVDEV_KEY_PAGEDOWN 109
#define EVDEV_KEY_INSERT 110
#define EVDEV_KEY_DELETE 111
#define EVDEV_KEY_MACRO 112
#define EVDEV_KEY_MUTE 113
#define EVDEV_KEY_VOLUMEDOWN 114
#define EVDEV_KEY_VOLUMEUP 115
#define EVDEV_KEY_POWER 116
#define EVDEV_KEY_KPEQUAL 117
#define EVDEV_KEY_KPPLUSMINUS 118
#define EVDEV_KEY_PAUSE 119
#define EVDEV_KEY_SCALE 120
#define EVDEV_KEY_KPCOMMA 121
#define EVDEV_KEY_HANGEUL 122
#define EVDEV_KEY_HANGUEL EVDEV_KEY_HANGEUL
#define EVDEV_KEY_HANJA 123
#define EVDEV_KEY_YEN 124
#define EVDEV_KEY_LEFTMETA 125
#define EVDEV_KEY_RIGHTMETA 126
#define EVDEV_KEY_COMPOSE 127
#define EVDEV_KEY_STOP 128
#define EVDEV_KEY_AGAIN 129
#define EVDEV_KEY_PROPS 130
#define EVDEV_KEY_UNDO 131
#define EVDEV_KEY_FRONT 132
#define EVDEV_KEY_COPY 133
#define EVDEV_KEY_OPEN 134
#define EVDEV_KEY_PASTE 135
#define EVDEV_KEY_FIND 136
#define EVDEV_KEY_CUT 137
#define EVDEV_KEY_HELP 138
#define EVDEV_KEY_MENU 139
#define EVDEV_KEY_CALC 140
#define EVDEV_KEY_SETUP 141
#define EVDEV_KEY_SLEEP 142
#define EVDEV_KEY_WAKEUP 143
#define EVDEV_KEY_FILE 144
#define EVDEV_KEY_SENDFILE 145
#define EVDEV_KEY_DELETEFILE 146
#define EVDEV_KEY_XFER 147
#define EVDEV_KEY_PROG1 148
#define EVDEV_KEY_PROG2 149
#define EVDEV_KEY_WWW 150
#define EVDEV_KEY_MSDOS 151
#define EVDEV_KEY_COFFEE 152
#define EVDEV_KEY_SCREENLOCK EVDEV_KEY_COFFEE
#define EVDEV_KEY_ROTATE_DISPLAY 153
#define EVDEV_KEY_DIRECTION EVDEV_KEY_ROTATE_DISPLAY
#define EVDEV_KEY_CYCLEWINDOWS 154
#define EVDEV_KEY_MAIL 155
#define EVDEV_KEY_BOOKMARKS 156
#define EVDEV_KEY_COMPUTER 157
#define EVDEV_KEY_BACK 158
#define EVDEV_KEY_FORWARD 159
#define EVDEV_KEY_CLOSECD 160
#define EVDEV_KEY_EJECTCD 161
#define EVDEV_KEY_EJECTCLOSECD 162
#define EVDEV_KEY_NEXTSONG 163
#define EVDEV_KEY_PLAYPAUSE 164
#define EVDEV_KEY_PREVIOUSSONG 165
#define EVDEV_KEY_STOPCD 166
#define EVDEV_KEY_RECORD 167
#define EVDEV_KEY_REWIND 168
#define EVDEV_KEY_PHONE 169
#define EVDEV_KEY_ISO 170
#define EVDEV_KEY_CONFIG 171
#define EVDEV_KEY_HOMEPAGE 172
#define EVDEV_KEY_REFRESH 173
#define EVDEV_KEY_EXIT 174
#define EVDEV_KEY_MOVE 175
#define EVDEV_KEY_EDIT 176
#define EVDEV_KEY_SCROLLUP 177
#define EVDEV_KEY_SCROLLDOWN 178
#define EVDEV_KEY_KPLEFTPAREN 179
#define EVDEV_KEY_KPRIGHTPAREN 180
#define EVDEV_KEY_NEW 181
#define EVDEV_KEY_REDO 182
#define EVDEV_KEY_F13 183
#define EVDEV_KEY_F14 184
#define EVDEV_KEY_F15 185
#define EVDEV_KEY_F16 186
#define EVDEV_KEY_F17 187
#define EVDEV_KEY_F18 188
#define EVDEV_KEY_F19 189
#define EVDEV_KEY_F20 190
#define EVDEV_KEY_F21 191
#define EVDEV_KEY_F22 192
#define EVDEV_KEY_F23 193
#define EVDEV_KEY_F24 194
#define EVDEV_KEY_195 195
#define EVDEV_KEY_196 196
#define EVDEV_KEY_197 197
#define EVDEV_KEY_198 198
#define EVDEV_KEY_199 199
#define EVDEV_KEY_PLAYCD 200
#define EVDEV_KEY_PAUSECD 201
#define EVDEV_KEY_PROG3 202
#define EVDEV_KEY_PROG4 203
#define EVDEV_KEY_DASHBOARD 204
#define EVDEV_KEY_SUSPEND 205
#define EVDEV_KEY_CLOSE 206
#define EVDEV_KEY_PLAY 207
#define EVDEV_KEY_FASTFORWARD 208
#define EVDEV_KEY_BASSBOOST 209
#define EVDEV_KEY_PRINT 210
#define EVDEV_KEY_HP 211
#define EVDEV_KEY_CAMERA 212
#define EVDEV_KEY_SOUND 213
#define EVDEV_KEY_QUESTION 214
#define EVDEV_KEY_EMAIL 215
#define EVDEV_KEY_CHAT 216
#define EVDEV_KEY_SEARCH 217
#define EVDEV_KEY_CONNECT 218
#define EVDEV_KEY_FINANCE 219
#define EVDEV_KEY_SPORT 220
#define EVDEV_KEY_SHOP 221
#define EVDEV_KEY_ALTERASE 222
#define EVDEV_KEY_CANCEL 223
#define EVDEV_KEY_BRIGHTNESSDOWN 224
#define EVDEV_KEY_BRIGHTNESSUP 225
#define EVDEV_KEY_MEDIA 226
#define EVDEV_KEY_SWITCHVIDEOMODE 227
#define EVDEV_KEY_KBDILLUMTOGGLE 228
#define EVDEV_KEY_KBDILLUMDOWN 229
#define EVDEV_KEY_KBDILLUMUP 230
#define EVDEV_KEY_SEND 231
#define EVDEV_KEY_REPLY 232
#define EVDEV_KEY_FORWARDMAIL 233
#define EVDEV_KEY_SAVE 234
#define EVDEV_KEY_DOCUMENTS 235
#define EVDEV_KEY_BATTERY 236
#define EVDEV_KEY_BLUETOOTH 237
#define EVDEV_KEY_WLAN 238
#define EVDEV_KEY_UWB 239
#define EVDEV_KEY_UNKNOWN 240
#define EVDEV_KEY_VIDEO_NEXT 241
#define EVDEV_KEY_VIDEO_PREV 242
#define EVDEV_KEY_BRIGHTNESS_CYCLE 243
#define EVDEV_KEY_BRIGHTNESS_AUTO 244
#define EVDEV_KEY_BRIGHTNESS_ZERO EVDEV_KEY_BRIGHTNESS_AUTO
#define EVDEV_KEY_DISPLAY_OFF 245
#define EVDEV_KEY_WWAN 246
#define EVDEV_KEY_WIMAX EVDEV_KEY_WWAN
#define EVDEV_KEY_RFKILL 247
#define EVDEV_KEY_MICMUTE 248

#define EVDEV_KEY_MAX EVDEV_KEY_MICMUTE

/**
 * See USB HID to PS/2 Scan Code Translation Table:
 * http://download.microsoft.com/download/1/6/1/161ba512-40e2-4cc9-843a-923143f3456c/translate.pdf
 */

static DWORD RDP_SCANCODE_TO_EVDEV_CODE[256][2] = {
		/* SCANCODE                     EXTENDED                            */
		{EVDEV_KEY_RESERVED, 0},					/* 0x00 */
		{EVDEV_KEY_ESC, 0},							/* 0x01 */
		{EVDEV_KEY_1, 0},							/* 0x02 */
		{EVDEV_KEY_2, 0},							/* 0x03 */
		{EVDEV_KEY_3, 0},							/* 0x04 */
		{EVDEV_KEY_4, 0},							/* 0x05 */
		{EVDEV_KEY_5, 0},							/* 0x06 */
		{EVDEV_KEY_6, 0},							/* 0x07 */
		{EVDEV_KEY_7, 0},							/* 0x08 */
		{EVDEV_KEY_8, 0},							/* 0x09 */
		{EVDEV_KEY_9, 0},							/* 0x0A */
		{EVDEV_KEY_0, 0},							/* 0x0B */
		{EVDEV_KEY_MINUS, 0},						/* 0x0C */
		{EVDEV_KEY_EQUAL, 0},						/* 0x0D */
		{EVDEV_KEY_BACKSPACE, 0},					/* 0x0E */
		{EVDEV_KEY_TAB, 0},							/* 0x0F */
		{EVDEV_KEY_Q, EVDEV_KEY_PREVIOUSSONG},		/* 0x10 */
		{EVDEV_KEY_W, 0},							/* 0x11 */
		{EVDEV_KEY_E, 0},							/* 0x12 */
		{EVDEV_KEY_R, 0},							/* 0x13 */
		{EVDEV_KEY_T, 0},							/* 0x14 */
		{EVDEV_KEY_Y, 0},							/* 0x15 */
		{EVDEV_KEY_U, 0},							/* 0x16 */
		{EVDEV_KEY_I, 0},							/* 0x17 */
		{EVDEV_KEY_O, 0},							/* 0x18 */
		{EVDEV_KEY_P, EVDEV_KEY_NEXTSONG},			/* 0x19 */
		{EVDEV_KEY_LEFTBRACE, 0},					/* 0x1A */
		{EVDEV_KEY_RIGHTBRACE, 0},					/* 0x1B */
		{EVDEV_KEY_ENTER, EVDEV_KEY_KPENTER},		/* 0x1C */
		{EVDEV_KEY_LEFTCTRL, EVDEV_KEY_RIGHTCTRL},	/* 0x1D */
		{EVDEV_KEY_A, 0},							/* 0x1E */
		{EVDEV_KEY_S, 0},							/* 0x1F */
		{EVDEV_KEY_D, EVDEV_KEY_MUTE},				/* 0x20 */
		{EVDEV_KEY_F, EVDEV_KEY_CALC},				/* 0x21 */
		{EVDEV_KEY_G, EVDEV_KEY_PLAYPAUSE},			/* 0x22 */
		{EVDEV_KEY_H, 0},							/* 0x23 */
		{EVDEV_KEY_J, EVDEV_KEY_STOPCD},			/* 0x24 */
		{EVDEV_KEY_K, 0},							/* 0x25 */
		{EVDEV_KEY_L, 0},							/* 0x26 */
		{EVDEV_KEY_SEMICOLON, 0},					/* 0x27 */
		{EVDEV_KEY_APOSTROPHE, 0},					/* 0x28 */
		{EVDEV_KEY_GRAVE, 0},						/* 0x29 */
		{EVDEV_KEY_LEFTSHIFT, 0},					/* 0x2A */
		{EVDEV_KEY_BACKSLASH, 0},					/* 0x2B */
		{EVDEV_KEY_Z, 0},							/* 0x2C */
		{EVDEV_KEY_X, 0},							/* 0x2D */
		{EVDEV_KEY_C, EVDEV_KEY_VOLUMEDOWN},		/* 0x2E */
		{EVDEV_KEY_V, 0},							/* 0x2F */
		{EVDEV_KEY_B, EVDEV_KEY_VOLUMEUP},			/* 0x30 */
		{EVDEV_KEY_N, 0},							/* 0x31 */
		{EVDEV_KEY_M, EVDEV_KEY_HOMEPAGE},			/* 0x32 */
		{EVDEV_KEY_COMMA, 0},						/* 0x33 */
		{EVDEV_KEY_DOT, 0},							/* 0x34 */
		{EVDEV_KEY_SLASH, EVDEV_KEY_KPSLASH},		/* 0x35 */
		{EVDEV_KEY_RIGHTSHIFT, 0},					/* 0x36 */
		{EVDEV_KEY_KPASTERISK, EVDEV_KEY_SYSRQ},	/* 0x37 */
		{EVDEV_KEY_LEFTALT, EVDEV_KEY_RIGHTALT},	/* 0x38 */
		{EVDEV_KEY_SPACE, 0},						/* 0x39 */
		{EVDEV_KEY_CAPSLOCK, 0},					/* 0x3A */
		{EVDEV_KEY_F1, 0},							/* 0x3B */
		{EVDEV_KEY_F2, 0},							/* 0x3C */
		{EVDEV_KEY_F3, 0},							/* 0x3D */
		{EVDEV_KEY_F4, 0},							/* 0x3E */
		{EVDEV_KEY_F5, 0},							/* 0x3F */
		{EVDEV_KEY_F6, 0},							/* 0x40 */
		{EVDEV_KEY_F7, 0},							/* 0x41 */
		{EVDEV_KEY_F8, 0},							/* 0x42 */
		{EVDEV_KEY_F9, 0},							/* 0x43 */
		{EVDEV_KEY_F10, 0},							/* 0x44 */
		{EVDEV_KEY_NUMLOCK, 0},						/* 0x45 */
		{EVDEV_KEY_SCROLLLOCK, 0},					/* 0x46 */
		{EVDEV_KEY_KP7, EVDEV_KEY_HOME},			/* 0x47 */
		{EVDEV_KEY_KP8, EVDEV_KEY_UP},				/* 0x48 */
		{EVDEV_KEY_KP9, EVDEV_KEY_PAGEUP},			/* 0x49 */
		{EVDEV_KEY_KPMINUS, 0},						/* 0x4A */
		{EVDEV_KEY_KP4, EVDEV_KEY_LEFT},			/* 0x4B */
		{EVDEV_KEY_KP5, 0},							/* 0x4C */
		{EVDEV_KEY_KP6, EVDEV_KEY_RIGHT},			/* 0x4D */
		{EVDEV_KEY_KPPLUS, 0},						/* 0x4E */
		{EVDEV_KEY_KP1, EVDEV_KEY_END},				/* 0x4F */
		{EVDEV_KEY_KP2, EVDEV_KEY_DOWN},			/* 0x50 */
		{EVDEV_KEY_KP3, EVDEV_KEY_PAGEDOWN},		/* 0x51 */
		{EVDEV_KEY_KP0, EVDEV_KEY_INSERT},			/* 0x52 */
		{EVDEV_KEY_KPDOT, EVDEV_KEY_DELETE},		/* 0x53 */
		{0, 0},										/* 0x54 */
		{0, 0},										/* 0x55 */
		{EVDEV_KEY_102ND, 0},						/* 0x56 */
		{EVDEV_KEY_F11, 0},							/* 0x57 */
		{EVDEV_KEY_F12, 0},							/* 0x58 */
		{EVDEV_KEY_KPEQUAL, 0},						/* 0x59 */
		{0, 0},										/* 0x5A */
		{0, EVDEV_KEY_LEFTMETA},					/* 0x5B */
		{EVDEV_KEY_KPJPCOMMA, EVDEV_KEY_RIGHTMETA}, /* 0x5C */
		{0, EVDEV_KEY_COMPOSE},						/* 0x5D */
		{0, EVDEV_KEY_POWER},						/* 0x5E */
		{0, EVDEV_KEY_SLEEP},						/* 0x5F */
		{0, 0},										/* 0x60 */
		{0, 0},										/* 0x61 */
		{0, 0},										/* 0x62 */
		{0, EVDEV_KEY_WAKEUP},						/* 0x63 */
		{EVDEV_KEY_F13, 0},							/* 0x64 */
		{EVDEV_KEY_F14, EVDEV_KEY_SEARCH},			/* 0x65 */
		{EVDEV_KEY_F15, EVDEV_KEY_BOOKMARKS},		/* 0x66 */
		{EVDEV_KEY_F16, EVDEV_KEY_REFRESH},			/* 0x67 */
		{EVDEV_KEY_F17, EVDEV_KEY_EXIT},			/* 0x68 */
		{EVDEV_KEY_F18, EVDEV_KEY_FORWARD},			/* 0x69 */
		{EVDEV_KEY_F19, EVDEV_KEY_BACK},			/* 0x6A */
		{EVDEV_KEY_F20, EVDEV_KEY_COMPUTER},		/* 0x6B */
		{EVDEV_KEY_F21, EVDEV_KEY_MAIL},			/* 0x6C */
		{EVDEV_KEY_F22, EVDEV_KEY_CONFIG},			/* 0x6D */
		{EVDEV_KEY_F23, 0},							/* 0x6E */
		{0, 0},										/* 0x6F */
		{EVDEV_KEY_KATAKANAHIRAGANA, 0},			/* 0x70 */
		{0, 0},										/* 0x71 */
		{0, 0},										/* 0x72 */
		{EVDEV_KEY_RO, 0},							/* 0x73 */
		{0, 0},										/* 0x74 */
		{0, 0},										/* 0x75 */
		{EVDEV_KEY_ZENKAKUHANKAKU, 0},				/* 0x76 */
		{EVDEV_KEY_HIRAGANA, 0},					/* 0x77 */
		{EVDEV_KEY_KATAKANA, 0},					/* 0x78 */
		{EVDEV_KEY_HENKAN, 0},						/* 0x79 */
		{0, 0},										/* 0x7A */
		{EVDEV_KEY_MUHENKAN, 0},					/* 0x7B */
		{0, 0},										/* 0x7C */
		{EVDEV_KEY_YEN, 0},							/* 0x7D */
		{0, 0},										/* 0x7E */
		{0, 0},										/* 0x7F */
		{0, 0},										/* 0x80 */
		{0, 0},										/* 0x81 */
		{0, 0},										/* 0x82 */
		{0, 0},										/* 0x83 */
		{0, 0},										/* 0x84 */
		{0, 0},										/* 0x85 */
		{0, 0},										/* 0x86 */
		{0, 0},										/* 0x87 */
		{0, 0},										/* 0x88 */
		{0, 0},										/* 0x89 */
		{0, 0},										/* 0x8A */
		{0, 0},										/* 0x8B */
		{0, 0},										/* 0x8C */
		{0, 0},										/* 0x8D */
		{0, 0},										/* 0x8E */
		{0, 0},										/* 0x8F */
		{0, 0},										/* 0x90 */
		{0, 0},										/* 0x91 */
		{0, 0},										/* 0x92 */
		{0, 0},										/* 0x93 */
		{0, 0},										/* 0x94 */
		{0, 0},										/* 0x95 */
		{0, 0},										/* 0x96 */
		{0, 0},										/* 0x97 */
		{0, 0},										/* 0x98 */
		{0, 0},										/* 0x99 */
		{0, 0},										/* 0x9A */
		{0, 0},										/* 0x9B */
		{0, 0},										/* 0x9C */
		{0, 0},										/* 0x9D */
		{0, 0},										/* 0x9E */
		{0, 0},										/* 0x9F */
		{0, 0},										/* 0xA0 */
		{0, 0},										/* 0xA1 */
		{0, 0},										/* 0xA2 */
		{0, 0},										/* 0xA3 */
		{0, 0},										/* 0xA4 */
		{0, 0},										/* 0xA5 */
		{0, 0},										/* 0xA6 */
		{0, 0},										/* 0xA7 */
		{0, 0},										/* 0xA8 */
		{0, 0},										/* 0xA9 */
		{0, 0},										/* 0xAA */
		{0, 0},										/* 0xAB */
		{0, 0},										/* 0xAC */
		{0, 0},										/* 0xAD */
		{0, 0},										/* 0xAE */
		{0, 0},										/* 0xAF */
		{0, 0},										/* 0xB0 */
		{0, 0},										/* 0xB1 */
		{0, 0},										/* 0xB2 */
		{0, 0},										/* 0xB3 */
		{0, 0},										/* 0xB4 */
		{0, 0},										/* 0xB5 */
		{0, 0},										/* 0xB6 */
		{0, 0},										/* 0xB7 */
		{0, 0},										/* 0xB8 */
		{0, 0},										/* 0xB9 */
		{0, 0},										/* 0xBA */
		{0, 0},										/* 0xBB */
		{0, 0},										/* 0xBC */
		{0, 0},										/* 0xBD */
		{0, 0},										/* 0xBE */
		{0, 0},										/* 0xBF */
		{0, 0},										/* 0xC0 */
		{0, 0},										/* 0xC1 */
		{0, 0},										/* 0xC2 */
		{0, 0},										/* 0xC3 */
		{0, 0},										/* 0xC4 */
		{0, 0},										/* 0xC5 */
		{0, 0},										/* 0xC6 */
		{0, 0},										/* 0xC7 */
		{0, 0},										/* 0xC8 */
		{0, 0},										/* 0xC9 */
		{0, 0},										/* 0xCA */
		{0, 0},										/* 0xCB */
		{0, 0},										/* 0xCC */
		{0, 0},										/* 0xCD */
		{0, 0},										/* 0xCE */
		{0, 0},										/* 0xCF */
		{0, 0},										/* 0xD0 */
		{0, 0},										/* 0xD1 */
		{0, 0},										/* 0xD2 */
		{0, 0},										/* 0xD3 */
		{0, 0},										/* 0xD4 */
		{0, 0},										/* 0xD5 */
		{0, 0},										/* 0xD6 */
		{0, 0},										/* 0xD7 */
		{0, 0},										/* 0xD8 */
		{0, 0},										/* 0xD9 */
		{0, 0},										/* 0xDA */
		{0, 0},										/* 0xDB */
		{0, 0},										/* 0xDC */
		{0, 0},										/* 0xDD */
		{0, 0},										/* 0xDE */
		{0, 0},										/* 0xDF */
		{0, 0},										/* 0xE0 */
		{0, 0},										/* 0xE1 */
		{0, 0},										/* 0xE2 */
		{0, 0},										/* 0xE3 */
		{0, 0},										/* 0xE4 */
		{0, 0},										/* 0xE5 */
		{0, 0},										/* 0xE6 */
		{0, 0},										/* 0xE7 */
		{0, 0},										/* 0xE8 */
		{0, 0},										/* 0xE9 */
		{0, 0},										/* 0xEA */
		{0, 0},										/* 0xEB */
		{0, 0},										/* 0xEC */
		{0, 0},										/* 0xED */
		{0, 0},										/* 0xEE */
		{0, 0},										/* 0xEF */
		{0, 0},										/* 0xF0 */
		{EVDEV_KEY_HANJA, 0},						/* 0xF1 */
		{EVDEV_KEY_HANGEUL, 0},						/* 0xF2 */
		{0, 0},										/* 0xF3 */
		{0, 0},										/* 0xF4 */
		{0, 0},										/* 0xF5 */
		{0, 0},										/* 0xF6 */
		{0, 0},										/* 0xF7 */
		{0, 0},										/* 0xF8 */
		{0, 0},										/* 0xF9 */
		{0, 0},										/* 0xFA */
		{0, 0},										/* 0xFB */
		{0, 0},										/* 0xFC */
		{0, 0},										/* 0xFD */
		{0, 0},										/* 0xFE */
		{0, 0},										/* 0xFF */
};

static const char *EVDEV_KEY_NAMES[EVDEV_KEY_MAX + 1] = {
		"RESERVED",			/* 0 */
		"ESC",				/* 1 */
		"1",				/* 2 */
		"2",				/* 3 */
		"3",				/* 4 */
		"4",				/* 5 */
		"5",				/* 6 */
		"6",				/* 7 */
		"7",				/* 8 */
		"8",				/* 9 */
		"9",				/* 10 */
		"0",				/* 11 */
		"MINUS",			/* 12 */
		"EQUAL",			/* 13 */
		"BACKSPACE",		/* 14 */
		"TAB",				/* 15 */
		"Q",				/* 16 */
		"W",				/* 17 */
		"E",				/* 18 */
		"R",				/* 19 */
		"T",				/* 20 */
		"Y",				/* 21 */
		"U",				/* 22 */
		"I",				/* 23 */
		"O",				/* 24 */
		"P",				/* 25 */
		"LEFTBRACE",		/* 26 */
		"RIGHTBRACE",		/* 27 */
		"ENTER",			/* 28 */
		"LEFTCTRL",			/* 29 */
		"A",				/* 30 */
		"S",				/* 31 */
		"D",				/* 32 */
		"F",				/* 33 */
		"G",				/* 34 */
		"H",				/* 35 */
		"J",				/* 36 */
		"K",				/* 37 */
		"L",				/* 38 */
		"SEMICOLON",		/* 39 */
		"APOSTROPHE",		/* 40 */
		"GRAVE",			/* 41 */
		"LEFTSHIFT",		/* 42 */
		"BACKSLASH",		/* 43 */
		"Z",				/* 44 */
		"X",				/* 45 */
		"C",				/* 46 */
		"V",				/* 47 */
		"B",				/* 48 */
		"N",				/* 49 */
		"M",				/* 50 */
		"COMMA",			/* 51 */
		"DOT",				/* 52 */
		"SLASH",			/* 53 */
		"RIGHTSHIFT",		/* 54 */
		"KPASTERISK",		/* 55 */
		"LEFTALT",			/* 56 */
		"SPACE",			/* 57 */
		"CAPSLOCK",			/* 58 */
		"F1",				/* 59 */
		"F2",				/* 60 */
		"F3",				/* 61 */
		"F4",				/* 62 */
		"F5",				/* 63 */
		"F6",				/* 64 */
		"F7",				/* 65 */
		"F8",				/* 66 */
		"F9",				/* 67 */
		"F10",				/* 68 */
		"NUMLOCK",			/* 69 */
		"SCROLLLOCK",		/* 70 */
		"KP7",				/* 71 */
		"KP8",				/* 72 */
		"KP9",				/* 73 */
		"KPMINUS",			/* 74 */
		"KP4",				/* 75 */
		"KP5",				/* 76 */
		"KP6",				/* 77 */
		"KPPLUS",			/* 78 */
		"KP1",				/* 79 */
		"KP2",				/* 80 */
		"KP3",				/* 81 */
		"KP0",				/* 82 */
		"KPDOT",			/* 83 */
		"KEY_84",			/* 84 */
		"ZENKAKUHANKAKU",	/* 85 */
		"102ND",			/* 86 */
		"F11",				/* 87 */
		"F12",				/* 88 */
		"RO",				/* 89 */
		"KATAKANA",			/* 90 */
		"HIRAGANA",			/* 91 */
		"HENKAN",			/* 92 */
		"KATAKANAHIRAGANA", /* 93 */
		"MUHENKAN",			/* 94 */
		"KPJPCOMMA",		/* 95 */
		"KPENTER",			/* 96 */
		"RIGHTCTRL",		/* 97 */
		"KPSLASH",			/* 98 */
		"SYSRQ",			/* 99 */
		"RIGHTALT",			/* 100 */
		"LINEFEED",			/* 101 */
		"HOME",				/* 102 */
		"UP",				/* 103 */
		"PAGEUP",			/* 104 */
		"LEFT",				/* 105 */
		"RIGHT",			/* 106 */
		"END",				/* 107 */
		"DOWN",				/* 108 */
		"PAGEDOWN",			/* 109 */
		"INSERT",			/* 110 */
		"DELETE",			/* 111 */
		"MACRO",			/* 112 */
		"MUTE",				/* 113 */
		"VOLUMEDOWN",		/* 114 */
		"VOLUMEUP",			/* 115 */
		"POWER",			/* 116 */
		"KPEQUAL",			/* 117 */
		"KPPLUSMINUS",		/* 118 */
		"PAUSE",			/* 119 */
		"SCALE",			/* 120 */
		"KPCOMMA",			/* 121 */
		"HANGEUL",			/* 122 */
		"HANJA",			/* 123 */
		"YEN",				/* 124 */
		"LEFTMETA",			/* 125 */
		"RIGHTMETA",		/* 126 */
		"COMPOSE",			/* 127 */
		"STOP",				/* 128 */
		"AGAIN",			/* 129 */
		"PROPS",			/* 130 */
		"UNDO",				/* 131 */
		"FRONT",			/* 132 */
		"COPY",				/* 133 */
		"OPEN",				/* 134 */
		"PASTE",			/* 135 */
		"FIND",				/* 136 */
		"CUT",				/* 137 */
		"HELP",				/* 138 */
		"MENU",				/* 139 */
		"CALC",				/* 140 */
		"SETUP",			/* 141 */
		"SLEEP",			/* 142 */
		"WAKEUP",			/* 143 */
		"FILE",				/* 144 */
		"SENDFILE",			/* 145 */
		"DELETEFILE",		/* 146 */
		"XFER",				/* 147 */
		"PROG1",			/* 148 */
		"PROG2",			/* 149 */
		"WWW",				/* 150 */
		"MSDOS",			/* 151 */
		"SCREENLOCK",		/* 152 */
		"ROTATE_DISPLAY",	/* 153 */
		"CYCLEWINDOWS",		/* 154 */
		"MAIL",				/* 155 */
		"BOOKMARKS",		/* 156 */
		"COMPUTER",			/* 157 */
		"BACK",				/* 158 */
		"FORWARD",			/* 159 */
		"CLOSECD",			/* 160 */
		"EJECTCD",			/* 161 */
		"EJECTCLOSECD",		/* 162 */
		"NEXTSONG",			/* 163 */
		"PLAYPAUSE",		/* 164 */
		"PREVIOUSSONG",		/* 165 */
		"STOPCD",			/* 166 */
		"RECORD",			/* 167 */
		"REWIND",			/* 168 */
		"PHONE",			/* 169 */
		"ISO",				/* 170 */
		"CONFIG",			/* 171 */
		"HOMEPAGE",			/* 172 */
		"REFRESH",			/* 173 */
		"EXIT",				/* 174 */
		"MOVE",				/* 175 */
		"EDIT",				/* 176 */
		"SCROLLUP",			/* 177 */
		"SCROLLDOWN",		/* 178 */
		"KPLEFTPAREN",		/* 179 */
		"KPRIGHTPAREN",		/* 180 */
		"NEW",				/* 181 */
		"REDO",				/* 182 */
		"F13",				/* 183 */
		"F14",				/* 184 */
		"F15",				/* 185 */
		"F16",				/* 186 */
		"F17",				/* 187 */
		"F18",				/* 188 */
		"F19",				/* 189 */
		"F20",				/* 190 */
		"F21",				/* 191 */
		"F22",				/* 192 */
		"F23",				/* 193 */
		"F24",				/* 194 */
		"KEY_195",			/* 195 */
		"KEY_196",			/* 196 */
		"KEY_197",			/* 197 */
		"KEY_198",			/* 198 */
		"KEY_199",			/* 199 */
		"PLAYCD",			/* 200 */
		"PAUSECD",			/* 201 */
		"PROG3",			/* 202 */
		"PROG4",			/* 203 */
		"DASHBOARD",		/* 204 */
		"SUSPEND",			/* 205 */
		"CLOSE",			/* 206 */
		"PLAY",				/* 207 */
		"FASTFORWARD",		/* 208 */
		"BASSBOOST",		/* 209 */
		"PRINT",			/* 210 */
		"HP",				/* 211 */
		"CAMERA",			/* 212 */
		"SOUND",			/* 213 */
		"QUESTION",			/* 214 */
		"EMAIL",			/* 215 */
		"CHAT",				/* 216 */
		"SEARCH",			/* 217 */
		"CONNECT",			/* 218 */
		"FINANCE",			/* 219 */
		"SPORT",			/* 220 */
		"SHOP",				/* 221 */
		"ALTERASE",			/* 222 */
		"CANCEL",			/* 223 */
		"BRIGHTNESSDOWN",	/* 224 */
		"BRIGHTNESSUP",		/* 225 */
		"MEDIA",			/* 226 */
		"SWITCHVIDEOMODE",	/* 227 */
		"KBDILLUMTOGGLE",	/* 228 */
		"KBDILLUMDOWN",		/* 229 */
		"KBDILLUMUP",		/* 230 */
		"SEND",				/* 231 */
		"REPLY",			/* 232 */
		"FORWARDMAIL",		/* 233 */
		"SAVE",				/* 234 */
		"DOCUMENTS",		/* 235 */
		"BATTERY",			/* 236 */
		"BLUETOOTH",		/* 237 */
		"WLAN",				/* 238 */
		"UWB",				/* 239 */
		"UNKNOWN",			/* 240 */
		"VIDEO_NEXT",		/* 241 */
		"VIDEO_PREV",		/* 242 */
		"BRIGHTNESS_CYCLE", /* 243 */
		"BRIGHTNESS_AUTO",	/* 244 */
		"DISPLAY_OFF",		/* 245 */
		"WWAN",				/* 246 */
		"RFKILL",			/* 247 */
		"MICMUTE",			/* 248 */
};

const char *ogon_evdev_keyname(DWORD evdevcode) {
	if (evdevcode > EVDEV_KEY_MAX) {
		return nullptr;
	}
	return EVDEV_KEY_NAMES[evdevcode];
}

DWORD ogon_rdp_scancode_to_evdev_code(
		DWORD flags, DWORD scancode, DWORD keyboardType) {
	OGON_UNUSED(keyboardType);
	if (scancode > 255) {
		return 0;
	}
	return RDP_SCANCODE_TO_EVDEV_CODE[scancode][(flags & 0x100) ? 1 : 0];
}
