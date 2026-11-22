#include "FusionNativeJackbox309Bridge.h"
#include "FusionNative.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeZombies.h"
#include "FusionNativeDayZombie273Bridge.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeZombiePreflight280.h"
#include "FusionNativeEffects269.h"
#include "FusionNativePlantDamage.h"
#include "FusionNativeWater268.h"
#include "FusionFireActionBatchRules.h"
#include "FusionBasePlantRules.h"
#include "FusionGarlicRules.h"
#include "FusionNativeWire.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Plant.h"
#include "../Net/NetPresentation.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace LawnFusion
{
namespace
{
bool Typed(const Zombie* z)
{const auto* d=GetNativeSourceZombieData(z);return d&&FusionJackbox300Kind(d->kind);}
bool World(const Board* b)
{return IsNativeSourceWorld313(b)&&GetNativeFusionRowCount(b)==5;}
bool Authority(const Zombie* z)
{return Typed(z)&&World(z->mBoard)&&!z->mNetGhost&&!z->mApp->NetIsClient();}
bool Playing(const Zombie* z)
{return Authority(z)&&!z->mDead&&!z->mBoard->mPaused&&z->mApp->mGameScene==SCENE_PLAYING&&CanApplyNativeFusionGameplay(z->mBoard);}
bool Reject(const char* why,std::string* error)
{if(error)*error=why;return false;}
void Failed(Board* b,const char* why)
{
    auto* c=GetNativeContext(b);if(!c)return;
    if(c->adventure.enabled){c->adventure.schedulerFailed=true;c->adventureScheduler.failed=true;}
    LawnNet::NetTrace("Fusion Jackbox309 stopped reason=%s tick=%d",why,b->mMainCounter);
}
bool ClockReady(uint16_t kind,std::string* error)
{
    if(!FusionJackbox300Kind(kind)||GetNativeSourceZombieCarrier(kind)!=ZOMBIE_NORMAL)
        return Reject("Jackbox requires the genuine NORMAL clock-only carrier.",error);
    const auto type=GetZombieDefinition(ZOMBIE_NORMAL).mReanimationType;
    ReanimatorEnsureDefinitionLoaded(type,false);
    if(!gReanimatorDefArray||unsigned(type)>=gReanimatorDefCount)
        return Reject("Jackbox native body definition unavailable.",error);
    const auto& tracks=gReanimatorDefArray[int(type)].mTracks;
    if(!tracks.tracks||tracks.count<=0)return Reject("Jackbox native body tracks unavailable.",error);
    for(uint8_t phase=0;phase<=uint8_t(FusionJackbox300Phase::Death);++phase)
    {
        FusionJackbox300Clip clip;if(!TryGetFusionJackbox300Clip(FusionJackbox300Phase(phase),clip))return false;
        bool found=false;
        for(int i=0;i<tracks.count;++i)if(tracks.tracks[i].mName&&
            stricmp(tracks.tracks[i].mName,clip.nativeTrack)==0&&tracks.tracks[i].mTransforms.count>0)found=true;
        if(!found)return Reject("Jackbox actual native phase track unavailable.",error);
    }
    return true;
}
bool Resources(Board* b,uint16_t kind,std::string* error)
{
    if(!World(b)||kNativeFusionSnapshotVersion<28)return Reject("Jackbox requires ordinary source grass and FNS28.",error);
    if(!ClockReady(kind,error)||!LawnNet::NetSourceJackbox309Ready(b->mApp))
        return Reject("Jackbox native clock/owner sound resources unavailable.",error);
    for(uint16_t sound:{90,91,43})if(!LawnNet::NetSourceSound284Ready(b->mApp,sound))
        return Reject("Jackbox reliable source sound unavailable.",error);
    if(NativeSourceEffectParticle269(42)<0||!EnsureNativeSourceEffect269(b,42,error))
        return Reject("Jackbox genuine explosion42 resources unavailable.",error);
    const uint16_t kinds[]={kind,24};
    for(auto dep:kinds)
        if(!EnsureNativeSourceZombieVisuals(b,dep,error)||!CanQueryNativeSourceZombieRoot273(b,dep))
            return Reject("Jackbox complete source model/root/part dependency unavailable.",error);
    return true;
}
bool Circle(Board* b,const NativeJackbox300Explosion& e,const Sexy::Rect& r)
{
    // Shared native source collision projection: preserve a source circle,
    // including the actual Board's Y spacing, rather than a pixel circle.
    const double ratio=kFusionBasePixelsY/GetNativeFusionPixelsY(b);
    return FusionFireCircleTouchesRect(e.centerX,e.centerY,2,r.mX,
        e.centerY+(r.mY-e.centerY)*ratio,r.mWidth,r.mHeight*ratio);
}
bool PlantRecipient(Plant* p,const NativeJackbox300Explosion& e)
{
    if(!p||p->mDead||p->mNetGhost||!NativeSourcePlantColliderEnabled(p)||std::abs(p->mRow-e.row)>1)return false;
    bool caltrop=false,nut=false;int level=0;
    return GetNativeSpecialZombie269PlantTraits(p,caltrop,nut,level)&&!caltrop&&Circle(p->mBoard,e,p->GetPlantRect());
}
bool ZombieRecipient(Zombie* z,const NativeJackbox300Explosion& e)
{return z&&!z->mDead&&!z->mNetGhost&&GetNativeSourceZombieData(z)&&
    z->mMindControlled!=e.controlled&&std::abs(z->mRow-e.row)<=1&&Circle(z->mBoard,e,z->GetZombieRect());}
struct Recipient
{uint32_t generation=0;uint16_t id=0;bool plant=false;};
bool Recipients(Board* b,const NativeJackbox300Explosion& e,std::vector<Recipient>& out,std::string* error)
{
    if(!World(b)||!std::isfinite(e.centerX+e.centerY+e.shadowX+e.shadowY)||e.row<0||e.row>=5||
        !e.ownerId||!e.ownerToken||!FusionJackbox300Kind(e.sourceKind))return Reject("Invalid Jackbox source explosion/world.",error);
    std::vector<Recipient> planned;
    if(!e.controlled)
    {
        Plant* p=nullptr;while(b->IteratePlants(p))
        {
            if(p->mDead||!p->mIsOnBoard)continue;
            bool caltrop=false,nut=false;int level=0;
            // Unknown source metadata is refused, never silently skipped.
            if(!GetNativePlantData(p)||!GetNativeSpecialZombie269PlantTraits(p,caltrop,nut,level))
                return Reject("Jackbox recipient lacks genuine source plant/collider tags.",error);
            if(PlantRecipient(p,e))planned.push_back({b->mPlants.DataArrayGetID(p),p->mNetId,true});
        }
    }
    Zombie* z=nullptr;while(b->IterateZombies(z))
    {
        if(z->mDead)continue;
        const auto* data=GetNativeSourceZombieData(z);
        if(!data)return Reject("Jackbox source explosion world contains an untyped NPC.",error);
        if(ZombieRecipient(z,e))
        {
            if(!CanApplyNativeJackbox309Charred(z,1800,error))return false;
            planned.push_back({b->mZombies.DataArrayGetID(z),z->mNetId,false});
        }
    }
    for(const auto& r:planned)if(!r.generation||!r.id)return Reject("Jackbox recipient has no full-generation identity.",error);
    out.swap(planned);return true;
}
bool Prepare(Zombie* z,NativeJackbox300Operation operation,const NativeJackbox300Explosion* explosion,
    bool potentialChild,std::string* error)
{
    if(!Authority(z)||!Resources(z->mBoard,GetNativeSourceZombieData(z)->kind,error))return false;
    std::vector<uint16_t> roots;Zombie* dependency=nullptr;
    while(z->mBoard->IterateZombies(dependency))
    {
        if(dependency->mDead)continue;
        const auto* d=GetNativeSourceZombieData(dependency);
        if(!d)return Reject("Jackbox callback world contains an untyped source dependency.",error);
        roots.push_back(d->kind);
    }
    if(!EnsureNativeSourceZombieClosure280(z->mBoard,roots,error))return false;
    if(explosion&&(operation==NativeJackbox300Operation::Explosion||potentialChild))
    {
        std::vector<Recipient> planned;
        if(!Recipients(z->mBoard,*explosion,planned,error))return false;
    }
    if(operation==NativeJackbox300Operation::Initialize||operation==NativeJackbox300Operation::Pop||
        operation==NativeJackbox300Operation::ArmorFall||
        (operation==NativeJackbox300Operation::Death&&!potentialChild))return true;
    // Conservative real child multiplicities for every still-live30 callback.
    // No parent retirement/dead-pending-cleanup credit. Common existing plant
    // callbacks retain their own readiness/capacity gates; this is not a new
    // claim that arbitrary recursive gameplay has rollback/atomic allocation.
    std::vector<uint16_t> children;bool currentIncluded=false;Zombie* actor=nullptr;
    while(z->mBoard->IterateZombies(actor))
    {
        const auto* d=GetNativeSourceZombieData(actor);const auto* s=GetNativeJackbox309(actor);
        if(!actor->mDead&&d&&d->kind==30&&s&&!s->deathEventHandled&&s->boxPresent&&s->countdown<s->originalCountdown)
        {children.push_back(24);if(actor==z)currentIncluded=true;}
    }
    if(potentialChild&&!currentIncluded)children.push_back(24);
    if(!children.empty()&&!CanAllocateNativeSourceDayBatch274(z->mBoard,children,error))return false;
    return true;
}
bool Explode(Board* b,const NativeJackbox300Explosion& e)
{
    if(!World(b)||b->mApp->NetIsClient())return false;
    std::vector<Recipient> targets;std::string error;
    if(!Recipients(b,e,targets,&error)){Failed(b,error.c_str());return false;}
    // Payload is standalone: normal24 has already retired. No owner lookup.
    LawnNet::NetPlaySourceSound284(b->mApp,43,.5f);
    SpawnNativeSourceEffect269(b,42,e.centerX,e.centerY,e.renderOrder+1,true);
    b->ShakeBoard(3,3); // Existing native board-shake presentation adaptation.
    LawnNet::NetTrace("Fusion Jackbox309 explosion id=%u token=%u kind=%u row=%d controlled=%d recipients=%zu sourceRadius=2 x=%.9g y=%.9g",e.ownerId,e.ownerToken,e.sourceKind,e.row,e.controlled?1:0,targets.size(),e.centerX,e.centerY);
    auto* app=b->mApp;
    for(const auto& target:targets)
    {
        if(app->mBoard!=b)return true;
        if(target.plant)
        {
            auto* p=b->mPlants.DataArrayTryToGet(target.generation);
            if(p&&p->mNetId==target.id&&PlantRecipient(p,e))ApplyNativeSourcePlantDamage(p,1000000,2);
        }
        else
        {
            auto* z=b->mZombies.DataArrayTryToGet(target.generation);
            if(z&&z->mNetId==target.id&&ZombieRecipient(z,e)&&!ApplyNativeJackbox309Charred(z,1800))
                return false;
        }
    }
    return true;
}
bool LoopAllowed(const Zombie* z)
{
    const auto* self=GetNativeJackbox309(z);if(!Authority(z)||!self||!self->boxPresent)return false;
    Zombie* other=nullptr;while(z->mBoard->IterateZombies(other))
    {
        // Source AvaliableToPlay scans actual other audioPlaying Jackboxes;
        // it does not add a head/status filter or globally stop their sound.
        if(other==z||other->mDead)continue;
        const auto* state=GetNativeJackbox309(other);if(state&&state->audioPlaying)return false;
    }
    return true;
}
void Audio(Zombie* z,NativeJackbox300Audio operation)
{if(Authority(z))LawnNet::NetSourceJackbox309Loop(z,operation);}
void Sound(Zombie* z,uint16_t kind,float volume)
{if(Authority(z))LawnNet::NetPlaySourceSound284(z->mApp,kind,volume);}
int RandomInt(Board*,int minimum,int exclusiveMaximum)
{return RandRangeInt(minimum,exclusiveMaximum-1);}
float RandomFloat(Board*,float minimum,float maximum)
{return RandRangeFloat(minimum,maximum);}
bool StartNextRow(Zombie* z,NativeJackbox300State& s)
{
    if(z->mRow==s.rowTarget){s.rowMoving=false;return true;}
    float rootX=0,rootY=0,x=0,y=0;
    if(!GetNativeSourceZombie269NodePoint(z,"",rootX,rootY)||!GetNativeSourceZombie269NodePoint(z,"Shadow",x,y))return false;
    s.rowNext=z->mRow+(s.rowTarget>z->mRow?1:-1);s.rowStartShadowY=y;
    s.rowTargetShadowY=y+float((s.rowNext-z->mRow)*1.67*GetNativeFusionPixelsY(z->mBoard));
    s.rowRootX=rootX;s.rowMoving=true;
    const float keepY=z->mPosY;
    z->SetRow(s.rowNext);z->mPosY=keepY;z->mY=int(keepY);
    NativeClockOnlyZombieClear309(z);
    return true;
}
bool MoveChild(Zombie* z,int target)
{
    auto* s=EditNativeJackbox309(z);
    if(!Authority(z)||!s||target<0||target>=5)return false;
    if(s->rowMoving)return true; // Source MoveSpawned returns when isMoving.
    s->rowTarget=target;return StartNextRow(z,*s);
}
bool Motion(Zombie* z,double delta)
{
    auto* s=EditNativeJackbox309(z);if(!Authority(z)||!s||!std::isfinite(delta)||!std::isfinite(z->mScaleZombie))return false;
    if(s->rowMoving)
    {
        if(z->IsDeadOrDying()){s->rowMoving=false;return true;}
        float rootX=0,rootY=0,shadowX=0,shadowY=0;
        if(!GetNativeSourceZombie269NodePoint(z,"",rootX,rootY)||!GetNativeSourceZombie269NodePoint(z,"Shadow",shadowX,shadowY))return false;
        const double left=s->rowTargetShadowY-shadowY;
        // Primary MoveRow uses |Animator.deltaPosition.x|, not a duration8
        // interpolation. Positive native Y is source-down, opposite Unity Y.
        const double step=std::abs(delta)*1.2*GetNativeFusionPixelsY(z->mBoard)*z->mScaleZombie;
        const double dy=(left<0?-1:1)*(std::min)(std::abs(left),step);
        const double dx=s->rowRootX-rootX;
        if(!std::isfinite(z->mPosX+dx)||!std::isfinite(z->mPosY+dy))return false;
        z->mPosX+=float(dx);z->mX=int(z->mPosX);z->mPosY+=float(dy);z->mY=int(z->mPosY);
        if(std::abs(left-dy)<=.01*GetNativeFusionPixelsY(z->mBoard))
        {
            z->mPosY+=float(left-dy);z->mY=int(z->mPosY);
            if(!StartNextRow(z,*s))return false;
        }
        return true;
    }
    // Declared native source-world projection of the genuine1.2 prefab root.
    // Source delta, native scale and allegiance reflection are applied once.
    const double dx=delta*1.2*kFusionBasePixelsX*z->mScaleZombie*(z->mMindControlled?-1:1);
    if(!std::isfinite(z->mPosX+dx))return false;
    z->mPosX+=float(dx);z->mX=int(z->mPosX);return true;
}
bool Input(Zombie* z,NativeJackbox300Input& out)
{
    const auto* d=GetNativeSourceZombieData(z);auto* s=EditNativeJackbox309(z);const auto* garlic=GetNativeZombieGarlicData(z);
    float factor=0;
    if(!Authority(z)||!d||!s||!TryGetFusionZombieStatusSpeedFactor(d->kind,z->mChilledCounter>0,z->mIceTrapCounter>0,
        z->mButteredCounter>0,garlic&&FusionGarlicStopsWalking(*garlic),factor))return false;
    NativeJackbox300Input value;value.sourceSpeed=value.animatorSpeed=d->originSpeed*factor*GetNativeWater268GrapSpeedFactor(z);
    value.sourcePlaying=Playing(z);value.beforeDying=d->beforeDying;
    value.targetRow=s->rowTarget;
    if(garlic&&garlic->phase==GarlicActionPhase::Moving)value.targetRow=garlic->targetRow;
    if(!z->IsDeadOrDying()&&!s->rowMoving)value.attacking=NativeClockOnlyZombieContact309(z,true);
    else NativeClockOnlyZombieClear309(z);
    out=value;return std::isfinite(value.sourceSpeed)&&value.sourceSpeed>=0;
}
void Retire(Zombie* z,int reason)
{
    if(!Authority(z)||z->mDead)return;
    // Capture and consume before source30's nested death explosion/child birth.
    const auto uid=z->mBoard->mZombies.DataArrayGetID(z);auto* b=z->mBoard;auto* app=z->mApp;
    const auto result=BeginNativeJackbox309Death(z,reason);
    if(result==NativeJackbox300DeathResult::Refused||result==NativeJackbox300DeathResult::OwnerRetired||app->mBoard!=b)return;
    z=b->mZombies.DataArrayTryToGet(uid);if(z&&!z->mDead){NativeJackbox309Removing(z);z->DieNoLoot();}
}
bool Armor(Zombie* z)
{return Authority(z)&&PlayNativeSourceZombieArmor284(z,true);}
}
NativeJackbox300State* EditNativeJackbox309(Zombie* z,bool create)
{
    auto* c=GetNativeContext(z?z->mBoard:nullptr);const auto* d=GetNativeSourceZombieData(z);
    if(!c||!d||!FusionJackbox300Kind(d->kind))return nullptr;
    const uint32_t uid=z->mBoard->mZombies.DataArrayGetID(z);const bool ghost=z->mNetGhost||z->mApp->NetIsClient();
    if(!uid||!d->instanceToken||(ghost&&create)||(!ghost&&d->instanceToken!=uid))return nullptr;
    auto it=c->jackboxZombies300.find(uid);
    if(it==c->jackboxZombies300.end())
    {if(!create)return nullptr;it=c->jackboxZombies300.emplace(uid,NativeJackbox300State{}).first;}
    return &it->second;
}
const NativeJackbox300State* GetNativeJackbox309(const Zombie* z)
{return EditNativeJackbox309(const_cast<Zombie*>(z));}
bool CanSpawnNativeJackbox309(Board* b,uint16_t kind,std::string* error)
{
    if(error)error->clear();
    return FusionJackbox300Kind(kind)&&Resources(b,kind,error)&&CanSpawnNativeGrassZombie284(b,kind,error);
}
const NativeJackbox300Hooks& GetNativeJackbox309Hooks()
{
    static const NativeJackbox300Hooks hooks=[]
    {
        NativeJackbox300Hooks h;h.stateFor=[](Zombie* z){return EditNativeJackbox309(z);};h.input=Input;
        h.randomInt=RandomInt;h.randomFloat=RandomFloat;h.preflight=Prepare;h.canPlayLoop=LoopAllowed;
        h.audio=Audio;h.sound=Sound;h.bite=NativeClockOnlyZombieBite309;h.moveRootDelta=Motion;
        h.explode=Explode;h.retire=Retire;h.moveChildToTargetRow=MoveChild;h.armorFall=Armor;h.failed=Failed;return h;
    }();return hooks;
}
bool InitializeNativeJackbox309(Zombie* z)
{
    if(!Authority(z))return false;auto* s=EditNativeJackbox309(z,true);if(!s)return false;
    if(s->initialized)return !s->failed&&FusionJackbox300ViewValid(GetNativeSourceZombieData(z)->kind,*s);
    const bool okay=InitializeNativeJackbox300(z,*s,GetNativeJackbox309Hooks());
    if(!okay){s->failed=true;Failed(z->mBoard,"source initialization/body clock");}return okay;
}
bool UpdateNativeJackbox309(Zombie* z)
{
    if(!Typed(z))return false;if(!Playing(z))return true;
    auto* s=EditNativeJackbox309(z);if(!s){Failed(z->mBoard,"missing full-generation controller");return true;}
    auto* b=z->mBoard;auto* app=z->mApp;const auto uid=b->mZombies.DataArrayGetID(z);
    const bool okay=UpdateNativeJackbox300(z,*s,GetNativeJackbox309Hooks());
    if(app->mBoard!=b)return true;z=b->mZombies.DataArrayTryToGet(uid);
    if(!z||z->mDead)return true;s=EditNativeJackbox309(z);
    if(!okay||!s||s->failed){if(s)s->failed=true;Failed(b,"source controller step");}return true;
}
bool UpdateNativeJackbox309Animation(Zombie* z)
{if(!Typed(z))return false;if(Authority(z))if(const auto* s=GetNativeJackbox309(z))UpdateNativeJackbox300Animation(z,*s,GetNativeJackbox309Hooks());return true;}
bool StartNativeJackbox309Walk(Zombie* z){return UpdateNativeJackbox309Animation(z);}
bool MoveNativeJackbox309(Zombie* z){return Typed(z);} // Core already consumed real source motion.
bool NativeJackbox309RowMoving(const Zombie* z)
{const auto* s=GetNativeJackbox309(z);return s&&s->rowMoving;}
NativeJackbox300DeathResult BeginNativeJackbox309Death(Zombie* z,int reason)
{
    if(!Typed(z))return NativeJackbox300DeathResult::NotHandled;
    if(!Authority(z))return NativeJackbox300DeathResult::Proceed;
    auto* s=EditNativeJackbox309(z);if(!s)return NativeJackbox300DeathResult::Refused;
    auto* b=z->mBoard;auto* app=z->mApp;const auto uid=b->mZombies.DataArrayGetID(z);
    const auto result=BeginNativeJackbox300Death(z,*s,reason,GetNativeJackbox309Hooks());
    if(app->mBoard!=b)return NativeJackbox300DeathResult::OwnerRetired;
    if(result==NativeJackbox300DeathResult::Refused)Failed(b,"source death callback before native reset");
    z=b->mZombies.DataArrayTryToGet(uid);
    if(!z||z->mDead)return NativeJackbox300DeathResult::OwnerRetired;
    if(result==NativeJackbox300DeathResult::Proceed)NativeClockOnlyZombieClear309(z);
    return result;
}
bool NativeJackbox309AudioStatus(Zombie* z,bool pause)
{auto* s=EditNativeJackbox309(z);return s&&NativeJackbox300AudioStatus(z,*s,pause,GetNativeJackbox309Hooks());}
bool NativeJackbox309LoseHead(Zombie* z)
{auto* s=EditNativeJackbox309(z);return s&&NativeJackbox300LoseHead(z,*s,GetNativeJackbox309Hooks());}
bool NativeJackbox309FirstArmorFall(Zombie* z)
{auto* s=EditNativeJackbox309(z);return s&&NativeJackbox300FirstArmorFall(z,*s,GetNativeJackbox309Hooks());}
bool ApplyNativeJackbox309Armor(Zombie* z,int damage,int& remaining)
{
    auto* d=GetNativeSourceZombieData(z);if(!d||d->kind!=30)return false;
    remaining=0;if(!Authority(z)||damage<0)return true;
    const int hp=z->mHelmHealth;
    if(hp<=0){remaining=damage;return true;}
    const int after=damage>=hp?0:hp-damage;uint8_t stage=0;int sprite=-1;
    if(!TryGetFusionJackbox300ArmorStage(30,after,z->mHelmMaxHealth,d->firstArmorStage,stage,sprite))return true;
    if(after==0&&!NativeJackbox309FirstArmorFall(z))return true;
    z->mHelmHealth=after;d->firstArmorStage=stage;
    if(after==0)z->mHelmType=HELMTYPE_NONE;
    remaining=damage>=hp?damage-hp:0;return true;
}
bool FilterNativeJackbox309Damage(const Zombie* z,int incoming,int& filtered)
{
    if(!Typed(z))return false;const auto* s=GetNativeJackbox309(z);const auto* d=GetNativeSourceZombieData(z);
    if(!s||!TryFilterFusionJackbox300Damage(d->kind,*s,incoming,filtered))filtered=0;
    return true;
}
bool CanDisarmNativeJackbox309(const Zombie* z)
{
    const auto* s=GetNativeJackbox309(z);const auto* d=GetNativeSourceZombieData(z);FusionJackbox300View result;
    return Authority(z)&&s&&s->initialized&&!s->failed&&TryDisarmFusionJackbox300(d->kind,*s,result);
}
bool CommitNativeJackbox309Disarm(Zombie* z)
{auto* s=EditNativeJackbox309(z);return s&&CommitNativeJackbox300Disarm(z,*s,GetNativeJackbox309Hooks());}
void NativeJackbox309Removing(Zombie* z)
{if(Typed(z))LawnNet::NetSourceJackbox309Retire(z);}
bool GetNativeJackbox309Presentation(const Zombie* z,const char*& state,float& normalized)
{const auto* s=GetNativeJackbox309(z);return s&&GetNativeJackbox300Presentation(z,*s,state,normalized);}
}
