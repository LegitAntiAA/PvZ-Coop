#include "FusionNativeWorldRemainder269.h"
#include "FusionNative.h"
#include "../Board.h"
#include "../Projectile.h"
#include "../../LawnApp.h"

namespace LawnFusion
{
namespace
{
bool MayCreate(const Projectile* p,bool create)
{return create&&p&&!p->mDead&&p->mApp&&!p->mApp->NetIsClient()&&!p->mNetGhost&&p->mApp->mBoard==p->mBoard;}
template<class T> T* Find(std::unordered_map<uint32_t,T>& map,uint32_t generation,bool create)
{
    if(!generation)return nullptr;const auto found=map.find(generation);
    if(found!=map.end())return &found->second;
    return create?&map.try_emplace(generation).first->second:nullptr;
}
}
NativeWorldRemainder269BoardState* EditNativeWorldRemainder269BoardState(Board* b,bool create)
{
    auto* c=GetNativeContext(b);if(!c)return nullptr;
    if(!c->worldRemainder269Board.initialized&&(!create||!b->mApp||b->mApp->NetIsClient()))return nullptr;
    return &c->worldRemainder269Board;
}
const NativeWorldRemainder269BoardState* GetNativeWorldRemainder269BoardState(const Board* b)
{return EditNativeWorldRemainder269BoardState(const_cast<Board*>(b));}
NativeWorldRemainder269AuxState* EditNativeWorldRemainder269AuxState(Projectile* p,bool create)
{
    auto* c=GetNativeContext(p?p->mBoard:nullptr);if(!c||GetNativeProjectileData(p))return nullptr;
    const auto generation=p->mBoard->mProjectiles.DataArrayGetID(p);
    if(c->companions269.contains(generation)||c->worldBeams269.contains(generation)||c->worldRemainder269Shots.contains(generation))return nullptr;
    return Find(c->worldRemainder269Aux,generation,MayCreate(p,create));
}
const NativeWorldRemainder269AuxState* GetNativeWorldRemainder269AuxState(const Projectile* p)
{return EditNativeWorldRemainder269AuxState(const_cast<Projectile*>(p));}
NativeWorldRemainder269ShotState* EditNativeWorldRemainder269ShotState(Projectile* p,bool create)
{
    auto* c=GetNativeContext(p?p->mBoard:nullptr);const auto* d=GetNativeProjectileData(p);
    if(!c||!d||d->kind!=91)return nullptr;const auto generation=p->mBoard->mProjectiles.DataArrayGetID(p);
    if(c->worldRemainder269Aux.contains(generation))return nullptr;
    return Find(c->worldRemainder269Shots,generation,MayCreate(p,create));
}
const NativeWorldRemainder269ShotState* GetNativeWorldRemainder269ShotState(const Projectile* p)
{return EditNativeWorldRemainder269ShotState(const_cast<Projectile*>(p));}
}
