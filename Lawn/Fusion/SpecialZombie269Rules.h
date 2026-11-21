#ifndef LAWN_FUSION_SPECIAL_ZOMBIE269_RULES_H
#define LAWN_FUSION_SPECIAL_ZOMBIE269_RULES_H
#include "FusionZombieRules.h"
namespace LawnFusion
{
enum SpecialZombie269Capability:uint32_t
{Special269Model=1,Special269Crash=2,Special269BodyParts=4,Special269DeathEffect=8,
 Special269PaperTransition=16,Special269PaperShot=32,Special269DriverTerrain=64,
 Special269PaperMotion=128,Special269PaperBite=256,Special269VehicleMotion=512,
 Special269VehicleContacts=1024,Special269IncomingDamage=2048,Special269DeathClock=4096,Special269Water=8192};
const FusionZombieProfile* FindSpecialZombie269Profile(uint16_t kind);
bool SpecialZombie269PaperKind(uint16_t kind);
enum class SpecialZombie269StatusEffect:uint8_t {Cold,Freeze,Butter};
bool SpecialZombie269RejectEffect(uint16_t kind,int sourceStatus,SpecialZombie269StatusEffect);
uint32_t SpecialZombie269RequiredCapabilities(uint16_t kind);
bool SpecialZombie269CapabilitiesReady(uint16_t kind,uint32_t actualCapabilities);
// Submarine toggles root children(stage,stage+3). Driver uses paired sprites.
bool TryGetSpecialZombie269BodyStage(uint16_t kind,float health,int maximum,uint8_t& stage);
struct SpecialZombie269PaperClip {const char* name=nullptr;float duration=0,speed=0;};
bool TryGetSpecialZombie269PaperClip(uint16_t kind,int sourceStatus,bool eating,SpecialZombie269PaperClip&);
bool SpecialZombie269PaperAngryDue(float normalizedPhase);
// Exact shared15/104 serialized root-X curve; scale is explicit native policy,
// not a proof of Unity Animator.applyRootMotion world calibration.
bool TrySampleSpecialZombie269PaperRoot(uint16_t kind,float clipSeconds,float& rootX);
bool TryGetSpecialZombie269PaperRootDelta(uint16_t kind,int sourceStatus,FusionZombieClipPosition previous,
    FusionZombieClipPosition current,bool facingRight,double nativeScale,double& delta);
bool TryCountSpecialZombie269PaperBites(uint16_t kind,FusionZombieClipPosition previous,
    FusionZombieClipPosition current,uint32_t& count);
bool StepSpecialZombie269PaperShootCounter(int& ticks,bool angry,bool timeStopped,bool headless);
enum class SpecialZombie269CrashAction:uint8_t {Ignore,Crush,Damage};
struct SpecialZombie269CrashPlan
{SpecialZombie269CrashAction action=SpecialZombie269CrashAction::Ignore;int damage=0;bool knockRight=false,killByCaltrop=false;};
// Genuine source IsCaltrop/tag.nutPlant/theLevel operands; never guessed from HP.
bool TryPlanSpecialZombie269Crash(uint16_t sourceKind,uint16_t plantKind,bool caltrop,
    bool isNut,int sourceLevel,SpecialZombie269CrashPlan&);
bool SpecialZombie269CrashZombieAllowed(bool sameRow,bool sameAllegiance,int sourceStatus);
struct SpecialZombie269CaltropPlan {int damage=0;bool stop=false;};
bool TryPlanSpecialZombie269DriverCaltrop(float health,int maximum,SpecialZombie269CaltropPlan&);
bool TryGetSpecialZombie269DriverVelocity(bool controlled,bool stopped,bool behindIceBoundary,
    float currentSpeed,float& sourceVelocityX);
bool TryGetSpecialZombie269SourcePlantTraits(uint16_t kind,bool& isCaltrop,bool& isNut,int& sourceLevel);
bool TryGetSpecialZombie269VehicleClip(uint16_t kind,bool pendingDeath,SpecialZombie269PaperClip&,bool severeDamage=false);
bool TryGetSpecialZombie269SubmarineRootDelta(FusionZombieClipPosition previous,FusionZombieClipPosition current,
    bool facingRight,double nativeProjection,double& delta);
bool SpecialZombie269VehicleTouchesRect(uint16_t kind,double rootX,double rootY,double scale,bool facingRight,
    double pixelsX,double pixelsY,double left,double top,double width,double height);
}
#endif
