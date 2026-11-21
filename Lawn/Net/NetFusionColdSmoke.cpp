#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../Coin.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionWire.h"
#include "../Fusion/FusionNativeGloveUI.h"
#include "../Fusion/FusionNativeCherryShooter.h"
#include "../Fusion/FusionShooterTiming.h"
#include "../Fusion/FusionNativeCold.h"
#include "../Fusion/FusionColdRules.h"
#include "../Fusion/FusionNativeWire.h"
#include "../Fusion/FusionRecipe.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../SexyAppFramework/DDInterface.h"
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
constexpr int kRow=2,kInitialWallet=2000,kTotalCost=1550,kFinalWallet=450;
struct Ref {uint16_t id=0;uint32_t token=0;};
struct Volley {int tick=0;std::vector<int> deadlines;std::vector<uint16_t> ids;};
struct Shot {int tick=0,age=0,volley=0,event=0;float x=0;bool retired=false;};
struct TargetState {int tick=0,body=0,helm=0,shield=0,level=0,threshold=100,slow=0,freeze=0,hits=0;};
struct Combat
{
 int kind=0,row=0,previousCounter=0,previousTick=0,desiredVolleys=2,hits=0;
 bool setup=false,started=false,parked=false,crossed=false,thawed=false,mcTraversed=false;
 std::array<uint16_t,7> targets{};std::array<TargetState,7> before{};
 std::vector<Volley> volleys;std::map<uint16_t,Shot> shots;std::set<uint16_t> clientSeen;
};
struct State
{
 int stage=0,step=0,waitingOwn=0,money=kInitialWallet,playingTick=-1;
 std::array<int,5> uses{};Ref current{},aux{},ice{};Combat combat{};
 std::array<uint16_t,4> globalIds{};uint16_t noAura=0;
 int iceFirstTick=0,iceFirstCounter=0,iceLastCounter=0,iceLastTick=0,iceRetireTick=0,iceEatingSamples=0;
 int pauseSlow=0,pauseFreeze=0,pauseTick=0,pauseChecks=0,pauseRequestedAt=0;
 uint32_t since=0,lastChat=0,introStart=0,introLast=0,introChecks=0,pickSamples=0,purchaseSamples=0,pauseSince=0;
 bool configured=false,clicked=false,placed=false,observed=false,toolClicked=false,budget=false,budgetWritten=false;
 bool globalSetup=false,globalHit=false,globalThawed=false,biterPositioned=false,negative=false,exactReplay=false;
 bool pauseRequested=false,pauseObserved=false,resumeQueued=false,pauseDone=false;
 std::map<std::string,int> firstDraw;std::set<std::string> frames;std::set<uint16_t> stationaryTargets,pendingSnapshotLogged;
} s;
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool ok,const char* why){if(!ok)a->CoopSmokeFail(why);return ok;}
void Advance(LawnApp* a,int stage)
{
    s.stage=stage;s.since=GetTickCount();s.clicked=s.placed=s.observed=s.toolClicked=false;
    LawnNet::NetTrace("SMOKE_FUSION_COLD_STAGE role=%s stage=%d",Role(a),stage);
}
bool Has(LawnApp* a,int who,const std::string& text)
{for(const auto& l:a->mNetManager->mChatLines)if(l.mSlot==who&&l.mText==text)return true;return false;}
bool Send(LawnApp* a,const std::string& text)
{
    if(Has(a,Slot(a),text))return true;if(s.lastChat&&GetTickCount()-s.lastChat<800)return false;
    s.lastChat=GetTickCount();a->mNetManager->SendChat(text);return Has(a,Slot(a),text);
}
bool Barrier(LawnApp* a,const std::string& text){Send(a,text);return Has(a,0,text)&&Has(a,1,text);}
bool Capture(LawnApp* a,const char* name)
{
    if(s.frames.contains(name))return true;if(!s.firstDraw.contains(name)){s.firstDraw[name]=a->mDrawCount;return false;}
    if(a->mDrawCount-s.firstDraw[name]<2)return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name),"Cold native renderer capture failed"))return false;
    s.frames.insert(name);LawnNet::NetTrace("SMOKE_FUSION_COLD_RENDER role=%s frame=%s freshDraws=%d",Role(a),name,a->mDrawCount-s.firstDraw[name]);return true;
}
bool Click(LawnApp* a,float x,float y,uint8_t button=SDL_BUTTON_LEFT,bool click=true)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    if(!click)return Require(a,SDL_PushEvent(&m),"Cold scoped motion failed");
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=button;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;
    return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Cold scoped SDL click failed");
}
bool BoardClick(LawnApp* a,float x,float y,uint8_t button=SDL_BUTTON_LEFT,bool click=true)
{return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY,button,click);}
bool Cell(LawnApp* a,int col,int row=kRow,uint8_t button=SDL_BUTTON_LEFT,bool click=true)
{return BoardClick(a,float(a->mBoard->GetCellCenterX(col,row)),float(a->mBoard->GridToPixelY(col,row)+50),button,click);}
bool Ready(LawnApp* a,GameButton* b)
{
    if(!b||b->mDisabled||b->mBtnNoDraw)return false;float x=b->mX+b->mWidth*.5f,y=b->mY+b->mHeight*.5f;
    if(!b->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"Cold Ready hover failed");return false;}
    LawnNet::NetTrace("SMOKE_FUSION_COLD_READY role=%s nativeSDL=1 hoverObserved=1",Role(a));return Click(a,x,y);
}
Plant* ByRef(Board* b,Ref r)
{Plant* p=nullptr;while(b->mPlants.IterateNext(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&p->mIsOnBoard&&p->mNetId==r.id&&d&&d->instanceToken==r.token)return p;}return nullptr;}
Plant* At(Board* b,int col,int row)
{Plant* p=nullptr;while(b->mPlants.IterateNext(p))if(!p->mDead&&p->mIsOnBoard&&p->mPlantCol==col&&p->mRow==row)return p;return nullptr;}
Ref Identity(Plant* p){const auto* d=GetNativePlantData(p);return p&&d?Ref{p->mNetId,d->instanceToken}:Ref{};}
SeedPacket* Packet(Board* b,int who,int kind,bool extra)
{for(int i=0;i<b->mSeedBank->mNumPackets;++i){auto* p=&b->mSeedBank->mSeedPackets[i];const auto* d=GetNativePacketData(p);if(d&&d->owner==who&&d->kind==kind&&d->isExtra==extra)return p;}return nullptr;}
bool HeldPacket(Board* b,SeedPacket* p)
{const auto* c=b->mCursorObject;return c&&p&&c->mCursorType==CURSOR_TYPE_PLANT_FROM_BANK&&c->mSeedBankIndex>=0&&c->mSeedBankIndex<b->mSeedBank->mNumPackets&&b->mSeedBank->mSeedPackets[c->mSeedBankIndex].mNetId==p->mNetId;}
bool PickPacket(LawnApp* a,SeedPacket* p)
{if(!Require(a,p&&p->mActive&&!p->mRefreshing,"Cold real packet not ready"))return false;auto* b=a->mBoard->mSeedBank;return BoardClick(a,float(b->mX+p->mX+p->mOffsetX+p->mWidth/2),float(b->mY+p->mY+p->mHeight/2));}

