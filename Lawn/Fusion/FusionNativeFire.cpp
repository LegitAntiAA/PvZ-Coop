#include "FusionNativeTravelConsumers369.h"
#include <limits>
#include "FusionNativeProjectileBuff369.h"
#include "FusionNativeFire.h"
#include "FusionNativePoison.h"
#include "FusionFireRules.h"
#include "FusionNative.h"
#include "FusionNativeMorePlants269Backend.h"
#include "FusionNativeCherryShooter.h"
#include "FusionNativeFireActionBatch.h"
#include "FusionNativeBasePlants.h"
#include "FusionNativeVisual.h"
#include "FusionNativeProjectileMetadata.h"
#include "FusionDeathRules.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../Board.h"
#include "../Net/NetCommon.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "../../Sexy.TodLib/TodParticle.h"
#include <algorithm>
#include <vector>
#include <cmath>

namespace LawnFusion
{
namespace
{
bool HostObject(const GameObject* object)
{
    return object && object->mApp && !object->mApp->NetIsClient() && !object->mNetGhost &&
        IsNativeFusionBoard(object->mBoard) && NativeFusionBackgroundValid(static_cast<uint8_t>(object->mBoard->mBackground));
}
bool AllocatedNormal(Zombie* target)
{
    if (!HostObject(target) || !IsNativeFusionGroundZombie(target)) return false;
    const auto id = target->mBoard->mZombies.DataArrayGetID(target);
    return id && target->mBoard->mZombies.DataArrayTryToGet(id) == target;
}
bool LiveNormal(Zombie* target)
{
    return AllocatedNormal(target) && !target->IsDeadOrDying() && target->mBodyHealth > 0 &&
        !target->mMindControlled && !target->IsFlying() &&
        (!GetNativeSourceZombieData(target) ||
            (!target->mOnHighGround && target->mZombiePhase == PHASE_ZOMBIE_NORMAL));
}
bool BlockedShield(const Zombie* target)
{
    return target->mShieldType == SHIELDTYPE_DOOR || target->mShieldType == SHIELDTYPE_LADDER;
}
bool GetIncoming(Projectile* projectile, NativeProjectileData& data, bool& legacyPea)
{
    legacyPea = false;
    if (const auto* existing = GetNativeProjectileData(projectile))
    {
        data = *existing;
        return FusionFireProjectileMetadataValid(data.kind,NativeProjectileValidationDamage369(data),data.sunOnHit,data.fireState,
            data.lastTorchId,data.lastTorchToken,projectile->mProjectileType,data.kind==0&&data.effectVariant==1);
    }
    // Current native-Fusion factory invariant: the only untyped friendly PEA
    // is source0. Other supported PEA-backed payloads are already typed. Fail
    // closed for every other motion/type; new factories must revisit this rule.
    if (projectile->mProjectileType != PROJECTILE_PEA || projectile->mMotionType != MOTION_STRAIGHT ||
        projectile->GetProjectileDef().mDamage != 20) return false;
    data = {};
    legacyPea = true;
    return true;
}
bool ReplaceProjectile(Plant* torch, Projectile* original, const NativeProjectileData& incoming,
    const FusionTorchConversion& plan)
{
    Board* board = torch->mBoard;
    NativeProjectileData melted;
    melted.kind=plan.kind;melted.damage=plan.damage;melted.specialType=incoming.specialType;
    if(incoming.sourceUnmodifiedDamage369&&plan.damage==incoming.sourceUnmodifiedDamage369)
    {melted.sourceUnmodifiedDamage369=plan.damage;melted.damage=incoming.damage;}
    melted.lastTorchId=torch->mNetId;melted.lastTorchToken=GetNativePlantData(torch)->instanceToken;
    if(!NativeProjectileMetadataValid(melted,PROJECTILE_PEA,original->mMotionType))return false;
    if (board->mProjectiles.mSize >= board->mProjectiles.mMaxSize ||
        !EnsureNativeProjectileVisuals(board,plan.kind)) return false;
    auto* fresh = board->AddProjectile(static_cast<int>(original->mPosX),static_cast<int>(original->mPosY),
        original->mRenderOrder,original->mRow,PROJECTILE_PEA);
    if (!fresh) return false;
    if (!fresh->mNetId) fresh->mNetId = board->NetAllocId();
    if (!fresh->mNetId) { fresh->Die(); return false; }
    fresh->mPosX=original->mPosX; fresh->mPosY=original->mPosY; fresh->mPosZ=original->mPosZ;
    fresh->mX=original->mX; fresh->mY=original->mY; fresh->mShadowY=original->mShadowY;
    fresh->mVelX=original->mVelX; fresh->mVelY=original->mVelY; fresh->mVelZ=original->mVelZ; fresh->mAccZ=original->mAccZ;
    fresh->mMotionType=original->mMotionType; fresh->mOnHighGround=original->mOnHighGround;
    fresh->mRotation=original->mRotation; fresh->mRotationSpeed=original->mRotationSpeed;
    fresh->mDamageRangeFlags=original->mDamageRangeFlags;
    fresh->mOverrideColor=original->mOverrideColor; fresh->mFilterEffect=original->mFilterEffect;
    fresh->mExtraAdditiveColor=original->mExtraAdditiveColor; fresh->mEnableExtraAdditiveDraw=original->mEnableExtraAdditiveDraw;
    const auto* plant = GetNativePlantData(torch);
    if (!AttachNativeProjectile(board,fresh,melted)) { fresh->Die(); return false; }
    // IceTorch copies source theExistTime (Bullet+0x48), retaining a Three or
    // Rage trajectory's phase. Ordinary Torchwood melting starts a fresh age.
    if(plant->kind==1182)fresh->mProjectileAge=original->mProjectileAge;
    LawnNet::NetTrace("Fusion native Torchwood melt torch=%u token=%u old=%u new=%u damage=%d age=%d motion=%d special=%u",
        torch->mNetId,plant->instanceToken,original->mNetId,fresh->mNetId,melted.damage,fresh->mProjectileAge,
        fresh->mMotionType,melted.specialType);
    original->Die();
    return true;
}
}

bool IsNativeHotProjectile(const Projectile* projectile)
{
    const auto* data = GetNativeProjectileData(projectile);
    return data && data->fireState > 0 && data->kind != 8 &&
        (IsNativeMore269Projectile(projectile)||FusionFireProjectileMetadataValid(data->kind,NativeProjectileValidationDamage369(*data),data->sunOnHit,
        data->fireState,data->lastTorchId,data->lastTorchToken,projectile->mProjectileType,data->kind==0&&data->effectVariant==1));
}
bool TryConvertNativeTorchProjectile(Plant* torch,Projectile* projectile,uint16_t sourceTorchKind,bool& counted)
{
    counted=false;
    const auto* plant=GetNativePlantData(torch);
    if(!plant||plant->kind!=sourceTorchKind||!HostObject(torch)||torch->mDead||!torch->mNetId||!plant->instanceToken||
       !HostObject(projectile)||projectile->mDead||projectile->mBoard!=torch->mBoard)return false;
    NativeProjectileData candidate;bool legacyPea=false;
    if(!GetIncoming(projectile,candidate,legacyPea))return false;
    FusionTorchConversion plan;
    if(!TryResolveFusionTorchConversion(sourceTorchKind,candidate.kind,NativeProjectileValidationDamage369(candidate),candidate.sunOnHit,
        candidate.fireState,candidate.specialType,projectile->mRow==torch->mRow,
        sourceTorchKind==1056&&candidate.isZombieBullet,projectile->mMotionType,
        candidate.lastTorchId,candidate.lastTorchToken,torch->mNetId,plant->instanceToken,plan,
        candidate.kind==0&&candidate.effectVariant==1))return false;
    if(plan.action==FusionTorchConversionAction::ThreeYellow)
    {
        if(!EmitNativeFireThree(torch))return false;
        projectile->Die();counted=true;return true;
    }
    if(plan.replace)
    {
        if(!ReplaceProjectile(torch,projectile,candidate,plan))return false;
        NativeFireConversionCommitted(torch);counted=true;return true;
    }
    const auto before369=candidate;
    candidate.kind=plan.kind;candidate.damage=plan.damage;candidate.sourceUnmodifiedDamage369=0;
    if(before369.sourceUnmodifiedDamage369)
    {
        int64_t actual=plan.damage;
        if((candidate.kind==0||candidate.kind==10)&&(before369.kind==0||before369.kind==10||before369.kind==15||before369.kind==97))
            actual=int64_t(before369.damage)+plan.damage-before369.sourceUnmodifiedDamage369;
        else if((candidate.kind==8||candidate.kind==1)&&plan.damage%before369.sourceUnmodifiedDamage369==0)
            actual=int64_t(before369.damage)*(plan.damage/before369.sourceUnmodifiedDamage369);
        if(actual<=0||actual>(std::numeric_limits<int>::max)())return false;
        if(actual!=plan.damage){candidate.sourceUnmodifiedDamage369=plan.damage;candidate.damage=int(actual);}
    }
    candidate.fireState=plan.fireState;candidate.sunOnHit=plan.sunOnHit;
    if(candidate.kind==0)candidate.effectVariant=plan.smallProjectile?1:0;
    if(plan.stamp){candidate.lastTorchId=torch->mNetId;candidate.lastTorchToken=plant->instanceToken;}
    const auto type=plan.action==FusionTorchConversionAction::Sun?PROJECTILE_PEA:PROJECTILE_FIREBALL;
    if (!FusionFireProjectileMetadataValid(candidate.kind,NativeProjectileValidationDamage369(candidate),candidate.sunOnHit,candidate.fireState,
        candidate.lastTorchId,candidate.lastTorchToken,type,candidate.kind==0&&candidate.effectVariant==1)) return false;
    if(!NativeProjectileMetadataValid(candidate,type,projectile->mMotionType))return false;
    // Column is only a legacy cosmetic guard, not the source identity.
    projectile->mHitTorchwoodGridX=-1;
    if(type==PROJECTILE_FIREBALL&&projectile->mProjectileType!=PROJECTILE_FIREBALL)
        projectile->ConvertToFireball(torch->mPlantCol);
    if (legacyPea) projectile->mVelX=3.33f; // Materialize its old effective native motion, formerly a constant.
    if (!AttachNativeProjectile(torch->mBoard,projectile,candidate)) { projectile->Die(); return false; }
    NativeFireConversionCommitted(torch);
    LawnNet::NetTrace("Fusion native Torchwood yellow torch=%u token=%u projectile=%u damage=%d vx=%.9g age=%d special=%u",
        torch->mNetId,plant->instanceToken,projectile->mNetId,candidate.damage,projectile->mVelX,
        projectile->mProjectileAge,candidate.specialType);
    counted=true;return true;
}
bool HandleNativeTorchwood(Plant* torch)
{
    const auto* plant = GetNativePlantData(torch);
    if (!plant || !FusionFireTorchKind(plant->kind)) return false;
    if (!HostObject(torch) || torch->mDead || !torch->mNetId || !plant->instanceToken) return true;
    const auto area = torch->GetPlantAttackRect(WEAPON_PRIMARY);
    // Capture actual generations before a melt allocates another native actor.
    std::vector<uint32_t> projectiles;
    Projectile* projectile = nullptr;
    while (torch->mBoard->IterateProjectiles(projectile))
        if (!projectile->mDead) projectiles.push_back(torch->mBoard->mProjectiles.DataArrayGetID(projectile));
    for (const auto id : projectiles)
    {
        projectile = torch->mBoard->mProjectiles.DataArrayTryToGet(id);
        if (!HostObject(projectile) || projectile->mDead) continue;
        if(GetRectOverlap(area,projectile->GetProjectileRect())<10)continue;
        bool counted=false;TryConvertNativeTorchProjectile(torch,projectile,plant->kind,counted);
    }
    return true;
}
void NativeWarmAfterDamage(Zombie* target)
{
    // Damage may already have made the still-allocated primary dead/dying.
    if (!AllocatedNormal(target)) return;
    WarmNativePoison(target);
    auto* data = GetNativeZombieColdData(target);
    if (data)
    {
        FusionColdState warmed;
        if (!TryWarmFusionColdState({data->level,data->threshold,target->mChilledCounter,target->mIceTrapCounter},warmed)) return;
        data->level=warmed.level; // Persistent threshold is deliberately unchanged.
        data->chilledTicks=data->frozenTicks=0;
    }
    target->SetColdCounters(0,0);
}
void HealNativeCherryNutFromCircle(Board* board,double x,double y,int row,int amount,double radius)
{
    if(!board||!board->mApp||board->mApp->mBoard!=board||board->mApp->NetIsClient()||
        board->mApp->mGameScene!=SCENE_PLAYING||board->mPaused||!IsNativeFusionBoard(board)||
        !NativeFusionBackgroundValid(static_cast<uint8_t>(board->mBackground))||amount<0||!std::isfinite(x+y+radius)||radius<=0)return;
    Plant* receiver=nullptr;
    while(board->IteratePlants(receiver))
    {
        auto* data=GetNativePlantData(receiver);
        if(!data||data->kind!=1003||receiver->mDead||receiver->mNetGhost||!receiver->mIsOnBoard||
            receiver->mPlantHealth<=0)continue;
        const int overlaps=FusionFireCherryNutHealOverlapCount(x,y,row,
            board->GetCellCenterX(receiver->mPlantCol,receiver->mRow),
            board->GridToPixelY(receiver->mPlantCol,receiver->mRow)+50,receiver->mRow,radius);
        for(int i=0;i<overlaps;++i)
        {
            const int before=receiver->mPlantHealth;int after=before;
            if(!TryApplyFusionDeathHeal(before,receiver->mPlantMaxHealth,amount,after))break;
            receiver->mPlantHealth=after;
            receiver->mEatenFlashCountdown=(std::max)(receiver->mEatenFlashCountdown,25);
            NativePlantHealthChanged(receiver);
            LawnNet::NetTrace("Fusion native Fire circleHeal id=%u token=%u kind=1003 collider=%d before=%d after=%d amount=%d radius=%.3f tick=%d",
                receiver->mNetId,data->instanceToken,i,before,after,amount,radius,board->mMainCounter);
        }
    }
}
bool ApplyNativeFireImpact(Projectile* projectile, Zombie* primary)
{
    const auto* source369=GetNativeProjectileData(projectile);
    const bool money369=source369&&(source369->kind==98||source369->kind==99)&&HasNativeTravelAdvanced369(projectile->mBoard,42);
    if (!IsNativeHotProjectile(projectile)&&!money369) return false;
    if (!HostObject(projectile) || projectile->mDead) return true;
    if (primary && (!LiveNormal(primary) || primary->mBoard != projectile->mBoard || primary->mRow != projectile->mRow)) return true;
    const int damage = GetNativeProjectileData(projectile)->damage;
    float x = projectile->mPosX+20.0f, y = projectile->mPosY+projectile->mPosZ+20.0f;
    projectile->PlayImpactSound(primary);
    if (primary)
    {
        primary->TakeDamage(damage,0U); // Hot way9 is still Normal0, never vanilla rear bypass.
        NativeWarmAfterDamage(primary);
        std::vector<ZombieID> targets;
        // Evaluate armor after the primary hit. Native death alone does not
        // suppress source AllowSputter; its status1 is not excluded.
        if (FusionFireAllowsSputter(0,BlockedShield(primary)))
        {
            Zombie* target = nullptr;
            while (projectile->mBoard->IterateZombies(target))
            {
                if (target == primary || !LiveNormal(target) || target->mRow != projectile->mRow ||
                    !FusionFireAllowsSputter(0,BlockedShield(target))) continue;
                const auto rect = target->GetZombieRect();
                if (FusionFireAreaTouchesRect(x,y,{static_cast<double>(rect.mX),static_cast<double>(rect.mY),
                    static_cast<double>(rect.mWidth),static_cast<double>(rect.mHeight)})) targets.push_back(projectile->mBoard->ZombieGetID(target));
            }
        }
        int splash = 0;
        if (!targets.empty() && TryGetFusionFireSplashDamage(GetNativeProjectileData(projectile)->kind,damage,static_cast<int>(targets.size()),splash))
            for (const auto id : targets)
                if (auto* target = projectile->mBoard->ZombieTryToGet(id))
                {
                    if (!LiveNormal(target)) continue;
                    target->TakeDamage(splash,0U);
                    NativeWarmAfterDamage(target);
                }
        LawnNet::NetTrace("Fusion native hot impact projectile=%u primary=%u damage=%d secondaryCount=%zu secondaryDamage=%d shieldAfter=%d",
            projectile->mNetId,primary->mNetId,damage,targets.size(),splash,primary->mShieldType);
    }
    projectile->mApp->AddTodParticle(x,y,projectile->mRenderOrder+1,PARTICLE_FIREBALL_DEATH);
    LawnNet::NetBroadcastParticle(projectile->mApp,x,y,projectile->mRenderOrder+1,PARTICLE_FIREBALL_DEATH);
    projectile->Die();
    return true;
}
void ApplyNativeFireSourceExplosion(Zombie* source,int damage,bool warm)
{
    if(!AllocatedNormal(source)||source->mMindControlled||damage<0)return;
    float x=0,y=0;if(!GetNativeBaseZombieGroundAnchor(source,x,y))return;
    y-=static_cast<float>(.7*(100.0/1.67));
    auto* board=source->mBoard;std::vector<ZombieID> ids;Zombie* target=nullptr;
    while(board->IterateZombies(target))
    {
        // The source tests the originating MC flag, not each secondary's flag.
        if(target==source||!AllocatedNormal(target)||target->mDead||target->IsDeadOrDying()||
            target->IsFlying()||target->mInPool||std::abs(target->mRow-source->mRow)>1)continue;
        const auto r=target->GetZombieRect();
        if(FusionFireCircleTouchesRect(x,y,1,r.mX,r.mY,r.mWidth,r.mHeight))ids.push_back(board->ZombieGetID(target));
    }
    for(auto id:ids)if((target=board->ZombieTryToGet(id))&&AllocatedNormal(target)&&!target->mDead&&!target->IsDeadOrDying())
    {target->TakeDamage(damage,1U);if(warm)NativeWarmAfterDamage(target);}
    source->mApp->AddTodParticle(x,y,RENDER_LAYER_TOP,PARTICLE_FIREBALL_DEATH);
    LawnNet::NetBroadcastParticle(source->mApp,x,y,RENDER_LAYER_TOP,PARTICLE_FIREBALL_DEATH);
    LawnNet::NetTrace("Fusion native Fire sourceExplosion source=%u hits=%zu damage=%d warm=%d",source->mNetId,ids.size(),damage,warm?1:0);
}
}
