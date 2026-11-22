#include "FusionNativeBossBungee366.h"
#include "FusionBossBungee366Rules.h"
#include "FusionNativeHeavyZombie313BBridge.h"
#include "FusionNative.h"
#include "FusionNativeZombies.h"
#include "FusionNativeZombiePreflight280.h"
#include "FusionSourceZombieClosure280.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include <algorithm>
#include <vector>
namespace LawnFusion
{
namespace
{
Zombie* Boss(Board* b,uint16_t id,uint32_t token)
{
 if(!b||!id||!token)return nullptr;auto* z=b->mZombies.DataArrayTryToGet(token);const auto* d=GetNativeSourceZombieData(z);
 return z&&!z->mDead&&z->mNetId==id&&d&&d->instanceToken==token&&(d->kind==44||d->kind==46)?z:nullptr;
}
bool Child(Zombie* z)
{
 const auto* d=GetNativeSourceZombieData(z);const auto* c=z?GetNativeContext(z->mBoard):nullptr;
 return z&&z->mApp&&z->mApp->mBoard==z->mBoard&&!z->mDead&&!z->mNetGhost&&!z->mApp->NetIsClient()&&c&&!c->adventureTeardown&&
  d&&d->kind==45&&d->instanceToken==z->mBoard->mZombies.DataArrayGetID(z);
}
}
bool CanEmitNativeBossBungeeTriple366(Zombie* boss,std::string* error)
{
 const auto* d=GetNativeSourceZombieData(boss);auto* b=boss?boss->mBoard:nullptr;
 if(!d||!b||!boss->mApp||boss->mApp->mBoard!=b||boss->mApp->NetIsClient()||boss->mNetGhost||!Boss(b,boss->mNetId,d->instanceToken))return false;
 const uint16_t children[3]={45,45,45};
 return EnsureNativeSourceZombieClosure280(b,children,error)&&CanAllocateNativeSourceZombieBatch280(b,children,error);
}
bool EmitNativeBossBungeeTriple366(Zombie* boss,std::string* error)
{
 if(!CanEmitNativeBossBungeeTriple366(boss,error))return false;auto* b=boss->mBoard;auto* app=boss->mApp;
 const uint32_t epoch=GetNativeContext(b)->epoch,token=GetNativeSourceZombieData(boss)->instanceToken;const uint16_t id=boss->mNetId;
 for(int column=1;column<=3;++column)
 {
  if(app->mBoard!=b||!GetNativeContext(b)||GetNativeContext(b)->epoch!=epoch)return false;
  boss=Boss(b,id,token);if(!boss)return false;
  NativeSourceZombieSpawnRequest r;r.kind=45;r.row=0;r.fromWave=0;r.positioned=true;
  r.shadowX=b->GetCellCenterX(0,0)+(9.9f+4.8f)*float(kFusionBasePixelsX);
  auto* child=SpawnNativeSourceZombie(b,r);
  if(app->mBoard!=b||!GetNativeContext(b)||GetNativeContext(b)->epoch!=epoch)return false;
  boss=Boss(b,id,token);if(!child||!boss||!ConfigureNativeBossBungee366(child,boss,column))return false;
 }
 return true;
}
bool ConfigureNativeBossBungee366(Zombie* z,Zombie* boss,int column)
{
 if(!Child(z)||!boss||z->mBoard!=boss->mBoard||column<1||column>3)return false;
 const auto* parent=GetNativeSourceZombieData(boss);if(!parent||!Boss(z->mBoard,boss->mNetId,parent->instanceToken))return false;
 auto* s=EditNativeHeavyZombie313B(z);if(!s||!s->initialized||s->failed||s->worldTick>=0||s->bossStarted366||s->bungee.setZombie||s->bungee.bossId366||s->bungee.bossToken366)return false;
 s->bungee.bossId366=boss->mNetId;s->bungee.bossToken366=parent->instanceToken;s->bungee.targetColumn=column;return true;
}
NativeBossBungee366Start StartNativeBossBungee366(Zombie* z,NativeHeavyZombie313BState& s,const NativeHeavyZombie313BHooks& h)
{
 using Result=NativeBossBungee366Start;
 if(!Child(z)||!h.plantTraits||!h.prepareCapture||!h.landY||!h.setAxis||!h.sound||s.bossStarted366)return Result::Failed;
 if(!s.bungee.bossId366&&!s.bungee.bossToken366)return Result::NotLinked;
 // Unity.Object.op_Implicit is tested only in Start. Later boss removal must
 // not re-target or cancel an already descending/catching Bungee.
 if(!Boss(z->mBoard,s.bungee.bossId366,s.bungee.bossToken366))
 {s.bungee.bossId366=0;s.bungee.bossToken366=0;return Result::NotLinked;}
 if(s.bungee.targetColumn<1||s.bungee.targetColumn>3||s.bungee.setZombie)return Result::Failed;
 std::vector<FusionHeavy313BPlant> all;std::vector<int> rows;Plant* p=nullptr;
 while(z->mBoard->mPlants.IterateNext(p))
 {
  if(p->mDead||!GetNativePlantData(p))continue;FusionHeavy313BPlant t;if(!h.plantTraits(p,t))return Result::Failed;
  if(!t.alive)continue;all.push_back(t);
  // Actual Start lambda3510A0 removes only column!=bossColumn. Every plant,
  // including multiple plants in one row, contributes one source RNG entry.
  
 }
 if(!BuildFusionBossBungee366RowPool(all,s.bungee.targetColumn,GetNativeFusionRowCount(z->mBoard),rows))return Result::Failed;
 std::vector<uint32_t> futureCaptures;
 for(int row=0;row<GetNativeFusionRowCount(z->mBoard);++row)
 {
  std::vector<FusionHeavy313BPlant> cell;for(const auto& t:all)if(t.column==s.bungee.targetColumn&&t.row==row)cell.push_back(t);
  for(const auto& t:cell)if(FusionBungee313BStealable(t,cell))futureCaptures.push_back(t.uid);
 }
 std::string error;if(!futureCaptures.empty()&&!h.prepareCapture(z,futureCaptures,&error))return Result::Failed;
 auto* b=z->mBoard;auto* app=z->mApp;const uint32_t epoch=GetNativeContext(b)->epoch,uid=b->mZombies.DataArrayGetID(z);
 s.bossStarted366=true; // Consume this Start before every callback/RNG.
 h.sound(z,uint16_t(RandRangeInt(122,124)));
 if(app->mBoard!=b||!GetNativeContext(b)||GetNativeContext(b)->epoch!=epoch)return Result::Started;
 z=b->mZombies.DataArrayTryToGet(uid);if(!Child(z))return Result::Started;
 const int row=rows.empty()?RandRangeInt(0,GetNativeFusionRowCount(b)-1):rows[size_t(RandRangeInt(0,int(rows.size())-1))];
 auto* current=EditNativeHeavyZombie313B(z);if(!current)return Result::Failed;
 const float x=-4.8f+1.35f*current->bungee.targetColumn;float land=0;if(!h.landY(b,x,row,land))return Result::Failed;
 current->status=current->bungee.status=23;current->bungee.markerVisible=false;current->bungee.targetRow=row;
 current->bungee.targetX=x;current->bungee.targetY=land;
 current->bungee.markerY=10.f-row*1.5f+.68f*(.9529412f-2.11f);
 z->mPhaseCounter=z->mSummonCounter=0;z->SetRow(row);
 if(!h.setAxis(z,x,10.f-row*1.5f))return Result::Failed;
 return Result::Started;
}
}
