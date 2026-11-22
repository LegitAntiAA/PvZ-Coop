#include "FusionNativeDryGround353.h"
#include "FusionNextZombie343Rules.h"
#include "FusionNativeRoof350.h"
#include "FusionNativeExtendedZombie347.h"
#include "FusionNativeZombies.h"
#include "FusionNative.h"
#include "FusionNativeAdventure273.h"
#include "FusionNativeDayZombie273Bridge.h"
#include "FusionNativeNightZombie291Bridge.h"
#include "FusionNativeJackbox309Bridge.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeSourceLifecycle313.h"
#include "FusionNativeZombie313Dispatch.h"
#include "FusionNativeWaterZombie313Bridge.h"
#include "FusionNativeHeavyZombie313BBridge.h"
#include "FusionNativeExtendedZombie313Bridge.h"
#include "FusionNativeZombieVisual.h"
#include "FusionZombieMotion.h"
#include "FusionGarlicRules.h"
#include "FusionNativeBatchActions.h"
#include "FusionNativeFireActionBatch.h"
#include "FusionNativeSummon269.h"
#include "FusionSummon269Rules.h"
#include "FusionNativeWater268.h"
#include "FusionSourceZombieDeps269.h"
#include "FusionNightZombie281Rules.h"
#include "NativeSpecialZombie269.h"
#include "FusionNativeEffects269.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/Attachment.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include <cstring>

