#pragma once
#include "FusionHeavyZombie313BRules.h"
#include "FusionNativeSourceGraph313.h"
#include "FusionNativeGoldBoss269.h"
#include <span>
#include <string>
class Board;class Zombie;class Plant;class Reanimation;
namespace LawnFusion
{
// Common graph/body clocks use the shared313 codec; this view adds only world values.
struct FusionHeavyZombie313BView
{
    uint8_t status=0,ammo=0,armorStage=0,pogoWait=0;
    uint8_t catapultFlags343=0; //216 only:bodyBroken1, weaponSet1removed2, set2removed4.
    uint16_t deliveryKind=0;
    bool shooting=false,ladderVisible=false,axePresent=true,lookBack=false,caltropDying=false;
    bool pogoPaused=false,pogoStarted=false,jumped=false,lostJumper=false,shadowEnabled=true;
    float jumpSpeed=1;
    FusionBungee313BView bungee;
    bool operator==(const FusionHeavyZombie313BView&) const=default;
};
struct NativeHeavyZombie313BState: FusionHeavyZombie313BView
{
    NativeGoldBoss269State gold; // Pure269 arithmetic/award guards, never another actor.
    // HOST ONLY. Local generations are distinct from opaque host wire tokens.
    uint32_t targetPlantUID=0,capturedPlantUID=0;
    int worldTick=-1;
    bool initialized=false,failed=false,bossStarted366=false;
};
struct NativeHeavyZombie313BHooks
{
    NativeSourceGraph313State* (*graphStateFor)(Zombie*)=nullptr; // Root context sourceGraphs313, one owner.
    bool (*plantTraits)(const Plant*,FusionHeavy313BPlant&)=nullptr;
    bool (*node)(const Zombie*,const char*,float&,float&)=nullptr; // Actual Board pixels.
    bool (*sourcePoint)(const Zombie*,const char*,float&,float&)=nullptr; // Same sampled node, source world units.
    bool (*setAxis)(Zombie*,float sourceX,float sourceY)=nullptr; // Same native pos/altitude.
    bool (*bounds)(const Board*,float& minX,float& maxX,float& maxY)=nullptr;
    bool (*landY)(const Board*,float sourceX,int row,float& sourceY)=nullptr;
    bool (*contact)(Zombie*,bool canAttack)=nullptr;
    void (*bite)(Zombie*)=nullptr;
    bool (*crash)(Zombie*)=nullptr;
    bool (*prepareEvent)(Zombie*,FusionSourceGraph313Event,std::string*)=nullptr;
    bool (*prepareShot)(Zombie*,Plant*,std::string*)=nullptr;
    bool (*prepareCapture)(Zombie*,std::span<const uint32_t>,std::string*)=nullptr;
    bool (*capture)(Zombie*,Plant*)=nullptr; // E1 real Die5/parented retained-model lease.
    bool (*umbrellaBlocks)(Plant*,Zombie*)=nullptr;
    bool (*magnetContents)(const Plant*,std::vector<uint16_t>&)=nullptr;
    void (*sound)(Zombie*,uint16_t sourceSound)=nullptr;
    void (*fallSound)(Zombie*)=nullptr;
    void (*sourceDie)(Zombie*,int sourceReason)=nullptr;
    bool (*sourceEffect)(Zombie*,uint16_t,float sourceYOffset)=nullptr;
    void (*ladderPlaced)(Zombie*,Plant*)=nullptr;
    // Gold leaves must not invoke the obsolete269 body/head clock updater.
    bool (*goldEvent)(Zombie*,NativeHeavyZombie313BState&,FusionSourceGraph313Event)=nullptr;
    bool (*goldUpdate)(Zombie*,NativeHeavyZombie313BState&)=nullptr;
};
bool ValidateFusionHeavyZombie313BView(uint16_t,const FusionHeavyZombie313BView&);
bool NativeHeavyZombie313BKind(uint16_t);
bool NativeHeavyZombie313BOwnsPhaseCounter(const Zombie*);
bool InitializeNativeHeavyZombie313B(Zombie*,NativeHeavyZombie313BState&,const NativeHeavyZombie313BHooks&,const NativeSourceGraph313Hooks&);
bool UpdateNativeHeavyZombie313B(Zombie*,NativeHeavyZombie313BState&,const NativeHeavyZombie313BHooks&,const NativeSourceGraph313Hooks&);
bool GetNativeHeavyZombie313BParameters(Zombie*,NativeHeavyZombie313BState&,const NativeHeavyZombie313BHooks&,FusionSourceGraph313Parameters&);
bool DispatchNativeHeavyZombie313BEvent(Zombie*,NativeHeavyZombie313BState&,FusionSourceGraph313Event,int32_t,const NativeHeavyZombie313BHooks&);
bool PreflightNativeHeavyZombie313BEvent(Zombie*,const NativeHeavyZombie313BState&,FusionSourceGraph313Event,const NativeHeavyZombie313BHooks&,std::string* error=nullptr);
bool RootDeltaNativeHeavyZombie313B(Zombie*,const NativeHeavyZombie313BState&,uint8_t sourceState,FusionSourceGraph313Cursor,FusionSourceGraph313Cursor,double&);
bool BeginNativeHeavyZombie313BDeath(Zombie*,NativeHeavyZombie313BState&,const NativeHeavyZombie313BHooks&);
bool NativeHeavyZombie313BContact(Zombie*,NativeHeavyZombie313BState&,Plant*,const NativeHeavyZombie313BHooks&);
bool NativeHeavyZombie313BLoseLadder(Zombie*,NativeHeavyZombie313BState&,const NativeHeavyZombie313BHooks&);
bool NativeHeavyZombie313BLoseJumper(Zombie*,NativeHeavyZombie313BState&,int sourceReason,const NativeHeavyZombie313BHooks&);
bool NativeMiner313BAxeLost(Zombie*,NativeHeavyZombie313BState&,bool naturalRise,const NativeHeavyZombie313BHooks&);
bool KillNativeCatapult313BByCaltrop(Zombie*,NativeHeavyZombie313BState&,const NativeHeavyZombie313BHooks&);
bool ApplyNativeCatapult313BBodyDamage(Zombie*,int damage,const NativeHeavyZombie313BHooks&);
bool ConfigureNativeBungee313BDelivery(Zombie*,NativeHeavyZombie313BState&,uint16_t sourceChildKind);
bool MoveNativeHeavyZombie313B(Zombie*); // Consumes stock motion; graph/world already moved.
}
