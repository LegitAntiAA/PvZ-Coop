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
#include "../Fusion/FusionSplitShooterRules.h"
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
constexpr int kRow=2;
struct Ref {uint16_t id=0;uint32_t token=0;};
struct Volley {int tick=0;std::array<int,2> delay{};std::array<uint16_t,3> ids{};};
struct Shot {int tick=0,age=0,volley=0,spec=0;float x=0,y=0;uint8_t phase=0;bool retired=false;uint32_t birthTx=0,lastAliveTx=0;};
struct Combat
{
 int kind=0,row=0,exitRow=0,previousCounter=0,previousTick=0,hits=0,turns=0;
 int moveTick=0,moveLaunch=0,moveShoot=0,moveCD=0;NativePlantData moveData{};
 float rearRight=0;bool setup=false,started=false,swapped=false,outboundDone=false,dropQueued=false,moved=false,moveObserved=false,clientRear=false,clientReturn=false;
 std::array<uint16_t,5> targets{};std::vector<Volley> volleys;std::map<uint16_t,Shot> shots;
} ;
struct State
{
 int stage=0,waitingOwn=0,money=1000,normalUses=0,extraUses=0,cherryUses=0,nutUses=0,puffUses=0,playingTick=-1;
 int passiveMoveTick=0,passiveCD=0,passiveHP=0,passiveLaunch=0;NativePlantData passiveData{};
 uint32_t since=0,lastChat=0,introStart=0,introLast=0,introChecks=0,pickSamples=0,purchaseSamples=0;
 bool configured=false,clicked=false,placed=false,observed=false,toolClicked=false,budget=false,budgetWritten=false,biterSetup=false;
 Ref current{},puffBase{},wallnut{},passive{};uint16_t eater=0;Combat combat{};
 std::map<std::string,int> firstDraw;std::set<std::string> frames;
} s;
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool ok,const char* why){if(!ok)a->CoopSmokeFail(why);return ok;}
void Advance(LawnApp* a,int stage)
{
    s.stage=stage;s.since=GetTickCount();s.clicked=s.placed=s.observed=s.toolClicked=false;
    LawnNet::NetTrace("SMOKE_FUSION_SPLIT_STAGE role=%s stage=%d",Role(a),stage);
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
    if(!Require(a,a->CoopSmokeCaptureFrame(name),"Split native renderer capture failed"))return false;
    s.frames.insert(name);LawnNet::NetTrace("SMOKE_FUSION_SPLIT_RENDER role=%s frame=%s freshDraws=%d",Role(a),name,a->mDrawCount-s.firstDraw[name]);return true;
}
bool Click(LawnApp* a,float x,float y,uint8_t button=SDL_BUTTON_LEFT,bool click=true)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    if(!click)return Require(a,SDL_PushEvent(&m),"Split scoped motion failed");
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=button;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;
    return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Split scoped SDL click failed");
}
bool BoardClick(LawnApp* a,float x,float y,uint8_t button=SDL_BUTTON_LEFT,bool click=true)
{return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY,button,click);}
bool Cell(LawnApp* a,int col,int row=kRow,uint8_t button=SDL_BUTTON_LEFT,bool click=true)
{return BoardClick(a,float(a->mBoard->GetCellCenterX(col,row)),float(a->mBoard->GridToPixelY(col,row)+50),button,click);}
bool Ready(LawnApp* a,GameButton* b)
{
    if(!b||b->mDisabled||b->mBtnNoDraw)return false;float x=b->mX+b->mWidth*.5f,y=b->mY+b->mHeight*.5f;
    if(!b->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"Split Ready hover failed");return false;}
    LawnNet::NetTrace("SMOKE_FUSION_SPLIT_READY role=%s nativeSDL=1 hoverObserved=1",Role(a));return Click(a,x,y);
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
{if(!Require(a,p&&p->mActive&&!p->mRefreshing,"Split real packet not ready"))return false;auto* b=a->mBoard->mSeedBank;return BoardClick(a,float(b->mX+p->mX+p->mOffsetX+p->mWidth/2),float(b->mY+p->mY+p->mHeight/2));}
bool Business(LawnApp* a)
{
 auto* b=a->mBoard;auto* normal=Packet(b,0,0,false);auto* extra=Packet(b,1,0,true);auto* cherry=Packet(b,1,2,false);auto* nut=Packet(b,1,3,false);auto* puff=Packet(b,0,6,false);
 if(!Require(a,normal&&extra&&cherry&&nut&&puff&&b->mSunMoney==s.money&&normal->mRefreshTime==(s.normalUses?750:0)&&extra->mRefreshTime==(s.extraUses?750:0)&&cherry->mRefreshTime==(s.cherryUses?5000:0)&&nut->mRefreshTime==(s.nutUses?3000:0)&&puff->mRefreshTime==(s.puffUses?750:0)&&
  (!a->NetIsHost()||(normal->mTimesUsed==s.normalUses&&extra->mTimesUsed==s.extraUses&&cherry->mTimesUsed==s.cherryUses&&nut->mTimesUsed==s.nutUses&&puff->mTimesUsed==s.puffUses)),"Split real prices/packet identity/cooldown conservation failed"))return false;
 Coin* c=nullptr;while(b->mCoins.IterateNext(c))if(!Require(a,c->mDead,"Split/passive unexpectedly generated Coin/sun"))return false;return true;
}
bool Purchase(LawnApp* a,int buyer,int kind,bool extra,int col,int row,int result,int before,int after,int first,int second,Ref& out,const char* gate)
{
 auto* b=a->mBoard;auto* card=Packet(b,buyer,kind,extra);
 if(Slot(a)==buyer&&!s.clicked){if(!card||!card->mActive||card->mRefreshing)return false;s.pickSamples=Samples(a);if(PickPacket(a,card))s.clicked=true;return false;}
 if(Slot(a)==buyer&&!s.placed){if(!HeldPacket(b,card)||(a->NetIsClient()&&Samples(a)<=s.pickSamples))return false;if(!Require(a,b->mSunMoney==before,"Split native pickup charged before planting"))return false;s.placed=Cell(a,col,row);return false;}
 if(!s.observed)
 {
  auto* p=At(b,col,row);const auto* d=GetNativePlantData(p);if(!d||d->kind!=result)return false;const int hp=result==3?4000:300;
  if(!Require(a,card&&d->instanceToken&&d->owner==buyer&&d->packetId==card->mNetId&&!d->randomOrigin&&d->firstParent==first&&d->secondParent==second&&p->mPlantHealth==hp&&b->mSunMoney==after,"Split actual purchase/recipe/HP/provenance differs"))return false;
  out=Identity(p);s.money=after;if(kind==6)++s.puffUses;else if(kind==2)++s.cherryUses;else if(kind==3)++s.nutUses;else if(extra)++s.extraUses;else ++s.normalUses;s.purchaseSamples=Samples(a);s.observed=true;
 }
 if(!Business(a)||!Barrier(a,gate))return false;
 LawnNet::NetTrace("SMOKE_FUSION_SPLIT_PURCHASE role=%s kind=%d id=%u token=%u packet=%u owner=%d extra=%d parents=%d,%d sun=%d nativeSDL=1",Role(a),result,out.id,out.token,card->mNetId,buyer,extra?1:0,first,second,after);return true;
}
bool Pickup(LawnApp* a,Ref ref)
{
 auto* b=a->mBoard;auto* p=ByRef(b,ref);if(!Require(a,p,"Split glove source vanished"))return false;
 if(Slot(a)==1)
 {
  if(!s.toolClicked){Sexy::Rect rect=b->GetShovelButtonRect();b->GetZenButtonRect(OBJECT_TYPE_GLOVE,rect);s.toolClicked=BoardClick(a,float(rect.mX+rect.mWidth/2),float(rect.mY+rect.mHeight/2));return false;}
  if(!s.clicked){if(b->mCursorObject->mCursorType!=CURSOR_TYPE_GLOVE)return false;s.clicked=Cell(a,p->mPlantCol,p->mRow);return false;}
 }
 const auto& h=GetNativeContext(b)->gloveHolds[1];return h.plantId==ref.id&&h.instanceToken==ref.token&&GetNativeGloveHeldPlant(b,1)==p&&(Slot(a)!=1||b->mCursorObject->mCursorType==CURSOR_TYPE_PLANT_FROM_GLOVE);
}
Zombie* Target(Board* b,uint16_t id){Zombie* z=nullptr;while(b->mZombies.IterateNext(z))if(z->mNetId==id&&!z->mDead)return z;return nullptr;}
Projectile* ShotById(Board* b,uint16_t id){Projectile* p=nullptr;while(b->mProjectiles.IterateNext(p))if(!p->mDead&&p->mNetId==id)return p;return nullptr;}
int LiveShots(Board* b){int n=0;Projectile* p=nullptr;while(b->mProjectiles.IterateNext(p))n+=!p->mDead;return n;}
void SetX(Zombie* z,float x){z->mPosX=x;z->mX=int(x);}
void Right(Zombie* z,float right){const auto r=z->GetZombieRect();SetX(z,z->mPosX+right-r.mX-r.mWidth);}
void Left(Zombie* z,float left){const auto r=z->GetZombieRect();SetX(z,z->mPosX+left-r.mX);}
bool SetupTargets(LawnApp* a)
{
 auto& q=s.combat;for(int i=0;i<5;++i)
 {
  auto* z=a->mBoard->AddZombieInRow(ZOMBIE_NORMAL,i==4?q.row+1:q.row,0);auto* body=z?a->ReanimationTryToGet(z->mBodyReanimID):nullptr;const float rate=body?body->mAnimRate:0;
  if(!Require(a,z&&z->mNetId&&body&&std::isfinite(rate)&&rate>0,"Split native target setup failed"))return false;
  if(i==1||i==2){z->mHelmType=HELMTYPE_BUCKET;z->AttachHelmet();}if(i<=2){z->mShieldType=SHIELDTYPE_DOOR;z->AttachShield();}
  z->mBodyHealth=z->mBodyMaxHealth=5000;z->mHelmHealth=z->mHelmMaxHealth=i==1||i==2?1000:0;z->mShieldHealth=z->mShieldMaxHealth=i<=2?1000:0;
  z->mMindControlled=i==3;z->mPosY=z->GetPosYBasedOnRow(z->mRow);z->mY=int(z->mPosY);z->mVelX=0;z->SetAnimRate(rate);z->ApplyAnimRate(0);SetX(z,i==3?650.0f:1100.0f+100*i);q.targets[i]=z->mNetId;
  LawnNet::NetTrace("SMOKE_FUSION_SPLIT_TARGET kind=%d index=%d id=%u row=%d body=5000 helm=%d shield=%d MC=%d initialHPOnly=1 boundedNativeNormal=1 frozenGround=1",q.kind,i,z->mNetId,z->mRow,z->mHelmHealth,z->mShieldHealth,i==3?1:0);
 }
 q.setup=true;return true;
}
std::string Ids(){std::ostringstream out;out<<"FS100_IDS "<<s.combat.kind;for(auto id:s.combat.targets)out<<' '<<id;return out.str();}
bool ReadIds(LawnApp* a)
{
 auto& q=s.combat;if(a->NetIsHost())return q.setup;
 for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==0&&line.mText.rfind("FS100_IDS ",0)==0)
 {
  std::istringstream in(line.mText.substr(10));int kind=0;if(!(in>>kind)||kind!=q.kind)continue;std::set<unsigned> seen;
  for(auto& id:q.targets){unsigned n=0;if(!Require(a,bool(in>>n)&&n&&n<=65535&&seen.insert(n).second,"Split controlled target IDs invalid"))return false;id=uint16_t(n);}std::string extra;return Require(a,!(in>>extra),"Split controlled target IDs trailing data");
 }
 return false;
}
bool PrepareCombat(LawnApp* a,int kind,int exitRow)
{
 auto* b=a->mBoard;auto* p=ByRef(b,s.current);const auto* d=GetNativePlantData(p);if(!Require(a,p&&d&&d->kind==kind&&p->mPlantHealth==300,"Split preparation source missing"))return false;
 if(s.combat.kind!=kind){s.combat={};s.combat.kind=kind;s.combat.row=p->mRow;s.combat.exitRow=exitRow;}
 auto& q=s.combat;const auto* ctx=GetNativeContext(b);if(ctx->gloveCooldownTicks&&!ctx->gloveHolds[1].plantId)return false;
 if(!Business(a)||!Pickup(a,s.current))return false;const std::string held="fusion_split_held"+std::to_string(kind);
 if(!Capture(a,held.c_str()))return false;if(a->NetIsHost()&&!q.setup&&!SetupTargets(a))return false;if(a->NetIsHost()&&!Send(a,Ids()))return false;if(!ReadIds(a))return false;
 for(auto id:q.targets)if(!Target(b,id))return false;if(!Barrier(a,"FS100_TARGETS_"+std::to_string(kind)))return false;
 if(!q.started)
 {
  q.previousCounter=p->mShootingCounter;q.previousTick=b->mMainCounter;q.started=true;
  if(a->NetIsHost())
  {
   float gx=0,gy=0;if(!Require(a,p->GetShadowCenter(gx,gy),"Split native shadow anchor missing"))return false;
   const float rear=gx+(kind==1153?-.2985f:-.9703125f)*60,forward=gx+(kind==1090?.514f:kind==1108?.414f:.204f)*60;
   // Keep outbound controls far enough for an actual client snapshot sample.
   // Pea0's native55px box otherwise makes this contact last only~15ticks.
   q.rearRight=rear-240;Right(Target(b,q.targets[0]),q.rearRight);Left(Target(b,q.targets[2]),forward+100);
   LawnNet::NetTrace("SMOKE_FUSION_SPLIT_ACTIVE kind=%d row=%d exitRow=%d rearRight=%.3f forwardLeft=%.3f controlledPositions=1 noHealthTimerWrites=1 heldOriginalActive=1",kind,q.row,q.exitRow,q.rearRight,forward+100);
  }
 }
 return true;
}
bool ObserveCombat(LawnApp* a)
{
 if(!Business(a))return false;auto* b=a->mBoard;auto& q=s.combat;auto* p=ByRef(b,s.current);const auto* d=GetNativePlantData(p);const int payload=q.kind==1090?0:q.kind==1108?1:10,damage=q.kind==1108?60:20;
 if(!Require(a,p&&d&&d->kind==q.kind&&p->mPlantHealth==300&&p->mIsOnBoard&&!p->NotOnGround(),"Split held/moved source stopped being a real live actor"))return false;
 if(a->NetIsHost())
 {
  if(p->mShootingCounter>q.previousCounter)
  {
   const std::array<float,2> events=q.kind==1153?std::array<float,2>{.38333333f,.6333333f}:std::array<float,2>{.6666667f,1.0f};const double speed=(q.kind==1153?1.0:2.0)*double(d->animationSpeed);
   std::array<int,2> deadlines{};for(int i=0;i<2;++i)deadlines[i]=int(std::ceil(double(events[i])/speed*100));const int initial=deadlines[1]+1,dt=b->mMainCounter-q.previousTick;
   if(!Require(a,q.volleys.size()<2&&(q.volleys.empty()||(q.volleys.back().ids[0]&&q.volleys.back().ids[1]&&q.volleys.back().ids[2]))&&p->mShootingCounter<=initial&&initial-p->mShootingCounter<=(std::max)(1,dt),"Split natural two-group/three-emission counter differs"))return false;
   const int tick=b->mMainCounter-(initial-p->mShootingCounter);q.volleys.push_back({tick,deadlines,{}});
   LawnNet::NetTrace("SMOKE_FUSION_SPLIT_TRIGGER kind=%d volley=%zu tick=%d initial=%d first=%d second=%d speed=%.9g groups=2 physicalPerVolley=3 naturalClock=1",q.kind,q.volleys.size(),tick,initial,deadlines[0],deadlines[1],d->animationSpeed);
  }
  q.previousCounter=p->mShootingCounter;q.previousTick=b->mMainCounter;
 }
 Projectile* shot=nullptr;while(b->mProjectiles.IterateNext(shot))
 {
  if(shot->mDead)continue;const auto* meta=GetNativeProjectileData(shot);float cx=0,cy=0;
  if(!Require(a,meta&&meta->kind==payload&&meta->damage==damage&&!meta->sunOnHit&&meta->specialType<=2&&shot->mMotionType==(meta->specialType==1?MOTION_BACKWARDS:MOTION_STRAIGHT)&&GetNativeTimedPeaProjectileCenter(shot,cx,cy),"Split source payload/phase/motion/native actor malformed"))return false;
  auto it=q.shots.find(shot->mNetId);const bool firstProjection=it==q.shots.end();const bool phaseChanged=!firstProjection&&it->second.phase!=meta->specialType;
  const auto actualRect=shot->GetProjectileRect();
  if(!Require(a,actualRect.mWidth>0&&actualRect.mHeight>0,"Split native projectile collision rectangle empty"))return false;
  if(payload!=0)
  {
   const double width=payload==1?2.13*.2*(72.0/1.35):.24274449*(72.0/1.35),height=payload==1?1.9312102*.2*(100.0/1.67):.35107538*(100.0/1.67);
   const double ox=payload==1?0:.0007451102*(72.0/1.35),oy=payload==1?.025604904*.2*(100.0/1.67):.051929936*(100.0/1.67);
   const int left=int(std::floor(cx+ox-width/2)),top=int(std::floor(cy+oy-height/2));
   if(!Require(a,actualRect.mX==left&&actualRect.mY==top&&actualRect.mWidth==int(std::ceil(cx+ox+width/2))-left&&actualRect.mHeight==int(std::ceil(cy+oy+height/2))-top,"Split front/rear/returned source BoxCollider projection differs"))return false;
  }
  if(firstProjection||phaseChanged)LawnNet::NetTrace("SMOKE_FUSION_SPLIT_RECT role=%s kind=%d id=%u phase=%u motion=%d x=%d y=%d w=%d h=%d policy=%s",Role(a),q.kind,shot->mNetId,meta->specialType,int(shot->mMotionType),actualRect.mX,actualRect.mY,actualRect.mWidth,actualRect.mHeight,payload==0?"nativePeaAdaptation":"sourceBox");
  if(a->NetIsHost())
  {
   if(it==q.shots.end())
   {
    const int birth=b->mMainCounter-shot->mProjectileAge+1;int volley=-1,event=-1;for(size_t v=0;v<q.volleys.size();++v)for(int e=0;e<2;++e)if(q.volleys[v].tick+q.volleys[v].delay[e]==birth){volley=int(v);event=e;}
    const int spec=event==0?(meta->specialType==1?0:1):2;
    if(!Require(a,volley>=0&&event>=0&&meta->specialType==(spec==1?0:1)&&!q.volleys[volley].ids[spec]&&q.shots.size()<6&&shot->mRow==q.row,"Split event-to-emission identity/order differs"))return false;
    q.volleys[volley].ids[spec]=shot->mNetId;it=q.shots.emplace(shot->mNetId,Shot{b->mMainCounter,shot->mProjectileAge,volley,spec,cx,cy,meta->specialType,false}).first;
    it->second.birthTx=it->second.lastAliveTx=Samples(a);
    float gx=0,gy=0;const bool rear=spec!=1;const float mx=rear?(q.kind==1153?-.2985f:-.9703125f):(q.kind==1090?.514f:q.kind==1108?.414f:.204f),my=rear?(q.kind==1153?.306f:.600305796f):(q.kind==1153?.08175f:.6003f);
    if(!Require(a,p->GetShadowCenter(gx,gy)&&std::abs(cx-(gx+mx*60+(rear?-3.2f:3.2f)*shot->mProjectileAge))<.05f&&std::abs(cy-(gy-my*60))<.05f,"Split nominal source event muzzle/native-ground adaptation differs"))return false;
    LawnNet::NetTrace("SMOKE_FUSION_SPLIT_SHOT kind=%d id=%u volley=%d event=%d spec=%d born=%d trigger=%d delay=%d payload=%d damage=%d phase=%u motion=%d nativeActor=1",q.kind,shot->mNetId,volley+1,event+1,spec,birth,q.volleys[volley].tick,q.volleys[volley].delay[event],payload,damage,meta->specialType,int(shot->mMotionType));
   }
   auto& old=it->second;const int dt=b->mMainCounter-old.tick;const double bound=76.0+(-10.0+4.8)*(72.0/1.35);const float expectedVelocity=old.phase==1?-3.2f:3.2f;
   if(!Require(a,!old.retired&&dt>=0&&dt<=1&&shot->mProjectileAge-old.age==dt&&std::abs(shot->mVelX-expectedVelocity)<.0001f&&std::abs(cx-old.x-expectedVelocity*dt)<.03f,"Split actual host old-motion step/age drifted"))return false;
   if(meta->specialType!=old.phase)
   {
    if(!Require(a,old.phase==1&&meta->specialType==2&&dt==1&&old.x<bound&&cx<old.x&&cx<bound&&shot->mVelX<0&&shot->mMotionType==MOTION_STRAIGHT,"Split turn clamped/refreshed velocity/age or used newX"))return false;++q.turns;
    LawnNet::NetTrace("SMOKE_FUSION_SPLIT_TURN kind=%d id=%u oldX=%.9g newX=%.9g bound=%.9g ageBefore=%d ageAfter=%d vx=%.9g phase=2 motion=%d oldNegativeStep=1 noClamp=1 ageNotReset=1",q.kind,shot->mNetId,old.x,cx,bound,old.age,shot->mProjectileAge,shot->mVelX,int(shot->mMotionType));
   }
   else if(dt&&old.phase==1&&!Require(a,old.x>=bound,"Split outbound failed to turn on oldX crossing"))return false;
   const unsigned flags=shot->GetDamageFlags(Target(b,q.targets[2]));const unsigned expectedFlags=meta->specialType==1&&payload!=10?1U:0U;
   if(!Require(a,flags==expectedFlags&&!NativeTimedPeaProjectileCanHit(shot,Target(b,q.targets[3]))&&!NativeTimedPeaProjectileCanHit(shot,Target(b,q.targets[4])),"Split actual damage flags/MC/row filters differ"))return false;
   if(old.tick!=b->mMainCounter||old.age==shot->mProjectileAge){old.tick=b->mMainCounter;old.age=shot->mProjectileAge;old.x=cx;old.y=cy;old.phase=meta->specialType;old.lastAliveTx=Samples(a);}
  }
  else
  {
   if(it==q.shots.end())it=q.shots.emplace(shot->mNetId,Shot{0,0,0,0,cx,cy,meta->specialType,false}).first;
   if(firstProjection||phaseChanged)LawnNet::NetTrace("SMOKE_FUSION_SPLIT_PHASE role=client kind=%d id=%u phase=%u motion=%d x=%.6f y=%.6f hostAgeVelocityNotSerialized=1",q.kind,shot->mNetId,meta->specialType,int(shot->mMotionType),cx,cy);
   it->second.phase=meta->specialType;it->second.x=cx;it->second.y=cy;
  }
  // Capture during the long second MISS volley, after short first contacts.
  // Synchronous BMP I/O must not hide a transient first-volley client sample.
  if(meta->specialType==1&&q.shots.size()>=4&&cx>40&&cx<760){q.clientRear=true;const std::string frame="fusion_split_rear"+std::to_string(q.kind);Capture(a,frame.c_str());}
  if(meta->specialType==2&&cx>40&&cx<760){q.clientReturn=true;const std::string frame="fusion_split_returned"+std::to_string(q.kind);Capture(a,frame.c_str());}
 }
 auto* aa=Target(b,q.targets[0]);auto* bb=Target(b,q.targets[1]);auto* cc=Target(b,q.targets[2]);if(!Require(a,aa&&bb&&cc&&Target(b,q.targets[3])&&Target(b,q.targets[4]),"Split fixture control retired unexpectedly"))return false;
 const int lossA=payload==10?1000-aa->mShieldHealth:5000-aa->mBodyHealth,lossB=payload==10?1000-bb->mShieldHealth:1000-bb->mHelmHealth,lossC=1000-cc->mShieldHealth;
 if(!Require(a,lossA>=0&&lossA%damage==0&&lossA/damage<=1&&lossB>=0&&lossB%damage==0&&lossB/damage<=1&&lossC>=0&&lossC%damage==0&&lossC/damage<=4&&
  aa->mHelmHealth==0&&(payload==10?aa->mBodyHealth==5000:aa->mShieldHealth==1000)&&bb->mBodyHealth==5000&&(payload==10?bb->mHelmHealth==1000:bb->mShieldHealth==1000)&&
  cc->mHelmHealth==1000&&cc->mBodyHealth==5000&&Target(b,q.targets[3])->mBodyHealth==5000&&Target(b,q.targets[4])->mBodyHealth==5000,"Split outbound/returned armor routing or noAoE controls differ"))return false;
 q.hits=(lossA+lossB+lossC)/damage;
 if(a->NetIsHost())
 {
  if(lossA==damage&&!q.swapped){SetX(aa,1100);Right(bb,q.rearRight);q.swapped=true;LawnNet::NetTrace("SMOKE_FUSION_SPLIT_OUTBOUND kind=%d case=shieldBody body=%d helm=0 shield=%d damage=%d flags=%u positionOnlySwap=1",q.kind,aa->mBodyHealth,aa->mShieldHealth,damage,payload==10?0:1);}
  if(lossB==damage&&!q.outboundDone){SetX(bb,1200);q.outboundDone=true;LawnNet::NetTrace("SMOKE_FUSION_SPLIT_OUTBOUND kind=%d case=helmetShield body=5000 helm=%d shield=%d damage=%d flags=%u secondVolleyRearMiss=1",q.kind,bb->mHelmHealth,bb->mShieldHealth,damage,payload==10?0:1);}
  int retired=0;for(auto& [id,state]:q.shots)if(!ShotById(b,id))
  {
   if(!state.retired)
   {
    const int birth=q.volleys[state.volley].tick+q.volleys[state.volley].delay[state.spec==2?1:0];
    LawnNet::NetTrace("SMOKE_FUSION_SPLIT_RETIRE kind=%d id=%u volley=%d spec=%d birthTick=%d retireTick=%d lastAliveTick=%d lastAliveAge=%d lifetimeTicks=%d birthTx=%u lastAliveTx=%u txWhileObservedAlive=%u phaseAtLastAlive=%u",
     q.kind,id,state.volley+1,state.spec,birth,b->mMainCounter,state.tick,state.age,b->mMainCounter-birth+1,state.birthTx,state.lastAliveTx,state.lastAliveTx-state.birthTx,state.phase);
   }
   state.retired=true;++retired;
  }
  if(!Require(a,retired==q.hits,"Split projectile retired without one exact armor/body hit"))return false;
  if(p->mRow==q.row){q.moveTick=b->mMainCounter;q.moveLaunch=p->mLaunchCounter;q.moveShoot=p->mShootingCounter;q.moveData=*d;}
  if(q.shots.size()==6&&!Send(a,"FS100_DROP_"+std::to_string(q.kind)))return true;
 }
 if(Slot(a)==1&&Has(a,0,"FS100_DROP_"+std::to_string(q.kind))&&!q.dropQueued){q.dropQueued=Cell(a,5,q.exitRow);}
 if(p->mRow==q.exitRow&&!q.moveObserved)
 {
  const int dt=a->NetIsHost()?b->mMainCounter-q.moveTick:0;const int expectedCD=q.kind==1153?334:1000;const auto* ctx=GetNativeContext(b);
  if(!Require(a,!ctx->gloveHolds[1].plantId&&ctx->gloveCooldownTicks>0&&ctx->gloveCooldownTicks<=expectedCD,"Split UI move did not clear hold/start final-kind cooldown"))return false;
  if(a->NetIsHost()&&!Require(a,dt>=0&&dt<q.moveLaunch&&p->mLaunchCounter==q.moveLaunch-dt&&p->mShootingCounter==(std::max)(0,q.moveShoot-dt)&&d->owner==q.moveData.owner&&d->packetId==q.moveData.packetId&&d->instanceToken==q.moveData.instanceToken&&d->animationSpeed==q.moveData.animationSpeed&&d->firstParent==q.moveData.firstParent&&d->secondParent==q.moveData.secondParent&&ctx->gloveCooldownTicks>=expectedCD-dt,"Split held-source move reinitialized/paused native fields"))return false;
  q.moveObserved=true;q.moveCD=ctx->gloveCooldownTicks;LawnNet::NetTrace("SMOKE_FUSION_SPLIT_MOVE role=%s kind=%d id=%u token=%u fromRow=%d toRow=%d HP=300 sameInstance=1 countersPreserved=1 provenancePreserved=1 expectedCD=%d observedCD=%d",Role(a),q.kind,s.current.id,s.current.token,q.row,q.exitRow,expectedCD,q.moveCD);
 }
 if(a->NetIsHost()&&q.hits==6)
 {
  if(!Require(a,q.volleys.size()==2&&q.shots.size()==6&&q.turns==2&&q.moveObserved&&q.volleys[0].ids[0]<q.volleys[0].ids[1]&&q.volleys[1].ids[0]<q.volleys[1].ids[1],"Split two volleys/order/two return turns/native move incomplete"))return false;
  Left(cc,1300);
 }
 return true;
}
bool SetupBiter(LawnApp* a)
{
 auto* b=a->mBoard;auto* p=ByRef(b,s.passive);auto* z=b->AddZombieInRow(ZOMBIE_NORMAL,2,0);auto* body=z?a->ReanimationTryToGet(z->mBodyReanimID):nullptr;const float rate=body?body->mAnimRate:0;
 if(!Require(a,p&&p->mPlantHealth==2000&&z&&z->mNetId&&body&&rate>0,"PuffNut initial2000/native biter setup failed"))return false;z->mBodyHealth=z->mBodyMaxHealth=5000;z->SetAnimRate(rate);z->mPosY=z->GetPosYBasedOnRow(2);z->mY=int(z->mPosY);bool found=false;
 for(int offset=80;offset>=-80;--offset){z->mPosX=float(p->mX+offset);z->mX=int(z->mPosX);if(z->FindPlantTarget(ATTACKTYPE_CHEW)==p){found=true;break;}}
 if(!Require(a,found,"PuffNut actual native bite target unavailable"))return false;s.eater=z->mNetId;s.biterSetup=true;LawnNet::NetTrace("SMOKE_FUSION_SPLIT_PASSIVE_BITER id=%u plant=%u token=%u initialPlantHP=2000 initialBiterHP=5000 noRootCrackStates=1 nativeEatPlant=1",s.eater,s.passive.id,s.passive.token);return true;
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionSplit(LawnApp* a)
{
 const char* native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE"),*enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_SPLIT");
 if(!a||!native||std::strcmp(native,"1")||!enabled||std::strcmp(enabled,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
 if(a->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();if(GetTickCount()-s.since>(s.stage<=3?60000U:45000U)){char error[90];sprintf_s(error,"Split deadline stage=%d role=%s",s.stage,Role(a));a->CoopSmokeFail(error);return true;}
 if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
 if(s.stage==0)
 {
  if(a->NetIsHost()&&!s.configured){if(!Require(a,!a->mBoard&&!a->mSeedChooserScreen,"Split requires clean native lobby"))return true;a->mNetManager->SetDifficulty(1);s.configured=true;}
  if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;if(a->NetIsClient()){Advance(a,1);Send(a,"FS100_START");}else if(Has(a,1,"FS100_START")){Advance(a,1);std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;
 }
 if(!a->mBoard||!IsNativeFusionBoard(a->mBoard)){if(s.stage==1&&a->NetIsClient())Send(a,"FS100_START");return true;}auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);auto* chooser=a->mSeedChooserScreen;const int slot=Slot(a);
 if(!Require(a,ctx&&ctx->validation&&!a->mFusionRuntime&&b->GetColumnCount()==10,"Split lost actual native Board"))return true;const auto& bank=ctx->selection.State();if((s.stage==9||s.stage==12||s.stage==18)&&!ObserveCombat(a))return true;
 if(s.stage==1)
 {
  if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||!b->mCutScene->mSeedChoosing||chooser->mSeedsInFlight)return true;const size_t target=(std::min)(kNativeFusionBankCapacity,ctx->eligible.size());
  if(!Require(a,ctx->eligible.size()>=16&&bank.targetCount==target&&bank.activeMask==3&&bank.quotas[0]+bank.quotas[1]==target,"Split live pool/capacity/quota differs"))return true;if(!Capture(a,"fusion_split_chooser"))return true;
  std::vector<int> choices;for(size_t i=0;i<ctx->eligible.size();++i){const auto& e=ctx->eligible[i];if(slot==0?(!e.isExtra&&e.kind!=2&&e.kind!=3):(e.isExtra||(e.kind==2||e.kind==3)))choices.push_back(int(i));}
  auto priority=[&](int i){const auto& e=ctx->eligible[i];return slot==0?(e.kind==0?0:e.kind==6?1:10+e.kind):(e.kind==0&&e.isExtra?0:e.kind==2&&!e.isExtra?1:e.kind==3&&!e.isExtra?2:10+e.kind);};std::stable_sort(choices.begin(),choices.end(),[&](int x,int y){return priority(x)<priority(y);});
  int own=0;for(const auto& packet:bank.packets)own+=packet.owner==slot;if(own<s.waitingOwn)return true;if(!Require(a,choices.size()>=bank.quotas[slot],"Split disjoint chooser candidates missing"))return true;
  if(own<bank.quotas[slot]){int x=0,y=0;chooser->GetSeedPositionInChooser(choices[own],x,y);if(Click(a,float(chooser->mX+x+25),float(chooser->mY+y+35-int(chooser->mScrollbar->mScrollValue))))s.waitingOwn=own+1;return true;}
  if(bank.packets.size()!=target||!Packet(b,0,0,false)||!Packet(b,0,6,false)||!Packet(b,1,0,true)||!Packet(b,1,2,false)||!Packet(b,1,3,false))return true;int rank=-1;
  for(int i=0;i<b->mSeedBank->mNumPackets;++i){const int next=NetSeedBankOwnerRank(slot,b->mSeedBank->mSeedPackets[i].mNetOwnerSlot);if(!Require(a,next>=rank,"Split native bank lost own-first layout"))return true;rank=next;}
  if(!Capture(a,"fusion_split_picked"))return true;NetTrace("SMOKE_FUSION_SPLIT_POOL role=%s eligible=%zu target=%u quota0=%u quota1=%u ownFirst=1 unfiltered=1 nativeChooser=1",Role(a),ctx->eligible.size(),bank.targetCount,bank.quotas[0],bank.quotas[1]);Advance(a,2);
 }
 else if(s.stage==2){if(bank.phase!=FusionBankPhase::Choosing)Advance(a,3);else if(chooser&&!chooser->mSeedsInFlight&&Ready(a,chooser->mStartButton))Advance(a,3);}
 else if(s.stage==3)
 {
  if(bank.phase==FusionBankPhase::Intro){if(!s.introStart)s.introStart=GetTickCount();s.introLast=GetTickCount();++s.introChecks;Capture(a,"fusion_split_intro");return true;}if(bank.phase!=FusionBankPhase::Playing||a->mGameScene!=SCENE_PLAYING||chooser)return true;
  if(!Require(a,s.introChecks>=10&&s.introLast-s.introStart>=1000&&s.frames.contains("fusion_split_intro")&&b->mSunMoney==500,"Split skipped full native intro/fresh500"))return true;s.playingTick=b->mMainCounter;Advance(a,4);
 }
 else if(s.stage==4)
 {
  if(b->mMainCounter-s.playingTick<2||!Capture(a,"fusion_split_playing")||!Barrier(a,"FS100_PLAYING"))return true;
  if(!s.budget){if(a->NetIsHost()&&!s.budgetWritten){if(!Require(a,!b->mPlants.mSize&&!b->mZombies.mSize&&b->mSunMoney==500,"Split budget override was not initial-only"))return true;b->mSunMoney=1000;b->mSunCountDown=b->mZombieCountDown=b->mZombieCountDownStart=100000;s.budgetWritten=true;}if(b->mSunMoney!=1000)return true;s.budget=true;NetTrace("SMOKE_FUSION_SPLIT_BUDGET role=%s campaignInitialSun=500 fixtureInitialSun=1000 sunWritesAfterSetup=0 purchaseTotal=900 finalExpected=100",Role(a));}
  if(Barrier(a,"FS100_BUDGET"))Advance(a,5);
 }
 else if(s.stage==5){if(Purchase(a,0,0,false,5,2,0,1000,900,kFusionNoPlant,kFusionNoPlant,s.current,"FS100_PEA"))Advance(a,6);}
 else if(s.stage==6){if(Purchase(a,1,0,true,5,2,1030,900,700,0,0,s.current,"FS100_DOUBLE"))Advance(a,7);}
 else if(s.stage==7){if(Purchase(a,0,0,false,5,2,1090,700,600,1030,0,s.current,"FS100_1090"))Advance(a,8);}
 else if(s.stage==8){if(PrepareCombat(a,1090,1))Advance(a,9);}
 else if(s.stage==9||s.stage==12||s.stage==18)
 {
  auto& q=s.combat;const std::string rear="fusion_split_rear"+std::to_string(q.kind),returned="fusion_split_returned"+std::to_string(q.kind),frame="fusion_split_combat"+std::to_string(q.kind);
  if(q.hits!=6||LiveShots(b)||!q.moveObserved||!s.frames.contains(rear)||!s.frames.contains(returned)||!Capture(a,frame.c_str())||!Barrier(a,"FS100_COMBAT_"+std::to_string(q.kind)))return true;
  auto* aa=Target(b,q.targets[0]);auto* bb=Target(b,q.targets[1]);auto* cc=Target(b,q.targets[2]);NetTrace("SMOKE_FUSION_SPLIT_COMBAT role=%s kind=%d hits=6 A=%d,%d,%d B=%d,%d,%d C=%d,%d,%d MC=5000 adjacent=5000 noSun=1 noAoE=1 noPierce=1",Role(a),q.kind,aa->mBodyHealth,aa->mHelmHealth,aa->mShieldHealth,bb->mBodyHealth,bb->mHelmHealth,bb->mShieldHealth,cc->mBodyHealth,cc->mHelmHealth,cc->mShieldHealth);Advance(a,s.stage==9?10:s.stage==12?13:19);
 }
 else if(s.stage==10){if(Purchase(a,1,2,false,5,1,1108,600,450,1090,2,s.current,"FS100_1108"))Advance(a,11);}
 else if(s.stage==11){if(PrepareCombat(a,1108,0))Advance(a,12);}
 else if(s.stage==13){if(Purchase(a,0,6,false,5,3,6,450,450,kFusionNoPlant,kFusionNoPlant,s.current,"FS100_PUFF"))Advance(a,14);}
 else if(s.stage==14){if(Purchase(a,0,0,false,5,3,1018,450,350,6,0,s.current,"FS100_1018"))Advance(a,15);}
 else if(s.stage==15){if(Purchase(a,0,0,false,5,3,1019,350,250,1018,0,s.current,"FS100_1019"))Advance(a,16);}
 else if(s.stage==16){if(Purchase(a,0,0,false,5,3,1153,250,150,1019,0,s.current,"FS100_1153"))Advance(a,17);}
 else if(s.stage==17){if(PrepareCombat(a,1153,4))Advance(a,18);}
 else if(s.stage==19){if(Purchase(a,0,6,false,2,2,6,150,150,kFusionNoPlant,kFusionNoPlant,s.puffBase,"FS100_PASSIVE_BASE"))Advance(a,20);}
 else if(s.stage==20){if(Purchase(a,1,3,false,8,2,3,150,100,kFusionNoPlant,kFusionNoPlant,s.wallnut,"FS100_WALLNUT"))Advance(a,21);}
 else if(s.stage==21){if(ctx->gloveCooldownTicks&&!ctx->gloveHolds[1].plantId)return true;if(Pickup(a,s.wallnut)&&Barrier(a,"FS100_NUT_HELD"))Advance(a,22);}
 else if(s.stage==22)
 {
  if(slot==1&&!s.clicked){s.clicked=Cell(a,2,2);return true;}auto* p=At(b,2,2);const auto* d=GetNativePlantData(p);if(!d||d->kind!=1021)return true;
  if(!s.observed){if(!Require(a,p->mPlantHealth==2000&&p->mPlantMaxHealth==2000&&d->owner==1&&d->packetId==Packet(b,1,3,false)->mNetId&&d->firstParent==6&&d->secondParent==3&&!ByRef(b,s.wallnut)&&!ByRef(b,s.puffBase)&&ctx->gloveCooldownTicks>334&&ctx->gloveCooldownTicks<=1000,"PuffNut directed real glove recipe/2000HP/incoming3CD mismatch"))return true;s.passive=Identity(p);s.passiveCD=ctx->gloveCooldownTicks;s.observed=true;}
  if(!Business(a)||!Capture(a,"fusion_split_puffnut_crafted")||!Barrier(a,"FS100_PASSIVE_CRAFTED"))return true;NetTrace("SMOKE_FUSION_SPLIT_PASSIVE_CRAFT role=%s id=%u token=%u HP=2000 maxHP=2000 owner=1 parents=6,3 packet=%u incomingKind=3 expectedCD=1000 observedCD=%d sun=100",Role(a),s.passive.id,s.passive.token,d->packetId,s.passiveCD);Advance(a,23);
 }
 else if(s.stage==23){if(ctx->gloveCooldownTicks&&!ctx->gloveHolds[1].plantId)return true;if(Pickup(a,s.passive)&&Barrier(a,"FS100_PASSIVE_MOVE_READY"))Advance(a,24);}
 else if(s.stage==24)
 {
  auto* p=ByRef(b,s.passive);const auto* d=GetNativePlantData(p);if(!Require(a,p&&d,"PuffNut move source vanished"))return true;
  if(p->mPlantCol==2){if(a->NetIsHost()){s.passiveMoveTick=b->mMainCounter;s.passiveData=*d;s.passiveHP=p->mPlantHealth;s.passiveLaunch=p->mLaunchCounter;}if(slot==1&&!s.clicked)s.clicked=Cell(a,3,2);return true;}
  if(!s.observed){const int dt=b->mMainCounter-s.passiveMoveTick;if(!Require(a,p->mPlantCol==3&&p->mRow==2&&p->mPlantHealth==2000&&p->mPlantMaxHealth==2000&&p->mShootingCounter==0&&d->productionTicks==0&&d->pendingProductionTicks==0&&ctx->gloveCooldownTicks>0&&ctx->gloveCooldownTicks<=334&&!ctx->gloveHolds[1].plantId,"PuffNut move added behavior/reset health/wrong334CD"))return true;
   if(a->NetIsHost()&&!Require(a,dt>=0&&p->mPlantHealth==s.passiveHP&&p->mLaunchCounter==s.passiveLaunch&&d->instanceToken==s.passiveData.instanceToken&&d->animationSpeed==s.passiveData.animationSpeed&&d->owner==s.passiveData.owner&&d->packetId==s.passiveData.packetId&&d->firstParent==6&&d->secondParent==3&&ctx->gloveCooldownTicks>=334-dt,"PuffNut move reinitialized passive/provenance"))return true;s.passiveCD=ctx->gloveCooldownTicks;s.observed=true;}
  if(!Business(a)||!Capture(a,"fusion_split_puffnut_moved")||!Barrier(a,"FS100_PASSIVE_MOVED"))return true;NetTrace("SMOKE_FUSION_SPLIT_PASSIVE_MOVE role=%s id=%u token=%u col=3 row=2 HP=2000 sameInstance=1 countersPreserved=1 provenancePreserved=1 expectedCD=334 observedCD=%d",Role(a),s.passive.id,s.passive.token,s.passiveCD);Advance(a,25);
 }
 else if(s.stage==25){if(a->NetIsHost()&&!s.biterSetup&&!SetupBiter(a))return true;if(a->NetIsHost()&&!Send(a,"FS100_BITER_READY"))return true;if(Has(a,0,"FS100_BITER_READY")&&Barrier(a,"FS100_BITER_ACK"))Advance(a,26);}
 else if(s.stage==26)
 {
  auto* p=ByRef(b,s.passive);const auto* d=GetNativePlantData(p);if(!Require(a,p&&d&&p->mPlantMaxHealth==2000&&p->mPlantHealth>0&&p->mShootingCounter==0&&!d->pendingProductionTicks&&!LiveShots(b),"PuffNut bounded native bite produced unsupported behavior"))return true;
  if(p->mPlantHealth>1900||!Business(a)||!Capture(a,"fusion_split_puffnut_bitten")||!Barrier(a,"FS100_PASSIVE_BITTEN"))return true;NetTrace("SMOKE_FUSION_SPLIT_PASSIVE_BITE role=%s id=%u token=%u HP=%d maxHP=2000 noRootCrackStates=1 noAttack=1 noSun=1 healthWrites=0",Role(a),s.passive.id,s.passive.token,p->mPlantHealth);Advance(a,27);
 }
 else if(s.stage==27)
 {
  if(slot==1&&!s.toolClicked){const auto rect=b->GetShovelButtonRect();s.toolClicked=BoardClick(a,float(rect.mX+rect.mWidth/2),float(rect.mY+rect.mHeight/2));return true;}
  if(slot==1&&!s.clicked){if(b->mCursorObject->mCursorType!=CURSOR_TYPE_SHOVEL)return true;s.clicked=Cell(a,3,2);return true;}
  if(ByRef(b,s.passive)||!Business(a)||ctx->gloveCooldownTicks||Samples(a)<8||!Barrier(a,"FS100_DONE")||!Capture(a,"fusion_split_complete"))return true;
  NetTrace("SMOKE_FUSION_SPLIT_COMPLETE role=%s sun=100 normal0Uses=5 extra0Uses=1 cherryUses=1 wallnutUses=1 normal6Uses=2 splitKinds=3 passive1021=1 passiveShoveled=1 noSunReward=1 sunWritesAfterSetup=0 plantHPWrites=0 plantTimerWrites=0 nativeBoard=1 secondWorld=0",Role(a));Advance(a,28);a->CoopSmokeWriteResult(true,nullptr);
 }
 return true;
}
}
