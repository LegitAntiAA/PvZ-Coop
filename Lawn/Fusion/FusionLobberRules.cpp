#include "FusionLobberRules.h"
#include "FusionShooterTiming.h"
#include "FusionCannon268Rules.h"
#include "FusionSolar269Rules.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <cstring>
namespace LawnFusion
{
namespace
{
constexpr FusionLobberProfile profiles[] = {
    {26,52,PROJECTILE_CABBAGE,40,-.1617,1.2408},
    {32,59,PROJECTILE_MELON,80,.2013,1.6071},
    {1116,60,PROJECTILE_WINTERMELON,80,.2013,1.6071},
    {28,54,PROJECTILE_KERNEL,20,.2211,1.6401},
    {1113,56,PROJECTILE_KERNEL,50,-.1617,1.2408},
    {1121,66,PROJECTILE_MELON,100,.2013,1.6071},
    {1111,53,PROJECTILE_CABBAGE,50,-.1617,1.2408},
    {1124,70,PROJECTILE_MELON,100,.2013,1.4652},
    {28,55,PROJECTILE_BUTTER,40,.2211,1.6401,true},
    {1113,57,PROJECTILE_BUTTER,70,-.1617,1.2408,true},
    {1121,67,PROJECTILE_MELON,120,.2013,1.6071,true},
    {1134,74,PROJECTILE_CABBAGE,60,-.1617,1.2408},
    {1138,76,PROJECTILE_KERNEL,40,.2211,1.6401},
    {1138,78,PROJECTILE_BUTTER,60,.2211,1.6401,true},
    {1141,81,PROJECTILE_MELON,100,.2013,1.6071},
    {1117,61,PROJECTILE_KERNEL,20,.2211,1.6401},
    {1117,62,PROJECTILE_BUTTER,40,.2211,1.6401,true},
    {1118,63,PROJECTILE_CABBAGE,40,-.1617,1.2408},
    {1119,64,PROJECTILE_MELON,80,.2013,1.6071},
    {1149,86,PROJECTILE_MELON,160,.2013,1.6071},
    {1126,71,PROJECTILE_MELON,140,.3325,1.8075},
    {1135,75,PROJECTILE_CABBAGE,80,-.1617,1.2408},
    {934,75,PROJECTILE_CABBAGE,300,0,0}}; // Live source Shoot node, not resting offsets.
constexpr double cabbageEvent[]={static_cast<double>(.31666666f)};
constexpr double cornEvent[]={static_cast<double>(.33333334f)};
constexpr double superEvent[]={static_cast<double>(.45f)};
constexpr FusionShooterTimingProfile extendedTiming[]={
    {1117,3,cornEvent,1,1,true,61}, {1117,3,cornEvent,1,1,true,62,"shoot2"},
    {1118,3,cabbageEvent,1,1,true,63}, {1119,3,cornEvent,1,1,true,64},
    {1149,3,cornEvent,1,1,true,86},
    {1126,5,superEvent,1,static_cast<double>(1.1666666f),true,71},
    {1135,3,cabbageEvent,1,1,true,75}, {1135,3,cabbageEvent,1,1,true,75,"supershoot"}};
constexpr FusionLobberProjectileTraits traits[] = {
    {52,1,FusionLobberBounce::None,false,false,false,false,false,2.0175073,1.3134866,-.012087107,-.07117817},
    {59,1,FusionLobberBounce::None,true,false,false,false,false,2.2239718,1.7006075,-.003484249,-.07547948},
    {60,1,FusionLobberBounce::None,true,true,false,false,false,2.2239718,1.7006075,-.003484249,-.07547948},
    {53,0,FusionLobberBounce::SunCabbage,false,false,false,true,true,2.0175073,1.3134866,-.012087107,-.07117817},
    {54,1,FusionLobberBounce::None,false,false,false,false,true,1.2586598,.51232433,-.012087107,-.022978932},
    {55,1,FusionLobberBounce::None,false,false,true,false,true,2.0175073,1.3134866,-.012087107,-.07117817},
    {56,3,FusionLobberBounce::HalfFirstTwo,false,false,false,false,true,1.6900997,1.1213596,-.012087107,-.022978932},
    {57,3,FusionLobberBounce::HalfFirstTwo,false,false,true,false,true,1.6900997,1.1213596,-.012087107,-.022978932},
    {66,1,FusionLobberBounce::None,true,false,false,false,true,2.2239718,1.7006075,-.003484249,-.07547948},
    {67,1,FusionLobberBounce::None,true,false,true,false,true,2.2239718,1.7006075,-.003484249,-.07547948},
    {70,2,FusionLobberBounce::Half,true,false,false,false,true,2.2239718,1.7006075,-.003484249,-.07547948},
    {74,1,FusionLobberBounce::None,false,false,false,false,true,2.0175073,1.3134866,-.012087107,-.07117817},
    {76,1,FusionLobberBounce::None,false,false,false,false,true,1.2586598,.51232433,-.012087107,-.022978932},
    {78,1,FusionLobberBounce::None,false,false,true,false,true,2.0175073,1.3134866,-.012087107,-.07117817,38},
    {81,1,FusionLobberBounce::None,true,false,false,false,true,2.2239718,1.7006075,-.003484249,-.07547948},
    {61,1,FusionLobberBounce::None,false,false,false,false,true,1.2586598,.51232433,-.012087107,-.022978932},
    {62,1,FusionLobberBounce::None,false,false,true,false,true,2.0175073,1.3134866,-.012087107,-.07117817},
    {63,2,FusionLobberBounce::SunCabbage,false,false,false,false,true,2.0175073,1.3134866,-.012087107,-.07117817},
    {64,1,FusionLobberBounce::None,true,false,false,false,true,2.2239718,1.7006075,-.003484249,-.07547948},
    {86,1,FusionLobberBounce::None,true,false,false,false,true,2.2239718,1.7006075,-.003484249,-.07547948},
    {71,1,FusionLobberBounce::None,true,false,false,false,true,2.2239718,1.7006075,-.003484249,-.07547948},
    {75,1,FusionLobberBounce::None,false,false,false,false,true,2.0175073,1.3134866,-.012087107,-.07117817}};
bool Point(const FusionLobberPoint& p) { return std::isfinite(p.x) && std::isfinite(p.y); }
bool Rect(const FusionLobberRect& r)
{ return Point({r.x,r.y}) && std::isfinite(r.width) && std::isfinite(r.height) && r.width >= 0 && r.height >= 0 && Point({r.x+r.width,r.y+r.height}); }
}
const FusionShooterTimingProfile* FindFusionExtendedLobberTimingProfile(uint16_t kind,const char* state)
{if(kind==934)return FindFusionSolar269Timing(state);if(state)for(const auto& row:extendedTiming)if(row.kind==kind&&!std::strcmp(state,row.visualState))return &row;return nullptr;}
const FusionLobberProfile* FindFusionLobberProfile(uint16_t kind)
{ return FindFusionLobberProfile(kind,false); }
const FusionLobberProfile* FindFusionLobberProfile(uint16_t kind, bool alternate)
{ if(const auto* row=FindFusionCannon268LobberProfile(kind,alternate))return row;for (const auto& row : profiles) if (row.kind == kind && row.alternate == alternate) return &row; return nullptr; }
const FusionLobberProfile* FindFusionLobberProjectileProfile(uint16_t kind)
{ if(const auto* row=FindFusionCannon268ProjectileProfile(kind))return row;for (const auto& row : profiles) if (row.projectileKind == kind) return &row; return nullptr; }
const FusionLobberProjectileTraits* FindFusionLobberProjectileTraits(uint16_t kind)
{ if(const auto* row=FindFusionCannon268ProjectileTraits(kind))return row;for (const auto& row : traits) if (row.kind == kind) return &row; return nullptr; }
int FusionLobberCornDenominator(uint16_t kind)
{ return FindFusionLobberProfile(kind,true) ? 4 : 0; }
bool TryChooseFusionLobberAlternate(uint16_t kind, int draw, bool& alternate)
{
    const int denominator = FusionLobberCornDenominator(kind);
    if (!denominator || draw < 0 || draw >= denominator) return false;
    alternate = draw == 0; return true;
}
bool TryGetFusionLobberPierceLimit(uint16_t kind, int wallet, int& limit)
{
    const auto* row = FindFusionLobberProjectileTraits(kind);
    if (!row || wallet < 0) return false;
    limit = row->fixedPierceLimit ? row->fixedPierceLimit : (std::clamp)(wallet/200+1,2,6);
    return true;
}
bool TryApplyFusionLobberBounce(uint16_t kind, int hits, float& vx, float& vy)
{
    const auto* row = FindFusionLobberProjectileTraits(kind);
    if (!row || row->bounce == FusionLobberBounce::None || hits <= 0 || (kind != 63 && hits > (row->fixedPierceLimit ? row->fixedPierceLimit : 6)) ||
        !std::isfinite(vx) || !std::isfinite(vy)) return false;
    if (vy <= 0) return true; // Source Vy<0 is descending, opposite native screen-Y velocity.
    if (row->bounce == FusionLobberBounce::HalfFirstTwo && hits >= 3) return true;
    if (row->bounce == FusionLobberBounce::SunCabbage && hits >= 3) { vy = -vy; return true; }
    vy *= -.5f; vx *= row->bounce == FusionLobberBounce::SunCabbage ? .4f : .5f;
    return true;
}
bool FusionLobberProjectileMetadataValid(uint16_t kind, int damage, int sunOnHit, ProjectileType nativeType)
{
    const auto* profile = FindFusionLobberProjectileProfile(kind);
    const auto* behavior = FindFusionLobberProjectileTraits(kind);
    return profile && behavior && (damage == profile->damage || (kind==75 && (damage==160||damage==300)) || (kind==53&&damage==300)) && sunOnHit == (behavior->sunReward ? 5 : 0) && nativeType == profile->nativeType;
}
bool TryGetFusionLobberCycleTicks(uint16_t kind, float jitter, int& ticks, float liveInterval)
{
    const auto* timing = FindFusionShooterTimingProfile(kind);
    if (!FindFusionLobberProfile(kind) || !timing || !std::isfinite(jitter) || jitter < -.1f || jitter > .1f) return false;
    if (!std::isfinite(liveInterval) || liveInterval < 0) return false;
    const float seconds = (liveInterval > 0 ? liveInterval : timing->attackIntervalSeconds) + jitter; // Source float addition, then native strict<0 deadline.
    if (!std::isfinite(seconds) || seconds <= 0 || seconds*100.0 >= (std::numeric_limits<int>::max)()) return false;
    ticks = static_cast<int>(std::floor(static_cast<double>(seconds)*100.0))+1;
    return true;
}
bool TryMakeFusionLobberLaunch(const FusionLobberPoint& muzzle, const FusionLobberPoint& sample,
    const FusionLobberPoint& current, double elapsed, const FusionLobberRect* collider, FusionLobberLaunch& launch,double pixelsY,double flightSeconds)
{
    if (!Point(muzzle) || !Point(sample) || !Point(current) || !std::isfinite(elapsed) || elapsed <= 0 ||
        !std::isfinite(pixelsY)||pixelsY<=0||(collider && !Rect(*collider))) return false;
    return TryMakeFusionLobberVelocityLaunch(muzzle,current,{(current.x-sample.x)/elapsed,(current.y-sample.y)/elapsed},flightSeconds,collider,launch,pixelsY);
}
bool TryMakeFusionLobberVelocityLaunch(const FusionLobberPoint& muzzle,const FusionLobberPoint& current,
    const FusionLobberPoint& velocity,double time,const FusionLobberRect* collider,FusionLobberLaunch& launch,double pixelsY)
{
    if(!Point(muzzle)||!Point(current)||!Point(velocity)||!std::isfinite(time)||time<=0||!std::isfinite(pixelsY)||pixelsY<=0||(collider&&!Rect(*collider)))return false;
    const double predictedX = current.x + velocity.x*time;
    const double predictedY = current.y + velocity.y*time;
    const double gravity = kFusionLobberGravity*pixelsY;
    FusionLobberLaunch result;
    result.spawn = muzzle;
    result.velocityX = (predictedX-muzzle.x)/time*.01;
    result.velocityY = ((predictedY-muzzle.y)-.5*gravity*time*time)/time*.01;
    result.accelerationY = gravity*.0001;
    if (collider && std::abs(collider->x-muzzle.x) < kFusionLobberPixelsX)
        result.spawn = {collider->x,collider->y+.1*pixelsY};
    if (!Point(result.spawn) || !std::isfinite(result.velocityX) || !std::isfinite(result.velocityY) ||
        !std::isfinite(result.accelerationY) || std::abs(result.velocityX) > (std::numeric_limits<float>::max)() ||
        std::abs(result.velocityY) > (std::numeric_limits<float>::max)() ||
        result.spawn.x-20 < (std::numeric_limits<int>::min)() || result.spawn.x-20 > (std::numeric_limits<int>::max)() ||
        result.spawn.y-20 < (std::numeric_limits<int>::min)() || result.spawn.y-20 > (std::numeric_limits<int>::max)()) return false;
    launch = result;
    return true;
}
bool AdvanceFusionLobberMotion(float vx, float acceleration, float& x, float& z, float& vy)
{
    const float nx = x+vx, nz = z+vy+.5f*acceleration, nv = vy+acceleration;
    if (!std::isfinite(vx) || !std::isfinite(acceleration) || acceleration <= 0 ||
        !std::isfinite(nx) || !std::isfinite(nz) || !std::isfinite(nv)) return false;
    x = nx; z = nz; vy = nv;
    return true;
}
bool FusionLobberGroundHit(float height, float velocity,double pixelsY)
{ return std::isfinite(height) && std::isfinite(velocity)&&std::isfinite(pixelsY)&&pixelsY>0 && height > static_cast<float>(-.2*pixelsY) && velocity > 0; }
bool FusionLobberExpired(double x, int age, double minimum, double maximum)
{ return !std::isfinite(x) || !std::isfinite(minimum) || !std::isfinite(maximum) || minimum >= maximum || age < 0 || age > 2000 || x < minimum || x > maximum; }
bool FusionLobberCanAcquire(int status, bool row, bool mind, bool dying, double px, double zx, double vision)
{
    if (status < 0 || status > 25 || !row || mind || dying || !std::isfinite(px) || !std::isfinite(zx) ||
        !std::isfinite(vision) || !(px < zx && zx < vision)) return false;
    switch (status) { case 3: case 9: case 10: case 12: case 13: case 16: case 20: case 21: case 22: case 23: case 24: return false; }
    return true;
}
bool FusionLobberSplashCanAffect(int status, int row, bool mind)
{
    if (status < 0 || status > 25 || row < -1 || row > 1 || mind) return false;
    switch (status) { case 3: case 9: case 10: case 12: case 13: case 20: case 21: case 22: case 23: case 24: return false; }
    return true;
}
bool TryMakeFusionLobberRect(uint16_t kind, double x, double y, FusionLobberRect& rect,double pixelsY)
{
    const auto* row = FindFusionLobberProjectileTraits(kind);
    if (!row || !Point({x,y})||!std::isfinite(pixelsY)||pixelsY<=0) return false;
    const double width = row->boxWidth*.4*kFusionLobberPixelsX;
    const double height = row->boxHeight*.4*pixelsY;
    FusionLobberRect candidate{x+row->boxOffsetX*.4*kFusionLobberPixelsX-width*.5,
        y-row->boxOffsetY*.4*pixelsY-height*.5,width,height};
    if (!Rect(candidate)) return false;
    rect = candidate; return true;
}
FusionLobberPoint FusionLobberSplashCenter(double x, double y,double pixelsY)
{ return {x,y+.5*pixelsY}; } // Native screen Y is opposite source world Y.
bool FusionLobberAreaTouchesRect(double x, double y, const FusionLobberRect& rect,double pixelsY)
{
    if (!Point({x,y}) || !Rect(rect)||!std::isfinite(pixelsY)||pixelsY<=0) return false;
    const double dx = (std::max)({rect.x-x,0.0,x-rect.x-rect.width})/kFusionLobberPixelsX;
    const double dy = (std::max)({rect.y-y,0.0,y-rect.y-rect.height})/pixelsY;
    return dx*dx+dy*dy <= 2.25+1e-12;
}
bool TryGetFusionLobberSplashDamage(int base, int count, int& damage, bool enhanced)
{
    if (base <= 0 || count <= 0) return false;
    if (enhanced)
    {
        const float scaled = static_cast<float>(base)*.6666667f;
        if (!std::isfinite(scaled) || static_cast<double>(scaled) > (std::numeric_limits<int>::max)()) return false;
        damage = static_cast<int>(scaled); return true;
    }
    const int cap = static_cast<int>(static_cast<float>(base)*.33333334f);
    damage = static_cast<int>((std::min)((std::max)(int64_t(1),static_cast<int64_t>(base)*8/count),static_cast<int64_t>(cap)));
    return true;
}
bool TryGetFusionLobberColdDamage(int base, bool frozen, int& damage)
{
    if (base <= 0 || (frozen && base > (std::numeric_limits<int>::max)()/4)) return false;
    damage = frozen ? base*4 : base; return true;
}
bool TryGetFusionPoisonLobberDamage(uint16_t kind,int base,int level,bool secondary,int& damage)
{
    if(kind<61||kind>64||base<=0||level<0||level>((std::numeric_limits<int>::max)()-base)/(secondary?5:20))return false;
    damage=base+level*(secondary?5:20);return true;
}

}
