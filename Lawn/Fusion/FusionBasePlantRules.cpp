#include "FusionBasePlantRules.h"
#include "FusionChomperRules.h"
#include "FusionShooterTiming.h"
#include <bit>
#include <cmath>
#include <limits>

namespace LawnFusion
{
namespace
{
constexpr FusionFumeProfile fumeProfiles[]={{7,0,false},{1037,5,false},{1023,0,true},{1159,0,false},{1160,0,false}};
constexpr double coldFumeEvents[]={static_cast<double>(.35f)};
constexpr double seaPuffEvents[]={static_cast<double>(.3f)};
const FusionShooterTimingProfile seaPuffTiming{19,1.5f,seaPuffEvents,1,.5,true,9};
const FusionShooterTimingProfile coldFumeTiming{1037,1.5f,coldFumeEvents,1,1,true,0xffff};
const FusionShooterTimingProfile hypnoFumeTiming{1023,1.5f,coldFumeEvents,1,1,true,0xffff};
const FusionShooterTimingProfile bucketTiming{1159,1.5f,coldFumeEvents,1,1,true,0xffff};
const FusionShooterTimingProfile helmetTiming{1160,1.5f,coldFumeEvents,1,1,true,0xffff};
constexpr double peaChomperEvent[]={static_cast<double>(.8333333f)};
const FusionShooterTimingProfile peaChomperTiming{1011,3,peaChomperEvent,1,2,true,4};
bool ShooterStatus(int status)
{
    if (status < 0) return false;
    switch (status)
    {
    case 1: case 3: case 7: case 9: case 10: case 12: case 16:
    case 20: case 21: case 22: case 23: case 24: return false;
    default: return true;
    }
}
}

const FusionFumeProfile* FindFusionFumeProfile(uint16_t kind)
{
    for (const auto& profile:fumeProfiles) if (profile.kind==kind) return &profile;
    return nullptr;
}
const FusionShooterTimingProfile* FindFusionBaseTimingProfile(uint16_t kind)
{
    if(kind==19)return &seaPuffTiming;
    if (kind==1159)return &bucketTiming;
    if (kind==1160)return &helmetTiming;
    if (kind==1011)return &peaChomperTiming;
    if (kind==1037) return &coldFumeTiming;
    if (kind==1023) return &hypnoFumeTiming;
    return kind==6 || kind==7 ? FindFusionShooterTimingProfile(kind) : nullptr;
}
bool TryGetFusionHypnoFumeThreshold(float health,int maximum,int armor,int armorMaximum,uint8_t mask,float& output)
{
    const int64_t total=static_cast<int64_t>(maximum)+armorMaximum;
    if (!std::isfinite(health) || health<0 || maximum<=0 || armor<0 || armorMaximum<0 || total<=0 ||
        total>(std::numeric_limits<int>::max)() || mask>127) return false;
    const int colors=std::popcount(mask);
    const volatile float combined=static_cast<float>(armor)+health;
    float threshold=combined/static_cast<float>(total);
    threshold=threshold>.5f ? 1.f : threshold+threshold;
    const volatile float adjustment=static_cast<float>(colors)*.05f;
    threshold=std::sqrt(threshold-adjustment);
    const float floor=.75f-adjustment;
    if (threshold<floor) threshold=floor;
    // Preserve source NaN comparison behavior for its negative sqrt domain:
    // NaN <= Random.value is false, so that branch damages instead of controls.
    output=threshold;
    return true;
}

bool TryGetFusionChomperTiming(float speed, FusionChomperTiming& result)
{
    return TryGetFusionChomperTiming(5,speed,result);
}

bool TryGetFusionBaseShotCounter(uint16_t kind, float speed, int& counter)
{
    const auto* timing=FindFusionBaseTimingProfile(kind);
    return timing && TryGetFusionShooterShotCounter(*timing, speed, counter);
}

float FusionBaseShootDuration(uint16_t kind, float speed)
{
    const auto* timing=FindFusionBaseTimingProfile(kind);
    return timing ? FusionShooterDuration(*timing, speed) : 0.0f;
}

bool FusionBaseCanAcquire(uint16_t kind, int status, bool sameRow,
    bool mindControlled, bool beforeDying, double deltaX)
{
    if ((kind != 6 && kind!=19 && !FindFusionFumeProfile(kind)) || !ShooterStatus(status) || !sameRow || mindControlled ||
        beforeDying || !std::isfinite(deltaX)) return false;
    return deltaX > 0 && deltaX < (kind == 6 || kind==19 ? 4.5 : 7.0) * kFusionBasePixelsX;
}

bool FusionPlantSearchUniqueZombieAllowed(int status, bool mindControlled, bool beforeDying)
{ return ShooterStatus(status) && !mindControlled && !beforeDying; }

bool FusionFumeCanHit(int status, bool sameRow, bool mindControlled, bool beforeDying, double deltaX)
{
    return ShooterStatus(status) && sameRow && !mindControlled && !beforeDying &&
        std::isfinite(deltaX) && deltaX >= 0 && deltaX <= 7.0 * kFusionBasePixelsX;
}

bool FusionChomperCanAcquire(int status, bool sameRow, bool mindControlled, bool beforeDying)
{
    if (status < 0 || !sameRow || mindControlled || beforeDying) return false;
    switch (status)
    {
    case 1: case 7: case 10: case 12: case 16:
    case 20: case 21: case 22: case 23: case 24: return false;
    default: return true;
    }
}

bool TryMakeFusionChomperRect(double x, double y, FusionBaseRect& result)
{
    if (!std::isfinite(x) || !std::isfinite(y)) return false;
    const double width = 1.5 * kFusionBasePixelsX, height = 1.5 * kFusionBasePixelsY;
    result = {x + .75 * kFusionBasePixelsX, y - 1.25 * kFusionBasePixelsY, width, height};
    return true;
}

bool TryMakeFusionPuffRect(double x, double y, FusionBaseRect& result)
{
    if (!std::isfinite(x) || !std::isfinite(y)) return false;
    const double width = .32 * kFusionBasePixelsX, height = .43096197 * kFusionBasePixelsY;
    result = {x - width / 2, y + .08048099 * kFusionBasePixelsY - height / 2, width, height};
    return true;
}

bool FusionPuffLifetimeExpired(int ageTicks) { return ageTicks < 0 || ageTicks > 75; }
}
