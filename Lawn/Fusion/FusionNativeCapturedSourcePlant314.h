#pragma once
#include <windows.h>
#include "FusionAnimation.h"
#include "../Net/NetCommon.h"
#include <map>
#include <string>
#include <vector>
class Board;class LawnApp;class Plant;class Zombie;
namespace Sexy {class Graphics;}
namespace LawnFusion
{
// Board pixels=(a*x+c*y+tx,b*x+d*y+ty). All canonical members own their
// storage. No Image/Plant/model pointer, native handle or world-mask span.
using CapturedMatrix314=std::array<float,6>;
struct CapturedSourceOptions314
{
 bool rootFlipX=false;std::array<int,4> colorMultiplierRGBA{255,255,255,255};float attackSpeed=1;
 std::string spritePathPrefix;
 std::vector<FusionAnimationDrawOptions::SpriteEnabled> spriteEnabledOverrides;
 std::vector<FusionAnimationDrawOptions::SpriteSorting> spriteSortingOverrides;
 std::vector<FusionAnimationDrawOptions::SortingGroup> sortingGroupOverrides;
 std::vector<FusionAnimationDrawOptions::MaskSorting> maskSortingOverrides;
 std::vector<std::string> hiddenNodePaths;
 std::vector<FusionAnimationDrawOptions::NodeActivity> nodeActivityOverrides;
 std::vector<FusionAnimationDrawOptions::NodeRotation> nodeRotations;
 std::vector<FusionAnimationDrawOptions::NodeWorldPosition> nodeWorldPositions;
 std::vector<FusionAnimationDrawOptions::ExtraState> extraStates;
 std::vector<FusionAnimationDrawOptions::SpriteReplacement> spriteReplacements;
 std::optional<FusionAnimationDrawOptions::BaseBlend> baseBlend;
};
struct FrozenSourcePose314
{
 std::string baseState,actionState;float baseSeconds=0,actionSeconds=-1,speed=1;
 CapturedSourceOptions314 options;
};
struct CapturedNativeTrack314
{
 std::string name;int imageResource=-1,renderGroup=0,blendCounter=0,blendTime=0;
 std::array<float,9> blendTransform{}; // XY/skewXY/scaleXY/frame/alpha/shakeOverride.
 std::array<float,2> shake{};std::array<int,4> color{255,255,255,255};
 bool ignoreClip=false,truncate=false,ignoreColor=false,ignoreAdditive=false,inBack=false,forceHidden=false;
};
struct CapturedNativeReanim314
{
 uint16_t type=0;LawnNet::NetZombieAnimState clock;CapturedMatrix314 relative{1,0,0,1,0,0};
 std::array<int,4> color{255,255,255,255},additive{},overlay{};
 bool additiveEnabled=false,overlayEnabled=false;int basePose=-2,filter=0;
 float offsetX=0,offsetY=0;std::vector<CapturedNativeTrack314> tracks;
};
struct CapturedNativeEdge314
{uint16_t parent=0,child=0,track=0;CapturedMatrix314 offset{1,0,0,1,0,0};bool hideWithParent=false,noColor=false;};
struct CapturedSourcePlant314View
{
 uint32_t epoch=0,captureToken=0,ownerToken=0,plantToken=0;
 uint16_t ownerId=0,plantId=0,kind=0,modelKind=0;
 bool nativeArtwork=false,retainedActor=false;
 CapturedMatrix314 localPlacement{1,0,0,1,0,0};
 FrozenSourcePose314 pose;
 std::array<int16_t,8> roles{-1,-1,-1,-1,-1,-1,-1,-1};
 std::vector<CapturedNativeReanim314> reanimations;
 std::vector<CapturedNativeEdge314> edges;
};
bool CapturedMatrix314Valid(const CapturedMatrix314&);
bool CapturedSourcePlant314ViewValid(const CapturedSourcePlant314View&,std::string* error=nullptr);
CapturedSourceOptions314 OwnCapturedSourceOptions314(const FusionAnimationDrawOptions&);
FusionAnimationDrawOptions DrawCapturedSourceOptions314(const CapturedSourceOptions314&);
// Root owns a unique_ptr in NativeContext; the getter does not create a second
// gameplay world. Owner/local IDs are never compared to opaque host tokens.
class NativeCapturedPlants314
{
public:
 explicit NativeCapturedPlants314(Board*);~NativeCapturedPlants314();
 NativeCapturedPlants314(const NativeCapturedPlants314&)=delete;
 NativeCapturedPlants314& operator=(const NativeCapturedPlants314&)=delete;
 bool Prepare(Plant*,Zombie*,uint32_t captureToken,const CapturedMatrix314& parent,CapturedSourcePlant314View&,std::string&);
 bool Commit(Plant*,Zombie*,const CapturedSourcePlant314View&,const CapturedMatrix314& currentParent,std::string&);
 bool PreflightAdopt(const CapturedSourcePlant314View&,std::string&);
 // Commit-only seam: after complete raw+FNS preflight, before old ghost
 // Plant removal. Reversible until Adopt binds the actual owner generation.
 bool StageExistingBeforeRaw(const CapturedSourcePlant314View&,std::string&);
 void RollbackStagedBeforeRaw();
 bool Adopt(Zombie*,const CapturedSourcePlant314View&,std::string&);
 void UpdateOwner(Zombie*,const CapturedMatrix314& parent);
 void Prune();
 bool Capture(Zombie*,CapturedSourcePlant314View&) const;
 bool Draw(Sexy::Graphics* boardGraphics,Zombie*) const;
 bool CollectMasks(Sexy::Graphics*,std::vector<FusionAnimationMask>&) const;
 bool Node(uint32_t ownerLocalUID,uint16_t kind,const char*,float&,float&,bool&,int& order) const;
 void Retire(Zombie*);void Clear();
private:
 struct State;Board* mBoard;std::map<uint32_t,std::unique_ptr<State>> mStates;
 std::map<std::pair<uint16_t,uint32_t>,std::unique_ptr<State>> mPending;
 void RetireUID(uint32_t);
};
NativeCapturedPlants314* GetNativeCapturedPlants314(Board*); // Root NativeContext accessor.
bool GetCapturedSourcePlant314Node(Board*,uint32_t ownerLocalUID,uint16_t kind,const char*,float&,float&,bool&,int&);
}
