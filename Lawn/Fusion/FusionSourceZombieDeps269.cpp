#include "FusionSourceZombieDeps269.h"
#include "FusionNative.h"
#include "../Zombie.h"
#include "../Board.h"
#include "../../LawnApp.h"
#include <algorithm>
#include <cmath>
#include <utility>
namespace LawnFusion
{
namespace
{
constexpr FusionZombieProfile profiles[]={
    {111,270,2200,0,50,true},
    {109,270,4400,0,50,false},
    {104,1500,0,200,50,true},
    {203,1500,0,1500,400,true}};
int WrappedTwoThirds(int maximum)
{const uint32_t twice=uint32_t(maximum)*2U;const int64_t signedTwice=twice&0x80000000U?int64_t(twice)-0x100000000LL:twice;return int(signedTwice/3);}
}
const FusionZombieProfile* FindFusionSourceZombieDeps269Profile(uint16_t kind)
{for(const auto& p:profiles)if(p.kind==kind)return &p;return nullptr;}
bool TryGetFusionSourceZombieDeps269ArmorStage(uint16_t kind,int health,int maximum,int previous,int& stage,int& sprite)
{
    if(!FindFusionSourceZombieDeps269Profile(kind)||maximum<=0||health<0||health>maximum||previous<0||previous>2)return false;
    int next=previous;if(health<WrappedTwoThirds(maximum)&&next<1)next=1;
    if(health<maximum/3&&next<2)next=2;
    stage=next;sprite=next?((kind==111?26:kind==109?21:9)+next):-1;return true; //11127/28,10922/23,Paper10/11.
}
bool TryStepFusionSourceZombie111Armor(int health,int damage,int& after,int& overkill)
{
    if(health<0||damage<0)return false;
    after=damage>=health?0:health-damage;overkill=damage>=health?damage-health:0;return true;
}
bool TryApplyNativeSourceZombie111Armor(Zombie* z,int damage,int& overkill)
{
    const auto* d=GetNativeSourceZombieData(z);if(!d||(d->kind!=111&&d->kind!=109))return false;
    overkill=0;if(!z->mApp||z->mNetGhost||z->mApp->NetIsClient()||z->mDead||!CanApplyNativeFusionGameplay(z->mBoard))return true;
    int after=0;if(!TryStepFusionSourceZombie111Armor(z->mHelmHealth,damage,after,overkill))return true;
    z->mHelmHealth=after;
    if(!after)z->mHelmType=HELMTYPE_NONE;
    // Inherited WallNutZ.FirstArmorTakeDamage356B70 directly clears its armor
    // reference; it NEVER calls FirstArmorFall, native DropHelm or particles.
    return true;
}
bool FusionSourceZombieDeps269RejectStatus(uint16_t kind,int status,FusionSourceZombieDeps269Status effect,bool debuff)
{
    if(kind==111)return effect==FusionSourceZombieDeps269Status::Cold||effect==FusionSourceZombieDeps269Status::Freeze;
    if(kind!=203)return false;
    if(effect==FusionSourceZombieDeps269Status::Cold)return debuff;
    return debuff||status==5;
}
int FusionSourcePaper269DamageType(uint16_t kind,int status,int incoming)
{return kind==203&&status==4?0:incoming;}
bool FusionSourcePaper269BodyAcceptsDamage(uint16_t kind,int status)
{return kind==104||(kind==203&&status==6);}
bool TryStepFusionSourcePaper269Shoot(float before,float interval,float delta,int status,bool stopped,bool headless,float& after,bool& play)
{
    if(!std::isfinite(before)||!std::isfinite(interval)||interval<=0||!std::isfinite(delta)||delta<0||delta>1)return false;
    after=before;play=false;if(status==6&&!stopped&&!headless){after-=delta;if(after<0){after=interval;play=true;}}return true;
}
bool TryMakeFusionSourcePaper269Shot(uint16_t kind,int status,bool headless,bool buttered,int move,bool controlled,FusionSourcePaper269Shot& out)
{
    if((kind!=104&&kind!=203)||status==1||headless||buttered)return false;
    FusionSourcePaper269Shot shot;shot.way=move==0?9:0;shot.hostile=!controlled;shot.offsetX=kind==104?.1f:0.f;out=shot;return true;
}
bool TryMakeFusionSourceRandom269Pool(const std::array<bool,150>& present,const std::array<bool,150>& excluded,const std::array<bool,150>& hypno,
    bool controlled,bool eve,std::vector<uint16_t>& out)
{
    if(eve)return false; // Source GetEveZombie has its own complete contract.
    std::vector<uint16_t> pool;for(uint16_t i=0;i<150;++i)if(present[i]&&!excluded[i]&&(!controlled||!hypno[i]))pool.push_back(i);
    if(pool.empty())return false;out=std::move(pool);return true;
}
bool TrySelectFusionSourceRandom269(const std::vector<uint16_t>& pool,int draw,uint16_t& kind)
{
    if(pool.empty()||draw<0||size_t(draw)>=pool.size())return false;
    for(size_t i=0;i<pool.size();++i)if(pool[i]>=150||(i&&pool[i]<=pool[i-1]))return false;
    kind=pool[size_t(draw)];return true;
}
}
