#include "FusionQuickJackson367Rules.h"
#include "FusionNativeLadder355.h"
#include "FusionNativeDryGround353.h"
#include "FusionNativeDayZombie273Bridge.h"
#include "FusionNextZombie343Rules.h"
#include "FusionNativeAdventure273.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeSourceGraph313.h"
#include "FusionNativeZombie313Dispatch.h"
#include "FusionNativeHeavyZombie313BBridge.h"
#include "FusionNativeExtendedZombie313Bridge.h"
#include "FusionNativeWaterZombie313.h"
#include "FusionNativeNightZombie281.h"
#include "FusionNativeZombiePreflight280.h"
#include "FusionSourceZombieClosure280.h"
#include "FusionCampaign272Rules.h"
#include "FusionNative.h"
#include "FusionNativeBasePlants.h"
#include "FusionBasePlantRules.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeVisual.h"
#include "FusionNativeEquipment.h"
#include "FusionNativeEffects269.h"
#include "FusionNativeGarlic.h"
#include "FusionNativeRemainingPlants269.h"
#include "FusionNativeProjectileMetadata.h"
#include "FusionNativeWire.h"
#include "FusionNativeJackbox309Bridge.h"
#include "NativeSpecialZombie269.h"
#include "SpecialZombie269Rules.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Coin.h"
#include "../Net/NetCommon.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include <algorithm>
#include <cmath>

