#include "FusionNativeAdventureProgress273.h"
#include "FusionNative.h"
#include "FusionNativeVisual.h"
#include "FusionNativeGlove.h"
#include "FusionNativeEquipment268.h"
#include "FusionNativeDroppedCards269.h"
#include "FusionBasePlantRules.h"
#include "../Board.h"
#include "../Coin.h"
#include "../Plant.h"
#include "../MessageWidget.h"
#include "../System/Music.h"
#include "../Net/NetManager.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../../Sexy.TodLib/Attachment.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace LawnFusion
{
namespace
{
bool Current(const Board* b)
{return b&&b->mApp&&b->mApp->mBoard==b&&IsNativeFusionBoard(b);}
bool Host(const Board* b){return Current(b)&&!b->mApp->NetIsClient();}
bool Playing(const Board* b)
{return Host(b)&&b->mApp->mGameScene==SCENE_PLAYING&&!b->mPaused&&!b->mApp->GetDialogCount();}
bool Actor(const Plant* p)
{return p&&!p->mDead&&!p->mNetGhost&&p->mIsOnBoard&&!p->mSquished&&p->mPlantHealth>0;}
SexyString Text(FusionAdventureMessage273 message)
{
    switch(message)
    {
    case FusionAdventureMessage273::Welcome:return _S("Добро пожаловать в PvZ Fusion!");
    case FusionAdventureMessage273::NewPlants:return _S("Здесь вас ждут новые растения и способы игры.");
    case FusionAdventureMessage273::Resources:return _S("Начнём с получения ресурсов!");
    case FusionAdventureMessage273::PlantSunflower:return _S("Посадите подсолнух.");
    case FusionAdventureMessage273::FusePeaSunflower:return _S("Подсолнух даёт солнце. Посадите на него горохострел.");
    case FusionAdventureMessage273::BuildGatling:return _S("Сажайте горох на горох, чтобы получить стрелка Гатлинга.");
    case FusionAdventureMessage273::Ready:return _S("Теперь высаживайте растения и защищайтесь от зомби!");
    case FusionAdventureMessage273::GloveUnlocked:return _S("Вы открыли перчатку — важный инструмент.");
    case FusionAdventureMessage273::GloveMove:return _S("Перчатка перемещает растения и позволяет объединять их.");
    case FusionAdventureMessage273::MoveTwinFlower:return _S("Переместите двойной подсолнух за орех.");
    case FusionAdventureMessage273::GloveReady:return _S("Вы освоили перчатку. Готовьте оборону от зомби!");
    default:return _S("");
    }
}
bool MissionCards(Board* b)
{
    if(b->mCoins.mMaxSize-b->mCoins.mSize<3||!CanCreateNativeDroppedCard269(b,0,0))return false;
    int count=0;Coin* existing=nullptr;
    while(b->IterateCoins(existing))if(!existing->mDead&&GetNativeDroppedCard269Data(existing))++count;
    if(count>125)return false;
    std::array<Coin*,3> cards{};
    for(int i=0;i<3;++i)
    {
        const int column=4+i,row=2;
        cards[i]=CreateNativeDroppedCard269(b,0,0,float(b->GetCellCenterX(column,row)),
            float(b->GridToPixelY(column,row)+50));
        if(!cards[i])
        {
            for(auto* card:cards)if(card)card->Die();
            return false;
        }
    }
    return true;
}
bool Lesson4OldKind(uint16_t kind)
{
    // Audited removal-only source4 closure. In particular SunNut1006 has
    // producer death rewards, and CherryNut1003 has AoE; neither may observe
    // the prepared children before its original Clear->spawn source order.
    switch(kind){case 0:case 1:case 2:case 3:case 1000:case 1030:case 1032:case 1033:return true;default:return false;}
}
bool PrepareLesson4(Board* b,uint32_t& mission)
{
    const auto reject=[&](const char* why)
    {if(b->mMainCounter%100==0)LawnNet::NetTrace("Fusion source Tutor4 Prepare blocked reason=%s tick=%d",why,b->mMainCounter);return false;};
    if(!CanApplyNativeFusionGameplay(b)||GetNativeContext(b)->selection.State().phase!=FusionBankPhase::Playing||
        b->mApp->mGameMode!=GAMEMODE_ADVENTURE||b->mBackground!=BACKGROUND_1_DAY||GetNativeFusionRowCount(b)!=5||
        !b->IsValidGridCell(9,2)||!b->IsValidGridCell(5,2)||b->mPlantRow[2]!=PLANTROW_NORMAL||
        b->mApp->IsWallnutBowlingLevel())return reject("uncovered native initializer/terrain");
    std::vector<uint32_t> old;
    // Source ClearPlant9,2 then5,2 visits every existing cell layer. Capture
    // exact generations BEFORE factory allocation; never clear by cell after
    // birth, when the two new default actors occupy those same cells.
    for(int column:{9,5})
    {
        Plant* p=nullptr;
        while(b->IteratePlants(p))if(!p->mDead&&p->mIsOnBoard&&p->mRow==2&&
            (p->mPlantCol==column||(p->mSeedType==SEED_COBCANNON&&p->mPlantCol+1==column)))
        {
            const auto* d=GetNativePlantData(p);const uint32_t uid=b->mPlants.DataArrayGetID(p);
            if(p->mNetGhost||!d||d->instanceToken!=uid||!Lesson4OldKind(d->kind)||
                p->mSeedType!=GetNativeSeedForPlant(d->kind)||IsNativeGlovePlantReserved(b,p,-1))
                return reject("uncovered cell occupant/removal callback or glove lease");
            old.push_back(uid);
        }
    }
    constexpr uint16_t kinds[]={1033,1003};
    const SeedType seeds[]={SEED_TWINSUNFLOWER,SEED_WALLNUT};
    const ReanimationType types[]={REANIM_TWIN_SUNFLOWER,REANIM_WALLNUT};
    for(int i=0;i<2;++i)
    {
        const auto* definition=FusionFindPlant(kinds[i]);
        if(GetNativeSeedForPlant(kinds[i])!=seeds[i]||!definition||!definition->hasStats||definition->maxHealth<=0||
            GetPlantDefinition(seeds[i]).mReanimationType!=types[i]||!EnsureNativePlantVisuals(b,kinds[i]))
            return reject("source model/stats/native carrier unavailable");
        ReanimatorEnsureDefinitionLoaded(types[i],false);
        if(!gReanimatorDefArray||unsigned(types[i])>=gReanimatorDefCount)return reject("native definition unavailable");
        const auto& tracks=gReanimatorDefArray[int(types[i])].mTracks;
        if(!tracks.tracks||tracks.count<=0)return reject("native definition has no tracks");
        for(int track=0;track<tracks.count;++track)
            if(tracks.tracks[track].mName&&strnicmp(tracks.tracks[track].mName,"attacher__",10)==0)
                return reject("native definition has an unbudgeted dynamic attacher");
    }
    auto* effects=b->mApp->mEffectSystem;
    if(!effects||effects!=gEffectSystem||!effects->mReanimationHolder||!effects->mAttachmentHolder)
        return reject("native effect pools unavailable");
    const auto& p=b->mPlants;const auto& r=effects->mReanimationHolder->mReanimations;
    const auto& a=effects->mAttachmentHolder->mAttachments;
    // Exact ordinary Day initializer: one TwinSunflower body +one WallNut
    // body, no seed-specific extra heads/sleep/blink/attachments. No slots
    // from future removals or already-dead actors are credited to this peak.
    if(!p.mBlock||!r.mBlock||!a.mBlock||p.mSize>p.mMaxSize||r.mSize>r.mMaxSize||a.mSize>a.mMaxSize||
        p.mMaxSize-p.mSize<2||r.mMaxSize-r.mSize<2)return reject("whole batch needs2 live Plant/2 Reanimation slots");
    std::array<Plant*,2> made{};
    const auto valid=[&](Plant* plant,int index)
    {
        const auto* data=GetNativePlantData(plant);const auto* definition=FusionFindPlant(kinds[index]);
        return plant&&!plant->mDead&&!plant->mNetGhost&&plant->mNetId&&data&&data->kind==kinds[index]&&
            data->instanceToken&&b->mPlants.DataArrayTryToGet(data->instanceToken)==plant&&
            plant->mSeedType==seeds[index]&&plant->mPlantCol==(index?5:9)&&plant->mRow==2&&plant->mIsOnBoard&&
            data->firstParent==kFusionNoPlant&&data->secondParent==kFusionNoPlant&&!data->packetId&&
            data->owner==LawnNet::NET_SHARED_OWNER&&!data->randomOrigin&&
            plant->mPlantHealth==definition->maxHealth&&plant->mPlantMaxHealth==definition->maxHealth;
    };
    const auto retire=[&](){for(auto* plant:made)if(plant&&!plant->mDead)plant->Die(255);};
    for(int i=0;i<2;++i)
    {
        NativePlantData fresh;fresh.kind=kinds[i];fresh.owner=LawnNet::NET_SHARED_OWNER;
        made[i]=b->AddPlant(i?5:9,2,seeds[i],SEED_NONE,&fresh);
        if(!valid(made[i],i)){retire();return reject("default factory binding failed before clear");}
    }
    for(uint32_t uid:old)if(auto* plant=b->mPlants.DataArrayTryToGet(uid);plant&&!plant->mDead)plant->Die(0);
    // The accepted old kind set cannot damage/spawn on these children. Keep
    // source Die0 callbacks; unsupported effectful callbacks refused above.
    if(!valid(made[0],0)||!valid(made[1],1))
    {
        LawnNet::NetTrace("Fusion source Tutor4 invariant FAILED after source Die0; old removal already committed tick=%d",b->mMainCounter);
        retire();return false;
    }
    mission=b->mPlants.DataArrayGetID(made[0]);RebuildNativeEquipmentGraph(b);
    LawnNet::NetTrace("Fusion source Tutor4 Prepare committed mission=%u uid=%u old=%zu hp=%d/%d sharedDefault=1 peakP=2 peakR=2 peakA=0 tick=%d",
        made[0]->mNetId,mission,old.size(),made[0]->mPlantHealth,made[1]->mPlantHealth,b->mMainCounter);
    return true;
}
bool Active(Board* b,int slot)
{
    const auto* c=GetNativeContext(b);
    if(!c||slot<0||slot>=int(kFusionBankPlayers)||!(c->selection.State().activeMask&(1U<<slot)))return false;
    if(!b->mApp->NetActive())return slot==0;
    const auto* net=b->mApp->mNetManager;if(!net)return false;
    const auto& player=net->GetPlayer(slot);return player.mActive&&!player.mSpectator;
}
}
void PresentNativeFusionAdventureTutorial273(Board* b,const FusionAdventureRun273& r)
{
    if(!Current(b)||!FusionAdventureRunValid273(r)||!r.level||!b->mAdvice)return;
    const auto text=Text(r.tutorial.message);
    if(text.empty())return; // Unimplemented lesson messages never replace native Advice with an empty label.
    if(!r.tutorial.messageTicks)
    {
        if(b->mAdvice->mLabel==text)
        {
            b->mAdvice->ClearLabel();b->mAdvice->mDuration=0;b->mHelpIndex=ADVICE_NONE;
        }
        return;
    }
    // Source message and lifetime already travel together in the accepted FNS
    // view. Use the native widget without a second legacy Advice clock/event.
    if(b->mAdvice->mLabel!=text||b->mAdvice->mDuration==0)
        b->mAdvice->SetLabel(text,MESSAGE_STYLE_TUTORIAL_LEVEL1_STAY);
    b->mHelpIndex=ADVICE_NONE;
    // Native Advice supplies font/layout/drawing. The source message clock,
    // including600s BuildGatling, stays in the explicitly replicated room view.
    b->mAdvice->mDuration=r.tutorial.messageTicks;
    b->mAdvice->mDisplayTime=FusionAdventureMessageDuration273(r.tutorial.message);
}
bool UpdateNativeFusionAdventureTutorial273(Board* b,FusionAdventureRun273& r,FusionAdventureTutorialEffects273& out)
{
    out={};
    if(!Playing(b)||!FusionAdventureRunValid273(r)||!r.level||
        (r.tutorial.active&&r.level!=1&&r.level!=4))return false;
    auto* context=GetNativeContext(b);if(!context)return false;
    if(r.level!=4||r.tutorial.stage<3||!r.tutorial.active)context->adventureMissionPlantUID=0;
    auto candidate=r;FusionAdventureTutorialEffects273 effects;
    if(r.level==4)
    {
        FusionAdventureLessonInput274 input;
        auto* mission=b->mPlants.DataArrayTryToGet(context->adventureMissionPlantUID);
        const auto* data=GetNativePlantData(mission);
        input.hasMissionPlant=mission&&!mission->mDead&&!mission->mNetGhost&&data&&
            data->instanceToken==context->adventureMissionPlantUID;
        if(input.hasMissionPlant)input.missionPlantColumn=mission->mPlantCol;
        if(!StepFusionAdventureTutorial273(candidate,1,input,effects))return false;
        if(effects.missionAction!=FusionAdventureMissionAction274::None&&
            (effects.missionAction!=FusionAdventureMissionAction274::Prepare4||
             !PrepareLesson4(b,context->adventureMissionPlantUID)))return false;
    }
    else
    {
        bool sunflower=false,peaSunflower=false,gatling=false;Plant* p=nullptr;
        while(b->IteratePlants(p))if(Actor(p))if(const auto* d=GetNativePlantData(p))
        {sunflower|=d->kind==1;peaSunflower|=d->kind==1000;gatling|=d->kind==1032;}
        if(!StepFusionAdventureTutorial273(candidate,1,sunflower,peaSunflower,gatling,effects))return false;
    }
    // Retry the same source stage if native allocation is temporarily full.
    // A partial group is retired and no stage/message/card grant is committed.
    if(effects.grantMissionCards&&!MissionCards(b))return false;
    r=candidate;out=effects;
    if(effects.completed)context->adventureMissionPlantUID=0;
    if(effects.messageChanged||!r.tutorial.messageTicks)PresentNativeFusionAdventureTutorial273(b,r);
    return true;
}
bool NativeFusionAdventureEnemyDied273(Board* b,FusionAdventureRun273& r,int wave,int maximum,int enemies,bool iz,float x,float y)
{
    if(!Host(b)||!std::isfinite(x)||!std::isfinite(y)||
        !TryArmFusionAdventureReward273(r,wave,maximum,enemies,iz))return false;
    // Unity clamps the root+(0,1) anchor to viewport[.2,.8]. Use the actual
    // native viewport and the established source vertical-unit projection.
    r.trophySpawnX=std::clamp(x,float(b->mWidth)*.2f,float(b->mWidth)*.8f);
    r.trophySpawnY=std::clamp(y-float(GetNativeFusionPixelsY(b)),float(b->mHeight)*.2f,float(b->mHeight)*.8f);
    EnsureNativeFusionAdventureTrophy273(b,r);return true;
}
bool EnsureNativeFusionAdventureTrophy273(Board* b,FusionAdventureRun273& r)
{
    if(!Host(b)||!FusionAdventureRunValid273(r)||!r.over||r.trophyNetId||r.rewardCollected||
        !b->mCoins.mBlock||b->mCoins.mSize>=b->mCoins.mMaxSize)return false;
    auto* coin=b->mCoins.DataArrayAlloc();coin->mApp=b->mApp;coin->mBoard=b;
    // The real Coin owns drawing, native drop physics and hit testing. Create
    // locally, then expose it through the normal complete native snapshot.
    coin->CoinInitialize(int(r.trophySpawnX),int(r.trophySpawnY),COIN_TROPHY,COIN_MOTION_COIN);
    coin->mNetGhost=false;coin->mNetId=b->NetAllocId();
    if(!coin->mNetId){coin->Die();return false;}
    coin->mPosX=r.trophySpawnX-coin->mWidth*.5f;coin->mPosY=r.trophySpawnY-coin->mHeight*.5f;
    coin->mGroundY=int(coin->mPosY+float(GetNativeFusionPixelsY(b))*.5f);
    if(!BindFusionAdventureTrophy273(r,coin->mNetId,b->mCoins.DataArrayGetID(coin)))
    {coin->Die();return false;}
    LawnNet::NetTrace("Fusion source Adventure trophy spawned level=%u id=%u token=%u over=1 progressCommitted=0",
        r.level,r.trophyNetId,r.trophyToken);return true;
}
bool IsNativeFusionAdventureTrophy273(const Coin* coin,const FusionAdventureRun273& r)
{
    if(!coin||!Current(coin->mBoard)||!FusionAdventureRunValid273(r)||!r.over||!r.trophyToken||
        coin->mType!=COIN_TROPHY||!coin->mNetId||coin->mNetId!=r.trophyNetId)return false;
    // Client Coin array generations are local; only the host compares its
    // canonical generation. Commands carry the replicated host token explicitly.
    return coin->mApp->NetIsClient()||coin->mBoard->mCoins.DataArrayGetID(const_cast<Coin*>(coin))==r.trophyToken;
}
bool CollectNativeFusionAdventureTrophy273(Coin* coin,int slot,FusionAdventureProfile273& profile,
    FusionAdventureRun273& r,bool& committed)
{
    committed=false;if(!IsNativeFusionAdventureTrophy273(coin,r))return false;
    auto* b=coin->mBoard;
    if(!Playing(b)||coin->mDead||coin->mNetGhost||!Active(b,slot)||
        !TryCollectFusionAdventureReward273(profile,r,coin->mNetId,b->mCoins.DataArrayGetID(coin)))return true;
    coin->mCollectX=coin->mPosX;coin->mCollectY=coin->mPosY;
    coin->mIsBeingCollected=true;coin->mHitGround=true;coin->mVelX=coin->mVelY=0;
    coin->mNetCollectionFxStarted=true;
    AttachmentDie(coin->mAttachmentID);
    b->mApp->mMusic->StopAllMusic();b->mApp->PlaySample(Sexy::SOUND_DIAMOND);
    committed=true;
    LawnNet::NetTrace("Fusion source Adventure trophy CLICK level=%u slot=%d frontier=%u cycle=%u replay=%d completed=%llu",
        r.level,slot,profile.frontierLevel,profile.cycle,r.replay?1:0,static_cast<unsigned long long>(profile.completedLevels));
    return true;
}
bool UpdateNativeFusionAdventureTrophy273(Coin* coin,const FusionAdventureRun273& r)
{
    if(!IsNativeFusionAdventureTrophy273(coin,r))return false;
    if(!Playing(coin->mBoard)||coin->mDead||coin->mNetGhost)return true;
    if(coin->mCoinAge<(std::numeric_limits<int>::max)())++coin->mCoinAge;
    if(!r.rewardCollected){coin->UpdateFall();return true;}
    const float t=(std::min)(1.0f,float(kFusionAdventureRewardTicks273-r.rewardTicks)/125.0f);
    auto* b=coin->mBoard;
    // Source target local(10,-3), projected with the same native cell basis.
    const float targetX=float(b->GetCellCenterX(0,0))+(10.0f+4.8f)*float(kFusionBasePixelsX)-coin->mWidth*.5f;
    const float targetY=float(b->GridToPixelY(0,0)+50)+(2.3f+3.0f)*float(GetNativeFusionPixelsY(b))-coin->mHeight*.5f;
    coin->mPosX=coin->mCollectX+(targetX-coin->mCollectX)*t;
    coin->mPosY=coin->mCollectY+(targetY-coin->mCollectY)*t;coin->mScale=1.0f-.5f*t;
    return true;
}
bool UpdateNativeFusionAdventureReward273(Board* b,FusionAdventureRun273& r)
{
    if(!Playing(b)||!FusionAdventureRunValid273(r)||!r.level)return false;
    if(r.over&&!r.trophyNetId)EnsureNativeFusionAdventureTrophy273(b,r);
    return StepFusionAdventureReward273(r,1);
}
}
