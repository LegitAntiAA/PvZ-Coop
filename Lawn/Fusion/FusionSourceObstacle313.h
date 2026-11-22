#pragma once
#include <cstdint>
#include <cmath>
#include "../Net/NetCommon.h"
class Board;class Projectile;class Zombie;class Plant;
namespace LawnFusion
{
enum class FusionSourceObstacle313Kind:uint8_t {DrownWeapon=1,KirovBomb=2,SuperBall=3,MachineBall=4};
struct FusionSourceObstacle313View
{
    FusionSourceObstacle313Kind kind=FusionSourceObstacle313Kind::DrownWeapon;
    uint32_t instanceToken=0;
    uint16_t ownerId=0,targetPlantId=0;
    uint32_t ownerToken=0,targetPlantToken=0;
    bool landed=false,hit=false,fromHypno=false,exploded=false;
    uint8_t variant=0,hitCount=0;
    uint32_t lastHitAge=0; // Source .3s flashTime, relative to raw native age.
    int32_t ageTicks=0; // Snapshot mirror of actual Projectile.mProjectileAge.
    float velocityX=0,velocityY=0,velocityZ=0,accelerationZ=0;
    LawnNet::NetZombieAnimState clock; // Genuine MachineBall Animator only.
};
inline bool FusionSourceObstacle313ControllerValid(const FusionSourceObstacle313View& d)
{
    if(!d.instanceToken||!d.ownerId||!d.ownerToken||bool(d.targetPlantId)!=bool(d.targetPlantToken))return false;
    if(d.kind==FusionSourceObstacle313Kind::DrownWeapon)return !d.fromHypno&&!d.exploded&&!d.variant&&!d.hitCount&&!d.lastHitAge;
    if(d.kind==FusionSourceObstacle313Kind::KirovBomb)return !d.targetPlantId&&!d.targetPlantToken&&!d.landed&&!d.hit&&!d.variant&&!d.hitCount&&!d.lastHitAge;
    if(d.kind==FusionSourceObstacle313Kind::SuperBall)return !d.targetPlantId&&!d.targetPlantToken&&!d.landed&&!d.hit&&!d.fromHypno&&d.variant<=1&&!d.hitCount&&!d.lastHitAge;
    if(d.kind==FusionSourceObstacle313Kind::MachineBall)return !d.targetPlantId&&!d.targetPlantToken&&!d.landed&&!d.hit&&!d.fromHypno&&d.variant<=1&&d.hitCount<=31&&(!d.hitCount?d.lastHitAge==0:true);
    return false;
}
inline bool FusionSourceObstacle313ViewValid(const FusionSourceObstacle313View& d)
{
    if(!FusionSourceObstacle313ControllerValid(d)||d.ageTicks<0||d.lastHitAge>uint32_t(d.ageTicks)||
       !std::isfinite(d.velocityX)||!std::isfinite(d.velocityY)||!std::isfinite(d.velocityZ)||!std::isfinite(d.accelerationZ)||
       std::abs(d.velocityX)>100000||std::abs(d.velocityY)>100000||std::abs(d.velocityZ)>100000||std::abs(d.accelerationZ)>100000)return false;const auto& c=d.clock;
    if(!std::isfinite(c.mAnimTime)||c.mAnimTime<0||c.mAnimTime>1||!std::isfinite(c.mAnimRate)||std::abs(c.mAnimRate)>10000||c.mLoopType>5||!LawnNet::NetZombieAnimStateFitsFrames(c,65535))return false;
    if(!c.mPresent&&(c.mDead||c.mFrameStart||c.mFrameCount||c.mAnimTime||c.mAnimRate||c.mLoopType||c.mLoopCount))return false;
    return d.kind==FusionSourceObstacle313Kind::MachineBall?c.mPresent&&!c.mDead:!c.mPresent;
}
struct FusionSourceObstacle313Spawn
{
    FusionSourceObstacle313Kind kind=FusionSourceObstacle313Kind::DrownWeapon;
    uint16_t ownerId=0,targetPlantId=0;
    uint32_t ownerToken=0,targetPlantToken=0;
    int row=0;
    float centerX=0,centerY=0;
    float groundY=0; // New2/3/4: copied into raw Projectile.mShadowY, no second position cache.
    int renderOrder=0; // Source factory sorting: Kirov row+1, Super owner+1, Machine owner exactly.
    bool fromHypno=false;
    uint8_t variant=0; // SuperBall0=43/1=212; MachineBall0=fire/1=ice.
};
using CanSpawnSourceObstacle313=bool(*)(Board*,const FusionSourceObstacle313Spawn&);
using SpawnSourceObstacle313=Projectile*(*)(Board*,const FusionSourceObstacle313Spawn&);
}
