#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetSmokeCoinClick.h"
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
#include "../Fusion/FusionNativeLobber.h"
#include "../Fusion/FusionMoneyRules.h"
#include "../Fusion/FusionProducerRules.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../SexyAppFramework/DDInterface.h"
#include "../../SexyAppFramework/ScrollbarWidget.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
using namespace LawnFusion;
using LawnNet::NetTrace;
struct Ref {uint16_t id=0;uint32_t token=0;};
struct Buy {uint16_t incoming,result;bool extra;int owner,column,row;};
constexpr Buy buys[]={
    {31,31,false,0,0,1},{31,31,true,1,1,1},{31,1140,false,0,1,1},
    {31,31,true,1,0,2},{26,1134,false,1,0,2},{31,31,false,0,0,3},
    {28,1138,false,1,0,3},{31,31,true,1,0,4},{32,1141,false,1,0,4}};
struct Producer {int counter=0,pending=0,callbacks=0,cycles=0;};
struct State
{
    int stage=0,step=0,waitingOwn=0,introChecks=0,entryFrame=-1,expectedSun=2500,profileCoins=0;
    uint32_t epoch=0,since=0,chat=0,wheel=0,introFirst=0,introLast=0,pickSamples=0,pauseSamples=0,creditedAt=0;
    bool configured=false,initialized=false,prepared=false,picked=false,placed=false,observed=false,paused=false,resumed=false,setup=false,matrix=false,clicked=false,credited=false,diagnosed=false;
    Ref parent{},current{};std::map<uint16_t,Ref> actors;std::map<uint16_t,int> uses,coinValues;std::map<uint16_t,Producer> producers;
    std::array<uint16_t,3> targets{};std::array<int,3> health{10000,10000,10000};unsigned hitMask=0;
    uint16_t coin=0;std::string ids;std::set<uint16_t> payloads,projectiles;std::set<std::string> frames;std::map<std::string,int> draw;
    LawnNet::NetSmokeCoinClickState settled;
} s;
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool ok,const char* why){if(!ok)a->CoopSmokeFail(why);return ok;}
void Advance(LawnApp* a,int stage){s.stage=stage;s.since=GetTickCount();s.diagnosed=false;NetTrace("SMOKE_MONEY234_STAGE role=%s stage=%d step=%d epoch=%u",Role(a),stage,s.step,s.epoch);}
std::string Tag(const char* name){return "FM234_"+std::string(name)+"_"+std::to_string(s.epoch);}
bool Has(LawnApp* a,int who,const std::string& text){for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==who&&line.mText==text)return true;return false;}
bool Send(LawnApp* a,const std::string& text){if(Has(a,Slot(a),text))return true;if(s.chat&&GetTickCount()-s.chat<800)return false;s.chat=GetTickCount();a->mNetManager->SendChat(text);return Has(a,Slot(a),text);}
bool Barrier(LawnApp* a,const std::string& name){const auto text=Tag(name.c_str());Send(a,text);return Has(a,0,text)&&Has(a,1,text);}
bool Click(LawnApp* a,float x,float y)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"MoneyBatch owned SDL click failed");
}
bool BoardClick(LawnApp* a,float x,float y){return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY);}
bool Key(LawnApp* a){SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"MoneyBatch owned SDL Escape failed");}
bool Choice(LawnApp* a,SeedChooserScreen* c,int index)
{
    int x=0,y=0;c->GetSeedPositionInChooser(index,x,y);const float cy=float(y+35-int(c->mScrollbar->mScrollValue));
    if(cy<158||cy>493){if(s.wheel&&GetTickCount()-s.wheel<250)return false;s.wheel=GetTickCount();SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=float(c->mX+300);m.motion.y=float(c->mY+300);SDL_Event w{};w.type=SDL_EVENT_MOUSE_WHEEL;w.wheel.windowID=m.motion.windowID;w.wheel.y=cy<158?1.f:-1.f;Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&w),"MoneyBatch chooser SDL wheel failed");return false;}
    if(std::abs(c->mScrollAmount)>.05f)return false;return Click(a,float(c->mX+x+25),float(c->mY)+cy);
}
bool Capture(LawnApp* a,const char* suffix)
{
    const auto name="fusion_moneybatch_"+std::string(suffix);if(s.frames.contains(name))return true;if(!s.draw.contains(name)){s.draw[name]=a->mDrawCount;return false;}if(a->mDrawCount-s.draw[name]<2)return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"MoneyBatch actual renderer capture failed"))return false;s.frames.insert(name);NetTrace("SMOKE_MONEY234_RENDER role=%s frame=%s freshDraws=%d",Role(a),name.c_str(),a->mDrawCount-s.draw[name]);return true;
}
Plant* Actor(Board* b,Ref ref){Plant* p=nullptr;while(b->mPlants.IterateNext(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&p->mIsOnBoard&&p->mNetId==ref.id&&d&&d->instanceToken==ref.token)return p;}return nullptr;}
Plant* Cell(Board* b,int column,int row){Plant* p=nullptr;while(b->mPlants.IterateNext(p))if(!p->mDead&&p->mIsOnBoard&&p->mPlantCol==column&&p->mRow==row)return p;return nullptr;}
Ref Identity(Plant* p){const auto* d=GetNativePlantData(p);return p&&d?Ref{p->mNetId,d->instanceToken}:Ref{};}
Zombie* Target(Board* b,uint16_t id){Zombie* z=nullptr;while(b->mZombies.IterateNext(z))if(!z->mDead&&z->mNetId==id)return z;return nullptr;}
SeedPacket* Card(Board* b,uint16_t kind,bool extra,int owner){for(int i=0;i<b->mSeedBank->mNumPackets;++i){auto* p=&b->mSeedBank->mSeedPackets[i];const auto* d=GetNativePacketData(p);if(d&&d->kind==kind&&d->isExtra==extra&&d->owner==owner)return p;}return nullptr;}
bool Purchase(LawnApp* a)
{
    const auto& spec=buys[s.step];auto* b=a->mBoard;auto* card=Card(b,spec.incoming,spec.extra,spec.owner);FusionBankEligibleCard expected;
    if(!Require(a,card&&TryMakeFusionEligibleCard(spec.incoming,spec.extra,expected),"MoneyBatch source incoming card missing"))return false;const auto* packet=GetNativePacketData(card);
    if(!Require(a,packet&&packet->price==expected.baseCost&&!packet->randomOrigin&&packet->rechargeSeconds==7.5f,"MoneyBatch canonical incoming price/origin differs"))return false;
    if(!s.prepared){auto* parent=Cell(b,spec.column,spec.row);if(spec.result!=31){const auto* pd=GetNativePlantData(parent);if(!Require(a,pd&&pd->kind==31,"MoneyBatch expected real31 parent missing"))return false;s.parent=Identity(parent);}else if(!Require(a,!parent,"MoneyBatch base destination unexpectedly occupied"))return false;s.prepared=true;}
    if(Slot(a)==spec.owner&&!s.picked)
    {if(!card->mActive||card->mRefreshing)return false;auto* bank=b->mSeedBank;s.pickSamples=Samples(a);s.picked=BoardClick(a,float(bank->mX+card->mX+card->mOffsetX+25),float(bank->mY+card->mY+35));return false;}
    if(Slot(a)==spec.owner&&!s.placed)
    {auto* cursor=b->mCursorObject;const bool held=cursor&&cursor->mCursorType==CURSOR_TYPE_PLANT_FROM_BANK&&cursor->mSeedBankIndex>=0&&cursor->mSeedBankIndex<b->mSeedBank->mNumPackets&&b->mSeedBank->mSeedPackets[cursor->mSeedBankIndex].mNetId==card->mNetId;if(!held||(a->NetIsClient()&&Samples(a)<=s.pickSamples))return false;if(!Require(a,b->mSunMoney==s.expectedSun,"MoneyBatch pickup charged before placement"))return false;s.placed=BoardClick(a,float(b->GetCellCenterX(spec.column,spec.row)),float(b->GridToPixelY(spec.column,spec.row)+50));return false;}
    if(!s.observed)
    {
        auto* p=Cell(b,spec.column,spec.row);const auto* d=GetNativePlantData(p);if(!d||d->kind!=spec.result||(s.parent.id&&p->mNetId==s.parent.id))return false;
        const auto* def=FusionFindPlant(spec.result);const int first=spec.result==31?kFusionNoPlant:31,second=spec.result==31?kFusionNoPlant:spec.incoming;
        if(!Require(a,def&&d->owner==spec.owner&&d->packetId==card->mNetId&&d->firstParent==first&&d->secondParent==second&&!d->randomOrigin&&d->instanceToken&&p->mSeedType==GetNativeSeedForPlant(spec.result)&&p->mPlantHealth==def->maxHealth&&p->mPlantMaxHealth==def->maxHealth&&def->maxHealth==300&&b->mSunMoney==s.expectedSun-expected.baseCost&&card->mRefreshTime==750,"MoneyBatch actual recipe/HP/owner/price/CD/provenance differs"))return false;
        if(s.parent.id&&!Require(a,!Actor(b,s.parent)&&p->mNetId!=s.parent.id&&d->instanceToken!=s.parent.token,"MoneyBatch parent did not retire into a fresh generation"))return false;
        s.current=Identity(p);s.expectedSun-=expected.baseCost;++s.uses[card->mNetId];s.observed=true;if(spec.result!=31||s.step==0)s.actors[spec.result]=s.current;
        NetTrace("SMOKE_MONEY234_PURCHASE role=%s epoch=%u step=%d incoming=%u extra=%d result=%u id=%u token=%u parentId=%u parentToken=%u parents=%d,%d owner=%d packet=%u cost=%d sun=%d HP=300 cooldown=750 nativeSDL=1",Role(a),s.epoch,s.step,spec.incoming,spec.extra?1:0,spec.result,s.current.id,s.current.token,s.parent.id,s.parent.token,first,second,spec.owner,card->mNetId,expected.baseCost,s.expectedSun);
    }
    return Barrier(a,"BUY_"+std::to_string(s.step));
}
bool Observe(LawnApp* a)
{
    auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);
    for(uint16_t kind:{uint16_t(31),uint16_t(1140)})if(s.actors.contains(kind))if(auto* p=Actor(b,s.actors.at(kind)))
    {
        const auto* d=GetNativePlantData(p);auto& r=s.producers[kind];const auto* profile=FindFusionProducerProfile(kind);
        if(r.counter>0&&p->mLaunchCounter>r.counter+2){++r.cycles;if(a->NetIsHost()&&!Require(a,p->mLaunchCounter>=(kind==31?2300:1800)&&p->mLaunchCounter<=(kind==31?2700:2200),"MoneyBatch actual producer cycle differs"))return false;NetTrace("SMOKE_MONEY234_CYCLE role=%s kind=%u actor=%u cycle=%d count=%d",Role(a),kind,p->mNetId,p->mLaunchCounter,profile->periodicCoinCount);}
        if(r.pending>0&&!d->pendingProductionTicks){++r.callbacks;NetTrace("SMOKE_MONEY234_CALLBACK role=%s kind=%u actor=%u callbacks=%d count=%d",Role(a),kind,p->mNetId,r.callbacks,profile->periodicCoinCount);}
        r.counter=p->mLaunchCounter;r.pending=d->pendingProductionTicks;
    }
    Projectile* p=nullptr;while(b->mProjectiles.IterateNext(p))if(!p->mDead)if(const auto* d=GetNativeProjectileData(p);d&&(d->kind==74||d->kind==76||d->kind==78||d->kind==81))
    {
        s.payloads.insert(d->kind);if(s.projectiles.insert(p->mNetId).second){const auto* profile=FindFusionLobberProjectileProfile(d->kind);if(!Require(a,profile&&d->damage==profile->damage&&!d->sunOnHit&&!d->melonEnhanced&&IsNativeLobberProjectile(p),"MoneyBatch actual Silver projectile descriptor differs"))return false;NetTrace("SMOKE_MONEY234_PROJECTILE role=%s id=%u payload=%u damage=%d type=%d motion=%d enhanced=0 sun=0",Role(a),p->mNetId,d->kind,d->damage,int(p->mProjectileType),int(p->mMotionType));}
    }
    if(s.setup)for(size_t i=0;i<s.targets.size();++i)if(auto* z=Target(b,s.targets[i]))
    {if(a->NetIsHost()){const int delta=s.health[i]-z->mBodyHealth;if(delta){const bool valid=(i==0&&delta==60)||(i==1&&(delta==40||delta==60))||(i==2&&delta==100);if(!Require(a,valid,"MoneyBatch unexpected actual Silver HP delta"))return false;s.hitMask|=i==0?1u:i==2?8u:delta==40?2u:4u;NetTrace("SMOKE_MONEY234_HIT index=%zu target=%u delta=%d body=%d mask=%u",i,z->mNetId,delta,z->mBodyHealth,s.hitMask);}s.health[i]=z->mBodyHealth;}}
    Coin* coin=nullptr;while(b->mCoins.IterateNext(coin))if(!coin->mDead&&IsNativeMoneyCoin(coin))
    {const int value=GetNativeCoinValue(coin,0);if(value>0){if(!Require(a,FusionMoneyCoinMetadataValid(coin->mType,value),"MoneyBatch invalid source money item"))return false;s.coinValues[coin->mNetId]=value;}if(coin->mIsBeingCollected&&s.coinValues.contains(coin->mNetId)){if(!Require(a,s.coinValues.at(coin->mNetId)==25&&(!s.coin||s.coin==coin->mNetId),"MoneyBatch unexpected money pickup"))return false;s.coin=coin->mNetId;}}
    if(!s.credited&&ctx->money!=3000){if(!Require(a,s.coin&&ctx->money==3025,"MoneyBatch credit lacks one observed Money25"))return false;s.credited=true;s.creditedAt=GetTickCount();NetTrace("SMOKE_MONEY234_CREDIT role=%s id=%u delta=25 money=3025 sun=%d profileUnchanged=1",Role(a),s.coin,b->mSunMoney);}
    return Require(a,ctx->money==(s.credited?3025:3000)&&a->mPlayerInfo->mCoins==s.profileCoins&&(s.stage<6||b->mSunMoney==s.expectedSun),"MoneyBatch SUN/profile/source-money ledgers diverged");
}
bool SetupTargets(LawnApp* a)
{
    auto* b=a->mBoard;if(a->NetIsHost()&&!s.setup)
    {for(int i=0;i<3;++i){auto* z=b->AddZombieInRow(ZOMBIE_NORMAL,i+2,0);auto* body=z?a->ReanimationTryToGet(z->mBodyReanimID):nullptr;if(!Require(a,z&&z->mNetId&&body,"MoneyBatch native NPC setup failed"))return false;z->mBodyHealth=z->mBodyMaxHealth=10000;z->mHelmHealth=z->mHelmMaxHealth=z->mShieldHealth=z->mShieldMaxHealth=0;z->mPosY=z->GetPosYBasedOnRow(i+2);z->mY=int(z->mPosY);const float x=i==0?500.f:i==1?700.f:450.f;const auto r=z->GetZombieRect();z->mPosX+=x-(r.mX+r.mWidth*.5f);z->mX=int(z->mPosX);z->mVelX=0;z->ApplyAnimRate(0);s.targets[i]=z->mNetId;NetTrace("SMOKE_MONEY234_TARGET_SETUP index=%d id=%u row=%d body=10000 centerX=%.1f canonicalY=1 oneTimeOnly=1",i,z->mNetId,i+2,x);}s.setup=true;std::ostringstream out;out<<Tag("IDS");for(auto id:s.targets)out<<' '<<id;s.ids=out.str();}
    if(a->NetIsHost()){if(!Send(a,s.ids))return false;}else if(!s.setup){auto prefix=Tag("IDS")+" ";for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==0&&line.mText.rfind(prefix,0)==0){std::istringstream in(line.mText.substr(prefix.size()));for(auto& id:s.targets){unsigned n=0;if(!Require(a,bool(in>>n)&&n&&n<=65535,"MoneyBatch NPC identity barrier malformed"))return false;id=uint16_t(n);}s.setup=true;break;}}
    if(!s.setup)return false;for(auto id:s.targets){auto* z=Target(b,id);if(!z||z->mBodyHealth!=10000)return false;}return Barrier(a,"TARGETS");
}
bool Matrix(LawnApp* a,const char* when)
{
    auto* b=a->mBoard;for(uint16_t kind:{uint16_t(31),uint16_t(1140),uint16_t(1134),uint16_t(1138),uint16_t(1141)})
    {auto* p=Actor(b,s.actors[kind]);const auto* d=GetNativePlantData(p);if(!Require(a,p&&d&&p->mPlantHealth==300&&p->mPlantMaxHealth==300,"MoneyBatch final source actor/HP missing"))return false;NetTrace("SMOKE_MONEY234_ACTOR role=%s when=%s kind=%u id=%u token=%u owner=%u packet=%u parents=%u,%u HP=300 alias=%d",Role(a),when,kind,p->mNetId,d->instanceToken,d->owner,d->packetId,d->firstParent,d->secondParent,int(p->mSeedType));}
    if(std::strcmp(when,"after")==0)for(size_t i=0;i<s.targets.size();++i){auto* z=Target(b,s.targets[i]);if(!Require(a,z&&z->mBodyHealth<10000&&z->mHelmHealth==0&&z->mShieldHealth==0,"MoneyBatch natural NPC impact missing"))return false;NetTrace("SMOKE_MONEY234_OUTCOME role=%s index=%zu id=%u row=%d body=%d max=%d",Role(a),i,z->mNetId,z->mRow,z->mBodyHealth,z->mBodyMaxHealth);}
    for(const auto& [id,uses]:s.uses){SeedPacket* p=nullptr;for(int i=0;i<b->mSeedBank->mNumPackets;++i)if(b->mSeedBank->mSeedPackets[i].mNetId==id)p=&b->mSeedBank->mSeedPackets[i];if(!Require(a,p&&p->mRefreshTime==750&&(!a->NetIsHost()||p->mTimesUsed==uses),"MoneyBatch real packet use/CD identity differs"))return false;}
    return true;
}
bool Collect(LawnApp* a)
{
    if(!a->NetIsClient()||s.clicked)return true;Coin* coin=nullptr;int x=0,y=0;
    while(a->mBoard->mCoins.IterateNext(coin))if(!coin->mDead&&IsNativeMoneyCoin(coin)&&GetNativeCoinValue(coin,0)==25&&LawnNet::NetSmokeSettledCoinPoint(a->mBoard,coin,Samples(a),s.settled,x,y))
    {s.coin=coin->mNetId;s.clicked=BoardClick(a,float(x),float(y));NetTrace("SMOKE_MONEY234_CLICK role=client id=%u value=25 point=%d,%d singleSDL=1 settledSnapshots=%u margin=8",s.coin,x,y,Samples(a)-s.settled.coins[s.coin].firstSnapshot);return s.clicked;}return true;
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionMoneyBatch(LawnApp* a)
{
    const auto* enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_MONEY_BATCH");const auto* native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
    if(!a||!enabled||std::strcmp(enabled,"1")||!native||std::strcmp(native,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
    if(a->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();if(GetTickCount()-s.since>(s.stage==8?120000U:45000U)){char why[150];sprintf_s(why,"MoneyBatch deadline stage=%d step=%d payloads=%zu hitMask=%u callbacks=%d,%d credit=%d",s.stage,s.step,s.payloads.size(),s.hitMask,s.producers[31].callbacks,s.producers[1140].callbacks,s.credited?1:0);a->CoopSmokeFail(why);return true;}
    if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
    if(s.stage==0){if(a->NetIsHost()&&!s.configured){a->mNetManager->SetDifficulty(1);s.configured=true;}if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;if(a->NetIsClient()){Advance(a,1);Send(a,"FM234_BEGIN");}else if(Has(a,1,"FM234_BEGIN")){Advance(a,1);std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;}
    if(!a->mBoard||!IsNativeFusionBoard(a->mBoard)){if(s.stage==1&&a->NetIsClient())Send(a,"FM234_BEGIN");return true;}auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);if(!s.epoch)s.epoch=ctx->epoch;
    if(!Require(a,ctx->epoch==s.epoch&&ctx->validation&&!a->mFusionRuntime&&b->GetColumnCount()==10,"MoneyBatch canonical native activity lost"))return true;const auto& bank=ctx->selection.State();auto* chooser=a->mSeedChooserScreen;
    if(s.stage==1)
    {
        if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||!b->mCutScene->mSeedChoosing||chooser->mSeedsInFlight)return true;
        if(!Require(a,bank.targetCount==10&&bank.quotas[0]==5&&bank.quotas[1]==5&&bank.activeMask==3,"MoneyBatch real chooser quota differs"))return true;
        const std::array<std::pair<uint16_t,bool>,5> choices=Slot(a)==0?std::array<std::pair<uint16_t,bool>,5>{{{31,false},{0,false},{1,false},{2,false},{3,false}}}:std::array<std::pair<uint16_t,bool>,5>{{{31,true},{26,false},{28,false},{32,false},{0,true}}};
        int own=0;for(const auto& p:bank.packets)own+=p.owner==Slot(a);if(own<s.waitingOwn)return true;
        if(own<5){int index=-1;for(size_t i=0;i<ctx->eligible.size();++i)if(ctx->eligible[i].kind==choices[own].first&&ctx->eligible[i].isExtra==choices[own].second)index=int(i);if(!Require(a,index>=0,"MoneyBatch exact source card eligibility missing"))return true;if(Choice(a,chooser,index))s.waitingOwn=own+1;return true;}
        if(bank.packets.size()!=10)return true;int rank=-1;for(int i=0;i<b->mSeedBank->mNumPackets;++i){int r=NetSeedBankOwnerRank(Slot(a),b->mSeedBank->mSeedPackets[i].mNetOwnerSlot);if(!Require(a,r>=rank,"MoneyBatch own-first bank changed"))return true;rank=r;}if(!Capture(a,"chooser"))return true;NetTrace("SMOKE_MONEY234_POOL role=%s eligible=%zu target=10 quotas=5,5 ownFirst=1",Role(a),ctx->eligible.size());Advance(a,2);
    }
    else if(s.stage==2){if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}auto* button=chooser?chooser->mStartButton:nullptr;if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;if(!button->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"MoneyBatch Ready hover failed");}else if(Click(a,x,y))Advance(a,3);}
    else if(s.stage==3)
    {if(bank.phase==FusionBankPhase::Intro){if(!s.introFirst)s.introFirst=GetTickCount();s.introLast=GetTickCount();++s.introChecks;return true;}if(bank.phase!=FusionBankPhase::Playing||a->mGameScene!=SCENE_PLAYING||chooser)return true;if(!Require(a,s.introChecks>=10&&s.introLast-s.introFirst>=1000&&b->mSunMoney==500&&ctx->money==3000,"MoneyBatch native full intro/fresh ledgers differ"))return true;if(s.entryFrame<0){s.entryFrame=a->mAppCounter;return true;}if(a->mAppCounter<=s.entryFrame)return true;s.profileCoins=a->mPlayerInfo->mCoins;Advance(a,4);}
    else if(s.stage==4)
    {if(a->NetIsHost()&&!s.initialized){if(!Require(a,!b->mPlants.mSize&&!b->mZombies.mSize,"MoneyBatch fresh world not naturally empty"))return true;b->mSunMoney=2500;b->mSunCountDown=b->mZombieCountDown=b->mZombieCountDownStart=100000;s.initialized=true;NetTrace("SMOKE_MONEY234_INITIAL_CONTROL role=host sun=2500 once=1 money=3000 productionClocksUntouched=1");}if(b->mSunMoney!=2500)return true;if(Barrier(a,"INITIAL"))Advance(a,5);}
    else if(s.stage==5)
    {const bool bought=Purchase(a);if(a->mCoopSmokeResultWritten)return true;if(!Observe(a))return true;if(bought){++s.step;s.prepared=s.picked=s.placed=s.observed=false;s.parent=s.current={};if(s.step==9){if(!Require(a,s.expectedSun==1775&&s.actors.size()==5,"MoneyBatch nine source purchases/total differ"))return true;Advance(a,6);}else Advance(a,5);}}
    else if(s.stage==6){if(!Observe(a))return true;if(a->NetIsHost()&&!s.paused)s.paused=Key(a);if(b->mPaused){if(SetupTargets(a))Advance(a,7);}}
    else if(s.stage==7)
    {if(!s.pauseSamples)s.pauseSamples=Samples(a);if(Samples(a)-s.pauseSamples<2)return true;if(!s.matrix){if(!Matrix(a,"before"))return true;s.matrix=true;}if(!Capture(a,"paid")||!Barrier(a,"MATRIX"))return true;if(a->NetIsHost()&&!s.resumed){s.resumed=Key(a);return true;}if(!b->mPaused&&!a->GetDialogCount())Advance(a,8);}
    else if(s.stage==8)
    {
        if(!Observe(a))return true;if(s.payloads.size()==4)Capture(a,"silver");if(s.producers[31].callbacks&&s.producers[1140].callbacks)Capture(a,"producers");if(!Collect(a))return true;
        const bool natural=s.payloads.size()==4&&s.producers[31].callbacks>=2&&s.producers[1140].callbacks>=2&&s.credited&&(!a->NetIsHost()||s.hitMask==15);
        if(natural&&GetTickCount()-s.creditedAt>=800&&s.frames.contains("fusion_moneybatch_silver")&&s.frames.contains("fusion_moneybatch_producers")&&Barrier(a,"NATURAL")){s.paused=false;s.pauseSamples=0;s.matrix=false;Advance(a,9);}
        else if(!s.diagnosed&&GetTickCount()-s.since>10000){s.diagnosed=true;NetTrace("SMOKE_MONEY234_WAIT role=%s payloads=%zu hitMask=%u callbacks=%d,%d credit=%d money=%d",Role(a),s.payloads.size(),s.hitMask,s.producers[31].callbacks,s.producers[1140].callbacks,s.credited?1:0,ctx->money);}
    }
    else if(s.stage==9){if(!Observe(a))return true;if(a->NetIsHost()&&!s.paused)s.paused=Key(a);if(b->mPaused)Advance(a,10);}
    else if(s.stage==10)
    {if(!s.pauseSamples)s.pauseSamples=Samples(a);if(Samples(a)-s.pauseSamples<2)return true;if(!Observe(a))return true;if(!s.matrix){if(!Matrix(a,"after"))return true;s.matrix=true;}if(!Capture(a,"complete")||!Barrier(a,"COMPLETE"))return true;NetTrace("SMOKE_MONEY234_COMPLETE role=%s epoch=%u kinds=31,1140,1134,1138,1141 purchases=9 cost=725 sun=1775 money=3025 credit25=1 profileUnchanged=1 producersCount=1,2 payloads=74,76,78,81 postSetupWrites=0 nativeBoard=1 secondWorld=0 sourceZombieAIUnported=1",Role(a),s.epoch);Advance(a,11);a->CoopSmokeWriteResult(true,nullptr);}
    return true;
}
}
