namespace cpp ogon
namespace java ogon
namespace php ogon
namespace perl ogon


typedef bool TBOOL
typedef i16 TINT16
typedef i32 TINT32
typedef i64 TINT64
typedef string TSTRING
typedef i32 TDWORD
typedef i8 TBYTE

struct TVersion {
	1:TINT32 VersionMajor;
	2:TINT32 VersionMinor;
}

struct TSessionInfo
{
	1:TDWORD sessionId;
	2:TSTRING winStationName;
	3:TDWORD connectState;
}

typedef list<TSessionInfo> TSessionList

struct TReturnEnumerateSession {
	1: TBOOL returnValue;
	2: TSessionList sessionInfoList;
}

struct TReturnVirtualChannelOpen {
	1: TSTRING pipeName;
	2: TDWORD instance;
}

struct TClientDisplay
{
	1:TINT32 displayWidth;
	2:TINT32 displayHeight;
	3:TINT32 colorDepth;
}

struct TWTSINFO
{
	1:TDWORD State;
	2:TDWORD SessionId;
	3:TDWORD IncomingBytes;
	4:TDWORD OutgoingBytes;
	5:TDWORD IncomingFrames;
	6:TDWORD OutgoingFrames;
	7:TDWORD IncomingCompressedBytes;
	8:TDWORD OutgoingCompressedBytes;
	9:TSTRING WinStationName;
	10:TSTRING Domain;
	11:TSTRING UserName;
	12:TINT64 ConnectTime;
	13:TINT64 DisconnectTime;
	14:TINT64 LastInputTime;
	15:TINT64 LogonTime;
	16:TINT64 CurrentTime;
}

union TSessionInfoValue
{
	1:TBOOL boolValue;
	2:TINT16 int16Value;
	3:TINT32 int32Value;
	4:TSTRING stringValue;
	5:TClientDisplay displayValue;
	6:TWTSINFO WTSINFO;
	7:TINT64 int64Value;
}

struct TReturnQuerySessionInformation
{
	1:TBOOL returnValue;
	2:TSessionInfoValue infoValue;
}
struct TReturnLogonConnection{
	1:TBOOL success;
	2:TSTRING authToken;
}

service otsapi {
	TVersion getVersionInfo(1:TVersion versionInfo);
	TReturnLogonConnection logonConnection(1:TSTRING username, 2:TSTRING password, 3:TSTRING domain);
	TDWORD getPermissionForToken(1:TSTRING authToken);
	bool logoffConnection(1:TSTRING authToken);
	TDWORD ping(1:TDWORD input);
	TReturnVirtualChannelOpen virtualChannelOpen(1:TSTRING authToken, 2:TDWORD sessionId, 3:TSTRING virtualName, 4:TBOOL isDynChannel, 5:TDWORD flags);
	bool virtualChannelClose(1:TSTRING authToken,2:TDWORD sessionId, 3:TSTRING virtualName, 4:TDWORD instance);
	bool disconnectSession(1:TSTRING authToken, 2:TDWORD sessionId, 3:TBOOL wait);
	bool logoffSession(1:TSTRING authToken,2:TDWORD sessionId, 3:TBOOL wait);
	TReturnEnumerateSession enumerateSessions(1:TSTRING authToken,2:TDWORD Version);
	TReturnQuerySessionInformation querySessionInformation(1:TSTRING authToken, 2:TDWORD sessionId, 3:TINT32 infoClass);
	bool startRemoteControlSession(1:TSTRING authToken, 2:TDWORD sourceLogonId, 3:TDWORD targetLogonId, 4:TBYTE HotkeyVk, 5:TINT16 HotkeyModifiers, 6:TDWORD flags);
	bool stopRemoteControlSession(1:TSTRING authToken, 2:TDWORD sourceLogonId, 3:TDWORD targetLogonId);
	TDWORD sendMessage(1:TSTRING authToken, 2:TDWORD sessionId, 3:TSTRING title, 4:TSTRING message, 5:TDWORD style, 6:TDWORD timeout, 7:TBOOL wait);
}
