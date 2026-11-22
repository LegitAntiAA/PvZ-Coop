#include "FusionNativeTravelConsumers369.h"
#include "FusionDriver363Rules.h"
#include "FusionNativeQuickJackson367.h"
#include "FusionNativeExtendedZombie347.h"
#include "FusionNativeSourceDamage315.h"
#include "FusionNativeBoss361Bridge.h"
#include "FusionSourceDamage315Rules.h"
#include "FusionNative.h"
#include "FusionNativeAdventure273.h"
#include "FusionNativeCharred311.h"
#include "FusionNativeSourceAnimated317.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeZombie313Dispatch.h"
#include "FusionNativeSourceLifecycle313.h"
#include "FusionNativeHeavyZombie313BBridge.h"
#include "FusionNativeExtendedZombie313Bridge.h"
#include "FusionNativeDayZombie273Bridge.h"
#include "FusionNativeNightZombie291Bridge.h"
#include "FusionNativeJackbox309Bridge.h"
#include "FusionNativeZombieVisual.h"
#include "../Zombie.h"
#include "../Board.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"

namespace LawnFusion
{
namespace
{
bool Authority(Zombie* z)
{return z&&z->mApp&&z->mBoard&&z->mApp->mBoard==z->mBoard&&!z->mApp->NetIsClient()&&
    !z->mNetGhost&&!z->mDead&&GetNativeSourceZombieData(z)&&IsNativeSourceWorld313(z->mBoard);}
bool Capture(Zombie* z,SourceCharred311Birth& birth,bool& visual,std::string* error)
{
    const auto* d=GetNativeSourceZombieData(z);if(!d)return false;
    int status=GetNativeFusionZombieStatus(z);bool inWater=z->mInPool,beforeDying=d->beforeDying;
    bool flip=z->IsWalkingBackwards();
    NativeZombie313Semantics semantics;
    if(IsNativeZombie313(z))
    {if(!GetNativeZombie313Semantics(z,semantics))return false;status=semantics.status;inWater=semantics.inWater;beforeDying=semantics.beforeDying;flip=semantics.rootFlip;}
    const uint8_t variant=SourceCharred311SourceVariant(d->kind,status,true,inWater,beforeDying);
    visual=variant!=0;
    if(!visual)return true;
    if(const auto* jackson=GetNativeJackson291(z))flip=z->mMindControlled!=jackson->lookBack;
    return TryMakeNativeSourceCharred311(z,status,inWater,beforeDying,flip,birth,error);
}
bool Terminal(Zombie* z)
{
    SourceCharred311Birth birth;bool visual=false;
    if(!CanRetireNativeSourceZombie313(z,2)||!Capture(z,birth,visual,nullptr))return false;
    if(visual&&!EnsureNativeSourceAnimated317(z->mBoard,birth.variant))return false;
    auto* app=z->mApp;auto* board=z->mBoard;const uint32_t uid=board->mZombies.DataArrayGetID(z);
    // DieEvent may explode and retire other real actors. Consume its source
    // reason2 before native teardown, then re-resolve this exact generation.
    if(!RetireNativeSourceZombie313(z,2))return false;
    if(app->mBoard!=board)return true;
    // Commit the immutable cosmetic birth after the recursive gameplay callback
    // succeeds. Its captured identity/pose remain valid if that callback already
    // retired the owner; no actor or callback is recreated for the picture.
    if(visual)
    {
        if(!StartNativeSourceCharred311(board,birth))return false;
        LawnNet::NetBroadcastSourceCharred315(app,birth);
    }
    (void)uid; // The source dispatcher already retired the captured generation.
    return true;
}
}
bool ResolveNativeSourceBaseDamage315(Zombie* z,int raw,bool fix,int& resolved)
{
    if(!z||!GetNativeSourceZombieData(z))return false;
    if(GetNativeSourceZombieData(z)->kind==207&&!ResolveNativeQuickJackson367RawDamage(z,raw,fix,raw))return false;
    const auto* action=GetNativeZombieActionData(z);
    const auto* context=GetNativeContext(z->mBoard);
    return TrySourceBaseDamage315(raw,z->mMindControlled,action&&action->jalaedOnDeath,resolved,
        context&&context->extendedBoard313.ultimateCount>0,fix,HasNativeTravelAdvanced369(z->mBoard,11),HasNativeTravelAdvanced369(z->mBoard,15),HasNativeTravelAdvanced369(z->mBoard,28));
}
bool CanApplyNativeSourceCharred315(Zombie* z,int damage,std::string* error)
{
    if(!Authority(z)||damage<0){if(error)*error="Source Charred requires an authoritative ordinary source world.";return false;}
    if(const auto* d=GetNativeSourceZombieData(z);d&&(d->kind==222||d->kind==218||d->kind==219||d->kind==220||d->kind==221||d->kind==224))return true;
    if(FusionBoss361Kind(GetNativeSourceZombieData(z)->kind))return true;
    if(UsesNativeHeavyCharred313B(z))return CanApplyNativeHeavyCharred313B(z,damage,false,error);
    // Validate the exact current picture before any shield/armor callback. A
    // status/flag that excludes standalone art still permits genuine Die2.
    SourceCharred311Birth birth;bool visual=false;
    if(!Capture(z,birth,visual,error))return false;
    int resolved=0;
    const auto* d=GetNativeSourceZombieData(z);
    if(!SourceCharred315Direct(d->health,z->mHelmHealth,damage,kFusionZombieSourcePreset)&&
       !ResolveNativeSourceBaseDamage315(z,damage,false,resolved))return false;
    return !visual||EnsureNativeSourceAnimated317(z->mBoard,birth.variant,1,error);
}
bool ApplyNativeSourceCharred315(Zombie* z,int damage)
{
    if(GetNativeSourceZombieData(z)&&GetNativeSourceZombieData(z)->kind==222)return ApplyNativeSourceTypedDamage313(z,10,damage,false);
    if(!CanApplyNativeSourceCharred315(z,damage))return false;
    if(FusionBoss361Kind(GetNativeSourceZombieData(z)->kind))return ApplyNativeSourceTypedDamage313(z,4,damage,false);
    if(ApplyNativeExtended347Charred(z,damage,false))return true;
    if(UsesNativeHeavyCharred313B(z))return ApplyNativeHeavyCharred313B(z,damage,false);
    const auto* d=GetNativeSourceZombieData(z);
    if(SourceCharred315Direct(d->health,z->mHelmHealth,damage,kFusionZombieSourcePreset))return Terminal(z);
    return ApplyNativeSourceTypedDamage313(z,12,damage,false);
}
bool ApplyNativeSourceTypedDamage313(Zombie* z,int sourceType,int raw,bool fix)
{
    if(!Authority(z)||raw<0||(sourceType!=0&&sourceType!=4&&sourceType!=10&&sourceType!=12))return false;
    if(sourceType==12&&!CanApplyNativeSourceCharred315(z,raw))return false;
    int filtered=raw;if(FilterNativeJackbox309Damage(z,raw,filtered)&&filtered==0)return true;
    if(GetNativeSourceZombieData(z)->kind==222)filtered=(std::min)(5000,filtered);
    int remaining=0;
    if(const auto* d=GetNativeSourceZombieData(z);d&&FusionExtendedZombie347Kind(d->kind))
    {if(!ResolveNativeExtended347Damage(z,filtered,fix,remaining))return false;}
    else if(!ResolveNativeSourceBaseDamage315(z,filtered,fix,remaining))return false;
    remaining=ResolveNativeHeavyDamage313B(z,remaining);
    if(const auto* d=GetNativeSourceZombieData(z);d&&FindFusionExtendedZombie313Profile(d->kind))
        if(!TryCapFusionExtendedZombie313Damage(d->kind,remaining,remaining))return false;
    if(ApplyNativeHeavyTakeDamage313B(z,remaining,sourceType))return true;
    auto* app=z->mApp;auto* board=z->mBoard;const uint32_t uid=board->mZombies.DataArrayGetID(z);
    const auto live=[&]() -> Zombie* {return app->mBoard==board?board->mZombies.DataArrayTryToGet(uid):nullptr;};
    // Primary38A5A0: source4 bypasses second armor ONLY; source10/12 damage second armor
    // independently; source0 consumes it before the same first-armor path.
    if(sourceType!=4&&remaining>0&&z->mShieldType!=SHIELDTYPE_NONE)
    {
        const int after=z->TakeShieldDamage(remaining,sourceType==0?0U:2U);
        if(sourceType==0)remaining=after;
    }
    z=live();if(!z||z->mDead)return true;
    if(remaining>0&&z->mHelmType!=HELMTYPE_NONE)remaining=z->TakeHelmDamage(remaining,0U);
    z=live();if(!z||z->mDead||remaining==0)return true;
    const auto* d=GetNativeSourceZombieData(z);if(!d)return false;
    if(sourceType==12&&SourceType12Charred315(d->health,remaining))return Terminal(z);
    z->TakeBodyDamage(remaining,0U);return true;
}
bool CanApplyNativeJackbox309Charred(Zombie* z,int damage,std::string* error)
{return CanApplyNativeSourceCharred315(z,damage,error);}
bool ApplyNativeJackbox309Charred(Zombie* z,int damage)
{return ApplyNativeSourceCharred315(z,damage);}
}
