#include "FusionNativeTravelConsumers369.h"
#include "FusionNative.h"
#include "FusionNativeCompanions269.h"
#include "FusionNativeWorldActors269.h"
#include "FusionNativeSummonPlants269.h"
#include "FusionNativeVisual.h"
#include "FusionNativeEffects269.h"
#include "FusionNativeChomper268.h"
#include "FusionNativeExtras268.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../../LawnApp.h"
#include <algorithm>
#include <limits>
namespace LawnFusion
{
namespace
{
bool HostLive(const Plant* p)
{return p&&p->mApp&&p->mApp->mBoard==p->mBoard&&!p->mApp->NetIsClient()&&!p->mNetGhost&&!p->mDead&&p->mIsOnBoard&&p->mPlantHealth>0&&GetNativePlantData(p);}
}
bool HasNativeCompanions269Trait(const Plant* p,NativeCompanions269Trait trait)
{
 if(!p||!GetNativePlantData(p))return false;
 switch(trait){case NativeCompanions269Trait::Common5:return HasNativeTravelAdvanced369(p->mBoard,5);
 case NativeCompanions269Trait::Advanced4:return HasNativeTravelAdvanced369(p->mBoard,4);
 case NativeCompanions269Trait::Ultimate24:return HasNativeTravelUltimate369(p->mBoard,24);
 case NativeCompanions269Trait::Ultimate25:return HasNativeTravelUltimate369(p->mBoard,25);}return false;
}
bool NativeCompanions269ShootingBoard(const Board*){return false;}
bool HasNativeWorld269Trait(const Plant* p,NativeWorld269Trait trait)
{return p&&GetNativePlantData(p)&&HasNativeTravelAdvanced369(p->mBoard,trait==NativeWorld269Trait::Advanced46?46:47);}
bool ApplyNativeCompanion269CherryExplosion(Projectile* p,int row,int variant)
{return p&&p->mApp&&!p->mApp->NetIsClient()&&IsNativeFusionBoard(p->mBoard)&&
    ApplyNativeSourceCherry269ExplosionAt(p->mBoard,p->mPosX+20,p->mPosY+p->mPosZ+20,row,variant);}
bool ApplyNativeCompanions269PlantCherryExplosion(Plant* p,float x,float y,int row)
{return HostLive(p)&&ApplyNativeSourceCherry269ExplosionAt(p->mBoard,x,y,row,0);}
void NativeCompanions269BirthEffect(Plant* p,int)
{if(HostLive(p))p->mBoard->DoPlantingEffects(p->mPlantCol,p->mRow,p);}
bool TrySpendNativeSuperSun269Sun(Plant* p,int cost)
{return HostLive(p)&&cost>0&&p->mBoard->mSunMoney>cost&&p->mBoard->TakeSunMoney(cost);}
bool RecoverNativeCompanions269Plant(Plant* p,int amount)
{
    if(!HostLive(p)||amount<0)return false;
    // Plant.Recover180466F90: resolve its real source resource before the
    // health operation. Cosmetic pool exhaustion must never cancel healing.
    const bool effectReady=EnsureNativeSourceEffect269(p->mBoard,16);
    if(!RecoverNativeMachine268(p,amount)&&!RecoverNativeChomper268(p,amount))
    {
        p->mPlantHealth=int((std::min)(int64_t(p->mPlantMaxHealth),int64_t(p->mPlantHealth)+amount));
        p->mEatenFlashCountdown=25;NativePlantHealthChanged(p);
    }
    float x=0,y=0;
    if(effectReady&&p->GetShadowCenter(x,y))
        SpawnNativeSourceEffect269(p->mBoard,16,x,y-float(.7*GetNativeFusionPixelsY(p->mBoard)),
            Board::MakeRenderOrder(RENDER_LAYER_PARTICLE,p->mRow,0),true);
    return true;
}
bool NativeCompanions269HasLayerNeighbour(const Plant* p)
{
    if(!p||!GetNativePlantData(p))return false;Plant* other=nullptr;
    while(p->mBoard->IteratePlants(other))if(other!=p&&!other->mDead&&other->mIsOnBoard&&other->mPlantHealth>0&&
        other->mPlantCol==p->mPlantCol&&other->mRow==p->mRow&&GetNativePlantData(other))return true;
    return false;
}
Plant* ReplaceNativeCompanions269Plant(Plant* p,uint16_t kind)
{
    if(!HostLive(p))return nullptr;const auto old=*GetNativePlantData(p);
    if(!((old.kind==935&&(kind==911||kind==922))||(old.kind==1082&&kind==912))||
        p->mBoard->mPlants.mSize>=p->mBoard->mPlants.mMaxSize||GetNativeSeedForPlant(kind)==SEED_NONE||
        !EnsureNativePlantVisuals(p->mBoard,kind))return nullptr;
    NativePlantData fresh;fresh.kind=kind;fresh.firstParent=old.kind;fresh.packetId=old.packetId;
    fresh.owner=old.owner;fresh.randomOrigin=old.randomOrigin;
    auto* result=p->mBoard->AddPlant(p->mPlantCol,p->mRow,GetNativeSeedForPlant(kind),SEED_NONE,&fresh);
    const auto* bound=GetNativePlantData(result);
    if(!result||result->mDead||!bound||bound->kind!=kind||!bound->instanceToken)
    {if(result)result->Die(255);return nullptr;}
    p->Die(2);return result;
}
// Source endpoint particles and connection ball are admitted by their resource
// adapter. Until that adapter exists, reject before damage or connection state.
bool CanCreateNativeWorld269Effects(const Plant* p,bool explosion){return p&&EnsureNativeSourceEffect269(p->mBoard,99)&&(!explosion||EnsureNativeSourceEffect269(p->mBoard,100));}
bool CreateNativeWorld269Effect(Plant* p,unsigned kind,float x,float y,int row){return HostLive(p)&&(kind==99||kind==100)&&SpawnNativeSourceEffect269(p->mBoard,uint16_t(kind),x,y,Board::MakeRenderOrder(RENDER_LAYER_PARTICLE,row,0),true);}
bool CreateNativeLaser269Connection(Plant* p,Plant* other)
{return HostLive(p)&&HostLive(other)&&p->mBoard==other->mBoard&&EnsureNativeLaser269Visuals(p->mBoard);}
void RemoveNativeLaser269Connection(Plant*){} // Derived native Tod leases retire when the canonical pair disappears.
}
