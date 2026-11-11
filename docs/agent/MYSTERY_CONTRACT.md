# Mystery/difficulty implementation contract (protocol32)

Baseline f1ca23d is pushed main. Build9b/protocol665 and Promote9b Passed. Production work may proceed in assigned modules. Root runs frozen9b original regressions; do not rebuild/launch games until slot granted.

## Ownership

- Root: NetManager/Common/App/Interactions, project wiring, cost call sites in SeedPacket.cpp (coordinate Entity), regression/tracking.
- UI: SeedChooserScreen.cpp/.h, mystery UI smoke helper+runner. No NetManager/SeedPacket edits.
- Entity: SeedPacket.h/.cpp safeguards/metadata/copy, System/SaveGame compatibility, NetBoardSync before IsCoopEndless, new portable NetMystery.h codec/tests; Board.h ONLY NetEncode/ApplyStartPlaying declarations near camera. No gameplay tail or cost methods. Root edits cost blocks in SeedPacket only after Entity release.
- Modes: Board/Challenge/Plant/Zombie gameplay and NetBoardSync after IsCoopEndless, new portable difficulty helper/tests. Board.h gameplay declarations near GetCoopDifficulty, adaptive state BEFORE mPaused so raw native save segment does not grow. No SeedPacket/NetManager/App/codec edits.

## Public interfaces

NetManager: uint8_t mChooserMysteryMask; bool RequestChooserMystery(bool); bool IsChooserMystery(int) const; bool HasActiveMysteryDeck() const. ChooserPick gets bool mMysteryToggle=false, bool mMysteryEnabled=false. Toggle request uses NETMSG_CHOOSER_MYSTERY45 one bool byte, authenticated slot. One FIFO with pick/unpick. UI exports bool CoopApplyMysteryRequest(int slot,bool) to clear mutable picks, set authoritative mask and reset readiness; Root dispatches request, UI poll applies. Hostlocal request enqueues same FIFO. Client mask changes only on validated STATE/START echo. Reset mask on OPEN/disconnect.

SeedPacket: uint8_t mRandomDiscountLevel (0 ordinary,1..10 origin difficulty), bool IsMysteryPending() const (level>0 && typeNONE); void SetMysteryPlaceholder(uint8_t level). New ordinary SetPacketType clears origin; reveal preserves level around call. Copy/compaction preserves origin, new cards reset it. Placeholder Draw/Update/tooltip/input must never call type-dependent lookup on NONE. Unknown packet visible as ? through INTRO to all owners/viewers, with no price/name/cooldown.

Board: bool ResolveMysteryPackets() host-only whilePLAYING and chooser still alive; once pending-only, private random_device-backed sampler, same SeedTypeAvailable/trial/mode restrictions, ordinary uniqueness within owner, fixed excludes ordinary overlap, one Imitator may copy existing rolled/fixed type. Other owners do not constrain. int GetSeedPacketCost(const SeedPacket&) const central base/personal discount/PvP support cost (pending unaffordable). Origin percent5..25 linear: basis points=500+(2000*(D-1)+4)/9; ceil integer cost, free stays0; total stacked reduction capped35%. PvP native zombie /2 applies before discounts.

## Wire / commit

STATE: slots4,maskU8, four {countU8,(typeU8,imitaterU8)*count}. Mystery owner lists consist of fixed(slot0 only) + pending255,255 markers. No real hidden types exist. Counts include markers. Mask eligible/no reserved high bits; exact pending quota when enabled. Decode transactional and validate all real availability/duplicates/fixed prefix before any mutation; markers never enter mChosenSeeds.

CHOOSER_START: legacy hostQuotaU8,slots4,maskU8, four {countU8,(typeU8,imitaterU8,idU16,originLevelU8)*count}. Max32 entries (actual bank max26). Pending255,255 with origin1..10 and nonzero unique ID. Real manual origin0. UI host builds final relative bank and allocates all IDs BEFORE broadcast; serialize by owner group using same committed bank identities, then no second rebuild. Clients preserve supplied IDs in relative bank. Exact quotas/ready checks remain.

START_PLAYING: existing camera8 followed revealCountU8 and {idU16,typeU8,imitaterU8}*N. Entity supplies Board::NetEncodeStartPlaying(NetBuffer&) and bool NetApplyStartPlaying(NetBuffer&). Root host sets scenePLAYING, resolves while chooser alive, kills chooser, advances phase, encodes combined barrier, broadcasts beforeStartLevel. Client decoder validates camera+exact pending ID set+types fully before applying either; reveal in same call as camera, then Root calls StartPlaying with no render between. Existing camera-only method stays8bytes for spectator bootstrap. No new REVEAL message.

Full snapshot seed entry16→20 append originLevel0..10, refreshTimeU16 and refreshingU8. Pending canonical tuple id!=0,type255,imitater255,origin>0,active0,cooldown255,slotNext255,slotCountdown0,slotPosition0,refreshTime0,refreshing0. Empty id0/type255/origin0. NetAllocId treats pending as live. Stable-ID matching/reassign accepts pending; full snapshot afterPLAYING preserves origin for latejoin/kick. Seed-entry decoding is bounded/transactional; the existing full snapshot applies scalar headers before validating all entities, so whole-snapshot transactionality is not claimed. Encoded refreshing is0 whenever encoded duration is0, including native zombie purchases; the decoder still rejects a wire refreshing flag without a positive duration. Never generate beforePLAYING; no type-derived side channels in cursors/Alt tooltip/chat.

## Save and tests

Keep SeedPacket native size via padding if possible, static_assert layout. Old raw save padding is not trusted: sanitize/migrate version2 origins to0; version3 must persist origin explicitly or have defined padding semantics. Do not silently wipe the user's profile/progress. Coordinate Board adaptive fields beforemPaused and save version with Root.

Required actual tests: toggle/cancel/ready/order, multi-owner manual overlap allowed, owner/remote secrecy throughINTRO and wire, Imitator repeat, stable-ID reveal, price display=spend, ownership persistence and latejoin; malformed codec restores position/state. All-mode family difficulty D1stock/D10measurable/PvEHPunchanged, DDA bothsides bounded/hysteresis/noheal/pause/terminal. Numeric helper tests must use actualproduction helpers. Separate implementation from runtime verification.

