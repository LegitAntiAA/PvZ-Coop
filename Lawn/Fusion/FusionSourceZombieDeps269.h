#ifndef LAWN_FUSION_SOURCE_ZOMBIE_DEPS269_H
#define LAWN_FUSION_SOURCE_ZOMBIE_DEPS269_H
#include "FusionZombieRules.h"
#include <array>
#include <vector>
class Zombie;
namespace LawnFusion
{
// Source facts/proposed opt-in profiles, NOT an admission registry. Paper
// kinds require their genuine shield transition and Bullet3 kernel first.
const FusionZombieProfile* FindFusionSourceZombieDeps269Profile(uint16_t kind);
bool TryGetFusionSourceZombieDeps269ArmorStage(uint16_t kind,int health,int maximum,int previous,int& stage,int& spriteSlot);
bool TryStepFusionSourceZombie111Armor(int health,int damage,int& after,int& overkill);
// Hook first inside native TakeHelmDamage; no DropHelm/armor-fall callback.
bool TryApplyNativeSourceZombie111Armor(Zombie*,int damage,int& overkill);
enum class FusionSourceZombieDeps269Status:uint8_t {Cold,Freeze,Butter};
bool FusionSourceZombieDeps269RejectStatus(uint16_t kind,int status,FusionSourceZombieDeps269Status,bool travelDebuff1=false);
int FusionSourcePaper269DamageType(uint16_t kind,int status,int incomingType);
bool FusionSourcePaper269BodyAcceptsDamage(uint16_t kind,int status);
bool TryStepFusionSourcePaper269Shoot(float before,float interval,float delta,int status,bool timeStopped,bool headless,float& after,bool& playShoot);
struct FusionSourcePaper269Shot
{uint16_t kind=3;int damage=300;uint8_t way=0;bool hostile=true;float offsetX=0,offsetY=-.1f;};
bool TryMakeFusionSourcePaper269Shot(uint16_t kind,int status,bool headless,bool buttered,int sourceMoveType,bool controlled,FusionSourcePaper269Shot&);
// Complete source RandomZombie.SetRandomZombie35B1F0 pool. Presence and both
// exclusion predicates come from source tables. NEVER pass native availability
// as source presence or filter this result to the implemented native subset.
bool TryMakeFusionSourceRandom269Pool(const std::array<bool,150>& sourcePresent,const std::array<bool,150>& notRandom,
    const std::array<bool,150>& uselessHypno,bool controlled,bool eveMode,std::vector<uint16_t>& out);
bool TrySelectFusionSourceRandom269(const std::vector<uint16_t>& fullSourcePool,int draw,uint16_t& kind);
}
#endif
