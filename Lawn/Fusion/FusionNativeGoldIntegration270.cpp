#include "FusionNativeGoldBoss269Adapter.h"
#include "FusionNativeUltimateGargantuar270.h"
#include "FusionNative.h"
#include "FusionNativeVisual.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Projectile.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include <cmath>
#include <limits>

namespace LawnFusion
{
namespace
{
bool Current(const Board* b)
{return b&&b->mApp&&b->mApp->mBoard==b&&IsNativeFusionBoard(b);}
template<class State> State* EditZombieState(Zombie* z,std::unordered_map<uint32_t,State>& states,bool create)
{
    const auto uid=z->mBoard->mZombies.DataArrayGetID(z);
    if(!uid)return nullptr;
    if(create)return &states[uid];
    const auto it=states.find(uid);return it==states.end()?nullptr:&it->second;
}
}
bool CanCreditNativeGoldPaper269SourcePoints(Board* b,float amount)
{
    const auto* c=Current(b)?GetNativeContext(b):nullptr;
    return c&&!b->mApp->NetIsClient()&&std::isfinite(amount)&&amount>=0&&
        std::isfinite(c->sourcePoints)&&c->sourcePoints>=0&&std::isfinite(c->sourcePoints+amount);
}
bool CreditNativeGoldPaper269SourcePoints(Board* b,float amount)
{
    if(!CanCreditNativeGoldPaper269SourcePoints(b,amount))return false;
    auto* c=GetNativeContext(b);c->sourcePoints+=amount;
    LawnNet::NetTrace("Fusion Gold270 points amount=%.9g total=%.9g tick=%d",amount,c->sourcePoints,b->mMainCounter);
    return true;
}
NativeGoldBoss269State* EditNativeGoldBoss269State(Zombie* z,bool create)
{
    auto* c=z&&Current(z->mBoard)&&IsNativeGoldPaper269(z)?GetNativeContext(z->mBoard):nullptr;
    return c?EditZombieState(z,c->goldPaper270,create):nullptr;
}
const NativeGoldBoss269State* GetNativeGoldBoss269State(const Zombie* z)
{return EditNativeGoldBoss269State(const_cast<Zombie*>(z),false);}
NativeUltimateGarg270State* EditNativeUltimateGarg270(Zombie* z,bool create)
{
    auto* c=z&&Current(z->mBoard)&&IsNativeUltimateGarg270(z)?GetNativeContext(z->mBoard):nullptr;
    return c?EditZombieState(z,c->ultimateGarg270,create):nullptr;
}
const NativeUltimateGarg270State* GetNativeUltimateGarg270(const Zombie* z)
{return EditNativeUltimateGarg270(const_cast<Zombie*>(z),false);}
NativeUltimateBall270State* EditNativeUltimateBall270(Projectile* p,bool create)
{
    if(!p||!Current(p->mBoard))return nullptr;
    auto* s=EditNativeWorldRemainder269AuxState(p,false);
    if(s)return s->family==NativeWorldRemainder269Family::UltimateBall?s:nullptr;
    if(!create)return nullptr;
    s=EditNativeWorldRemainder269AuxState(p,true);
    if(s){*s={};s->family=NativeWorldRemainder269Family::UltimateBall;s->instanceToken=p->mBoard->mProjectiles.DataArrayGetID(p);s->ageTicks=p->mProjectileAge;}
    return s;
}
const NativeUltimateBall270State* GetNativeUltimateBall270(const Projectile* p)
{return EditNativeUltimateBall270(const_cast<Projectile*>(p),false);}
bool EnsureNativeUltimateBall270Visual(Board* b)
{return Current(b)&&EnsureNativeWorldRemainder269AuxVisual(b,NativeWorldRemainder269Family::UltimateBall,0);}
void NativeUltimateGarg270BoardEvent(Zombie* z,bool born)
{
    if(!z||!Current(z->mBoard)||z->mNetGhost||z->mApp->NetIsClient())return;
    auto* c=GetNativeContext(z->mBoard);if(!c)return;
    auto* state=EditNativeUltimateGarg270(z,false);if(!state)return;
    if(born)
    {
        if(state->boardCounted)return;
        if(c->ultimateGargantuarCount==(std::numeric_limits<int>::max)())return;
        state->boardCounted=true;++c->ultimateGargantuarCount;c->sourcePunishTime=100000.f;
    }
    else if(!z->mMindControlled)
    {
        if(!state->boardCounted)return;
        state->boardCounted=false;
        if(c->ultimateGargantuarCount>0)--c->ultimateGargantuarCount;
        else LawnNet::NetTrace("Fusion Ultimate212 unexpected unmatched death id=%u",z->mNetId);
    }
    LawnNet::NetTrace("Fusion Ultimate212 boardEvent id=%u born=%d count=%d punish=%.9g",z->mNetId,born?1:0,c->ultimateGargantuarCount,c->sourcePunishTime);
}
}
