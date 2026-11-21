#include "FusionNativePlantDamage.h"
#include "FusionNative.h"
#include "FusionNativeCompanions269.h"
#include "FusionNativeWorldActors269.h"
#include "FusionNativeRemainingPlants269.h"
#include "FusionNativeAction267.h"
#include "FusionNativeUmbrellaBatch.h"
#include "FusionNativeWaterPlants267.h"
#include "FusionWaterPlants267Rules.h"
#include "FusionDefenderRules.h"
#include "FusionNativeChomper268.h"
#include "FusionNativeExtras268.h"
#include "FusionNativeStar269.h"
#include "FusionNativeDoom269.h"
#include "../Plant.h"
#include "../Board.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include <algorithm>

namespace LawnFusion
{
bool NativeSourcePlantColliderEnabled(const Plant* p)
{
    if(IsNativeRollingActor269(p))return p&&!p->mDead&&p->mIsOnBoard;
    const auto* data=GetNativePlantData(p);
    if(!data||p->mDead||!p->mIsOnBoard||p->mPlantHealth<=0)return false;
    if(data->kind==12)return !NativeLily267Covered(p);
    const auto* profile=FindFusionDefenderProfile(data->kind);
    if(!profile||profile->layer!=FusionDefenderLayer::Under)return true;
    Plant* other=nullptr;
    while(p->mBoard->mPlants.IterateNext(other))
    {
        const auto* d=GetNativePlantData(other);
        if(other==p||other->mDead||!other->mIsOnBoard||other->mPlantHealth<=0||!d||other->mRow!=p->mRow)continue;
        if(other->mPlantCol==p->mPlantCol)
        {
            const auto* layer=FindFusionDefenderProfile(d->kind);
            if(d->kind!=12&&d->kind!=241&&d->kind!=912&&d->kind!=1082&&
                (!layer||layer->layer!=FusionDefenderLayer::Under))return false;
        }
        if(other->mPlantCol==p->mPlantCol-1&&FusionLily267DoubleBoxKind(d->kind))return false;
    }
    return true;
}
bool AdjustNativeSourcePlantDamage(Plant* p,int& damage,int sourceType)
{
    if(ApplyNativeRollingActor269Damage(p,damage,sourceType)||NativeRemaining269Invulnerable(p))return true;
    if(!p||p->mDead||!p->mIsOnBoard||p->mPlantHealth<=0||p->mNetGhost||
        !GetNativePlantData(p)||!CanApplyNativeFusionGameplay(p->mBoard)||damage<=0)return false;
    if(NativeStar269Invulnerable(p)||TryHandleNativeDoom269Damage(p,damage,sourceType))return true;
    if(AdjustNativeCompanions269Damage(p,damage,sourceType))return true;
    AdjustNativeLaser269Damage(p,damage);if(damage<=0)return true;
    if(NativeLily267Covered(p))return true;
    int capped=damage;
    if(TryCapNativeChomper268IncomingDamage(p,damage,capped))damage=capped;
    // Source MachineNut.Instead is false: it cannot recurse into protectors.
    if(CapNativeRemaining269Damage(p,damage)||CapNativeMachine268Damage(p,damage))return false;
    if(TryHandleNativeDefender267Damage(p,damage,sourceType))return true;
    if(TryHandleNativeUmbrella267Damage(p,damage,sourceType))return true;
    return TryRedirectNativeMachine268Damage(p,damage);
}
void ApplyNativeSourcePlantDamage(Plant* p,int damage,int sourceType)
{
    if(ApplyNativeRollingActor269Damage(p,damage,sourceType)||NativeRemaining269Invulnerable(p))return;
    if(!p||p->mDead||!p->mIsOnBoard||p->mPlantHealth<=0||p->mNetGhost||
        !GetNativePlantData(p)||!CanApplyNativeFusionGameplay(p->mBoard)||damage<=0)return;
    if(AdjustNativeSourcePlantDamage(p,damage,sourceType)||damage<=0)return;
    const int before=p->mPlantHealth;
    p->mPlantHealth=damage>=before?0:before-damage;
    p->mRecentlyEatenCountdown=50;
    NativePlantHealthChanged(p);
    LawnNet::NetTrace("Fusion native source Plant damage id=%u type=%d input=%d hp=%d->%d tick=%d",
        p->mNetId,sourceType,damage,before,p->mPlantHealth,p->mBoard->mMainCounter);
    if(!p->mDead&&p->mPlantHealth<=0)p->Die();
}
}
