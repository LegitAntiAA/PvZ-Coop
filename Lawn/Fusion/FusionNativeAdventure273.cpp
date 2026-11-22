#include "FusionAdventureLesson346Rules.h"
#include "FusionNativeAdventure273.h"
#include "FusionNative.h"
#include "FusionNativeRoof350.h"
#include "FusionNativeAdventureScheduler273.h"
#include "FusionNativeAdventureProgress273.h"
#include "FusionNativeZombieVisual.h"
#include "FusionCampaign272Rules.h"
#include "FusionCampaign342Rules.h"
#include "FusionNativeSourceClock313.h"
#include "FusionWire.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Coin.h"
#include "../MessageWidget.h"
#include "../Net/NetCommon.h"
#include "../Net/NetManager.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include <cmath>
#include <limits>
#include <utility>

namespace LawnFusion
{
namespace
{
using Result=FusionAdventureScheduler273Result;
bool Current(const Board* b)
{return b&&b->mApp&&b->mApp->mBoard==b&&IsNativeFusionBoard(b)&&!GetNativeContext(b)->adventureTeardown;}
bool Playing(const Board* b)
{return Current(b)&&b->mApp->mGameScene==SCENE_PLAYING&&!b->mPaused&&!b->mApp->GetDialogCount();}
bool RandomOwner(const Board* b)
{return Current(b)&&!b->mApp->NetIsClient();}
int BoardRandomInteger(void* owner,int minimum,int maximum)
{
    auto* b=static_cast<Board*>(owner);
    if(!RandomOwner(b)||maximum<=minimum)return maximum;
    return RandRangeInt(minimum,maximum-1);
}
float BoardRandomReal(void* owner,float minimum,float maximum)
{
    auto* b=static_cast<Board*>(owner);
    if(!RandomOwner(b))return (std::numeric_limits<float>::quiet_NaN)();
    return RandRangeFloat(minimum,maximum);
}
FusionAdventureScheduler273Random BoardRandom(Board* b)
{return {b,BoardRandomInteger,BoardRandomReal};}
void Failed(Board* b,const char* reason)
{
    auto* c=GetNativeContext(b);if(!c||c->adventure.schedulerFailed)return;
    c->adventure.schedulerFailed=true;c->adventureScheduler.failed=true;
    LawnNet::NetTrace("Fusion source Adventure273 stopped level=%u wave=%u reason=%s",
        c->adventure.run.level,c->adventure.wave,reason);
    b->DisplayAdvice(_S("Ошибка уровня Fusion. Начните уровень заново из меню."),
        MESSAGE_STYLE_HINT_STAY,ADVICE_NONE);
}
void View(Board* b)
{
    auto* c=GetNativeContext(b);auto& view=c->adventure;const auto& scheduler=c->adventureScheduler;
    view.wave=uint16_t(scheduler.wave);view.maximumWaves=uint16_t(scheduler.maximumWaves);
    view.countdown=scheduler.countdown;view.hugeElapsed=scheduler.hugeElapsed;
    view.hugeWarning=scheduler.hugeWarning;view.schedulerFailed|=scheduler.failed;
    b->mCurrentWave=scheduler.wave;b->mNumWaves=scheduler.maximumWaves;
    b->mZombieCountDown=int(std::ceil(double(scheduler.countdown)*100.0));
}
void Banners(Board* b,const FusionAdventureScheduler273Events& events)
{
    if(events.hugeWarning)
    {
        b->mApp->PlaySample(Sexy::SOUND_HUGE_WAVE);
        b->DisplayAdvice(_S("Приближается огромная волна зомби!"),MESSAGE_STYLE_HUGE_WAVE,ADVICE_NONE);
        if(b->mAdvice)b->mAdvice->mDuration=400;
    }
    if(events.firstWave)b->mApp->PlaySample(Sexy::SOUND_AWOOGA);
    if(events.finalWave)
    {
        b->mApp->PlaySample(Sexy::SOUND_FINALWAVE);
        b->DisplayAdvice(_S("Последняя волна!"),MESSAGE_STYLE_HUGE_WAVE,ADVICE_NONE);
        if(b->mAdvice)b->mAdvice->mDuration=400;
    }
}
}
bool IsNativeSourceAdventure273(const Board* board)
{const auto* c=GetNativeContext(board);return Current(board)&&c&&c->adventure.enabled;}
bool IsNativeSourceAdventureGrassWorld281(const Board* b)
{
    const auto* c=GetNativeContext(b);
    const auto* source=c?FindFusionSourceAdventure272(c->adventure.run.level):nullptr;
    if(!Current(b)||!c||!c->adventure.enabled||!b->mApp->IsFusionSourceAdventure()||
        !c->epoch||c->epoch!=b->mApp->mNativeFusionEpoch||
        !source||(source->scene!=0&&source->scene!=1)||source->rows!=5||source->columns!=10||
        int(b->mBackground)!=source->scene||b->GetColumnCount()!=10||GetNativeFusionRowCount(b)!=5||
        !b->mTDPath.empty()||b->mGridItems.mSize)return false;
    for(int row=0;row<5;++row)
    {
        if(b->mPlantRow[row]!=PLANTROW_NORMAL)return false;
        for(int col=0;col<10;++col)if(b->GetGridSquareTypeAt(col,row)!=GRIDSQUARE_GRASS)return false;
    }
    return true;
}
bool IsNativeSourceAdventureWorld342(const Board* b)
{
    const auto* c=GetNativeContext(b);
    const auto* source=c?FindFusionSourceAdventure272(c->adventure.run.level):nullptr;
    return IsNativeSourceAdventure273(b)&&b->mApp->IsFusionSourceAdventure()&&source&&
        IsNativeSourceWorld313(b)&&int(b->mBackground)==source->scene&&
        GetNativeFusionRowCount(b)==source->rows&&b->GetColumnCount()==source->columns;
}
bool InitializeNativeSourceAdventure273(Board* b,const FusionAdventureSession273& boot,std::string* error)
{
    if(error)error->clear();
    FusionCampaign342Plan plan;
    const auto* level=FindFusionSourceAdventure272(boot.run.level);auto* c=GetNativeContext(b);
    if(!Current(b)||!c||!boot.enabled||!FusionAdventureSessionValid273(boot)||!level||
        !NativeSourceAdventureLevelReady273(boot.run.level,boot.run.cycle)||
        !BuildFusionCampaign342Plan(boot.run.level,boot.run.cycle,plan,error))
    {if(error)*error="Source Adventure boot is invalid or outside supported scene/controller capabilities.";return false;}
    const bool host=!b->mApp->NetIsClient();
    if(host&&(boot.wave||boot.run.over||boot.run.rewardCollected||boot.schedulerFailed||
        b->mZombies.mSize||b->mPlants.mSize))
    {if(error)*error="Source Adventure host initialization requires a fresh empty Board/run.";return false;}
    if(host&&c->adventureScheduler.initialized)
    {if(error)*error="Source Adventure scheduler is already initialized; duplicate boot refused.";return false;}
    if(c->adventure.enabled)
    {
        LawnNet::NetBuffer existing,incoming;
        if(c->epoch!=b->mApp->mNativeFusionEpoch||
            !EncodeFusionAdventureSession273(c->adventure,existing)||!EncodeFusionAdventureSession273(boot,incoming)||
            existing.mData!=incoming.mData)
        {if(error)*error="Source Adventure preinstalled boot/profile/epoch differs from the requested run.";return false;}
    }
    if(!ConfigureNativeFusionBackground(b,level->scene))
    {if(error)*error="Source Adventure native background configuration failed.";return false;}
    // ConfigureNativeFusionBackground deliberately no-ops for an equal scene;
    // stock Adventure1 may still have a one-row lawn, so always set full rows.
    for(int row=0;row<MAX_GRID_SIZE_Y;++row)
    {
        const bool water=(plan.waterRows&(1<<row))!=0;
        b->mPlantRow[row]=row<level->rows?(water?PLANTROW_POOL:PLANTROW_NORMAL):PLANTROW_DIRT;
        for(int col=0;col<b->GetColumnCount();++col)
            b->GetGridSquareTypeAt(col,row)=row<level->rows?(water?GRIDSQUARE_POOL:GRIDSQUARE_GRASS):GRIDSQUARE_NONE;
    }
    if(!IsNativeSourceAdventureWorld342(b))
    {if(error)*error="Source Adventure requires its exact imported scene/row/grid world without foreign initializers.";return false;}
    if(host&&level->roofPreplantColumns&&!PreflightNativeRoofPreplants350(b,*level,error))return false;
    FusionAdventureScheduler273State scheduler;
    if(host&&(level->tutorialId==7||level->tutorialId==8||level->tutorialId==10||level->tutorialId==13||level->tutorialId==15||FusionAdventureLesson346Supported(boot.run.level))&&
        !PreflightNativeFusionAdventureLesson278(b,boot.run.level,error))return false;
    if(host&&InitializeNativeAdventureScheduler273(b,scheduler,boot.run.level,int(boot.run.cycle),
        boot.sourceDifficulty,BoardRandom(b),error)!=Result::Okay)return false;
    if(host&&level->roofPreplantColumns&&!InitializeNativeRoofPreplants350(b,*level,error))return false;
    c->adventure=boot;c->adventureScheduler=std::move(scheduler);c->adventureTick=-1;
    c->adventureEnemies.clear();c->adventureEnemyCount=0;c->adventureSkySunTicks=750;
    c->validation=false;c->money=level->initialMoney;c->level=boot.run.level;
    b->mLevel=boot.run.level;b->mSunMoney=level->initialSun;
    b->mTutorialState=TUTORIAL_OFF;b->mTutorialTimer=0;b->mSodPosition=1000;
    b->mNumWaves=boot.maximumWaves;b->mCurrentWave=boot.wave;
    b->mZombieCountDownStart=1500;b->mZombieCountDown=1500;b->mHugeWaveCountDown=0;
    b->mSunCountDown=750;b->mNumSunsFallen=0;
    if(host)View(b);
    LawnNet::NetTrace("Fusion source Adventure273 initialized role=%s level=%u cycle=%u sourceDifficulty=%u rows=%u columns=10 sun=%d money=%d waves=%u classicProgressUsed=0",
        host?"host":"client",boot.run.level,boot.run.cycle,boot.sourceDifficulty,level->rows,b->mSunMoney,c->money,c->adventure.maximumWaves);
    return true;
}
bool UpdateNativeSourceAdventure273(Board* b)
{
    if(!IsNativeSourceAdventure273(b))return false;
    auto* c=GetNativeContext(b);auto& view=c->adventure;
    if(b->mApp->NetIsClient())
    {PresentNativeFusionAdventureTutorial273(b,view.run);return true;}
    if(!Playing(b)||view.schedulerFailed||c->adventureTick==b->mMainCounter)return true;
    c->adventureTick=b->mMainCounter;
    if(!FusionAdventureSessionValid273(view))
    {Failed(b,"invalid authoritative source session");return true;}
    if(view.run.over)
    {UpdateNativeFusionAdventureReward273(b,view.run);return true;}
    FusionAdventureTutorialEffects273 tutorial;
    // A full mission-card pool retries its tutorial stage. Wave cache still
    // updates, but the active source tutorial keeps normal spawning paused.
    UpdateNativeFusionAdventureTutorial273(b,view.run,tutorial);
    if(tutorial.completed&&!FusionAdventureScheduler273TutorOver(c->adventureScheduler))
    {Failed(b,"source TutorOver could not set the five-second wave countdown");return true;}
    FusionAdventureScheduler273Input input;input.host=true;input.deltaSeconds=.01f;
    input.tutorial=view.run.tutorial.active;
    // Admitted ordinary grass worlds have allowSummon=true and no
    // Boss/ScaryPot/IceDoom controller; shared capability admission owns this.
    FusionAdventureScheduler273Events events;std::string error;
    const auto status=UpdateNativeAdventureScheduler273(b,c->adventureScheduler,input,BoardRandom(b),events,&error);
    if(status!=Result::Okay)
    {Failed(b,error.empty()?"host scheduler rejected the source tick":error.c_str());return true;}
    View(b);Banners(b,events);
    return true;
}
bool UpdateNativeSourceAdventureSun273(Board* b)
{
    if(!IsNativeSourceAdventure273(b))return false;
    auto* c=GetNativeContext(b);
    if(b->mApp->NetIsClient()||!Playing(b)||c->adventure.run.over||c->adventure.schedulerFailed)return true;
    const auto* source=FindFusionSourceAdventure272(c->adventure.run.level);
    // Source LevelUpdate1803189F0 requires !isNight/!isConvey and fallSun.
    // This ordinary world excludes Tower/Indestructible/SeedRain controllers;
    // they must provide their own source policy before native admission.
    if(!source||!IsNativeSourceAdventureWorld342(b)||source->scene==1||source->scene==3||source->conveyor)return true;
    if(c->adventureSkySunTicks>0)--c->adventureSkySunTicks;
    b->mSunCountDown=c->adventureSkySunTicks;
    if(c->adventureSkySunTicks>0)return true;
    if(b->mCoins.mSize>=b->mCoins.mMaxSize)return true;
    // Board.SunUpdate0x180324340 selects RandomRangeInt(3,9). Item0's
    // CoinSun.Start0x180440E40 confirms value25; reset0x40f00000 is7.5s.
    const int column=BoardRandomInteger(b,3,9);
    auto* sun=b->AddCoin(b->GetCellCenterX(column,0)-20,60,COIN_SUN,COIN_MOTION_FROM_SKY,SEED_NONE,25);
    if(!sun){Failed(b,"native sky Sun allocation failed");return true;}
    ++b->mNumSunsFallen;c->adventureSkySunTicks=750;b->mSunCountDown=750;
    LawnNet::NetTrace("Fusion source Adventure273 skySun coin=%u value=25 column=%d nextTicks=750 tick=%d",
        sun->mNetId,column,b->mMainCounter);
    return true;
}
bool RegisterNativeSourceAdventureEnemy273(Zombie* z)
{
    if(!z||!IsNativeSourceAdventure273(z->mBoard)||z->mApp->NetIsClient()||z->mNetGhost||z->mMindControlled)return false;
    auto* c=GetNativeContext(z->mBoard);const auto* source=GetNativeSourceZombieData(z);
    const uint32_t generation=z->mBoard->mZombies.DataArrayGetID(z);
    if(!source||!generation||source->instanceToken!=generation||!z->mNetId)return false;
    if(!c->adventureEnemies.insert(generation).second)return false;
    ++c->adventureEnemyCount;
    LawnNet::NetTrace("Fusion source Adventure273 enemy birth id=%u token=%u kind=%u enemies=%d",
        z->mNetId,generation,source->kind,c->adventureEnemyCount);
    return true;
}
bool RetireNativeSourceAdventureEnemy273(Zombie* z)
{
    if(!z||!IsNativeSourceAdventure273(z->mBoard)||z->mApp->NetIsClient()||z->mNetGhost||z->mMindControlled)return false;
    auto* b=z->mBoard;auto* c=GetNativeContext(b);
    const uint32_t generation=b->mZombies.DataArrayGetID(z);
    if(!generation||!c->adventureEnemies.erase(generation))return false;
    // Source Zombie.Die0x180381A20 decrements once on first status1, before
    // DropItem/TryDropAward, and clamps the source enemy count at zero.
    if(c->adventureEnemyCount>0)--c->adventureEnemyCount;
    LawnNet::NetTrace("Fusion source Adventure273 enemy death id=%u token=%u enemies=%d wave=%u/%u",
        z->mNetId,generation,c->adventureEnemyCount,c->adventure.wave,c->adventure.maximumWaves);
    if(c->adventure.schedulerFailed||c->adventure.run.over||c->adventure.run.tutorial.active||
        c->adventureEnemyCount>0||c->adventure.wave<c->adventure.maximumWaves)return true;
    float rootX=0,rootY=0;
    if(!GetNativeSourceZombie269NodePoint(z,"",rootX,rootY))
    {Failed(b,"source enemy death root must be captured before native body cleanup");return true;}
    NativeFusionAdventureEnemyDied273(b,c->adventure.run,c->adventure.wave,c->adventure.maximumWaves,
        c->adventureEnemyCount,false,rootX,rootY);
    return true;
}
bool HandleNativeSourceAdventureRewardCommand273(Board* b,int slot,const FusionCommand& command)
{
    if(!IsNativeSourceAdventure273(b)||b->mApp->NetIsClient()||!Playing(b))return false;
    auto* c=GetNativeContext(b);auto& session=c->adventure;
    if(c->selection.State().phase!=FusionBankPhase::Playing||!session.run.over||session.run.rewardCollected||
        command.sourceId!=session.run.trophyNetId||command.sourceToken!=session.run.trophyToken)return false;
    Coin* coin=nullptr;
    while(b->mCoins.IterateNext(coin))
    {
        if(coin->mNetId!=command.sourceId||b->mCoins.DataArrayGetID(coin)!=command.sourceToken)continue;
        bool committed=false;
        if(!CollectNativeFusionAdventureTrophy273(coin,slot,session.profile,session.run,committed)||!committed)return false;
        if(auto* net=b->mApp->mNetManager)
        {
            net->mConfig.mFusionSourceProfile=session.profile;
            if(!LawnNet::NetSaveConfig(net->mConfig))
                LawnNet::NetTrace("Fusion source Adventure273 profile save failed after trophy click");
        }
        b->mLastSnapshotTick=0;return true;
    }
    return false;
}
bool HandleNativeSourceAdventurePointer273(Board* b,int button,int x,int y)
{
    if(!IsNativeSourceAdventure273(b)||button<=0||!Playing(b)||b->mTimeStopCounter>0)return false;
    const auto& run=GetNativeContext(b)->adventure.run;
    if(!run.over||!run.trophyNetId)return false;
    Coin* coin=nullptr;
    while(b->mCoins.IterateNext(coin))
    {
        if(!IsNativeFusionAdventureTrophy273(coin,run))continue;
        HitResult hit;
        if(!coin->MouseHitTest(x,y,&hit))continue;
        if(!run.rewardCollected)
        {
            FusionCommand command;command.action=FusionCommandKind::AdventureRewardCollect;
            command.sourceId=run.trophyNetId;command.sourceToken=run.trophyToken;
            b->mApp->SubmitNativeFusionCommand(command);
        }
        return true;
    }
    return false;
}
}
