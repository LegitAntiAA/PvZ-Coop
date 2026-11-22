#pragma once
#include "FusionExtendedZombie313Rules.h"
#include "FusionNativeSourceGraph313.h"
#include <vector>
class Plant;
namespace LawnFusion
{
struct NativeExtendedZombie313State:FusionExtendedZombie313View
{
    // HOST ONLY. Source frame-yield/contact-enter/lifecycle bookkeeping.
    int bornFrame=-1,logicTick=-1;
    bool initialized=false,failed=false,deathConsumed=false;
    std::vector<uint32_t> ironEnterContacts;
};
struct NativeExtendedZombie313Input
{
    float sourceSpeed=1,sourceRootX=0,sourceRootY=0,sourceAxisX=0,sourceAxisY=0,sourceShadowX=0,sourceShadowY=0,landY=0;
    int factoryAxisColumn=9,frame=0,sourceStatusLevel=0;
    float sourceRightBoundary=9.9f;
    bool playMachineSound=true,alternateMachineSound=false;
    bool beforeDying=false,inWater=false,changingRow=false,slope=false,eveBalanced=false,eveWorld=false,travelDebuff6=false;
};
struct NativeExtendedZombie313Ref {uint32_t localUID=0;bool zombie=false;};
struct NativeExtendedZombie313Box {float x=0,y=0,width=0,height=0;};
enum class NativeExtendedZombie313Operation:uint8_t {Initialize,Hammer,Throw,BodyChange,IronContact,IronDeath,MachineContact,MachineDeath,Fall,KirovBomb,KirovCrash,KirovDeath,Particle};
struct NativeExtendedZombie313Hooks
{
    NativeExtendedZombie313State* (*stateFor)(Zombie*)=nullptr;
    NativeSourceGraph313Hooks graph;
    bool (*input)(Zombie*,NativeExtendedZombie313Input&)=nullptr;
    int (*randomInt)(Board*,int minimum,int exclusiveMaximum)=nullptr;
    float (*randomFloat)(Board*,float minimum,float maximum)=nullptr;
    // Genuine source-model/FX/item/projectile closure and complete native peak,
    // including ALL possible descendants before RNG/Die/target mutation.
    bool (*preflight)(Zombie*,NativeExtendedZombie313Operation,std::string*)=nullptr;
    bool (*contacts)(Zombie*,const NativeExtendedZombie313Box&,std::vector<NativeExtendedZombie313Ref>&)=nullptr;
    bool (*bodyContacts)(Zombie*,std::vector<NativeExtendedZombie313Ref>&)=nullptr;
    bool (*eatContact)(Zombie*,const NativeExtendedZombie313Ref&,bool& sourceReacted)=nullptr;
    bool (*baseContact)(Zombie*,bool& sourceAttacking)=nullptr;
    bool (*baseBite)(Zombie*)=nullptr;
    bool (*plantTraits)(const Plant*,bool& nut,bool& bigNut,int& sourceLevel,bool& caltrop,bool& umbrella,bool& canBeBitten)=nullptr;
    bool (*setVelocity)(Zombie*,float sourceVX,float sourceVY)=nullptr;
    bool (*translateRoot)(Zombie*,float sourceDX,float sourceDY)=nullptr;
    bool (*sourceNode)(Zombie*,const char* path,float& sourceX,float& sourceY)=nullptr;
    Zombie* (*spawn)(Zombie* owner,uint16_t childKind,float sourceFactoryRootX,bool controlled)=nullptr;
    bool (*placeRoot)(Zombie*,float sourceX,float sourceY)=nullptr;
    bool (*transferThrowStatus)(Zombie* parent,Zombie* child)=nullptr; // Positive Cold/Freeze +isJalaed, exact source callbacks.
    bool (*alignShadow)(Zombie*,float sourceX,float sourceY)=nullptr;
    bool (*adjustImpFlyNode)(Zombie*,float sourceShadowX,float sourceLandY)=nullptr;
    bool (*changeRow)(Zombie*,float sourceSeconds)=nullptr; // Existing source GetAvailableRow + ChangeRowAndMove.
    bool (*crush)(Zombie*,Plant*,bool sourceCrashVariant,int sourceSound)=nullptr;
    bool (*plantDamage)(Plant*,int amount,int sourceType)=nullptr;
    bool (*zombieDamage)(Zombie*,int sourceType,int amount)=nullptr;
    bool (*projectile)(Zombie*,uint16_t sourceOwnerKind)=nullptr; // Actual SuperBall/UltimateBall, not a pea.
    bool (*kirovBomb)(Zombie*)=nullptr; // Shared actualProjectile obstacle kind KirovBomb2.
    bool (*kirovCrash)(Zombie*,float sourceCenterX,float sourceCenterY,float sourceRadius,int amount)=nullptr; // Shared AoeDamage.MidBomb.
    bool (*machineDeath)(Zombie*,uint8_t machineType)=nullptr; // Existing genuine item29/32/33 +5 actual37 +FX54.
    bool (*ironMindControl)(Zombie*)=nullptr; // Source367C70; common MC alone is insufficient.
    bool (*ironHead)(Zombie*)=nullptr;
    bool (*armorFall)(Zombie*,bool directDrop)=nullptr;
    bool (*sourceBoard212)(Zombie*,bool born)=nullptr; // Source punishTime/count, once per genuine lifetime.
    bool (*effect)(Zombie*,uint16_t sourceEffect,float sourceX,float sourceY)=nullptr;
    void (*sound)(Zombie*,uint16_t sourceSound,float volume)=nullptr;
    void (*flash)(Plant*)=nullptr;
    void (*shake)(Board*,float seconds)=nullptr;
    void (*retire)(Zombie*,int sourceReason)=nullptr;
    void (*failed)(Board*,const char*)=nullptr;
};
bool InitializeNativeExtendedZombie313(Zombie*,NativeExtendedZombie313State&,const NativeExtendedZombie313Hooks&);
bool UpdateNativeExtendedZombie313(Zombie*,NativeExtendedZombie313State&,const NativeExtendedZombie313Hooks&);
bool ApplyNativeExtendedZombie313BodyDamage(Zombie*,NativeExtendedZombie313State&,int,const NativeExtendedZombie313Hooks&);
bool FallNativeExtendedZombie313(Zombie*,NativeExtendedZombie313State&,const NativeExtendedZombie313Hooks&);
bool ApplyNativeExtendedZombie313ArmorDamage(Zombie*,NativeExtendedZombie313State&,int,int& overflow,const NativeExtendedZombie313Hooks&);
bool ThrowNativeExtendedImp313(Zombie*,NativeExtendedZombie313State&,const NativeExtendedZombie313Hooks&);
bool MindControlNativeExtendedZombie313(Zombie*,int sourceControlLevel,const NativeExtendedZombie313Hooks&);
bool DispatchNativeExtendedZombie313Event(Zombie*,FusionSourceGraph313Event,int sourceInt,const NativeExtendedZombie313Hooks&);
enum class NativeExtendedZombie313Death:uint8_t {NotHandled,Proceed,Refused,OwnerRetired};
NativeExtendedZombie313Death BeginNativeExtendedZombie313Death(Zombie*,NativeExtendedZombie313State&,int& sourceReason,const NativeExtendedZombie313Hooks&);
bool NativeExtendedZombie313StateValid(uint16_t,const NativeExtendedZombie313State&);
struct NativeExtendedZombie313Semantics {uint16_t kind=0;int status=0;bool beforeDying=false,inWater=false,rootFlip=false,ownBodyDamage=false,baseLimbCallbacks=false;};
bool GetNativeExtendedZombie313Semantics(Zombie*,const NativeExtendedZombie313State&,const NativeExtendedZombie313Hooks&,NativeExtendedZombie313Semantics&);
bool SetNativeExtendedZombie313BeforeDying(Zombie*,NativeExtendedZombie313State&); // BaseBodyTakeDamage exact pre-LoseHeadEvent.
}
