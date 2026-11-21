#pragma once
#include "FusionAdventureSession273.h"
#include <string>
class Board;class Zombie;class Coin;
namespace LawnFusion
{
// One bounded readiness policy for host entry, remote entry and live scheduler.
// Public exposure and profile unlocks remain separate requirements.
constexpr bool NativeSourceAdventureLevelReady273(int level,uint32_t cycle)
{return cycle==1&&level>=1&&level<=4;}
struct FusionCommand;
// Identifies the real source session on host and snapshot clients; legacy
// Classic-Adventure+Fusion and validation fixtures do not enable this state.
bool IsNativeSourceAdventure273(const Board*);
// After stock Board initialization, before chooser/intro. Source geometry and
// economy replace stock defaults; clients adopt boot view without RNG/planning.
bool InitializeNativeSourceAdventure273(Board*,const FusionAdventureSession273& boot,std::string* error=nullptr);
// True means source session owns this path, including pause/client/error/award.
// Call instead of stock wave update, once per actual native gameplay tick.
// Clients only present the replicated tutorial. Never creates another Board.
bool UpdateNativeSourceAdventure273(Board*);
// Separate Board::UpdateSunSpawning early hook. The wave update does not tick
// this clock. Confirmed source item0 is25; first/repeat7.5s, columns3..8.
// PRIMARY: levels-ghidra-batch4.json SunUpdate0x180324340 resets0x40f00000;
// zombie-data-sun.json CoinSun.Start0x180440E40 item0 sets0x19=25.
// This supersedes the old levels-first-campaign.json subsequent7.0 entry.
bool UpdateNativeSourceAdventureSun273(Board*);
// Factory hook AFTER successful full typed initialization, once per generation.
// A friendly birth is not an enemy; no native award/WIN is committed here.
bool RegisterNativeSourceAdventureEnemy273(Zombie*);
// Actual source death, BEFORE native death animation/DropLoot/body destruction.
// NOT an unconditional DataArrayFree/board-teardown callback. Duplicate calls
// are ignored. Source1..3 has no MC conversion; friendly deaths are excluded.
bool RetireNativeSourceAdventureEnemy273(Zombie*);
bool HandleNativeSourceAdventurePointer273(Board*,int button,int x,int y);
bool HandleNativeSourceAdventureRewardCommand273(Board*,int slot,const FusionCommand&);
}
