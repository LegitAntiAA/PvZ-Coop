#include "NetFusionAdventure56_277Smoke.h"
#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetCommon.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Projectile.h"
#include "../Coin.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../System/PlayerInfo.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativeAdventure273.h"
#include "../Fusion/FusionNativeZombies.h"
#include "../Fusion/FusionNativeDayZombie273.h"
#include "../Fusion/FusionNativeZombieVisual.h"
#include "../Fusion/FusionNativeProjectileMetadata.h"
#include "../Fusion/FusionCampaign272Rules.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../SexyAppFramework/ScrollbarWidget.h"
#include "../../SexyAppFramework/DDInterface.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>

namespace
{
using namespace LawnFusion;using LawnNet::NetTrace;
struct Ref {uint16_t id=0;uint32_t token=0;};
struct Enemy {Ref ref;uint16_t kind=0;uint8_t wave=0;float firstX=0;bool moved=false;};
struct Purchase {int slot;uint16_t kind;bool extra;int col,row,cost;};
constexpr std::array<Purchase,4> purchases={Purchase{0,3,false,4,1,50},Purchase{1,3,true,4,3,100},
    Purchase{0,1,false,0,1,50},Purchase{1,1,true,0,3,100}};
struct State
{
    int stage=0,level=5,waiting=0,input=0,playingFrame=-1,classicLevel=-1,classicFinished=-1;
    uint32_t started=0,since=0,chat=0,epoch=0,samples=0,pickSamples=0,sunSamples=0,diagnostic=0;
    bool initial=false,profileSetup=false,pause=false,checkpoint=false,completionLogged=false,shot100=false,seen100=false,shotRequired=false,visibleLogged=false;
    bool sunReady=false,sunClicked=false,sunObserved=false,toolClicked=false,shovelClicked=false,shovelObserved=false;
    std::array<Ref,4> plants{};std::map<uint32_t,Enemy> enemies;std::array<int,2> counts{};
    bool sunBaseline=false;std::set<uint16_t> oldCoins;
    uint16_t sunId=0;std::string checkpointTag;FusionAdventureProfile273 localProfile{};
    std::set<std::string> frames;std::map<std::string,int> draws;
}s;
bool Enabled(const char* key){const auto* v=std::getenv(key);return v&&std::strcmp(v,"1")==0;}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool okay,const char* reason){if(!okay)a->CoopSmokeFail(reason);return okay;}
bool SameProfile(const FusionAdventureProfile273& x,const FusionAdventureProfile273& y)
{return x.completedLevels==y.completedLevels&&x.frontierLevel==y.frontierLevel&&x.cycle==y.cycle;}
FusionAdventureProfile273 ExpectedProfile(int level)
{FusionAdventureProfile273 p;p.completedLevels=level==5?15:31;p.frontierLevel=uint16_t(level);p.cycle=1;return p;}
void Advance(LawnApp* a,int stage)
{s.stage=stage;s.since=GetTickCount();s.input=0;NetTrace("SMOKE_ADVENTURE56_277_STAGE role=%s level=%d stage=%d epoch=%u",Role(a),s.level,stage,s.epoch);}
std::string Tag(const std::string& value){return "A56277_"+std::to_string(s.epoch)+"_"+value;}
bool Has(LawnApp* a,int slot,const std::string& value)
{for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==slot&&line.mText==value)return true;return false;}
bool Send(LawnApp* a,const std::string& value)
{
    if(!Require(a,value.size()<=LawnNet::NET_MAX_CHAT_CHARS,"Adventure56 canonical proof exceeds chat limit"))return false;
    if(Has(a,Slot(a),value))return true;if(s.chat&&GetTickCount()-s.chat<800)return false;
    s.chat=GetTickCount();a->mNetManager->SendChat(value);return Has(a,Slot(a),value);
}
bool Barrier(LawnApp* a,const std::string& name)
{const auto tag=Tag(name);Send(a,tag);return Has(a,0,tag)&&Has(a,1,tag);}
bool Click(LawnApp* a,float x,float y)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;
    return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Adventure56 owned SDL click failed");
}
bool BoardClick(LawnApp* a,float x,float y)
{return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY);}
bool Escape(LawnApp* a)
{
    SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;
    SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;
    return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Adventure56 owned SDL Escape failed");
}
bool Capture(LawnApp* a,const char* suffix)
{
    const auto name="fusion_adventure56_277_level"+std::to_string(s.level)+"_"+suffix;if(s.frames.contains(name))return true;
    if(!s.draws.contains(name))s.draws[name]=a->mDrawCount;
    const int requested=s.draws[name]+2;if(!a->CoopSmokeEnsureFreshFrameAfterDraw(requested))return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Adventure56 fresh owned BMP failed"))return false;
    s.frames.insert(name);NetTrace("SMOKE_ADVENTURE56_277_RENDER role=%s frame=%s requestedDraw=%d capturedDraw=%d",Role(a),name.c_str(),requested,a->mCoopSmokeFrameDrawCount);return true;
}
Plant* At(Board* b,const Purchase& wanted)
{Plant* p=nullptr;while(b->IteratePlants(p))if(!p->mDead&&p->mPlantCol==wanted.col&&p->mRow==wanted.row)
    if(const auto* d=GetNativePlantData(p);d&&d->kind==wanted.kind)return p;return nullptr;}
