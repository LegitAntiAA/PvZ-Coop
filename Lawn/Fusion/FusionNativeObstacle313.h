#pragma once
#include "FusionSourceObstacle313.h"
#include "../../SexyAppFramework/Rect.h"
#include <memory>
#include <string>
#include <vector>
namespace Sexy {class Graphics;}
namespace LawnFusion
{
struct FusionAnimationBoxCollider303;
struct NativeSourceObstacle313Contact {uint32_t generation=0;uint8_t entity=0;}; //1plant,2zombie,3projectile.
struct NativeSourceObstacle313Data:FusionSourceObstacle313View
{
    uint32_t clockUID=0; // LOCAL actual native R full generation, never hosttoken.
    std::vector<NativeSourceObstacle313Contact> entered; // HOST ONLY Unity enter-contact memory.
    int tick=-1;bool failed=false; // HOST ONLY, not simulation/wire clocks.
};
class NativeSourceObstacle313Visuals
{
public:
    explicit NativeSourceObstacle313Visuals(Board*);~NativeSourceObstacle313Visuals();
    bool Ensure(FusionSourceObstacle313Kind,uint8_t variant,std::string& error);
    bool Draw(Sexy::Graphics*,Projectile*,const NativeSourceObstacle313Data&) const;
    bool Rect(const Projectile*,const NativeSourceObstacle313Data&,Sexy::Rect&) const;
    bool Box313(const Projectile*,const NativeSourceObstacle313Data&,FusionAnimationBoxCollider303&) const;
private:struct Impl;std::unique_ptr<Impl> mImpl;
};
NativeSourceObstacle313Data* EditNativeSourceObstacle313(Projectile*,bool create=false);
const NativeSourceObstacle313Data* GetNativeSourceObstacle313(const Projectile*);
// Root shared actual Board.Projectile factory, used by Drown1 as well. It calls
// InitializeNativeExtendedObstacle313 for2/3/4 before publishing the fresh raw
// identity. Never puts these records in NativeProjectileData/FusionBullet.
bool CanSpawnNativeSourceObstacle313(Board*,const FusionSourceObstacle313Spawn&,std::string* error=nullptr);
Projectile* SpawnNativeSourceObstacle313(Board*,const FusionSourceObstacle313Spawn&);
bool CanCreateNativeExtendedObstacle313(Board*,const FusionSourceObstacle313Spawn&,std::string* error=nullptr);
bool InitializeNativeExtendedObstacle313(Projectile*,const FusionSourceObstacle313Spawn&,NativeSourceObstacle313Data&,std::string* error=nullptr);
// Root increments raw mProjectileAge once BEFORE all typed obstacle dispatch,
// then this consumes stock update/physics/collision. Never tick on clients.
bool UpdateNativeExtendedObstacle313(Projectile*);
bool DrawNativeExtendedObstacle313(Sexy::Graphics*,Projectile*);
bool GetNativeExtendedObstacle313Rect(const Projectile*,Sexy::Rect&);
bool GetNativeSourceObstacle313Box(const Projectile*,FusionAnimationBoxCollider303&);
bool CaptureNativeObstacle313Clock(const Projectile*,LawnNet::NetZombieAnimState&);
bool AdoptNativeObstacle313Clock(Projectile*,NativeSourceObstacle313Data&,const LawnNet::NetZombieAnimState&);
void RemoveNativeObstacle313Clock(Projectile*,NativeSourceObstacle313Data&);
// Genuine source Obstacle.FlashUpdate .02 decrement steps; raw age clock is
//100Hz, source FixedUpdate50Hz. No visual time accumulated by Draw.
float NativeObstacle313Brightness(const Projectile*,const FusionSourceObstacle313View&);
}
