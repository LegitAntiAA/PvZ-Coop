#include "FusionChomperRules.h"
#include <cmath>

namespace LawnFusion
{
namespace
{
constexpr FusionChomperProfile profiles[] = {
    {5,300,3000,.65,.85,1.0,2.0833333,.8,1.1666666,1.0,0,0,0,false},
    {1011,300,4000,static_cast<double>(.6666667f),static_cast<double>(1.0f),1.0,
        static_cast<double>(2.0833333f),static_cast<double>(.8f),static_cast<double>(1.1666666f),1,0,0,0,false,"idle","idle","shoot"},
    {1015,300,4000,.5,static_cast<double>(.9166667f),static_cast<double>(1.4166666f),
        1.0,static_cast<double>(.8f),static_cast<double>(1.1666666f),1,0,0,0,false},
    {1012,4000,4000,static_cast<double>(1.2166667f),static_cast<double>(1.9166666f),
        static_cast<double>(2.3333333f),1.0,static_cast<double>(.8f),1.0,
        static_cast<double>(.8f),1000,0,0,false},
    {1014,300,4000,.65,.85,1.0,2.0833333,.8,1.1666666,1.0,0,200,4,false},
    {1016,300,4000,.65,.85,1.0,2.0833333,.8,1.1666666,1.0,0,0,0,true},
    {1174,1000,1100,static_cast<double>(.8333333f),static_cast<double>(1.1666666f),
        static_cast<double>(1.3333334f),2.0,static_cast<double>(.8f),static_cast<double>(1.3333334f),1.0,0,0,0,false},
    {1192,300,4000,static_cast<double>(.65f),static_cast<double>(.85f),1.0,
        static_cast<double>(2.0833333f),static_cast<double>(.8f),static_cast<double>(1.1666666f),1.0,0,0,0,false}};
bool SpeedValid(float speed)
{ return std::isfinite(speed) && speed >= .9f && speed <= 1.1f; }
int EventTicks(double seconds, double speed)
{ return static_cast<int>(std::ceil(seconds / speed * 100.0)); }
}

const FusionChomperProfile* FindFusionChomperProfile(uint16_t kind)
{
    for (const auto& profile : profiles) if (profile.kind == kind) return &profile;
    return nullptr;
}

bool TryGetFusionChomperTiming(uint16_t kind, float speed, FusionChomperTiming& result)
{
    const auto* profile = FindFusionChomperProfile(kind);
    if (!profile || !SpeedValid(speed)) return false;
    result = {EventTicks(profile->biteSeconds,speed),EventTicks(profile->chewSeconds,speed),
        EventTicks(profile->biteDuration,speed),
        EventTicks(profile->swallowDuration,profile->swallowStateSpeed * speed)};
    return true;
}

bool TryGetFusionChomperChewTicks(uint16_t kind, float speed, int& ticks)
{
    const auto* profile = FindFusionChomperProfile(kind);
    if (!profile || !SpeedValid(speed)) return false;
    ticks = EventTicks(profile->chewDuration,speed);
    return true;
}

bool TryRecoverFusionChomper(uint16_t kind, int health, int maxHealth, int& recoveredHealth)
{
    const auto* profile = FindFusionChomperProfile(kind);
    return profile && profile->recovery > 0 && maxHealth == profile->maxHealth && health > 0 &&
        TryApplyFusionDeathHeal(health,maxHealth,profile->recovery,recoveredHealth);
}

bool TryAdvanceFusionChomperDamageStage(uint16_t kind, int health,
    uint8_t previousStage, uint8_t& stage)
{
    if (kind != 1012 || previousStage > 2) return false;
    // ReplaceSprite has strict comparisons on both sides. At exactly1333/2666
    // Unity performs no write, so HP alone cannot reconstruct this appearance.
    stage = health > 2666 ? 0 : health > 1333 && health < 2666 ? 1 :
        health < 1333 ? 2 : previousStage;
    return true;
}

bool AdvanceFusionChomperSunReward(uint16_t kind, bool callbackActive,
    int& remainingTicks, int& coinCount)
{
    const auto* profile = FindFusionChomperProfile(kind);
    if (!profile || profile->sunCount == 0 || remainingTicks < 0 || remainingTicks > profile->sunDelayTicks)
        return false;
    coinCount = 0;
    if (!callbackActive) remainingTicks = 0;
    else if (remainingTicks > 0 && --remainingTicks == 0) coinCount = profile->sunCount;
    return true;
}

bool TryMakeFusionChomperBlast(uint16_t kind, double shadowX, double shadowY,
    int row, NativeBlastSpec& spec)
{
    const auto* profile = FindFusionChomperProfile(kind);
    if (!profile || !profile->cherryBite || !std::isfinite(shadowX) || !std::isfinite(shadowY)) return false;
    NativeBlastSpec result;
    result.centerX = shadowX + 1.58 * (72.0 / 1.35);
    result.centerY = shadowY - .34 * (100.0 / 1.67);
    result.row = row;
    if (!FusionDeathBlastValid(result)) return false;
    spec = result;
    return true;
}
}