namespace LawnFusion
{
namespace
{
// Deliberate native projection, not a claim about Unity's unresolved builtin
// root-motion pass. The raw curve is scaled and reflected exactly once.
constexpr double kRootPixels = .31 * 72.0 / 1.35;

thread_local const Board* sourceBirthBoard=nullptr;
struct SourceBirthScope
{
    const Board* previous;
    explicit SourceBirthScope(const Board* board):previous(sourceBirthBoard){sourceBirthBoard=board;}
    ~SourceBirthScope(){sourceBirthBoard=previous;}
};

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
        garlic && !NativeSourceDeathIgnoresGarlic313(zombie) && FusionGarlicStopsWalking(*garlic),factor);
}
bool WalkCurve2(const NativeSourceZombieData& data)
{return data.kind==111||(data.kind!=109&&data.kind!=9&&data.originSpeed>kFusionZombieWalk2Threshold);}
float WalkDuration(uint16_t kind){return kind==9||kind==109?2.f:kFusionZombieWalkClipSeconds;}
float AttackDuration(uint16_t kind){return FusionDayZombie273Pole(kind)?2.25f:kind==9||kind==109?2.9166667f:kFusionZombieAttackClipSeconds;}
uint16_t MotionKind(const NativeSourceZombieData& data)
{return data.kind==4||data.kind==111?0:FusionDayZombie273Doll(data.kind)?2:data.kind;}
bool EatingBlocked(Zombie* zombie)
{
    const auto* garlic=GetNativeZombieGarlicData(zombie);
    return zombie->IsImmobilizied() || (garlic && FusionGarlicPreventsEating(*garlic));
}
void ClearTarget(Zombie* zombie,NativeSourceZombieData& data,bool sourceClockOnly=false)
{
    data.targetEntity=NativeZombieTargetEntity::None; data.targetId=0; data.targetGeneration=0;
    if(!sourceClockOnly)if (auto* cursor=Cursor(zombie)) cursor->attackValid=false;
    if(sourceClockOnly)zombie->mIsEating=false;
    else if (zombie->mIsEating) zombie->StopEating();
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
bool SourceClockPlantAllowed(Zombie* zombie,Plant* plant)
{
    bool caltrop=false,nut=false;int level=0;
    return plant&&GetNativeSpecialZombie269PlantTraits(plant,caltrop,nut,level)&&!caltrop&&
        !zombie->mBoard->GetLadderAt(plant->mPlantCol,plant->mRow);
}
bool Acquire(Zombie* zombie,NativeSourceZombieData& data,bool sourceClockOnly=false,bool assign=true)
{
    if(sourceClockOnly)
    {
        // Source contact before PointOver may trigger summon but must not bind
        // a target or switch the real native body clock. Keep the same locked
        // ID/full-generation and native collider eligibility when binding is allowed.
        auto picked=data;picked.targetEntity=NativeZombieTargetEntity::None;
        picked.targetId=0;picked.targetGeneration=0;
        if(auto* target=zombie->FindZombieTarget())
        {
            picked.targetEntity=NativeZombieTargetEntity::Zombie;picked.targetId=target->mNetId;
            picked.targetGeneration=zombie->mBoard->mZombies.DataArrayGetID(target);
            if(!target->mNetId||!LockedZombie(zombie,picked))picked.targetEntity=NativeZombieTargetEntity::None;
        }
        if(picked.targetEntity==NativeZombieTargetEntity::None&&!zombie->mMindControlled)
        {
            Plant* plant=nullptr;
            while(zombie->mBoard->IteratePlants(plant))
            {
                if(!plant->mNetId||plant->mRow!=zombie->mRow||!SourceClockPlantAllowed(zombie,plant)||
                    GetRectOverlap(zombie->GetZombieAttackRect(),plant->GetPlantRect())<20||
                    !zombie->CanTargetPlant(plant,ATTACKTYPE_CHEW))continue;
                picked.targetEntity=NativeZombieTargetEntity::Plant;picked.targetId=plant->mNetId;
                picked.targetGeneration=zombie->mBoard->mPlants.DataArrayGetID(plant);
                if(LockedPlant(zombie,picked))break;picked.targetEntity=NativeZombieTargetEntity::None;
            }
        }
        if(picked.targetEntity==NativeZombieTargetEntity::None)return false;
        if(assign)
        {
            data.targetEntity=picked.targetEntity;data.targetId=picked.targetId;data.targetGeneration=picked.targetGeneration;
            zombie->mIsEating=true; // Native bookkeeping only; Core owns the source phase.
        }
        return true;
    }
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

namespace {
bool ClockOnlyKind313(uint16_t kind)
{return kind==10||FusionJackbox300Kind(kind)||FindFusionSourceGraph313(kind)||FusionWaterZombie313Kind(kind);}
}
void NativeClockOnlyZombieClear309(Zombie* zombie)
{
    auto* data=GetNativeSourceZombieData(zombie);
    if(data&&ClockOnlyKind313(data->kind)&&!zombie->mNetGhost&&!zombie->mApp->NetIsClient())ClearTarget(zombie,*data,true);
}
bool NativeClockOnlyZombieContact309(Zombie* zombie,bool canAttack)
{
    auto* data=GetNativeSourceZombieData(zombie);
    if(!data||!ClockOnlyKind313(data->kind)||!FlatLive(zombie)||!zombie->mHasHead||NativeJackbox309RowMoving(zombie))return false;
    if(!canAttack)return Acquire(zombie,*data,true,false);
    if(auto* plant=LockedPlant(zombie,*data);plant&&SourceClockPlantAllowed(zombie,plant))
    {zombie->mIsEating=true;return true;}
    if(LockedZombie(zombie,*data)){zombie->mIsEating=true;return true;}
    ClearTarget(zombie,*data,true);
    return Acquire(zombie,*data,true,true);
}
namespace
{
// Called only after the owning source controller consumed its actual event.
// The locked real target and native Eat callbacks remain a single mechanism.
void BiteLockedTarget313(Zombie* zombie,bool legacy,unsigned phase,FusionZombieClipPosition eventPosition)
{
    auto* data=GetNativeSourceZombieData(zombie);if(!data)return;
    auto* board=zombie->mBoard;auto* app=zombie->mApp;const auto uid=board->mZombies.DataArrayGetID(zombie);
    const auto kind=data->kind;
    const auto actorId=zombie->mNetId;const auto actorToken=data->instanceToken;
    const auto entity=data->targetEntity;const auto targetId=data->targetId;const auto targetGeneration=data->targetGeneration;
    int beforeHP=0,afterHP=0;bool afterKnown=false;
    if(auto* plant=LockedPlant(zombie,*data);plant&&SourceClockPlantAllowed(zombie,plant))
    {
        beforeHP=plant->mPlantHealth;
        if(!HandleNativeBatchPlantEaten(zombie,plant))zombie->EatPlant(plant);
        if(app->mBoard==board)if(auto* actual=board->mPlants.DataArrayTryToGet(targetGeneration);actual&&actual->mNetId==targetId)
        {afterKnown=true;afterHP=actual->mPlantHealth;}
    }
    else if(auto* target=LockedZombie(zombie,*data))
    {
        beforeHP=target->mBodyHealth;zombie->EatZombie(target);
        if(app->mBoard==board)if(auto* actual=board->mZombies.DataArrayTryToGet(targetGeneration);actual&&actual->mNetId==targetId)
        {afterKnown=true;afterHP=actual->mBodyHealth;}
    }
    else{ClearTarget(zombie,*data,true);return;}
    if(legacy)
    {
    if(afterKnown)
        LawnNet::NetTrace(kind==10?"Fusion Jackson291 bite id=%u token=%u entity=%u target=%u generation=%u phase=%u loop=%u sourceSeconds=%.9g beforeHP=%d afterHP=%d actualNativeCallback=1":"Fusion Jackbox309 bite id=%u token=%u entity=%u target=%u generation=%u phase=%u loop=%u sourceSeconds=%.9g beforeHP=%d afterHP=%d actualNativeCallback=1",actorId,actorToken,unsigned(entity),targetId,targetGeneration,unsigned(phase),eventPosition.loop,eventPosition.seconds,beforeHP,afterHP);
    else
        LawnNet::NetTrace(kind==10?"Fusion Jackson291 bite id=%u token=%u entity=%u target=%u generation=%u phase=%u loop=%u sourceSeconds=%.9g beforeHP=%d targetRetired=1 actualNativeCallback=1":"Fusion Jackbox309 bite id=%u token=%u entity=%u target=%u generation=%u phase=%u loop=%u sourceSeconds=%.9g beforeHP=%d targetRetired=1 actualNativeCallback=1",actorId,actorToken,unsigned(entity),targetId,targetGeneration,unsigned(phase),eventPosition.loop,eventPosition.seconds,beforeHP);
    }
    else LawnNet::NetTrace("Fusion SourceClock313 bite id=%u token=%u kind=%u entity=%u target=%u generation=%u beforeHP=%d afterKnown=%d afterHP=%d actualNativeCallback=1",
        actorId,actorToken,kind,unsigned(entity),targetId,targetGeneration,beforeHP,afterKnown?1:0,afterHP);
    // Root's typed10 StartEating/StopEating guards retain real native bite
    // effects/counters while consuming every stock animation-track switch.
    if(app->mBoard!=board)return;
    app->PlayFoley(FOLEY_CHOMP);
    zombie=board->mZombies.DataArrayTryToGet(uid);
    if(zombie&&!zombie->mDead)
    {
        data=GetNativeSourceZombieData(zombie);
        if(data&&!LockedPlant(zombie,*data)&&!LockedZombie(zombie,*data))ClearTarget(zombie,*data,true);
    }
}
}
void NativeClockOnlyZombieBite309(Zombie* zombie)
{
    auto* data=GetNativeSourceZombieData(zombie);const auto* controller=GetNativeJackson291(zombie);
    const auto* jackbox=GetNativeJackbox309(zombie);
    if(!data||(data->kind!=10&&!FusionJackbox300Kind(data->kind))||
        (data->kind==10?(!controller||!controller->canAttack):(!jackbox||!jackbox->initialized||jackbox->failed))||
        !FlatLive(zombie)||!zombie->mHasHead||EatingBlocked(zombie)||NativeJackbox309RowMoving(zombie))return;
    const unsigned phase=controller?unsigned(controller->phase):unsigned(jackbox->phase);
    const auto eventPosition=controller?controller->eventPosition:jackbox->incomingPosition;
    BiteLockedTarget313(zombie,true,phase,eventPosition);
}
void NativeClockOnlyZombieBite313(Zombie* zombie)
{
    const auto* data=GetNativeSourceZombieData(zombie);
    if(!data||!ClockOnlyKind313(data->kind)||!FlatLive(zombie)||!zombie->mHasHead||
       EatingBlocked(zombie)||NativeJackbox309RowMoving(zombie))return;
    BiteLockedTarget313(zombie,false,0,{});
}

void ClearNativeJackson291Target(Zombie* z){NativeClockOnlyZombieClear309(z);}
bool NativeJackson291Contact(Zombie* z,bool canAttack){return NativeClockOnlyZombieContact309(z,canAttack);}
void NativeJackson291Bite(Zombie* z){NativeClockOnlyZombieBite309(z);}

bool IsNativeSourceZombieBirth(const Board* board)
{return board&&sourceBirthBoard==board&&IsNativeFusionBoard(board);}

bool CanAllocateNativeSourceDayBatch274(Board* board,std::span<const uint16_t> kinds,std::string* error)
{
    if(error)error->clear();
    const auto reject=[&](const char* why){if(error)*error=why;return false;};
    if(!CurrentBoard(board)||board->mApp->NetIsClient()||!IsNativeSourceWorld313(board))
        return reject("Source grass allocation requires the host Day Board or actual source Night world.");
    for(int row=0;row<GetNativeFusionRowCount(board);++row)if(board->mPlantRow[row]!=PLANTROW_NORMAL&&board->mPlantRow[row]!=PLANTROW_POOL)
        return reject("Source grass allocation does not cover a pool or dirt initializer.");
    size_t reanimations=0,attachments=0;
    for(auto kind:kinds)
    {
        const int carrier=GetNativeSourceZombieCarrier(kind);
        if(carrier!=ZOMBIE_NORMAL&&carrier!=ZOMBIE_TRAFFIC_CONE&&carrier!=ZOMBIE_PAIL&&
            carrier!=ZOMBIE_DOOR&&carrier!=ZOMBIE_POLEVAULTER&&carrier!=ZOMBIE_NEWSPAPER&&carrier!=ZOMBIE_FOOTBALL)
            return reject("Source Day batch contains an uncovered native carrier initializer.");
        const bool head=FusionDayZombie273Shooter(kind)||kind==104;
        const ReanimationType types[]={GetZombieDefinition(static_cast<ZombieType>(carrier)).mReanimationType,
            head?REANIM_PEASHOOTER:REANIM_NONE};
        for(auto type:types)if(type!=REANIM_NONE)
        {
            ReanimatorEnsureDefinitionLoaded(type,false);
            if(!gReanimatorDefArray||unsigned(type)>=gReanimatorDefCount)return reject("Source Day native definition is unavailable.");
            const auto& tracks=gReanimatorDefArray[int(type)].mTracks;
            if(!tracks.tracks||tracks.count<=0)return reject("Source Day native definition has no tracks.");
            for(int i=0;i<tracks.count;++i)if(tracks.tracks[i].mName&&strnicmp(tracks.tracks[i].mName,"attacher__",10)==0)
                return reject("Source Day native definition has an unbudgeted dynamic attacher.");
            ++reanimations;
        }
        if(head)++attachments;
    }
    // Loading is complete before reading availability. These synchronous host
    // factories do not pump messages or run gameplay between check and birth.
    auto* effects=board->mApp->mEffectSystem;
    if(!effects||effects!=gEffectSystem||!effects->mReanimationHolder||!effects->mAttachmentHolder)
        return reject("Source Day native effect pools are unavailable.");
    const auto& r=effects->mReanimationHolder->mReanimations;
    const auto& a=effects->mAttachmentHolder->mAttachments;
    if(!r.mBlock||!a.mBlock||r.mSize>r.mMaxSize||a.mSize>a.mMaxSize||
        reanimations>size_t(r.mMaxSize-r.mSize)||attachments>size_t(a.mMaxSize-a.mSize))
        return reject("Source Day whole-batch native animation capacity is unavailable.");
    if(board->mZombies.mSize>=board->mZombies.mMaxSize||
        kinds.size()>=size_t(board->mZombies.mMaxSize-board->mZombies.mSize))
        return reject("Source Day whole-batch native zombie capacity is unavailable.");
    return true;
}

bool IsNativeSourceZombie(const Zombie* zombie)
{
    const auto* data=GetNativeSourceZombieData(zombie);
    return data && CurrentBoard(zombie->mBoard) && zombie->mApp == zombie->mBoard->mApp &&
        FindFusionZombieProfile(data->kind) &&
        int(zombie->mZombieType)==GetNativeSourceZombieCarrier(data->kind);
}

bool ApplyNativeSourceZombie111Armor(Zombie* zombie,int damage,int& overkill)
{
    auto* data=GetNativeSourceZombieData(zombie);if(!data)return false;
    if(data->kind==9)
    {
        overkill=0;if(!HostPlaying(zombie)||zombie->IsDeadOrDying())return true;
        int after=0,excess=0;
        if(!TryStepFusionSourceZombie111Armor(zombie->mHelmHealth,damage,after,excess))return true;
        if(zombie->mHelmType!=HELMTYPE_NONE&&!after&&!PlayNativeSourceZombieArmor284(zombie,true))return true;
        uint8_t stage=data->firstArmorStage;int sprite=-1;
        if(after)TryGetFusionNightZombie281ArmorStage(9,after,zombie->mHelmMaxHealth,stage,stage,sprite);
        data->firstArmorStage=stage;zombie->mHelmHealth=after;
        if(!after)zombie->mHelmType=HELMTYPE_NONE;
        overkill=excess;return true; // Source FirstArmorFall, never stock DropHelm.
    }
    if(data->kind!=111&&data->kind!=109)return false;
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
    if(!data||!SpecialZombie269PaperKind(data->kind)||!HostPlaying(zombie)||
        zombie->mShieldHealth<=0||zombie->mShieldMaxHealth<=0)return;
    const int hp=zombie->mShieldHealth,maximum=zombie->mShieldMaxHealth;
    if(data->kind==5)
    {
        int sprite=-1;uint8_t stage=data->firstArmorStage;
        if(TryGetFusionNightZombie281ArmorStage(5,hp,maximum,stage,stage,sprite))data->firstArmorStage=stage;
        return;
    }
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
    const bool waterFamily=FusionWaterZombie313Kind(kind),heavyFamily=NativeHeavyZombie313BKind(kind);
    const bool extendedFamily=FindFusionExtendedZombie313Profile(kind)!=nullptr;
    const bool typed313=waterFamily||heavyFamily||extendedFamily;
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
    if(waterFamily)
    {
        std::string error;
        if(!CanSpawnNativeWaterZombie313(board,kind,row,request.friendly,&error))
        {LawnNet::NetTrace("Fusion Water313 spawn gated kind=%u row=%d reason=%s",kind,row,error.c_str());return nullptr;}
        // Controlled dry-row creation is an admitted no-effect constructor;
        // effectful MC uses the initialized live actor's source transaction.
        if(request.friendly&&request.withEffect&&FusionWaterZombie313SourceInWater(kind)&&board->mPlantRow[row]!=PLANTROW_POOL)return nullptr;
    }
    if(heavyFamily)
    {
        std::string error;
        if(board->mPlantRow[row]==PLANTROW_POOL||!CanSpawnNativeHeavyZombie313B(board,kind,&error))
        {LawnNet::NetTrace("Fusion Heavy313 spawn gated kind=%u row=%d reason=%s",kind,row,error.c_str());return nullptr;}
    }
    if(extendedFamily)
    {
        std::string error;
        if(!CanSpawnNativeExtendedZombie313(board,kind,&error))
        {LawnNet::NetTrace("Fusion Extended313 spawn gated kind=%u row=%d reason=%s",kind,row,error.c_str());return nullptr;}
    }
    if(FusionJackbox300Kind(kind))
    {
        std::string error;
        if(!CanSpawnNativeJackbox309(board,kind,&error))
        {LawnNet::NetTrace("Fusion Jackbox309 spawn gated kind=%u row=%d reason=%s",kind,row,error.c_str());return nullptr;}
    }
    if(kind==10)
    {
        std::string error;
        if(!CanSpawnNativeJackson291(board,&error))
        {LawnNet::NetTrace("Fusion Jackson291 spawn gated row=%d reason=%s",row,error.c_str());return nullptr;}
    }
    if((kind==5||kind==9||kind==10||kind==109||kind==18||kind==211||FusionJackbox300Kind(kind))&&!NativeSourceDryRow353(board,row))return nullptr;
    if(kind==5||kind==9)
    {
        std::string error;
        if(!CanSpawnNativeGrassZombie284(board,kind,&error))
        {LawnNet::NetTrace("Fusion source284 spawn gated kind=%u row=%d reason=%s",kind,row,error.c_str());return nullptr;}
    }
    if(FindFusionDayZombie273Profile(kind))
    {
        // These are the bounded ground families. A copied profile does not
        // authorize a spawn, a new terrain or an incomplete transitive child.
        std::string error;
        if(board->mPlantRow[row]==PLANTROW_POOL||!CanSpawnNativeDayZombie273(board,kind,&error))
        {LawnNet::NetTrace("Fusion Day273 spawn gated kind=%u row=%d reason=%s",kind,row,error.c_str());return nullptr;}
    }
    if((FusionSubmarine343Kind(kind)||kind==18)&&!EnsureNativeSourceEffect269(board,FusionSubmarine343Kind(kind)?35:36))return nullptr;
    std::string resourceError;
    if (!EnsureNativeSourceZombieVisuals(board,kind,&resourceError))
    {
        LawnNet::NetTrace("Fusion source Zombie resource rejected kind=%u reason=%s",kind,resourceError.c_str());
        return nullptr;
    }
    const int nativeCarrier=GetNativeSourceZombieCarrier(kind);
    if(typed313||kind==200||kind==211)
    {
        FusionSourceDeathBudget319 peak;const uint16_t batch[]={kind};
        if(!GetNativeSourceZombieBirthBudget319(board,batch,peak,&resourceError))return nullptr;
        if(typed313&&request.friendly&&request.withEffect)
        {
            FusionSourceDeathBudget319 mc,combined;
            if(!NativeZombie313MindControlAllowed(kind,0,0)||!PrepareNativeSourceMindControl313(board,mc,&resourceError)||
               !AddFusionSourceDeathBudget319(peak,mc,combined))return nullptr;
            peak=combined;
        }
        if(!CanAllocateNativeSourceBudget319(board,peak,&resourceError))
        {LawnNet::NetTrace("Fusion Source313 allocation deferred kind=%u reason=%s",kind,resourceError.c_str());return nullptr;}
    }
    else if(IsNativeSourceWorld313(board)&&
        (nativeCarrier==ZOMBIE_NORMAL||nativeCarrier==ZOMBIE_TRAFFIC_CONE||nativeCarrier==ZOMBIE_PAIL||
         nativeCarrier==ZOMBIE_DOOR||nativeCarrier==ZOMBIE_POLEVAULTER||nativeCarrier==ZOMBIE_NEWSPAPER||nativeCarrier==ZOMBIE_FOOTBALL))
    {
        // Includes transitive births such as SilverDoll23 -> ordinary Cone2.
        // Source metadata is attached after the native initializer, so every
        // covered factory must guard its peak before that initializer can run.
        const uint16_t batch[]={kind};
        if(!CanAllocateNativeSourceDayBatch274(board,batch,&resourceError))
        {LawnNet::NetTrace("Fusion source Day allocation deferred kind=%u reason=%s",kind,resourceError.c_str());return nullptr;}
    }
    FusionZombieSpawn spawn;
    if (!TryPrepareFusionZombieSpawn(kind,preset,RandRangeFloat(kFusionZombieMinimumOriginSpeed,
        kFusionZombieMaximumOriginSpeed),spawn)) return nullptr;
    Zombie* zombie=nullptr;
    {
        // Source packages provide their complete character artwork. Native
        // roster avatars would be invisible extra allocations on this carrier.
        SourceBirthScope birth(board);
        zombie=board->AddZombieInRow(static_cast<ZombieType>(GetNativeSourceZombieCarrier(kind)),row,fromWave);
    }
    if (!zombie) return nullptr;
    // Typed source factories need stable identities offline as well: source
    // targets and standalone loss events outlive the raw native allocation.
    if(!zombie->mNetId)zombie->mNetId=board->NetAllocId();
    if(!zombie->mNetId){zombie->DieNoLoot();return nullptr;}
    if(kind!=0&&kind!=2)
    {
        // Allocation already applied native co-op/endless health scaling.
        // Zamboni's base body is1350; dividing every carrier by270 would
        // multiply both source vehicles byfive before their first Update.
        // The real Pole carrier initializes at500; only Normal/Cone/other
        // admitted body aliases initialize at270. Never scale its720 by500/270.
        const float carrierHealth=zombie->mZombieType==ZOMBIE_ZAMBONI?1350.f:
            zombie->mZombieType==ZOMBIE_POLEVAULTER?500.f:270.f;
        const float scale=float(zombie->mBodyMaxHealth)/carrierHealth;
        zombie->mBodyHealth=zombie->mBodyMaxHealth=int(profile->maxHealth*scale);
        zombie->mHelmHealth=zombie->mHelmMaxHealth=int(profile->firstArmor*scale);
        zombie->mShieldHealth=zombie->mShieldMaxHealth=int(profile->secondArmor*scale);
    }
    // Normal carriers have no native helmet by default; source armor still
    // needs the existing TakeHelmDamage path before body subtraction.
    if(kind==106)zombie->mHelmType=HELMTYPE_BUCKET;
    if(kind==103||kind==107)zombie->mHelmType=HELMTYPE_WALLNUT;
    if(kind==30)zombie->mHelmType=HELMTYPE_TRAFFIC_CONE; // Covered armor damage path; all artwork/callbacks remain source.
    if(typed313&&profile->firstArmor)zombie->mHelmType=HELMTYPE_BUCKET;
    if(typed313&&profile->secondArmor)zombie->mShieldType=kind==33?SHIELDTYPE_LADDER:SHIELDTYPE_DOOR;
    NativeSourceZombieData data;
    data.kind=kind; data.instanceToken=board->mZombies.DataArrayGetID(zombie);
    data.originSpeed=spawn.originSpeed;
    data.sourceParentId=request.parentId;data.sourceParentToken=request.parentToken;data.dieWithParent=request.dieWithParent;
    // Preserve AddZombieInRow's existing co-op/endless health scaling. The native
    // base aliases already have source270 body / source2 cone370 before scaling.
    data.health=static_cast<float>(zombie->mBodyHealth);
    if (!AttachNativeSourceZombieData(board,zombie,data))
    { zombie->DieNoLoot(); return nullptr; }
    NativeSourceZombieInitialization313 sourceInitialization(zombie);
    if(board->StageHasRoof())
    {
        if(request.positioned)
        {
            float x=0,y=0;if(!zombie->GetShadowCenter(x,y))return nullptr;
            zombie->mPosX+=request.shadowX-x;zombie->mX=int(zombie->mPosX);
        }
        if(!ReconcileNativeRoofZombie350(zombie))return nullptr;
    }
    if(FindSpecialZombie269Profile(kind))
    {
        if(!InitializeNativeSpecialZombie269(zombie)||
           (kind==104&&!InitializeNativeSpecialZombie269PaperAddon(zombie)))
        {zombie->DieNoLoot();return nullptr;}
    }
    if(FindFusionDayZombie273Profile(kind)&&!InitializeNativeDayZombie273(zombie))
    {zombie->DieNoLoot();return nullptr;}
    if(request.positioned&&!typed313)
    {
        float x=0,y=0;if(!zombie->GetShadowCenter(x,y)){zombie->DieNoLoot();return nullptr;}
        zombie->mPosX+=request.shadowX-x;zombie->mX=int(zombie->mPosX);
        if(FusionSubmarine343Kind(kind)){zombie->mInPool=board->mPlantRow[row]==PLANTROW_POOL;zombie->mZombieHeight=HEIGHT_ZOMBIE_NORMAL;}
        else {zombie->CheckForPool();zombie->CompletePoolEntry();}
        if(profile->waterOnly&&!zombie->mInPool){zombie->DieNoLoot();return nullptr;}
    }
    if(request.friendly&&(!typed313||!request.withEffect))
    {if(request.withEffect)zombie->StartMindControlled();else{zombie->mMindControlled=true;zombie->mLastPortalX=-1;}}
    if(typed313&&request.positioned)
    {
        float x=0,y=0;if(!GetNativeSourceZombie269NodePoint(zombie,"Shadow",x,y))return nullptr;
        zombie->mPosX+=request.shadowX-x;zombie->mX=int(zombie->mPosX);
    }
    if(!ReconcileNativeRoofZombie350(zombie))return nullptr;
    if(waterFamily&&!InitializeNativeWaterZombie313Actor(zombie))return nullptr;
    if(heavyFamily&&!InitializeNativeHeavyZombie313BActor(zombie))return nullptr;
    if(extendedFamily&&!InitializeNativeExtendedZombie313Bridge(zombie))return nullptr;
    if(typed313&&request.friendly&&request.withEffect)
    {
        auto* app=board->mApp;zombie->StartMindControlled();
        if(app->mBoard!=board||board->mZombies.DataArrayTryToGet(data.instanceToken)!=zombie||
           zombie->mDead||!zombie->mMindControlled)return nullptr;
    }
    if(FusionJackbox300Kind(kind)&&!InitializeNativeJackbox309(zombie))
    {zombie->DieNoLoot();return nullptr;}
    if(kind==10&&!InitializeNativeJackson291(zombie))
    {zombie->DieNoLoot();return nullptr;}
    if(!typed313){if(profile->nativeLocomotion)zombie->StartWalkAnim(0);else StartNativeSourceZombieWalk(zombie,0);}
    if(extendedFamily&&!CommitNativeExtended347Factory(zombie))return nullptr;
    sourceInitialization.Commit();
    RegisterNativeSourceAdventureEnemy273(zombie);
    LawnNet::NetTrace("Fusion source Zombie spawn id=%u token=%u kind=%u preset=%d speed=%.9g body=%d max=%d helm=%d walk2=%d tick=%d",
        zombie->mNetId,data.instanceToken,kind,preset,data.originSpeed,zombie->mBodyHealth,zombie->mBodyMaxHealth,
        zombie->mHelmHealth,spawn.walk2?1:0,board->mMainCounter);
    if(kind!=0&&kind!=2)LawnNet::NetTrace("Fusion Summon269 controller id=%u kind=%u locomotion=%s sourceRootCurveParity=%s",zombie->mNetId,kind,IsFusionSummon269Money(kind)?"source-stationary":profile->nativeLocomotion?"native":"source-root",IsFusionSummon269Money(kind)?"no-root-curve":profile->nativeLocomotion?"NotClaimed":"shared-controller");
    return zombie;
}

bool UpdateNativeSourceZombieHealth(Zombie* zombie)
{
    if (!IsNativeSourceZombie(zombie) || !HostPlaying(zombie) || zombie->IsDeadOrDying()) return false;
    if(UpdateNativeDayZombie273PendingHeadLoss(zombie))return true;
    auto* data=GetNativeSourceZombieData(zombie);
    if (static_cast<int>(data->health)<1)
    {
        if(IsNativeSpecialZombie269Vehicle(zombie))
        {DieNativeSpecialZombie269Vehicle(zombie,EmitNativeSpecialZombie269VehicleFx);return true;}
        ClearTarget(zombie,*data);
        zombie->mBodyHealth=0;
        if(IsFusionSummon269Money(data->kind)){NativeSummon269MoneyDied(zombie);zombie->DieNoLoot();return true;}
        // Explicit native death/loot adaptation; source drop table is not enabled.
        auto* board=zombie->mBoard;auto* app=zombie->mApp;
        const auto uid=board->mZombies.DataArrayGetID(zombie);const auto id=zombie->mNetId;
        const auto kind=data->kind;const float health=data->health;const int tick=board->mMainCounter;
        zombie->PlayDeathAnim(0U);
        // Source30's real death callback can recursively retire this exact
        // actor before PlayDeathAnim returns. Never use the old pointer/token.
        if(app->mBoard==board)
            if(auto* live=board->mZombies.DataArrayTryToGet(uid);live&&!live->mDead)live->DropLoot();
        LawnNet::NetTrace("Fusion source Zombie death id=%u kind=%u health=%.9g tick=%d",
            id,kind,health,tick);
        return true;
    }
    FusionZombieHealthStep step;
    if (!TryStepFusionZombieHealth(data->kind,data->health,zombie->mBodyMaxHealth,data->beforeDying,.01f,step)) return false;
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
    if(remaining<float(zombie->mBodyMaxHealth)/3.f)data->beforeDying=true;
    zombie->mBodyHealth=NativeSourceZombieLivenessHealth(remaining);
    return true; // Existing native damage-state callbacks continue after subtraction.
}

int GetNativeSourceZombieBiteDamage(const Zombie* zombie,int nativeDamage)
{
    const auto* data=GetNativeSourceZombieData(zombie);
    const auto* profile=IsNativeSourceZombie(zombie) ? FindFusionZombieProfile(data->kind) : nullptr;
    return profile ? NativeExtended347AttackDamage(zombie,profile->biteDamage) : nativeDamage;
}

bool ApplyNativeSourceZombieDeathClock284(Zombie* zombie)
{
    if(UpdateNativeJackbox309Animation(zombie))return true;
    if(UpdateNativeJackson291Animation(zombie))return true;
    if(!IsNativeSourceZombie(zombie)||!HostPlaying(zombie)||zombie->mZombiePhase!=PHASE_ZOMBIE_DYING)return false;
    const auto* data=GetNativeSourceZombieData(zombie);auto* body=Body(zombie);
    if(!body||body->mFrameCount<=0)return false;
    float seconds=0,multiplier=0;
    if(data->kind==5){seconds=.8333333f;multiplier=.5f;}
    else if(data->kind==9){seconds=1.4166666f;multiplier=1.2f;}
    else if(data->kind==6||data->kind==7)
    {FusionDayZombie273Clip clip;if(!TryGetFusionDayZombie273Clip(data->kind,1,false,clip))return false;
     seconds=clip.seconds;multiplier=clip.multiplier;}
    else return false;
    const float rate=body->mFrameCount*multiplier*data->originSpeed/seconds;
    if(!std::isfinite(rate)||rate<=0)return false;
    zombie->mOriginalAnimRate=rate;body->mAnimRate=rate;return true;
}

bool UpdateNativeSourceZombieDeath284(Zombie* zombie)
{
    if(UpdateNativeJackbox309(zombie))return true;
    if(UpdateNativeJackson291(zombie))return true;
    if(!IsNativeSourceZombie(zombie))return false;
    const auto* data=GetNativeSourceZombieData(zombie);
    if(data->kind!=5&&data->kind!=6&&data->kind!=7&&data->kind!=9)return false;
    if(zombie->mZombiePhase!=PHASE_ZOMBIE_DYING)return false;
    if(!HostPlaying(zombie))return true;
    auto* body=Body(zombie);if(!body){zombie->DieNoLoot();return true;}
    const float fall=data->kind==5?.43333334f/.8333333f:data->kind==9?
        1.0833334f/1.4166666f:1.5f/2.1666667f;
    if(body->ShouldTriggerTimedEvent(fall))zombie->mApp->PlayFoley(FOLEY_ZOMBIE_FALLING);
    if(body->mLoopCount>0)zombie->DieNoLoot(); // Source DestoryZombie at actual clip end.
    return true;
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
    if(UpdateNativeJackbox309Animation(zombie))return true;
    if(UpdateNativeJackson291Animation(zombie))return true;
    if(UpdateNativeDayZombie273Animation(zombie))return true;
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
    if(data->kind==9||data->kind==109)rate=(zombie->mIsEating?2.f:1.7f)*data->originSpeed*factor;
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
    if(StartNativeJackbox309Walk(zombie))return true;
    if(StartNativeJackson291Walk(zombie))return true;
    if(StartNativeDayZombie273Walk(zombie,blendTime))return true;
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
    if(MoveNativeJackbox309(zombie))return true;
    if(MoveNativeJackson291(zombie))return true;
    if(MoveNativeDayZombie273(zombie))return true;
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
        if(data->kind==9||data->kind==109)
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
    if(UpdateNativeJackbox309(zombie))return true;
    if(UpdateNativeJackson291(zombie))return true;
    if(UpdateNativeSpecialZombie269PaperAttack(zombie))return true;
    if (!IsNativeSourceZombie(zombie)) return false;
    if (!HostPlaying(zombie)) return true;
    auto* data=GetNativeSourceZombieData(zombie); auto* cursor=Cursor(zombie); auto* body=Body(zombie);
    if (!cursor || !body) return true;
    if(!NativeDayZombie273CanUseSharedBite(zombie)){ClearTarget(zombie,*data);return true;}
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
    if(const auto* data=GetNativeSourceZombieData(zombie);data&&FusionJackbox300Kind(data->kind))
        return GetNativeJackbox309Presentation(zombie,stateName,normalizedPhase);
    if(const auto* data=GetNativeSourceZombieData(zombie);data&&data->kind==10)
        return GetNativeJackson291Presentation(zombie,stateName,normalizedPhase);
    if(GetNativeDayZombie273Presentation(zombie,stateName,normalizedPhase))return true;
    if(GetNativeSpecialZombie269Presentation(zombie,stateName,normalizedPhase))return true;
    if (!IsNativeSourceZombie(zombie)) return false;
    const auto* data=GetNativeSourceZombieData(zombie); const auto* body=Body(zombie);
    if (!body || !std::isfinite(body->mAnimTime) || body->mAnimTime<0 || body->mAnimTime>1) return false;
    const char* state=const_cast<Zombie*>(zombie)->IsDeadOrDying() ? "die" : zombie->mIsEating ?
        (data->kind==9?"eat":FindFusionZombieProfile(data->kind)->waterOnly?"attackinwater":"attack") : zombie->mInPool ? "swim" :
        data->kind==111||data->kind==109||data->kind==9?"walk":data->originSpeed>kFusionZombieWalk2Threshold ? "walk2" : "walk";
    if(IsFusionSummon269Money(data->kind))state="walk";
    stateName=state; normalizedPhase=body->mAnimTime;
    return true;
}
}
