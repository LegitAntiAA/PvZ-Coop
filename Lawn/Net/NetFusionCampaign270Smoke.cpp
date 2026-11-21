#include "NetFusionCampaign270Smoke.h"
#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetCommon.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../CursorObject.h"
#include "../MessageWidget.h"
#include "../Cutscene.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativeVisual.h"
#include "../Fusion/FusionNativeZombies.h"
#include "../Projectile.h"
#include "../../SexyAppFramework/ScrollbarWidget.h"
#include "../../SexyAppFramework/DDInterface.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <iterator>
#include <vector>
#include <set>
#include <string>
namespace
{
using namespace LawnFusion;using LawnNet::NetTrace;
struct Ref{uint16_t id=0;uint32_t token=0;};
struct State
{
 int group=0,stage=0,waiting=0,entry=-1,start=-1,mask=0;uint32_t epoch=0,since=0,chat=0,wheel=0,samples=0,diagnostic=0;
 bool configured=false,setup=false,pauseRequested=false,resumeRequested=false;
 std::array<Ref,5> parents{};unsigned armorMask=0;std::set<uint16_t> ordinary,damaged;uint16_t selectedHit=0;
 bool projectileSeen=false;std::set<std::string> frames;std::map<std::string,int> draws;
 std::array<Ref,4> waveActors{};std::map<uint16_t,float> firstWaveX;
 bool vehicleProof=false,hpSetup=false,hpKeys=false,hpMoneyDone=false;uint32_t hpSamples=0,hpMoneySince=0,hpMoneySamples=0;
 Ref hpMoney;int hpMoneyPhase=0,hpMoneyDraw=0,hpMoneyHealth=0,hpMoneyMaxHealth=0;
}s;
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool okay,const char* why){if(!okay)a->CoopSmokeFail(why);return okay;}
void Advance(LawnApp* a,int stage){s.stage=stage;s.since=GetTickCount();s.diagnostic=0;NetTrace("SMOKE_CAMPAIGN270_STAGE role=%s group=%d epoch=%u stage=%d",Role(a),s.group,s.epoch,stage);}
std::string Tag(const char* name){return "C270_"+std::to_string(s.group)+"_"+std::to_string(s.epoch)+"_"+name;}
bool Has(LawnApp* a,int who,const std::string& value){for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==who&&line.mText==value)return true;return false;}
bool Send(LawnApp* a,const std::string& value)
{if(!Require(a,value.size()<=LawnNet::NET_MAX_CHAT_CHARS,"Campaign270 chat proof exceeded native limit"))return false;if(Has(a,Slot(a),value))return true;if(s.chat&&GetTickCount()-s.chat<800)return false;s.chat=GetTickCount();a->mNetManager->SendChat(value);return Has(a,Slot(a),value);}
bool Barrier(LawnApp* a,const char* name){const auto value=Tag(name);Send(a,value);return Has(a,0,value)&&Has(a,1,value);}
bool Click(LawnApp* a,float x,float y)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Campaign270 owned SDL click failed");
}
bool Escape(LawnApp* a)
{SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Campaign270 owned SDL Escape failed");}
bool Choice(LawnApp* a,SeedChooserScreen* c,int index)
{
    int x=0,y=0;c->GetSeedPositionInChooser(index,x,y);const float cy=float(y+35-int(c->mScrollbar->mScrollValue));
    if(cy<158||cy>493)
    {if(s.wheel&&GetTickCount()-s.wheel<250)return false;s.wheel=GetTickCount();SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=float(c->mX+300);m.motion.y=float(c->mY+300);SDL_Event w{};w.type=SDL_EVENT_MOUSE_WHEEL;w.wheel.windowID=m.motion.windowID;w.wheel.y=cy<158?1.f:-1.f;Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&w),"Campaign270 chooser wheel failed");return false;}
    return std::abs(c->mScrollAmount)<=.05f&&Click(a,float(c->mX+x+25),float(c->mY)+cy);
}
bool Capture(LawnApp* a,const char* suffix)
{
    const auto name="fusion_campaign270_g"+std::to_string(s.group)+"_"+suffix;if(s.frames.contains(name))return true;
    if(!s.draws.contains(name))s.draws[name]=a->mDrawCount;
    const int requested=s.draws[name]+2;if(!a->CoopSmokeEnsureFreshFrameAfterDraw(requested))return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Campaign270 fresh owned BMP failed"))return false;
    s.frames.insert(name);NetTrace("SMOKE_CAMPAIGN270_RENDER role=%s group=%d frame=%s requestedDraw=%d capturedDraw=%d",Role(a),s.group,name.c_str(),requested,a->mCoopSmokeFrameDrawCount);return true;
}
Plant* P(Board* b,int i)
{Plant* p=nullptr;while(b->IteratePlants(p))if(!p->mDead&&p->mNetId==s.parents[i].id){const auto* d=GetNativePlantData(p);if(d&&d->instanceToken==s.parents[i].token)return p;}return nullptr;}
bool Preflight(LawnApp* a)
{
 std::string error;auto* b=a->mBoard;
 const bool okay=GetNativeSeedForPlant(0)!=SEED_NONE&&EnsureNativePlantVisuals(b,0,&error);
 NetTrace("SMOKE_CAMPAIGN270_PREFLIGHT model1000=%d error=%s nativeAdventure=1 sourceNpcFactoryUsed=0",okay,error.c_str());
 return Require(a,okay,"Campaign270 real1000 native profile/model unavailable");
}
bool Setup(LawnApp* a)
{
 auto* b=a->mBoard;
 if(!Require(a,a->NetIsHost()&&!b->mPaused&&!b->mPlants.mSize&&!b->mZombies.mSize,"Campaign270 needs initial empty native world")||!Preflight(a))return false;
 if(!Require(a,b->mSunMoney==150&&b->mNumWaves==4,"Campaign opening sun/wave defaults are not native150/4"))return false;
 for(int r=0;r<5;++r)if(!Require(a,b->mPlantRow[r]==(r==2?PLANTROW_NORMAL:PLANTROW_DIRT),"Campaign opening must have only middle row"))return false;
 for(const auto& card:GetNativeContext(b)->eligible)if(!Require(a,card.kind==0,"Campaign opening exposed an unearned plant"))return false;
 for(int row=0;row<5;++row)
 {
  NativePlantData d;d.kind=0;d.owner=LawnNet::NET_SHARED_OWNER;
  auto* p=b->AddPlant(row,2,GetNativeSeedForPlant(0),SEED_NONE,&d);const auto* live=GetNativePlantData(p);
  if(!Require(a,p&&!p->mDead&&p->mNetId&&live&&live->instanceToken&&live->kind==0&&p->mPlantHealth==p->mPlantMaxHealth,"Campaign270 actual initialPea factory failed"))return false;
  s.parents[row]={p->mNetId,live->instanceToken};
  NetTrace("SMOKE_CAMPAIGN270_INITIAL role=host id=%u token=%u kind=0 row=%d col=2 hp=%d/%d parents=none shared=1 acquisitionNotExecuted=1 initialHPOverrides=0 initialClockOverrides=0",p->mNetId,live->instanceToken,row,p->mPlantHealth,p->mPlantMaxHealth);
 }
 const ZombieType types[]={ZOMBIE_NORMAL,ZOMBIE_TRAFFIC_CONE,ZOMBIE_PAIL};
 for(int i=0;i<3;++i){auto* z=b->AddZombieInRow(types[i],2,-1);if(!Require(a,z&&!GetNativeSourceZombieData(z),"Native armor fixture allocation failed"))return false;z->mPosX=650.f+50*i;z->mX=int(z->mPosX);NetTrace("SMOKE_CAMPAIGN270_ARMOR_INITIAL id=%u type=%d body=%d helm=%d initialPositionOnly=1",z->mNetId,int(z->mZombieType),z->mBodyHealth,z->mHelmHealth);}
 s.setup=true;s.pauseRequested=Escape(a);return true;
}
bool Bind(LawnApp* a)
{
 auto* b=a->mBoard;
 if(!Require(a,b->mLevel==1&&b->mBackground==BACKGROUND_1_DAY&&GetNativeFusionRowCount(b)==5&&b->mNumWaves>0,"Campaign270 ordinary Adventure1 waves/background missing"))return false;
 for(int row=0;row<5;++row)
 {
  if(!s.parents[row].id){Plant* p=nullptr;while(b->IteratePlants(p))if(!p->mDead&&p->mPlantCol==row&&p->mRow==2)if(const auto* d=GetNativePlantData(p);d&&d->kind==0){s.parents[row]={p->mNetId,d->instanceToken};break;}}
  auto* p=P(b,row);const auto* d=GetNativePlantData(p);
  if(!p||!d)return false;
  if(!Require(a,d->kind==0&&d->instanceToken&&d->owner==LawnNet::NET_SHARED_OWNER&&p->mPlantHealth==p->mPlantMaxHealth,"Campaign270 initial full-generation/source-parent metadata mismatch"))return false;
 }
 s.mask|=1;return true;
}
bool Identity(LawnApp* a)
{
 LawnNet::NetBuffer bytes;for(auto p:s.parents){bytes.WriteU16(p.id);bytes.WriteU32(p.token);}
 char name[32];sprintf_s(name,"IDS_%08x",LawnNet::NetFnv1a32(bytes.Data(),bytes.Size()));return Barrier(a,name);
}
bool Observe(LawnApp* a)
{
 auto* b=a->mBoard;const int elapsed=s.start<0?0:b->mMainCounter-s.start;
 if(!Require(a,a->mGameScene==SCENE_PLAYING&&b->mLevel==1,"Campaign270 unexpected terminal/next-board before proof"))return false;
 Projectile* q=nullptr;while(b->IterateProjectiles(q))if(!q->mDead)if(const auto* d=GetNativeProjectileData(q);d&&d->kind==0&&d->damage==20&&d->sunOnHit==0&&!d->isZombieBullet)
 {if(!s.projectileSeen)NetTrace("SMOKE_CAMPAIGN270_NATIVE_SHOT role=%s id=%u kind=0 damage=20 sunOnHit=0 row=%d elapsed=%d",Role(a),q->mNetId,q->mRow,elapsed);s.projectileSeen=true;s.mask|=4;}
 Zombie* z=nullptr;while(b->IterateZombies(z))if(!z->mDead&&!z->IsDeadOrDying()&&!z->mMindControlled&&z->mNetId)
 {
  if(!Require(a,!GetNativeSourceZombieData(z),"Campaign270 wave actor unexpectedly has source NPC metadata"))return false;
  if(z->mZombieType!=ZOMBIE_NORMAL&&z->mZombieType!=ZOMBIE_FLAG&&z->mZombieType!=ZOMBIE_TRAFFIC_CONE&&z->mZombieType!=ZOMBIE_PAIL)continue;
  if(s.ordinary.insert(z->mNetId).second)NetTrace("SMOKE_CAMPAIGN270_WAVE role=%s id=%u type=%d row=%d hp=%d/%d currentWave=%d numWaves=%d typedSourceNPC=0 elapsed=%d",Role(a),z->mNetId,int(z->mZombieType),z->mRow,z->mBodyHealth,z->mBodyMaxHealth,b->mCurrentWave,b->mNumWaves,elapsed);
  const unsigned previousMask=s.armorMask;
  if(z->mZombieType==ZOMBIE_NORMAL&&z->mBodyHealth<z->mBodyMaxHealth)s.armorMask|=1;
  if(z->mZombieType==ZOMBIE_TRAFFIC_CONE&&z->mHelmHealth<z->mHelmMaxHealth)s.armorMask|=2;
  if(z->mZombieType==ZOMBIE_PAIL&&z->mHelmHealth<z->mHelmMaxHealth)s.armorMask|=4;
  if(s.armorMask!=previousMask)NetTrace("SMOKE_CAMPAIGN270_ARMOR_HIT role=%s id=%u type=%d armor=%d/%d mask=%u",Role(a),z->mNetId,int(z->mZombieType),z->mHelmHealth,z->mHelmMaxHealth,s.armorMask);
  if(s.armorMask==7)s.mask|=2;
  // This cohort contains only the five sourcePea plants. No manually spawned
  // zombies, damage calls, phase changes or timer writes occur after Setup.
  if(s.projectileSeen&&z->mBodyMaxHealth-z->mBodyHealth>=20&&s.damaged.insert(z->mNetId).second)
   NetTrace("SMOKE_CAMPAIGN270_DAMAGE role=%s id=%u type=%d row=%d body=%d/%d loss=%d typedSourceNPC=0 elapsed=%d",Role(a),z->mNetId,int(z->mZombieType),z->mRow,z->mBodyHealth,z->mBodyMaxHealth,z->mBodyMaxHealth-z->mBodyHealth,elapsed);
 }
 if(a->NetIsHost()&&!s.selectedHit&&!s.damaged.empty())s.selectedHit=*s.damaged.begin();
 const auto prefix=Tag("HIT_");
 if(a->NetIsHost()&&s.selectedHit)Send(a,prefix+std::to_string(s.selectedHit));
 if(a->NetIsClient()&&!s.selectedHit)for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==0&&line.mText.starts_with(prefix))
 {const auto suffix=line.mText.substr(prefix.size());char* end=nullptr;const unsigned long id=std::strtoul(suffix.c_str(),&end,10);if(end&&!*end&&id>0&&id<=65535)s.selectedHit=uint16_t(id);}
 if(s.selectedHit&&s.damaged.contains(s.selectedHit)){const auto name="HITACK_"+std::to_string(s.selectedHit);if(Barrier(a,name.c_str()))s.mask|=8;}
 if(!s.diagnostic||GetTickCount()-s.diagnostic>5000)
 {s.diagnostic=GetTickCount();NetTrace("SMOKE_CAMPAIGN270_PROOF role=%s elapsed=%d mask=%02x wave=%d/%d countdown=%d ordinary=%u damaged=%u selectedHit=%u shot=%d scene=%d pause=%d validation=0 postSetupActorWrites=0",Role(a),elapsed,s.mask,b->mCurrentWave,b->mNumWaves,b->mZombieCountDown,unsigned(s.ordinary.size()),unsigned(s.damaged.size()),s.selectedHit,s.projectileSeen,int(a->mGameScene),b->mPaused);}
 if(s.start>=0&&!b->mPaused)
 {if(s.projectileSeen&&!Capture(a,"action"))return false;if(elapsed>=1000&&s.mask==15&&!Capture(a,"late"))return false;}
 return true;
}
bool Final(LawnApp* a)
{
 auto* b=a->mBoard;LawnNet::NetBuffer bytes;bytes.WriteU32(s.epoch);bytes.WriteI32(b->mLevel);bytes.WriteI32(b->mSunMoney);bytes.WriteI32(b->mCurrentWave);bytes.WriteI32(b->mNumWaves);bytes.WriteI32(b->mZombieCountDown);bytes.WriteU16(s.selectedHit);
 for(int i=0;i<5;++i)
 {
  auto* p=P(b,i);const auto* d=GetNativePlantData(p);
  bytes.WriteU16(s.parents[i].id);bytes.WriteU32(s.parents[i].token);bytes.WriteU8(p&&d);
  if(p&&d){bytes.WriteU16(d->kind);bytes.WriteI32(p->mPlantHealth);bytes.WriteI32(p->mPlantMaxHealth);bytes.WriteI32(d->productionTicks);bytes.WriteI32(d->pendingProductionTicks);}
 }
 // Use exactly native synchronized integer identity/health, without requiring
 // equality of host-only clocks or client-interpolated presentation positions.
 std::map<uint16_t,Zombie*> zombies;Zombie* z=nullptr;while(b->IterateZombies(z))if(!z->mDead)zombies.emplace(z->mNetId,z);
 for(const auto& [id,actor]:zombies){bytes.WriteU16(id);bytes.WriteU16(uint16_t(actor->mZombieType));bytes.WriteI32(actor->mBodyHealth);bytes.WriteI32(actor->mHelmHealth);bytes.WriteI32(actor->mShieldHealth);bytes.WriteU8(uint8_t(actor->mRow));}
 char tag[32];sprintf_s(tag,"FINAL_%08x",LawnNet::NetFnv1a32(bytes.Data(),bytes.Size()));return Barrier(a,tag);
}

