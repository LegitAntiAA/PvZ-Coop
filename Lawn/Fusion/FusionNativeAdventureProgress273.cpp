#include "FusionNativeAdventureProgress273.h"
#include "FusionNativeAdventure273.h"
#include "FusionNative.h"
#include "FusionNativeVisual.h"
#include "FusionNativeGlove.h"
#include "FusionNativeEquipment268.h"
#include "FusionNativeDroppedCards269.h"
#include "FusionNativeZombies.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeDayZombie273Bridge.h"
#include "FusionNativeProjectileMetadata.h"
#include "FusionNativePlacement.h"
#include "FusionNativePlantDamage.h"
#include "FusionBasePlantRules.h"
#include "../Board.h"
#include "../Coin.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../MessageWidget.h"
#include "../System/Music.h"
#include "../Net/NetManager.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../../Sexy.TodLib/Attachment.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "../../Sexy.TodLib/TodParticle.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <span>
#include <vector>

namespace LawnFusion
{
namespace
{
bool Current(const Board* b)
{return b&&b->mApp&&b->mApp->mBoard==b&&IsNativeFusionBoard(b);}
bool Host(const Board* b){return Current(b)&&!b->mApp->NetIsClient();}
bool Playing(const Board* b)
{return Host(b)&&b->mApp->mGameScene==SCENE_PLAYING&&!b->mPaused&&!b->mApp->GetDialogCount();}
bool Actor(const Plant* p)
{return p&&!p->mDead&&!p->mNetGhost&&p->mIsOnBoard&&!p->mSquished&&p->mPlantHealth>0;}
SexyString Text(FusionAdventureMessage273 message)
{
    switch(message)
    {
    case FusionAdventureMessage273::Welcome:return _S("Добро пожаловать в PvZ Fusion!");
    case FusionAdventureMessage273::NewPlants:return _S("Здесь вас ждут новые растения и способы игры.");
    case FusionAdventureMessage273::Resources:return _S("Начнём с получения ресурсов!");
    case FusionAdventureMessage273::PlantSunflower:return _S("Посадите подсолнух.");
    case FusionAdventureMessage273::FusePeaSunflower:return _S("Подсолнух даёт солнце. Посадите на него горохострел.");
    case FusionAdventureMessage273::BuildGatling:return _S("Сажайте горох на горох, чтобы получить стрелка Гатлинга.");
    case FusionAdventureMessage273::Ready:return _S("Теперь высаживайте растения и защищайтесь от зомби!");
    case FusionAdventureMessage273::GloveUnlocked:return _S("Вы открыли перчатку — важный инструмент.");
    case FusionAdventureMessage273::GloveMove:return _S("Перчатка перемещает растения и позволяет объединять их.");
    case FusionAdventureMessage273::MoveTwinFlower:return _S("Переместите двойной подсолнух за орех.");
    case FusionAdventureMessage273::GloveReady:return _S("Вы освоили перчатку. Готовьте оборону от зомби!");
    case FusionAdventureMessage273::PresentIntro:return _S("Подарок может дать любое базовое растение.");
    case FusionAdventureMessage273::UsePresent:return _S("Положите подарок на этого повторителя.");
    case FusionAdventureMessage273::PresentReady:return _S("Вы освоили подарок. Продолжим!");
    case FusionAdventureMessage273::CherryPaperWarning:return _S("Вишнёвый газетчик на дорожке 1! Поставьте вишнёвый орех.");
    case FusionAdventureMessage273::PresentDefenseReady:return _S("Отлично! Высаживайте растения и защищайтесь от зомби!");
    case FusionAdventureMessage273::EquipmentIntro:return _S("Можно ли надеть зомбиное снаряжение на растение?");
    case FusionAdventureMessage273::BucketNutWarning:return _S("Убейте орехозомби с ведром на дорожке 1. Без вишни и пожирателя!");
    case FusionAdventureMessage273::UseBucket:return _S("Подбери выпавшее ведро и надень на горохострел или орех");
    case FusionAdventureMessage273::IronPlants:return _S("Железные растения — лучшие друзья ранней игры!");
    case FusionAdventureMessage273::EquipmentReady:return _S("Высаживайте растения и защищайтесь от зомби!");
    // Existing translated source literal IDs7422/6286/7630 (zero-based).
    case FusionAdventureMessage273::PuffIntro:return _S("Малый Пыхтун очень важен ночью (туториал)");
    case FusionAdventureMessage273::PuffStack:return _S("В одну клетку помещаются 3 Малых Пыхтуна");
    case FusionAdventureMessage273::PuffMix:return _S("Фьюжн внутрь «поселения» пыхтунов даёт сразу тройной фьюжн");
    case FusionAdventureMessage273::PuffReady:return _S("Высаживайте растения и защищайтесь от зомби!");
    // Source13/15 literals; preserve existing translations where present.
    case FusionAdventureMessage273::FertilizeIntro:return _S("Вы открыли удобрение. Оно периодически появляется в левом верхнем углу поля.");
    case FusionAdventureMessage273::FertilizeGrow:return _S("Удобрение мгновенно выращивает солнечный гриб.");
    case FusionAdventureMessage273::FertilizeUpgrade:return _S("Хищник может сглотнуть немедленно");
    case FusionAdventureMessage273::FertilizeLesson:return _S("Удобрение всегда переводит растение на следующую стадию");
    case FusionAdventureMessage273::UseFertilizeNut:return _S("Попробуйте применить удобрение к этому ореху.");
    case FusionAdventureMessage273::FertilizeNutReady:return _S("Удобрение работает как фиолетовая карта — улучшает растение");
    case FusionAdventureMessage273::RepairNutChomper:return _S("Теперь примените удобрение к этому ореху-пожирателю.");
    case FusionAdventureMessage273::FertilizeRepairReady:return _S("Удобрение полностью восстанавливает здоровье растения.");
    case FusionAdventureMessage273::FertilizeReady:case FusionAdventureMessage273::IceReady:return _S("Высаживайте растения и защищайтесь от зомби!");
    case FusionAdventureMessage273::IceIntro:return _S("Познакомимся с новой механикой ледяных растений — шкалой заморозки.");
    case FusionAdventureMessage273::IceDefense:return _S("Ледяные атаки заполняют шкалу заморозки. Полная шкала замораживает зомби и обнуляется. Замороженный зомби получает вчетверо больше урона от ледяных снарядов.");
    case FusionAdventureMessage273::IceLessonReady:return _S("Предел заморозки растёт с каждым замораживанием");
    default:return _S("");
    }
}
bool MissionCards(Board* b)
{
    if(b->mCoins.mMaxSize-b->mCoins.mSize<3||!CanCreateNativeDroppedCard269(b,0,0))return false;
    int count=0;Coin* existing=nullptr;
    while(b->IterateCoins(existing))if(!existing->mDead&&GetNativeDroppedCard269Data(existing))++count;
    if(count>125)return false;
    std::array<Coin*,3> cards{};
    for(int i=0;i<3;++i)
    {
        const int column=4+i,row=2;
        cards[i]=CreateNativeDroppedCard269(b,0,0,float(b->GetCellCenterX(column,row)),
            float(b->GridToPixelY(column,row)+50));
        if(!cards[i])
        {
            for(auto* card:cards)if(card)card->Die();
            return false;
        }
    }
    return true;
}
bool Lesson4OldKind(uint16_t kind)
{
    // Audited removal-only source4 closure. In particular SunNut1006 has
    // producer death rewards, and CherryNut1003 has AoE; neither may observe
    // the prepared children before its original Clear->spawn source order.
    switch(kind){case 0:case 1:case 2:case 3:case 1000:case 1030:case 1032:case 1033:return true;default:return false;}
}
bool PrepareLesson4(Board* b,uint32_t& mission)
{
    const auto reject=[&](const char* why)
    {if(b->mMainCounter%100==0)LawnNet::NetTrace("Fusion source Tutor4 Prepare blocked reason=%s tick=%d",why,b->mMainCounter);return false;};
    if(!CanApplyNativeFusionGameplay(b)||GetNativeContext(b)->selection.State().phase!=FusionBankPhase::Playing||
        b->mApp->mGameMode!=GAMEMODE_ADVENTURE||b->mBackground!=BACKGROUND_1_DAY||GetNativeFusionRowCount(b)!=5||
        !b->IsValidGridCell(9,2)||!b->IsValidGridCell(5,2)||b->mPlantRow[2]!=PLANTROW_NORMAL||
        b->mApp->IsWallnutBowlingLevel())return reject("uncovered native initializer/terrain");
    std::vector<uint32_t> old;
    // Source ClearPlant9,2 then5,2 visits every existing cell layer. Capture
    // exact generations BEFORE factory allocation; never clear by cell after
    // birth, when the two new default actors occupy those same cells.
    for(int column:{9,5})
    {
        Plant* p=nullptr;
        while(b->IteratePlants(p))if(!p->mDead&&p->mIsOnBoard&&p->mRow==2&&
            (p->mPlantCol==column||(p->mSeedType==SEED_COBCANNON&&p->mPlantCol+1==column)))
        {
            const auto* d=GetNativePlantData(p);const uint32_t uid=b->mPlants.DataArrayGetID(p);
            if(p->mNetGhost||!d||d->instanceToken!=uid||!Lesson4OldKind(d->kind)||
                p->mSeedType!=GetNativeSeedForPlant(d->kind)||IsNativeGlovePlantReserved(b,p,-1))
                return reject("uncovered cell occupant/removal callback or glove lease");
            old.push_back(uid);
        }
    }
    constexpr uint16_t kinds[]={1033,1003};
    const SeedType seeds[]={SEED_TWINSUNFLOWER,SEED_WALLNUT};
    const ReanimationType types[]={REANIM_TWIN_SUNFLOWER,REANIM_WALLNUT};
    for(int i=0;i<2;++i)
    {
        const auto* definition=FusionFindPlant(kinds[i]);
        if(GetNativeSeedForPlant(kinds[i])!=seeds[i]||!definition||!definition->hasStats||definition->maxHealth<=0||
            GetPlantDefinition(seeds[i]).mReanimationType!=types[i]||!EnsureNativePlantVisuals(b,kinds[i]))
            return reject("source model/stats/native carrier unavailable");
        ReanimatorEnsureDefinitionLoaded(types[i],false);
        if(!gReanimatorDefArray||unsigned(types[i])>=gReanimatorDefCount)return reject("native definition unavailable");
        const auto& tracks=gReanimatorDefArray[int(types[i])].mTracks;
        if(!tracks.tracks||tracks.count<=0)return reject("native definition has no tracks");
        for(int track=0;track<tracks.count;++track)
            if(tracks.tracks[track].mName&&strnicmp(tracks.tracks[track].mName,"attacher__",10)==0)
                return reject("native definition has an unbudgeted dynamic attacher");
    }
    auto* effects=b->mApp->mEffectSystem;
    if(!effects||effects!=gEffectSystem||!effects->mReanimationHolder||!effects->mAttachmentHolder)
        return reject("native effect pools unavailable");
    const auto& p=b->mPlants;const auto& r=effects->mReanimationHolder->mReanimations;
    const auto& a=effects->mAttachmentHolder->mAttachments;
    // Exact ordinary Day initializer: one TwinSunflower body +one WallNut
    // body, no seed-specific extra heads/sleep/blink/attachments. No slots
    // from future removals or already-dead actors are credited to this peak.
    if(!p.mBlock||!r.mBlock||!a.mBlock||p.mSize>p.mMaxSize||r.mSize>r.mMaxSize||a.mSize>a.mMaxSize||
        p.mMaxSize-p.mSize<2||r.mMaxSize-r.mSize<2)return reject("whole batch needs2 live Plant/2 Reanimation slots");
    std::array<Plant*,2> made{};
    const auto valid=[&](Plant* plant,int index)
    {
        const auto* data=GetNativePlantData(plant);const auto* definition=FusionFindPlant(kinds[index]);
        return plant&&!plant->mDead&&!plant->mNetGhost&&plant->mNetId&&data&&data->kind==kinds[index]&&
            data->instanceToken&&b->mPlants.DataArrayTryToGet(data->instanceToken)==plant&&
            plant->mSeedType==seeds[index]&&plant->mPlantCol==(index?5:9)&&plant->mRow==2&&plant->mIsOnBoard&&
            data->firstParent==kFusionNoPlant&&data->secondParent==kFusionNoPlant&&!data->packetId&&
            data->owner==LawnNet::NET_SHARED_OWNER&&!data->randomOrigin&&
            plant->mPlantHealth==definition->maxHealth&&plant->mPlantMaxHealth==definition->maxHealth;
    };
    const auto retire=[&](){for(auto* plant:made)if(plant&&!plant->mDead)plant->Die(255);};
    for(int i=0;i<2;++i)
    {
        NativePlantData fresh;fresh.kind=kinds[i];fresh.owner=LawnNet::NET_SHARED_OWNER;
        made[i]=b->AddPlant(i?5:9,2,seeds[i],SEED_NONE,&fresh);
        if(!valid(made[i],i)){retire();return reject("default factory binding failed before clear");}
    }
    for(uint32_t uid:old)if(auto* plant=b->mPlants.DataArrayTryToGet(uid);plant&&!plant->mDead)plant->Die(0);
    // The accepted old kind set cannot damage/spawn on these children. Keep
    // source Die0 callbacks; unsupported effectful callbacks refused above.
    if(!valid(made[0],0)||!valid(made[1],1))
    {
        LawnNet::NetTrace("Fusion source Tutor4 invariant FAILED after source Die0; old removal already committed tick=%d",b->mMainCounter);
        retire();return false;
    }
    mission=b->mPlants.DataArrayGetID(made[0]);RebuildNativeEquipmentGraph(b);
    LawnNet::NetTrace("Fusion source Tutor4 Prepare committed mission=%u uid=%u old=%zu hp=%d/%d sharedDefault=1 peakP=2 peakR=2 peakA=0 tick=%d",
        made[0]->mNetId,mission,old.size(),made[0]->mPlantHealth,made[1]->mPlantHealth,b->mMainCounter);
    return true;
}
bool LessonWorld278(Board* b,int level,std::string* error)
{
    const auto* context=GetNativeContext(b);
    if(!Host(b)||!context||!context->adventure.enabled||context->adventure.run.level!=level||
        context->adventure.run.cycle!=1||(level!=7&&level!=8)||b->mApp->mGameMode!=GAMEMODE_ADVENTURE||
        b->mBackground!=BACKGROUND_1_DAY||b->GetColumnCount()!=10||GetNativeFusionRowCount(b)!=5||
        b->mApp->IsWallnutBowlingLevel())
    {if(error)*error="Source Tutor7/8 requires its ordinary cycle1 Day Board.";return false;}
    for(int row=0;row<5;++row)if(b->mPlantRow[row]!=PLANTROW_NORMAL)
    {if(error)*error="Source Tutor7/8 row initializer differs.";return false;}
    return true;
}
bool RepeaterPeak278(Board* b,std::string* error)
{
    const auto reject=[&](const char* why){if(error)*error=why;return false;};
    const auto* definition=FusionFindPlant(1030);
    if(!definition||!definition->hasStats||definition->maxHealth<=0||GetNativeSeedForPlant(1030)!=SEED_PEASHOOTER||
        GetPlantDefinition(SEED_PEASHOOTER).mReanimationType!=REANIM_PEASHOOTER||
        !EnsureNativePlantVisuals(b,1030,error))return reject("Source1030 model/stats/native carrier unavailable.");
    ReanimatorEnsureDefinitionLoaded(REANIM_PEASHOOTER,false);
    if(!gReanimatorDefArray||unsigned(REANIM_PEASHOOTER)>=gReanimatorDefCount)
        return reject("Source1030 native definition unavailable.");
    const auto& tracks=gReanimatorDefArray[int(REANIM_PEASHOOTER)].mTracks;
    if(!tracks.tracks||tracks.count<=0)return reject("Source1030 native definition has no tracks.");
    for(int i=0;i<tracks.count;++i)if(tracks.tracks[i].mName&&strnicmp(tracks.tracks[i].mName,"attacher__",10)==0)
        return reject("Source1030 native definition has an unbudgeted dynamic attacher.");
    auto* effects=b->mApp->mEffectSystem;
    if(!effects||effects!=gEffectSystem||!effects->mReanimationHolder||!effects->mAttachmentHolder)
        return reject("Source1030 native effect pools unavailable.");
    const auto& p=b->mPlants;const auto& r=effects->mReanimationHolder->mReanimations;
    const auto& a=effects->mAttachmentHolder->mAttachments;
    // Pea carrier initializes one body, one head and one attachment, before
    // the source metadata adapts it. No dead/removed slots count as free.
    if(!p.mBlock||!r.mBlock||!a.mBlock||p.mSize>p.mMaxSize||r.mSize>r.mMaxSize||a.mSize>a.mMaxSize||
        p.mMaxSize-p.mSize<1||r.mMaxSize-r.mSize<2||a.mMaxSize-a.mSize<1)
        return reject("Source1030 whole batch needs1 Plant/2 Reanimation/1 Attachment slots.");
    return true;
}
bool MissionBatchReady278(Board* b,int level,std::string* error)
{
    const auto reject=[&](const char* why){if(error)*error=why;return false;};
    const uint16_t kind=level==7?104:106;
    if(!EnsureNativeSourceZombieVisuals(b,kind,error))return false;
    if(!CanQueryNativeSourceZombieRoot273(b,kind))return reject("Source mission NPC root is unavailable.");
    if(kind==106&&!CanSpawnNativeDayZombie273(b,kind,error))return false;
    if(kind==104)
    {
        NativeProjectileData shot;shot.kind=3;shot.damage=300;shot.specialType=9;shot.isZombieBullet=true;
        if(!NativeProjectileMetadataValid(shot,PROJECTILE_PEA,MOTION_BACKWARDS))
            return reject("Source104 actual Cherry projectile tuple is unavailable.");
        if(!EnsureNativeProjectileVisuals(b,3,error))return false;
    }
    const uint16_t batch[]={kind};
    if(!CanAllocateNativeSourceDayBatch274(b,batch,error))return false;
    const uint16_t cards7[]={1003},cards8[]={1007,1007,1010};
    const auto cards=level==7?std::span<const uint16_t>(cards7):std::span<const uint16_t>(cards8);
    if(!b->mCoins.mBlock||b->mCoins.mSize>b->mCoins.mMaxSize||
        cards.size()>size_t(b->mCoins.mMaxSize-b->mCoins.mSize))return reject("Source mission whole card batch has no Coin capacity.");
    size_t count=0;Coin* old=nullptr;
    while(b->mCoins.IterateNext(old))if(!old->mDead&&GetNativeDroppedCard269Data(old))++count;
    if(count+cards.size()>128)return reject("Source mission whole card batch exceeds source card capacity.");
    for(uint16_t card:cards)
        if(!CanCreateNativeDroppedCard269(b,card,0)||!EnsureNativePlantVisuals(b,card,error))
            return reject("Source mission card portrait/plant resource is unavailable.");
    return true;
}
bool PrepareLesson7(Board* b,uint32_t& mission)
{
    std::string error;
    const auto reject=[&](const char* why)
    {if(b->mMainCounter%100==0)LawnNet::NetTrace("Fusion source Tutor7 Prepare blocked reason=%s tick=%d",why,b->mMainCounter);return false;};
    if(!LessonWorld278(b,7,&error)||!CanApplyNativeFusionGameplay(b)||
        GetNativeContext(b)->selection.State().phase!=FusionBankPhase::Playing||!b->IsValidGridCell(1,2))return reject(error.c_str());
    std::vector<uint32_t> old;Plant* p=nullptr;
    while(b->mPlants.IterateNext(p))if(!p->mDead&&p->mIsOnBoard&&p->mRow==2&&
        (p->mPlantCol==1||(p->mSeedType==SEED_COBCANNON&&p->mPlantCol+1==1)))
    {
        const auto* d=GetNativePlantData(p);const auto uid=b->mPlants.DataArrayGetID(p);
        if(p->mNetGhost||!d||d->instanceToken!=uid||!Lesson4OldKind(d->kind)||
            p->mSeedType!=GetNativeSeedForPlant(d->kind)||IsNativeGlovePlantReserved(b,p,-1))
            return reject("uncovered cell occupant/removal callback or glove lease");
        old.push_back(uid);
    }
    if(!RepeaterPeak278(b,&error))return reject(error.c_str());
    NativePlantData fresh;fresh.kind=1030;fresh.owner=LawnNet::NET_SHARED_OWNER;
    auto* made=b->AddPlant(1,2,SEED_PEASHOOTER,SEED_NONE,&fresh);
    const auto valid=[&]()
    {
        const auto* d=GetNativePlantData(made);const auto* definition=FusionFindPlant(1030);
        return made&&!made->mDead&&!made->mNetGhost&&made->mNetId&&d&&d->kind==1030&&d->instanceToken&&
            b->mPlants.DataArrayTryToGet(d->instanceToken)==made&&made->mSeedType==SEED_PEASHOOTER&&
            made->mPlantCol==1&&made->mRow==2&&made->mIsOnBoard&&d->firstParent==kFusionNoPlant&&
            d->secondParent==kFusionNoPlant&&!d->packetId&&d->owner==LawnNet::NET_SHARED_OWNER&&!d->randomOrigin&&
            made->mPlantHealth==definition->maxHealth&&made->mPlantMaxHealth==definition->maxHealth;
    };
    if(!valid()){if(made&&!made->mDead)made->Die(255);return reject("default factory binding failed before clear");}
    for(uint32_t uid:old)if(auto* actor=b->mPlants.DataArrayTryToGet(uid);actor&&!actor->mDead)actor->Die(0);
    if(!valid())
    {LawnNet::NetTrace("Fusion source Tutor7 invariant FAILED after source Die0; old removal already committed tick=%d",b->mMainCounter);if(!made->mDead)made->Die(255);return false;}
    mission=b->mPlants.DataArrayGetID(made);RebuildNativeEquipmentGraph(b);
    LawnNet::NetTrace("Fusion source Tutor7 Prepare committed mission=%u uid=%u old=%zu kind=1030 col=1 row=2 sharedDefault=1 peakP=1 peakR=2 peakA=1 giftGranted=0 tick=%d",made->mNetId,mission,old.size(),b->mMainCounter);
    return true;
}
bool SpawnLessonEnemyAndCards278(Board* b,int level,uint32_t& mission)
{
    std::string error;
    const auto reject=[&](const char* why)
    {if(b->mMainCounter%100==0)LawnNet::NetTrace("Fusion source Tutor%d batch blocked reason=%s tick=%d",level,why,b->mMainCounter);return false;};
    if(!LessonWorld278(b,level,&error)||!CanApplyNativeFusionGameplay(b)||b->mCurrentWave!=0||
        GetNativeContext(b)->selection.State().phase!=FusionBankPhase::Playing||!MissionBatchReady278(b,level,&error))return reject(error.c_str());
    const uint16_t kind=level==7?104:106;
    auto* zombie=SpawnNativeSourceZombie(b,kind,0,0);float x=0,y=0;
    const auto* data=GetNativeSourceZombieData(zombie);
    const float target=float(b->GetCellCenterX(b->GetColumnCount()-1,0))+2.6f*(float(b->GetCellPitchX())/1.35f);
    if(!zombie||!data||data->kind!=kind||!data->instanceToken||!zombie->mNetId||
        b->mZombies.DataArrayTryToGet(data->instanceToken)!=zombie||!GetNativeSourceZombie269NodePoint(zombie,"",x,y)||!std::isfinite(target))
    {if(zombie&&!zombie->mDead)zombie->DieNoLoot();return reject("source default NPC/root binding failed");}
    zombie->mPosX+=target-x;zombie->mX=int(zombie->mPosX);
    const uint16_t cards7[]={1003},cards8[]={1007,1007,1010};
    const auto cards=level==7?std::span<const uint16_t>(cards7):std::span<const uint16_t>(cards8);
    std::array<Coin*,3> made{};
    for(size_t i=0;i<cards.size();++i)
    {
        const int col=level==7?4:int(i)+1;
        made[i]=CreateNativeDroppedCard269(b,cards[i],0,float(b->GetCellCenterX(col,2)),float(b->GridToPixelY(col,2)+50));
        if(!made[i])
        {
            for(auto* card:made)if(card&&!card->mDead)card->Die();
            zombie->DieNoLoot();return reject("source card factory failed; partial group retired without stage commit");
        }
    }
    mission=b->mZombies.DataArrayGetID(zombie);
    LawnNet::NetTrace("Fusion source Tutor%d batch committed mission=%u uid=%u kind=%u row=0 rootX=%.9g sourceX=sentinel9.9 fromWave=0 cards=%zu cost=0 way=0 tick=%d",level,zombie->mNetId,mission,kind,target,cards.size(),b->mMainCounter);
    return true;
}
bool Lesson10World281(Board* b,std::string* error)
{
    const auto* c=GetNativeContext(b);
    if(!Host(b)||!c||!c->adventure.enabled||c->adventure.run.level!=10||c->adventure.run.cycle!=1||
        b->mBackground!=BACKGROUND_2_NIGHT||!IsNativeSourceAdventureGrassWorld281(b))
    {if(error)*error="Source Tutor10 requires its ordinary cycle1 Night grass world.";return false;}
    return true;
}
bool Lesson10Peak281(Board* b,int count,std::string* error)
{
    const auto reject=[&](const char* why){if(error)*error=why;return false;};
    // Genuine source kinds, one ordinary native body each in Night; no sleep,
    // heads or attachments. Loading/probing finishes before any actor RNG.
    const uint16_t kinds[]={6,1,1031};
    const SeedType seeds[]={SEED_PUFFSHROOM,SEED_SUNFLOWER,SEED_SUNSHROOM};
    const ReanimationType types[]={REANIM_PUFFSHROOM,REANIM_SUNFLOWER,REANIM_SUNSHROOM};
    for(int i=0;i<3;++i)
    {
        const auto* definition=FusionFindPlant(kinds[i]);
        if(!definition||!definition->hasStats||definition->maxHealth<=0||GetNativeSeedForPlant(kinds[i])!=seeds[i]||
            GetPlantDefinition(seeds[i]).mReanimationType!=types[i]||!EnsureNativePlantVisuals(b,kinds[i],error))
            return reject("Source Tutor10 plant/model/native carrier unavailable.");
        ReanimatorEnsureDefinitionLoaded(types[i],false);
        if(!gReanimatorDefArray||unsigned(types[i])>=gReanimatorDefCount)return reject("Source Tutor10 native definition unavailable.");
        const auto& tracks=gReanimatorDefArray[int(types[i])].mTracks;
        if(!tracks.tracks||tracks.count<=0)return reject("Source Tutor10 native definition has no tracks.");
        for(int track=0;track<tracks.count;++track)
            if(tracks.tracks[track].mName&&strnicmp(tracks.tracks[track].mName,"attacher__",10)==0)
                return reject("Source Tutor10 native definition has an unbudgeted dynamic attacher.");
    }
    auto* effects=b->mApp->mEffectSystem;
    if(!effects||effects!=gEffectSystem||!effects->mReanimationHolder)return reject("Source Tutor10 native effect pools unavailable.");
    const auto& p=b->mPlants;const auto& r=effects->mReanimationHolder->mReanimations;
    if(count<0||!p.mBlock||!r.mBlock||p.mSize>p.mMaxSize||r.mSize>r.mMaxSize||
        p.mMaxSize-p.mSize<count||r.mMaxSize-r.mSize<count)
        return reject("Source Tutor10 whole batch has insufficient native Plant/Reanimation capacity.");
    return true;
}
bool PrepareLesson10(Board* b)
{
    std::string error;
    const auto reject=[&](const char* why)
    {if(b->mMainCounter%100==0)LawnNet::NetTrace("Fusion source Tutor10 prepare blocked reason=%s tick=%d",why,b->mMainCounter);return false;};
    if(!Lesson10World281(b,&error)||!CanApplyNativeFusionGameplay(b)||
        GetNativeContext(b)->selection.State().phase!=FusionBankPhase::Playing)return reject(error.c_str());
    std::vector<uint32_t> old;Plant* p=nullptr;
    while(b->mPlants.IterateNext(p))if(!p->mDead&&p->mIsOnBoard&&p->mPlantCol==2&&p->mRow==2)
    {
        const auto* d=GetNativePlantData(p);const auto uid=b->mPlants.DataArrayGetID(p);
        // SmallPuff inherits Plant's empty DieEvent; SunPuff has no producer
        // death reward. Other removal callbacks retain the audited4/7 gate.
        if(p->mNetGhost||!d||d->instanceToken!=uid||(!Lesson4OldKind(d->kind)&&d->kind!=6&&d->kind!=1031)||
            p->mSeedType!=GetNativeSeedForPlant(d->kind)||IsNativeGlovePlantReserved(b,p,-1))
            return reject("uncovered cell occupant/removal callback or glove lease");
        old.push_back(uid);
    }
    if(!Lesson10Peak281(b,3,&error))return reject(error.c_str());
    // Source ClearPlant(reason0) precedes the three ordinary SetPlant calls.
    // The complete peak was checked without crediting any of these removals.
    for(uint32_t uid:old)if(auto* actor=b->mPlants.DataArrayTryToGet(uid);actor&&!actor->mDead)actor->Die(0);
    NativePlantData fresh;fresh.kind=6;fresh.owner=LawnNet::NET_SHARED_OWNER;
    std::array<Plant*,3> made{};
    for(size_t i=0;i<made.size();++i)
    {
        made[i]=TrySpawnNativeFusionPlant(b,2,2,fresh);const auto* d=GetNativePlantData(made[i]);
        if(!made[i]||made[i]->mDead||!d||d->kind!=6||!d->instanceToken||!made[i]->mNetId||
            b->mPlants.DataArrayTryToGet(d->instanceToken)!=made[i]||d->owner!=LawnNet::NET_SHARED_OWNER||
            d->packetId||d->randomOrigin||d->firstParent!=kFusionNoPlant||d->secondParent!=kFusionNoPlant||
            made[i]->mPlantCol!=2||made[i]->mRow!=2||made[i]->mSeedType!=SEED_PUFFSHROOM||made[i]->mIsAsleep)
        {
            for(auto* actor:made)if(actor&&!actor->mDead)actor->Die(255);
            return reject("source Puff factory failed; partial batch retired without stage commit");
        }
    }
    LawnNet::NetTrace("Fusion source Tutor10 prepare committed old=%zu kinds=6,6,6 ids=%u,%u,%u tokens=%u,%u,%u col=2 row=2 defaultShared=1 peakP=3 peakR=3 peakA=0 tick=%d",
        old.size(),made[0]->mNetId,made[1]->mNetId,made[2]->mNetId,
        b->mPlants.DataArrayGetID(made[0]),b->mPlants.DataArrayGetID(made[1]),b->mPlants.DataArrayGetID(made[2]),b->mMainCounter);
    return true;
}
bool MixLesson10(Board* b)
{
    std::string error;
    if(!Lesson10World281(b,&error)||!CanApplyNativeFusionGameplay(b)||
        GetNativeContext(b)->selection.State().phase!=FusionBankPhase::Playing||!Lesson10Peak281(b,3,&error))return false;
    // Source SetPlant(1) uses CheckBox/CheckMix. Root's common multi-Puff
    // operation snapshots recipients and handles each ByMix removal/child;
    // this caller must not clone one arbitrary body or replace the whole cell.
    NativePlantData fresh;fresh.kind=1;fresh.owner=LawnNet::NET_SHARED_OWNER;
    auto* made=TrySpawnNativeFusionPlant(b,2,2,fresh);const auto* d=GetNativePlantData(made);
    if(!made||made->mDead||!d||!d->instanceToken||!made->mNetId||
        b->mPlants.DataArrayTryToGet(d->instanceToken)!=made||d->owner!=LawnNet::NET_SHARED_OWNER||d->packetId||d->randomOrigin)return false;
    LawnNet::NetTrace("Fusion source Tutor10 followup committed incoming=1 firstChild=%u token=%u kind=%u sharedMultiPuff=1 col=2 row=2 tick=%d",
        made->mNetId,d->instanceToken,d->kind,b->mMainCounter);return true;
}
struct LessonPlant284 {uint16_t kind;int column,row;};
// TutorUpdate15 180349B70 preserves this clear/birth order, not row grouping.
constexpr LessonPlant284 icePlants284[]={{1034,3,1},{1037,5,2},{1038,3,3},{1034,4,1},{1037,6,2},{1038,4,3}};
bool LessonWorld284(Board* b,int level,std::string* error)
{
    const auto* c=GetNativeContext(b);
    if(!Host(b)||!c||!c->adventure.enabled||c->adventure.run.level!=level||c->adventure.run.cycle!=1||
        (level!=13&&level!=15)||b->mBackground!=BACKGROUND_2_NIGHT||!IsNativeSourceAdventureGrassWorld281(b))
    {if(error)*error="Source Tutor13/15 requires its ordinary cycle1 Night grass world.";return false;}
    return true;
}
bool LessonPlantPeak284(Board* b,std::span<const LessonPlant284> plants,int extraR,int coins,std::string* error)
{
    const auto reject=[&](const char* why){if(error)*error=why;return false;};
    int reanimations=extraR,attachments=0;
    for(const auto& actor:plants)
    {
        SeedType expected=SEED_NONE;
        switch(actor.kind){case 3:expected=SEED_WALLNUT;break;case 1027:expected=SEED_TALLNUT;break;
        case 1012:expected=SEED_CHOMPER;break;case 1034:expected=SEED_PEASHOOTER;break;
        case 1037:expected=SEED_FUMESHROOM;break;case 1038:expected=SEED_SCAREDYSHROOM;break;}
        const auto* stats=FusionFindPlant(actor.kind);
        if(expected==SEED_NONE||GetNativeSeedForPlant(actor.kind)!=expected||!stats||!stats->hasStats||stats->maxHealth<=0||
            !EnsureNativePlantVisuals(b,actor.kind,error))return reject("Source lesson plant/model/native carrier unavailable.");
        const auto type=GetPlantDefinition(expected).mReanimationType;
        if(type==REANIM_NONE)return reject("Source lesson native definition unavailable.");
        ReanimatorEnsureDefinitionLoaded(type,false);
        if(!gReanimatorDefArray||unsigned(type)>=gReanimatorDefCount)
            return reject("Source lesson native definition unavailable.");
        const auto& tracks=gReanimatorDefArray[int(type)].mTracks;
        if(!tracks.tracks||tracks.count<=0)return reject("Source lesson native definition has no tracks.");
        for(int i=0;i<tracks.count;++i)if(tracks.tracks[i].mName&&strnicmp(tracks.tracks[i].mName,"attacher__",10)==0)
            return reject("Source lesson native definition has an unbudgeted dynamic attacher.");
        // Ordinary Night has one body; Pea additionally initializes a head/A.
        reanimations+=expected==SEED_PEASHOOTER?2:1;attachments+=expected==SEED_PEASHOOTER?1:0;
    }
    auto* effects=b->mApp->mEffectSystem;
    if(!effects||effects!=gEffectSystem||!effects->mReanimationHolder||!effects->mAttachmentHolder)
        return reject("Source lesson native effect pools unavailable.");
    const auto& p=b->mPlants;const auto& r=effects->mReanimationHolder->mReanimations;
    const auto& a=effects->mAttachmentHolder->mAttachments;const auto& c=b->mCoins;
    if(extraR<0||coins<0||!p.mBlock||!r.mBlock||!a.mBlock||!c.mBlock||p.mSize>p.mMaxSize||
        r.mSize>r.mMaxSize||a.mSize>a.mMaxSize||c.mSize>c.mMaxSize||
        plants.size()>size_t(p.mMaxSize-p.mSize)||reanimations>r.mMaxSize-r.mSize||
        attachments>a.mMaxSize-a.mSize||coins>c.mMaxSize-c.mSize)
        return reject("Source lesson whole live Plant/Coin/R/A peak unavailable; no removal credit.");
    // Board::AddPlant also calls DoPlantingEffects for each grass birth.
    // These are safe cosmetic allocators, but reserve their complete batch
    // before a destructive source clear rather than silently dropping it.
    auto* particles=effects->mParticleHolder;
    if(!particles||!gParticleDefArray||int(PARTICLE_PLANTING)>=gParticleDefCount)
        return reject("Source lesson native planting particle definition unavailable.");
    const auto& planting=gParticleDefArray[int(PARTICLE_PLANTING)];const int emitters=planting.mEmitterDefCount;
    const auto& ps=particles->mParticleSystems;const auto& pe=particles->mEmitters;
    if(emitters<=0||!planting.mEmitterDefs||!ps.mBlock||!pe.mBlock||ps.mSize>ps.mMaxSize||pe.mSize>pe.mMaxSize||
        plants.size()>size_t(ps.mMaxSize-ps.mSize)||plants.size()*size_t(emitters)>size_t(pe.mMaxSize-pe.mSize))
        return reject("Source lesson whole planting ParticleSystem/emitter peak unavailable.");
    return true;
}
bool LessonOldActors284(Board* b,std::span<const LessonPlant284> cells,std::vector<uint32_t>& old,std::string* error)
{
    for(const auto& cell:cells)
    {
        Plant* p=nullptr;while(b->IteratePlants(p))if(p->mIsOnBoard&&p->mRow==cell.row&&
            (p->mPlantCol==cell.column||(p->mSeedType==SEED_COBCANNON&&p->mPlantCol+1==cell.column)))
        {
            const auto* d=GetNativePlantData(p);const auto uid=b->mPlants.DataArrayGetID(p);
            // These families inherit empty source DieEvent. Damage/death
            // rewards, live transient tools and glove leases remain gated.
            const bool safe=d&&(Lesson4OldKind(d->kind)||d->kind==6||d->kind==1031||d->kind==1012||
                d->kind==1027||d->kind==1034||d->kind==1037||d->kind==1038);
            if(!safe||p->mNetGhost||d->instanceToken!=uid||p->mSeedType!=GetNativeSeedForPlant(d->kind)||
                IsNativeGlovePlantReserved(b,p,-1))
            {if(error)*error="Source lesson has an unsupported removal callback, foreign actor or glove lease.";return false;}
            if(std::find(old.begin(),old.end(),uid)==old.end())old.push_back(uid);
        }
    }
    return true;
}
void ClearLessonActors284(Board* b,const std::vector<uint32_t>& old)
{for(auto uid:old)if(auto* p=b->mPlants.DataArrayTryToGet(uid);p&&!p->mDead)p->Die(0);}
bool FreshLessonPlant284(Board* b,Plant* p,const LessonPlant284& actor)
{
    const auto* d=GetNativePlantData(p);
    return Actor(p)&&p->mNetId&&d&&d->kind==actor.kind&&d->instanceToken&&
        b->mPlants.DataArrayTryToGet(d->instanceToken)==p&&p->mSeedType==GetNativeSeedForPlant(actor.kind)&&
        p->mPlantCol==actor.column&&p->mRow==actor.row&&!p->mIsAsleep&&!p->mWakeUpCounter&&
        d->owner==LawnNet::NET_SHARED_OWNER&&!d->packetId&&!d->randomOrigin&&
        d->firstParent==kFusionNoPlant&&d->secondParent==kFusionNoPlant;
}
bool LessonMutationFailed284(Board* b,const char* why)
{
    // A failed post-preflight initializer cannot safely replay a consumed
    // source action. Stop this source controller without committing its stage.
    auto* c=GetNativeContext(b);c->adventure.schedulerFailed=true;c->adventureScheduler.failed=true;
    LawnNet::NetTrace("Fusion source Tutor284 invariant FAILED after mutation reason=%s tick=%d",why,b->mMainCounter);return false;
}
bool PrepareLesson13(Board* b,bool chomper,uint32_t& mission)
{
    std::string error;const LessonPlant284 actor={uint16_t(chomper?1012:3),chomper?3:2,2};
    const std::span<const LessonPlant284> batch(&actor,1);std::vector<uint32_t> old;
    if(!LessonWorld284(b,13,&error)||!CanApplyNativeFusionGameplay(b)||
        GetNativeContext(b)->selection.State().phase!=FusionBankPhase::Playing||
        !PreflightNativeFusionAdventureLesson284Effects(b,13,&error)||!LessonPlantPeak284(b,batch,0,1,&error)||
        !LessonOldActors284(b,batch,old,&error))return false;
    ClearLessonActors284(b,old);
    NativePlantData fresh;fresh.kind=actor.kind;fresh.owner=LawnNet::NET_SHARED_OWNER;
    auto* made=TrySpawnNativeFusionPlant(b,actor.column,actor.row,fresh);
    if(!FreshLessonPlant284(b,made,actor))return LessonMutationFailed284(b,"source13 default plant factory failed");
    if(chomper)ApplyNativeSourcePlantDamage(made,3999,0); // Exact virtual source action, never HP injection.
    if(!FreshLessonPlant284(b,made,actor))
        return LessonMutationFailed284(b,"source13 damaged plant generation failed");
    if(!EmitNativeFusionAdventureLesson284Fertilize(made,&error))
    {
        LawnNet::NetTrace("Fusion source Tutor13 Fertilize grant refused reason=%s",error.c_str());
        return LessonMutationFailed284(b,"source13 Fertilize/Sound66 action failed");
    }
    if(chomper)mission=b->mPlants.DataArrayGetID(made);
    RebuildNativeEquipmentGraph(b);
    LawnNet::NetTrace("Fusion source Tutor13 prepare kind=%u id=%u token=%u hp=%d/%d col=%d row=2 old=%zu sourceDamage=%d realFertilize=1 sharedDefault=1 tick=%d",
        actor.kind,made->mNetId,b->mPlants.DataArrayGetID(made),made->mPlantHealth,made->mPlantMaxHealth,
        actor.column,old.size(),chomper?3999:0,b->mMainCounter);return true;
}
bool Lesson15Peak284(Board* b,std::string* error)
{
    constexpr uint16_t zombies[]={2,2,2};
    if(!EnsureNativeSourceZombieVisuals(b,2,error))return false;
    if(!CanQueryNativeSourceZombieRoot273(b,2))
    {if(error)*error="Source Tutor15 Cone root unavailable.";return false;}
    return CanAllocateNativeSourceDayBatch274(b,zombies,error)&&
        LessonPlantPeak284(b,icePlants284,3,0,error); // Combined6P/3Z/11R/2A, no dead credit.
}
bool PrepareLesson15(Board* b)
{
    std::string error;std::vector<uint32_t> old;
    if(!LessonWorld284(b,15,&error)||!CanApplyNativeFusionGameplay(b)||
        GetNativeContext(b)->selection.State().phase!=FusionBankPhase::Playing||
        !PreflightNativeFusionAdventureLesson284Effects(b,15,&error)||!Lesson15Peak284(b,&error)||
        !LessonOldActors284(b,icePlants284,old,&error))return false;
    ClearLessonActors284(b,old);
    for(const auto& actor:icePlants284)
    {
        NativePlantData fresh;fresh.kind=actor.kind;fresh.owner=LawnNet::NET_SHARED_OWNER;
        if(!FreshLessonPlant284(b,TrySpawnNativeFusionPlant(b,actor.column,actor.row,fresh),actor))
            return LessonMutationFailed284(b,"source15 default six-plant factory failed");
    }
    for(int row:{1,2,3})
    {
        auto* z=SpawnNativeSourceZombie(b,2,row,0);const auto* d=GetNativeSourceZombieData(z);float x=0,y=0;
        // Source SetZombie(...,x=9,idle=false). The source last cell is7.3;
        // align its actual ROOT during birth, as existing7/8 at source9.9.
        const float target=float(b->GetCellCenterX(b->GetColumnCount()-1,row))+1.7f*(float(b->GetCellPitchX())/1.35f);
        if(!z||!d||d->kind!=2||!d->instanceToken||!z->mNetId||
            b->mZombies.DataArrayTryToGet(d->instanceToken)!=z||!GetNativeSourceZombie269NodePoint(z,"",x,y)||!std::isfinite(target))
            return LessonMutationFailed284(b,"source15 genuine Cone/root binding failed");
        z->mPosX+=target-x;z->mX=int(z->mPosX);
    }
    RebuildNativeEquipmentGraph(b);
    LawnNet::NetTrace("Fusion source Tutor15 prepare plants=6 Cone2=3 rows=1,2,3 sourceRootX=9 fromWave=0 peakP=6 peakZ=3 peakR=11 peakA=2 old=%zu tick=%d",old.size(),b->mMainCounter);
    return true;
}
bool ClearLesson15(Board* b)
{
    std::string error;std::vector<uint32_t> old;
    if(!LessonWorld284(b,15,&error)||!CanApplyNativeFusionGameplay(b)||!LessonOldActors284(b,icePlants284,old,&error))return false;
    ClearLessonActors284(b,old);RebuildNativeEquipmentGraph(b);
    LawnNet::NetTrace("Fusion source Tutor15 finalClear old=%zu reason=0 tick=%d",old.size(),b->mMainCounter);return true;
}
bool Active(Board* b,int slot)
{
    const auto* c=GetNativeContext(b);
    if(!c||slot<0||slot>=int(kFusionBankPlayers)||!(c->selection.State().activeMask&(1U<<slot)))return false;
    if(!b->mApp->NetActive())return slot==0;
    const auto* net=b->mApp->mNetManager;if(!net)return false;
    const auto& player=net->GetPlayer(slot);return player.mActive&&!player.mSpectator;
}
}
bool PreflightNativeFusionAdventureLesson278(Board* b,int level,std::string* error)
{
    if(error)error->clear();
    // Six bodies cover the fresh3-Puff preparation plus the later3-child
    // replacement peak; no old actor/animation is credited as already freed.
    if(level==10)return Lesson10World281(b,error)&&Lesson10Peak281(b,6,error);
    if(level==13||level==15)
    {
        if(!LessonWorld284(b,level,error)||!PreflightNativeFusionAdventureLesson284Effects(b,level,error))return false;
        if(level==15)return Lesson15Peak284(b,error);
        constexpr LessonPlant284 plants[]={{3,2,2},{1027,2,2},{1012,3,2}};
        return LessonPlantPeak284(b,plants,0,2,error);
    }
    return LessonWorld278(b,level,error)&&(level!=7||RepeaterPeak278(b,error))&&MissionBatchReady278(b,level,error);
}
void PresentNativeFusionAdventureTutorial273(Board* b,const FusionAdventureRun273& r)
{
    if(!Current(b)||!FusionAdventureRunValid273(r)||!r.level||!b->mAdvice)return;
    const auto text=Text(r.tutorial.message);
    if(text.empty())return; // Unimplemented lesson messages never replace native Advice with an empty label.
    if(!r.tutorial.messageTicks)
    {
        if(b->mAdvice->mLabel==text)
        {
            b->mAdvice->ClearLabel();b->mAdvice->mDuration=0;b->mHelpIndex=ADVICE_NONE;
        }
        return;
    }
    // Source message and lifetime already travel together in the accepted FNS
    // view. Use the native widget without a second legacy Advice clock/event.
    if(b->mAdvice->mLabel!=text||b->mAdvice->mDuration==0)
        b->mAdvice->SetLabel(text,MESSAGE_STYLE_TUTORIAL_LEVEL1_STAY);
    b->mHelpIndex=ADVICE_NONE;
    // Native Advice supplies font/layout/drawing. The source message clock,
    // including600s BuildGatling, stays in the explicitly replicated room view.
    b->mAdvice->mDuration=r.tutorial.messageTicks;
    b->mAdvice->mDisplayTime=FusionAdventureMessageDuration273(r.tutorial.message);
}
bool UpdateNativeFusionAdventureTutorial273(Board* b,FusionAdventureRun273& r,FusionAdventureTutorialEffects273& out)
{
    out={};
    if(!Playing(b)||!FusionAdventureRunValid273(r)||!r.level||
        (r.tutorial.active&&r.level!=1&&r.level!=4&&r.level!=7&&r.level!=8&&r.level!=10&&r.level!=13&&r.level!=15))return false;
    auto* context=GetNativeContext(b);if(!context)return false;
    if(!r.tutorial.active||!((r.level==4&&r.tutorial.stage>=3)||(r.level==7&&r.tutorial.stage==2)||
        (r.level==13&&r.tutorial.stage>=7)))context->adventureMissionPlantUID=0;
    if(!r.tutorial.active||(r.level!=7&&r.level!=8)||(r.level==7&&r.tutorial.stage<4)||
        (r.level==8&&r.tutorial.stage<2))context->adventureMissionZombieUID=0;
    auto candidate=r;FusionAdventureTutorialEffects273 effects;
    if(r.level==4||r.level==7||r.level==8||r.level==10||r.level==13||r.level==15)
    {
        FusionAdventureLessonInput274 input;
        auto* mission=b->mPlants.DataArrayTryToGet(context->adventureMissionPlantUID);
        const auto* data=GetNativePlantData(mission);
        input.hasMissionPlant=mission&&(r.level==7||r.level==13||!mission->mDead)&&!mission->mNetGhost&&data&&
            data->instanceToken==context->adventureMissionPlantUID;
        if(input.hasMissionPlant)input.missionPlantColumn=mission->mPlantCol;
        if(r.level==13)
        {
            if(r.tutorial.active&&r.tutorial.stage==7&&(!input.hasMissionPlant||data->kind!=1012))
                return LessonMutationFailed284(b,"source13 tracked NutChomper generation is missing");
            input.missionPlantFullyRepaired=input.hasMissionPlant&&mission->mPlantHealth==mission->mPlantMaxHealth;
            Plant* p=nullptr;while(b->IteratePlants(p))if(const auto* d=GetNativePlantData(p);d&&d->kind==1027)
                input.hasTallNut1027=true;
        }
        input.sourceEnemyCount=context->adventureEnemyCount; // Actual whole-world source count, not three mission flags.
        auto* enemy=b->mZombies.DataArrayTryToGet(context->adventureMissionZombieUID);
        const auto* source=GetNativeSourceZombieData(enemy);
        input.hasMissionZombie=enemy&&!enemy->mNetGhost&&source&&source->instanceToken==context->adventureMissionZombieUID&&
            source->kind==(r.level==7?104:106);
        if(input.hasMissionZombie)
        {input.missionZombieBeforeDying=!enemy->mHasHead;input.missionZombieDying=GetNativeFusionZombieStatus(enemy)==1;}
        if(r.level==8)
        {
            Plant* plant=nullptr;
            while(b->mPlants.IterateNext(plant))if(const auto* identity=GetNativePlantData(plant);identity&&
                identity->instanceToken==b->mPlants.DataArrayGetID(plant)&&(identity->kind==1020||identity->kind==1029))
                input.hasBucketPlant1020or1029=true;
        }
        if(!StepFusionAdventureTutorial273(candidate,1,input,effects))return false;
        switch(effects.missionAction)
        {
        case FusionAdventureMissionAction274::None:break;
        case FusionAdventureMissionAction274::Prepare4:if(!PrepareLesson4(b,context->adventureMissionPlantUID))return false;break;
        case FusionAdventureMissionAction274::Prepare7:if(!PrepareLesson7(b,context->adventureMissionPlantUID))return false;break;
        case FusionAdventureMissionAction274::Spawn7EnemyAndCard:if(!SpawnLessonEnemyAndCards278(b,7,context->adventureMissionZombieUID))return false;break;
        case FusionAdventureMissionAction274::Prepare8:if(!SpawnLessonEnemyAndCards278(b,8,context->adventureMissionZombieUID))return false;break;
        case FusionAdventureMissionAction274::Prepare10:if(!PrepareLesson10(b))return false;break;
        case FusionAdventureMissionAction274::Mix10:if(!MixLesson10(b))return false;break;
        case FusionAdventureMissionAction274::Prepare13Nut:if(!PrepareLesson13(b,false,context->adventureMissionPlantUID))return false;break;
        case FusionAdventureMissionAction274::Prepare13Chomper:if(!PrepareLesson13(b,true,context->adventureMissionPlantUID))return false;break;
        case FusionAdventureMissionAction274::Prepare15:if(!PrepareLesson15(b))return false;break;
        case FusionAdventureMissionAction274::Clear15:if(!ClearLesson15(b))return false;break;
        }
    }
    else
    {
        bool sunflower=false,peaSunflower=false,gatling=false;Plant* p=nullptr;
        while(b->IteratePlants(p))if(Actor(p))if(const auto* d=GetNativePlantData(p))
        {sunflower|=d->kind==1;peaSunflower|=d->kind==1000;gatling|=d->kind==1032;}
        if(!StepFusionAdventureTutorial273(candidate,1,sunflower,peaSunflower,gatling,effects))return false;
    }
    // Expected capacity failures retry before mutation. A13/15 invariant
    // failure after mutation stops its source controller instead of replaying.
    if(effects.grantMissionCards&&!MissionCards(b))return false;
    r=candidate;out=effects;
    if(effects.completed)context->adventureMissionPlantUID=context->adventureMissionZombieUID=0;
    if(effects.messageChanged||!r.tutorial.messageTicks)PresentNativeFusionAdventureTutorial273(b,r);
    return true;
}
bool NativeFusionAdventureEnemyDied273(Board* b,FusionAdventureRun273& r,int wave,int maximum,int enemies,bool iz,float x,float y)
{
    if(!Host(b)||!std::isfinite(x)||!std::isfinite(y)||
        !TryArmFusionAdventureReward273(r,wave,maximum,enemies,iz))return false;
    // Unity clamps the root+(0,1) anchor to viewport[.2,.8]. Use the actual
    // native viewport and the established source vertical-unit projection.
    r.trophySpawnX=std::clamp(x,float(b->mWidth)*.2f,float(b->mWidth)*.8f);
    r.trophySpawnY=std::clamp(y-float(GetNativeFusionPixelsY(b)),float(b->mHeight)*.2f,float(b->mHeight)*.8f);
    EnsureNativeFusionAdventureTrophy273(b,r);return true;
}
bool EnsureNativeFusionAdventureTrophy273(Board* b,FusionAdventureRun273& r)
{
    if(!Host(b)||!FusionAdventureRunValid273(r)||!r.over||r.trophyNetId||r.rewardCollected||
        !b->mCoins.mBlock||b->mCoins.mSize>=b->mCoins.mMaxSize)return false;
    auto* coin=b->mCoins.DataArrayAlloc();coin->mApp=b->mApp;coin->mBoard=b;
    // The real Coin owns drawing, native drop physics and hit testing. Create
    // locally, then expose it through the normal complete native snapshot.
    coin->CoinInitialize(int(r.trophySpawnX),int(r.trophySpawnY),COIN_TROPHY,COIN_MOTION_COIN);
    coin->mNetGhost=false;coin->mNetId=b->NetAllocId();
    if(!coin->mNetId){coin->Die();return false;}
    coin->mPosX=r.trophySpawnX-coin->mWidth*.5f;coin->mPosY=r.trophySpawnY-coin->mHeight*.5f;
    coin->mGroundY=int(coin->mPosY+float(GetNativeFusionPixelsY(b))*.5f);
    if(!BindFusionAdventureTrophy273(r,coin->mNetId,b->mCoins.DataArrayGetID(coin)))
    {coin->Die();return false;}
    LawnNet::NetTrace("Fusion source Adventure trophy spawned level=%u id=%u token=%u over=1 progressCommitted=0",
        r.level,r.trophyNetId,r.trophyToken);return true;
}
bool IsNativeFusionAdventureTrophy273(const Coin* coin,const FusionAdventureRun273& r)
{
    if(!coin||!Current(coin->mBoard)||!FusionAdventureRunValid273(r)||!r.over||!r.trophyToken||
        coin->mType!=COIN_TROPHY||!coin->mNetId||coin->mNetId!=r.trophyNetId)return false;
    // Client Coin array generations are local; only the host compares its
    // canonical generation. Commands carry the replicated host token explicitly.
    return coin->mApp->NetIsClient()||coin->mBoard->mCoins.DataArrayGetID(const_cast<Coin*>(coin))==r.trophyToken;
}
bool CollectNativeFusionAdventureTrophy273(Coin* coin,int slot,FusionAdventureProfile273& profile,
    FusionAdventureRun273& r,bool& committed)
{
    committed=false;if(!IsNativeFusionAdventureTrophy273(coin,r))return false;
    auto* b=coin->mBoard;
    if(!Playing(b)||coin->mDead||coin->mNetGhost||!Active(b,slot)||
        !TryCollectFusionAdventureReward273(profile,r,coin->mNetId,b->mCoins.DataArrayGetID(coin)))return true;
    coin->mCollectX=coin->mPosX;coin->mCollectY=coin->mPosY;
    coin->mIsBeingCollected=true;coin->mHitGround=true;coin->mVelX=coin->mVelY=0;
    coin->mNetCollectionFxStarted=true;
    AttachmentDie(coin->mAttachmentID);
    b->mApp->mMusic->StopAllMusic();b->mApp->PlaySample(Sexy::SOUND_DIAMOND);
    committed=true;
    LawnNet::NetTrace("Fusion source Adventure trophy CLICK level=%u slot=%d frontier=%u cycle=%u replay=%d completed=%llu",
        r.level,slot,profile.frontierLevel,profile.cycle,r.replay?1:0,static_cast<unsigned long long>(profile.completedLevels));
    return true;
}
bool UpdateNativeFusionAdventureTrophy273(Coin* coin,const FusionAdventureRun273& r)
{
    if(!IsNativeFusionAdventureTrophy273(coin,r))return false;
    if(!Playing(coin->mBoard)||coin->mDead||coin->mNetGhost)return true;
    if(coin->mCoinAge<(std::numeric_limits<int>::max)())++coin->mCoinAge;
    if(!r.rewardCollected){coin->UpdateFall();return true;}
    const float t=(std::min)(1.0f,float(kFusionAdventureRewardTicks273-r.rewardTicks)/125.0f);
    auto* b=coin->mBoard;
    // Source target local(10,-3), projected with the same native cell basis.
    const float targetX=float(b->GetCellCenterX(0,0))+(10.0f+4.8f)*float(kFusionBasePixelsX)-coin->mWidth*.5f;
    const float targetY=float(b->GridToPixelY(0,0)+50)+(2.3f+3.0f)*float(GetNativeFusionPixelsY(b))-coin->mHeight*.5f;
    coin->mPosX=coin->mCollectX+(targetX-coin->mCollectX)*t;
    coin->mPosY=coin->mCollectY+(targetY-coin->mCollectY)*t;coin->mScale=1.0f-.5f*t;
    return true;
}
bool UpdateNativeFusionAdventureReward273(Board* b,FusionAdventureRun273& r)
{
    if(!Playing(b)||!FusionAdventureRunValid273(r)||!r.level)return false;
    if(r.over&&!r.trophyNetId)EnsureNativeFusionAdventureTrophy273(b,r);
    return StepFusionAdventureReward273(r,1);
}
}
