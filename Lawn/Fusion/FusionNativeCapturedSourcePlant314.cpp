#include "FusionNativeCapturedSourcePlant314.h"
#include "FusionNativeVisual.h"
#include "FusionNative.h"
#include "FusionNativeZombies.h"
#include "FusionNativeEffects269.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/Attachment.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/TodParticle.h"
#include "../../Sexy.TodLib/Trail.h"
#include "../../SexyAppFramework/Graphics.h"
#include <algorithm>
#include <cmath>
#include <set>
namespace LawnFusion
{
namespace
{
using Matrix=CapturedMatrix314;
Matrix Multiply(const Matrix& a,const Matrix& b)
{return {a[0]*b[0]+a[2]*b[1],a[1]*b[0]+a[3]*b[1],a[0]*b[2]+a[2]*b[3],a[1]*b[2]+a[3]*b[3],a[0]*b[4]+a[2]*b[5]+a[4],a[1]*b[4]+a[3]*b[5]+a[5]};}
bool Inverse(const Matrix& m,Matrix& out)
{if(!CapturedMatrix314Valid(m))return false;const float d=m[0]*m[3]-m[1]*m[2];Matrix r{m[3]/d,-m[1]/d,-m[2]/d,m[0]/d,0,0};r[4]=-r[0]*m[4]-r[2]*m[5];r[5]=-r[1]*m[4]-r[3]*m[5];out=r;return CapturedMatrix314Valid(out);}
Matrix Array(const Sexy::SexyTransform2D& m){return {m.m00,m.m10,m.m01,m.m11,m.m02,m.m12};}
Sexy::SexyTransform2D Native(const Matrix& m)
{Sexy::SexyTransform2D r;r.m00=m[0];r.m10=m[1];r.m01=m[2];r.m11=m[3];r.m02=m[4];r.m12=m[5];return r;}
std::array<int,4> RGBA(const Sexy::Color& c){return {c.mRed,c.mGreen,c.mBlue,c.mAlpha};}
Sexy::Color Color(const std::array<int,4>& c){return Sexy::Color(c[0],c[1],c[2],c[3]);}
std::array<ReanimationID*,8> Roles(Plant* p)
{return {&p->mBodyReanimID,&p->mHeadReanimID,&p->mHeadReanimID2,&p->mHeadReanimID3,&p->mBlinkReanimID,&p->mLightReanimID,&p->mSleepingReanimID,&p->mSpecialReanimID};}
bool Clock(Reanimation* r,LawnNet::NetZombieAnimState& out)
{
 if(!r||r->mDead||!r->mDefinition||!r->mDefinition->mTracks.tracks||r->mDefinition->mTracks.count<=0)return false;
 LawnNet::NetZombieAnimState v;v.mPresent=true;v.mFrameStart=uint16_t(r->mFrameStart);v.mFrameCount=uint16_t(r->mFrameCount);
 v.mAnimTime=r->mAnimTime;v.mAnimRate=r->mAnimRate;v.mLoopType=uint8_t(r->mLoopType);v.mLoopCount=uint8_t((std::clamp)(r->mLoopCount,0,255));
 if(r->mFrameStart<0||r->mFrameStart>65535||r->mFrameCount<=0||r->mFrameCount>65535||
  !LawnNet::NetZombieAnimStateFitsFrames(v,r->mDefinition->mTracks.tracks[0].mTransforms.count))return false;out=v;return true;
}
void Clock(Reanimation* r,const LawnNet::NetZombieAnimState& v)
{r->mFrameStart=v.mFrameStart;r->mFrameCount=v.mFrameCount;r->mAnimTime=r->mLastFrameTime=v.mAnimTime;r->mAnimRate=v.mAnimRate;r->mLoopType=ReanimLoopType(v.mLoopType);r->mLoopCount=v.mLoopCount;}
#include "CapturedNativeImageIds314.inc"
int ImageId(Sexy::Image* image)
{return image?int(Sexy::GetIdByImage(image)):-1;}
bool Definition(uint16_t type,const CapturedNativeReanim314& v,std::string& error)
{
 if(type>=gReanimatorDefCount){error="captured native reanimation type out of range";return false;}
 ReanimatorEnsureDefinitionLoaded(ReanimationType(type),false);auto& d=gReanimatorDefArray[type];
 if(!d.mTracks.tracks||d.mTracks.count!=int(v.tracks.size())||d.mTracks.count<=0||
  !LawnNet::NetZombieAnimStateFitsFrames(v.clock,d.mTracks.tracks[0].mTransforms.count)||v.basePose>=d.mTracks.tracks[0].mTransforms.count){error="captured native definition/clock differs";return false;}
 for(int i=0;i<d.mTracks.count;++i)
 {if(v.tracks[i].name!=d.mTracks.tracks[i].mName||!strnicmp(d.mTracks.tracks[i].mName,"attacher__",10))
  {error="captured native dynamic attacher/track identity unsupported";return false;}
  if(v.tracks[i].imageResource>=0&&(!ImageResource314(v.tracks[i].imageResource)||!Sexy::GetImageById(Sexy::ResourceId(v.tracks[i].imageResource))))
  {error="captured native image resource is unavailable";return false;}}
 return true;
}
bool CaptureGraphData(LawnApp* app,float plantX,float plantY,const std::array<ReanimationID,8>& roles,CapturedSourcePlant314View& v,std::vector<uint32_t>& ids,std::string& error)
{
 std::map<uint32_t,uint16_t> index;std::vector<uint32_t> pending;
 const auto add=[&](ReanimationID id)->int
 {auto* r=app->ReanimationTryToGet(id);if(!r||r->mDead)return -1;const auto uid=uint32_t(app->ReanimationGetID(r));
  if(auto found=index.find(uid);found!=index.end())return found->second;
  if(index.size()>=128)return -2;const auto n=uint16_t(index.size());index.emplace(uid,n);pending.push_back(uid);return n;};
 for(size_t i=0;i<roles.size();++i){const int n=add(roles[i]);if(n==-2)return false;v.roles[i]=int16_t(n);}
 if(v.roles[0]<0){error="captured real main reanimation is missing";return false;}
 auto* main=app->ReanimationTryToGet(roles[0]);Matrix mainWorld=Array(main->mOverlayMatrix);mainWorld[4]+=plantX;mainWorld[5]+=plantY;Matrix inverse;
 if(!Inverse(mainWorld,inverse))return false;
 for(size_t n=0;n<pending.size();++n)
 {
  auto* r=app->ReanimationTryToGet(ReanimationID(pending[n]));if(!r)return false;CapturedNativeReanim314 node;
  node.type=uint16_t(r->mReanimationType);if(!Clock(r,node.clock))return false;
  Matrix world=Array(r->mOverlayMatrix);world[4]+=plantX;world[5]+=plantY;node.relative=Multiply(inverse,world);
  node.color=RGBA(r->mColorOverride);node.additive=RGBA(r->mExtraAdditiveColor);node.overlay=RGBA(r->mExtraOverlayColor);
  node.additiveEnabled=r->mEnableExtraAdditiveDraw;node.overlayEnabled=r->mEnableExtraOverlayDraw;
  node.basePose=r->mFrameBasePose;node.filter=int(r->mFilterEffect);node.offsetX=r->mOffsetX;node.offsetY=r->mOffsetY;
  for(int t=0;t<r->mDefinition->mTracks.count;++t)
  {
   const auto& actual=r->mTrackInstances[t];CapturedNativeTrack314 track;track.name=r->mDefinition->mTracks.tracks[t].mName;
   track.imageResource=ImageId(actual.mImageOverride);if(actual.mImageOverride&&track.imageResource==int(Sexy::ResourceId::RESOURCE_ID_MAX))
   {error="captured image override has no canonical native ResourceId: "+track.name;return false;}
   if(actual.mBlendTransform.mFont||actual.mBlendTransform.mText&&actual.mBlendTransform.mText[0])
   {error="captured native text track is not a plant sprite: "+track.name;return false;}
   const auto& b=actual.mBlendTransform;track.blendTransform={b.mTransX,b.mTransY,b.mSkewX,b.mSkewY,b.mScaleX,b.mScaleY,b.mFrame,b.mAlpha,actual.mShakeOverride};
   track.blendCounter=actual.mBlendCounter;track.blendTime=actual.mBlendTime;track.shake={actual.mShakeX,actual.mShakeY};track.renderGroup=actual.mRenderGroup;
   track.color=RGBA(actual.mTrackColor);track.ignoreClip=actual.mIgnoreClipRect;track.truncate=actual.mTruncateDisappearingFrames;
   track.ignoreColor=actual.mIgnoreColorOverride;track.ignoreAdditive=actual.mIgnoreExtraAdditiveColor;track.inBack=actual.mRenderInBack;track.forceHidden=actual.mForceDontRender;
   node.tracks.push_back(std::move(track));
   auto* effects=app->mEffectSystem;if(!effects||!effects->mAttachmentHolder)return false;
   auto* attachment=effects->mAttachmentHolder->mAttachments.DataArrayTryToGet(actual.mAttachmentID);
   if(!attachment)continue;if(attachment->mNumEffects<0||attachment->mNumEffects>MAX_EFFECTS_PER_ATTACHMENT)return false;
   for(int a=0;a<attachment->mNumEffects;++a)
   {
    const auto& e=attachment->mEffectArray[a];
    if(e.mEffectType==EFFECT_PARTICLE||e.mEffectType==EFFECT_TRAIL)continue; // Existing transient-FX policy; retain actual host lease, no historical replay.
    if(e.mEffectType!=EFFECT_REANIM){error="captured nested native attachment container needs a proven graph edge";return false;}
    const int child=add(ReanimationID(e.mEffectID));if(child<0){if(child==-1)continue;return false;}
    v.edges.push_back({uint16_t(n),uint16_t(child),uint16_t(t),Array(e.mOffset),e.mDontDrawIfParentHidden,e.mDontPropogateColor});
   }
  }
  v.reanimations.push_back(std::move(node));
 }
 ids=std::move(pending);return true;
}
bool CaptureGraph(Plant* p,CapturedSourcePlant314View& v,std::vector<uint32_t>& ids,std::string& error)
{std::array<ReanimationID,8> values;const auto refs=Roles(p);for(size_t i=0;i<8;++i)values[i]=*refs[i];return CaptureGraphData(p->mApp,float(p->mX),float(p->mY),values,v,ids,error);}

void ApplyNode314(Reanimation* r,const CapturedNativeReanim314& n)
{
 Clock(r,n.clock);r->mFrameBasePose=n.basePose;r->mFilterEffect=FilterEffect(n.filter);
   r->mColorOverride=Color(n.color);r->mExtraAdditiveColor=Color(n.additive);r->mExtraOverlayColor=Color(n.overlay);r->mEnableExtraAdditiveDraw=n.additiveEnabled;r->mEnableExtraOverlayDraw=n.overlayEnabled;r->mOffsetX=n.offsetX;r->mOffsetY=n.offsetY;
   for(size_t t=0;t<n.tracks.size();++t)
   {const auto& x=n.tracks[t];auto& y=r->mTrackInstances[t];y.mImageOverride=x.imageResource<0?nullptr:Sexy::GetImageById(Sexy::ResourceId(x.imageResource));y.mRenderGroup=x.renderGroup;y.mBlendCounter=x.blendCounter;y.mBlendTime=x.blendTime;
    auto& b=y.mBlendTransform;b.mTransX=x.blendTransform[0];b.mTransY=x.blendTransform[1];b.mSkewX=x.blendTransform[2];b.mSkewY=x.blendTransform[3];b.mScaleX=x.blendTransform[4];b.mScaleY=x.blendTransform[5];b.mFrame=x.blendTransform[6];b.mAlpha=x.blendTransform[7];y.mShakeOverride=x.blendTransform[8];y.mShakeX=x.shake[0];y.mShakeY=x.shake[1];
    y.mTrackColor=Color(x.color);y.mIgnoreClipRect=x.ignoreClip;y.mTruncateDisappearingFrames=x.truncate;y.mIgnoreColorOverride=x.ignoreColor;y.mIgnoreExtraAdditiveColor=x.ignoreAdditive;y.mRenderInBack=x.inBack;y.mForceDontRender=x.forceHidden;}
}
Plant* ActualPlant(Board* b,const CapturedSourcePlant314View& v)
{Plant* p=nullptr;while(b->IteratePlants(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&p->mNetId==v.plantId&&d&&d->instanceToken==v.plantToken)return p;}return nullptr;}
}
CapturedSourceOptions314 OwnCapturedSourceOptions314(const FusionAnimationDrawOptions& d)
{
 CapturedSourceOptions314 x;x.rootFlipX=d.rootFlipX;x.colorMultiplierRGBA=d.colorMultiplierRGBA;x.attackSpeed=d.attackSpeed;x.spritePathPrefix=d.spritePathPrefix;
 x.spriteEnabledOverrides=d.spriteEnabledOverrides;x.spriteSortingOverrides=d.spriteSortingOverrides;x.sortingGroupOverrides=d.sortingGroupOverrides;x.maskSortingOverrides=d.maskSortingOverrides;
 x.hiddenNodePaths=d.hiddenNodePaths;x.nodeActivityOverrides=d.nodeActivityOverrides;x.nodeRotations=d.nodeRotations;x.nodeWorldPositions=d.nodeWorldPositions;
 x.extraStates=d.extraStates;x.spriteReplacements=d.spriteReplacements;x.baseBlend=d.baseBlend;return x;
}
FusionAnimationDrawOptions DrawCapturedSourceOptions314(const CapturedSourceOptions314& x)
{
 FusionAnimationDrawOptions d;d.rootFlipX=x.rootFlipX;d.colorMultiplierRGBA=x.colorMultiplierRGBA;d.attackSpeed=x.attackSpeed;d.spritePathPrefix=x.spritePathPrefix;
 d.spriteEnabledOverrides=x.spriteEnabledOverrides;d.spriteSortingOverrides=x.spriteSortingOverrides;d.sortingGroupOverrides=x.sortingGroupOverrides;d.maskSortingOverrides=x.maskSortingOverrides;
 d.hiddenNodePaths=x.hiddenNodePaths;d.nodeActivityOverrides=x.nodeActivityOverrides;d.nodeRotations=x.nodeRotations;d.nodeWorldPositions=x.nodeWorldPositions;
 d.extraStates=x.extraStates;d.spriteReplacements=x.spriteReplacements;d.baseBlend=x.baseBlend;return d;
}
struct NativeCapturedPlants314::State
{
 CapturedSourcePlant314View view;uint32_t ownerUID=0,originalPlantUID=0,particleID=0;int tick=-1;bool pinned=false;
 Matrix parent{1,0,0,1,0,0};std::vector<uint32_t> ids;std::vector<bool> originalAttachmentFlags;
};
NativeCapturedPlants314::NativeCapturedPlants314(Board* b):mBoard(b){}
NativeCapturedPlants314::~NativeCapturedPlants314(){Clear();}
bool NativeCapturedPlants314::Prepare(Plant* p,Zombie* owner,uint32_t token,const Matrix& parent,CapturedSourcePlant314View& out,std::string& error)
{
 error.clear();auto* c=GetNativeContext(mBoard);const auto* d=GetNativePlantData(p);const auto* zd=GetNativeSourceZombieData(owner);Matrix inv,world;
 if(!c||!p||p->mDead||p->mBoard!=mBoard||!owner||owner->mDead||owner->mBoard!=mBoard||!zd||!d||!token||!Inverse(parent,inv))return false;
 if(const auto derived=GetNativeEmbedded269DerivedPlantModelKey(p))
 {error="retained runtime-derived source model needs its separate pose/clock binding: modelKey="+std::to_string(derived);return false;}
 if(!EnsureNativePlantVisuals(mBoard,d->kind,&error)||!NativePlantVisualStateSupported(p,&error))return false;
 CapturedSourcePlant314View v;v.epoch=c->epoch;v.captureToken=token;v.ownerId=owner->mNetId;v.ownerToken=zd->instanceToken;v.plantId=p->mNetId;v.plantToken=d->instanceToken;v.kind=d->kind;
 if(!c->visuals||!c->visuals->CapturePlant314(p,v.pose,v.modelKind,v.nativeArtwork,world,error))return false;
 std::vector<uint32_t> ids;if(!CaptureGraph(p,v,ids,error))return false;
 if(v.nativeArtwork){auto* body=p->mApp->ReanimationTryToGet(p->mBodyReanimID);world=Array(body->mOverlayMatrix);world[4]+=p->mX;world[5]+=p->mY;}
 else
 {Matrix modelInverse;if(!Inverse(world,modelInverse))return false;auto* main=p->mApp->ReanimationTryToGet(p->mBodyReanimID);
  Matrix nativeWorld=Array(main->mOverlayMatrix);nativeWorld[4]+=p->mX;nativeWorld[5]+=p->mY;const auto basis=Multiply(modelInverse,nativeWorld);
  for(auto& node:v.reanimations)node.relative=Multiply(basis,node.relative);}
 v.localPlacement=Multiply(inv,world);if(!CapturedSourcePlant314ViewValid(v,&error))return false;out=std::move(v);return true;
}
bool NativeCapturedPlants314::Commit(Plant* p,Zombie* owner,const CapturedSourcePlant314View& supplied,const Matrix& currentParent,std::string& error)
{
 if(!p||!owner||owner->mBoard!=mBoard||p->mBoard!=mBoard||mBoard->mApp->NetIsClient()||mStates.contains(mBoard->mZombies.DataArrayGetID(owner)))return false;
 auto* c=GetNativeContext(mBoard);if(!c||!c->visuals||!CapturedSourcePlant314ViewValid(supplied,&error))return false;
 const auto* d=GetNativePlantData(p);const auto* z=GetNativeSourceZombieData(owner);
 if(!d||!z||p->mNetId!=supplied.plantId||d->instanceToken!=supplied.plantToken||owner->mNetId!=supplied.ownerId||z->instanceToken!=supplied.ownerToken)return false;
 CapturedSourcePlant314View current;if(!Prepare(p,owner,supplied.captureToken,currentParent,current,error))return false;
 auto s=std::make_unique<State>();s->view=std::move(current);s->parent=currentParent;s->ownerUID=mBoard->mZombies.DataArrayGetID(owner);s->originalPlantUID=mBoard->mPlants.DataArrayGetID(p);
 auto graph=supplied;graph.reanimations.clear();graph.edges.clear();if(!CaptureGraph(p,graph,s->ids,error))return false;
 const auto roles=Roles(p);std::array<ReanimationID,8> old{};for(size_t i=0;i<8;++i){old[i]=*roles[i];*roles[i]=REANIMATIONID_NULL;}
 s->particleID=uint32_t(p->mParticleID);p->mParticleID=PARTICLESYSTEMID_NULL;
 // Invoke the existing real source virtual policy. Empty Rolling.Die5 must
 // preserve the ACTUAL actor and every lease, not fall through to baseDie.
 auto* app=mBoard->mApp;const auto ownerUID=s->ownerUID,plantUID=s->originalPlantUID;p->Die(5);
 if(app->mBoard!=mBoard){error="source capture changed the current Board during Die5";return false;}
 p=mBoard->mPlants.DataArrayTryToGet(plantUID);owner=mBoard->mZombies.DataArrayTryToGet(ownerUID);
 c=GetNativeContext(mBoard);
 if(!owner||owner->mDead||!c||!c->visuals)
 {if(p&&!p->mDead){const auto refs=Roles(p);for(size_t i=0;i<8;++i)*refs[i]=old[i];p->mParticleID=ParticleSystemID(s->particleID);}
  else {for(auto id:s->ids)app->RemoveReanimation(ReanimationID(id));if(s->particleID)app->RemoveParticle(ParticleSystemID(s->particleID));}
  error="source capture owner retired recursively during Die5";return false;}
 if(p&&!p->mDead)
 {for(size_t i=0;i<8;++i)*roles[i]=old[i];p->mParticleID=ParticleSystemID(s->particleID);s->ids.clear();s->particleID=0;s->view.retainedActor=true;}
 else
 {
  for(auto id:s->ids)if(auto* r=mBoard->mApp->ReanimationTryToGet(ReanimationID(id))){r->mIsAttachment=true;r->mAnimRate=0;}
  if(auto* main=mBoard->mApp->ReanimationTryToGet(old[0]))main->mAnimRate=0;
  if(!s->view.nativeArtwork){c->visuals->PinCapturedPlant314(s->view.modelKind,true);s->pinned=true;}
  if(auto* effects=GetNativeSourceEffects269(mBoard,false))effects->CapturePlant314(mBoard,s->originalPlantUID,s->ownerUID,s->view.captureToken,s->view.kind);
 }
 mStates.emplace(s->ownerUID,std::move(s));return true;
}
bool NativeCapturedPlants314::PreflightAdopt(const CapturedSourcePlant314View& v,std::string& error)
{
 auto* c=GetNativeContext(mBoard);if(!c||c->epoch!=v.epoch||!CapturedSourcePlant314ViewValid(v,&error)||!EnsureNativePlantVisuals(mBoard,v.modelKind,&error))return false;
 for(const auto& n:v.reanimations)if(!Definition(n.type,n,error))return false;
 if(v.retainedActor)return true; // Root raw cross-reference promises the real live Plant; checked after commit in Adopt.
 auto* effects=mBoard->mApp->mEffectSystem;if(!effects||!effects->mReanimationHolder||!effects->mAttachmentHolder)return false;
 std::set<std::pair<uint16_t,uint16_t>> containers;for(const auto& e:v.edges)containers.emplace(e.parent,e.track);
 const auto& r=effects->mReanimationHolder->mReanimations;const auto& a=effects->mAttachmentHolder->mAttachments;
 if(!r.mBlock||r.mSize+v.reanimations.size()>r.mMaxSize||!a.mBlock||a.mSize+containers.size()>a.mMaxSize){error="captured actual R/A graph capacity unavailable (no deletion credit)";return false;}return true;
}
bool NativeCapturedPlants314::Adopt(Zombie* owner,const CapturedSourcePlant314View& v,std::string& error)
{
 const auto* z=GetNativeSourceZombieData(owner);if(!owner||owner->mBoard!=mBoard||!z||z->instanceToken!=v.ownerToken||owner->mNetId!=v.ownerId||!PreflightAdopt(v,error))return false;
 const uint32_t uid=mBoard->mZombies.DataArrayGetID(owner);
 if(auto found=mStates.find(uid);found!=mStates.end()&&found->second->view.captureToken==v.captureToken)
 {
  auto& state=*found->second;
  if(state.view.retainedActor&&v.retainedActor){if(!ActualPlant(mBoard,v))return false;state.view=v;return true;}
  if(state.ids.size()!=v.reanimations.size()||state.view.edges.size()!=v.edges.size())return false;
  for(size_t i=0;i<v.edges.size();++i)if(state.view.edges[i].parent!=v.edges[i].parent||state.view.edges[i].child!=v.edges[i].child||state.view.edges[i].track!=v.edges[i].track)return false;
  for(size_t i=0;i<state.ids.size();++i){auto* r=mBoard->mApp->ReanimationTryToGet(ReanimationID(state.ids[i]));if(!r||r->mReanimationType!=ReanimationType(v.reanimations[i].type))return false;ApplyNode314(r,v.reanimations[i]);}
  state.view=v;return true;
 }
 if(v.retainedActor&&!ActualPlant(mBoard,v)){error="retained live source Plant absent after raw adoption";return false;}
 auto next=std::make_unique<State>();next->view=v;next->ownerUID=uid;
 const auto pending=mPending.find({v.ownerId,v.ownerToken});
 if(pending!=mPending.end()&&pending->second->view.captureToken==v.captureToken)
 {next=std::move(pending->second);mPending.erase(pending);next->view=v;next->ownerUID=uid;}
 if(auto* original=ActualPlant(mBoard,v))next->originalPlantUID=mBoard->mPlants.DataArrayGetID(original);
 if(!v.retainedActor)
 {
  const bool reused=!next->ids.empty();
  if(!reused)for(const auto& n:v.reanimations){auto* r=mBoard->mApp->AddReanimation(0,0,0,ReanimationType(n.type));if(!r){error="captured native graph allocator refused after preflight";for(auto id:next->ids)mBoard->mApp->RemoveReanimation(ReanimationID(id));return false;}r->mIsAttachment=true;next->ids.push_back(uint32_t(mBoard->mApp->ReanimationGetID(r)));}
  for(size_t i=0;i<v.reanimations.size();++i)
  {
   ApplyNode314(mBoard->mApp->ReanimationTryToGet(ReanimationID(next->ids[i])),v.reanimations[i]);
  }
  if(!reused)for(const auto& e:v.edges)
  {auto* parent=mBoard->mApp->ReanimationTryToGet(ReanimationID(next->ids[e.parent]));auto& attach=parent->mTrackInstances[e.track].mAttachmentID;
   auto* effect=CreateEffectAttachment(attach,EFFECT_REANIM,next->ids[e.child],0,0);if(!effect){error="captured attachment allocator refused";for(auto id:next->ids)mBoard->mApp->RemoveReanimation(ReanimationID(id));return false;}
   effect->mOffset=Native(e.offset);effect->mDontDrawIfParentHidden=e.hideWithParent;effect->mDontPropogateColor=e.noColor;}
  if(!v.nativeArtwork){GetNativeContext(mBoard)->visuals->PinCapturedPlant314(v.modelKind,true);next->pinned=true;}
  if(auto* fx=GetNativeSourceEffects269(mBoard,false))fx->CapturePlant314(mBoard,next->originalPlantUID,uid,v.captureToken,v.kind);
 }
 Retire(owner);mStates.emplace(uid,std::move(next));return true;
}
void NativeCapturedPlants314::UpdateOwner(Zombie* owner,const Matrix& parent)
{
 if(!owner||owner->mBoard!=mBoard||!CapturedMatrix314Valid(parent))return;const auto uid=mBoard->mZombies.DataArrayGetID(owner);const auto found=mStates.find(uid);if(found==mStates.end())return;
 auto& s=*found->second;const auto* data=GetNativeSourceZombieData(owner);auto* c=GetNativeContext(mBoard);
 if(!c||c->epoch!=s.view.epoch||!data||data->instanceToken!=s.view.ownerToken||owner->mDead){Retire(owner);return;}s.parent=parent;
 if(s.view.retainedActor)return;const Matrix base=Multiply(parent,s.view.localPlacement);std::set<size_t> children;
 for(const auto& e:s.view.edges)children.insert(e.child);
 for(size_t i=0;i<s.ids.size();++i)if(!children.contains(i))if(auto* r=mBoard->mApp->ReanimationTryToGet(ReanimationID(s.ids[i])))r->mOverlayMatrix=Native(Multiply(base,s.view.reanimations[i].relative));
 if(mBoard->mPaused||s.tick==mBoard->mMainCounter)return;s.tick=mBoard->mMainCounter;
 // Every currently ported source prefab has one actual Unity Animator.
 // Native body/head/blink R objects project that SAME removed component:
 // freeze their clocks/blends/shake; only real attached PS/Trails keep ticking.
 for(auto id:s.ids)if(auto* r=mBoard->mApp->ReanimationTryToGet(ReanimationID(id)))
  for(int track=0;track<r->mDefinition->mTracks.count;++track)
  {
   auto* effects=mBoard->mApp->mEffectSystem;auto* a=effects->mAttachmentHolder->mAttachments.DataArrayTryToGet(r->mTrackInstances[track].mAttachmentID);
   if(!a)continue;Sexy::SexyTransform2D matrix;r->GetAttachmentOverlayMatrix(track,matrix);a->SetMatrix(matrix);
   for(int e=0;e<a->mNumEffects;++e)
   {const auto& actual=a->mEffectArray[e];
    if(actual.mEffectType==EFFECT_PARTICLE){auto* ps=mBoard->mApp->ParticleTryToGet(ParticleSystemID(actual.mEffectID));if(ps&&ps->mIsAttachment)ps->Update();}
    else if(actual.mEffectType==EFFECT_TRAIL){auto* trail=effects->mTrailHolder->mTrails.DataArrayTryToGet(actual.mEffectID);if(trail&&trail->mIsAttachment)trail->Update();}
   }
  }
 if(s.particleID)if(auto* ps=mBoard->mApp->ParticleTryToGet(ParticleSystemID(s.particleID))){ps->SystemMove(base[4],base[5]);if(ps->mIsAttachment)ps->Update();}
}
bool NativeCapturedPlants314::Capture(Zombie* owner,CapturedSourcePlant314View& out) const
{
 if(!owner||owner->mBoard!=mBoard)return false;const auto at=mStates.find(mBoard->mZombies.DataArrayGetID(owner));if(at==mStates.end())return false;
 auto next=at->second->view;if(next.retainedActor){auto* p=ActualPlant(mBoard,next);if(!p)return false;std::vector<uint32_t> ids;next.reanimations.clear();next.edges.clear();std::string error;if(!CaptureGraph(p,next,ids,error))return false;}
 else
 {
  std::array<ReanimationID,8> roles;roles.fill(REANIMATIONID_NULL);for(size_t i=0;i<8;++i)if(next.roles[i]>=0)roles[i]=ReanimationID(at->second->ids[next.roles[i]]);
  next.reanimations.clear();next.edges.clear();std::vector<uint32_t> actual;std::string error;
  if(!CaptureGraphData(mBoard->mApp,0,0,roles,next,actual,error))return false;
  if(!next.nativeArtwork)
  {Matrix modelInverse;if(!Inverse(Multiply(at->second->parent,next.localPlacement),modelInverse))return false;
   auto* main=mBoard->mApp->ReanimationTryToGet(roles[0]);const auto basis=Multiply(modelInverse,Array(main->mOverlayMatrix));
   for(auto& node:next.reanimations)node.relative=Multiply(basis,node.relative);}
 }
 if(!CapturedSourcePlant314ViewValid(next))return false;out=std::move(next);return true;
}
bool NativeCapturedPlants314::Draw(Sexy::Graphics* g,Zombie* owner) const
{
 if(!g||!owner||owner->mBoard!=mBoard)return false;const auto at=mStates.find(mBoard->mZombies.DataArrayGetID(owner));if(at==mStates.end())return false;
 const auto& s=*at->second;if(s.view.retainedActor){auto* p=ActualPlant(mBoard,s.view);if(!p)return false;Sexy::Graphics local(*g);local.mTransX+=p->mX;local.mTransY+=p->mY;p->Draw(&local);return true;}
 if(!s.view.nativeArtwork)return GetNativeContext(mBoard)->visuals->DrawCapturedPlant314(g,s.view.modelKind,s.view.pose,Multiply(s.parent,s.view.localPlacement));
 std::set<size_t> children;for(const auto& edge:s.view.edges)children.insert(edge.child);
 for(size_t i=0;i<s.ids.size();++i)if(!children.contains(i))if(auto* r=mBoard->mApp->ReanimationTryToGet(ReanimationID(s.ids[i])))r->Draw(g);return true;
}
bool NativeCapturedPlants314::CollectMasks(Sexy::Graphics* g,std::vector<FusionAnimationMask>& out) const
{const auto* c=GetNativeContext(mBoard);if(!c||!c->visuals)return mStates.empty();for(const auto& item:mStates){const auto& s=*item.second;if(!s.view.retainedActor&&!s.view.nativeArtwork&&!c->visuals->CollectCapturedPlantMasks314(g,s.view.modelKind,s.view.pose,Multiply(s.parent,s.view.localPlacement),out))return false;}return true;}
bool NativeCapturedPlants314::Node(uint32_t ownerUID,uint16_t kind,const char* path,float& x,float& y,bool& active,int& order) const
{const auto at=mStates.find(ownerUID);if(at==mStates.end()||at->second->view.kind!=kind)return false;const auto& s=*at->second;
 auto* owner=mBoard->mZombies.DataArrayTryToGet(ownerUID);if(!owner||owner->mDead)return false;order=owner->mRenderOrder+26;
 if(s.view.retainedActor){auto* p=ActualPlant(mBoard,s.view);return p&&GetNativeSourcePlantNodePoint(p,path,x,y)&&GetNativeSourcePlantNodeActivity(p,path,active);}
 return !s.view.nativeArtwork&&GetNativeContext(mBoard)->visuals->CapturedPlantNode314(s.view.modelKind,s.view.pose,Multiply(s.parent,s.view.localPlacement),path,x,y,active);}
void NativeCapturedPlants314::Retire(Zombie* owner)
{if(owner&&owner->mBoard==mBoard)RetireUID(mBoard->mZombies.DataArrayGetID(owner));}
void NativeCapturedPlants314::Clear()
{RollbackStagedBeforeRaw();while(!mStates.empty())RetireUID(mStates.begin()->first);}
bool GetCapturedSourcePlant314Node(Board* b,uint32_t uid,uint16_t kind,const char* path,float& x,float& y,bool& active,int& order)
{auto* captures=GetNativeCapturedPlants314(b);return captures&&captures->Node(uid,kind,path,x,y,active,order);}
}

