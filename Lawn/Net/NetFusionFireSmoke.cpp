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
#include "../Fusion/FusionNativeFire.h"
#include "../Fusion/FusionFireRules.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/TodCommon.h"
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
constexpr int kRow=2,kInitialWallet=525;
struct Ref {uint16_t id=0;uint32_t token=0;};
struct Sample {int tick=0,age=0;float x=0,y=0,z=0;};
struct Flight
{
 uint16_t original=0,current=0;uint32_t generation=0;Sample sample{};
 bool born=false,hot=false,passedSecond=false,melted=false,sameTorchIgnored=false,retired=false,reported=false;
 bool clientOriginal=false,clientMelted=false,clientHot=false;
};
struct State
{
 int stage=0,step=0,caseIndex=-1,waitingOwn=0,money=kInitialWallet,peaUses=0,torchUses=0,iceUses=0,playingTick=-1;
 uint32_t since=0,lastChat=0,introStart=0,introLast=0,introChecks=0,pickSamples=0,purchaseSamples=0;
 bool configured=false,clicked=false,placed=false,observed=false,toolClicked=false,budget=false,budgetWritten=false,active=false,caseSpawned=false;
 bool sourceHeld=false,moveQueued=false,moveObserved=false,hitObserved=false;
 Ref shooter{},torch1{},torch2{};NativePlantData torchMoveData{};uint16_t aim=0;
 std::array<uint16_t,7> targets{};std::array<int,7> beforeHP{},beforeShield{},beforeFreeze{},beforeLevel{};
 Flight flight{};std::set<uint16_t> stationary,allProjectiles,pendingLogged;
 std::map<std::string,int> firstDraw;std::set<std::string> frames;
} s;
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool ok,const char* why){if(!ok)a->CoopSmokeFail(why);return ok;}
void Advance(LawnApp* a,int stage)
{
    s.stage=stage;s.since=GetTickCount();s.clicked=s.placed=s.observed=s.toolClicked=false;
    LawnNet::NetTrace("SMOKE_FUSION_FIRE_STAGE role=%s stage=%d",Role(a),stage);
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
    if(!Require(a,a->CoopSmokeCaptureFrame(name),"Fire native renderer capture failed"))return false;
    s.frames.insert(name);LawnNet::NetTrace("SMOKE_FUSION_FIRE_RENDER role=%s frame=%s freshDraws=%d",Role(a),name,a->mDrawCount-s.firstDraw[name]);return true;
}
bool Click(LawnApp* a,float x,float y,uint8_t button=SDL_BUTTON_LEFT,bool click=true)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    if(!click)return Require(a,SDL_PushEvent(&m),"Fire scoped motion failed");
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=button;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;
    return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Fire scoped SDL click failed");
}
bool BoardClick(LawnApp* a,float x,float y,uint8_t button=SDL_BUTTON_LEFT,bool click=true)
{return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY,button,click);}
bool Cell(LawnApp* a,int col,int row=kRow,uint8_t button=SDL_BUTTON_LEFT,bool click=true)
{return BoardClick(a,float(a->mBoard->GetCellCenterX(col,row)),float(a->mBoard->GridToPixelY(col,row)+50),button,click);}
bool Ready(LawnApp* a,GameButton* b)
{
    if(!b||b->mDisabled||b->mBtnNoDraw)return false;float x=b->mX+b->mWidth*.5f,y=b->mY+b->mHeight*.5f;
    if(!b->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"Fire Ready hover failed");return false;}
    LawnNet::NetTrace("SMOKE_FUSION_FIRE_READY role=%s nativeSDL=1 hoverObserved=1",Role(a));return Click(a,x,y);
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
{if(!Require(a,p&&p->mActive&&!p->mRefreshing,"Fire real packet not ready"))return false;auto* b=a->mBoard->mSeedBank;return BoardClick(a,float(b->mX+p->mX+p->mOffsetX+p->mWidth/2),float(b->mY+p->mY+p->mHeight/2));}

