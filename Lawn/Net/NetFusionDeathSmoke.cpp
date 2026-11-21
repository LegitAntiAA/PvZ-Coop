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
#include "../Fusion/FusionNativeDeath.h"
#include "../Fusion/FusionDeathRules.h"
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
constexpr int kRow=2,kInitialWallet=1500,kFinalWallet=150;
struct Ref {uint16_t id=0;uint32_t token=0;};
struct BiteGroup {Ref plant{};std::array<uint16_t,3> ids{};bool setup=false,parked=false;int lastHP=4000;};
struct Mine {Ref ref{};int col=0,row=0,firstTick=0,firstCounter=0,riseTick=0,armedTick=0;bool rising=false,armed=false;};
struct State
{
 int stage=0,step=0,waitingOwn=0,money=kInitialWallet,playingTick=-1;std::array<int,6> uses{};
 Ref donor{},receiver{},potatoNut{},cherryMine{},plain{},occupied{},baseCherry{},worldNut{};
 BiteGroup donorBites{},receiverBites{},potatoBites{};std::array<Mine,2> mines{};
 uint16_t guard=0,factoryTarget=0;int receiverBeforeHeal=0,donorBeforeHeal=0,receiverAfterBase=0;
 int moveCol=0,moveRow=0,moveHP=0,moveFlags=0;Ref moveRef{};NativePlantData moveData{};
 uint32_t since=0,lastChat=0,introStart=0,introLast=0,introChecks=0,pickSamples=0,purchaseSamples=0,negativeSamples=0;
 bool configured=false,clicked=false,placed=false,observed=false,toolClicked=false,budget=false,budgetWritten=false;
 bool baseHealed=false,worldObserved=false,factoryParked=false,potatoDeath=false,negativeSent=false,negativeDone=false,terminalPrepared=false,terminalHealed=false;
 int thresholdSeen=0,lastPotatoHP=4000,terminalBefore=0;uint32_t lowHPDraw=0;
 std::set<uint16_t> stationary,npcIds,pendingLogged;std::map<std::string,int> firstDraw;std::set<std::string> frames;
} s;
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool ok,const char* why){if(!ok)a->CoopSmokeFail(why);return ok;}
void Advance(LawnApp* a,int stage)
{
    s.stage=stage;s.since=GetTickCount();s.clicked=s.placed=s.observed=s.toolClicked=false;
    LawnNet::NetTrace("SMOKE_FUSION_DEATH_STAGE role=%s stage=%d",Role(a),stage);
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
    if(!Require(a,a->CoopSmokeCaptureFrame(name),"Death native renderer capture failed"))return false;
    s.frames.insert(name);LawnNet::NetTrace("SMOKE_FUSION_DEATH_RENDER role=%s frame=%s freshDraws=%d",Role(a),name,a->mDrawCount-s.firstDraw[name]);return true;
}
bool Click(LawnApp* a,float x,float y,uint8_t button=SDL_BUTTON_LEFT,bool click=true)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    if(!click)return Require(a,SDL_PushEvent(&m),"Death scoped motion failed");
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=button;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;
    return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Death scoped SDL click failed");
}
bool BoardClick(LawnApp* a,float x,float y,uint8_t button=SDL_BUTTON_LEFT,bool click=true)
{return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY,button,click);}
bool Cell(LawnApp* a,int col,int row=kRow,uint8_t button=SDL_BUTTON_LEFT,bool click=true)
{return BoardClick(a,float(a->mBoard->GetCellCenterX(col,row)),float(a->mBoard->GridToPixelY(col,row)+50),button,click);}
bool Ready(LawnApp* a,GameButton* b)
{
    if(!b||b->mDisabled||b->mBtnNoDraw)return false;float x=b->mX+b->mWidth*.5f,y=b->mY+b->mHeight*.5f;
    if(!b->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"Death Ready hover failed");return false;}
    LawnNet::NetTrace("SMOKE_FUSION_DEATH_READY role=%s nativeSDL=1 hoverObserved=1",Role(a));return Click(a,x,y);
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
{if(!Require(a,p&&p->mActive&&!p->mRefreshing,"Death real packet not ready"))return false;auto* b=a->mBoard->mSeedBank;return BoardClick(a,float(b->mX+p->mX+p->mOffsetX+p->mWidth/2),float(b->mY+p->mY+p->mHeight/2));}

