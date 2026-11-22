#include "FusionNativeBoss361Bridge.h"
#include "FusionNative.h"
#include "FusionNativeZombies.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeSourceLifecycle313.h"
#include "FusionNativeBossBall366.h"
#include "FusionNativeBossBungee366.h"
#include "FusionNativeBossRoom366.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeZombiePreflight280.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeProjection313.h"
#include "FusionNativeEffects269.h"
#include "FusionNativeCold.h"
#include "FusionNativeWater268.h"
#include "FusionNativeStar269.h"
#include "FusionNativeGarlic.h"
#include "FusionNativeDroppedCards269.h"
#include "FusionNativeConveyor308.h"
#include "FusionNativeSourceDamage315.h"
#include "FusionCatalog.h"
#include "../Plant.h"
#include "../Coin.h"
#include "../Net/NetPresentation.h"
#include "../Net/NetCommon.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include "../Board.h"
#include "../Zombie.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/Attachment.h"
#include "../../Sexy.TodLib/EffectSystem.h"
namespace LawnFusion
{
NativeBoss361State* EditNativeBoss361State(Zombie* z,bool create)
{
 const auto* data=GetNativeSourceZombieData(z);auto* c=z?GetNativeContext(z->mBoard):nullptr;
 if(!z||!z->mApp||!c||z->mApp->mBoard!=z->mBoard||c->adventureTeardown||!data||!FusionBoss361Kind(data->kind))return nullptr;
 const auto uid=z->mBoard->mZombies.DataArrayGetID(z);const bool ghost=z->mNetGhost||z->mApp->NetIsClient();
 if(!uid||(create&&ghost)||(!ghost&&data->instanceToken!=uid))return nullptr;
 const auto it=c->bossZombies361.find(uid);if(it!=c->bossZombies361.end())return &it->second;
 return create?&c->bossZombies361.try_emplace(uid).first->second:nullptr;
}
const NativeBoss361State* GetNativeBoss361State(const Zombie* z)
{return EditNativeBoss361State(const_cast<Zombie*>(z));}
bool PrepareNativeBoss361Clocks(Zombie* z,std::string* error)
{
 const auto* data=GetNativeSourceZombieData(z);auto* graph=EditNativeSourceGraph313(z,true);
 if(!data||!FusionBoss361Kind(data->kind)||z->mZombieType!=ZOMBIE_BOSS||!graph)return false;

 auto* body=z->mApp->ReanimationTryToGet(z->mBodyReanimID);auto* driver=z->mApp->ReanimationTryToGet(z->mSpecialHeadReanimID);
 if(!body||!body->TrackExists("Boss_head2")||!body->TrackExists("Boss_RV"))return false;
 auto* rv=graph->auxiliaryUIDs[0]?z->mApp->ReanimationTryToGet(ReanimationID(graph->auxiliaryUIDs[0])):nullptr;
 const unsigned missing=unsigned(!driver)+unsigned(!rv);
 if(missing){auto* fx=z->mApp->mEffectSystem;if(!fx||!fx->mReanimationHolder||!fx->mAttachmentHolder)return false;
  const auto& r=fx->mReanimationHolder->mReanimations;const auto& a=fx->mAttachmentHolder->mAttachments;
  if(!r.mBlock||!a.mBlock||r.mSize>r.mMaxSize||a.mSize>a.mMaxSize||missing>r.mMaxSize-r.mSize||missing>a.mMaxSize-a.mSize)return false;}
 // Client raw construction owns only its body. Whole-frame preflight reserves
 // both distinct source auxiliary clocks/attachments before any raw commit.
 if(!driver){ReanimatorEnsureDefinitionLoaded(REANIM_BOSS_DRIVER,false);driver=z->mApp->AddReanimation(0,0,0,REANIM_BOSS_DRIVER);if(!driver)return false;
  driver->PlayReanim("anim_idle",REANIM_LOOP,0,0);auto* attached=AttachReanim(body->GetTrackInstanceByName("Boss_head2")->mAttachmentID,driver,28,-84);if(!attached){driver->ReanimationDie();return false;}
  attached->mOffset.m00=attached->mOffset.m11=1.2f;attached->mDontDrawIfParentHidden=true;z->mSpecialHeadReanimID=z->mApp->ReanimationGetID(driver);}
 if(!rv){ReanimatorEnsureDefinitionLoaded(REANIM_BOSS,false);rv=z->mApp->AddReanimation(0,0,0,REANIM_BOSS);if(!rv)return false;rv->PlayReanim("anim_RV_1",REANIM_PLAY_ONCE_AND_HOLD,0,0);
  if(!AttachReanim(body->GetTrackInstanceByName("Boss_RV")->mAttachmentID,rv,0,0)){rv->ReanimationDie();return false;}}
 graph->auxiliaryUIDs[0]=uint32_t(z->mApp->ReanimationGetID(rv));graph->auxiliaryUIDs[1]=uint32_t(z->mApp->ReanimationGetID(driver));
 return true;
}
void RemoveNativeBoss361State(Zombie* z)
{
 auto* c=z?GetNativeContext(z->mBoard):nullptr;if(!c)return;
 const auto uid=z->mBoard->mZombies.DataArrayGetID(z);if(!uid)return;
 // Body/driver/RV are real attachments and are retired by the native owner.
 // Lease removal must happen before sourceGraphs313 is erased by central cleanup.
 if(auto* g=EditNativeSourceGraph313(z))
 {
  if(auto* r=z->mApp->ReanimationTryToGet(z->mBodyReanimID))RegisterNativeSourceGraphClock313(r,false);
  for(auto id:g->auxiliaryUIDs)if(auto* r=z->mApp->ReanimationTryToGet(ReanimationID(id)))RegisterNativeSourceGraphClock313(r,false);
 }
 c->bossZombies361.erase(uid);
}
namespace
{
using Op=NativeBoss361Operation;
bool World(const Board* b,uint16_t kind)
{return FusionBoss361Kind(kind)&&b&&IsNativeSourceWorld313(b)&&GetNativeFusionRowCount(b)==5;}
void Failure(Board* b,const char* text)
{if(auto* c=GetNativeContext(b)){c->adventure.schedulerFailed=true;c->adventureScheduler.failed=true;}LawnNet::NetTrace("Boss361 refused: %s",text);}
NativeSourceGraph313State* GraphState(Zombie* z){return EditNativeSourceGraph313(z,true);}
Reanimation* Clock(Zombie* z,uint8_t layer)
{if(!z||!z->mApp)return nullptr;if(!layer)return z->mApp->ReanimationTryToGet(z->mBodyReanimID);const auto* g=GetNativeSourceGraph313(z);return g&&layer<=2?z->mApp->ReanimationTryToGet(ReanimationID(g->auxiliaryUIDs[layer-1])):nullptr;}
bool LayerActive(Zombie* z,uint8_t layer)
{const auto* s=GetNativeBoss361State(z);return s&&(layer!=1||s->view.rvVisible);}
bool Defined(Board*,std::span<uint16_t> out,uint16_t& count)
{constexpr uint16_t genuineLoaded[]={0,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49};uint16_t n=0;for(uint16_t k:genuineLoaded){if(n>=out.size())return false;out[n++]=k;}count=n;return true;}
bool Facts(Zombie* z,NativeBoss361Facts& f)
{
 const auto* d=GetNativeSourceZombieData(z);const auto* s=GetNativeBoss361State(z);const auto* room=GetNativeBossRoom366(z->mBoard);if(!d||!s||!room)return false;
 f={};f.sourceHealth=d->health;f.maximumHealth=z->mBodyMaxHealth;f.boardBoss2=room->boss2;
 f.input.gameActive=!z->mBoard->mPaused&&CanApplyNativeFusionGameplay(z->mBoard);
 f.input.freezeSpeed=z->mIceTrapCounter?0.f:1.f;
 const auto* garlic=GetNativeZombieGarlicData(z);
 if(!TryGetFusionZombieStatusSpeedFactor(0,z->mChilledCounter>0,z->mIceTrapCounter>0,z->mButteredCounter>0,
 garlic&&FusionGarlicStopsWalking(*garlic)&&!NativeSourceDeathIgnoresGarlic313(z),f.mainAnimatorSpeed))return false;
 f.mainAnimatorSpeed*=GetNativeWater268GrapSpeedFactor(z);
 Plant* p=nullptr;while(z->mBoard->mPlants.IterateNext(p))if(!p->mDead&&p->mIsOnBoard&&!p->mSquished&&p->mPlantCol>=6&&GetNativePlantData(p)&&!NativeSourcePlantInvincible366(p))
 {f.input.firstCrushPlantRow=p->mRow;break;}
 if(IsNativeSourceConveyor308(z->mBoard))for(const auto& packet:GetNativeContext(z->mBoard)->selection.State().packets){if(packet.kind==16)f.input.droppedCard16=true;if(packet.kind==10)f.input.droppedCard10=true;}
 Coin* c=nullptr;while(z->mBoard->mCoins.IterateNext(c))if(!c->mDead)if(const auto* card=GetNativeDroppedCard269Data(c))
 {if(card->kind==16)f.input.droppedCard16=true;if(card->kind==10)f.input.droppedCard10=true;}
 return std::isfinite(f.mainAnimatorSpeed)&&f.mainAnimatorSpeed>=0;
}
bool Node(Zombie* z,const char* path,float& x,float& y){return GetNativeSourceZombie269NodePoint(z,path,x,y);}
bool TargetGround(Zombie* z,int row,float x,float& y)
{float groundX=0,groundY=0,sourceX=0,sourceY=0,land=0;if(!NativeSourceGroundAnchor313(z,groundX,groundY)||
 !NativeBoardPointToSource313(z->mBoard,z->mRow,groundY,x,groundY,sourceX,sourceY)||!NativeSourceRowLandY313(z->mBoard,sourceX,row,land))return false;
 return NativeSourcePointToBoard313(z->mBoard,row,z->mBoard->GetPosYBasedOnRow(x,row),sourceX,land,groundX,y);}
bool BallRequest(Zombie* z,const FusionBoss361View& v,FusionSourceObstacle313Spawn& request,float& x,float& y,std::string* error)
{float land=0;const auto* room=GetNativeBossRoom366(z->mBoard);return room&&Node(z,"Zombie_boss_jaw",x,y)&&TargetGround(z,v.targetRow,x,land)&&
 MakeNativeBossBall366Request(z,v.ballType,v.targetRow,x,y,land,room->fastBoard,Board::MakeRenderOrder(RENDER_LAYER_ZOMBIE,v.targetRow,0),request,error);}
bool ChildPool(Board* b,bool boss2,std::vector<uint16_t>& out)
{uint16_t defined[99]{},pool[99]{},n=0,count=0;if(!Defined(b,defined,n)||!FusionBoss361BuildChildPool(boss2,std::span(defined,n),pool,count))return false;out.assign(pool,pool+count);return !out.empty();}
bool Ensure(Board* b,uint16_t kind,std::string* error)
{
 if(!World(b,kind)||GetNativeSourceZombieCarrier(kind)!=ZOMBIE_BOSS||!EnsureNativeSourceZombieVisuals(b,kind,error)||!EnsureNativeBossRoom366(b,error))return false;
 ReanimatorEnsureDefinitionLoaded(REANIM_BOSS,false);ReanimatorEnsureDefinitionLoaded(REANIM_BOSS_DRIVER,false);
 for(uint16_t sound:{8,9,43,74,84,112,118,119,120,121})if(!LawnNet::NetSourceSound284Ready(b->mApp,sound))return false;
 for(uint16_t fx:{75,76,77})if(!EnsureNativeSourceEffect269(b,fx,error))return false;
 const auto* room=GetNativeBossRoom366(b);std::vector<uint16_t> children;
 if(!room||!ChildPool(b,room->boss2,children))return false;children.push_back(45);
 return EnsureNativeSourceZombieClosure280(b,children,error);
}
bool Preflight(Zombie* z,Op op,const NativeBoss361Action& a,std::string* error)
{
 auto* b=z->mBoard;const auto* c=GetNativeContext(b);if(!c||c->adventureTeardown)return false;
 switch(op)
 {
 case Op::Initialize:return EnsureNativeBossRoom366(b,error);
 case Op::SpawnChild:
 {const auto* room=GetNativeBossRoom366(b);std::vector<uint16_t> kinds;if(!room||!ChildPool(b,room->boss2,kinds))return false;
  if(a.view.summonCount<5)kinds={0};else if(a.view.summonCount<8)kinds={2};
  // One random child, not one of each. Prove each possible alternative before RNG.
  for(auto kind:kinds)if(!CanAllocateNativeSourceZombieBatch280(b,std::span(&kind,1),error))return false;float x=0,y=0;return Node(z,"Zombie_boss_outerarm_finger1_2",x,y);}
 case Op::BungiTriple:return CanEmitNativeBossBungeeTriple366(z,error);
 case Op::PutBall:{FusionSourceObstacle313Spawn r;float x=0,y=0;return BallRequest(z,a.view,r,x,y,error);}
 case Op::Stomp:case Op::RvCrushAndCards:return PreflightNativeBossCrush366(z,a.view,op==Op::RvCrushAndCards,error);
 case Op::DeathBegin:
 {std::vector<uint32_t> ids;if(!GetNativeSourceZombieBirthOrder313(b,ids))return false;
  std::erase_if(ids,[&](uint32_t uid){const auto* d=GetNativeSourceZombieData(b->mZombies.DataArrayTryToGet(uid));return !d||FusionBoss361Kind(d->kind);});
  FusionSourceDeathBudget319 award;award.coins=1;return CanRetireNativeSourceZombieBatch313(b,ids,1,award,error);}
 case Op::DeathEnd:return CanCreateNativeBossTrophy366(b,error);
 case Op::DieExplode:return EnsureNativeSourceEffect269(b,75,error);
 case Op::GrantBodyTools:return EnsureNativeBossRoom366(b,error);
 default:return true;
 }
}
void Reset(Zombie* z,bool all)
{
 z->mIceTrapCounter=0;if(auto* c=GetNativeZombieColdData(z)){c->level=0;c->frozenTicks=0;if(all)c->chilledTicks=0;}
 if(all){z->mChilledCounter=0;z->mButteredCounter=0;if(auto* butter=GetNativeZombieButterData(z)){butter->ticks=0;butter->visible=false;}}
}
bool Apply(Zombie* z,Op op,const NativeBoss361Action& a)
{
 auto* b=z->mBoard;auto* app=z->mApp;const uint32_t epoch=GetNativeContext(b)->epoch,uid=b->mZombies.DataArrayGetID(z);
 const auto still=[&](){return app->mBoard==b&&GetNativeContext(b)&&GetNativeContext(b)->epoch==epoch&&!GetNativeContext(b)->adventureTeardown;};
 const auto sound=[&](uint16_t id){return LawnNet::NetPlaySourceSound284(app,id,.5f);};
 switch(op)
 {
 case Op::Initialize:{auto* room=EditNativeBossRoom366(b);if(!room)return false;room->enabled=true;return true;}
 case Op::Decision:return a.decision!=FusionBoss361Decision::HeadEnter||sound(119);
 case Op::AlignRv:
 {float rootX=0,rootY=0,rvX=0,rvY=0,shadowX=0,shadowY=0;if(!Node(z,"",rootX,rootY)||!Node(z,"BossRv",rvX,rvY)||!Node(z,"BossRv/Shadow",shadowX,shadowY))return false;
  auto* s=EditNativeBoss361State(z);if(!s)return false;const float basis=60*z->mScaleZombie;
  const float x=float(b->GetCellCenterX(a.view.targetColumn,a.view.targetRow));const float y=float(b->GridToPixelY(a.view.targetColumn,a.view.targetRow)+50);
  s->view.rvOffsetX=(rvX+x-shadowX-rootX)/basis;s->view.rvOffsetY=(rootY-rvY-y+shadowY)/basis;s->view.rvAligned=true;return true;}
 case Op::SpawnChild:
 {float x=0,y=0;if(!Node(z,"Zombie_boss_outerarm_finger1_2",x,y))return false;NativeSourceZombieSpawnRequest r;r.kind=a.childKind;r.row=a.view.targetRow;r.fromWave=0;r.positioned=true;r.shadowX=x;return SpawnNativeSourceZombie(b,r)!=nullptr;}
 case Op::BungiTriple:return EmitNativeBossBungeeTriple366(z);
 case Op::Stomp:case Op::RvCrushAndCards:return ApplyNativeBossCrush366(z,a.view,op==Op::RvCrushAndCards);
 case Op::PutBall:{FusionSourceObstacle313Spawn r;float x=0,y=0;return BallRequest(z,a.view,r,x,y,nullptr)&&EmitNativeBossBall366(z,r,x,y);}
 case Op::FootSound:b->ShakeBoard(2,-2);return sound(74);
 case Op::MachineSound2:return sound(118);
 case Op::DieExplode:
 {if(!sound(43))return false;const float sx=RandRangeFloat(4,9),sy=RandRangeFloat(-1,5);float gx=0,gy=0,x=0,y=0;
  if(!NativeSourceGroundAnchor313(z,gx,gy)||!NativeSourcePointToBoard313(b,z->mRow,gy,sx,sy,x,y))return false;
  SpawnNativeSourceEffect269(b,75,x,y,Board::MakeRenderOrder(RENDER_LAYER_COIN_BANK,0,30000),true);b->ShakeBoard(5,-5);return true;}
 case Op::ResetDebuffs:Reset(z,true);return true;
 case Op::ClearFreeze:Reset(z,false);return true;
 case Op::GrantBodyTools:return GrantNativeBossTools366(b,a.payload);
 case Op::DeathBegin:
 {if(!RetireNativeSourceZombieBalls366(b))return false;Reset(z,true);if(!sound(112))return false;b->mCurrentWave=b->mNumWaves;
  auto* room=EditNativeBossRoom366(b);if(!room)return false;++room->enemyCount;
  for(int pass=0;pass<2;++pass)
  {std::vector<uint32_t> ids;if(!GetNativeSourceZombieBirthOrder313(b,ids))return false;
   for(auto it=ids.rbegin();it!=ids.rend();++it){if(!still())return true;auto* other=b->mZombies.DataArrayTryToGet(*it);const auto* d=GetNativeSourceZombieData(other);if(other&&!other->mDead&&d&&!FusionBoss361Kind(d->kind)&&!RetireNativeSourceZombie313(other,1))return false;}}
  return true;}
 case Op::DeathEnd:
 {auto* room=EditNativeBossRoom366(b);if(!room)return false;--room->enemyCount;if(!CreateNativeBossTrophy366(z))return false;
  if(!still())return true;z=b->mZombies.DataArrayTryToGet(uid);if(z){return RetireNativeSourceZombie313(z,2);}return true;}
 }
 return false;
}
bool Params(Zombie* z,FusionSourceGraph313Parameters& p){return NativeBoss361Parameters(z,0,p,GetNativeBoss361Hooks());}
bool LayerParams(Zombie* z,uint8_t layer,FusionSourceGraph313Parameters& p){return NativeBoss361Parameters(z,layer,p,GetNativeBoss361Hooks());}
bool GraphPreflight(Zombie* z,FusionSourceGraph313Event e,std::string* error){return PreflightNativeBoss361Event(z,e,GetNativeBoss361Hooks(),error);}
bool GraphEvent(Zombie* z,FusionSourceGraph313Event e,int value){return DispatchNativeBoss361Event(z,e,value,GetNativeBoss361Hooks());}
bool RootDelta(Zombie*,uint8_t,FusionSourceGraph313Cursor,FusionSourceGraph313Cursor,double& value){value=0;return true;}
bool MoveRoot(Zombie*,double value){return value==0;}
bool NullReady(Zombie*,uint8_t,uint8_t){return true;}
}
const NativeSourceGraph313Hooks& GetNativeBoss361GraphHooks()
{static const NativeSourceGraph313Hooks h{GraphState,Clock,Params,GraphPreflight,GraphEvent,RootDelta,MoveRoot,NullReady,Failure,LayerParams,LayerActive};return h;}
const NativeBoss361Hooks& GetNativeBoss361Hooks()
{static const NativeBoss361Hooks h{[](Zombie* z){return EditNativeBoss361State(z,true);},&GetNativeBoss361GraphHooks(),World,Ensure,Facts,Defined,Preflight,Apply,
 [](Board*,int lo,int hi){return hi>lo?RandRangeInt(lo,hi-1):lo;},Failure};return h;}
bool CanSpawnNativeBoss361Actor(Board* b,uint16_t kind,std::string* error)
{return CanSpawnNativeBoss361(b,kind,GetNativeBoss361Hooks(),error);}
bool InitializeNativeBoss361Actor(Zombie* z)
{auto* s=EditNativeBoss361State(z,true);if(!s||!PrepareNativeBoss361Clocks(z)||!InitializeNativeBoss361(z,*s,GetNativeBoss361Hooks()))return false;z->mZombiePhase=PHASE_ZOMBIE_NORMAL;return true;}
bool UpdateNativeBoss361Actor(Zombie* z)
{auto* s=EditNativeBoss361State(z);return s&&UpdateNativeBoss361(z,*s,GetNativeBoss361Hooks());}
bool AdoptNativeBoss361Actor(Zombie* z,const FusionBoss361View& view)
{if(!z||!z->mApp||!z->mApp->NetIsClient()||!FusionBoss361ViewValid(view)||!GetNativeSourceZombieData(z)||GetNativeSourceZombieData(z)->kind!=view.kind)return false;
 auto* c=GetNativeContext(z->mBoard);if(!c)return false;const auto uid=z->mBoard->mZombies.DataArrayGetID(z);auto& s=c->bossZombies361[uid];s.view=view;s.localUID=uid;s.initialized=true;s.failed=false;return PrepareNativeBoss361Clocks(z);}
bool ApplyNativeBoss361ActorBodyDamage(Zombie* z,int resolved){return ApplyNativeBoss361BodyDamage(z,resolved,GetNativeBoss361Hooks());}
bool BeginNativeBoss361ActorDeath(Zombie* z){return BeginNativeBoss361Death(z,GetNativeBoss361Hooks());}
bool GetNativeBoss361Semantics(const Zombie* z,int& status,bool& collider)
{const auto* s=GetNativeBoss361State(z);if(!s||!s->initialized||s->failed)return false;status=s->view.sourceStatus;collider=s->view.colliderEnabled;return true;}
}
