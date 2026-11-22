#pragma once
#include <array>
#include <vector>
#include <cstdint>
namespace LawnFusion {
// Source TravelMgr component data. GameAPP owns it, Board owns only cooldown.
// The source methods do not save these bits into Adventure completion/profile.
struct FusionTravelBuff360Ledger {
 uint64_t advanced=0;uint32_t ultimate=0;
 std::array<int32_t,48> advancedUnlockRound{};
};
struct FusionTravelBuff360Pool {std::vector<uint8_t> advanced,ultimate;};
enum class FusionTravelBuff360Type:uint8_t {Advanced=1,Ultimate=2};
struct FusionTravelBuff360Award {FusionTravelBuff360Type type=FusionTravelBuff360Type::Advanced;uint8_t index=0;};
bool FusionTravelBuff360LedgerValid(const FusionTravelBuff360Ledger&);
bool HasFusionTravelAdvanced360(const FusionTravelBuff360Ledger&,int index);
bool HasFusionTravelUltimate360(const FusionTravelBuff360Ledger&,int index);
bool BuildFusionTravelBuff360Pool(const FusionTravelBuff360Ledger&,bool happyRandom,FusionTravelBuff360Pool&);
// Source uses TWO draws: weighted group draw [0,A+U), then index in chosen
// group [0,groupCount). It does not flatten these into one random selection.
bool SelectFusionTravelBuff360Award(const FusionTravelBuff360Pool&,int groupDraw,int indexDraw,FusionTravelBuff360Award&);
bool CommitFusionTravelBuff360Award(const FusionTravelBuff360Ledger&,FusionTravelBuff360Award,int currentSurvivalRound,FusionTravelBuff360Ledger&);
bool StepFusionTravelBuff360Cooldown(float previous,float nativeElapsedSeconds,float& next);
}
