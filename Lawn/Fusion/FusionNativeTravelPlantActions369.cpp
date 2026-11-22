#include "FusionNativeTravelPlantActions369.h"
#include "FusionNativeTravelConsumers369.h"
#include "FusionNativeZombiePreflight280.h"
#include "FusionNativeSourceLifecycle313.h"
#include "FusionSummon269Rules.h"
#include "FusionNative.h"
#include "../Plant.h"
#include "../Board.h"
#include "../../LawnApp.h"
#include <array>
namespace LawnFusion {
bool PrepareNativeHypnoDoom369(const Plant* p,std::vector<NativeSourceZombieSpawnRequest>& out)
{
 const auto* data=GetNativePlantData(p);
 if(!p||!data||data->kind!=928||!p->mApp||p->mApp->NetIsClient()||p->mNetGhost||p->mDead)return false;
 std::vector<NativeSourceZombieSpawnRequest> prepared;
 if(!HasNativeTravelAdvanced369(p->mBoard,39)){out=prepared;return true;}
 std::array<int,3> rows;int count=0;float x=0,y=0;
 if(!TryGetFusionHypnoDoom269Rows(p->mRow,GetNativeFusionRowCount(p->mBoard),true,rows,count)||
  !const_cast<Plant*>(p)->GetShadowCenter(x,y))return false;
 std::vector<uint16_t> kinds;
 for(int i=0;i<count;++i)
 {
  FusionSummon269Choice choice;if(!TryChooseFusionHypnoDoom269(p->mBoard->mPlantRow[rows[i]]==PLANTROW_POOL,choice))return false;
  NativeSourceZombieSpawnRequest request;request.kind=choice.kind;request.row=rows[i];request.positioned=true;
  request.shadowX=x;request.friendly=true;request.withEffect=false;prepared.push_back(request);kinds.push_back(request.kind);
 }
 FusionSourceDeathBudget319 peak;
 if(!EnsureNativeSourceZombieClosure280(p->mBoard,kinds)||!GetNativeSourceZombieBirthBudget319(p->mBoard,kinds,peak)||
  !CanAllocateNativeSourceBudget319(p->mBoard,peak))return false;
 out=prepared;return true;
}
bool CommitNativeHypnoDoom369(Board* b,const std::vector<NativeSourceZombieSpawnRequest>& requests)
{for(const auto& request:requests)if(!SpawnNativeSourceZombie(b,request))return false;return true;}
}
