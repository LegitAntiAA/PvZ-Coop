#include "FusionNativeProjectileBuff369.h"
#include "FusionNativeRollingProjectile.h"
#include "FusionRollingProjectileRules.h"
#include "FusionNative.h"
#include "FusionShooterTiming.h"
#include "FusionNativeVisual.h"
#include "FusionNativeWater268.h"
#include "FusionNativeProjectileMetadata.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodParticle.h"
#include <cmath>
#include <vector>

namespace LawnFusion
{
namespace
{
bool Host(Projectile* projectile)
{
    return projectile && projectile->mApp && !projectile->mApp->NetIsClient() &&
        !projectile->mNetGhost && !projectile->mDead && IsNativeRollingProjectile(projectile);
}
bool Overlaps(const Sexy::Rect& a, const Sexy::Rect& b)
{
    return a.mX <= b.mX + b.mWidth && a.mX + a.mWidth >= b.mX &&
        a.mY <= b.mY + b.mHeight && a.mY + a.mHeight >= b.mY;
}
bool Expired(Projectile* projectile)
{
    float x = 0, y = 0;
    if (!GetNativeRollingProjectileCenter(projectile, x, y)) return true;
    Board* board = projectile->mBoard;
    const double sourceX = (x - board->GetCellCenterX(0,0)) / kFusionRollingPixelsX - 4.8;
    if(GetNativeProjectileData(projectile)->kind==6)
    {
        const double py=GetNativeFusionPixelsY(board),pitch=GetNativeFusionRowCount(board)==6?85.:100.;
        const double sourceY=2.3-(y-(board->GridToPixelY(0,0)+50))/py;
        return projectile->mProjectileAge>2000||sourceX< -12||sourceX>1.35*(board->GetColumnCount()-1)-4.8+3.85||
            sourceY<2.3-pitch/py*(GetNativeFusionRowCount(board)-1)-.6||sourceY>6.5;
    }
    const double sourceY = 2.3 - (y - (board->GridToPixelY(0,0) + 50)) / kFusionRollingPixelsY;
    return FusionRollingProjectileExpired(sourceX, sourceY, projectile->mProjectileAge, -10,
        1.35 * (board->GetColumnCount() - 1) - 4.8 + 1.85, 2.3 - 1.67 * (kNativeFusionRows - 1) - .6, 6.5);
}
}

bool IsNativeRollingProjectile(const Projectile* projectile)
{
    const auto* data = GetNativeProjectileData(projectile);
    return data && !data->sunOnHit && projectile->mProjectileType == PROJECTILE_PEA &&
        ((data->kind==2&&NativeProjectileValidationDamage369(*data)==20&&!data->specialType)||
        ((data->kind==4||data->kind==5||data->kind==6)&&NativeProjectileValidationDamage369(*data)==80&&data->specialType==1));
}

bool NativeRollingProjectileCanHit(const Projectile* projectile, Zombie* zombie)
{
    return IsNativeRollingProjectile(projectile) && zombie && zombie->mBoard == projectile->mBoard &&
        !zombie->mDead && !zombie->IsDeadOrDying() && !NativeWater268ZombieCaptured(zombie) &&
        IsNativeFusionGroundZombie(zombie) && FusionRollingProjectileCanHit(GetNativeFusionZombieStatus(zombie),
            zombie->mRow == projectile->mRow, zombie->mMindControlled, zombie->IsFlying(),
            GetNativeFusionZombieHealth(zombie));
}

bool LaunchNativeChomperBlock(Plant* plant,uint16_t kind)
{
    const auto* source=GetNativePlantData(plant);
    if(!source||source->kind!=1011||(kind!=4&&kind!=5)||!plant->mApp||plant->mApp->NetIsClient()||plant->mNetGhost||plant->mDead)return false;
    auto* b=plant->mBoard;
    if(b->mProjectiles.mSize>=b->mProjectiles.mMaxSize||!EnsureNativeProjectileVisuals(b,kind))return false;
    float x=0,y=0;if(!plant->GetShadowCenter(x,y))return false;
    x+=float(.7722*kFusionRollingPixelsX);y-=float(.54*kFusionRollingPixelsY);
    auto* p=b->AddProjectile(int(x-20),int(y-20),plant->mRenderOrder-1,plant->mRow,PROJECTILE_PEA);
    if(!p)return false;
    NativeProjectileData d;d.kind=kind;d.damage=80;d.specialType=1;
    p->mMotionType=MOTION_STRAIGHT;
    if(!AttachNativeTravelProjectile369(plant,p,d)){p->Die();return false;}
    p->mPosX=x-20;p->mPosY=y-20;p->mPosZ=0;p->mVelX=float(-6*kFusionRollingPixelsX*.01);
    p->mVelY=p->mVelZ=p->mAccZ=0;p->mShadowY=y;p->mRotation=p->mRotationSpeed=0;
    LawnNet::NetTrace("Fusion native PeaChomper fire plant=%u projectile=%u payload=%u damage=80 way=1 tick=%d",plant->mNetId,p->mNetId,kind,b->mMainCounter);
    return true;
}

void CreateNativeRollingProjectile(Plant* plant, Projectile* projectile)
{
    const auto* data = GetNativePlantData(plant);
    const auto* profile = data ? FindFusionShooterTimingProfile(data->kind) : nullptr;
    if (!profile || profile->projectileKind != 2 || !plant->mApp || plant->mApp->NetIsClient() || plant->mNetGhost ||
        !projectile || projectile->mNetGhost || projectile->mBoard != plant->mBoard) return;
    float x = 0, ground = 0;
    NativeProjectileData bullet; bullet.kind = 2;
    if (!plant->GetShadowCenter(x, ground) || !AttachNativeTravelProjectile369(plant,projectile,bullet))
    { projectile->Die(); return; }
    const float centerX = static_cast<float>(x + .785 * kFusionRollingPixelsX);
    const float landedY = static_cast<float>(ground - .1 * kFusionRollingPixelsY);
    projectile->mPosX = centerX - 20;
    projectile->mPosY = landedY - 20;
    projectile->mPosZ = static_cast<float>(-(.69888 - .1) * kFusionRollingPixelsY);
    projectile->mVelX = kFusionRollingPixelsPerTick;
    projectile->mVelY = 0;
    projectile->mVelZ = kFusionRollingStartVelocity;
    projectile->mAccZ = kFusionRollingGravityPerTick;
    projectile->mShadowY = static_cast<float>(ground + .1 * kFusionRollingPixelsY);
    projectile->mRotation = projectile->mRotationSpeed = 0;
    projectile->mMotionType = MOTION_STRAIGHT;
    projectile->mX = static_cast<int>(projectile->mPosX);
    projectile->mY = static_cast<int>(projectile->mPosY + projectile->mPosZ);
    LawnNet::NetTrace("Fusion native rolling create plant=%u id=%u kind=2 damage=20 vx=%.9g vyStart=%.9g acc=%.9g z=%.9g",
        plant->mNetId, projectile->mNetId, projectile->mVelX, projectile->mVelZ, projectile->mAccZ, projectile->mPosZ);
}

bool GetNativeRollingProjectileCenter(const Projectile* projectile, float& x, float& y)
{
    if (!IsNativeRollingProjectile(projectile)) return false;
    const float px = projectile->mPosX + 20, py = projectile->mPosY + projectile->mPosZ + 20;
    if (!std::isfinite(px) || !std::isfinite(py)) return false;
    x = px; y = py;
    return true;
}

bool GetNativeRollingProjectileRect(const Projectile* projectile, Sexy::Rect& rect)
{
    if (!IsNativeRollingProjectile(projectile)) return false;
    float x = 0, y = 0;
    FusionRollingRect bounds;
    if (!GetNativeRollingProjectileCenter(projectile, x, y) || !TryMakeFusionRollingRect(GetNativeProjectileData(projectile)->kind,x, y, bounds))
    { rect = Sexy::Rect(0,0,0,0); return true; }
    if(GetNativeProjectileData(projectile)->kind==6)
    {const double ratio=GetNativeFusionPixelsY(projectile->mBoard)/kFusionRollingPixelsY;bounds.y=y+(bounds.y-y)*ratio;bounds.height*=ratio;}
    const int left = static_cast<int>(std::floor(bounds.x)), top = static_cast<int>(std::floor(bounds.y));
    rect = Sexy::Rect(left, top, static_cast<int>(std::ceil(bounds.x + bounds.width)) - left,
        static_cast<int>(std::ceil(bounds.y + bounds.height)) - top);
    return true;
}

bool UpdateNativeRollingProjectileMotion(Projectile* projectile)
{
    if (!IsNativeRollingProjectile(projectile)) return false;
    if(Host(projectile) && GetNativeProjectileData(projectile)->kind!=2)
    {projectile->mPosX+=projectile->mVelX;return true;}
    if (Host(projectile) && !AdvanceFusionRollingMotion(
        {projectile->mVelX, projectile->mAccZ, kFusionRollingRadiansPerTick},
        projectile->mPosX, projectile->mPosZ, projectile->mVelZ, projectile->mRotation)) projectile->Die();
    return true;
}

bool HandleNativeRollingProjectileCollision(Projectile* projectile)
{
    if (!IsNativeRollingProjectile(projectile)) return false;
    if (!Host(projectile)) return true;
    if (Expired(projectile)) { projectile->Die(); return true; }
    Sexy::Rect rect;
    if (!GetNativeRollingProjectileRect(projectile, rect)) return true;
    // Visit every overlapping actor, not just FindCollisionTarget's first one.
    std::vector<ZombieID> targets;
    Zombie* zombie = nullptr;
    while (projectile->mBoard->IterateZombies(zombie))
        if (NativeRollingProjectileCanHit(projectile, zombie) && Overlaps(rect, zombie->GetZombieRect()))
            targets.push_back(projectile->mBoard->ZombieGetID(zombie));
    for (ZombieID id : targets)
        if (auto* target = projectile->mBoard->ZombieTryToGet(id)) projectile->DoImpact(target);
    return true;
}

bool ApplyNativeRollingProjectileImpact(Projectile* projectile, Zombie* zombie)
{
    if (!IsNativeRollingProjectile(projectile)) return false;
    if (!Host(projectile) || !NativeRollingProjectileCanHit(projectile, zombie)) return true;
    auto* context = GetNativeContext(projectile->mBoard);
    const uint32_t projectileId = projectile->mBoard->mProjectiles.DataArrayGetID(projectile);
    const uint32_t zombieId = static_cast<uint32_t>(projectile->mBoard->ZombieGetID(zombie));
    if (!context || !RememberFusionRollingHit(context->rollingHits, projectileId, zombieId)) return true;
    const int damage=GetNativeProjectileData(projectile)->damage;
    projectile->PlayImpactSound(zombie);
    zombie->TakeDamage(damage, 0U);
    float x = 0, y = 0;
    if (GetNativeRollingProjectileCenter(projectile, x, y))
        projectile->mApp->AddTodParticle(x, y, projectile->mRenderOrder + 1, PARTICLE_PEA_SPLAT);
    LawnNet::NetTrace("Fusion native rolling hit id=%u target=%u generation=%u damage=%d visited=%zu deadProjectile=%d",
        projectile->mNetId, zombie->mNetId, zombieId, damage, context->rollingHits[projectileId].size(), projectile->mDead);
    return true; // Source NutBullet never retires on a successful hit.
}

void RetireNativeRollingProjectile(Projectile* projectile)
{
    if (!Host(projectile)) return;
    if (auto* context = GetNativeContext(projectile->mBoard))
        context->rollingHits.erase(projectile->mBoard->mProjectiles.DataArrayGetID(projectile));
}
}
