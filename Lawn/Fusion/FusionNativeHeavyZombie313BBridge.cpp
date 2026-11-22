#include "FusionNativeDryGround353.h"
#include "FusionNativeHeavy313BInternal.h"
#include "FusionNextZombie343Rules.h"
#include "FusionNativeNextZombie343.h"
#include "FusionNativeDrown313Bridge.h"
#include "FusionNativeDrownWeapon313.h"
#include "FusionNative.h"
#include "FusionNativeAdventure273.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeSourceLifecycle313.h"
#include "FusionNativeProjection313.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeVisual.h"
#include "FusionNativePlantDamage.h"
#include "FusionNativePlacement.h"
#include "FusionNativeEffects269.h"
#include "FusionNativeCold.h"
#include "FusionNativeUmbrella.h"
#include "FusionNativeUmbrellaBatch.h"
#include "FusionNativeEquipment268.h"
#include "FusionNativeRemainingPlants269.h"
#include "FusionNativeGlove.h"
#include "FusionNativeCapturedSourcePlant314.h"
#include "FusionNativeRollingActors269.h"
#include "FusionNativeHostileProjectile.h"
#include "FusionNativeSourceDamage315.h"
#include "FusionNativeWater268.h"
#include "FusionSourceZombieTypes313.h"
#include "FusionEquipmentRules.h"
#include "FusionBasePlantRules.h"
#include "FusionDefenderRules.h"
#include "SpecialZombie269Rules.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Coin.h"
#include "../Net/NetCommon.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/Attachment.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace LawnFusion
{
namespace Heavy313B
{
uint16_t Kind(const Zombie* z){const auto* d=GetNativeSourceZombieData(z);return d?d->kind:0xffff;}
bool World(const Board* b)
{return b&&b->mApp&&b->mApp->mBoard==b&&IsNativeSourceWorld313(b);}
// Source45 chooses the selected plant's actual row after its dry birth.
// Keep that existing full-generation actor accessible over water; this does
// not authorize a new Heavy constructor on a water row.
bool RowAllowed358(const Zombie* z,bool creating=false)
{
 if(!z||!World(z->mBoard)||z->mRow<0||z->mRow>=GetNativeFusionRowCount(z->mBoard))return false;
 if(NativeSourceDryRow353(z->mBoard,z->mRow))return true;
 if(creating||Kind(z)!=45)return false;
 const auto* c=GetNativeContext(z->mBoard);const auto uid=z->mBoard->mZombies.DataArrayGetID(const_cast<Zombie*>(z));
 if(!c||!uid||z->mBoard->mZombies.DataArrayTryToGet(uid)!=z)return false;
 const auto found=c->heavyZombies313B.find(uid);
 return found!=c->heavyZombies313B.end()&&found->second.initialized&&
  found->second.bungee.targetRow==z->mRow&&ValidateFusionHeavyZombie313BView(45,found->second);
}
bool Host(const Zombie* z)
{return z&&World(z->mBoard)&&RowAllowed358(z)&&z->mApp==z->mBoard->mApp&&!z->mNetGhost&&!z->mApp->NetIsClient()&&NativeHeavyZombie313BKind(Kind(z));}
bool Playing(const Zombie* z)
{return Host(z)&&!z->mDead&&!z->mBoard->mPaused&&CanApplyNativeFusionGameplay(z->mBoard);}
void Failed(Board* b,const char* why)
{if(auto* c=GetNativeContext(b)){c->adventure.schedulerFailed=true;c->adventureScheduler.failed=true;}LawnNet::NetTrace("Fusion Heavy313B refused reason=%s",why);}
Reanimation* Clock(Zombie* z,uint8_t layer)
{if(!z||!z->mApp)return nullptr;if(!layer)return z->mApp->ReanimationTryToGet(z->mBodyReanimID);const auto* g=GetNativeSourceGraph313(z);return g&&layer<=2?z->mApp->ReanimationTryToGet(ReanimationID(g->auxiliaryUIDs[layer-1])):nullptr;}
bool Pool(Board* b,int z,int r,int a,int p,int c)
{
 auto* e=b&&b->mApp?b->mApp->mEffectSystem:nullptr;if(!e||e!=gEffectSystem||!e->mReanimationHolder||!e->mAttachmentHolder)return false;
 const auto& rs=e->mReanimationHolder->mReanimations;const auto& as=e->mAttachmentHolder->mAttachments;
 return b->mZombies.mBlock&&b->mProjectiles.mBlock&&b->mCoins.mBlock&&rs.mBlock&&as.mBlock&&
  z>=0&&r>=0&&a>=0&&p>=0&&c>=0&&
  uint64_t(b->mZombies.mMaxSize-b->mZombies.mSize)>=uint64_t(z)&&uint64_t(rs.mMaxSize-rs.mSize)>=uint64_t(r)&&uint64_t(as.mMaxSize-as.mSize)>=uint64_t(a)&&
  uint64_t(b->mProjectiles.mMaxSize-b->mProjectiles.mSize)>=uint64_t(p)&&uint64_t(b->mCoins.mMaxSize-b->mCoins.mSize)>=uint64_t(c);
}
}
namespace
{
using namespace Heavy313B;using Event=FusionSourceGraph313Event;using Param=FusionSourceGraph313Param;
bool Node(const Zombie* z,const char* path,float& x,float& y){return GetNativeSourceZombie269NodePoint(z,path,x,y);}
bool PlantTraits(const Plant* p,FusionHeavy313BPlant& out)
{
 if(!p||!p->mBoard)return false;const auto* d=GetNativePlantData(p);if(!d){out={};return p->mDead;}
 FusionHeavy313BPlant t;t.uid=p->mBoard->mPlants.DataArrayGetID(const_cast<Plant*>(p));t.kind=d->kind;t.column=p->mPlantCol;t.row=p->mRow;
 t.alive=!p->mDead&&p->mIsOnBoard; // Unity object presence, not positive HP.
 if(!t.alive){out=t;return true;}
 bool spike=false,nut=false;int level=0;float px=0,py=0;
 if(!TryGetSpecialZombie269SourcePlantTraits(t.kind,spike,nut,level)||!const_cast<Plant*>(p)->GetShadowCenter(px,py)||
    !NativeBoardPointToSource313(p->mBoard,p->mRow,py,px,py,t.shadowX,t.shadowY))return false;
 t.flying=IsFusionSourceFlyingKind(t.kind);t.caltrop=spike;t.nut=nut;t.tallNut=nut&&level>=3;t.bigNut=FusionSourcePlantIsBigNut313(t.kind);
 bool pumpkin=false;if(!GetNativeRemaining269PlantTags(p,t.flying,pumpkin,t.pot,t.lily))return false;
 t.goldPot=t.kind==1137;t.doubleBox=FusionSourcePlantIsDoubleBox313(t.kind);
 t.ladder=p->mBoard->GetLadderAt(t.column,t.row)!=nullptr;out=t;return true;
}
bool Contact(Zombie* z,bool attack){if(FusionCatapult343Kind(Kind(z))||Kind(z)==223||Kind(z)==45)return false;return NativeClockOnlyZombieContact309(z,attack);}
void Bite(Zombie* z){NativeClockOnlyZombieBite313(z);}
void Sound(Zombie* z,uint16_t k){if(Playing(z))LawnNet::NetPlaySourceSound284(z->mApp,k,.5f);}
void FallSound(Zombie* z){if(Playing(z))z->mApp->PlayFoley(FOLEY_ZOMBIE_FALLING);}
void SourceDie(Zombie* z,int reason)
{std::string error;if(!CanRetireNativeSourceZombie313(z,reason,&error)||!RetireNativeSourceZombie313(z,reason))Failed(z->mBoard,error.c_str());}
bool Effect(Zombie* z,uint16_t kind,float offset)
{float x=0,y=0;if(!Node(z,"Shadow",x,y)||!EnsureNativeSourceEffect269(z->mBoard,kind))return false;SpawnNativeSourceEffect269(z->mBoard,kind,x,y-offset*float(GetNativeFusionPixelsY(z->mBoard)),z->mRenderOrder+1,true);return true;}
bool ShotReady(Zombie* z,Plant* p,std::string* error)
{const uint16_t shot=Kind(z)==112&&GetNativeHeavyZombie313B(z)&&GetNativeHeavyZombie313B(z)->ammo<=20?3:58;
 return Host(z)&&p&&!p->mDead&&EnsureNativeProjectileVisuals(z->mBoard,shot)&&Pool(z->mBoard,0,0,0,1,0);}
bool CaptureMatrix(Zombie* z,CapturedMatrix314& matrix)
{return GetNativeSourceZombieMatrix313(z,"",matrix)&&CapturedMatrix314Valid(matrix);}
bool PrepareCapture(Zombie* z,std::span<const uint32_t> candidates,std::string* error)
{
 auto* helper=GetNativeCapturedPlants314(z->mBoard);CapturedMatrix314 matrix;
 if(!helper||!CaptureMatrix(z,matrix)){if(error)*error="Source45 actual parent matrix/capture cache unavailable";return false;}
 const uint32_t token=z->mBoard->mZombies.DataArrayGetID(z);
 for(auto uid:candidates){auto* p=z->mBoard->mPlants.DataArrayTryToGet(uid);CapturedSourcePlant314View view;std::string why;
  if(p&&NativeRollingActor269SuppressSourceDie(p,5)){if(error)*error="Source45 LIVE rolling parent affine/retirement integration is not ready";return false;}
  if(!p||!helper->Prepare(p,z,token,matrix,view,why)){if(error)*error=why;return false;}}
 return true;
}
bool Capture(Zombie* z,Plant* p)
{
 auto* helper=GetNativeCapturedPlants314(z->mBoard);CapturedMatrix314 m;CapturedSourcePlant314View view;std::string error;
 return helper&&CaptureMatrix(z,m)&&helper->Prepare(p,z,z->mBoard->mZombies.DataArrayGetID(z),m,view,error)&&helper->Commit(p,z,view,m,error);
}
bool Umbrella(Plant* p,Zombie* z)
{
 auto* d=GetNativePlantData(p);
 if(!d||!IsNativeUmbrellaBatch(p)||p->mDead||!p->mIsOnBoard||!std::isfinite(d->animationSpeed)||d->animationSpeed==0)return false;
 // Source checks the UmbrellaLeaf component and actual Animator.speed!=0.
 // Exact source umbrella family: ordinary and its source derived hybrids.
 auto* c=GetNativeContext(p->mBoard);auto* state=EditNativeHeavyZombie313B(z);if(!state||!c)return false;
 state->status=state->bungee.status=24;d->batchMode=0;d->shootSeconds=0;
 auto& pose=c->umbrellaPoses[p->mBoard->mPlants.DataArrayGetID(p)];pose.plantGeneration=p->mBoard->mPlants.DataArrayGetID(p);pose.blocking=true;
 return ApplyNativeUmbrellaPoseEvent(p,pose,FusionUmbrellaPoseEvent::BlockStart);
}
bool MagnetContents(const Plant* p,std::vector<uint16_t>& out)
{
 const auto* d=GetNativePlantData(p);if(!d)return false;
 std::vector<const Plant*> members;Plant* q=nullptr;
 while(p->mBoard->mPlants.IterateNext(q))if(!q->mDead&&q->mIsOnBoard&&GetNativePlantData(q)&&FusionEquipmentMagnetMember(GetNativePlantData(q)->kind))members.push_back(q);
 auto own=std::find(members.begin(),members.end(),p);if(own==members.end())return false;
 std::vector<bool> reached(members.size());reached[size_t(own-members.begin())]=true;bool changed=true;
 while(changed){changed=false;for(size_t i=0;i<members.size();++i)if(!reached[i])for(size_t j=0;j<members.size();++j)if(reached[j]&&FusionEquipmentGraphAdjacent(members[i]->mPlantCol,members[i]->mRow,members[j]->mPlantCol,members[j]->mRow)){reached[i]=true;changed=true;break;}}
 std::vector<uint16_t> next;for(size_t i=0;i<members.size();++i)if(reached[i])next.push_back(GetNativePlantData(members[i])->kind);out=std::move(next);return true;
}
void LadderPlaced(Zombie* z,Plant* p)
{NativeClockOnlyZombieClear309(z);const auto* d=GetNativePlantData(p);auto* c=GetNativeContext(z->mBoard);if(!d||!c)return;
 for(int slot=0;slot<int(c->gloveHolds.size());++slot)if(c->gloveHolds[slot].plantId==p->mNetId&&c->gloveHolds[slot].instanceToken==d->instanceToken)CancelNativeGlove(z->mBoard,slot);}
bool Crash(Zombie* z)
{
 if(!Playing(z)||z->IsDeadOrDying()||(z->mBoard->mMainCounter&1))return true;
 // Shared actual native contact substrate; exact source CrashEntity callbacks
 // remain source-kind aware rather than substituting native chewing.
 Plant* p=nullptr;while(!z->mMindControlled&&z->mBoard->mPlants.IterateNext(p))
 {
  FusionHeavy313BPlant t;if(!PlantTraits(p,t))return false;if(!t.alive||t.row!=z->mRow||t.caltrop||t.flying||!NativeSourcePlantColliderEnabled(p))continue;
  if(GetRectOverlap(z->GetZombieRect(),p->GetPlantRect())<=0)continue;
  const bool tough=t.kind==903||t.kind==913||t.kind==921||t.kind==925||t.kind==1151;
  const bool slippery=t.kind==931||t.kind==1048;
  if(slippery)
  {if((Kind(z)==203||Kind(z)==223)&&!GoldDirectFall(z))return false;Sound(z,77);}
  if(tough||slippery||t.nut&&t.tallNut){ApplyNativeSourcePlantDamage(p,tough||slippery?500:1000,0);z->mPosX+=float(kFusionBasePixelsX);z->mX=int(z->mPosX);Sound(z,uint16_t(RandRangeInt(72,73)));}
  else {(void)RandRangeInt(8,9);p->Squish();}
  if(z->mDead)return true;
 }
 Zombie* enemy=nullptr;while(z->mBoard->mZombies.IterateNext(enemy))
 {if(enemy==z||enemy->mDead||enemy->mRow!=z->mRow||enemy->mMindControlled==z->mMindControlled||NativeWater268ZombieCaptured(enemy))continue;
  const int status=GetNativeFusionZombieStatus(enemy);if(status==10||status==12||GetRectOverlap(z->GetZombieRect(),enemy->GetZombieRect())<=0)continue;
  if(!ApplyNativeSourceTypedDamage313(enemy,4,20,false))return false;if(z->mDead)return true;}
 return true;
}
bool PrepareEvent(Zombie* z,Event e,std::string* error)
{
 const auto k=Kind(z);if(k==203||k==223)return GoldPrepareEvent(z,e,error);
 if(e==Event::Destroy)return CanRetireNativeSourceZombie313(z,2,error);
 if(e==Event::Shoot){if(k==216){float x=0,y=0;return GetNativeSourceZombie269NodePoint(z,"Shoot",x,y)&&CanSpawnNativeDrownVolley343(z,x,y,GetNativeDrownWeapon313Hooks(),error);}return EnsureNativeProjectileVisuals(z->mBoard,58)&&Pool(z->mBoard,0,0,0,1,0);}
 if(e==Event::JumpFail)return EnsureNativeSourceEffect269(z->mBoard,23,error);
 if(e==Event::SetLadder)return z->mBoard->mGridItems.mBlock&&z->mBoard->mGridItems.mSize<z->mBoard->mGridItems.mMaxSize;
 if(e==Event::Catch){auto* s=EditNativeHeavyZombie313B(z);if(!s)return false;std::vector<FusionHeavy313BPlant> cell;Plant* p=nullptr;while(z->mBoard->mPlants.IterateNext(p))if(!p->mDead&&p->mRow==s->bungee.targetRow&&p->mPlantCol==s->bungee.targetColumn){FusionHeavy313BPlant t;if(!PlantTraits(p,t))return false;cell.push_back(t);}std::vector<uint32_t> candidates;for(const auto& t:cell)if(FusionBungee313BStealable(t,cell))candidates.push_back(t.uid);return PrepareCapture(z,candidates,error);}
 return true;
}
bool Parameters(Zombie* z,FusionSourceGraph313Parameters& p)
{auto* s=EditNativeHeavyZombie313B(z);return s&&GetNativeHeavyZombie313BParameters(z,*s,GetNativeHeavyZombie313BHooks(),p);}
bool GraphPreflight(Zombie* z,Event e,std::string* error)
{auto* s=EditNativeHeavyZombie313B(z);return s&&PreflightNativeHeavyZombie313BEvent(z,*s,e,GetNativeHeavyZombie313BHooks(),error);}
bool GraphEvent(Zombie* z,Event e,int32_t value)
{auto* s=EditNativeHeavyZombie313B(z);return s&&DispatchNativeHeavyZombie313BEvent(z,*s,e,value,GetNativeHeavyZombie313BHooks());}
bool RootDelta(Zombie* z,uint8_t state,FusionSourceGraph313Cursor a,FusionSourceGraph313Cursor b,double& out)
{auto* s=GetNativeHeavyZombie313B(z);return s&&RootDeltaNativeHeavyZombie313B(z,*s,state,a,b,out);}
bool MoveRoot(Zombie* z,double dx)
{float x=0,y=0;return std::isfinite(dx)&&NativeSourcePoint313(z,"axis",x,y)&&SetNativeSourceAxis313(z,float(x+dx),y);}
bool NullReady(Zombie*,uint8_t,uint8_t){return false;}
NativeSourceGraph313State* GraphState(Zombie* z){return EditNativeSourceGraph313(z,false);}
bool ClockDefinition(uint16_t kind,std::string* error)
{
 const auto* g=FindFusionSourceGraph313(kind);if(!g)return false;
 const auto type=GetZombieDefinition(ZOMBIE_NORMAL).mReanimationType;ReanimatorEnsureDefinitionLoaded(type,false);
 if(!gReanimatorDefArray||unsigned(type)>=gReanimatorDefCount)return false;const auto& ts=gReanimatorDefArray[type].mTracks;
 for(uint8_t n=0;n<g->stateCount;++n){bool found=false;for(int i=0;i<ts.count;++i)if(ts.tracks[i].mName&&!_stricmp(ts.tracks[i].mName,g->states[n].nativeTrack)&&ts.tracks[i].mTransforms.count>0)found=true;if(!found){if(error)*error=std::string("Heavy313B native clock track unavailable: ")+g->states[n].nativeTrack;return false;}}
 return true;
}
bool AddAux(Zombie* z)
{
 const auto* definition=FindFusionSourceGraph313(Kind(z));auto* g=EditNativeSourceGraph313(z,true);auto* body=Clock(z,0);
 if(!definition||!g||!body||!body->TrackExists("anim_head1"))return false;
 uint32_t born[2]{};
 for(uint8_t layer=1;layer<definition->layerCount;++layer)
 {
  if(g->auxiliaryUIDs[layer-1])continue;
  auto* r=z->mApp->AddReanimation(0,0,0,REANIM_ZOMBIE);if(!r)goto failed;
  born[layer-1]=uint32_t(z->mApp->ReanimationGetID(r));
  if(!AttachReanim(body->GetTrackInstanceByName("anim_head1")->mAttachmentID,r,0,0))goto failed;
  g->auxiliaryUIDs[layer-1]=born[layer-1];
 }
 return true;
 failed:for(int i=0;i<2;++i)if(born[i]){z->mApp->RemoveReanimation(ReanimationID(born[i]));g->auxiliaryUIDs[i]=0;}return false;
}
}
NativeHeavyZombie313BState* EditNativeHeavyZombie313B(Zombie* z,bool create)
{
 auto* c=z?GetNativeContext(z->mBoard):nullptr;const auto* d=GetNativeSourceZombieData(z);
 if(!c||!d||!NativeHeavyZombie313BKind(d->kind)||!z->mApp||!World(z->mBoard)||!RowAllowed358(z,create)||!d->instanceToken)return nullptr;
 const uint32_t uid=z->mBoard->mZombies.DataArrayGetID(z);const bool ghost=z->mNetGhost||z->mApp->NetIsClient();
 if(!uid||(create&&ghost)||(!ghost&&d->instanceToken!=uid))return nullptr;
 auto it=c->heavyZombies313B.find(uid);if(it==c->heavyZombies313B.end()){if(!create)return nullptr;it=c->heavyZombies313B.emplace(uid,NativeHeavyZombie313BState{}).first;}return &it->second;
}
const NativeHeavyZombie313BState* GetNativeHeavyZombie313B(const Zombie* z){return EditNativeHeavyZombie313B(const_cast<Zombie*>(z));}
const NativeHeavyZombie313BHooks& GetNativeHeavyZombie313BHooks()
{
 static const NativeHeavyZombie313BHooks h{GraphState,PlantTraits,Node,NativeSourcePoint313,SetNativeSourceAxis313,NativeSourceBounds313,NativeSourceRowLandY313,
  Contact,Bite,Crash,PrepareEvent,ShotReady,PrepareCapture,Capture,Umbrella,MagnetContents,Sound,FallSound,SourceDie,Effect,LadderPlaced,GoldEvent,GoldUpdate};return h;
}
const NativeSourceGraph313Hooks& GetNativeHeavyZombie313BGraphHooks()
{static const NativeSourceGraph313Hooks h{GraphState,Clock,Parameters,GraphPreflight,GraphEvent,RootDelta,MoveRoot,NullReady,Failed};return h;}
bool Heavy313B::CrashContacts(Zombie* z){return Crash(z);}
namespace {bool Resources(Board* b,uint16_t kind,std::string* error)
{
 const auto* graph=FindFusionSourceGraph313(kind);if(!World(b)||!graph||!NativeHeavyZombie313BKind(kind)||GetNativeSourceZombieCarrier(kind)!=ZOMBIE_NORMAL)return false;
 if(!EnsureNativeSourceZombieVisuals(b,kind,error)||!CanQueryNativeSourceZombieRoot273(b,kind)||!ClockDefinition(kind,error))return false;
 const auto sound=[&](uint16_t id){if(LawnNet::NetSourceSound284Ready(b->mApp,id))return true;if(error)*error="Heavy313B genuine source sound unavailable: "+std::to_string(id);return false;};
 if(kind==28&&!sound(94))return false;
 if(FusionCatapult343Kind(kind)){for(uint16_t id:{43,72,73,77,101})if(!sound(id))return false;}
 if(kind==112&&!sound(40))return false;
 if(kind==32||kind==34){for(uint16_t id:{64,109})if(!sound(id))return false;}
 if(kind==45){for(uint16_t id:{50,62,122,123,124})if(!sound(id))return false;}
 if(kind==203||kind==223){for(uint16_t id:{3,4,44,66})if(!sound(id))return false;if(kind==203){if(!sound(45)||!sound(46))return false;}else for(uint16_t id:{0,1,2,43,72,73,77})if(!sound(id))return false;}
 if(FusionCatapult343Kind(kind)&&!EnsureNativeSourceEffect269(b,54,error))return false;
 if(FusionCatapult343Kind(kind)&&kind!=216&&!EnsureNativeProjectileVisuals(b,58))return false;
 if(kind==216&&!EnsureNativeDrownWeapon313Visuals(b,error,1))return false;
 if(kind==112&&!EnsureNativeProjectileVisuals(b,3))return false;
 if((kind==32||kind==34)&&!EnsureNativeSourceEffect269(b,23,error))return false;
 if((kind==203||kind==223)&&!GoldResources(b,kind,error))return false;
 if(kind==45)
 {if(!GetNativeCapturedPlants314(b)){if(error)*error="Source45 retained-plant presentation cache unavailable";return false;}
  std::vector<FusionHeavy313BPlant> all;Plant* p=nullptr;while(b->mPlants.IterateNext(p)){FusionHeavy313BPlant t;if(!PlantTraits(p,t))return false;all.push_back(t);}
  std::vector<uint32_t> targets;if(!BuildFusionBungee313BCandidates(all,targets))return false;
  for(auto uid:targets)if(auto* target=b->mPlants.DataArrayTryToGet(uid);target&&NativeRollingActor269SuppressSourceDie(target,5)){if(error)*error="Source45 LIVE rolling parent affine/retirement integration is not ready";return false;}}
 return true;
}}
bool EnsureNativeHeavyZombie313BResources(Board* b,uint16_t kind,std::string* error)
{return Resources(b,kind,error);}
bool CanSpawnNativeHeavyZombie313B(Board* b,uint16_t kind,std::string* error)
{const auto* graph=FindFusionSourceGraph313(kind);return graph&&Resources(b,kind,error)&&(b->mApp->NetIsClient()||Pool(b,1,graph->layerCount,graph->layerCount>1?1:0,0,0));}
bool InitializeNativeHeavyZombie313BActor(Zombie* z)
{
 if(!z||!NativeHeavyZombie313BKind(Kind(z)))return false;
 const auto* graph=FindFusionSourceGraph313(Kind(z));if(!graph||!Resources(z->mBoard,Kind(z),nullptr)||
  !Pool(z->mBoard,0,graph->layerCount-1,graph->layerCount>1?1:0,0,0)||!AddAux(z))return false;
 if(z->mNetGhost||z->mApp->NetIsClient())return true;
 auto* s=EditNativeHeavyZombie313B(z,true);return s&&InitializeNativeHeavyZombie313B(z,*s,GetNativeHeavyZombie313BHooks(),GetNativeHeavyZombie313BGraphHooks());
}
bool UpdateNativeHeavyZombie313BActor(Zombie* z)
{
 if(!z||!NativeHeavyZombie313BKind(Kind(z)))return false;if(!Playing(z))return true;
 auto* s=EditNativeHeavyZombie313B(z);if(!s){Failed(z->mBoard,"Heavy313B missing initialized host view");return true;}
 if((Kind(z)==28||Kind(z)==33)&&!(z->mBoard->mMainCounter&1))
 {Plant* p=nullptr;while(z->mBoard->mPlants.IterateNext(p))if(!p->mDead&&p->mRow==z->mRow&&NativeSourcePlantColliderEnabled(p)&&GetRectOverlap(z->GetZombieRect(),p->GetPlantRect())>0)
  NativeHeavyZombie313BContact(z,*s,p,GetNativeHeavyZombie313BHooks());}
 const bool handled=UpdateNativeHeavyZombie313B(z,*s,GetNativeHeavyZombie313BHooks(),GetNativeHeavyZombie313BGraphHooks());
 if(Kind(z)==45&&!z->mDead){CapturedMatrix314 matrix;if(CaptureMatrix(z,matrix))if(auto* captures=GetNativeCapturedPlants314(z->mBoard))captures->UpdateOwner(z,matrix);}
 if(s->failed)Failed(z->mBoard,"Heavy313B source callback/preflight failed");return handled;
}
bool GetNativeHeavyGold313BView(const Zombie* z,FusionHeavyGold313BView& out)
{const auto* s=GetNativeHeavyZombie313B(z);if(!s)return false;FusionHeavyGold313BView next{s->gold.status,s->gold.firstStage,s->gold.secondStage};if(!ValidateFusionHeavyGold313BView(Kind(z),next))return false;out=next;return true;}
bool AdoptNativeHeavyZombie313B(Zombie* z,const FusionHeavyZombie313BView& view,const FusionHeavyGold313BView& gold)
{
 const auto kind=Kind(z);if(!z||(!z->mNetGhost&&!z->mApp->NetIsClient())||!ValidateFusionHeavyZombie313BView(kind,view)||
  !ValidateFusionHeavyGold313BView(kind,gold))return false;
 auto* c=GetNativeContext(z->mBoard);if(!c)return false;const auto uid=z->mBoard->mZombies.DataArrayGetID(z);if(!uid)return false;
 const auto* graph=FindFusionSourceGraph313(kind);if(!graph)return false;
 for(uint8_t layer=0;layer<graph->layerCount;++layer)if(!Clock(z,layer))return false;
 for(uint8_t layer=0;layer<graph->layerCount;++layer)if(!RegisterNativeSourceGraphClock313(Clock(z,layer),true))return false;
 NativeHeavyZombie313BState next;static_cast<FusionHeavyZombie313BView&>(next)=view;next.gold.status=gold.status;next.gold.firstStage=gold.firstStage;next.gold.secondStage=gold.secondStage;next.initialized=true;c->heavyZombies313B[uid]=std::move(next);return true;
}
bool BeginNativeHeavyZombie313BActorDeath(Zombie* z)
{if(Kind(z)==223)return RetireNativeSourceZombie313(z,2);if(Kind(z)==45)return RetireNativeSourceZombie313(z,1);auto* s=EditNativeHeavyZombie313B(z);return s&&BeginNativeHeavyZombie313BDeath(z,*s,GetNativeHeavyZombie313BHooks());}
bool NativeHeavyZombie313BActorContact(Zombie* z,Plant* p)
{auto* s=EditNativeHeavyZombie313B(z);return s&&NativeHeavyZombie313BContact(z,*s,p,GetNativeHeavyZombie313BHooks());}
bool NativeHeavyZombie313BActorLoseObject(Zombie* z,int reason)
{auto* s=EditNativeHeavyZombie313B(z);if(!s)return false;if(Kind(z)==28){s->axePresent=false;z->mHasObject=false;return NativeMiner313BAxeLost(z,*s,false,GetNativeHeavyZombie313BHooks());}if(Kind(z)==33)return NativeHeavyZombie313BLoseLadder(z,*s,GetNativeHeavyZombie313BHooks());return NativeHeavyZombie313BLoseJumper(z,*s,reason,GetNativeHeavyZombie313BHooks());}
bool GetNativeHeavyZombie313BEquipment(const Zombie* z,int& first,int& second,int& unique)
{
 const auto* s=GetNativeHeavyZombie313B(z);if(!s)return false;int f=0,b=0,u=0;
 switch(Kind(z)){case 28:u=s->axePresent?3:0;break;case 32:u=s->lostJumper?0:2;break;
 case 33:b=s->ladderVisible&&z->mShieldType!=SHIELDTYPE_NONE?3:0;break;
 case 34:f=z->mHelmType!=HELMTYPE_NONE?1:0;break;case 203:b=z->mShieldType!=SHIELDTYPE_NONE?2:0;break;
 case 223:f=z->mHelmType!=HELMTYPE_NONE?5:0;b=z->mShieldType!=SHIELDTYPE_NONE?2:0;break;}
 first=f;second=b;unique=u;return true;
}
bool NativeHeavyZombie313BActorCaltrop(Zombie* z)
{auto* s=EditNativeHeavyZombie313B(z);return s&&KillNativeCatapult313BByCaltrop(z,*s,GetNativeHeavyZombie313BHooks());}
bool NativeHeavyZombie313BActorHeadLost(Zombie* z)
{const auto k=Kind(z);if(k!=32&&k!=34)return false;if(Playing(z)){float x=0,y=0,low=0,high=0,top=0;if(NativeSourcePoint313(z,"axis",x,y)&&NativeSourceBounds313(z->mBoard,low,high,top)){high-=2.2f;SetNativeSourceAxis313(z,x>high?x:(std::min)(x+1,high),y);}}return true;}
bool NativeHeavyZombie313BRejectStatus(const Zombie* z,bool mc,bool knock,bool freeze)
{const auto* s=GetNativeHeavyZombie313B(z);const auto k=Kind(z);if(!s)return false;if(k==223)return FusionGoldBoss269RejectStatus(s->gold.status,mc,knock,freeze);if(k==203)return freeze&&s->gold.status==5;if(k==45)return knock;return false;}
bool NativeHeavyZombie313BRejectCold(const Zombie* z)
{const auto* s=GetNativeHeavyZombie313B(z);return s&&Kind(z)==28&&!FusionMiner313BCanCold(s->status);}
bool ApplyNativeHeavyFreeze313B(Zombie* z,int ticks)
{const auto k=Kind(z);const auto* s=GetNativeHeavyZombie313B(z);if(!s||!(k==28||k==32||k==34))return false;if(!Playing(z))return true;
 if(k==28)return s->status==12;
 if(FusionPogo313BFreezeBecomesCold(s->status,GetNativeSourceZombieData(z)->beforeDying)){z->mChilledCounter=(std::max)(z->mChilledCounter,1500);return true;}return false;}
int ResolveNativeHeavyDamage313B(const Zombie* z,int damage){return Kind(z)==223?(std::min)(damage,5000):damage;}
bool ApplyNativeHeavyTakeDamage313B(Zombie* z,int damage,int type)
{const auto k=Kind(z);if(k!=203&&k!=223)return false;if(Playing(z)&&!GoldDamage(z,damage,type,false,nullptr))Failed(z->mBoard,"Gold313B source damage callback refused");return true;}
bool ApplyNativeHeavyBodyDamage313B(Zombie* z,int damage,unsigned)
{const auto k=Kind(z);if(k==216){auto* s=EditNativeHeavyZombie313B(z);return !Playing(z)||!s||ApplyNativeDrownCatapult343Body(z,*s,damage,GetNativeHeavyZombie313BHooks());}if(FusionCatapult343Kind(k))return ApplyNativeCatapult313BBodyDamage(z,damage,GetNativeHeavyZombie313BHooks());if(k!=203&&k!=223)return false;const auto* s=GetNativeHeavyZombie313B(z);return !s||!Playing(z)||s->gold.status!=6;}
bool ApplyNativeHeavyArmorDamage313B(Zombie* z,bool first,int damage,int& overflow)
{
 const auto k=Kind(z);if(!NativeHeavyZombie313BKind(k))return false;if(!Playing(z)||damage<0){overflow=0;return true;}
 if(k==203||k==223)return GoldArmorDamage(z,first,damage,overflow);
 auto* s=EditNativeHeavyZombie313B(z);if(!s){overflow=0;return true;}int& hp=first?z->mHelmHealth:z->mShieldHealth;
 if(k==216&&first){const int before=hp;overflow=(std::max)(0,damage-before);hp=(std::max)(0,before-damage);if(!hp){z->mHelmMaxHealth=0;z->mHelmType=HELMTYPE_NONE;}else s->armorStage=FusionDrownCatapult343ArmorStage(hp,z->mHelmMaxHealth,s->armorStage);return true;}
 if(first&&k!=34&&k!=223||!first&&k!=33&&k!=203&&k!=223)return false;
 const int maximum=first?z->mHelmMaxHealth:z->mShieldMaxHealth;overflow=(std::max)(0,damage-hp);
 if(damage>=hp&&hp>0&&!PlayNativeSourceZombieArmor284(z,first)){overflow=0;return true;}
 hp=(std::max)(0,hp-damage);
 if(!hp){if(first){z->mHelmType=HELMTYPE_NONE;if(k==34)NativeHeavyZombie313BLoseJumper(z,*s,0,GetNativeHeavyZombie313BHooks());}
  else if(k==33)NativeHeavyZombie313BLoseLadder(z,*s,GetNativeHeavyZombie313BHooks());}
 else s->armorStage=uint8_t(FusionGoldBoss269ArmorStage(hp,maximum,s->armorStage));return true;
}
bool UsesNativeHeavyCharred313B(const Zombie* z)
{const auto k=Kind(z);const auto* s=GetNativeHeavyZombie313B(z);return k==223||k==203&&(!s||s->gold.status!=6);}
bool CanApplyNativeHeavyCharred313B(Zombie* z,int raw,bool fix,std::string* error)
{int resolved=0;return UsesNativeHeavyCharred313B(z)&&Playing(z)&&ResolveNativeSourceBaseDamage315(z,raw,fix,resolved)&&GoldDamage(z,resolved,10,true,error);}
bool ApplyNativeHeavyCharred313B(Zombie* z,int raw,bool fix)
{int resolved=0;return UsesNativeHeavyCharred313B(z)&&Playing(z)&&ResolveNativeSourceBaseDamage315(z,raw,fix,resolved)&&GoldDamage(z,resolved,10,false,nullptr);}
bool CanRetireNativeHeavyZombie313B(Zombie* z,int,std::string* error)
{if(!Host(z))return false;const auto k=Kind(z);if(k==203||k==223)return GoldCanRetire(z,error);if(FusionCatapult343Kind(k))return EnsureNativeSourceEffect269(z->mBoard,54,error);return true;}
bool BeforeRetireNativeHeavyZombie313B(Zombie* z,int reason)
{if(!CanRetireNativeHeavyZombie313B(z,reason))return false;const auto k=Kind(z);if((k==203||k==223)&&!GoldBeforeRetire(z))return false;if(FusionCatapult343Kind(k)&&!Effect(z,54,.6f))return false;return true;}
bool RetireNativeHeavyZombie313B(Zombie* z,int reason)
{if(!BeforeRetireNativeHeavyZombie313B(z,reason))return false;z->mDroppedLoot=true;z->DieNoLoot();return true;}
bool GetNativeHeavyZombie313BSemantics(const Zombie* z,int& status,bool& before,bool& collider,bool& flip)
{const auto* s=GetNativeHeavyZombie313B(z);const auto* d=GetNativeSourceZombieData(z);if(!s||!d)return false;status=(d->kind==203||d->kind==223)?s->gold.status:s->status;before=d->beforeDying;collider=!s->caltropDying&&!(d->kind==45&&s->status==24);flip=z->mMindControlled!=s->lookBack;return true;}
}
