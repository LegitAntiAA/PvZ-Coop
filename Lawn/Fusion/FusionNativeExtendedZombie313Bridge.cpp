#include "../Net/NetCommon.h"
#include "FusionNativeExtendedZombie313Bridge.h"
#include "FusionNativeExtendedZombie347.h"
#include "FusionNativeGroundShooter343.h"
#include "FusionNativeSnowZombie363.h"
#include "FusionNative.h"
#include "FusionNativeZombies.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeProjection313.h"
#include "FusionNativeObstacle313.h"
#include "FusionNativeZombiePreflight280.h"
#include "FusionNativePlantDamage.h"
#include "FusionNativeEffects269.h"
#include "FusionNativeEmbeddedEffects269.h"
#include "FusionNativeEquipment.h"
#include "FusionNativeRemainingPlants269.h"
#include "FusionNativeWater268.h"
#include "FusionNativeBasePlants.h"
#include "FusionNativeGarlic.h"
#include "FusionNativeCold.h"
#include "FusionNativeBatchActions.h"
#include "FusionNativePlacement.h"
#include "FusionNativeSourceLifecycle313.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeSourceDamage315.h"
#include "NativeSpecialZombie269.h"
#include "FusionSourceZombieTypes313.h"
#include "FusionSourceRoot313.h"
#include "FusionAnimation.h"
#include "FusionBasePlantRules.h"
#include "SpecialZombie269Rules.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/Attachment.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <array>
namespace LawnFusion
{
// E2's bounded extension of the existing Garlic row movement; no face/bite
// side effects and no replacement one-second duration for source Machine40.
bool BeginNativeSourceRowDuration313(Zombie*,float sourceSeconds);
namespace
{
bool Reject(const char* why,std::string* e){if(e)*e=why;return false;}
uint16_t Kind(const Zombie* z){const auto* d=GetNativeSourceZombieData(z);return d?d->kind:0xffff;}
bool World(const Board* b)
{return IsNativeSourceWorld313(b);}
bool Host(const Zombie* z){return z&&World(z->mBoard)&&GetNativeSourceZombieData(z)&&!z->mNetGhost&&!z->mApp->NetIsClient();}

struct InitParticle313 {uint16_t kind;float x,y;int order;};
struct InitSound313 {uint16_t kind;float volume;};
struct InitJournal313;
thread_local InitJournal313* initializing313=nullptr;
struct InitJournal313
{
    LawnApp* app;Board* board;uint32_t uid;InitJournal313* previous;
    std::vector<InitParticle313> particles;std::vector<InitSound313> sounds;bool board212=false;
    explicit InitJournal313(Zombie* z):app(z->mApp),board(z->mBoard),uid(board->mZombies.DataArrayGetID(z)),previous(initializing313){initializing313=this;}
    ~InitJournal313(){initializing313=previous;}
    bool Matches(const Zombie* z)const{return z&&z->mApp==app&&z->mBoard==board&&board->mZombies.DataArrayTryToGet(uid)==z;}
};
InitJournal313* Journal(Zombie* z){return initializing313&&initializing313->Matches(z)?initializing313:nullptr;}

bool Live(const Plant* p){return p&&!p->mDead&&!p->mSquished&&p->mIsOnBoard&&p->mPlantHealth>0&&GetNativePlantData(p)&&NativeSourcePlantColliderEnabled(p);}
void Failed(Board* b,const char* why)
{if(auto* c=GetNativeContext(b);c&&c->adventure.enabled){c->adventure.schedulerFailed=true;c->adventureScheduler.failed=true;}LawnNet::NetTrace("Fusion Extended313 stopped tick=%d reason=%s",b?b->mMainCounter:-1,why);}
NativeSourceGraph313State* Graph(Zombie* z)
{auto* c=GetNativeContext(z?z->mBoard:nullptr);if(!c||!z||!FindFusionExtendedZombie313Profile(Kind(z)))return nullptr;const uint32_t uid=z->mBoard->mZombies.DataArrayGetID(z);auto i=c->sourceGraphs313.find(uid);return i==c->sourceGraphs313.end()?nullptr:&i->second;}
Reanimation* Clock(Zombie* z,uint8_t layer)
{if(!z||layer>2)return nullptr;if(!layer)return z->mApp->ReanimationTryToGet(z->mBodyReanimID);const auto* g=Graph(z);return g?z->mApp->ReanimationTryToGet(ReanimationID(g->auxiliaryUIDs[layer-1])):nullptr;}
bool ClockDefinition(uint16_t kind,std::string* e)
{
    const auto* g=FindFusionSourceGraph313(kind);if(!g||GetNativeSourceZombieCarrier(kind)!=ZOMBIE_NORMAL)return Reject("Extended313 requires genuine graph and NORMAL clock-only carrier.",e);
    ReanimatorEnsureDefinitionLoaded(REANIM_ZOMBIE,false);if(!gReanimatorDefArray||unsigned(REANIM_ZOMBIE)>=gReanimatorDefCount)return false;
    const auto& tracks=gReanimatorDefArray[int(REANIM_ZOMBIE)].mTracks;if(!tracks.tracks||tracks.count<=0)return false;
    const auto ready=[&](const char* name){for(int i=0;i<tracks.count;++i)if(tracks.tracks[i].mName&&!_stricmp(tracks.tracks[i].mName,name)&&tracks.tracks[i].mTransforms.count>0)return true;return false;};
    for(unsigned i=0;i<g->stateCount;++i)if(!ready(g->states[i].nativeTrack))return Reject("Extended313 graph actual native track has no real frame span.",e);
    return g->layerCount==1||ready("anim_head1");
}
bool FxReady(Board* b,uint16_t k,std::string* e){return NativeSourceEffectParticle269(k)>=0&&EnsureNativeSourceEffect269(b,k,e);}
bool Resources(Board* b,uint16_t kind,std::string* e)
{
    if(!World(b)||!FindFusionExtendedZombie313Profile(kind)||!ClockDefinition(kind,e))return false;
    if(FusionExtendedZombie347Kind(kind))return EnsureNativeExtended347Resources(b,kind,e);
    if(FusionGroundShooter343Kind(kind))return EnsureNativeGroundShooter343Resources(b,kind,e);
    if(FusionSnowZombie363Kind(kind))return EnsureNativeSnowZombie363Resources(b,kind,e);
    std::vector<uint16_t> models{kind};if(kind==35||kind==36||kind==43||kind==40)models.push_back(37);if(kind==212){models.push_back(213);models.push_back(40);models.push_back(37);}if(kind==43)models.push_back(40);
    for(auto k:models)if(!EnsureNativeSourceZombieVisuals(b,k,e)||!CanQueryNativeSourceZombieRoot273(b,k))return false;
    std::vector<uint16_t> effects;std::vector<uint16_t> sounds;
    if(kind==35||kind==36){sounds={8,9,110,111,112,74};}
    if(kind==38||kind==39){effects={54};sounds={8,9,43,62,63};if(!EnsureNativeSourceAnimatedEffect313(b,20,e))return false;}
    if(kind==43||kind==212){effects={11,41,42,54,63,64};sounds={8,9,43,74,112};}
    if(kind==40){effects={54,63,64};sounds={8,9,43,53,54,55,72,73,77};if(!CanCreateNativeEquipmentVisual(b,FusionEquipmentType::Machine))return Reject("Source40 possible Machine equipment resource unavailable before type RNG.",e);SpecialZombie269CrashPlan plan;if(!TryPlanSpecialZombie269Crash(40,0,false,false,0,plan))return Reject("Shared source CrashEntity40 policy not integrated yet.",e);}
    if(kind==213)effects={11};
    if(kind==25||kind==29)sounds={77,86};
    if(kind==26){effects={34,41};sounds={3,4,43,76,92};}
    for(auto k:effects)if(!FxReady(b,k,e))return false;for(auto k:sounds)if(!LawnNet::NetSourceSound284Ready(b->mApp,k))return Reject("Extended313 genuine source sound unavailable.",e);
    Zombie* z=nullptr;while(b->IterateZombies(z))if(!z->mDead&&!GetNativeSourceZombieData(z))return Reject("Extended313 source world contains an untyped NPC.",e);
    Plant* p=nullptr;while(b->IteratePlants(p))if(!p->mDead&&p->mIsOnBoard&&!GetNativePlantData(p))return Reject("Extended313 source world contains an untyped plant.",e);
    if(kind==26){const auto* graph=FindFusionSourceGraph313(kind);if(!graph||graph->layerCount!=2)return false;const auto& state=graph->states[graph->layers[1].defaultState];if(!state.nullMotion||state.duration!=1.f||state.speed!=1.f||state.speedParameter||state.loop)return Reject("Source26 pinned no-motion clock policy is not integrated.",e);}
    return true;
}
bool Input(Zombie* z,NativeExtendedZombie313Input& out)
{
    auto* s=EditNativeExtendedZombie313(z);auto* d=GetNativeSourceZombieData(z);if(!s||!d||!World(z->mBoard))return false;
    NativeExtendedZombie313Input i;i.frame=z->mBoard->mMainCounter;i.beforeDying=s->beforeDying;
    if(!NativeSourcePoint313(z,"",i.sourceRootX,i.sourceRootY)||!NativeSourcePoint313(z,"axis",i.sourceAxisX,i.sourceAxisY)||!NativeSourcePoint313(z,"Shadow",i.sourceShadowX,i.sourceShadowY)||!NativeSourceRowLandY313(z->mBoard,i.sourceShadowX,z->mRow,i.landY))return false;
    float low=0,top=0;if(!NativeSourceBounds313(z->mBoard,low,i.sourceRightBoundary,top))return false;
    const auto* row=GetNativeZombieGarlicData(z);i.changingRow=row&&row->phase==GarlicActionPhase::Moving;
    float factor=0;if(!TryGetFusionZombieStatusSpeedFactor(d->kind,z->mChilledCounter>0,z->mIceTrapCounter>0,z->mButteredCounter>0,row&&FusionGarlicStopsWalking(*row)&&!NativeSourceDeathIgnoresGarlic313(z),factor))return false;
    i.sourceSpeed=d->originSpeed*factor*s->uniqueSpeed*GetNativeWater268GrapSpeedFactor(z);
    i.inWater=z->mBoard->mPlantRow[z->mRow]==PLANTROW_POOL;i.slope=z->mBoard->StageHasRoof();
    i.factoryAxisColumn=z->mBoard->PixelToGridX(int(z->mPosX+40),int(z->mPosY));
    // Ordinary source worlds: no IZE/Eve/Travel/script variants are admitted.
    i.playMachineSound=true;i.alternateMachineSound=false;
    if((d->kind==26||FusionExtendedZombie347Kind(d->kind))&&!ReadNativeSourceStatusLevel313(z,i.sourceStatusLevel))return false;
    out=i;return std::isfinite(i.sourceSpeed)&&i.sourceSpeed>=0;
}
bool NativeBox(Zombie* z,const FusionAnimationBoxCollider303& source,Sexy::Rect& out)
{
    if(!source.enabled||!source.nodeActive)return false;float x=0,y=0;if(!GetNativeSourceZombie269NodePoint(z,"",x,y))return false;
    const float scale=60*z->mScaleZombie;const float left=x+scale*source.aabb[0],top=y-scale*source.aabb[3];
    const int l=int(std::floor(left)),t=int(std::floor(top));out=Sexy::Rect(l,t,int(std::ceil(x+scale*source.aabb[2]))-l,int(std::ceil(y-scale*source.aabb[1]))-t);return true;
}
bool Touch(const Sexy::Rect& a,const Sexy::Rect& b){return a.mX<=b.mX+b.mWidth&&a.mX+a.mWidth>=b.mX&&a.mY<=b.mY+b.mHeight&&a.mY+a.mHeight>=b.mY;}
bool ContactsRect(Zombie* z,const Sexy::Rect& rect,std::vector<NativeExtendedZombie313Ref>& out)
{
    std::vector<NativeExtendedZombie313Ref> hits;Plant* p=nullptr;while(z->mBoard->IteratePlants(p))if(Live(p)&&p->mRow==z->mRow&&Touch(rect,p->GetPlantRect()))hits.push_back({z->mBoard->mPlants.DataArrayGetID(p),false});
    Zombie* q=nullptr;while(z->mBoard->IterateZombies(q))
    {if(q==z||q->mDead||q->mRow!=z->mRow||!GetNativeSourceZombieData(q))continue;FusionAnimationBoxCollider303 box;Sexy::Rect body;
     const auto kind=Kind(q);bool eligible=GetNativeSourceZombieBox313(q,box)&&NativeBox(q,box,body);
     // Earlier accepted source families retain their existing native collision
     // adaptation. Never manufacture a NORMAL box for a new source26/no-box.
     if(!eligible&&!FindFusionSourceGraph313(kind)){body=q->GetZombieRect();eligible=true;}
     if(eligible&&Touch(rect,body))hits.push_back({z->mBoard->mZombies.DataArrayGetID(q),true});}
    out.swap(hits);return true;
}
bool Contacts(Zombie* z,const NativeExtendedZombie313Box& b,std::vector<NativeExtendedZombie313Ref>& out)
{float gx=0,gy=0,x=0,y=0;if(!NativeSourceGroundAnchor313(z,gx,gy)||!NativeSourcePointToBoard313(z->mBoard,z->mRow,gy,b.x,b.y,x,y))return false;const float px=float(kFusionBasePixelsX),py=float(GetNativeFusionPixelsY(z->mBoard));return ContactsRect(z,{int(std::floor(x-b.width*.5f*px)),int(std::floor(y-b.height*.5f*py)),int(std::ceil(b.width*px)),int(std::ceil(b.height*py))},out);}
bool BodyContacts(Zombie* z,std::vector<NativeExtendedZombie313Ref>& out)
{if((Kind(z)==40||Kind(z)==210||Kind(z)==219)&&(z->mBoard->mMainCounter&1)){out.clear();return true;}FusionAnimationBoxCollider303 source;Sexy::Rect native;if(!GetNativeSourceZombieBox313(z,source)||!NativeBox(z,source,native))return false;return ContactsRect(z,native,out);}
bool Traits(const Plant* p,bool& nut,bool& big,int& level,bool& cal,bool& umbrella,bool& spick)
{const auto* d=GetNativePlantData(p);if(!d||!GetNativeSpecialZombie269PlantTraits(p,cal,nut,level))return false;big=FusionSourcePlantIsBigNut313(d->kind);
 umbrella=false;for(uint16_t k:{30,916,923,937,1115,1127,1128,1131,1143,1144,1148,1185,1186})if(d->kind==k)umbrella=true;
 spick=d->kind==906||d->kind==1060||d->kind==1074||d->kind==1075;return true;}
bool BaseContact(Zombie* z,bool& attacking){attacking=NativeClockOnlyZombieContact309(z,true);return true;}
bool Bite(Zombie* z){NativeClockOnlyZombieBite313(z);return true;}
bool Translate(Zombie*,float,float);
void Flash(Plant*);
bool EatContact(Zombie* z,const NativeExtendedZombie313Ref& ref,bool& reacted)
{
    auto* d=GetNativeSourceZombieData(z);if(!d)return false;reacted=false;
    // Slot47 is Zombie.CrashEntity, NOT EatEffect or an ordinary timed bite.
    // Its opposite-NPC branch deals20/type4 but returnsFALSE: no row change.
    if(ref.zombie){auto* q=z->mBoard->mZombies.DataArrayTryToGet(ref.localUID);if(!q||q->mDead||q->mMindControlled==z->mMindControlled||q->mRow!=z->mRow)return true;const int status=GetNativeFusionZombieStatus(q);return status==10||status==12||ApplyNativeSourceTypedDamage313(q,4,20,false);}
    auto* p=z->mBoard->mPlants.DataArrayTryToGet(ref.localUID);if(!Live(p)||z->mMindControlled)return true;
    bool cal=false,nut=false;int level=0;SpecialZombie269CrashPlan plan;
    if(!GetNativeSpecialZombie269PlantTraits(p,cal,nut,level)||!TryPlanSpecialZombie269Crash(40,GetNativePlantData(p)->kind,cal,nut,level,plan))return false;
    if(plan.action==SpecialZombie269CrashAction::Ignore)return true;
    if(plan.action==SpecialZombie269CrashAction::Crush){const int sound=RandRangeInt(8,9);LawnNet::NetPlaySourceSound284(z->mApp,uint16_t(sound),.5f);p->Die(0);reacted=true;return true;}
    auto* app=z->mApp;auto* board=z->mBoard;const uint32_t uid=board->mZombies.DataArrayGetID(z);if(plan.killByCaltrop)LawnNet::NetPlaySourceSound284(app,77,.5f);Flash(p);ApplyNativeSourcePlantDamage(p,plan.damage,0);
    if(app->mBoard!=board)return true;z=board->mZombies.DataArrayTryToGet(uid);if(!z||z->mDead)return true;
    if(plan.knockRight&&!Translate(z,1,0))return false;LawnNet::NetPlaySourceSound284(app,uint16_t(RandRangeInt(72,73)),.5f);reacted=true;return true;
}
bool Translate(Zombie* z,float dx,float dy)
{float sx=0,sy=0;return Host(z)&&NativeSourcePoint313(z,"axis",sx,sy)&&SetNativeSourceAxis313(z,sx+dx,sy+dy);}
bool Velocity(Zombie* z,float vx,float vy)
{if(!Host(z)||!std::isfinite(vx)||!std::isfinite(vy))return false;z->mVelX=vx*float(kFusionBasePixelsX)*.01f;z->mVelZ=vy*float(GetNativeFusionPixelsY(z->mBoard))*.01f;return true;}
bool RootPlace(Zombie* z,float x,float y)
{float rx=0,ry=0,ax=0,ay=0;return NativeSourcePoint313(z,"",rx,ry)&&NativeSourcePoint313(z,"axis",ax,ay)&&SetNativeSourceAxis313(z,ax+x-rx,ay+y-ry);}
bool Align(Zombie* z,float x,float y){return SetNativeSourceAxis313(z,x,y);}
bool ImpNode(Zombie* z,float x,float land)
{float fx=0,fy=0;return NativeSourcePoint313(z,"RealShadow",fx,fy)&&std::abs(fx-x)<.001f&&std::abs(fy-land)<.001f;}
bool Transfer(Zombie* parent,Zombie* child)
{if(!Host(parent)||!Host(child))return false;if(parent->mChilledCounter>0){ApplyNativeColdSlow(child);child->mChilledCounter=parent->mChilledCounter;if(auto* c=GetOrCreateNativeZombieColdData(child))c->chilledTicks=child->mChilledCounter;}if(parent->mIceTrapCounter>0&&!ApplyNativeColdFreeze(child,parent->mIceTrapCounter))return false;if(const auto* a=GetNativeZombieActionData(parent);a&&a->jalaedOnDeath){auto* b=GetOrCreateNativeZombieActionData(child);if(!b)return false;b->jalaedOnDeath=true;}return true;}
bool SourceNode(Zombie* z,const char* p,float& x,float& y){return NativeSourcePoint313(z,p,x,y);}
bool PlantDamage(Plant* p,int n,int t){if(!Live(p)||n<0)return false;ApplyNativeSourcePlantDamage(p,n,t);return true;}
bool Crush(Zombie* z,Plant* p,bool sourceCrash,int sound)
{if(!Host(z)||!Live(p))return false;(void)sourceCrash;LawnNet::NetPlaySourceSound284(z->mApp,uint16_t(sound),.5f);p->mSquished=true;p->Die(0);return true;}
bool Effect(Zombie* z,uint16_t k,float sx,float sy)
{float gx=0,gy=0,x=0,y=0;if(!Host(z)||!FxReady(z->mBoard,k,nullptr)||!NativeSourceGroundAnchor313(z,gx,gy)||!NativeSourcePointToBoard313(z->mBoard,z->mRow,gy,sx,sy,x,y))return false;const int order=Board::MakeRenderOrder(RENDER_LAYER_PARTICLE,z->mRow,0);if(auto* journal=Journal(z))journal->particles.push_back({k,x,y,order});else SpawnNativeSourceEffect269(z->mBoard,k,x,y,order,true);return true;}
void Sound(Zombie* z,uint16_t k,float volume){if(!Host(z))return;if(auto* journal=Journal(z))journal->sounds.push_back({k,volume});else LawnNet::NetPlaySourceSound284(z->mApp,k,volume);}
void Shake(Board* b,float seconds){if(b&&seconds>0)b->ShakeBoard(seconds>.1f?9:3,seconds>.1f?9:3);}
void Flash(Plant* p){if(p&&!p->mDead)p->mEatenFlashCountdown=25;}
void Retire(Zombie* z,int reason){if(Host(z)&&!RetireNativeSourceZombie313(z,reason))Failed(z->mBoard,"Prepared Extended313 source retirement refused.");}
bool ObstacleRequest(Zombie* z,FusionSourceObstacle313Kind kind,uint8_t variant,const char* node,FusionSourceObstacle313Spawn& out)
{const auto* d=GetNativeSourceZombieData(z);if(!d)return false;FusionSourceObstacle313Spawn r;r.kind=kind;r.ownerId=z->mNetId;r.ownerToken=d->instanceToken;r.row=z->mRow;r.variant=variant;r.fromHypno=kind==FusionSourceObstacle313Kind::KirovBomb&&z->mMindControlled;float gx=0;if(!GetNativeSourceZombie269NodePoint(z,node,r.centerX,r.centerY)||!NativeSourceGroundAnchor313(z,gx,r.groundY))return false;r.renderOrder=kind==FusionSourceObstacle313Kind::KirovBomb?Board::MakeRenderOrder(RENDER_LAYER_PROJECTILE,z->mRow,1):z->mRenderOrder+(kind==FusionSourceObstacle313Kind::SuperBall?1:0);out=r;return true;}
bool Ball(Zombie* z,uint16_t kind){FusionSourceObstacle313Spawn r;return ObstacleRequest(z,FusionSourceObstacle313Kind::SuperBall,kind==212?1:0,"Shoot",r)&&SpawnNativeSourceObstacle313(z->mBoard,r);}
bool Bomb(Zombie* z){if(FusionExtendedZombie347Kind(Kind(z)))return NativeExtended347Bomb(z);FusionSourceObstacle313Spawn r;return ObstacleRequest(z,FusionSourceObstacle313Kind::KirovBomb,0,"Bomb",r)&&SpawnNativeSourceObstacle313(z->mBoard,r);}
bool IronHead(Zombie* z){return EmitNativeSourceZombiePart313(z,1);}
bool Armor(Zombie* z,bool direct)
{if(direct)return Reject("Direct source DropArmor item requires separately prepared source equipment operation.",nullptr);return PlayNativeSourceZombieArmor284(z,true);}
bool Board212(Zombie* z,bool born)
{auto* c=GetNativeContext(z->mBoard);if(!c)return false;if(born){if(auto* journal=Journal(z))journal->board212=true;else c->extendedBoard313.punishTime=100000.f;}else if(!z->mMindControlled){if(c->extendedBoard313.ultimateCount<=0)return false;--c->extendedBoard313.ultimateCount;}return true;}
bool Preflight(Zombie* z,NativeExtendedZombie313Operation op,std::string* e)
{
    if(!Host(z)||!Resources(z->mBoard,Kind(z),e))return false;const auto kind=Kind(z);
    if(FusionExtendedZombie347Kind(kind))return PreflightNativeExtended347(z,op==NativeExtendedZombie313Operation::Initialize?NativeExtended347Operation::Initialize:op==NativeExtendedZombie313Operation::MachineDeath||op==NativeExtendedZombie313Operation::KirovDeath||op==NativeExtendedZombie313Operation::IronDeath?NativeExtended347Operation::Death:NativeExtended347Operation::Throw,e);std::vector<uint16_t> children;
    if(op==NativeExtendedZombie313Operation::Hammer&&(kind==43||kind==212))children.assign(3,kind==212?213:37);
    if((op==NativeExtendedZombie313Operation::Throw||op==NativeExtendedZombie313Operation::BodyChange)&&(kind==35||kind==36))children={37};
    if(op==NativeExtendedZombie313Operation::MachineDeath){const auto* s=GetNativeExtendedZombie313(z);if(!s)return false;if(s->machineType==0)return CanCreateNativeEquipment(z->mBoard,FusionEquipmentType::Machine);children.assign(5,37);const auto* row=GetNativeZombieGarlicData(z);if(row&&row->phase==GarlicActionPhase::Moving)return true;if(!z->mMindControlled){FusionSourceObstacle313Spawn r;if(!ObstacleRequest(z,FusionSourceObstacle313Kind::MachineBall,s->machineType==2?1:0,"Shadow",r)||!CanSpawnNativeSourceObstacle313(z->mBoard,r,e))return false;}}
    if(op==NativeExtendedZombie313Operation::KirovBomb){FusionSourceObstacle313Spawn r;if(!ObstacleRequest(z,FusionSourceObstacle313Kind::KirovBomb,0,"Bomb",r)||!CanSpawnNativeSourceObstacle313(z->mBoard,r,e))return false;}
    if(op==NativeExtendedZombie313Operation::Throw&&(kind==43||kind==212)){FusionSourceObstacle313Spawn r;if(!ObstacleRequest(z,FusionSourceObstacle313Kind::SuperBall,kind==212?1:0,"Shoot",r)||!CanSpawnNativeSourceObstacle313(z->mBoard,r,e))return false;}
    if(!children.empty()&&!CanAllocateNativeSourceZombieBatch280(z->mBoard,children,e))return false;
    if(op==NativeExtendedZombie313Operation::MachineDeath&&!z->mMindControlled)
    {
        const auto* s=GetNativeExtendedZombie313(z);const auto* row=GetNativeZombieGarlicData(z);
        if(s&&s->machineType&&!(row&&row->phase==GarlicActionPhase::Moving))
        {
            auto* fx=z->mApp->mEffectSystem;if(!fx||fx!=gEffectSystem||!fx->mReanimationHolder||!fx->mAttachmentHolder)return false;
            const auto& rs=fx->mReanimationHolder->mReanimations;const auto& as=fx->mAttachmentHolder->mAttachments;
            // Five real37 clocks + one MachineBall clock, one ball attachment.
            // Separate per-factory checks must not spend the same free slot.
            if(!rs.mBlock||rs.mSize>rs.mMaxSize||rs.mMaxSize-rs.mSize<6||!as.mBlock||as.mSize>=as.mMaxSize)return Reject("Combined source40 death peak6R+1A unavailable before RNG/retirement.",e);
        }
    }
    return true;
}
bool EventPreflight(Zombie* z,FusionSourceGraph313Event ev,std::string* e)
{if(FusionGroundShooter343Kind(Kind(z))&&ev==FusionSourceGraph313Event::Shoot)return CanShootNativeGround343(z,e);auto op=NativeExtendedZombie313Operation::Particle;switch(ev){case FusionSourceGraph313Event::Throw:op=NativeExtendedZombie313Operation::Throw;break;case FusionSourceGraph313Event::Crash:op=Kind(z)==26?NativeExtendedZombie313Operation::KirovCrash:NativeExtendedZombie313Operation::Hammer;break;case FusionSourceGraph313Event::Bomb:op=NativeExtendedZombie313Operation::KirovBomb;break;default:break;}return Preflight(z,op,e);}
bool GraphParameters(Zombie* z,FusionSourceGraph313Parameters& p)
{NativeExtendedZombie313Input i;auto* s=EditNativeExtendedZombie313(z);if(!s||!Input(z,i))return false;p.speed=i.sourceSpeed;p.attacking=z->mIsEating;if(Kind(z)==114)p.wait=z->mShieldType==SHIELDTYPE_NONE;return true;}
bool GraphEvent(Zombie* z,FusionSourceGraph313Event ev,int32_t n){if(FusionGroundShooter343Kind(Kind(z))&&ev==FusionSourceGraph313Event::Shoot)return ShootNativeGround343(z);return DispatchNativeExtendedZombie313Event(z,ev,n,GetNativeExtendedZombie313Hooks());}
bool RootDelta(Zombie* z,uint8_t state,FusionSourceGraph313Cursor a,FusionSourceGraph313Cursor b,double& out){return FusionSnowZombie363Kind(Kind(z))?SourceRootDeltaSnow363(Kind(z),state,a,b,out):FusionGroundShooter343Kind(Kind(z))?SourceRootDeltaGround343(Kind(z),state,a,b,out):FusionExtendedZombie347Kind(Kind(z))?SourceRootDelta347(Kind(z),state,a,b,out):SourceRootDelta313(Kind(z),state,a,b,out);}
bool MoveRoot(Zombie* z,double source)
{const auto* s=GetNativeExtendedZombie313(z);if(!s||!std::isfinite(source))return false;const double delta=source*kFusionBasePixelsX*z->mScaleZombie*(s->rootFlip?-1:1);if(!std::isfinite(z->mPosX+delta))return false;z->mPosX+=float(delta);z->mX=int(z->mPosX);return true;}
bool NullExit(Zombie*,uint8_t,uint8_t){return false;} // Exact Kirov null-motion timed exit remains an explicit source engine seam.
bool MachineDeath(Zombie* z,uint8_t type)
{
    if(FusionExtendedZombie347Kind(Kind(z)))return NativeExtended347MachineDeath(z);
    float sx=0,sy=0,bx=0,by=0;if(!NativeSourcePoint313(z,"axis",sx,sy)||!GetNativeSourceZombie269NodePoint(z,"Shadow",bx,by)||!Effect(z,54,sx,sy+.6f))return false;Sound(z,43,.5f);
    if(type==0)return CreateNativeEquipment(z->mBoard,FusionEquipmentType::Machine,bx,by-float(GetNativeFusionPixelsY(z->mBoard)),true)!=nullptr;
    const auto* row=GetNativeZombieGarlicData(z);if(row&&row->phase==GarlicActionPhase::Moving)return true;
    if(!z->mMindControlled){FusionSourceObstacle313Spawn r;if(!ObstacleRequest(z,FusionSourceObstacle313Kind::MachineBall,type==2?1:0,"Shadow",r)||!SpawnNativeSourceObstacle313(z->mBoard,r))return false;}
    auto* app=z->mApp;auto* b=z->mBoard;const uint32_t uid=b->mZombies.DataArrayGetID(z);const bool controlled=z->mMindControlled;
    for(int n=0;n<5;++n){const float jitter=RandRangeFloat(-1,1);if(!SpawnNativeExtendedZombieAtAxis313(b,37,z->mRow,sx+jitter,sy,controlled))return false;if(app->mBoard!=b)return true;z=b->mZombies.DataArrayTryToGet(uid);if(!z||z->mDead)return true;}return true;
}
bool IronMC(Zombie* z)
{
    float sx=0,sy=0;if(!Host(z)||!NativeSourcePoint313(z,"axis",sx,sy))return false;
    auto* app=z->mApp;auto* board=z->mBoard;const uint32_t uid=board->mZombies.DataArrayGetID(z);
    if(!ApplyNativeSourceMindControl313(z,1))return false;if(app->mBoard!=board)return true;z=board->mZombies.DataArrayTryToGet(uid);if(!z||z->mDead)return true;
    auto* s=EditNativeExtendedZombie313(z);if(!s||!z->mMindControlled)return false;s->rootFlip=true;NativeClockOnlyZombieClear309(z);
    if(!SetNativeSourceAxis313(z,sx,sy))return false;
    return true; // The common MC transaction owns its single genuine effect.
}
bool KirovCrash(Zombie* z,float sx,float sy,float radius,int damage)
{
    float gx=0,gy=0,x=0,y=0;if(!Host(z)||radius!=2||damage!=1000||!NativeSourceGroundAnchor313(z,gx,gy)||!NativeSourcePointToBoard313(z->mBoard,z->mRow,gy,sx,sy,x,y))return false;
    // Same shared source ellipse projection as the accepted area-damage
    // mechanism. Snapshot actual recipients before recursive source callbacks.
    std::vector<NativeExtendedZombie313Ref> refs;const float px=float(kFusionBasePixelsX),py=float(GetNativeFusionPixelsY(z->mBoard));
    Plant* p=nullptr;if(!z->mMindControlled)while(z->mBoard->IteratePlants(p))if(Live(p)&&std::abs(p->mRow-z->mRow)<=1){auto r=p->GetPlantRect();const float dx=(std::clamp(x,float(r.mX),float(r.mX+r.mWidth))-x)/px,dy=(std::clamp(y,float(r.mY),float(r.mY+r.mHeight))-y)/py;if(dx*dx+dy*dy<=radius*radius)refs.push_back({z->mBoard->mPlants.DataArrayGetID(p),false});}
    Zombie* q=nullptr;while(z->mBoard->IterateZombies(q))if(q!=z&&!q->mDead&&q->mMindControlled!=z->mMindControlled&&std::abs(q->mRow-z->mRow)<=1){auto r=q->GetZombieRect();const float dx=(std::clamp(x,float(r.mX),float(r.mX+r.mWidth))-x)/px,dy=(std::clamp(y,float(r.mY),float(r.mY+r.mHeight))-y)/py;if(dx*dx+dy*dy<=radius*radius)refs.push_back({z->mBoard->mZombies.DataArrayGetID(q),true});}
    auto* b=z->mBoard;auto* app=z->mApp;for(const auto& r:refs){if(app->mBoard!=b)return true;if(r.zombie){if(auto* a=b->mZombies.DataArrayTryToGet(r.localUID);a&&!a->mDead&&!ApplyNativeSourceTypedDamage313(a,10,1000))return false;}else if(auto* a=b->mPlants.DataArrayTryToGet(r.localUID);Live(a))ApplyNativeSourcePlantDamage(a,1000,2);}return true;
}
}
NativeExtendedZombie313State* EditNativeExtendedZombie313(Zombie* z,bool create)
{auto* c=GetNativeContext(z?z->mBoard:nullptr);const auto* d=GetNativeSourceZombieData(z);if(!c||!d||!FindFusionExtendedZombie313Profile(d->kind))return nullptr;const uint32_t uid=z->mBoard->mZombies.DataArrayGetID(z);if(!uid)return nullptr;auto i=c->extendedZombies313.find(uid);if(i==c->extendedZombies313.end()){if(!create)return nullptr;i=c->extendedZombies313.emplace(uid,NativeExtendedZombie313State{}).first;}return &i->second;}
const NativeExtendedZombie313State* GetNativeExtendedZombie313(const Zombie* z){return EditNativeExtendedZombie313(const_cast<Zombie*>(z));}
const NativeExtendedZombie313Hooks& GetNativeExtendedZombie313Hooks()
{
    static const NativeExtendedZombie313Hooks h=[](){NativeExtendedZombie313Hooks h;h.stateFor=[](Zombie* z){return EditNativeExtendedZombie313(z);};h.graph.stateFor=Graph;h.graph.clock=Clock;h.graph.parameters=GraphParameters;h.graph.preflightEvent=EventPreflight;h.graph.event=GraphEvent;h.graph.rootDelta=RootDelta;h.graph.moveRootDelta=MoveRoot;h.graph.nullExitReady=NullExit;h.graph.failed=Failed;h.input=Input;h.randomInt=[](Board*,int a,int b){return RandRangeInt(a,b-1);};h.randomFloat=[](Board*,float a,float b){return RandRangeFloat(a,b);};h.preflight=Preflight;h.contacts=Contacts;h.bodyContacts=BodyContacts;h.eatContact=EatContact;h.baseContact=BaseContact;h.baseBite=Bite;h.plantTraits=Traits;h.setVelocity=Velocity;h.translateRoot=Translate;h.sourceNode=SourceNode;
    h.spawn=[](Zombie* z,uint16_t kind,float x,bool mc){float land=0;if(!NativeSourceRowLandY313(z->mBoard,x,z->mRow,land))return static_cast<Zombie*>(nullptr);return SpawnNativeExtendedZombieAtAxis313(z->mBoard,kind,z->mRow,x,land,mc);};h.placeRoot=RootPlace;h.transferThrowStatus=Transfer;h.alignShadow=Align;h.adjustImpFlyNode=ImpNode;h.changeRow=BeginNativeSourceRowDuration313;h.crush=Crush;h.plantDamage=PlantDamage;h.zombieDamage=[](Zombie* z,int t,int n){return ApplyNativeSourceTypedDamage313(z,t,n,false);};h.projectile=Ball;h.kirovBomb=Bomb;h.kirovCrash=KirovCrash;h.machineDeath=MachineDeath;h.ironMindControl=IronMC;h.ironHead=IronHead;h.armorFall=Armor;h.sourceBoard212=Board212;h.effect=Effect;h.sound=Sound;h.flash=Flash;h.shake=Shake;h.retire=Retire;h.failed=Failed;return h;}();return h;
}
bool EnsureNativeExtendedZombie313Resources(Board* b,uint16_t kind,std::string* e)
{return Resources(b,kind,e);}
bool CanSpawnNativeExtendedZombie313(Board* b,uint16_t kind,std::string* e)
{FusionSourceDeathBudget319 peak;return Resources(b,kind,e)&&GetNativeSourceZombieBirthBudget319(b,std::span(&kind,1),peak,e)&&CanAllocateNativeSourceBudget319(b,peak,e);}
bool PrepareNativeExtendedZombie313Clocks(Zombie* z,std::string* e)
{
    if(!z||!World(z->mBoard)||!FindFusionExtendedZombie313Profile(Kind(z))||!ClockDefinition(Kind(z),e))return false;auto* c=GetNativeContext(z->mBoard);const uint32_t uid=z->mBoard->mZombies.DataArrayGetID(z);auto [i,_]=c->sourceGraphs313.try_emplace(uid);auto& state=i->second;const auto* graph=FindFusionSourceGraph313(Kind(z));
    auto* body=Clock(z,0);if(!body)return false;auto* fx=z->mApp->mEffectSystem;if(!fx||fx!=gEffectSystem||!fx->mReanimationHolder||!fx->mAttachmentHolder)return false;
    auto& rs=fx->mReanimationHolder->mReanimations;auto& as=fx->mAttachmentHolder->mAttachments;unsigned need=0;for(uint8_t n=1;n<graph->layerCount;++n)if(!Clock(z,n))++need;
    if(!need)return NativeSourceGraph313ClockTracksReady(z,GetNativeExtendedZombie313Hooks().graph,e);
    if(!rs.mBlock||need>unsigned(rs.mMaxSize-rs.mSize)||!as.mBlock||!body->TrackExists("anim_head1"))return false;auto* track=body->GetTrackInstanceByName("anim_head1");auto* a=as.DataArrayTryToGet(unsigned(track->mAttachmentID));
    if(a&&!a->mDead){if(a->mNumEffects<0||need>unsigned(MAX_EFFECTS_PER_ATTACHMENT-a->mNumEffects))return false;}else if(as.mSize>=as.mMaxSize)return false;
    for(uint8_t n=1;n<graph->layerCount;++n)if(!Clock(z,n)){auto* r=z->mApp->AddReanimation(0,0,0,REANIM_ZOMBIE);if(!r)return false;r->PlayReanim("anim_head1",REANIM_LOOP,0,0);for(int t=0;t<r->mDefinition->mTracks.count;++t)r->mTrackInstances[t].mForceDontRender=true;if(!AttachReanim(track->mAttachmentID,r,0,0)){r->ReanimationDie();return false;}state.auxiliaryUIDs[n-1]=uint32_t(z->mApp->ReanimationGetID(r));}
    return NativeSourceGraph313ClockTracksReady(z,GetNativeExtendedZombie313Hooks().graph,e);
}
bool InitializeNativeExtendedZombie313Bridge(Zombie* z)
{
    if(!Host(z)||!PrepareNativeExtendedZombie313Clocks(z))return false;
    auto* s=EditNativeExtendedZombie313(z,true);if(!s)return false;
    if(s->initialized)return !s->failed;
    InitJournal313 journal(z);
    // Capture source callbacks against this exact full generation. No initial
    // sound, board212 mutation or entry0 particle is published before every
    // fallible graph/model initialization has succeeded.
    if(!InitializeNativeExtendedZombie313(z,*s,GetNativeExtendedZombie313Hooks()))return false;
    if(FusionGroundShooter343Kind(Kind(z))&&!InitializeNativeGroundShooter343(z))return false;
    if(FusionSnowZombie363Kind(Kind(z))&&!InitializeNativeSnowZombie363(z))return false;
    if(journal.app->mBoard!=journal.board)return false;
    z=journal.board->mZombies.DataArrayTryToGet(journal.uid);s=EditNativeExtendedZombie313(z);
    const auto* data=GetNativeSourceZombieData(z);const auto* graph=Graph(z);
    if(!z||z->mDead||!s||s->failed||!s->initialized||!data||data->instanceToken!=journal.uid||
        !graph||!graph->initialized||graph->failed||graph->localUID!=journal.uid)return false;
    auto* context=GetNativeContext(journal.board);if(!context)return false;
    if(Kind(z)==212)
    {
        if(!journal.board212)return false;
        context->extendedBoard313.punishTime=100000.f;
        if(!z->mMindControlled)++context->extendedBoard313.ultimateCount;
    }
    // Direct commit of the immutable captures avoids recapturing/replaying the
    // callbacks while the source initialization journal is still scoped.
    for(const auto& sound:journal.sounds)LawnNet::NetPlaySourceSound284(journal.app,sound.kind,sound.volume);
    for(const auto& fx:journal.particles)SpawnNativeSourceEffect269(journal.board,fx.kind,fx.x,fx.y,fx.order,true);
    return true;
}
bool UpdateNativeExtendedZombie313Bridge(Zombie* z)
{auto* s=EditNativeExtendedZombie313(z);if(!s)return false;if(!Host(z))return true;if(FusionSnowZombie363Kind(Kind(z))&&!UpdateNativeSnowZombie363Attribute(z)){s->failed=true;return true;}if(FusionGroundShooter343Kind(Kind(z))&&!UpdateNativeGroundShooter343Attribute(z)){s->failed=true;return true;}return UpdateNativeExtendedZombie313(z,*s,GetNativeExtendedZombie313Hooks());}
bool MoveNativeExtendedZombie313Physics(Zombie* z)
{auto* s=EditNativeExtendedZombie313(z);if(!s)return false;if(!Host(z)||z->mDead||z->mBoard->mPaused||z->mApp->mGameScene!=SCENE_PLAYING)return true;const auto k=Kind(z);if(k!=26&&k!=205&&k!=206&&!(k==37&&(s->sourceStatus==16||s->sourceStatus==17)))return true;const float dx=z->mVelX/float(kFusionBasePixelsX),dy=z->mVelZ/float(GetNativeFusionPixelsY(z->mBoard));if(!Translate(z,dx,dy))Failed(z->mBoard,"Extended313 native rigid-body projection failed.");return true;}
Zombie* SpawnNativeExtendedZombieAtAxis313(Board* b,uint16_t kind,int row,float x,float y,bool controlled)
{if(!World(b)||b->mApp->NetIsClient()||!std::isfinite(x)||!std::isfinite(y)||row<0||row>=GetNativeFusionRowCount(b))return nullptr;NativeSourceZombieSpawnRequest r;r.kind=kind;r.row=row;r.fromWave=0;r.friendly=controlled;r.positioned=true;r.shadowX=float(b->GetCellCenterX(0,row)+(x+4.8f)*kFusionBasePixelsX);auto* z=SpawnNativeSourceZombie(b,r);if(!z)return nullptr;if(!SetNativeSourceAxis313(z,x,y)){Failed(b,"Prepared child313 actual axis placement failed after birth.");return nullptr;}return z;}
bool AdoptNativeExtendedZombie313Clocks(Zombie* z,std::span<const LawnNet::NetZombieAnimState> clocks,std::string* e)
{const auto* g=FindFusionSourceGraph313(Kind(z));if(!g||clocks.size()!=g->layerCount||!PrepareNativeExtendedZombie313Clocks(z,e))return false;for(uint8_t n=0;n<g->layerCount;++n){auto* r=Clock(z,n);if(!r||!LawnNet::NetZombieAnimStateFitsFrames(clocks[n],r->mDefinition->mTracks.tracks[0].mTransforms.count))return false;}for(uint8_t n=0;n<g->layerCount;++n){auto* r=Clock(z,n);const auto& c=clocks[n];r->mFrameStart=c.mFrameStart;r->mFrameCount=c.mFrameCount;r->mAnimTime=r->mLastFrameTime=c.mAnimTime;r->mAnimRate=c.mAnimRate;r->mLoopType=ReanimLoopType(c.mLoopType);r->mLoopCount=c.mLoopCount;if(!RegisterNativeSourceGraphClock313(r,true))return false;}return true;}
void RemoveNativeExtendedZombie313Bridge(Zombie* z)
{if(!z)return;ReleaseNativeSourceGraph313Clocks(z,GetNativeExtendedZombie313Hooks().graph);auto* c=GetNativeContext(z->mBoard);if(!c)return;const uint32_t uid=z->mBoard->mZombies.DataArrayGetID(z);c->extendedZombies313.erase(uid);c->sourceGraphs313.erase(uid);if(FusionGroundShooter343Kind(Kind(z))||FusionSnowZombie363Kind(Kind(z)))c->nextZombies343.erase(uid);}
}