void Next(LawnApp* a){++s.step;s.since=GetTickCount();s.clicked=s.placed=s.observed=s.toolClicked=false;LawnNet::NetTrace("SMOKE_FUSION_FIRE_STEP role=%s step=%d",Role(a),s.step);}
Zombie* Target(Board* b,uint16_t id)
{Zombie* z=nullptr;while(b->mZombies.IterateNext(z))if(!z->mDead&&z->mNetId==id&&(!z->mNetGhost||z->mVisible))return z;return nullptr;}
Projectile* ProjectileById(Board* b,uint16_t id)
{Projectile* p=nullptr;while(b->mProjectiles.IterateNext(p))if(!p->mDead&&p->mNetId==id)return p;return nullptr;}
void SetX(Zombie* z,float x){z->mPosX=x;z->mX=int(x);}
Zombie* Spawn(LawnApp* a,int row,int shield,bool cold,float x)
{
 auto* z=a->mBoard->AddZombieInRow(ZOMBIE_NORMAL,row,0);auto* body=z?a->ReanimationTryToGet(z->mBodyReanimID):nullptr;const float rate=body?body->mAnimRate:0;
 if(!Require(a,z&&z->mNetId&&body&&rate>0,"Fire initial native actor setup failed"))return nullptr;
 z->mBodyHealth=z->mBodyMaxHealth=5000;z->mHelmHealth=z->mHelmMaxHealth=0;z->mShieldHealth=z->mShieldMaxHealth=shield;
 if(shield){z->mShieldType=SHIELDTYPE_DOOR;z->AttachShield();}
 z->mPosY=z->GetPosYBasedOnRow(row);z->mY=int(z->mPosY);SetX(z,x);z->mVelX=0;z->SetAnimRate(rate);z->ApplyAnimRate(0);
 if(cold){NativeZombieColdData initial{30,200,1400,400};if(!Require(a,AttachNativeZombieColdData(a->mBoard,z,initial),"Fire initial cold fixture state invalid"))return nullptr;z->SetColdCounters(1400,400);z->ApplyAnimRate(0);}
 s.stationary.insert(z->mNetId);return z;
}
bool Business(LawnApp* a)
{
 auto* b=a->mBoard;auto* pea=Packet(b,0,0,false);auto* torch=Packet(b,1,18,false);auto* ice=Packet(b,0,10,false);
 const bool good=pea&&torch&&ice&&GetNativePacketData(pea)->price==100&&GetNativePacketData(torch)->price==175&&GetNativePacketData(ice)->price==75&&b->mSunMoney==s.money&&
  pea->mRefreshTime==(s.peaUses?750:0)&&torch->mRefreshTime==(s.torchUses?750:0)&&ice->mRefreshTime==(s.iceUses?5000:0)&&(!a->NetIsHost()||(pea->mTimesUsed==s.peaUses&&torch->mTimesUsed==s.torchUses&&ice->mTimesUsed==s.iceUses));
 if(!good)LawnNet::NetTrace("SMOKE_FUSION_FIRE_BUSINESS role=%s sun=%d expected=%d pea=%u cd=%d used=%d/%d torch=%u cd=%d used=%d/%d ice=%u cd=%d used=%d/%d players=%d",Role(a),b->mSunMoney,s.money,pea?pea->mNetId:0,pea?pea->mRefreshTime:-1,pea?pea->mTimesUsed:-1,s.peaUses,torch?torch->mNetId:0,torch?torch->mRefreshTime:-1,torch?torch->mTimesUsed:-1,s.torchUses,ice?ice->mNetId:0,ice?ice->mRefreshTime:-1,ice?ice->mTimesUsed:-1,s.iceUses,a->mNetManager->GetPlayerCount());
 return Require(a,good,"Fire actual native prices/packet IDs/cooldown/wallet changed");
}
bool Purchase(LawnApp* a,int who,int kind,int col,int result,int before,int after,int first,int second,Ref& out)
{
 auto* b=a->mBoard;auto* card=Packet(b,who,kind,false);
 if(Slot(a)==who&&!s.clicked){if(!card||!card->mActive||card->mRefreshing)return false;s.pickSamples=Samples(a);if(PickPacket(a,card))s.clicked=true;return false;}
 if(Slot(a)==who&&!s.placed){if(!HeldPacket(b,card)||(a->NetIsClient()&&Samples(a)<=s.pickSamples))return false;if(!Require(a,b->mSunMoney==before,"Fire pickup charged before placement"))return false;s.placed=Cell(a,col,kRow);return false;}
 if(!s.observed)
 {
  auto* p=At(b,col,kRow);const auto* d=GetNativePlantData(p);if(!d||d->kind!=result)return false;
  if(!Require(a,card&&d->packetId==card->mNetId&&d->owner==who&&d->instanceToken&&!d->randomOrigin&&d->firstParent==first&&d->secondParent==second&&p->mPlantHealth==300&&!p->mIsAsleep&&b->mSunMoney==after,"Fire actual purchase/provenance/awake HP differs"))return false;
  out=Identity(p);s.money=after;if(kind==0)++s.peaUses;else if(kind==18)++s.torchUses;else ++s.iceUses;s.observed=true;
  LawnNet::NetTrace("SMOKE_FUSION_FIRE_PURCHASE role=%s kind=%d id=%u token=%u packet=%u owner=%d parents=%d,%d sun=%d HP=300 nativeSDL=1",Role(a),result,out.id,out.token,card->mNetId,who,first,second,after);
 }return Business(a)&&Barrier(a,"FF123_BUY_"+std::to_string(s.step));
}
int SecondaryCandidates(){return s.caseIndex==0?1:s.caseIndex==1?2:s.caseIndex==2?1:s.caseIndex==3?4:1;}
int SecondaryCount(){return s.caseIndex==0?0:SecondaryCandidates();}
int SplashDamage(){return s.caseIndex==3?10:13;}
int InitialShield(){return s.caseIndex==0?80:s.caseIndex==1?20:0;}
bool SetupCohort(LawnApp* a)
{
 const int n=SecondaryCandidates();for(int i=0;i<7;++i)
 {
  const bool included=i==0||(i>=1&&i<=n);const float x=i==0||i==6?879.0f:i<=n?880.0f+i:i==5?1050.0f:1400.0f;
  auto* z=Spawn(a,i==6?kRow+1:kRow,i==0?InitialShield():0,included&&s.caseIndex<4,x);if(!z)return false;s.targets[i]=z->mNetId;
  LawnNet::NetTrace("SMOKE_FUSION_FIRE_TARGET case=%d index=%d id=%u row=%d x=%.3f body=5000 shield=%d initialCold=%d initialLevel=%d initialThreshold=%d actorSetupOnly=1",s.caseIndex,i,z->mNetId,z->mRow,z->mPosX,z->mShieldHealth,included&&s.caseIndex<4?1:0,included&&s.caseIndex<4?30:0,included&&s.caseIndex<4?200:100);
 }
 s.caseSpawned=true;
 // These are outside Plant's actual acquisition rectangle yet within a hot
 // projectile's source bound. The existing shot still hits naturally.
 auto* gun=ByRef(a->mBoard,s.shooter);SetX(Target(a->mBoard,s.aim),1400);
 if(!Require(a,gun&&!gun->FindTargetZombie(kRow,WEAPON_PRIMARY),"Fire scene left a native target for another unintended shot"))return false;
 return true;
}
std::string Ids(){std::ostringstream o;o<<"FF123_IDS "<<s.caseIndex<<' '<<s.flight.original;for(auto id:s.targets)o<<' '<<id;return o.str();}
bool ReadIds(LawnApp* a)
{
 if(a->NetIsHost())return s.caseSpawned;
 for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==0&&line.mText.rfind("FF123_IDS ",0)==0)
 {
  std::istringstream in(line.mText.substr(10));int which=-1;unsigned original=0;if(!(in>>which>>original)||which!=s.caseIndex)continue;
  if(!Require(a,original&&original<=65535,"Fire source ID invalid"))return false;s.flight.original=uint16_t(original);
  for(auto& id:s.targets){unsigned value=0;if(!Require(a,bool(in>>value)&&value&&value<=65535,"Fire target ID invalid"))return false;id=uint16_t(value);}return true;
 }return false;
}
bool BeginCase(LawnApp* a,int which)
{
 if(s.caseIndex!=which)
 {
  s.caseIndex=which;s.active=s.caseSpawned=s.hitObserved=false;s.targets={};s.flight={};s.beforeHP={};s.beforeShield={};s.beforeFreeze={};s.beforeLevel={};
  if(a->NetIsHost())
  {
   if(!s.aim){auto* z=Spawn(a,kRow,0,false,1400);if(!z)return false;s.aim=z->mNetId;}
  }
  LawnNet::NetTrace("SMOKE_FUSION_FIRE_CASE role=%s case=%d expectedN=%d expectedSplash=%d noHPOrTimerWritesAfterActorSetup=1",Role(a),which,SecondaryCount(),SplashDamage());
 }
 if(!Barrier(a,"FF123_STARTCASE_"+std::to_string(which)))return false;
 if(!s.active){if(a->NetIsHost())SetX(Target(a->mBoard,s.aim),600);s.active=true;}
 return true;
}
bool HoldTorch(LawnApp* a)
{
 auto* b=a->mBoard;auto* p=ByRef(b,s.torch1);const auto* ctx=GetNativeContext(b);if(!Require(a,p&&p->mPlantHealth==300,"Fire real Torch move source missing"))return false;
 if(ctx->gloveCooldownTicks&&!ctx->gloveHolds[1].plantId)return false;
 if(Slot(a)==1&&!s.toolClicked){auto rect=b->GetShovelButtonRect();b->GetZenButtonRect(OBJECT_TYPE_GLOVE,rect);s.toolClicked=BoardClick(a,float(rect.mX+rect.mWidth/2),float(rect.mY+rect.mHeight/2));return false;}
 if(Slot(a)==1&&!s.clicked){if(b->mCursorObject->mCursorType!=CURSOR_TYPE_GLOVE)return false;s.clicked=Cell(a,p->mPlantCol,p->mRow);return false;}
 if(ctx->gloveHolds[1].plantId!=s.torch1.id||ctx->gloveHolds[1].instanceToken!=s.torch1.token||GetNativeGloveHeldPlant(b,1)!=p)return false;
 if(Slot(a)==1&&b->mCursorObject->mCursorType!=CURSOR_TYPE_PLANT_FROM_GLOVE)return false;
 s.torchMoveData=*GetNativePlantData(p);s.sourceHeld=true;return Barrier(a,"FF123_TORCH_HELD");
}

