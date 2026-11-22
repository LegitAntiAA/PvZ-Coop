#pragma once
#include "FusionZombieRules.h"
#include <span>
#include <vector>
#include <string>
namespace LawnFusion
{
constexpr bool FusionRandomZombie360Kind(uint16_t k){return k==105||k==110||k==215;}
const FusionZombieProfile* FindFusionRandomZombie360Profile(uint16_t);
bool FusionRandom360NotRandom(uint16_t);
bool FusionRandom360UselessHypno(uint16_t);
std::span<const uint16_t> FusionRandom360SourcePrefabs();
// Source105/110 scans0..149;215scans200..arrayLength; ascending nonnullslots,
// exact TypeMgr exclusion and allegiance filter. No native capability filter.
bool BuildFusionRandom360Pool(uint16_t,bool controlled,std::vector<uint16_t>&,std::string* error=nullptr);
// Source direct SetZombie ordinary non-IZ terrain check, not wave row repair.
// false means genuine source null, retaining the RNG draw/retry. output atomic.
bool ResolveFusionRandom360DirectKind(uint16_t selected,bool water,uint16_t& emitted);
struct FusionRandom360Health {float health=0;int maximum=0,first=0,firstMaximum=0,second=0,secondMaximum=0;bool operator==(const FusionRandom360Health&)const=default;};
bool ScaleFusionRandom360Health(const FusionRandom360Health&,float factor,FusionRandom360Health&);
// Ordinary105factor .2..1.2 (draw1..6),110identity/no draw,215 .8..1.2(draw4..6).
bool FusionRandom360ScaleFromDraw(uint16_t,int draw,float& factor);
//105/110 stages latch;215 exact renderer/child appearance recomputes on damage.
bool FusionRandom360ArmorStage(uint16_t,int health,int maximum,uint8_t before,uint8_t&);
struct FusionRandom360ArmorDamage {int armor=0,overflow=0;uint8_t stage=0;bool fall=false;};
bool PlanFusionRandom360ArmorDamage(uint16_t,int armor,int maximum,uint8_t stage,int damage,bool dying,FusionRandom360ArmorDamage&);
}
