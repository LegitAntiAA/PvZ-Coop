#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetSmokeCoinClick.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Coin.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../System/PlayerInfo.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativeProducer.h"
#include "../Fusion/FusionProducerRules.h"
#include "../../SexyAppFramework/DDInterface.h"
#include "../../SexyAppFramework/ScrollbarWidget.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>

namespace
{
using namespace LawnFusion;
using LawnNet::NetTrace;
struct Ref {uint16_t id=0;uint32_t token=0;};
struct Pause {bool started=false,verified=false;uint32_t seen=0,samples=0,at=0;int launch=0,pending=0,counter1=0,counter31=0;};
struct State
{
    int stage=0,step=0,waiting=0,introChecks=0,entry=-1,sun=500,profileCoins=0,lastCounter=0,lastPending=0,cycles=0,callbacks=0;
    uint32_t epoch=0,since=0,chat=0,wheel=0,introFirst=0,introLast=0,pickSamples=0,creditAt=0,clickAt=0;
    bool configured=false,setup=false,prepared=false,picked=false,placed=false,observed=false,pauseRequest=false,resumeRequest=false,resumeObserved=false,clicked=false,credited=false,diag=false;
    Ref parent{},child{};uint16_t coin=0;std::set<uint16_t> coins;std::array<Pause,2> pauses{};
    std::set<std::string> frames;std::map<std::string,int> draw;LawnNet::NetSmokeCoinClickState settled;
} s;
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool ok,const char* why){if(!ok)a->CoopSmokeFail(why);return ok;}
void Advance(LawnApp* a,int stage){s.stage=stage;s.since=GetTickCount();s.diag=false;NetTrace("SMOKE_SUN248_STAGE role=%s stage=%d step=%d epoch=%u",Role(a),stage,s.step,s.epoch);}
std::string Tag(const char* name){return "FS248_"+std::string(name)+"_"+std::to_string(s.epoch);}
bool Has(LawnApp* a,int slot,const std::string& text){for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==slot&&line.mText==text)return true;return false;}
bool Send(LawnApp* a,const std::string& text){if(Has(a,Slot(a),text))return true;if(s.chat&&GetTickCount()-s.chat<800)return false;s.chat=GetTickCount();a->mNetManager->SendChat(text);return Has(a,Slot(a),text);}
bool Barrier(LawnApp* a,const char* name){auto text=Tag(name);Send(a,text);return Has(a,0,text)&&Has(a,1,text);}
bool Click(LawnApp* a,float x,float y)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"SilverSunflower owned SDL click failed");
}
bool BoardClick(LawnApp* a,float x,float y){return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY);}
bool Key(LawnApp* a){SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"SilverSunflower owned SDL Escape failed");}
bool Choice(LawnApp* a,SeedChooserScreen* c,int index)
{
    int x=0,y=0;c->GetSeedPositionInChooser(index,x,y);const float cy=float(y+35-int(c->mScrollbar->mScrollValue));
    if(cy<158||cy>493){if(s.wheel&&GetTickCount()-s.wheel<250)return false;s.wheel=GetTickCount();SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=float(c->mX+300);m.motion.y=float(c->mY+300);SDL_Event w{};w.type=SDL_EVENT_MOUSE_WHEEL;w.wheel.windowID=m.motion.windowID;w.wheel.y=cy<158?1.f:-1.f;Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&w),"SilverSunflower actual chooser wheel failed");return false;}
    if(std::abs(c->mScrollAmount)>.05f)return false;return Click(a,float(c->mX+x+25),float(c->mY)+cy);
}
bool Capture(LawnApp* a,const char* suffix)
{const auto name="fusion_silversunflower_"+std::string(suffix);if(s.frames.contains(name))return true;if(!s.draw.contains(name)){s.draw[name]=a->mDrawCount;return false;}if(a->mDrawCount-s.draw[name]<2)return false;if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"SilverSunflower actual renderer capture failed"))return false;s.frames.insert(name);NetTrace("SMOKE_SUN248_RENDER role=%s frame=%s freshDraws=%d",Role(a),name.c_str(),a->mDrawCount-s.draw[name]);return true;}
Plant* Actor(Board* b,Ref ref){Plant* p=nullptr;while(b->mPlants.IterateNext(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&p->mIsOnBoard&&p->mNetId==ref.id&&d&&d->instanceToken==ref.token)return p;}return nullptr;}
Plant* Cell(Board* b){Plant* p=nullptr;while(b->mPlants.IterateNext(p))if(!p->mDead&&p->mIsOnBoard&&p->mPlantCol==3&&p->mRow==2)return p;return nullptr;}
SeedPacket* Card(Board* b,int kind,int owner){for(int i=0;i<b->mSeedBank->mNumPackets;++i){auto* p=&b->mSeedBank->mSeedPackets[i];const auto* d=GetNativePacketData(p);if(d&&d->kind==kind&&!d->isExtra&&d->owner==owner)return p;}return nullptr;}
bool Purchase(LawnApp* a)
{
    auto* b=a->mBoard;const int incoming=s.step?31:1,owner=s.step?1:0,result=s.step?1187:1,cost=s.step?25:50;
    auto* card=Card(b,incoming,owner);const auto* packet=GetNativePacketData(card);const auto* def=FusionFindPlant(result);
    if(!Require(a,card&&packet&&def&&def->maxHealth==300&&packet->price==cost&&!packet->randomOrigin&&packet->rechargeSeconds==7.5f,"SilverSunflower source card/definition missing"))return false;
    if(!s.prepared){auto* p=Cell(b);if(!Require(a,s.step?Actor(b,s.parent)==p&&p:!p,"SilverSunflower observed destination identity differs"))return false;s.prepared=true;}
    if(Slot(a)==owner&&!s.picked){if(!card->mActive||card->mRefreshing)return false;auto* bank=b->mSeedBank;s.pickSamples=Samples(a);s.picked=BoardClick(a,float(bank->mX+card->mX+card->mOffsetX+25),float(bank->mY+card->mY+35));return false;}
    if(Slot(a)==owner&&!s.placed){auto* cursor=b->mCursorObject;const bool held=cursor&&cursor->mCursorType==CURSOR_TYPE_PLANT_FROM_BANK&&cursor->mSeedBankIndex>=0&&cursor->mSeedBankIndex<b->mSeedBank->mNumPackets&&b->mSeedBank->mSeedPackets[cursor->mSeedBankIndex].mNetId==card->mNetId;if(!held||(a->NetIsClient()&&Samples(a)<=s.pickSamples))return false;if(!Require(a,b->mSunMoney==s.sun,"SilverSunflower pickup charged before planting"))return false;s.placed=BoardClick(a,float(b->GetCellCenterX(3,2)),float(b->GridToPixelY(3,2)+50));return false;}
    if(!s.observed)
    {
        auto* p=Cell(b);const auto* d=GetNativePlantData(p);if(!d||d->kind!=result)return false;
        if(!Require(a,d->instanceToken&&d->owner==owner&&d->packetId==card->mNetId&&!d->randomOrigin&&d->firstParent==(s.step?1:kFusionNoPlant)&&d->secondParent==(s.step?31:kFusionNoPlant)&&p->mSeedType==GetNativeSeedForPlant(result)&&p->mPlantHealth==300&&p->mPlantMaxHealth==300&&b->mSunMoney==s.sun-cost&&card->mRefreshTime==750&&(!a->NetIsHost()||card->mTimesUsed==1),"SilverSunflower actual paid ancestry/owner/SUN/CD/HP differs"))return false;
        Ref ref{p->mNetId,d->instanceToken};if(s.step){if(!Require(a,!Actor(b,s.parent)&&ref.id!=s.parent.id&&ref.token!=s.parent.token&&IsNativeProducer(p)&&def->sunCost==75&&def->productionIntervalSeconds==15,"SilverSunflower did not freshly replace its parent"))return false;s.child=ref;}else s.parent=ref;
        s.sun-=cost;s.observed=true;NetTrace("SMOKE_SUN248_PURCHASE role=%s epoch=%u step=%d kind=%d incoming=%d id=%u token=%u parentId=%u parentToken=%u owner=%d packet=%u cost=%d sun=%d HP=300 cooldown=750 alias=%d nativeSDL=1",Role(a),s.epoch,s.step,result,incoming,ref.id,ref.token,s.step?s.parent.id:0,s.step?s.parent.token:0,owner,card->mNetId,cost,s.sun,int(p->mSeedType));
    }
    return Barrier(a,s.step?"PAID1187":"PAID1");
}
bool Observe(LawnApp* a)
{
    auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);auto* p=Actor(b,s.child);const auto* d=GetNativePlantData(p);
    if(!Require(a,p&&d&&d->kind==1187&&p->mPlantHealth==300&&p->mPlantMaxHealth==300&&p->mLaunchCounter==d->productionTicks,"SilverSunflower real producer/generation/exact FNS clock differs"))return false;
    if(s.lastCounter>0&&p->mLaunchCounter>s.lastCounter+2){++s.cycles;if(a->NetIsHost()&&!Require(a,p->mLaunchCounter>=1301&&p->mLaunchCounter<=1701&&(p->mLaunchCounter-1)%100==0,"SilverSunflower source integer15+-2 cycle differs"))return false;NetTrace("SMOKE_SUN248_CYCLE role=%s actor=%u cycle=%d cycles=%d",Role(a),p->mNetId,p->mLaunchCounter,s.cycles);}
    if(s.lastPending>0&&!d->pendingProductionTicks){++s.callbacks;NetTrace("SMOKE_SUN248_CALLBACK role=%s actor=%u count=1 callbacks=%d",Role(a),p->mNetId,s.callbacks);}
    s.lastCounter=p->mLaunchCounter;s.lastPending=d->pendingProductionTicks;
    Coin* coin=nullptr;while(b->mCoins.IterateNext(coin))if(!coin->mDead&&coin->IsSun())
    {if(!Require(a,coin->GetSunValue()==25,"SilverSunflower unexpected sun denomination"))return false;if(s.coins.insert(coin->mNetId).second)NetTrace("SMOKE_SUN248_COIN role=%s id=%u value=25 child=%u token=%u",Role(a),coin->mNetId,s.child.id,s.child.token);if(coin->mIsBeingCollected){if(!Require(a,!s.coin||s.coin==coin->mNetId,"SilverSunflower multiple actual sun pickups"))return false;s.coin=coin->mNetId;}}
    if(!s.credited&&b->mSunMoney==450){if(!Require(a,s.coin&&s.coins.contains(s.coin),"SilverSunflower SUN credit lacks actual native coin"))return false;s.sun=450;s.credited=true;s.creditAt=GetTickCount();NetTrace("SMOKE_SUN248_CREDIT role=%s id=%u before=425 after=450 value=25 money=3000 profileUnchanged=1",Role(a),s.coin);}
    return Require(a,b->mSunMoney==s.sun&&ctx->money==3000&&a->mPlayerInfo->mCoins==s.profileCoins,"SilverSunflower SUN/source money/profile ledgers diverged");
}
bool PauseProof(LawnApp* a,int round)
{
    auto* b=a->mBoard;auto& q=s.pauses[round];if(q.verified)return true;auto* p=Actor(b,s.child);const auto* d=GetNativePlantData(p);auto* first=Card(b,1,0);auto* second=Card(b,31,1);
    if(!Require(a,b->mPaused&&p&&d&&first&&second&&first->mRefreshTime==750&&second->mRefreshTime==750,"SilverSunflower pause lost actual actor/cards"))return false;
    if(Samples(a)-q.seen<2)return false;
    if(!q.started){q.started=true;q.samples=Samples(a);q.at=GetTickCount();q.launch=p->mLaunchCounter;q.pending=d->pendingProductionTicks;q.counter1=first->mRefreshCounter;q.counter31=second->mRefreshCounter;if(!Require(a,round!=0||q.pending>0,"SilverSunflower first pause missed pending callback"))return false;return false;}
    if(!Require(a,p->mLaunchCounter==q.launch&&d->pendingProductionTicks==q.pending&&first->mRefreshCounter==q.counter1&&second->mRefreshCounter==q.counter31,"SilverSunflower actual producer/card clock changed while paused"))return false;
    if(GetTickCount()-q.at<400||Samples(a)-q.samples<2)return false;q.verified=true;
    NetTrace("SMOKE_SUN248_PAUSE role=%s round=%d epoch=%u launch=%d pending=%d card1=%d time1=750 card31=%d time31=750 stableMs=%u snapshots=%u sun=%d money=3000",Role(a),round,s.epoch,q.launch,q.pending,q.counter1,q.counter31,GetTickCount()-q.at,Samples(a)-q.samples,b->mSunMoney);return true;
}
bool Collect(LawnApp* a)
{
    if(a->NetIsClient()&&s.clicked&&!s.credited&&GetTickCount()-s.clickAt>4000)return Require(a,false,"SilverSunflower single settled sun click lacked credit");
    if(!a->NetIsClient()||s.clicked)return true;Coin* coin=nullptr;int x=0,y=0;
    while(a->mBoard->mCoins.IterateNext(coin))if(!coin->mDead&&coin->IsSun()&&coin->GetSunValue()==25&&LawnNet::NetSmokeSettledCoinPoint(a->mBoard,coin,Samples(a),s.settled,x,y))
    {s.coin=coin->mNetId;s.clicked=BoardClick(a,float(x),float(y));s.clickAt=GetTickCount();NetTrace("SMOKE_SUN248_CLICK role=client id=%u value=25 point=%d,%d singleSDL=1 settledSnapshots=%u margin=8",s.coin,x,y,Samples(a)-s.settled.coins[s.coin].firstSnapshot);return s.clicked;}return true;
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionSilverSunflower(LawnApp* a)
{
    const auto* flag=std::getenv("PVZ_COOP_SMOKE_FUSION_SILVER_SUNFLOWER");const auto* native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
    if(!a||!flag||std::strcmp(flag,"1")||!native||std::strcmp(native,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
    if(a->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();if(GetTickCount()-s.since>45000U){char why[140];sprintf_s(why,"SilverSunflower deadline stage=%d callbacks=%d cycles=%d coin=%u credit=%d",s.stage,s.callbacks,s.cycles,s.coin,s.credited?1:0);a->CoopSmokeFail(why);return true;}
    if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
    if(s.stage==0){if(a->NetIsHost()&&!s.configured){a->mNetManager->SetDifficulty(1);s.configured=true;}if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;if(a->NetIsClient()){Advance(a,1);Send(a,"FS248_BEGIN");}else if(Has(a,1,"FS248_BEGIN")){Advance(a,1);std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;}
    if(!a->mBoard||!IsNativeFusionBoard(a->mBoard)){if(s.stage==1&&a->NetIsClient())Send(a,"FS248_BEGIN");return true;}auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);if(!s.epoch)s.epoch=ctx->epoch;
    if(!Require(a,ctx->epoch==s.epoch&&ctx->validation&&!a->mFusionRuntime&&b->GetColumnCount()==10,"SilverSunflower canonical native activity missing"))return true;const auto& bank=ctx->selection.State();auto* chooser=a->mSeedChooserScreen;
    if(s.stage==1)
    {
        if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||!b->mCutScene->mSeedChoosing||chooser->mSeedsInFlight)return true;if(!Require(a,bank.activeMask==3&&bank.targetCount==10&&bank.quotas[0]==5&&bank.quotas[1]==5,"SilverSunflower real fair quota differs"))return true;
        const std::array<std::pair<uint16_t,bool>,5> choices=Slot(a)==0?std::array<std::pair<uint16_t,bool>,5>{{{1,false},{0,false},{2,false},{3,false},{4,false}}}:std::array<std::pair<uint16_t,bool>,5>{{{31,false},{0,true},{1,true},{2,true},{3,true}}};
        int own=0;for(const auto& p:bank.packets)own+=p.owner==Slot(a);if(own<s.waiting)return true;if(own<5){int index=-1;for(size_t i=0;i<ctx->eligible.size();++i)if(ctx->eligible[i].kind==choices[own].first&&ctx->eligible[i].isExtra==choices[own].second)index=int(i);if(!Require(a,index>=0,"SilverSunflower source1/31 eligibility missing"))return true;if(Choice(a,chooser,index))s.waiting=own+1;return true;}if(bank.packets.size()!=10)return true;
        int rank=-1;for(int i=0;i<b->mSeedBank->mNumPackets;++i){int r=NetSeedBankOwnerRank(Slot(a),b->mSeedBank->mSeedPackets[i].mNetOwnerSlot);if(!Require(a,r>=rank,"SilverSunflower own-first bank order differs"))return true;rank=r;}if(!Capture(a,"chooser"))return true;NetTrace("SMOKE_SUN248_POOL role=%s eligible=%zu target=10 quotas=5,5 ownFirst=1",Role(a),ctx->eligible.size());Advance(a,2);
    }
    else if(s.stage==2){if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}auto* button=chooser?chooser->mStartButton:nullptr;if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;if(!button->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"SilverSunflower Ready hover failed");}else if(Click(a,x,y))Advance(a,3);}
    else if(s.stage==3){if(bank.phase==FusionBankPhase::Intro){if(!s.introFirst)s.introFirst=GetTickCount();s.introLast=GetTickCount();++s.introChecks;return true;}if(bank.phase!=FusionBankPhase::Playing||a->mGameScene!=SCENE_PLAYING||chooser)return true;if(!Require(a,s.introChecks>=10&&s.introLast-s.introFirst>=1000&&b->mSunMoney==500&&ctx->money==3000,"SilverSunflower skipped native full intro/fresh ledgers"))return true;if(s.entry<0){s.entry=a->mAppCounter;return true;}if(a->mAppCounter<=s.entry)return true;s.profileCoins=a->mPlayerInfo->mCoins;Advance(a,4);}
    else if(s.stage==4){if(a->NetIsHost()&&!s.setup){if(!Require(a,!b->mPlants.mSize&&!b->mZombies.mSize,"SilverSunflower initial world not naturally empty"))return true;b->mSunCountDown=b->mZombieCountDown=b->mZombieCountDownStart=100000;s.setup=true;NetTrace("SMOKE_SUN248_SETUP sun=500 money=3000 ambientOnly=1 sunWrites=0 actorWrites=0");}if(Barrier(a,"INITIAL"))Advance(a,5);}
    else if(s.stage==5){if(s.step==2){if(Capture(a,"fused"))Advance(a,6);return true;}const bool done=Purchase(a);if(a->mCoopSmokeResultWritten)return true;if(done){++s.step;s.prepared=s.picked=s.placed=s.observed=false;}}
    else if(s.stage==6){if(!Observe(a))return true;const auto* d=GetNativePlantData(Actor(b,s.child));if(a->NetIsHost()&&d->pendingProductionTicks>0&&!s.pauseRequest)s.pauseRequest=Key(a);if(b->mPaused){s.pauses[0].seen=Samples(a);Advance(a,7);}}
    else if(s.stage==7){if(!Observe(a)||!PauseProof(a,0)||!Capture(a,"paused0")||!Barrier(a,"PAUSED0"))return true;Advance(a,8);}
    else if(s.stage==8)
    {
        if(!Observe(a))return true;if(a->NetIsHost()&&!s.resumeRequest){s.resumeRequest=Key(a);return true;}auto* p=Actor(b,s.child);const auto* d=GetNativePlantData(p);
        if(b->mPaused||a->GetDialogCount()||!(p->mLaunchCounter<s.pauses[0].launch||d->pendingProductionTicks!=s.pauses[0].pending))return true;
        if(!s.resumeObserved){s.resumeObserved=true;NetTrace("SMOKE_SUN248_RESUMED role=%s round=0 epoch=%u launch=%d pending=%d observedPlaying=1",Role(a),s.epoch,p->mLaunchCounter,d->pendingProductionTicks);}
        if(Barrier(a,"RESUMED0")){NetTrace("SMOKE_SUN248_RESUME_ACKS role=%s epoch=%u host=1 client=1 beforeRepause=1",Role(a),s.epoch);Advance(a,9);}
    }
    else if(s.stage==9)
    {if(!Observe(a))return true;if(s.callbacks)Capture(a,"produced");if(!Collect(a))return true;if(s.callbacks>=2&&s.cycles>=2&&s.credited&&GetTickCount()-s.creditAt>=800&&s.frames.contains("fusion_silversunflower_produced")&&Barrier(a,"NATURAL")){s.pauseRequest=false;Advance(a,10);}else if(!s.diag&&GetTickCount()-s.since>10000){s.diag=true;NetTrace("SMOKE_SUN248_WAIT role=%s callbacks=%d cycles=%d clicked=%d credit=%d coins=%zu sun=%d",Role(a),s.callbacks,s.cycles,s.clicked?1:0,s.credited?1:0,s.coins.size(),b->mSunMoney);}}
    else if(s.stage==10){if(!Observe(a))return true;if(a->NetIsHost()&&!s.pauseRequest)s.pauseRequest=Key(a);if(b->mPaused){s.pauses[1].seen=Samples(a);Advance(a,11);}}
    else if(s.stage==11){if(!Observe(a)||!PauseProof(a,1)||!Capture(a,"paused1")||!Capture(a,"complete")||!Barrier(a,"COMPLETE"))return true;NetTrace("SMOKE_SUN248_COMPLETE role=%s epoch=%u id=%u token=%u parents=1,31 owner=1 purchases=2 cost=75 sun=450 collected=25 money=3000 profileUnchanged=1 interval=15 count=1 callbacks=%d cycles=%d pauses=2 resumedBoth=1 sunWrites=0 actorWrites=0 nativeBoard=1 secondWorld=0",Role(a),s.epoch,s.child.id,s.child.token,s.callbacks,s.cycles);Advance(a,12);a->CoopSmokeWriteResult(true,nullptr);}
    return true;
}
}
