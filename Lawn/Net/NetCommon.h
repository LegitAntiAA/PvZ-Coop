#ifndef __NETCOMMON_H__
#define __NETCOMMON_H__

/**
 * PvZ Co-op — common network definitions.
 *
 * Architecture: host-authoritative (server-client).
 *  - The host runs the only real simulation (Board, zombies, plants, sun, waves, RNG).
 *  - Clients mirror the state from full-state snapshots and send raw input events.
 *  - Transport: TCP over the Radmin VPN LAN interface, length-prefixed frames.
 */

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <deque>
#include <map>

namespace LawnNet
{

// Set while the client creates ghost entities so their init sounds stay silent
// (the host broadcasts the same sounds via events).
extern bool gNetSuppressSounds;

static const uint32_t NET_PROTOCOL_VERSION = 8; // authoritative intro presentation + gameplay state
static const uint16_t NET_DEFAULT_PORT = 27713;			// PvZ co-op port
static const int NET_MAX_PLAYERS = 4;					// 1 host + 3 clients
static const int NET_MAX_NICK = 16;						// bytes (without terminator)
static const int NET_MAX_FRAME = 65535;					// u16 wire length hard cap
static const uint8_t NET_SHARED_OWNER = 0xFF;			// team-shared seed packet

static const int NET_SNAPSHOT_HZ_MIN = 15;				// adaptive snapshot rate bounds
static const int NET_SNAPSHOT_HZ_MAX = 30;
static const int NET_SNAPSHOT_HZ_DEFAULT = 15;
static const int NET_CURSOR_HZ = 30;					// cosmetic cursor updates

// Relative player colors: viewer always sees self uncolored; others get
// blue/red/green in slot order starting after the viewer (wrapping around).
// With 2 players both sides mutually see each other as blue.
static const int NET_PLAYER_COLOR_COUNT = 3;
struct NetColor { uint8_t r, g, b; };
inline NetColor GetRelativePlayerColor(int theViewerSlot, int theOwnerSlot)
{
	if (theOwnerSlot == theViewerSlot)
		return { 255, 255, 255 };
	int anOffset = ((theOwnerSlot - theViewerSlot) % NET_MAX_PLAYERS + NET_MAX_PLAYERS) % NET_MAX_PLAYERS;
	// anOffset in 1..3 for the other players -> blue, red, green
	static const NetColor aColors[NET_PLAYER_COLOR_COUNT] = { { 70, 130, 255 }, { 255, 70, 70 }, { 80, 200, 80 } };
	return aColors[(anOffset - 1) % NET_PLAYER_COLOR_COUNT];
}
inline NetColor GetPlayerColorBySlot(int theSlot)
{
	static const NetColor aColors[NET_MAX_PLAYERS] = { { 255, 255, 255 }, { 70, 130, 255 }, { 255, 70, 70 }, { 80, 200, 80 } };
	return aColors[theSlot];
}

// -------------------------------------------------------------------------
// Message types
// -------------------------------------------------------------------------
enum NetMsgType : uint8_t
{
	NETMSG_PING = 1,			// both: u32 seq
	NETMSG_PONG = 2,			// both: u32 seq
	NETMSG_HELLO = 3,			// c->h: u32 version, string nick
	NETMSG_WELCOME = 4,			// h->c: u8 yourSlot, string hostNick
	NETMSG_LOBBY_FULL = 5,		// h->c: (disconnect after)
	NETMSG_VERSION_MISMATCH = 6,// h->c: (disconnect after)

	NETMSG_LOBBY_STATE = 10,	// h->all: 4 x {u8 active, u8 slot, string nick, u16 ping, u8 team, u8 ready}
	NETMSG_TEAM_SELECT = 11,	// c->h: u8 team (0 plants / 1 zombies); h echoes via LOBBY_STATE
	NETMSG_START_GAME = 12,		// h->all: u8 gameMode, i32 level, u8 playerCount, u8 teams[4], TD path cells
	NETMSG_SCENE_CMD = 13,		// h->all: u8 cmd, i32 param (menu/challenge/award/etc sync)
	NETMSG_BACK_TO_MENU = 14,	// h->all: (host ended the game)
	NETMSG_START_PLAYING = 15,	// h->all: seed chooser/intro finished; enter gameplay now
	NETMSG_INTRO_STATE = 16,	// h->all: authoritative cutscene presentation + preview zombies

	NETMSG_SNAPSHOT = 20,		// h->c: full logical board state
	NETMSG_BOARD_EVENT = 21,	// h->c: u8 eventType + payload (sounds, instant visuals)

	NETMSG_INPUT_MOUSE = 30,	// c->h: u8 down(1)/up(0), u8 clickCount(signed byte), i16 x, i16 y
	NETMSG_INPUT_KEY = 31,		// c->h: u16 keyCode, u8 down
	NETMSG_CURSOR = 32,			// both: u8 slot, i16 x, i16 y, u8 cursorType, u8 heldSeed, u8 heldImitater, u8 flags
	NETMSG_PAUSE = 33,			// h->c: u8 paused
	NETMSG_PAUSE_REQ = 34,		// c->h: (client asks; host may ignore)

