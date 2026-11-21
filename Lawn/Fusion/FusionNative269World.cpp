#include "FusionNative.h"
#include "FusionNativeHypno269.h"
#include "FusionNativeSolar269.h"
#include "FusionNativeWorldRemainder269.h"
#include "FusionNativeBatchActions.h"
#include "../Board.h"
#include "../Plant.h"

namespace LawnFusion
{
// Normal-D2 currently has no Travel selection or source-level progression.
// Those traits stay absent until their actual mode/progression owns them.
int GetNativeHypno269PlantLevel(const Plant*) {return 0;}
bool HasNativeHypno269Ultimate(const Plant*,unsigned) {return false;}
bool HasNativeHypno269ProjectileUltimate(const Projectile*,unsigned) {return false;}
bool HasNativeDoom269Trait(const Plant*,NativeDoom269Trait) {return false;}
int GetNativeStar269MagnetCount(const Plant* plant)
{return GetNativeEquipmentGraphCount(plant);}

bool ApplyNativeDoom269World(Plant* plant,int damage,bool ice,bool setPit)
{
    if(!plant||!IsNativeFusionBoard(plant->mBoard)||damage!=1800)return false;
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
bool NativeSolar269TravelUltimate22(const Plant*) {return false;}
bool NativeSolar269ShootingBoard(const Board*) {return false;}
}