namespace LawnFusion
{
namespace
{
bool Current(const Board* b)
{return b&&b->mApp&&b->mApp->mBoard==b&&IsNativeFusionBoard(b)&&!GetNativeContext(b)->adventureTeardown;}
bool GroundController(uint16_t kind)
{
    const auto* profile=FindFusionZombieProfile(kind);
    if(!profile||profile->waterOnly||profile->noAttack)return false;
    if(FindFusionDayZombie273Profile(kind)||SpecialZombie269PaperKind(kind))return true;
    // Existing actual ordinary, Door, TallIceNut and Football mechanisms.
    // Jackson additionally requires the FNS27 source controller contract below.
    return kind==0||kind==2||kind==4||kind==8||kind==111||kind==9||kind==109||FusionJacksonFamily367Kind(kind)||FusionJackbox300Kind(kind);
}
bool Coexisting313Controller(uint16_t kind)
{return (GetNativeSourceZombieCarrier(kind)>=0&&FindSpecialZombie269Profile(kind))||
    FusionWaterZombie313Kind(kind)||FindFusionSourceGraph313(kind);}
bool Coexisting313Actor(Zombie* z)
{
    // Vehicle269 owns its native body clock rather than a313 graph/state
    // record. Validate that real controller when a Day actor joins its world.
    if(IsNativeSpecialZombie269Vehicle(z))
        return NativeSpecialZombie269VehicleTerrainValid(z)&&z->mApp->ReanimationTryToGet(z->mBodyReanimID);
    NativeZombie313Semantics semantics;
    if(!GetNativeZombie313Semantics(z,semantics))return false;
    if(FusionWaterZombie313Kind(semantics.kind))
        return NativeWaterZombie313WorldValid(z->mBoard,semantics.kind,z->mRow,nullptr,z->mMindControlled);
    const auto* graph=GetNativeSourceGraph313(z);
    const auto uid=z->mBoard->mZombies.DataArrayGetID(z);
    if(!graph||!graph->initialized||graph->failed||graph->localUID!=uid)return false;
    if(NativeHeavyZombie313BKind(semantics.kind))
    {
        const auto* state=GetNativeHeavyZombie313B(z);
        return state&&state->initialized&&!state->failed;
    }
    const auto* state=GetNativeExtendedZombie313(z);
    return state&&state->initialized&&!state->failed;
}
bool OrdinarySourceGrass(Board* b,uint16_t candidate,std::vector<uint16_t>& closure)
{
    // Common source world admits only real native ladders as grid items.
    if(!IsNativeSourceWorld313(b)||
        !GroundController(candidate))return false;
    std::vector<uint16_t> roots{candidate};Zombie* actor=nullptr;
    while(b->IterateZombies(actor))
    {
        if(actor->mDead)continue;
        const auto* source=GetNativeSourceZombieData(actor);
        const auto uid=b->mZombies.DataArrayGetID(actor);
        if(!IsNativeSourceZombie(actor)||!source||!uid||source->instanceToken!=uid||
            actor->mRow<0||actor->mRow>=GetNativeFusionRowCount(b))return false;
        const auto* profile=FindFusionZombieProfile(source->kind);
        const int carrier=GetNativeSourceZombieCarrier(source->kind);
        const bool baseWater=profile&&profile->waterOnly&&b->mPlantRow[actor->mRow]==PLANTROW_POOL&&
            (carrier==ZOMBIE_NORMAL||carrier==ZOMBIE_TRAFFIC_CONE||carrier==ZOMBIE_PAIL);
        if(GroundController(source->kind)?!NativeSourceDryRow353(b,actor->mRow):(!baseWater&&!Coexisting313Actor(actor)))return false;
        roots.push_back(source->kind);
    }
    // Keep the complete source child closure, including known independent
    // A/B/C controllers; these are never substituted with a Day controller.
    if(!BuildFusionSourceZombieClosure280(roots,closure))return false;
    for(auto kind:closure)
    {
        const auto* profile=FindFusionZombieProfile(kind);const int carrier=GetNativeSourceZombieCarrier(kind);
        const bool baseWater=profile&&profile->waterOnly&&
            (carrier==ZOMBIE_NORMAL||carrier==ZOMBIE_TRAFFIC_CONE||carrier==ZOMBIE_PAIL);
        if(!GroundController(kind)&&!Coexisting313Controller(kind)&&!baseWater)return false;
    }
    return true;
}
bool Host(const Zombie* z)
{return z&&Current(z->mBoard)&&!z->mNetGhost&&!z->mApp->NetIsClient()&&!z->mDead&&
    z->mApp->mGameScene==SCENE_PLAYING&&!z->mBoard->mPaused&&NativeSourceDryRow353(z->mBoard,z->mRow);}
bool PlantTraits(const Plant* p,FusionDayZombie273PlantTraits& result)
{
    const auto* d=GetNativePlantData(p);if(!d||!Current(p->mBoard))return false;
    bool flying=false,pumpkin=false,pot=false,lily=false,caltrop=false,nut=false;int level=0;
    if(!GetNativeRemaining269PlantTags(p,flying,pumpkin,pot,lily)||
        !TryGetSpecialZombie269SourcePlantTraits(d->kind,caltrop,nut,level))return false;
    FusionDayZombie273PlantTraits out;out.flying=flying;out.caltrop=caltrop;
    // TypeMgr.IsTallNut40D8C0, not the broader IsNut table or native HP.
    for(uint16_t kind:{243,903,913,921,1027,1028,1039,1073})if(d->kind==kind)out.tallNut=true;
    out.ladder=p->mBoard->GetLadderAt(p->mPlantCol,p->mRow)!=nullptr;
    if(!GetNativeBasePlantGroundAnchor(const_cast<Plant*>(p),out.shadowX,out.shadowY))return false;
    result=out;return true;
}
void PoleContact(Zombie* z,Plant* p)
{ContactNativeDayLadder355(z,p);}
bool FxReady(Board* b,uint16_t kind,std::string* error=nullptr)
{
    if(NativeSourceEffectParticle269(kind)<0)
    {if(error)*error="Source cosmetic kind has no reliable native dispatcher.";return false;}
    return EnsureNativeSourceEffect269(b,kind,error);
}
bool OrdinaryEveBalanced(const Board*)
{return false;} // Only the proved ordinary grass world can reach these hooks.
bool PrepareDanceChildren(Zombie* parent,std::span<const uint16_t> kinds,std::string* error)
{
    return Host(parent)&&NativeSourceDryRow353(parent->mBoard,parent->mRow)&&
        FxReady(parent->mBoard,11,error)&&CanAllocateNativeSourceZombieBatch280(parent->mBoard,kinds,error);
}
bool DanceChildEffect(Zombie* parent,Zombie* child,bool dance)
{
    const auto* source=GetNativeSourceZombieData(parent);const auto* born=GetNativeSourceZombieData(child);
    if(!Host(parent)||!child||child->mBoard!=parent->mBoard||!source||!born||
        (dance?(!FusionDancePole343Kind(source->kind)||born->kind!=7):(!FusionJacksonFamily367Kind(source->kind)||born->kind!=6)))return false;
    float x=0,y=0;if(!GetNativeSourceZombie269NodePoint(child,dance?"":"Shadow",x,y))return false;
    // The resource was validated before any RNG/birth. Native cosmetic pool
    // exhaustion is an acknowledged one-shot drop, never another child batch.
    const bool played=SpawnNativeSourceEffect269(parent->mBoard,11,x,
        y-float((dance?.5:.7)*GetNativeFusionPixelsY(parent->mBoard)),child->mRenderOrder+1,true)!=nullptr;
    if(!played)LawnNet::NetTrace("Fusion Dance284 cosmetic capacity drop parent=%u child=%u kind=11",parent->mNetId,child->mNetId);
    return true;
}
const NativeNightZombie281Hooks& DanceHooks()
{
    static const NativeNightZombie281Hooks hooks=[]
    {NativeNightZombie281Hooks value;value.eveBalanced=OrdinaryEveBalanced;
     value.prepareChildren=PrepareDanceChildren;value.childEffect=DanceChildEffect;return value;}();
    return hooks;
}
void PoleEvent(Zombie* z,uint8_t event)
{
    if(!Host(z))return;
    // LoadSound3A1330 slots50/51: grassstep / polevault. PlaySample already
    // publishes these native samples through reliable BOARDEV_SOUND once.
    if(event==Day273JumpSound1)z->mApp->PlaySample(Sexy::SOUND_GRASSSTEP);
    else if(event==Day273JumpSound2)z->mApp->PlaySample(Sexy::SOUND_POLEVAULT);
    else if(event==Day273JumpOver)
    {
        const auto* source=GetNativeSourceZombieData(z);auto* state=EditNativeDayZombie273(z);
        if(!source||!FusionDancePole343Kind(source->kind)||!state||state->danceSpawnPending||state->danceSpawnFailed)return;
        float x=0,y=0;if(!GetNativeSourceZombie269NodePoint(z,"Shadow",x,y))
        {
            state->danceSpawnFailed=true;
            if(auto* context=GetNativeContext(z->mBoard);context&&context->adventure.enabled)
            {context->adventure.schedulerFailed=true;context->adventureScheduler.failed=true;}
            LawnNet::NetTrace("Fusion Dance284 event anchor unavailable parent=%u token=%u",z->mNetId,source->instanceToken);
            return;
        }
        state->danceSpawnRow=z->mRow;state->danceSpawnShadowX=x;state->danceSpawnPending=true;
        RetryNativeDancePole284(z);
    }
    else if(event==Day273JumpFail)
    {
        float x=0,y=0;
        if(FxReady(z->mBoard,23)&&GetNativeSourceZombie269NodePoint(z,"Shadow",x,y))
            SpawnNativeSourceEffect269(z->mBoard,23,x,y-float(1.75*GetNativeFusionPixelsY(z->mBoard)),z->mRenderOrder+1,true);
    }
}
bool PrepareDoll(Zombie* z)
{
    const auto* source=GetNativeSourceZombieData(z);uint16_t child=0;
    if(!Host(z)||!source||!TryGetFusionDayZombie273DollChild(source->kind,child)||
        !FusionDayZombie273DollCanFall(z->mBoard->mZombies.mSize,z->mBoard->mZombies.mMaxSize))return false;
    // MoveSpawnedZombieToTargetRow3866B0 starts an8s MoveRow continuation.
    // The existing1s Garlic presentation is NOT an equivalent replacement.
    if(const auto* row=GetNativeZombieGarlicData(z);row&&row->phase==GarlicActionPhase::Moving)return false;
    std::string error;
    if(!EnsureNativeSourceZombieVisuals(z->mBoard,source->kind,&error)||!FxReady(z->mBoard,11,&error))return false;
    if(FindFusionDayZombie273Profile(child))return CanSpawnNativeDayZombie273(z->mBoard,child,&error);
    return FindFusionZombieProfile(child)&&GetNativeSourceZombieCarrier(child)>=0&&EnsureNativeSourceZombieVisuals(z->mBoard,child,&error);
}
void FinishDoll(Zombie* parent,Zombie* child)
{
    if(!Host(parent)||!child||child->mBoard!=parent->mBoard||child->mDead)return;
    // Prepared transaction: the real child exists and is registered before
    // parent retirement. ConeFall's child is synchronously culled by Die(2);
    // only a pinned ownerDestroyedBeforeRender model proof may admit this path.
    // No stock cone particle or alternate NPC artwork is manufactured here.
    parent->DieWithLoot();
}
bool ProjectileHit(Zombie* z,int sourceType,int damage,float knockback)
{
    if(!Host(z)||z->IsDeadOrDying()||damage<0||(sourceType!=0&&sourceType!=4)||!std::isfinite(knockback))return false;
    float x=0,y=0;double delta=0;
    if(knockback!=0)
    {
        // Zombie.Awake37F7F0 sets Axis=Shadow. KnockBack386370 uses the
        // signed operand and the zombieMaxX-2.2 right boundary only.
        if(!GetNativeSourceZombie269NodePoint(z,"Shadow",x,y)&&!GetNativeBaseZombieGroundAnchor(z,x,y))return false;
        const double px=kFusionBasePixelsX,origin=z->mBoard->GetCellCenterX(0,z->mRow);
        const double axis=(x-origin)/px-4.8;
        const double edge=1.35*(z->mBoard->GetColumnCount()-1)-4.8+4.0-2.2;
        const double next=axis>edge?axis:(std::min)(axis+double(knockback),edge);
        delta=(next-axis)*px;if(!std::isfinite(z->mPosX+delta))return false;
    }
    if(sourceType==4)
    {if(!ApplyNativeSpecialZombie269VehicleDamage4(z,damage))return false;}
    else z->TakeDamage(damage,0U);
    if(knockback!=0&&!z->mDead)
    {z->mPosX+=float(delta);z->mX=int(z->mPosX);}
    return true;
}
bool ProjectileContact(Projectile* p,Zombie* z,int type,int damage,float knockback)
{
    const auto* source=GetNativeProjectileData(p);
    return p&&source&&!p->mDead&&!p->mNetGhost&&z&&p->mBoard==z->mBoard&&source->isZombieBullet&&z->mMindControlled&&
        NativeDayZombie273ProjectileMetadataValid(*source,p->mProjectileType,p->mMotionType)&&ProjectileHit(z,type,damage,knockback);
}
}
NativeDayZombie273State* EditNativeDayZombie273(Zombie* zombie,bool create)
{
    auto* context=GetNativeContext(zombie?zombie->mBoard:nullptr);
    const auto* source=GetNativeSourceZombieData(zombie);
    if(!context||!source||!FindFusionDayZombie273Profile(source->kind))return nullptr;
    const auto generation=zombie->mBoard->mZombies.DataArrayGetID(zombie);
    if(!generation)return nullptr;
    auto found=context->dayZombies273.find(generation);
    if(found==context->dayZombies273.end())
    {
        if(!create)return nullptr;
        found=context->dayZombies273.emplace(generation,NativeDayZombie273State{}).first;
    }
    return &found->second;
}
const NativeDayZombie273State* GetNativeDayZombie273(const Zombie* zombie)
{return EditNativeDayZombie273(const_cast<Zombie*>(zombie),false);}

const NativeDayZombie273Hooks& GetNativeDayZombie273Hooks()
{
    static const NativeDayZombie273Hooks hooks{PlantTraits,PoleContact,PoleEvent,PrepareDoll,FinishDoll,ProjectileContact};
    return hooks;
}
bool PlayNativeNightChildEffect291(Zombie* parent,Zombie* child,bool dance)
{return DanceChildEffect(parent,child,dance);}
bool CanSpawnNativeGrassZombie284(Board* b,uint16_t kind,std::string* error)
{
    if(error)error->clear();
    const auto reject=[&](const char* reason){if(error)*error=reason;return false;};
    std::vector<uint16_t> closure;
    if(!Current(b)||b->mApp->mGameMode!=GAMEMODE_ADVENTURE||!OrdinarySourceGrass(b,kind,closure))
        return reject("Source family requires the genuine ordinary grass controller/dependency world.");
    if(kNativeFusionSnapshotVersion<26)return reject("Source controller state is absent from the native wire contract.");
    for(auto dependency:closure)
    {
        if(FusionJackbox300Kind(dependency)&&(kNativeFusionSnapshotVersion<28||
            !LawnNet::NetSourceJackbox309Ready(b->mApp)||!FxReady(b,42,error)))
            return reject("Jackbox requires FNS28, genuine per-owner sound and source effect42.");
        if(FusionJacksonFamily367Kind(dependency)&&(kNativeFusionSnapshotVersion<27||!LawnNet::NetSourceSound284Ready(b->mApp,69)||!FxReady(b,11,error)))
            return reject("Jackson requires FNS27, real Sound69 and source effect11.");
        if(!EnsureNativeSourceZombieVisuals(b,dependency,error))return false;
        if(!CanQueryNativeSourceZombieRoot273(b,dependency))
            return reject("Source family actual model/root/state dependency is unavailable.");
        if(FusionDayZombie273Pole(dependency)&&
            (Sexy::SOUND_GRASSSTEP<0||Sexy::SOUND_POLEVAULT<0||!FxReady(b,23,error)))return false;
        if((FusionDayZombie273Doll(dependency)||dependency==6)&&!FxReady(b,11,error))return false;
        if(dependency==100&&!FxReady(b,0,error))return false;
        if(dependency==101&&!FxReady(b,10,error))return false;
        if(dependency==102&&!FxReady(b,16,error))return false;
        if(dependency==108&&!FxReady(b,18,error))return false;
        if((dependency==106||dependency==108)&&!CanCreateNativeEquipmentVisual(b,FusionEquipmentType::Bucket))
            return reject("Actual source Bucket item resource is unavailable.");
        if(FusionDayZombie273Shooter(dependency))
        {
            FusionDayZombie273Shot sourceShot;if(!TryMakeFusionDayZombie273Shot(dependency,0,false,false,0,false,sourceShot))return false;
            NativeProjectileData shot;shot.kind=sourceShot.kind;shot.damage=sourceShot.damage;shot.specialType=sourceShot.way;shot.isZombieBullet=sourceShot.hostile;
            if(!NativeProjectileMetadataValid(shot,PROJECTILE_PEA,MOTION_BACKWARDS)||!EnsureNativeProjectileVisuals(b,shot.kind))
                return reject("Source hostile projectile family is not ready.");
        }
    }
    const uint16_t batch[]={kind};return CanAllocateNativeSourceDayBatch274(b,batch,error);
}
bool CanSpawnNativeDayZombie273(Board* b,uint16_t kind,std::string* error)
{
    if(!FindFusionDayZombie273Profile(kind)){if(error)*error="Source kind does not use the shared Day-family controller.";return false;}
    return CanSpawnNativeGrassZombie284(b,kind,error);
}
void RetryNativeDancePole284(Zombie* z)
{
    const auto* source=GetNativeSourceZombieData(z);auto* state=EditNativeDayZombie273(z);
    if(!source||!FusionDancePole343Kind(source->kind)||!state||!state->danceSpawnPending||state->danceSpawnFailed||!Host(z))return;
    if(z->IsDeadOrDying()||!z->mHasHead){state->danceSpawnPending=false;return;}
    std::string error;bool failed=false;
    const bool born=SpawnNativeDancePole284Children(z,state->danceSpawnRow,state->danceSpawnShadowX,DanceHooks(),failed,&error);
    if(born)state->danceSpawnPending=false;
    if(failed)
    {
        state->danceSpawnPending=false;state->danceSpawnFailed=true;
        if(auto* context=GetNativeContext(z->mBoard);context&&context->adventure.enabled)
        {context->adventure.schedulerFailed=true;context->adventureScheduler.failed=true;}
        LawnNet::NetTrace("Fusion Dance284 partial birth failed parent=%u token=%u reason=%s",z->mNetId,source->instanceToken,error.c_str());
    }
}
bool PrepareNativeDayZombie273ChomperArmor(Zombie* z,Coin*& born,int& oldType,int& oldHealth)
{
    born=nullptr;oldType=oldHealth=0;
    const auto* d=GetNativeSourceZombieData(z);
    // BucketNutZ.DropArmor33A890 is unconditional: the source item is still
    // born after ordinary armor depletion, not only while a native helm exists.
    if(!d||d->kind!=106)return true;
    if(!Host(z)||!CanCreateNativeEquipment(z->mBoard,FusionEquipmentType::Bucket))return false;
    born=DropNativeDayZombie273Bucket(z);if(!born)return false;
    oldType=int(z->mHelmType);oldHealth=z->mHelmHealth;
    z->mHelmHealth=0;z->mHelmType=HELMTYPE_NONE;return true;
}
void RollbackNativeDayZombie273ChomperArmor(Zombie* z,Coin* born,int oldType,int oldHealth)
{
    if(!born||!z||!Host(z)||z->mDead)return;
    ConsumeNativeEquipment(born);z->mHelmType=HelmType(oldType);z->mHelmHealth=oldHealth;
}
}