bool Key(LawnApp* a,SDL_Keycode code)
{
 SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=code;d.key.scancode=SDL_GetScancodeFromKey(code,nullptr);d.key.down=true;
 SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Cold owned SDL key failed");
}
void Next(LawnApp* a){++s.step;s.since=GetTickCount();s.clicked=s.placed=s.observed=s.toolClicked=false;LawnNet::NetTrace("SMOKE_FUSION_COLD_STEP role=%s step=%d",Role(a),s.step);}
int CardIndex(int kind,bool extra){return kind==0?(extra?1:0):kind==6?2:extra?4:3;}
bool Business(LawnApp* a)
{
 auto* b=a->mBoard;constexpr int owners[]={0,1,0,0,1},kinds[]={0,0,6,10,10},times[]={750,750,750,5000,5000};
 for(int i=0;i<5;++i)
 {
  auto* p=Packet(b,owners[i],kinds[i],i==1||i==4);const auto* d=GetNativePacketData(p);
  const int price=i==0?100:i==1?200:i==2?0:i==3?75:150;
  const bool valid=p&&d&&d->price==price&&d->owner==owners[i]&&!d->randomOrigin&&p->mRefreshTime==(s.uses[i]?times[i]:0)&&(!a->NetIsHost()||p->mTimesUsed==s.uses[i]);
  if(!valid)LawnNet::NetTrace("SMOKE_FUSION_COLD_PACKET_MISMATCH role=%s index=%d kind=%d expectedOwner=%d found=%d packet=%u owner=%d price=%d expectedPrice=%d refreshTime=%d expectedTime=%d timesUsed=%d expectedUses=%d players=%d",Role(a),i,kinds[i],owners[i],p?1:0,p?p->mNetId:0,d?d->owner:-1,d?d->price:-1,price,p?p->mRefreshTime:-1,s.uses[i]?times[i]:0,p?p->mTimesUsed:-1,s.uses[i],a->mNetManager->GetPlayerCount());
  if(!Require(a,valid,"Cold canonical price/owner/used-card cooldown/identity changed"))return false;
 }
 if(!Require(a,b->mSunMoney==s.money,"Cold wallet drift/automatic sun credit"))return false;
 Coin* coin=nullptr;while(b->mCoins.IterateNext(coin))if(!Require(a,coin->mDead,"Cold generated an unsupported reward/sun"))return false;return true;
}
Zombie* Target(Board* b,uint16_t id)
{
 Zombie* z=nullptr;while(b->mZombies.IterateNext(z))if(!z->mDead&&z->mNetId==id)
 {
  // BOARDEV_ZOMBIE_ADDED publishes identity before host callers finish setup.
  // mVisible becomes true only after the first complete native snapshot.
  if(z->mNetGhost&&!z->mVisible)
  {
   if(s.pendingSnapshotLogged.insert(id).second)LawnNet::NetTrace("SMOKE_FUSION_COLD_TARGET_PENDING id=%u body=%d slow=%d freeze=%d metadata=%d visible=0 ghost=1 applied=%u awaitingActualSnapshot=1",id,z->mBodyHealth,z->mChilledCounter,z->mIceTrapCounter,GetNativeZombieColdData(z)?1:0,Samples(b->mApp));
   return nullptr;
  }
  return z;
 }return nullptr;
}
void SetX(Zombie* z,float x){z->mPosX=x;z->mX=int(x);}
bool Ground(LawnApp* a,Zombie* z,float x)
{
 float gx=0,gy=0;if(!Require(a,z&&z->GetShadowCenter(gx,gy),"Cold native shadow unavailable"))return false;SetX(z,z->mPosX+x-gx);return true;
}
Zombie* Spawn(LawnApp* a,int row,int helm,int shield,bool mc,float x)
{
 auto* z=a->mBoard->AddZombieInRow(ZOMBIE_NORMAL,row,0);auto* body=z?a->ReanimationTryToGet(z->mBodyReanimID):nullptr;const float rate=body?body->mAnimRate:0;
 if(!Require(a,z&&z->mNetId&&body&&std::isfinite(rate)&&rate>0,"Cold native target setup failed"))return nullptr;
 if(helm){z->mHelmType=HELMTYPE_BUCKET;z->AttachHelmet();}if(shield){z->mShieldType=SHIELDTYPE_DOOR;z->AttachShield();}
 z->mBodyHealth=z->mBodyMaxHealth=5000;z->mHelmHealth=z->mHelmMaxHealth=helm;z->mShieldHealth=z->mShieldMaxHealth=shield;
 z->mMindControlled=mc;z->mPosY=z->GetPosYBasedOnRow(row);z->mY=int(z->mPosY);z->mVelX=0;z->SetAnimRate(rate);z->ApplyAnimRate(0);SetX(z,x);
 s.stationaryTargets.insert(z->mNetId);return z;
}
std::string Ids(const char* tag,int kind,const std::array<uint16_t,7>& ids)
{std::ostringstream o;o<<tag<<' '<<kind;for(auto id:ids)o<<' '<<id;return o.str();}
bool ReadIds(LawnApp* a,const char* tag,int kind,std::array<uint16_t,7>& ids)
{
 for(const auto& l:a->mNetManager->mChatLines)if(l.mSlot==0&&l.mText.rfind(tag,0)==0)
 {
  std::istringstream in(l.mText.substr(std::strlen(tag)));int readKind=0;if(!(in>>readKind)||readKind!=kind)continue;std::set<unsigned> seen;
  for(auto& id:ids){unsigned value=0;if(!Require(a,bool(in>>value)&&value&&value<=65535&&seen.insert(value).second,"Cold canonical target IDs invalid"))return false;id=uint16_t(value);}std::string trailing;return Require(a,!(in>>trailing),"Cold target IDs trailing bytes");
 }return false;
}
TargetState Read(Zombie* z,int tick)
{const auto* d=GetNativeZombieColdData(z);return {tick,z->mBodyHealth,z->mHelmHealth,z->mShieldHealth,d?d->level:0,d?d->threshold:100,z->mChilledCounter,z->mIceTrapCounter,0};}
bool Purchase(LawnApp* a,int who,int kind,bool extra,int col,int row,int result,int before,int after,int first,int second,Ref& out)
{
 auto* b=a->mBoard;auto* card=Packet(b,who,kind,extra);
 if(Slot(a)==who&&!s.clicked){if(!card||!card->mActive||card->mRefreshing)return false;s.pickSamples=Samples(a);if(PickPacket(a,card))s.clicked=true;return false;}
 if(Slot(a)==who&&!s.placed){if(!HeldPacket(b,card)||(a->NetIsClient()&&Samples(a)<=s.pickSamples))return false;if(!Require(a,b->mSunMoney==before,"Cold actual pickup charged before placement"))return false;s.placed=Cell(a,col,row);return false;}
 if(!s.observed)
 {
  auto* p=At(b,col,row);const auto* d=GetNativePlantData(p);if(!d||d->kind!=result)return false;
  if(!Require(a,card&&d->owner==who&&d->packetId==card->mNetId&&d->instanceToken&&!d->randomOrigin&&d->firstParent==first&&d->secondParent==second&&p->mPlantHealth==300&&!p->mIsAsleep&&b->mSunMoney==after,"Cold actual directed purchase/awake/HP/provenance differs"))return false;
  out=Identity(p);s.money=after;++s.uses[CardIndex(kind,extra)];s.observed=true;
  if(result==10){s.ice=out;s.iceFirstTick=s.iceLastTick=b->mMainCounter;s.iceFirstCounter=s.iceLastCounter=a->NetIsHost()?p->mDoSpecialCountdown:-1;}
  LawnNet::NetTrace("SMOKE_FUSION_COLD_PURCHASE role=%s step=%d kind=%d id=%u token=%u packet=%u owner=%d extra=%d parents=%d,%d sun=%d awake=1 HP=300 nativeSDL=1",Role(a),s.step,result,out.id,out.token,card->mNetId,who,extra?1:0,first,second,after);
 }
 return Business(a)&&Barrier(a,"FC110_BUY_"+std::to_string(s.step));
}
bool Mix(LawnApp* a,int result,int existing,int used)
{
 auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);auto* source=ByRef(b,s.aux);auto* old=ByRef(b,s.current);
 if(!s.observed)
 {
  if(!source||!old){auto* p=At(b,2,existing==1019?3:0);const auto* d=GetNativePlantData(p);if(d&&d->kind==result){s.current=Identity(p);s.observed=true;}else return false;}
  else
  {
   if(ctx->gloveCooldownTicks&&!ctx->gloveHolds[1].plantId)return false;
   if(Slot(a)==1&&!s.toolClicked){auto rect=b->GetShovelButtonRect();b->GetZenButtonRect(OBJECT_TYPE_GLOVE,rect);s.toolClicked=BoardClick(a,float(rect.mX+rect.mWidth/2),float(rect.mY+rect.mHeight/2));return false;}
   if(Slot(a)==1&&!s.clicked){if(b->mCursorObject->mCursorType!=CURSOR_TYPE_GLOVE)return false;s.clicked=Cell(a,source->mPlantCol,source->mRow);return false;}
   if(ctx->gloveHolds[1].plantId!=s.aux.id||ctx->gloveHolds[1].instanceToken!=s.aux.token||GetNativeGloveHeldPlant(b,1)!=source)return false;
   if(Slot(a)==1&&!s.placed){if(b->mCursorObject->mCursorType!=CURSOR_TYPE_PLANT_FROM_GLOVE)return false;s.placed=Cell(a,old->mPlantCol,old->mRow);return false;}
   return false;
  }
 }
 auto* p=ByRef(b,s.current);const auto* d=GetNativePlantData(p);auto* packet=Packet(b,1,0,true);
 if(!Require(a,p&&d&&d->kind==result&&d->firstParent==existing&&d->secondParent==used&&d->owner==1&&d->packetId==packet->mNetId&&!d->randomOrigin&&p->mPlantHealth==300&&!ByRef(b,s.aux)&&!ctx->gloveHolds[1].plantId,"Cold real glove fusion identity/parents/ownership differ"))return false;
 if(!Business(a)||!Barrier(a,"FC110_MIX_"+std::to_string(result)))return false;
 LawnNet::NetTrace("SMOKE_FUSION_COLD_MIX role=%s kind=%d id=%u token=%u parents=%d,%d owner=1 packet=%u sun=%d actualSDL=1 noAdditionalCharge=1",Role(a),result,p->mNetId,d->instanceToken,existing,used,packet->mNetId,s.money);return true;
}
bool GlobalSetup(LawnApp* a)
{
 if(a->NetIsHost()&&!s.globalSetup)
 {
  for(int i=0;i<4;++i){auto* z=Spawn(a,i==0?0:i==1?1:4,i==0?1000:0,i==0?1000:0,i==3, i==3?650.0f:1000.0f);if(!z)return false;s.globalIds[i]=z->mNetId;}
  s.globalSetup=true;LawnNet::NetTrace("SMOKE_FUSION_COLD_GLOBAL_TARGETS shield=%u adjacent=%u biter=%u MC=%u initialHPOnly=1 boundedNativeNormal=1 sourceZombieAIUnported=1",s.globalIds[0],s.globalIds[1],s.globalIds[2],s.globalIds[3]);
 }
 const std::string tag="FC110_GLOBAL";std::ostringstream o;o<<tag;for(auto id:s.globalIds)o<<' '<<id;
 if(a->NetIsHost()&&!Send(a,o.str()))return false;
 if(a->NetIsClient())for(const auto& l:a->mNetManager->mChatLines)if(l.mSlot==0&&l.mText.rfind(tag+" ",0)==0){std::istringstream in(l.mText.substr(tag.size()));for(auto& id:s.globalIds){unsigned v=0;if(!(in>>v)||!v||v>65535)return false;id=uint16_t(v);}}
 for(auto id:s.globalIds)if(!Target(a->mBoard,id))return false;return Barrier(a,"FC110_GLOBAL_ACK");
}
bool ObserveIce(LawnApp* a)
{
 if(!s.ice.id)return true;auto* b=a->mBoard;auto* p=ByRef(b,s.ice);
 if(p)
 {
  // The ordinary native plant snapshot contains no mDoSpecialCountdown.
  // Ghosts mirror identity/HP/state and authoritative effects/retirement;
  // only the host owns and proves the actual151tick gameplay fuse.
  const bool live=p->mPlantHealth==300&&!p->mIsAsleep&&IsNativeIceShroom(p)&&(!a->NetIsHost()||(p->mDoSpecialCountdown>0&&p->mDoSpecialCountdown<=151));
  if(!live)LawnNet::NetTrace("SMOKE_FUSION_COLD_ICE_MISMATCH role=%s id=%u HP=%d asleep=%d sourceKind10=%d state=%d countdown=%d hostFuseOnly=1",Role(a),p->mNetId,p->mPlantHealth,p->mIsAsleep?1:0,IsNativeIceShroom(p)?1:0,int(p->mState),p->mDoSpecialCountdown);
  if(!Require(a,live,"IceShroom lost awake/invulnerable/live fuse contract"))return false;
  if(a->NetIsHost())
  {
   auto* z=Target(b,s.globalIds[2]);if(!s.biterPositioned&&z){for(int dx=80;dx>=-80;--dx){SetX(z,float(p->mX+dx));if(z->FindPlantTarget(ATTACKTYPE_CHEW)==p){s.biterPositioned=true;break;}}if(!Require(a,s.biterPositioned,"IceShroom actual biter targeting unavailable"))return false;}
   if(z&&z->mIsEating&&z->FindPlantTarget(ATTACKTYPE_CHEW)==p)++s.iceEatingSamples;
   const int dt=b->mMainCounter-s.iceLastTick;if(!Require(a,p->mDoSpecialCountdown==s.iceLastCounter-dt,"IceShroom fuse skipped native elapsed ticks"))return false;s.iceLastCounter=p->mDoSpecialCountdown;s.iceLastTick=b->mMainCounter;
  }
  return true;
 }
 if(!s.iceRetireTick){s.iceRetireTick=b->mMainCounter;LawnNet::NetTrace("SMOKE_FUSION_COLD_FUSE role=%s id=%u token=%u firstTick=%d firstRemaining=%d retireTick=%d elapsed=%d sourceFuse=151 biteSamples=%d invulnerableHP300=1",Role(a),s.ice.id,s.ice.token,s.iceFirstTick,s.iceFirstCounter,s.iceRetireTick,s.iceRetireTick-s.iceFirstTick,s.iceEatingSamples);}
 bool hit=true;for(int i=0;i<4;++i)
 {
  auto* z=Target(b,s.globalIds[i]);if(!z)return true;const auto* d=GetNativeZombieColdData(z);
  if(i==3){if(!Require(a,z->mBodyHealth==5000&&!z->mChilledCounter&&!z->mIceTrapCounter&&!d,"IceShroom applied damage/cold to mind-controlled control"))return false;continue;}
  hit &= d&&d->level==0&&d->threshold==100&&z->mIceTrapCounter>0&&z->mChilledCounter>1000&&z->mChilledCounter-z->mIceTrapCounter==1000;
  hit &= i==0?(z->mBodyHealth==5000&&z->mHelmHealth==980&&z->mShieldHealth==980):z->mBodyHealth==4980;
 }
 if(hit&&!s.globalHit)
 {
  if(a->NetIsHost()&&!Require(a,s.iceEatingSamples>0&&b->mMainCounter-s.iceFirstTick>=s.iceFirstCounter,"IceShroom natural151/bite proof missing"))return false;
  s.globalHit=true;LawnNet::NetTrace("SMOKE_FUSION_COLD_GLOBAL role=%s plant=%u normalTargets=3 normalAll20=1 globalAcrossRowsAndOffscreen=1 freeze400=1 slow1400=1 MCunchanged=1 boardAura=%d",Role(a),s.ice.id,b->mIceTrapCounter);
 }
 return Require(a,b->mIceTrapCounter==0,"IceShroom leaked vanilla future-spawn board aura");
}
std::vector<double> Events(int kind)
{if(kind==1034)return {double(.6666667f)};if(kind==1105)return {double(.53333336f),double(1.0666667f)};if(kind==1106)return {double(.33333334f),double(.5f),double(.6666667f),double(.8333333f)};if(kind==1035)return {double(.38333333f)};return {double(.5f),double(.7f),double(.9f),double(1.1f)};}
bool NoNativeBites(LawnApp* a)
{
 auto& q=s.combat;
 for(auto id:q.targets)
 {
  auto* z=Target(a->mBoard,id);auto* prey=z?z->FindZombieTarget():nullptr;
  if(prey){const auto attack=z->GetZombieAttackRect(),body=prey->GetZombieRect();LawnNet::NetTrace("SMOKE_FUSION_COLD_SCENE_BITE kind=%d actor=%u MC=%d prey=%u attack=%d,%d,%d,%d body=%d,%d,%d,%d",q.kind,id,z->mMindControlled?1:0,prey->mNetId,attack.mX,attack.mY,attack.mWidth,attack.mHeight,body.mX,body.mY,body.mWidth,body.mHeight);}
  if(!Require(a,z&&!prey,"Cold controlled scene permits native zombie-versus-zombie biting"))return false;
 }
 return true;
}
bool SetupCombat(LawnApp* a,int kind)
{
 auto* b=a->mBoard;auto* p=ByRef(b,s.current);const auto* d=GetNativePlantData(p);if(!Require(a,p&&d&&d->kind==kind,"Cold combat source missing"))return false;
 if(s.combat.kind!=kind){s.combat={};s.combat.kind=kind;s.combat.row=p->mRow;s.combat.desiredVolleys=kind==1106?4:2;}
 auto& q=s.combat;
 if(a->NetIsHost()&&!q.setup)
 {
  for(int i=0;i<7;++i)
  {
   Zombie* z=kind==1034&&i==0?Target(b,s.globalIds[0]):Spawn(a,i==5?(q.row+1)%5:q.row,(kind==1035||kind==908)&&i==1?1000:0,(kind==1035||kind==908)&&i<=1?1000:0,i==4,i==4?450.0f:1000.0f);
   if(!z)return false;q.targets[i]=z->mNetId;
   if(!Ground(a,z,i==4?450.0f:i==6?100.0f:1000.0f))return false;
   LawnNet::NetTrace("SMOKE_FUSION_COLD_TARGET kind=%d index=%d id=%u row=%d body=%d helm=%d shield=%d MC=%d initialHPOnly=1 boundedNativeNormal=1",kind,i,z->mNetId,z->mRow,z->mBodyHealth,z->mHelmHealth,z->mShieldHealth,z->mMindControlled?1:0);
  }if(!NoNativeBites(a))return false;q.setup=true;
 }
 if(a->NetIsHost()&&!Send(a,Ids("FC110_IDS",kind,q.targets)))return false;if(a->NetIsClient()&&!ReadIds(a,"FC110_IDS",kind,q.targets))return false;
 for(auto id:q.targets)if(!Target(b,id))return false;
 if(!Capture(a,("fusion_cold_model"+std::to_string(kind)).c_str())||!Barrier(a,"FC110_TARGETS_"+std::to_string(kind)))return false;
 if(!q.started)
 {
  for(int i=0;i<7;++i)q.before[i]=Read(Target(b,q.targets[i]),b->mMainCounter);
  q.previousCounter=p->mShootingCounter;q.previousTick=b->mMainCounter;
  if(a->NetIsHost())
  {
   const int active=kind==1035||kind==908?3:1;
   for(int i=0;i<active;++i)if(!Ground(a,Target(b,q.targets[i]),570.0f+20*i))return false;
   if(!Ground(a,Target(b,q.targets[3]),730.0f)||!Ground(a,Target(b,q.targets[5]),570.0f))return false;
   if(!NoNativeBites(a))return false;
   // Target3 is the fourth collinear actor for IceSpark. Snow target3 is beyond target0.
   LawnNet::NetTrace("SMOKE_FUSION_COLD_ACTIVE kind=%d row=%d volleys=%d activeTargets=%d targetPositionsOnly=1 noHPOrTimerWrites=1",kind,q.row,q.desiredVolleys,active);
  }
  q.started=true;
 }return true;
}

