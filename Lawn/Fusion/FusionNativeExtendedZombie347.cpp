#include "FusionNativeExtendedZombie347.h"
#include "FusionNativeExtendedZombie313Bridge.h"
#include "FusionNative.h"
#include "FusionNativeZombies.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeSourceDamage315.h"
#include "FusionNativeSourceLifecycle313.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeZombiePreflight280.h"
#include "FusionNativeProjection313.h"
#include "FusionNativeSourceObstacleFactory313.h"
#include "FusionNativeObstacle313.h"
#include "FusionNativeDrown313Bridge.h"
#include "FusionNativeJackbox309Bridge.h"
#include "FusionNativeEquipment.h"
#include "FusionNativeBasePlants.h"
#include "FusionNativeGarlic.h"
#include "FusionNativeDefender.h"
#include "FusionNativePlantDamage.h"
#include "FusionBasePlantRules.h"
#include "FusionNativeEffects269.h"
#include "FusionNativeSourceGraph313.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace LawnFusion
{
namespace
{
uint16_t Kind(const Zombie* z){const auto* d=GetNativeSourceZombieData(z);return d?d->kind:0xffff;}
bool Host(const Zombie* z){return z&&z->mApp&&IsNativeSourceWorld313(z->mBoard)&&!z->mNetGhost&&!z->mApp->NetIsClient()&&FusionExtendedZombie347Kind(Kind(z));}
struct Owner
{
 LawnApp* app;Board* board;uint32_t uid;uint16_t kind;
 explicit Owner(Zombie* z):app(z->mApp),board(z->mBoard),uid(board->mZombies.DataArrayGetID(z)),kind(Kind(z)){}
 Zombie* Get()const{if(app->mBoard!=board)return nullptr;auto* z=board->mZombies.DataArrayTryToGet(uid);const auto* d=GetNativeSourceZombieData(z);return z&&!z->mDead&&d&&d->kind==kind&&d->instanceToken==uid?z:nullptr;}
};
bool Reject(const char* why,std::string* e){if(e)*e=why;return false;}
bool ReadyEffect(Board* b,uint16_t source,std::string* e){return EnsureNativeSourceEffect269(b,source,e);}
bool Explosion(Zombie* z,int row,float sx,float sy,NativeJackbox300Explosion& out)
{
 float gx=0,gy=0;if(!z||!GetNativeSourceZombieData(z)||!NativeSourceGroundAnchor313(z,gx,gy))return false;
 NativeJackbox300Explosion e;e.ownerId=z->mNetId;e.ownerToken=GetNativeSourceZombieData(z)->instanceToken;e.sourceKind=Kind(z);e.row=row;e.fromWave=z->mFromWave;e.controlled=z->mMindControlled;e.renderOrder=z->mRenderOrder;
 if(!NativeSourcePointToBoard313(z->mBoard,row,gy,sx,sy,e.centerX,e.centerY))return false;
 e.shadowX=e.centerX;e.shadowY=e.centerY;out=e;return true;
}
bool BirthPlan(Zombie* z,int level,std::vector<FusionExtended347Birth>& out)
{
 const auto* s=GetNativeExtendedZombie313(z);float x=0,y=0;
 return s&&NativeSourcePoint313(z,"axis",x,y)&&PlanFusionExtended347Births(Kind(z),s->sourceStatus,level,GetNativeFusionRowCount(z->mBoard),z->mBoard->GetColumnCount(),z->mMindControlled,x,out);
}
bool BirthBudget(Board* b,const std::vector<FusionExtended347Birth>& children,FusionSourceDeathBudget319& peak,std::string* e)
{std::vector<uint16_t> kinds;for(const auto& child:children)kinds.push_back(child.kind);return kinds.empty()||(GetNativeSourceZombieBirthBudget319(b,kinds,peak,e)&&CanAllocateNativeSourceBudget319(b,peak,e));}
bool StartChildPop(Zombie* child)
{
 auto* s=EditNativeJackbox309(child);if(!s||!s->initialized||s->failed||!s->boxPresent)return false;
 const auto& h=GetNativeJackbox309Hooks();std::string error;
 if(!h.preflight(child,NativeJackbox300Operation::Pop,nullptr,false,&error))return false;
 // Exact Jackbox.Pop contract: countdown0, pop trigger, status11,
 // sound90/91 and pause its genuine source loop. The normal controller then
 // enters its real Pop phase; no synthetic clock advance or instant blast.
 s->countdown=0;s->popTriggered=true;s->audioPlaying=false;
 const int sound=h.randomInt(child->mBoard,90,92);if(sound<90||sound>=92)return false;
 h.sound(child,uint16_t(sound),.5f);h.audio(child,NativeJackbox300Audio::Pause);return true;
}
bool Births(Zombie* z,int level)
{
 std::vector<FusionExtended347Birth> plan;if(!BirthPlan(z,level,plan))return false;Owner owner(z);
 for(const auto& entry:plan)
 {
  z=owner.Get();if(!z)return true;float sx=entry.sourceX,sy=0;
  if(entry.column>=0)
  {const float bx=float(z->mBoard->GetCellCenterX(entry.column,entry.row));float ground=0,unused=0;if(!NativeSourceGroundAnchor313(z,unused,ground)||!NativeBoardPointToSource313(z->mBoard,entry.row,ground,bx,ground,sx,sy))return false;}
  NativeSourceZombieSpawnRequest request;request.kind=entry.kind;request.row=entry.row;request.friendly=entry.controlled;
  request.positioned=true;request.shadowX=float(z->mBoard->GetCellCenterX(0,entry.row)+(sx+4.8f)*kFusionBasePixelsX);
  auto* born=SpawnNativeSourceZombie(z->mBoard,request);if(!born)return false;
  if(entry.popImmediately&&!StartChildPop(born))return false;
  if(entry.column>=0)
  {float x=0,y=0;if(!NativeSourcePoint313(born,"",x,y)||!GetNativeExtendedZombie313Hooks().effect(born,11,x,y))return false;}
 }
 return true;
}
}
int NativeUltimateFootballDrownCount343(const Board* b)
{const auto* c=GetNativeContext(b);return c&&IsNativeSourceWorld313(b)?c->extendedBoard313.ultimateFootballDrownCount:0;}
bool CommitNativeExtended347Factory(Zombie* z)
{
 if(!Host(z)||Kind(z)!=218||z->mMindControlled)return Kind(z)!=218||z&&z->mMindControlled;
 auto* s=EditNativeExtendedZombie313(z);auto* c=GetNativeContext(z->mBoard);
 if(!s||!s->initialized||s->failed||!c||s->boardCounted347)return s&&s->boardCounted347;
 if(c->extendedBoard313.ultimateFootballDrownCount==(std::numeric_limits<int32_t>::max)())return false;
 ++c->extendedBoard313.ultimateFootballDrownCount;s->boardCounted347=true;return true;
}
bool EnsureNativeExtended347Resources(Board* b,uint16_t kind,std::string* e)
{
 if(!IsNativeSourceWorld313(b)||!FusionExtendedZombie347Kind(kind))return false;
 if(!EnsureNativeSourceZombieVisuals(b,kind,e)||!CanQueryNativeSourceZombieRoot273(b,kind))return false;
 for(auto child:FusionExtended347Dependencies(kind))
 {if(FusionExtendedZombie347Kind(child)){if(!EnsureNativeExtended347Resources(b,child,e))return false;}else if(!EnsureNativeSourceZombieClosure280(b,std::span(&child,1),e))return false;}
 for(uint16_t sound:{3,4,43,53,74,76,90,91})if(!LawnNet::NetSourceSound284Ready(b->mApp,sound))return Reject("347 genuine source sound unavailable",e);
 if((kind==205||kind==206||kind==221)&&!ReadyEffect(b,34,e))return false;
 if((kind==210||kind==219)&&(!ReadyEffect(b,54,e)||!CanCreateNativeEquipmentVisual(b,FusionEquipmentType::SuperMachine)))return false;
 if((kind==218||kind==220||kind==221||kind==224)&&(!ReadyEffect(b,11,e)||!ReadyEffect(b,42,e)))return false;
 if(kind==202||kind==218)return EnsureNativeSourceObstacle313Resources(b,FusionSourceObstacle313Kind::DrownWeapon,kind==218?3:0,e);
 if(kind==206)return EnsureNativeSourceObstacle313Resources(b,FusionSourceObstacle313Kind::KirovBomb,1,e);
 if(kind==221)return EnsureNativeSourceObstacle313Resources(b,static_cast<FusionSourceObstacle313Kind>(5),0,e);
 return true;
}
bool PreflightNativeExtended347(Zombie* z,NativeExtended347Operation op,std::string* e)
{
 if(!Host(z)||!EnsureNativeExtended347Resources(z->mBoard,Kind(z),e))return false;
 const auto kind=Kind(z);FusionSourceDeathBudget319 peak;std::vector<FusionExtended347Birth> births;
 if(op==NativeExtended347Operation::Attribute&&(kind==218||kind==221))
 {if(!BirthPlan(z,0,births)||!BirthBudget(z->mBoard,births,peak,e))return false;}
 if(op==NativeExtended347Operation::Pop&&kind==224)
 {
  // Both possible real child groups are ready BEFORE selecting a random branch.
  // They are alternatives, so reserve their component-wise maximum, not sum.
  FusionSourceDeathBudget319 alternatives[2];
  for(int branch=0;branch<2;++branch)if(!BirthPlan(z,branch,births)||!BirthBudget(z->mBoard,births,alternatives[branch],e))return false;
  peak.zombies=(std::max)(alternatives[0].zombies,alternatives[1].zombies);peak.reanimations=(std::max)(alternatives[0].reanimations,alternatives[1].reanimations);peak.attachments=(std::max)(alternatives[0].attachments,alternatives[1].attachments);
 }
 if(op==NativeExtended347Operation::Throw&&(kind==205||kind==220))
 {const uint16_t child=kind==205?206:204;if(!GetNativeSourceZombieBirthBudget319(z->mBoard,std::span(&child,1),peak,e))return false;}
 if(op==NativeExtended347Operation::Throw&&(kind==202||kind==218||kind==206||kind==221))++peak.projectiles;
 if(op==NativeExtended347Operation::Death&&(kind==210||kind==219))
 {if(!CanCreateNativeEquipment(z->mBoard,FusionEquipmentType::SuperMachine))return false;++peak.coins;}
 if(!CanAllocateNativeSourceBudget319(z->mBoard,peak,e))return false;
 if((kind==224&&(op==NativeExtended347Operation::Pop||op==NativeExtended347Operation::Death))||kind==221&&op==NativeExtended347Operation::Throw)
 {
  float x=0,y=0;if(!NativeSourcePoint313(z,"axis",x,y))return false;
  const int first=op==NativeExtended347Operation::Death?z->mRow:0,last=op==NativeExtended347Operation::Death?z->mRow+1:GetNativeFusionRowCount(z->mBoard);
  for(int row=first;row<last;++row){NativeJackbox300Explosion explosion;float land=0;if(!NativeSourceRowLandY313(z->mBoard,x,row,land)||!Explosion(z,row,x,land+.5f,explosion)||!CanEmitNativeSourceZombieExplosion343(z->mBoard,explosion,e,kind==221?2:0))return false;}
 }
 return true;
}
bool InitializeNativeExtended347Fields(Zombie* z,NativeExtendedZombie313State& s,const NativeExtendedZombie313Hooks& h)
{
 const auto kind=Kind(z);if(!Host(z))return false;
 if(kind==205||kind==206){s.sourceStatus=10;s.bombPresent=true;GetNativeSourceZombieData(z)->sourceStatusLevel=kind==205?-10:7;}
 if(kind==202||kind==218)
 {const int a=h.randomInt(z->mBoard,3,6);if(a<3||a>=6)return false;s.throwTime=float(a);if(kind==218){const int b=h.randomInt(z->mBoard,2,4);if(b<2||b>=4)return false;s.throwTime=float(b);s.bombPresent=true;}}
 s.attributeCountdown=kind==218?1.5f:kind==220||kind==221||kind==224?3.f:0;
 if(kind==220)s.attackDamage=200;
 if(kind==210||kind==219)h.sound(z,53,.5f);if(kind==205)h.sound(z,76,1);
 return true;
}
bool ResolveNativeExtended347Damage(Zombie* z,int raw,bool fix,int& resolved)
{
 auto* s=EditNativeExtendedZombie313(z);if(!Host(z)||!s)return false;FusionExtended347DamagePlan p;
 if(!TryPlanFusionExtended347Damage(Kind(z),s->sourceStatus,z->mMindControlled,raw,s->attributeCountdown,p))return false;
 if(p.ignored){resolved=0;return true;}
 if(!ResolveNativeSourceBaseDamage315(z,p.raw,fix,resolved))return false;
 resolved=(std::min)(resolved,p.maximumResolved);s->attributeCountdown=p.attributeCountdown;return true;
}
bool ApplyNativeExtended347Charred(Zombie* z,int raw,bool fix)
{
 const auto kind=Kind(z);if(kind!=218&&kind!=219&&kind!=220&&kind!=221&&kind!=224)return false;
 return !Host(z)||ApplyNativeSourceTypedDamage313(z,10,raw,fix);
}
int NativeExtended347AttackDamage(const Zombie* z,int fallback)
{const auto* s=GetNativeExtendedZombie313(z);return Kind(z)==220&&s&&s->attackDamage>=200?s->attackDamage:fallback;}
bool UpdateNativeExtended347Fields(Zombie* z,NativeExtendedZombie313State& s,const NativeExtendedZombie313Hooks& h)
{
 if(!Host(z)||z->mDead)return false;const auto kind=Kind(z);Owner owner(z);
 if(s.attributeCountdown>0)
 {
  if(s.attributeCountdown<=.01f&&!PreflightNativeExtended347(z,NativeExtended347Operation::Attribute))return false;
  s.attributeCountdown=(std::max)(0.f,s.attributeCountdown-.01f);
  if(s.attributeCountdown==0)
  {
   FusionExtended347AttributePlan p;const auto* d=GetNativeSourceZombieData(z);
   if(!TryPlanFusionExtended347Attribute(kind,s.sourceStatus,d->originSpeed,kind==220?s.attackDamage:0,z->mHelmHealth,z->mHelmMaxHealth,p))return false;
   if((kind==218||kind==221)&&!Births(z,0))return false;z=owner.Get();if(!z)return true;
   auto* current=EditNativeExtendedZombie313(z);if(!current)return false;current->attributeCountdown=p.seconds;
   if(kind==220){GetNativeSourceZombieData(z)->originSpeed=p.originSpeed;current->attackDamage=p.attackDamage;if(z->mHelmHealth>0){z->mHelmHealth=p.firstArmorHealth;current->firstArmorStage=FusionExtended347ArmorStage(kind,z->mHelmHealth,z->mHelmMaxHealth,current->firstArmorStage);}}
   if(p.pop){const int branch=h.randomInt(z->mBoard,0,3);if(branch<0||branch>=3)return false;GetNativeSourceZombieData(z)->sourceStatusLevel=branch;h.graph.stateFor(z)->parameters.triggers|=FusionSourceGraph313Trigger(FusionSourceGraph313Param::Pop);}
   if(p.shoot)h.graph.stateFor(z)->parameters.triggers|=FusionSourceGraph313Trigger(FusionSourceGraph313Param::Shoot);
  }
 }
 z=owner.Get();if(!z)return true;auto* current=EditNativeExtendedZombie313(z);if(!current)return false;
 if((kind==202||kind==218)&&current->throwTime>0&&!current->isThrow&&!z->mMindControlled&&current->sourceStatus!=1)
 {current->throwTime-=.01f;if(current->throwTime<=0){if(!PreflightNativeExtended347(z,NativeExtended347Operation::Throw))return false;current->isThrow=true;h.graph.stateFor(z)->parameters.triggers|=FusionSourceGraph313Trigger(FusionSourceGraph313Param::Throw);}}
 if((kind==202||kind==218)&&!current->isThrow&&!z->mMindControlled)
 {
  std::vector<NativeExtendedZombie313Ref> hits;if(!h.bodyContacts(z,hits))return false;
  for(auto hit:hits)if(!hit.zombie){auto* p=z->mBoard->mPlants.DataArrayTryToGet(hit.localUID);if(!p||p->mDead||p->mRow!=z->mRow)continue;bool nut=false,big=false,cal=false,umbrella=false,bitten=false;int level=0;if(!h.plantTraits(p,nut,big,level,cal,umbrella,bitten))return false;if(!cal&&!z->mBoard->GetLadderAt(p->mPlantCol,p->mRow)){if(!PreflightNativeExtended347(z,NativeExtended347Operation::Throw))return false;current->isThrow=true;h.graph.stateFor(z)->parameters.triggers|=FusionSourceGraph313Trigger(FusionSourceGraph313Param::Throw);break;}}
 }
 if(kind==206)
 {NativeExtendedZombie313Input input;if(!h.input(z,input)||!h.setVelocity(z,(z->mMindControlled?.5f:-.5f)*input.sourceSpeed,0))return false;}
 if(kind==221&&current->sourceStatus==0&&z->mBoard->mMainCounter%2==0)
 {
  std::vector<NativeExtendedZombie313Ref> contacts;if(!h.bodyContacts(z,contacts))return false;
  for(const auto& contact:contacts){bool reacted=false;if(!PreflightNativeExtended347(z,NativeExtended347Operation::Convert)||!h.eatContact(z,contact,reacted))return false;z=owner.Get();if(!z)return true;}
 }
 return true;
}
bool NativeExtended347Bomb(Zombie* z)
{
 if(!Host(z)||!PreflightNativeExtended347(z,NativeExtended347Operation::Throw))return false;
 const auto kind=Kind(z);if(kind==205)
 {float sx=0,sy=0;if(!NativeSourcePoint313(z,"Shadow",sx,sy))return false;NativeSourceZombieSpawnRequest r;r.kind=206;r.row=z->mRow;r.friendly=z->mMindControlled;r.positioned=true;r.shadowX=float(z->mBoard->GetCellCenterX(0,z->mRow)+(sx+4.8f)*kFusionBasePixelsX);return SpawnNativeSourceZombie(z->mBoard,r)!=nullptr;}
 if(kind!=206)return false;
 FusionSourceObstacle313Spawn request;float x=0,y=0;if(!GetNativeSourceZombie269NodePoint(z,"Superbomb_1",x,y))return false;
 request.kind=FusionSourceObstacle313Kind::KirovBomb;request.variant=1;request.ownerId=z->mNetId;request.ownerToken=GetNativeSourceZombieData(z)->instanceToken;request.row=z->mRow;request.centerX=x;request.centerY=y;request.fromHypno=z->mMindControlled;float unused=0;if(!NativeSourceGroundAnchor313(z,unused,request.groundY))return false;request.renderOrder=Board::MakeRenderOrder(RENDER_LAYER_ZOMBIE,z->mRow,1);
 return SpawnNativeSourceObstacle313(z->mBoard,request)!=nullptr;
}
bool NativeExtended347MachineDeath(Zombie* z)
{
 if(!Host(z)||!PreflightNativeExtended347(z,NativeExtended347Operation::Death))return false;
 const auto& h=GetNativeExtendedZombie313Hooks();float sx=0,sy=0,bx=0,by=0;
 if(!NativeSourcePoint313(z,"axis",sx,sy)||!GetNativeSourceZombie269NodePoint(z,"Shadow",bx,by))return false;
 h.sound(z,43,.5f);if(!h.effect(z,54,sx,sy+.6f))return false;
 return CreateNativeEquipment(z->mBoard,FusionEquipmentType::SuperMachine,bx,by-float(GetNativeFusionPixelsY(z->mBoard)),true)!=nullptr;
}
namespace
{
bool BiteWorld354(const NativeExtended347BiteCapture& c)
{
 if(!c.app||!c.board||c.app->mBoard!=c.board)return false;
 const auto* context=GetNativeContext(c.board);return context&&context->epoch==c.epoch&&IsNativeSourceWorld313(c.board);
}
Zombie* BiteOwner354(const NativeExtended347BiteCapture& c)
{
 if(!BiteWorld354(c))return nullptr;auto* z=c.board->mZombies.DataArrayTryToGet(c.ownerUID);const auto* d=GetNativeSourceZombieData(z);
 return z&&!z->mDead&&Host(z)&&d&&d->kind==220&&d->instanceToken==c.ownerToken?z:nullptr;
}
Plant* BiteTarget354(const NativeExtended347BiteCapture& c,bool requireSource)
{
 if(!BiteWorld354(c))return nullptr;auto* p=c.board->mPlants.DataArrayTryToGet(c.targetUID);if(!p||p->mDead)return nullptr;
 const auto* d=GetNativePlantData(p);
 if((requireSource&&!d)||(d&&d->instanceToken!=c.targetToken))return nullptr;
 return p;
}
bool BiteFailure354(const NativeExtended347BiteCapture& c,const char* why)
{if(BiteWorld354(c))GetNativeExtendedZombie313Hooks().graph.failed(c.board,why);return false;}
bool FinishBite354(const NativeExtended347BiteCapture& c)
{
 if(!BiteOwner354(c))return true;
 NativeSourceZombieSpawnRequest request;request.kind=204;request.row=c.row;request.positioned=true;request.shadowX=c.shadowX;
 if(!SpawnNativeSourceZombie(c.board,request))return BiteFailure354(c,"Prepared source220 child birth failed after common bite; no replay.");
 if(!BiteWorld354(c))return true;
 SpawnNativeSourceEffect269(c.board,11,c.shadowX,c.shadowY-float(.5*GetNativeFusionPixelsY(c.board)),c.renderOrder,true);
 // Lethal common damage may already have removed metadata/retired this actor.
 // Birth and FX still happened. Only the SAME surviving allocated generation
 // receives the source trailing Die(0); never kill a replacement in its slot.
 if(auto* p=BiteTarget354(c,false))p->Die(0);
 return true;
}
}
NativeExtended347BitePreparation PrepareNativeExtended347Bite(Zombie* z,Plant* p,NativeExtended347BiteCapture& out)
{
 if(Kind(z)!=220)return NativeExtended347BitePreparation::NotHandled;
 if(!Host(z)||!p||p->mBoard!=z->mBoard||p->mDead)return NativeExtended347BitePreparation::Refused;
 const auto* zd=GetNativeSourceZombieData(z);const auto* pd=GetNativePlantData(p);const auto* context=GetNativeContext(z->mBoard);
 if(!zd||!pd||!context)return NativeExtended347BitePreparation::Refused;
 NativeExtended347BiteCapture c;c.app=z->mApp;c.board=z->mBoard;c.epoch=context->epoch;c.ownerUID=c.board->mZombies.DataArrayGetID(z);c.ownerToken=zd->instanceToken;c.targetUID=c.board->mPlants.DataArrayGetID(p);c.targetToken=pd->instanceToken;c.row=p->mRow;c.renderOrder=z->mRenderOrder+1;
 bool pumpkin=false,flying=false,pot=false;
 if(!c.ownerUID||!c.targetUID||!GetNativeDrownWeapon313Hooks().plantTraits(p,c.invincible,pumpkin,flying,pot))return NativeExtended347BitePreparation::Refused;
 if(!c.invincible&&(!GetNativeBasePlantGroundAnchor(p,c.shadowX,c.shadowY)||!PreflightNativeExtended347(z,NativeExtended347Operation::Throw)))return NativeExtended347BitePreparation::Refused;
 out=c;return NativeExtended347BitePreparation::Ready;
}
bool ApplyNativeExtended347CapturedBite(const NativeExtended347BiteCapture& c,int damage)
{
 if(c.invincible||!BiteOwner354(c))return true;
 // The same existing common bite operations, with no alternate damage law.
 // Keep their order and re-resolve full generations after each callback.
 if(auto* p=BiteTarget354(c,true))
 {
  auto* z=BiteOwner354(c);if(!z)return true;
  if(IsNativeGarlicPlant(p))
  {
   auto* garlic=GetOrCreateNativeZombieGarlicData(z);
   if(!garlic||BeginNativeGarlicBite(z,p,*garlic,damage)!=NativeGarlicBiteResult::Started)return true;
   z=BiteOwner354(c);if(!z)return true;z->UpdateAnimSpeed();
  }
 }
 if(auto* p=BiteTarget354(c,true))
 {
  auto* z=BiteOwner354(c);if(!z)return true;NativeDefenderBeforeBite(z,p,damage);
 }
 if(auto* p=BiteTarget354(c,true))
 {
  if(!BiteOwner354(c))return true;
  const bool handled=AdjustNativeSourcePlantDamage(p,damage,0);
  if(!handled)if(auto* current=BiteTarget354(c,true))current->mPlantHealth-=damage;
 }
 if(auto* p=BiteTarget354(c,false))p->mRecentlyEatenCountdown=50;
 return FinishBite354(c);
}
bool BeforeRetireNativeExtended347(Zombie* z,int,const NativeExtendedZombie313Hooks& h)
{
 if(!Host(z)||!PreflightNativeExtended347(z,NativeExtended347Operation::Death))return false;const auto kind=Kind(z);auto* s=EditNativeExtendedZombie313(z);if(!s)return false;
 float x=0,y=0;if(!NativeSourcePoint313(z,"axis",x,y))return false;
 if(kind==218&&!z->mMindControlled){auto* c=GetNativeContext(z->mBoard);if(!c||!s->boardCounted347||c->extendedBoard313.ultimateFootballDrownCount<=0)return false;--c->extendedBoard313.ultimateFootballDrownCount;s->boardCounted347=false;}
 if(kind==210||kind==219)return NativeExtended347MachineDeath(z);
 if(kind==221){h.sound(z,43,.5f);return h.effect(z,34,x,y+.6f);}
 if(kind==224){NativeJackbox300Explosion e;return Explosion(z,z->mRow,x,y+.5f,e)&&EmitNativeSourceZombieExplosion343(z->mBoard,e,0);}
 if(kind==205||kind==206){h.sound(z,43,.5f);return h.effect(z,34,x,y+1.5f);}
 return true;
}
bool DispatchNativeExtended347Event(Zombie* z,FusionSourceGraph313Event event,int,const NativeExtendedZombie313Hooks& h)
{
 if(!Host(z))return false;auto* s=EditNativeExtendedZombie313(z);if(!s||!s->initialized||s->failed)return false;
 const auto kind=Kind(z);Owner owner(z);
 switch(event)
 {
 case FusionSourceGraph313Event::Bomb:return NativeExtended347Bomb(z);
 case FusionSourceGraph313Event::ReplaceBomb:s->bombPresent=true;return true;
 case FusionSourceGraph313Event::Throw:
  if(kind!=202&&kind!=218)return false;
  if(z->mMindControlled||GetNativeSourceZombieData(z)->beforeDying)return true;
  if(!PreflightNativeExtended347(z,NativeExtended347Operation::Throw))return false;
  if(kind==218)s->bombPresent=false; // Source SetWeapon destroys heldW before fresh weapon birth.
  return ThrowNativeDrownWeapon313(z,GetNativeDrownWeapon313Hooks());
 case FusionSourceGraph313Event::Crash:
 {
  if(kind!=205&&kind!=206)return false;NativeExtendedZombie313Input input;
  if(!h.input(z,input)||!PreflightNativeExtended347(z,NativeExtended347Operation::Death)||!h.kirovCrash(z,input.sourceShadowX,input.sourceShadowY,2,1000))return false;
  z=owner.Get();if(z)h.retire(z,2);return true;
 }
 case FusionSourceGraph313Event::Shoot:
 {
  if(kind!=221||!PreflightNativeExtended347(z,NativeExtended347Operation::Throw))return false;
  FusionSourceObstacle313Spawn request;request.kind=static_cast<FusionSourceObstacle313Kind>(5);request.ownerId=z->mNetId;request.ownerToken=GetNativeSourceZombieData(z)->instanceToken;request.row=z->mRow;request.fromHypno=z->mMindControlled;
  float unused=0;if(!GetNativeSourceZombie269NodePoint(z,"back/shoot",request.centerX,request.centerY)||!NativeSourceGroundAnchor313(z,unused,request.groundY))return false;
  request.renderOrder=Board::MakeRenderOrder(RENDER_LAYER_ZOMBIE,z->mRow,0);
  return SpawnNativeSourceObstacle313(z->mBoard,request)!=nullptr;
 }
 case FusionSourceGraph313Event::Pop:
 {
  if(kind!=224||!PreflightNativeExtended347(z,NativeExtended347Operation::Pop))return false;
  int level=0;if(!ReadNativeSourceStatusLevel313(z,level))return false;
  if(level==0||level==1)return Births(z,level);
  if(level!=2)return false;float x=0,y=0;if(!NativeSourcePoint313(z,"axis",x,y))return false;
  for(int row=0;row<GetNativeFusionRowCount(owner.board);++row)
  {z=owner.Get();if(!z)return true;float land=0;NativeJackbox300Explosion e;if(!NativeSourceRowLandY313(owner.board,x,row,land)||!Explosion(z,row,x,land+.5f,e)||!EmitNativeSourceZombieExplosion343(owner.board,e,0))return false;}
  return true;
 }
 case FusionSourceGraph313Event::FootballThrowExit:
  if(kind!=202)return false;
  if(s->sourceStatus==1){h.graph.stateFor(z)->parameters.triggers|=FusionSourceGraph313Trigger(FusionSourceGraph313Param::Die);return true;}
  if(NativeUltimateFootballDrownCount343(z->mBoard)>0&&!GetNativeSourceZombieData(z)->beforeDying&&!z->mMindControlled)
   return PlayNativeSourceGraph313(z,*h.graph.stateFor(z),0,"throw",0,h.graph);
  return true;
 case FusionSourceGraph313Event::SpiderRaised:
 {
  if(kind!=221)return false;float x=0,y=0;if(!NativeSourcePoint313(z,"axis",x,y))return false;
  s->sourceStatus=10;h.sound(z,43,.5f);return h.effect(z,34,x,y);
 }
 case FusionSourceGraph313Event::Bite:return h.baseBite(z);
 case FusionSourceGraph313Event::FallSound:h.sound(z,74,.5f);h.shake(z->mBoard,.05f);return true;
 case FusionSourceGraph313Event::Destroy:h.retire(z,2);return true;
 default:return false;
 }
}
// Source SpiderWeapon is a dynamic Rigidbody, independent of the actor. Root's
// factory initializes raw vX to (+/-4)*px*.01 and vY=0; acceleration is genuine
// project gravity9.81. Raw Projectile pos/velocity remain the sole physics data.
bool UpdateNativeSpiderWeapon347(Projectile* p)
{
 auto* d=EditNativeSourceObstacle313(p);if(!d||unsigned(d->kind)!=5)return false;
 if(!p||!p->mApp||!IsNativeSourceWorld313(p->mBoard)||p->mDead||p->mNetGhost||p->mApp->NetIsClient()||p->mBoard->mPaused||p->mApp->mGameScene!=SCENE_PLAYING||d->failed||d->tick==p->mBoard->mMainCounter)return true;
 d->tick=p->mBoard->mMainCounter;if(d->exploded)return true;
 float sx=0,sy=0,land=0;if(!NativeBoardPointToSource313(p->mBoard,p->mRow,p->mShadowY,p->mPosX+20,p->mPosY+20,sx,sy)||!NativeSourceRowLandY313(p->mBoard,sx,p->mRow,land)){d->failed=true;return true;}
 if(FusionSpider347HitsGround(sy,land))
 {
  NativeJackbox300Explosion e;e.ownerId=d->ownerId;e.ownerToken=d->ownerToken;e.sourceKind=221;e.row=p->mRow;e.controlled=d->fromHypno;e.renderOrder=p->mRenderOrder;e.centerX=e.shadowX=p->mPosX+20;e.centerY=e.shadowY=p->mPosY+20;
  // Source operand2 is preserved by the shared343 API extension. Admission is
  // closed until that exact variant is implemented; never alias explosion0.
  if(!CanEmitNativeSourceZombieExplosion343(p->mBoard,e,nullptr,2)){d->failed=true;return true;}
  d->exploded=true;auto* board=p->mBoard;const auto uid=board->mProjectiles.DataArrayGetID(p);
  if(!EmitNativeSourceZombieExplosion343(board,e,2)){d->failed=true;return true;}
  p=board->mProjectiles.DataArrayTryToGet(uid);if(p&&!p->mDead)p->Die();return true;
 }
 p->mPosX+=p->mVelX;p->mPosY+=p->mVelY;
 p->mVelY+=float(9.81*GetNativeFusionPixelsY(p->mBoard)*.0001);p->mX=int(p->mPosX);p->mY=int(p->mPosY);return true;
}
}
