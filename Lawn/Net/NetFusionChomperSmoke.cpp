#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Coin.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativeGloveUI.h"
#include "../Fusion/FusionNativeBasePlants.h"
#include "../Fusion/FusionChomperRules.h"
#include "../Fusion/FusionRecipe.h"
#include "../../Sexy.TodLib/Attachment.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/TodParticle.h"
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
constexpr int kInitialWallet=1000,kFinalWallet=300;
struct Ref {uint16_t id=0;uint32_t token=0;};
enum Npc {NUT_PREY,NUT_BITER,SUN_PREY,CHERRY_PREY,BLAST_VICTIM,TERMINAL_PREY,NPC_COUNT};
struct Meal
{
 int kind=0,row=0,lastState=-1,cycle=0,lastTick=0,lastCounter=0,lastHP=0,poses=0;
 Ref ref{};std::array<int,2> lock{},bite{},swallow{},ready{},healBefore{},healAfter{};
};
struct State
{
 int stage=0,step=0,waitingOwn=0,money=kInitialWallet,playingTick=-1;
 std::array<int,4> uses{};std::array<Meal,3> meals{{{1012,0},{1014,2},{1016,4}}};
 std::array<uint16_t,NPC_COUNT> npc{};std::set<uint16_t> coins;std::set<std::string> frames;
 std::map<std::string,int> firstDraw;std::map<uint16_t,int> coinSeenTick;uint32_t since=0,lastChat=0,introStart=0,introLast=0,pickSamples=0;
 int introChecks=0,parkHP=0,sunBirthTick=0,moveHP=0,moveCounter=0,moveTick=0,moveState=0;
 NativePlantData moveData{};Ref moveRef{};bool configured=false,setup=false,budgetWritten=false,budget=false;
 bool clicked=false,placed=false,observed=false,toolClicked=false,primaries=false,biterActive=false,biterParked=false;
 bool victimActive=false,firstBlast=false,secondMeal=false,finalActive=false,firstFX=false,terminalFX=false;
 bool heldDuringChew=false,sunMovedPending=false,secondHeal=false,terminal=false,nutProjectionPending=false,rewardVisualLogged=false;int coinSamples=0;
} s;
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool ok,const char* why){if(!ok)a->CoopSmokeFail(why);return ok;}
void Advance(LawnApp* a,int stage){s.stage=stage;s.since=GetTickCount();s.clicked=s.placed=s.observed=s.toolClicked=false;LawnNet::NetTrace("SMOKE_FUSION_CHOMPER_STAGE role=%s stage=%d",Role(a),stage);}
void Next(LawnApp* a){++s.step;Advance(a,s.stage);LawnNet::NetTrace("SMOKE_FUSION_CHOMPER_STEP role=%s step=%d",Role(a),s.step);}
bool Has(LawnApp* a,int who,const std::string& text){for(const auto& l:a->mNetManager->mChatLines)if(l.mSlot==who&&l.mText==text)return true;return false;}
bool Send(LawnApp* a,const std::string& text)
{if(Has(a,Slot(a),text))return true;if(s.lastChat&&GetTickCount()-s.lastChat<800)return false;s.lastChat=GetTickCount();a->mNetManager->SendChat(text);return Has(a,Slot(a),text);}
bool Barrier(LawnApp* a,const std::string& text){Send(a,text);return Has(a,0,text)&&Has(a,1,text);}
bool Capture(LawnApp* a,const char* name)
{
 if(s.frames.contains(name))return true;if(!s.firstDraw.contains(name)){s.firstDraw[name]=a->mDrawCount;return false;}if(a->mDrawCount-s.firstDraw[name]<2)return false;
 if(!Require(a,a->CoopSmokeCaptureFrame(name),"Chomper actual renderer capture failed"))return false;s.frames.insert(name);LawnNet::NetTrace("SMOKE_FUSION_CHOMPER_RENDER role=%s frame=%s freshDraws=%d",Role(a),name,a->mDrawCount-s.firstDraw[name]);return true;
}
bool Click(LawnApp* a,float x,float y)
{
 SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
 SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
 SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Chomper owned SDL click failed");
}
bool BoardClick(LawnApp* a,float x,float y){return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY);}
bool Cell(LawnApp* a,int col,int row){return BoardClick(a,float(a->mBoard->GetCellCenterX(col,row)),float(a->mBoard->GridToPixelY(col,row)+50));}
bool Ready(LawnApp* a,GameButton* b)
{
 if(!b||b->mDisabled||b->mBtnNoDraw)return false;const float x=b->mX+b->mWidth*.5f,y=b->mY+b->mHeight*.5f;
 if(!b->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"Chomper Ready hover failed");return false;}
 LawnNet::NetTrace("SMOKE_FUSION_CHOMPER_READY role=%s nativeSDL=1 hoverObserved=1",Role(a));return Click(a,x,y);
}
Plant* ByRef(Board* b,Ref ref){Plant* p=nullptr;while(b->mPlants.IterateNext(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&p->mIsOnBoard&&p->mNetId==ref.id&&d&&d->instanceToken==ref.token)return p;}return nullptr;}
Plant* At(Board* b,int col,int row){Plant* p=nullptr;while(b->mPlants.IterateNext(p))if(!p->mDead&&p->mIsOnBoard&&p->mPlantCol==col&&p->mRow==row)return p;return nullptr;}
Ref Identity(Plant* p){const auto* d=GetNativePlantData(p);return p&&d?Ref{p->mNetId,d->instanceToken}:Ref{};}
Zombie* Target(Board* b,uint16_t id){Zombie* z=nullptr;while(b->mZombies.IterateNext(z))if(!z->mDead&&z->mNetId==id&&(!z->mNetGhost||z->mVisible))return z;return nullptr;}
SeedPacket* Packet(Board* b,int owner,int kind)
{for(int i=0;i<b->mSeedBank->mNumPackets;++i){auto* p=&b->mSeedBank->mSeedPackets[i];const auto* d=GetNativePacketData(p);if(d&&d->kind==kind&&d->owner==owner&&!d->isExtra)return p;}return nullptr;}
bool HeldPacket(Board* b,SeedPacket* p){auto* c=b->mCursorObject;return c&&p&&c->mCursorType==CURSOR_TYPE_PLANT_FROM_BANK&&c->mSeedBankIndex>=0&&c->mSeedBankIndex<b->mSeedBank->mNumPackets&&b->mSeedBank->mSeedPackets[c->mSeedBankIndex].mNetId==p->mNetId;}
bool PickPacket(LawnApp* a,SeedPacket* p)
{if(!Require(a,p&&p->mActive&&!p->mRefreshing,"Chomper real packet not ready"))return false;auto* b=a->mBoard->mSeedBank;return BoardClick(a,float(b->mX+p->mX+p->mOffsetX+p->mWidth/2),float(b->mY+p->mY+p->mHeight/2));}
int CardIndex(int kind){return kind==5?0:kind==3?1:kind==1?2:3;}
bool Business(LawnApp* a)
{
 constexpr int kinds[]={5,3,1,2},owners[]={0,1,1,1},prices[]={150,50,50,150},cds[]={750,3000,750,5000};auto* b=a->mBoard;
 for(int i=0;i<4;++i)
 {
  auto* p=Packet(b,owners[i],kinds[i]);const auto* d=GetNativePacketData(p);const bool ok=p&&d&&d->price==prices[i]&&p->mRefreshTime==(s.uses[i]?cds[i]:0)&&(!a->NetIsHost()||p->mTimesUsed==s.uses[i]);
  if(!ok)LawnNet::NetTrace("SMOKE_FUSION_CHOMPER_PACKET_MISMATCH role=%s index=%d price=%d refresh=%d used=%d expectedPrice=%d expectedRefresh=%d expectedUses=%d",Role(a),i,d?d->price:-1,p?p->mRefreshTime:-1,p?p->mTimesUsed:-1,prices[i],s.uses[i]?cds[i]:0,s.uses[i]);
  if(!Require(a,ok,"Chomper canonical packet identity/price/cooldown changed"))return false;
 }
 if(b->mSunMoney!=s.money)LawnNet::NetTrace("SMOKE_FUSION_CHOMPER_WALLET role=%s actual=%d expected=%d step=%d",Role(a),b->mSunMoney,s.money,s.step);
 return Require(a,b->mSunMoney==s.money,"Chomper callback/hold/move unexpectedly changed wallet");
}
bool Purchase(LawnApp* a,int who,int kind,int row,int result,int before,int after,Ref& out)
{
 auto* b=a->mBoard;auto* card=Packet(b,who,kind);
 if(Slot(a)==who&&!s.clicked){if(!card||!card->mActive||card->mRefreshing)return false;s.pickSamples=Samples(a);if(PickPacket(a,card))s.clicked=true;return false;}
 if(Slot(a)==who&&!s.placed){if(!HeldPacket(b,card)||(a->NetIsClient()&&Samples(a)<=s.pickSamples))return false;if(!Require(a,b->mSunMoney==before,"Chomper pickup charged before placement"))return false;s.placed=Cell(a,2,row);return false;}
 if(!s.observed)
 {
  auto* p=At(b,2,row);const auto* d=GetNativePlantData(p);if(!d||d->kind!=result)return false;const int hp=result==1012?4000:300,first=result==5?kFusionNoPlant:5,second=result==5?kFusionNoPlant:kind;
  if(!Require(a,card&&d->packetId==card->mNetId&&d->owner==who&&d->instanceToken&&!d->randomOrigin&&d->firstParent==first&&d->secondParent==second&&p->mPlantHealth==hp&&p->mPlantMaxHealth==hp&&b->mSunMoney==after,"Chomper actual directed purchase/provenance/HP differs"))return false;
  out=Identity(p);s.money=after;++s.uses[CardIndex(kind)];s.observed=true;LawnNet::NetTrace("SMOKE_FUSION_CHOMPER_PURCHASE role=%s step=%d kind=%d id=%u token=%u packet=%u owner=%d parents=%d,%d HP=%d sun=%d nativeSDL=1",Role(a),s.step,result,out.id,out.token,card->mNetId,who,first,second,hp,after);
 }return Business(a)&&Barrier(a,"FC138_BUY_"+std::to_string(s.step));
}
void SetX(Zombie* z,float x){z->mPosX=x;z->mX=int(x);}
bool SetEatingPosition(LawnApp* a,Zombie* z,Plant* p)
{
 float x=0,y=0;FusionBaseRect area;if(!z||!p||!GetNativeBasePlantGroundAnchor(p,x,y)||!TryMakeFusionChomperRect(x,y,area))return false;
 for(int offset=100;offset>=-80;--offset)
 {
  SetX(z,float(p->mX+offset));const auto r=z->GetZombieRect();
  if(z->FindPlantTarget(ATTACKTYPE_CHEW)==p&&r.mX<area.x+area.width&&r.mX+r.mWidth>area.x&&r.mY<area.y+area.height&&r.mY+r.mHeight>area.y)
   return Require(a,!z->FindZombieTarget(),"Chomper fixture NPC acquired another NPC");
 }return Require(a,false,"Chomper native eating/source trigger geometry unavailable");
}
bool Setup(LawnApp* a)
{
 auto* b=a->mBoard;
 if(a->NetIsHost()&&!s.setup)
 {
  if(!Require(a,!b->mPlants.mSize&&!b->mZombies.mSize&&b->mSunMoney==500,"Chomper initial setup was not empty fresh500"))return false;
  b->mSunMoney=kInitialWallet;b->mSunCountDown=b->mZombieCountDown=b->mZombieCountDownStart=100000;b->mCurrentWave=b->mNumWaves;
  constexpr int rows[]={0,0,2,4,4,4},hp[]={5000,1500,5000,5000,1500,5000};
  for(int i=0;i<NPC_COUNT;++i)
  {
   auto* z=b->AddZombieInRow(ZOMBIE_NORMAL,rows[i],0);auto* body=z?a->ReanimationTryToGet(z->mBodyReanimID):nullptr;const float rate=body?body->mAnimRate:0;
   if(!Require(a,z&&z->mNetId&&body&&rate>0,"Chomper controlled native actor initialization failed"))return false;
   z->mBodyHealth=z->mBodyMaxHealth=hp[i];z->mHelmHealth=z->mHelmMaxHealth=z->mShieldHealth=z->mShieldMaxHealth=0;
   z->mPosY=z->GetPosYBasedOnRow(rows[i]);z->mY=int(z->mPosY);SetX(z,i==TERMINAL_PREY?700.0f:1500.0f);z->mVelX=0;z->SetAnimRate(rate);z->ApplyAnimRate(0);s.npc[i]=z->mNetId;
   LawnNet::NetTrace("SMOKE_FUSION_CHOMPER_NPC index=%d id=%u row=%d initialHP=%d actualNativeFamily=normal sourceZombieAIUnported=1",i,z->mNetId,rows[i],hp[i]);
  }s.setup=true;LawnNet::NetTrace("SMOKE_FUSION_CHOMPER_TERMINAL_SETUP currentWave=%d numWaves=%d guard=%u initialFixtureCondition=1 phaseWritesAfterSetup=0",b->mCurrentWave,b->mNumWaves,s.npc[TERMINAL_PREY]);
 }
 std::ostringstream message;message<<"FC138_IDS";for(auto id:s.npc)message<<' '<<id;
 if(a->NetIsHost()&&!Send(a,message.str()))return false;
 if(a->NetIsClient()&&!s.setup)for(const auto& l:a->mNetManager->mChatLines)if(l.mSlot==0&&l.mText.rfind("FC138_IDS ",0)==0)
 {std::istringstream in(l.mText.substr(10));for(auto& id:s.npc){unsigned n=0;if(!(in>>n)||!n||n>65535)return false;id=uint16_t(n);}s.setup=true;}
 if(!s.setup||b->mSunMoney!=kInitialWallet)return false;
 for(int i=0;i<NPC_COUNT;++i)if(!Target(b,s.npc[i]))return false;
 if(!s.budget){s.budget=true;LawnNet::NetTrace("SMOKE_FUSION_CHOMPER_BUDGET role=%s campaignInitialSun=500 fixtureInitialSun=1000 totalCost=700 finalExpected=300 sunWritesAfterSetup=0",Role(a));}
 return Barrier(a,"FC138_SETUP");
}
int Powies(LawnApp* a)
{if(!a->mEffectSystem||!a->mEffectSystem->mParticleHolder)return 0;int count=0;TodParticleSystem* p=nullptr;while(a->mEffectSystem->mParticleHolder->mParticleSystems.IterateNext(p))if(!p->mDead&&p->mParticleDef&&p->mEffectType==PARTICLE_POWIE)++count;return count;}
bool Observe(LawnApp* a)
{
 auto* b=a->mBoard;
 for(auto& rec:s.meals)
 {
  auto* p=ByRef(b,rec.ref);const auto* d=GetNativePlantData(p);if(!p||!d||d->kind!=rec.kind)continue;
  if(!Require(a,IsNativeChomper(p)&&p->mPlantHealth>0&&p->mPlantHealth<=p->mPlantMaxHealth&&d->owner==1&&!d->randomOrigin&&!d->producerRewardFlags&&!d->deathEventFlags,"Chomper canonical live family/ownership/flags changed"))return false;
  const char* pose=nullptr;float seconds=-1;FusionChomperTiming timing;if(!Require(a,TryGetFusionChomperTiming(d->kind,d->animationSpeed,timing)&&GetNativeChomperVisualPose(p,pose,seconds),"Chomper source phase cache/model state unavailable"))return false;
  int bit=std::strcmp(pose,"idle")==0?1:std::strcmp(pose,"bite")==0?2:std::strcmp(pose,"chew")==0?4:std::strcmp(pose,"swallow")==0?8:0;
  if(!Require(a,bit&&std::isfinite(seconds)&&seconds>=0,"Chomper canonical cached source pose invalid"))return false;
  if(!(rec.poses&bit)){rec.poses|=bit;LawnNet::NetTrace("SMOKE_FUSION_CHOMPER_POSE role=%s kind=%d id=%u token=%u state=%s elapsed=%.9g canonicalCache=1 clientNoLocalClock=1",Role(a),rec.kind,p->mNetId,d->instanceToken,pose,seconds);}
  if(p->mState==STATE_CHOMPER_BITING&&rec.lastState!=STATE_CHOMPER_BITING)
  {
   if(!Require(a,rec.cycle<2,"Chomper acquired an unintended third meal"))return false;const int c=rec.cycle++;rec.lock[c]=b->mMainCounter;
   if(a->NetIsHost())
   {
    const int index=rec.kind==1012?(c==0?NUT_PREY:NUT_BITER):rec.kind==1014?SUN_PREY:c==0?CHERRY_PREY:TERMINAL_PREY;
    auto* target=Target(b,s.npc[index]);if(!Require(a,target&&p->mTargetZombieID==b->ZombieGetID(target)&&p->mStateCountdown==timing.bite,"Chomper actual lock chose wrong native generation/deadline"))return false;
    LawnNet::NetTrace("SMOKE_FUSION_CHOMPER_LOCK kind=%d id=%u token=%u cycle=%d target=%u nativeGeneration=%u speed=%.9g biteTicks=%d chewTicks=%d swallowTicks=%d tick=%d",rec.kind,p->mNetId,d->instanceToken,rec.cycle,target->mNetId,uint32_t(p->mTargetZombieID),d->animationSpeed,timing.bite,timing.chew,timing.swallow,b->mMainCounter);
   }
  }
  if((p->mState==STATE_CHOMPER_BITING_GOT_ONE||p->mState==STATE_CHOMPER_DIGESTING)&&rec.cycle>0&&!rec.bite[rec.cycle-1])
  {
   const int c=rec.cycle-1;rec.bite[c]=b->mMainCounter;
   if(a->NetIsHost()&&!Require(a,p->mState==STATE_CHOMPER_BITING_GOT_ONE&&rec.bite[c]-rec.lock[c]==timing.bite&&p->mLaunchCounter==4000&&p->mTargetZombieID==ZOMBIEID_NULL,"Chomper natural bite/timing/digest start differs"))return false;
   LawnNet::NetTrace("SMOKE_FUSION_CHOMPER_BITE role=%s kind=%d id=%u token=%u cycle=%d observedElapsed=%d actualDigest=%d tick=%d hostExact=%d",Role(a),rec.kind,p->mNetId,d->instanceToken,rec.cycle,rec.bite[c]-rec.lock[c],a->NetIsHost()?p->mLaunchCounter:-1,b->mMainCounter,a->NetIsHost()?1:0);
  }
  if(a->NetIsHost()&&rec.lastTick&&p->mState==STATE_CHOMPER_DIGESTING&&rec.lastState==STATE_CHOMPER_DIGESTING)
   if(!Require(a,p->mLaunchCounter==rec.lastCounter-(b->mMainCounter-rec.lastTick),"Chomper actual digest clock reset/stopped (including held/moved actor)"))return false;
  if(p->mState==STATE_CHOMPER_SWALLOWING&&rec.lastState!=STATE_CHOMPER_SWALLOWING)
  {
   if(!Require(a,rec.cycle>0,"Chomper swallowed without observed real meal"))return false;const int c=rec.cycle-1;rec.swallow[c]=b->mMainCounter;
   if(a->NetIsHost())
   {
    if(!Require(a,rec.swallow[c]-rec.bite[c]==4000&&p->mStateCountdown==timing.swallow,"Chomper did not wait exact natural40s before Swallow"))return false;
    if(rec.kind==1012){rec.healBefore[c]=rec.lastHP;rec.healAfter[c]=p->mPlantHealth;const int expected=(std::min)(4000,rec.lastHP+1000);if(!Require(a,p->mPlantHealth==expected,"NutChomper natural recovery1000/clamp differs"))return false;LawnNet::NetTrace("SMOKE_FUSION_CHOMPER_HEAL cycle=%d id=%u token=%u before=%d after=%d clamp=%d tick=%d naturalDigestTicks=4000",rec.cycle,p->mNetId,d->instanceToken,rec.lastHP,p->mPlantHealth,expected==4000?1:0,b->mMainCounter);}
    if(rec.kind==1014&&!Require(a,p->mLaunchCounter==200,"SunChomper deferred reward not armed on real Swallow"))return false;
   }
   LawnNet::NetTrace("SMOKE_FUSION_CHOMPER_SWALLOW role=%s kind=%d id=%u token=%u cycle=%d elapsedFromBite=%d HP=%d phaseCache=%.9g tick=%d",Role(a),rec.kind,p->mNetId,d->instanceToken,rec.cycle,rec.swallow[c]-rec.bite[c],p->mPlantHealth,seconds,b->mMainCounter);
  }
  if(p->mState==STATE_READY&&rec.lastState==STATE_CHOMPER_SWALLOWING&&rec.cycle)
  {const int c=rec.cycle-1;rec.ready[c]=b->mMainCounter;if(a->NetIsHost()&&!Require(a,rec.ready[c]-rec.swallow[c]==timing.swallow,"Chomper source swallow phase ended at wrong deadline"))return false;}
  rec.lastState=p->mState;rec.lastTick=b->mMainCounter;rec.lastCounter=p->mLaunchCounter;rec.lastHP=p->mPlantHealth;
 }
 if(a->NetIsHost())
 {
  auto* nut=ByRef(b,s.meals[0].ref);auto* cherry=ByRef(b,s.meals[2].ref);
  if(nut&&s.meals[0].kind==GetNativePlantData(nut)->kind&&s.meals[0].bite[0]&&!s.biterActive)
  {if(!SetEatingPosition(a,Target(b,s.npc[NUT_BITER]),nut))return false;s.biterActive=true;LawnNet::NetTrace("SMOKE_FUSION_CHOMPER_NATURAL_BITER id=%u plant=%u initialHP=1500 plantHPWrites=0",s.npc[NUT_BITER],nut->mNetId);}
  if(nut&&s.biterActive&&!s.biterParked&&nut->mPlantHealth<=2500)
  {auto* z=Target(b,s.npc[NUT_BITER]);if(!Require(a,z&&nut->mPlantHealth>=2490&&nut->mState==STATE_CHOMPER_DIGESTING,"NutChomper controlled bite loss overshot/special phase wrong"))return false;SetX(z,1500);s.biterParked=true;s.parkHP=nut->mPlantHealth;LawnNet::NetTrace("SMOKE_FUSION_CHOMPER_BITTEN plant=%u token=%u HP=%d nativeBites=1 positionOnlyParking=1 tick=%d",nut->mNetId,s.meals[0].ref.token,s.parkHP,b->mMainCounter);}
  if(cherry&&s.meals[2].cycle==1&&cherry->mState==STATE_CHOMPER_BITING&&!s.victimActive)
  {
   auto* z=Target(b,s.npc[BLAST_VICTIM]);float x=0,y=0;NativeBlastSpec spec;if(!Require(a,z&&cherry->GetShadowCenter(x,y)&&TryMakeFusionChomperBlast(1016,x,y,cherry->mRow,spec),"CherryChomper actual blast geometry missing"))return false;
   const auto rect=z->GetZombieRect();SetX(z,z->mPosX+float(spec.centerX-(rect.mX+rect.mWidth*.5)));z->mPosY+=float(spec.centerY-(rect.mY+rect.mHeight*.5));z->mY=int(z->mPosY);const auto positioned=z->GetZombieRect();
   if(!Require(a,FusionDeathBlastRectOverlaps(spec,positioned.mX,positioned.mY,positioned.mWidth,positioned.mHeight),"CherryChomper controlled victim outside exact source ellipse"))return false;s.victimActive=true;
  }
 }
 if(s.victimActive||s.meals[2].bite[0])
 {auto* z=Target(b,s.npc[BLAST_VICTIM]);if(!z||z->IsDeadOrDying()){if(!s.firstBlast){s.firstBlast=true;LawnNet::NetTrace("SMOKE_FUSION_CHOMPER_BLAST role=%s victim=%u initialHP=1500 dying=1 sourceCherry1800=1 tick=%d",Role(a),s.npc[BLAST_VICTIM],b->mMainCounter);}}}
 const int fx=Powies(a);if(fx>0)
 {bool& seen=s.finalActive?s.terminalFX:s.firstFX;if(!seen){seen=true;LawnNet::NetTrace("SMOKE_FUSION_CHOMPER_FX role=%s terminal=%d livePowie=%d nativeEffectHolder=1 tick=%d",Role(a),s.finalActive?1:0,fx,b->mMainCounter);}}
 Coin* coin=nullptr;while(b->mCoins.IterateNext(coin))
 {
  if(coin->mDead||!coin->mNetId||coin->mType!=COIN_SUN||s.coins.contains(coin->mNetId)||(!a->NetIsHost()&&!coin->mVisible))continue;
  auto* sun=ByRef(b,s.meals[1].ref);if(!Require(a,sun&&sun->mPlantCol==7&&sun->mRow==2&&coin->GetSunValue()==25&&coin->mCoinMotion==COIN_MOTION_FROM_PLANT&&!coin->mIsBeingCollected&&std::abs(coin->mPosX-sun->mX)<100,"SunChomper real25Coin/current moved cell differs"))return false;
  if(a->NetIsHost()&&!Require(a,s.meals[1].swallow[0]&&b->mMainCounter-s.meals[1].swallow[0]==200,"SunChomper reward did not wait exactly200ticks after Swallow"))return false;
  s.coins.insert(coin->mNetId);s.coinSeenTick[coin->mNetId]=b->mMainCounter;if(!s.sunBirthTick)s.sunBirthTick=b->mMainCounter;LawnNet::NetTrace("SMOKE_FUSION_CHOMPER_COIN role=%s id=%u actor=%u token=%u value=25 col=7 row=2 x=%.4f y=%.4f elapsedFromSwallow=%d tick=%d",Role(a),coin->mNetId,sun->mNetId,s.meals[1].ref.token,coin->mPosX,coin->mPosY,b->mMainCounter-s.meals[1].swallow[0],b->mMainCounter);
 }
 return Require(a,s.coins.size()<=4,"SunChomper emitted duplicate or periodic reward");
}
bool RewardVisualReady(LawnApp* a)
{
 if(s.coins.size()!=4)return false;std::array<Coin*,4> ready{};size_t found=0;
 for(auto id:s.coins)
 {
  Coin* coin=nullptr;Coin* candidate=nullptr;while(a->mBoard->mCoins.IterateNext(candidate))if(!candidate->mDead&&candidate->mNetId==id){coin=candidate;break;}
  if(!Require(a,coin&&coin->mVisible&&!coin->mIsBeingCollected&&coin->mType==COIN_SUN&&coin->GetSunValue()==25,"Chomper visual checkpoint lost a real uncollected25 Sun"))return false;
  const int observedAge=a->mBoard->mMainCounter-s.coinSeenTick.at(id);
  // Ghost Coin age is not transmitted/ticked. Its actual received body has
  // nevertheless existed for80 local Board ticks; no gameplay clock is added.
  if(observedAge<80||(a->NetIsHost()&&coin->mCoinAge<80)||coin->mScale+.001f<coin->GetSunScale())return false;
  auto* body=a->ReanimationTryToGet(coin->mReanimationID);
  if(!body||body->mDead||body->mReanimationType!=REANIM_SUN||!body->mDefinition||!body->mTrackInstances||
      body->mFrameCount<=0||FindReanimAttachment(coin->mAttachmentID)!=body)return false;
  bool visibleImage=false;
  for(int i=0;i<body->mDefinition->mTracks.count;++i)
  {
   ReanimatorTransform transform;body->GetCurrentTransform(i,&transform);
   if(body->mTrackInstances[i].mRenderGroup==RENDER_GROUP_NORMAL&&transform.mAlpha>0&&transform.mFrame>=0&&
       body->GetCurrentTrackImage(body->mDefinition->mTracks.tracks[i].mName)){visibleImage=true;break;}
  }
  if(!visibleImage)return false;ready[found++]=coin;
 }
 if(!s.rewardVisualLogged)
 {
  for(auto* coin:ready)LawnNet::NetTrace("SMOKE_FUSION_CHOMPER_SUN_READY role=%s id=%u observedAge=%d nativeAge=%d scale=%.9g targetScale=%.9g nativeSunReanim=1 attached=1 visibleImageTrack=1",Role(a),coin->mNetId,a->mBoard->mMainCounter-s.coinSeenTick.at(coin->mNetId),coin->mCoinAge,coin->mScale,coin->GetSunScale());
  s.rewardVisualLogged=true;
 }
 return true;
}
bool Hold(LawnApp* a,Meal& rec)
{
 auto* b=a->mBoard;auto* p=ByRef(b,rec.ref);const auto* d=GetNativePlantData(p);const auto* ctx=GetNativeContext(b);if(!Require(a,p&&d,"Chomper glove source missing"))return false;if(ctx->gloveCooldownTicks&&!ctx->gloveHolds[1].plantId)return false;
 if(Slot(a)==1&&!s.toolClicked){auto r=b->GetShovelButtonRect();b->GetZenButtonRect(OBJECT_TYPE_GLOVE,r);s.toolClicked=BoardClick(a,float(r.mX+r.mWidth/2),float(r.mY+r.mHeight/2));return false;}
 if(Slot(a)==1&&!s.clicked){if(b->mCursorObject->mCursorType!=CURSOR_TYPE_GLOVE)return false;s.clicked=Cell(a,p->mPlantCol,p->mRow);return false;}
 if(ctx->gloveHolds[1].plantId!=rec.ref.id||ctx->gloveHolds[1].instanceToken!=rec.ref.token||GetNativeGloveHeldPlant(b,1)!=p)return false;
 if(Slot(a)==1&&b->mCursorObject->mCursorType!=CURSOR_TYPE_PLANT_FROM_GLOVE)return false;
 s.moveRef=rec.ref;s.moveData=*d;s.moveHP=p->mPlantHealth;s.moveCounter=p->mLaunchCounter;s.moveTick=b->mMainCounter;s.moveState=p->mState;
 if(!Require(a,p->mState==STATE_CHOMPER_DIGESTING,"Chomper ordinary glove hold did not occur during chew"))return false;
 return Barrier(a,"FC138_HELD_"+std::to_string(rec.kind));
}
bool Move(LawnApp* a,Meal& rec,int col,int row)
{
 auto* b=a->mBoard;auto* p=ByRef(b,rec.ref);const auto* d=GetNativePlantData(p);const auto* ctx=GetNativeContext(b);if(!Require(a,p&&d,"Chomper live held/moved source disappeared"))return false;
 if(Slot(a)==1&&!s.clicked){s.clicked=Cell(a,col,row);return false;}if(p->mPlantCol!=col||p->mRow!=row)return false;
 if(!Require(a,p->mPlantHealth==s.moveHP&&d->instanceToken==s.moveRef.token&&d->packetId==s.moveData.packetId&&d->owner==s.moveData.owner&&d->firstParent==s.moveData.firstParent&&d->secondParent==s.moveData.secondParent&&d->animationSpeed==s.moveData.animationSpeed&&!ctx->gloveHolds[1].plantId&&ctx->gloveCooldownTicks>0&&ctx->gloveCooldownTicks<=1000,"Chomper glove move reinitialized HP/provenance/speed/identity"))return false;
 if(a->NetIsHost()&&rec.kind==1012&&!Require(a,p->mState==s.moveState&&s.moveCounter-p->mLaunchCounter==b->mMainCounter-s.moveTick,"NutChomper actual held/moved digest reset"))return false;
 if(!Business(a)||!Barrier(a,"FC138_MOVED_"+std::to_string(rec.kind)))return false;
 LawnNet::NetTrace("SMOKE_FUSION_CHOMPER_MOVE role=%s kind=%d id=%u token=%u col=%d row=%d HP=%d owner=%u packet=%u sameInstance=1 digestNotReset=1 nativeSDL=1",Role(a),rec.kind,p->mNetId,rec.ref.token,col,row,p->mPlantHealth,d->owner,d->packetId);return true;
}
bool ActivatePrimary(LawnApp* a)
{
 if(a->NetIsHost()&&!s.primaries)
 {for(int i=0;i<3;++i)if(!SetEatingPosition(a,Target(a->mBoard,s.npc[i==0?NUT_PREY:i==1?SUN_PREY:CHERRY_PREY]),ByRef(a->mBoard,s.meals[i].ref)))return false;s.primaries=true;}
 return Barrier(a,"FC138_PRIMARY_ACTIVE");
}
bool AllGoneExceptGuard(Board* b)
{for(int i=0;i<NPC_COUNT;++i)if(i!=TERMINAL_PREY)if(auto* z=Target(b,s.npc[i]))if(!z->IsDeadOrDying())return false;return Target(b,s.npc[TERMINAL_PREY])!=nullptr;}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionChomper(LawnApp* a)
{
 const char* native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");const char* enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_CHOMPER");
 if(!a||!native||std::strcmp(native,"1")||!enabled||std::strcmp(enabled,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
 if(a->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();if(GetTickCount()-s.since>65000U){char why[120];sprintf_s(why,"Chomper deadline stage=%d step=%d role=%s",s.stage,s.step,Role(a));a->CoopSmokeFail(why);return true;}
 if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
 if(s.stage==0)
 {
  if(a->NetIsHost()&&!s.configured){if(!Require(a,!a->mBoard&&!a->mSeedChooserScreen,"Chomper needs fresh lobby"))return true;a->mNetManager->SetDifficulty(1);s.configured=true;}
  if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
  if(a->NetIsClient()){Advance(a,1);Send(a,"FC138_START");}else if(Has(a,1,"FC138_START")){Advance(a,1);std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;
 }
 if(!a->mBoard||!IsNativeFusionBoard(a->mBoard)){if(s.stage==1&&a->NetIsClient())Send(a,"FC138_START");return true;}auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);auto* chooser=a->mSeedChooserScreen;const int slot=Slot(a);
 if(!Require(a,ctx&&ctx->validation&&!a->mFusionRuntime&&b->GetColumnCount()==10,"Chomper lost native board"))return true;const auto& bank=ctx->selection.State();
 if(a->NetIsHost())for(auto id:s.npc)if(auto* z=Target(b,id))z->ApplyAnimRate(0);if(!Observe(a))return true;
 if(s.stage==1)
 {
  if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||!b->mCutScene->mSeedChoosing||chooser->mSeedsInFlight)return true;
  if(!Require(a,ctx->eligible.size()==20&&bank.targetCount==10&&bank.quotas[0]==5&&bank.quotas[1]==5&&bank.activeMask==3,"Chomper eligible20/native10/shared quotas differ"))return true;if(!Capture(a,"fusion_chomper_chooser"))return true;
  std::vector<int> choices;for(size_t i=0;i<ctx->eligible.size();++i){const auto& e=ctx->eligible[i];if(slot==0?(!e.isExtra&&e.kind!=1&&e.kind!=2&&e.kind!=3):(e.isExtra||e.kind==1||e.kind==2||e.kind==3))choices.push_back(int(i));}
  auto priority=[&](int i){const auto& e=ctx->eligible[i];return slot==0?(e.kind==5?0:10+e.kind):(!e.isExtra?(e.kind==3?0:e.kind==1?1:2):20+e.kind);};std::stable_sort(choices.begin(),choices.end(),[&](int x,int y){return priority(x)<priority(y);});
  int own=0;for(const auto& p:bank.packets)own+=p.owner==slot;if(own<s.waitingOwn)return true;if(!Require(a,choices.size()>=5,"Chomper disjoint live chooser priorities unavailable"))return true;
  if(own<5){int x=0,y=0;chooser->GetSeedPositionInChooser(choices[own],x,y);if(Click(a,float(chooser->mX+x+25),float(chooser->mY+y+35-int(chooser->mScrollbar->mScrollValue))))s.waitingOwn=own+1;return true;}
  if(bank.packets.size()!=10||!Packet(b,0,5)||!Packet(b,1,3)||!Packet(b,1,1)||!Packet(b,1,2))return true;int rank=-1;for(int i=0;i<b->mSeedBank->mNumPackets;++i){const int r=NetSeedBankOwnerRank(slot,b->mSeedBank->mSeedPackets[i].mNetOwnerSlot);if(!Require(a,r>=rank,"Chomper own-first native bank lost"))return true;rank=r;}
  NetTrace("SMOKE_FUSION_CHOMPER_POOL role=%s eligible=20 target=10 quota0=5 quota1=5 ownFirst=1 nativeChooser=1",Role(a));Advance(a,2);
 }
 else if(s.stage==2){if(bank.phase!=FusionBankPhase::Choosing)Advance(a,3);else if(chooser&&!chooser->mSeedsInFlight&&Ready(a,chooser->mStartButton))Advance(a,3);}
 else if(s.stage==3)
 {
  if(bank.phase==FusionBankPhase::Intro){if(!s.introStart)s.introStart=GetTickCount();s.introLast=GetTickCount();++s.introChecks;return true;}
  if(bank.phase!=FusionBankPhase::Playing||a->mGameScene!=SCENE_PLAYING||chooser)return true;if(!Require(a,s.introChecks>=10&&s.introLast-s.introStart>=1000&&b->mSunMoney==500,"Chomper skipped real full intro/fresh500"))return true;s.playingTick=b->mMainCounter;Advance(a,4);
 }
 else if(s.stage==4){if(b->mMainCounter-s.playingTick<2||!Capture(a,"fusion_chomper_playing")||!Barrier(a,"FC138_PLAYING"))return true;if(Setup(a))Advance(a,5);}
 else if(s.stage==5)
 {
  auto& nut=s.meals[0];auto& sun=s.meals[1];auto& cherry=s.meals[2];
  switch(s.step)
  {
  case 0:if(Purchase(a,0,5,0,5,1000,850,nut.ref))Next(a);break;
  case 1:if(Purchase(a,1,3,0,1012,850,800,nut.ref))Next(a);break;
  case 2:if(Purchase(a,0,5,2,5,800,650,sun.ref))Next(a);break;
  case 3:if(Purchase(a,1,1,2,1014,650,600,sun.ref))Next(a);break;
  case 4:if(Purchase(a,0,5,4,5,600,450,cherry.ref))Next(a);break;
  case 5:if(Purchase(a,1,2,4,1016,450,300,cherry.ref))Next(a);break;
  case 6:if(ActivatePrimary(a))Next(a);break;
  case 7:
   if(a->NetIsHost()&&s.biterParked)Send(a,"FC138_NUT_PARKED");if(!Has(a,0,"FC138_NUT_PARKED"))break;
   {
    auto* p=ByRef(b,nut.ref);const auto* data=GetNativePlantData(p);
    const bool projected=p&&p->mPlantHealth>0&&p->mPlantHealth<=2500&&p->mState==STATE_CHOMPER_DIGESTING;
    if(a->NetIsHost())
    {if(!Require(a,projected,"NutChomper host lost actual naturally bitten digest state"))break;}
    else if(!projected)
    {
     if(!s.nutProjectionPending)LawnNet::NetTrace("SMOKE_FUSION_CHOMPER_PROJECTION_PENDING role=client id=%u expectedToken=%u actualToken=%u HP=%d expectedHPMax=2500 state=%d expectedState=%d received=%u applied=%u chatIsNotStateBarrier=1",nut.ref.id,nut.ref.token,data?data->instanceToken:0,p?p->mPlantHealth:-1,p?int(p->mState):-1,int(STATE_CHOMPER_DIGESTING),a->mCoopSmokeSnapshotsReceived,a->mCoopSmokeSnapshotsApplied);
     s.nutProjectionPending=true;break;
    }
    if(s.nutProjectionPending)LawnNet::NetTrace("SMOKE_FUSION_CHOMPER_PROJECTED role=client id=%u token=%u HP=%d state=%d applied=%u actualStateBarrier=1",p->mNetId,nut.ref.token,p->mPlantHealth,int(p->mState),a->mCoopSmokeSnapshotsApplied);
    s.nutProjectionPending=false;
   }
   if(!s.firstBlast||!s.firstFX||nut.lastState!=STATE_CHOMPER_DIGESTING||sun.lastState!=STATE_CHOMPER_DIGESTING||cherry.lastState!=STATE_CHOMPER_DIGESTING)break;
   if(Capture(a,"fusion_chomper_chew")&&Barrier(a,"FC138_CHEW"))Next(a);break;
  case 8:if(Hold(a,nut))Next(a);break;
  case 9:if(Move(a,nut,3,0))Next(a);break;
  case 10:
  {auto* p=ByRef(b,sun.ref);const auto* d=GetNativePlantData(p);if(!p||p->mState!=STATE_CHOMPER_DIGESTING||!d||d->shootSeconds<30.0f)break;if(Hold(a,sun)){s.heldDuringChew=true;Next(a);}break;}
  case 11:
  {auto* p=ByRef(b,sun.ref);if(!Require(a,p&&GetNativeGloveHeldPlant(b,1)==p,"Held SunChomper stopped being fully alive"))break;if(p->mState!=STATE_CHOMPER_SWALLOWING)break;if(a->NetIsHost()&&!Require(a,p->mLaunchCounter>0,"SunChomper pending callback elapsed before actual UI move"))break;Next(a);break;}
  case 12:if(Move(a,sun,7,2)){s.sunMovedPending=true;Next(a);}break;
  case 13:
   if(!RewardVisualReady(a))break;if(!Business(a)||!Capture(a,"fusion_chomper_reward")||!Barrier(a,"FC138_FOUR_COINS"))break;
   NetTrace("SMOKE_FUSION_CHOMPER_REWARD role=%s actor=%u token=%u count=4 value=25 total=100 currentCol=7 currentRow=2 wallet=300 duplicate=0 pendingHeldAlive=1",Role(a),sun.ref.id,sun.ref.token);Next(a);break;
  case 14:
  {
   auto* p=ByRef(b,nut.ref);if(!p||p->mState!=STATE_READY||nut.swallow[0]==0||p->mPlantHealth<=3000||p->mPlantHealth>=4000)break;
   if(Barrier(a,"FC138_SECOND_NUT_READY"))Next(a);break;
  }
  case 15:
   if(a->NetIsHost()&&!s.secondMeal){if(!SetEatingPosition(a,Target(b,s.npc[NUT_BITER]),ByRef(b,nut.ref)))break;s.secondMeal=true;}
   if(Barrier(a,"FC138_SECOND_NUT_ACTIVE"))Next(a);break;
  case 16:
  {
   auto* p=ByRef(b,nut.ref);if(!p||nut.swallow[1]==0||p->mPlantHealth!=4000)break;s.secondHeal=true;
   if(!Capture(a,"fusion_chomper_recovered")||!Barrier(a,"FC138_CLAMP"))break;NetTrace("SMOKE_FUSION_CHOMPER_RECOVERED role=%s id=%u token=%u HP=4000 twoNatural40s=1 clamp=1 noHPWrites=1",Role(a),nut.ref.id,nut.ref.token);Next(a);break;
  }
  case 17:
   if(!AllGoneExceptGuard(b)||cherry.lastState!=STATE_READY||!s.secondHeal||!s.sunMovedPending||s.coins.size()!=4)break;
   if(a->NetIsHost()&&!Require(a,!b->HasLevelAwardDropped()&&b->AreEnemyZombiesOnScreen(),"CherryChomper terminal award already happened before final real meal"))break;
   if(Barrier(a,"FC138_TERMINAL_ARMED")){s.finalActive=true;Next(a);}break;
  case 18:
   if(a->NetIsHost()&&!s.terminal){if(!SetEatingPosition(a,Target(b,s.npc[TERMINAL_PREY]),ByRef(b,cherry.ref)))break;s.terminal=true;}
   if(cherry.bite[1]==0||!s.terminalFX)break;
   if(a->NetIsHost()){if(!Require(a,b->HasLevelAwardDropped()&&!b->AreEnemyZombiesOnScreen(),"CherryChomper actual lastkill did not award before accepted blast continuation"))break;if(!Send(a,"FC138_ACTUAL_AWARD"))break;}
   if(!Has(a,0,"FC138_ACTUAL_AWARD"))break;
   {bool award=false;Coin* c=nullptr;while(b->mCoins.IterateNext(c))if(!c->mDead&&c->IsLevelAward())award=true;if(!award)break;}
   if(!Capture(a,"fusion_chomper_terminal")||!Barrier(a,"FC138_TERMINAL"))break;
   NetTrace("SMOKE_FUSION_CHOMPER_TERMINAL role=%s actor=%u token=%u actualLastkillAward=1 liveNativeFX=1 continuationAfterAward=1 terminalTargetDamageNotClaimed=1 phaseWritesAfterSetup=0",Role(a),cherry.ref.id,cherry.ref.token);Next(a);break;
  case 19:
   if(!Require(a,s.uses==std::array<int,4>{3,1,1,1}&&s.heldDuringChew&&s.sunMovedPending&&s.secondHeal&&s.firstBlast&&s.firstFX&&s.terminalFX&&s.coins.size()==4,"Chomper final semantic proofs incomplete"))break;
   for(const auto& rec:s.meals)if(!Require(a,(rec.poses&15)==15,"Chomper client/host did not observe every source idle/bite/chew/swallow model phase"))return true;
   if(!Business(a)||!Capture(a,"fusion_chomper_complete")||!Barrier(a,"FC138_COMPLETE"))break;
   NetTrace("SMOKE_FUSION_CHOMPER_COMPLETE role=%s kinds=1012,1014,1016 purchases=6 initialSun=1000 totalCost=700 finalSun=300 uses=3,1,1,1 naturalDigest=4000 heal1000AndClamp=1 coins4x25=1 gloveMoves=2 terminalAdmission=1 nativeBoard=1 secondWorld=0 plantHPWrites=0 plantTimerWrites=0 phaseWritesAfterSetup=0 sourceZombieAIUnported=1",Role(a));Advance(a,6);a->CoopSmokeWriteResult(true,nullptr);break;
  }
 }
 return true;
}
}