namespace LawnFusion
{
bool NativeCapturedPlants314::StageExistingBeforeRaw(const CapturedSourcePlant314View& v,std::string& error)
{
 if(!CapturedSourcePlant314ViewValid(v,&error))return false;if(v.retainedActor)return true;
 const auto key=std::make_pair(v.ownerId,v.ownerToken);if(mPending.contains(key))return mPending.at(key)->view.captureToken==v.captureToken;
 for(const auto& entry:mStates)if(entry.second->view.ownerId==v.ownerId&&entry.second->view.ownerToken==v.ownerToken&&entry.second->view.captureToken==v.captureToken)return true;
 auto* p=ActualPlant(mBoard,v);if(!p)return true;auto next=std::make_unique<State>();next->view=v;next->originalPlantUID=mBoard->mPlants.DataArrayGetID(p);
 auto actual=v;actual.reanimations.clear();actual.edges.clear();if(!CaptureGraph(p,actual,next->ids,error))return false;
 bool same=actual.roles==v.roles&&actual.reanimations.size()==v.reanimations.size()&&actual.edges.size()==v.edges.size();
 if(same)for(size_t i=0;i<v.reanimations.size();++i)if(actual.reanimations[i].type!=v.reanimations[i].type)same=false;
 if(same)for(size_t i=0;i<v.edges.size();++i)if(actual.edges[i].parent!=v.edges[i].parent||actual.edges[i].child!=v.edges[i].child||actual.edges[i].track!=v.edges[i].track)same=false;
 if(!same)return true; // Genuine current canonical graph will be rebuilt; historical transient FX are not replayed.
 const auto refs=Roles(p);for(auto* id:refs)*id=REANIMATIONID_NULL;next->particleID=uint32_t(p->mParticleID);p->mParticleID=PARTICLESYSTEMID_NULL;
 for(auto id:next->ids)if(auto* r=mBoard->mApp->ReanimationTryToGet(ReanimationID(id))){next->originalAttachmentFlags.push_back(r->mIsAttachment);r->mIsAttachment=true;}
 mPending.emplace(key,std::move(next));return true;
}
void NativeCapturedPlants314::RollbackStagedBeforeRaw()
{
 for(auto& entry:mPending)
 {
  auto& s=*entry.second;auto* p=mBoard->mPlants.DataArrayTryToGet(s.originalPlantUID);const auto* d=GetNativePlantData(p);
  if(p&&!p->mDead&&d&&d->instanceToken==s.view.plantToken)
  {const auto refs=Roles(p);for(size_t i=0;i<8;++i)if(s.view.roles[i]>=0&&*refs[i]==REANIMATIONID_NULL)*refs[i]=ReanimationID(s.ids[s.view.roles[i]]);
   if(p->mParticleID==PARTICLESYSTEMID_NULL)p->mParticleID=ParticleSystemID(s.particleID);
   for(size_t i=0;i<s.ids.size();++i)if(auto* r=mBoard->mApp->ReanimationTryToGet(ReanimationID(s.ids[i]));r&&i<s.originalAttachmentFlags.size())r->mIsAttachment=s.originalAttachmentFlags[i];
  }
  else {for(auto id:s.ids)mBoard->mApp->RemoveReanimation(ReanimationID(id));if(s.particleID)mBoard->mApp->RemoveParticle(ParticleSystemID(s.particleID));}
 }
 mPending.clear();
}
void NativeCapturedPlants314::RetireUID(uint32_t uid)
{
 const auto found=mStates.find(uid);if(found==mStates.end())return;auto& s=*found->second;
 if(auto* fx=GetNativeSourceEffects269(mBoard,false))fx->ReleaseCapturedPlant314(uid,s.view.captureToken);
 for(auto id:s.ids)mBoard->mApp->RemoveReanimation(ReanimationID(id));if(s.particleID)mBoard->mApp->RemoveParticle(ParticleSystemID(s.particleID));
 if(s.pinned)if(auto* c=GetNativeContext(mBoard);c&&c->visuals)c->visuals->PinCapturedPlant314(s.view.modelKind,false);mStates.erase(found);
}
void NativeCapturedPlants314::Prune()
{
 const auto* c=GetNativeContext(mBoard);std::vector<uint32_t> retired;
 for(const auto& entry:mStates){auto* z=mBoard->mZombies.DataArrayTryToGet(entry.first);const auto* d=GetNativeSourceZombieData(z);
  if(!c||c->epoch!=entry.second->view.epoch||!z||z->mDead||!d||d->instanceToken!=entry.second->view.ownerToken)retired.push_back(entry.first);}
 for(auto uid:retired)RetireUID(uid);
}
}