void Next(LawnApp* a){++s.step;s.since=GetTickCount();s.clicked=s.placed=s.observed=s.toolClicked=false;LawnNet::NetTrace("SMOKE_FUSION_DEATH_STEP role=%s step=%d",Role(a),s.step);}
Zombie* Target(Board* b,uint16_t id)
{Zombie* z=nullptr;while(b->mZombies.IterateNext(z))if(!z->mDead&&z->mNetId==id&&(!z->mNetGhost||z->mVisible))return z;return nullptr;}
void SetX(Zombie* z,float x){z->mPosX=x;z->mX=int(x);}
Zombie* Spawn(LawnApp* a,int row,int hp,float x)
{
 auto* z=a->mBoard->AddZombieInRow(ZOMBIE_NORMAL,row,0);auto* body=z?a->ReanimationTryToGet(z->mBodyReanimID):nullptr;const float rate=body?body->mAnimRate:0;
 if(!Require(a,z&&z->mNetId&&body&&rate>0,"Death controlled native actor creation failed"))return nullptr;
 z->mBodyHealth=z->mBodyMaxHealth=hp;z->mHelmHealth=z->mHelmMaxHealth=z->mShieldHealth=z->mShieldMaxHealth=0;
 z->mPosY=z->GetPosYBasedOnRow(row);z->mY=int(z->mPosY);z->mVelX=0;SetX(z,x);z->SetAnimRate(rate);z->ApplyAnimRate(0);s.stationary.insert(z->mNetId);s.npcIds.insert(z->mNetId);return z;
}
int CardIndex(int kind,bool extra){return kind==0?0:kind==2?(extra?2:1):kind==3?(extra?4:3):5;}
bool Business(LawnApp* a)
{
 constexpr int kinds[]={0,2,2,3,3,4},owners[]={0,1,0,0,1,0},prices[]={100,150,300,50,100,25},cds[]={750,5000,5000,3000,3000,3000};auto* b=a->mBoard;
 for(int i=0;i<6;++i)
 {
  auto* p=Packet(b,owners[i],kinds[i],i==2||i==4);const auto* d=GetNativePacketData(p);const bool valid=p&&d&&d->price==prices[i]&&p->mRefreshTime==(s.uses[i]?cds[i]:0)&&(!a->NetIsHost()||p->mTimesUsed==s.uses[i]);
  if(!valid)LawnNet::NetTrace("SMOKE_FUSION_DEATH_PACKET_MISMATCH role=%s index=%d id=%u price=%d expectedPrice=%d refresh=%d expectedRefresh=%d used=%d expectedUsed=%d",Role(a),i,p?p->mNetId:0,d?d->price:-1,prices[i],p?p->mRefreshTime:-1,s.uses[i]?cds[i]:0,p?p->mTimesUsed:-1,s.uses[i]);
  if(!Require(a,valid,"Death canonical actual packet/cost/cooldown changed"))return false;
 }
 return Require(a,b->mSunMoney==s.money,"Death effect/world factory/stale intent altered wallet");
}
bool Purchase(LawnApp* a,int who,int kind,bool extra,int col,int row,int result,int before,int after,int first,int second,Ref& out)
{
 auto* b=a->mBoard;auto* card=Packet(b,who,kind,extra);
 if(Slot(a)==who&&!s.clicked){if(!card||!card->mActive||card->mRefreshing)return false;s.pickSamples=Samples(a);if(PickPacket(a,card))s.clicked=true;return false;}
 if(Slot(a)==who&&!s.placed){if(!HeldPacket(b,card)||(a->NetIsClient()&&Samples(a)<=s.pickSamples))return false;if(!Require(a,b->mSunMoney==before,"Death pickup charged before actual placement"))return false;s.placed=Cell(a,col,row);return false;}
 if(!s.observed)
 {
  auto* p=At(b,col,row);const auto* d=GetNativePlantData(p);if(!d||d->kind!=result)return false;const int hp=result==3||result==1003||result==1010?4000:300;
  if(!Require(a,card&&d->packetId==card->mNetId&&d->owner==who&&d->instanceToken&&!d->randomOrigin&&d->firstParent==first&&d->secondParent==second&&!d->deathEventFlags&&p->mPlantHealth==hp&&p->mPlantMaxHealth==hp&&b->mSunMoney==after,"Death real directed purchase/HP/flags/provenance differs"))return false;
  out=Identity(p);s.money=after;++s.uses[CardIndex(kind,extra)];s.observed=true;
  LawnNet::NetTrace("SMOKE_FUSION_DEATH_PURCHASE role=%s step=%d kind=%d id=%u token=%u packet=%u owner=%d extra=%d parents=%d,%d HP=%d flags=0 sun=%d nativeSDL=1",Role(a),s.step,result,out.id,out.token,card->mNetId,who,extra?1:0,first,second,hp,after);
 }return Business(a)&&Barrier(a,"FD131_BUY_"+std::to_string(s.step));
}
bool SetupBites(LawnApp* a,BiteGroup& g,Ref ref,const char* tag,int hp)
{
 g.plant=ref;auto* p=ByRef(a->mBoard,ref);if(!Require(a,p,"Death real biter source plant missing"))return false;
 if(a->NetIsHost()&&!g.setup)
 {
  for(auto& id:g.ids)
  {
   auto* z=Spawn(a,p->mRow,hp,1500);if(!z)return false;bool target=false;
   for(int offset=80;offset>=-80;--offset){SetX(z,float(p->mX+offset));if(z->FindPlantTarget(ATTACKTYPE_CHEW)==p){target=true;break;}}
   if(!Require(a,target&&!z->FindZombieTarget(),"Death actual native bite geometry invalid"))return false;id=z->mNetId;
   LawnNet::NetTrace("SMOKE_FUSION_DEATH_BITER group=%s id=%u plant=%u token=%u initialHP=%d row=%d nativeChew=1 plantHPWrites=0",tag,id,p->mNetId,ref.token,hp,z->mRow);
  }g.setup=true;
 }
 std::ostringstream msg;msg<<"FD131_BITERS "<<tag;for(auto id:g.ids)msg<<' '<<id;
 if(a->NetIsHost()&&!Send(a,msg.str()))return false;
 if(a->NetIsClient())for(const auto& l:a->mNetManager->mChatLines)if(l.mSlot==0&&l.mText.rfind(std::string("FD131_BITERS ")+tag+" ",0)==0){std::istringstream in(l.mText.substr(13+std::strlen(tag)));for(auto& id:g.ids){unsigned n=0;if(!(in>>n)||!n||n>65535)return false;id=uint16_t(n);}g.setup=true;}
 for(auto id:g.ids)if(!Target(a->mBoard,id))return false;return Barrier(a,std::string("FD131_BITE_ACK_")+tag);
}
bool ObserveBites(LawnApp* a,BiteGroup& g,int kind,const char* tag)
{
 if(!g.setup||g.parked)return true;auto* p=ByRef(a->mBoard,g.plant);const auto* d=GetNativePlantData(p);if(!Require(a,p&&d&&p->mPlantHealth>0&&p->mPlantHealth<=4000,"Death biter plant died before bounded proof"))return false;
 if(a->NetIsHost()&&kind==1010)
 {
  if(d->deathEventFlags!=s.thresholdSeen)
  {
   const bool valid=(s.thresholdSeen==0&&d->deathEventFlags==1&&p->mPlantHealth>1333&&p->mPlantHealth<2666)||(s.thresholdSeen==1&&d->deathEventFlags==3&&p->mPlantHealth<1333);
   if(!Require(a,valid,"PotatoNut strict one-shot bands/flag ordering differs"))return false;s.thresholdSeen=d->deathEventFlags;
   LawnNet::NetTrace("SMOKE_FUSION_DEATH_BAND id=%u token=%u HP=%d flags=%d sourceStrict=1 naturalBites=1 tick=%d",p->mNetId,d->instanceToken,p->mPlantHealth,s.thresholdSeen,a->mBoard->mMainCounter);
  }
 }
 if(p->mPlantHealth<=1300)
 {
  if(a->NetIsHost())for(auto id:g.ids){auto* z=Target(a->mBoard,id);if(!Require(a,z,"Death biter actor disappeared before parking"))return false;SetX(z,1500);}
  g.parked=true;g.lastHP=p->mPlantHealth;
  LawnNet::NetTrace("SMOKE_FUSION_DEATH_BITTEN role=%s group=%s id=%u token=%u kind=%d HP=%d flags=%u naturalChew=1 stationaryParkingOnly=1",Role(a),tag,p->mNetId,d->instanceToken,kind,p->mPlantHealth,d->deathEventFlags);
 }
 return true;
}
bool ObserveAll(LawnApp* a)
{
 if(!ObserveBites(a,s.donorBites,1003,"donor")||!ObserveBites(a,s.receiverBites,1003,"receiver")||!ObserveBites(a,s.potatoBites,1010,"potato"))return false;
 if(s.baseCherry.id&&!s.baseHealed)
 {
  auto* r=ByRef(a->mBoard,s.receiver);auto* d=ByRef(a->mBoard,s.donor);
  if(r&&d&&r->mPlantHealth>s.receiverBeforeHeal)
  {
   if(!Require(a,r->mPlantHealth==(std::min)(4000,s.receiverBeforeHeal+2000)&&d->mPlantHealth==s.donorBeforeHeal&&!ByRef(a->mBoard,s.baseCherry),"Base Cherry source2 healing2/compatible outside0 differs"))return false;
   s.baseHealed=true;s.receiverAfterBase=r->mPlantHealth;LawnNet::NetTrace("SMOKE_FUSION_DEATH_BASE_HEAL role=%s receiver=%u before=%d after=%d donor=%u donorHP=%d healColliderEntries=2 outsideEntries=0 noPlantHPWrites=1",Role(a),r->mNetId,s.receiverBeforeHeal,r->mPlantHealth,d->mNetId,d->mPlantHealth);
  }
 }
 return true;
}
bool Hold(LawnApp* a,Ref ref)
{
 auto* b=a->mBoard;auto* p=ByRef(b,ref);const auto* d=GetNativePlantData(p);const auto* ctx=GetNativeContext(b);if(!Require(a,p&&d,"Death actual glove source missing"))return false;
 if(ctx->gloveCooldownTicks&&!ctx->gloveHolds[1].plantId)return false;
 if(Slot(a)==1&&!s.toolClicked){auto rect=b->GetShovelButtonRect();b->GetZenButtonRect(OBJECT_TYPE_GLOVE,rect);s.toolClicked=BoardClick(a,float(rect.mX+rect.mWidth/2),float(rect.mY+rect.mHeight/2));return false;}
 if(Slot(a)==1&&!s.clicked){if(b->mCursorObject->mCursorType!=CURSOR_TYPE_GLOVE)return false;s.clicked=Cell(a,p->mPlantCol,p->mRow);return false;}
 if(ctx->gloveHolds[1].plantId!=ref.id||ctx->gloveHolds[1].instanceToken!=ref.token||GetNativeGloveHeldPlant(b,1)!=p)return false;
 if(Slot(a)==1&&b->mCursorObject->mCursorType!=CURSOR_TYPE_PLANT_FROM_GLOVE)return false;s.moveRef=ref;s.moveData=*d;s.moveHP=p->mPlantHealth;s.moveFlags=d->deathEventFlags;s.moveCol=p->mPlantCol;s.moveRow=p->mRow;
 return Barrier(a,"FD131_HELD_"+std::to_string(ref.id));
}
bool Move(LawnApp* a,Ref ref,int col,int row)
{
 auto* b=a->mBoard;auto* p=ByRef(b,ref);const auto* d=GetNativePlantData(p);const auto* ctx=GetNativeContext(b);if(!Require(a,p&&d,"Death live moved actor missing"))return false;
 if(Slot(a)==1&&!s.clicked){s.clicked=Cell(a,col,row);return false;}if(p->mPlantCol!=col||p->mRow!=row)return false;
 if(!Require(a,p->mPlantHealth==s.moveHP&&d->instanceToken==ref.token&&d->owner==s.moveData.owner&&d->packetId==s.moveData.packetId&&d->firstParent==s.moveData.firstParent&&d->secondParent==s.moveData.secondParent&&d->deathEventFlags==s.moveFlags&&!ctx->gloveHolds[1].plantId&&ctx->gloveCooldownTicks>0&&ctx->gloveCooldownTicks<=1000,"Death glove move reset health/flags/provenance or removed actor"))return false;
 if(!Business(a)||!Barrier(a,"FD131_MOVED_"+std::to_string(ref.id)))return false;LawnNet::NetTrace("SMOKE_FUSION_DEATH_MOVE role=%s kind=%u id=%u token=%u col=%d row=%d HP=%d flags=%u owner=%u packet=%u sameInstance=1 nativeSDL=1",Role(a),d->kind,p->mNetId,d->instanceToken,col,row,p->mPlantHealth,d->deathEventFlags,d->owner,d->packetId);return true;
}
bool Shovel(LawnApp* a,int who,Ref ref,int col,int row,const char* tag)
{
 auto* b=a->mBoard;
 if(Slot(a)==who&&!s.toolClicked){const auto r=b->GetShovelButtonRect();s.toolClicked=BoardClick(a,float(r.mX+r.mWidth/2),float(r.mY+r.mHeight/2));return false;}
 if(Slot(a)==who&&!s.clicked){if(b->mCursorObject->mCursorType!=CURSOR_TYPE_SHOVEL)return false;s.clicked=Cell(a,col,row);return false;}
 if(ByRef(b,ref)||!Business(a)||!Barrier(a,std::string("FD131_SHOVEL_")+tag))return false;LawnNet::NetTrace("SMOKE_FUSION_DEATH_SHOVEL role=%s group=%s id=%u token=%u reason=7 nativeSDL=1",Role(a),tag,ref.id,ref.token);return true;
}

