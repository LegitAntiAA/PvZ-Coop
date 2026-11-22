#include "FusionNativeCapturedSourcePlant314.h"
#include "../../Resources.h"
#include "../../Sexy.TodLib/FilterEffectEnum.h"
#include "../../Sexy.TodLib/AttachmentLimits.h"
#include <algorithm>
#include <cmath>
namespace LawnFusion
{
namespace
{
#include "CapturedNativeImageIds314.inc"
bool RGBAValid(const std::array<int,4>& c){return std::all_of(c.begin(),c.end(),[](int x){return x>=0&&x<=255;});}
}
bool CapturedMatrix314Valid(const CapturedMatrix314& m)
{return std::all_of(m.begin(),m.end(),[](float f){return std::isfinite(f)&&std::abs(f)<1e7f;});}
bool CapturedSourcePlant314ViewValid(const CapturedSourcePlant314View& v,std::string* error)
{
 const auto reject=[&](const char* s){if(error)*error=s;return false;};if(error)error->clear();
 if(!v.epoch||!v.captureToken||!v.ownerId||!v.ownerToken||!v.plantId||!v.plantToken||!CapturedMatrix314Valid(v.localPlacement)||v.reanimations.empty()||v.reanimations.size()>128||v.edges.size()>2048)return reject("captured identity/matrix/graph bounds");
 const auto& p=v.pose;const auto text=[](const std::string& s){return s.size()<=512&&s.find('\0')==std::string::npos;};
 if(!text(p.baseState)||!text(p.actionState)||!std::isfinite(p.baseSeconds)||p.baseSeconds<0||!std::isfinite(p.actionSeconds)||p.actionSeconds< -1||!std::isfinite(p.speed)||p.speed<=0||p.speed>100||!RGBAValid(p.options.colorMultiplierRGBA)||!std::isfinite(p.options.attackSpeed)||p.options.attackSpeed<=0)return reject("captured source pose");
 const auto paths=[&](const auto& xs){if(xs.size()>2048)return false;return std::all_of(xs.begin(),xs.end(),[&](const auto& x){return text(x.nodePath);});};
 if(!text(p.options.spritePathPrefix)||!paths(p.options.spriteEnabledOverrides)||!paths(p.options.spriteSortingOverrides)||!paths(p.options.sortingGroupOverrides)||!paths(p.options.maskSortingOverrides)||!paths(p.options.nodeActivityOverrides)||!paths(p.options.nodeRotations)||!paths(p.options.nodeWorldPositions)||!paths(p.options.spriteReplacements)||p.options.hiddenNodePaths.size()>2048||p.options.extraStates.size()>128)return reject("captured option bounds");
 for(const auto& s:p.options.hiddenNodePaths)if(!text(s))return reject("captured hidden path");
 const auto blend=[&](const auto& s){return text(s.outgoingState)&&std::isfinite(s.outgoingElapsedSeconds)&&s.outgoingElapsedSeconds>=0&&std::isfinite(s.incomingWeight)&&s.incomingWeight>=0&&s.incomingWeight<=1;};
 for(const auto& s:p.options.extraStates)if(!text(s.stateName)||!std::isfinite(s.elapsedSeconds)||s.elapsedSeconds<0||!std::isfinite(s.weight)||s.weight<0||s.weight>1||(s.baseBlend&&!blend(*s.baseBlend)))return reject("captured extra state");
 for(const auto& s:p.options.nodeRotations)if(!std::isfinite(s.radiansZ))return reject("captured rotation");
 for(const auto& s:p.options.nodeWorldPositions)if(!std::isfinite(s.x)||!std::isfinite(s.y))return reject("captured node placement");
 for(const auto& s:p.options.spriteReplacements)if(!text(s.spriteGuid)||!std::isfinite(s.scaleX)||!std::isfinite(s.scaleY)||!std::isfinite(s.scaleZ))return reject("captured sprite replacement");
 if(p.options.baseBlend){const auto& s=*p.options.baseBlend;if(!text(s.outgoingState)||!std::isfinite(s.outgoingElapsedSeconds)||s.outgoingElapsedSeconds<0||!std::isfinite(s.incomingWeight)||s.incomingWeight<0||s.incomingWeight>1)return reject("captured base blend");}
 for(int r:v.roles)if(r< -1||r>=int(v.reanimations.size()))return reject("captured role index");if(v.roles[0]<0)return reject("captured main role absent");
 for(const auto& n:v.reanimations)
 {if(!CapturedMatrix314Valid(n.relative)||!LawnNet::NetZombieAnimStateFitsFrames(n.clock,65535)||!n.clock.mPresent||n.clock.mDead||!std::isfinite(n.clock.mAnimTime)||n.clock.mAnimTime<0||n.clock.mAnimTime>1||!std::isfinite(n.clock.mAnimRate)||std::abs(n.clock.mAnimRate)>10000||n.clock.mLoopType>5||n.basePose< -2||n.filter<FILTER_EFFECT_NONE||n.filter>=NUM_FILTER_EFFECTS||!RGBAValid(n.color)||!RGBAValid(n.additive)||!RGBAValid(n.overlay)||n.tracks.empty()||n.tracks.size()>2048||!std::isfinite(n.offsetX)||!std::isfinite(n.offsetY))return reject("captured native node");
  for(const auto& t:n.tracks)if(!text(t.name)||t.imageResource< -1||t.imageResource>=int(Sexy::ResourceId::RESOURCE_ID_MAX)||(t.imageResource>=0&&!ImageResource314(t.imageResource))||!RGBAValid(t.color)||t.blendCounter<0||t.blendTime<0||t.blendCounter>t.blendTime||!std::all_of(t.blendTransform.begin(),t.blendTransform.end(),[](float f){return std::isfinite(f);})||!std::isfinite(t.shake[0])||!std::isfinite(t.shake[1]))return reject("captured native appearance");}
 std::vector<int> parent(v.reanimations.size(),-1);std::map<std::pair<int,int>,unsigned> attachmentCounts;
 for(const auto& e:v.edges)
 {if(e.parent>=v.reanimations.size()||e.child>=v.reanimations.size()||e.parent==e.child||e.track>=v.reanimations[e.parent].tracks.size()||!CapturedMatrix314Valid(e.offset)||parent[e.child]!=-1||++attachmentCounts[{e.parent,e.track}]>MAX_EFFECTS_PER_ATTACHMENT)return reject("captured attachment edge");parent[e.child]=e.parent;}
 if(parent[v.roles[0]]!=-1)return reject("captured main has a parent");
 for(size_t n=0;n<parent.size();++n){int at=int(n);for(size_t step=0;at>=0;++step){if(step>=parent.size())return reject("captured attachment cycle");at=parent[at];}}
 return true;
}
}
