#include "FusionNativeProjectileBuff369.h"
#include "FusionNativeBasePlants.h"
#include "FusionBasePlantRules.h"
#include "FusionChomperRules.h"
#include "FusionNativeDeath.h"
#include "FusionNativeProducer.h"
#include "FusionNativeCold.h"
#include "FusionNativeShooterBatch.h"
#include "FusionNativeRollingProjectile.h"
#include "FusionNativeWater268.h"
#include "FusionNativeAction266.h"
#include "FusionCherryShooterRules.h"
#include "FusionNative.h"
#include "FusionNativeShooterTiming.h"
#include "FusionNativeRemainingPlants269.h"
#include "FusionNativeDayZombie273Bridge.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Projectile.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include <cmath>
#include <vector>
#include <cstring>
#include <algorithm>

namespace LawnFusion
{
namespace
{
bool HostPlant(Plant* plant)
{
    return plant && plant->mApp && plant->mBoard && !plant->mApp->NetIsClient() &&
        !plant->mNetGhost && !plant->mDead && GetNativePlantData(plant);
}
bool NormalZombie(Zombie* zombie)
{
    return zombie && !zombie->mDead && !NativeWater268ZombieCaptured(zombie) && IsNativeFusionGroundZombie(zombie);
}
bool EatingThisPlant(Plant* plant, Zombie* zombie)
{
    // Basic native zombies find their real eating target; mTargetPlantID is
    // otherwise reserved for special actors such as Bungee, not ordinary eating.
    return zombie->mIsEating && zombie->FindPlantTarget(ATTACKTYPE_CHEW) == plant;
}
void PlayTimedBody(Plant* plant, const char* layer, int ticks, bool loop)
{
    auto* body = plant->mApp->ReanimationTryToGet(plant->mBodyReanimID);
    if (!body || !body->TrackExists(layer)) return;
    int start = 0, count = 0;
    body->GetFramesForLayer(layer, start, count);
    // Reanimation::Update advances time by dt*rate/mFrameCount.
    const float rate = ticks > 0 ? (count > 0 ? count : 1) * 100.0f / ticks : 15.0f;
    plant->PlayBodyReanim(layer, loop ? REANIM_LOOP : REANIM_PLAY_ONCE_AND_HOLD, 0, rate);
}
Zombie* FindChomperTarget(Plant* plant)
{
    float px = 0, py = 0;
    FusionBaseRect area;
    if (!GetNativeBasePlantGroundAnchor(plant, px, py) || !TryMakeFusionChomperRect(px, py, area)) return nullptr;
    if(GetNativePlantData(plant)->kind==1015)
        area={px+.25*kFusionBasePixelsX,py-1.0*kFusionBasePixelsY,.5*kFusionBasePixelsX,1.0*kFusionBasePixelsY};
    if(GetNativePlantData(plant)->kind==1174)
    {
        float x=0,y=0,width=0,height=0;
        if(!GetNativeBigChomper269Range(plant,false,x,y,width,height))return nullptr;
        area={x-width*.5,y-height*.5,width,height};
    }
    Zombie* zombie = nullptr;
    Zombie* nearest = nullptr;
    float nearestX = 0;
    while (plant->mBoard->IterateZombies(zombie))
    {
        if (!NormalZombie(zombie) || !FusionChomperCanAcquire(zombie->IsDeadOrDying() ? 1 : 0,
            zombie->mRow == plant->mRow, zombie->mMindControlled, zombie->IsDeadOrDying())) continue;
        if (EatingThisPlant(plant, zombie)) return zombie;
        const auto rect = zombie->GetZombieRect();
        if (rect.mX >= area.x + area.width || rect.mX + rect.mWidth <= area.x ||
            rect.mY >= area.y + area.height || rect.mY + rect.mHeight <= area.y) continue;
        // Source orders by axis.worldX, not the collider's left edge.
        if (std::isfinite(zombie->mPosX) && (!nearest || zombie->mPosX < nearestX))
        { nearest = zombie; nearestX = zombie->mPosX; }
    }
    return nearest;
}
}

bool IsNativeBasePlant(const Plant* plant, uint16_t kind)
{
    const auto* data = GetNativePlantData(plant);
    return kind >= 5 && kind <= 7 && data && data->kind == kind;
}
bool IsNativeBaseMushroom(const Plant* plant)
{
    const auto* data=GetNativePlantData(plant);
    return IsNativeBasePlant(plant, 6) || (data && (data->kind==19 || FindFusionFumeProfile(data->kind)));
}

bool IsNativeChomper(const Plant* plant)
{
    const auto* data = GetNativePlantData(plant);
    return data && FindFusionChomperProfile(data->kind) && plant->mSeedType == SEED_CHOMPER;
}

bool GetNativeChomperVisualPose(const Plant* plant, const char*& sourceState, float& elapsedSeconds)
{
    if (!IsNativeChomper(plant)) return false;
    const auto* data = GetNativePlantData(plant);
    if (data->kind == 5) return false; // Preserve the accepted native5 body path.
    if(data->kind==1011 && data->batchRageTicks>0)
    {
        sourceState="shoot";elapsedSeconds=(std::max)(0.f,FusionBaseShootDuration(1011,data->animationSpeed)-data->batchRageTicks*.01f);
        return std::isfinite(elapsedSeconds)&&elapsedSeconds>=0;
    }
    const char* state = nullptr;
    switch (plant->mState)
    {
    case STATE_READY: state = "idle"; break;
    case STATE_CHOMPER_BITING:
    case STATE_CHOMPER_BITING_GOT_ONE:
    case STATE_CHOMPER_BITING_MISSED: state = "bite"; break;
    case STATE_CHOMPER_DIGESTING: state = "chew"; break;
    case STATE_CHOMPER_SWALLOWING: state = "swallow"; break;
    default: return false;
    }
    if(data->kind==1011 && (plant->mState==STATE_CHOMPER_DIGESTING||plant->mState==STATE_CHOMPER_SWALLOWING))state="idle";
    // BigChomper.Chomp crossfades to chew at BiteEvent, before the inherited
    // ToChew event. Native state transitions still use both source callbacks.
    if(data->kind==1174&&plant->mState==STATE_CHOMPER_BITING_GOT_ONE)state="chew";
    const float seconds = plant->mState == STATE_READY || (data->kind==1011&&!std::strcmp(state,"idle")) ? data->idleSeconds : data->shootSeconds;
    if (!std::isfinite(seconds) || seconds < 0) return false;
    sourceState = state;
    elapsedSeconds = seconds;
    return true;
}

void NativeChomperHealthChanged(Plant* plant)
{
    if (!HostPlant(plant) || !IsNativeChomper(plant)) return;
    auto* data = GetNativePlantData(plant);
    TryAdvanceFusionChomperDamageStage(data->kind,plant->mPlantHealth,data->damageStage,data->damageStage);
}

void InitializeNativeBasePlant(Plant* plant)
{
    if (!HostPlant(plant)) return;
    auto* data = GetNativePlantData(plant);
    const auto* chomper = IsNativeChomper(plant) ? FindFusionChomperProfile(data->kind) : nullptr;
    if (!chomper && !IsNativeBaseMushroom(plant)) return;
    plant->mPlantHealth = plant->mPlantMaxHealth = chomper ? chomper->maxHealth : data->kind==1159?2000:data->kind==1160?3000:300;
    if (chomper)
    {
        data->animationSpeed = RandRangeFloat(.9f, 1.1f);
        data->productionTicks = data->pendingProductionTicks = 0;
        plant->mLaunchRate = plant->mLaunchCounter = 0;
        plant->mState = STATE_READY;
        plant->mTargetZombieID = ZOMBIEID_NULL;
        if(data->kind==1011)
        {
            TryGetFusionShooterInitialTicks(*FindFusionBaseTimingProfile(1011),RandRangeFloat(.2f,.8f),plant->mTargetX);
            plant->mTargetY=0;
        }
        if(data->kind==1174)plant->mTargetX=0; // Host-only original digest length, not a second countdown.
    }
    else if (!InitializeNativeShooterTiming(plant, *FindFusionBaseTimingProfile(data->kind), true)) return;
    LawnNet::NetTrace("Fusion native base init id=%u kind=%u health=%d speed=%.9g launch=%d awake=%d",
        plant->mNetId, data->kind, plant->mPlantHealth, data->animationSpeed, plant->mLaunchCounter, !plant->mIsAsleep);
}

bool ResetNativeBaseLaunchCounter(Plant* plant)
{
    return HostPlant(plant) && IsNativeBaseMushroom(plant) &&
        ResetNativeShooterLaunchCounter(plant, *FindFusionBaseTimingProfile(GetNativePlantData(plant)->kind));
}

void BeginNativeBaseShot(Plant* plant)
{
    if (!HostPlant(plant) || !IsNativeBaseMushroom(plant)) return;
    auto* data = GetNativePlantData(plant);
    if (!BeginNativeShooterShot(plant, *FindFusionBaseTimingProfile(data->kind))) return;
    PlayTimedBody(plant, "anim_shooting", static_cast<int>(std::ceil(FusionBaseShootDuration(data->kind, data->animationSpeed) * 100)), false);
    LawnNet::NetTrace("Fusion native base trigger id=%u kind=%u shootCounter=%d speed=%.9g",
        plant->mNetId, data->kind, plant->mShootingCounter, data->animationSpeed);
}

void UpdateNativeBasePlantPhase(Plant* plant)
{
    if (!HostPlant(plant)) return;
    auto* data = GetNativePlantData(plant);
    if (IsNativeChomper(plant))
    {
        if (data->kind == 5) return; // No old5 metadata/timing/RNG changes.
        if(data->kind==1011){data->batchRageTicks=static_cast<uint16_t>((std::max)(0,plant->mTargetY));data->batchMode=data->batchRageTicks?1:0;}
        data->idleSeconds += .01f;
        FusionChomperTiming timing;
        if (!TryGetFusionChomperTiming(data->kind,data->animationSpeed,timing)) return;
        const auto* profile = FindFusionChomperProfile(data->kind);
        int elapsed = -1;
        switch (plant->mState)
        {
        case STATE_CHOMPER_BITING: elapsed = timing.bite - plant->mStateCountdown; break;
        case STATE_CHOMPER_BITING_GOT_ONE: elapsed = timing.chew - plant->mStateCountdown; break;
        case STATE_CHOMPER_BITING_MISSED: elapsed = timing.biteEnd - plant->mStateCountdown; break;
        case STATE_CHOMPER_DIGESTING:
            elapsed = profile->digestTicks - plant->mLaunchCounter - (timing.chew - timing.bite); break;
        case STATE_CHOMPER_SWALLOWING: elapsed = timing.swallow - plant->mStateCountdown; break;
        default: break;
        }
        if(data->kind==1174&&(plant->mState==STATE_CHOMPER_BITING_GOT_ONE||plant->mState==STATE_CHOMPER_DIGESTING))
            elapsed=plant->mTargetX-plant->mLaunchCounter;
        // A presentation cache of actual native counters; clients never tick it.
        data->shootSeconds = elapsed < 0 ? -1.0f : elapsed * .01f;
        return;
    }
    if (IsNativeBaseMushroom(plant))
        UpdateNativeShooterPhase(plant,*FindFusionBaseTimingProfile(data->kind));
}

bool GetNativeBasePlantGroundAnchor(Plant* plant, float& x, float& y)
{
    return plant && plant->mBoard && GetNativePlantData(plant) && plant->GetShadowCenter(x, y);
}

bool GetNativeBaseZombieGroundAnchor(Zombie* zombie, float& x, float& y)
{
    return NormalZombie(zombie) && IsNativeFusionBoard(zombie->mBoard) && zombie->GetShadowCenter(x, y);
}

Zombie* FindNativeBaseShooterTarget(Plant* plant, int row)
{
    if (!HostPlant(plant) || !IsNativeBaseMushroom(plant)) return nullptr;
    float px = 0, py = 0;
    if (!GetNativeBasePlantGroundAnchor(plant, px, py)) return nullptr;
    const auto kind = GetNativePlantData(plant)->kind;
    const double vision = plant->mBoard->GetCellCenterX(plant->mBoard->GetColumnCount() - 1, row) + 1.85 * kFusionBasePixelsX;
    Zombie* zombie = nullptr;
    while (plant->mBoard->IterateZombies(zombie))
    {
        float x = 0, y = 0;
        if (!NormalZombie(zombie) || !GetNativeBaseZombieGroundAnchor(zombie, x, y) || x >= vision) continue;
        if (FusionBaseCanAcquire(kind, GetNativeFusionZombieStatus(zombie), zombie->mRow == row,
            zombie->mMindControlled, !zombie->mHasHead || zombie->IsDeadOrDying(), x - px)) return zombie;
    }
    return nullptr;
}

bool UpdateNativeChomper(Plant* plant)
{
    if (!IsNativeChomper(plant)) return false;
    if (!HostPlant(plant)) return true;
    const auto* data = GetNativePlantData(plant);
    const uint16_t kind = data->kind;
    const uint32_t token = data->instanceToken;
    const float speed = data->animationSpeed;
    const auto* profile = FindFusionChomperProfile(kind);
    if(kind==1011)
    {
        if(plant->mTargetY>0)--plant->mTargetY;
        if(plant->mLaunchCounter>0 && --plant->mTargetX<0)
        {
            TryGetFusionShooterCycleTicks(*FindFusionBaseTimingProfile(1011),RandRangeFloat(.95f,1.05f),plant->mTargetX);
            if(plant->mTargetY==0 && BeginNativeShooterShot(plant,*FindFusionBaseTimingProfile(1011)))
                plant->mTargetY=int(std::ceil(FusionBaseShootDuration(1011,speed)*100));
        }
    }
    FusionChomperTiming timing;
    if (!TryGetFusionChomperTiming(kind,speed,timing)) return true;
    if (plant->mState == STATE_READY)
    {
        Zombie* target = FindChomperTarget(plant);
        if (target)
        {
            plant->mTargetZombieID = plant->mBoard->ZombieGetID(target);
            plant->mState = STATE_CHOMPER_BITING;
            plant->mStateCountdown = timing.bite;
            PlayTimedBody(plant, "anim_bite", timing.biteEnd, false);
            LawnNet::NetTrace("Fusion native Chomper lock id=%u target=%u biteTicks=%d kind=%u token=%u tick=%d",
                plant->mNetId,target->mNetId,timing.bite,kind,token,plant->mBoard->mMainCounter);
        }
    }
    else if (plant->mState == STATE_CHOMPER_BITING && plant->mStateCountdown == 0)
    {
        Zombie* target = plant->mBoard->ZombieTryToGet(plant->mTargetZombieID);
        bool eat = NormalZombie(target) && (EatingThisPlant(plant, target) ||
            (!target->mMindControlled && !target->IsDeadOrDying() && target->mBodyHealth > 0));
        NativeBlastSpec blast;
        if (eat && profile->cherryBite)
        {
            float x = 0, y = 0;
            // Admission precedes the target kill. A native last-kill award must
            // not discard the already-admitted Cherry continuation afterward.
            eat = GetNativeBasePlantGroundAnchor(plant,x,y) &&
                TryMakeFusionChomperBlast(kind,x,y,plant->mRow,blast) &&
                CanStartNativeFusionBlast(plant->mBoard,blast);
        }
        Coin* dayBucket=nullptr;int dayOldHelm=0,dayOldHealth=0;
        // Direct source106 DropArmor precedes Die(2), including the mass bite.
        // Capacity failure keeps this exact locked bite pending without sound.
        if(eat&&!PrepareNativeDayZombie273ChomperArmor(target,dayBucket,dayOldHelm,dayOldHealth))return true;
        plant->mTargetZombieID = ZOMBIEID_NULL;
        plant->mApp->PlayFoley(FOLEY_BIGCHOMP);
        int actualDigest=profile->digestTicks;
        if(eat&&kind==1174)
        {
            // false is the source boss40/ChompBack path, never permission to
            // fall through into the ordinary native instant-chomp branch.
            const int sourceKind=GetNativeFusionZombieKind(target);
            eat=ApplyNativeBigChomper269Bite(plant,target);
            if(eat)
            {
                actualDigest=plant->mStateCountdown; // Source Random.value*3+8, quantized once.
                plant->mTargetX=actualDigest;
            }
            LawnNet::NetTrace("Fusion native BigChomper269 bite plant=%u token=%u target=%u sourceKind=%d digest=%d chompBack=%d tick=%d",
                plant->mNetId,token,target->mNetId,sourceKind,eat?actualDigest:0,eat?0:1,plant->mBoard->mMainCounter);
        }
        if (eat)
        {
            const auto id = target->mNetId;
            // The mass leaf may already have retired this actual primary.
            // A locked primary that moved outside the secondary area still
            // receives source Chomp's unconditional final Die(2).
            if(!target->mDead)
            {target->DropHelm(0U); target->DropShield(0U); target->DropFlag();target->DieWithLoot();}
            plant->mState = STATE_CHOMPER_BITING_GOT_ONE;
            plant->mStateCountdown = timing.chew - timing.bite;
            plant->mLaunchCounter = actualDigest;
            LawnNet::NetTrace("Fusion native Chomper swallow id=%u target=%u digest=%d kind=%u token=%u tick=%d",
                plant->mNetId,id,plant->mLaunchCounter,kind,token,plant->mBoard->mMainCounter);
            if (kind==1192) NativeShooterBatchChomperSwallowed(plant);
            if (profile->cherryBite) ContinueNativeFusionBlast(plant->mBoard,blast);
        }
        else
        {
            RollbackNativeDayZombie273ChomperArmor(target,dayBucket,dayOldHelm,dayOldHealth);
            plant->mState = STATE_CHOMPER_BITING_MISSED;
            plant->mStateCountdown = timing.biteEnd - timing.bite;
            plant->mLaunchCounter = 0;
            LawnNet::NetTrace("Fusion native Chomper miss id=%u noReacquire=1 kind=%u token=%u tick=%d",
                plant->mNetId,kind,token,plant->mBoard->mMainCounter);
        }
    }
    else if (plant->mState == STATE_CHOMPER_BITING_GOT_ONE || plant->mState == STATE_CHOMPER_DIGESTING)
    {
        if (plant->mLaunchCounter > 0) --plant->mLaunchCounter;
        if (plant->mState == STATE_CHOMPER_BITING_GOT_ONE && plant->mStateCountdown == 0)
        {
            plant->mState = STATE_CHOMPER_DIGESTING;
            int chewTicks = 0;
            TryGetFusionChomperChewTicks(kind,speed,chewTicks);
            PlayTimedBody(plant, "anim_chew", chewTicks, true);
        }
        if (plant->mLaunchCounter == 0)
        {
            plant->mState = STATE_CHOMPER_SWALLOWING;
            plant->mStateCountdown = timing.swallow;
            if(kind==1015)NativeAction266ChomperSwallowStarted(plant);
            PlayTimedBody(plant, "anim_swallow", timing.swallow, false);
            const int before = plant->mPlantHealth;
            int after = before;
            if (TryRecoverFusionChomper(kind,before,plant->mPlantMaxHealth,after))
            {
                plant->mPlantHealth = after;
                NativePlantHealthChanged(plant);
                LawnNet::NetTrace("Fusion native Chomper recover id=%u token=%u kind=%u before=%d after=%d max=%d tick=%d",
                    plant->mNetId,token,kind,before,after,plant->mPlantMaxHealth,plant->mBoard->mMainCounter);
            }
            if (profile->sunCount && !plant->mApp->IsIZombieLevel())
            {
                plant->mLaunchCounter = profile->sunDelayTicks;
                LawnNet::NetTrace("Fusion native Chomper reward scheduled id=%u token=%u kind=%u pending=%d count=%d tick=%d",
                    plant->mNetId,token,kind,plant->mLaunchCounter,profile->sunCount,plant->mBoard->mMainCounter);
            }
        }
    }
    else if (plant->mState == STATE_CHOMPER_SWALLOWING || plant->mState == STATE_CHOMPER_BITING_MISSED)
    {
        if (plant->mState == STATE_CHOMPER_SWALLOWING && profile->sunCount)
        {
            int count = 0;
            if (AdvanceFusionChomperSunReward(kind,true,plant->mLaunchCounter,count) && count)
            {
                const bool emitted = EmitNativeSunBatch(plant,count,true);
                LawnNet::NetTrace("Fusion native Chomper reward id=%u token=%u kind=%u count=%d value=25 emitted=%d capacityOmit=%d col=%d row=%d tick=%d",
                    plant->mNetId,token,kind,count,emitted ? 1 : 0,emitted ? 0 : 1,
                    plant->mPlantCol,plant->mRow,plant->mBoard->mMainCounter);
            }
        }
        if (plant->mStateCountdown == 0)
        {
            plant->PlayIdleAnim(static_cast<float>(15.0 * profile->idleStateSpeed * speed));
            plant->mState = STATE_READY;
        }
    }
    return true;
}

bool ApplyNativeFumeAreaDamage(Plant* plant, int damage, unsigned int flags)
{
    const auto* data=GetNativePlantData(plant);
    const auto* profile=data ? FindFusionFumeProfile(data->kind) : nullptr;
    if (!profile) return false;
    if (!HostPlant(plant)) return true;
    if(data->kind==1159){damage=40;flags=2U;}
    if(data->kind==1160)damage=80;
    float px = 0, py = 0;
    if (!GetNativeBasePlantGroundAnchor(plant, px, py)) return true;
    std::vector<ZombieID> targets;
    Zombie* zombie = nullptr;
    while (plant->mBoard->IterateZombies(zombie))
    {
        float x = 0, y = 0;
        if (!NormalZombie(zombie) || !GetNativeBaseZombieGroundAnchor(zombie, x, y)) continue;
        if (FusionFumeCanHit(GetNativeFusionZombieStatus(zombie), zombie->mRow == plant->mRow,
            zombie->mMindControlled, !zombie->mHasHead || zombie->IsDeadOrDying(), x - px)) targets.push_back(plant->mBoard->ZombieGetID(zombie));
    }
    for (auto it = targets.rbegin(); it != targets.rend(); ++it)
        if (auto* target = plant->mBoard->ZombieTryToGet(*it))
        {
            if (profile->hypnotize)
            {
                const auto* source=GetNativeSourceZombieData(target);
                const auto* colors=GetNativeZombieActionData(target);
                float threshold=0;
                if (!TryGetFusionHypnoFumeThreshold(source?source->health:static_cast<float>(target->mBodyHealth),
                    target->mBodyMaxHealth,target->mHelmHealth,target->mHelmMaxHealth,colors?colors->colorMask:0,threshold)) continue;
                const float draw=RandRangeFloat(0,1);
                LawnNet::NetTrace("Fusion native HypnoFume id=%u target=%u threshold=%.9g draw=%.9g",plant->mNetId,target->mNetId,threshold,draw);
                if (draw>=threshold)
                { target->StopEating(); target->StartMindControlled(); target->TrySpawnLevelAward(); continue; }
            }
            const FusionColdDamageContext cold{0,FusionColdDamageDelivery::IceAll};
            const int delivered=data->kind==1159&&target->mHelmType!=HELMTYPE_NONE?damage*3:damage;
            if(data->kind==1160)ApplyNativeSourceRealDamage(target,delivered);
            else target->TakeDamage(delivered, flags,profile->coldLevelsAfterDamage ? &cold : nullptr);
            if (profile->coldLevelsAfterDamage) AddNativeColdLevel(target,profile->coldLevelsAfterDamage);
            plant->mApp->PlayFoley(FOLEY_SPLAT);
        }
    return true;
}

bool FireNativePeaChomper(Plant* plant)
{
    const auto* data=GetNativePlantData(plant);if(!data||data->kind!=1011)return false;
    if(HostPlant(plant))LaunchNativeChomperBlock(plant,static_cast<uint16_t>(RandRangeInt(4,5)));
    return true;
}

bool IsNativePuffProjectile(const Projectile* projectile)
{
    const auto* data = GetNativeProjectileData(projectile);
    return data && data->kind == 9 && NativeProjectileValidationDamage369(*data) == 20 && data->sunOnHit == 0 &&
        projectile->mProjectileType == PROJECTILE_PUFF && projectile->mMotionType == MOTION_PUFF;
}
void CreateNativePuffProjectile(Plant* plant, Projectile* projectile)
{
    const auto* source=GetNativePlantData(plant);
    if (!HostPlant(plant) || !source || (source->kind!=6&&source->kind!=19) || !projectile) return;
    NativeProjectileData data; data.kind = 9;
    if (!AttachNativeTravelProjectile369(plant,projectile,data)) { projectile->Die(); return; }
    // Preserve real native Puff muzzle/FX/art. Only physics and source identity
    // change; the native40x40 logical centre is still (posX+20,posY+posZ+20).
    projectile->mMotionType = MOTION_PUFF;
    projectile->mVelX = kFusionPuffPixelsPerTick;
    projectile->mVelY = projectile->mVelZ = projectile->mAccZ = 0;
    if(source->kind==19)
    {
        // Actual SeaShroom prefab is SmallPuff with its own muzzle/controller.
        float x=0,y=0;
        if(plant->GetShadowCenter(x,y))
        {
            projectile->mPosX=x+float(.40775*kFusionBasePixelsX)-20;
            projectile->mPosY=y-float(.15325*kFusionBasePixelsY)-20;
            projectile->mPosZ=0;projectile->mShadowY=projectile->mPosY+20;
            projectile->mX=int(projectile->mPosX);projectile->mY=int(projectile->mPosY);
        }
    }
}
bool GetNativePuffProjectileRect(const Projectile* projectile, Sexy::Rect& rect)
{
    if (!IsNativePuffProjectile(projectile)) return false;
    FusionBaseRect bounds;
    if (!TryMakeFusionPuffRect(projectile->mPosX + 20, projectile->mPosY + projectile->mPosZ + 20, bounds))
    { rect = Sexy::Rect(0,0,0,0); return true; }
    const int x = static_cast<int>(std::floor(bounds.x)), y = static_cast<int>(std::floor(bounds.y));
    rect = Sexy::Rect(x, y, static_cast<int>(std::ceil(bounds.x + bounds.width)) - x,
        static_cast<int>(std::ceil(bounds.y + bounds.height)) - y);
    return true;
}
bool NativePuffProjectileCanHit(const Projectile* projectile, Zombie* zombie)
{
    return IsNativePuffProjectile(projectile) && !projectile->mNetGhost && projectile->mApp && !projectile->mApp->NetIsClient() &&
        NormalZombie(zombie) && FusionCherryProjectileCanHit(zombie->IsDeadOrDying() ? 1 : 0,
            zombie->mRow == projectile->mRow, zombie->mMindControlled, static_cast<float>(zombie->mBodyHealth));
}
bool NativePuffProjectileExpired(const Projectile* projectile)
{
    if (!IsNativePuffProjectile(projectile) || projectile->mNetGhost || !projectile->mBoard ||
        !projectile->mApp || projectile->mApp->NetIsClient()) return false;
    if (FusionPuffLifetimeExpired(projectile->mProjectileAge)) return true;
    Board* board = projectile->mBoard;
    const double x = (projectile->mPosX + 20 - board->GetCellCenterX(0,0)) / kFusionBasePixelsX - 4.8;
    const double y = 2.3 - (projectile->mPosY + projectile->mPosZ + 20 - (board->GridToPixelY(0,0) + 50)) / kFusionBasePixelsY;
    return FusionCherryProjectileExpired(x, y, projectile->mProjectileAge, -10,
        1.35 * (board->GetColumnCount() - 1) - 4.8 + 1.85, 2.3 - 1.67 * (kNativeFusionRows - 1) - .6, 6.5);
}
}
