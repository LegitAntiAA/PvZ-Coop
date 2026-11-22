#pragma once
#include <cstdint>
#include <span>
#include <vector>
#include "FusionZombieRules.h"

namespace LawnFusion
{
const FusionZombieProfile* FindFusionHeavyZombie313BProfile(uint16_t kind);
// Source identities/tags are supplied by the existing native actors/TypeMgr.
// Positions/velocities here are source units, never another actor/world.
struct FusionHeavy313BPlant
{
    uint32_t uid=0;
    uint16_t kind=0xffff;
    int column=0,row=0;
    float shadowX=0,shadowY=0;
    bool alive=false,flying=false,caltrop=false,nut=false,tallNut=false,bigNut=false;
    bool lily=false,pot=false,goldPot=false,doubleBox=false,ladder=false;
};
bool FusionCatapult313BPlantAllowed(const FusionHeavy313BPlant&,int row,float axisX);
// First eligible item wins an equal-distance tie, matching the source List loop.
bool SelectFusionCatapult313BTarget(std::span<const FusionHeavy313BPlant>,int row,
    float axisX,float shadowX,uint32_t& uid);
bool FusionCatapult313BCanSearch(bool controlled,float shadowX,float boardMaxX,uint8_t ammo);
bool FusionCatapult313BBroken(float health,int maximum);
// Bits are source root GetChild(3..6), not native sprite-name guesses.
uint8_t FusionCatapult313BHiddenBallChildren(uint8_t ammo);

enum class FusionMiner313BRise:uint8_t { None=0,Natural=1,Surprised=2 };
FusionMiner313BRise FusionMiner313BCheckRise(uint8_t status,float shadowX,bool axeExists,bool eveBalanced=false);
bool FusionMiner313BCanCold(uint8_t status);
bool FusionMiner313BPlantSurprises(uint16_t kind,std::span<const uint16_t> magnetContents={});

struct FusionPogo313BView
{
    uint8_t status=14,wait=0;
    bool paused=false,started=false,jumped=false,shadowEnabled=true,lostJumper=false;
    bool operator==(const FusionPogo313BView&) const=default;
};
bool FusionPogo313BViewValid(uint16_t kind,const FusionPogo313BView&);
bool FusionPogo313BCanPause(const FusionPogo313BView&,bool controlled,int row,float shadowX,const FusionHeavy313BPlant&);
bool FusionPogo313BLandSound(FusionPogo313BView&);
bool FusionPogo313BJumpBlocked(const FusionPogo313BView&,bool beforeDying,int row,float shadowX,const FusionHeavy313BPlant&);
bool FusionPogo313BFreezeBecomesCold(uint8_t status,bool beforeDying);
float FusionPogo313BHorizontalSpeed(uint8_t wait,float speed,bool towardsRight);
void FusionPogo313BLoseJumper(FusionPogo313BView&,uint16_t kind,bool dying,int reason);
struct FusionSuperPogo313BStep
{
    FusionPogo313BView after;
    float waitSeconds=0,velocityX=0,velocityY=0;
    bool startJump=false;
};
// waitSeconds is the source authoritative waitTime; a native adapter stores it
// in its existing phase counter. Ghosts adopt the canonical view/body clock.
bool StepFusionSuperPogo313B(const FusionPogo313BView&,float waitSeconds,float deltaSeconds,
    float axisX,float jumpX,float boardMaxX,bool controlled,FusionSuperPogo313BStep&);
struct FusionSuperPogo313BMotion
{
    float x=0,y=0,velocityY=0;
    bool landed=false;
};
// Source PositionUpdate observes the current real axis AFTER setting velocity,
// then changes the next vertical velocity; strict < land. It does not integrate.
bool StepFusionSuperPogo313BMotion(float x,float y,float vx,float vy,float deltaSeconds,
    float landY,FusionSuperPogo313BMotion&);

bool FusionLadder313BCanSet(uint8_t status,bool controlled,bool beforeDying,int row,const FusionHeavy313BPlant&);
enum class FusionLadder313BSet:uint8_t { Retry=0,Place=1 };
FusionLadder313BSet FusionLadder313BSetResult(bool trackedAlive,bool trackedHasLadder);
uint8_t FusionLadder313BArmorStage(int health,int maximum);

bool FusionBungee313BStealable(const FusionHeavy313BPlant&,std::span<const FusionHeavy313BPlant> cell);
// Preserves actor-list order and removes source kinds1/1033 ONLY if exactly one
// such actor is present in the initially stealable list (SetTarget35F530).
bool BuildFusionBungee313BCandidates(std::span<const FusionHeavy313BPlant>,std::vector<uint32_t>&);
struct FusionBungee313BView
{
    uint8_t status=23;
    int targetColumn=0,targetRow=0;
    float targetX=0,targetY=0,cordScaleX=1.4705882f,cordScaleY=20.f,cordScaleZ=.5735294f,markerY=0;
    bool setZombie=false,targetLanded=false,markerVisible=true;
    bool operator==(const FusionBungee313BView&) const=default;
};
bool FusionBungee313BViewValid(const FusionBungee313BView&,int columns,int rows);
struct FusionBungee313BMotion
{
    FusionBungee313BView after;
    float rootY=0;
    bool land=false,checkUmbrella=false,putZombie=false,retire=false;
};
// Actual source constructor moveSpeed12/startY14; boss Start overrides startY10.
// Board maxY is supplied from native board geometry, not a hardcoded screen edge.
bool StepFusionBungee313BMotion(const FusionBungee313BView&,float rootY,float deltaSeconds,
    float boardMaxY,FusionBungee313BMotion&);
bool StepFusionBungee313BMarker(float currentY,float targetY,float deltaSeconds,float& after,bool& landed);
// MoveToTarget changes the owner FIRST, then preserves the target child's Y.
// Genuine prefab root.68 * (BungeeTarget.localY .9529412 - Shadow.localY2.11).
float FusionBungee313BInitialMarkerY(int sourceRow);
// Genuine controller ROOT curves. Output includes the source prefab X scale;
// bridge applies the existing Board pixels/source-unit projection exactly once.
bool TryGetFusionHeavy313BRootDelta(uint16_t kind,const char* sourceState,
    FusionZombieClipPosition previous,FusionZombieClipPosition current,bool right,double& output);
}
