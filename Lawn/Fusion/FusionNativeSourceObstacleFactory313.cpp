#include "FusionNativeFinish363.h"
#include "FusionNativeExtendedZombie347.h"
#include "FusionDrown343Rules.h"
#include "FusionNativeSourceObstacleFactory313.h"
#include "FusionNativeObstacle313.h"
#include "FusionNativeDrown313Bridge.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNative313SnapshotBridge.h"
#include "FusionNativeEffects269.h"
#include "FusionNative.h"
#include "../Board.h"
#include "../Projectile.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/Attachment.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include <cmath>
#include <limits>

namespace LawnFusion
{
namespace
{
bool Reject(const char* why,std::string* error){if(error)*error=why;return false;}
NativeSourceObstacle313Visuals* Visuals(Board* b)
{
    auto* c=GetNativeContext(b);if(!c)return nullptr;
    if(!c->sourceObstacleVisuals313)c->sourceObstacleVisuals313=std::make_unique<NativeSourceObstacle313Visuals>(b);
    return c->sourceObstacleVisuals313.get();
}
bool KeptClock(Board* b,const Projectile* p,const FusionSourceObstacle313View& v)
{
    const auto* d=p?GetNativeSourceObstacle313(p):nullptr;
    const auto* r=d&&d->clockUID?b->mApp->ReanimationTryToGet(ReanimationID(d->clockUID)):nullptr;
    if(!p||p->mBoard!=b||!d||d->instanceToken!=v.instanceToken||d->kind!=v.kind||!r||r->mDead||r->mReanimationType!=REANIM_ZOMBIE)return false;
    const auto* fx=b->mApp->mEffectSystem;
    const auto* a=fx&&fx->mAttachmentHolder?fx->mAttachmentHolder->mAttachments.DataArrayTryToGet(uint32_t(p->mAttachmentID)):nullptr;
    if(!a||a->mDead||a->mNumEffects<0||a->mNumEffects>MAX_EFFECTS_PER_ATTACHMENT)return false;
    for(int n=0;n<a->mNumEffects;++n)
        if(a->mEffectArray[n].mEffectType==EFFECT_REANIM&&a->mEffectArray[n].mEffectID==d->clockUID)return true;
    return false;
}
}
bool EnsureNativeSourceObstacle313Resources(Board* b,FusionSourceObstacle313Kind kind,uint8_t variant,std::string* error)
{
    if(!b||!b->mApp||b->mApp->mBoard!=b||!GetNativeContext(b)||unsigned(kind)<1||unsigned(kind)>7||
       (kind==FusionSourceObstacle313Kind::DrownWeapon?variant>3:kind==FusionSourceObstacle313Kind::SpiderWeapon?variant!=0:variant>1))
        return Reject("Source obstacle resource identity invalid.",error);
    auto* v=Visuals(b);std::string why;if(!v||!v->Ensure(kind,variant,why)){if(error)*error=why;return false;}
    if((kind==FusionSourceObstacle313Kind::ZombieSquash||kind==FusionSourceObstacle313Kind::MachineBall||kind==FusionSourceObstacle313Kind::BossBall))
    {
        ReanimatorEnsureDefinitionLoaded(REANIM_ZOMBIE,false);
        if(!gReanimatorDefArray||unsigned(REANIM_ZOMBIE)>=gReanimatorDefCount)return false;
    }
    if(kind==FusionSourceObstacle313Kind::ZombieSquash)return LawnNet::NetSourceSound284Ready(b->mApp,74)&&LawnNet::NetSourceSound284Ready(b->mApp,42);
    for(uint16_t fx:kind==FusionSourceObstacle313Kind::DrownWeapon?std::initializer_list<uint16_t>{}:
        (kind==FusionSourceObstacle313Kind::MachineBall||kind==FusionSourceObstacle313Kind::BossBall)?std::initializer_list<uint16_t>{63,64}:std::initializer_list<uint16_t>{41,42})
        if(!EnsureNativeSourceEffect269(b,fx,error))return false;
    return true;
}
bool PreflightNativeSourceObstacle313(Board* b,const FusionSourceObstacle313View& v,Projectile* old,FusionSourceDeathBudget319& additional,std::string* error)
{
    if(!FusionSourceObstacle313ViewValid(v)||!EnsureNativeSourceObstacle313Resources(b,v.kind,v.variant,error))return false;
    FusionSourceDeathBudget319 peak;
    if(v.kind==FusionSourceObstacle313Kind::ZombieSquash||v.kind==FusionSourceObstacle313Kind::MachineBall||v.kind==FusionSourceObstacle313Kind::BossBall)
    {
        if(!Native313ClockTrackMatches(uint16_t(REANIM_ZOMBIE),"anim_walk",v.clock))
            return Reject("MachineBall actual clock does not match anim_walk.",error);
        if(!KeptClock(b,old,v))peak.reanimations=peak.attachments=1;
    }
    additional=peak;return true;
}
bool AdoptNativeSourceObstacle313(Projectile* p,const FusionSourceObstacle313View& v,std::string* error)
{
    if(!p||!p->mNetGhost||!p->mApp->NetIsClient()||!IsNativeSourceWorld313(p->mBoard)||
       !FusionSourceObstacle313ViewValid(v)||!EnsureNativeSourceObstacle313Resources(p->mBoard,v.kind,v.variant,error))return false;
    auto* d=EditNativeSourceObstacle313(p,true);if(!d)return false;
    if((v.kind!=FusionSourceObstacle313Kind::ZombieSquash&&v.kind!=FusionSourceObstacle313Kind::MachineBall&&v.kind!=FusionSourceObstacle313Kind::BossBall)||!KeptClock(p->mBoard,p,v))
    {
        RemoveNativeObstacle313Clock(p,*d);AttachmentDie(p->mAttachmentID);p->mAttachmentID=ATTACHMENTID_NULL;
    }
    const uint32_t localClock=d->clockUID;
    static_cast<FusionSourceObstacle313View&>(*d)=v;d->clockUID=localClock;d->tick=-1;d->failed=false;d->entered.clear();
    p->mProjectileAge=v.ageTicks;p->mVelX=v.velocityX;p->mVelY=v.velocityY;p->mVelZ=v.velocityZ;p->mAccZ=v.accelerationZ;
    return AdoptNativeObstacle313Clock(p,*d,v.clock);
}
bool CanSpawnNativeSourceObstacle313(Board* b,const FusionSourceObstacle313Spawn& s,std::string* error)
{
    if(!IsNativeSourceWorld313(b)||b->mApp->NetIsClient()||!CanApplyNativeFusionGameplay(b)||
       s.row<0||s.row>=GetNativeFusionRowCount(b)||!s.ownerId||!s.ownerToken||
       !std::isfinite(s.centerX)||!std::isfinite(s.centerY)||!std::isfinite(s.groundY)||
       std::abs(s.centerX)>4096||std::abs(s.centerY)>4096||std::abs(s.groundY)>4096||
       !b->mProjectiles.mBlock||b->mProjectiles.mSize>=b->mProjectiles.mMaxSize)
        return Reject("Source obstacle factory requires the live host world and one actual projectile slot.",error);
    if(s.kind!=FusionSourceObstacle313Kind::BossBall&&(s.sourceBirthVelocityX366||s.growthRenderOrder366))return Reject("BossBall birth-only fields on another source obstacle.",error);
    const auto* owner=b->mZombies.DataArrayTryToGet(s.ownerToken);
    const auto* data=GetNativeSourceZombieData(owner);
    if(!owner||owner->mDead||owner->mNetId!=s.ownerId||!data||data->instanceToken!=s.ownerToken)
        return Reject("Source obstacle factory owner generation changed.",error);
    if((s.kind==FusionSourceObstacle313Kind::KirovBomb&&data->kind!=(s.variant?206:26))||
       (s.kind==FusionSourceObstacle313Kind::SpiderWeapon&&data->kind!=221)||
       (s.kind==FusionSourceObstacle313Kind::BossBall&&(data->kind!=44&&data->kind!=46)))return Reject("Source obstacle owner/variant differs.",error);
    if(s.kind==FusionSourceObstacle313Kind::ZombieSquash&&((data->kind!=115&&data->kind!=116)||s.variant!=(data->kind==116?1:0)||s.squashColumn>=10||s.fromHypno))return Reject("Source Squash owner/prefab/cell differs.",error);
    if(s.kind!=FusionSourceObstacle313Kind::DrownWeapon)return CanCreateNativeExtendedObstacle313(b,s,error);
    if(!FusionDrown343OwnerVariant(data->kind,s.variant)||s.fromHypno||((s.variant&1)&&s.targetPlantId)||bool(s.targetPlantId)!=bool(s.targetPlantToken))
        return Reject("Source DrownWeapon factory contract differs.",error);
    if(s.targetPlantId)
    {
        auto* target=b->mPlants.DataArrayTryToGet(s.targetPlantToken);const auto* plant=GetNativePlantData(target);
        if(!target||!plant||plant->instanceToken!=s.targetPlantToken||target->mNetId!=s.targetPlantId)
            return Reject("Source DrownWeapon target generation changed.",error);
    }
    auto* v=Visuals(b);std::string failure;const bool ready=v&&v->Ensure(s.kind,s.variant,failure);
    if(error)*error=failure;return ready;
}
Projectile* SpawnNativeSourceObstacle313(Board* b,const FusionSourceObstacle313Spawn& s)
{
    if(!CanSpawnNativeSourceObstacle313(b,s))return nullptr;
    // The native holder owns position, age, lifetime, raw identity and teardown.
    // PEA is only its inert allocation carrier: typed Draw/Update/Rect consume
    // every stock branch, and no FusionBullet metadata is attached.
    auto* p=b->AddProjectile(int(s.centerX-20),int(s.centerY-20),s.renderOrder,s.row,PROJECTILE_PEA);
    if(!p)return nullptr;
    auto* d=EditNativeSourceObstacle313(p,true);bool ready=d!=nullptr;
    if(ready&&s.kind==FusionSourceObstacle313Kind::DrownWeapon)
    {
        *d={};d->kind=s.kind;d->variant=s.variant;d->instanceToken=b->mProjectiles.DataArrayGetID(p);
        d->ownerId=s.ownerId;d->ownerToken=s.ownerToken;d->targetPlantId=s.targetPlantId;d->targetPlantToken=s.targetPlantToken;
        p->mPosX=s.centerX-20;p->mPosY=s.centerY-20;p->mShadowY=s.groundY;
        p->mRenderOrder=s.renderOrder;p->mX=int(p->mPosX);p->mY=int(p->mPosY);
        ready=FusionSourceObstacle313ViewValid(*d);
    }
    else if(ready)ready=InitializeNativeExtendedObstacle313(p,s,*d);
    if(!ready){p->Die();return nullptr;}
    return p;
}
bool UpdateNativeSourceObstacle313(Projectile* p)
{
    auto* d=EditNativeSourceObstacle313(p);if(!d)return false;
    if(p->mDead||p->mNetGhost||p->mApp->NetIsClient()||!CanApplyNativeFusionGameplay(p->mBoard)||
       d->failed||d->tick==p->mBoard->mMainCounter)return true;
    if(p->mProjectileAge==(std::numeric_limits<int>::max)()){p->Die();return true;}
    ++p->mProjectileAge;
    if(d->kind==FusionSourceObstacle313Kind::DrownWeapon)
    {d->tick=p->mBoard->mMainCounter;return UpdateNativeDrownWeapon313(p,*d,GetNativeDrownWeapon313Hooks());}
    if(d->kind==FusionSourceObstacle313Kind::ZombieSquash){d->tick=p->mBoard->mMainCounter;if(!UpdateNativeSquashItem363(p))if(auto* current=EditNativeSourceObstacle313(p))current->failed=true;return true;}
    if(d->kind==FusionSourceObstacle313Kind::SpiderWeapon)return UpdateNativeSpiderWeapon347(p);
    return UpdateNativeExtendedObstacle313(p);
}
bool DrawNativeSourceObstacle313(Sexy::Graphics* g,Projectile* p)
{
    const auto* d=GetNativeSourceObstacle313(p);if(!d)return false;
    const auto* c=GetNativeContext(p->mBoard);
    if(c&&c->sourceObstacleVisuals313)c->sourceObstacleVisuals313->Draw(g,p,*d);
    return true;
}
bool GetNativeSourceObstacle313Rect(const Projectile* p,Sexy::Rect& out)
{
    const auto* d=GetNativeSourceObstacle313(p);if(!d)return false;
    out=Sexy::Rect();const auto* c=GetNativeContext(p->mBoard);
    if(c&&c->sourceObstacleVisuals313)c->sourceObstacleVisuals313->Rect(p,*d,out);
    return true;
}
void NativeSourceObstacle313Removing(Projectile* p)
{if(auto* d=EditNativeSourceObstacle313(p))RemoveNativeObstacle313Clock(p,*d);}
}
