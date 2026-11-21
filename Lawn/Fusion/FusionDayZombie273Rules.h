#ifndef LAWN_FUSION_DAY_ZOMBIE273_RULES_H
#define LAWN_FUSION_DAY_ZOMBIE273_RULES_H
#include "FusionZombieRules.h"
#include <span>

namespace LawnFusion
{
// Proposed profiles only. Root admits each family after its real dependencies,
// model, native hooks and wire state are present on both peers.
const FusionZombieProfile* FindFusionDayZombie273Profile(uint16_t kind);
bool FusionDayZombie273Doll(uint16_t kind);
bool FusionDayZombie273Shooter(uint16_t kind);
std::span<const uint16_t> FusionDayZombie273NpcDependencies(uint16_t kind);
enum FusionDayZombie273Capability : uint32_t
{
    Day273Model=1, Day273Wire=2, Day273SharedBody=4, Day273Armor=8,
    Day273DollFall=16, Day273Pole=32, Day273HeadClock=64,
    Day273Bullet0=128, Day273Bullet11=256, Day273PlantContact=512,
    Day273Fx=1024, Day273BodyParts=2048, Day273BucketItem=4096
};
uint32_t FusionDayZombie273RequiredCapabilities(uint16_t kind);
bool FusionDayZombie273CapabilitiesReady(uint16_t kind,uint32_t actual);
// Root's admission gate must use this combined predicate. Listing a Doll
// profile/model alone cannot admit it without every genuine spawned NPC.
bool FusionDayZombie273Ready(uint16_t kind,uint32_t actual,std::span<const uint16_t> readyNpcKinds);

struct FusionDayZombie273Clip
{
    const char* sourceName=nullptr;
    const char* nativeTrack=nullptr;
    float seconds=0, multiplier=0;
    bool loop=true;
};
// Pole status2 runs,3 vaults,0 walks/eats; plant zombies have one walk2 curve.
// Dolls retain the existing common Zombie/Cone controller and bite dispatcher.
bool TryGetFusionDayZombie273Clip(uint16_t kind,int sourceStatus,bool eating,FusionDayZombie273Clip&);
bool TryGetFusionDayZombie273RootDelta(uint16_t kind,int sourceStatus,FusionZombieClipPosition before,
    FusionZombieClipPosition after,bool facingRight,double nativeProjection,double& delta);
bool TryCountFusionDayZombie273Bites(uint16_t kind,FusionZombieClipPosition before,
    FusionZombieClipPosition after,uint32_t& count);
enum FusionDayZombie273PoleEvent : uint8_t {Day273JumpSound1=1,Day273JumpSound2=2,Day273JumpFail=4,Day273JumpOver=8};
bool TryGetFusionDayZombie273PoleEvents(FusionZombieClipPosition before,FusionZombieClipPosition after,uint8_t& events);
struct FusionDayZombie273PlantTraits
{
    bool flying=false,caltrop=false,tallNut=false,ladder=false;
    float shadowX=0,shadowY=0;
};
bool FusionDayZombie273PoleCanStart(int status,bool controlled,bool frozen,bool sameRow,bool plantAhead,
    const FusionDayZombie273PlantTraits&);
bool FusionDayZombie273PoleBlocksLanding(bool sameRow,const FusionDayZombie273PlantTraits&);

// Update36DD70: only Dying, Board time-stop and beforeDying stop this timer.
// AnimShoot36DB60/367C80 also excludes changing-row. BodyTakeDamage380280
// sets beforeDying at the head-loss threshold; native projection is !mHasHead.
// Freeze/butter are distinct source fields, not substitutes for these guards.
bool TryStepFusionDayZombie273Shoot(uint16_t kind,float before,float interval,float delta,int sourceStatus,
    bool timeStopped,bool beforeDying,float jitter,float& after,bool& playShoot);
struct FusionDayZombie273Shot
{
    uint16_t kind=0;
    int damage=0;
    uint8_t way=0;
    bool hostile=false;
    float offsetX=0;
};
bool TryMakeFusionDayZombie273Shot(uint16_t kind,int sourceStatus,bool beforeDying,bool changingRow,
    int sourceMoveType,bool controlled,FusionDayZombie273Shot&);
bool FusionDayZombie273ProjectileTuple(uint16_t kind,int damage,uint8_t way,bool hostile);
bool FusionDayZombie273ProjectileCanHit(int sourceStatus);
struct FusionDayZombie273ZombieHit
{int damage=0,sourceDamageType=0;float knockback=0;};
// Pea reverse way9 uses source damage4. Iron adds second armor with C# I32
// wrapping, uses damage0 and KnockBack(+.2/- .2,reason1), never first armor.
bool TryPlanFusionDayZombie273ZombieHit(uint16_t kind,uint8_t way,int damage,int secondArmor,
    bool targetControlled,FusionDayZombie273ZombieHit&);
bool TryGetFusionDayZombie273ArmorStage(uint16_t kind,int health,int maximum,uint8_t previous,
    uint8_t& stage,int& spriteSlot);
// WallNutZ356B70 clears its reference directly. No FirstArmorFall for106/107.
bool TryStepFusionDayZombie273Armor(uint16_t kind,int before,int damage,int& after,int& overkill);
bool TryGetFusionDayZombie273DollChild(uint16_t kind,uint16_t& child);
bool FusionDayZombie273DollCanFall(int live,int capacity);
}
#endif
