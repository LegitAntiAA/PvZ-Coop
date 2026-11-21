#include "FusionNativeZombies.h"
#include "FusionNative.h"
#include "FusionNativeZombieVisual.h"
#include "FusionZombieMotion.h"
#include "FusionGarlicRules.h"
#include "FusionNativeBatchActions.h"
#include "FusionNativeFireActionBatch.h"
#include "FusionNativeSummon269.h"
#include "FusionSummon269Rules.h"
#include "FusionNativeWater268.h"
#include "FusionSourceZombieDeps269.h"
#include "NativeSpecialZombie269.h"
#include "FusionNativeEffects269.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "../../Sexy.TodLib/TodFoley.h"

namespace LawnFusion
{
namespace
{
// Deliberate native projection, not a claim about Unity's unresolved builtin
// root-motion pass. The raw curve is scaled and reflected exactly once.
constexpr double kRootPixels = .31 * 72.0 / 1.35;

bool CurrentBoard(Board* board)
{
    if (!board || !board->mApp || board->mApp->mBoard != board || !IsNativeFusionBoard(board) ||
        !NativeFusionBackgroundValid(uint8_t(board->mBackground))) return false;
    for (int row=0; row<GetNativeFusionRowCount(board); ++row)
        if (board->mPlantRow[row] != PLANTROW_NORMAL&&board->mPlantRow[row]!=PLANTROW_POOL&&board->mPlantRow[row]!=PLANTROW_DIRT) return false;
    return true;
}
bool HostPlaying(Zombie* zombie)
{
    return zombie && CurrentBoard(zombie->mBoard) && zombie->mApp == zombie->mBoard->mApp &&
        !zombie->mApp->NetIsClient() && !zombie->mNetGhost && !zombie->mDead &&
        zombie->mApp->mGameScene == SCENE_PLAYING && !zombie->mBoard->mPaused;
}
bool FlatLive(Zombie* zombie)
{
    return HostPlaying(zombie) && !zombie->IsDeadOrDying() && zombie->mBodyHealth>0 &&
        zombie->mZombiePhase == PHASE_ZOMBIE_NORMAL && zombie->mZombieHeight == HEIGHT_ZOMBIE_NORMAL &&
        !zombie->mOnHighGround && zombie->mRow>=0 && zombie->mRow<GetNativeFusionRowCount(zombie->mBoard)&&
        !NativeWater268ZombieCaptured(zombie);
}
Reanimation* Body(const Zombie* zombie)
{ return zombie && zombie->mApp ? zombie->mApp->ReanimationTryToGet(zombie->mBodyReanimID) : nullptr; }
NativeSourceZombieCursor* Cursor(Zombie* zombie)
{
    auto* context=GetNativeContext(zombie->mBoard);
    return context ? &context->sourceZombieCursors[zombie->mBoard->mZombies.DataArrayGetID(zombie)] : nullptr;
}
bool ReadPosition(const Reanimation* body,float duration,FusionZombieClipPosition& output)
{
    if (!body || body->mDead || body->mFrameCount<=0 || body->mLoopCount<0 ||
        !std::isfinite(body->mAnimTime) || body->mAnimTime<0 || body->mAnimTime>1) return false;
    output={static_cast<uint32_t>(body->mLoopCount),body->mAnimTime*duration};
    return true;
}
bool SpeedFactor(Zombie* zombie,const NativeSourceZombieData& data,float& factor)
{
    const auto* garlic=GetNativeZombieGarlicData(zombie);
    return TryGetFusionZombieStatusSpeedFactor(data.kind,zombie->mChilledCounter>0,
        zombie->mIceTrapCounter>0,zombie->mButteredCounter>0,
        garlic && FusionGarlicStopsWalking(*garlic),factor);
}
bool WalkCurve2(const NativeSourceZombieData& data)
{return data.kind==111||(data.kind!=109&&data.originSpeed>kFusionZombieWalk2Threshold);}
float WalkDuration(uint16_t kind){return kind==109?2.f:kFusionZombieWalkClipSeconds;}
float AttackDuration(uint16_t kind){return kind==109?2.9166667f:kFusionZombieAttackClipSeconds;}
uint16_t MotionKind(const NativeSourceZombieData& data)
{return data.kind==4||data.kind==111?0:data.kind;}
bool EatingBlocked(Zombie* zombie)
{
    const auto* garlic=GetNativeZombieGarlicData(zombie);
    return zombie->IsImmobilizied() || (garlic && FusionGarlicPreventsEating(*garlic));
}
void ClearTarget(Zombie* zombie,NativeSourceZombieData& data)
{
    data.targetEntity=NativeZombieTargetEntity::None; data.targetId=0; data.targetGeneration=0;
    if (auto* cursor=Cursor(zombie)) cursor->attackValid=false;
    if (zombie->mIsEating) zombie->StopEating();
}
Plant* LockedPlant(Zombie* zombie,const NativeSourceZombieData& data)
{
    if (data.targetEntity != NativeZombieTargetEntity::Plant || zombie->mMindControlled) return nullptr;
    auto* plant=zombie->mBoard->mPlants.DataArrayTryToGet(data.targetGeneration);
    if (!plant || plant->mNetId != data.targetId || plant->mBoard != zombie->mBoard || plant->mDead ||
        !plant->mIsOnBoard || plant->mSquished || plant->mPlantHealth<=0 || plant->mRow != zombie->mRow ||
        NativeBatchPlantUntargetable(plant) || NativeFireActionUntargetable(plant)) return nullptr;
    // Acquisition uses native layer priority. Continuation tests this original
    // collider, rather than letting a new cover layer steal a locked target.
    return GetRectOverlap(zombie->GetZombieAttackRect(),plant->GetPlantRect())>0 ? plant : nullptr;
}
Zombie* LockedZombie(Zombie* zombie,const NativeSourceZombieData& data)
{
    if (data.targetEntity != NativeZombieTargetEntity::Zombie) return nullptr;
    auto* target=zombie->mBoard->mZombies.DataArrayTryToGet(data.targetGeneration);
    if (!target || target == zombie || target->mNetId != data.targetId || target->mDead ||
        target->IsDeadOrDying() || target->mBodyHealth<=0 || target->mMindControlled == zombie->mMindControlled ||
        target->mRow != zombie->mRow || target->IsFlying()||NativeWater268ZombieCaptured(target)) return nullptr;
    return GetRectOverlap(zombie->GetZombieAttackRect(),target->GetZombieRect())>0 ? target : nullptr;
}
bool Acquire(Zombie* zombie,NativeSourceZombieData& data)
{
    if (auto* target=zombie->FindZombieTarget())
    {
        if (!target->mNetId) return false;
        data.targetEntity=NativeZombieTargetEntity::Zombie; data.targetId=target->mNetId;
        data.targetGeneration=zombie->mBoard->mZombies.DataArrayGetID(target);
    }
    else if (!zombie->mMindControlled)
    {
        auto* plant=zombie->FindPlantTarget(ATTACKTYPE_CHEW);
        if (!plant || !plant->mNetId) return false;
        data.targetEntity=NativeZombieTargetEntity::Plant; data.targetId=plant->mNetId;
        data.targetGeneration=zombie->mBoard->mPlants.DataArrayGetID(plant);
    }
    else return false;
    zombie->StartEating();
    auto* cursor=Cursor(zombie); auto* body=Body(zombie);
    if (!cursor || !ReadPosition(body,AttackDuration(data.kind),cursor->attack))
    { ClearTarget(zombie,data); return false; }
    cursor->attackFrameStart=body->mFrameStart; cursor->attackValid=true; cursor->motionValid=false;
    UpdateNativeSourceZombieAnimation(zombie);
    LawnNet::NetTrace("Fusion source Zombie target id=%u kind=%u entity=%u target=%u generation=%u tick=%d",
        zombie->mNetId,data.kind,static_cast<unsigned>(data.targetEntity),data.targetId,data.targetGeneration,zombie->mBoard->mMainCounter);
    return true;
}
}

bool IsNativeSourceZombie(const Zombie* zombie)
{
    const auto* data=GetNativeSourceZombieData(zombie);
    return data && CurrentBoard(zombie->mBoard) && zombie->mApp == zombie->mBoard->mApp &&
        FindFusionZombieProfile(data->kind) &&
        int(zombie->mZombieType)==GetNativeSourceZombieCarrier(data->kind);
}

int GetNativeSourceZombieCarrier(uint16_t kind)
{
    switch(kind){case 0:case 11:case 41:case 42:return ZOMBIE_NORMAL;case 2:case 12:return ZOMBIE_TRAFFIC_CONE;
    case 4:case 13:case 111:return ZOMBIE_PAIL;case 109:return ZOMBIE_FOOTBALL;case 8:return ZOMBIE_DOOR;
    case 15:case 104:return ZOMBIE_NEWSPAPER;case 14:case 18:return ZOMBIE_ZAMBONI;default:return -1;}
}
bool ApplyNativeSourceZombie111Armor(Zombie* zombie,int damage,int& overkill)
{
    auto* data=GetNativeSourceZombieData(zombie);if(!data||(data->kind!=111&&data->kind!=109))return false;
    if(!TryApplyNativeSourceZombie111Armor(zombie,damage,overkill))return false;
    // Source lethal branch clears the armor reference without Broken(). Its
    // last head sprite therefore survives, even on a direct2200->0 hit.
    if(HostPlaying(zombie)&&zombie->mHelmHealth>0)
    {
        int stage=0,slot=-1;
        if(TryGetFusionSourceZombieDeps269ArmorStage(data->kind,zombie->mHelmHealth,zombie->mHelmMaxHealth,data->firstArmorStage,stage,slot))
            data->firstArmorStage=static_cast<uint8_t>(stage);
    }
    return true;
}
bool EmitNativeSpecialZombie269VehicleFx(Zombie* zombie,uint16_t kind,float x,float y)
{
    if(!IsNativeSpecialZombie269Vehicle(zombie)||(kind!=35&&kind!=36)||
       !HostPlaying(zombie))return false;
    return SpawnNativeSourceEffect269(zombie->mBoard,kind,x,y,zombie->mRenderOrder+1,true)!=nullptr;
}
bool GetNativeSpecialZombie269PlantTraits(const Plant* plant,bool& caltrop,bool& nut,int& level)
{
    const auto* data=GetNativePlantData(plant);
    return data&&TryGetSpecialZombie269SourcePlantTraits(data->kind,caltrop,nut,level);
}
void RefreshNativeSpecialZombie269PaperArmor(Zombie* zombie)
{
    auto* data=GetNativeSourceZombieData(zombie);
    if(!data||(data->kind!=15&&data->kind!=104)||!HostPlaying(zombie)||
        zombie->mShieldHealth<=0||zombie->mShieldMaxHealth<=0)return;
    const int hp=zombie->mShieldHealth,maximum=zombie->mShieldMaxHealth;
    if(hp<int(int64_t(maximum)*2/3)&&data->firstArmorStage<1)data->firstArmorStage=1;
    if(hp<maximum/3&&data->firstArmorStage<2)data->firstArmorStage=2;
}
Zombie* SpawnNativeSourceZombie(Board* board,uint16_t kind,int row,int fromWave,int preset)
{
    NativeSourceZombieSpawnRequest request;request.kind=kind;request.row=row;request.fromWave=fromWave;request.sourcePreset=preset;
    return SpawnNativeSourceZombie(board,request);
}
Zombie* TrySpawnNativeAdventureWaveVariant(Board* board, int nativeType, int row, int wave, bool allowVehicle)
{
    if (!CurrentBoard(board) || board->mApp->NetIsClient() || !board->mApp->IsFusionAdventure() ||
        GetNativeContext(board)->validation || board->mApp->mGameScene != SCENE_PLAYING || board->mPaused ||
        board->mTutorialState != TUTORIAL_OFF || wave < 0 || wave >= board->mNumWaves ||
        !board->IsValidGridCell(0, row)) return nullptr;
    const auto terrain = board->mPlantRow[row];
    if (terrain != PLANTROW_NORMAL && terrain != PLANTROW_POOL) return nullptr;
    const bool water = terrain == PLANTROW_POOL;
    const bool vehicle = allowVehicle && board->StageHasPool() && board->IsFlagWave(wave);
    NativeSourceZombieSpawnRequest request;
    request.row = row;
    request.fromWave = wave;
    switch (nativeType)
    {
    case ZOMBIE_NORMAL: request.kind = water ? 11 : 0; break;
    case ZOMBIE_TRAFFIC_CONE: request.kind = water ? 12 : 2; break;
    case ZOMBIE_PAIL: request.kind = vehicle ? (water ? 14 : 18) : (water ? 13 : 4); break;
    case ZOMBIE_DOOR: if (water) return nullptr; request.kind = 8; break;
    case ZOMBIE_NEWSPAPER: if (water) return nullptr; request.kind = board->IsFlagWave(wave) ? 104 : 15; break;
    default: return nullptr;
    }
    auto* zombie = SpawnNativeSourceZombie(board, request);
    if (!zombie && vehicle && nativeType == ZOMBIE_PAIL)
        zombie = SpawnNativeSourceZombie(board, water ? 13 : 4, row, wave);
    if (const auto* data = GetNativeSourceZombieData(zombie))
        LawnNet::NetTrace("Fusion Adventure wave variant wave=%d row=%d native=%d kind=%u vehicleAllowed=%d vehicle=%d id=%u",
            wave,row,nativeType,data->kind,vehicle?1:0,(data->kind==14||data->kind==18)?1:0,zombie->mNetId);
    return zombie;
}

Zombie* SpawnNativeSourceZombie(Board* board,const NativeSourceZombieSpawnRequest& request)
{
    const auto kind=request.kind;const int row=request.row,fromWave=request.fromWave,preset=request.sourcePreset;
    const auto* profile=FindFusionZombieProfile(kind);
    if (!profile || preset != kFusionZombieSourcePreset || !CurrentBoard(board) || board->mApp->NetIsClient() ||
        board->mApp->mGameScene != SCENE_PLAYING || board->mPaused || row<0 || row>=GetNativeFusionRowCount(board) ||
        fromWave<0 || !board->RowCanHaveZombies(row) || board->mZombies.mSize>=board->mZombies.mMaxSize-1||
        (request.positioned&&!std::isfinite(request.shadowX))||bool(request.parentId)!=bool(request.parentToken)||
        (request.dieWithParent&&!request.parentId)||(IsFusionSummon269Money(kind)&&request.friendly)||
        (profile->waterOnly&&board->mPlantRow[row]!=PLANTROW_POOL)||
        ((kind==8||IsFusionSummon269Money(kind))&&board->mPlantRow[row]==PLANTROW_POOL)) return nullptr;
    if(request.parentId)
    {
        auto* parent=board->mPlants.DataArrayTryToGet(request.parentToken);const auto* identity=GetNativePlantData(parent);
        if(!parent||parent->mDead||parent->mNetId!=request.parentId||!identity||identity->instanceToken!=request.parentToken)return nullptr;
    }
    if(kind==111&&(request.withEffect||board->mPlantRow[row]==PLANTROW_POOL))
    {
        LawnNet::NetTrace("Fusion source111 spawn bounded refusal row=%d effect=%d reason=onlyGroundWithEffectFalseAdmitted",row,request.withEffect?1:0);
        return nullptr;
    }
    if((kind==109||kind==18)&&board->mPlantRow[row]==PLANTROW_POOL)return nullptr;
    if((kind==14||kind==18)&&!EnsureNativeSourceEffect269(board,kind==14?35:36))return nullptr;
    std::string resourceError;
    if (!EnsureNativeSourceZombieVisuals(board,kind,&resourceError))
    {
        LawnNet::NetTrace("Fusion source Zombie resource rejected kind=%u reason=%s",kind,resourceError.c_str());
        return nullptr;
    }
    FusionZombieSpawn spawn;
    if (!TryPrepareFusionZombieSpawn(kind,preset,RandRangeFloat(kFusionZombieMinimumOriginSpeed,
        kFusionZombieMaximumOriginSpeed),spawn)) return nullptr;
    auto* zombie=board->AddZombieInRow(static_cast<ZombieType>(GetNativeSourceZombieCarrier(kind)),row,fromWave);
    if (!zombie) return nullptr;
    if(kind!=0&&kind!=2)
    {
        // Allocation already applied native co-op/endless health scaling.
        // Zamboni's base body is1350; dividing every carrier by270 would
        // multiply both source vehicles byfive before their first Update.
        const float carrierHealth=zombie->mZombieType==ZOMBIE_ZAMBONI?1350.f:270.f;
        const float scale=float(zombie->mBodyMaxHealth)/carrierHealth;
        zombie->mBodyHealth=zombie->mBodyMaxHealth=int(profile->maxHealth*scale);
        zombie->mHelmHealth=zombie->mHelmMaxHealth=int(profile->firstArmor*scale);
        zombie->mShieldHealth=zombie->mShieldMaxHealth=int(profile->secondArmor*scale);
    }
    NativeSourceZombieData data;
    data.kind=kind; data.instanceToken=board->mZombies.DataArrayGetID(zombie);
    data.originSpeed=spawn.originSpeed;
    data.sourceParentId=request.parentId;data.sourceParentToken=request.parentToken;data.dieWithParent=request.dieWithParent;
    // Preserve AddZombieInRow's existing co-op/endless health scaling. The native
    // base aliases already have source270 body / source2 cone370 before scaling.
    data.health=static_cast<float>(zombie->mBodyHealth);
    if (!AttachNativeSourceZombieData(board,zombie,data))
    { zombie->DieNoLoot(); return nullptr; }
    if(FindSpecialZombie269Profile(kind))
    {
        if(!InitializeNativeSpecialZombie269(zombie)||
           (kind==104&&!InitializeNativeSpecialZombie269PaperAddon(zombie)))
        {zombie->DieNoLoot();return nullptr;}
    }
    if(request.positioned)
    {
        float x=0,y=0;if(!zombie->GetShadowCenter(x,y)){zombie->DieNoLoot();return nullptr;}
        zombie->mPosX+=request.shadowX-x;zombie->mX=int(zombie->mPosX);
        if(kind==14){zombie->mInPool=true;zombie->mZombieHeight=HEIGHT_ZOMBIE_NORMAL;}
        else {zombie->CheckForPool();zombie->CompletePoolEntry();}
        if(profile->waterOnly&&!zombie->mInPool){zombie->DieNoLoot();return nullptr;}
    }
    if(request.friendly){if(request.withEffect)zombie->StartMindControlled();else{zombie->mMindControlled=true;zombie->mLastPortalX=-1;}}
    if(profile->nativeLocomotion)zombie->StartWalkAnim(0);else StartNativeSourceZombieWalk(zombie,0);
    LawnNet::NetTrace("Fusion source Zombie spawn id=%u token=%u kind=%u preset=%d speed=%.9g body=%d max=%d helm=%d walk2=%d tick=%d",
        zombie->mNetId,data.instanceToken,kind,preset,data.originSpeed,zombie->mBodyHealth,zombie->mBodyMaxHealth,
        zombie->mHelmHealth,spawn.walk2?1:0,board->mMainCounter);
    if(kind!=0&&kind!=2)LawnNet::NetTrace("Fusion Summon269 controller id=%u kind=%u locomotion=%s sourceRootCurveParity=%s",zombie->mNetId,kind,IsFusionSummon269Money(kind)?"source-stationary":profile->nativeLocomotion?"native":"source-root",IsFusionSummon269Money(kind)?"no-root-curve":profile->nativeLocomotion?"NotClaimed":"shared-controller");
    return zombie;
}

bool UpdateNativeSourceZombieHealth(Zombie* zombie)
{
    if (!IsNativeSourceZombie(zombie) || !HostPlaying(zombie) || zombie->IsDeadOrDying()) return false;
    auto* data=GetNativeSourceZombieData(zombie);
    if (static_cast<int>(data->health)<1)
    {
        if(IsNativeSpecialZombie269Vehicle(zombie))
        {DieNativeSpecialZombie269Vehicle(zombie,EmitNativeSpecialZombie269VehicleFx);return true;}
        ClearTarget(zombie,*data);
        zombie->mBodyHealth=0;
        if(IsFusionSummon269Money(data->kind)){NativeSummon269MoneyDied(zombie);zombie->DieNoLoot();return true;}
        // Explicit native death/loot adaptation; source drop table is not enabled.
        zombie->PlayDeathAnim(0U); zombie->DropLoot();
        LawnNet::NetTrace("Fusion source Zombie death id=%u kind=%u health=%.9g tick=%d",
            zombie->mNetId,data->kind,data->health,zombie->mBoard->mMainCounter);
        return true;
    }
    FusionZombieHealthStep step;
    if (!TryStepFusionZombieHealth(data->kind,data->health,zombie->mBodyMaxHealth,!zombie->mHasHead,.01f,step)) return false;
    data->health=step.health;
    if(IsFusionSummon269Money(data->kind))
        TryStepFusionMoney269Health(data->kind,data->health,zombie->mBodyMaxHealth,float(zombie->mZombieAge)*.01f,.01f,data->health);
    zombie->mBodyHealth=NativeSourceZombieLivenessHealth(data->health);
    return false;
}

bool ApplyNativeSourceZombieBodyDamage(Zombie* zombie,int damage)
{
    if (!IsNativeSourceZombie(zombie)) return false;
    if (!HostPlaying(zombie) || zombie->IsDeadOrDying()) return true;
    auto* data=GetNativeSourceZombieData(zombie);
    float remaining=0; int projection=0;
    if (!TryApplyFusionZombieBodyDamage(data->kind,data->health,damage,remaining,projection)) return true;
    data->health=remaining;
    zombie->mBodyHealth=NativeSourceZombieLivenessHealth(remaining);
    return true; // Existing native damage-state callbacks continue after subtraction.
}

int GetNativeSourceZombieBiteDamage(const Zombie* zombie,int nativeDamage)
{
    const auto* data=GetNativeSourceZombieData(zombie);
    const auto* profile=IsNativeSourceZombie(zombie) ? FindFusionZombieProfile(data->kind) : nullptr;
    return profile ? profile->biteDamage : nativeDamage;
}

bool TryShrinkNativeSourceZombie(Zombie* zombie)
{
    if (!IsNativeSourceZombie(zombie) || !FlatLive(zombie)) return false;
    auto* data=GetNativeSourceZombieData(zombie);
    FusionZombieShrinkHealth next;
    const float scale=zombie->mScaleZombie*.5f;
    if (!NativeSourceZombieDataValid(*data,zombie->mBodyMaxHealth) || !std::isfinite(scale) || scale<=0 ||
        !TryShrinkFusionZombieHealth(data->kind,data->health,zombie->mBodyMaxHealth,next)) return false;
    // Prepare all values before the first mutation. Armor max>0 is the source
    // gate, including broken armor whose current HP is already zero.
    const int helm=zombie->mHelmMaxHealth>0 ? zombie->mHelmHealth/2 : zombie->mHelmHealth;
    const int helmMax=zombie->mHelmMaxHealth>0 ? zombie->mHelmMaxHealth/2 : zombie->mHelmMaxHealth;
    const int shield=zombie->mShieldMaxHealth>0 ? zombie->mShieldHealth/2 : zombie->mShieldHealth;
    const int shieldMax=zombie->mShieldMaxHealth>0 ? zombie->mShieldMaxHealth/2 : zombie->mShieldMaxHealth;
    data->health=next.health;
    zombie->mBodyMaxHealth=next.maximumHealth; zombie->mBodyHealth=next.livenessHealth;
    zombie->mHelmHealth=helm; zombie->mHelmMaxHealth=helmMax;
    zombie->mShieldHealth=shield; zombie->mShieldMaxHealth=shieldMax;
    zombie->mScaleZombie=scale;
    zombie->UpdateAnimSpeed();
    LawnNet::NetTrace("Fusion source Zombie shrink id=%u token=%u kind=%u health=%.9g body=%d max=%d helm=%d/%d shield=%d/%d scale=%.9g tick=%d",
        zombie->mNetId,data->instanceToken,data->kind,data->health,zombie->mBodyHealth,zombie->mBodyMaxHealth,
        helm,helmMax,shield,shieldMax,scale,zombie->mBoard->mMainCounter);
    return true;
}

bool UpdateNativeSourceZombieAnimation(Zombie* zombie)
{
    if(UpdateNativeSpecialZombie269VehicleAnimation(zombie))return true;
    if(UpdateNativeSpecialZombie269Animation(zombie))return true;
    if (!IsNativeSourceZombie(zombie)) return false;
    const auto* profile=FindFusionZombieProfile(GetNativeSourceZombieData(zombie)->kind);
    const bool money=IsFusionSummon269Money(profile->kind);
    if(profile->nativeLocomotion&&!zombie->mIsEating&&!money)return false;
    if (zombie->mZombiePhase == PHASE_ZOMBIE_DYING)
    {
        // PlayDeathAnim supplies and stores its positive native rate before
        // calling UpdateAnimSpeed. Retained live-status metadata (notably
        // Garlic Waiting) must not freeze that accepted death animation.
        // Do not clear cold/garlic state or choose another death/loot clock.
        if (auto* body=Body(zombie))
            if (std::isfinite(zombie->mOriginalAnimRate) && zombie->mOriginalAnimRate>0)
                body->mAnimRate=zombie->mOriginalAnimRate;
        return true;
    }
    if (zombie->IsDeadOrDying()) return false;
    if (!HostPlaying(zombie)) return true;
    auto* body=Body(zombie); const auto* data=GetNativeSourceZombieData(zombie);
    float factor=0,rate=0;
    if (!body || body->mFrameCount<=0 || !SpeedFactor(zombie,*data,factor)) return true;
    if(money)
    {
        // Source Money walk1s is a visual loop with no root curve and
        // applyRootMotion=false. Keep the actor stationary, not a NORMAL AI.
        zombie->mOriginalAnimRate=body->mFrameCount*data->originSpeed*factor*GetNativeWater268GrapSpeedFactor(zombie);
        body->mAnimRate=zombie->mOriginalAnimRate;return true;
    }
    const float duration=zombie->mIsEating ? AttackDuration(data->kind) : WalkDuration(data->kind);
    if(data->kind==109)rate=(zombie->mIsEating?2.f:1.7f)*data->originSpeed*factor;
    else if (zombie->mIsEating) rate=data->originSpeed*factor;
    else if (!TryGetFusionZombieWalkRate(MotionKind(*data),WalkCurve2(*data),data->originSpeed,factor,rate)) return true;
    // ApplyAnimRate would halve chill a second time. This rate already includes
    // the typed source status factor and consumes the same native animation.
    zombie->mOriginalAnimRate=body->mFrameCount*rate/duration;
    body->mAnimRate=zombie->mOriginalAnimRate;
    return true;
}

bool StartNativeSourceZombieWalk(Zombie* zombie,int blendTime)
{
    if(StartNativeSpecialZombie269Vehicle(zombie))return true;
    if(StartNativeSpecialZombie269Walk(zombie,blendTime))return true;
    if (!IsNativeSourceZombie(zombie) || zombie->IsDeadOrDying()) return false;
    const bool money=IsFusionSummon269Money(GetNativeSourceZombieData(zombie)->kind);
    if(!money&&FindFusionZombieProfile(GetNativeSourceZombieData(zombie)->kind)->nativeLocomotion)return false;
    if (!HostPlaying(zombie)) return true;
    const auto* data=GetNativeSourceZombieData(zombie); auto* body=Body(zombie);
    if(money)
    {if(body&&body->TrackExists("anim_walk")){zombie->mVelX=0;zombie->PlayZombieReanim("anim_walk",REANIM_LOOP,blendTime,0);UpdateNativeSourceZombieAnimation(zombie);}return true;}
    const char* track=WalkCurve2(*data) ? "anim_walk2" : "anim_walk";
    if (!body || !body->TrackExists(track)) return true;
    zombie->PlayZombieReanim(track,REANIM_LOOP,blendTime,0);
    if (auto* cursor=Cursor(zombie))
    {
        cursor->motionFrameStart=body->mFrameStart;
        cursor->motionValid=ReadPosition(body,WalkDuration(data->kind),cursor->motion);
        cursor->attackValid=false;
    }
    UpdateNativeSourceZombieAnimation(zombie);
    return true;
}

bool UpdateNativeSourceZombieWalking(Zombie* zombie)
{
    if(IsNativeSpecialZombie269Vehicle(zombie))return true; // Exact source position callback is outside the stock action gate.
    if(MoveNativeSpecialZombie269Paper(zombie))return true;
    if (!IsNativeSourceZombie(zombie)) return false;
    if(IsFusionSummon269Money(GetNativeSourceZombieData(zombie)->kind))return true;
    if(FindFusionZombieProfile(GetNativeSourceZombieData(zombie)->kind)->nativeLocomotion)return false;
    if (!HostPlaying(zombie)) return true;
    auto* cursor=Cursor(zombie); auto* body=Body(zombie);
    const auto* data=GetNativeSourceZombieData(zombie);
    FusionZombieClipPosition current;
    if (!cursor || !ReadPosition(body,WalkDuration(data->kind),current)) return true;
    const bool walking=FlatLive(zombie) && !zombie->mIsEating && !zombie->ZombieNotWalking();
    if (walking && cursor->motionValid && cursor->motionFrameStart==body->mFrameStart &&
        std::isfinite(zombie->mScaleZombie) && zombie->mScaleZombie>0)
    {
        double delta=0;
        if(data->kind==109)
        {
            // Actual TallNutFootball walk root:6.73->6.2499995 over2s,
            // linear Hermite slope-.24000025, source root scale1.2.
            if(current.loop>=cursor->motion.loop)
            {
                const double time=double(current.loop-cursor->motion.loop)*2+current.seconds-cursor->motion.seconds;
                delta=time*double(6.2499995f-6.73f)/2*1.2*(72.0/1.35)*zombie->mScaleZombie;
                if(zombie->mMindControlled)delta=-delta;
                if(time>=0&&std::isfinite(zombie->mPosX+delta))zombie->mPosX+=static_cast<float>(delta);
            }
        }
        else if (TryGetFusionZombieRootDelta(MotionKind(*data),WalkCurve2(*data),
            cursor->motion,current,zombie->mMindControlled,delta,(data->kind==4?.30*72.0/1.35:kRootPixels)*zombie->mScaleZombie) &&
            std::isfinite(zombie->mPosX+delta)) zombie->mPosX+=static_cast<float>(delta);
    }
    // Consume even stopped spans: restoring movement cannot replay frozen or
    // eating motion. UpdateZombiePosition retains native row/Y/edge handling.
    cursor->motion=current; cursor->motionFrameStart=body->mFrameStart; cursor->motionValid=walking;
    return true;
}

bool UpdateNativeSourceZombieAttack(Zombie* zombie)
{
    if(UpdateNativeSpecialZombie269PaperAttack(zombie))return true;
    if (!IsNativeSourceZombie(zombie)) return false;
    if (!HostPlaying(zombie)) return true;
    auto* data=GetNativeSourceZombieData(zombie); auto* cursor=Cursor(zombie); auto* body=Body(zombie);
    if (!cursor || !body) return true;
    if(FindFusionZombieProfile(data->kind)->noAttack){ClearTarget(zombie,*data);return true;}
    if (!FlatLive(zombie) || !zombie->mHasHead)
    { ClearTarget(zombie,*data); return true; }
    if (data->targetEntity==NativeZombieTargetEntity::None)
    {
        if (!EatingBlocked(zombie)) Acquire(zombie,*data);
        return true;
    }
    if (!LockedPlant(zombie,*data) && !LockedZombie(zombie,*data))
    { ClearTarget(zombie,*data); return true; } // No retarget within this exit/event.
    FusionZombieClipPosition current;
    if (!ReadPosition(body,AttackDuration(data->kind),current)) return true;
    uint32_t events=0;
    if (cursor->attackValid && cursor->attackFrameStart==body->mFrameStart)
        TryCountFusionZombieAttackEvents(data->kind,cursor->attack,current,events);
    cursor->attack=current; cursor->attackFrameStart=body->mFrameStart; cursor->attackValid=true;
    // Garlic Moving may allow walking animation but still forbids eating.
    if (EatingBlocked(zombie)) return true;
    for (uint32_t i=0;i<events;++i)
    {
        if (!FlatLive(zombie) || !zombie->mHasHead || EatingBlocked(zombie)) break;
        if (auto* plant=LockedPlant(zombie,*data))
        {
            if (!HandleNativeBatchPlantEaten(zombie,plant)) zombie->EatPlant(plant);
        }
        else if (auto* target=LockedZombie(zombie,*data)) zombie->EatZombie(target);
        else { ClearTarget(zombie,*data); break; }
        zombie->mApp->PlayFoley(FOLEY_CHOMP);
        LawnNet::NetTrace("Fusion source Zombie bite id=%u kind=%u entity=%u target=%u generation=%u loop=%u seconds=%.9g tick=%d",
            zombie->mNetId,data->kind,static_cast<unsigned>(data->targetEntity),data->targetId,
            data->targetGeneration,current.loop,current.seconds,zombie->mBoard->mMainCounter);
        if (!zombie->mIsEating) { ClearTarget(zombie,*data); break; }
    }
    return true;
}

bool GetNativeSourceZombiePresentation(const Zombie* zombie,const char*& stateName,float& normalizedPhase)
{
    if(GetNativeSpecialZombie269Presentation(zombie,stateName,normalizedPhase))return true;
    if (!IsNativeSourceZombie(zombie)) return false;
    const auto* data=GetNativeSourceZombieData(zombie); const auto* body=Body(zombie);
    if (!body || !std::isfinite(body->mAnimTime) || body->mAnimTime<0 || body->mAnimTime>1) return false;
    const char* state=const_cast<Zombie*>(zombie)->IsDeadOrDying() ? "die" : zombie->mIsEating ?
        (FindFusionZombieProfile(data->kind)->waterOnly?"attackinwater":"attack") : zombie->mInPool ? "swim" :
        data->kind==111||data->kind==109?"walk":data->originSpeed>kFusionZombieWalk2Threshold ? "walk2" : "walk";
    if(IsFusionSummon269Money(data->kind))state="walk";
    stateName=state; normalizedPhase=body->mAnimTime;
    return true;
}
}