bool HighBitReject(LawnApp* a)
{
 auto* b=a->mBoard;auto* plain=ByRef(b,s.plain);auto* packet=Packet(b,0,0,false);const auto* ctx=GetNativeContext(b);const auto* recipe=FindFusionRecipe(0,0);
 if(!s.negativeSent)
 {
  if(!Require(a,plain&&packet&&packet->mActive&&!packet->mRefreshing&&s.money>=100&&recipe&&recipe->result==1030&&ctx->gloveHolds[1].plantId==s.receiver.id,"Death high-bit rejection preconditions not meaningful"))return false;
  s.negativeSamples=Samples(a);s.negativeSent=true;
  if(a->NetIsHost()){FusionCommand cmd;cmd.action=FusionCommandKind::Plant;cmd.packetId=packet->mNetId;cmd.column=1;cmd.row=1;cmd.targetId=0x10000U|s.plain.id;if(!Require(a,a->SubmitNativeFusionCommand(cmd),"Death high-bit intent was not authenticated/queued"))return false;LawnNet::NetTrace("SMOKE_FUSION_DEATH_HIGHBIT_SENT target=%u low16=%u packet=%u validLow16Recipe1030=1 ready=1 sufficientSun=1",cmd.targetId,s.plain.id,packet->mNetId);}
 }
 if(a->NetIsHost()&&!Send(a,"FD131_HIGHBIT_SENT"))return false;if(!Has(a,0,"FD131_HIGHBIT_SENT")||Samples(a)-s.negativeSamples<2)return false;
 const auto* data=GetNativePlantData(plain);
 if(!Require(a,plain&&data&&data->kind==0&&plain->mPlantCol==1&&plain->mRow==1&&plain->mPlantHealth==300&&packet&&packet->mActive&&!packet->mRefreshing&&ctx->gloveHolds[1].plantId==s.receiver.id&&ctx->gloveHolds[1].instanceToken==s.receiver.token&&!ctx->gloveCooldownTicks,"Death high-bit rejection mutated native actor/packet/held state"))return false;
 if(!Business(a)||!Barrier(a,"FD131_HIGHBIT_REJECTED"))return false;s.negativeDone=true;LawnNet::NetTrace("SMOKE_FUSION_DEATH_HIGHBIT role=%s old=%u token=%u kind=0 wallet=150 packetReady=1 heldPreserved=1 noEffects=1 realQueuedIntent=1",Role(a),plain->mNetId,data->instanceToken);return true;
}
bool ObserveWorld(LawnApp* a)
{
 auto* b=a->mBoard;auto* at0=At(b,0,0);auto* at1=At(b,0,1);const auto* d0=GetNativePlantData(at0);const auto* d1=GetNativePlantData(at1);
 if(!d0||!d1||d0->kind!=4||d1->kind!=4)return false;
 auto* mixed=At(b,1,0);const auto* md=GetNativePlantData(mixed);auto* preserved=ByRef(b,s.plain);auto* packet=Packet(b,0,2,true);
 if(!Require(a,mixed&&md&&md->kind==1010&&mixed->mPlantHealth==4000&&!md->deathEventFlags&&md->firstParent==3&&md->secondParent==4&&preserved&&preserved==At(b,1,1)&&preserved->mPlantHealth==300&&!ByRef(b,s.occupied)&&!ByRef(b,s.cherryMine),"CherryMine clipped factory occupancy/supported result/unsupported preservation differs"))return false;
 if(!s.worldObserved)
 {
  s.worldNut=Identity(mixed);std::array<Plant*,2> plants={at0,at1};
  for(int i=0;i<2;++i)
  {
   auto* p=plants[i];const auto* d=GetNativePlantData(p);
   if(!Require(a,p->mPlantHealth==300&&d->owner==0&&d->packetId==packet->mNetId&&!d->randomOrigin&&!d->deathEventFlags&&d->firstParent==kFusionNoPlant&&d->secondParent==kFusionNoPlant&&d->instanceToken&&p->mNetId!=s.cherryMine.id,"World source4 fresh identity/provenance/HP differs"))return false;
   s.mines[i].ref=Identity(p);s.mines[i].col=0;s.mines[i].row=i;s.mines[i].firstTick=b->mMainCounter;s.mines[i].firstCounter=a->NetIsHost()?p->mStateCountdown:-1;
   if(a->NetIsHost()&&!Require(a,p->mState==STATE_NOTREADY&&p->mStateCountdown>0&&p->mStateCountdown<=501,"World mine missing its real5s preparation"))return false;
   LawnNet::NetTrace("SMOKE_FUSION_DEATH_WORLD role=%s index=%d id=%u token=%u kind=4 col=0 row=%d HP=300 owner=%u packet=%u firstRemaining=%d fresh=1 noSunCharge=1 noPacketUse=1",Role(a),i,p->mNetId,d->instanceToken,i,d->owner,d->packetId,s.mines[i].firstCounter);
  }
  if(!Require(a,md->owner==0&&md->packetId==packet->mNetId&&md->instanceToken!=s.occupied.token&&mixed->mNetId!=s.occupied.id,"World occupied result did not get fresh incoming provenance"))return false;
  s.worldObserved=true;LawnNet::NetTrace("SMOKE_FUSION_DEATH_FACTORY role=%s attempts=4 fresh4=2 supported1010=%u token=%u flags=0 preserved0=%u token=%u wallet=150 unsupportedCell=1,1 normalFactory=1",Role(a),mixed->mNetId,md->instanceToken,preserved->mNetId,s.plain.token);
 }
 for(auto& mine:s.mines)
 {
  auto* p=ByRef(b,mine.ref);if(!Require(a,p&&p->mPlantHealth==300,"World mine unexpectedly died before arming"))return false;
  if(p->mState==STATE_POTATO_RISING&&!mine.rising)
  {
   mine.rising=true;mine.riseTick=b->mMainCounter;const int elapsed=mine.riseTick-mine.firstTick;
   if(a->NetIsHost()&&!Require(a,elapsed>=mine.firstCounter&&elapsed<=mine.firstCounter+2,"World mine did not wait natural501 preparation"))return false;
   LawnNet::NetTrace("SMOKE_FUSION_DEATH_RISE role=%s id=%u firstRemaining=%d elapsed=%d state=RISING nativeClock=1",Role(a),p->mNetId,mine.firstCounter,elapsed);
  }
  if(p->mState==STATE_POTATO_ARMED&&!mine.armed)
  {
   if(!Require(a,mine.rising,"World mine skipped actual rise state"))return false;auto* light=a->ReanimationTryToGet(p->mLightReanimID);
   if(!Require(a,light&&!light->mDead,"World client/native armed mine glow missing"))return false;mine.armed=true;mine.armedTick=b->mMainCounter;
   if(a->NetIsHost()&&!Require(a,mine.armedTick-mine.riseTick>=82&&mine.armedTick-mine.riseTick<=86,"World mine changed source84tick rise"))return false;
   LawnNet::NetTrace("SMOKE_FUSION_DEATH_ARMED role=%s id=%u riseObservedTicks=%d nativeLight=1 noTimerWrites=1",Role(a),p->mNetId,mine.armedTick-mine.riseTick);
  }
 }
 return Business(a)&&s.mines[0].armed&&s.mines[1].armed;
}
bool PotatoVictims(LawnApp* a,bool after)
{
 const int hp=after?1700:3500;
 for(auto id:s.potatoBites.ids){auto* z=Target(a->mBoard,id);if(!z)return false;if(!Require(a,z->mBodyHealth==hp&&!z->IsDeadOrDying(),"PotatoNut actual500+500+1800 native damage ledger differs"))return false;}
 return true;
}
bool PrepareTerminal(LawnApp* a)
{
 auto* b=a->mBoard;auto* d=ByRef(b,s.donor);auto* r=ByRef(b,s.receiver);
 if(!Require(a,d&&r&&d->mPlantCol==5&&d->mRow==1&&r->mPlantCol==6&&r->mRow==2&&!b->HasLevelAwardDropped()&&b->AreEnemyZombiesOnScreen(),"Death terminal preconditions invalid before real lastkill"))return false;
 if(a->NetIsHost()&&!s.terminalPrepared)
 {
  NativeBlastSpec area;if(!Require(a,TryMakeFusionDeathBlast(1003,b->GetCellCenterX(5,1),b->GridToPixelY(5,1)+50,1,false,area),"Death terminal source blast precondition invalid"))return false;
  for(auto id:s.npcIds)
  {
   auto* z=Target(b,id);if(!Require(a,z&&z->mBodyHealth>0&&z->mBodyHealth<=1800&&(z->mRow==0||z->mRow==2),"Death terminal actor did not reach naturally earned blast health"))return false;
   auto rect=z->GetZombieRect();SetX(z,z->mPosX+float(area.centerX)-(rect.mX+rect.mWidth*.5f));rect=z->GetZombieRect();
   if(!Require(a,FusionDeathBlastRectOverlaps(area,rect.mX,rect.mY,rect.mWidth,rect.mHeight),"Death terminal native collider outside actual blast"))return false;
   LawnNet::NetTrace("SMOKE_FUSION_DEATH_TERMINAL_TARGET id=%u row=%d HP=%d x=%.3f positionOnly=1 noHPWrites=1",id,z->mRow,z->mBodyHealth,z->mPosX);
  }
 }
 s.terminalPrepared=true;s.terminalBefore=r->mPlantHealth;
 return Business(a)&&Barrier(a,"FD131_TERMINAL_READY");
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionDeath(LawnApp* a)
{
 const char* native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE"),*enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_DEATH");
 if(!a||!native||std::strcmp(native,"1")||!enabled||std::strcmp(enabled,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
 if(a->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();if(GetTickCount()-s.since>80000U){char why[100];sprintf_s(why,"Death deadline stage=%d step=%d role=%s",s.stage,s.step,Role(a));a->CoopSmokeFail(why);return true;}
 if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
 if(s.stage==0)
 {
  if(a->NetIsHost()&&!s.configured){if(!Require(a,!a->mBoard&&!a->mSeedChooserScreen,"Death requires fresh lobby"))return true;a->mNetManager->SetDifficulty(1);s.configured=true;}
  if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
  if(a->NetIsClient()){Advance(a,1);Send(a,"FD131_START");}else if(Has(a,1,"FD131_START")){Advance(a,1);std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;
 }
 if(!a->mBoard||!IsNativeFusionBoard(a->mBoard)){if(s.stage==1&&a->NetIsClient())Send(a,"FD131_START");return true;}auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);auto* chooser=a->mSeedChooserScreen;const int slot=Slot(a);
 if(!Require(a,ctx&&ctx->validation&&!a->mFusionRuntime&&b->GetColumnCount()==10,"Death lost actual native board"))return true;const auto& bank=ctx->selection.State();
 if(a->NetIsHost())for(auto id:s.stationary)if(auto* z=Target(b,id))z->ApplyAnimRate(0);
 if(!ObserveAll(a))return true;
 if(s.stage==1)
 {
  if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||!b->mCutScene->mSeedChoosing||chooser->mSeedsInFlight)return true;const size_t target=(std::min)(kNativeFusionBankCapacity,ctx->eligible.size());
  if(!Require(a,ctx->eligible.size()==20&&target==10&&bank.targetCount==target&&bank.quotas[0]==5&&bank.quotas[1]==5&&bank.activeMask==3,"Death real eligible20/cap10/quotas differ"))return true;if(!Capture(a,"fusion_death_chooser"))return true;
  std::vector<int> choices;for(size_t i=0;i<ctx->eligible.size();++i){const auto& e=ctx->eligible[i];if(slot==0?((!e.isExtra&&e.kind!=2)||(e.isExtra&&e.kind==2)):((e.isExtra&&e.kind!=2)||(!e.isExtra&&e.kind==2)))choices.push_back(int(i));}
  auto priority=[&](int i){const auto& e=ctx->eligible[i];return slot==0?(e.kind==0?0:e.kind==2?1:e.kind==3?2:e.kind==4?3:10+e.kind):(e.kind==2?0:e.kind==3?1:10+e.kind);};std::stable_sort(choices.begin(),choices.end(),[&](int x,int y){return priority(x)<priority(y);});
  int own=0;for(const auto& p:bank.packets)own+=p.owner==slot;if(own<s.waitingOwn)return true;if(!Require(a,choices.size()>=5,"Death disjoint card priorities missing"))return true;
  if(own<5){int x=0,y=0;chooser->GetSeedPositionInChooser(choices[own],x,y);if(Click(a,float(chooser->mX+x+25),float(chooser->mY+y+35-int(chooser->mScrollbar->mScrollValue))))s.waitingOwn=own+1;return true;}
  if(bank.packets.size()!=10||!Packet(b,0,0,false)||!Packet(b,1,2,false)||!Packet(b,0,2,true)||!Packet(b,0,3,false)||!Packet(b,1,3,true)||!Packet(b,0,4,false))return true;int rank=-1;
  for(int i=0;i<b->mSeedBank->mNumPackets;++i){const int r=NetSeedBankOwnerRank(slot,b->mSeedBank->mSeedPackets[i].mNetOwnerSlot);if(!Require(a,r>=rank,"Death bank lost own-first view"))return true;rank=r;}
  NetTrace("SMOKE_FUSION_DEATH_POOL role=%s eligible=20 target=10 quota0=5 quota1=5 ownFirst=1 nativeChooser=1",Role(a));Advance(a,2);
 }
 else if(s.stage==2){if(bank.phase!=FusionBankPhase::Choosing)Advance(a,3);else if(chooser&&!chooser->mSeedsInFlight&&Ready(a,chooser->mStartButton))Advance(a,3);}
 else if(s.stage==3)
 {
  if(bank.phase==FusionBankPhase::Intro){if(!s.introStart)s.introStart=GetTickCount();s.introLast=GetTickCount();++s.introChecks;return true;}
  if(bank.phase!=FusionBankPhase::Playing||a->mGameScene!=SCENE_PLAYING||chooser)return true;
  if(!Require(a,s.introChecks>=10&&s.introLast-s.introStart>=1000&&b->mSunMoney==500,"Death skipped native full intro/fresh500"))return true;s.playingTick=b->mMainCounter;Advance(a,4);
 }
 else if(s.stage==4)
 {
  if(b->mMainCounter-s.playingTick<2||!Capture(a,"fusion_death_playing")||!Barrier(a,"FD131_PLAYING"))return true;
  if(!s.budget)
  {
   if(a->NetIsHost()&&!s.budgetWritten)
   {
    if(!Require(a,!b->mPlants.mSize&&!b->mZombies.mSize&&b->mSunMoney==500,"Death initial regression fixture setup was not empty"))return true;
    b->mSunMoney=kInitialWallet;b->mSunCountDown=b->mZombieCountDown=b->mZombieCountDownStart=100000;
    b->mCurrentWave=b->mNumWaves;auto* guard=Spawn(a,0,1500,700);if(!guard)return true;s.guard=guard->mNetId;s.budgetWritten=true;
    NetTrace("SMOKE_FUSION_DEATH_TERMINAL_SETUP guard=%u HP=1500 row=0 currentWave=%d numWaves=%d initialFixtureCondition=1 futurePhaseWrites=0",s.guard,b->mCurrentWave,b->mNumWaves);
   }
   if(b->mSunMoney!=kInitialWallet)return true;s.budget=true;NetTrace("SMOKE_FUSION_DEATH_BUDGET role=%s campaignInitialSun=500 fixtureInitialSun=1500 totalCost=1350 finalExpected=150 sunWritesAfterSetup=0",Role(a));
  }
  if(Barrier(a,"FD131_BUDGET"))Advance(a,5);
 }
 else if(s.stage==5)
 {
  switch(s.step)
  {
  case 0:if(Purchase(a,0,3,false,5,2,3,1500,1450,kFusionNoPlant,kFusionNoPlant,s.donor))Next(a);break;
  case 1:if(Purchase(a,1,2,false,5,2,1003,1450,1300,3,2,s.donor))Next(a);break;
  case 2:if(Purchase(a,1,3,true,7,2,3,1300,1200,kFusionNoPlant,kFusionNoPlant,s.receiver))Next(a);break;
  case 3:if(Purchase(a,0,2,true,7,2,1003,1200,900,3,2,s.receiver))Next(a);break;
  case 4:if(SetupBites(a,s.donorBites,s.donor,"donor",1500)&&SetupBites(a,s.receiverBites,s.receiver,"receiver",1500))Next(a);break;
  case 5:if(Purchase(a,0,3,false,2,2,3,900,850,kFusionNoPlant,kFusionNoPlant,s.potatoNut))Next(a);break;
  case 6:if(Purchase(a,0,4,false,2,2,1010,850,825,3,4,s.potatoNut))Next(a);break;
  case 7:if(SetupBites(a,s.potatoBites,s.potatoNut,"potato",4500))Next(a);break;
  case 8:if(Purchase(a,0,0,false,1,1,0,825,725,kFusionNoPlant,kFusionNoPlant,s.plain))Next(a);break;
  case 9:if(Purchase(a,1,3,true,1,0,3,725,625,kFusionNoPlant,kFusionNoPlant,s.occupied))Next(a);break;
  case 10:if(Purchase(a,0,4,false,0,0,4,625,600,kFusionNoPlant,kFusionNoPlant,s.cherryMine))Next(a);break;
  case 11:if(Purchase(a,0,2,true,0,0,1162,600,300,4,2,s.cherryMine))Next(a);break;
  case 12:
   if(!s.donorBites.parked||!s.receiverBites.parked||!s.potatoBites.parked||!PotatoVictims(a,false))break;
   if(!Require(a,GetNativePlantData(ByRef(b,s.potatoNut))->deathEventFlags==3,"PotatoNut did not retain both consumed bands"))break;
   if(Capture(a,"fusion_death_low")&&Barrier(a,"FD131_LOW"))Next(a);break;
  case 13:
   if(!s.receiverBeforeHeal){s.receiverBeforeHeal=ByRef(b,s.receiver)->mPlantHealth;s.donorBeforeHeal=ByRef(b,s.donor)->mPlantHealth;}
   if(Purchase(a,1,2,false,8,2,2,300,150,kFusionNoPlant,kFusionNoPlant,s.baseCherry))Next(a);break;
  case 14:if(s.baseHealed&&Business(a)&&Barrier(a,"FD131_BASE_HEALED"))Next(a);break;
  case 15:if(Hold(a,s.receiver))Next(a);break;
  case 16:if(HighBitReject(a))Next(a);break;
  case 17:if(Move(a,s.receiver,6,2))Next(a);break;
  case 18:if(Shovel(a,0,s.cherryMine,0,0,"mine"))Next(a);break;
  case 19:if(ObserveWorld(a)&&Capture(a,"fusion_death_world")&&Barrier(a,"FD131_WORLD_ARMED"))Next(a);break;
  case 20:if(Hold(a,s.potatoNut))Next(a);break;
  case 21:if(Move(a,s.potatoNut,3,2))Next(a);break;
  case 22:
   if(!PotatoVictims(a,false))break;
   if(a->NetIsHost()){auto* p=ByRef(b,s.potatoNut);const float center=float(b->GetCellCenterX(p->mPlantCol,p->mRow));for(auto id:s.potatoBites.ids){auto* z=Target(b,id);const auto r=z->GetZombieRect();SetX(z,z->mPosX+center-(r.mX+r.mWidth*.5f));}}
   if(Barrier(a,"FD131_POTATO_DEATH_READY"))Next(a);break;
  case 23:if(Shovel(a,0,s.potatoNut,3,2,"potato"))Next(a);break;
  case 24:
   if(!PotatoVictims(a,true)||!Barrier(a,"FD131_POTATO_LEDGER"))break;
   NetTrace("SMOKE_FUSION_DEATH_POTATO_LEDGER role=%s parent=%u victims=3 initial=4500 mid500=1 low500=1 death1800=1 after=1700 noCharred=1 flagsPreservedByMove=3",Role(a),s.potatoNut.id);s.potatoDeath=true;Next(a);break;
  case 25:if(Hold(a,s.donor))Next(a);break;
  case 26:if(Move(a,s.donor,5,1))Next(a);break;
  case 27:if(PrepareTerminal(a))Next(a);break;
  case 28:if(Shovel(a,1,s.donor,5,1,"donor"))Next(a);break;
  case 29:
  {
   auto* r=ByRef(b,s.receiver);if(!r||r->mPlantHealth!=4000)break;
   if(a->NetIsHost())
   {
    if(!Require(a,b->HasLevelAwardDropped()&&!b->AreEnemyZombiesOnScreen()&&s.npcIds.size()==10,"Death real lastkill did not enter native terminal award before continuation"))break;
    if(!Send(a,"FD131_ACTUAL_AWARD"))break;
   }
   if(!Has(a,0,"FD131_ACTUAL_AWARD")||!Business(a)||!Capture(a,"fusion_death_terminal")||!Barrier(a,"FD131_TERMINAL_HEALED"))break;
   NetTrace("SMOKE_FUSION_DEATH_TERMINAL role=%s receiver=%u token=%u before=%d after=4000 healEntries=2 clamp=1 actualLastkillAward=1 phaseWritesAfterSetup=0 healAfterAwardContinuation=1",Role(a),r->mNetId,s.receiver.token,s.terminalBefore);s.terminalHealed=true;Next(a);break;
  }
  case 30:
  {
   constexpr std::array<int,6> counts={1,2,2,2,2,2};
   if(!Require(a,s.money==150&&s.uses==counts&&s.baseHealed&&s.worldObserved&&s.potatoDeath&&s.negativeDone&&s.terminalHealed,"Death final real purchase/effect proofs incomplete"))break;
   if(!Business(a)||!Capture(a,"fusion_death_complete")||!Barrier(a,"FD131_COMPLETE"))break;
   NetTrace("SMOKE_FUSION_DEATH_COMPLETE role=%s kinds=1003,1010,1162 realPurchases=11 initialSun=1500 totalCost=1350 finalSun=150 uses=1,2,2,2,2,2 naturalBites=1 world4Attempts=4 healing2and0=1 boundary1PureOnly=1 nativeBoard=1 secondWorld=0 plantHPWrites=0 plantTimerWrites=0 phaseWritesAfterSetup=0 sourceZombieAIUnported=1",Role(a));Advance(a,6);a->CoopSmokeWriteResult(true,nullptr);break;
  }
  }
 }
 return true;
}
}
