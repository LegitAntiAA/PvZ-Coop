#include "FusionNativeZombiePreflight280.h"
#include "FusionSourceZombieClosure280.h"
#include "FusionNative.h"
#include "FusionNativeAdventure273.h"
#include "FusionNativeZombies.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeDayZombie273Bridge.h"
#include "FusionNativeEffects269.h"
#include "FusionNativeEquipment.h"
#include "FusionNativeVisual.h"
#include "FusionNativeProjectileMetadata.h"
#include "FusionSourceZombieDeps269.h"
#include "FusionMoneyRules.h"
#include "FusionSummon269Rules.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include <cstring>

namespace LawnFusion
{
namespace
{
bool Reject(uint16_t kind,const std::string& why,std::string* error)
{if(error)*error="Source NPC dependencyKind="+std::to_string(kind)+": "+why;return false;}
bool TrackReady(ReanimationType type,const char* name)
{
    ReanimatorEnsureDefinitionLoaded(type,false);
    if(!gReanimatorDefArray||unsigned(type)>=gReanimatorDefCount)return false;
    const auto& tracks=gReanimatorDefArray[int(type)].mTracks;
    if(!tracks.tracks||tracks.count<=0)return false;
    for(int i=0;i<tracks.count;++i)if(tracks.tracks[i].mName&&stricmp(tracks.tracks[i].mName,name)==0)return true;
    return false;
}
bool ShotReady(Board* board,uint16_t kind,bool controlled,std::string& error)
{
    NativeProjectileData data;
    if(kind==104)
    {
        FusionSourcePaper269Shot shot;
        if(!TryMakeFusionSourcePaper269Shot(kind,6,false,false,controlled?1:0,controlled,shot))return false;
        data.kind=shot.kind;data.damage=shot.damage;data.specialType=shot.way;data.isZombieBullet=shot.hostile;
    }
    else
    {
        FusionDayZombie273Shot shot;
        if(!TryMakeFusionDayZombie273Shot(kind,0,false,false,controlled?1:0,controlled,shot))return false;
        data.kind=shot.kind;data.damage=shot.damage;data.specialType=shot.way;data.isZombieBullet=shot.hostile;
    }
    return NativeProjectileMetadataValid(data,PROJECTILE_PEA,data.specialType==9?MOTION_BACKWARDS:MOTION_STRAIGHT)&&
        EnsureNativeProjectileVisuals(board,data.kind,&error);
}
}
bool EnsureNativeSourceZombieClosure280(Board* board,std::span<const uint16_t> roots,std::string* error)
{
    if(error)error->clear();
    if(!board||!board->mApp||board->mApp->mBoard!=board||!IsNativeFusionBoard(board)||!GetNativeContext(board)||GetNativeContext(board)->adventureTeardown)
    {if(error)*error="Source NPC resource preflight requires the current native Fusion Board.";return false;}
    std::vector<uint16_t> closure;if(!BuildFusionSourceZombieClosure280(roots,closure,error))return false;
    for(auto kind:closure)
    {
        std::string detail;
        if(!EnsureNativeSourceZombieVisuals(board,kind,&detail))return Reject(kind,detail,error);
        if(!CanQueryNativeSourceZombieRoot273(board,kind))return Reject(kind,"actual loaded model root/state pose is unavailable",error);
        const auto effect=[&](uint16_t fx)
        {return EnsureNativeSourceEffect269(board,fx,&detail);};
        if(FusionDayZombie273Doll(kind)&&!effect(11))return Reject(kind,detail,error);
        if(kind==3&&(Sexy::SOUND_GRASSSTEP<0||Sexy::SOUND_POLEVAULT<0||!effect(23)))return Reject(kind,"Pole sound/effect23 dependency unavailable: "+detail,error);
        if(kind==100&&!effect(0))return Reject(kind,detail,error);
        if(kind==108&&!effect(18))return Reject(kind,detail,error);
        if((kind==14||kind==18)&&!effect(kind==14?35:36))return Reject(kind,detail,error);
        if((kind==106||kind==108)&&!CanCreateNativeEquipmentVisual(board,FusionEquipmentType::Bucket))
            return Reject(kind,"actual source Bucket item visual is unavailable",error);
        if(kind==100||kind==108||kind==104)
        {
            const auto body=GetZombieDefinition(static_cast<ZombieType>(GetNativeSourceZombieCarrier(kind))).mReanimationType;
            if(!TrackReady(body,"anim_head1")||!TrackReady(REANIM_PEASHOOTER,"anim_shooting"))
                return Reject(kind,"actual native head attachment/anim_shooting track is unavailable",error);
            if(!ShotReady(board,kind,false,detail)||!ShotReady(board,kind,true,detail))
                return Reject(kind,"actual hostile/friendly source projectile metadata/resource is unavailable: "+detail,error);
            if(kind==104&&!board->mApp->NetIsClient()&&GetNativeFusionRowCount(board)==5&&
                (board->mBackground==BACKGROUND_1_DAY||IsNativeSourceAdventureGrassWorld281(board)))
            {
                const uint16_t one[]={kind};
                if(!CanAllocateNativeSourceDayBatch274(board,one,&detail))return Reject(kind,detail,error);
            }
        }
        // Preserve the existing host controller/world/minimum allocation gate
        // before initial wave-plan RNG. Client resource probes do not simulate
        // the controller or claim host pool capacity.
        if(FindFusionDayZombie273Profile(kind)&&!board->mApp->NetIsClient()&&!CanSpawnNativeDayZombie273(board,kind,&detail))
            return Reject(kind,detail,error);
        if(IsFusionSummon269Money(kind))
        {
            int item=0,count=0,value=0;CoinType type;
            if(!TryGetFusionMoney269Reward(kind,item,count)||!TryGetFusionMoneyItem(uint16_t(item),type,value)||count<1)
                return Reject(kind,"actual native Money reward contract is unavailable",error);
        }
    }
    return true;
}
bool CanAllocateNativeSourceZombieBatch280(Board* board,std::span<const uint16_t> actualKinds,std::string* error)
{
    if(!EnsureNativeSourceZombieClosure280(board,actualKinds,error))return false;
    if(board->mApp->NetIsClient()||board->mPaused||board->mApp->mGameScene!=SCENE_PLAYING||!board->mTDPath.empty()||board->mGridItems.mSize)
    {if(error)*error="Source NPC batch requires the active host ordinary world without Tower/grid initializers.";return false;}
    std::vector<uint16_t> unique;if(!BuildFusionSourceZombieClosure280(actualKinds,unique,error))return false;
    for(auto kind:unique)
    {
        if(FindFusionZombieProfile(kind)->waterOnly)return Reject(kind,"water-only source NPC has no admitted ordinary Day row policy",error);
    }
    // Actual wave multiplicities, not the deduplicated dependency set. Includes
    //104's real2R/1A head. Future death callbacks retain their existing gates.
    return CanAllocateNativeSourceDayBatch274(board,actualKinds,error);
}
}
