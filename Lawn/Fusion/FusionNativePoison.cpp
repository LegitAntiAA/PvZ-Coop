#include "FusionNativePoison.h"
#include "FusionNative.h"
#include "FusionNativeZombie313Dispatch.h"
#include "FusionNativeSummon269.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"

namespace LawnFusion
{
namespace
{
bool HostActor(const Zombie* z)
{
    bool source313=false;if(NativeZombie313StatusTarget(z,FusionExtendedEffect313::Poison,source313))return source313;
    return z&&z->mApp&&!z->mApp->NetIsClient()&&!z->mNetGhost&&z->mApp->mBoard==z->mBoard&&
        IsNativeFusionBoard(z->mBoard)&&NativeFusionBackgroundValid(static_cast<uint8_t>(z->mBoard->mBackground))&&
        IsNativeFusionGroundZombie(z);
}
FusionPoisonState Read(const NativeZombieActionData& data)
{return {data.poisonLevel,data.poisonTicks,data.poisonDamageTicks};}
void Commit(NativeZombieActionData& data,const FusionPoisonState& s)
{data.poisonLevel=s.level;data.poisonTicks=s.ticks;data.poisonDamageTicks=s.damageTicks;}
}
int GetNativePoisonLevel(const Zombie* z)
{const auto* data=GetNativeZombieActionData(z);return data?data->poisonLevel:0;}
bool AddNativePoisonLevel(Zombie* z)
{
    if(!HostActor(z)||z->mDead||NativeSummon269RejectStatus(z))return false;
    auto* data=GetOrCreateNativeZombieActionData(z);if(!data)return false;
    const auto before=Read(*data);FusionPoisonState after;
    if(!TryAddFusionPoisonLevel(before,after))return false;
    Commit(*data,after);
    if(before.level>=5)data->jalaedOnDeath=false;
    LawnNet::NetTrace("Fusion native Poison add zombie=%u level=%d remaining=%d pulse=%d tick=%d",z->mNetId,after.level,after.ticks,after.damageTicks,z->mBoard->mMainCounter);
    return true;
}
bool SetNativePoison(Zombie* z,int ticks)
{
    if(!HostActor(z)||z->mDead||NativeSummon269RejectStatus(z))return false;
    auto* data=GetOrCreateNativeZombieActionData(z);if(!data)return false;
    FusionPoisonState after;if(!TrySetFusionPoison(Read(*data),ticks,after))return false;
    Commit(*data,after);data->jalaedOnDeath=false;
    LawnNet::NetTrace("Fusion native Poison set zombie=%u level=%d remaining=%d pulse=%d tick=%d",z->mNetId,after.level,after.ticks,after.damageTicks,z->mBoard->mMainCounter);
    return true;
}
void WarmNativePoison(Zombie* z)
{
    if(!HostActor(z))return;auto* data=GetNativeZombieActionData(z);if(!data)return;
    FusionPoisonState after;if(TryWarmFusionPoison(Read(*data),after))Commit(*data,after);
}
void UpdateNativePoison(Zombie* z)
{
    if(!HostActor(z)||z->mDead||z->IsDeadOrDying()||z->mBodyHealth<=0||z->mBoard->mPaused||
        z->mApp->mGameScene!=SCENE_PLAYING)return;
    auto* data=GetNativeZombieActionData(z);if(!data)return;
    const auto before=Read(*data);FusionPoisonState after;int damage=0;bool pulse=false;
    if(!TryStepFusionPoison(before,after,damage,pulse))return;
    // Commit before native damage: canonical death can erase metadata or recurse.
    Commit(*data,after);
    if(before.ticks>0&&!after.ticks)
        LawnNet::NetTrace("Fusion native Poison expired zombie=%u pulseRemaining=%d tick=%d",z->mNetId,after.damageTicks,z->mBoard->mMainCounter);
    if(pulse)
    {
        LawnNet::NetTrace("Fusion native Poison pulse zombie=%u level=%d damage=%d remaining=%d tick=%d",z->mNetId,after.level,damage,after.ticks,z->mBoard->mMainCounter);
        z->TakeDamage(damage,1U); // Source Shieldless4: bypass shield, preserve helmet/body.
    }
}
}
