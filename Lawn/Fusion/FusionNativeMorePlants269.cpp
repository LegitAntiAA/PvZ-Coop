#include "FusionNativeMorePlants269.h"
#include "FusionNative.h"
#include "FusionNativeShooterTiming.h"
#include "FusionNativeVisual.h"
#include "FusionNativeProducer.h"
#include "FusionBasePlantRules.h"
#include "FusionNativeDroppedCards269.h"
#include "FusionNativeRemainingPlants269.h"
#include "FusionNativeWater268.h"
#include "FusionNativePlacement.h"
#include "FusionRecipe.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Coin.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>
namespace LawnFusion
{
namespace
{
bool Current(const Plant* p){return p&&p->mBoard&&p->mApp&&p->mApp->mBoard==p->mBoard&&IsNativeFusionBoard(p->mBoard)&&GetNativePlantData(p);}
bool Host(const Plant* p){return Current(p)&&!p->mDead&&!p->mNetGhost&&!p->mApp->NetIsClient();}
bool Playing(const Plant* p){return Host(p)&&CanApplyNativeFusionGameplay(p->mBoard)&&!p->mBoard->mPaused;}
const FusionShooterTimingProfile* Timing(const Plant* p)
{return Current(p)?FindFusionMorePlants269Timing(GetNativePlantData(p)->kind,GetNativePlantData(p)->batchBagCount):nullptr;}
bool HamburgerReady(const Plant* p)
{const auto* d=GetNativePlantData(p);for(auto k:FusionHamburger269Roster(d->batchBagCount))
 {if(!CanEmitNativeMore269HamburgerShot(p->mBoard,k,0))return false;
  if(k==0)for(uint8_t fire=1;fire<=3;++fire)if(!CanEmitNativeMore269HamburgerShot(p->mBoard,k,fire))return false;}return true;}
bool HamburgerShot(Plant* p)
{
 if(!HamburgerReady(p))return false;const auto* d=GetNativePlantData(p);const auto roster=FusionHamburger269Roster(d->batchBagCount);
 const int draw=RandRangeInt(0,int(roster.size())-1);const int pea=roster[draw]==0?RandRangeInt(0,3):3;
 FusionHamburger269Shot shot;const auto* def=FusionFindPlant(243);if(!def||!TryChooseFusionHamburger269Shot(d->batchBagCount,def->damage,draw,pea,shot))return false;
 float x=0,y=0;if(!GetNativeSourcePlantNodePoint(p,"Shoot",x,y))return false;
 if(!EmitNativeMore269HamburgerShot(p,shot,x+float(.1*kFusionBasePixelsX),y))return false;
 ++GetNativePlantData(p)->batchShotCount;return true;
}
std::vector<Plant*> PresentRecipients(Plant* p)
{std::vector<Plant*> found;Plant* q=nullptr;while(p->mBoard->IteratePlants(q))if(q!=p&&!q->mDead&&q->mPlantCol==p->mPlantCol&&q->mRow==p->mRow)
 {bool flying=false,pumpkin=false,pot=false,lily=false;if(GetNativeRemaining269PlantTags(q,flying,pumpkin,pot,lily)&&!flying&&!pumpkin&&!pot&&!lily)found.push_back(q);}return found;}
bool PresentReady(Plant* p,const std::vector<Plant*>& recipients)
{
 if(NativeMore269PresentSuperRandom(p)||!CanCreateNativeRemaining269DroppedCard(p->mBoard,256))return false;
 bool ready=true;
 for(auto k:FusionPresent269AllCandidates())
 {if(!CanSpawnNativeMore269Plant(p->mBoard,k))ready=false;
  for(auto* q:recipients)if(const auto* r=FindFusionRecipe(GetNativePlantData(q)->kind,k))
   if(!CanSpawnNativeMore269Plant(p->mBoard,r->result))ready=false;}return ready;
}
bool Present(Plant* p)
{
 const auto recipients=PresentRecipients(p);if(!PresentReady(p,recipients))return false;
 std::vector<uint16_t> pool(FusionPresent269AllCandidates().begin(),FusionPresent269AllCandidates().end());
 // This initial random draw exists even when an empty-cell terrain draw follows.
 uint16_t chosen=pool[RandRangeInt(0,int(pool.size())-1)];
 if(recipients.empty())
 {std::vector<uint16_t> terrain;const bool water=NativeMore269PresentWaterCell(p);
  for(auto k:pool)if(NativeMore269SourceWaterPlant(k)==water)terrain.push_back(k);
  if(terrain.empty())return false;chosen=terrain[RandRangeInt(0,int(terrain.size())-1)];}
 else for(auto* target:recipients)
 {while(!pool.empty())
  {const int draw=RandRangeInt(0,int(pool.size())-1);chosen=pool[draw];
   if(FindFusionRecipe(GetNativePlantData(target)->kind,chosen))break;
   pool.erase(pool.begin()+draw);}}
 auto* b=p->mBoard;const int col=p->mPlantCol,row=p->mRow;float x=0,y=0;if(!p->GetShadowCenter(x,y))return false;
 p->Die(); // SOURCE AnimEvent dies before all factory attempts.
 NativePlantData fresh;fresh.kind=chosen;fresh.owner=LawnNet::NET_SHARED_OWNER;
 if(chosen==6){TrySpawnNativeFusionPlant(b,col,row,fresh);TrySpawnNativeFusionPlant(b,col,row,fresh);}
 if(!TrySpawnNativeFusionPlant(b,col,row,fresh))CreateNativeDroppedCard269(b,256,0,x,y);
 LawnNet::NetTrace("Fusion More269 Present id=%u selected=%u recipientCount=%zu fullSourceRoster=33",p->mNetId,chosen,recipients.size());return true;
}
}
bool IsNativeMorePlants269(const Plant* p)
{return Current(p)&&(GetNativePlantData(p)->kind==243||GetNativePlantData(p)->kind==248||GetNativePlantData(p)->kind==256);}
bool InitializeNativeMorePlants269(Plant* p)
{
 if(!IsNativeMorePlants269(p)||!Host(p))return false;auto* d=GetNativePlantData(p);const auto* def=FusionFindPlant(d->kind);if(!def)return false;
 p->mPlantHealth=p->mPlantMaxHealth=def->maxHealth;p->SetSleeping(false);p->mWakeUpCounter=0;
 if(d->kind!=243)d->animationSpeed=RandRangeFloat(.9f,1.1f);d->idleSeconds=0;d->shootSeconds=-1;
 d->batchMode=d->batchBagCount=d->batchRageTicks=0;d->batchShotCount=0;d->productionTicks=d->pendingProductionTicks=0;
 p->mStateCountdown=p->mDoSpecialCountdown=p->mShootingCounter=0;
 if(d->kind==243){if(!InitializeNativeShooterTiming(p,*Timing(p),false))return false;p->mStateCountdown=0;}
 if(d->kind==256)BeginNativeShooterShot(p,*Timing(p));
 if(d->kind==248){p->mState=STATE_NOTREADY;p->mTargetZombieID=ZOMBIEID_NULL;}
 return true;
}
bool UpdateNativeMorePlants269(Plant* p)
{
 if(!IsNativeMorePlants269(p))return false;if(!Playing(p))return true;auto* d=GetNativePlantData(p);
 if(d->kind==248){UpdateNativeMore269SquashMotion(p);return true;}
 if(d->kind!=243)return true;const auto* t=Timing(p);
 if(d->pendingProductionTicks>0&&!--d->pendingProductionTicks)
  if(!EmitNativeSourceSun(p,0))d->pendingProductionTicks=1;
 // Source ProducerUpdate subtracts delta then tests<0. Shared Plant.Update
 // already decremented positive mStateCountdown; zero is the next crossing.
 if(p->mStateCountdown==0)
 {p->mStateCountdown=(25+RandRangeInt(-2,2))*100;d->pendingProductionTicks=50;}
 d->productionTicks=p->mStateCountdown;
 if(--p->mLaunchCounter<0)
 {ResetNativeShooterLaunchCounter(p,*t);bool target=false;Zombie* z=nullptr;float x=0,y=0;p->GetShadowCenter(x,y);
  while(p->mBoard->IterateZombies(z))if(!z->mDead&&FusionPlantSearchUniqueZombieAllowed(GetNativeFusionZombieStatus(z),z->mMindControlled,!z->mHasHead||z->IsDeadOrDying())&&!NativeWater268ZombieCaptured(z)&&z->mRow==p->mRow)
  {float zx=0,zy=0;if(z->GetShadowCenter(zx,zy)&&zx>x&&zx<p->mBoard->GetCellCenterX(p->mBoard->GetColumnCount()-1,p->mRow)+1.85*kFusionBasePixelsX){target=true;break;}}
  if(target&&HamburgerReady(p))BeginNativeShooterShot(p,*t);}
 return true;
}
void UpdateNativeMorePlants269Phase(Plant* p)
{if(!IsNativeMorePlants269(p)||!Playing(p))return;if(const auto* t=Timing(p))UpdateNativeShooterPhase(p,*t);else GetNativePlantData(p)->idleSeconds+=.01f;}
bool DispatchNativeMorePlants269Events(Plant* p)
{
 if(!IsNativeMorePlants269(p))return false;if(!Playing(p))return true;auto* d=GetNativePlantData(p);const auto* t=Timing(p);
 if(!t||d->shootSeconds<0)return true;
 for(size_t i=0;i<t->eventClipSeconds.size();++i){bool due=false;if(!TryGetFusionShooterEventAtCounter(*t,d->animationSpeed,p->mShootingCounter,i,due)||!due)continue;
  if(d->kind==243&&FusionHamburger269EventAllowed(d->batchBagCount,unsigned(i)))HamburgerShot(p);
  if(d->kind==256)Present(p);}
 return true;
}
bool GetNativeMorePlants269VisualPose(const Plant* p,const char*& state,float& seconds)
{if(!IsNativeMorePlants269(p)||GetNativePlantData(p)->kind==248)return false;const auto* d=GetNativePlantData(p);if(d->batchBagCount>3)return false;const auto* t=Timing(p);
 constexpr const char* idle[]={"idle","idle2","idle3","idle4"};state=d->shootSeconds>=0&&t?t->visualState:idle[d->kind==243?d->batchBagCount:0];seconds=d->shootSeconds>=0?d->shootSeconds:d->idleSeconds;return true;}
bool GetNativeHamburger269NodeActive(const Plant* p,const char* path,bool& active)
{if(!Current(p)||GetNativePlantData(p)->kind!=243||!path)return false;const auto stage=GetNativePlantData(p)->batchBagCount;
 constexpr const char* paths[]={"hamburger_0011_1/R1","hamburger_0011_1/R2","hamburger_0011_1/R3"};
 for(unsigned i=0;i<3;++i)if(!std::strcmp(path,paths[i])){active=stage==i;return true;}return false;}
bool ApplyNativeSqualour269Attack(Plant* p,bool pop)
{
 if(!Host(p)||GetNativePlantData(p)->kind!=248)return false;auto* d=GetNativePlantData(p);float x=0,y=0;if(!p->GetShadowCenter(x,y))return false;
 const float hw=float(.75*kFusionBasePixelsX),hh=float(1.5*GetNativeFusionPixelsY(p->mBoard));
 std::vector<ZombieID> victims;Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))if(!z->mDead&&!NativeWater268ZombieCaptured(z))
 {const int status=GetNativeFusionZombieStatus(z);const bool land=IsNativeFusionGroundZombie(z)&&!z->mMindControlled&&!z->IsDeadOrDying();const auto r=z->GetZombieRect();
  if(FusionMorePlants269SquashTarget(status,z->mRow==p->mRow,land)&&x-hw<=r.mX+r.mWidth&&x+hw>=r.mX&&y-hh<=r.mY+r.mHeight&&y+hh>=r.mY)victims.push_back(p->mBoard->ZombieGetID(z));}
 for(auto id:victims)if((z=p->mBoard->ZombieTryToGet(id))&&!z->mDead)
 {ApplyNativeSourceSquashDamage(z,FusionFindPlant(248)->damage);++d->batchShotCount;d->batchMode=1;}
 if(pop)p->Die();return true;
}
bool FinishNativeSqualour269(Plant* p)
{
 if(!Host(p)||GetNativePlantData(p)->kind!=248||NativeMore269SqualourTravel(p))return false;
 const auto* d=GetNativePlantData(p);const auto drops=PlanFusionSqualour269Drops(d->batchMode!=0,d->batchShotCount);
 const unsigned count=unsigned(drops.self)+unsigned(drops.upgraded)+drops.squash;
 if(!FusionProducerCanFitCallback(p->mBoard->mCoins.mSize,p->mBoard->mCoins.mMaxSize,int(count)))return false;
 unsigned cards=0;Coin* coin=nullptr;while(p->mBoard->IterateCoins(coin))if(!coin->mDead&&GetNativeDroppedCard269Data(coin))++cards;
 if(cards+count>128)return false;
 if((drops.self&&!CanCreateNativeRemaining269DroppedCard(p->mBoard,248))||(drops.upgraded&&!CanCreateNativeRemaining269DroppedCard(p->mBoard,1054))||
    (drops.squash&&!CanCreateNativeRemaining269DroppedCard(p->mBoard,13)))return false;
 float x=0,y=0;if(!p->GetShadowCenter(x,y))return false;y-=float(.5*GetNativeFusionPixelsY(p->mBoard));
 if(drops.self)CreateNativeDroppedCard269(p->mBoard,248,0,x,y);
 if(drops.upgraded)CreateNativeDroppedCard269(p->mBoard,1054,0,x,y);
 for(unsigned i=0;i<drops.squash;++i)CreateNativeDroppedCard269(p->mBoard,13,0,x,y);
 p->Die();return true;
}
}
