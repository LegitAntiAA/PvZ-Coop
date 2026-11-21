#ifndef LAWN_FUSION_SUMMON269_RULES_H
#define LAWN_FUSION_SUMMON269_RULES_H
#include <array>
#include <cstddef>
#include <cstdint>
#include "FusionShooterTiming.h"
namespace LawnFusion
{
enum class FusionSummon269Post : uint8_t { None, BreakSecondArmor, Health60, MinerRiseHealth60, PogoStartHealth60, PlantThirdHealth };
struct FusionSummon269Choice
{
    uint16_t kind=0xffff;
    FusionSummon269Post post=FusionSummon269Post::None;
    bool friendly=true,withEffect=true;
    int healthOverride=0;
    bool operator==(const FusionSummon269Choice&) const = default;
};
struct FusionSummon269NpcProfile
{
    uint16_t kind;
    const char* sourceClass;
    int body,firstArmor,secondArmor,biteDamage;
    bool commonWalker;
};
const FusionSummon269NpcProfile* FindFusionSummon269NpcProfile(uint16_t kind);
// Dependency profiles are not an implementation registry. Only commonWalker
// has the proven0/2/4 controller; specialist leaves remain explicit blockers.
bool IsFusionSummon269Caller(uint16_t plantKind);
bool TryChooseFusionHypno269(uint16_t plantKind,bool water,bool shooting,bool advanced1,bool eventBalanced,int draw,FusionSummon269Choice& output);
bool TryChooseFusionNut269(bool rainbow,bool water,FusionSummon269Choice& output);
bool TryChooseFusionIceHypno269(bool water,FusionSummon269Choice& output);
bool TryChooseFusionMachine269(bool water,int plantHealth,FusionSummon269Choice& output);
bool TryChooseFusionMagnet269(uint8_t item,int draw,bool water,FusionSummon269Choice& output);
bool TryGetFusionSummon269Interval(uint16_t plantKind,bool shooting,bool advanced,float& seconds);
bool TryGetFusionHypnoDoom269Rows(int row,int rowCount,bool advanced39,std::array<int,3>& output,int& count);
bool TryChooseFusionHypnoDoom269(bool water,FusionSummon269Choice& output);
struct FusionGoldHypno269Plan
{
    std::array<uint16_t,2> bosses{};
    int count=0;
    bool operator==(const FusionGoldHypno269Plan&) const = default;
};
bool TryPlanFusionGoldHypno269(int background,bool alreadySummoned,int draw,FusionGoldHypno269Plan& output);
bool TryGetFusionGoldHypno269Minion(uint16_t boss,uint16_t& output);
enum class FusionUmbrella269Action : uint8_t { None, Garlic, Replace, Coins, DamageAndCoins };
struct FusionUmbrella269Plan
{
    FusionUmbrella269Action action=FusionUmbrella269Action::None;
    uint16_t kind=0xffff;
    int coins=0,damage=0;
    bool retireOriginal=false;
    bool operator==(const FusionUmbrella269Plan&) const = default;
};
bool TryPlanFusionUmbrella269(uint16_t plantKind,float body,int firstArmor,int bodyMaximum,int firstMaximum,
    float goldRatio,float silverRatio,bool big,bool water,bool sameRow,bool beforeDying,FusionUmbrella269Plan& output);
// MoneyZombie is a specialist leaf, never an ordinary attacking Zombie alias.
bool IsFusionSummon269Money(uint16_t kind);
bool IsFusionSummon269BigKind(uint16_t kind);
bool TryStepFusionMoney269Health(uint16_t kind,float health,int maximum,float age,float delta,float& output);
bool TryGetFusionMoney269Reward(uint16_t kind,int& sourceCoinKind,int& count);
bool TryGetFusionHypno269Projectile(uint16_t plantKind,int attackDamage,int plantLevel,uint16_t& payload,int& damage);
bool TryGetFusionPuffLove269Threshold(float body,int firstArmor,int bodyMaximum,int firstMaximum,bool ultimate18,float& threshold);
bool TryUseFusionUltimateHypno269Item(int status,float countdown,int& nextStatus,float& nextCountdown);
// Shot status is captured when the source trigger fires. Item expiration must
// not shorten an already running shoot2/shoot3 clip.
const FusionShooterTimingProfile* FindFusionHypno269TimingProfile(uint16_t kind,int shotStatus=0);
const FusionShooterTimingProfile* FindFusionHypno269TimingProfile(uint16_t kind,const char* state);
float FusionHypno269MaximumDuration(uint16_t kind,float animationSpeed);
bool FusionHypno269PlantStateValid(uint16_t kind,int mode,int shotStatus,int attributeTicks,
    float idleSeconds,float shootSeconds,float animationSpeed,uint32_t shots);
bool FusionPuffLove269CanHit(int status,bool sameRow,bool mindControlled,bool captured);
bool FusionPuffLove269CanSplash(int status,int rowDelta,bool mindControlled,bool captured);
bool FusionPuffLove269CircleTouchesRect(double centerX,double centerY,double left,double bottom,double width,double height);
bool FusionHypno269ProjectileMetadataValid(uint16_t kind,int damage,uint8_t way);
}
#endif