bool InspectHit(LawnApp* a)
{
 auto* b=a->mBoard;auto* primary=Target(b,s.targets[0]);if(!primary)return true;
 if(!s.hitObserved)
 {
  const int body=5000-(40-(std::min)(40,InitialShield())),shield=(std::max)(0,InitialShield()-40);
  if(primary->mBodyHealth==5000&&primary->mShieldHealth==InitialShield())return true;
  for(int i=0;i<7;++i)
  {
   auto* z=Target(b,s.targets[i]);if(!z)return true;const auto* cold=GetNativeZombieColdData(z);
   const int expected=i==0?body:i<=SecondaryCount()?5000-SplashDamage():5000;
   const bool warmed=s.caseIndex<4&&(i==0||i<=SecondaryCount());
   const bool good=z->mBodyHealth==expected&&z->mShieldHealth==(i==0?shield:0)&&(!warmed||(cold&&cold->level==0&&cold->threshold==200&&!z->mIceTrapCounter&&!z->mChilledCounter));
   if(!good)LawnNet::NetTrace("SMOKE_FUSION_FIRE_HIT_MISMATCH role=%s case=%d index=%d id=%u body=%d expectedBody=%d shield=%d expectedShield=%d level=%d threshold=%d slow=%d freeze=%d warmExpected=%d visible=%d",Role(a),s.caseIndex,i,z->mNetId,z->mBodyHealth,expected,z->mShieldHealth,i==0?shield:0,cold?cold->level:-1,cold?cold->threshold:-1,z->mChilledCounter,z->mIceTrapCounter,warmed?1:0,z->mVisible?1:0);
   if(!Require(a,good,"Fire primary/shield/posthit circle distribution/Warm differs"))return false;
   if(a->NetIsHost()&&warmed&&!Require(a,s.beforeFreeze[i]>0&&s.beforeLevel[i]==30,"Fire initial cold condition expired before the natural warm hit"))return false;
   if(a->NetIsHost()&&s.caseIndex==0&&i==1&&!Require(a,cold&&cold->level==30&&cold->threshold==200&&z->mIceTrapCounter>0&&z->mChilledCounter>0,"Fire blocked shield incorrectly warmed an untouched secondary"))return false;
   LawnNet::NetTrace("SMOKE_FUSION_FIRE_OUTCOME role=%s case=%d index=%d id=%u body=%d shield=%d level=%d threshold=%d slow=%d freeze=%d",Role(a),s.caseIndex,i,z->mNetId,z->mBodyHealth,z->mShieldHealth,cold?cold->level:0,cold?cold->threshold:100,z->mChilledCounter,z->mIceTrapCounter);
  }
  s.hitObserved=true;LawnNet::NetTrace("SMOKE_FUSION_FIRE_HIT role=%s case=%d projectile=%u primary=%u damage=40 secondaryCount=%d secondaryDamage=%d shieldAfter=%d warmThreshold200=%d noFrozenDamageMultiplier=1 noFourthOutside=1 adjacentUnchanged=1",Role(a),s.caseIndex,s.flight.current,s.targets[0],SecondaryCount(),SecondaryCount()?SplashDamage():0,int(primary->mShieldType),s.caseIndex<4?1:0);
 }
 for(int i=0;i<7;++i){auto* z=Target(b,s.targets[i]);if(!z)return true;s.beforeHP[i]=z->mBodyHealth;s.beforeShield[i]=z->mShieldHealth;s.beforeFreeze[i]=z->mIceTrapCounter;s.beforeLevel[i]=GetNativeZombieColdData(z)?GetNativeZombieColdData(z)->level:0;}
 return true;
}
bool ObserveFlight(LawnApp* a)
{
 auto* b=a->mBoard;auto& f=s.flight;const bool snow=s.caseIndex==4;std::set<uint16_t> live;
 Projectile* shot=nullptr;while(b->mProjectiles.IterateNext(shot))
 {
  if(shot->mDead||shot->mRow!=kRow)continue;live.insert(shot->mNetId);const auto* data=GetNativeProjectileData(shot);
  if(a->NetIsHost()&&!f.born)
  {
   if(!Require(a,snow?(data&&data->kind==15&&data->damage==20&&!data->fireState):(!data&&shot->mProjectileType==PROJECTILE_PEA),"Fire expected actual basic/snow natural source emission"))return false;
   f.born=true;f.original=f.current=shot->mNetId;f.generation=b->mProjectiles.DataArrayGetID(shot);f.sample={b->mMainCounter,shot->mProjectileAge,shot->mPosX,shot->mPosY,shot->mPosZ};s.allProjectiles.insert(shot->mNetId);
   LawnNet::NetTrace("SMOKE_FUSION_FIRE_BORN case=%d id=%u source=%d age=%d tick=%d x=%.6f nativeNaturalFire=1 initialKind=%d",s.caseIndex,shot->mNetId,snow?1034:0,shot->mProjectileAge,b->mMainCounter,shot->mPosX,snow?15:0);
   if(!SetupCohort(a))return false;
  }
  if(a->NetIsHost()&&f.born&&shot->mNetId!=f.current)
  {
   if(snow&&!f.melted&&data&&data->kind==0&&!data->fireState&&data->lastTorchId==s.torch1.id&&data->lastTorchToken==s.torch1.token)
   {
    if(!Require(a,!ProjectileById(b,f.original)&&shot->mProjectileAge>0&&shot->mProjectileAge<=b->mMainCounter-f.sample.tick&&shot->mProjectileAge<f.sample.age&&data->damage==20&&!data->sunOnHit&&!data->specialType&&shot->mMotionType==MOTION_STRAIGHT,"Fire melt did not allocate fresh unheated ordinary actor/age"))return false;
    const int dt=b->mMainCounter-f.sample.tick;if(!Require(a,std::abs(shot->mPosX-f.sample.x-3.2f*dt)<.07f&&std::abs(shot->mPosY-f.sample.y)<.001f,"Fire melt changed copied trajectory"))return false;
    f.melted=true;f.current=shot->mNetId;f.generation=b->mProjectiles.DataArrayGetID(shot);s.allProjectiles.insert(shot->mNetId);
    LawnNet::NetTrace("SMOKE_FUSION_FIRE_MELT old=%u fresh=%u oldObservedAge=%d freshAge=%d torch=%u token=%u damage=20 fire=0 vx=3.2 copiedTrajectory=1 newIdentity=1",f.original,f.current,f.sample.age,shot->mProjectileAge,s.torch1.id,s.torch1.token);
    f.sample={b->mMainCounter,shot->mProjectileAge,shot->mPosX,shot->mPosY,shot->mPosZ};
   }
   else if(!Require(a,false,"Fire fixture observed an unintended additional source shot"))return false;
  }
  if(a->NetIsHost()&&shot->mNetId==f.current)
  {
   const int dt=b->mMainCounter-f.sample.tick;const float speed=snow?3.2f:3.33f;
   if(!Require(a,shot->mProjectileAge==f.sample.age+dt&&std::abs(shot->mPosX-f.sample.x-speed*dt)<.07f&&std::abs(shot->mPosY-f.sample.y)<.001f&&std::abs(shot->mPosZ-f.sample.z)<.001f,"Fire same-actor age/position/velocity changed at conversion"))return false;
   if(data)
   {
    if(!Require(a,data->kind==0||snow&&!f.melted&&data->kind==15,"Fire changed source kind instead of its bounded hot variant"))return false;
    if(data->fireState)
    {
     if(!Require(a,data->kind==0&&data->damage==40&&!data->sunOnHit&&shot->mProjectileType==PROJECTILE_FIREBALL&&std::abs(shot->mVelX-speed)<.001f,"Fire yellow variant/+20/base-versus-timed effective speed mismatch"))return false;
     const auto stamp=snow?s.torch2:s.torch1;if(!Require(a,data->lastTorchId==stamp.id&&data->lastTorchToken==stamp.token,"Fire nonstacking changed authoritative Torch identity stamp"))return false;
     if(!f.hot){f.hot=true;LawnNet::NetTrace("SMOKE_FUSION_FIRE_YELLOW case=%d id=%u age=%d tick=%d kind=0 damage=40 fire=1 stamp=%u token=%u vx=%.9g sameIdentity=%d ageTrajectoryPreserved=1",s.caseIndex,shot->mNetId,shot->mProjectileAge,b->mMainCounter,stamp.id,stamp.token,speed,snow?0:1);}
    }
   }
   auto* second=ByRef(b,s.torch2);if(f.hot&&GetRectOverlap(second->GetPlantAttackRect(WEAPON_PRIMARY),shot->GetProjectileRect())>=10&&!f.passedSecond)
   {f.passedSecond=true;LawnNet::NetTrace("SMOKE_FUSION_FIRE_SECOND case=%d id=%u secondTorch=%u damage=40 fire=1 nonstack=1",s.caseIndex,shot->mNetId,second->mNetId);}
   if(snow&&f.melted&&!f.hot)
   {
    auto* first=ByRef(b,s.torch1);if(first->mPlantCol==5&&GetRectOverlap(first->GetPlantAttackRect(WEAPON_PRIMARY),shot->GetProjectileRect())>=10)
    {if(!Require(a,data->lastTorchId==s.torch1.id&&data->lastTorchToken==s.torch1.token&&data->damage==20&&!data->fireState,"Moved same Torch reheated its stamped replacement"))return false;if(!f.sameTorchIgnored){f.sameTorchIgnored=true;LawnNet::NetTrace("SMOKE_FUSION_FIRE_SAME_TORCH id=%u torch=%u token=%u movedCol=5 actualOverlap=1 kind=0 damage=20 fire=0 sameStampIgnored=1",shot->mNetId,s.torch1.id,s.torch1.token);}}
   }
   f.sample={b->mMainCounter,shot->mProjectileAge,shot->mPosX,shot->mPosY,shot->mPosZ};
  }
  if(a->NetIsClient())
  {
   if(!data&&shot->mProjectileType==PROJECTILE_PEA&&!snow&&!f.clientOriginal){f.clientOriginal=true;f.original=shot->mNetId;LawnNet::NetTrace("SMOKE_FUSION_FIRE_SEEN case=%d id=%u kind=0 damage=20 fire=0 stamp=0 type=%d hostAgeNotSerialized=1",s.caseIndex,shot->mNetId,int(shot->mProjectileType));}
   if(data&&snow&&data->kind==15&&!f.clientOriginal){f.clientOriginal=true;f.original=shot->mNetId;LawnNet::NetTrace("SMOKE_FUSION_FIRE_SEEN case=4 id=%u kind=15 damage=20 fire=0 stamp=0 type=%d hostAgeNotSerialized=1",shot->mNetId,int(shot->mProjectileType));}
   if(data&&snow&&data->kind==0&&!data->fireState&&data->lastTorchId==s.torch1.id&&!f.clientMelted)
   {f.clientMelted=true;f.current=shot->mNetId;LawnNet::NetTrace("SMOKE_FUSION_FIRE_SEEN case=4 id=%u kind=0 damage=20 fire=0 stamp=%u type=%d hostAgeNotSerialized=1",shot->mNetId,data->lastTorchId,int(shot->mProjectileType));}
   if(data&&data->fireState&&!f.clientHot)
   {f.clientHot=true;f.current=shot->mNetId;LawnNet::NetTrace("SMOKE_FUSION_FIRE_SEEN case=%d id=%u kind=0 damage=40 fire=1 stamp=%u type=%d hostAgeNotSerialized=1",s.caseIndex,shot->mNetId,data->lastTorchId,int(shot->mProjectileType));}
  }
 }
 if(a->NetIsHost()&&f.born&&!live.contains(f.current)&&(!snow||f.melted))f.retired=true;
 if(s.caseSpawned&&a->NetIsHost())Send(a,Ids());ReadIds(a);
 if(snow&&a->NetIsClient()&&f.clientMelted&&!s.moveQueued)
 {auto* t=ByRef(b,s.torch1);const auto* ctx=GetNativeContext(b);if(!Require(a,t&&ctx->gloveHolds[1].plantId==s.torch1.id&&ctx->gloveHolds[1].instanceToken==s.torch1.token,"Fire held-live Torch tuple vanished before actual SDL move"))return false;s.moveQueued=Cell(a,5,kRow);}
 if(snow&&!s.moveObserved)
 {
  auto* t=ByRef(b,s.torch1);const auto* d=GetNativePlantData(t);const auto* ctx=GetNativeContext(b);
  if(t&&t->mPlantCol==5)
  {
   if(!Require(a,d&&d->instanceToken==s.torch1.token&&d->owner==s.torchMoveData.owner&&d->packetId==s.torchMoveData.packetId&&d->firstParent==s.torchMoveData.firstParent&&d->secondParent==s.torchMoveData.secondParent&&t->mPlantHealth==300&&!ctx->gloveHolds[1].plantId&&ctx->gloveCooldownTicks>0&&ctx->gloveCooldownTicks<=1000,"Fire ordinary Torch move reinitialized identity/provenance"))return false;
   s.moveObserved=true;LawnNet::NetTrace("SMOKE_FUSION_FIRE_TORCH_MOVE role=%s id=%u token=%u owner=%u packet=%u col=5 HP=300 sameInstance=1 heldOriginalActive=1",Role(a),t->mNetId,d->instanceToken,d->owner,d->packetId);
  }
 }
 if(!s.targets[0])return true;
 for(int i=0;i<7;++i)if(!Target(b,s.targets[i]))return true;
 if(!InspectHit(a))return false;
 // Refresh actual pre-hit values after each observer. No counter writes.
 if(!s.hitObserved)for(int i=0;i<7;++i){auto* z=Target(b,s.targets[i]);s.beforeFreeze[i]=z->mIceTrapCounter;s.beforeLevel[i]=GetNativeZombieColdData(z)?GetNativeZombieColdData(z)->level:0;}
 return true;
}
bool RunCase(LawnApp* a,int which)
{
 if(!BeginCase(a,which)||!Business(a)||!ObserveFlight(a))return false;auto& f=s.flight;
 if(a->NetIsClient()&&f.clientHot&&(which==2||which==4)&&!Capture(a,which==2?"fusion_fire_hot":"fusion_fire_moved"))return false;
 if(a->NetIsClient()&&f.clientHot&&f.clientOriginal&&(which!=4||f.clientMelted)&&s.hitObserved)Send(a,"FF123_OBSERVED_"+std::to_string(which));
 if(a->NetIsHost())
 {
  if(!f.retired||!f.hot||!f.passedSecond||!s.hitObserved||(which==4&&(!f.melted||!f.sameTorchIgnored||!s.moveObserved)))return false;
  if(!Send(a,"FF123_HIT_"+std::to_string(which)))return false;
 }
 if(!Has(a,0,"FF123_HIT_"+std::to_string(which))||!Has(a,1,"FF123_OBSERVED_"+std::to_string(which))||!Barrier(a,"FF123_DONE_"+std::to_string(which)))return false;
 if(a->NetIsHost())for(auto id:s.targets)if(auto* z=Target(a->mBoard,id))SetX(z,1500);
 LawnNet::NetTrace("SMOKE_FUSION_FIRE_CASE_DONE role=%s case=%d projectile=%u hot40=1 N=%d secondaryDamage=%d noSun=1 nativeBoard=1",Role(a),which,f.current,SecondaryCount(),SecondaryCount()?SplashDamage():0);
 s.active=false;return true;
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionFire(LawnApp* a)
{
 const char* native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE"),*enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_FIRE");
 if(!a||!native||std::strcmp(native,"1")||!enabled||std::strcmp(enabled,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
 if(a->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();if(GetTickCount()-s.since>75000U){char why[110];sprintf_s(why,"Fire deadline stage=%d step=%d case=%d role=%s",s.stage,s.step,s.caseIndex,Role(a));a->CoopSmokeFail(why);return true;}
 if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
 if(s.stage==0)
 {
  if(a->NetIsHost()&&!s.configured){if(!Require(a,!a->mBoard&&!a->mSeedChooserScreen,"Fire requires fresh lobby"))return true;a->mNetManager->SetDifficulty(1);s.configured=true;}
  if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
  if(a->NetIsClient()){Advance(a,1);Send(a,"FF123_START");}else if(Has(a,1,"FF123_START")){Advance(a,1);std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;
 }
 if(!a->mBoard||!IsNativeFusionBoard(a->mBoard)){if(s.stage==1&&a->NetIsClient())Send(a,"FF123_START");return true;}auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);auto* chooser=a->mSeedChooserScreen;const int slot=Slot(a);
 if(!Require(a,ctx&&ctx->validation&&!a->mFusionRuntime&&b->GetColumnCount()==10,"Fire lost actual native10x5 board"))return true;const auto& bank=ctx->selection.State();
 if(a->NetIsHost())for(auto id:s.stationary)if(auto* z=Target(b,id))z->ApplyAnimRate(0);
 if(s.stage==1)
 {
  if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||!b->mCutScene->mSeedChoosing||chooser->mSeedsInFlight)return true;const size_t target=(std::min)(kNativeFusionBankCapacity,ctx->eligible.size());
  if(!Require(a,ctx->eligible.size()==20&&target==10&&bank.targetCount==target&&bank.activeMask==3&&bank.quotas[0]==5&&bank.quotas[1]==5,"Fire20eligible/10bank/5+5 actual native chooser differs"))return true;if(!Capture(a,"fusion_fire_chooser"))return true;
  std::vector<int> choices;for(size_t i=0;i<ctx->eligible.size();++i){const auto& e=ctx->eligible[i];if(slot==0?(!e.isExtra&&e.kind!=18):(e.isExtra||e.kind==18))choices.push_back(int(i));}
  auto priority=[&](int i){const auto& e=ctx->eligible[i];return slot==0?(e.kind==0?0:e.kind==10?1:10+e.kind):(e.kind==18&&!e.isExtra?0:10+e.kind);};std::stable_sort(choices.begin(),choices.end(),[&](int x,int y){return priority(x)<priority(y);});
  int own=0;for(const auto& packet:bank.packets)own+=packet.owner==slot;if(own<s.waitingOwn)return true;if(!Require(a,choices.size()>=bank.quotas[slot],"Fire disjoint chooser candidates missing"))return true;
  if(own<bank.quotas[slot]){int x=0,y=0;chooser->GetSeedPositionInChooser(choices[own],x,y);if(Click(a,float(chooser->mX+x+25),float(chooser->mY+y+35-int(chooser->mScrollbar->mScrollValue))))s.waitingOwn=own+1;return true;}
  if(bank.packets.size()!=target||!Packet(b,0,0,false)||!Packet(b,0,10,false)||!Packet(b,1,18,false))return true;int rank=-1;for(int i=0;i<b->mSeedBank->mNumPackets;++i){const int r=NetSeedBankOwnerRank(slot,b->mSeedBank->mSeedPackets[i].mNetOwnerSlot);if(!Require(a,r>=rank,"Fire bank lost own-first view"))return true;rank=r;}
  NetTrace("SMOKE_FUSION_FIRE_POOL role=%s eligible=20 target=10 quota0=5 quota1=5 ownFirst=1 nativeChooser=1 unfiltered=1",Role(a));Advance(a,2);
 }
 else if(s.stage==2){if(bank.phase!=FusionBankPhase::Choosing)Advance(a,3);else if(chooser&&!chooser->mSeedsInFlight&&Ready(a,chooser->mStartButton))Advance(a,3);}
 else if(s.stage==3)
 {
  if(bank.phase==FusionBankPhase::Intro){if(!s.introStart)s.introStart=GetTickCount();s.introLast=GetTickCount();++s.introChecks;return true;}
  if(bank.phase!=FusionBankPhase::Playing||a->mGameScene!=SCENE_PLAYING||chooser)return true;
  if(!Require(a,s.introChecks>=10&&s.introLast-s.introStart>=1000&&b->mSunMoney==500,"Fire skipped native full intro/initial500"))return true;s.playingTick=b->mMainCounter;Advance(a,4);
 }
 else if(s.stage==4)
 {
  if(b->mMainCounter-s.playingTick<2||!Capture(a,"fusion_fire_playing")||!Barrier(a,"FF123_PLAYING"))return true;
  if(!s.budget)
  {
   if(a->NetIsHost()&&!s.budgetWritten){if(!Require(a,!b->mPlants.mSize&&!b->mZombies.mSize&&b->mSunMoney==500,"Fire wallet setup was not initial-only"))return true;b->mSunMoney=kInitialWallet;b->mSunCountDown=b->mZombieCountDown=b->mZombieCountDownStart=100000;s.budgetWritten=true;}
   if(b->mSunMoney!=kInitialWallet)return true;s.budget=true;NetTrace("SMOKE_FUSION_FIRE_BUDGET role=%s campaignInitialSun=500 fixtureInitialSun=525 totalCost=525 finalExpected=0 sunWritesAfterSetup=0",Role(a));
  }
  if(Barrier(a,"FF123_BUDGET"))Advance(a,5);
 }
 else if(s.stage==5)
 {
  switch(s.step)
  {
  case 0:if(Purchase(a,0,0,0,0,525,425,kFusionNoPlant,kFusionNoPlant,s.shooter))Next(a);break;
  case 1:if(Purchase(a,1,18,2,18,425,250,kFusionNoPlant,kFusionNoPlant,s.torch1))Next(a);break;
  case 2:if(Purchase(a,1,18,7,18,250,75,kFusionNoPlant,kFusionNoPlant,s.torch2))Next(a);break;
  case 3:case 4:case 5:case 6:if(RunCase(a,s.step-3))Next(a);break;
  case 7:if(Purchase(a,0,10,0,1034,75,0,0,10,s.shooter))Next(a);break;
  case 8:if(HoldTorch(a))Next(a);break;
  case 9:if(RunCase(a,4))Next(a);break;
  case 10:
   if(!Require(a,s.money==0&&s.peaUses==1&&s.torchUses==2&&s.iceUses==1&&s.moveObserved,"Fire complete packet counts/ordinary move missing"))return true;
   if(!Business(a)||!Capture(a,"fusion_fire_complete")||!Barrier(a,"FF123_COMPLETE"))return true;
   NetTrace("SMOKE_FUSION_FIRE_COMPLETE role=%s source18=1 realPurchases=4 initialSun=525 totalCost=525 finalSun=0 peaUses=1 torchUses=2 iceUses=1 hotCases=4 snowMeltCase=1 sameTorchMoved=1 nativeBoard=1 secondWorld=0 sunWritesAfterSetup=0 plantHPWrites=0 plantTimerWrites=0 sourceZombieAIUnported=1",Role(a));Advance(a,6);a->CoopSmokeWriteResult(true,nullptr);break;
  }
 }
 return true;
}
}
