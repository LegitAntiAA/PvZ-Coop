#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativeLobber.h"
#include "../Fusion/FusionShooterTiming.h"
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
struct Ref {uint16_t id=0;uint32_t token=0;};
struct Health {int body=5000,helm=0,shield=0;};
struct Trigger {int tick=0,deadline=0;uint32_t target=0;float x=0,y=0;};
struct Flight {int kind=0,tick=0,age=0,firstSeen=0,samples=0;float x=0,z=0,vx=0,vz=0,acc=0;bool live=true;};
struct Cohort
{
 int kind=0,row=0,count=0,hits=0;std::array<uint16_t,5> ids{};std::array<Health,5> before{};
 bool spawned=false,initialized=false,active=false,positioned=false,done=false;int sampled=-1;
 std::set<uint16_t> offsetProjectiles;int offsetProofs=0,offsetImpactPostTick=-1;
};
struct State
{
 int stage=0,step=0,waitingOwn=0,money=500,playingTick=-1;std::array<int,3> uses{};
 Ref cabbage{},melon{},winter{};std::array<Cohort,3> groups{{{26,4,3},{32,2,5},{1116,2,5}}};
 std::map<uint16_t,Flight> flights;std::map<int,std::vector<Trigger>> triggers;std::set<std::string> frames;
 std::map<std::string,int> firstDraw;uint32_t since=0,lastChat=0,introStart=0,introLast=0,pickSamples=0;
 int introChecks=0,weakDeathTick=0;float weakAnim=0;bool weakPositive=false,weakProgress=false,weakRetired=false;
 bool configured=false,ambient=false,clicked=false,placed=false,observed=false,toolClicked=false;
} s;
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool ok,const char* why){if(!ok)a->CoopSmokeFail(why);return ok;}
void Advance(LawnApp* a,int stage){s.stage=stage;s.since=GetTickCount();s.clicked=s.placed=s.observed=s.toolClicked=false;LawnNet::NetTrace("SMOKE_FUSION_LOBBER_STAGE role=%s stage=%d",Role(a),stage);}
void Next(LawnApp* a){++s.step;Advance(a,s.stage);LawnNet::NetTrace("SMOKE_FUSION_LOBBER_STEP role=%s step=%d",Role(a),s.step);}
bool Has(LawnApp* a,int who,const std::string& text){for(const auto& l:a->mNetManager->mChatLines)if(l.mSlot==who&&l.mText==text)return true;return false;}
bool Send(LawnApp* a,const std::string& text)
{if(Has(a,Slot(a),text))return true;if(s.lastChat&&GetTickCount()-s.lastChat<800)return false;s.lastChat=GetTickCount();a->mNetManager->SendChat(text);return Has(a,Slot(a),text);}
bool Barrier(LawnApp* a,const std::string& text){Send(a,text);return Has(a,0,text)&&Has(a,1,text);}
bool Capture(LawnApp* a,const char* name)
{
 if(s.frames.contains(name))return true;if(!s.firstDraw.contains(name)){s.firstDraw[name]=a->mDrawCount;return false;}if(a->mDrawCount-s.firstDraw[name]<2)return false;
 if(!Require(a,a->CoopSmokeCaptureFrame(name),"Lobber actual renderer capture failed"))return false;s.frames.insert(name);LawnNet::NetTrace("SMOKE_FUSION_LOBBER_RENDER role=%s frame=%s freshDraws=%d",Role(a),name,a->mDrawCount-s.firstDraw[name]);return true;
}
bool Click(LawnApp* a,float x,float y)
{
 SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
 SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
 SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Lobber owned SDL click failed");
}
bool BoardClick(LawnApp* a,float x,float y){return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY);}
bool Cell(LawnApp* a,int col,int row){return BoardClick(a,float(a->mBoard->GetCellCenterX(col,row)),float(a->mBoard->GridToPixelY(col,row)+50));}
bool Ready(LawnApp* a,GameButton* b)
{
 if(!b||b->mDisabled||b->mBtnNoDraw)return false;const float x=b->mX+b->mWidth*.5f,y=b->mY+b->mHeight*.5f;
 if(!b->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"Lobber Ready hover failed");return false;}
 LawnNet::NetTrace("SMOKE_FUSION_LOBBER_READY role=%s nativeSDL=1 hoverObserved=1",Role(a));return Click(a,x,y);
}
Plant* ByRef(Board* b,Ref ref){Plant* p=nullptr;while(b->mPlants.IterateNext(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&p->mIsOnBoard&&p->mNetId==ref.id&&d&&d->instanceToken==ref.token)return p;}return nullptr;}
Plant* At(Board* b,int col,int row){Plant* p=nullptr;while(b->mPlants.IterateNext(p))if(!p->mDead&&p->mIsOnBoard&&p->mPlantCol==col&&p->mRow==row)return p;return nullptr;}
Ref Identity(Plant* p){const auto* d=GetNativePlantData(p);return p&&d?Ref{p->mNetId,d->instanceToken}:Ref{};}
Zombie* Target(Board* b,uint16_t id){Zombie* z=nullptr;while(b->mZombies.IterateNext(z))if(!z->mDead&&z->mNetId==id&&(!z->mNetGhost||z->mVisible))return z;return nullptr;}
SeedPacket* Packet(Board* b,int owner,int kind)
{for(int i=0;i<b->mSeedBank->mNumPackets;++i){auto* p=&b->mSeedBank->mSeedPackets[i];const auto* d=GetNativePacketData(p);if(d&&d->kind==kind&&d->owner==owner&&!d->isExtra)return p;}return nullptr;}
bool HeldPacket(Board* b,SeedPacket* p){auto* c=b->mCursorObject;return c&&p&&c->mCursorType==CURSOR_TYPE_PLANT_FROM_BANK&&c->mSeedBankIndex>=0&&c->mSeedBankIndex<b->mSeedBank->mNumPackets&&b->mSeedBank->mSeedPackets[c->mSeedBankIndex].mNetId==p->mNetId;}
bool PickPacket(LawnApp* a,SeedPacket* p)
{if(!Require(a,p&&p->mActive&&!p->mRefreshing,"Lobber real packet not ready"))return false;auto* b=a->mBoard->mSeedBank;return BoardClick(a,float(b->mX+p->mX+p->mOffsetX+p->mWidth/2),float(b->mY+p->mY+p->mHeight/2));}

