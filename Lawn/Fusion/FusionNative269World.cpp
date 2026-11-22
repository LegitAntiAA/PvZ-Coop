#include "FusionNativeTravelConsumers369.h"
#include "FusionNative.h"
#include "FusionNativeHypno269.h"
#include "FusionNativeSolar269.h"
#include "FusionNativeWorldRemainder269.h"
#include "FusionNativeBatchActions.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"

namespace LawnFusion
{
// Base plant levels/unlockPlant remain separate from the rare buff ledger.
int GetNativeHypno269PlantLevel(const Plant*) {return 0;}
bool HasNativeHypno269Ultimate(const Plant* p,unsigned index) {return p&&GetNativePlantData(p)&&HasNativeTravelUltimate369(p->mBoard,index);}
bool HasNativeHypno269ProjectileUltimate(const Projectile* p,unsigned index) {return p&&GetNativeProjectileData(p)&&HasNativeTravelUltimate369(p->mBoard,index);}
bool HasNativeDoom269Trait(const Plant* p,NativeDoom269Trait trait)
{
 if(!p||!GetNativePlantData(p))return false;
 switch(trait){case NativeDoom269Trait::Advanced2:return HasNativeTravelAdvanced369(p->mBoard,2);
 case NativeDoom269Trait::Expert10:return HasNativeTravelUltimate369(p->mBoard,10);
 case NativeDoom269Trait::Expert11:return HasNativeTravelUltimate369(p->mBoard,11);
 default:return false;} // Basic3 is unlockPlant; EveningBalanced is a mode.
}
int GetNativeStar269MagnetCount(const Plant* plant)
{return GetNativeEquipmentGraphCount(plant);}

bool ApplyNativeDoom269World(Plant* plant,int damage,bool ice,bool setPit)
{
    if(!plant||!IsNativeFusionBoard(plant->mBoard)||(damage!=1800&&damage!=1000000))return false;
    float x=0,y=0;if(!plant->GetShadowCenter(x,y))return false;
    ApplyNativeBatchDoomAt(plant->mBoard,x,y,plant->mPlantCol,plant->mRow,damage,setPit,ice);
    return true;
}

// These source world/recipient subsystems are not native actors yet. A caller
// cannot spend currency or publish a successful activation through these gates.
bool TryActivateNativeStar269Meteorite(Plant* plant)
{const auto* data=GetNativePlantData(plant);return data&&data->kind==1104&&ActivateNativeWorldRemainder269Meteorite(plant->mBoard);}
bool TryApplyNativeStar269UltimateRecipient(Zombie*,bool& objectBroken)
{objectBroken=false;return false;}
FusionSolar269WorldBranch QueryNativeSolar269WorldBranch(const Board*)
{return FusionSolar269WorldBranch::Unavailable;}
bool CanCreateNativeSolar269SuperParticle(const Board*) {return false;}
bool CreateNativeSolar269SuperParticle(Plant*,float,float) {return false;}
void CommitNativeSolar269World(Plant*,FusionSolar269WorldBranch) {}
bool NativeSolar269TravelUltimate22(const Plant* p) {return p&&GetNativePlantData(p)&&HasNativeTravelUltimate369(p->mBoard,22);}
bool NativeSolar269ShootingBoard(const Board*) {return false;}
}
