#include "FusionNativeExtendedZombie313Bridge.h"
#include "FusionNativeSourceAnimated317.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeSourceLifecycle313.h"
#include "FusionNativeZombie313Dispatch.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeAdventure273.h"
#include "FusionNative.h"
#include "FusionNativeWaterZombie313Bridge.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include <cmath>

namespace LawnFusion
{
bool EnsureNativeSourceAnimatedEffect313(Board* b,uint16_t effect,std::string* error)
{
    if(effect!=20){if(error)*error="Unsupported source animated effect.";return false;}
    return EnsureNativeSourceAnimated317(b,4,1,error);
}
bool PlayNativeSourceAnimatedEffect313(Zombie* z,uint16_t effect,float x,float y)
{return effect==20&&EmitNativeSourceAnimated317(z,4,x,y,400040);}
bool PrepareNativeSourceMindControl313(Board* b,FusionSourceDeathBudget319& peak,std::string* error)
{
    if(!IsNativeSourceWorld313(b)||!LawnNet::NetSourceSound284Ready(b->mApp,62)||
       !LawnNet::NetSourceSound284Ready(b->mApp,63)||!EnsureNativeSourceAnimated317(b,4,2,error))return false;
    for(const auto type:{REANIM_SUN,REANIM_COIN_SILVER,REANIM_COIN_GOLD,REANIM_DIAMOND})
        ReanimatorEnsureDefinitionLoaded(type,false);
    peak={};peak.coins=1;peak.reanimations=2;peak.attachments=1;return true;
}
bool ApplyNativeSourceMindControl313(Zombie* z,int level)
{
    const auto* source=GetNativeSourceZombieData(z);NativeZombie313Semantics semantic;
    if(!z||!source||!GetNativeZombie313Semantics(z,semantic)||!IsNativeSourceWorld313(z->mBoard)||
       z->mNetGhost||z->mApp->NetIsClient()||z->mDead)return false;
    (void)level; // Base SetMindControl ignores level; Iron's override owns its gate.
    if(z->mMindControlled||semantic.status==1)return true;
    auto* b=z->mBoard;auto* app=z->mApp;auto* context=GetNativeContext(b);
    const uint32_t uid=b->mZombies.DataArrayGetID(z),token=source->instanceToken,epoch=context->epoch;
    float oldX=0,oldY=0;
    if(!GetNativeSourceZombie269NodePoint(z,"Shadow",oldX,oldY))return false;
    // Retain the established bounded native loot path. At most one ordinary
    // carrier loot Coin (1R/1A), plus this standalone genuine MC effect (1R).
    // Full source DropItem/Garden tables remain a separate port dependency.
    FusionSourceDeathBudget319 peak;
    if(!PrepareNativeSourceMindControl313(b,peak)||!CanAllocateNativeSourceBudget319(b,peak))return false;
    const auto live=[&]() -> Zombie*
    {
        if(app->mBoard!=b)return nullptr;const auto* c=GetNativeContext(b);if(!c||c->epoch!=epoch)return nullptr;
        auto* value=b->mZombies.DataArrayTryToGet(uid);const auto* d=GetNativeSourceZombieData(value);
        return value&&!value->mDead&&d&&d->instanceToken==token?value:nullptr;
    };
    RetireNativeSourceAdventureEnemy273(z);
    z->DropLoot();if(!(z=live()))return true;
    LawnNet::NetPlaySourceSound284(app,62,.5f);LawnNet::NetPlaySourceSound284(app,63,.5f);
    if(!EmitNativeSourceAnimated317(z,4,oldX,oldY-float(1.5*GetNativeFusionPixelsY(b)),400040))return false;
    if(auto* action=GetOrCreateNativeZombieActionData(z))action->jalaedOnDeath=false;
    z->mMindControlled=true;z->mLastPortalX=-1;
    if(auto it=context->extendedZombies313.find(uid);it!=context->extendedZombies313.end())it->second.rootFlip=true;
    if(auto it=context->waterZombies313.find(uid);it!=context->waterZombies313.end())
        if(!NativeWaterZombie313ActorSetFacing(z,true))return false;
    float newX=0,newY=0;
    if(!GetNativeSourceZombie269NodePoint(z,"Shadow",newX,newY))return false;
    z->mPosX+=oldX-newX;z->mPosY+=oldY-newY;z->mX=int(z->mPosX);z->mY=int(z->mPosY);
    if(auto* d=GetNativeSourceZombieData(z))
    {d->targetEntity=NativeZombieTargetEntity::None;d->targetId=0;d->targetGeneration=0;}
    z->mIsEating=false;
    if(auto* graph=EditNativeSourceGraph313(z))graph->parameters.attacking=false;
    return true;
}
}