	NETMSG_CHOOSER_OPEN = 40,	// h->all: u8 bankCount, u8 pickQuota[4]
	NETMSG_SEED_PICK = 41,		// c->h: u8 seedType, u8 imitaterType
	NETMSG_SEED_UNPICK = 42,	// c->h: u8 seedType
	NETMSG_CHOOSER_STATE = 43,	// h->all: per player: picked list
	NETMSG_CHOOSER_START = 44,	// h->all: combined seed list (u8 count, then pairs seedType+imitater, then owner per slot)

	NETMSG_PROFILE_SYNC = 50,	// h->c: coins, level, seed counts, challenge records
	NETMSG_STORE_ACTION = 51,	// c->h: u8 item, u8 action
	NETMSG_STORE_SYNC = 52,		// h->c: coins + store purchases

	NETMSG_STATS = 60,			// c->h: u16 pingMs, u16 snapshotApplyMs
	NETMSG_RATE = 61,			// h->c: u16 snapshotHz

	NETMSG_DEBUG_TEXT = 70		// both: string (debug log)
};

// Scene commands (NETMSG_SCENE_CMD)
enum NetSceneCmd : uint8_t
{
	SCENECMD_SHOW_GAME_SELECTOR = 0,
	SCENECMD_SHOW_CHALLENGE_PAGE = 1,	// param = page
	SCENECMD_SHOW_STORE = 2,
	SCENECMD_SHOW_AWARD = 3,			// param = award type
	SCENECMD_KILL_BOARD = 4,
	SCENECMD_SHOW_SEED_CHOOSER = 5,
	SCENECMD_SHOW_QUICKPLAY = 6
};

// Board event types (NETMSG_BOARD_EVENT)
enum NetBoardEvent : uint8_t
{
	BOARDEV_PLANT_ADDED = 0,		// u16 id, u8 type, u8 imitater, u8 row, u8 col, u8 flags
	BOARDEV_PLANT_REMOVED = 1,		// u16 id
	BOARDEV_SUN_ADDED = 2,			// u16 id, u8 coinType, u8 motion, i16 x4, i16 y4, u16 value, u8 usableSeed
	BOARDEV_SUN_COLLECTED = 3,		// u16 id
	BOARDEV_ZOMBIE_ADDED = 4,		// u16 id, u8 type, u8 row, u8 variant, u8 fromWave, f32 speed, f32 posX
	BOARDEV_ZOMBIE_REMOVED = 5,		// u16 id
	BOARDEV_WAVE_SPAWNED = 6,		// u8 wave (banner/horn handled by host sound events below)
	BOARDEV_SOUND = 7,				// u16 soundId
	BOARDEV_FOLEY = 8,				// u16 foleyId, u16 pitch
	BOARDEV_MOWER_USED = 9,			// u8 row
	BOARDEV_LEVEL_PROGRESS = 10,	// u8 progress
	BOARDEV_ZOMBIE_KILLED = 11,		// u16 id, u8 deathType
	BOARDEV_SHAKE = 12,				// u16 amount
	BOARDEV_FOG_BLOWN = 13,
	BOARDEV_PVP_BRAIN = 14,			// u8 brainRow
	BOARDEV_ENDLESS_REPICK = 15,	// (every 10 waves notice)
	BOARDEV_BOSS_EVENT = 16,
	BOARDEV_GRIDITEM_SYNC = 17,		// full grid item state refresh
	BOARDEV_SEED_PACKET_UPDATE = 18,// packet cooldown refresh
	BOARDEV_PLANT_STATE = 19,		// u16 id, u8 state byte (anim transitions)
	BOARDEV_ZOMBIE_STATE = 20		// u16 id, phase byte (anim transitions)
};

// Board snapshot flags
enum NetSnapshotFlag : uint8_t
{
	SNAPFLAG_PAUSED = 1,
	SNAPFLAG_LEVEL_COMPLETE = 2,
	SNAPFLAG_ZOMBIES_WON = 4,
	SNAPFLAG_FOG_BLOWN = 8
};

// PvP teams
enum NetTeam : uint8_t
{
	NETTEAM_NONE = 0,
	NETTEAM_PLANTS = 1,
	NETTEAM_ZOMBIES = 2
};

// -------------------------------------------------------------------------
// Player info
// -------------------------------------------------------------------------
struct NetPlayerInfo
{
	bool	mActive;
	uint8_t	mSlot;			// 0 = host
	uint8_t	mTeam;			// NetTeam (PvP only)
	uint8_t	mReady;			// seed chooser / team select readiness
	uint16_t mPing;
	char	mNick[NET_MAX_NICK + 1];

	NetPlayerInfo() { Reset(); }
	void Reset()
	{
		mActive = false;
		mSlot = 0;
		mTeam = NETTEAM_NONE;
		mReady = 0;
		mPing = 0;
		memset(mNick, 0, sizeof(mNick));
	}
};

// -------------------------------------------------------------------------
// Byte buffer for serialization
// -------------------------------------------------------------------------
class NetBuffer
{
public:
	std::vector<uint8_t> mData;
	size_t mReadPos;

public:
	NetBuffer() : mReadPos(0) {}

