#pragma once
#include "FusionWaterZombie313Rules.h"
#include "FusionSourceObstacle313.h"
#include <string>
class Zombie;class Plant;class Board;class Projectile;
namespace LawnFusion
{
struct NativeWaterZombie313State: FusionWaterZombie313View
{
    uint32_t localUID=0,auxiliaryID=0;
    int tick=-1,driverStopAge=-1;
    FusionZombieClipPosition incoming{},previous{};
    bool initialized=false,failed=false,throwRequested=false,jumpRequested=false,jumpFailed=false,deathCallbackHandled=false,driverDeathEventDone=false;
    float throwCountdown=0,failShadowX=0,failShadowY=0;
};
enum class NativeWaterZombie313Operation:uint8_t {Initialize,Jump,Splash,Throw,DriverDamage,DriverStop,Death};
struct NativeWaterZombie313Splash
{uint16_t ownerId=0;uint32_t ownerToken=0;int row=0;float shadowX=0,shadowY=0;
 static constexpr float sourceYOffset=-.4f,rootScale=.4f,lifetime=.8333333f;};
struct NativeWaterZombie313Hooks
{
    NativeWaterZombie313State* (*stateFor)(Zombie*)=nullptr;
    // Whole genuine model/native body+aux/splash/obstacle/part closure. Initial
    // water families need2R0A (Driver+Shake, Snorkel/Dolphin+water), Drown1R0A.
    bool (*prepare)(Zombie*,NativeWaterZombie313Operation,std::string*)=nullptr;
    // Same source clock-only native target/Eat pipeline as other source NPCs.
    bool (*contact)(Zombie*,bool assign)=nullptr;
    bool (*drownContact)(Zombie*)=nullptr; // Initial same-row noncaltrop Plant contact.
    void (*bite)(Zombie*)=nullptr;
    // Actual source plant tags/collider query. The probe dimensions are .7x2,
    // centered Shadow+(-.4,+1), independent of prefab body collider/scale.
    bool (*dolphinContact)(Zombie*,bool& blocked,float& failShadowX,float& failShadowY)=nullptr;
    bool (*placeShadow)(Zombie*,float x,float y)=nullptr;
    bool (*facingKeepShadow)(Zombie*,bool right)=nullptr;
    bool (*splash)(Board*,const NativeWaterZombie313Splash&)=nullptr;
    void (*particle)(Zombie*,uint16_t sourceKind,float sourceYOffset)=nullptr;
    void (*sound)(Zombie*,uint16_t,float volume)=nullptr;
    void (*fallSound)(Zombie*)=nullptr;
    void (*retire)(Zombie*,int sourceReason)=nullptr;
    void (*driverContacts)(Zombie*)=nullptr;
    void (*driverTerrain)(Zombie*)=nullptr;
    // Genuine source target selection and exact typed Board.Projectile birth.
    // Preflight resources/whole-own-shot capacity precedes sound RNG.
    bool (*throwWeapon)(Zombie*)=nullptr;
    int (*randomInt)(Board*,int minimum,int exclusiveMaximum)=nullptr;
};
bool NativeWaterZombie313WorldValid(const Board*,uint16_t,int row,std::string* error=nullptr,bool controlled=false);
bool InitializeNativeWaterZombie313(Zombie*,NativeWaterZombie313State&,const NativeWaterZombie313Hooks&);
bool UpdateNativeWaterZombie313(Zombie*,NativeWaterZombie313State&,const NativeWaterZombie313Hooks&);
bool UpdateNativeWaterZombie313Animation(Zombie*,NativeWaterZombie313State&);
bool MoveNativeWaterZombie313(Zombie*); // Typed consumes stock motion/AI.
bool BeginNativeWaterZombie313Death(Zombie*,NativeWaterZombie313State&,int sourceReason,const NativeWaterZombie313Hooks&);
bool NativeWaterZombie313LoseHead(Zombie*,NativeWaterZombie313State&,const NativeWaterZombie313Hooks&);
bool ApplyNativeDriver313BodyDamage(Zombie*,NativeWaterZombie313State&,int,const NativeWaterZombie313Hooks&);
bool NativeDriver313Caltrop(Zombie*,NativeWaterZombie313State&,const NativeWaterZombie313Hooks&);
bool NativeDriver313DeathEvent(Zombie*,NativeWaterZombie313State&,const NativeWaterZombie313Hooks&);
bool NativeWaterZombie313SetFacing(Zombie*,NativeWaterZombie313State&,bool right,const NativeWaterZombie313Hooks&);
bool NativeWaterZombie313ColliderEnabled(const Zombie*,const NativeWaterZombie313State&);
int NativeWaterZombie313Status(const NativeWaterZombie313State&);
bool CaptureNativeWaterZombie313Auxiliary(const Zombie*,const NativeWaterZombie313State&,LawnNet::NetZombieAnimState&);
bool AdoptNativeWaterZombie313Auxiliary(Zombie*,NativeWaterZombie313State&,const LawnNet::NetZombieAnimState&);
void RemoveNativeWaterZombie313Auxiliary(Zombie*,NativeWaterZombie313State&);
bool GetNativeWaterZombie313Presentation(const Zombie*,const NativeWaterZombie313State&,const char*&,float&);
bool GetNativeWaterZombie313Blend(const Zombie*,const NativeWaterZombie313State&,const char*&,float&,float&);
bool GetNativeWaterZombie313Auxiliary(const Zombie*,const NativeWaterZombie313State&,const char*&,float&,float&,int& layer);
bool TryMakeNativeWaterZombie313Splash(const Zombie*,NativeWaterZombie313Splash&);
}
