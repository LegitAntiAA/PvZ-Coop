#ifndef LAWN_FUSION_NATIVE_PROJECTILE_METADATA_H
#define LAWN_FUSION_NATIVE_PROJECTILE_METADATA_H
#include "FusionNative.h"
#include "FusionBatchActionRules.h"
#include "FusionFireRules.h"
#include "FusionLobberRules.h"
#include "FusionSplitShooterRules.h"
#include "FusionStraightProjectileRules.h"
#include "FusionRecipientShooterRules.h"
#include "FusionShooterBatchRules.h"
#include "FusionFireActionBatchRules.h"
#include "FusionShooter267Rules.h"
#include "FusionWater268Rules.h"
#include "FusionCannon268Rules.h"
#include "FusionShulk268Rules.h"
#include "FusionSummon269Rules.h"
#include "FusionSummonPlants269Rules.h"
#include "FusionRemainingLeaves269Rules.h"
#include "FusionMorePlants269BackendRules.h"
#include "FusionLastPlants269Rules.h"
#include "FusionHostileProjectileRules.h"
#include <limits>

namespace LawnFusion
{
inline bool NativeMore269MetadataValid(const NativeProjectileData& d,ProjectileType type,ProjectileMotion motion)
{return !d.sunOnHit&&!d.effectVariant&&!d.melonEnhanced&&!d.isZombieBullet&&!d.hitLatched&&!d.umbrellaed&&!d.goldAttracted&&
 FusionMore269ProjectileMetadataValid(d.kind,d.damage,d.fireState,d.specialType,type,motion,d.lastTorchId,d.lastTorchToken);}
inline bool NativeWater268Metadata(const NativeProjectileData& d)
{return d.kind==20||d.kind==29||d.kind==32||d.kind==34||d.kind==35||d.kind==41||d.kind==42||(d.kind==37&&(d.specialType==6||d.specialType==7));}
inline bool NativeCannon268Metadata(const NativeProjectileData& d)
{return IsFusionCannon268Way14(d.kind)||FindFusionCannon268ProjectileProfile(d.kind)||d.kind==85;}
inline bool NativeNew268ProjectileExtrasClear(const NativeProjectileData& d)
{return !d.sunOnHit&&!d.fireState&&!d.lastTorchId&&!d.lastTorchToken&&!d.melonEnhanced&&!d.effectVariant;}
inline ProjectileType NativeProjectileMetadataType(const NativeProjectileData& data)
{
    if(data.kind==91)return PROJECTILE_SPIKE;
    if(data.kind==30)return PROJECTILE_PEA;
    if(FusionStar269ProjectileClass(data.kind))return PROJECTILE_STAR;
    if(NativeWater268Metadata(data))return data.kind==34||data.kind==35||data.kind==37?PROJECTILE_SPIKE:(data.kind==41||data.kind==42)?PROJECTILE_STAR:PROJECTILE_PEA;
    if(IsFusionCannon268Way14(data.kind))return PROJECTILE_COBBIG;
    if(data.kind==85)return PROJECTILE_KERNEL;
    if(const auto* p=FindFusionCannon268ProjectileProfile(data.kind))return p->nativeType;
    if(data.kind==6||data.kind==89||data.kind==94)return PROJECTILE_PEA;
    if(data.kind==58&&data.specialType==13)return PROJECTILE_BASKETBALL;
    if(data.kind==47)return PROJECTILE_SPIKE;
    if(data.kind==87)return PROJECTILE_FIREBALL;
    const auto* straight = FindFusionStraightProjectileProfile(data.kind);
    const auto* lobber = FindFusionLobberProjectileProfile(data.kind);
    return data.kind == 37 || data.kind == 38 || data.kind == 100 || data.kind == 102 ? PROJECTILE_SPIKE :
        data.kind == 39 || data.kind == 40 || data.kind == 46 ? PROJECTILE_STAR :
        data.fireState && data.kind != 8 ? PROJECTILE_FIREBALL : straight ? straight->nativeType :
        lobber ? lobber->nativeType : data.kind == 9 || data.kind == 13 || data.kind == 14 ? PROJECTILE_PUFF : PROJECTILE_PEA;
}

inline ProjectileMotion NativeProjectileMetadataMotion(const NativeProjectileData& data)
{
    if(data.kind==3&&data.specialType==9)return MOTION_BACKWARDS;
    if(data.kind==91)return data.specialType==6?MOTION_HOMING:MOTION_STRAIGHT;
    if(data.kind==30)return data.specialType==2?MOTION_STAR:MOTION_STRAIGHT;
    if((data.kind==0||data.kind==10)&&data.specialType==2&&data.lastTorchId&&data.lastTorchToken)return MOTION_STAR;
    // Typed frames carry source way, while the raw native body carries its
    // physical axis. Preserve the existing Lobber representative when source
    // Butter40/way0 overlaps Hamburger's real planar Butter40/way0.
    const auto moreType=FusionMore269ProjectileType(data.kind,data.fireState);
    const auto moreMotion=FusionMore269ProjectileMotion(data.kind);
    if(NativeMore269MetadataValid(data,moreType,moreMotion)&&
       !FusionLobberProjectileMetadataValid(data.kind,data.damage,data.sunOnHit,moreType))return moreMotion;
    if(data.kind==51)return MOTION_STAR;
    if(data.kind==8&&data.specialType==16&&data.effectVariant==3)return MOTION_HOMING;
    if(data.kind==37&&data.specialType==12)return MOTION_STAR;
    if(NativeWater268Metadata(data))return data.specialType==6?MOTION_HOMING:
        data.kind==41||data.kind==42||data.specialType==7?MOTION_STAR:data.specialType==255?MOTION_LOBBED:
        data.specialType==4||data.specialType==5?MOTION_THREEPEATER:MOTION_STRAIGHT;
    if(NativeCannon268Metadata(data))return data.kind==85?MOTION_STRAIGHT:MOTION_LOBBED;
    if(data.kind==6||data.kind==89)return MOTION_STRAIGHT;
    if(data.kind==94)return data.specialType==16?MOTION_HOMING:MOTION_STAR;
    if((data.kind==58||data.kind==59)&&data.specialType==13)return MOTION_LOBBED;
    if((data.kind==28||data.kind==36)&&data.specialType==255)return MOTION_LOBBED;
    if(data.kind==7&&data.specialType==2)return MOTION_STAR;
    if((data.kind==28||data.kind==36||data.kind==87||data.kind==98||data.kind==99)&&
        (data.specialType==4||data.specialType==5))return MOTION_THREEPEATER;
    if (data.kind == 4 || data.kind == 5) return MOTION_STRAIGHT;
    if (data.kind == 39 || data.kind == 40 || data.kind == 46 || ((data.kind == 100 || data.kind == 102) && data.specialType == 18) ||
        ((data.kind == 0 || data.kind == 15 || data.kind == 97) && data.specialType == 15)) return MOTION_STAR;
    if (data.kind == 92) return MOTION_PUFF;
    if (data.kind == 28 && data.specialType == 1) return MOTION_LOBBED;
    if (data.kind == 12 && (data.specialType == kFusionThreeUpper || data.specialType == kFusionThreeLower)) return MOTION_THREEPEATER;
    if ((data.kind == 0 || data.kind == 15 || data.kind == 97) &&
        (data.specialType == kFusionThreeUpper || data.specialType == kFusionThreeLower))
        return MOTION_THREEPEATER;
    if (FindFusionLobberProjectileProfile(data.kind)) return MOTION_LOBBED;
    return data.specialType == 1 ? MOTION_BACKWARDS : MOTION_STRAIGHT;
}

// Shared admission contract for native allocation, pure metadata and body preflight.
// The frame has no physics copy; its canonical motion is refined by the raw body.
inline bool NativeProjectileMetadataValidCore(const NativeProjectileData& data,
    ProjectileType type, ProjectileMotion motion)
{
    if(NativeMore269MetadataValid(data,type,motion))return true;
    if(data.kind==14)return FusionHypno269ProjectileMetadataValid(data.kind,data.damage,data.specialType)&&
        !data.sunOnHit&&!data.fireState&&!data.lastTorchId&&!data.lastTorchToken&&!data.melonEnhanced&&!data.effectVariant&&
        type==PROJECTILE_PUFF&&motion==MOTION_STRAIGHT;
    if((data.kind==75||data.kind==53)&&data.damage==300&&data.specialType==13)
        return data.sunOnHit==(data.kind==53?5:0)&&!data.fireState&&!data.lastTorchId&&!data.lastTorchToken&&
            !data.melonEnhanced&&!data.effectVariant&&type==PROJECTILE_CABBAGE&&motion==MOTION_LOBBED;
    if(NativeWater268Metadata(data))return NativeNew268ProjectileExtrasClear(data)&&
        FusionWater268ProjectileMetadataValid(data.kind,data.damage,data.specialType)&&
        type==NativeProjectileMetadataType(data)&&motion==NativeProjectileMetadataMotion(data);
    if(NativeCannon268Metadata(data))return !data.fireState&&!data.lastTorchId&&!data.lastTorchToken&&
        FusionCannon268ProjectileMetadataValid(data.kind,data.damage,data.sunOnHit,data.specialType,data.effectVariant,data.melonEnhanced,type,motion);
    if(data.kind==89||data.kind==94)return NativeNew268ProjectileExtrasClear(data)&&
        type==PROJECTILE_PEA&&FusionChomper268ProjectileMetadataValid(data.kind,data.damage,data.specialType)&&
        (data.kind==89?motion==MOTION_STRAIGHT:data.specialType==16?motion==MOTION_HOMING:motion==MOTION_STAR);
    if (data.kind==4 || data.kind==5 || data.kind==6)
        return data.damage==80 && !data.sunOnHit && data.specialType==1 && !data.fireState &&
            !data.lastTorchId && !data.lastTorchToken && !data.melonEnhanced && !data.effectVariant &&
            type==PROJECTILE_PEA && motion==MOTION_STRAIGHT;
    if ((data.kind>=61&&data.kind<=64)||data.kind==71||data.kind==75||data.kind==86)
    {
        if(data.specialType||data.fireState||data.lastTorchId||data.lastTorchToken||
            (data.melonEnhanced&&!(data.kind>=61&&data.kind<=64)&&data.kind!=71&&data.kind!=86)||
            (data.kind==71?data.effectVariant>1:data.kind==75?
                (data.effectVariant>1||data.damage!=(data.effectVariant?160:80)):data.effectVariant!=0))return false;
        return motion==MOTION_LOBBED&&FusionLobberProjectileMetadataValid(data.kind,data.damage,data.sunOnHit,type);
    }
    if (data.kind == 12 || data.kind == 28)
        return !data.lastTorchId && !data.lastTorchToken && !data.melonEnhanced && !data.effectVariant &&
            FusionFireActionProjectileMetadataValid(data.kind,data.damage,data.sunOnHit,data.specialType,data.fireState,type,motion);
    const bool batchShot = data.kind == 3 || data.kind == 23 || data.kind == 38 || data.kind == 40 || data.kind == 100 ||
        (data.kind==39&&data.damage==40) || data.kind == 11 || data.kind == 90 || data.kind == 46 || data.kind == 92 ||
        data.kind == 95 || data.kind == 102 || ((data.kind == 0 || data.kind == 15 || data.kind == 97) && data.specialType >= 15);
    if (batchShot)
    {
        if (data.melonEnhanced || data.effectVariant > 1 || (data.effectVariant &&
            data.kind!=23&&data.kind!=38&&data.kind!=40&&(data.kind != 0 || !data.fireState))) return false;
        if(data.kind==23&&data.damage!=(data.effectVariant?1800:300))return false;
        if (data.fireState)
            return data.kind == 0 && type == PROJECTILE_FIREBALL &&
                FusionShooterBatchProjectileMetadataValid(data.kind,20,0,data.specialType,PROJECTILE_PEA,motion) &&
                FusionFireProjectileMetadataValid(data.kind,data.damage,data.sunOnHit,data.fireState,
                    data.lastTorchId,data.lastTorchToken,type,data.effectVariant == 1);
        return ((!data.lastTorchId && !data.lastTorchToken) ||
            ((data.kind == 15 || data.kind == 97) && data.lastTorchId && data.lastTorchToken)) &&
            FusionShooterBatchProjectileMetadataValid(data.kind,data.damage,data.sunOnHit,data.specialType,type,motion);
    }
    if ((data.kind == 15 || data.kind == 97) &&
        (data.specialType == kFusionThreeUpper || data.specialType == kFusionThreeLower))
        return !data.fireState && data.lastTorchId && data.lastTorchToken && !data.melonEnhanced && !data.effectVariant &&
            motion == MOTION_THREEPEATER && FusionFireProjectileMetadataValid(data.kind,data.damage,data.sunOnHit,0,
                data.lastTorchId,data.lastTorchToken,type);
    if (data.kind == 37 || data.kind == 39)
        return !data.fireState && !data.lastTorchId && !data.lastTorchToken && !data.melonEnhanced && !data.effectVariant &&
            FusionRecipientProjectileMetadataValid(data.kind,data.damage,data.sunOnHit,data.specialType,type,motion);
    if (data.melonEnhanced && (data.kind != 59 && data.kind != 60 && data.kind != 66 &&
        data.kind != 67 && data.kind != 70 && data.kind != 81 && data.kind != 82)) return false;
    if (data.kind == 13 ? data.effectVariant > 6 :
        data.kind == 0 && data.fireState ? data.effectVariant > 1 : data.effectVariant != 0) return false;
    const bool batch = data.kind == 13 || (data.kind == 9 && motion == MOTION_STRAIGHT) ||
        (data.kind == 0 && (data.specialType == kFusionThreeUpper || data.specialType == kFusionThreeLower));
    if (batch && data.kind == 0 && data.fireState)
        return !data.melonEnhanced && motion == MOTION_THREEPEATER &&
            FusionFireProjectileMetadataValid(data.kind,data.damage,data.sunOnHit,data.fireState,
                data.lastTorchId,data.lastTorchToken,type,data.effectVariant == 1);
    if (batch)
        return !data.fireState && !data.lastTorchId && !data.lastTorchToken && !data.melonEnhanced &&
            type == (data.kind == 0 ? PROJECTILE_PEA : PROJECTILE_PUFF) &&
            FusionBatchProjectileMetadataValid(data.kind,data.damage,data.sunOnHit,data.specialType,data.effectVariant,motion);
    if (data.kind == 9 && motion != MOTION_PUFF) return false;
    if ((data.kind == 2 || data.kind == 8) && motion != MOTION_STRAIGHT) return false;
    if (!FusionFireProjectileMetadataValid(data.kind,data.damage,data.sunOnHit,data.fireState,
        data.lastTorchId,data.lastTorchToken,type,data.kind == 0 && data.effectVariant == 1)) return false;
    if (FindFusionStraightProjectileProfile(data.kind))
        return FusionSplitProjectilePhaseValid(data.kind,data.specialType,motion);
    return !data.specialType && (!FindFusionLobberProjectileProfile(data.kind) || motion == MOTION_LOBBED);
}
inline bool NativeStar269MetadataValid(const NativeProjectileData& d,ProjectileType type,ProjectileMotion motion,
    const NativeStar269ProjectileState* state=nullptr)
{
    const int maximum=d.kind==49?400:120;
    if(!FusionStar269ProjectileClass(d.kind)||type!=PROJECTILE_STAR||d.damage<0||d.damage>(state&&state->accelerated?maximum*3:maximum)||
        d.sunOnHit||d.fireState||d.lastTorchId||d.lastTorchToken||d.melonEnhanced||d.isZombieBullet||d.hitLatched||d.umbrellaed||
        (d.effectVariant&&(d.kind==48||d.kind==49?d.effectVariant!=4:d.kind!=40||d.effectVariant!=1)))return false;
    if(state)
        return NativeStar269ProjectileStateValid(d.kind,d.specialType,*state)&&
            motion==(d.specialType==6?MOTION_HOMING:MOTION_STAR)&&
            (d.specialType==0||d.specialType==1||d.specialType==2||d.specialType==9||d.specialType==6||d.specialType==10);
    return motion==MOTION_STAR&&(d.specialType==0||d.specialType==1||d.specialType==2||d.specialType==9);
}
inline bool NativeProjectileMetadataValid(const NativeProjectileData& data,
    ProjectileType type,ProjectileMotion motion,const NativeStar269ProjectileState* star=nullptr)
{
    if(data.kind==91)return type==PROJECTILE_SPIKE&&motion==(data.specialType==6?MOTION_HOMING:MOTION_STRAIGHT)&&
        (data.specialType==0||data.specialType==6||data.specialType==10)&&
        (data.damage==160||data.damage==480||data.damage==800)&&
        NativeNew268ProjectileExtrasClear(data)&&!data.isZombieBullet&&!data.hitLatched&&!data.umbrellaed&&!data.goldAttracted&&!star;
    if(data.kind==30)return FusionLastPlants269KelpProjectileValid(data.kind,data.damage,data.specialType)&&
        type==PROJECTILE_PEA&&motion==(data.specialType==2?MOTION_STAR:MOTION_STRAIGHT)&&
        data.fireState==1&&data.lastTorchId&&data.lastTorchToken&&!data.sunOnHit&&!data.melonEnhanced&&
        !data.effectVariant&&!data.isZombieBullet&&!data.hitLatched&&!data.umbrellaed&&!data.goldAttracted&&!star;
    // Source KelpTorch's way2 keeps its actual planar STAR body and source stamp.
    if(data.specialType==2&&motion==MOTION_STAR&&data.lastTorchId&&data.lastTorchToken&&
        !data.sunOnHit&&!data.melonEnhanced&&!data.effectVariant&&!data.isZombieBullet&&!data.hitLatched&&!data.umbrellaed&&!data.goldAttracted&&!star&&
        ((data.kind==0&&((!data.fireState&&data.damage==20&&type==PROJECTILE_PEA)||(data.fireState==1&&data.damage==40&&type==PROJECTILE_FIREBALL)))||
         (data.kind==10&&data.fireState==1&&data.damage==40&&type==PROJECTILE_FIREBALL)))return true;
    if(data.kind==51||(data.kind==8&&data.specialType==16&&data.effectVariant==3))
        return FusionRemainingLeaves269ProjectileValid(data.kind,data.damage,data.specialType,data.effectVariant,type,motion)&&
            !data.goldAttracted&&!data.isZombieBullet&&!data.sunOnHit&&!data.fireState&&!data.lastTorchId&&!data.lastTorchToken&&
            !data.melonEnhanced&&!data.umbrellaed&&!data.hitLatched;
    if(data.goldAttracted)
    {
        auto original=data;original.goldAttracted=false;
        if(star&&(data.specialType==6||data.specialType==10))
            return NativeProjectileMetadataValid(original,type,motion,star);
        if((data.specialType!=4&&data.specialType!=5)||motion!=MOTION_THREEPEATER||!FusionGoldMagnet269Planar(type,motion))return false;
        original.specialType=0;
        if(star)return NativeStar269MetadataValid(original,type,MOTION_STAR,star);
        return NativeProjectileMetadataValid(original,type,NativeProjectileMetadataMotion(original))||
            (original.kind==9&&NativeProjectileMetadataValid(original,type,MOTION_PUFF));
    }
    if(star)return NativeStar269MetadataValid(data,type,motion,star);
    if(data.kind==3)return NativeNew268ProjectileExtrasClear(data)&&(!data.hitLatched||data.isZombieBullet)&&!data.umbrellaed&&!data.goldAttracted&&
        FusionSuperCherryProjectileMetadataValid(data.kind,data.damage,data.specialType,data.isZombieBullet,type,motion);
    if(data.kind==37&&(data.specialType==8||data.specialType==12))return type==PROJECTILE_SPIKE&&motion==(data.specialType==12?MOTION_STAR:MOTION_STRAIGHT)&&data.damage==20&&
        NativeNew268ProjectileExtrasClear(data)&&!data.isZombieBullet&&!data.hitLatched&&!data.umbrellaed;
    if(data.kind==58)
        return data.specialType==13&&data.damage==60&&data.isZombieBullet&&!data.umbrellaed&&
            !data.sunOnHit&&!data.fireState&&!data.lastTorchId&&!data.lastTorchToken&&
            !data.melonEnhanced&&!data.effectVariant&&type==PROJECTILE_BASKETBALL&&motion==MOTION_LOBBED;
    if(data.kind==59&&data.specialType==13)
        return !data.isZombieBullet&&!data.sunOnHit&&!data.fireState&&!data.lastTorchId&&!data.lastTorchToken&&
            !data.melonEnhanced&&!data.effectVariant&&type==PROJECTILE_MELON&&motion==MOTION_LOBBED&&
            (data.umbrellaed?(data.damage==140||data.damage==280):(data.damage==80||data.damage==160));
    if(data.isZombieBullet||data.hitLatched)return false;
    if(data.umbrellaed)
    {
        if(!FindFusionLobberProjectileProfile(data.kind)||motion!=MOTION_LOBBED||data.damage<0)return false;
        // Source int damage truncates after the one-shot float multiplier1.75.
        // Reconstruct its only possible integer input, then validate the full
        // original family contract. Unrelated kinds cannot claim this flag.
        const int64_t base=(static_cast<int64_t>(data.damage)*4+6)/7;
        if(base>(std::numeric_limits<int>::max)()||base*7/4!=data.damage)return false;
        auto original=data;original.damage=static_cast<int>(base);original.umbrellaed=false;
        return NativeProjectileMetadataValidCore(original,type,motion);
    }
    const bool newShot=data.kind==7||data.kind==36||data.kind==47||data.kind==87||data.kind==93||
        data.kind==98||data.kind==99||(data.kind==11&&data.effectVariant==1)||
        (data.kind==28&&data.specialType!=1);
    if(newShot)
        return !data.sunOnHit&&!data.melonEnhanced&&((data.lastTorchId==0)==(data.lastTorchToken==0))&&
            FusionShooter267ProjectileMetadataValid(data.kind,data.damage,data.specialType,data.fireState,
                data.effectVariant,type,motion);
    return NativeProjectileMetadataValidCore(data,type,motion);
}
}
#endif
