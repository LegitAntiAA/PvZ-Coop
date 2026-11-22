#include "FusionNativeDryGround353.h"
#include "FusionGoldConversion313B.h"
#include "FusionNative.h"
#include "FusionNativeGoldBoss269Adapter.h"
#include "FusionNativeZombies.h"
#include "FusionNativeProjection313.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/Attachment.h"
#include <cmath>
#include <limits>
namespace LawnFusion
{
namespace
{
bool Host(const Zombie* z)
{const auto* data=GetNativeSourceZombieData(z);return z&&data&&data->kind==223&&z->mBoard&&z->mApp&&z->mApp->mBoard==z->mBoard&&
    !z->mDead&&!z->mNetGhost&&!z->mApp->NetIsClient()&&!z->mBoard->mPaused&&CanApplyNativeFusionGameplay(z->mBoard);}
bool Hooks(const NativeGoldConversion313BHooks& h)
{return h.birthOrder&&h.axisPoint&&h.prepareChildren&&h.prepareRetire&&h.beginPaperFall&&h.sourceDie2;}
bool Capacity(Board* b,size_t count)
{
    auto* e=b&&b->mApp?b->mApp->mEffectSystem:nullptr;
    if(!e||e!=gEffectSystem||!e->mReanimationHolder||!e->mAttachmentHolder||!b->mZombies.mBlock)return false;
    const auto& r=e->mReanimationHolder->mReanimations;const auto& a=e->mAttachmentHolder->mAttachments;
    return r.mBlock&&a.mBlock&&b->mZombies.mMaxSize-b->mZombies.mSize>=int(count)&&
        r.mMaxSize-r.mSize>=int(count*2)&&a.mMaxSize-a.mSize>=int(count);
}
}
bool PrepareNativeGoldConversion313B(Zombie* parent,const NativeGoldConversion313BHooks& h,NativeGoldConversion313BPlan& output,std::string* error)
{
    const auto fail=[&](const char* message){if(error)*error=message;return false;};
    if(!Host(parent)||!Hooks(h))return fail("Gold313B missing source223 authority/callbacks");
    auto* b=parent->mBoard;std::vector<uint32_t> order;if(!h.birthOrder(b,order))return fail("Gold313B source birth order unavailable");
    std::vector<FusionGoldConversion313BActor> actors;actors.reserve(order.size());
    for(uint32_t uid:order)
    {
        auto* old=b->mZombies.DataArrayTryToGet(uid);const auto* d=GetNativeSourceZombieData(old);
        if(!old||old->mDead||!d)continue;
        float x=0,y=0;if(!h.axisPoint(old,x,y)||old->mRow<0||old->mRow>=GetNativeFusionRowCount(b))return fail("Gold313B actual source axis/row unavailable");
        actors.push_back({uid,d->kind,uint8_t(old->mRow),uint8_t(b->mPlantRow[old->mRow]==PLANTROW_POOL?1:0),x,true});
    }
    NativeGoldConversion313BPlan next;
    if(!BuildFusionGoldConversion313B(actors,next.actors))return fail("Gold313B malformed source actor order");
    std::vector<uint16_t> children(next.actors.size(),203);std::vector<uint32_t> retire;retire.reserve(next.actors.size());
    for(const auto& a:next.actors){if(!NativeSourceDryRow353(b,a.row))return fail("Gold313B replacement row is not a prepared dry source row");retire.push_back(a.localUID);}
    if(!Capacity(b,next.actors.size())||!h.prepareChildren(b,children,error)||!h.prepareRetire(parent,retire,error))return false;
    next.parentLocalUID=b->mZombies.DataArrayGetID(parent);next.preparedTick=b->mMainCounter;
    output=std::move(next);return true;
}
bool CommitNativeGoldConversion313B(Zombie* parent,const NativeGoldConversion313BHooks& h,const NativeGoldConversion313BPlan& plan,bool& failed,std::string* error)
{
    const auto fail=[&](const char* message){failed=true;if(error)*error=message;return false;};
    if(failed||!Host(parent)||!Hooks(h)||!plan.parentLocalUID||
        parent->mBoard->mZombies.DataArrayGetID(parent)!=plan.parentLocalUID||parent->mBoard->mMainCounter!=plan.preparedTick)return false;
    auto* b=parent->mBoard;
    if(!Capacity(b,plan.actors.size()))return false;
    // Validate every original identity before any irreversible virtual callback.
    for(const auto& actor:plan.actors)
    {
        auto* old=b->mZombies.DataArrayTryToGet(actor.localUID);const auto* d=GetNativeSourceZombieData(old);float x=0,y=0;
        if(!old||old->mDead||!d||d->kind!=actor.kind||old->mRow!=actor.row||!h.axisPoint(old,x,y)||x!=actor.axisX)return false;
    }
    if(!h.beginPaperFall(parent))return fail("Gold313B prepared parent SecondArmorFall failed");
    for(const auto& actor:plan.actors)
    {
        auto* old=b->mZombies.DataArrayTryToGet(actor.localUID);
        if(!old||old->mDead)return fail("Gold313B old full generation changed during conversion callbacks");
        NativeSourceZombieSpawnRequest request;request.kind=203;request.row=actor.row;request.fromWave=parent->mFromWave;
        // SetZombie's default allegiance is genuine: it does not inherit hypnosis.
        auto* child=SpawnNativeSourceZombie(b,request);auto* d=GetNativeSourceZombieData(child);float axis=0,y=0;
        if(!child||!d||d->kind!=203||!h.axisPoint(child,axis,y))return fail("Gold313B prepared203 factory/axis failed");
        const double maximum=double(child->mBodyMaxHealth)*5,first=double(child->mHelmMaxHealth)*5,second=double(child->mShieldMaxHealth)*5;
        if(!std::isfinite(d->health*5)||maximum>(std::numeric_limits<int>::max)()||first>(std::numeric_limits<int>::max)()||second>(std::numeric_limits<int>::max)())
            return fail("Gold313B child health scale not covered by preflight");
        if(!SetNativeSourceAxis313(child,actor.axisX,y))return fail("Gold313B prepared source-axis projection failed");
        d->health*=5;child->mBodyHealth=NativeSourceZombieLivenessHealth(d->health);child->mBodyMaxHealth=int(maximum);
        child->mHelmHealth*=5;child->mHelmMaxHealth=int(first);child->mShieldMaxHealth=int(second);child->mShieldHealth=0;
        if(!h.beginPaperFall(child))return fail("Gold313B prepared203 SecondArmorFall failed");
        h.sourceDie2(old);
    }
    return true;
}
}