int CardIndex(int kind){return kind==26?0:kind==32?1:2;}
Ref Source(int kind){return kind==26?s.cabbage:kind==32?s.melon:s.winter;}
Cohort& Group(int kind){return s.groups[kind==26?0:kind==32?1:2];}
bool Business(LawnApp* a)
{
 constexpr int kinds[]={26,32,10},owners[]={0,0,1},prices[]={100,300,75},cds[]={750,750,5000};auto* b=a->mBoard;
 for(int i=0;i<3;++i)
 {
  auto* p=Packet(b,owners[i],kinds[i]);const auto* d=GetNativePacketData(p);const bool ok=p&&d&&d->price==prices[i]&&p->mRefreshTime==(s.uses[i]?cds[i]:0)&&(!a->NetIsHost()||p->mTimesUsed==s.uses[i]);
  if(!ok)LawnNet::NetTrace("SMOKE_FUSION_LOBBER_PACKET_MISMATCH role=%s kind=%d price=%d refresh=%d used=%d expectedPrice=%d expectedRefresh=%d expectedUsed=%d",Role(a),kinds[i],d?d->price:-1,p?p->mRefreshTime:-1,p?p->mTimesUsed:-1,prices[i],s.uses[i]?cds[i]:0,s.uses[i]);
  if(!Require(a,ok,"Lobber actual bank/price/cooldown changed"))return false;
 }
 return Require(a,b->mSunMoney==s.money,"Lobber wallet changed outside real purchases");
}
bool Purchase(LawnApp* a,int who,int kind,int row,int result,int before,int after,Ref& out)
{
 auto* b=a->mBoard;auto* card=Packet(b,who,kind);
 if(Slot(a)==who&&!s.clicked){if(!card||!card->mActive||card->mRefreshing)return false;s.pickSamples=Samples(a);if(PickPacket(a,card))s.clicked=true;return false;}
 if(Slot(a)==who&&!s.placed){if(!HeldPacket(b,card)||(a->NetIsClient()&&Samples(a)<=s.pickSamples))return false;if(!Require(a,b->mSunMoney==before,"Lobber pickup charged before actual placement"))return false;s.placed=Cell(a,2,row);return false;}
 if(!s.observed)
 {
  auto* p=At(b,2,row);const auto* d=GetNativePlantData(p);if(!d||d->kind!=result)return false;const int first=result==1116?32:kFusionNoPlant,second=result==1116?10:kFusionNoPlant;
  if(!Require(a,card&&d->owner==who&&d->packetId==card->mNetId&&d->instanceToken&&!d->randomOrigin&&d->firstParent==first&&d->secondParent==second&&p->mPlantHealth==300&&p->mPlantMaxHealth==300&&!p->mIsAsleep&&b->mSunMoney==after&&IsNativeLobber(p),"Lobber real recipe/HP/awake/owner/provenance differs"))return false;
  out=Identity(p);s.money=after;++s.uses[CardIndex(kind)];s.observed=true;
  if(result==1116&&!Require(a,!ByRef(b,s.melon)&&out.id!=s.melon.id&&out.token!=s.melon.token,"WinterMelon fusion retained old generation"))return false;
  LawnNet::NetTrace("SMOKE_FUSION_LOBBER_PURCHASE role=%s kind=%d id=%u token=%u packet=%u owner=%d parents=%d,%d HP=300 sun=%d nativeSDL=1",Role(a),result,out.id,out.token,card->mNetId,who,first,second,after);
 }return Business(a)&&Barrier(a,"FL147_BUY_"+std::to_string(result));
}
void SetX(Zombie* z,float x){z->mPosX=x;z->mX=int(x);}
void CenterX(Zombie* z,float x){const auto r=z->GetZombieRect();SetX(z,z->mPosX+x-(r.mX+r.mWidth*.5f));}
Health ReadHealth(Zombie* z){return {z->mBodyHealth,z->mHelmHealth,z->mShieldHealth};}
bool Same(Health a,Health b){return a.body==b.body&&a.helm==b.helm&&a.shield==b.shield;}
// UpdateGameObjects uses the current logic tick, then UpdateGame increments it.
// CoopSmoke samples after Board::Update, so fixture observations use this epoch.
int PostFrameTick(int logicTick){return logicTick+1;}
bool ReadIds(LawnApp* a,Cohort& g)
{
 const std::string tag="FL147_IDS "+std::to_string(g.kind)+" ";
 for(const auto& l:a->mNetManager->mChatLines)if(l.mSlot==0&&l.mText.rfind(tag,0)==0)
 {std::istringstream in(l.mText.substr(tag.size()));for(int i=0;i<g.count;++i){unsigned n=0;if(!(in>>n)||!n||n>65535)return false;g.ids[i]=uint16_t(n);}g.spawned=true;return true;}return false;
}
bool SetupTargets(LawnApp* a,Cohort& g)
{
 auto* b=a->mBoard;
 if(a->NetIsHost()&&!g.spawned)
 {
  for(int i=0;i<g.count;++i)
  {
   const int row=g.kind==26?(i==2?3:4):i==1||i==2?1:i==3||(g.kind==32&&i==4)?3:2;
   const bool armor=g.kind==26?i==1:(g.kind==32&&i==0)||i==2;
   const int hp=g.kind==1116&&i==4?160:5000;
   auto* z=b->AddZombieInRow(ZOMBIE_NORMAL,row,0);auto* body=z?a->ReanimationTryToGet(z->mBodyReanimID):nullptr;const float rate=body?body->mAnimRate:0;
   if(!Require(a,z&&z->mNetId&&body&&std::isfinite(rate)&&rate>0,"Lobber controlled native NPC initialization failed"))return false;
   if(armor){z->mHelmType=HELMTYPE_BUCKET;z->AttachHelmet();z->mShieldType=SHIELDTYPE_DOOR;z->AttachShield();}
   z->mBodyHealth=z->mBodyMaxHealth=hp;z->mHelmHealth=z->mHelmMaxHealth=armor?1200:0;z->mShieldHealth=z->mShieldMaxHealth=armor?3000:0;
   z->mPosY=z->GetPosYBasedOnRow(row);z->mY=int(z->mPosY);z->mVelX=0;SetX(z,1500);z->SetAnimRate(rate);z->ApplyAnimRate(0);g.ids[i]=z->mNetId;
   LawnNet::NetTrace("SMOKE_FUSION_LOBBER_SETUP kind=%d index=%d id=%u row=%d initialHP=%d helmet=%d shield=%d actualNativeFamily=normal sourceZombieAIUnported=1 oneTimeInitialValues=1",g.kind,i,z->mNetId,row,hp,z->mHelmHealth,z->mShieldHealth);
  }g.spawned=true;
 }
 std::ostringstream text;text<<"FL147_IDS "<<g.kind;for(int i=0;i<g.count;++i)text<<' '<<g.ids[i];
 if(a->NetIsHost()&&!Send(a,text.str()))return false;if(a->NetIsClient()&&!g.spawned&&!ReadIds(a,g))return false;
 if(!g.spawned)return false;
 for(int i=0;i<g.count;++i)
 {
  auto* z=Target(b,g.ids[i]);if(!z)return false;const bool armor=g.kind==26?i==1:(g.kind==32&&i==0)||i==2;const int hp=g.kind==1116&&i==4?160:5000;
  if(z->mBodyHealth!=hp||z->mHelmHealth!=(armor?1200:0)||z->mShieldHealth!=(armor?3000:0))return false;
  g.before[i]=ReadHealth(z);
 }
 g.initialized=true;return Barrier(a,"FL147_TARGETS_"+std::to_string(g.kind));
}
bool Activate(LawnApp* a,Cohort& g)
{
 g.active=true;
 if(a->NetIsHost()&&!g.positioned)
 {
  for(int i=0;i<g.count;++i)
  {
   auto* z=Target(a->mBoard,g.ids[i]);if(!Require(a,z,"Lobber controlled NPC missing before activation"))return false;
    const float x=g.kind==26?(i==1?1500:600):i==3?800:i==4?(g.kind==32?1500:480):650;CenterX(z,x);
   if(!Require(a,!z->FindZombieTarget(),"Lobber fixture permits zombie-versus-zombie combat"))return false;
  }g.positioned=true;
 }
 return Barrier(a,"FL147_ACTIVE_"+std::to_string(g.kind));
}
void Park(Cohort& g,Board* b)
{for(int i=0;i<g.count;++i)if(auto* z=Target(b,g.ids[i]))SetX(z,1500);}
bool PrepareOffsetBody(LawnApp* a,Projectile* p,const FusionLobberProfile& profile)
{
 if(!a->NetIsHost()||profile.kind!=32)return true;auto& g=Group(32);
 if(g.offsetProjectiles.contains(p->mNetId))return true;
 auto* primary=Target(a->mBoard,g.ids[0]);auto* edge=Target(a->mBoard,g.ids[4]);
 if(!Require(a,primary&&edge&&edge->mRow==3&&edge->mHelmHealth==0&&edge->mShieldHealth==0,"Melon offset-only bare BODY target missing"))return false;
 const auto primaryRect=primary->GetZombieRect();float x=p->mPosX,z=p->mPosZ,vz=p->mVelZ;int steps=0;FusionLobberPoint root;
 // Predict from the real emitted actor solely to position one stationary NPC.
 // No actor clock/velocity/health is modified; actual flight remains observed below.
 for(int step=1;step<=2000;++step)
 {
  FusionLobberRect box;if(!AdvanceFusionLobberMotion(p->mVelX,p->mAccZ,x,z,vz)||!TryMakeFusionLobberRect(GetNativeProjectileData(p)->kind,x+20,p->mPosY+z+20,box))break;
  const int left=int(std::floor(box.x)),top=int(std::floor(box.y));
  const Sexy::Rect predicted(left,top,int(std::ceil(box.x+box.width))-left,int(std::ceil(box.y+box.height))-top);
  if(predicted.Intersects(primaryRect)){steps=step;root={x+20,p->mPosY+z+20};break;}
  if(FusionLobberGroundHit(z,vz))break;
 }
 if(!Require(a,steps>0,"Melon offset BODY setup could not predict the unchanged stationary primary collider"))return false;
 CenterX(edge,float(root.x));const auto oldRect=edge->GetZombieRect();
 edge->mPosY+=float(root.y+1.7*kFusionLobberPixelsY-oldRect.mY);edge->mY=int(edge->mPosY);
 const auto nativeBody=edge->GetZombieRect();const FusionLobberRect body{double(nativeBody.mX),double(nativeBody.mY),double(nativeBody.mWidth),double(nativeBody.mHeight)};
 const auto area=FusionLobberSplashCenter(root.x,root.y);int corrected=0,legacy=0,existing=0;bool outside=false;
 for(int i=1;i<g.count;++i)
 {
  auto* target=Target(a->mBoard,g.ids[i]);if(!Require(a,target!=nullptr,"Melon source geometry target disappeared"))return false;
  const auto r=target->GetZombieRect();const FusionLobberRect rect{double(r.mX),double(r.mY),double(r.mWidth),double(r.mHeight)};
  const bool hit=FusionLobberAreaTouchesRect(area.x,area.y,rect);corrected+=hit?1:0;legacy+=FusionLobberAreaTouchesRect(root.x,root.y,rect)?1:0;
  if(i==1||i==2)existing+=hit?1:0;if(i==3)outside=hit;
 }
 if(!Require(a,!FusionLobberAreaTouchesRect(root.x,root.y,body)&&FusionLobberAreaTouchesRect(area.x,area.y,body)&&corrected==3&&legacy==2&&existing==2&&!outside,"Melon offset-only BODY/existing-neighbor/outside ellipse preconditions differ"))return false;
 g.offsetProjectiles.insert(p->mNetId);++g.offsetProofs;g.offsetImpactPostTick=a->mBoard->mMainCounter+steps;
 LawnNet::NetTrace("SMOKE_FUSION_LOBBER_OFFSET_GEOMETRY role=host kind=32 projectile=%u target=%u primary=%u step=%d root=%.6f,%.6f area=%.6f,%.6f body=%d,%d,%d,%d legacyHit=0 correctedHit=1 candidates=3 legacyCandidates=2 existingNeighborHits=2 outsideHit=0 impactPostTick=%d positionOnly=1",
  p->mNetId,edge->mNetId,primary->mNetId,g.hits+1,root.x,root.y,area.x,area.y,nativeBody.mX,nativeBody.mY,nativeBody.mWidth,nativeBody.mHeight,g.offsetImpactPostTick);
 return true;
}
bool ObserveFlights(LawnApp* a)
{
 auto* b=a->mBoard;
 if(a->NetIsHost())for(auto& g:s.groups)if(g.active&&!g.done)
 {
  auto* p=ByRef(b,Source(g.kind));const auto* d=GetNativePlantData(p);if(!p||!d)continue;
  const auto* ctx=GetNativeContext(b);const auto key=uint32_t(b->mPlants.DataArrayGetID(p));auto aim=ctx->lobberAims.find(key);
  if(aim!=ctx->lobberAims.end()&&(s.triggers[g.kind].empty()||s.triggers[g.kind].back().tick!=PostFrameTick(aim->second.sampleTick)))
  {
   const auto* profile=FindFusionShooterTimingProfile(g.kind);int initial=0;if(!Require(a,profile&&TryGetFusionShooterShotCounter(*profile,d->animationSpeed,initial),"Lobber profile counter unavailable"))return false;
   auto* target=b->ZombieTryToGet(static_cast<ZombieID>(aim->second.targetGeneration));
   const int observedTrigger=PostFrameTick(aim->second.sampleTick);
   const bool valid=target&&p->mShootingCounter>0&&initial-p->mShootingCounter==b->mMainCounter-observedTrigger;
   if(!valid)LawnNet::NetTrace("SMOKE_FUSION_LOBBER_TRIGGER_MISMATCH kind=%d actor=%u initial=%d remaining=%d logicSampleTick=%d postFrameTrigger=%d postFrameNow=%d targetGeneration=%u targetPresent=%d",g.kind,p->mNetId,initial,p->mShootingCounter,aim->second.sampleTick,observedTrigger,b->mMainCounter,aim->second.targetGeneration,target?1:0);
   if(!Require(a,valid,"Lobber actual source trigger/identity counter differs"))return false;
   s.triggers[g.kind].push_back({observedTrigger,initial-1,aim->second.targetGeneration,aim->second.targetCenterX,aim->second.targetTopY});
   LawnNet::NetTrace("SMOKE_FUSION_LOBBER_TRIGGER kind=%d actor=%u token=%u index=%zu target=%u generation=%u tick=%d deadline=%d speed=%.9g B0=%.4f,%.4f nativeCounter=1 logicSampleTick=%d postUpdateEpoch=1",g.kind,p->mNetId,d->instanceToken,s.triggers[g.kind].size(),target->mNetId,aim->second.targetGeneration,observedTrigger,initial-1,d->animationSpeed,aim->second.targetCenterX,aim->second.targetTopY,aim->second.sampleTick);
  }
 }
 std::set<uint16_t> live;Projectile* p=nullptr;
 while(b->mProjectiles.IterateNext(p))
 {
  if(p->mDead)continue;const auto* d=GetNativeProjectileData(p);const auto* profile=d?FindFusionLobberProjectileProfile(d->kind):nullptr;
  if(!Require(a,profile&&IsNativeLobberProjectile(p)&&p->mMotionType==MOTION_LOBBED&&p->mProjectileType==profile->nativeType&&d->damage==profile->damage&&!d->sunOnHit&&!d->specialType&&!d->fireState,"Lobber actual projectile identity/type/payload differs"))return false;
   if(!PrepareOffsetBody(a,p,*profile))return false;
  float x=0,y=0;Sexy::Rect rect;if(!Require(a,GetNativeLobberProjectileCenter(p,x,y)&&GetNativeLobberProjectileRect(p,rect)&&rect.mWidth>0&&rect.mHeight>0&&std::abs(x-(p->mPosX+20))<.001f&&std::abs(y-(p->mPosY+p->mPosZ+20))<.001f,"Lobber real native center/collider differs"))return false;
  live.insert(p->mNetId);auto it=s.flights.find(p->mNetId);
  if(it==s.flights.end())
  {
   Flight record;record.kind=profile->kind;record.tick=record.firstSeen=b->mMainCounter;record.age=a->NetIsHost()?p->mProjectileAge:-1;record.x=p->mPosX;record.z=p->mPosZ;record.vx=p->mVelX;record.vz=p->mVelZ;record.acc=p->mAccZ;s.flights[p->mNetId]=record;
   if(a->NetIsHost())
   {
    // Age1 includes the birth update's integration; this is already the same
    // post-Board epoch as the converted trigger, not the native aim's raw tick.
    const int fireTick=b->mMainCounter-p->mProjectileAge+1;const Trigger* trigger=nullptr;for(const auto& t:s.triggers[profile->kind])if(t.tick+t.deadline==fireTick)trigger=&t;
    if(!Require(a,trigger&&p->mVelX>0&&std::abs(p->mAccZ-.08811377245508983)<.000001,"Lobber natural event/positive velocity/source gravity differs"))return false;
    auto* source=ByRef(b,Source(profile->kind));float sx=0,sy=0;if(!Require(a,source&&source->GetShadowCenter(sx,sy),"Lobber actual native muzzle anchor missing"))return false;
    const double muzzleX=sx+profile->muzzleX*(72.0/1.35),muzzleY=sy-profile->muzzleY*(100.0/1.67);
    const double expectedVX=(trigger->x-muzzleX)/150.0,expectedVZ=((trigger->y-muzzleY)-.5*14.715*(100.0/1.67)*1.5*1.5)/150.0;
    const double initialVZ=p->mVelZ-p->mAccZ*p->mProjectileAge;
    if(!Require(a,std::abs(p->mVelX-expectedVX)<.003&&std::abs(initialVZ-expectedVZ)<.003,"Lobber stationary captured-target1.5s source solution differs"))return false;
    LawnNet::NetTrace("SMOKE_FUSION_LOBBER_FIRE kind=%d id=%u bullet=%u damage=%d fireTick=%d triggerTick=%d deadline=%d age=%d vx=%.9g vz=%.9g acc=%.9g stationaryLead=1 nativeMotion=LOBBED",profile->kind,p->mNetId,d->kind,d->damage,fireTick,trigger->tick,trigger->deadline,p->mProjectileAge,p->mVelX,p->mVelZ,p->mAccZ);
   }
   LawnNet::NetTrace("SMOKE_FUSION_LOBBER_PROJECTILE role=%s kind=%d id=%u bullet=%u damage=%d motion=LOBBED x=%.4f y=%.4f Z=%.4f canonicalActor=1",Role(a),profile->kind,p->mNetId,d->kind,d->damage,p->mPosX,p->mPosY,p->mPosZ);
  }
  else if(a->NetIsHost())
  {
   auto& old=it->second;const int dt=b->mMainCounter-old.tick;if(dt>0)
   {
    const double expectedZ=old.z+old.vz*dt+.5*old.acc*dt*dt;
    if(!Require(a,p->mProjectileAge-old.age==dt&&std::abs(p->mPosX-old.x-old.vx*dt)<.03&&std::abs(p->mPosZ-expectedZ)<.03&&std::abs(p->mVelZ-old.vz-old.acc*dt)<.0001&&std::abs(p->mVelX-old.vx)<.0001,"Lobber native midpoint flight/age/gravity changed"))return false;
    ++old.samples;old.tick=b->mMainCounter;old.age=p->mProjectileAge;old.x=p->mPosX;old.z=p->mPosZ;old.vz=p->mVelZ;
   }
  }
  if(profile->kind==26&&b->mMainCounter-s.flights[p->mNetId].firstSeen>=35)Capture(a,"fusion_lobber_flight");
 }
 for(auto& [id,q]:s.flights)if(q.live&&!live.contains(id)){q.live=false;LawnNet::NetTrace("SMOKE_FUSION_LOBBER_RETIRE role=%s kind=%d id=%u lastAge=%d midpointSamples=%d observedTicks=%d",Role(a),q.kind,id,q.age,q.samples,b->mMainCounter-q.firstSeen);}
 return true;
}
bool ObserveWeak(LawnApp* a,Cohort& g)
{
 auto* z=Target(a->mBoard,g.ids[4]);const int now=a->mBoard->mMainCounter;
 if(z&&z->mBodyHealth<=0&&!s.weakDeathTick)
 {
  s.weakDeathTick=now;auto* body=a->ReanimationTryToGet(z->mBodyReanimID);s.weakAnim=body?body->mAnimTime:0;
  s.weakPositive=body&&!body->mDead&&body->mAnimRate>0;
  LawnNet::NetTrace("SMOKE_FUSION_LOBBER_DYING role=%s id=%u HP=%d phase=%d rate=%.9g animTime=%.9g positiveNativeRate=%d tick=%d fixtureDoesNotFreezeDying=1",Role(a),z->mNetId,z->mBodyHealth,int(z->mZombiePhase),body?body->mAnimRate:0,body?body->mAnimTime:0,s.weakPositive?1:0,now);
 }
 if(s.weakDeathTick&&!s.weakRetired)
 {
  if(!z)
  {if(!Require(a,now-s.weakDeathTick<400,"Winter post-lethal Add50 froze native death/retirement for4s"))return false;s.weakRetired=true;LawnNet::NetTrace("SMOKE_FUSION_LOBBER_DYING_RETIRED role=%s id=%u elapsed=%d positiveRate=%d animationProgress=%d retiredBefore400=1",Role(a),g.ids[4],now-s.weakDeathTick,s.weakPositive?1:0,s.weakProgress?1:0);}
  else
  {
   auto* body=a->ReanimationTryToGet(z->mBodyReanimID);if(body&&body->mAnimRate>0&&std::abs(body->mAnimTime-s.weakAnim)>.0001f)s.weakProgress=true;
   if(!Require(a,now-s.weakDeathTick<400,"Winter dying actor remained frozen beyond native death window"))return false;
  }
 }return true;
}
bool ObserveDamage(LawnApp* a,Cohort& g)
{
 if(!g.active)return true;auto* b=a->mBoard;if(a->NetIsClient()&&g.sampled==int(Samples(a)))return true;g.sampled=int(Samples(a));
 std::array<Health,5> current=g.before;for(int i=0;i<g.count;++i){auto* z=Target(b,g.ids[i]);if(!z){if(g.kind==1116&&i==4&&g.before[i].body<=0)continue;return Require(a,false,"Lobber canonical controlled live target disappeared");}current[i]=ReadHealth(z);}
 int index=g.kind==26?(g.hits==0?0:1):g.kind==1116&&g.hits<2?4:0;
 const bool changed=!Same(current[index],g.before[index]);
 if(!changed)
 {
  for(int i=0;i<g.count;++i)if(!Require(a,Same(current[i],g.before[i]),"Lobber outside/secondary target changed without primary hit"))return false;
  return g.kind!=1116||ObserveWeak(a,g);
 }
 if(!Require(a,!g.done,"Lobber emitted an unintended extra hit after bounded cohort"))return false;
 const int hit=++g.hits,damage=g.kind==26?40:g.kind==1116&&hit==5?320:80;
 for(int i=0;i<g.count;++i)
 {
   const bool primary=i==index,secondary=g.kind!=26&&(g.kind!=1116||hit>=3)&&(i==1||i==2||(g.kind==32&&i==4));
  const int body=g.before[i].body-current[i].body,helm=g.before[i].helm-current[i].helm,shield=g.before[i].shield-current[i].shield;
  int expectedBody=0,expectedHelm=0,expectedShield=0;
  if(primary){if(g.before[i].helm>0)expectedHelm=damage;else expectedBody=damage;if(g.kind==1116)expectedShield=(std::min)(g.before[i].shield,damage);}
  else if(secondary){expectedShield=(std::min)(g.before[i].shield,26);if(g.before[i].helm>0)expectedHelm=26;else expectedBody=26;}
  if(body!=expectedBody||helm!=expectedHelm||shield!=expectedShield)LawnNet::NetTrace("SMOKE_FUSION_LOBBER_DAMAGE_MISMATCH role=%s kind=%d hit=%d index=%d id=%u loss=%d,%d,%d expected=%d,%d,%d",Role(a),g.kind,hit,i,g.ids[i],body,helm,shield,expectedBody,expectedHelm,expectedShield);
  if(!Require(a,body==expectedBody&&helm==expectedHelm&&shield==expectedShield,"Lobber primary/Shieldless/NormalAll26/splash boundary ledger differs"))return false;
 }
 auto* primary=Target(b,g.ids[index]);const auto* cold=GetNativeZombieColdData(primary);
  if(g.kind==32)
  {
   if(a->NetIsHost()&&!Require(a,g.offsetProofs==hit&&b->mMainCounter>=g.offsetImpactPostTick,"Melon offset-only BODY hit lacked its real flight/ellipse precondition"))return false;
   LawnNet::NetTrace("SMOKE_FUSION_LOBBER_OFFSET_HIT role=%s kind=32 hit=%d target=%u body=%d damage=%d candidates=3 legacyWouldMiss=1 nativeBody=1 positionOnly=1",Role(a),hit,g.ids[4],current[4].body,g.before[4].body-current[4].body);
  }
 if(g.kind==1116)
 {
  const int level=hit==1||hit==3||hit==5?50:0,threshold=hit<=1||hit==3?100:200;
  if(!Require(a,primary&&cold&&cold->level==level&&cold->threshold==threshold&&(hit==2||hit>=4?primary->mIceTrapCounter>0:primary->mIceTrapCounter==0)&&primary->mChilledCounter>0,"Winter primary pre-damage frozen multiplier/postdamage50 ordering differs"))return false;
  if(hit>=3)for(int i:{1,2}){auto* z=Target(b,g.ids[i]);const auto* c=GetNativeZombieColdData(z);if(!Require(a,c&&c->level==(hit-2)*5&&c->threshold==100&&!z->mIceTrapCounter&&z->mChilledCounter>0,"Winter secondary pre-add5/IceAll slow through native armor differs"))return false;}
 }
  LawnNet::NetTrace("SMOKE_FUSION_LOBBER_HIT role=%s kind=%d hit=%d target=%u damage=%d body=%d helmet=%d shield=%d level=%d threshold=%d slow=%d freeze=%d splashCount=%d splashDamage=%d tick=%d noHPWrites=1",Role(a),g.kind,hit,g.ids[index],damage,current[index].body,current[index].helm,current[index].shield,cold?cold->level:0,cold?cold->threshold:100,primary?primary->mChilledCounter:0,primary?primary->mIceTrapCounter:0,g.kind==32?3:g.kind==1116&&hit>=3?2:0,g.kind!=26&&(g.kind!=1116||hit>=3)?26:0,b->mMainCounter);
 g.before=current;
 if(a->NetIsHost()&&g.kind==26&&hit==1){SetX(Target(b,g.ids[0]),1500);CenterX(Target(b,g.ids[1]),600);}
 const int required=g.kind==1116?5:2;
 if(hit==required){g.done=true;if(a->NetIsHost())Park(g,b);LawnNet::NetTrace("SMOKE_FUSION_LOBBER_COMBAT role=%s kind=%d hits=%d sourceSplash=1 controlsUntouched=1 sourceZombieAIUnported=1",Role(a),g.kind,hit);}
 return g.kind!=1116||ObserveWeak(a,g);
}
bool Observe(LawnApp* a)
{
 auto* b=a->mBoard;
 if(a->NetIsHost())for(const auto& g:s.groups)for(int i=0;i<g.count;++i)if(auto* z=Target(b,g.ids[i]))
  if(!z->IsDeadOrDying()&&z->mBodyHealth>0)z->ApplyAnimRate(0); // Never freeze the death regression actor.
 if(!ObserveFlights(a))return false;for(auto& g:s.groups)if(!ObserveDamage(a,g))return false;return true;
}
bool GroupDone(LawnApp* a,int kind)
{
 auto& g=Group(kind);if(!g.done)return false;
 for(const auto& [id,q]:s.flights)if(q.kind==kind&&q.live)return false;
 if(kind==26&&!s.frames.contains("fusion_lobber_flight"))return false;
 if(kind==1116&&!s.weakRetired)return false;
 return Business(a)&&Barrier(a,"FL147_DONE_"+std::to_string(kind));
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionLobber(LawnApp* a)
{
 const char* native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");const char* enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_LOBBER");
 if(!a||!native||std::strcmp(native,"1")||!enabled||std::strcmp(enabled,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
 if(a->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();if(GetTickCount()-s.since>45000U){char why[100];sprintf_s(why,"Lobber deadline stage=%d step=%d role=%s",s.stage,s.step,Role(a));a->CoopSmokeFail(why);return true;}
 if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
 if(s.stage==0)
 {
  if(a->NetIsHost()&&!s.configured){if(!Require(a,!a->mBoard&&!a->mSeedChooserScreen,"Lobber requires fresh lobby"))return true;a->mNetManager->SetDifficulty(1);s.configured=true;}
  if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
  if(a->NetIsClient()){Advance(a,1);Send(a,"FL147_START");}else if(Has(a,1,"FL147_START")){Advance(a,1);std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;
 }
 if(!a->mBoard||!IsNativeFusionBoard(a->mBoard)){if(s.stage==1&&a->NetIsClient())Send(a,"FL147_START");return true;}auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);auto* chooser=a->mSeedChooserScreen;const int slot=Slot(a);
 if(!Require(a,ctx&&ctx->validation&&!a->mFusionRuntime&&b->GetColumnCount()==10,"Lobber lost real native board"))return true;const auto& bank=ctx->selection.State();if(!Observe(a))return true;
 if(s.stage==1)
 {
  if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||!b->mCutScene->mSeedChoosing||chooser->mSeedsInFlight)return true;
  if(!Require(a,ctx->eligible.size()==26&&bank.targetCount==10&&bank.quotas[0]==5&&bank.quotas[1]==5&&bank.activeMask==3,"Lobber actual26pool/native10/shared quotas differ"))return true;if(!Capture(a,"fusion_lobber_chooser"))return true;
  std::vector<int> choices;for(size_t i=0;i<ctx->eligible.size();++i){const auto& e=ctx->eligible[i];if(slot==0?(!e.isExtra&&e.kind!=10):(e.isExtra||e.kind==10))choices.push_back(int(i));}
  auto priority=[&](int i){const auto& e=ctx->eligible[i];return slot==0?(e.kind==26?0:e.kind==32?1:10+e.kind):(!e.isExtra?0:10+e.kind);};std::stable_sort(choices.begin(),choices.end(),[&](int x,int y){return priority(x)<priority(y);});
  int own=0;for(const auto& p:bank.packets)own+=p.owner==slot;if(own<s.waitingOwn)return true;if(!Require(a,choices.size()>=5,"Lobber disjoint live chooser choices unavailable"))return true;
  if(own<5){int x=0,y=0;chooser->GetSeedPositionInChooser(choices[own],x,y);if(Click(a,float(chooser->mX+x+25),float(chooser->mY+y+35-int(chooser->mScrollbar->mScrollValue))))s.waitingOwn=own+1;return true;}
  if(bank.packets.size()!=10||!Packet(b,0,26)||!Packet(b,0,32)||!Packet(b,1,10))return true;int rank=-1;for(int i=0;i<b->mSeedBank->mNumPackets;++i){const int r=NetSeedBankOwnerRank(slot,b->mSeedBank->mSeedPackets[i].mNetOwnerSlot);if(!Require(a,r>=rank,"Lobber native bank lost own-first order"))return true;rank=r;}
  NetTrace("SMOKE_FUSION_LOBBER_POOL role=%s eligible=26 target=10 quota0=5 quota1=5 ownFirst=1 nativeChooser=1",Role(a));Advance(a,2);
 }
 else if(s.stage==2){if(bank.phase!=FusionBankPhase::Choosing)Advance(a,3);else if(chooser&&!chooser->mSeedsInFlight&&Ready(a,chooser->mStartButton))Advance(a,3);}
 else if(s.stage==3)
 {
  if(bank.phase==FusionBankPhase::Intro){if(!s.introStart)s.introStart=GetTickCount();s.introLast=GetTickCount();++s.introChecks;return true;}
  if(bank.phase!=FusionBankPhase::Playing||a->mGameScene!=SCENE_PLAYING||chooser)return true;if(!Require(a,s.introChecks>=10&&s.introLast-s.introStart>=1000&&b->mSunMoney==500,"Lobber skipped real full intro/fresh500"))return true;s.playingTick=b->mMainCounter;Advance(a,4);
 }
 else if(s.stage==4)
 {
  if(b->mMainCounter-s.playingTick<2)return true;if(a->NetIsHost()&&!s.ambient){if(!Require(a,!b->mPlants.mSize&&!b->mZombies.mSize&&b->mSunMoney==500,"Lobber initial empty scene differs"))return true;b->mSunCountDown=b->mZombieCountDown=b->mZombieCountDownStart=100000;s.ambient=true;}
  if(Barrier(a,"FL147_PLAYING"))Advance(a,5);
 }
 else if(s.stage==5)switch(s.step)
 {
 case 0:if(Purchase(a,0,26,4,26,500,400,s.cabbage))Next(a);break;
 case 1:if(SetupTargets(a,s.groups[0]))Next(a);break;
 case 2:if(Activate(a,s.groups[0]))Next(a);break;
 case 3:if(GroupDone(a,26))Next(a);break;
 case 4:if(Purchase(a,0,32,2,32,400,100,s.melon))Next(a);break;
 case 5:if(SetupTargets(a,s.groups[1]))Next(a);break;
 case 6:if(Activate(a,s.groups[1]))Next(a);break;
 case 7:if(GroupDone(a,32)&&Capture(a,"fusion_lobber_impact"))Next(a);break;
 case 8:if(Purchase(a,1,10,2,1116,100,25,s.winter))Next(a);break;
 case 9:if(SetupTargets(a,s.groups[2]))Next(a);break;
 case 10:if(Activate(a,s.groups[2]))Next(a);break;
 case 11:if(GroupDone(a,1116))Next(a);break;
 case 12:
  if(!Require(a,s.uses==std::array<int,3>{1,1,1}&&s.money==25&&s.weakRetired,"Lobber final real purchase/death proofs incomplete"))break;
  if(!Business(a)||!Capture(a,"fusion_lobber_complete")||!Barrier(a,"FL147_COMPLETE"))break;
  NetTrace("SMOKE_FUSION_LOBBER_COMPLETE role=%s kinds=26,32,1116 purchases=3 initialSun=500 totalCost=475 finalSun=25 uses=1,1,1 hits=2,2,5 nativeMidpoint=1 primaryShieldless=1 splash26=1 offsetOnlyBody=1 offsetCandidates=3 sourceCenter=1 winter80Then80Then320=1 lethal160Retired=1 nativeBoard=1 secondWorld=0 HPWritesAfterSetup=0 sunWrites=0 timerWrites=0 coldWrites=0 sourceZombieAIUnported=1",Role(a));Advance(a,6);a->CoopSmokeWriteResult(true,nullptr);break;
 }
 return true;
}
}
