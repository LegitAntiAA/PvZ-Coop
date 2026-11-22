#pragma once
#include <cstdint>
#include <cstddef>
namespace LawnFusion
{
// Bounded source phase tables, not a Unity object/controller interpreter.
// Indices are local to the genuine source kind; no source paths on the wire.
enum class FusionSourceGraph313Param:uint8_t {Speed,Attacking,JumpSpeed,Shooting,Wait,Die,Throw,Fall,Landing,Crash,Walk2,Shake,Walk,Ladder,LoseLadder,LadderOver,Back,Jump,JumpOver,Steal,IdleToWalk,LosePaper,Shoot,Shoot2,Stop,Rise,Surprise,Pop,Fly};
enum class FusionSourceGraph313Event:uint8_t {Throw,Crash,DieSound,FallSound,Destroy,Bite,StartLand,Land,FallComplete,Particle,Bomb,ReplaceBomb,Shoot,JumpFail,LandSound,SetLadder,BigJump,Catch,Angry,AngrySound,Shoot2,Status6,ChangeStatus=FallComplete,Pop=22,FootballThrowExit=23,SpiderRaised=24};
struct FusionSourceGraph313Condition {FusionSourceGraph313Param parameter;uint8_t mode;float threshold;};
struct FusionSourceGraph313Edge
{
    uint8_t target;float duration,offset,exitTime;bool hasExitTime,fixedDuration,self;
    const FusionSourceGraph313Condition* conditions;uint8_t conditionCount;
};
struct FusionSourceGraph313Marker {float seconds;FusionSourceGraph313Event event;int32_t sourceInt;};
struct FusionSourceGraph313State
{
    const char* name;const char* nativeTrack;float duration,speed;
    bool loop;uint8_t speedParameter;bool nullMotion; //0 none,1 Speed,2 jumpSpeed.
    const FusionSourceGraph313Edge* edges;uint8_t edgeCount;
    const FusionSourceGraph313Marker* markers;uint8_t markerCount;
    int16_t exitEvent; // -1 none; source-proven Paper_beAngry.OnStateExit only.
};
struct FusionSourceGraph313Layer {uint8_t defaultState;const uint8_t* states;uint8_t stateCount;const FusionSourceGraph313Edge* any;uint8_t anyCount;bool additive;};
struct FusionSourceGraph313
{
    uint16_t kind;const FusionSourceGraph313State* states;uint8_t stateCount;
    const FusionSourceGraph313Layer* layers;uint8_t layerCount;
};
const FusionSourceGraph313* FindFusionSourceGraph313(uint16_t kind);
bool FusionSourceGraph313OwnsState(const FusionSourceGraph313&,uint8_t layer,uint8_t state);
struct FusionSourceGraph313Parameters {float speed=1,jumpSpeed=1;bool attacking=false,shooting=false,wait=false;uint32_t triggers=0;};
constexpr uint32_t FusionSourceGraph313Trigger(FusionSourceGraph313Param p)
{return p>=FusionSourceGraph313Param::Die?uint32_t(1U<<(uint8_t(p)-uint8_t(FusionSourceGraph313Param::Die))):0;}
// Native body/aux Reanimation owns incoming time. Only outgoing/fade are
// canonical presentation values; cursors/parameter triggers stay HOST ONLY.
struct FusionSourceGraph313View
{
    uint8_t state=0,outgoing=0;
    float outgoingSeconds=0,fadeElapsed=0,fadeDuration=0;
    bool fadeNormalized=false; // Normalized: elapsed/duration are outgoing clip seconds.
};
bool FusionSourceGraph313ViewValid(const FusionSourceGraph313&,uint8_t layer,const FusionSourceGraph313View&);
struct FusionSourceGraph313Cursor {uint32_t loop=0;float seconds=0;};
bool FusionSourceGraph313CursorValid(const FusionSourceGraph313State&,FusionSourceGraph313Cursor);
bool AdvanceFusionSourceGraph313(const FusionSourceGraph313State&,FusionSourceGraph313Cursor,float speedParameter,float dt,FusionSourceGraph313Cursor&);
// Selection preserves ordered AnyState then ordered outgoing state edges.
// active fade is uninterruptible (all genuine source InterruptionSource=0).
// Explicit approved native death policy may replace A->B by B->Death.
// nullExitReady is supplied ONLY by a source-proven typed null-motion policy;
// false refuses its timed edge. It never invents a default null clip duration.
bool SelectFusionSourceGraph313Edge(const FusionSourceGraph313&,uint8_t layer,const FusionSourceGraph313View&,
    FusionSourceGraph313Cursor previous,FusionSourceGraph313Cursor current,const FusionSourceGraph313Parameters&,
    bool nativeDeathOverride,bool nullExitReady,const FusionSourceGraph313Edge*&,uint32_t& consumedTriggers);
// Earliest *future* source marker or eligible exit boundary, in real seconds.
// Caller segments actual native clocks, consumes events before gameplay, then
// re-resolves full UID and source parameters after every synchronous callback.
bool NextFusionSourceGraph313Boundary(const FusionSourceGraph313&,uint8_t layer,const FusionSourceGraph313View&,
    FusionSourceGraph313Cursor,const FusionSourceGraph313Parameters&,float& realSeconds);
bool NextFusionSourceGraph313Marker(const FusionSourceGraph313State&,FusionSourceGraph313Cursor,
    FusionSourceGraph313Cursor&,FusionSourceGraph313Event&);
bool NextFusionSourceGraph313Exit(const FusionSourceGraph313&,uint8_t layer,const FusionSourceGraph313View&,
    FusionSourceGraph313Cursor,const FusionSourceGraph313Parameters&,FusionSourceGraph313Cursor&);
float FusionSourceGraph313Rate(const FusionSourceGraph313State&,float speedParameter);
float FusionSourceGraph313Multiplier(const FusionSourceGraph313State&,const FusionSourceGraph313Parameters&);
bool AdvanceFusionSourceGraph313Fade(const FusionSourceGraph313&,const FusionSourceGraph313View&,
    FusionSourceGraph313Cursor outgoingBefore,FusionSourceGraph313Cursor outgoingAfter,float dt,FusionSourceGraph313View&);
}