bool ObserveHealthBars(LawnApp* app)
{
 auto* b=app->mBoard;
 if(app->NetIsHost()&&!s.hpSetup)
 {
  for(const auto [kind,row]:{std::pair<uint16_t,int>{1004,0},{1189,4}})
  {
   std::string error;if(!Require(app,EnsureNativePlantVisuals(b,kind,&error),"HP272 hybrid resource missing"))return false;
   NativePlantData data;data.kind=kind;data.owner=LawnNet::NET_SHARED_OWNER;
   auto* p=b->AddPlant(1,row,GetNativeSeedForPlant(kind),SEED_NONE,&data);
   if(!Require(app,p&&!p->mDead,"HP272 hybrid native factory failed"))return false;
   p->mPlantHealth=p->mPlantMaxHealth/2;NativePlantHealthChanged(p);
  }
  for(const auto [kind,row]:{std::pair<uint16_t,int>{0,1},{14,3}})
  {
   NativeSourceZombieSpawnRequest request;request.kind=kind;request.row=row;request.positioned=true;request.shadowX=500;request.friendly=true;
   if(!Require(app,SpawnNativeSourceZombie(b,request)!=nullptr,"HP272 friendly native factory failed"))return false;
  }
  std::string error;if(!Require(app,EnsureNativePlantVisuals(b,1,&error),"Money272 source Sunflower resource missing"))return false;
  NativePlantData data;data.kind=1;data.owner=LawnNet::NET_SHARED_OWNER;
  auto* p=b->AddPlant(3,4,GetNativeSeedForPlant(1),SEED_NONE,&data);const auto* identity=GetNativePlantData(p);
  if(!Require(app,p&&!p->mDead&&p->mNetId&&identity&&identity->kind==1&&identity->instanceToken,
      "Money272 source Sunflower native factory failed"))return false;
  NetTrace("SMOKE_MONEY272_INITIAL role=host kind=1 id=%u token=%u row=4 col=3 shared=1 wallet=%d initialEconomyWrites=0",
      p->mNetId,identity->instanceToken,GetNativeContext(b)->money);
  s.hpSetup=true;
  NetTrace("SMOKE_HP272_INITIAL hybridKinds=1004,1189 friendlyKinds=0,14 initialPlantHalfHealth=1 subsequentHealthWrites=0");
 }
 unsigned plants=0,friends=0;Plant* p=nullptr;Zombie* z=nullptr;
 while(b->IteratePlants(p))if(!p->mDead)if(const auto* d=GetNativePlantData(p);d&&p->mPlantHealth>0&&p->mPlantHealth<p->mPlantMaxHealth)
 {if(d->kind==1004)plants|=1;if(d->kind==1189)plants|=2;}
 while(b->IterateZombies(z))if(!z->mDead&&z->mMindControlled)if(const auto* d=GetNativeSourceZombieData(z);d&&d->health>0)
 {if(d->kind==0)friends|=1;if(d->kind==14)friends|=2;}
 if(plants!=3||friends!=3)return false;
 if(!s.hpMoneyDone)
 {
  Plant* target=nullptr;p=nullptr;
  while(b->IteratePlants(p))if(!p->mDead&&p->mPlantCol==3&&p->mRow==4)
   if(const auto* data=GetNativePlantData(p);data&&data->kind==1){target=p;break;}
  if(!target){if(s.hpMoney.id)Require(app,false,"Money272 original source Sunflower disappeared");return false;}
  const auto* identity=GetNativePlantData(target);const auto* context=GetNativeContext(b);
  if(!s.hpMoney.id)
  {
   s.hpMoney={target->mNetId,identity->instanceToken};s.hpMoneyHealth=target->mPlantHealth;s.hpMoneyMaxHealth=target->mPlantMaxHealth;
   s.hpMoneySince=GetTickCount();s.hpMoneySamples=Samples(app);
  }
  if(!Require(app,GetTickCount()-s.hpMoneySince<30000,"Money272 bounded client UI deadline")||
     !Require(app,context&&context->money==3000&&identity->owner==LawnNet::NET_SHARED_OWNER&&
         target->mNetId==s.hpMoney.id&&identity->instanceToken==s.hpMoney.token&&
         target->mPlantHealth==s.hpMoneyHealth&&target->mPlantMaxHealth==s.hpMoneyMaxHealth&&
         target->mPlantHealth==target->mPlantMaxHealth,"Money272 source identity/health/wallet changed"))return false;
  if(s.hpMoneyPhase==0)
  {
   const auto name="MONEY_TARGET_"+std::to_string(s.hpMoney.id)+"_"+std::to_string(s.hpMoney.token);
   if(!Barrier(app,name.c_str()))return false;
   s.hpMoneyPhase=1;
   if(app->NetIsClient())
   {
    SDL_Event down{};down.type=SDL_EVENT_KEY_DOWN;down.key.windowID=SDL_GetWindowID(app->mSDLWindow);down.key.key=SDLK_Z;down.key.down=true;
    SDL_Event up=down;up.type=SDL_EVENT_KEY_UP;up.key.down=false;
    if(!Require(app,SDL_PushEvent(&down)&&SDL_PushEvent(&up),"Money272 owned SDL Z failed"))return false;
   }
   return false;
  }
  if(app->NetIsClient())
  {
   if(s.hpMoneyPhase==1)
   {
    if(!b->mCursorObject||b->mCursorObject->mCursorType!=CURSOR_TYPE_MONEY_SIGN)return false;
    float x=0,y=0;if(!target->GetShadowCenter(x,y))return false;y-=20;
    SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.windowID=SDL_GetWindowID(app->mSDLWindow);
    motion.motion.x=x+b->mX+app->mDDInterface->mWideScreenOffsetX;motion.motion.y=y+b->mY+app->mDDInterface->mWideScreenOffsetY;
    if(!Require(app,SDL_PushEvent(&motion),"Money272 owned SDL hover failed"))return false;
    s.hpMoneyDraw=app->mDrawCount;s.hpMoneyPhase=2;return false;
   }
   if(s.hpMoneyPhase==2)
   {
    if(app->mDrawCount<s.hpMoneyDraw+2)return false;
    float x=0,y=0;if(!target->GetShadowCenter(x,y))return false;y-=20;
    if(!Require(app,b->mCursorObject&&b->mCursorObject->mCursorType==CURSOR_TYPE_MONEY_SIGN&&
        b->ToolHitTest(int(x),int(y))==target&&!target->mHighlighted,"Money272 unsupported hover affordance remains"))return false;
    NetTrace("SMOKE_MONEY272_UI role=client stage=hover kind=1 id=%u token=%u highlighted=0 wallet=3000 actualZ=1",
        target->mNetId,identity->instanceToken);
    if(!Click(app,x+b->mX+app->mDDInterface->mWideScreenOffsetX,y+b->mY+app->mDDInterface->mWideScreenOffsetY))return false;
    s.hpMoneyDraw=app->mDrawCount;s.hpMoneyPhase=3;return false;
   }
   if(s.hpMoneyPhase==3)
   {
    if(app->mDrawCount<s.hpMoneyDraw+2)return false;
    if(!Require(app,b->mAdvice&&b->mAdvice->mDuration>0&&b->mAdvice->mMessageStyle==MESSAGE_STYLE_HINT_FAST&&
        b->mAdvice->mLabel==_S("Монетка не усиливает это растение."),"Money272 unsupported click did not show native Advice"))return false;
    if(!Capture(app,"moneyhint"))return false;
    NetTrace("SMOKE_MONEY272_UI role=client stage=hint kind=1 id=%u token=%u nativeAdvice=1 actualLMB=1 wallet=3000 noCharge=1",
        target->mNetId,identity->instanceToken);
    s.hpMoneyPhase=4;
   }
  }
  const auto complete=Tag("MONEY_UNSUPPORTED");
  if(app->NetIsHost()&&!Has(app,1,complete))return false;
  if((app->NetIsClient()&&s.hpMoneyPhase!=4)||Samples(app)-s.hpMoneySamples<2||!Barrier(app,"MONEY_UNSUPPORTED"))return false;
  s.hpMoneyDone=true;
  NetTrace("SMOKE_MONEY272_COMPLETE role=%s kind=1 id=%u token=%u wallet=3000 sourceUnchanged=1 actualClientZHoverLMB=1 clientNativeAdvice=1 noCharge=1",
      Role(app),target->mNetId,identity->instanceToken);
 }
 if(app->NetIsHost()&&!s.hpKeys)
 {
  SDL_Event down{};down.type=SDL_EVENT_KEY_DOWN;down.key.windowID=SDL_GetWindowID(app->mSDLWindow);down.key.key=SDLK_TAB;down.key.down=true;
  SDL_Event up=down;up.type=SDL_EVENT_KEY_UP;up.key.down=false;
  if(!Require(app,SDL_PushEvent(&down)&&SDL_PushEvent(&up),"HP272 native Tab failed"))return false;
  s.hpKeys=Escape(app);return false;
 }
 if(!b->mPaused||!app->mShowHealthBar)return false;
 if(!s.hpSamples){s.hpSamples=Samples(app);return false;}
 if(Samples(app)-s.hpSamples<2||!Capture(app,"healthbars")||!Barrier(app,"HP_TAB_SHARED"))return false;
 NetTrace("SMOKE_HP272_COMPLETE role=%s hybridMask=03 friendlyMask=03 showHealth=1 paused=1 actualHostTab=1",Role(app));
 return true;
}
bool ObserveVehicleWaves(LawnApp* app,int level)
{
 auto* b=app->mBoard;
 const char* hpFlag=std::getenv("PVZ_COOP_SMOKE_FUSION_HP272");
 const bool hp=hpFlag&&std::strcmp(hpFlag,"1")==0;
 if(hp&&s.vehicleProof)
 {
  if(!ObserveHealthBars(app))return false;
  NetTrace("SMOKE_FUSION_VEHICLES272_COMPLETE role=%s level=%d mask=0f nativeBirthPosition=1 postSetupActorWrites=0",Role(app),level);
  return true;
 }
 if(!Require(app,b->mLevel==level&&b->StageHasPool()&&b->mTutorialState==TUTORIAL_OFF,
     "Vehicle272 requires ordinary Pool/Fog campaign"))return false;
 constexpr std::array<uint16_t,4> kinds{4,13,14,18};
 auto bind=[&]()
 {
  Zombie* z=nullptr;while(b->IterateZombies(z))
   if(const auto* d=GetNativeSourceZombieData(z);d&&!z->mDead)
    for(size_t i=0;i<kinds.size();++i)if(d->kind==kinds[i]&&
        z->mFromWave==(i<2?5:b->GetNumWavesPerFlag()-1)&&(!s.waveActors[i].id||z->mNetId<s.waveActors[i].id))
     s.waveActors[i]={z->mNetId,d->instanceToken};
 };
 if(app->NetIsHost()&&!s.setup)
 {
  const int previousWave=b->mCurrentWave,flag=b->GetNumWavesPerFlag()-1;
  for(int attempt=0;attempt<12;++attempt)
  {
   const int wave=attempt%2?flag:5;
   auto& slots=b->mZombiesInWave[wave];std::array<ZombieType,MAX_ZOMBIES_IN_WAVE> prior;
   std::copy(std::begin(slots),std::end(slots),prior.begin());
   std::fill(std::begin(slots),std::end(slots),ZOMBIE_INVALID);
   slots[0]=slots[1]=ZOMBIE_PAIL;
   b->mCurrentWave=wave;b->SpawnZombieWave();
   std::copy(prior.begin(),prior.end(),std::begin(slots));bind();
   if(std::all_of(s.waveActors.begin(),s.waveActors.end(),[](Ref r){return r.id!=0;}))break;
  }
  b->mCurrentWave=previousWave;s.setup=true;
  if(!Require(app,std::all_of(s.waveActors.begin(),s.waveActors.end(),[](Ref r){return r.id!=0;}),
       "Vehicle272 native lane selection did not cover four source variants"))return false;
  NetTrace("SMOKE_FUSION_VEHICLES272_SETUP level=%d nativeBirthPosition=1 nativeRowRng=1 actorOverrides=0",level);
 }
 bind();LawnNet::NetBuffer identity;bool ready=true;
 for(size_t i=0;i<kinds.size();++i)
 {
  Zombie* z=nullptr;Zombie* candidate=nullptr;
  while(b->IterateZombies(candidate))if(!candidate->mDead&&candidate->mNetId==s.waveActors[i].id){z=candidate;break;}
  const auto* d=GetNativeSourceZombieData(z);
  if(!d){ready=false;continue;}
  if(!Require(app,d->kind==kinds[i]&&d->instanceToken==s.waveActors[i].token&&
      ((kinds[i]==13||kinds[i]==14)?b->mPlantRow[z->mRow]==PLANTROW_POOL:b->mPlantRow[z->mRow]==PLANTROW_NORMAL),
      "Vehicle272 source identity/terrain mismatch"))return false;
  constexpr std::array<int,4> bodyHealth{270,270,1350,2700};
  if(!Require(app,z->mBodyMaxHealth==bodyHealth[i]&&z->mBodyHealth==bodyHealth[i]&&d->health==float(bodyHealth[i]),
      "Vehicle272 source default health changed without damage"))return false;
  const auto [first,added]=s.firstWaveX.emplace(z->mNetId,z->mPosX);
  if(added||std::abs(first->second-z->mPosX)<1.f||z->mPosX>740.f)ready=false;
  identity.WriteU16(z->mNetId);identity.WriteU32(d->instanceToken);identity.WriteU16(d->kind);identity.WriteU8(static_cast<uint8_t>(z->mRow));
 }
 if(!ready)return false;
 char tag[48];sprintf_s(tag,"VEHICLE_IDS_%08x",LawnNet::NetFnv1a32(identity.Data(),identity.Size()));
 if(!Capture(app,"vehicles")||!Barrier(app,tag))return false;
 if(hp){s.vehicleProof=true;return false;}
 NetTrace("SMOKE_FUSION_VEHICLES272_COMPLETE role=%s level=%d mask=0f nativeBirthPosition=1 postSetupActorWrites=0",Role(app),level);
 return true;
}
bool ObserveCampaignWaves(LawnApp* app)
{
 auto* board=app->mBoard;
 if(!Require(app,board->mLevel==11&&board->mBackground==BACKGROUND_2_NIGHT&&board->mZombieAllowed[ZOMBIE_NEWSPAPER],
     "Fusion waves smoke requires the real Night adventure"))return false;
 const int flagWave=board->GetNumWavesPerFlag()-1;
 if(app->NetIsHost()&&!s.setup)
 {
  // Control the two wave entries only. The actual campaign path chooses rows,
  // spawns the variants, and transmits their own native actors to the client.
  const int previousWave=board->mCurrentWave;
  for(int wave:{0,flagWave})
  {
   std::array<ZombieType,MAX_ZOMBIES_IN_WAVE> prior;
   auto& slots=board->mZombiesInWave[wave];
   std::copy(std::begin(slots),std::end(slots),prior.begin());
   std::fill(std::begin(slots),std::end(slots),ZOMBIE_INVALID);
   slots[0]=ZOMBIE_NEWSPAPER;
   board->mCurrentWave=wave;
   board->SpawnZombieWave();
   std::copy(prior.begin(),prior.end(),std::begin(slots));
  }
  board->mCurrentWave=previousWave;
  s.setup=true;
  NetTrace("SMOKE_FUSION_WAVES271_SETUP level=11 controlledWaveSlots=2 actorOverrides=0");
 }
 std::array<Zombie*,2> paper{};
 Zombie* zombie=nullptr;
 while(board->IterateZombies(zombie))
 {
  const auto* data=GetNativeSourceZombieData(zombie);
  if(!data||zombie->mDead||zombie->mZombieType!=ZOMBIE_NEWSPAPER)continue;
  if(data->kind==15&&zombie->mFromWave==0)paper[0]=zombie;
  if(data->kind==104&&zombie->mFromWave==flagWave)paper[1]=zombie;
 }
 if(!paper[0]||!paper[1]||paper[0]->mPosX>620.0f||paper[1]->mPosX>620.0f)return false;
 if(!Require(app,!paper[0]->mInPool&&!paper[1]->mInPool&&paper[0]->mNetId&&paper[1]->mNetId&&
    paper[0]->mNetId!=paper[1]->mNetId,"Fusion wave actors did not retain distinct native identities"))return false;
 const auto identities="WAVE_ACTORS_"+std::to_string(paper[0]->mNetId)+"_"+std::to_string(paper[1]->mNetId);
 if(!Capture(app,"waves")||!Barrier(app,identities.c_str()))return false;
 NetTrace("SMOKE_FUSION_WAVES271_COMPLETE role=%s level=11 regular15=%u flag104=%u x15=%.3f x104=%.3f",
    Role(app),paper[0]->mNetId,paper[1]->mNetId,paper[0]->mPosX,paper[1]->mPosX);
 return true;
}
}
namespace LawnNet
{
bool NetSmokeUpdateFusionCampaign270(LawnApp* a)
{
 const char* enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_CAMPAIGN270"),*native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
 if(!a||!enabled||std::strcmp(enabled,"1")||!native||std::strcmp(native,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
 const char* wavesFlag=std::getenv("PVZ_COOP_SMOKE_FUSION_WAVES271");
 const bool waves=wavesFlag&&std::strcmp(wavesFlag,"1")==0;
 const char* vehicleFlag=std::getenv("PVZ_COOP_SMOKE_FUSION_WAVES272");
 const int vehicleLevel=vehicleFlag?std::atoi(vehicleFlag):0;
 if(vehicleLevel&&vehicleLevel!=21&&vehicleLevel!=31){a->CoopSmokeFail("Vehicle272 unsupported test level");return true;}
 if(a->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();if(GetTickCount()-s.since>180000){a->CoopSmokeFail("Campaign270 bounded grouped deadline; see source proof mask");return true;}
 if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
 if(s.stage==0){if(a->NetIsHost()&&!s.configured){a->mNetManager->SetDifficulty(1);s.configured=true;}if(a->mNetManager->GetPlayerCount()!=2)return true;
  if(a->NetIsClient()){Advance(a,1);Send(a,"C270_BEGIN");}else if(Has(a,1,"C270_BEGIN")){Advance(a,1);a->KillGameSelector();a->NetCloseTransientScreens();a->PreNewGame(GAMEMODE_ADVENTURE,false,vehicleLevel?vehicleLevel:waves?11:1);}return true;}
 if(!a->mBoard||!IsNativeFusionBoard(a->mBoard))return true;auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);if(!s.epoch)s.epoch=ctx->epoch;
 if(!Require(a,ctx->epoch==s.epoch&&!ctx->validation&&ctx->visuals&&!a->mFusionRuntime&&b->GetColumnCount()==10,"Campaign270 requires matching native campaign world"))return true;
 const auto& bank=ctx->selection.State();auto* chooser=a->mSeedChooserScreen;
 if(s.stage==1){if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||chooser->mSeedsInFlight||!b->mCutScene->mSeedChoosing)return true;
  int own=0;for(const auto& packet:bank.packets)if(packet.owner==Slot(a))++own;if(own<s.waiting)return true;
  if(own<bank.quotas[Slot(a)]){int dense=own;for(int i=0;i<Slot(a);++i)dense+=bank.quotas[i];if(!Require(a,dense>=0,"Campaign270 chooser source card missing"))return true;if(Choice(a,chooser,dense))s.waiting=own+1;return true;}if(bank.packets.size()==bank.targetCount)Advance(a,2);}
 else if(s.stage==2){if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}auto* button=chooser?chooser->mStartButton:nullptr;
  if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
  if(!button->IsMouseOver()){SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.windowID=SDL_GetWindowID(a->mSDLWindow);motion.motion.x=x;motion.motion.y=y;Require(a,SDL_PushEvent(&motion),"Campaign270 Ready hover failed");}
  else if(Click(a,x,y))Advance(a,3);}
 else if(s.stage==3){if(bank.phase!=FusionBankPhase::Playing||chooser||a->mGameScene!=SCENE_PLAYING)return true;if(s.entry<0){s.entry=a->mAppCounter;return true;}if(a->mAppCounter<=s.entry)return true;
  if(waves){if(ObserveCampaignWaves(a))a->CoopSmokeWriteResult(true,nullptr);return true;}
  if(vehicleLevel){if(ObserveVehicleWaves(a,vehicleLevel))a->CoopSmokeWriteResult(true,nullptr);return true;}
  if(a->NetIsHost()&&!s.setup&&!Setup(a))return true;if(b->mPaused)Advance(a,4);}
 else if(s.stage==4){if(!b->mPaused||!Bind(a))return true;if(!s.samples){s.samples=Samples(a);return true;}if(Samples(a)-s.samples<2||!Identity(a)||!Capture(a,"before")||!Barrier(a,"BASELINE"))return true;if(s.start<0)s.start=b->mMainCounter;
  if(a->NetIsHost()&&!s.resumeRequested)s.resumeRequested=Escape(a);if(s.resumeRequested||a->NetIsClient())Advance(a,5);}
 else if(s.stage==5){if(b->mPaused||!Observe(a))return true;if(s.mask!=15||b->mMainCounter-s.start<1000||!s.frames.contains("fusion_campaign270_g0_late")||!Barrier(a,"EFFECTS"))return true;s.pauseRequested=false;Advance(a,6);}
 else if(s.stage==6){if(a->NetIsHost()&&!s.pauseRequested)s.pauseRequested=Escape(a);if(b->mPaused){s.samples=Samples(a);Advance(a,7);}}
 else if(s.stage==7){if(!b->mPaused||Samples(a)-s.samples<2||!Observe(a)||!Final(a)||!Capture(a,"complete")||!Barrier(a,"COMPLETE"))return true;
  NetTrace("SMOKE_CAMPAIGN270_CASE role=%s group=0 epoch=%u kinds=1 mask=0f finalSameState=1 snapshots=%u initialProps=controlled postSetupActorWrites=0",Role(a),s.epoch,Samples(a));
  NetTrace("SMOKE_CAMPAIGN270_COMPLETE role=%s groups=1 sourcePlants=1 parentActors=5 selectedGroup=0 freshFrames=4 actualPause=1 nativeAdventure=1 validation=0 nativeArmorTargets=1 sourcePeaDamage=1 openingProgression=1 initialHPOverrides=0 initialClockOverrides=0 campaignAll50=NotClaimed nextLevel=NotExecuted acceptedAdded=0 postSetupActorWrites=0",Role(a));a->CoopSmokeWriteResult(true,nullptr);}
 return true;
}
}