bool ObserveTargets(LawnApp* a)
{
 auto* b=a->mBoard;auto& q=s.combat;const int payload=q.kind==1035||q.kind==908?18:15,increment=payload==18?5:10;
 for(int i=0;i<7;++i)
 {
  auto* z=Target(b,q.targets[i]);if(!Require(a,z&&!z->IsDeadOrDying(),"Cold controlled normal actor unexpectedly died"))return false;
  auto now=Read(z,b->mMainCounter);auto& old=q.before[i];const int dt=now.tick-old.tick;
  const int bodyLoss=old.body-now.body,helmLoss=old.helm-now.helm,shieldLoss=old.shield-now.shield;
  if(i>=3)
  {
   if(bodyLoss||helmLoss||shieldLoss){auto* prey=z->FindZombieTarget();auto* mc=Target(b,q.targets[4]);auto* mcPrey=mc?mc->FindZombieTarget():nullptr;LawnNet::NetTrace("SMOKE_FUSION_COLD_CONTROL_DAMAGE role=%s kind=%d index=%d id=%u loss=%d,%d,%d HP=%d,%d,%d before=%d,%d,%d dt=%d pos=%.3f,%.3f MC=%d eating=%d nativePrey=%u mcPrey=%u",Role(a),q.kind,i,z->mNetId,bodyLoss,helmLoss,shieldLoss,now.body,now.helm,now.shield,old.body,old.helm,old.shield,dt,z->mPosX,z->mPosY,z->mMindControlled?1:0,z->mIsEating?1:0,prey?prey->mNetId:0,mcPrey?mcPrey->mNetId:0);}
   if(!Require(a,!bodyLoss&&!helmLoss&&!shieldLoss,"Cold fourth/MC/adjacent/rear control took damage"))return false;
  }
  else if(bodyLoss||helmLoss||shieldLoss)
  {
   if(a->NetIsHost())
   {
    const int damage=payload==18?bodyLoss+helmLoss:bodyLoss+helmLoss+shieldLoss;
    if(!Require(a,(damage==20||damage==80)&&dt>=0&&dt<=10&&bodyLoss>=0&&helmLoss>=0&&shieldLoss>=0,"Cold hit was duplicated/wrong direct damage"))return false;
    const bool previouslyFrozen=old.freeze>dt;int level=old.freeze>0&&old.freeze<=dt?0:old.level;
    level+=increment;const bool cross=level>=old.threshold&&!previouslyFrozen;const int threshold=cross?(std::min)(400,old.threshold+100):old.threshold;
    if(cross)level=0;
    if(!Require(a,damage==(previouslyFrozen||cross?80:20)&&now.level==level&&now.threshold==threshold,"Cold accumulation/threshold-before-damage/frozen multiplier differs"))return false;
    const int expectedShield=(std::min)(old.shield,damage),remaining=payload==18?damage:damage-expectedShield;
    if(!Require(a,shieldLoss==expectedShield&&helmLoss==(std::min)(old.helm,remaining)&&bodyLoss==remaining-(std::min)(old.helm,remaining),"Cold source Ice versus IceAll shield/helmet/body routing differs"))return false;
    const bool cold=payload==18||old.shield<damage;const int expectedSlow=cold?1000:cross?1400:(std::max)(0,old.slow-dt);
    const int expectedFreeze=cross?400:(std::max)(0,old.freeze-dt);
    const int tolerance=(cold||cross)?(std::max)(0,dt-1):0;
    if(!Require(a,now.slow<=expectedSlow&&now.slow>=expectedSlow-tolerance&&now.freeze<=expectedFreeze&&now.freeze>=expectedFreeze-(cross?tolerance:0),"Cold overwrite/blocked-shield preservation/native remaining counters differ"))return false;
    ++old.hits;++q.hits;if(cross){q.crossed=true;LawnNet::NetTrace("SMOKE_FUSION_COLD_THRESHOLD kind=%d target=%u level=%d threshold=%d slow=%d freeze=%d damage=%d beforeDamageFreeze=1",q.kind,z->mNetId,now.level,now.threshold,now.slow,now.freeze,damage);}
    LawnNet::NetTrace("SMOKE_FUSION_COLD_HIT kind=%d target=%u payload=%d damage=%d shieldPassed=%d delta=%d,%d,%d level=%d threshold=%d slow=%d freeze=%d observedGap=%d tick=%d nativeDamage=1",q.kind,z->mNetId,payload,damage,cold?1:0,bodyLoss,helmLoss,shieldLoss,now.level,now.threshold,now.slow,now.freeze,dt,b->mMainCounter);
   }
  }
  else if(a->NetIsHost()&&dt>0)
  {
   const int freeze=(std::max)(0,old.freeze-dt),slow=(std::max)(0,old.slow-dt);
   if(!Require(a,now.freeze==freeze&&now.slow==slow&&now.threshold==old.threshold&&now.level==(old.freeze>0&&!freeze?0:old.level),"Cold real native counter drain/state changed without impact"))return false;
  }
  if(a->NetIsHost()&&old.freeze>0&&!now.freeze)
  {
   if(!Require(a,now.level==0&&now.threshold==old.threshold,"Cold natural thaw reset persistent threshold or retained accumulated level"))return false;
   if(q.kind==1106&&now.threshold==200){q.thawed=true;LawnNet::NetTrace("SMOKE_FUSION_COLD_THAW role=host kind=1106 target=%u level=0 threshold=200 slow=%d naturalExpiry=1",z->mNetId,now.slow);}
  }
  if(a->NetIsClient()&&(now.body!=old.body||now.helm!=old.helm||now.shield!=old.shield||now.level!=old.level||now.threshold!=old.threshold))
   LawnNet::NetTrace("SMOKE_FUSION_COLD_STATE role=client kind=%d target=%u body=%d helm=%d shield=%d level=%d threshold=%d mirrorSlow=%d mirrorFreeze=%d cosmeticSlow=%d cosmeticFreeze=%d",q.kind,z->mNetId,now.body,now.helm,now.shield,now.level,now.threshold,GetNativeZombieColdData(z)?GetNativeZombieColdData(z)->chilledTicks:0,GetNativeZombieColdData(z)?GetNativeZombieColdData(z)->frozenTicks:0,now.slow,now.freeze);
  const int hits=old.hits;old=now;old.hits=hits;
 }return true;
}
bool ClientExactNegative(LawnApp* a)
{
 if(a->NetIsHost()||s.negative||!a->mBoard->mPaused)return true;auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);
 for(const auto& pending:a->mNetManager->mPendingBoardMsgs)
 {
  if(pending.mKind!=1)continue;auto wire=pending.mData;NativeSnapshotFrame frame;LawnNet::NetBuffer body;
  if(!DecodeNativeSnapshot(wire,ctx->eligible,ctx->epoch,frame,body)||body.mData.empty()||!(body.mData[0]&LawnNet::SNAPFLAG_PAUSED))continue;
  const auto entry=std::find_if(frame.coldZombies.begin(),frame.coldZombies.end(),[](const auto& r){return r.id==s.combat.targets[0]&&r.data.frozenTicks>0;});if(entry==frame.coldZombies.end())continue;
  // Only change the metadata counter. Encode recomputes/retains a valid native-body CRC.
  auto badFrame=frame;auto badEntry=std::find_if(badFrame.coldZombies.begin(),badFrame.coldZombies.end(),[&](const auto& r){return r.id==entry->id;});badEntry->data.frozenTicks=0;
  auto bad=body;if(!Require(a,EncodeNativeSnapshot(badFrame,ctx->eligible,bad),"Cold CRC-valid negative encode failed"))return false;
  LawnNet::NetBuffer before,after;b->NetEncodeSnapshot(before);bool applied=true;b->NetApplySnapshot(bad,&applied);b->NetEncodeSnapshot(after);
  if(!Require(a,!applied&&before.mData==after.mData&&b->mPaused,"CRC-valid cold/native timer mismatch mutated the native Board"))return false;
  auto healthy=pending.mData;applied=false;b->NetApplySnapshot(healthy,&applied);
  if(!Require(a,applied&&b->mPaused,"Actual paused host snapshot healthy replay failed"))return false;
  for(const auto& record:frame.coldZombies)
  {
   auto* z=Target(b,record.id);const auto* d=GetNativeZombieColdData(z);
   if(!Require(a,z&&d&&d->level==record.data.level&&d->threshold==record.data.threshold&&d->chilledTicks==record.data.chilledTicks&&d->frozenTicks==record.data.frozenTicks&&z->mChilledCounter==record.data.chilledTicks&&z->mIceTrapCounter==record.data.frozenTicks,"FNS7 failed exact native timer/level restoration"))return false;
   LawnNet::NetTrace("SMOKE_FUSION_COLD_EXACT role=client target=%u level=%d threshold=%d slow=%d freeze=%d nativeCountersEqual=1 actualHostBytes=1 controlledReplay=1 lateJoinTested=0",record.id,d->level,d->threshold,d->chilledTicks,d->frozenTicks);
  }
  s.negative=s.exactReplay=true;LawnNet::NetTrace("SMOKE_FUSION_COLD_NEGATIVE role=client target=%u FNS10=1 crcValid=1 nativeCounterMismatch=1 rejected=1 wholeBoardBytesUnchanged=1 sourceOriginalUntouched=1",entry->id);return true;
 }return true;
}
bool PauseAndThaw(LawnApp* a)
{
 auto* b=a->mBoard;auto* z=Target(b,s.combat.targets[0]);if(!Require(a,z,"Cold pause control missing"))return false;
 if(a->NetIsHost()&&!s.pauseRequested&&s.combat.parked&&s.combat.hits==16&&z->mIceTrapCounter>0&&z->mIceTrapCounter<=5)
 {s.pauseRequestedAt=z->mIceTrapCounter;s.pauseRequested=true;if(!Key(a,SDLK_ESCAPE))return false;LawnNet::NetTrace("SMOKE_FUSION_COLD_PAUSE_REQUEST remaining=%d tick=%d ownedSDL=1 nativeOptions=1 counterWrites=0",s.pauseRequestedAt,b->mMainCounter);}
 if(!b->mPaused)
 {
  if(s.pauseObserved&&s.resumeQueued){s.pauseDone=true;return true;}
  if(a->NetIsHost()&&s.combat.thawed&&!s.pauseRequested)return Require(a,false,"Cold pause missed the natural pre-thaw1..5tick window");return true;
 }
 if(!ClientExactNegative(a))return false;if(a->NetIsClient()&&!s.exactReplay)return true;
 if(!s.pauseObserved){if(!Require(a,z->mIceTrapCounter>0&&z->mIceTrapCounter<=5,"Cold pause did not preserve a short positive freeze"))return false;s.pauseObserved=true;s.pauseSince=GetTickCount();s.pauseTick=b->mMainCounter;s.pauseFreeze=z->mIceTrapCounter;s.pauseSlow=z->mChilledCounter;}
 if(!Require(a,b->mMainCounter==s.pauseTick&&z->mIceTrapCounter==s.pauseFreeze&&z->mChilledCounter==s.pauseSlow,"Paused host/client ghost advanced native cold timers"))return false;++s.pauseChecks;
 if(s.pauseChecks<10||GetTickCount()-s.pauseSince<1200||!Capture(a,"fusion_cold_paused"))return true;
 // Record client readiness before sending: the host can resume before this
 // client's chat echo, exactly as the ordinary Ready/start barrier can race.
 if(a->NetIsClient()&&!s.resumeQueued){s.resumeQueued=true;LawnNet::NetTrace("SMOKE_FUSION_COLD_PAUSED role=client tick=%d slow=%d freeze=%d uiChecks=%d elapsedMs=%u exactStable=1 decoderLive=1",s.pauseTick,s.pauseSlow,s.pauseFreeze,s.pauseChecks,GetTickCount()-s.pauseSince);}
 if(!Barrier(a,"FC110_PAUSED"))return true;
 if(a->NetIsHost()&&!s.resumeQueued){LawnNet::NetTrace("SMOKE_FUSION_COLD_PAUSED role=host tick=%d slow=%d freeze=%d uiChecks=%d elapsedMs=%u exactStable=1 decoderLive=1",s.pauseTick,s.pauseSlow,s.pauseFreeze,s.pauseChecks,GetTickCount()-s.pauseSince);if(!Key(a,SDLK_ESCAPE))return false;s.resumeQueued=true;}
 return true;
}
bool ObserveCombat(LawnApp* a)
{
 auto& q=s.combat;if(!q.started)return true;auto* b=a->mBoard;auto* plant=ByRef(b,s.current);const auto* d=GetNativePlantData(plant);const int payload=q.kind==1035||q.kind==908?18:15;
 if(!Require(a,plant&&d&&d->kind==q.kind&&plant->mPlantHealth==300&&!plant->mIsAsleep,"Cold source actor stopped actual native shooting"))return false;
 if(a->NetIsHost()&&plant->mShootingCounter>q.previousCounter)
 {
  const auto events=Events(q.kind);const double state=q.kind==1034||q.kind==1105?2.0:q.kind==908?double(1.3f):1.0;
  Volley v;for(double e:events)v.deadlines.push_back(int(std::ceil(e/(state*double(d->animationSpeed))*100)));const int initial=v.deadlines.back()+1;
  if(!Require(a,q.volleys.size()<size_t(q.desiredVolleys)&&plant->mShootingCounter<=initial&&initial-plant->mShootingCounter<=(std::max)(1,b->mMainCounter-q.previousTick),"Cold natural multi-event native counter differs"))return false;
  v.tick=b->mMainCounter-(initial-plant->mShootingCounter);v.ids.resize(events.size());q.volleys.push_back(v);
  std::ostringstream deadlines;for(size_t i=0;i<v.deadlines.size();++i){if(i)deadlines<<',';deadlines<<v.deadlines[i];}
  LawnNet::NetTrace("SMOKE_FUSION_COLD_TRIGGER kind=%d volley=%zu tick=%d initial=%d deadlines=%s speed=%.9g stateSpeed=%.17g eventCount=%zu visualState=%s naturalClock=1",q.kind,q.volleys.size(),v.tick,initial,deadlines.str().c_str(),d->animationSpeed,state,events.size(),q.kind==908?"shoot_g":"shoot");
 }
 q.previousCounter=plant->mShootingCounter;q.previousTick=b->mMainCounter;
 std::set<uint16_t> live;Projectile* shot=nullptr;while(b->mProjectiles.IterateNext(shot))
 {
  if(shot->mDead||shot->mRow!=q.row)continue;const auto* m=GetNativeProjectileData(shot);if(!m||m->kind!=payload)continue;live.insert(shot->mNetId);
  if(!Require(a,m->damage==20&&!m->sunOnHit&&!m->specialType&&shot->mMotionType==MOTION_STRAIGHT,"Cold projectile payload/ordinary-motion/provenance differs"))return false;
  Sexy::Rect rect;if(!Require(a,GetNativeTimedPeaProjectileRect(shot,rect)&&rect.mWidth>0&&rect.mHeight>0,"Cold source projected collider missing"))return false;
  if(a->NetIsHost()&&!q.mcTraversed)
  {
   auto* mc=Target(b,q.targets[4]);const auto body=mc->GetZombieRect();
   if(rect.mX<=body.mX+body.mWidth&&rect.mX+rect.mWidth>=body.mX&&rect.mY<=body.mY+body.mHeight&&rect.mY+rect.mHeight>=body.mY)
   {
    if(!Require(a,!NativeTimedPeaProjectileCanHit(shot,mc)&&mc->mBodyHealth==5000&&!mc->mChilledCounter&&!mc->mIceTrapCounter&&!GetNativeZombieColdData(mc),"Cold actual overlapping mind-controlled actor was eligible/damaged/chilled"))return false;
    q.mcTraversed=true;LawnNet::NetTrace("SMOKE_FUSION_COLD_MC_PASS kind=%d projectile=%u target=%u actualSourceRectOverlap=1 canHit=0 HP=5000 coldStateAbsent=1",q.kind,shot->mNetId,mc->mNetId);
   }
  }
  if(a->NetIsHost())
  {
   if(!Require(a,std::abs(shot->mVelX-3.2f)<.0001f,"Cold straight native speed differs"))return false;
   auto found=q.shots.find(shot->mNetId);
   if(found==q.shots.end())
   {
    const int fire=b->mMainCounter-shot->mProjectileAge+1;int vi=-1,ei=-1;
    for(size_t v=0;v<q.volleys.size();++v)for(size_t e=0;e<q.volleys[v].deadlines.size();++e)if(q.volleys[v].tick+q.volleys[v].deadlines[e]==fire){vi=int(v);ei=int(e);}
    if(!Require(a,vi>=0&&ei>=0&&!q.volleys[vi].ids[ei],"Cold duplicate/early/late actual AnimationEvent emission"))return false;q.volleys[vi].ids[ei]=shot->mNetId;
    q.shots.emplace(shot->mNetId,Shot{b->mMainCounter,shot->mProjectileAge,vi,ei,shot->mPosX,false});
    LawnNet::NetTrace("SMOKE_FUSION_COLD_SHOT kind=%d id=%u volley=%d event=%d fire=%d trigger=%d deadline=%d payload=%d damage=20 special=0 motion=0 age=%d vx=3.2 sourceBox=%d,%d sun=0",q.kind,shot->mNetId,vi+1,ei+1,fire,q.volleys[vi].tick,q.volleys[vi].deadlines[ei],payload,shot->mProjectileAge,rect.mWidth,rect.mHeight);
   }
   else {const int dt=b->mMainCounter-found->second.tick;if(!Require(a,std::abs(shot->mPosX-found->second.x-3.2f*dt)<.06f,"Cold actual projectile displacement differs"))return false;found->second.tick=b->mMainCounter;found->second.age=shot->mProjectileAge;found->second.x=shot->mPosX;}
  }
  else if(q.clientSeen.insert(shot->mNetId).second)LawnNet::NetTrace("SMOKE_FUSION_COLD_SEEN role=client kind=%d id=%u payload=%d damage=20 special=0 motion=0 rect=%d,%d hostAgeVelocityNotSerialized=1",q.kind,shot->mNetId,payload,rect.mWidth,rect.mHeight);
 }
 const int expected=int(Events(q.kind).size())*q.desiredVolleys;
 if(a->NetIsHost()&&int(q.shots.size())==expected&&!q.parked)
 {
  for(int i=0;i<4;++i)if(!Ground(a,Target(b,q.targets[i]),850.0f+15*i))return false;q.parked=true;
  LawnNet::NetTrace("SMOKE_FUSION_COLD_PARK kind=%d shots=%d outsideAcquisitionInsideProjectileBounds=1 positionsOnly=1 noHPOrTimerWrites=1",q.kind,expected);
 }
 if(a->NetIsClient()&&int(q.clientSeen.size())==expected)
 {
  // Every physical ID was seen before any synchronous BMP. No future trigger remains after host parking.
  if(!Capture(a,("fusion_cold_projectile"+std::to_string(q.kind)).c_str()))return true;Send(a,"FC110_SEEN_"+std::to_string(q.kind));
 }
 if(a->NetIsHost())for(auto& [id,sample]:q.shots)if(!sample.retired&&!live.contains(id)){sample.retired=true;LawnNet::NetTrace("SMOKE_FUSION_COLD_RETIRE kind=%d id=%u lastAge=%d lastAliveTick=%d txAtRetire=%u",q.kind,id,sample.age,sample.tick,Samples(a));}
 if(!ObserveTargets(a))return false;if(q.kind==1106&&!PauseAndThaw(a))return false;
 return true;
}
bool CombatDone(LawnApp* a,int kind)
{
 auto& q=s.combat;if(!q.started){if(!SetupCombat(a,kind))return false;return false;}
 if(!ObserveCombat(a)||!Business(a))return false;const int expected=int(Events(kind).size())*q.desiredVolleys;const int hits=expected*(kind==1035||kind==908?3:1);
 if(a->NetIsHost())
 {
  if(q.hits!=hits||int(q.shots.size())!=expected||std::any_of(q.shots.begin(),q.shots.end(),[](const auto& x){return !x.second.retired;}))return false;
  if(!Require(a,q.mcTraversed,"Cold projectile never traversed the protected MC control"))return false;
  if(kind==1106&&(!q.crossed||!q.thawed||!s.pauseDone))return false;
  if(!Send(a,"FC110_HITS_"+std::to_string(kind)))return false;
 }
 if(!Has(a,0,"FC110_HITS_"+std::to_string(kind))||!Has(a,1,"FC110_SEEN_"+std::to_string(kind)))return false;
 if(!Capture(a,("fusion_cold_combat"+std::to_string(kind)).c_str())||!Barrier(a,"FC110_COMBAT_"+std::to_string(kind)))return false;
 for(int i=0;i<7;++i){auto* z=Target(a->mBoard,q.targets[i]);const auto* d=GetNativeZombieColdData(z);LawnNet::NetTrace("SMOKE_FUSION_COLD_FINAL role=%s kind=%d index=%d id=%u body=%d helm=%d shield=%d level=%d threshold=%d",Role(a),kind,i,z->mNetId,z->mBodyHealth,z->mHelmHealth,z->mShieldHealth,d?d->level:0,d?d->threshold:100);}
 LawnNet::NetTrace("SMOKE_FUSION_COLD_COMBAT role=%s kind=%d volleys=%d physical=%d hits=%d payload=%d uniqueLimit=%d noFourth=1 controlsUnchanged=1 noSun=1 naturalAcquisition=1",Role(a),kind,q.desiredVolleys,expected,hits,kind==1035||kind==908?18:15,kind==1035||kind==908?3:1);
 if(a->NetIsHost())for(int i=0;i<7;++i)if(i!=4&&!Ground(a,Target(a->mBoard,q.targets[i]),1000.0f+15*i))return false;
 q.started=false;return true;
}

