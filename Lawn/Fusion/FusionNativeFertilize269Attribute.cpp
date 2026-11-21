#include "FusionNativeRemainingLeaves269.h"
#include "FusionNative.h"
#include "FusionEquipmentRules.h"
#include "FusionAction267Rules.h"
#include "../Plant.h"
#include "../Board.h"
#include "../../LawnApp.h"

namespace LawnFusion
{
namespace
{
struct AttributeClock
{
    int* countdown=nullptr;
    int* mirror=nullptr;
};

AttributeClock SourceAttributeClock(Plant* p,NativePlantData& d)
{
    // Equipment's inherited attribute is TargetX. SunMagnet's Producer clock
    // and delayed item callback (LaunchCounter/TargetY) are independent.
    if(const auto* profile=FindFusionEquipmentProfile(d.kind);
       profile&&profile->family!=FusionEquipmentFamily::Defender)
        return {&p->mTargetX,d.kind==1166?&d.productionTicks:nullptr};

    // Ordinary Producer countdown (+0x7C) is not attributeCountdown (+0x80).
    // Other unbound/inactive attributes are no-ops; attack/digest/phase clocks are never guessed.
    switch(d.kind)
    {
    case 917: case 1147: case 1150:
        // These controllers never borrow StateCountdown for their animations.
        return {&p->mStateCountdown,&d.productionTicks};
    case 924:
        return d.batchMode==4||d.batchMode==5?
            AttributeClock{&p->mStateCountdown,&d.productionTicks}:AttributeClock{};
    case 1031:
        return d.batchMode==0?AttributeClock{&p->mStateCountdown,nullptr}:AttributeClock{};
    case 1188:
        return {&p->mStateCountdown,&d.productionTicks};
    case 1078:
        return d.batchMode==0?AttributeClock{&p->mStateCountdown,&d.productionTicks}:AttributeClock{};
    case 1077:
        return d.batchMode<2?AttributeClock{&p->mTargetX,nullptr}:AttributeClock{};
    default:
        if(IsFusionObsidian267Kind(d.kind))return {&p->mTargetX,nullptr};
        return {};
    }
}
}

void ReduceNativeFertilize269Attribute(Plant* p,int maximumTicks)
{
    if(!p||maximumTicks<0||!p->mApp||!p->mBoard||p->mApp->mBoard!=p->mBoard||
       p->mApp->NetIsClient()||p->mNetGhost||p->mDead||!p->mIsOnBoard||
       !IsNativeFusionBoard(p->mBoard)||!CanApplyNativeFusionGameplay(p->mBoard))return;
    auto* d=GetNativePlantData(p);if(!d||p->mSeedType!=GetNativeSeedForPlant(d->kind))return;
    const auto clock=SourceAttributeClock(p,*d);
    if(!clock.countdown||*clock.countdown<=maximumTicks)return;
    *clock.countdown=maximumTicks;
    if(clock.mirror)*clock.mirror=maximumTicks;
}
}
