#ifndef LAWN_FUSION_NATIVE_ULTIMATE_DEPS270_H
#define LAWN_FUSION_NATIVE_ULTIMATE_DEPS270_H
#include "FusionUltimateDeps270Rules.h"
#include "FusionNativeWorldRemainder269.h"
#include <vector>
class Board;class Zombie;class Plant;class Projectile;
namespace Sexy {class Rect;}
namespace LawnFusion
{
struct NativeUltimateDeps270State
{
    uint8_t machineType=0,bodyStage=0;
    bool visible=true,moving=false;
    int8_t originRow=-1,targetRow=-1;
    float startY=0;
    uint16_t rowTicks=0; // Mirror ONLY: actual Zombie.mPhaseCounter owns2s.
    bool deathExecuting=false; // Host-only recursion guard, DO NOT encode/hash.
};
// Root adds one family discriminator to the existing real AUX record/vector.
// type=0fire/1ice, shots=bulletHitCount0..31, nextShotAge=lastHitAge, ready=false.
using NativeUltimateMachineBall270State=NativeWorldRemainder269AuxState;
struct NativeUltimateDeps270Contact {uint32_t generation=0;bool zombie=false;};
NativeUltimateDeps270State* EditNativeUltimateDeps270(Zombie*,bool create=false);
const NativeUltimateDeps270State* GetNativeUltimateDeps270(const Zombie*);
NativeUltimateMachineBall270State* EditNativeUltimateMachineBall270(Projectile*,bool create=false);
const NativeUltimateMachineBall270State* GetNativeUltimateMachineBall270(const Projectile*);
// Host-only Physics2D enter-contact memory; never an actor/clock or a wire field.
std::vector<NativeUltimateDeps270Contact>* EditNativeUltimateMachineBall270Contacts(Projectile*);
bool EnsureNativeUltimateMachineBall270Visual(Board*,uint8_t type);
bool GetNativeUltimateMachineBall270Rect(const Projectile*,Sexy::Rect&);
bool IsNativeUltimateDeps270(const Zombie*);
bool CanCreateNativeUltimateDeps270(Board*,uint16_t kind,int row);
bool InitializeNativeUltimateDeps270(Zombie*);
bool UpdateNativeUltimateDeps270Animation(Zombie*);
bool StartNativeUltimateDeps270Walk(Zombie*,int blendTicks);
bool MoveNativeUltimateDeps270(Zombie*);
bool UpdateNativeUltimateDeps270(Zombie*);
bool GetNativeUltimateDeps270Presentation(const Zombie*,const char*& clip,float& phase);
bool GetNativeUltimateDeps270Status(const Zombie*,int& status);
bool GetNativeUltimateDeps270Rect(const Zombie*,Sexy::Rect&);
void NativeUltimateDeps270AfterKnockback(Zombie*,int sourceReason);
bool ApplyNativeUltimateDeps270BodyDamage(Zombie*,int damage);
// Complete source40 immediate Die transaction, called BEFORE generic source
// health death / DieNoLoot. True consumes the update, including a blocked retry;
// nested native DieNoLoot returns false via the host-only recursion guard.
bool NativeUltimateDeps270Died(Zombie*);
Projectile* CreateNativeUltimateMachineBall270(Board*,uint8_t type,float x,float y,int row,int renderOrder);
bool UpdateNativeUltimateMachineBall270(Projectile*);
bool NativeUltimateDeps270StateValid(uint16_t,const NativeUltimateDeps270State&);
}
#endif