	inline void Clear() { mData.clear(); mReadPos = 0; }
	inline size_t Size() const { return mData.size(); }
	inline const uint8_t* Data() const { return mData.data(); }
	inline size_t Remaining() const { return mData.size() - mReadPos; }

	inline void Reserve(size_t theSize) { mData.reserve(theSize); }

	inline void WriteBytes(const void* theData, size_t theSize)
	{
		const uint8_t* aPtr = (const uint8_t*)theData;
		mData.insert(mData.end(), aPtr, aPtr + theSize);
	}
	inline bool ReadBytes(void* theData, size_t theSize)
	{
		if (Remaining() < theSize)
			return false;
		memcpy(theData, mData.data() + mReadPos, theSize);
		mReadPos += theSize;
		return true;
	}

	inline void WriteU8(uint8_t v) { WriteBytes(&v, 1); }
	inline void WriteU16(uint16_t v) { WriteBytes(&v, 2); }
	inline void WriteU32(uint32_t v) { WriteBytes(&v, 4); }
	inline void WriteI16(int16_t v) { WriteBytes(&v, 2); }
	inline void WriteI32(int32_t v) { WriteBytes(&v, 4); }
	inline void WriteF32(float v) { WriteBytes(&v, 4); }

	inline bool ReadU8(uint8_t* v) { return ReadBytes(v, 1); }
	inline bool ReadU16(uint16_t* v) { return ReadBytes(v, 2); }
	inline bool ReadU32(uint32_t* v) { return ReadBytes(v, 4); }
	inline bool ReadI16(int16_t* v) { return ReadBytes(v, 2); }
	inline bool ReadI32(int32_t* v) { return ReadBytes(v, 4); }
	inline bool ReadF32(float* v) { return ReadBytes(v, 4); }

	inline void WriteString(const char* theStr)
	{
		size_t aLen = theStr ? strlen(theStr) : 0;
		if (aLen > 255) aLen = 255;
		WriteU8((uint8_t)aLen);
		WriteBytes(theStr, aLen);
	}
	inline bool ReadString(char* theOut, int theMaxLen)
	{
		uint8_t aLen;
		if (!ReadU8(&aLen)) return false;
		if (Remaining() < aLen) return false;
		int aCopy = aLen < theMaxLen - 1 ? aLen : theMaxLen - 1;
		memcpy(theOut, mData.data() + mReadPos, aCopy);
		theOut[aCopy] = 0;
		mReadPos += aLen;
		return true;
	}

	// Bounds-safe integer reads that return 0 on truncation (robust against malformed frames)
	inline uint8_t ReadU8Or() { uint8_t v = 0; ReadU8(&v); return v; }
	inline uint16_t ReadU16Or() { uint16_t v = 0; ReadU16(&v); return v; }
	inline uint32_t ReadU32Or() { uint32_t v = 0; ReadU32(&v); return v; }
	inline int16_t ReadI16Or() { int16_t v = 0; ReadI16(&v); return v; }
	inline int32_t ReadI32Or() { int32_t v = 0; ReadI32(&v); return v; }
	inline float ReadF32Or() { float v = 0.f; ReadF32(&v); return v; }
};

// Frame: [u16 payloadSize][u8 msgType][payload]
bool NetEncodeFrame(const NetBuffer& thePayload, uint8_t theMsgType, NetBuffer& theFrame);
bool NetDecodeFrame(const uint8_t* theData, size_t theSize, uint8_t* theMsgType, NetBuffer& thePayload);

// Per-session diagnostics. Logs are written beside the executable under
// coop_logs and are safe to call from the connection worker threads.
void NetTraceOpen(const char* theRole);
void NetTraceClose();
void NetTrace(const char* theFormat, ...);
const char* NetMessageName(uint8_t theMsgType);

// Config file (coop.ini): nickname + unlock flags
struct NetConfig
{
	char mNick[NET_MAX_NICK + 1];
	bool mUnlockAllMinigames;
	bool mEnableCoop;			// master switch
	int  mBaseSnapshotHz;
	int  mWindowWidth;			// 0 = default 1066
	int  mWindowHeight;			// 0 = default 600
	bool mBorderlessFullscreen;	// fullscreen = borderless desktop (native res)

	NetConfig() { Reset(); }
	void Reset()
	{
		strcpy_s(mNick, "Player");
		mUnlockAllMinigames = false;
		mEnableCoop = true;
		mBaseSnapshotHz = NET_SNAPSHOT_HZ_DEFAULT;
		mWindowWidth = 0;
		mWindowHeight = 0;
		mBorderlessFullscreen = true;
	}
};

bool NetLoadConfig(NetConfig& theConfig);		// reads coop.ini next to the exe / userdata
bool NetSaveConfig(const NetConfig& theConfig);

} // namespace LawnNet

#endif // __NETCOMMON_H__