bool Recipes(LawnApp* a)
{
 constexpr std::array<std::array<uint16_t,3>,9> expected={{{0,10,1034},{1034,0,1105},{0,0,1030},{1105,1030,1106},{6,0,1018},{1018,10,1035},{1018,0,1019},{1019,1030,1065},{1065,10,908}}};
 for(const auto& row:expected){const auto* r=FindFusionRecipe(row[0],row[1]);if(!Require(a,r&&r->result==row[2],"Cold directed recipe golden differs from authoritative catalog"))return false;}return true;
}
bool RunStep(LawnApp* a)
{
 auto* b=a->mBoard;auto* ctx=GetNativeContext(b);
 switch(s.step)
 {
 case 0:if(GlobalSetup(a))Next(a);break;
 case 1:if(Purchase(a,1,10,true,5,4,10,2000,1850,kFusionNoPlant,kFusionNoPlant,s.ice))Next(a);break;
 case 2:
 {
  if(!s.globalHit||ByRef(b,s.ice)||!Business(a))break;
  if(a->NetIsHost()&&!s.noAura){auto* z=Spawn(a,3,0,0,false,1000.0f);if(!z)break;s.noAura=z->mNetId;}
  if(a->NetIsHost()&&!Send(a,"FC110_NOAURA "+std::to_string(s.noAura)))break;
  if(a->NetIsClient())for(const auto& l:a->mNetManager->mChatLines)if(l.mSlot==0&&l.mText.rfind("FC110_NOAURA ",0)==0){unsigned id=0;std::istringstream in(l.mText.substr(13));if(in>>id)s.noAura=uint16_t(id);}
  auto* z=Target(b,s.noAura);if(!z)break;
  const bool clean=z->mBodyHealth==5000&&!z->mChilledCounter&&!z->mIceTrapCounter&&!GetNativeZombieColdData(z);
  if(!clean)LawnNet::NetTrace("SMOKE_FUSION_COLD_NOAURA_MISMATCH role=%s id=%u body=%d slow=%d freeze=%d metadata=%d visible=%d ghost=%d applied=%u",Role(a),z->mNetId,z->mBodyHealth,z->mChilledCounter,z->mIceTrapCounter,GetNativeZombieColdData(z)?1:0,z->mVisible?1:0,z->mNetGhost?1:0,Samples(a));
  if(!Require(a,clean,"IceShroom incorrectly froze a future-spawn actor"))break;
  if(!Capture(a,"fusion_cold_global")||!Barrier(a,"FC110_GLOBAL_DONE"))break;
  if(a->NetIsHost()){if(!Ground(a,Target(b,s.globalIds[2]),900.0f))break;}
  LawnNet::NetTrace("SMOKE_FUSION_COLD_NOAURA role=%s newActor=%u nativeCounters=0 sourceStateAbsent=1 boardAura=0 postEffectSpawn=1",Role(a),s.noAura);Next(a);break;
 }
 case 3:if(Purchase(a,0,0,false,2,0,0,1850,1750,kFusionNoPlant,kFusionNoPlant,s.current))Next(a);break;
 case 4:if(Purchase(a,0,10,false,2,0,1034,1750,1675,0,10,s.current))Next(a);break;
 case 5:if(CombatDone(a,1034))Next(a);break;
 case 6:if(Purchase(a,0,0,false,2,0,1105,1675,1575,1034,0,s.current))Next(a);break;
 case 7:if(CombatDone(a,1105))Next(a);break;
 case 8:if(Purchase(a,0,0,false,1,4,0,1575,1475,kFusionNoPlant,kFusionNoPlant,s.aux))Next(a);break;
 case 9:if(Purchase(a,1,0,true,1,4,1030,1475,1275,0,0,s.aux))Next(a);break;
 case 10:if(Mix(a,1106,1105,1030))Next(a);break;
 case 11:if(CombatDone(a,1106))Next(a);break;
 case 12:if(Purchase(a,0,6,false,2,2,6,1275,1275,kFusionNoPlant,kFusionNoPlant,s.current))Next(a);break;
 case 13:if(Purchase(a,0,0,false,2,2,1018,1275,1175,6,0,s.current))Next(a);break;
 case 14:if(Purchase(a,1,10,true,2,2,1035,1175,1025,1018,10,s.current))Next(a);break;
 case 15:if(CombatDone(a,1035))Next(a);break;
 case 16:if(Purchase(a,0,6,false,2,3,6,1025,1025,kFusionNoPlant,kFusionNoPlant,s.current))Next(a);break;
 case 17:if(Purchase(a,0,0,false,2,3,1018,1025,925,6,0,s.current))Next(a);break;
 case 18:if(Purchase(a,0,0,false,2,3,1019,925,825,1018,0,s.current))Next(a);break;
 case 19:if(Purchase(a,0,0,false,1,4,0,825,725,kFusionNoPlant,kFusionNoPlant,s.aux))Next(a);break;
 case 20:if(Purchase(a,1,0,true,1,4,1030,725,525,0,0,s.aux))Next(a);break;
 case 21:if(Mix(a,1065,1019,1030))Next(a);break;
 case 22:if(Purchase(a,0,10,false,2,3,908,525,450,1065,10,s.current))Next(a);break;
 case 23:if(CombatDone(a,908))Next(a);break;
 case 24:
 {
  constexpr std::array<int,5> expected={7,2,2,2,2};
  if(!Require(a,s.money==kFinalWallet&&s.uses==expected&&s.globalHit&&s.pauseDone&&(a->NetIsHost()||s.negative&&s.exactReplay),"Cold complete purchase/status/pause/transaction proof missing"))break;
  if(!Business(a)||!Capture(a,"fusion_cold_complete")||!Barrier(a,"FC110_DONE"))break;
  LawnNet::NetTrace("SMOKE_FUSION_COLD_COMPLETE role=%s sourceKinds=6 hybridShooters=5 initialSun=2000 totalCost=1550 finalSun=450 uses=7,2,2,2,2 sunWritesAfterSetup=0 plantHPWrites=0 plantTimerWrites=0 statusWrites=0 thresholdWrites=0 nativeBoard=1 secondWorld=0 sourceZombieAIUnported=1",Role(a));Advance(a,6);a->CoopSmokeWriteResult(true,nullptr);break;
 }
 }return true;
}
}
namespace LawnNet
{
bool NetSmokeUpdateFusionCold(LawnApp* a)
{
 const char* native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE"),*enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_COLD");
 if(!a||!native||std::strcmp(native,"1")||!enabled||std::strcmp(enabled,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
 if(a->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();if(GetTickCount()-s.since>(s.stage<=3?60000U:75000U)){char why[120];sprintf_s(why,"Cold deadline stage=%d step=%d role=%s",s.stage,s.step,Role(a));a->CoopSmokeFail(why);return true;}
 if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
 if(s.stage==0)
 {
  if(a->NetIsHost()&&!s.configured){if(!Require(a,!a->mBoard&&!a->mSeedChooserScreen,"Cold requires fresh lobby"))return true;a->mNetManager->SetDifficulty(1);s.configured=true;}
  if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
  if(a->NetIsClient()){Advance(a,1);Send(a,"FC110_START");}else if(Has(a,1,"FC110_START")){Advance(a,1);std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;
 }
 if(!a->mBoard||!IsNativeFusionBoard(a->mBoard)){if(s.stage==1&&a->NetIsClient())Send(a,"FC110_START");return true;}auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);auto* chooser=a->mSeedChooserScreen;const int slot=Slot(a);
 if(!Require(a,ctx&&ctx->validation&&!a->mFusionRuntime&&b->GetColumnCount()==10&&b->mBackground==BACKGROUND_1_DAY,"Cold lost native10x5/day Board"))return true;const auto& bank=ctx->selection.State();
 // SetColdCounters/RemoveIceTrap legitimately restore original animation rate.
 // Reapply the fixture's stationary pose, retaining positive mOriginalAnimRate.
 // This controls native ground-track motion, never HP, statuses or timers.
 if(a->NetIsHost())for(auto id:s.stationaryTargets)if(auto* z=Target(b,id))z->ApplyAnimRate(0);
 if(!ObserveIce(a))return true;
 if(s.stage==1)
 {
  if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||!b->mCutScene->mSeedChoosing||chooser->mSeedsInFlight)return true;const size_t target=(std::min)(kNativeFusionBankCapacity,ctx->eligible.size());
  if(!Require(a,ctx->eligible.size()==20&&bank.targetCount==target&&bank.activeMask==3&&bank.quotas[0]+bank.quotas[1]==target,"Cold20 live base cards/target10/quotas differ"))return true;if(!Capture(a,"fusion_cold_chooser"))return true;
  std::vector<int> choices;for(size_t i=0;i<ctx->eligible.size();++i)if(ctx->eligible[i].isExtra==(slot==1))choices.push_back(int(i));
  auto priority=[&](int i){const auto k=ctx->eligible[i].kind;return slot==0?(k==0?0:k==6?1:k==10?2:10+k):(k==0?0:k==10?1:10+k);};std::stable_sort(choices.begin(),choices.end(),[&](int x,int y){return priority(x)<priority(y);});
  int own=0;for(const auto& p:bank.packets)own+=p.owner==slot;if(own<s.waitingOwn)return true;if(!Require(a,choices.size()>=bank.quotas[slot],"Cold disjoint chooser candidates absent"))return true;
  if(own<bank.quotas[slot]){int x=0,y=0;chooser->GetSeedPositionInChooser(choices[own],x,y);if(Click(a,float(chooser->mX+x+25),float(chooser->mY+y+35-int(chooser->mScrollbar->mScrollValue))))s.waitingOwn=own+1;return true;}
  if(bank.packets.size()!=target||!Packet(b,0,0,false)||!Packet(b,0,6,false)||!Packet(b,0,10,false)||!Packet(b,1,0,true)||!Packet(b,1,10,true))return true;int rank=-1;
  for(int i=0;i<b->mSeedBank->mNumPackets;++i){const int r=NetSeedBankOwnerRank(slot,b->mSeedBank->mSeedPackets[i].mNetOwnerSlot);if(!Require(a,r>=rank,"Cold bank lost own-first view"))return true;rank=r;}
  if(!Capture(a,"fusion_cold_picked"))return true;NetTrace("SMOKE_FUSION_COLD_POOL role=%s eligible=%zu target=%u quota0=%u quota1=%u ownFirst=1 nativeChooser=1 unfiltered=1",Role(a),ctx->eligible.size(),bank.targetCount,bank.quotas[0],bank.quotas[1]);Advance(a,2);
 }
 else if(s.stage==2){if(bank.phase!=FusionBankPhase::Choosing)Advance(a,3);else if(chooser&&!chooser->mSeedsInFlight&&Ready(a,chooser->mStartButton))Advance(a,3);}
 else if(s.stage==3)
 {
  if(bank.phase==FusionBankPhase::Intro){if(!s.introStart)s.introStart=GetTickCount();s.introLast=GetTickCount();++s.introChecks;Capture(a,"fusion_cold_intro");return true;}
  if(bank.phase!=FusionBankPhase::Playing||a->mGameScene!=SCENE_PLAYING||chooser)return true;
  if(!Require(a,s.introChecks>=10&&s.introLast-s.introStart>=1000&&s.frames.contains("fusion_cold_intro")&&b->mSunMoney==500,"Cold bypassed native full intro/fresh500"))return true;s.playingTick=b->mMainCounter;Advance(a,4);
 }
 else if(s.stage==4)
 {
  if(b->mMainCounter-s.playingTick<2||!Capture(a,"fusion_cold_playing")||!Barrier(a,"FC110_PLAYING"))return true;
  if(!s.budget)
  {
   if(a->NetIsHost()&&!s.budgetWritten){if(!Require(a,!b->mPlants.mSize&&!b->mZombies.mSize&&b->mSunMoney==500&&Recipes(a),"Cold one-time wallet setup was not initial"))return true;b->mSunMoney=kInitialWallet;b->mSunCountDown=b->mZombieCountDown=b->mZombieCountDownStart=100000;s.budgetWritten=true;}
   if(b->mSunMoney!=kInitialWallet)return true;s.budget=true;NetTrace("SMOKE_FUSION_COLD_BUDGET role=%s campaignInitialSun=500 fixtureInitialSun=2000 purchaseTotal=1550 finalExpected=450 setupAmbientTimersOnly=1 sunWritesAfterSetup=0",Role(a));
  }
  if(Barrier(a,"FC110_BUDGET"))Advance(a,5);
 }
 else if(s.stage==5)RunStep(a);
 return true;
}
}