Plant* ByRef(Board* b,Ref ref)
{Plant* p=nullptr;while(b->IteratePlants(p))if(!p->mDead&&p->mNetId==ref.id)
    if(const auto* d=GetNativePlantData(p);d&&d->instanceToken==ref.token)return p;return nullptr;}
SeedPacket* Packet(Board* b,int owner,uint16_t kind,bool extra)
{for(int i=0;i<b->mSeedBank->mNumPackets;++i){auto* p=&b->mSeedBank->mSeedPackets[i];
    if(const auto* d=GetNativePacketData(p);d&&d->owner==owner&&d->kind==kind&&d->isExtra==extra)return p;}return nullptr;}
Coin* Sun(Board* b,uint16_t id)
{Coin* coin=nullptr;while(b->IterateCoins(coin))if(!coin->mDead&&coin->mNetId==id&&coin->IsSun()&&
    coin->mCoinMotion==COIN_MOTION_FROM_PLANT&&coin->GetSunValue()==25)return coin;return nullptr;}
bool World(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);if(!c)return false;const auto& v=c->adventure;
    if(!Require(a,IsNativeSourceAdventure273(b)&&!c->validation&&!a->mFusionRuntime&&v.run.level==s.level&&
        v.run.cycle==1&&v.sourceDifficulty==2&&!v.run.replay&&!v.run.over&&!v.schedulerFailed&&v.maximumWaves==20&&
        !v.run.tutorial.active&&!v.run.tutorial.stage&&SameProfile(v.profile,ExpectedProfile(s.level))&&
        c->money==3000&&b->GetColumnCount()==10&&GetNativeFusionRowCount(b)==5&&b->mBackground==BACKGROUND_1_DAY&&
        a->mPlayerInfo&&a->mPlayerInfo->mLevel==s.classicLevel&&a->mPlayerInfo->mFinishedAdventure==s.classicFinished&&
        SameProfile(a->mNetManager->mConfig.mFusionSourceProfile,a->NetIsHost()?ExpectedProfile(s.level):s.localProfile)&&
        !b->HasConveyorBeltSeedBank()&&!a->IsWallnutBowlingLevel()&&!a->IsFirstTimeAdventureMode(),
        "Adventure56 actual source world/profile/native policy/Classic preservation differs"))return false;
    for(int row=0;row<5;++row)if(!Require(a,b->mPlantRow[row]==PLANTROW_NORMAL,"Adventure56 restricted native row leaked"))return false;
    if(!s.epoch)s.epoch=c->epoch;return Require(a,s.epoch==c->epoch,"Adventure56 source epoch changed before checkpoint");
}
bool Choose(LawnApp* a)
{
    auto* b=a->mBoard;auto* chooser=a->mSeedChooserScreen;const auto* c=GetNativeContext(b);
    if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||chooser->mSeedsInFlight||!b->mCutScene->mSeedChoosing)return false;
    const auto& bank=c->selection.State();std::set<unsigned> got,expected;
    for(const auto& card:c->eligible)got.insert(unsigned(card.kind)*2+(card.isExtra?1:0));
    for(unsigned kind=0;kind<unsigned(s.level==5?5:6);++kind){expected.insert(kind*2);expected.insert(kind*2+1);}
    if(!Require(a,got==expected&&c->eligible.size()==expected.size()&&bank.targetCount==10&&bank.quotas[0]==5&&bank.quotas[1]==5,
        "Adventure56 earned normal/extra choices or native10 bank quota differs"))return false;
    const std::array<uint16_t,5> wanted={0,1,2,3,uint16_t(s.level==5?4:5)};int own=0;
    for(const auto& p:bank.packets)if(p.owner==Slot(a))
    {if(!Require(a,p.isExtra==(Slot(a)==1)&&std::find(wanted.begin(),wanted.end(),p.kind)!=wanted.end(),"Adventure56 actual local selected identity differs"))return false;++own;}
    if(own<s.waiting)return false;
    if(own<5)
    {
        int dense=-1;for(size_t i=0;i<c->eligible.size();++i)if(c->eligible[i].kind==wanted[own]&&c->eligible[i].isExtra==(Slot(a)==1))dense=int(i);
        if(!Require(a,dense>=0,"Adventure56 actual local source card missing"))return false;
        if(std::abs(chooser->mScrollAmount)>.05f)return false;int x=0,y=0;chooser->GetSeedPositionInChooser(dense,x,y);
        if(Click(a,float(chooser->mX+x+25),float(chooser->mY+y+35-int(chooser->mScrollbar->mScrollValue))))s.waiting=own+1;
        return false;
    }
    return bank.packets.size()==10&&Capture(a,"chooser")&&Barrier(a,"CHOOSER");
}
bool OwnFirst(LawnApp* a)
{
    auto* bank=a->mBoard->mSeedBank;if(!Require(a,bank->mNumPackets==10,"Adventure56 native bank packet count differs"))return false;
    for(int i=0;i<10;++i)
    {const auto* d=GetNativePacketData(&bank->mSeedPackets[i]);if(!Require(a,d&&d->owner==(i<5?Slot(a):1-Slot(a)),"Adventure56 own native packets are not displayed first"))return false;}
    return true;
}
bool Plan(LawnApp* a)
{
    if(!a->NetIsHost())return true;const auto* c=GetNativeContext(a->mBoard);const auto& state=c->adventureScheduler;
    if(!Require(a,state.initialized&&!state.failed&&state.level==s.level&&state.cycle==1&&state.sourceDifficulty==2&&state.maximumWaves==20&&state.waves.size()==20,
        "Adventure56 actual host source scheduler plan differs"))return false;
    const auto* config=FindFusionSourceAdventure272(s.level);
    for(size_t i=0;i<state.waves.size();++i)
    {
        const auto& wave=state.waves[i];if(!Require(a,!wave.empty(),"Adventure56 source wave plan is empty"))return false;
        for(const auto kind:wave)if(!Require(a,FusionSourceAdventureAllowsZombie272(s.level,kind)&&
            (s.level!=6||kind!=21||i==19),"Adventure56 weighted plan emitted an unallowed/final-only NPC early"))return false;
    }
    const auto& final=state.waves.back();
    if(!Require(a,final.size()>=config->allowedZombieKinds.size()&&std::equal(config->allowedZombieKinds.begin(),config->allowedZombieKinds.end(),
        final.end()-config->allowedZombieKinds.size()),"Adventure56 source final allow-list suffix differs"))return false;
    NetTrace("SMOKE_ADVENTURE56_277_PLAN level=%d waves=20 firstCount=%zu secondCount=%zu finalCount=%zu finalDollScheduled=%d finalDollRuntime=NotExecuted fullWavePlanOnly=1 RNGWrites=0",
        s.level,state.waves[0].size(),state.waves[1].size(),final.size(),s.level==6);return true;
}
bool PurchasePlant(LawnApp* a,int index)
{
    auto* b=a->mBoard;const auto& want=purchases[index];auto* packet=Packet(b,want.slot,want.kind,want.extra);
    if(!Require(a,packet!=nullptr,"Adventure56 actual selected purchase packet missing"))return false;
    int before=500;for(int i=0;i<index;++i)before-=purchases[i].cost;
    auto* plant=At(b,want);
    if(!plant&&Slot(a)==want.slot&&s.input==0)
    {
        if(!Require(a,b->GetSeedPacketCost(*packet)==want.cost&&b->mSunMoney==before,"Adventure56 pre-purchase native Sun ledger differs"))return false;
        if(!packet->CanPickUp()||b->mCursorObject->mCursorType!=CURSOR_TYPE_NORMAL)return false;
        s.pickSamples=Samples(a);auto* bank=b->mSeedBank;
        if(BoardClick(a,float(bank->mX+packet->mX+packet->mOffsetX+25),float(bank->mY+packet->mY+35)))s.input=1;return false;
    }
    if(!plant&&Slot(a)==want.slot&&s.input==1)
    {
        const auto* cursor=b->mCursorObject;
        if(cursor->mCursorType!=CURSOR_TYPE_PLANT_FROM_BANK||cursor->mSeedBankIndex<0||cursor->mSeedBankIndex>=b->mSeedBank->mNumPackets||
            b->mSeedBank->mSeedPackets[cursor->mSeedBankIndex].mNetId!=packet->mNetId||(a->NetIsClient()&&Samples(a)<=s.pickSamples))return false;
        if(BoardClick(a,float(b->GetCellCenterX(want.col,want.row)),float(b->GridToPixelY(want.col,want.row)+50)))s.input=2;return false;
    }
    const auto* data=GetNativePlantData(plant);if(!plant||!data)return false;
    if(!Require(a,data->owner==want.slot&&data->packetId==packet->mNetId&&data->instanceToken&&plant->mNetId&&
        plant->mPlantHealth==plant->mPlantMaxHealth&&b->mSunMoney==before-want.cost,
        "Adventure56 authoritative purchase identity/HP/Sun debit differs"))return false;
    if(!s.plants[index].id)
    {
        s.plants[index]={plant->mNetId,data->instanceToken};
        NetTrace("SMOKE_ADVENTURE56_277_PURCHASE role=%s level=%d step=%d owner=%d kind=%u extra=%d packet=%u id=%u token=%u beforeSun=%d price=%d afterSun=%d sourceInputOnly=1",
            Role(a),s.level,index,want.slot,want.kind,want.extra,packet->mNetId,plant->mNetId,data->instanceToken,before,want.cost,b->mSunMoney);
    }
    return Barrier(a,"BUY"+std::to_string(index)+"_"+std::to_string(plant->mNetId)+"_"+std::to_string(data->instanceToken));
}
bool SunPickup(LawnApp* a)
{
    auto* b=a->mBoard;const auto prefix=Tag("SUN_");
    auto* producer=ByRef(b,s.plants[2]);unsigned producers=0;Plant* p=nullptr;
    while(b->IteratePlants(p))if(!p->mDead)if(const auto* d=GetNativePlantData(p);d&&d->kind==1)++producers;
    if(!Require(a,producer&&producers==1&&!ByRef(b,s.plants[3]),"Adventure56 new Sun has no unique canonical surviving producer"))return false;
    if(!s.sunBaseline)
    {
        Coin* existing=nullptr;while(b->IterateCoins(existing))if(!existing->mDead)s.oldCoins.insert(existing->mNetId);
        s.sunBaseline=true;NetTrace("SMOKE_ADVENTURE56_277_SUN_BASELINE role=%s level=%d producerId=%u producerToken=%u liveProducers=1 excludedExistingCoins=%zu parentProof=exclusive-live-producer-after-shovel-barrier CoinParentMetadata=NotPresent SkyExcluded=motion",
            Role(a),s.level,s.plants[2].id,s.plants[2].token,s.oldCoins.size());
    }
    if(!s.sunId&&a->NetIsClient())
    {
        Coin* coin=nullptr;while(b->IterateCoins(coin))if(!coin->mDead&&coin->mNetId&&!s.oldCoins.contains(coin->mNetId)&&
            coin->mCoinMotion==COIN_MOTION_FROM_PLANT&&coin->IsSun()&&coin->GetSunValue()==25&&!coin->mIsBeingCollected&&coin->mHitGround)
            if(!s.sunId||coin->mNetId<s.sunId)s.sunId=coin->mNetId;
    }
    if(a->NetIsClient()&&s.sunId)Send(a,prefix+std::to_string(s.sunId));
    if(a->NetIsHost()&&!s.sunId)for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==1&&line.mText.starts_with(prefix))
    {
        unsigned id=0;const auto value=line.mText.substr(prefix.size());const auto parsed=std::from_chars(value.data(),value.data()+value.size(),id);
        if(parsed.ec==std::errc{}&&parsed.ptr==value.data()+value.size()&&id&&id<=65535){s.sunId=uint16_t(id);break;}
    }
    if(!s.sunId)return false;
    if(!s.sunReady)
    {
        auto* selected=Sun(b,s.sunId);if(!selected)return false;
        if(!Require(a,!s.oldCoins.contains(s.sunId)&&!selected->mIsBeingCollected,
            "Adventure56 Sun proposal used a pre-baseline/collected Coin"))return false;
        s.sunReady=true;
    }
    if(!Barrier(a,"SUN_READY_"+std::to_string(s.sunId)))return false;
    if(a->NetIsClient()&&!s.sunClicked)
    {
        auto* coin=Sun(b,s.sunId);if(!coin)return false;HitResult hit;const int x=int(coin->mPosX+coin->mWidth*.5f),y=int(coin->mPosY+coin->mHeight*.5f);
        if(!b->MouseHitTest(x,y,&hit)||hit.mObject!=coin)return false;
        if(!Require(a,b->mSunMoney==200,"Adventure56 native Sun balance changed before its selected real pickup"))return false;
        s.sunSamples=Samples(a);s.sunClicked=BoardClick(a,float(x),float(y));return false;
    }
    auto* coin=Sun(b,s.sunId);
    if(b->mSunMoney!=225||(coin&&!coin->mIsBeingCollected)||(a->NetIsClient()&&Samples(a)<=s.sunSamples))return false;
    if(!s.sunObserved){s.sunObserved=true;NetTrace("SMOKE_ADVENTURE56_277_SUN role=%s level=%d id=%u value=25 before=200 after=225 producerId=%u producerToken=%u parentProof=exclusive-live-producer-after-shovel-barrier CoinParentMetadata=NotPresent motion=FROM_PLANT SkyExcluded=1 clientOwnedSDL=1 balanceWrites=0",Role(a),s.level,s.sunId,s.plants[2].id,s.plants[2].token);}
    return Capture(a,"sun")&&Barrier(a,"SUN225_"+std::to_string(s.sunId));
}
bool Shovel(LawnApp* a)
{
    auto* b=a->mBoard;auto* plant=ByRef(b,s.plants[3]);
    if(a->NetIsClient()&&!s.toolClicked)
    {
        if(!Require(a,b->mShowShovel&&plant,"Adventure56 client native shovel/owned target unavailable"))return false;
        const auto rect=b->GetShovelButtonRect();s.toolClicked=BoardClick(a,float(rect.mX+rect.mWidth/2),float(rect.mY+rect.mHeight/2));return false;
    }
    if(a->NetIsClient()&&!s.shovelClicked)
    {
        if(b->mCursorObject->mCursorType!=CURSOR_TYPE_SHOVEL)return false;
        if(!Require(a,plant!=nullptr,"Adventure56 client shovel target vanished before input"))return false;
        const auto& target=purchases[3];s.pickSamples=Samples(a);s.shovelClicked=BoardClick(a,float(b->GetCellCenterX(target.col,target.row)),float(b->GridToPixelY(target.col,target.row)+50));return false;
    }
    if(plant||(a->NetIsClient()&&(!s.shovelClicked||Samples(a)<=s.pickSamples)))return false;
    if(!Require(a,b->mSunMoney==200,"Adventure56 ordinary shovel unexpectedly changed the native Sun ledger"))return false;
    if(!s.shovelObserved){s.shovelObserved=true;NetTrace("SMOKE_ADVENTURE56_277_SHOVEL role=%s level=%d owner=1 id=%u token=%u actualRemoved=1 sun=200 ownedSDL=1 directDeath=0",Role(a),s.level,s.plants[3].id,s.plants[3].token);}
    return Capture(a,"clienttool")&&Barrier(a,"SHOVEL_"+std::to_string(s.plants[3].id)+"_"+std::to_string(s.plants[3].token));
}
bool ObserveWaves(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);Zombie* z=nullptr;bool visible=false;
    while(b->IterateZombies(z))if(!z->mDead&&z->mFromWave>=1&&z->mFromWave<=2)
    {
        const auto* d=GetNativeSourceZombieData(z);const auto* p=d?FindFusionZombieProfile(d->kind):nullptr;
        if(!Require(a,d&&p&&d->instanceToken&&z->mNetId&&!z->mMindControlled&&FusionSourceAdventureAllowsZombie272(s.level,d->kind)&&
            (s.level!=6||d->kind!=21)&&z->mBodyMaxHealth==p->maxHealth&&z->mHelmMaxHealth==p->firstArmor&&
            z->mZombieType==GetNativeSourceZombieCarrier(d->kind),"Adventure56 actual source first-wave identity/carrier/HP differs"))return false;
        auto [entry,fresh]=s.enemies.try_emplace(d->instanceToken,Enemy{{z->mNetId,d->instanceToken},d->kind,uint8_t(z->mFromWave),z->mPosX,false});
        auto& enemy=entry->second;
        if(!Require(a,enemy.ref.id==z->mNetId&&enemy.kind==d->kind&&enemy.wave==z->mFromWave,"Adventure56 wave canonical generation changed"))return false;
        if(fresh)NetTrace("SMOKE_ADVENTURE56_277_NPC role=%s level=%d kind=%u id=%u token=%u wave=%d row=%d body=%d armor=%d sourceFactory=1 stockFlag=0",
            Role(a),s.level,d->kind,z->mNetId,d->instanceToken,z->mFromWave,z->mRow,z->mBodyHealth,z->mHelmHealth);
        if(std::abs(enemy.firstX-z->mPosX)>=2.f)enemy.moved=true;
        s.seen100|=d->kind==100;
        float x=0,y=0,rx=0,ry=0;if(!GetNativeSourceZombie269NodePoint(z,"Shadow",x,y)||!GetNativeSourceZombie269NodePoint(z,"",rx,ry))continue;
        const auto* day=GetNativeDayZombie273(z);auto* head=a->ReanimationTryToGet(z->mSpecialHeadReanimID);
        if(d->kind==100&&z->mHasHead&&!z->IsDeadOrDying()&&!z->mIceTrapCounter&&!z->mButteredCounter&&
            !b->mTimeStopCounter&&day&&day->status==0&&head&&!head->mDead)
        {
            Plant* target=nullptr;while(b->IteratePlants(target))if(!target->mDead&&target->mIsOnBoard&&target->mRow==z->mRow)
            {
                float px=0,py=0;if(target->GetShadowCenter(px,py)&&px<x){s.shotRequired=true;break;}
            }
        }
        const float dx=float(b->mX+a->mDDInterface->mWideScreenOffsetX),dy=float(b->mY+a->mDDInterface->mWideScreenOffsetY);
        // ROOT can lie below a bottom-row sprite. Shadow is the real ground
        // anchor; log both rather than claiming a full sprite bounding box.
        if(enemy.moved&&x+dx>=80&&x+dx<=a->mWidth-80&&y+dy>=100&&y+dy<=a->mHeight-16)
        {
            visible=true;if(c->adventure.wave>=2&&!s.visibleLogged)
            {s.visibleLogged=true;NetTrace("SMOKE_ADVENTURE56_277_VISIBLE role=%s level=%d id=%u token=%u ROOT=(%.2f,%.2f) Shadow=(%.2f,%.2f) screenROOT=(%.2f,%.2f) screenShadow=(%.2f,%.2f) fullSpriteBounds=NotClaimed",
                Role(a),s.level,z->mNetId,d->instanceToken,rx,ry,x,y,rx+dx,ry+dy,x+dx,y+dy);}
        }
    }
    Projectile* shot=nullptr;while(b->IterateProjectiles(shot))if(const auto* d=GetNativeProjectileData(shot);d&&!shot->mDead&&d->kind==0&&
        d->isZombieBullet&&d->damage==20&&d->specialType==9&&NativeDay273ProjectileMetadataValid(*d,shot->mProjectileType,shot->mMotionType))
    {
        if(!s.shot100)NetTrace("SMOKE_ADVENTURE56_277_HOSTILE0 role=%s level=%d projectile=%u row=%d age=%d damage=20 way=9 naturallyFired=1 producerFamily100=exclusive-source-allow-list",Role(a),s.level,shot->mNetId,shot->mRow,shot->mProjectileAge);
        s.shot100=true;
    }
    if(!s.diagnostic||GetTickCount()-s.diagnostic>2000)
    {s.diagnostic=GetTickCount();NetTrace("SMOKE_ADVENTURE56_277_WAIT role=%s level=%d wave=%u scene=%d observations=%zu visible=%d seen100=%d eligibleHeadTarget100=%d hostile0=%d sun=%d",
        Role(a),s.level,c->adventure.wave,int(a->mGameScene),s.enemies.size(),visible,s.seen100,s.shotRequired,s.shot100,b->mSunMoney);}
    if(c->adventure.wave<2||!visible||s.enemies.empty()||(s.shotRequired&&!s.shot100))return false;
    std::array<int,2> counts{};bool firstMoved=false;for(const auto& pair:s.enemies)
    {const auto& e=pair.second;++counts[e.wave-1];if(e.wave==1)firstMoved|=e.moved;}
    if(!Require(a,counts[0]==1&&counts[1]>=1&&counts[1]<=3,"Adventure56 genuine D2 first-two-wave count exceeds source budget bounds"))return false;
    if(a->NetIsHost())
    {const auto& plan=c->adventureScheduler.waves;if(!Require(a,counts[0]==int(plan[0].size())&&counts[1]==int(plan[1].size()),"Adventure56 actual emitted wave counts differ from the real host plan"))return false;}
    if(!firstMoved)return false;s.counts=counts;
    return Capture(a,"firstwaves");
}
bool WaveProof(LawnApp* a,bool final)
{
    LawnNet::NetBuffer proof;for(const auto& pair:s.enemies)
    {const auto& e=pair.second;proof.WriteU16(e.ref.id);proof.WriteU32(e.ref.token);proof.WriteU16(e.kind);proof.WriteU8(e.wave);}
    char value[48];sprintf_s(value,"%s_%08x",final?"FINAL":"WAVES",LawnNet::NetFnv1a32(proof.Data(),proof.Size()));
    if(final){s.checkpoint=true;s.checkpointTag=Tag(value);}
    return Barrier(a,value);
}
void Completion(LawnApp* a)
{
    if(s.completionLogged)return;s.completionLogged=true;
    NetTrace("SMOKE_ADVENTURE56_277_LEVEL_COMPLETE role=%s level=%d epoch=%u profileFixture=1 progressionEarned=0 firstCount=%d secondCount=%d purchases=4 ownPacketsFirst=1 nativeBankCapacity=10 eligible=%d seen100=%d hostile0=%d eligibleHeadTarget100=%d shotWithoutEligibleTarget=NotRequired full20WaveVictory=NotExecuted finalDollChain=NotExecuted trophy=NotExecuted",Role(a),s.level,s.epoch,s.counts[0],s.counts[1],s.level==5?10:12,s.seen100,s.shot100,s.shotRequired);
}
bool Restart(LawnApp* a)
{
    const auto* c=GetNativeContext(a->mBoard);
    if(c&&c->epoch!=s.epoch)
    {
        if(!Require(a,s.checkpoint&&!s.checkpointTag.empty(),"Adventure56 fresh epoch arrived without local opening proof"))return true;
        if(!Has(a,0,s.checkpointTag)||!Has(a,1,s.checkpointTag))return true;Completion(a);
        s.level=6;s.waiting=0;s.playingFrame=-1;s.epoch=0;s.initial=s.pause=s.checkpoint=s.completionLogged=s.shot100=s.seen100=s.shotRequired=s.visibleLogged=false;
        s.sunReady=s.sunClicked=s.sunObserved=s.toolClicked=s.shovelClicked=s.shovelObserved=s.sunBaseline=false;s.oldCoins.clear();s.sunId=0;s.plants={};s.enemies.clear();s.counts={};s.checkpointTag.clear();Advance(a,1);return true;
    }
    if(a->NetIsHost()&&!s.profileSetup)
    {
        s.profileSetup=true;a->mNetManager->mConfig.mFusionSourceProfile=ExpectedProfile(6);
        NetTrace("SMOKE_ADVENTURE56_277_PROFILE_SETUP hostOnly=1 level=6 completed=31 frontier=6 isolatedRuntimeConfig=1 profilePersist=0 earnedByTrophy=0");
        Require(a,a->StartFusionSourceAdventure(6,false),"Adventure56 isolated source6 fresh opening failed");
    }
    return true;
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionAdventure56_277(LawnApp* a)
{
    if(!Enabled("PVZ_COOP_SMOKE_FUSION_ADVENTURE56_277"))return false;
    if(!a||!a->CoopSmokeActive()||!a->mNetManager||a->mCoopSmokeResultWritten)return true;
    if(!Require(a,Enabled("PVZ_COOP_SMOKE_FUSION_NATIVE")&&Enabled("PVZ_COOP_SMOKE_FUSION_SOURCE273")&&
        a->mCoopSmokeManifest.mScenario==NETSMOKE_SCENARIO_FUSION_VALIDATION,"Adventure56 requires owned native/source fusion_validation"))return true;
    if(!s.started)s.started=s.since=GetTickCount();
    if(GetTickCount()-s.started>360000||GetTickCount()-s.since>90000)
    {a->CoopSmokeFail("Adventure56 native opening/input/natural-wave deadline; inspect stage/ledger/wave traces");return true;}
    if(!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
    if(!s.stage)
    {
        if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
        if(!Require(a,Slot(a)>=0&&Slot(a)<2&&a->mNetManager->GetDifficulty()==1&&a->mPlayerInfo,"Adventure56 requires two peers/difficulty1/local profile"))return true;
        if(s.classicLevel<0){s.classicLevel=a->mPlayerInfo->mLevel;s.classicFinished=a->mPlayerInfo->mFinishedAdventure;s.localProfile=a->mNetManager->mConfig.mFusionSourceProfile;}
        if(a->NetIsClient()){if(Send(a,"A56277_BEGIN"))Advance(a,1);return true;}
        if(!Has(a,1,"A56277_BEGIN"))return true;
        if(!Require(a,SameProfile(s.localProfile,ExpectedProfile(5))&&a->StartFusionSourceAdventure(5,false),"Adventure56 Root-private initial5 profile/source admission failed"))return true;
        Advance(a,1);return true;
    }
    const auto* context=GetNativeContext(a->mBoard);
    if(s.stage==20||(s.checkpoint&&context&&context->epoch!=s.epoch))return Restart(a);
    if(!a->mBoard||!World(a))return true;auto* b=a->mBoard;const auto& bank=GetNativeContext(b)->selection.State();
    if(s.stage==1){if(Choose(a))Advance(a,2);}
    else if(s.stage==2)
    {
        if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}
        auto* chooser=a->mSeedChooserScreen;auto* button=chooser?chooser->mStartButton:nullptr;
        if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;
        const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
        if(!button->IsMouseOver())
        {SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"Adventure56 Ready hover failed");}
        else if(Click(a,x,y))Advance(a,3);
    }
    else if(s.stage==3)
    {
        if(bank.phase!=FusionBankPhase::Playing||a->mSeedChooserScreen||a->mGameScene!=SCENE_PLAYING)return true;
        if(s.playingFrame<0){s.playingFrame=a->mAppCounter;s.samples=Samples(a);return true;}
        if(a->mAppCounter<=s.playingFrame||Samples(a)-s.samples<2||!OwnFirst(a))return true;
        if(!s.initial)
        {
            if(!Require(a,b->mSunMoney==500&&!b->mPlants.mSize&&!b->mZombies.mSize&&GetNativeContext(b)->adventure.wave==0,
                "Adventure56 initial world contains stock preplants/waves or wrong Sun"))return true;
            if(!Plan(a)||!Capture(a,"initial"))return true;s.initial=true;
        }
        if(Barrier(a,"INITIAL"))Advance(a,4);
    }
    else if(s.stage>=4&&s.stage<=7)
    {if(PurchasePlant(a,s.stage-4))Advance(a,s.stage+1);}
    else if(s.stage==8)
    {if(Capture(a,"purchases")&&Barrier(a,"PURCHASES200"))Advance(a,9);}
    else if(s.stage==9)
    {if(Shovel(a))Advance(a,10);}
    else if(s.stage==10)
    {if(SunPickup(a))Advance(a,11);}
    else if(s.stage==11)
    {if(OwnFirst(a)&&ObserveWaves(a)&&WaveProof(a,false)){s.pause=false;Advance(a,12);}}
    else if(s.stage==12)
    {
        if(a->NetIsHost()&&!s.pause)s.pause=Escape(a);
        if(b->mPaused){s.samples=Samples(a);Advance(a,13);}
    }
    else if(s.stage==13)
    {
        if(!b->mPaused||Samples(a)-s.samples<2||!OwnFirst(a)||!Capture(a,"paused")||!WaveProof(a,true))return true;
        Completion(a);if(s.level==5){Advance(a,20);return true;}
        NetTrace("SMOKE_ADVENTURE56_277_COMPLETE role=%s sourceLevels=5,6 nativeOpenings=2 firstTwoWaves=2 realPurchases=8 clientSunPickup=2 clientShovel=2 ownPacketsFirst=2 sourceD2=1 cycle1=1 nativeBank10Policy=1 full20WaveVictory=NotExecuted finalDollRuntime=NotExecuted trophyProgress=NotExecuted level6Profile=IsolatedFixture actorWritesAfterBirth=0 directDamage=0 clockWrites=0 RNGWrites=0",Role(a));
        a->CoopSmokeWriteResult(true,nullptr);
    }
    return true;
}
}
