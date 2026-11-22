#include "NetFusionCampaign356Smoke.h"
#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetCommon.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../MessageWidget.h"
#include "../System/PlayerInfo.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativeAdventure273.h"
#include "../Fusion/FusionNativeConveyor308.h"
#include "../Fusion/FusionNativeZombies.h"
#include "../Fusion/FusionNativeZombieVisual.h"
#include "../Fusion/FusionCampaign342Rules.h"
#include "../../SexyAppFramework/ScrollbarWidget.h"
#include "../../SexyAppFramework/DDInterface.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <tuple>

namespace
{
using namespace LawnFusion;using LawnNet::NetTrace;
constexpr std::array<int,11> levels={19,20,21,22,23,24,25,26,27,28,37};
struct Ref {uint16_t id=0,kind=0;uint32_t token=0;int column=0,row=0;};
struct Enemy {Ref ref;float firstX=0;bool moved=false;};
struct State
{
 int index=0,firstIndex=0,stage=0,waiting=0,input=0,classicLevel=-1,classicFinished=-1,playingFrame=-1,completed=0;
 int beforeSun=0,price=0,buyColumn=0,buyRow=4;uint16_t buyPacket=0,buyKind=0;
 uint32_t started=0,caseStarted=0,since=0,chat=0,wheel=0,epoch=0,samples=0,pickSamples=0;
 bool startRequested=false,pauseRequested=false,localProof=false,boot=false,beltSeen=false,visible=false;
 unsigned adviceMask=0;Ref purchase;FusionAdventureProfile273 clientProfile;
 std::map<uint16_t,Enemy> waves;std::map<std::tuple<int,int,int>,Ref> authored;
 std::map<uint16_t,Ref> missionEnemies;std::vector<Ref> pots;
 std::string checkpoint;std::set<std::string> frames;std::map<std::string,int> draws;
}s;
int Level(){return levels[size_t(s.index)];}
bool Enabled(const char* name){const auto* p=std::getenv(name);return p&&std::strcmp(p,"1")==0;}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool okay,const char* why){if(!okay)a->CoopSmokeFail(why);return okay;}
FusionAdventureProfile273 Profile(int level){return {(uint64_t{1}<<(level-1))-1,1,uint16_t(level)};}
bool Same(const FusionAdventureProfile273& x,const FusionAdventureProfile273& y)
{return x.completedLevels==y.completedLevels&&x.cycle==y.cycle&&x.frontierLevel==y.frontierLevel;}
void Advance(LawnApp* a,int stage)
{s.stage=stage;s.since=GetTickCount();s.input=0;NetTrace("SMOKE_CAMPAIGN356_STAGE role=%s level=%d stage=%d epoch=%u",Role(a),Level(),stage,s.epoch);}
bool Has(LawnApp* a,int slot,const std::string& text)
{for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==slot&&line.mText==text)return true;return false;}
bool Send(LawnApp* a,const std::string& text)
{if(Has(a,Slot(a),text))return true;if(s.chat&&GetTickCount()-s.chat<850)return false;s.chat=GetTickCount();a->mNetManager->SendChat(text);return Has(a,Slot(a),text);}
std::string Tag(const std::string& name){return "C356_"+std::to_string(Level())+"_"+std::to_string(s.epoch)+"_"+name;}
bool Barrier(LawnApp* a,const std::string& name)
{const auto tag=Tag(name);Send(a,tag);return Has(a,0,tag)&&Has(a,1,tag);}
bool Click(LawnApp* a,float x,float y)
{
 SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
 SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
 SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;
 return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Campaign356 owned SDL click failed");
}
bool BoardClick(LawnApp* a,float x,float y)
{return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY);}
bool Escape(LawnApp* a)
{SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Campaign356 SDL pause failed");}
bool Capture(LawnApp* a,const char* suffix)
{
 const auto name="fusion_campaign356_level"+std::to_string(Level())+"_"+suffix;if(s.frames.contains(name))return true;
 if(!s.draws.contains(name))s.draws[name]=a->mDrawCount;
 if(!a->CoopSmokeEnsureFreshFrameAfterDraw(s.draws[name]+2))return false;
 if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Campaign356 fresh native BMP failed"))return false;
 s.frames.insert(name);NetTrace("SMOKE_CAMPAIGN356_RENDER role=%s level=%d frame=%s draw=%d",Role(a),Level(),name.c_str(),a->mCoopSmokeFrameDrawCount);return true;
}
bool World(LawnApp* a)
{
 auto* b=a->mBoard;const auto* c=GetNativeContext(b);const auto* d=FindFusionSourceAdventure272(Level());if(!c||!d)return false;
 FusionSourceNativeProjection272 projection;if(!TryProjectFusionSourceAdventure272(Level(),projection))return false;
 const auto& v=c->adventure;
 if(!Require(a,IsNativeSourceAdventure273(b)&&!c->validation&&!a->mFusionRuntime&&v.enabled&&v.run.level==Level()&&v.run.cycle==1&&v.sourceDifficulty==2&&!v.run.replay&&!v.run.over&&!v.schedulerFailed&&
  Same(v.profile,Profile(Level()))&&c->money==d->initialMoney&&b->GetColumnCount()==10&&GetNativeFusionRowCount(b)==d->rows&&int(b->mBackground)==projection.background&&
  a->mPlayerInfo&&a->mPlayerInfo->mLevel==s.classicLevel&&a->mPlayerInfo->mFinishedAdventure==s.classicFinished&&
  Same(a->mNetManager->mConfig.mFusionSourceProfile,a->NetIsHost()?Profile(Level()):s.clientProfile)&&IsNativeSourceConveyor308(b)==d->conveyor,
  "Campaign356 source session/profile/terrain/policy/Classic state differs"))return false;
 for(int row=0;row<d->rows;++row)
 {
  if(!Require(a,b->mPlantRow[row]==(((d->scene==2||d->scene==3)&&(row==2||row==3))?PLANTROW_POOL:PLANTROW_NORMAL),"Campaign356 native source row terrain differs"))return false;
  for(int col=0;col<10;++col){const int x=b->GetCellCenterX(col,row),y=b->GridToPixelY(col,row)+50;
   if(!Require(a,b->PixelToGridX(x,y)==col&&b->PixelToGridY(x,y)==row,"Campaign356 source cell coordinate roundtrip differs"))return false;}
 }
 if(!s.epoch)s.epoch=c->epoch;return Require(a,s.epoch==c->epoch,"Campaign356 epoch changed inside a case");
}
bool Choice(LawnApp* a,SeedChooserScreen* c,int index)
{
 int x=0,y=0;c->GetSeedPositionInChooser(index,x,y);const float cy=float(y+35-int(c->mScrollbar->mScrollValue));
 if(cy<158||cy>493)
 {if(s.wheel&&GetTickCount()-s.wheel<250)return false;s.wheel=GetTickCount();SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=float(c->mX+300);m.motion.y=float(c->mY+300);SDL_Event w{};w.type=SDL_EVENT_MOUSE_WHEEL;w.wheel.windowID=m.motion.windowID;w.wheel.y=cy<158?1.f:-1.f;Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&w),"Campaign356 chooser scroll failed");return false;}
 return std::abs(c->mScrollAmount)<=.05f&&Click(a,float(c->mX+x+25),float(c->mY)+cy);
}
bool Choose(LawnApp* a)
{
 const auto* c=GetNativeContext(a->mBoard);const auto& bank=c->selection.State();
 if(IsNativeSourceConveyor308(a->mBoard))return bank.phase==FusionBankPhase::Playing&&a->mGameScene==SCENE_PLAYING&&Capture(a,"boot");
 auto* ui=a->mSeedChooserScreen;if(!ui||!ui->mNativeFusionChooser||!ui->mMouseVisible||ui->mSeedsInFlight||!a->mBoard->mCutScene->mSeedChoosing)return false;
 const int quota=bank.quotas[size_t(Slot(a))];std::vector<int> choices;
 for(size_t i=0;i<c->eligible.size();++i)if(c->eligible[i].isExtra==(Slot(a)==1)&&c->eligible[i].kind<=4)choices.push_back(int(i));
 if(!Require(a,quota>0&&quota<=int(choices.size()),"Campaign356 actual source chooser quota/filler availability differs"))return false;
 int count=0;for(const auto& p:bank.packets)if(p.owner==Slot(a))++count;
 if(count<s.waiting)return false;if(count<quota){if(Choice(a,ui,choices[size_t(count)]))s.waiting=count+1;return false;}
 return bank.packets.size()==bank.targetCount&&Capture(a,"boot")&&Barrier(a,"CHOOSER");
}
SeedPacket* Packet(Board* b,uint16_t id)
{for(int i=0;i<b->mSeedBank->mNumPackets;++i)if(b->mSeedBank->mSeedPackets[i].mNetId==id)return &b->mSeedBank->mSeedPackets[i];return nullptr;}
Plant* Purchased(Board* b)
{Plant* p=nullptr;while(b->IteratePlants(p))if(!p->mDead&&p->mPlantCol==s.buyColumn&&p->mRow==s.buyRow)if(const auto* d=GetNativePlantData(p);d&&d->kind==s.buyKind&&d->packetId==s.buyPacket)return p;return nullptr;}
bool Boot(LawnApp* a)
{
 auto* b=a->mBoard;const auto* c=GetNativeContext(b);const auto* d=FindFusionSourceAdventure272(Level());
 if(s.boot)return true;
 if(!Require(a,b->mSunMoney==d->initialSun,"Campaign356 real initial Sun differs"))return false;
 if(!d->conveyor)
 {const auto& bank=c->selection.State();if(!Require(a,b->mSeedBank->mNumPackets==bank.targetCount,"Campaign356 native selected bank size differs"))return false;
  for(int i=0;i<b->mSeedBank->mNumPackets;++i){const auto* p=GetNativePacketData(&b->mSeedBank->mSeedPackets[i]);if(!Require(a,p&&p->owner==(i<bank.quotas[size_t(Slot(a))]?Slot(a):1-Slot(a)),"Campaign356 own native packets not displayed first"))return false;}}
 if(a->NetIsHost())
 {const auto& plan=c->adventureScheduler;if(!Require(a,plan.initialized&&!plan.failed&&plan.level==Level()&&plan.maximumWaves==int(d->firstCycleWaves)&&plan.waves.size()==d->firstCycleWaves,"Campaign356 real host campaign plan differs"))return false;}
 if(d->roofPreplantColumns)
 {
  std::set<std::pair<int,int>> cells;Plant* p=nullptr;while(b->IteratePlants(p))if(!p->mDead)if(const auto* x=GetNativePlantData(p);x&&x->kind==27)
  {if(!Require(a,p->mPlantCol>=0&&p->mPlantCol<5&&p->mRow>=0&&p->mRow<5&&x->owner==LawnNet::NET_SHARED_OWNER&&!x->packetId&&p->mNetId&&x->instanceToken&&p->mPlantHealth==p->mPlantMaxHealth,"Campaign356 authored Roof Pot identity/cell/default differs"))return false;cells.emplace(p->mPlantCol,p->mRow);s.pots.push_back({p->mNetId,x->kind,x->instanceToken,p->mPlantCol,p->mRow});}
  if(!Require(a,cells.size()==25&&s.pots.size()==25,"Campaign356 Roof requires exact25 genuine preplanted Pots"))return false;
 }
 s.boot=true;NetTrace("SMOKE_CAMPAIGN356_BOOT role=%s level=%d epoch=%u scene=%u rows=%u cols=10 sun=%d money=%d maximumWaves=%u conveyor=%d pots=%zu profileFixture=1 earned=0",Role(a),Level(),s.epoch,d->scene,d->rows,b->mSunMoney,c->money,d->firstCycleWaves,d->conveyor,s.pots.size());return true;
}
bool Belt(LawnApp* a)
{
 auto* b=a->mBoard;const auto* c=GetNativeContext(b);if(!IsNativeSourceConveyor308(b))return true;
 if(!Require(a,b->mSeedBank->mNumPackets==13&&c->selection.State().targetCount==13&&c->selection.State().packets.size()<=13,"Campaign356 real raw13 conveyor bank differs"))return false;
 for(int i=0;i<13;++i)
 {const auto* p=&b->mSeedBank->mSeedPackets[i];if(!p->mNetId)continue;const auto* d=GetNativePacketData(p);
  if(!Require(a,d&&d->owner==LawnNet::NET_SHARED_OWNER&&!d->isExtra&&!d->price&&d->rechargeSeconds==0&&!p->mRefreshing&&!p->mRefreshTime,"Campaign356 conveyor free/shared/raw packet differs"))return false;s.beltSeen=true;}
 return true;
}
bool Buy(LawnApp* a)
{
 auto* b=a->mBoard;const bool belt=IsNativeSourceConveyor308(b);if(!Belt(a))return false;
 const auto planPrefix=Tag("BUY_");
 if(!s.buyPacket&&a->NetIsHost())
 {
  for(int i=0;i<b->mSeedBank->mNumPackets;++i){auto* p=&b->mSeedBank->mSeedPackets[i];const auto* d=GetNativePacketData(p);if(!d)continue;
   const bool desired=belt?(d->kind==3||d->kind==14||d->kind==16||d->kind==17):(d->owner==1&&(d->kind==0||d->kind==1||d->kind==3)&&d->isExtra&&b->GetSeedPacketCost(*p)<=b->mSunMoney);
   if(!desired||(belt&&(!p->mActive||p->mOffsetX>0||!CanUseNativeConveyorPacket308(b,p->mNetId,1))))continue;
   Plant* target=nullptr;const int row=4;
   if(belt&&(!ResolveNativeFusionPlacementTarget(b,s.buyColumn,row,d->kind,target)||target))continue;
   s.buyPacket=p->mNetId;s.buyKind=d->kind;s.buyRow=row;s.beforeSun=b->mSunMoney;s.price=b->GetSeedPacketCost(*p);break;}
 }
 if(a->NetIsHost()&&s.buyPacket)
 {const auto plan=planPrefix+std::to_string(s.buyPacket)+"_"+std::to_string(s.buyKind)+"_"+std::to_string(s.price)+"_"+std::to_string(s.beforeSun)+"_"+std::to_string(s.buyRow);if(!Send(a,plan))return false;}
 if(a->NetIsClient()&&!s.buyPacket)for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==0&&line.mText.starts_with(planPrefix))
 {
  unsigned id=0,kind=0;int price=0,sun=0,row=0;
  if(std::sscanf(line.mText.c_str()+planPrefix.size(),"%u_%u_%d_%d_%d",&id,&kind,&price,&sun,&row)!=5||!id||id>65535||kind>65535||price<0||sun<price||(row!=2&&row!=4))return Require(a,false,"Campaign356 host fixture packet plan invalid");
  s.buyPacket=uint16_t(id);s.buyKind=uint16_t(kind);s.price=price;s.beforeSun=sun;s.buyRow=row;break;
 }
 if(!s.buyPacket)return false;
 auto* plant=Purchased(b);auto* packet=Packet(b,s.buyPacket);
 if(!plant&&Slot(a)==1&&!s.input)
 {if(!packet||!packet->CanPickUp()||b->mCursorObject->mCursorType!=CURSOR_TYPE_NORMAL)return false;s.pickSamples=Samples(a);const auto* bank=b->mSeedBank;
  if(BoardClick(a,float(bank->mX+packet->mX+packet->mOffsetX+(belt?19:25)),float(bank->mY+packet->mY+(belt?27:35))))s.input=1;return false;}
 if(!plant&&Slot(a)==1&&s.input==1)
 {const auto* cursor=b->mCursorObject;if(cursor->mCursorType!=CURSOR_TYPE_PLANT_FROM_BANK||cursor->mSeedBankIndex<0||cursor->mSeedBankIndex>=b->mSeedBank->mNumPackets||b->mSeedBank->mSeedPackets[cursor->mSeedBankIndex].mNetId!=s.buyPacket||Samples(a)<=s.pickSamples)return false;
  if(BoardClick(a,float(b->GetCellCenterX(s.buyColumn,s.buyRow)),float(b->GridToPixelY(s.buyColumn,s.buyRow)+50)))s.input=2;return false;}
 if(Slot(a)==1&&s.input&&!plant&&GetTickCount()-s.wheel>2000)
 {s.wheel=GetTickCount();const auto* cursor=b->mCursorObject;NetTrace("SMOKE_CAMPAIGN356_PICK_WAIT level=%d input=%d expected=%u cursor=%d index=%d actual=%u samples=%u pickedAt=%u",Level(),s.input,s.buyPacket,int(cursor->mCursorType),cursor->mSeedBankIndex,cursor->mSeedBankIndex>=0&&cursor->mSeedBankIndex<b->mSeedBank->mNumPackets?b->mSeedBank->mSeedPackets[cursor->mSeedBankIndex].mNetId:0,Samples(a),s.pickSamples);}
 const auto* d=GetNativePlantData(plant);if(!plant||!d)return false;
 if(!Require(a,plant->mNetId&&d->instanceToken&&d->owner==1&&b->mSunMoney==s.beforeSun-s.price&&(!belt||s.price==0),"Campaign356 actual CLIENT placement/default identity/Sun debit differs"))return false;
 if(belt&&Packet(b,s.buyPacket))return false;
 if(!s.purchase.id){s.purchase={plant->mNetId,d->kind,d->instanceToken,plant->mPlantCol,plant->mRow};NetTrace("SMOKE_CAMPAIGN356_CLIENT_USE role=%s level=%d packet=%u kind=%u id=%u token=%u price=%d beforeSun=%d afterSun=%d owner=%u actualSDL=1 conveyor=%d",Role(a),Level(),s.buyPacket,d->kind,plant->mNetId,d->instanceToken,s.price,s.beforeSun,b->mSunMoney,d->owner,belt);}
 return Barrier(a,"USE_"+std::to_string(s.purchase.id)+"_"+std::to_string(s.purchase.token));
}
void Observe(LawnApp* a)
{
 auto* b=a->mBoard;const auto* c=GetNativeContext(b);if(!c||a->mGameScene!=SCENE_PLAYING)return;
 const auto message=c->adventure.run.tutorial.message;
 if(((Level()==22&&message==FusionAdventureMessage273::KelpMillion)||(Level()==24&&message==FusionAdventureMessage273::WarmBonus))&&
    c->adventure.run.tutorial.messageTicks&&b->mAdvice&&!b->mAdvice->mLabel.empty()&&b->mAdvice->mDuration>0)
 {if(!(s.adviceMask&2))NetTrace("SMOKE_CAMPAIGN356_AUTHORED_ADVICE role=%s level=%d message=%u sourceTicks=%u nativeDuration=%d",Role(a),Level(),unsigned(message),c->adventure.run.tutorial.messageTicks,b->mAdvice->mDuration);s.adviceMask|=2;}
 Plant* p=nullptr;while(b->IteratePlants(p))if(!p->mDead)if(const auto* d=GetNativePlantData(p))
 {
  const bool expected=Level()==22?d->kind==1051&&(p->mPlantCol==1||p->mPlantCol==2)&&(p->mRow==2||p->mRow==3):Level()==24?((d->kind==1062&&p->mPlantCol==9&&p->mRow==0)||(d->kind==17&&p->mPlantCol==9&&p->mRow==1)||(d->kind==0&&p->mPlantCol==1&&(p->mRow==0||p->mRow==1))):false;
  if(expected&&d->owner==LawnNet::NET_SHARED_OWNER&&!d->packetId&&p->mNetId&&d->instanceToken)s.authored.try_emplace({d->kind,p->mPlantCol,p->mRow},Ref{p->mNetId,d->kind,d->instanceToken,p->mPlantCol,p->mRow});
 }
 FusionCampaign342Plan plan;std::string error;BuildFusionCampaign342Plan(Level(),1,plan,&error);Zombie* z=nullptr;
 while(b->IterateZombies(z))if(!z->mDead)if(const auto* d=GetNativeSourceZombieData(z);d&&z->mNetId&&d->instanceToken)
 {
  if((Level()<=21||Level()==28)&&z->mFromWave==1)
  {
   if(!Require(a,std::find(plan.possibleKinds.begin(),plan.possibleKinds.end(),d->kind)!=plan.possibleKinds.end()&&z->mZombieType==GetNativeSourceZombieCarrier(d->kind),"Campaign356 genuine first wave produced unsupported source identity"))return;
   const bool water=z->mRow==2||z->mRow==3;
   if(!Require(a,(d->kind!=11&&d->kind!=12&&d->kind!=13||water)&&(!water||(d->kind!=0&&d->kind!=2&&d->kind!=4&&d->kind!=9))&&!(Level()==21&&d->kind==17),"Campaign356 source water-row repair/duck conversion/final-only roster differs"))return;
   auto [it,fresh]=s.waves.try_emplace(z->mNetId,Enemy{{z->mNetId,d->kind,d->instanceToken,0,z->mRow},z->mPosX,false});
   if(fresh)NetTrace("SMOKE_CAMPAIGN356_WAVE role=%s level=%d epoch=%u wave=1 id=%u token=%u kind=%u row=%d carrier=%d",Role(a),Level(),s.epoch,z->mNetId,d->instanceToken,d->kind,z->mRow,int(z->mZombieType));
   if(!Require(a,it->second.ref.token==d->instanceToken,"Campaign356 first wave generation changed"))return;it->second.moved|=std::abs(z->mPosX-it->second.firstX)>1;
   float x=0,y=0;if(it->second.moved&&GetNativeSourceZombie269NodePoint(z,"Shadow",x,y)&&x+b->mX+a->mDDInterface->mWideScreenOffsetX>=30&&x+b->mX+a->mDDInterface->mWideScreenOffsetX<=a->mWidth-20&&(Level()!=28||x<=b->GetCellCenterX(3,z->mRow)))s.visible=true;
  }
  if(z->mFromWave==0&&((Level()==22&&d->kind==14&&z->mRow==2&&z->mBodyMaxHealth==1000000)||(Level()==24&&d->kind==0&&(z->mRow==0||z->mRow==1))))s.missionEnemies.try_emplace(z->mNetId,Ref{z->mNetId,d->kind,d->instanceToken,0,z->mRow});
 }
}
bool Goal(LawnApp* a)
{
 const auto* c=GetNativeContext(a->mBoard);
 if(Level()<=21||Level()==28)
 {if(s.waves.empty()||!s.visible||c->adventure.wave<1)return false;if(a->NetIsHost()&&s.waves.size()!=c->adventureScheduler.waves.front().size())return false;}
 if(Level()==22&&(s.authored.size()!=4||s.missionEnemies.empty()||!(s.adviceMask&2)))return false;
 if(Level()==24&&(s.authored.size()!=4||s.missionEnemies.size()<2||!(s.adviceMask&2)))return false;
 return Level()!=27||s.beltSeen;
}
uint32_t Proof()
{
 LawnNet::NetBuffer b;b.WriteU16(uint16_t(Level()));b.WriteU32(s.epoch);
 const auto ref=[&](const Ref& r){b.WriteU16(r.id);b.WriteU32(r.token);b.WriteU16(r.kind);b.WriteU8(uint8_t(r.column));b.WriteU8(uint8_t(r.row));};
 ref(s.purchase);for(const auto& [id,v]:s.waves)ref(v.ref);for(const auto& [cell,r]:s.authored)ref(r);for(const auto& [id,r]:s.missionEnemies)ref(r);
 auto pots=s.pots;std::sort(pots.begin(),pots.end(),[](const Ref& x,const Ref& y){return x.id<y.id;});for(const auto& r:pots)ref(r);
 return LawnNet::NetFnv1a32(b.Data(),b.Size());
}
void Restart(LawnApp* a)
{
 const auto* c=GetNativeContext(a->mBoard);
 if(c&&c->epoch!=s.epoch)
 {
  if(!Require(a,s.localProof&&!s.checkpoint.empty()&&Has(a,0,s.checkpoint)&&Has(a,1,s.checkpoint),"Campaign356 next epoch arrived without both prior local proofs"))return;
  State next;next.index=s.index+1;next.firstIndex=s.firstIndex;next.stage=1;next.started=s.started;next.caseStarted=next.since=GetTickCount();next.classicLevel=s.classicLevel;next.classicFinished=s.classicFinished;next.clientProfile=s.clientProfile;next.completed=s.completed;s=std::move(next);return;
 }
 if(a->NetIsHost()&&!s.startRequested)
 {s.startRequested=true;const int level=levels[size_t(s.index+1)];a->mNetManager->mConfig.mFusionSourceProfile=Profile(level);NetTrace("SMOKE_CAMPAIGN356_PROFILE level=%d setup=IsolatedFixtureOnly earned=0 explicitSave=0",level);Require(a,a->StartFusionSourceAdventure(level,false),"Campaign356 next real source startup rejected");}
}
}
namespace LawnNet
{
bool NetSmokeUpdateFusionCampaign356(LawnApp* a)
{
 if(!Enabled("PVZ_COOP_SMOKE_FUSION_CAMPAIGN356"))return false;
 if(!a||!a->CoopSmokeActive()||!a->mNetManager||a->mCoopSmokeResultWritten)return true;
 if(!Require(a,Enabled("PVZ_COOP_SMOKE_FUSION_NATIVE")&&Enabled("PVZ_COOP_SMOKE_FUSION_SOURCE273")&&a->mCoopSmokeManifest.mScenario==NETSMOKE_SCENARIO_FUSION_VALIDATION,"Campaign356 requires private native/source validation"))return true;
 if(!s.started)
 {
  const char* first=std::getenv("PVZ_COOP_SMOKE_FUSION_CAMPAIGN356_FIRST");int level=first?std::atoi(first):19;
  const auto at=std::find(levels.begin(),levels.end(),level);
  if(!Require(a,at!=levels.end(),"Campaign356 invalid first diagnostic case"))return true;
  s.index=s.firstIndex=int(at-levels.begin());s.started=s.caseStarted=s.since=GetTickCount();
 }
 if(GetTickCount()-s.started>900000||GetTickCount()-s.caseStarted>120000){NetTrace("SMOKE_CAMPAIGN356_TIMEOUT role=%s level=%d stage=%d epoch=%u purchase=%u waves=%zu authored=%zu mission=%zu",Role(a),Level(),s.stage,s.epoch,s.purchase.id,s.waves.size(),s.authored.size(),s.missionEnemies.size());a->CoopSmokeFail("Campaign356 bounded source boot/mechanism deadline");return true;}
 if(!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
 if(!s.stage)
 {
  if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
  if(!Require(a,Slot(a)>=0&&Slot(a)<2&&a->mNetManager->GetDifficulty()==1&&a->mPlayerInfo,"Campaign356 requires two native peers/difficulty1/local profile"))return true;
  if(s.classicLevel<0){s.classicLevel=a->mPlayerInfo->mLevel;s.classicFinished=a->mPlayerInfo->mFinishedAdventure;s.clientProfile=a->mNetManager->mConfig.mFusionSourceProfile;}
  if(a->NetIsClient()){if(Send(a,"C356_BEGIN"))Advance(a,1);return true;}
  if(!Has(a,1,"C356_BEGIN"))return true;
  if(!Require(a,Same(s.clientProfile,Profile(Level()))&&a->StartFusionSourceAdventure(Level(),false),"Campaign356 private initial19 profile/source startup rejected"))return true;Advance(a,1);return true;
 }
 const auto* c=GetNativeContext(a->mBoard);
 if(s.stage==7||(s.localProof&&c&&c->epoch!=s.epoch)){Restart(a);return true;}
 if(!a->mBoard||!World(a))return true;Observe(a);if(a->mCoopSmokeResultWritten)return true;
 const auto& bank=c->selection.State();
 if(s.stage==1){if(Choose(a))Advance(a,IsNativeSourceConveyor308(a->mBoard)?3:2);}
 else if(s.stage==2)
 {
  if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}
  auto* ui=a->mSeedChooserScreen;auto* button=ui?ui->mStartButton:nullptr;if(!button||button->mDisabled||button->mBtnNoDraw||ui->mSeedsInFlight)return true;
  const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
  if(!button->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"Campaign356 Ready hover failed");}
  else if(Click(a,x,y))Advance(a,3);
 }
 else if(s.stage==3)
 {
  if(bank.phase!=FusionBankPhase::Playing||a->mSeedChooserScreen||a->mGameScene!=SCENE_PLAYING)return true;
  if(s.playingFrame<0){s.playingFrame=a->mAppCounter;s.samples=Samples(a);return true;}
  if(a->mAppCounter<=s.playingFrame||Samples(a)-s.samples<2||!Boot(a))return true;
  if(Barrier(a,"BOOT"))Advance(a,4);
 }
 else if(s.stage==4){if(Buy(a))Advance(a,5);}
 else if(s.stage==5)
 {
  if(!Goal(a)||!Barrier(a,"GOAL"))return true;if(a->NetIsHost()&&!s.pauseRequested)s.pauseRequested=Escape(a);
  if(a->mBoard->mPaused){s.samples=Samples(a);Advance(a,6);}
 }
 else if(s.stage==6)
 {
  if(!a->mBoard->mPaused||Samples(a)-s.samples<2||!Goal(a)||!Capture(a,"proof"))return true;
  const auto hash=Proof();char text[32];sprintf_s(text,"PROOF_%08x",hash);
  if(!s.localProof){s.localProof=true;s.checkpoint=Tag(text);++s.completed;NetTrace("SMOKE_CAMPAIGN356_CASE_COMPLETE role=%s level=%d epoch=%u hash=%08x waves=%zu authoredPlants=%zu authoredNPCs=%zu pots=%zu clientUse=1 conveyor=%d localProofBeforeReceipt=1 fullVictory=NotExecuted profileFixtureOnly=1 actorWrites=0 HPWrites=0 clockWrites=0 RNGWrites=0",Role(a),Level(),s.epoch,hash,s.waves.size(),s.authored.size(),s.missionEnemies.size(),s.pots.size(),IsNativeSourceConveyor308(a->mBoard));}
  Send(a,s.checkpoint);if(!Has(a,0,s.checkpoint)||!Has(a,1,s.checkpoint))return true;
  if(size_t(s.index+1)<levels.size()){Advance(a,7);return true;}
  NetTrace("SMOKE_CAMPAIGN356_COMPLETE role=%s cases=%d firstLevel=%d nativeFrames=%d profileEarned=0 fullNaturalWins=NotExecuted",Role(a),s.completed,levels[size_t(s.firstIndex)],s.completed*2);a->CoopSmokeWriteResult(true,nullptr);
 }
 return true;
}
}
