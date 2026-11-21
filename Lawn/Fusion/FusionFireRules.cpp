#include "FusionFireRules.h"
#include "FusionStraightProjectileRules.h"
#include "FusionLobberRules.h"
#include "FusionBatchActionRules.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace LawnFusion
{
namespace
{
bool StampValid(uint16_t id, uint32_t token) { return (!id && !token) || (id && token); }
}
bool FusionFireProjectileMetadataValid(uint16_t kind, int damage, int sun, uint8_t fire,
    uint16_t id, uint32_t token, ProjectileType nativeType,bool smallProjectile)
{
    if (!StampValid(id,token) || fire > 3 ||
        (id && kind != 0 && kind != 1 && kind != 10 && kind != 15 && kind != 97)) return false;
    // An already hot returning Pea can have an empty stamp after its bounce.
    if(smallProjectile&&(kind!=0||!fire))return false;
    if(id&&!fire&&kind!=0&&kind!=15&&kind!=97)return false;
    if (fire)
    {
        // Source SmallSun only quadruples damage/rewards25. Its HitZombie does
        // not call FireZombie, despite the source isHot marker.
        if (kind == 8) return fire == 1 && damage == 400 && sun == 25 &&
            !id && nativeType == PROJECTILE_PEA;
        if (sun || nativeType != PROJECTILE_FIREBALL) return false;
        if (kind == 1) return fire == 1 && damage == 120;
        if (kind == 10) return fire == 1 && damage == 40;
        if (kind != 0) return false;
        if(smallProjectile)
        {
            if(fire==1)return damage==30;
            if(fire==2)return damage==55;
            return damage==70||damage==95;
        }
        if (fire == 1) return damage == 40;
        if (fire == 2) return damage == 60 || damage == 80;
        return damage == 80 || damage == 100 || damage == 120 || damage == 140;
    }
    return FusionLobberProjectileMetadataValid(kind,damage,sun,nativeType) ||
        FusionStraightProjectileMetadataValid(kind,nativeType,damage,sun) ||
        (kind == 2 && damage == 20 && sun == 0 && nativeType == PROJECTILE_PEA) ||
        (kind == 8 && damage == 100 && (sun == 0 || sun == 5) && nativeType == PROJECTILE_PEA) ||
        (kind == 9 && damage == 20 && sun == 0 && nativeType == PROJECTILE_PUFF);
}
bool TryResolveFusionTorchConversion(uint16_t torchKind, uint16_t kind, int damage,
    int sun, uint8_t fire, uint8_t special, bool sameRow, bool hostile,
    ProjectileMotion motion, uint16_t lastId, uint32_t lastToken,
    uint16_t id, uint32_t token, FusionTorchConversion& output,bool smallProjectile)
{
    const bool three = motion == MOTION_THREEPEATER &&
        (special == kFusionThreeUpper || special == kFusionThreeLower);
    const bool speedTier=kind==0&&motion==MOTION_STRAIGHT&&special>=16&&special<=18;
    const bool rage=(kind==0||kind==15||kind==97)&&motion==MOTION_STAR&&special==15;
    const bool kelpWay2=torchKind==1056&&motion==MOTION_STAR&&special==2&&(kind==0||kind==10||kind==15);
    if (!id || !token || !StampValid(lastId,lastToken) || fire > 3 || hostile ||
        (lastId == id && lastToken == token) ||
        (motion != MOTION_STRAIGHT && motion != MOTION_BACKWARDS && !three&&!rage&&!kelpWay2)) return false;
    const bool ordinary = kind == 0 && sun == 0 &&
        (smallProjectile?((fire==1&&damage==30)||(fire==2&&damage==55)||
            (fire==3&&(damage==70||damage==95))):
        ((!fire && damage == 20) || (fire == 1 && damage == 40) ||
         (fire == 2 && (damage == 60 || damage == 80)) ||
         (fire == 3 && (damage == 80 || damage == 100 || damage == 120 || damage == 140))));
    if(smallProjectile&&!ordinary)return false;
    if(smallProjectile&&special!=0&&special!=kFusionThreeUpper&&special!=kFusionThreeLower)return false;
    if ((special > 4&&!speedTier&&!rage) || (three && kind != 0 && kind != 10 && kind != 15 && kind != 97)) return false;
    FusionTorchConversion plan;
    plan.kind = kind; plan.damage = damage; plan.sunOnHit = sun; plan.fireState = fire;plan.smallProjectile=smallProjectile;
    if (torchKind == 1055)
    {
        // ThreeTorch does not call CheckFire: physical contact may occur while
        // an adjacent-row projectile is still crossing the middle lane.
        if (!ordinary || fire) return false;
        plan.action = FusionTorchConversionAction::ThreeYellow;
        plan.kind = 0; plan.damage = 30; plan.replace = true;plan.smallProjectile=true;
        plan.fireState = 1; plan.emittedCount = 3;
    }
    else
    {
        if (!sameRow&&!rage&&!kelpWay2) return false; // KelpTorch470180 also exempts actual source way2.
        if (torchKind == 1056)
        {
            // KelpTorch invokes the original YellowFirePea/FirePuffPea/
            // FireSnowPea callbacks; its source kind and stamp remain1056.
            if(ordinary&&!fire)
            {plan.action=FusionTorchConversionAction::Yellow;plan.damage=40;plan.fireState=1;}
            else if(kind==10&&damage==20&&!sun&&!fire)
            {plan.action=FusionTorchConversionAction::Puff;plan.damage=40;plan.fireState=1;}
            else if(kind==15&&damage==20&&!sun&&!fire)
            {plan.action=FusionTorchConversionAction::RemoveHeat;plan.kind=0;plan.replace=true;}
            else return false;
        }
        else if (torchKind == 1182)
        {
            // IceTorch explicitly ignores Bullet.isFromThreeTorch before
            // either the cold conversion or the hot-to-ordinary branch.
            if(smallProjectile)return false;
            if (ordinary)
            {
                plan.action = fire ? FusionTorchConversionAction::RemoveHeat : FusionTorchConversionAction::Snow;
                plan.kind = fire ? 0 : 15; plan.damage = fire ? 20 : damage;
            }
            else if (kind == 15 && damage == 20 && !sun && !fire)
            {
                plan.action = FusionTorchConversionAction::ExtremeSnow;
                plan.kind = 97;
            }
            else return false;
            plan.fireState = 0; plan.replace = true;plan.smallProjectile=false;
        }
        else if (torchKind == 1061)
        {
            if (kind != 10 || damage != 20 || sun || fire) return false;
            plan.action = FusionTorchConversionAction::Puff;
            plan.damage = 40; plan.fireState = 1;
        }
        else if (torchKind == 1052 || torchKind == 1053 || torchKind == 902)
        {
            if (ordinary)
            {
                const uint8_t level = torchKind == 1052 || torchKind == 902 ? 3 : 2;
                if (fire >= level) return false;
                plan.action = level == 3 ? FusionTorchConversionAction::Red : FusionTorchConversionAction::Orange;
                plan.damage += level == 3 ? (smallProjectile?40:60) : (smallProjectile?25:40); plan.fireState = level;
            }
            else if (kind == 15 && damage == 20 && !sun && !fire)
            {
                plan.action = FusionTorchConversionAction::RemoveHeat;
                plan.kind = 0; plan.replace = true;
            }
            else if ((torchKind == 1052 || torchKind == 902) && kind == 1 && damage == 60 && !sun && !fire)
            {
                plan.action = FusionTorchConversionAction::Cherry;
                plan.damage = 120; plan.fireState = 1;
            }
            else if (kind == 8 && damage == 100 && (sun == 0 || sun == 5) && !fire && special != 3)
            {
                plan.action = FusionTorchConversionAction::Sun;
                plan.damage = 400; plan.sunOnHit = 25; plan.fireState = 1;
                plan.stamp = false;
            }
            else return false;
        }
        else if (torchKind == 18 || torchKind == 1059 || torchKind == 1178)
        {
            if (ordinary && !fire)
            {
                plan.action = FusionTorchConversionAction::Yellow;
                plan.damage = 40; plan.fireState = 1;
            }
            else if (kind == 15 && damage == 20 && !sun && !fire)
            {
                plan.action = FusionTorchConversionAction::RemoveHeat;
                plan.kind = 0; plan.replace = true;
            }
            else return false;
        }
        else return false;
    }
    output = plan;
    return true;
}
FusionTorchwoodAction ResolveFusionTorchwoodAction(uint16_t kind, uint8_t fire, bool sameRow,
    bool zombieBullet, ProjectileMotion motion, uint16_t lastId, uint32_t lastToken,
    uint16_t id, uint32_t token, uint8_t specialType)
{
    const bool three = kind == 0 && motion == MOTION_THREEPEATER &&
        (specialType == kFusionThreeUpper || specialType == kFusionThreeLower);
    if (!id || !token || !StampValid(lastId,lastToken) || fire > 1 || !sameRow || zombieBullet ||
        (motion != MOTION_STRAIGHT && motion != MOTION_BACKWARDS && !three) || (lastId == id && lastToken == token))
        return FusionTorchwoodAction::None;
    if (kind == 0 && !fire) return FusionTorchwoodAction::YellowPea;
    if (kind == 15 && !fire) return FusionTorchwoodAction::MeltSnow;
    return FusionTorchwoodAction::None; // Source7 and advanced/multiline Torch variants are not enabled.
}
bool TryGetFusionYellowFireDamage(int damage, uint8_t fire, int& result)
{
    if (fire || damage < 0 || damage > (std::numeric_limits<int>::max)()-20) return false;
    result = damage+20; // Source addition, not multiplication.
    return true;
}
bool TryGetFusionFireSplashDamage(int damage, int count, int& result)
{
    if (damage < 0 || count <= 0) return false;
    const float cap = static_cast<float>(damage)*.33333334f;
    if (!std::isfinite(cap) || cap > (std::numeric_limits<int>::max)()) return false;
    result = (std::min)((std::max)(1,damage/count),static_cast<int>(cap));
    return true;
}
bool TryGetFusionFireSplashDamage(uint16_t kind, int damage, int count, int& result)
{
    if (kind == 1)
    {
        if (damage < 0 || count <= 0) return false;
        result = damage; // FireOtherZombie explicitly exempts Cherry from division/cap.
        return true;
    }
    return TryGetFusionFireSplashDamage(damage,count,result);
}
bool FusionFireAllowsSputter(int status, bool blockedArmor)
{
    if (status < 0 || blockedArmor) return false;
    switch (status)
    {
    case 7: case 10: case 12: case 20: case 22: case 23: case 24: return false;
    default: return true;
    }
}
bool FusionFireAreaTouchesRect(double x, double y, const FusionFireRect& rect)
{
    if (!std::isfinite(x+y+rect.x+rect.y+rect.width+rect.height) || rect.width < 0 || rect.height < 0) return false;
    // A source circle becomes an ellipse under the existing physical projection.
    const double nearestX = (std::clamp)(x,rect.x,rect.x+rect.width);
    const double nearestY = (std::clamp)(y,rect.y,rect.y+rect.height);
    const double dx = (x-nearestX)/(72.0/1.35), dy = (y-nearestY)/(100.0/1.67);
    return dx*dx+dy*dy <= 1.0;
}
bool TryWarmFusionColdState(const FusionColdState& before, FusionColdState& after)
{
    if (!FusionColdStateValid(before.level,before.threshold,before.chilledTicks,before.frozenTicks)) return false;
    after = {0,before.threshold,0,0};
    return true;
}
}
