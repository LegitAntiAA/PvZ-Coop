#include "FusionNativeDayZombie273Bridge.h"
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
#include "NativeSpecialZombie269.h"
#include "SpecialZombie269Rules.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Coin.h"
#include "../Net/NetCommon.h"
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
bool Host(const Zombie* z)
{return z&&Current(z->mBoard)&&!z->mNetGhost&&!z->mApp->NetIsClient()&&!z->mDead&&
    z->mApp->mGameScene==SCENE_PLAYING&&!z->mBoard->mPaused;}
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
void PoleContact(Zombie*,Plant*)
{
    // OnTriggerStay2D36F390 dispatches +0x2a8 = slot23 GetLadder385A40,
    // NOT slot48 AttackEffect37F310. Contact deals no damage. Source3 stays
    // gated until its actual ladder timer/coordinate continuation is mapped.
}
bool FxReady(Board* b,uint16_t kind,std::string* error=nullptr)
{
    if(NativeSourceEffectParticle269(kind)<0)
    {if(error)*error="Source cosmetic kind has no reliable native dispatcher.";return false;}
    return EnsureNativeSourceEffect269(b,kind,error);
}
void PoleEvent(Zombie* z,uint8_t event)
{
    if(!Host(z))return;
    // LoadSound3A1330 slots50/51: grassstep / polevault. PlaySample already
    // publishes these native samples through reliable BOARDEV_SOUND once.
    if(event==Day273JumpSound1)z->mApp->PlaySample(Sexy::SOUND_GRASSSTEP);
    else if(event==Day273JumpSound2)z->mApp->PlaySample(Sexy::SOUND_POLEVAULT);
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
bool CanSpawnNativeDayZombie273(Board* b,uint16_t kind,std::string* error)
{
    if(error)error->clear();
    const auto reject=[&](const char* reason){if(error)*error=reason;return false;};
    if(!Current(b)||!FindFusionDayZombie273Profile(kind)||b->mApp->mGameMode!=GAMEMODE_ADVENTURE)
        return reject("Day family requires its bounded native Adventure world.");
    // Root/E1: raw ghost commit cannot yet reserve all body/head/Attachment
    // allocations transactionally. Never admit a host that its peer cannot adopt.
    if(b->mApp->NetActive())return reject("Day network admission awaits shared native body/head allocation reservation.");
    if(kind==3)return reject("Pole contact awaits the source GetLadder timer and coordinate continuation.");
    if(FusionDayZombie273Doll(kind))return reject("Doll family awaits complete8s spawned target-row continuation and pinned lifecycle proof.");
    if(kNativeFusionSnapshotVersion<26)return reject("Day source state is absent from the native wire contract.");
    if(!EnsureNativeSourceZombieVisuals(b,kind,error))return false;
    if(kind==3&&(Sexy::SOUND_GRASSSTEP<0||Sexy::SOUND_POLEVAULT<0||!FxReady(b,23,error)))return false;
    if(kind==100&&!FxReady(b,0,error))return false;
    if(kind==108&&!FxReady(b,18,error))return false;
    if((kind==106||kind==108)&&!CanCreateNativeEquipmentVisual(b,FusionEquipmentType::Bucket))return reject("Actual source Bucket item resource is unavailable.");
    if(kind==100||kind==108)
    {
        NativeProjectileData shot;shot.kind=kind==100?0:11;shot.damage=kind==100?20:80;shot.specialType=9;shot.isZombieBullet=true;
        if(!NativeProjectileMetadataValid(shot,PROJECTILE_PEA,MOTION_BACKWARDS)||!EnsureNativeProjectileVisuals(b,shot.kind))
            return reject("Day hostile source projectile family is not ready.");
    }
    return true;
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
