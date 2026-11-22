#include "../Fusion/FusionQuickJackson367Rules.h"
#include "../Fusion/FusionNativeConveyor308.h"
#include "../Fusion/FusionConveyor348Rules.h"
#include "../Board.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Projectile.h"
#include "../Coin.h"
#include "../LawnMower.h"
#include "../GridItem.h"
#include "../SeedPacket.h"
#include "../ZenGarden.h"
#include "../CursorObject.h"
#include "../Challenge.h"
#include "../Cutscene.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../../Resources.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/Attachment.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/TodParticle.h"
#include "../../SexyAppFramework/SoundManager.h"
#include "../../SexyAppFramework/SoundInstance.h"
#include "../System/PlayerInfo.h"
#include "../../GameConstants.h"
#include "../../SexyAppFramework/Graphics.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include "../../SexyAppFramework/DDInterface.h"
#include "NetManager.h"
#include "NetMystery.h"
#include "NetEndlessWeather.h"
#include "NetZenGarden.h"
#include "NetPresentation.h"
#include "NetSourceCharred315.h"
#include "../Fusion/FusionNativeSourceClock313.h"
#include "../Fusion/FusionNative313SnapshotBridge.h"
#include "../Fusion/FusionNativeBoss361Bridge.h"
#include "../Fusion/FusionNativeBossRoom366.h"
#include "../Fusion/FusionNativeZombie313Dispatch.h"
#include "../Fusion/FusionNativeObstacle313.h"
#include "../Fusion/FusionNativeSourceObstacleFactory313.h"
#include "../Fusion/FusionNativeHeavyZombie313BBridge.h"
#include "NetZombatar.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativePlantDisable363.h"
#include "../Fusion/FusionNativeWire.h"
#include "../Fusion/FusionNativeEffects269.h"
#include "../Fusion/FusionNativeAdventureProgress273.h"
#include "../Fusion/FusionNativeAdventure273.h"
#include "../Fusion/FusionNativeVisual.h"
#include "../Fusion/FusionNativeZombieVisual.h"
#include "../Fusion/NativeSpecialZombie269.h"
#include "../Fusion/FusionNativeGarlic.h"
#include "../Fusion/FusionLobberRules.h"
#include "../Fusion/FusionShooterTiming.h"
#include "../Fusion/FusionStraightProjectileRules.h"
#include "../Fusion/FusionSplitShooterRules.h"
#include "../Fusion/FusionFireRules.h"
#include "../Fusion/FusionProducerRules.h"
#include "../Fusion/FusionChomperRules.h"
#include "../Fusion/FusionMoneyRules.h"
#include "../Fusion/FusionNativeProjectileMetadata.h"
#include "../Fusion/FusionNativeCannon268.h"
#include "../Fusion/FusionNativeEquipment268.h"
#include "../Fusion/FusionNativeTools268.h"
#include "../Fusion/FusionChomper268Rules.h"
#include "../Fusion/FusionShulk268Rules.h"
#include "../Fusion/FusionExtras268Rules.h"
#include "../System/Music.h"
#include <float.h>
#include <limits.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <initializer_list>
#include <set>

using namespace LawnNet;

bool LawnNet::gNetSuppressSounds = false;

namespace
{
bool NetCaptureReanimState(LawnApp*,ReanimationID,NetZombieAnimState&);
bool NetGetZombieTypeFrameCapacity(uint8_t,uint32_t&);
void NetApplyReanimState(LawnApp*,ReanimationID,const NetZombieAnimState&);
bool NetPaperHeadFitsNativeFrames(const NetZombieAnimState& state)
{
    if(!state.mPresent)return true;
    ReanimatorEnsureDefinitionLoaded(REANIM_PEASHOOTER,false);
    if(!gReanimatorDefArray||unsigned(REANIM_PEASHOOTER)>=gReanimatorDefCount)return false;
    const auto& tracks=gReanimatorDefArray[REANIM_PEASHOOTER].mTracks;
    if(!tracks.tracks||tracks.count<=0||tracks.tracks[0].mTransforms.count<=0)return false;
    return NetZombieAnimStateFitsFrames(state,uint32_t(tracks.tracks[0].mTransforms.count));
}
enum NetDaveAnimKey : uint8_t
{
	NET_DAVE_ANIM_NONE = 0,
	NET_DAVE_ANIM_ENTER,
	NET_DAVE_ANIM_ENTER_UP,
	NET_DAVE_ANIM_LEAVE,
	NET_DAVE_ANIM_GRAB,
	NET_DAVE_ANIM_IDLE,
	NET_DAVE_ANIM_IDLE_HANDING,
	NET_DAVE_ANIM_TALK_HANDING,
	NET_DAVE_ANIM_TALK_CRAZY,
	NET_DAVE_ANIM_TALK_SMALL,
	NET_DAVE_ANIM_TALK_MEDIUM,
	NET_DAVE_ANIM_TALK_BLAH,
	NET_DAVE_ANIM_COUNT
};

struct NetDaveAnimSpec
{
	const char* mTrackName;
	ReanimLoopType mLoopType;
	float mAnimRate;
};

const NetDaveAnimSpec gNetDaveAnimSpecs[NET_DAVE_ANIM_COUNT] =
{
	{ nullptr,             ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD,  0.0f },
	{ "anim_enter",        ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 24.0f },
	{ "anim_enterup",      ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 12.0f },
	{ "anim_leave",        ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 24.0f },
	{ "anim_grab",         ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 18.0f },
	{ "anim_idle",         ReanimLoopType::REANIM_LOOP,               12.0f },
	{ "anim_idle_handing", ReanimLoopType::REANIM_LOOP,               12.0f },
	{ "anim_talk_handing", ReanimLoopType::REANIM_LOOP,               12.0f },
	{ "anim_crazy",        ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 12.0f },
	{ "anim_smalltalk",    ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 12.0f },
	{ "anim_mediumtalk",   ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 12.0f },
	{ "anim_blahblah",     ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 12.0f }
};

static const size_t NET_DAVE_STATE_BYTES = 12;
static_assert(MAX_GRID_SIZE_X + 4 == NET_ENDLESS_FOG_COLUMNS &&
	MAX_GRID_SIZE_Y + 1 == NET_ENDLESS_FOG_ROWS, "Weather wire must cover the native fog dimensions");
static_assert(sizeof(((Board*)nullptr)->mGridCelFog) / sizeof(int) == NET_ENDLESS_FOG_CELLS,
	"Weather wire must include every native fog alpha cell");
// Seed packet: stable id, type, imitater, cooldown, owner, active, signed
// horizontal offset, slot-machine countdown/next card/phase, intrinsic origin,
// and authoritative refresh duration/flag (including late join after a use).
static const size_t NET_SEED_PACKET_WIRE_BYTES = NET_MYSTERY_SEED_SNAPSHOT_BYTES;
// Plant: base state (20), three exact presentation counters, Zen display state,
// and every bounded
// reanimation channel owned directly by Plant (8 * 11).  Child attachments and
// particles remain local presentation resources, never raw network pointers.
static const int NET_PLANT_REANIM_ROLES = 8;
static const size_t NET_PLANT_WIRE_BYTES = 23 + NET_ZEN_PLANT_WIRE_BYTES +
        NET_PLANT_REANIM_ROLES * NET_ZOMBIE_ANIM_WIRE_BYTES;
// Zombie: id/type/row/position(8), eight U32 health pools(32), phase/visual state(10),
// velocity/altitude/original animation rate(12), and exact body animation(11).
static const size_t NET_ZOMBIE_WIRE_BYTES = 68 + NET_ZOMBIE_ANIM_WIRE_BYTES + NET_ZOMBATAR_WIRE_BYTES;
// id/type/row/from-wave/order/XY(14), exact health(32), then five phase/flags bytes.
static const size_t NET_ZOMBIE_CHILLED_OFFSET = 14 + NET_ZOMBIE_HEALTH_WIRE_BYTES + 5;
static const size_t NET_PLANT_STATE_OFFSET = 10; // After id/type/imitater/row/col/currentHP/maxHP.
static const size_t NET_ZOMBIE_BUTTERED_OFFSET = NET_ZOMBIE_CHILLED_OFFSET + 1;
static const size_t NET_ZOMBIE_FROZEN_OFFSET = NET_ZOMBIE_CHILLED_OFFSET + 2;
static_assert(NET_ZOMBIE_FROZEN_OFFSET < NET_ZOMBIE_WIRE_BYTES);
// Projectile: id(2), type/row(2), x/y/z/shadow(16), order(4),
// rotation(4), motion/frame/high-ground(3), actual native rotationSpeed(4).
static const size_t NET_PROJECTILE_WIRE_BYTES = 35; // Protocol53 appends actual native mRotationSpeed.
// Coin: id/type/motion(4), x/y(8), order(4), scale/collect(4),
// flags/seed/fade(3), disappear(2), potted visual seed/garden/facing/variation/age(5), exact age/full lifetimeI32(8).
static const size_t NET_COIN_WIRE_BYTES = 39; // Native age/full lifetime plus typed world-card use count.
// Mower records live in NetCommon so the strict transactional codec is covered
// by the protocol-only test target.

bool NetFusionPlantHealthValid(const LawnFusion::NativeSnapshotFrame* frame,uint16_t id,int health,int maximum)
{
    if(!maximum||health<0)return false;
    if(frame)
    {
        const auto record=std::find_if(frame->plants.begin(),frame->plants.end(),[=](const auto& r){return r.id==id;});
        if(record==frame->plants.end())return false;
        const auto& d=record->data;
        if(d.kind==255)
        {const auto rolling=std::find_if(frame->rollingPlants269.begin(),frame->rollingPlants269.end(),[=](const auto& p){return p.id==id;});
         return rolling!=frame->rollingPlants269.end()&&maximum==4000&&health==(std::max)(0,(std::min)(65535,rolling->data.health));}
        if(const auto* strong=LawnFusion::FindFusionStrongChomperProfile(d.kind))
        {int bounded=0;return maximum==strong->baseHealth&&(health==0||(LawnFusion::TryCapFusionStrongChomperHealth(d.kind,health,bounded)&&bounded==health));}
        if(LawnFusion::IsFusionExtras268Kind(d.kind))return LawnFusion::FusionExtras268SnapshotValid(d.kind,health,maximum,d.damageStage,d.batchMode);
    }
    return health<=maximum;
}
bool NetSourceObstacle313RawValid(const NetBuffer& bytes,size_t start,const LawnFusion::FusionSourceObstacle313View& v)
{
    if(start>bytes.Size()||bytes.Size()-start<NET_PROJECTILE_WIRE_BYTES||!LawnFusion::FusionSourceObstacle313ViewValid(v))return false;
    NetBuffer raw=bytes;raw.mReadPos=start;raw.ReadU16Or();const auto type=raw.ReadU8Or();raw.ReadU8Or();
    const float x=raw.ReadF32Or(),y=raw.ReadF32Or(),z=raw.ReadF32Or(),shadow=raw.ReadF32Or();
    const int order=raw.ReadI32Or();const float angle=raw.ReadF32Or();const auto motion=raw.ReadU8Or(),picture=raw.ReadU8Or(),flags=raw.ReadU8Or();const float spin=raw.ReadF32Or();
    return type==PROJECTILE_PEA&&motion==MOTION_STRAIGHT&&!picture&&!flags&&z==0&&spin==0&&
        _finite(x)&&_finite(y)&&_finite(shadow)&&x>= -1024&&x<=2048&&y>= -1024&&y<=2048&&shadow>= -1024&&shadow<=2048&&order>= -1000000&&order<=2000000&&
        _finite(angle)&&(v.kind==LawnFusion::FusionSourceObstacle313Kind::DrownWeapon?std::abs(angle)<=3.141593f:angle==0)&&v.velocityZ==0;
}

bool NetFusionEquipmentCoinRawValid(const NetBuffer& bytes,size_t start,const LawnFusion::NativeEquipmentData& d)
{
    if(start>bytes.Size()||bytes.Size()-start<NET_COIN_WIRE_BYTES||!LawnFusion::NativeEquipmentDataValid(d))return false;
    NetBuffer r=bytes;r.mReadPos=start;r.ReadU16Or();
    const auto type=r.ReadU8Or(),motion=r.ReadU8Or();const float x=r.ReadF32Or(),y=r.ReadF32Or();
    const int order=r.ReadI32Or();const auto scale=r.ReadU16Or(),distance=r.ReadU16Or();const auto flags=r.ReadU8Or(),seed=r.ReadU8Or(),fade=r.ReadU8Or();
    const int shortLife=r.ReadU16Or();bool pottedEmpty=true;for(int j=0;j<5;++j)pottedEmpty&=r.ReadU8Or()==255;
    const int age=r.ReadI32Or(),life=r.ReadI32Or();
    if(type!=(uint8_t)COIN_NONE||motion!=(uint8_t)COIN_MOTION_COIN||!_finite(x)||!_finite(y)||x< -1024||x>2048||y< -1024||y>2048||
       order< -1000000||order>2000000||scale!=4096||distance||flags&5||seed!=255||fade||!pottedEmpty||
       !LawnFusion::NativeEquipmentRawClockValid(d,age,life,static_cast<uint16_t>(shortLife)))return false;
    if(d.phase!=LawnFusion::NativeEquipmentPhase::Cursor&&bool(flags&2)!=(d.phase!=LawnFusion::NativeEquipmentPhase::Falling))return false;
    return true;
}
bool NetHas313(const LawnFusion::NativeSnapshotFrame& frame)
{return !frame.sourceGraphs313.empty()||!frame.waterZombies313.empty()||!frame.heavyZombies313.empty()||!frame.extendedZombies313.empty()||!frame.sourceObstacles313.empty()||!frame.capturedPlants314.empty()||!frame.bossZombies361.empty()||frame.bossRoom366.enabled;}
void NetGather313(const LawnFusion::NativeSnapshotFrame& frame,uint16_t id,LawnFusion::Native313Snapshot& out)
{
    out={};
    for(const auto& r:frame.sourceGraphs313)if(r.id==id)out.graph=r.data;
    for(const auto& r:frame.waterZombies313)if(r.id==id)out.water=r.data;
    for(const auto& r:frame.heavyZombies313)if(r.id==id)out.heavy=r.data;
    for(const auto& r:frame.extendedZombies313)if(r.id==id)out.extended=r.data;
    for(const auto& r:frame.bossZombies361)if(r.id==id)out.boss=r.data;
}
struct NetCaptured314Staging
{
    LawnFusion::NativeCapturedPlants314* helper=nullptr;
    ~NetCaptured314Staging(){if(helper)helper->RollbackStagedBeforeRaw();}
};

void NetPruneNativeFusionBridgeMetadata(Board* board,LawnFusion::NativeContext& context)
{
    const auto prune=[](auto& metadata,auto& array)
    {for(auto it=metadata.begin();it!=metadata.end();)if(!array.DataArrayTryToGet(it->first))it=metadata.erase(it);else ++it;};
    // Retain metadata for dead-but-allocated presentation until the full
    // DataArray generation is actually freed, matching existing native maps.
    prune(context.equipmentCoins,board->mCoins);prune(context.equipmentPlants,board->mPlants);prune(context.zombieInstanceTokens,board->mZombies);
    prune(context.star269Plants,board->mPlants);prune(context.star269Projectiles,board->mProjectiles);prune(context.doom269Plants,board->mPlants);
    prune(context.companions269,board->mProjectiles);prune(context.companionPlants269,board->mPlants);
    prune(context.worldPlants269,board->mPlants);prune(context.worldBeams269,board->mProjectiles);
    prune(context.worldRemainder269Aux,board->mProjectiles);prune(context.worldRemainder269Shots,board->mProjectiles);
    prune(context.sourceGraphs313,board->mZombies);prune(context.waterZombies313,board->mZombies);
    prune(context.heavyZombies313B,board->mZombies);prune(context.extendedZombies313,board->mZombies);prune(context.bossZombies361,board->mZombies);
    prune(context.sourceObstacles313,board->mProjectiles);LawnFusion::PruneNativeSourceGraph313Clocks(board);
    if(auto* captures=LawnFusion::GetNativeCapturedPlants314(board))captures->Prune();
    prune(context.droppedCards269,board->mCoins);prune(context.fertilizeCoins269,board->mCoins);prune(context.rollingPlants269,board->mPlants);prune(context.wheatPlants269,board->mPlants);
}
bool NetCaptureNativeFusionFrame(Board* board, LawnFusion::NativeSnapshotFrame& frame)
{
	auto* context = LawnFusion::GetNativeContext(board);
	if (!context || !board->mSeedBank) return false;
    NetPruneNativeFusionBridgeMetadata(board,*context);
	frame.epoch = context->epoch;
    frame.sourcePoints=context->sourcePoints;frame.extendedBoard313=context->extendedBoard313;
    frame.bossRoom366=context->bossRoom366;frame.bossRoom366.lastTick=-1;
    if(frame.bossRoom366.trophyId&&!(frame.bossRoom366.won&&frame.bossRoom366.trophyClaimed))
    {
        // Host capture proves the real generation. The transmitted token stays
        // opaque on clients; their local Coin UID is never substituted for it.
        if(board->mApp->NetIsClient())return false;
        const auto* trophy=board->mCoins.DataArrayTryToGet(frame.bossRoom366.trophyToken);
        if(!trophy||trophy->mDead||trophy->mType!=COIN_TROPHY||trophy->mNetId!=frame.bossRoom366.trophyId)return false;
    }
    frame.worldRemainder269Board=context->worldRemainder269Board;frame.goldHypnoSummoned=context->goldHypnoSummoned;
	frame.background = static_cast<uint8_t>(board->mBackground);
	frame.selectionCommitted = context->selectionCommitted;
	frame.selection = context->selection.State();
	frame.gloveHolds = context->gloveHolds;
	frame.gloveCooldownTicks = context->gloveCooldownTicks;
	for (int i = 0; i < board->mSeedBank->mNumPackets; ++i)
	{
		const SeedPacket& packet = board->mSeedBank->mSeedPackets[i];
		if (const auto* data = LawnFusion::GetNativePacketData(&packet)) frame.packets.push_back({packet.mNetId, *data});
	}
	Plant* plant = nullptr;
	while (board->mPlants.IterateNext(plant))
		if (!plant->mDead && plant->mIsOnBoard)
		{
			const auto* data = LawnFusion::GetNativePlantData(plant);
			if (!data) return false;
            std::string visualStateError;
            if(!LawnFusion::NativePlantVisualStateSupported(plant,&visualStateError))
            {
                NetTrace("Fusion native presentation state unavailable kind=%u error=%s",data->kind,visualStateError.c_str());
                return false;
            }
			auto captured = *data;
			if (captured.kind != 1000 && LawnFusion::IsNativeProducerKind(captured.kind))
				captured.productionTicks = plant->mLaunchCounter;
			frame.plants.push_back({plant->mNetId, captured});
            if(const auto* equipment=LawnFusion::GetNativeEquipmentPlantState(plant))frame.equipmentPlants.push_back({plant->mNetId,*equipment});
            if(const auto* star=LawnFusion::GetNativeStar269PlantState(plant))frame.star269Plants.push_back({plant->mNetId,*star});
            if(const auto* doom=LawnFusion::GetNativeDoom269PlantState(plant))frame.doom269Plants.push_back({plant->mNetId,*doom});
            if(const auto* companions=LawnFusion::GetNativeCompanions269PlantState(plant))frame.companionPlants269.push_back({plant->mNetId,*companions});
            if(const auto* world=LawnFusion::GetNativeWorld269PlantState(plant))frame.worldPlants269.push_back({plant->mNetId,*world});
            if(const auto* rolling=LawnFusion::GetNativeRollingActor269State(plant))
            {LawnFusion::NativeRollingActor269Snapshot mirror;static_cast<LawnFusion::NativeRollingActor269State&>(mirror)=*rolling;mirror.health=plant->mPlantHealth;frame.rollingPlants269.push_back({plant->mNetId,mirror});}
            if(const auto* wheat=LawnFusion::GetNativeWheat269State(plant))
                frame.wheatPlants269.push_back({plant->mNetId,{wheat->isFromWheat,wheat->wheatTime}});
		}
	Projectile* projectile = nullptr;
	while (board->mProjectiles.IterateNext(projectile))
		if (!projectile->mDead)
        {
            if(const auto* obstacle=LawnFusion::GetNativeSourceObstacle313(projectile))
            {
                auto captured=static_cast<const LawnFusion::FusionSourceObstacle313View&>(*obstacle);
                captured.ageTicks=projectile->mProjectileAge;captured.velocityX=projectile->mVelX;captured.velocityY=projectile->mVelY;
                captured.velocityZ=projectile->mVelZ;captured.accelerationZ=projectile->mAccZ;
                if(!LawnFusion::CaptureNativeObstacle313Clock(projectile,captured.clock)||!LawnFusion::FusionSourceObstacle313ViewValid(captured))return false;
                frame.sourceObstacles313.push_back({projectile->mNetId,captured});
            }
            if (const auto* data = LawnFusion::GetNativeProjectileData(projectile))
            {
                frame.projectiles.push_back({projectile->mNetId,*data});
                if(const auto* star=LawnFusion::GetNativeStar269ProjectileState(projectile))frame.star269Projectiles.push_back({projectile->mNetId,*star});
            }
            if(const auto* companion=LawnFusion::GetNativeCompanion269State(projectile))
            {auto captured=*companion;captured.ageTicks=projectile->mProjectileAge;frame.companions269.push_back({projectile->mNetId,captured});}
            if(const auto* beam=LawnFusion::GetNativeWorld269BeamState(projectile))
            {auto captured=*beam;captured.ageTicks=projectile->mProjectileAge;frame.worldBeams269.push_back({projectile->mNetId,captured});}
            if(const auto* aux=LawnFusion::GetNativeWorldRemainder269AuxState(projectile))
            {auto captured=*aux;captured.ageTicks=projectile->mProjectileAge;frame.worldRemainder269Aux.push_back({projectile->mNetId,captured});}
            if(const auto* shot=LawnFusion::GetNativeWorldRemainder269ShotState(projectile))
            {auto captured=*shot;captured.ageTicks=projectile->mProjectileAge;frame.worldRemainder269Shots.push_back({projectile->mNetId,captured});}
        }
	Zombie* zombie = nullptr;
	while (board->mZombies.IterateNext(zombie))
	{
		if (!zombie->mDead)
		{
            const uint32_t token=board->mZombies.DataArrayGetID(zombie);
            if(!zombie->mNetId||!token)return false;
            frame.zombieIdentities.push_back({zombie->mNetId,token});
            if (const auto* data = LawnFusion::GetNativeSourceZombieData(zombie))
            {
                auto captured=*data;
                if(data->kind==18)captured.vehicleStopped=zombie->mFlatTires;
                if(data->kind==104)
                {
                    if(zombie->mSummonCounter<0||zombie->mSummonCounter>75||
                       zombie->mBossHeadCounter<0||zombie->mBossHeadCounter>2||
                       !NetCaptureReanimState(board->mApp,zombie->mSpecialHeadReanimID,captured.paperHead))return false;
                    captured.paperShootTicks=uint8_t(zombie->mSummonCounter);
                    captured.paperShootEvent=uint8_t(zombie->mBossHeadCounter);
                }
                frame.sourceZombies.push_back({zombie->mNetId,captured});
                if(LawnFusion::FusionWaterZombie313Kind(data->kind)||LawnFusion::NativeHeavyZombie313BKind(data->kind)||LawnFusion::FindFusionExtendedZombie313Profile(data->kind)||LawnFusion::FusionBoss361Kind(data->kind))
                {
                    LawnFusion::Native313Snapshot mirror;std::string error;
                    if(!LawnFusion::CaptureNative313(zombie,mirror,&error)){NetTrace("FNS28 source313 capture rejected id=%u kind=%u error=%s",zombie->mNetId,data->kind,error.c_str());return false;}
                    if(mirror.graph)frame.sourceGraphs313.push_back({zombie->mNetId,*mirror.graph});
                    if(mirror.water)frame.waterZombies313.push_back({zombie->mNetId,*mirror.water});
                    if(mirror.heavy)frame.heavyZombies313.push_back({zombie->mNetId,*mirror.heavy});
                    if(mirror.extended)frame.extendedZombies313.push_back({zombie->mNetId,*mirror.extended});
                    if(mirror.boss)frame.bossZombies361.push_back({zombie->mNetId,*mirror.boss});
                }
                if(data->kind==45)
                {
                    const auto* state=LawnFusion::GetNativeHeavyZombie313B(zombie);auto* captures=LawnFusion::GetNativeCapturedPlants314(board);
                    LawnFusion::CapturedSourcePlant314View held;
                    if(captures&&captures->Capture(zombie,held))frame.capturedPlants314.push_back({zombie->mNetId,std::move(held)});
                    else if(state&&state->capturedPlantUID)return false;
                }
                if(LawnFusion::FusionJackbox300Kind(data->kind))
                {
                    const auto found=context->jackboxZombies300.find(token);
                    if(found==context->jackboxZombies300.end()||!found->second.initialized||found->second.failed)return false;
                    frame.jackboxZombies300.push_back({zombie->mNetId,static_cast<const LawnFusion::FusionJackbox300View&>(found->second)});
                }
                if(LawnFusion::FusionJacksonFamily367Kind(data->kind))
                {
                    const auto found=context->nightZombies281.find(token);
                    if(found==context->nightZombies281.end()||!found->second.initialized||found->second.failed)return false;
                    frame.nightZombies281.push_back({zombie->mNetId,static_cast<const LawnFusion::FusionNightZombie281View&>(found->second)});
                }

                if(const auto* day=LawnFusion::GetNativeDayZombie273(zombie))
                {
                    auto state=*day;
                    if(!NetCaptureReanimState(board->mApp,zombie->mSpecialHeadReanimID,state.head))return false;
                    frame.dayZombies273.push_back({zombie->mNetId,state});
                }

            }
			if (const auto* data = LawnFusion::GetNativeZombieColdData(zombie))
			{
				auto captured = *data;
				captured.chilledTicks = zombie->mChilledCounter;
				captured.frozenTicks = zombie->mIceTrapCounter;
				frame.coldZombies.push_back({zombie->mNetId, captured});
			}
			if (const auto* data = LawnFusion::GetNativeZombieButterData(zombie))
			{
				auto captured = *data;
				captured.ticks = zombie->mButteredCounter;
				captured.visible = captured.ticks > 0 && data->visible;
				frame.butterZombies.push_back({zombie->mNetId,captured});
			}
			if (const auto* data = LawnFusion::GetNativeZombieActionData(zombie))
			{
				auto captured = *data; captured.scale = zombie->mScaleZombie;
				frame.zombieActions.push_back({zombie->mNetId,captured});
			}
            if(const auto* row=LawnFusion::GetNativeZombieGarlicData(zombie))
            {
                const auto* source=LawnFusion::GetNativeSourceZombieData(zombie);
                const bool sourceMachine=source&&LawnFusion::FusionSourceMachineRowMoving323(source->kind,*row);
                if(LawnFusion::FusionGarlicActionActive(*row)&&!zombie->IsDeadOrDying()&&zombie->mHasHead&&
                    LawnFusion::IsNativeFusionGroundZombie(zombie)&&zombie->mZombiePhase==PHASE_ZOMBIE_NORMAL&&(!zombie->mMindControlled||sourceMachine))
                    frame.garlicZombies.push_back({zombie->mNetId,*row});
            }
		}
	}
	Coin* coin = nullptr;
	while (board->mCoins.IterateNext(coin))
		if (!coin->mDead)
		{
            if(const auto* equipment=LawnFusion::GetNativeEquipmentData(coin))
            {frame.equipmentCoins.push_back({coin->mNetId,*equipment});continue;}
            if(const auto* fertilizer=LawnFusion::GetNativeFertilize269Data(coin)){frame.fertilizeCoins269.push_back({coin->mNetId,*fertilizer});continue;}
            if(const auto* card=LawnFusion::GetNativeDroppedCard269Data(coin))
            {frame.droppedCards269.push_back({coin->mNetId,*card});continue;}
			const int value = LawnFusion::GetNativeCoinValue(coin, -1);
			if (value != -1) frame.coins.push_back({coin->mNetId, {value,LawnFusion::IsNativeMoneyCoin(coin)}});
		}
    frame.travelBuff369=board->mApp->mFusionTravelBuff369;frame.travelBuffCooldown369=context->travelBuffCooldown369;
	frame.money = context->money;
	frame.adventure = context->adventure;
	frame.temporaryLightLevel = context->temporaryLightLevel;
    LawnFusion::ProjectNativeStar269LiveTargets(frame);
	return true;
}

// Structural preflight adds reference validation for Fusion metadata before the
// existing native decoder touches the board. It deliberately does not recreate
// that decoder's gameplay, animation or presentation validation.
bool NetPreflightNativeFusionBody(const Board* board,const NetBuffer& body, const LawnFusion::NativeSnapshotFrame& frame)
{
	const size_t coreBytes = 43 + MAX_GRID_SIZE_Y * 4 + 9 + 10 +
		NET_CHALLENGE_RUNTIME_WIRE_BYTES + NET_ENDLESS_WEATHER_WIRE_BYTES + NET_ZEN_GARDEN_WIRE_BYTES;
	if (body.Size() < coreBytes + 7) return false;
	NetBuffer scan = body;
	scan.mReadPos = coreBytes + 6;
	const uint8_t packetCount = scan.ReadU8Or();
	if (packetCount > SEEDBANK_CAPACITY || scan.Remaining() < packetCount * NET_SEED_PACKET_WIRE_BYTES) return false;
    const bool sourceConveyor=LawnFusion::FusionAdventureBankPolicy308(frame.adventure)==LawnFusion::FusionBankPolicy::SourceConveyor;
    if(sourceConveyor)
    {
        NetBuffer raw=body;raw.mReadPos=coreBytes;
        const int x=raw.ReadI16Or(),y=raw.ReadI16Or();
        if(x< -2048||x>2048||y< -1024||y>1024)return false;
        raw.mReadPos=scan.mReadPos;
        std::array<NetMysterySeedSnapshot,13> packets;
        if(packetCount!=packets.size())return false;
        const NetMysteryLimits limits={(uint8_t)NUM_SEEDS_IN_CHOOSER,(uint8_t)SEED_IMITATER,(uint8_t)SEED_GATLINGPEA};
        for(auto& packet:packets)
            if(!NetDecodeMysterySeedSnapshot(raw,packet,limits,(uint8_t)NUM_SEED_TYPES,
                [](uint8_t type){return type==255||type<(uint8_t)NUM_SEED_TYPES;},[](uint8_t type){return type==255;}))return false;
        if(!LawnFusion::FusionConveyorRaw348Valid(frame.selection,std::span(frame.packets),std::span<const NetMysterySeedSnapshot>(packets),
            [](uint16_t kind){return uint8_t(LawnFusion::GetNativeSeedForPlant(kind));}))return false;
    }
	std::set<uint16_t> allIds, packetIds, plantIds, zombieIds, projectileIds, coinIds;
	for (int i = 0; i < packetCount; ++i)
	{
		const size_t start = scan.mReadPos;
		const uint16_t id = scan.ReadU16Or();
		const uint8_t type = scan.ReadU8Or();
		const uint8_t owner = scan.mData[start + 5], origin = scan.mData[start + 16];
		const auto metadata = std::find_if(frame.packets.begin(), frame.packets.end(), [=](const auto& r) { return r.id == id; });
		if (id)
		{
			if (!allIds.insert(id).second || metadata == frame.packets.end()) return false;
			packetIds.insert(id);
			const auto& data = metadata->data;
			const uint8_t nativeType = static_cast<uint8_t>(LawnFusion::GetNativeSeedForPlant(data.kind));
			if (type != nativeType || owner != data.owner || origin != data.randomOrigin) return false;
            if(data.kind==245&&frame.selection.phase==LawnFusion::FusionBankPhase::Playing&&
               LawnFusion::FusionAdventureBankPolicy308(frame.adventure)!=LawnFusion::FusionBankPolicy::SourceConveyor&&
               LawnFusion::HasFusionTravelAdvanced360(frame.travelBuff369,32))
            {NetBuffer refresh=scan;refresh.mReadPos=start+17;if(refresh.ReadU16Or()!=5000)return false;}
		}
		else if (type != 255 || origin) return false;
		scan.mReadPos = start + NET_SEED_PACKET_WIRE_BYTES;
	}
	if (packetIds.size() != frame.packets.size()) return false;
	const size_t sizes[] = {NET_PLANT_WIRE_BYTES, NET_ZOMBIE_WIRE_BYTES, NET_PROJECTILE_WIRE_BYTES, NET_COIN_WIRE_BYTES, NET_MOWER_WIRE_BYTES, 37};
	for (int section = 0; section < 6; ++section)
	{
		uint16_t count = 0;
		if (!scan.ReadU16(&count) || count > (section == 4 ? NET_MOWER_MAX_COUNT : 1024) || count > scan.Remaining() / sizes[section]) return false;
		for (int i = 0; i < count; ++i)
		{
			const size_t start = scan.mReadPos;
			const uint16_t id = scan.ReadU16Or();
			const uint8_t type = scan.ReadU8Or();
			if (!id || !allIds.insert(id).second) return false;
			if (section == 0)
			{
				if (scan.mData[start + 4] >= LawnFusion::NativeFusionRowsForBackground(frame.background) ||
					scan.mData[start + 5] >= LawnFusion::kNativeFusionColumns) return false;
				plantIds.insert(id);
				const auto record = std::find_if(frame.plants.begin(), frame.plants.end(), [=](const auto& r) { return r.id == id; });
				if (record == frame.plants.end() || type != static_cast<uint8_t>(LawnFusion::GetNativeSeedForPlant(record->data.kind)) ||
					(record->data.packetId && !sourceConveyor && !packetIds.contains(record->data.packetId))) return false;
                NetBuffer plantBody=scan;plantBody.mReadPos=start+6;
                const int current=plantBody.ReadU16Or(),maximum=plantBody.ReadU16Or();
                const auto state=plantBody.ReadU8Or(),flags=plantBody.ReadU8Or();const int x=plantBody.ReadI16Or(),y=plantBody.ReadI16Or(),order=plantBody.ReadI32Or();
                uint8_t maximumState=static_cast<uint8_t>(STATE_LILYPAD_INVULNERABLE);
#ifdef _DS_MINIGAMES
                maximumState=static_cast<uint8_t>(STATE_HEAT_WAVE_POWERED);
#endif
                if(scan.mData[start+3]!=255&&scan.mData[start+3]>=static_cast<uint8_t>(NUM_SEED_TYPES))return false;
                if(state>maximumState||(flags&~3)||x< -2048||x>2048||y< -2048||y>2048||order<0||order>1000000)return false;
                if(!NetFusionPlantHealthValid(&frame,id,current,maximum))return false;
                if(const auto* cannon=LawnFusion::FindFusionCannon268Profile(record->data.kind);cannon&&cannon->manual)
                {
                    const auto mode=record->data.batchMode;
                    const auto expected=mode==0?STATE_COBCANNON_ARMING:mode==1?STATE_COBCANNON_LOADING:mode==2?STATE_COBCANNON_READY:STATE_COBCANNON_FIRING;
                    if(scan.mData[start+NET_PLANT_STATE_OFFSET]!=(uint8_t)expected)return false;
                }
                if(LawnFusion::FindFusionEquipmentProfile(record->data.kind)&&
                    std::none_of(frame.equipmentPlants.begin(),frame.equipmentPlants.end(),[=](const auto& r){return r.id==id;}))return false;
				if (record->data.shootSeconds >= 0 && !LawnFusion::FindFusionCannon268Profile(record->data.kind)&&
                    !LawnFusion::IsFusionWater268Kind(record->data.kind)&&!LawnFusion::FindFusionEquipmentProfile(record->data.kind)&&
                    !LawnFusion::FindFusionStrongChomperProfile(record->data.kind)&&!LawnFusion::IsFusionShulk268Kind(record->data.kind))
				{
					const bool digest=LawnFusion::FindFusionChomperProfile(record->data.kind)!=nullptr;
					const auto* timing = digest?nullptr:LawnFusion::FindFusionShooterTimingProfile(record->data.kind);
                    if(record->data.kind==924||record->data.kind==1026)
                        timing=LawnFusion::FindFusionHypno269TimingProfile(record->data.kind,record->data.batchBagCount);
					if (scan.mData[start + NET_PLANT_STATE_OFFSET] == static_cast<uint8_t>(STATE_KERNELPULT_BUTTER))
						if (const auto* alternate = LawnFusion::FindFusionShooterTimingProfile(record->data.kind,
							record->data.kind == 1025 ? "shoot1" : "shoot2")) timing = alternate;
					const bool multiplePhases=!digest&&(LawnFusion::IsFusionShooterBatchKind(record->data.kind) ||
                            LawnFusion::FindFusionExtendedLobberTimingProfile(record->data.kind)!=nullptr ||
                            LawnFusion::FusionFireActionMaximumDuration(record->data.kind,record->data.animationSpeed)>0);
                        const float duration=multiplePhases ? LawnFusion::FusionShooterMaximumDuration(record->data.kind,record->data.animationSpeed) :
                            timing ? LawnFusion::FusionShooterDuration(*timing,record->data.animationSpeed) : 0;
                        if (duration>0 && record->data.shootSeconds>=duration) return false;
				}
			}
			else if (section == 1)
			{
				if (scan.mData[start + 3] >= LawnFusion::NativeFusionRowsForBackground(frame.background)) return false;
				zombieIds.insert(id);
                NetBuffer rawZombie=scan;rawZombie.mReadPos=start+4;const int from=rawZombie.ReadI16Or(),order=rawZombie.ReadI32Or();rawZombie.ReadI16Or();rawZombie.ReadI16Or();
                NetZombieHealthState health;if(!NetDecodeZombieHealthState(rawZombie,health))return false;
                const auto phase=rawZombie.ReadU8Or(),height=rawZombie.ReadU8Or(),helm=rawZombie.ReadU8Or(),shield=rawZombie.ReadU8Or();
                rawZombie.mReadPos=start+56;const float velocity=rawZombie.ReadF32Or(),altitude=rawZombie.ReadF32Or(),rate=rawZombie.ReadF32Or();
                NetZombieAnimState animation;NetZombatarAppearance appearance;uint32_t capacity=0;
#ifdef _HAS_BLOOM_AND_DOOM_CONTENTS
                const auto maxPhase=uint8_t(PHASE_DOG_FREE);
#else
                const auto maxPhase=uint8_t(PHASE_ZAMBONI_FLAT);
#endif
                if(type>=NUM_ZOMBIE_TYPES||from<Zombie::ZOMBIE_WAVE_WINNER||order<0||order>1000000||phase>maxPhase||height>HEIGHT_ZOMBIQUARIUM||helm>HELMTYPE_BLACK_FOOTBALL||shield>SHIELDTYPE_TRASHCAN||
                    !_finite(velocity)||std::abs(velocity)>2048||!_finite(altitude)||std::abs(altitude)>8192||!_finite(rate)||rate<=0||rate>1000||
                    !NetDecodeZombieAnimState(rawZombie,animation)||!NetDecodeZombatar(rawZombie,appearance)||
                    (appearance.mPresent&&type!=ZOMBIE_NORMAL&&type!=ZOMBIE_FLAG)||!NetGetZombieTypeFrameCapacity(type,capacity)||!NetZombieAnimStateFitsFrames(animation,capacity))return false;
                if(std::none_of(frame.zombieIdentities.begin(),frame.zombieIdentities.end(),[=](const auto& r){return r.id==id;}))return false;
                const auto capture=std::find_if(frame.plants.begin(),frame.plants.end(),[=](const auto& p){return p.data.kind!=1175&&p.data.waterTargetId==id&&(p.data.batchMode==1||p.data.batchMode==2);});
                NetBuffer capturedBody=scan;capturedBody.mReadPos=start+14;NetZombieHealthState capturedHealth;
                if(!NetDecodeZombieHealthState(capturedBody,capturedHealth))return false;
                capturedBody.ReadU8Or();const auto capturedHeight=capturedBody.ReadU8Or();
                capturedBody.ReadU8Or();capturedBody.ReadU8Or();const auto capturedFlags=capturedBody.ReadU8Or();
                if(capture!=frame.plants.end()&&(!capturedHealth.mBody||(capturedFlags&128)||
                    capturedHeight!=(capture->data.batchMode==2?HEIGHT_DRAGGED_UNDER:HEIGHT_ZOMBIE_NORMAL)))return false;
				const auto source = std::find_if(frame.sourceZombies.begin(),frame.sourceZombies.end(),[=](const auto& r) { return r.id == id; });
				if (source != frame.sourceZombies.end())
				{
					NetBuffer sourceBody = scan; sourceBody.mReadPos = start+14;
					NetZombieHealthState sourceHealth;
					if (!NetDecodeZombieHealthState(sourceBody,sourceHealth) ||
						!LawnFusion::NativeSourceZombieDataValid(source->data,static_cast<int>(sourceHealth.mBodyMax)) ||
						type != static_cast<uint8_t>(LawnFusion::GetNativeSourceZombieCarrier(source->data.kind))) return false;
                    const auto identity=std::find_if(frame.zombieIdentities.begin(),frame.zombieIdentities.end(),[=](const auto& r){return r.id==id;});
                    if(identity==frame.zombieIdentities.end()||identity->data!=source->data.instanceToken)return false;
                    const auto* sourceProfile=LawnFusion::FindFusionZombieProfile(source->data.kind);
                    if(!NetPaperHeadFitsNativeFrames(source->data.paperHead))return false;
                    const auto day=std::find_if(frame.dayZombies273.begin(),frame.dayZombies273.end(),
                        [=](const auto& value){return value.id==id;});
                    if(day!=frame.dayZombies273.end()&&!NetPaperHeadFitsNativeFrames(day->data.head))return false;
					const int sourceRow=scan.mData[start+3];
					const bool incomingDirect362=frame.adventure.enabled||(LawnFusion::GetNativeContext(board)&&LawnFusion::GetNativeContext(board)->validation);
                    if(sourceProfile&&sourceProfile->waterOnly&&!incomingDirect362&&(LawnFusion::NativeFusionRowsForBackground(frame.background)!=6||(sourceRow!=2&&sourceRow!=3)))return false;
					const auto sourcePhase = sourceBody.ReadU8Or(), sourceHeight = sourceBody.ReadU8Or();
					const bool sourcePoolTransition=(sourceHeight==HEIGHT_IN_TO_POOL||sourceHeight==HEIGHT_OUT_OF_POOL)&&
						LawnFusion::NativeFusionRowsForBackground(frame.background)==6&&(sourceRow==2||sourceRow==3);
					if (sourcePhase == PHASE_ZOMBIE_NORMAL &&
						((sourceHeight != HEIGHT_ZOMBIE_NORMAL&&!sourcePoolTransition&&!(capture!=frame.plants.end()&&capture->data.batchMode==2&&sourceHeight==HEIGHT_DRAGGED_UNDER)) || sourceHealth.mBody !=
						 static_cast<uint32_t>(LawnFusion::NativeSourceZombieLivenessHealth(source->data.health)))) return false;
				}
				const auto record = std::find_if(frame.coldZombies.begin(), frame.coldZombies.end(), [=](const auto& r) { return r.id == id; });
				if (record != frame.coldZombies.end() &&
					(scan.mData[start + NET_ZOMBIE_CHILLED_OFFSET] != static_cast<uint8_t>(min(255, record->data.chilledTicks)) ||
					 scan.mData[start + NET_ZOMBIE_FROZEN_OFFSET] != static_cast<uint8_t>(min(255, record->data.frozenTicks)))) return false;
				const auto butter = std::find_if(frame.butterZombies.begin(), frame.butterZombies.end(), [=](const auto& r) { return r.id == id; });
				if (butter != frame.butterZombies.end() && scan.mData[start + NET_ZOMBIE_BUTTERED_OFFSET] !=
					static_cast<uint8_t>(min(255,butter->data.ticks))) return false;
				const auto garlic = std::find_if(frame.garlicZombies.begin(),frame.garlicZombies.end(),[=](const auto& r) { return r.id == id; });
				if (garlic != frame.garlicZombies.end())
				{
					NetBuffer body = scan; body.mReadPos = start+6;
					const int renderOrder = body.ReadI32Or();
					body.ReadI16Or(); const int y4 = body.ReadI16Or();
					NetZombieHealthState health;
					if (!NetDecodeZombieHealthState(body,health) || !health.mBody) return false;
					const auto phase = body.ReadU8Or(), height = body.ReadU8Or();
					body.ReadU8Or(); body.ReadU8Or(); const auto flags = body.ReadU8Or();
					const auto& state = garlic->data;
					const int row = scan.mData[start+3];
					const bool sourceCarrier = source != frame.sourceZombies.end() && type == LawnFusion::GetNativeSourceZombieCarrier(source->data.kind);
                    const bool sourceMachine=sourceCarrier&&LawnFusion::FusionSourceMachineRowMoving323(source->data.kind,state);
                    if ((type != ZOMBIE_NORMAL && !sourceCarrier) || phase != PHASE_ZOMBIE_NORMAL || height != HEIGHT_ZOMBIE_NORMAL ||
                        !LawnFusion::NativeGarlicRawFlagsValid(frame.background,row,sourceMachine?uint8_t(flags&~128):flags) || LawnFusion::FusionGarlicLogicalRow(state,row) != row)
                    {
                        NetTrace("Fusion garlic raw state rejected id=%u background=%u row=%d flags=%u type=%u phase=%u height=%u",id,frame.background,row,flags,type,phase,height);
                        return false;
                    }
					if (state.phase == LawnFusion::GarlicActionPhase::Moving)
					{
						const float y = static_cast<float>(double(state.startY)+(double(state.targetY)-state.startY)*state.moveElapsedTicks/state.moveDurationTicks);
						if (y4 != static_cast<int16_t>(y*4) || (!sourceMachine&&(flags & 1)) ||
							renderOrder != Board::MakeRenderOrder(RENDER_LAYER_ZOMBIE,LawnFusion::FusionGarlicSortingRow(state,row),4)) return false;
					}
				}
			}
			else if (section == 2)
			{
				if (scan.mData[start + 3] >= LawnFusion::NativeFusionRowsForBackground(frame.background)) return false;
				projectileIds.insert(id);
                const auto companion=std::find_if(frame.companions269.begin(),frame.companions269.end(),[=](const auto& c){return c.id==id;});
                if(companion!=frame.companions269.end()&&(type!=PROJECTILE_PEA||scan.mData[start+28]!=MOTION_STAR))return false;
                const auto beam=std::find_if(frame.worldBeams269.begin(),frame.worldBeams269.end(),[=](const auto& c){return c.id==id;});
                if(beam!=frame.worldBeams269.end()&&(type!=PROJECTILE_PEA||scan.mData[start+28]!=MOTION_STAR))return false;
                const auto aux=std::find_if(frame.worldRemainder269Aux.begin(),frame.worldRemainder269Aux.end(),[=](const auto& c){return c.id==id;});
                if(aux!=frame.worldRemainder269Aux.end()&&(type!=PROJECTILE_PEA||scan.mData[start+28]!=MOTION_STAR))return false;
                const auto obstacle=std::find_if(frame.sourceObstacles313.begin(),frame.sourceObstacles313.end(),[=](const auto& r){return r.id==id;});
                if(obstacle!=frame.sourceObstacles313.end())
                {
                    if(!NetSourceObstacle313RawValid(scan,start,obstacle->data))return false;
                    if(std::any_of(frame.projectiles.begin(),frame.projectiles.end(),[=](const auto& r){return r.id==id;})||
                        companion!=frame.companions269.end()||beam!=frame.worldBeams269.end()||aux!=frame.worldRemainder269Aux.end()||
                        std::any_of(frame.worldRemainder269Shots.begin(),frame.worldRemainder269Shots.end(),[=](const auto& r){return r.id==id;}))return false;
                }
                NetBuffer rawSpin=scan;rawSpin.mReadPos=start+31;const float rotationSpeed=rawSpin.ReadF32Or();
                if(!_finite(rotationSpeed)||rotationSpeed< -100000.f||rotationSpeed>100000.f)return false;
				const auto record = std::find_if(frame.projectiles.begin(), frame.projectiles.end(), [=](const auto& r) { return r.id == id; });
				if (record != frame.projectiles.end())
				{
					const auto& data = record->data;
                    const auto state=std::find_if(frame.star269Projectiles.begin(),frame.star269Projectiles.end(),[=](const auto& s){return s.id==id;});
                    const auto* star=state==frame.star269Projectiles.end()?nullptr:&state->data;
                    NetBuffer rotation=scan;rotation.mReadPos=start+24;const float rootAngle=rotation.ReadF32Or();
                    if(!LawnFusion::NativeProjectileRawRotationValid(data,static_cast<ProjectileType>(type),static_cast<ProjectileMotion>(scan.mData[start+28]),rootAngle,rotationSpeed,star))return false;
					if (!LawnFusion::NativeProjectileMetadataValid(data,static_cast<ProjectileType>(type),
						static_cast<ProjectileMotion>(scan.mData[start + 28]),star)) return false;
				}
			}
			else if (section == 3)
			{
                if(id==frame.bossRoom366.trophyId&&type!=COIN_TROPHY)return false;
				coinIds.insert(id);
                NetBuffer clock=scan;clock.mReadPos=start+30;const int age=clock.ReadI32Or(),lifetime=clock.ReadI32Or();
                NetBuffer prefix=scan;prefix.mReadPos=start+23;
                if(age<0||lifetime<0||prefix.ReadU16Or()!=(std::min)(65535,lifetime))return false;
                const auto equipment=std::find_if(frame.equipmentCoins.begin(),frame.equipmentCoins.end(),[=](const auto& r){return r.id==id;});
                const auto fertilizer=std::find_if(frame.fertilizeCoins269.begin(),frame.fertilizeCoins269.end(),[=](const auto& r){return r.id==id;});
                if(type==(uint8_t)COIN_NONE?(equipment==frame.equipmentCoins.end()&&fertilizer==frame.fertilizeCoins269.end()):equipment!=frame.equipmentCoins.end()||fertilizer!=frame.fertilizeCoins269.end())return false;
                if(fertilizer!=frame.fertilizeCoins269.end()&&(age<fertilizer->data.phaseStartAge||scan.mData[start+38]||scan.mData[start+21]!=255||(scan.mData[start+20]&5)))return false;
                if(equipment!=frame.equipmentCoins.end()&&!NetFusionEquipmentCoinRawValid(body,start,equipment->data))return false;
                const auto card=std::find_if(frame.droppedCards269.begin(),frame.droppedCards269.end(),[=](const auto& r){return r.id==id;});
                if(card!=frame.droppedCards269.end())
                {if(type!=COIN_USABLE_SEED_PACKET||scan.mData[start+21]!=LawnFusion::GetNativeSeedForPlant(card->data.kind)||
                    !LawnFusion::NativeDroppedCard269CoinStateValid(card->data,age,scan.mData[start+38],lifetime)||
                    bool(scan.mData[start+20]&2)!=card->data.landed||(scan.mData[start+20]&5))return false;}
                else if(scan.mData[start+38])return false;
				const auto record = std::find_if(frame.coins.begin(), frame.coins.end(), [=](const auto& r) { return r.id == id; });
				if (type == (uint8_t)COIN_SMALLSUN && record == frame.coins.end()) return false;
				if (record != frame.coins.end())
				{
					const auto& value = record->data;
					if (value.money ? !LawnFusion::FusionMoneyCoinMetadataValid(static_cast<CoinType>(type),value.value) :
						!LawnFusion::FusionSunCoinMetadataValid(type,value.value)) return false;
				}
			}
			else if (section == 4)
			{
				if (scan.mData[start + 2] >= LawnFusion::NativeFusionRowsForBackground(frame.background)) return false;
			}
			else if (section == 5)
			{
                if(NetHas313(frame)&&type!=GRIDITEM_LADDER)return false;
				if (scan.mData[start + 4] >= LawnFusion::kNativeFusionColumns ||
					scan.mData[start + 5] >= LawnFusion::NativeFusionRowsForBackground(frame.background)) return false;
			}
			scan.mReadPos = start + sizes[section];
		}
	}
	if (scan.Remaining() || plantIds.size() != frame.plants.size()||zombieIds.size()!=frame.zombieIdentities.size()) return false;
    for(const auto& r:frame.zombieIdentities)if(!zombieIds.contains(r.id))return false;
    for(const auto& r:frame.equipmentCoins)
    {
        if(!coinIds.contains(r.id))return false;
        if(r.data.sourcePlantId&&!r.data.weapon&&std::none_of(frame.equipmentPlants.begin(),frame.equipmentPlants.end(),[&](const auto& source)
            {return source.id==r.data.sourcePlantId&&source.data.targetItemId==r.id&&source.data.targetItemToken==r.data.instanceToken;}))return false;
    }
    for(const auto& r:frame.equipmentPlants)if(!plantIds.contains(r.id))return false;
	for (const auto& record : frame.projectiles) if (!projectileIds.contains(record.id)) return false;
	for (const auto& record : frame.coldZombies) if (!zombieIds.contains(record.id)) return false;
	for (const auto& record : frame.butterZombies) if (!zombieIds.contains(record.id)) return false;
	for (const auto& record : frame.zombieActions) if (!zombieIds.contains(record.id)) return false;
	for (const auto& record : frame.garlicZombies) if (!zombieIds.contains(record.id)) return false;
	for (const auto& record : frame.sourceZombies) if (!zombieIds.contains(record.id)) return false;
    for(const auto& record:frame.dayZombies273)if(!zombieIds.contains(record.id))return false;
    for(const auto& record:frame.nightZombies281)if(!zombieIds.contains(record.id))return false;
    for(const auto& record:frame.jackboxZombies300)if(!zombieIds.contains(record.id))return false;
    for(const auto& record:frame.sourceGraphs313)if(!zombieIds.contains(record.id))return false;
    for(const auto& record:frame.waterZombies313)if(!zombieIds.contains(record.id))return false;
    for(const auto& record:frame.heavyZombies313)if(!zombieIds.contains(record.id))return false;
    for(const auto& record:frame.extendedZombies313)if(!zombieIds.contains(record.id))return false;
    for(const auto& record:frame.bossZombies361)if(!zombieIds.contains(record.id))return false;
    if(frame.bossRoom366.trophyId&&!coinIds.contains(frame.bossRoom366.trophyId)&&!(frame.bossRoom366.won&&frame.bossRoom366.trophyClaimed))return false;
    for(const auto& record:frame.sourceObstacles313)if(!projectileIds.contains(record.id))return false;
    for(const auto& record:frame.capturedPlants314)if(!zombieIds.contains(record.id))return false;
    if((!frame.sourceGraphs313.empty()||!frame.waterZombies313.empty()||!frame.sourceObstacles313.empty()||!frame.capturedPlants314.empty())&&
       (frame.background>BACKGROUND_6_BOSS||frame.sourceZombies.size()!=zombieIds.size()))return false;
    // Raw source row bounds also cover six-row dry births and genuine Bungee
    // target-row movement; canonical family state is validated above.
	for (const auto& record : frame.coins) if (!coinIds.contains(record.id)) return false;
    for(const auto& record:frame.companions269)if(!projectileIds.contains(record.id))return false;
    for(const auto& record:frame.companionPlants269)if(!plantIds.contains(record.id))return false;
    for(const auto& record:frame.worldPlants269)if(!plantIds.contains(record.id))return false;
    for(const auto& record:frame.worldBeams269)if(!projectileIds.contains(record.id))return false;
    for(const auto& record:frame.worldRemainder269Aux)if(!projectileIds.contains(record.id))return false;
    for(const auto& record:frame.worldRemainder269Shots)if(!projectileIds.contains(record.id))return false;
    for(const auto& record:frame.droppedCards269)if(!coinIds.contains(record.id))return false;
    for(const auto& record:frame.fertilizeCoins269)if(!coinIds.contains(record.id))return false;
    for(const auto& record:frame.rollingPlants269)if(!plantIds.contains(record.id))return false;
    for(const auto& record:frame.wheatPlants269)if(!plantIds.contains(record.id))return false;
	return true;
}

bool NetPreflight313Resources(Board* board,const NetBuffer& body,const LawnFusion::NativeSnapshotFrame& frame,
    LawnFusion::FusionSourceDeathBudget319& out)
{
    using namespace LawnFusion;
    if(!NetHas313(frame)){out={};return true;}
    if(!board||!board->mApp||board->mApp->mBoard!=board||!board->mTDPath.empty()||board->GetColumnCount()!=10||frame.background>BACKGROUND_6_BOSS)return false;
    std::vector<Native313Snapshot> mirrors;std::vector<Native313GhostRequest> requests;
    mirrors.reserve(frame.sourceZombies.size());requests.reserve(frame.sourceZombies.size());
    // Resource-only cache admission before raw actors/background are changed.
    if(frame.bossRoom366.enabled&&!EnsureNativeBossRoom366(board))return false;
    NetBuffer scan=body;scan.mReadPos=43+MAX_GRID_SIZE_Y*4+9+10+NET_CHALLENGE_RUNTIME_WIRE_BYTES+NET_ENDLESS_WEATHER_WIRE_BYTES+NET_ZEN_GARDEN_WIRE_BYTES+6;
    uint8_t packets=0;if(!scan.ReadU8(&packets)||scan.Remaining()<size_t(packets)*NET_SEED_PACKET_WIRE_BYTES)return false;
    scan.mReadPos+=size_t(packets)*NET_SEED_PACKET_WIRE_BYTES;uint16_t plants=0,zombies=0;
    if(!scan.ReadU16(&plants)||scan.Remaining()<size_t(plants)*NET_PLANT_WIRE_BYTES)return false;
    scan.mReadPos+=size_t(plants)*NET_PLANT_WIRE_BYTES;
    if(!scan.ReadU16(&zombies)||scan.Remaining()<size_t(zombies)*NET_ZOMBIE_WIRE_BYTES)return false;
    for(unsigned n=0;n<zombies;++n)
    {
        const size_t start=scan.mReadPos;const uint16_t id=scan.ReadU16Or();const uint8_t carrier=scan.ReadU8Or(),row=scan.ReadU8Or();
        const auto source=std::find_if(frame.sourceZombies.begin(),frame.sourceZombies.end(),[=](const auto& r){return r.id==id;});
        if(source==frame.sourceZombies.end())return false;
        const auto kind=source->data.kind;auto* existing=board->NetGetGhostZombie(id);const auto* previous=LawnFusion::GetNativeSourceZombieData(existing);
        if(previous&&previous->instanceToken==source->data.instanceToken&&previous->kind!=kind)return false;
        if(existing&&(!previous||previous->instanceToken!=source->data.instanceToken||int(existing->mZombieType)!=carrier))existing=nullptr;
        if(FusionWaterZombie313Kind(kind))
        {
            const int rows=NativeFusionRowsForBackground(frame.background);uint8_t water[6]{};if(rows==6)water[2]=water[3]=1;
            if(!FusionWaterZombie313WorldValid(kind,rows,rows==6,std::span<const uint8_t>(water,rows),row,bool(scan.mData[start+50]&128)))return false;
        }
        if(FusionWaterZombie313Kind(kind)||NativeHeavyZombie313BKind(kind)||FindFusionExtendedZombie313Profile(kind)||FusionBoss361Kind(kind))
        {
            NetBuffer clock=scan;clock.mReadPos=start+68;NetZombieAnimState bodyClock;
            if(!NetDecodeZombieAnimState(clock,bodyClock))return false;
            mirrors.emplace_back();NetGather313(frame,id,mirrors.back());requests.push_back({existing,kind,bodyClock,&mirrors.back()});
        }
        scan.mReadPos=start+NET_ZOMBIE_WIRE_BYTES;
    }
    FusionSourceDeathBudget319 sum;std::string error;
    if(!PreflightNative313GhostBatch(board,requests,sum,&error)){NetTrace("FNS28 source313 resource/aux preflight rejected: %s",error.c_str());return false;}
    for(const auto& r:frame.sourceObstacles313)
    {
        auto* old=board->NetGetGhostProjectile(r.id);const auto* previous=GetNativeSourceObstacle313(old);
        if(previous&&previous->instanceToken==r.data.instanceToken&&(previous->kind!=r.data.kind||previous->variant!=r.data.variant))return false;
        if(!previous||previous->instanceToken!=r.data.instanceToken)old=nullptr;
        FusionSourceDeathBudget319 add;
        if(!PreflightNativeSourceObstacle313(board,r.data,old,add,&error)||!AddFusionSourceDeathBudget319(sum,add,sum))
        {NetTrace("FNS28 source obstacle preflight rejected id=%u error=%s",r.id,error.c_str());return false;}
    }
    auto* captures=GetNativeCapturedPlants314(board);
    if(!frame.capturedPlants314.empty()&&!captures)return false;
    for(const auto& r:frame.capturedPlants314)
    {
        if(!captures->PreflightAdopt(r.data,error)){NetTrace("FNS28 captured plant preflight rejected: %s",error.c_str());return false;}
        if(r.data.retainedActor)continue;
        FusionSourceDeathBudget319 add;add.reanimations=r.data.reanimations.size();
        std::set<std::pair<uint16_t,uint16_t>> containers;for(const auto& edge:r.data.edges)containers.emplace(edge.parent,edge.track);
        add.attachments=containers.size();if(!AddFusionSourceDeathBudget319(sum,add,sum))return false;
    }
    out=sum;return true;
}

bool NetBindNativeFusionFrame(Board* board, const LawnFusion::NativeSnapshotFrame& frame, const LawnFusion::FusionSeedBank& selection)
{
	auto* context = LawnFusion::GetNativeContext(board);
	if (!context) return false;
	// Missing native allocations cannot create an independent metadata actor.
	for (const auto& record : frame.plants) if (!board->NetGetGhostPlant(record.id)) return false;
	for (const auto& record : frame.projectiles) if (!board->NetGetGhostProjectile(record.id)) return false;
    for(const auto& record:frame.companions269)if(!board->NetGetGhostProjectile(record.id))return false;
    for(const auto& record:frame.companionPlants269)if(!board->NetGetGhostPlant(record.id))return false;
    for(const auto& record:frame.worldPlants269)if(!board->NetGetGhostPlant(record.id))return false;
    for(const auto& record:frame.worldBeams269)if(!board->NetGetGhostProjectile(record.id))return false;
    for(const auto& record:frame.worldRemainder269Aux)if(!board->NetGetGhostProjectile(record.id))return false;
    for(const auto& record:frame.worldRemainder269Shots)if(!board->NetGetGhostProjectile(record.id))return false;
    for(const auto& record:frame.droppedCards269)if(!board->NetGetGhostCoin(record.id))return false;
    for(const auto& record:frame.fertilizeCoins269)if(!board->NetGetGhostCoin(record.id))return false;
    for(const auto& record:frame.rollingPlants269)if(!board->NetGetGhostPlant(record.id))return false;
    for(const auto& record:frame.wheatPlants269)if(!board->NetGetGhostPlant(record.id))return false;
	for (const auto& record : frame.coldZombies)
	{
		const Zombie* zombie = board->NetGetGhostZombie(record.id);
		if (!zombie || zombie->mDead) return false;
	}
	for (const auto& record : frame.coins) if (!board->NetGetGhostCoin(record.id)) return false;
	for (const auto& record : frame.butterZombies)
	{
		const Zombie* zombie = board->NetGetGhostZombie(record.id);
		if (!zombie || zombie->mDead) return false;
	}
	for (const auto& record : frame.zombieActions)
	{
		const Zombie* zombie = board->NetGetGhostZombie(record.id);
		if (!zombie || zombie->mDead) return false;
	}
	for (const auto& record : frame.garlicZombies)
	{
		const Zombie* zombie = board->NetGetGhostZombie(record.id);
		if (!zombie || zombie->mDead) return false;
	}
	for (const auto& record : frame.sourceZombies)
	{
		const Zombie* zombie = board->NetGetGhostZombie(record.id);
		if (!zombie || zombie->mDead ||
			!LawnFusion::NativeSourceZombieDataValid(record.data,zombie->mBodyMaxHealth) ||
			zombie->mZombieType != LawnFusion::GetNativeSourceZombieCarrier(record.data.kind)) return false;
	}
    for(const auto& record:frame.zombieIdentities)
    {const auto* z=board->NetGetGhostZombie(record.id);if(!z||z->mDead)return false;}
    for(const auto& record:frame.equipmentCoins)
    {const auto* c=board->NetGetGhostCoin(record.id);if(!c||c->mDead||c->mType!=COIN_NONE||c->mCoinAge<record.data.phaseStartAge)return false;}
    for(const auto& record:frame.equipmentPlants)if(!board->NetGetGhostPlant(record.id))return false;
    for(const auto& record:frame.plants)if(record.data.waterTargetId)
    {const auto* z=board->NetGetGhostZombie(record.data.waterTargetId);if(!z||z->mDead)return false;}
	// Extensions follow native DataArray generations through retirement. A
	// snapshot can mark a ghost dead before the array actually frees it; losing
	// its metadata here would temporarily restore Classic values/appearance.
	LawnFusion::PruneNativeMetadata(board);
    NetPruneNativeFusionBridgeMetadata(board,*context);
    for(const auto& record:frame.zombieIdentities)
    {auto* z=board->NetGetGhostZombie(record.id);context->zombieInstanceTokens[board->mZombies.DataArrayGetID(z)]=record.data;}
    context->star269Plants.clear();context->star269Projectiles.clear();context->doom269Plants.clear();
    context->companions269.clear();context->companionPlants269.clear();
    context->worldPlants269.clear();context->worldBeams269.clear();
    context->worldRemainder269Aux.clear();context->worldRemainder269Shots.clear();
    context->worldRemainder269Board=frame.worldRemainder269Board;context->goldHypnoSummoned=frame.goldHypnoSummoned;

    context->droppedCards269.clear();context->rollingPlants269.clear();context->fertilizeCoins269.clear();context->wheatPlants269.clear();
    for(const auto& record:frame.star269Plants)
    {auto* p=board->NetGetGhostPlant(record.id);context->star269Plants[board->mPlants.DataArrayGetID(p)]=record.data;}
    for(const auto& record:frame.star269Projectiles)
    {auto* p=board->NetGetGhostProjectile(record.id);context->star269Projectiles[board->mProjectiles.DataArrayGetID(p)]=record.data;}
    for(const auto& record:frame.doom269Plants)
    {auto* p=board->NetGetGhostPlant(record.id);context->doom269Plants[board->mPlants.DataArrayGetID(p)]=record.data;}
    for(const auto& record:frame.companions269)
    {auto* p=board->NetGetGhostProjectile(record.id);context->companions269[board->mProjectiles.DataArrayGetID(p)]=record.data;p->mProjectileAge=record.data.ageTicks;}
    for(const auto& record:frame.companionPlants269)
    {auto* p=board->NetGetGhostPlant(record.id);context->companionPlants269[board->mPlants.DataArrayGetID(p)]=record.data;}
    for(const auto& record:frame.worldPlants269)
    {auto* p=board->NetGetGhostPlant(record.id);context->worldPlants269[board->mPlants.DataArrayGetID(p)]=record.data;}
    for(const auto& record:frame.worldBeams269)
    {auto* p=board->NetGetGhostProjectile(record.id);context->worldBeams269[board->mProjectiles.DataArrayGetID(p)]=record.data;p->mProjectileAge=record.data.ageTicks;}
    for(const auto& record:frame.worldRemainder269Aux)
    {auto* p=board->NetGetGhostProjectile(record.id);context->worldRemainder269Aux[board->mProjectiles.DataArrayGetID(p)]=record.data;p->mProjectileAge=record.data.ageTicks;}
    for(const auto& record:frame.worldRemainder269Shots)
    {auto* p=board->NetGetGhostProjectile(record.id);context->worldRemainder269Shots[board->mProjectiles.DataArrayGetID(p)]=record.data;p->mProjectileAge=record.data.ageTicks;}
    for(const auto& record:frame.droppedCards269)
    {auto* c=board->NetGetGhostCoin(record.id);context->droppedCards269[board->mCoins.DataArrayGetID(c)]=record.data;}
    for(const auto& record:frame.fertilizeCoins269)
    {auto* c=board->NetGetGhostCoin(record.id);context->fertilizeCoins269[board->mCoins.DataArrayGetID(c)]=record.data;}
    for(const auto& record:frame.rollingPlants269)
    {auto* p=board->NetGetGhostPlant(record.id);context->rollingPlants269[board->mPlants.DataArrayGetID(p)]=record.data;p->mPlantHealth=record.data.health;}
    for(const auto& record:frame.wheatPlants269)
    {
        auto* p=board->NetGetGhostPlant(record.id);
        auto& wheat=context->wheatPlants269[board->mPlants.DataArrayGetID(p)];
        wheat.isFromWheat=record.data.isFromWheat;wheat.wheatTime=record.data.wheatTime;wheat.lastTick=-1;
    }
	context->packets.clear();
	for (const auto& record : frame.packets) context->packets.emplace(record.id, record.data);
	for (const auto& record : frame.plants)
	{
		Plant* plant = board->NetGetGhostPlant(record.id);
		LawnFusion::AttachNativePlant(board, plant, record.data);
        if(record.data.kind==909||record.data.kind==1104)plant->mDoSpecialCountdown=record.data.pendingProductionTicks;
		if (record.data.kind != 1000 && LawnFusion::IsNativeProducerKind(record.data.kind))
			plant->mLaunchCounter = record.data.productionTicks;
	}
    for(const auto& record:frame.plants)
    {
        const auto kind=record.data.kind;
        if(kind==15||kind==1049||kind==1050||kind==1051||kind==1066||kind==1156)
        {
            auto* p=board->NetGetGhostPlant(record.id);auto* z=record.data.waterTargetId?board->NetGetGhostZombie(record.data.waterTargetId):nullptr;
            p->mTargetZombieID=z?board->ZombieGetID(z):ZOMBIEID_NULL;
        }
    }
    for(const auto& record:frame.equipmentPlants)
    {auto* p=board->NetGetGhostPlant(record.id);context->equipmentPlants[board->mPlants.DataArrayGetID(p)]=record.data;}
    for(const auto& record:frame.equipmentCoins)
    {auto* c=board->NetGetGhostCoin(record.id);context->equipmentCoins[board->mCoins.DataArrayGetID(c)]=record.data;}
	LawnFusion::RefreshNativeDroppedCard269Cursors(board);
	for (const auto& record : frame.projectiles) LawnFusion::AttachNativeProjectile(board, board->NetGetGhostProjectile(record.id), record.data);
	for (const auto& record : frame.sourceZombies)
    {
        auto* zombie=board->NetGetGhostZombie(record.id);
        if(!LawnFusion::AttachNativeSourceZombieData(board,zombie,record.data))return false;
        if(record.data.kind==18)zombie->mFlatTires=record.data.vehicleStopped;
        if(record.data.kind==104)
        {
            if(!LawnFusion::InitializeNativeSpecialZombie269PaperAddon(zombie))return false;
            zombie->mSummonCounter=record.data.paperShootTicks;
            zombie->mBossHeadCounter=record.data.paperShootEvent;
            NetApplyReanimState(board->mApp,zombie->mSpecialHeadReanimID,record.data.paperHead);
        }

    }
    for(const auto& source:frame.sourceZombies)
    {
        if(!LawnFusion::FusionWaterZombie313Kind(source.data.kind)&&!LawnFusion::NativeHeavyZombie313BKind(source.data.kind)&&!LawnFusion::FindFusionExtendedZombie313Profile(source.data.kind)&&!LawnFusion::FusionBoss361Kind(source.data.kind))continue;
        LawnFusion::Native313Snapshot mirror;NetGather313(frame,source.id,mirror);std::string error;
        if(!LawnFusion::AdoptNative313(board->NetGetGhostZombie(source.id),mirror,&error))
        {NetTrace("FNS28 prepared source313 adoption failed id=%u error=%s",source.id,error.c_str());return false;}
    }
    context->bossRoom366=frame.bossRoom366;context->bossRoom366.lastTick=-1;
    for(const auto& record:frame.sourceObstacles313)
        if(!LawnFusion::AdoptNativeSourceObstacle313(board->NetGetGhostProjectile(record.id),record.data))return false;
    if(auto* captures=LawnFusion::GetNativeCapturedPlants314(board))
    {
        Zombie* owner=nullptr;while(board->mZombies.IterateNext(owner))if(!owner->mDead)
        {
            const auto held=std::find_if(frame.capturedPlants314.begin(),frame.capturedPlants314.end(),[&](const auto& r){return r.id==owner->mNetId;});
            if(held==frame.capturedPlants314.end())captures->Retire(owner);
        }
        for(const auto& record:frame.capturedPlants314)
        {
            auto* owner=board->NetGetGhostZombie(record.id);std::string error;LawnFusion::CapturedMatrix314 matrix;
            if(!captures->Adopt(owner,record.data,error)||!LawnFusion::GetNativeSourceZombieMatrix313(owner,"",matrix))return false;
            captures->UpdateOwner(owner,matrix);
        }
        captures->Prune();
    }
    else if(!frame.capturedPlants314.empty())return false;
    context->sourcePoints=frame.sourcePoints;context->extendedBoard313=frame.extendedBoard313;
    context->jackboxZombies300.clear();
    for(const auto& record:frame.jackboxZombies300)
    {
        auto* zombie=board->NetGetGhostZombie(record.id);
        const auto* source=LawnFusion::GetNativeSourceZombieData(zombie);
        if(!zombie||!source||!LawnFusion::FusionJackbox300Kind(source->kind))return false;
        auto& state=context->jackboxZombies300[board->mZombies.DataArrayGetID(zombie)];
        static_cast<LawnFusion::FusionJackbox300View&>(state)=record.data;
        state.initialized=true; // Adopted ghosts never tick host RNG/events.
    }
    context->nightZombies281.clear();
    for(const auto& record:frame.nightZombies281)
    {
        auto* zombie=board->NetGetGhostZombie(record.id);
        const auto* source=LawnFusion::GetNativeSourceZombieData(zombie);
        if(!zombie||!source||!LawnFusion::FusionJacksonFamily367Kind(source->kind))return false;
        auto& state=context->nightZombies281[board->mZombies.DataArrayGetID(zombie)];
        static_cast<LawnFusion::FusionNightZombie281View&>(state)=record.data;
        state.initialized=true; // Ghosts adopt this view and never tick source callbacks.
    }
	context->dayZombies273.clear();
	for(const auto& record:frame.dayZombies273)
	{
		auto* zombie=board->NetGetGhostZombie(record.id);
		const auto* source=LawnFusion::GetNativeSourceZombieData(zombie);
		if(!zombie||!source)return false;
		context->dayZombies273[board->mZombies.DataArrayGetID(zombie)]=record.data;
		if(LawnFusion::FusionDayZombie273Shooter(source->kind))
		{
			// The complete raw batch's native allocations were bounded before
			// commit, including this actual attached head and its track container.
			if(record.data.head.mPresent&&!LawnFusion::EnsureNativeDayZombie273ShooterHead(zombie))return false;
			if(!record.data.head.mPresent)
			{
				board->mApp->RemoveReanimation(zombie->mSpecialHeadReanimID);
				zombie->mSpecialHeadReanimID=REANIMATIONID_NULL;
			}
			NetApplyReanimState(board->mApp,zombie->mSpecialHeadReanimID,record.data.head);
		}
	}
	// Removing a metadata record never clears the counters already decoded from
	// the ordinary body. Only records present in this frame restore exact values.
	context->coldZombies.clear();
	for (const auto& record : frame.coldZombies)
	{
		Zombie* zombie = board->NetGetGhostZombie(record.id);
		if (!LawnFusion::AttachNativeZombieColdData(board, zombie, record.data)) return false;
		zombie->mChilledCounter = record.data.chilledTicks;
		zombie->mIceTrapCounter = record.data.frozenTicks;
	}
	for (const auto& record : frame.coins) LawnFusion::AttachNativeCoinValue(board, board->NetGetGhostCoin(record.id), record.data.value, record.data.money);
	context->butterZombies.clear();
	for (const auto& record : frame.butterZombies)
	{
		Zombie* zombie = board->NetGetGhostZombie(record.id);
		if (!LawnFusion::AttachNativeZombieButterData(board,zombie,record.data)) return false;
		zombie->mButteredCounter = record.data.ticks;
	}
	context->zombieActions.clear();
	for (const auto& record : frame.zombieActions)
	{
		auto* zombie = board->NetGetGhostZombie(record.id);
		if (!LawnFusion::AttachNativeZombieActionData(board,zombie,record.data)) return false;
		zombie->mScaleZombie = record.data.scale;
	}
	for (const auto& [id,state] : context->garlicZombies)
		if (auto* zombie = board->mZombies.DataArrayTryToGet(id))
			if (std::none_of(frame.garlicZombies.begin(),frame.garlicZombies.end(),[&](const auto& r) { return r.id == zombie->mNetId; }))
				LawnFusion::RestoreNativeGarlicPresentation(zombie,{});
	context->garlicZombies.clear();
	for (const auto& record : frame.garlicZombies)
	{
		auto* zombie = board->NetGetGhostZombie(record.id);
		if (!LawnFusion::AttachNativeZombieGarlicData(board,zombie,record.data) ||
			!LawnFusion::RestoreNativeGarlicPresentation(zombie,record.data)) return false;
	}
    board->mApp->mFusionTravelBuff369=frame.travelBuff369;context->travelBuffCooldown369=frame.travelBuffCooldown369;
	context->money = frame.money;
	context->adventure = frame.adventure;
	context->temporaryLightLevel = frame.temporaryLightLevel;
	context->selection = selection;
	context->selectionCommitted = frame.selectionCommitted;
	context->gloveHolds = frame.gloveHolds;
	context->gloveCooldownTicks = frame.gloveCooldownTicks;
    LawnFusion::ReconcileNativePlantDisables363(board);
	LawnFusion::ReleaseUnusedNativeVisuals(board);
	LawnFusion::ReleaseUnusedNativeSourceZombieVisuals(board);
	// Client Board::Update skips host spawning/tutorial updates. Materialize the
	// source Advice only after the full accepted native view was committed.
	if(context->adventure.enabled)
		LawnFusion::PresentNativeFusionAdventureTutorial273(board,context->adventure.run);
	return true;
}

// A client ghost is only a presentation mirror. Calling Zombie::DieNoLoot on
// it also awards challenge points, spawns level rewards, mutates linked
// bobsled/bungee/boss gameplay and can consume RNG. Snapshot omission and
// reliable removal events therefore use this resource-only retirement path.
void NetRetireZombiePresentation(LawnApp* theApp, Zombie* theZombie)
{
	if (!theApp || !theZombie || theZombie->mDead)
		return;

    if(auto* context=LawnFusion::GetNativeContext(theZombie->mBoard))
    {
        const auto uid=theZombie->mBoard->mZombies.DataArrayGetID(theZombie);
        if(const auto found=context->waterZombies313.find(uid);found!=context->waterZombies313.end()&&found->second.auxiliaryID)
            theApp->RemoveReanimation(ReanimationID(found->second.auxiliaryID));
        if(auto* captured=LawnFusion::GetNativeCapturedPlants314(theZombie->mBoard))captured->Retire(theZombie);
    }
	theZombie->StopZombieSound();
	AttachmentDie(theZombie->mAttachmentID);
	theApp->RemoveReanimation(theZombie->mBodyReanimID);
	theApp->RemoveReanimation(theZombie->mMoweredReanimID);
	theApp->RemoveReanimation(theZombie->mSpecialHeadReanimID);
	theZombie->mDead = true;
}

struct NetArmorVisual
{
	const char* mTrack = nullptr;
	Image* mImages[3] = {};
};

NetArmorVisual NetGetHelmetVisual(HelmType theType)
{
	switch (theType)
	{
	case HELMTYPE_TRAFFIC_CONE: return { "anim_cone", { IMAGE_REANIM_ZOMBIE_CONE1, IMAGE_REANIM_ZOMBIE_CONE2, IMAGE_REANIM_ZOMBIE_CONE3 } };
	case HELMTYPE_BUCKET: return { "anim_bucket", { IMAGE_REANIM_ZOMBIE_BUCKET1, IMAGE_REANIM_ZOMBIE_BUCKET2, IMAGE_REANIM_ZOMBIE_BUCKET3 } };
	case HELMTYPE_FOOTBALL: return { "zombie_football_helmet", { IMAGE_REANIM_ZOMBIE_FOOTBALL_HELMET, IMAGE_REANIM_ZOMBIE_FOOTBALL_HELMET2, IMAGE_REANIM_ZOMBIE_FOOTBALL_HELMET3 } };
	case HELMTYPE_BLACK_FOOTBALL: return { "zombie_football_helmet", { IMAGE_REANIM_ZOMBIE_BLACK_FOOTBALL_HELMET, IMAGE_REANIM_ZOMBIE_BLACK_FOOTBALL_HELMET2, IMAGE_REANIM_ZOMBIE_BLACK_FOOTBALL_HELMET3 } };
	case HELMTYPE_DIGGER: return { "Zombie_digger_hardhat", { IMAGE_REANIM_ZOMBIE_DIGGER_HARDHAT, IMAGE_REANIM_ZOMBIE_DIGGER_HARDHAT2, IMAGE_REANIM_ZOMBIE_DIGGER_HARDHAT3 } };
	default: return {};
	}
}

NetArmorVisual NetGetShieldVisual(ShieldType theType)
{
	switch (theType)
	{
	case SHIELDTYPE_DOOR: return { "anim_screendoor", { IMAGE_REANIM_ZOMBIE_SCREENDOOR1, IMAGE_REANIM_ZOMBIE_SCREENDOOR2, IMAGE_REANIM_ZOMBIE_SCREENDOOR3 } };
	case SHIELDTYPE_TRASHCAN: return { "anim_screendoor", { IMAGE_REANIM_ZOMBIE_TRASHCAN1, IMAGE_REANIM_ZOMBIE_TRASHCAN2, IMAGE_REANIM_ZOMBIE_TRASHCAN3 } };
	case SHIELDTYPE_NEWSPAPER: return { "Zombie_paper_paper", { IMAGE_REANIM_ZOMBIE_PAPER_PAPER1, IMAGE_REANIM_ZOMBIE_PAPER_PAPER2, IMAGE_REANIM_ZOMBIE_PAPER_PAPER3 } };
	case SHIELDTYPE_LADDER: return { "Zombie_ladder_1", { IMAGE_REANIM_ZOMBIE_LADDER_1, IMAGE_REANIM_ZOMBIE_LADDER_1_DAMAGE1, IMAGE_REANIM_ZOMBIE_LADDER_1_DAMAGE2 } };
	default: return {};
	}
}

// A ghost receives health/type state without running the host's damage code.
// Reuse native attachments and damage stages to project adoption, loss and repair;
// never replay damage, death, loot or shield-specific gameplay transitions here.
void NetApplyZombieArmorPresentation(Zombie* theZombie, HelmType thePreviousHelmet,
	ShieldType thePreviousShield, bool theHelmetChanged, bool theShieldChanged)
{
	Reanimation* aBody = theZombie->mApp->ReanimationTryToGet(theZombie->mBodyReanimID);
	if (!aBody) return;
	if (theHelmetChanged)
	{
		const auto aPrevious = NetGetHelmetVisual(thePreviousHelmet);
		if (aPrevious.mTrack && aBody->TrackExists(aPrevious.mTrack))
		{
			aBody->AssignRenderGroupToPrefix(aPrevious.mTrack, RENDER_GROUP_HIDDEN);
			if (theZombie->mHasHead)
				aBody->AssignRenderGroupToPrefix("anim_hair", RENDER_GROUP_NORMAL);
		}
		const auto aCurrent = NetGetHelmetVisual(theZombie->mHelmType);
		if (aCurrent.mTrack && aBody->TrackExists(aCurrent.mTrack))
		{
			if (theZombie->mHelmHealth > 0 && theZombie->mHasHead)
			{
				theZombie->AttachHelmet();
				aBody->SetImageOverride(aCurrent.mTrack, aCurrent.mImages[theZombie->GetHelmDamageIndex()]);
			}
			else aBody->AssignRenderGroupToPrefix(aCurrent.mTrack, RENDER_GROUP_HIDDEN);
		}
	}
	if (theShieldChanged)
	{
		const auto aPrevious = NetGetShieldVisual(thePreviousShield);
		if (aPrevious.mTrack && aBody->TrackExists(aPrevious.mTrack))
		{
			aBody->AssignRenderGroupToPrefix(aPrevious.mTrack, RENDER_GROUP_HIDDEN);
			if (thePreviousShield == SHIELDTYPE_DOOR || thePreviousShield == SHIELDTYPE_TRASHCAN)
				theZombie->ShowDoorArms(false);
			else if (thePreviousShield == SHIELDTYPE_NEWSPAPER)
				aBody->AssignRenderGroupToPrefix("Zombie_paper_hands", RENDER_GROUP_NORMAL);
			else if (thePreviousShield == SHIELDTYPE_LADDER)
			{
				aBody->AssignRenderGroupToPrefix("Zombie_outerarm", theZombie->mHasArm ? RENDER_GROUP_NORMAL : RENDER_GROUP_HIDDEN);
				aBody->AssignRenderGroupToPrefix("Zombie_ladder_innerarm_hand2", RENDER_GROUP_HIDDEN);
			}
		}
		const auto aCurrent = NetGetShieldVisual(theZombie->mShieldType);
		if (aCurrent.mTrack && aBody->TrackExists(aCurrent.mTrack))
		{
			if (theZombie->mShieldHealth > 0)
			{
				theZombie->AttachShield();
				aBody->SetImageOverride(aCurrent.mTrack, aCurrent.mImages[theZombie->GetShieldDamageIndex()]);
			}
			else aBody->AssignRenderGroupToPrefix(aCurrent.mTrack, RENDER_GROUP_HIDDEN);
		}
		if (!theZombie->mHasArm)
			theZombie->SetupReanimForLostArm(1U << (int)DamageFlags::DAMAGE_DOESNT_LEAVE_BODY);
	}
}

uint32_t NetGetReanimationFrameCapacity(const Reanimation* theReanim)
{
	if (!theReanim || !theReanim->mDefinition ||
		theReanim->mDefinition->mTracks.count <= 0 ||
		!theReanim->mDefinition->mTracks.tracks ||
		theReanim->mDefinition->mTracks.tracks[0].mTransforms.count <= 0)
		return 0;
	return (uint32_t)theReanim->mDefinition->mTracks.tracks[0].mTransforms.count;
}

bool NetDay273AllocationCapacity(Board* board,const NetBuffer& body,const LawnFusion::NativeSnapshotFrame& frame,const LawnFusion::FusionSourceDeathBudget319& extra)
{
    const bool cohort313=NetHas313(frame)||frame.adventure.enabled;
    if(!cohort313&&frame.dayZombies273.empty()&&frame.nightZombies281.empty()&&frame.jackboxZombies300.empty()&&!(frame.adventure.enabled&&frame.background==BACKGROUND_2_NIGHT))return true;
    const auto reject=[](const char* reason,int section=-1,unsigned type=0)
    {
        NetTrace("Day274 snapshot allocation rejected reason=%s section=%d type=%u",reason,section,type);return false;
    };
    if(!board||!board->mApp||!LawnFusion::GetNativeContext(board))return reject("no-board-context");
    auto* app=board->mApp;auto* effects=app->mEffectSystem;
    if(app->mUpdateAppDepth>1||!board->mTDPath.empty()||board->GetColumnCount()!=10||
       (cohort313?frame.background>BACKGROUND_6_BOSS:
        (!LawnFusion::IsNativeSourceWorld313(board)||LawnFusion::GetNativeFusionRowCount(board)!=5||app->mGameMode!=GAMEMODE_ADVENTURE||frame.background!=board->mBackground)))
        return reject("only-nonreentrant-ordinary-source-world");
    if(!effects||effects!=gEffectSystem||!effects->mReanimationHolder||!effects->mAttachmentHolder||!effects->mParticleHolder)
        return reject("no-effect-pools");
    if(board->mMustacheMode||board->mFutureMode||board->mPinataMode)return reject("extra-native-limb-FX-modes-not-budgeted");
    if(!cohort313)for(int row=0;row<5;++row)if(board->mPlantRow[row]!=PLANTROW_NORMAL)return reject("non-Day-row");
    // A source-proven upper bound for ALL incoming actors, including reused
    // ones. This deliberately overcounts rather than duplicating every reuse
    // predicate. No credit for omitted/replaced/dead actors pending cleanup.
    // NetDrainMessages applies synchronously on the main thread. Definitions
    // are loaded before the final pool read; no message pump follows the check.
    uint64_t reanims=extra.reanimations,attachments=extra.attachments,systems=extra.particleSystems,emitters=extra.emitters;std::array<unsigned,6> counts{};
    const auto particleUpper=[&](std::initializer_list<ParticleEffect> choices,unsigned calls)
    {
        unsigned maximum=0;
        for(const auto type:choices)
        {
            if(!gParticleDefArray||unsigned(type)>=unsigned(gParticleDefCount))return false;
            const auto& def=gParticleDefArray[type];
            if(def.mEmitterDefCount<0||(def.mEmitterDefCount&&!def.mEmitterDefs))return false;
            maximum=(std::max)(maximum,unsigned(def.mEmitterDefCount));
        }
        systems+=calls;emitters+=uint64_t(calls)*maximum;return true;
    };
    const auto plainDefinition=[](ReanimationType type,const char* required=nullptr)
    {
        if(unsigned(type)>=gReanimatorDefCount)return false;
        ReanimatorEnsureDefinitionLoaded(type,false);
        if(!gReanimatorDefArray)return false;
        const auto& tracks=gReanimatorDefArray[type].mTracks;
        if(!tracks.tracks||tracks.count<=0||tracks.tracks[0].mTransforms.count<=0)return false;
        bool found=!required;
        for(int i=0;i<tracks.count;++i)
        {
            const auto* name=tracks.tracks[i].mName;if(!name)return false;
            // ZombieInitialize calls UpdateReanim, which evaluates these and
            // may allocate arbitrary nested native child resources.
            if(_strnicmp(name,"attacher__",10)==0)return false;
            if(required&&_stricmp(name,required)==0)found=true;
        }
        return found;
    };
    const auto attachmentRoom=[&](AttachmentID id,unsigned additions)
    {
        const auto& pool=effects->mAttachmentHolder->mAttachments;
        if(!pool.mBlock)return false;
        const auto* item=effects->mAttachmentHolder->mAttachments.DataArrayTryToGet(unsigned(id));
        return !item||item->mDead||(item->mNumEffects>=0&&unsigned(item->mNumEffects)+additions<=MAX_EFFECTS_PER_ATTACHMENT);
    };
    const auto trackRoom=[&](Reanimation* reanim,const char* track,unsigned additions)
    {
        if(!reanim)return true; // A fresh body's track containers start empty.
        if(!reanim->TrackExists(track))return false;
        return attachmentRoom(reanim->GetTrackInstanceByName(track)->mAttachmentID,additions);
    };
    NetBuffer scan=body;
    const size_t coreBytes=43+MAX_GRID_SIZE_Y*4+9+10+NET_CHALLENGE_RUNTIME_WIRE_BYTES+
        NET_ENDLESS_WEATHER_WIRE_BYTES+NET_ZEN_GARDEN_WIRE_BYTES;
    scan.mReadPos=coreBytes+6;uint8_t packets=0;
    if(!scan.ReadU8(&packets)||packets>SEEDBANK_CAPACITY||scan.Remaining()<packets*NET_SEED_PACKET_WIRE_BYTES)
        return reject("packet-framing");
    scan.mReadPos+=packets*NET_SEED_PACKET_WIRE_BYTES;
    const size_t sizes[]={NET_PLANT_WIRE_BYTES,NET_ZOMBIE_WIRE_BYTES,NET_PROJECTILE_WIRE_BYTES,NET_COIN_WIRE_BYTES,NET_MOWER_WIRE_BYTES,37};
    for(int section=0;section<6;++section)
    {
        uint16_t count=0;
        if(!scan.ReadU16(&count)||count>scan.Remaining()/sizes[section])return reject("actor-framing",section);
        counts[section]=count;
        if(section==5&&count&&!cohort313)return reject("GridItem-visual-factory-not-budgeted",section);
        for(unsigned i=0;i<count;++i)
        {
            const size_t start=scan.mReadPos;const auto id=scan.ReadU16Or();const unsigned type=scan.ReadU8Or();
            if(section==0)
            {
                unsigned heads=0;
                switch(type)
                {
                case SEED_PEASHOOTER:case SEED_SNOWPEA:case SEED_REPEATER:case SEED_LEFTPEATER:case SEED_GATLINGPEA:heads=1;break;
                case SEED_SPLITPEA:heads=2;break;
                case SEED_THREEPEATER:heads=3;break;
                default:
                    // The raw/Fusion joint preflight has already matched this
                    // carrier to its typed plant. All native chooser seeds use
                    // one body; the few extra initializer resources are counted
                    // explicitly below. Keep the real definition/attacher check.
                    if(type>=NUM_SEEDS_IN_CHOOSER)return reject("plant-factory-not-budgeted",section,type);
                    break;
                }
                const auto reanim=GetPlantDefinition(SeedType(type)).mReanimationType;
                // Present's IMITATER carrier initializes one body. Its typed
                // controller intercepts stock morph; the definition must still
                // pass the same no-dynamic-attacher proof as every carrier.
                if(!plainDefinition(reanim))return reject("plant-definition-or-attacher",section,type);
                reanims+=1+heads;attachments+=heads; // Each Attach call can require at most one container.
                // A raw daytime mushroom initializes before Fusion metadata
                // is bound and can allocate its sleep reanimation (PlantInitialize).
                if(Plant::IsNocturnal(SeedType(type)))
                {++reanims;if(!plainDefinition(REANIM_SLEEPING))return reject("mushroom-initial-sleep-definition",section,type);}
                if(scan.mData[start+11]&1){++reanims;if(!plainDefinition(REANIM_SLEEPING))return reject("sleep-definition",section,type);}
                if(type==SEED_PLANTERN)
                {
                    ++attachments;
                    if(!particleUpper({PARTICLE_LANTERN_SHINE},1))return reject("plantern-particle-definition",section,type);
                }
                if(type==SEED_POTATOMINE)
                {
                    ++reanims;++attachments; // Initializer/presentation glow uses one resource, never both.
                    auto* old=board->NetGetGhostPlant(id);
                    if(!plainDefinition(reanim,"anim_light")||!trackRoom(old?app->ReanimationTryToGet(old->mBodyReanimID):nullptr,"anim_light",1))
                        return reject("potato-glow-track-capacity",section,type);
                    if(!particleUpper({PARTICLE_POTATO_MINE_RISE},1))return reject("potato-particle-definition",section,type);
                }
            }
            else if(section==1)
            {
                switch(type)
                {
                case ZOMBIE_NORMAL:case ZOMBIE_TRAFFIC_CONE:case ZOMBIE_PAIL:case ZOMBIE_DOOR:case ZOMBIE_POLEVAULTER:
                case ZOMBIE_NEWSPAPER:case ZOMBIE_FOOTBALL:break;
                case ZOMBIE_ZAMBONI:if(!cohort313)return reject("zamboni-only-source313",section,type);break;
                case ZOMBIE_BOSS:
                    if(!cohort313||std::none_of(frame.sourceZombies.begin(),frame.sourceZombies.end(),[=](const auto& r){return r.id==id&&LawnFusion::FusionBoss361Kind(r.data.kind);}))
                        return reject("boss-only-canonical-source361",section,type);
                    // Raw ghost construction owns its body only. The shared
                    // Boss bridge reserves RV+driver2R/2A: total3R/2A.
                    if(!plainDefinition(REANIM_BOSS_DRIVER)||!plainDefinition(REANIM_BOSS,"Boss_head2")||!plainDefinition(REANIM_BOSS,"Boss_RV"))
                        return reject("boss-aux-definitions",section,type);
                    break;
                default:return reject("zombie-carrier-not-budgeted",section,type);
                }
                const auto reanim=GetZombieDefinition(ZombieType(type)).mReanimationType;
                if(!plainDefinition(reanim))return reject("zombie-definition-or-attacher",section,type);
                ++reanims;
                auto* old=board->NetGetGhostZombie(id);
                const auto flags=scan.mData[start+NET_ZOMBIE_CHILLED_OFFSET-1];
                if(flags&2)
                {const auto row=scan.mData[start+3];if(!cohort313||LawnFusion::NativeFusionRowsForBackground(frame.background)!=6||(row!=2&&row!=3))return reject("pool-zombie-outside-water",section,type);}
                // One arm plus one head at most for these six ground carriers.
                // Newspaper substitutes its head effect; source106/108 omit it.
                // All incoming actors are counted, not just observed loss edges.
                if(!particleUpper({PARTICLE_ZOMBIE_ARM,PARTICLE_MOWERED_ZOMBIE_ARM,PARTICLE_ZOMBIE_HEAD,
                    PARTICLE_MOWERED_ZOMBIE_HEAD,PARTICLE_ZOMBIE_NEWSPAPER_HEAD},2))return reject("limb-particle-definitions",section,type);
                const auto source=std::find_if(frame.sourceZombies.begin(),frame.sourceZombies.end(),[&](const auto& value){return value.id==id;});
                unsigned headEffects=0;
                if(source==frame.sourceZombies.end()&&type==ZOMBIE_NORMAL)
                {
                    // Actual maximum: base1 + hair2 + eyewear2 + accessory1 + hat2.
                    reanims+=8;++attachments;headEffects=8;
                    if(!plainDefinition(REANIM_ZOMBATAR))return reject("Zombatar-definition",section,type);
                }
                if(source!=frame.sourceZombies.end()&&(source->data.kind==104||LawnFusion::FusionDayZombie273Shooter(source->data.kind)))
                {
                    ++reanims;++attachments;headEffects=1;
                    if(!plainDefinition(REANIM_PEASHOOTER))return reject("source-head-definition",section,type);
                }
                if(headEffects&&(!plainDefinition(reanim,"anim_head1")||!trackRoom(old?app->ReanimationTryToGet(old->mBodyReanimID):nullptr,"anim_head1",headEffects)))
                    return reject("zombie-head-track-capacity",section,type);
            }
            else if(section==2)
            {
                switch(type)
                {
                case PROJECTILE_FIREBALL:++reanims;++attachments;if(!plainDefinition(REANIM_FIRE_PEA))return reject("fireball-definition",section,type);break;
                case PROJECTILE_SNOWPEA:case PROJECTILE_WINTERMELON:
                    ++attachments;if(!particleUpper({PARTICLE_SNOWPEA_TRAIL},1))return reject("snow-trail-definition",section,type);break;
                case PROJECTILE_PUFF:
                    ++attachments;if(!particleUpper({PARTICLE_PUFFSHROOM_TRAIL},1))return reject("puff-trail-definition",section,type);break;
                case PROJECTILE_PEA:case PROJECTILE_CABBAGE:case PROJECTILE_MELON:case PROJECTILE_STAR:case PROJECTILE_SPIKE:
                case PROJECTILE_BASKETBALL:case PROJECTILE_KERNEL:case PROJECTILE_COBBIG:case PROJECTILE_BUTTER:case PROJECTILE_ZOMBIE_PEA:break;
                default:return reject("projectile-factory-not-budgeted",section,type);
                }
            }
            else if(section==3)
            {
                ReanimationType reanim=REANIM_NONE;
                switch(type)
                {
                case COIN_SUN:case COIN_SMALLSUN:case COIN_LARGESUN:reanim=REANIM_SUN;break;
                case COIN_SILVER:reanim=REANIM_COIN_SILVER;break;
                case COIN_GOLD:reanim=REANIM_COIN_GOLD;break;
                case COIN_DIAMOND:reanim=REANIM_DIAMOND;break;
                case COIN_NONE:case COIN_TROPHY:case COIN_USABLE_SEED_PACKET:break;
                default:return reject("coin-factory-not-budgeted",section,type);
                }
                if(reanim!=REANIM_NONE){++reanims;++attachments;if(!plainDefinition(reanim))return reject("coin-definition",section,type);}
                ++attachments; // NetUpdatePresentation: either arrow or collection effect, including old Coins.
                if(!particleUpper({PARTICLE_AWARD_PICKUP_ARROW,PARTICLE_COIN_PICKUP_ARROW,PARTICLE_STARBURST,PARTICLE_SEED_PACKET_PICKUP},2))
                    return reject("coin-particle-definitions",section,type);
                auto* old=board->NetGetGhostCoin(id);
                if(old&&!attachmentRoom(old->mAttachmentID,1))return reject("coin-effect-track-capacity",section,type);
            }
            else if(section==4)
            {
                NetBuffer mowerWire=scan;mowerWire.mReadPos=start;
                NetMowerSnapshotState mower;
                if(!NetDecodeMowerSnapshotState(mowerWire,mower)||(mower.mType!=LAWNMOWER_LAWN&&(!cohort313||(mower.mType!=LAWNMOWER_POOL&&!(mower.mType==LAWNMOWER_ROOF&&frame.background==BACKGROUND_5_ROOF)))))
                    return reject("unsupported-source-mower",section,mower.mType);
                ++reanims;const auto reanim=mower.mType==LAWNMOWER_POOL?REANIM_POOL_CLEANER:mower.mType==LAWNMOWER_ROOF?REANIM_ROOF_CLEANER:REANIM_LAWNMOWER;
                if(!plainDefinition(reanim))return reject("mower-definition",section,type);
            }
            else if(section==5&&type!=GRIDITEM_LADDER)return reject("only-source-native-ladder-grid",section,type);
            scan.mReadPos=start+sizes[section];
        }
    }
    if(scan.Remaining())return reject("trailing-raw-body");
    const auto room=[](const auto& pool,uint64_t added,unsigned spare=0)
    {return !added||(pool.mBlock&&uint64_t(pool.mSize)+added+spare<=pool.mMaxSize);};
    if(!room(board->mPlants,counts[0])||!room(board->mZombies,uint64_t(counts[1])+extra.zombies,1)||!room(board->mProjectiles,uint64_t(counts[2])+extra.projectiles)||
        !room(board->mCoins,uint64_t(counts[3])+extra.coins)||!room(board->mLawnMowers,counts[4])||!room(board->mGridItems,counts[5]))return reject("native-entity-pool-upper-bound");
    if(!room(effects->mReanimationHolder->mReanimations,reanims)||!room(effects->mAttachmentHolder->mAttachments,attachments))
        return reject("reanimation-or-attachment-pool-upper-bound");
    if(!room(effects->mParticleHolder->mParticleSystems,systems)||!room(effects->mParticleHolder->mEmitters,emitters))
        return reject("particle-system-or-emitter-pool-upper-bound");
    if(std::any_of(frame.dayZombies273.begin(),frame.dayZombies273.end(),[&](const auto& record){return !board->NetGetGhostZombie(record.id);}))
        NetTrace("Day274 snapshot capacity admitted R=%u+%llu/%u A=%u+%llu/%u PS=%u+%llu/%u emitters=%u+%llu/%u allIncomingUpperBound=1 deletionCredit=0",
            effects->mReanimationHolder->mReanimations.mSize,static_cast<unsigned long long>(reanims),effects->mReanimationHolder->mReanimations.mMaxSize,
            effects->mAttachmentHolder->mAttachments.mSize,static_cast<unsigned long long>(attachments),effects->mAttachmentHolder->mAttachments.mMaxSize,
            effects->mParticleHolder->mParticleSystems.mSize,static_cast<unsigned long long>(systems),effects->mParticleHolder->mParticleSystems.mMaxSize,
            effects->mParticleHolder->mEmitters.mSize,static_cast<unsigned long long>(emitters),effects->mParticleHolder->mEmitters.mMaxSize);
    return true;
}

ReanimationID NetGetPlantReanimID(const Plant* thePlant, int theRole)
{
        switch (theRole)
        {
        case 0: return thePlant->mBodyReanimID;
        case 1: return thePlant->mHeadReanimID;
        case 2: return thePlant->mHeadReanimID2;
        case 3: return thePlant->mHeadReanimID3;
        case 4: return thePlant->mBlinkReanimID;
        case 5: return thePlant->mLightReanimID;
        case 6: return thePlant->mSleepingReanimID;
        case 7: return thePlant->mSpecialReanimID;
        default: return ReanimationID::REANIMATIONID_NULL;
        }
}

bool NetCaptureReanimState(LawnApp* theApp, ReanimationID theId,
        NetZombieAnimState& theState)
{
        theState.Reset();
        if (!theApp)
                return true;
        Reanimation* aReanim = theApp->ReanimationTryToGet(theId);
        if (!aReanim)
                return true;
        const uint32_t aFrameCapacity = NetGetReanimationFrameCapacity(aReanim);
        const int aLoopType = (int)aReanim->mLoopType;
        if (aReanim->mFrameStart < 0 || aReanim->mFrameStart > 65535 ||
                aReanim->mFrameCount <= 0 || aReanim->mFrameCount > 65535 ||
                aLoopType < 0 || aLoopType > 5 || !_finite(aReanim->mAnimTime) ||
                !_finite(aReanim->mAnimRate))
                return false;
        theState.mPresent = true;
        theState.mDead = aReanim->mDead;
        theState.mFrameStart = (uint16_t)aReanim->mFrameStart;
        theState.mFrameCount = (uint16_t)aReanim->mFrameCount;
        theState.mAnimTime = aReanim->mAnimTime;
        theState.mAnimRate = aReanim->mAnimRate;
        theState.mLoopType = (uint8_t)aLoopType;
        theState.mLoopCount = (uint8_t)max(0, min(255, aReanim->mLoopCount));
        return NetZombieAnimStateFitsFrames(theState, aFrameCapacity);
}

void NetApplyReanimState(LawnApp* theApp, ReanimationID theId,
        const NetZombieAnimState& theState)
{
        Reanimation* aReanim = theApp ? theApp->ReanimationTryToGet(theId) : nullptr;
        if (!aReanim || !theState.mPresent ||
                !NetZombieAnimStateFitsFrames(theState, NetGetReanimationFrameCapacity(aReanim)))
                return;
        aReanim->mFrameStart = theState.mFrameStart;
        aReanim->mFrameCount = theState.mFrameCount;
        aReanim->mAnimTime = theState.mAnimTime;
        aReanim->mLastFrameTime = theState.mAnimTime;
        aReanim->mAnimRate = theState.mAnimRate;
        aReanim->mLoopType = (ReanimLoopType)theState.mLoopType;
        aReanim->mLoopCount = theState.mLoopCount;
        aReanim->mDead = theState.mDead;
}

bool NetCaptureZombieAnimState(LawnApp* theApp, Zombie* theZombie,
	NetZombieAnimState& theState)
{
	theState.Reset();
	if (!theApp || !theZombie)
		return true;

	Reanimation* aBody = theApp->ReanimationTryToGet(theZombie->mBodyReanimID);
	if (!aBody)
		return true;
	const uint32_t aFrameCapacity = NetGetReanimationFrameCapacity(aBody);
	const int aLoopType = (int)aBody->mLoopType;
	if (aBody->mFrameStart < 0 || aBody->mFrameStart > 65535 ||
		aBody->mFrameCount <= 0 || aBody->mFrameCount > 65535 ||
		aLoopType < 0 || aLoopType > 5 || !_finite(aBody->mAnimTime) ||
		!_finite(aBody->mAnimRate))
		return false;

	theState.mPresent = true;
	theState.mDead = aBody->mDead;
	theState.mFrameStart = (uint16_t)aBody->mFrameStart;
	theState.mFrameCount = (uint16_t)aBody->mFrameCount;
	theState.mAnimTime = aBody->mAnimTime;
	theState.mAnimRate = aBody->mAnimRate;
	theState.mLoopType = (uint8_t)aLoopType;
	theState.mLoopCount = (uint8_t)max(0, min(255, aBody->mLoopCount));
	return NetZombieAnimStateFitsFrames(theState, aFrameCapacity);
}

bool NetCaptureMowerAnimState(LawnApp* theApp, LawnMower* theMower,
	NetZombieAnimState& theState)
{
	theState.Reset();
	if (!theApp || !theMower)
		return true;

	Reanimation* aReanim = theApp->ReanimationTryToGet(theMower->mReanimID);
	if (!aReanim)
		return true;
	const uint32_t aFrameCapacity = NetGetReanimationFrameCapacity(aReanim);
	const int aLoopType = (int)aReanim->mLoopType;
	if (aReanim->mFrameStart < 0 || aReanim->mFrameStart > 65535 ||
		aReanim->mFrameCount <= 0 || aReanim->mFrameCount > 65535 ||
		aLoopType < 0 || aLoopType > 5 || !_finite(aReanim->mAnimTime) ||
		!_finite(aReanim->mAnimRate))
		return false;

	theState.mPresent = true;
	theState.mDead = aReanim->mDead;
	theState.mFrameStart = (uint16_t)aReanim->mFrameStart;
	theState.mFrameCount = (uint16_t)aReanim->mFrameCount;
	theState.mAnimTime = aReanim->mAnimTime;
	theState.mAnimRate = aReanim->mAnimRate;
	theState.mLoopType = (uint8_t)aLoopType;
	theState.mLoopCount = (uint8_t)max(0, min(255, aReanim->mLoopCount));
	return NetZombieAnimStateFitsFrames(theState, aFrameCapacity);
}

bool NetGetMowerTypeFrameCapacity(uint8_t theMowerType,
	uint32_t& theFrameCapacity)
{
	theFrameCapacity = 0;
	ReanimationType aReanimType;
	switch ((LawnMowerType)theMowerType)
	{
	case LawnMowerType::LAWNMOWER_LAWN:
		aReanimType = ReanimationType::REANIM_LAWNMOWER;
		break;
	case LawnMowerType::LAWNMOWER_POOL:
		aReanimType = ReanimationType::REANIM_POOL_CLEANER;
		break;
	case LawnMowerType::LAWNMOWER_ROOF:
		aReanimType = ReanimationType::REANIM_ROOF_CLEANER;
		break;
	default:
		return false;
	}

	const int aReanimIndex = (int)aReanimType;
	if (aReanimIndex < 0 || (unsigned int)aReanimIndex >= gReanimatorDefCount ||
		!gReanimatorDefArray)
		return false;
	ReanimatorEnsureDefinitionLoaded(aReanimType, true);
	const ReanimatorDefinition& aDefinition = gReanimatorDefArray[aReanimIndex];
	if (aDefinition.mTracks.count <= 0 || !aDefinition.mTracks.tracks ||
		aDefinition.mTracks.tracks[0].mTransforms.count <= 0)
		return false;
	theFrameCapacity =
		(uint32_t)aDefinition.mTracks.tracks[0].mTransforms.count;
	return true;
}

bool NetGetZombieTypeFrameCapacity(uint8_t theZombieType, uint32_t& theFrameCapacity)
{
	theFrameCapacity = 0;
	if (theZombieType >= (uint8_t)ZombieType::NUM_ZOMBIE_TYPES)
		return false;
	const ZombieDefinition& aZombieDef = GetZombieDefinition((ZombieType)theZombieType);
	const int aReanimType = (int)aZombieDef.mReanimationType;
	if (aReanimType < 0 || (unsigned int)aReanimType >= gReanimatorDefCount ||
		!gReanimatorDefArray)
		return false;
	ReanimatorEnsureDefinitionLoaded(aZombieDef.mReanimationType, true);
	const ReanimatorDefinition& aDefinition = gReanimatorDefArray[aReanimType];
	if (aDefinition.mTracks.count <= 0 || !aDefinition.mTracks.tracks ||
		aDefinition.mTracks.tracks[0].mTransforms.count <= 0)
		return false;
	theFrameCapacity = (uint32_t)aDefinition.mTracks.tracks[0].mTransforms.count;
	return true;
}

bool NetIsValidSeedPacketType(uint8_t theType)
{
	if (theType == 255 || theType < (uint8_t)SeedType::NUM_SEED_TYPES)
		return true;

	return theType == (uint8_t)SeedType::SEED_BEGHOULED_BUTTON_SHUFFLE ||
		theType == (uint8_t)SeedType::SEED_BEGHOULED_BUTTON_CRATER ||
		theType == (uint8_t)SeedType::SEED_SLOT_MACHINE_SUN ||
		theType == (uint8_t)SeedType::SEED_SLOT_MACHINE_DIAMOND ||
		theType == (uint8_t)SeedType::SEED_ZOMBIQUARIUM_SNORKLE ||
		theType == (uint8_t)SeedType::SEED_ZOMBIQUARIUM_TROPHY ||
		(theType >= (uint8_t)SeedType::SEED_ZOMBIE_NORMAL &&
		 theType <= (uint8_t)SeedType::SEED_ZOMBIE_NEWSPAPER);
}

bool NetIsValidSlotMachineSeed(uint8_t theType)
{
	return theType == 255 ||
		theType == (uint8_t)SeedType::SEED_SUNFLOWER ||
		theType == (uint8_t)SeedType::SEED_PEASHOOTER ||
		theType == (uint8_t)SeedType::SEED_SNOWPEA ||
		theType == (uint8_t)SeedType::SEED_WALLNUT ||
		theType == (uint8_t)SeedType::SEED_SLOT_MACHINE_SUN ||
		theType == (uint8_t)SeedType::SEED_SLOT_MACHINE_DIAMOND;
}

struct NetDaveWireState
{
	uint8_t mState;
	uint8_t mAnimKey;
	int mMessageIndex;
	uint16_t mAnimTimeQ;
	int mX;
	int mY;
};

uint8_t NetGetDaveAnimKey(Reanimation* theReanim)
{
	if (!theReanim || theReanim->mDead)
		return NET_DAVE_ANIM_NONE;

	// Check the exceptional tracks before their more common siblings.  Some
	// Dave animations share base-pose frames, while IsAnimPlaying compares the
	// complete frame range selected by the named layer.
	const uint8_t aPriority[] =
	{
		NET_DAVE_ANIM_ENTER_UP, NET_DAVE_ANIM_GRAB, NET_DAVE_ANIM_LEAVE,
		NET_DAVE_ANIM_ENTER, NET_DAVE_ANIM_TALK_HANDING,
		NET_DAVE_ANIM_IDLE_HANDING, NET_DAVE_ANIM_TALK_CRAZY,
		NET_DAVE_ANIM_TALK_SMALL, NET_DAVE_ANIM_TALK_MEDIUM,
		NET_DAVE_ANIM_TALK_BLAH, NET_DAVE_ANIM_IDLE
	};
	for (uint8_t aKey : aPriority)
	{
		if (theReanim->IsAnimPlaying(gNetDaveAnimSpecs[aKey].mTrackName))
			return aKey;
	}
	return NET_DAVE_ANIM_NONE;
}

uint8_t NetGetFallbackDaveAnimKey(CrazyDaveState theState)
{
	switch (theState)
	{
	case CrazyDaveState::CRAZY_DAVE_ENTERING:        return NET_DAVE_ANIM_ENTER;
	case CrazyDaveState::CRAZY_DAVE_LEAVING:         return NET_DAVE_ANIM_LEAVE;
	case CrazyDaveState::CRAZY_DAVE_IDLING:          return NET_DAVE_ANIM_IDLE;
	case CrazyDaveState::CRAZY_DAVE_TALKING:         return NET_DAVE_ANIM_TALK_MEDIUM;
	case CrazyDaveState::CRAZY_DAVE_HANDING_TALKING: return NET_DAVE_ANIM_TALK_HANDING;
	case CrazyDaveState::CRAZY_DAVE_HANDING_IDLING:  return NET_DAVE_ANIM_IDLE_HANDING;
	default:                                          return NET_DAVE_ANIM_NONE;
	}
}

NetDaveWireState NetCaptureDaveWireState(LawnApp* theApp)
{
	NetDaveWireState aState = {};
	aState.mState = (uint8_t)CrazyDaveState::CRAZY_DAVE_OFF;
	aState.mAnimKey = NET_DAVE_ANIM_NONE;
	aState.mMessageIndex = -1;

	Reanimation* aDaveReanim = theApp->ReanimationTryToGet(theApp->mCrazyDaveReanimID);
	const int aRawDaveState = (int)theApp->mCrazyDaveState;
	if (!aDaveReanim || aDaveReanim->mDead ||
		aRawDaveState <= (int)CrazyDaveState::CRAZY_DAVE_OFF ||
		aRawDaveState > (int)CrazyDaveState::CRAZY_DAVE_HANDING_IDLING)
	{
		return aState;
	}

	aState.mAnimKey = NetGetDaveAnimKey(aDaveReanim);
	if (aState.mAnimKey == NET_DAVE_ANIM_NONE)
		aState.mAnimKey = NetGetFallbackDaveAnimKey(theApp->mCrazyDaveState);
	if (aState.mAnimKey == NET_DAVE_ANIM_NONE)
		return aState;

	aState.mState = (uint8_t)aRawDaveState;
	if (theApp->mCrazyDaveMessageIndex >= 0 && theApp->mCrazyDaveMessageIndex <= 9999)
		aState.mMessageIndex = theApp->mCrazyDaveMessageIndex;
	int aTimeQ = (int)(aDaveReanim->mAnimTime * 65535.0f + 0.5f);
	aState.mAnimTimeQ = (uint16_t)max(0, min(65535, aTimeQ));
	aState.mX = max(-32768, min(32767, (int)aDaveReanim->mOverlayMatrix.m02));
	aState.mY = max(-32768, min(32767, (int)aDaveReanim->mOverlayMatrix.m12));
	return aState;
}

void NetWriteDaveWireState(NetBuffer& theBuffer, const NetDaveWireState& theState)
{
	theBuffer.WriteU8(theState.mState);
	theBuffer.WriteU8(theState.mAnimKey);
	theBuffer.WriteI32(theState.mMessageIndex);
	theBuffer.WriteU16(theState.mAnimTimeQ);
	theBuffer.WriteI16((int16_t)theState.mX);
	theBuffer.WriteI16((int16_t)theState.mY);
}

bool NetValidateDaveWireState(const NetDaveWireState& theState)
{
	const bool aStateValid = theState.mState <= (uint8_t)CrazyDaveState::CRAZY_DAVE_HANDING_IDLING;
	const bool anAnimValid = theState.mAnimKey < NET_DAVE_ANIM_COUNT;
	const bool aMessageValid = theState.mMessageIndex == -1 ||
		(theState.mMessageIndex >= 0 && theState.mMessageIndex <= 9999);
	const bool anOffCanonical = theState.mState != (uint8_t)CrazyDaveState::CRAZY_DAVE_OFF ||
		(theState.mAnimKey == NET_DAVE_ANIM_NONE && theState.mMessageIndex == -1 &&
		 theState.mAnimTimeQ == 0 && theState.mX == 0 && theState.mY == 0);
	const bool anOnCanonical = theState.mState == (uint8_t)CrazyDaveState::CRAZY_DAVE_OFF ||
		theState.mAnimKey != NET_DAVE_ANIM_NONE;
	return aStateValid && anAnimValid && aMessageValid && anOffCanonical && anOnCanonical;
}

bool NetReadDaveWireState(NetBuffer& theBuffer, NetDaveWireState& theState)
{
	if (theBuffer.Remaining() < NET_DAVE_STATE_BYTES)
		return false;
	theState.mState = theBuffer.ReadU8Or();
	theState.mAnimKey = theBuffer.ReadU8Or();
	theState.mMessageIndex = theBuffer.ReadI32Or();
	theState.mAnimTimeQ = theBuffer.ReadU16Or();
	theState.mX = theBuffer.ReadI16Or();
	theState.mY = theBuffer.ReadI16Or();
	return NetValidateDaveWireState(theState);
}

void NetApplyDavePresentation(Board* theBoard, LawnApp* anApp, const NetDaveWireState& theState)
{
	if (theState.mState == (uint8_t)CrazyDaveState::CRAZY_DAVE_OFF)
	{
		theBoard->NetCleanupCrazyDavePresentation();
		return;
	}

	Reanimation* aDaveReanim = anApp->ReanimationTryToGet(anApp->mCrazyDaveReanimID);
	if (!aDaveReanim || aDaveReanim->mDead)
	{
		theBoard->NetCleanupCrazyDavePresentation();
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_CRAZY_DAVE, true);
		aDaveReanim = anApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_CRAZY_DAVE);
		if (aDaveReanim)
		{
			aDaveReanim->mIsAttachment = true;
			aDaveReanim->SetBasePoseFromAnim("anim_idle_handing");
			anApp->mCrazyDaveReanimID = anApp->ReanimationGetID(aDaveReanim);
			anApp->mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_IDLING;
			anApp->mCrazyDaveMessageIndex = -1;
			anApp->mCrazyDaveMessageText.clear();
			anApp->mCrazyDaveBlinkCounter = INT_MAX;
			if (anApp->IsStormyNightLevel())
				aDaveReanim->mColorOverride = Color(64, 64, 64);
			NetTrace("DAVE_STATE materialized client root=%u", (unsigned)anApp->mCrazyDaveReanimID);
		}
	}

	if (!aDaveReanim)
		return;

	if (theBoard->mNetDaveAppliedMessageIndex != theState.mMessageIndex)
	{
		if (theState.mMessageIndex >= 0)
		{
			// CrazyDaveTalkIndex may create a hand prop and local foley, so it is
			// strictly edge-triggered and never advances dialog authority here.
			SexyString aDaveMessage = anApp->GetCrazyDaveText(theState.mMessageIndex);
			const bool aReplacesHandProp =
				aDaveMessage.find(_S("{SHOW_WALLNUT}")) != SexyString::npos ||
				aDaveMessage.find(_S("{SHOW_HAMMER}")) != SexyString::npos ||
				aDaveMessage.find(_S("{SHOW_FERTILIZER}")) != SexyString::npos ||
				aDaveMessage.find(_S("{SHOW_TREE_FOOD}")) != SexyString::npos ||
				aDaveMessage.find(_S("{SHOW_MONEYBAG}")) != SexyString::npos;
			if (aReplacesHandProp && aDaveReanim->TrackExists("Dave_handinghand"))
			{
				ReanimatorTrackInstance* aHandTrack =
					aDaveReanim->GetTrackInstanceByName("Dave_handinghand");
				if (aHandTrack)
					AttachmentDie(aHandTrack->mAttachmentID);
			}
			// PlayFoley/PlayFoleyPitch only use gNetSuppressSounds as a
			// host rebroadcast guard.  Mute this synchronous client-side
			// presentation call so the authoritative BOARDEV_FOLEY is
			// heard once, then restore the pre-existing cutscene mute.
			const bool aWasMutedForCutscene = anApp->mMuteSoundsForCutscene;
			anApp->mMuteSoundsForCutscene = true;
			anApp->CrazyDaveTalkIndex(theState.mMessageIndex);
			anApp->mMuteSoundsForCutscene = aWasMutedForCutscene;
		}
		else
		{
			aDaveReanim->SetImageOverride("Dave_mouths", nullptr);
			if (theState.mState != (uint8_t)CrazyDaveState::CRAZY_DAVE_HANDING_TALKING &&
				theState.mState != (uint8_t)CrazyDaveState::CRAZY_DAVE_HANDING_IDLING)
			{
				if (aDaveReanim->TrackExists("Dave_handinghand"))
				{
					ReanimatorTrackInstance* aHandTrack =
						aDaveReanim->GetTrackInstanceByName("Dave_handinghand");
					if (aHandTrack)
						AttachmentDie(aHandTrack->mAttachmentID);
				}
			}
			anApp->mCrazyDaveMessageIndex = -1;
			anApp->mCrazyDaveMessageText.clear();
			anApp->CrazyDaveStopSound();
		}
		theBoard->mNetDaveAppliedMessageIndex = theState.mMessageIndex;
		NetTrace("DAVE_STATE applied message edge index=%d", theState.mMessageIndex);
	}

	if (theBoard->mNetDaveAppliedAnimKey != theState.mAnimKey)
	{
		const NetDaveAnimSpec& aSpec = gNetDaveAnimSpecs[theState.mAnimKey];
		aDaveReanim->PlayReanim(aSpec.mTrackName, aSpec.mLoopType, 0, aSpec.mAnimRate);
		theBoard->mNetDaveAppliedAnimKey = theState.mAnimKey;
		NetTrace("DAVE_STATE applied anim edge key=%u track=%s", theState.mAnimKey, aSpec.mTrackName);
	}

	const float aDaveAnimTime = theState.mAnimTimeQ / 65535.0f;
	aDaveReanim->mAnimTime = aDaveAnimTime;
	aDaveReanim->mLastFrameTime = aDaveAnimTime;
	aDaveReanim->mLoopCount = 0;
	aDaveReanim->SetPosition((float)theState.mX, (float)theState.mY);
	anApp->mCrazyDaveState = (CrazyDaveState)theState.mState;
}

bool NetGridStateMatchesType(uint8_t theType, uint8_t theState)
{
	switch ((GridItemType)theType)
	{
	case GRIDITEM_GRAVESTONE:		return theState <= GRIDITEM_STATE_GRAVESTONE_SPECIAL;
	case GRIDITEM_CRATER:
	case GRIDITEM_LADDER:
	case GRIDITEM_BRAIN:			return theState == GRIDITEM_STATE_NORMAL;
	case GRIDITEM_PORTAL_CIRCLE:
	case GRIDITEM_PORTAL_SQUARE:	return theState == GRIDITEM_STATE_NORMAL || theState == GRIDITEM_STATE_PORTAL_CLOSED;
	case GRIDITEM_SCARY_POT:		return theState >= GRIDITEM_STATE_SCARY_POT_QUESTION && theState <= GRIDITEM_STATE_SCARY_POT_ZOMBIE;
	case GRIDITEM_SQUIRREL:		return theState >= GRIDITEM_STATE_SQUIRREL_WAITING && theState <= GRIDITEM_STATE_SQUIRREL_ZOMBIE;
	case GRIDITEM_ZEN_TOOL:		return theState >= GRIDITEM_STATE_ZEN_TOOL_WATERING_CAN && theState <= GRIDITEM_STATE_ZEN_TOOL_GOLD_WATERING_CAN;
	case GRIDITEM_STINKY:			return theState >= GRIDITEM_STINKY_WALKING_LEFT && theState <= GRIDITEM_STINKY_WAKING_UP;
	case GRIDITEM_RAKE:			return theState >= GRIDITEM_STATE_RAKE_ATTRACTING && theState <= GRIDITEM_STATE_RAKE_TRIGGERED;
	case GRIDITEM_IZOMBIE_BRAIN:	return theState == GRIDITEM_STATE_NORMAL || theState == GRIDITEM_STATE_BRAIN_SQUISHED;
	default:						return false;
	}
}

bool NetProjectileFrameIsSafe(uint8_t theType, uint8_t theFrame)
{
	Image* anImage = nullptr;
	switch ((ProjectileType)theType)
	{
	case ProjectileType::PROJECTILE_PEA:
	case ProjectileType::PROJECTILE_ZOMBIE_PEA:
		anImage = IMAGE_PROJECTILEPEA;
		break;
	case ProjectileType::PROJECTILE_SNOWPEA:
		anImage = IMAGE_PROJECTILESNOWPEA;
		break;
	case ProjectileType::PROJECTILE_FIREBALL:
		// FIREBALL is drawn entirely by its REANIM_FIRE_PEA attachment.
		return theFrame == 0;
	case ProjectileType::PROJECTILE_SPIKE:
#ifdef _PIERCING_CACTUS
	case ProjectileType::PROJECTILE_PIERCE_SPIKE:
#endif
		anImage = IMAGE_PROJECTILECACTUS;
		break;
	case ProjectileType::PROJECTILE_STAR:
		anImage = IMAGE_PROJECTILE_STAR;
		break;
	case ProjectileType::PROJECTILE_PUFF:
		anImage = IMAGE_PUFFSHROOM_PUFF1;
		break;
	case ProjectileType::PROJECTILE_BASKETBALL:
		anImage = IMAGE_REANIM_ZOMBIE_CATAPULT_BASKETBALL;
		break;
	case ProjectileType::PROJECTILE_CABBAGE:
		anImage = IMAGE_REANIM_CABBAGEPULT_CABBAGE;
		break;
	case ProjectileType::PROJECTILE_KERNEL:
		anImage = IMAGE_REANIM_CORNPULT_KERNAL;
		break;
	case ProjectileType::PROJECTILE_BUTTER:
		anImage = IMAGE_REANIM_CORNPULT_BUTTER;
		break;
	case ProjectileType::PROJECTILE_MELON:
		anImage = IMAGE_REANIM_MELONPULT_MELON;
		break;
	case ProjectileType::PROJECTILE_WINTERMELON:
		anImage = IMAGE_REANIM_WINTERMELON_PROJECTILE;
		break;
	case ProjectileType::PROJECTILE_COBBIG:
		anImage = IMAGE_REANIM_COBCANNON_COB;
		break;
	default:
		anImage = IMAGE_PROJECTILE;
		break;
	}
	return anImage && theFrame < anImage->mNumCols;
}

void NetEnsureGridItemVisual(GridItem* theGridItem, GridItemState thePreviousState)
{
	Reanimation* aReanim = theGridItem->mApp->ReanimationTryToGet(theGridItem->mGridItemReanimID);
	bool aCreatedVisual = false;
	if (theGridItem->mGridItemType == GridItemType::GRIDITEM_ZEN_TOOL)
	{
		ReanimationType type = ReanimationType::REANIM_ZENGARDEN_WATERINGCAN;
		float x = theGridItem->mPosX - 8.0f, y = theGridItem->mPosY - 40.0f;
		switch (theGridItem->mGridItemState)
		{
		case GRIDITEM_STATE_ZEN_TOOL_GOLD_WATERING_CAN: x = theGridItem->mPosX; y = theGridItem->mPosY; break;
		case GRIDITEM_STATE_ZEN_TOOL_WATERING_CAN: break;
		case GRIDITEM_STATE_ZEN_TOOL_FERTILIZER:
			type = ReanimationType::REANIM_ZENGARDEN_FERTILIZER; x -= 32.0f; break;
		case GRIDITEM_STATE_ZEN_TOOL_BUG_SPRAY:
			type = ReanimationType::REANIM_ZENGARDEN_BUGSPRAY; x += 22.0f; break;
		case GRIDITEM_STATE_ZEN_TOOL_PHONOGRAPH:
			type = ReanimationType::REANIM_ZENGARDEN_PHONOGRAPH; x -= 12.0f; y += 34.0f; break;
		default: return;
		}
		if (aReanim && (aReanim->mDead || aReanim->mReanimationType != type))
		{
			aReanim->ReanimationDie(); aReanim = nullptr;
		}
		if (!aReanim)
		{
			ReanimatorEnsureDefinitionLoaded(type, true);
			aReanim = theGridItem->mApp->AddReanimation(x, y, 0, type);
			if (!aReanim) return;
			theGridItem->mGridItemReanimID = theGridItem->mApp->ReanimationGetID(aReanim);
			if (theGridItem->mGridItemState == GRIDITEM_STATE_ZEN_TOOL_WATERING_CAN)
				aReanim->PlayReanim("anim_water", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 0.0f);
			else if (theGridItem->mGridItemState == GRIDITEM_STATE_ZEN_TOOL_GOLD_WATERING_CAN)
				aReanim->PlayReanim("anim_water_area", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 8.0f);
			else
			{
				aReanim->mLoopType = theGridItem->mGridItemState == GRIDITEM_STATE_ZEN_TOOL_PHONOGRAPH ?
					ReanimLoopType::REANIM_LOOP : ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD;
				if (theGridItem->mGridItemState == GRIDITEM_STATE_ZEN_TOOL_PHONOGRAPH) aReanim->mAnimRate = 20.0f;
			}
		}
		aReanim->SetPosition(x, y);
		return;
	}

	if (theGridItem->mGridItemType == GridItemType::GRIDITEM_PORTAL_CIRCLE ||
		theGridItem->mGridItemType == GridItemType::GRIDITEM_PORTAL_SQUARE)
	{
		if (!aReanim)
		{
			theGridItem->OpenPortal();
			aReanim = theGridItem->mApp->ReanimationTryToGet(theGridItem->mGridItemReanimID);
			aCreatedVisual = true;
		}
		if (theGridItem->mGridItemState == GridItemState::GRIDITEM_STATE_PORTAL_CLOSED &&
			(aCreatedVisual || thePreviousState != theGridItem->mGridItemState))
		{
			theGridItem->ClosePortal();
		}
		else if (!aCreatedVisual && thePreviousState == GridItemState::GRIDITEM_STATE_PORTAL_CLOSED &&
			theGridItem->mGridItemState == GridItemState::GRIDITEM_STATE_NORMAL)
		{
			theGridItem->OpenPortal();
		}
		return;
	}

	if (theGridItem->mGridItemType == GridItemType::GRIDITEM_RAKE)
	{
		if (!aReanim)
		{
			aReanim = theGridItem->mBoard->CreateRakeReanim(
				(float)theGridItem->mGridX, (float)theGridItem->mGridY, 0);
			theGridItem->mGridItemReanimID = theGridItem->mApp->ReanimationGetID(aReanim);
		}
		if (aReanim)
			aReanim->mAnimRate = theGridItem->mGridItemState == GridItemState::GRIDITEM_STATE_RAKE_TRIGGERED ? 20.0f : 0.0f;
		return;
	}

	if (theGridItem->mGridItemType != GridItemType::GRIDITEM_STINKY)
		return;

	if (!aReanim)
	{
		aReanim = theGridItem->mApp->AddReanimation(
			theGridItem->mPosX, theGridItem->mPosY, 0, ReanimationType::REANIM_STINKY);
		theGridItem->mGridItemReanimID = theGridItem->mApp->ReanimationGetID(aReanim);
		aCreatedVisual = true;
	}
	if (!aReanim)
		return;

	if (aCreatedVisual || thePreviousState != theGridItem->mGridItemState)
	{
		theGridItem->mMotionTrailCount = 0;
		switch (theGridItem->mGridItemState)
		{
		case GridItemState::GRIDITEM_STINKY_WALKING_LEFT:
		case GridItemState::GRIDITEM_STINKY_WALKING_RIGHT:
			aReanim->PlayReanim("anim_crawl", ReanimLoopType::REANIM_LOOP, 0, 6.0f);
			break;
		case GridItemState::GRIDITEM_STINKY_TURNING_LEFT:
		case GridItemState::GRIDITEM_STINKY_TURNING_RIGHT:
			aReanim->PlayReanim("turn", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 6.0f);
			break;
		case GridItemState::GRIDITEM_STINKY_FALLING_ASLEEP:
			aReanim->PlayReanim("anim_in", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 6.0f);
			break;
		case GridItemState::GRIDITEM_STINKY_WAKING_UP:
			aReanim->PlayReanim("anim_out", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 6.0f);
			break;
		case GridItemState::GRIDITEM_STINKY_SLEEPING:
			aReanim->PlayReanim("anim_out", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 0.0f);
			aReanim->mAnimRate = 0.0f;
			break;
		default:
			break;
		}
	}
	// Stinky's sleeping animation is a child of his shell, not a body track.
	// Mirror that attachment without calling ZenGarden's profile/advice logic.
	Reanimation* aSleeping = FindReanimAttachment(
		aReanim->GetTrackInstanceByName("shell")->mAttachmentID);
	if (theGridItem->mGridItemState == GridItemState::GRIDITEM_STINKY_SLEEPING)
	{
		if (!aSleeping || aSleeping->mDead)
		{
			aSleeping = theGridItem->mApp->AddReanimation(0.0f, 0.0f, 0,
				ReanimationType::REANIM_SLEEPING);
			if (aSleeping)
			{
				aSleeping->mAnimRate = 3.0f;
				aSleeping->mLoopType = ReanimLoopType::REANIM_LOOP;
				AttachReanim(aReanim->GetTrackInstanceByName("shell")->mAttachmentID,
					aSleeping, 34.0f, 39.0f);
			}
		}
	}
	else if (aSleeping && !aSleeping->mDead)
		aSleeping->ReanimationDie();

	if (theGridItem->mGridItemState == GridItemState::GRIDITEM_STINKY_WALKING_RIGHT ||
		theGridItem->mGridItemState == GridItemState::GRIDITEM_STINKY_TURNING_LEFT)
	{
		aReanim->OverrideScale(-0.8f, 0.8f);
		aReanim->SetPosition(theGridItem->mPosX + 69.0f, theGridItem->mPosY);
	}
	else
	{
		aReanim->OverrideScale(0.8f, 0.8f);
		aReanim->SetPosition(theGridItem->mPosX, theGridItem->mPosY);
	}
}
}

void LawnNet::NetBroadcastSourceCharred315(LawnApp* app,const LawnFusion::SourceCharred311Birth& birth)
{
    const auto* context=LawnFusion::GetNativeContext(app?app->mBoard:nullptr);
    if(!app||!app->NetIsHost()||!app->mNetManager||!context||context->epoch!=birth.epoch)return;
    NetBuffer b;if(NetEncodeSourceCharred315(birth,b))app->mNetManager->SendBoardEvent(b,BOARDEV_FUSION_CHARRED315);
}
void LawnNet::NetBroadcastFusionLimb276(LawnApp* app,const NetFusionLimbEvent276& event)
{
    if(!app||!app->NetIsHost()||!app->mNetManager||!app->mBoard)return;
    const auto* context=LawnFusion::GetNativeContext(app->mBoard);
    if(!context||context->epoch!=event.mEpoch)return;
    NetBuffer payload;
    if(NetEncodeFusionLimbEvent276(event,payload))
        app->mNetManager->SendBoardEvent(payload,BOARDEV_FUSION_LIMB276);
}
namespace
{
int SourceSound284(uint16_t kind)
{
    // Source GameAPP.LoadSound slots map to the resident original samples.
    // Stable native sample IDs retain the existing reliable sound event path.
    switch(kind)
    {
    case 0:return Sexy::SOUND_SPLAT;
    case 1:return Sexy::SOUND_SPLAT2;
    case 2:return Sexy::SOUND_SPLAT3;
    case 3:return Sexy::SOUND_THROW;
    case 4:return Sexy::SOUND_THROW2;
    case 5:return Sexy::SOUND_ZOMBIE_FALLING_1;
    case 6:return Sexy::SOUND_ZOMBIE_FALLING_2;
    case 7:return Sexy::SOUND_LIMBS_POP;
    case 8:return Sexy::SOUND_CHOMP;
    case 9:return Sexy::SOUND_CHOMP2;
    case 40:return Sexy::SOUND_CHERRYBOMB; // Source LoadSound[40]: Sound/Bomb/cherrybomb.
    case 42:return Sexy::SOUND_JALAPENO; // Source LoadSound[42]: Sound/Bomb/jalapeno.
    case 43:return Sexy::SOUND_EXPLOSION;
    case 44:return Sexy::SOUND_NEWSPAPER_RIP;
    case 45:return Sexy::SOUND_NEWSPAPER_RARRGH;
    case 46:return Sexy::SOUND_NEWSPAPER_RARRGH2;
    case 50:return Sexy::SOUND_GRASSSTEP;
    case 53:return Sexy::SOUND_BOWLING;
    case 54:return Sexy::SOUND_BOWLINGIMPACT;
    case 55:return Sexy::SOUND_BOWLINGIMPACT2;
    case 62:return Sexy::SOUND_FLOOP;
    case 63:return Sexy::SOUND_MINDCONTROLLED;
    case 64:return Sexy::SOUND_BONK;
    case 66:return Sexy::SOUND_PRIZE;
    case 68:return Sexy::SOUND_SNOW_PEA_SPARKLES;
    case 104:return Sexy::SOUND_MELONIMPACT;
    case 105:return Sexy::SOUND_MELONIMPACT2;
    case 69:return Sexy::SOUND_DANCER;
    case 72:return Sexy::SOUND_SQUASH_HMM;
    case 73:return Sexy::SOUND_SQUASH_HMM2;
    case 74:return Sexy::SOUND_GARGANTUAR_THUMP;
    case 75:return Sexy::SOUND_ZOMBIESPLASH;
    case 76:return Sexy::SOUND_ZAMBONI;
    case 77:return Sexy::SOUND_BALLOON_POP;
    case 78:return Sexy::SOUND_DOLPHIN_APPEARS;
    case 79:return Sexy::SOUND_DOLPHIN_BEFORE_JUMPING;
    case 86:return Sexy::SOUND_BALLOONINFLATE;
    case 84:return Sexy::SOUND_WINMUSIC;
    case 93:return Sexy::SOUND_SWING;
    case 90:return Sexy::SOUND_JACK_SURPRISE;
    case 91:return Sexy::SOUND_JACK_SURPRISE2;
    case 94:return Sexy::SOUND_WAKEUP;
    case 101:return Sexy::SOUND_BASKETBALL;
    case 109:return Sexy::SOUND_POGO_ZOMBIE;
    case 110:return Sexy::SOUND_IMP;
    case 111:return Sexy::SOUND_IMP2;
    case 112:return Sexy::SOUND_GARGANTUDEATH;
    case 118:return Sexy::SOUND_HYDRAULIC;
    case 119:return Sexy::SOUND_HYDRAULIC_SHORT;
    case 120:return Sexy::SOUND_RVTHROW;
    case 121:return Sexy::SOUND_BOSSBOULDERATTACK;
    case 122:return Sexy::SOUND_BUNGEE_SCREAM;
    case 123:return Sexy::SOUND_BUNGEE_SCREAM2;
    case 124:return Sexy::SOUND_BUNGEE_SCREAM3;
    default:return -1;
    }
}
int ResolveSourceSound313(LawnApp* app,uint16_t kind)
{
    if(kind!=92)return SourceSound284(kind);
    return app&&app->mSoundManager?app->mSoundManager->LoadSound("fusion/sounds/source-92"):-1;
}
bool PlayVolumeSound284(LawnApp* app,int sound,float volume,bool broadcast,float pitchRatio=1.f)
{
    if(!app||!app->mSoundManager||sound<0||sound>UINT16_MAX||!std::isfinite(volume)||volume<0||volume>1)return false;
    if(app->mMuteSoundsForCutscene)return true;
    if(volume==1.f&&pitchRatio==1.f){app->PlaySample(sound);return true;}
    if(auto* instance=app->mSoundManager->GetSoundInstance(sound))
    {instance->SetVolume(volume);instance->AdjustPitch(12.0*std::log2(pitchRatio));instance->Play(false,true);}
    // An exhausted local audio channel is cosmetic; peers still consume the
    // one reliable cue, and no source action is replayed to retry that sound.
    if(broadcast&&app->NetIsHost()&&app->mNetManager&&!LawnNet::gNetSuppressSounds)
    {
        NetBuffer payload;payload.WriteU16(uint16_t(sound));payload.WriteF32(volume);if(pitchRatio!=1.f)payload.WriteF32(pitchRatio);
        app->mNetManager->SendBoardEvent(payload,BOARDEV_SOUND);
    }
    return true;
}
}
bool LawnNet::NetSourceSound284Ready(LawnApp* app,uint16_t sourceSound)
{const int id=ResolveSourceSound313(app,sourceSound);return app&&app->mSoundManager&&id>=0&&id<=UINT16_MAX;}
bool LawnNet::NetPlaySourceSound284(LawnApp* app,uint16_t sourceSound,float volume,float pitchRatio)
{
    if(!app||!app->mSoundManager||!std::isfinite(volume)||volume<0||volume>1||!std::isfinite(pitchRatio)||pitchRatio<=0||pitchRatio>4)return false;
    const int id=ResolveSourceSound313(app,sourceSound);if(id<0||id>UINT16_MAX)return false;
    if(sourceSound==92)
    {
        // Dynamically loaded sample IDs can differ between peers. Transmit the
        // stable source slot, then resolve its genuine local sample on receipt.
        if(!app->mMuteSoundsForCutscene)
            if(auto* instance=app->mSoundManager->GetSoundInstance(id))
            {instance->SetVolume(volume);instance->AdjustPitch(12.0*std::log2(pitchRatio));instance->Play(false,true);}
        if(app->NetIsHost()&&app->mNetManager&&!gNetSuppressSounds)
        {
            NetBuffer payload;payload.WriteU16(uint16_t(0x8000u|sourceSound));payload.WriteF32(volume);if(pitchRatio!=1.f)payload.WriteF32(pitchRatio);
            app->mNetManager->SendBoardEvent(payload,BOARDEV_SOUND);
        }
    }
    else if(!PlayVolumeSound284(app,id,volume,true,pitchRatio))return false;
    NetTrace("Fusion source sound284 produced source=%u native=%d volume=%.9g role=%s",sourceSound,id,volume,app->NetIsClient()?"client":"host");
    return true;
}

void LawnNet::NetBroadcastParticle(LawnApp* theApp, float theX, float theY,
	int theRenderOrder, int theEffect)
{
	if (!theApp || !theApp->NetIsHost() || !theApp->mNetManager)
		return;
	NetBuffer anEvent;
	anEvent.WriteU16((uint16_t)theEffect);
	anEvent.WriteF32(theX);
	anEvent.WriteF32(theY);
	anEvent.WriteI32(theRenderOrder);
	theApp->mNetManager->SendBoardEvent(anEvent, BOARDEV_PRESENTATION_PARTICLE);
}

void LawnNet::NetBroadcastReanimation(LawnApp* theApp, Reanimation* theReanim,
	float theX, float theY, int theRenderOrder, int theType)
{
	if (!theApp || !theApp->NetIsHost() || !theApp->mNetManager || !theReanim)
		return;
	NetZombieAnimState aState;
	if (!NetCaptureReanimState(theApp, theApp->ReanimationGetID(theReanim), aState))
		return;
	NetBuffer anEvent;
	anEvent.WriteU16((uint16_t)theType);
	anEvent.WriteF32(theX);
	anEvent.WriteF32(theY);
	anEvent.WriteI32(theRenderOrder);
	if (NetEncodeZombieAnimState(aState, anEvent))
		theApp->mNetManager->SendBoardEvent(anEvent, BOARDEV_PRESENTATION_REANIM);
}

void LawnNet::NetBroadcastFwoosh(LawnApp* theApp, int theRow)
{
	if (!theApp || !theApp->NetIsHost() || !theApp->mNetManager || !theApp->mBoard ||
		theRow < 0 || theRow >= MAX_GRID_SIZE_Y) return;
	NetBuffer anEvent;
	anEvent.WriteU8(static_cast<uint8_t>(theRow));
	for (int i=0;i<12;++i)
	{
		const auto id = theApp->mBoard->mFwooshID[theRow][i];
		Reanimation* effect = theApp->ReanimationTryToGet(id);
		NetZombieAnimState state;
		if (!effect || !NetCaptureReanimState(theApp,id,state) || !state.mPresent || state.mDead) return;
		anEvent.WriteF32(effect->mOverlayMatrix.m02); anEvent.WriteF32(effect->mOverlayMatrix.m12);
		anEvent.WriteF32(effect->mOverlayMatrix.m00); anEvent.WriteF32(effect->mOverlayMatrix.m11);
		anEvent.WriteI32(effect->mRenderOrder);
		if (!NetEncodeZombieAnimState(state,anEvent)) return;
	}
	theApp->mNetManager->SendBoardEvent(anEvent,BOARDEV_PRESENTATION_FWOOSH);
}

void Board::NetCleanupCrazyDavePresentation()
{
	Reanimation* aBlinkReanim = mApp->ReanimationTryToGet(mApp->mCrazyDaveBlinkReanimID);
	if (aBlinkReanim)
		aBlinkReanim->ReanimationDie();

	Reanimation* aDaveReanim = mApp->ReanimationTryToGet(mApp->mCrazyDaveReanimID);
	if (aDaveReanim)
		aDaveReanim->ReanimationDie(); // also owns every hand/body attachment

	const bool aHadDavePresentation = aBlinkReanim || aDaveReanim ||
		mApp->mCrazyDaveState != CrazyDaveState::CRAZY_DAVE_OFF ||
		mApp->mCrazyDaveMessageIndex != -1 || !mApp->mCrazyDaveMessageText.empty();
	mApp->mCrazyDaveReanimID = ReanimationID::REANIMATIONID_NULL;
	mApp->mCrazyDaveBlinkReanimID = ReanimationID::REANIMATIONID_NULL;
	mApp->mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_OFF;
	mApp->mCrazyDaveBlinkCounter = 0;
	mApp->mCrazyDaveMessageIndex = -1;
	mApp->mCrazyDaveMessageText.clear();
	if (aHadDavePresentation)
		mApp->CrazyDaveStopSound();

	mNetDaveAppliedAnimKey = 255;
	mNetDaveAppliedMessageIndex = INT_MIN;
}

void Board::NetEncodeDaveState(NetBuffer& theBuffer)
{
	NetWriteDaveWireState(theBuffer, NetCaptureDaveWireState(mApp));
}

void Board::NetApplyDaveState(NetBuffer& theBuffer)
{
	if (theBuffer.Remaining() != NET_DAVE_STATE_BYTES)
	{
		NetTrace("DAVE_STATE rejected: bytes=%zu expected=%zu",
			theBuffer.Remaining(), NET_DAVE_STATE_BYTES);
		return;
	}

	NetDaveWireState aState = {};
	if (!NetReadDaveWireState(theBuffer, aState) || theBuffer.Remaining() != 0)
	{
		NetTrace("DAVE_STATE rejected: state=(%u,%u,%d,%u,%d,%d) remaining=%zu",
			aState.mState, aState.mAnimKey, aState.mMessageIndex, aState.mAnimTimeQ,
			aState.mX, aState.mY, theBuffer.Remaining());
		return;
	}

	NetApplyDavePresentation(this, mApp, aState);
}

namespace LawnNet
{

// ==========================================================================
// Tower defense path (shared between host and clients; host generates,
// clients receive it with the start-game message)
// ==========================================================================
}

void Board::GenerateTowerDefensePath(std::vector<uint8_t>& thePath)
{
	thePath.clear();
	const int aBottomRow = 4; // standard day lawn rows are 0..4

	// Three well-separated vertical runs make the road touch alternating
	// horizontal edges without folding back alongside itself.  Mirror the
	// pattern from the authoritative board seed so both top->bottom->top and
	// bottom->top->bottom occur randomly in play and reproducibly in smoke runs.
	const int anEdgeVisitColumns[] = { 7, 4, 1 };
	const uint32_t aPathSeed = gLawnApp && gLawnApp->mNetBoardSeed != 0 ?
		gLawnApp->mNetBoardSeed : (uint32_t)Rand();
	const int aFirstEdge = (aPathSeed & 1U) == 0U ? 0 : aBottomRow;
	const int anEdgeRows[] = { aFirstEdge, aBottomRow - aFirstEdge, aFirstEdge };
	int aRow = 1 + (int)((aPathSeed >> 1) % (uint32_t)(aBottomRow - 1));
	int aCol = MAX_GRID_SIZE_X - 1;
	auto AppendCell = [&]()
	{
		thePath.push_back((uint8_t)(aRow * 2));
		thePath.push_back((uint8_t)aCol);
	};
	AppendCell();

	auto MoveToRow = [&](int theTargetRow)
	{
		while (aRow != theTargetRow)
		{
			aRow += theTargetRow > aRow ? 1 : -1;
			AppendCell();
		}
	};

	for (int aVisit = 0; aVisit < 3; ++aVisit)
	{
		while (aCol > anEdgeVisitColumns[aVisit])
		{
			--aCol;
			AppendCell();
		}
		MoveToRow(anEdgeRows[aVisit]);
	}
	while (aCol > 0)
	{
		--aCol;
		AppendCell();
	}
}

void Board::SetTowerDefensePath(const std::vector<uint8_t>& thePath)
{
	if (!gLawnApp || !gLawnApp->mBoard)
		return;
	gLawnApp->mBoard->mTDPath = thePath;
}

bool Board::NetTdIsPathCell(int theGridX, int theGridY)
{
	if (mTDPath.empty())
		return false;
	for (size_t i = 0; i + 1 < mTDPath.size(); i += 2)
	{
		uint8_t aRow = mTDPath[i] >> 1;
		uint8_t aCol = mTDPath[i + 1];
		if (aRow == theGridY && aCol == theGridX)
			return true;
	}
	return false;
}

// Map a 0..1 position along the path to a (row, col) cell
int Board::NetTdPathPosToCell(float thePos, int& theRow, int& theCol)
{
	int aCells = (int)(mTDPath.size() / 2);
	if (aCells == 0)
	{
		theRow = 0;
		theCol = MAX_GRID_SIZE_X - 1;
		return 0;
	}
	int aIndex = (int)(thePos * aCells);
	if (aIndex < 0) aIndex = 0;
	if (aIndex >= aCells) aIndex = aCells - 1;
	theRow = mTDPath[aIndex * 2] >> 1;
	theCol = mTDPath[aIndex * 2 + 1];
	return aIndex;
}

namespace LawnNet
{

// ==========================================================================
// Host: id allocation + snapshot
// ==========================================================================
}

uint16_t Board::NetAllocId()
{
	// Zero is the wire sentinel for "no authoritative identity".  Endless
	// sessions can wrap the 16-bit counter, so also avoid every still-live id.
	auto IsLiveId = [&](uint16_t theId)
	{
		if (mSeedBank)
		{
			for (int i = 0; i < mSeedBank->mNumPackets && i < SEEDBANK_CAPACITY; i++)
			{
				const SeedPacket& aPacket = mSeedBank->mSeedPackets[i];
				if ((aPacket.mPacketType != SeedType::SEED_NONE || aPacket.IsMysteryPending()) && aPacket.mNetId == theId)
					return true;
			}
		}
		Plant* aPlant = nullptr;
		while (mPlants.IterateNext(aPlant)) if (!aPlant->mDead && aPlant->mNetId == theId) return true;
		Zombie* aZombie = nullptr;
		while (mZombies.IterateNext(aZombie)) if (!aZombie->mDead && aZombie->mNetId == theId) return true;
		Projectile* aProjectile = nullptr;
		while (mProjectiles.IterateNext(aProjectile)) if (!aProjectile->mDead && aProjectile->mNetId == theId) return true;
		Coin* aCoin = nullptr;
		while (mCoins.IterateNext(aCoin)) if (!aCoin->mDead && aCoin->mNetId == theId) return true;
		LawnMower* aMower = nullptr;
		while (mLawnMowers.IterateNext(aMower)) if (!aMower->mDead && aMower->mNetId == theId) return true;
		GridItem* aGridItem = nullptr;
		while (mGridItems.IterateNext(aGridItem)) if (!aGridItem->mDead && aGridItem->mNetId == theId) return true;
		return false;
	};

	for (uint32_t anAttempt = 0; anAttempt < 0xFFFFU; ++anAttempt)
	{
		do
		{
			++mNextNetId;
		} while (mNextNetId == 0);
		if (!IsLiveId(mNextNetId))
			return mNextNetId;
	}
	NetTrace("fatal: exhausted all authoritative entity ids");
	return 0;
}

void Board::NetHostTick()
{
	NetManager* aNet = mApp->mNetManager;
	if (!aNet || !aNet->IsHosting())
		return;

	// snapshot at adaptive rate
	uint32_t aNow = GetTickCount();
	if (mLastSnapshotTick == 0)
		mLastSnapshotTick = aNow;
	int aInterval = 1000 / max(1, aNet->GetSnapshotHz());
	if (aNow - mLastSnapshotTick >= (uint32_t)aInterval)
	{
		mLastSnapshotTick = aNow;
		if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO)
		{
			// The intro uses the same authoritative entities as gameplay.  Keeping
			// only street-zombie previews dropped the introductory plants, moving
			// zombies and projectiles because clients never run that simulation.
			NetBuffer aSnap;
			NetEncodeSnapshot(aSnap);
			aNet->SendSnapshot(aSnap);
			NetBuffer anIntro;
			NetEncodeIntroState(anIntro);
			aNet->BroadcastMessage(NETMSG_INTRO_STATE, anIntro);
		}
		else if (mApp->mGameScene == GameScenes::SCENE_PLAYING ||
			mApp->mGameScene == GameScenes::SCENE_ZOMBIES_WON)
		{
			NetBuffer aSnap;
			NetEncodeSnapshot(aSnap);
			aNet->SendSnapshot(aSnap);

			// Dave can enter after the intro (Scary Potter, Zen Garden and Tree of
			// Wisdom).  Keep his presentation separate from the large snapshot and
			// send canonical OFF as well so clients remove him at dialog end.
			NetBuffer aDaveState;
			NetEncodeDaveState(aDaveState);
			aNet->BroadcastMessage(NETMSG_DAVE_STATE, aDaveState);
		}
	}

}

void Board::NetEncodeSnapshot(NetBuffer& b)
{
	// ---- board core ----
	uint8_t aFlags = 0;
	if (mPaused) aFlags |= SNAPFLAG_PAUSED;
	if (mLevelComplete) aFlags |= SNAPFLAG_LEVEL_COMPLETE;
	if (mApp->mGameScene == GameScenes::SCENE_ZOMBIES_WON) aFlags |= SNAPFLAG_ZOMBIES_WON;
	if (mFogBlownCountDown > 0) aFlags |= SNAPFLAG_FOG_BLOWN;
	if (mApp->mShowHealthBar) aFlags |= SNAPFLAG_HEALTHBAR;
	if (mChallenge && mChallenge->mShowBowlingLine) aFlags |= SNAPFLAG_BOWLING_LINE;

	b.WriteU8(aFlags);
	b.WriteI32(mSunMoney);
	b.WriteI32(mApp->mPlayerInfo ? mApp->mPlayerInfo->mCoins : 0);
	b.WriteU8((uint8_t)min(255, mCurrentWave));
	b.WriteU8((uint8_t)min(255, mNumWaves));
	b.WriteU16((uint16_t)max(0, mHugeWaveCountDown));
	b.WriteU16((uint16_t)max(0, mZombieCountDown));
	// -1 means that no fade is active.  Zero is an active, fully opaque fade,
	// so clamping the sentinel to zero covered every client with a white quad.
	b.WriteU16((uint16_t)min(65535, max(0, mBoardFadeOutCounter + 1)));
	b.WriteU16((uint16_t)max(0, mNextSurvivalStageCounter));
	b.WriteU16((uint16_t)(mFogOffset * 2));
	b.WriteU16((uint16_t)max(0, mFogBlownCountDown));
	b.WriteU8((uint8_t)min(255, mLevel));
	b.WriteU8((uint8_t)mTutorialState);
	b.WriteI32(mTutorialTimer);
	b.WriteU8(mShowShovel ? 1 : 0);
	b.WriteI16((int16_t)(mApp->mMusic ? mApp->mMusic->mCurMusicTune : MusicTune::MUSIC_TUNE_NONE));
	b.WriteU8(mApp->mMusic && mApp->mMusic->mPaused ? 1 : 0);
	b.WriteU8((uint8_t)min(255, mIceTrapCounter));
	b.WriteU8((uint8_t)min(255, mShakeCounter));
	// Board widget coordinates are canonical 800x600 camera coordinates.  The
	// local widescreen viewport offset is deliberately not folded into them.
	b.WriteI16((int16_t)max(-2048, min(2048, mX)));
	b.WriteI16((int16_t)max(-1024, min(1024, mY)));
	b.WriteI16((int16_t)max(-4096, min(4096, mTreeX)));
	b.WriteI16((int16_t)max(-4096, min(4096, mPoleX)));

	// ice trails (visual)
	for (int i = 0; i < MAX_GRID_SIZE_Y; i++)
	{
		b.WriteI16((int16_t)mIceMinX[i]);
		b.WriteU16((uint16_t)max(0, mIceTimer[i]));
	}

	// challenge essentials
	b.WriteU32((uint32_t)max(0, mChallenge->mSurvivalStage));
	b.WriteU8((uint8_t)mChallenge->mChallengeState);
	b.WriteU16((uint16_t)min(65535, max(0, mChallenge->mChallengeStateCounter)));
	b.WriteI16((int16_t)max(-32768, min(32767, mChallenge->mChallengeScore)));

	// Fixed-size challenge runtime/presentation block.  The client mirrors these
	// values but never advances challenge gameplay from them.
	NetChallengeRuntimeState aChallengeRuntime;
	aChallengeRuntime.mProgressMeterWidth = (uint16_t)max(0,
		min(NET_CHALLENGE_PROGRESS_MAX, mProgressMeterWidth));
	aChallengeRuntime.mChallengePoints = max(0,
		min(NET_CHALLENGE_POINTS_MAX, mChallenge->mChallengePoints));
	aChallengeRuntime.mBombsLeft = (int16_t)max(NET_CHALLENGE_BOMBS_MIN,
		min(NET_CHALLENGE_BOMBS_MAX, mChallenge->mBombsLeft));
	aChallengeRuntime.mSlotMachineRollCount = (uint16_t)max(0,
		min(NET_CHALLENGE_SLOT_ROLL_MAX, mChallenge->mSlotMachineRollCount));
	aChallengeRuntime.mTreeOfWisdomTalkIndex = (uint16_t)max(0,
		min(NET_CHALLENGE_TREE_TALK_MAX, mChallenge->mTreeOfWisdomTalkIndex));
	const int aChallengeRecordIndex = mApp->GetCurrentChallengeIndex();
	const int aChallengeRecordCount = mApp->mPlayerInfo ?
		(int)(sizeof(mApp->mPlayerInfo->mChallengeRecords) /
			sizeof(mApp->mPlayerInfo->mChallengeRecords[0])) : 0;
	if (aChallengeRecordIndex >= 0 && aChallengeRecordIndex < aChallengeRecordCount)
		aChallengeRuntime.mCurrentChallengeRecord =
			max(0, min(NET_CHALLENGE_RECORD_MAX,
				mApp->mPlayerInfo->mChallengeRecords[aChallengeRecordIndex]));
	for (int aGridX = 0; aGridX < 9; ++aGridX)
		for (int aGridY = 0; aGridY < 6; ++aGridY)
			aChallengeRuntime.SetBeghouledEaten(aGridX, aGridY,
				mChallenge->mBeghouledEated[aGridX][aGridY]);
	const bool aChallengeRuntimeWritten =
		NetEncodeChallengeRuntimeState(aChallengeRuntime, b);
	TOD_ASSERT(aChallengeRuntimeWritten);
	if (!aChallengeRuntimeWritten)
	{
		// Capture above is bounded.  Preserve record alignment in a corrupted
		// release build instead of shifting the variable-length sections.
		aChallengeRuntime.Reset();
		NetEncodeChallengeRuntimeState(aChallengeRuntime, b);
	}

	// Native fog alphas are authoritative in ordinary fog levels and T/ST alike.
	NetEndlessWeatherState aWeather;
	const bool aEndlessT = IsEndlessT();
	if (aEndlessT)
	{
		aWeather.mDayPhase = mNetDayPhase;
		aWeather.mWeatherTimer = mNetWeatherTimer;
		aWeather.mFogTimer = mNetFogTimer;
		aWeather.mStormTimer = mNetStormTimer;
		aWeather.mNightBlend = mNetNightBlend;
		aWeather.mStormFlashKind = mNetStormFlashKind;
		aWeather.mStormFlashCounter = mNetStormFlashCounter;
	}
	for (size_t x = 0; x < NET_ENDLESS_FOG_COLUMNS; ++x)
		for (size_t y = 0; y < NET_ENDLESS_FOG_ROWS; ++y)
			aWeather.mFogGrid[x * NET_ENDLESS_FOG_ROWS + y] = (uint8_t)max(0, min(255, mGridCelFog[x][y]));
	const bool aWeatherWritten = NetEncodeEndlessWeather(aWeather, b, aEndlessT);
	TOD_ASSERT(aWeatherWritten);
	if (!aWeatherWritten)
	{
		NetTrace("snapshot weather host state invalid; encoding neutral state");
		aWeather = NetEndlessWeatherState();
		NetEncodeEndlessWeather(aWeather, b, aEndlessT);
	}

	const bool aZenMode = mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN;
	NetZenGardenState aZenState;
	if (aZenMode && mApp->mZenGarden && mApp->mPlayerInfo)
	{
		aZenState.mGarden = (uint8_t)mApp->mZenGarden->mGardenType;
		aZenState.mStinkyChocolate = mApp->mZenGarden->IsStinkyHighOnChocolate();
		const StoreItem supplies[] = { STORE_ITEM_FERTILIZER, STORE_ITEM_BUG_SPRAY, STORE_ITEM_CHOCOLATE };
		for (int i = 0; i < 3; ++i)
			aZenState.mSupplies[i] = (uint16_t)max(0, min(2000, mApp->mPlayerInfo->mPurchases[supplies[i]]));
		if (PottedPlant* potted = mApp->mZenGarden->GetPottedPlantInWheelbarrow())
		{
			aZenState.mWheelbarrow.mSeedType = (uint8_t)potted->mSeedType;
			aZenState.mWheelbarrow.mAge = (uint8_t)potted->mPlantAge;
			aZenState.mWheelbarrow.mFacing = (uint8_t)potted->mFacing;
			aZenState.mWheelbarrow.mVariation = (uint8_t)potted->mDrawVariation;
		}
	}
	const bool aZenWritten = NetEncodeZenGarden(aZenState, b, aZenMode,
		(uint8_t)SeedType::NUM_SEED_TYPES, (uint8_t)DrawVariation::NUM_VARIATIONS);
	TOD_ASSERT(aZenWritten);
	if (!aZenWritten)
	{
		aZenState = NetZenGardenState();
		if (aZenMode) aZenState.mGarden = 0;
		NetEncodeZenGarden(aZenState, b, aZenMode,
			(uint8_t)SeedType::NUM_SEED_TYPES, (uint8_t)DrawVariation::NUM_VARIATIONS);
	}

	// pvp / td
	b.WriteU8((uint8_t)min(255, mNetPvPBrainScore));
	b.WriteU8((uint8_t)min(255, mNetPvPKillScore));
	b.WriteU8(IsPvPMode() ? (uint8_t)mNetPvPBalance.mBeneficiary : 0);
	b.WriteU16(IsPvPMode() ? (uint16_t)max(0, min(1000, mNetPvPBalance.mStrength)) : 0);
	b.WriteI32(mNetPvPBrains);
	b.WriteU8((uint8_t)max(0, mNetTDLives));

	// ---- seed packets ----
	// Packet identity alone is not enough for conveyor levels: a client that
	// enters playing without receiving the last intro frame can otherwise keep
	// the entire bank offscreen.  Carry the stock bank presentation in every
	// gameplay snapshot so this also covers late join/recovery in every conveyor
	// mode, rather than special-casing Portal Combat.
	b.WriteI16((int16_t)max(-2048, min(2048, mSeedBank->mX)));
	b.WriteI16((int16_t)max(-1024, min(1024, mSeedBank->mY)));
	// Draw uses counter/4%6, so the complete visual phase is modulo 24.  A U16
	// wrap would introduce a two-frame phase jump because 65536 is not /24.
	b.WriteU8((uint8_t)(mSeedBank->mConveyorBeltCounter % 24));
	b.WriteU8((uint8_t)max(0, min(255, mSeedBank->mCutSceneDarken)));
	b.WriteU8((uint8_t)mSeedBank->mNumPackets);
	for (int i = 0; i < mSeedBank->mNumPackets; i++)
	{
		SeedPacket& aPacket = mSeedBank->mSeedPackets[i];
		const bool aPending = aPacket.IsMysteryPending();
		if (aPacket.mPacketType == SeedType::SEED_NONE && !aPending)
			aPacket.mNetId = 0;
		else if (aPacket.mNetId == 0)
			aPacket.mNetId = NetAllocId();
		b.WriteU16(aPacket.mNetId);
		b.WriteU8((uint8_t)aPacket.mPacketType);
		b.WriteU8(aPending ? 255 : (uint8_t)aPacket.mImitaterType);
		uint8_t aCooldown = 255;
		if (!aPending && aPacket.mRefreshTime > 0)
			aCooldown = (uint8_t)(aPacket.mRefreshCounter * 255 / aPacket.mRefreshTime);
		b.WriteU8(aCooldown);
		b.WriteU8(aPacket.mNetOwnerSlot);
		b.WriteU8(!aPending && aPacket.mActive ? 1 : 0);
		b.WriteI16((int16_t)max(0, min(1024, aPacket.mOffsetX)));
		b.WriteU16(aPending ? 0 : (uint16_t)max(0, min(SLOT_MACHINE_TIME, aPacket.mSlotMachineCountDown)));
		uint8_t aNextSlotSeed = (uint8_t)aPacket.mSlotMachiningNextSeed;
		b.WriteU8(!aPending && NetIsValidSlotMachineSeed(aNextSlotSeed) ? aNextSlotSeed : 255);
		float aSlotPosition = aPending ? 0.0f : aPacket.mSlotMachiningPosition;
		if (!_finite(aSlotPosition) || aSlotPosition < 0.0f || aSlotPosition > 1.0f)
			aSlotPosition = 0.0f;
		b.WriteF32(aSlotPosition);
		b.WriteU8(aPacket.mRandomDiscountLevel);
		const uint16_t aRefreshTime = aPending || aPacket.mPacketType == SEED_NONE ? 0 :
			(uint16_t)max(0, min(60000, aPacket.mRefreshTime));
		b.WriteU16(aRefreshTime);
		b.WriteU8(NetSeedRefreshWireFlag(aRefreshTime, aPacket.mRefreshing));
	}

	// ---- plants ----
	uint16_t aCount = 0;
	NetBuffer aBody;
	Plant* aPlant = nullptr;
	while (mPlants.IterateNext(aPlant))
	{
		if (aPlant->mDead || !aPlant->mIsOnBoard)
			continue;
		if (aPlant->mNetId == 0)
			aPlant->mNetId = NetAllocId();
		aBody.WriteU16(aPlant->mNetId);
		aBody.WriteU8((uint8_t)aPlant->mSeedType);
		aBody.WriteU8((uint8_t)aPlant->mImitaterType);
		aBody.WriteU8((uint8_t)aPlant->mRow);
		aBody.WriteU8((uint8_t)aPlant->mPlantCol);
		aBody.WriteU16((uint16_t)min(65535, max(0, aPlant->mPlantHealth)));
		aBody.WriteU16((uint16_t)min(65535, max(0, aPlant->mPlantMaxHealth)));
		aBody.WriteU8((uint8_t)aPlant->mState);
		uint8_t aPFlags = 0;
                if (aPlant->mIsAsleep) aPFlags |= 1;
                if (aPlant->mSquished) aPFlags |= 2;
                aBody.WriteU8(aPFlags);
                aBody.WriteI16((int16_t)max(-2048, min(2048, aPlant->mX)));
                aBody.WriteI16((int16_t)max(-2048, min(2048, aPlant->mY)));
                aBody.WriteI32(max(0, min(1000000, aPlant->mRenderOrder)));
                aBody.WriteU8((uint8_t)max(0, min(255, aPlant->mRecentlyEatenCountdown)));
                aBody.WriteU8((uint8_t)max(0, min(255, aPlant->mEatenFlashCountdown)));
                aBody.WriteU8((uint8_t)max(0, min(255, aPlant->mBeghouledFlashCountdown)));
                NetZenPlantState aZenPlant = mApp->mZenGarden ?
                    mApp->mZenGarden->GetNetPlantState(aPlant) : NetZenPlantState();
                const bool aZenPlantWritten = NetEncodeZenPlant(aZenPlant, aBody, aZenMode,
                    (uint8_t)SeedType::NUM_SEED_TYPES, (uint8_t)DrawVariation::NUM_VARIATIONS);
                TOD_ASSERT(aZenPlantWritten);
                if (!aZenPlantWritten)
                    NetEncodeZenPlant(NetZenPlantState(), aBody, aZenMode,
                        (uint8_t)SeedType::NUM_SEED_TYPES, (uint8_t)DrawVariation::NUM_VARIATIONS);
                for (int aRole = 0; aRole < NET_PLANT_REANIM_ROLES; aRole++)
                {
                        NetZombieAnimState anAnimState;
                        if (!NetCaptureReanimState(mApp, NetGetPlantReanimID(aPlant, aRole), anAnimState))
                        {
                                NetTrace("snapshot plant animation omitted: invalid host state id=%u type=%d role=%d",
                                        aPlant->mNetId, (int)aPlant->mSeedType, aRole);
                                anAnimState.Reset();
                        }
                        const bool anAnimWritten = NetEncodeZombieAnimState(anAnimState, aBody);
                        TOD_ASSERT(anAnimWritten);
                        if (!anAnimWritten)
                        {
                                anAnimState.Reset();
                                NetEncodeZombieAnimState(anAnimState, aBody);
                        }
                }
                aCount++;
	}
	TOD_ASSERT(aBody.Size() == (size_t)aCount * NET_PLANT_WIRE_BYTES);
	b.WriteU16(aCount);
	b.WriteBytes(aBody.Data(), aBody.Size());

	// ---- zombies ----
	aCount = 0;
	aBody.Clear();
	Zombie* aZombie = nullptr;
	while (mZombies.IterateNext(aZombie))
	{
		if (aZombie->mDead)
			continue;
		if (aZombie->mNetId == 0)
			aZombie->mNetId = NetAllocId();
		aBody.WriteU16(aZombie->mNetId);
		aBody.WriteU8((uint8_t)aZombie->mZombieType);
		aBody.WriteU8((uint8_t)aZombie->mRow);
		aBody.WriteI16((int16_t)max(-4, min(32767, aZombie->mFromWave)));
		aBody.WriteI32(aZombie->mRenderOrder);
		aBody.WriteI16((int16_t)(aZombie->mPosX * 4));
		aBody.WriteI16((int16_t)(aZombie->mPosY * 4));
		NetZombieHealthState aHealth;
		aHealth.mBody = (uint32_t)max(0, aZombie->mBodyHealth);
		aHealth.mBodyMax = (uint32_t)max(0, aZombie->mBodyMaxHealth);
		aHealth.mHelm = (uint32_t)max(0, aZombie->mHelmHealth);
		aHealth.mHelmMax = (uint32_t)max(0, aZombie->mHelmMaxHealth);
		aHealth.mShield = (uint32_t)max(0, aZombie->mShieldHealth);
		aHealth.mShieldMax = (uint32_t)max(0, aZombie->mShieldMaxHealth);
		aHealth.mFlying = (uint32_t)max(0, aZombie->mFlyingHealth);
		aHealth.mFlyingMax = (uint32_t)max(0, aZombie->mFlyingMaxHealth);
		const bool aHealthWritten = NetEncodeZombieHealthState(aHealth, aBody);
		TOD_ASSERT(aHealthWritten);
		aBody.WriteU8((uint8_t)aZombie->mZombiePhase);
		aBody.WriteU8((uint8_t)aZombie->mZombieHeight);
		aBody.WriteU8((uint8_t)aZombie->mHelmType);
		aBody.WriteU8((uint8_t)aZombie->mShieldType);
		uint8_t aZFlags = 0;
		if (aZombie->mIsEating) aZFlags |= 1;
		if (aZombie->mInPool) aZFlags |= 2;
		if (aZombie->mOnHighGround) aZFlags |= 4;
		if (aZombie->mHasHead) aZFlags |= 8;
		if (aZombie->mHasArm) aZFlags |= 16;
		if (aZombie->mHasObject) aZFlags |= 32;
		if (aZombie->mVariant) aZFlags |= 64;
		if (aZombie->mMindControlled) aZFlags |= 128;
		aBody.WriteU8(aZFlags);
		aBody.WriteU8((uint8_t)min(255, aZombie->mChilledCounter));
		aBody.WriteU8((uint8_t)min(255, aZombie->mButteredCounter));
		aBody.WriteU8((uint8_t)min(255, aZombie->mIceTrapCounter));
		// Zombie drawing uses -1 for fully visible and 0 for fully transparent.
		aBody.WriteU8(aZombie->mZombieFade < 0 ? 255 : (uint8_t)min(254, aZombie->mZombieFade));
		aBody.WriteU8((uint8_t)min(255, max(0, aZombie->mJustGotShotCounter)));
		aBody.WriteF32(aZombie->mVelX);
		aBody.WriteF32(aZombie->mAltitude);
		aBody.WriteF32(aZombie->mOriginalAnimRate > 0 ? aZombie->mOriginalAnimRate : 12.0f);
		NetZombieAnimState anAnimState;
		if (!NetCaptureZombieAnimState(mApp, aZombie, anAnimState))
		{
			NetTrace("snapshot zombie body animation omitted: invalid host state id=%u type=%d",
				aZombie->mNetId, (int)aZombie->mZombieType);
			anAnimState.Reset();
		}
		const bool anAnimWritten = NetEncodeZombieAnimState(anAnimState, aBody);
		TOD_ASSERT(anAnimWritten);
		if (!anAnimWritten)
		{
			// NetCaptureZombieAnimState has already bounded every live field.  Keep
			// the record aligned even in a corrupted release build.
			anAnimState.Reset();
			NetEncodeZombieAnimState(anAnimState, aBody);
		}
		if (!NetEncodeZombatar(aZombie->mNetZombatar, aBody))
			NetEncodeZombatar(NetZombatarAppearance(), aBody);
		aCount++;
	}
	TOD_ASSERT(aBody.Size() == (size_t)aCount * NET_ZOMBIE_WIRE_BYTES);
	b.WriteU16(aCount);
	b.WriteBytes(aBody.Data(), aBody.Size());

	// ---- projectiles ----
	aCount = 0;
	aBody.Clear();
	Projectile* aProjectile = nullptr;
        while (mProjectiles.IterateNext(aProjectile))
        {
                if (aProjectile->mDead)
                        continue;
                aBody.WriteU16(aProjectile->mNetId);
                aBody.WriteU8((uint8_t)aProjectile->mProjectileType);
                aBody.WriteU8((uint8_t)aProjectile->mRow);
                aBody.WriteF32(aProjectile->mPosX);
                aBody.WriteF32(aProjectile->mPosY);
                aBody.WriteF32(aProjectile->mPosZ);
                aBody.WriteF32(aProjectile->mShadowY);
                aBody.WriteI32(aProjectile->mRenderOrder);
                aBody.WriteF32(aProjectile->mRotation);
                aBody.WriteU8((uint8_t)aProjectile->mMotionType);
                aBody.WriteU8((uint8_t)max(0, min(255, aProjectile->mFrame)));
                aBody.WriteU8(aProjectile->mOnHighGround ? 1 : 0);
                aBody.WriteF32(aProjectile->mRotationSpeed);
                aCount++;
        }
	TOD_ASSERT(aBody.Size() == (size_t)aCount * NET_PROJECTILE_WIRE_BYTES);
	b.WriteU16(aCount);
	b.WriteBytes(aBody.Data(), aBody.Size());

	// ---- coins ----
	aCount = 0;
	aBody.Clear();
	Coin* aCoin = nullptr;
	while (mCoins.IterateNext(aCoin))
	{
		if (aCoin->mDead)
			continue;
		if (aCoin->mNetId == 0)
			aCoin->mNetId = NetAllocId();
                aBody.WriteU16(aCoin->mNetId);
                aBody.WriteU8((uint8_t)aCoin->mType);
                aBody.WriteU8((uint8_t)aCoin->mCoinMotion);
                aBody.WriteF32(aCoin->mPosX);
                aBody.WriteF32(aCoin->mPosY);
                aBody.WriteI32(aCoin->mRenderOrder);
                aBody.WriteU16((uint16_t)min(16384, max(0, (int)(aCoin->mScale * 4096.0f + 0.5f))));
                aBody.WriteU16((uint16_t)min(8192, max(0, (int)(aCoin->mCollectionDistance * 4.0f + 0.5f))));
                uint8_t aCFlags = 0;
                if (aCoin->mIsBeingCollected) aCFlags |= 1;
                if (aCoin->mHitGround) aCFlags |= 2;
                if (aCoin->mNeedsBouncyArrow) aCFlags |= 4;
                aBody.WriteU8(aCFlags);
                aBody.WriteU8((uint8_t)aCoin->mUsableSeedType);
                aBody.WriteU8((uint8_t)min(255, max(0, aCoin->mFadeCount)));
                aBody.WriteU16((uint16_t)min(65535, max(0, aCoin->mDisappearCounter)));
                const bool aPottedPresent = aCoin->mType == CoinType::COIN_PRESENT_PLANT ||
                        aCoin->mType == CoinType::COIN_AWARD_PRESENT;
                aBody.WriteU8(aPottedPresent ? (uint8_t)aCoin->mPottedPlantSpec.mSeedType : 255);
                aBody.WriteU8(aPottedPresent ? (uint8_t)aCoin->mPottedPlantSpec.mWhichZenGarden : 255);
                aBody.WriteU8(aPottedPresent ? (uint8_t)aCoin->mPottedPlantSpec.mFacing : 255);
                aBody.WriteU8(aPottedPresent ? (uint8_t)aCoin->mPottedPlantSpec.mDrawVariation : 255);
                aBody.WriteU8(aPottedPresent ? (uint8_t)aCoin->mPottedPlantSpec.mPlantAge : 255);
                aBody.WriteI32(aCoin->mCoinAge);aBody.WriteI32(aCoin->mDisappearCounter);
                aBody.WriteU8(LawnFusion::IsNativeDroppedCard269(aCoin)?uint8_t(aCoin->mTimesDropped):0);
                aCount++;
        }
	TOD_ASSERT(aBody.Size() == (size_t)aCount * NET_COIN_WIRE_BYTES);
	b.WriteU16(aCount);
	b.WriteBytes(aBody.Data(), aBody.Size());

	// ---- lawn mowers ----
	aCount = 0;
	aBody.Clear();
	LawnMower* aMower = nullptr;
	while (mLawnMowers.IterateNext(aMower))
	{
		if (aMower->mDead)
			continue;
		if (aMower->mNetId == 0)
			aMower->mNetId = NetAllocId();
		NetMowerSnapshotState aState;
		aState.mId = aMower->mNetId;
		aState.mRow = (uint8_t)aMower->mRow;
		aState.mState = (uint8_t)aMower->mMowerState;
		aState.mType = (uint8_t)aMower->mMowerType;
		aState.mVisible = aMower->mVisible;
		aState.mPosX = aMower->mPosX;
		aState.mPosY = aMower->mPosY;
		aState.mRenderOrder = aMower->mRenderOrder;
		aState.mAltitude = aMower->mAltitude;
		aState.mHeight = (uint8_t)aMower->mMowerHeight;
		if (!NetCaptureMowerAnimState(mApp, aMower, aState.mReanim))
		{
			NetTrace("snapshot mower animation omitted: invalid host state id=%u type=%d",
				aMower->mNetId, (int)aMower->mMowerType);
			aState.mReanim.Reset();
		}
		if (!NetEncodeMowerSnapshotState(aState, aBody))
		{
			NetTrace("snapshot mower omitted: invalid host state id=%u row=%d state=%d type=%d pos=(%.2f,%.2f) order=%d altitude=%.2f height=%d",
				aMower->mNetId, aMower->mRow, (int)aMower->mMowerState,
				(int)aMower->mMowerType, aMower->mPosX, aMower->mPosY,
				aMower->mRenderOrder, aMower->mAltitude,
				(int)aMower->mMowerHeight);
			continue;
		}
		aCount++;
	}
	TOD_ASSERT(aBody.Size() == (size_t)aCount * NET_MOWER_WIRE_BYTES);
	b.WriteU16(aCount);
	b.WriteBytes(aBody.Data(), aBody.Size());

	// ---- grid items ----
	aCount = 0;
	aBody.Clear();
	GridItem* aGridItem = nullptr;
	while (mGridItems.IterateNext(aGridItem))
	{
		if (aGridItem->mDead)
			continue;
		// Several stock modes (notably Vasebreaker) allocate grid items directly,
		// bypassing the helpers that normally assign network identities.
		if (aGridItem->mNetId == 0)
			aGridItem->mNetId = NetAllocId();
		aBody.WriteU16(aGridItem->mNetId);
		aBody.WriteU8((uint8_t)aGridItem->mGridItemType);
		aBody.WriteU8((uint8_t)aGridItem->mGridItemState);
		aBody.WriteU8((uint8_t)aGridItem->mGridX);
		aBody.WriteU8((uint8_t)aGridItem->mGridY);
		aBody.WriteI32(aGridItem->mGridItemCounter);
		aBody.WriteI32(aGridItem->mRenderOrder);
		aBody.WriteF32(aGridItem->mPosX);
		aBody.WriteF32(aGridItem->mPosY);
		aBody.WriteF32(aGridItem->mGoalX);
		aBody.WriteF32(aGridItem->mGoalY);
		aBody.WriteU8(aGridItem->mZombieType == ZombieType::ZOMBIE_INVALID ? 255 : (uint8_t)aGridItem->mZombieType);
		aBody.WriteU8(aGridItem->mSeedType == SeedType::SEED_NONE ? 255 : (uint8_t)aGridItem->mSeedType);
		aBody.WriteU8((uint8_t)aGridItem->mScaryPotType);
		aBody.WriteU16((uint16_t)min(65535, max(0, aGridItem->mTransparentCounter)));
		aBody.WriteU8((uint8_t)min(255, max(0, aGridItem->mSunCount)));
		aBody.WriteU8(aGridItem->mHighlighted ? 1 : 0);
		aCount++;
	}
	b.WriteU16(aCount);
	b.WriteBytes(aBody.Data(), aBody.Size());
	if (const auto* context = LawnFusion::GetNativeContext(this))
	{
		LawnFusion::NativeSnapshotFrame frame;
        const bool captured=NetCaptureNativeFusionFrame(this,frame);
        const bool metadata=captured && LawnFusion::ValidateNativeSnapshotFrame(frame,context->eligible);
        const bool body=captured && NetPreflightNativeFusionBody(this,b,frame);
        if (!captured || !metadata || !body || !LawnFusion::EncodeNativeSnapshot(frame,context->eligible,b))
        {
            NetTrace("native Fusion snapshot encode rejected epoch=%u capture=%d metadata=%d body=%d",context->epoch,captured?1:0,metadata?1:0,body?1:0);
            if (captured && !metadata)
            {
                auto probe=frame;
                probe.plants.clear();probe.projectiles.clear();probe.coldZombies.clear();probe.butterZombies.clear();
                probe.zombieActions.clear();probe.coins.clear();probe.garlicZombies.clear();probe.sourceZombies.clear();
                probe.equipmentCoins.clear();probe.equipmentPlants.clear();probe.zombieIdentities.clear();probe.gloveHolds={};
                probe.star269Plants.clear();probe.star269Projectiles.clear();probe.doom269Plants.clear();
                probe.companions269.clear();probe.companionPlants269.clear();
                for(const auto& record:frame.plants)
                {
                    probe.plants={record};probe.zombieIdentities.clear();
                    if(record.data.waterTargetId)
                        for(const auto& identity:frame.zombieIdentities)if(identity.id==record.data.waterTargetId)probe.zombieIdentities.push_back(identity);
                    if (!LawnFusion::ValidateNativeSnapshotFrame(probe,context->eligible))
                        NetTrace("Fusion invalid plant record id=%u kind=%u parents=%u,%u owner=%u idle=%.9g shoot=%.9g speed=%.9g shots=%u rage=%u bag=%u mode=%u damageStage=%u production=%d pending=%d",record.id,record.data.kind,record.data.firstParent,record.data.secondParent,record.data.owner,record.data.idleSeconds,record.data.shootSeconds,record.data.animationSpeed,record.data.batchShotCount,record.data.batchRageTicks,record.data.batchBagCount,record.data.batchMode,record.data.damageStage,record.data.productionTicks,record.data.pendingProductionTicks);
                }
            }
			b.Clear();
		}
	}
}

// ==========================================================================
// Client: message drain + ghost management
// ==========================================================================

Plant* Board::NetGetGhostPlant(unsigned short theId)
{
	if (theId == 0)
		return nullptr;
	Plant* aPlant = nullptr;
	while (mPlants.IterateNext(aPlant))
		if (aPlant->mNetGhost && aPlant->mNetId == theId && !aPlant->mDead)
			return aPlant;
	return nullptr;
}

Zombie* Board::NetGetGhostZombie(unsigned short theId)
{
	if (theId == 0)
		return nullptr;
	Zombie* aZombie = nullptr;
	while (mZombies.IterateNext(aZombie))
		if (aZombie->mNetGhost && aZombie->mNetId == theId && !aZombie->mDead)
			return aZombie;
	return nullptr;
}

Projectile* Board::NetGetGhostProjectile(unsigned short theId)
{
	if (theId == 0)
		return nullptr;
	Projectile* aProjectile = nullptr;
	while (mProjectiles.IterateNext(aProjectile))
		if (aProjectile->mNetGhost && aProjectile->mNetId == theId && !aProjectile->mDead)
			return aProjectile;
	return nullptr;
}

Coin* Board::NetGetGhostCoin(unsigned short theId)
{
	if (theId == 0)
		return nullptr;
	Coin* aCoin = nullptr;
	while (mCoins.IterateNext(aCoin))
		if (aCoin->mNetGhost && aCoin->mNetId == theId && !aCoin->mDead)
			return aCoin;
	return nullptr;
}

GridItem* Board::NetGetGhostGridItem(unsigned short theId)
{
	if (theId == 0)
		return nullptr;
	GridItem* aGridItem = nullptr;
	while (mGridItems.IterateNext(aGridItem))
		if (aGridItem->mNetGhost && aGridItem->mNetId == theId && !aGridItem->mDead)
			return aGridItem;
	return nullptr;
}

LawnMower* Board::NetGetGhostMower(unsigned short theId)
{
	if (theId == 0)
		return nullptr;
	LawnMower* aMower = nullptr;
	while (mLawnMowers.IterateNext(aMower))
		if (aMower->mNetGhost && aMower->mNetId == theId && !aMower->mDead)
			return aMower;
	return nullptr;
}

void Board::NetDrainMessages()
{
	NetManager* aNet = mApp->mNetManager;
	if (!aNet)
		return;

	uint32_t anApplyStart = GetTickCount();

	while (!aNet->mPendingBoardMsgs.empty())
	{
		NetManager::PendingMsg aMsg = std::move(aNet->mPendingBoardMsgs.front());
		aNet->mPendingBoardMsgs.pop_front();
		if (aMsg.mKind == 0)
			NetApplyEvent(aMsg.mData);
		else
			NetApplySnapshot(aMsg.mData);
	}

	mNetLastApplyMs = (int)(GetTickCount() - anApplyStart);
}

void Board::NetApplyEvent(NetBuffer& b)
{
	uint8_t aEvent = b.ReadU8Or();
	switch (aEvent)
	{
	case BOARDEV_FUSION_CHARRED315:
    {
        LawnFusion::SourceCharred311Birth birth;const auto* context=LawnFusion::GetNativeContext(this);
        if(!mApp||!mApp->NetIsClient()||mApp->mBoard!=this||!context||
           !LawnFusion::IsNativeSourceWorld313(this)||!NetDecodeSourceCharred315(b,birth)||
           context->epoch!=birth.epoch)break;
        LawnFusion::StartNativeSourceCharred311(this,birth);
        break;
    }
	case BOARDEV_FUSION_LIMB276:
	{
        NetFusionLimbEvent276 event;
        const auto* context=LawnFusion::GetNativeContext(this);
        if(!mApp||!mApp->NetIsClient()||mApp->mBoard!=this||!context||
            !LawnFusion::IsNativeSourceWorld313(this)||!NetDecodeFusionLimbEvent276(b,event)||
            context->epoch!=event.mEpoch)break;
        // The real loss can retire the owner before its next snapshot.
        // Event identity and the epoch, rather than a live ghost, own this FX.
        LawnFusion::PlayNativeSourceZombieLimbEvent276(this,event);
        break;
	}
	case BOARDEV_PRESENTATION_FWOOSH:
	{
		if (b.Remaining() != 1 + 12*(20 + NET_ZOMBIE_ANIM_WIRE_BYTES)) break;
		const auto row = b.ReadU8Or();
		if (row >= (StageHas6Rows() ? 6 : 5)) break;
		struct Flame {float x,y,sx,sy;int order;NetZombieAnimState state;};
		Flame flames[12]; bool valid = true;
		ReanimatorEnsureDefinitionLoaded(REANIM_JALAPENO_FIRE,true);
		if (!gReanimatorDefArray || REANIM_JALAPENO_FIRE >= gReanimatorDefCount) break;
		const auto& definition = gReanimatorDefArray[REANIM_JALAPENO_FIRE];
		if (!definition.mTracks.tracks || definition.mTracks.count <= 0) break;
		for (auto& f : flames)
		{
			f.x=b.ReadF32Or();f.y=b.ReadF32Or();f.sx=b.ReadF32Or();f.sy=b.ReadF32Or();f.order=b.ReadI32Or();
			valid = NetDecodeZombieAnimState(b,f.state) && f.state.mPresent && !f.state.mDead &&
				_finite(f.x) && _finite(f.y) && _finite(f.sx) && _finite(f.sy) &&
				fabsf(f.x)<=4096 && fabsf(f.y)<=4096 && fabsf(f.sx)<=4 && fabsf(f.sy)<=4 &&
				fabsf(f.sx)>=.1f && fabsf(f.sy)>=.1f && f.order>=0 && f.order<=1000000 &&
				NetZombieAnimStateFitsFrames(f.state,definition.mTracks.tracks[0].mTransforms.count) && valid;
		}
		if (!valid) break;
		for (int i=0;i<12;++i)
		{
			if (auto* old=mApp->ReanimationTryToGet(mFwooshID[row][i])) old->ReanimationDie();
			const auto& f=flames[i];
			auto* effect=mApp->AddReanimation(f.x,f.y,f.order,REANIM_JALAPENO_FIRE);
			mFwooshID[row][i]=effect ? mApp->ReanimationGetID(effect) : ReanimationID::REANIMATIONID_NULL;
			if (effect) {effect->OverrideScale(f.sx,f.sy);NetApplyReanimState(mApp,mFwooshID[row][i],f.state);}
		}
		mFwooshCounterID[row]=100;mFwooshCountDown=100;
		NetTrace("presentation fwoosh applied row=%u owned=12 nativeController=1",row);
		break;
	}
	case BOARDEV_PRESENTATION_PARTICLE:
	case BOARDEV_PRESENTATION_REANIM:
	{
		const bool aReanimation = aEvent == BOARDEV_PRESENTATION_REANIM;
		const size_t anExpectedBytes = 14 + (aReanimation ? NET_ZOMBIE_ANIM_WIRE_BYTES : 0);
		if (b.Remaining() != anExpectedBytes)
		{
			NetTrace("presentation effect rejected bytes=%zu expected=%zu", b.Remaining(), anExpectedBytes);
			break;
		}
		const uint16_t aType = b.ReadU16Or();
		const float aX = b.ReadF32Or();
		const float aY = b.ReadF32Or();
		const int aRenderOrder = b.ReadI32Or();
		uint16_t aSourceEffect=0;
		if (!_finite(aX) || !_finite(aY) || fabsf(aX) > 4096.0f || fabsf(aY) > 4096.0f ||
			aRenderOrder < 0 || aRenderOrder > 1000000 ||
			(aReanimation ? aType >= (int)ReanimationType::NUM_REANIMS :
			 (aType >= (int)ParticleEffect::NUM_PARTICLES &&
			  !(LawnFusion::IsNativeFusionBoard(this) && LawnFusion::NativeSourceEffectKind269(aType,aSourceEffect)))))
		{
			NetTrace("presentation effect rejected type=%u pos=(%.2f,%.2f) order=%d", aType, aX, aY, aRenderOrder);
			break;
		}
		if (!aReanimation)
		{
			mApp->AddTodParticle(aX, aY, aRenderOrder, (ParticleEffect)aType);
			if (LawnFusion::IsNativeFusionBoard(this) && aType == ParticleEffect::PARTICLE_POWIE)
			{
				mApp->PlayFoley(FoleyType::FOLEY_CHERRYBOMB);
				mApp->PlayFoley(FoleyType::FOLEY_JUICY);
				ShakeBoard(3, -4);
			}
		}
		else
		{
			NetZombieAnimState aState;
			if (!NetDecodeZombieAnimState(b, aState) || !aState.mPresent)
				break;
			ReanimatorEnsureDefinitionLoaded((ReanimationType)aType, true);
			if (!gReanimatorDefArray || aType >= gReanimatorDefCount)
				break;
			const ReanimatorDefinition& aDefinition = gReanimatorDefArray[aType];
			if (aDefinition.mTracks.count <= 0 || !aDefinition.mTracks.tracks ||
				!NetZombieAnimStateFitsFrames(aState,
					(uint32_t)aDefinition.mTracks.tracks[0].mTransforms.count))
				break;
			Reanimation* aReanim = mApp->AddReanimation(aX, aY, aRenderOrder, (ReanimationType)aType);
			if (aReanim)
				NetApplyReanimState(mApp, mApp->ReanimationGetID(aReanim), aState);
		}
		NetTrace("presentation effect applied kind=%s type=%u", aReanimation ? "reanim" : "particle", aType);
		break;
	}
	case BOARDEV_PLANT_ADDED:
	{
		uint16_t anId = b.ReadU16Or();
		uint8_t aType = b.ReadU8Or();
		uint8_t anImitater = b.ReadU8Or();
		uint8_t aRow = b.ReadU8Or();
		uint8_t aCol = b.ReadU8Or();
		SeedType anImitaterType = anImitater == 255 ? SeedType::SEED_NONE : (SeedType)anImitater;
		if (anId == 0 || aType >= (uint8_t)SeedType::NUM_SEED_TYPES ||
			(anImitater != 255 && anImitater >= (uint8_t)SeedType::NUM_SEED_TYPES) ||
			!IsValidGridCell(aCol, aRow))
		{
			NetTrace("PLANT_ADDED rejected id=%u type=%u imitater=%u cell=(%u,%u)", anId, aType, anImitater, aCol, aRow);
			break;
		}
		if (!NetGetGhostPlant(anId))
		{
			gNetSuppressSounds = true;
			Plant* aPlant = NewPlant(aCol, aRow, (SeedType)aType, anImitaterType);
			gNetSuppressSounds = false;
			if (aPlant)
			{
				aPlant->mNetId = anId;
				aPlant->mNetGhost = true;
				// NewPlant gives network interpolation targets their default (0,0).
				// Seed them from the authoritative spawn immediately so the first
				// client update cannot drag a rolling Bowling nut into the HUD.
				aPlant->mNetTargetX = (float)aPlant->mX;
				aPlant->mNetTargetY = (float)aPlant->mY;
			}
		}
		break;
	}
	case BOARDEV_PLANT_REMOVED:
	{
		uint16_t anId = b.ReadU16Or();
		Plant* aPlant = NetGetGhostPlant(anId);
		if (aPlant)
		{
			// A client ghost must not execute Plant::Die gameplay callbacks.
			aPlant->mDead = true;
			aPlant->RemoveEffects();
		}
		break;
	}
	case BOARDEV_SUN_ADDED:
	{
		if (!NetPayloadHasExactBytes(b, NET_SUN_ADDED_PAYLOAD_BYTES))
		{
			NetTrace("SUN_ADDED rejected: invalid payload size=%zu expected=%zu",
				b.Remaining(), NET_SUN_ADDED_PAYLOAD_BYTES);
			break;
		}
		uint16_t anId = b.ReadU16Or();
		uint8_t aCoinType = b.ReadU8Or();
		uint8_t aMotion = b.ReadU8Or();
		int aX = b.ReadI16Or() / 4;
		int aY = b.ReadI16Or() / 4;
		uint16_t aValue = b.ReadU16Or();
		uint8_t aUsableSeed = b.ReadU8Or();
		uint8_t aPottedSeed = b.ReadU8Or();
		uint8_t aPottedGarden = b.ReadU8Or();
		uint8_t aPottedFacing = b.ReadU8Or();
		uint8_t aPottedVariation = b.ReadU8Or();
		uint8_t aPottedAge = b.ReadU8Or();
		bool aUsablePacket = aCoinType == (uint8_t)CoinType::COIN_USABLE_SEED_PACKET;
		bool aPottedPresent = aCoinType == (uint8_t)CoinType::COIN_PRESENT_PLANT ||
			aCoinType == (uint8_t)CoinType::COIN_AWARD_PRESENT;
		bool aValidPottedVisual = aPottedPresent ?
			(aPottedSeed < (uint8_t)SeedType::NUM_SEED_TYPES &&
			 aPottedGarden <= (uint8_t)GardenType::GARDEN_AQUARIUM &&
			 aPottedFacing <= (uint8_t)PottedPlant::FacingDirection::FACING_LEFT &&
			 aPottedVariation < (uint8_t)DrawVariation::NUM_VARIATIONS &&
			 aPottedAge <= (uint8_t)PottedPlantAge::PLANTAGE_FULL) :
			(aPottedSeed == 255 && aPottedGarden == 255 && aPottedFacing == 255 &&
			 aPottedVariation == 255 && aPottedAge == 255);
		const bool aNativeMoneyType = aCoinType == (uint8_t)COIN_SILVER || aCoinType == (uint8_t)COIN_GOLD || aCoinType == (uint8_t)COIN_DIAMOND;
		const bool aNativeFusionValueValid = !LawnFusion::IsNativeFusionBoard(this) ||
			((aCoinType != (uint8_t)COIN_SMALLSUN || aValue == 5 || aValue == 15) && (aCoinType != (uint8_t)COIN_SUN || aValue == 25) &&
			 (aCoinType != (uint8_t)COIN_LARGESUN || aValue == 50) &&
			 (!aNativeMoneyType || LawnFusion::FusionMoneyCoinMetadataValid(static_cast<CoinType>(aCoinType),aValue)));
		if (anId == 0 || aCoinType < (uint8_t)CoinType::COIN_SILVER ||
			aCoinType > (uint8_t)CoinType::COIN_PRESENT_SURVIVAL_MODE ||
			aMotion > (uint8_t)CoinMotion::COIN_MOTION_FROM_BOSS ||
			(aUsablePacket && aUsableSeed >= (uint8_t)SeedType::NUM_SEED_TYPES) ||
			(!aUsablePacket && aUsableSeed != 255) || !aValidPottedVisual || !aNativeFusionValueValid)
		{
			NetTrace("SUN_ADDED rejected: id=%u coinType=%u seed=%u potted=(%u,%u,%u,%u,%u) value=%u",
				anId, aCoinType, aUsableSeed, aPottedSeed, aPottedGarden,
				aPottedFacing, aPottedVariation, aPottedAge, aValue);
			break;
		}
		PottedPlant aPottedVisual = {};
		const PottedPlant* aPottedVisualPtr = nullptr;
		if (aPottedPresent)
		{
			aPottedVisual.mSeedType = (SeedType)aPottedSeed;
			aPottedVisual.mWhichZenGarden = (GardenType)aPottedGarden;
			aPottedVisual.mFacing = (PottedPlant::FacingDirection)aPottedFacing;
			aPottedVisual.mDrawVariation = (DrawVariation)aPottedVariation;
			aPottedVisual.mPlantAge = (PottedPlantAge)aPottedAge;
			aPottedVisualPtr = &aPottedVisual;
		}
		Coin* aCoin = NetGetGhostCoin(anId);
		bool aCreatedFromEvent = false;
		if (aCoin && aCoin->mType != (CoinType)aCoinType)
		{
			NetTrace("SUN_ADDED rejected: id=%u changed coin type old=%d new=%u", anId, (int)aCoin->mType, aCoinType);
			break;
		}
		if (!aCoin)
		{
			aCoin = mCoins.DataArrayAlloc();
			if (aCoin)
			{
				aCoin->CoinInitializeNetGhost(aX, aY, (CoinType)aCoinType,
					(CoinMotion)aMotion, aPottedVisualPtr);
				aCoin->mUsableSeedType = aUsableSeed == 255 ? SeedType::SEED_NONE : (SeedType)aUsableSeed;
				aCreatedFromEvent = true;
			}
		}
		if (aCoin)
		{
			aCoin->mNetId = anId;
			aCoin->mNetGhost = true;
			if (LawnFusion::IsNativeFusionBoard(this) && LawnFusion::FusionSunCoinMetadataValid(aCoinType,aValue))
				LawnFusion::AttachNativeCoinValue(this, aCoin, aValue);
			else if (LawnFusion::IsNativeFusionBoard(this) && aNativeMoneyType)
				LawnFusion::AttachNativeCoinValue(this,aCoin,aValue,true);
			if (aCreatedFromEvent)
			{
				// AddCoin emits before some callers finish configuring velocity and
				// ground state.  Keep the provisional ghost hidden until the first
				// complete snapshot supplies an authoritative position instead of
				// displaying a motionless coin at the spawn coordinates.
				aCoin->mVisible = false;
				aCoin->mHitGround = false;
				aCoin->mPosX = (float)aX;
				aCoin->mPosY = (float)aY;
				aCoin->mNetTargetX = (float)aX;
				aCoin->mNetTargetY = (float)aY;
			}
			aCoin->mCoinMotion = (CoinMotion)aMotion;
			aCoin->mUsableSeedType = aUsableSeed == 255 ? SeedType::SEED_NONE : (SeedType)aUsableSeed;
			if (aPottedPresent)
			{
				aCoin->mPottedPlantSpec.mSeedType = aPottedVisual.mSeedType;
				aCoin->mPottedPlantSpec.mWhichZenGarden = aPottedVisual.mWhichZenGarden;
				aCoin->mPottedPlantSpec.mFacing = aPottedVisual.mFacing;
				aCoin->mPottedPlantSpec.mDrawVariation = aPottedVisual.mDrawVariation;
				aCoin->mPottedPlantSpec.mPlantAge = aPottedVisual.mPlantAge;
			}
		}
		break;
	}
	case BOARDEV_SUN_COLLECTED:
	{
		if (!NetPayloadHasExactBytes(b, NET_SUN_COLLECTED_PAYLOAD_BYTES))
		{
			NetTrace("SUN_COLLECTED rejected: invalid payload size=%zu expected=%zu",
				b.Remaining(), NET_SUN_COLLECTED_PAYLOAD_BYTES);
			break;
		}
		uint16_t anId = b.ReadU16Or();
		Coin* aCoin = NetGetGhostCoin(anId);
		if (aCoin && !aCoin->mIsBeingCollected)
		{
			aCoin->mCollectX = aCoin->mPosX;
			aCoin->mCollectY = aCoin->mPosY;
			aCoin->mIsBeingCollected = true;
			aCoin->mDead = false;
			aCoin->NetUpdatePresentation();
			if (aCoin->IsMoney())
				ShowCoinBank();
		}
		break;
	}
	case BOARDEV_ZOMBIE_ADDED:
	{
        // Native Fusion actor identity and controller state arrive together in
        // the complete FNS snapshot. The legacy event is published inside the
        // carrier factory, before that metadata exists; allocating here would
        // expose an untyped provisional actor and bypass snapshot admission.
        if(LawnFusion::IsNativeFusionBoard(this))break;
		uint16_t anId = b.ReadU16Or();
		uint8_t aType = b.ReadU8Or();
		uint8_t aRow = b.ReadU8Or();
		uint8_t aVariant = b.ReadU8Or();
		uint8_t aFromWave = b.ReadU8Or();
		float aSpeed = b.ReadF32Or();
		float aPosX = b.ReadF32Or();
		if (!NetGetGhostZombie(anId))
		{
			gNetSuppressSounds = true;
			Zombie* aZombie = AddZombieInRow((ZombieType)aType, aRow, aFromWave);
			gNetSuppressSounds = false;
			if (aZombie)
			{
				aZombie->mNetId = anId;
				aZombie->mNetGhost = true;
				// Spawn helpers publish this event before callers finish placing an
				// entity (vases, graves and scripted spawns all do this).  Its first
				// complete snapshot, rather than the provisional spawn, is visible.
				aZombie->mVisible = false;
				aZombie->mVariant = aVariant != 0;
				aZombie->mVelX = aSpeed;
				aZombie->mPosX = aPosX;
				aZombie->mX = (int)aPosX;
				// AddZombieInRow initializes the ghost at a local spawn and leaves
				// its network target at (0,0).  Keep the event-created presentation
				// at the host position until the first full snapshot arrives.
				aZombie->mNetTargetX = aZombie->mPosX;
				aZombie->mNetTargetY = aZombie->mPosY;
				aZombie->mNetAnimKey = 255; // force anim apply
			}
		}
		break;
	}
	case BOARDEV_ZOMBIE_REMOVED:
	{
		if (b.Remaining() != 2)
		{
			NetTrace("ZOMBIE_REMOVED rejected bytes=%zu", b.Remaining());
			break;
		}
		uint16_t anId = b.ReadU16Or();
		Zombie* aZombie = NetGetGhostZombie(anId);
		if (aZombie)
			NetRetireZombiePresentation(mApp, aZombie);
		break;
	}
	case BOARDEV_MOWER_USED:
	{
		uint8_t aRow = b.ReadU8Or();
		LawnMower* aMower = FindLawnMowerInRow(aRow);
		if (aMower && aMower->mMowerState == MOWER_READY && !aMower->mDead)
		{
			aMower->mMowerState = MOWER_TRIGGERED;
			aMower->mDead = false;
		}
		break;
	}
	case BOARDEV_SOUND:
	{
		uint16_t aSound=0;float aVolume=1.f,aPitch=1.f;
		if(!NetDecodeSoundEvent284(b,aSound,aVolume,aPitch))break;
        if(aSound&0x8000u)
        {
            if(aSound==uint16_t(0x8000u|92)&&mApp&&mApp->NetIsClient()&&LawnFusion::IsNativeFusionBoard(this)&&!gNetSuppressSounds)
            {
                const int local=ResolveSourceSound313(mApp,92);
                if(local>=0)PlayVolumeSound284(mApp,local,aVolume,false,aPitch);
            }
            break;
        }
		if(aVolume!=1.f)NetTrace("Fusion source sound284 received native=%u volume=%.9g source66=%d role=%s",aSound,aVolume,aSound==Sexy::SOUND_PRIZE?1:0,mApp&&mApp->NetIsClient()?"client":"host");
		if(!gNetSuppressSounds)PlayVolumeSound284(mApp,aSound,aVolume,false,aPitch);
		break;
	}
	case BOARDEV_FOLEY:
	{
		uint16_t aFoley = b.ReadU16Or();
		uint16_t aPitchRaw = b.ReadU16Or();
		if (!gNetSuppressSounds)
		{
			if (aPitchRaw == 0x4000)
				mApp->PlayFoley((FoleyType)aFoley);
			else
				mApp->PlayFoleyPitch((FoleyType)aFoley, aPitchRaw / 4096.0f);
		}
		break;
	}
	case BOARDEV_SHAKE:
	{
		uint16_t anAmount = b.ReadU16Or();
		ShakeBoard(anAmount, anAmount / 2);
		break;
	}
	case BOARDEV_POTATO_EXPLODED:
	{
		static const size_t POTATO_EVENT_BYTES = 7;
		if (b.Remaining() != POTATO_EVENT_BYTES)
		{
			NetTrace("POTATO_EXPLODED rejected: payload bytes=%zu expected=%zu", b.Remaining(), POTATO_EVENT_BYTES);
			break;
		}
		uint16_t anId = b.ReadU16Or();
		int anX = b.ReadI16Or();
		int anY = b.ReadI16Or();
		uint8_t aRow = b.ReadU8Or();
		if (anId == 0 || aRow >= MAX_GRID_SIZE_Y || anX < -512 || anX > 1536 || anY < -512 || anY > 1536)
		{
			NetTrace("POTATO_EXPLODED rejected: id=%u pos=(%d,%d) row=%u", anId, anX, anY, aRow);
			break;
		}

		Plant* aPlant = NetGetGhostPlant(anId);
		TodParticleSystem* aParticle = mApp->AddTodParticle(
			(float)anX, (float)anY,
			MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, aRow, 0),
			ParticleEffect::PARTICLE_POTATO_MINE);
		if (aPlant && aPlant->mSeedType == SeedType::SEED_POTATOMINE)
		{
			Reanimation* aBodyReanim = mApp->ReanimationTryToGet(aPlant->mBodyReanimID);
			if (aParticle && aBodyReanim && aBodyReanim->mFilterEffect != FilterEffect::FILTER_EFFECT_NONE)
				aParticle->OverrideFilterEffect(nullptr, aBodyReanim->mFilterEffect);
			if (LawnFusion::IsNativeFusionPotato(aPlant))
			{
				// Fusion keeps the real mine for its source .1s delayed death.
				// Only authoritative snapshot omission retires this client ghost.
				NetTrace("Fusion native Potato effect id=%u fxOnly=1 awaitHostRetirement=1", anId);
			}
			else
			{
				aPlant->mDead = true;
				aPlant->RemoveEffects();
			}
		}
		ShakeBoard(3, -4);
		break;
	}
	case BOARDEV_BUSH_RUSTLE:
	{
		static const size_t BUSH_RUSTLE_EVENT_BYTES = 1;
		if (b.Remaining() != BUSH_RUSTLE_EVENT_BYTES)
		{
			NetTrace("BUSH_RUSTLE rejected: payload bytes=%zu expected=%zu",
				b.Remaining(), BUSH_RUSTLE_EVENT_BYTES);
			break;
		}

		uint8_t aRow = b.ReadU8Or();
		const bool aValidContext = mApp && mApp->NetIsClient() &&
			mApp->mBoard == this && mApp->mGameScene == GameScenes::SCENE_PLAYING &&
			mApp->mNetManager && mApp->mNetManager->IsClient();
		if (!aValidContext || aRow >= MAX_GRID_SIZE_Y || b.Remaining() != 0)
		{
			NetTrace("BUSH_RUSTLE rejected: row=%u context=%d trailing=%zu",
				aRow, aValidContext ? 1 : 0, b.Remaining());
			break;
		}

		Reanimation* aBushReanim = mApp->ReanimationTryToGet(mBushesID[aRow]);
		if (!aBushReanim)
		{
			NetTrace("BUSH_RUSTLE ignored: row=%u reanimation unavailable", aRow);
			break;
		}

		// Presentation only: the host already decided when this row rustles.
		aBushReanim->StartBlend(20);
		aBushReanim->mLastFrameTime = 0.0f;
		aBushReanim->mAnimTime = 0.0f;
		aBushReanim->mLoopCount = 0;
		break;
	}
	case BOARDEV_READY_SET_PLANT:
	{
		const bool aValidContext = b.Remaining() == 0 && mApp &&
			mApp->NetIsClient() && mApp->mBoard == this &&
			mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO;
		if (!aValidContext)
		{
			NetTrace("READY_SET_PLANT rejected: bytes=%zu client=%d scene=%d boardMatch=%d",
				b.Remaining(), mApp && mApp->NetIsClient() ? 1 : 0,
				mApp ? (int)mApp->mGameScene : -1,
				mApp && mApp->mBoard == this ? 1 : 0);
			break;
		}
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_READYSETPLANT, true);
		Reanimation* aReady = mApp->AddReanimation(400.0f, 324.0f,
			MakeRenderOrder(RenderLayer::RENDER_LAYER_SCREEN_FADE, 0, 0),
			ReanimationType::REANIM_READYSETPLANT);
		if (!mApp->IsFinalBossLevel())
			mApp->mMusic->FadeOut(150);
		if (aReady)
			NetTrace("READY_SET_PLANT applied reanim=%u cutsceneTime=%d",
				(unsigned)mApp->ReanimationGetID(aReady), mCutScene->mCutsceneTime);
		break;
	}
	default:
		break;
	}
}

void Board::NetBroadcastPotatoExplosion(Plant* thePlant)
{
	NetManager* aNet = mApp ? mApp->mNetManager : nullptr;
	if (!thePlant || !aNet || !aNet->IsHosting())
		return;
	if (thePlant->mNetId == 0)
		thePlant->mNetId = NetAllocId();
	if (thePlant->mNetId == 0)
		return;

	NetBuffer anEvent;
	anEvent.WriteU16(thePlant->mNetId);
	anEvent.WriteI16((int16_t)max(-512, min(1536, thePlant->mX + thePlant->mWidth / 2)));
	anEvent.WriteI16((int16_t)max(-512, min(1536, thePlant->mY + thePlant->mHeight / 2)));
	anEvent.WriteU8((uint8_t)max(0, min(MAX_GRID_SIZE_Y - 1, thePlant->mRow)));
	aNet->SendBoardEvent(anEvent, BOARDEV_POTATO_EXPLODED);
}

void Board::NetApplyPlantPresentation(Plant* thePlant, PlantState thePreviousState, bool theCreatedOrAdopted)
{
	if (!thePlant)
		return;
	// Sleeping has its own resource as well as a body track.  Match the stock
	// offsets, but use a deterministic cosmetic clock instead of SetSleeping's
	// random gameplay-RNG calls; exact timing follows the host reanimation block.
	Reanimation* aSleeping = mApp->ReanimationTryToGet(thePlant->mSleepingReanimID);
	if (thePlant->mIsAsleep && !aSleeping)
	{
		float aX = thePlant->mX + 50.0f;
		float aY = thePlant->mY + 40.0f;
		if (thePlant->mSeedType == SeedType::SEED_FUMESHROOM) aX += 12.0f;
		else if (thePlant->mSeedType == SeedType::SEED_SCAREDYSHROOM) aY -= 20.0f;
		else if (thePlant->mSeedType == SeedType::SEED_GLOOMSHROOM) aY -= 12.0f;
		aSleeping = mApp->AddReanimation(aX, aY, thePlant->mRenderOrder + 2,
			ReanimationType::REANIM_SLEEPING);
		if (aSleeping)
		{
			aSleeping->mLoopType = ReanimLoopType::REANIM_LOOP;
			aSleeping->mAnimRate = 7.0f;
			thePlant->mSleepingReanimID = mApp->ReanimationGetID(aSleeping);
		}
	}
	else if (!thePlant->mIsAsleep && aSleeping)
	{
		mApp->RemoveReanimation(thePlant->mSleepingReanimID);
		thePlant->mSleepingReanimID = ReanimationID::REANIMATIONID_NULL;
	}
	if (thePlant->mSeedType != SeedType::SEED_POTATOMINE)
		return;

	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(thePlant->mBodyReanimID);
	if (!aBodyReanim)
		return;

	if (thePlant->mState == PlantState::STATE_POTATO_RISING &&
		(theCreatedOrAdopted || thePreviousState != PlantState::STATE_POTATO_RISING))
	{
		mApp->AddTodParticle(thePlant->mX + thePlant->mWidth / 2,
			thePlant->mY + thePlant->mHeight / 2, thePlant->mRenderOrder,
			ParticleEffect::PARTICLE_POTATO_MINE_RISE);
		aBodyReanim->StartBlend(20);
		aBodyReanim->mAnimRate = 18.0f;
		aBodyReanim->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD;
		aBodyReanim->mLoopCount = 0;
		aBodyReanim->SetFramesForLayer("anim_rise");
	}

	const bool aFusionDelayedPotato = thePlant->mState == PlantState::STATE_DOINGSPECIAL && LawnFusion::IsNativeFusionPotato(thePlant);
	if (thePlant->mState != PlantState::STATE_POTATO_ARMED && !aFusionDelayedPotato)
		return;

	if (theCreatedOrAdopted || thePreviousState != PlantState::STATE_POTATO_ARMED)
	{
		aBodyReanim->mAnimRate = 13.0f;
		aBodyReanim->mLoopType = ReanimLoopType::REANIM_LOOP;
		aBodyReanim->mLoopCount = 0;
		aBodyReanim->SetFramesForLayer("anim_armed");
	}

	// A late join can first observe an already armed mine.  Materialize its
	// attached glow without consuming gameplay RNG and never create duplicates.
	if (!mApp->ReanimationTryToGet(thePlant->mLightReanimID))
	{
		Reanimation* aLightReanim = mApp->AddReanimation(0.0f, 0.0f,
			thePlant->mRenderOrder + 2, GetPlantDefinition(thePlant->mSeedType).mReanimationType);
		if (aLightReanim)
		{
			aLightReanim->mLoopType = ReanimLoopType::REANIM_LOOP;
			aLightReanim->mAnimRate = 11.0f;
			aLightReanim->SetFramesForLayer("anim_glow");
			aLightReanim->mFrameCount = 10;
			aLightReanim->ShowOnlyTrack("anim_glow");
			aLightReanim->SetTruncateDisappearingFrames("anim_glow", false);
			thePlant->mLightReanimID = mApp->ReanimationGetID(aLightReanim);
			aLightReanim->AttachToAnotherReanimation(aBodyReanim, "anim_light");
			if (aBodyReanim->mFilterEffect != FilterEffect::FILTER_EFFECT_NONE)
				aLightReanim->mFilterEffect = FilterEffect::FILTER_EFFECT_WASHED_OUT;
		}
	}
}

void Board::NetApplySnapshot(NetBuffer& b, bool* theApplied)
{
	if (theApplied) *theApplied = false;
	const size_t aSmokeSnapshotBytes = b.Size();
	const uint32_t aSmokeSnapshotHash = NetFnv1a32(b.Data(), b.Size());
	LawnFusion::NativeSnapshotFrame aFusionFrame;
	LawnFusion::FusionSeedBank aFusionSelection;
    LawnFusion::NativeConveyor308CursorState aConveyorCursors;
    NetCaptured314Staging aCaptured314Staging;
	const bool aNativeFusion = LawnFusion::IsNativeFusionBoard(this);
	bool aZombieTerrainChanged = false;
	if (aNativeFusion)
	{
		const auto* context = LawnFusion::GetNativeContext(this);
		NetBuffer wire = b, nativeBody;
		aFusionSelection = context->selection;
		if (!LawnFusion::DecodeNativeSnapshot(wire, context->eligible, context->epoch, aFusionFrame, nativeBody) ||
			aFusionFrame.adventure.enabled!=context->adventure.enabled ||
			(aFusionFrame.adventure.enabled &&
			 (aFusionFrame.adventure.run.level!=context->adventure.run.level ||
			  aFusionFrame.adventure.run.cycle!=context->adventure.run.cycle ||
			  aFusionFrame.adventure.sourceDifficulty!=context->adventure.sourceDifficulty ||
              aFusionFrame.adventure.coopDifficulty!=context->adventure.coopDifficulty ||
			  aFusionFrame.adventure.wave<context->adventure.wave ||
			  aFusionFrame.adventure.run.tutorial.stage<context->adventure.run.tutorial.stage ||
			  (context->adventure.run.over&&!aFusionFrame.adventure.run.over) ||
			  (context->adventure.run.rewardCollected&&!aFusionFrame.adventure.run.rewardCollected) ||
			  (aFusionFrame.adventure.profile.completedLevels|context->adventure.profile.completedLevels)!=aFusionFrame.adventure.profile.completedLevels)) ||
			!aFusionSelection.ApplySnapshot(aFusionFrame.selection) ||
			!NetPreflightNativeFusionBody(this,nativeBody, aFusionFrame))
		{
			NetTrace("native Fusion snapshot preflight rejected epoch=%u bytes=%zu", context->epoch, b.Remaining());
			return;
		}
		std::vector<uint16_t> plantVisuals, projectileVisuals;
		for (const auto& record : aFusionFrame.plants) plantVisuals.push_back(record.data.kind);
		for (const auto& record : aFusionFrame.packets)
			if (record.data.kind != LawnFusion::kFusionNoPlant) plantVisuals.push_back(record.data.kind);
		for (const auto& record : aFusionFrame.projectiles) projectileVisuals.push_back(record.data.kind);
		std::string visualError;
		bool zombieVisualsReady = true;
		for (const auto& record : aFusionFrame.sourceZombies)
			if (!LawnFusion::EnsureNativeSourceZombieVisuals(this,record.data.kind,&visualError))
			{ zombieVisualsReady = false; break; }
        for(const auto& record:aFusionFrame.equipmentCoins)
            if(!LawnFusion::CanCreateNativeEquipmentVisual(this,record.data.factoryType))
            {zombieVisualsReady=false;visualError="Source equipment package unavailable";break;}
        for(const auto& record:aFusionFrame.fertilizeCoins269)if(!LawnFusion::EnsureNativeFertilize269Visuals(this)){zombieVisualsReady=false;visualError="Source Fertilize resource unavailable";break;}
        for(const auto& record:aFusionFrame.droppedCards269)
            if(!LawnFusion::EnsureNativePortraitVisuals(this,record.data.kind,&visualError)){zombieVisualsReady=false;break;}
        for(const auto& record:aFusionFrame.rollingPlants269)
            if(!LawnFusion::EnsureNativePlantVisuals(this,record.data.factoryKind,&visualError)){zombieVisualsReady=false;break;}
        for(const auto& record:aFusionFrame.companions269)
            if(!LawnFusion::EnsureNativeCompanion269Visual(this,record.data.family,record.data.smallVariant,&visualError))
            {zombieVisualsReady=false;break;}
        for(const auto& record:aFusionFrame.worldRemainder269Aux)
            if(!LawnFusion::EnsureNativeWorldRemainder269Assets(this,record.data.family,record.data.type))
            {zombieVisualsReady=false;visualError="Source world auxiliary resource unavailable";break;}
		if (!zombieVisualsReady || !LawnFusion::EnsureNativeVisuals(this,plantVisuals,projectileVisuals,&visualError))
		{
			NetTrace("native Fusion snapshot resources unavailable epoch=%u error=%s",context->epoch,visualError.c_str());
			if (mApp && mApp->mNetManager)
				mApp->mNetManager->ReportFusionResourceFailure("Fusion resources could not be loaded. Check your game installation.");
			return;
		}
        LawnFusion::FusionSourceDeathBudget319 additional313;
        if(!NetPreflight313Resources(this,nativeBody,aFusionFrame,additional313)||!NetDay273AllocationCapacity(this,nativeBody,aFusionFrame,additional313))return;
        if(!aFusionFrame.capturedPlants314.empty())
        {
            aCaptured314Staging.helper=LawnFusion::GetNativeCapturedPlants314(this);std::string error;
            if(!aCaptured314Staging.helper)return;
            for(const auto& record:aFusionFrame.capturedPlants314)
                if(!aCaptured314Staging.helper->StageExistingBeforeRaw(record.data,error)){NetTrace("FNS28 captured staging rejected: %s",error.c_str());return;}
        }
        if(LawnFusion::IsNativeSourceConveyor308(this))aConveyorCursors=LawnFusion::CaptureNativeConveyorCursors308(this);
		aZombieTerrainChanged = static_cast<uint8_t>(mBackground) != aFusionFrame.background;
		if (!LawnFusion::ConfigureNativeFusionBackground(this,aFusionFrame.background)) return;
		b = std::move(nativeBody);
	}
	// Presentation baselines advance only after the entire snapshot validates.
	// Core fields below are committed early, so using mChallenge here would let a
	// later malformed section consume an edge-triggered Slot Machine animation.
	const ChallengeState aPreviousChallengeState = mNetPresentedSlotMachineState;
	const int aPreviousSlotMachineRollCount = mNetPresentedSlotMachineRollCount;
	// Fixed core through the seed-packet count.  Validate it before touching
	// board/profile state so a truncated datagram cannot partially apply.
	static const size_t NET_SNAPSHOT_CORE_BYTES =
		88 + NET_CHALLENGE_RUNTIME_WIRE_BYTES + NET_ENDLESS_WEATHER_WIRE_BYTES + NET_ZEN_GARDEN_WIRE_BYTES - 7;
	if (b.Remaining() < NET_SNAPSHOT_CORE_BYTES)
	{
		NetTrace("snapshot core rejected: bytes=%zu expected-at-least=%zu", b.Remaining(), NET_SNAPSHOT_CORE_BYTES);
		return;
	}

	// ---- board core ----
	uint8_t aFlags = b.ReadU8Or();
	bool aPaused = (aFlags & SNAPFLAG_PAUSED) != 0;
	bool aLevelComplete = (aFlags & SNAPFLAG_LEVEL_COMPLETE) != 0;
	bool aZombiesWon = (aFlags & SNAPFLAG_ZOMBIES_WON) != 0;
	bool aShowHealthBar = (aFlags & SNAPFLAG_HEALTHBAR) != 0;
	bool aShowBowlingLine = (aFlags & SNAPFLAG_BOWLING_LINE) != 0;

	int aSunMoney = b.ReadI32Or();
	int aCoinTotal = b.ReadI32Or();
	if ((aFlags & ~(SNAPFLAG_PAUSED | SNAPFLAG_LEVEL_COMPLETE | SNAPFLAG_ZOMBIES_WON |
		SNAPFLAG_FOG_BLOWN | SNAPFLAG_HEALTHBAR | SNAPFLAG_BOWLING_LINE)) != 0 ||
		aCoinTotal < 0 || aCoinTotal > 99999)
	{
		NetTrace("snapshot core rejected: flags=%u coins=%d", aFlags, aCoinTotal);
		return;
	}
	mApp->mShowHealthBar = aShowHealthBar;
	mSunMoney = aSunMoney;
	if (mApp->mPlayerInfo)
		mApp->mPlayerInfo->mCoins = aCoinTotal;
	int aCurrentWave = b.ReadU8Or();
	int aNumWaves = b.ReadU8Or();
	mHugeWaveCountDown = b.ReadU16Or();
	mZombieCountDown = b.ReadU16Or();
	mBoardFadeOutCounter = (int)b.ReadU16Or() - 1;
	mNextSurvivalStageCounter = b.ReadU16Or();
	const int aFogOffset2 = b.ReadU16Or();
	const int aFogBlownCountDown = b.ReadU16Or();
	mLevel = b.ReadU8Or();
	mTutorialState = (TutorialState)b.ReadU8Or();
	const int aTutorialTimer = b.ReadI32Or();
	const uint8_t aShowShovel = b.ReadU8Or();
	const int aMusicTune = b.ReadI16Or();
	const uint8_t aMusicPaused = b.ReadU8Or();
	if (aTutorialTimer < -1 || aTutorialTimer > 1000000 || aShowShovel > 1 ||
		aMusicTune < MusicTune::MUSIC_TUNE_NONE || aMusicTune == 0 ||
		aMusicTune >= MusicTune::NUM_MUSIC_TUNES || aMusicPaused > 1)
	{
		NetTrace("snapshot presentation rejected timer=%d shovel=%u music=%d paused=%u",
			aTutorialTimer, aShowShovel, aMusicTune, aMusicPaused);
		return;
	}
	mIceTrapCounter = b.ReadU8Or();
	mShakeCounter = b.ReadU8Or();
	const int aGameplayBoardX = b.ReadI16Or();
	const int aGameplayBoardY = b.ReadI16Or();
	const int aGameplayTreeX = b.ReadI16Or();
	const int aGameplayPoleX = b.ReadI16Or();
	if (aGameplayBoardX < -2048 || aGameplayBoardX > 2048 ||
		aGameplayBoardY < -1024 || aGameplayBoardY > 1024 ||
		aGameplayTreeX < -4096 || aGameplayTreeX > 4096 ||
		aGameplayPoleX < -4096 || aGameplayPoleX > 4096)
	{
		NetTrace("snapshot camera rejected: board=(%d,%d) tree=%d pole=%d",
			aGameplayBoardX, aGameplayBoardY, aGameplayTreeX, aGameplayPoleX);
		return;
	}

	for (int i = 0; i < MAX_GRID_SIZE_Y; i++)
	{
		mIceMinX[i] = b.ReadI16Or();
		mIceTimer[i] = b.ReadU16Or();
	}

	const uint32_t aSurvivalStageWire = b.ReadU32Or();
	if (aSurvivalStageWire > NET_ENDLESS_STAGE_MAX)
	{
		NetTrace("snapshot survival stage rejected: stage=%u", aSurvivalStageWire);
		return;
	}
	const int aSurvivalStage = (int)aSurvivalStageWire;
	const ChallengeState aChallengeState = (ChallengeState)b.ReadU8Or();
	const int aChallengeStateCounter = b.ReadU16Or();
	const int aChallengeScore = b.ReadI16Or();
	NetChallengeRuntimeState aChallengeRuntime;
	if (!NetDecodeChallengeRuntimeState(b, aChallengeRuntime))
	{
		NetTrace("snapshot challenge runtime rejected: remaining=%zu expected=%zu",
			b.Remaining(), NET_CHALLENGE_RUNTIME_WIRE_BYTES);
		return;
	}

	// Apply the complete fixed block only after its logical bounds and unused
	// Beghouled bits have all passed validation.
	mChallenge->mSurvivalStage = aSurvivalStage;
	mChallenge->mChallengeState = aChallengeState;
	mChallenge->mChallengeStateCounter = aChallengeStateCounter;
	mChallenge->mChallengeScore = aChallengeScore;
	mProgressMeterWidth = aChallengeRuntime.mProgressMeterWidth;
	mChallenge->mChallengePoints = aChallengeRuntime.mChallengePoints;
	mChallenge->mBombsLeft = aChallengeRuntime.mBombsLeft;
	mChallenge->mSlotMachineRollCount = aChallengeRuntime.mSlotMachineRollCount;
	mChallenge->mTreeOfWisdomTalkIndex =
		aChallengeRuntime.mTreeOfWisdomTalkIndex;
	for (int aGridX = 0; aGridX < 9; ++aGridX)
		for (int aGridY = 0; aGridY < 6; ++aGridY)
			mChallenge->mBeghouledEated[aGridX][aGridY] =
				aChallengeRuntime.IsBeghouledEaten(aGridX, aGridY);

	const int aChallengeRecordIndex = mApp->GetCurrentChallengeIndex();
	const int aChallengeRecordCount = mApp->mPlayerInfo ?
		(int)(sizeof(mApp->mPlayerInfo->mChallengeRecords) /
			sizeof(mApp->mPlayerInfo->mChallengeRecords[0])) : 0;
	if (aChallengeRuntime.mCurrentChallengeRecord != NET_CHALLENGE_RECORD_NONE &&
		aChallengeRecordIndex >= 0 && aChallengeRecordIndex < aChallengeRecordCount)
	{
		const int aPreviousRecord =
			mApp->mPlayerInfo->mChallengeRecords[aChallengeRecordIndex];
		mApp->mPlayerInfo->mChallengeRecords[aChallengeRecordIndex] =
			aChallengeRuntime.mCurrentChallengeRecord;
		if (mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM &&
			aPreviousRecord != aChallengeRuntime.mCurrentChallengeRecord)
		{
			Reanimation* aTreeReanim =
				mApp->ReanimationTryToGet(mChallenge->mReanimChallenge);
			if (aTreeReanim)
			{
				const int aTreeSize = max(1,
					min(51, aChallengeRuntime.mCurrentChallengeRecord));
				aTreeReanim->PlayReanim(
					StrFormat("anim_grow%d", aTreeSize).c_str(),
					ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 8.0f);
			}
		}
	}

	NetEndlessWeatherState aWeather;
	if (!NetDecodeEndlessWeather(b, aWeather, IsEndlessT()))
	{
		NetTrace("snapshot weather rejected: remaining=%zu", b.Remaining());
		return;
	}
	NetZenGardenState aZenState;
	if (!NetDecodeZenGarden(b, aZenState,
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN,
		(uint8_t)SeedType::NUM_SEED_TYPES, (uint8_t)DrawVariation::NUM_VARIATIONS))
	{
		NetTrace("snapshot Zen garden rejected: remaining=%zu", b.Remaining());
		return;
	}

	if (b.Remaining() < 10) { NetTrace("snapshot PvP/balance fields truncated bytes=%zu", b.Remaining()); return; }
	mNetPvPBrainScore = b.ReadU8Or();
	mNetPvPKillScore = b.ReadU8Or();
	const uint8_t aBalanceSide = b.ReadU8Or();
	const uint16_t aBalanceStrength = b.ReadU16Or();
	if (!PvPBalanceWireValid(aBalanceSide, aBalanceStrength, IsPvPMode()))
	{ NetTrace("snapshot PvP balance rejected side=%u strength=%u", aBalanceSide, aBalanceStrength); return; }
	mNetPvPBalance.mBeneficiary = aBalanceSide;
	mNetPvPBalance.mStrength = aBalanceStrength;
	mNetPvPBrains = b.ReadI32Or();
	mNetTDLives = b.ReadU8Or();

	// huge wave warning banner (mirrors host-side advice)
	static int aLastWave = -1;
	if (aCurrentWave != aLastWave && aLastWave != -1 && mApp->mGameScene == GameScenes::SCENE_PLAYING)
	{
		if (IsFlagWave(aCurrentWave) && mHugeWaveCountDown > 0)
			DisplayAdvice(_S("[A_HUGE_WAVE_OF_ZOMBIES_IS_APPROACHING]"), MESSAGE_STYLE_HINT_TALL_FAST, ADVICE_NONE);
		else if (aCurrentWave == 0 || aCurrentWave > 0)
		{
			/* regular wave banner is host sound-driven */
		}
	}
	aLastWave = aCurrentWave;
	mCurrentWave = aCurrentWave;
	mNumWaves = aNumWaves;

	// pause overlay state (host controls pausing)
	mPaused = aPaused;
	mNetPausedOverlay = aPaused;
	mApp->mNetManager->mRemotePaused = aPaused;
	if (aLevelComplete)
		mLevelComplete = true;

	// ---- seed packets ----
	if (b.Remaining() < 7)
	{
		NetTrace("snapshot seed section truncated before presentation/count bytes=%zu", b.Remaining());
		return;
	}
	const int aSeedBankX = b.ReadI16Or();
	const int aSeedBankY = b.ReadI16Or();
	const int aConveyorCounter = b.ReadU8Or();
	const int aSeedBankDarken = b.ReadU8Or();
	uint8_t aNumPackets = b.ReadU8Or();
	if (!mSeedBank || aSeedBankX < -2048 || aSeedBankX > 2048 ||
		aSeedBankY < -1024 || aSeedBankY > 1024 ||
		aNumPackets > SEEDBANK_CAPACITY ||
		b.Remaining() < (size_t)aNumPackets * NET_SEED_PACKET_WIRE_BYTES)
	{
		NetTrace("snapshot seed section rejected bank=(%d,%d) counter=%d darken=%d count=%u remaining=%zu bank=%d",
			aSeedBankX, aSeedBankY, aConveyorCounter, aSeedBankDarken,
			aNumPackets, b.Remaining(), mSeedBank ? 1 : 0);
		return;
	}

	using NetSeedPacketWire = NetMysterySeedSnapshot;
	const NetMysteryLimits aMysteryLimits = {(uint8_t)NUM_SEEDS_IN_CHOOSER, (uint8_t)SEED_IMITATER, (uint8_t)SEED_GATLINGPEA};
	static_assert(SLOT_MACHINE_TIME == 400 && NET_MAX_PLAYERS == 4, "Mystery snapshot codec bounds must match gameplay");
	std::vector<NetSeedPacketWire> aSeedPackets;
	aSeedPackets.reserve(aNumPackets);
	for (int i = 0; i < aNumPackets; i++)
	{
		NetSeedPacketWire aWire = {};
		if (!NetDecodeMysterySeedSnapshot(b, aWire, aMysteryLimits, (uint8_t)NUM_SEED_TYPES,
			NetIsValidSeedPacketType, NetIsValidSlotMachineSeed))
		{
			NetTrace("snapshot seed rejected index=%d remaining=%zu", i, b.Remaining());
			return;
		}
		bool anIdValid = true;
		if (anIdValid && aWire.mId != 0)
		{
			for (const NetSeedPacketWire& anExisting : aSeedPackets)
			{
				if (anExisting.mId == aWire.mId)
				{
					anIdValid = false;
					break;
				}
			}
		}
		if (!anIdValid)
		{
			NetTrace("snapshot seed rejected index=%d id=%u type=%u imitater=%u owner=%u active=%u offset=%d slot=(%u,%u,%.3f)",
				i, aWire.mId, aWire.mType, aWire.mImitater, aWire.mOwner, aWire.mActive,
				aWire.mOffsetX, aWire.mSlotCountdown, aWire.mSlotNextSeed, aWire.mSlotPosition);
			return;
		}
		aSeedPackets.push_back(aWire);
	}

	// Chosen co-op banks are intentionally viewer-relative: each machine keeps
	// its own packets first.  Match the host records as a multiset of semantic
	// identities, consuming one wire occurrence per local occurrence so duplicate
	// cards and shared PvP zombie cards remain deterministic without reordering
	// the local bank.
	int aWireForLocal[SEEDBANK_CAPACITY];
	bool aWireUsed[SEEDBANK_CAPACITY] = {};
	for (int i = 0; i < SEEDBANK_CAPACITY; i++)
		aWireForLocal[i] = -1;

	const int aLocalPacketCount = mSeedBank->mNumPackets;
	bool anIdentityMultisetMatch = aLocalPacketCount >= 0 &&
		aLocalPacketCount <= SEEDBANK_CAPACITY && aLocalPacketCount == aNumPackets;
	int aMismatchLocalIndex = -1;
	for (int local = 0; local < aLocalPacketCount && local < SEEDBANK_CAPACITY; ++local)
		for (const NetSeedPacketWire& wire : aSeedPackets)
			if (!aNativeFusion && mSeedBank->mSeedPackets[local].IsMysteryPending() && mSeedBank->mSeedPackets[local].mNetId == wire.mId && wire.mType != 255)
			{ NetTrace("snapshot mystery reveal rejected outside START_PLAYING id=%u", wire.mId); return; }
	if (anIdentityMultisetMatch)
	{
		for (int aLocalIndex = 0; aLocalIndex < aLocalPacketCount; aLocalIndex++)
		{
			const SeedPacket& aLocalPacket = mSeedBank->mSeedPackets[aLocalIndex];
			const uint8_t aLocalType = (uint8_t)aLocalPacket.mPacketType;
			const uint8_t aLocalImitater = (uint8_t)aLocalPacket.mImitaterType;
			if (aLocalPacket.mNetId != 0)
			{
				for (int aWireIndex = 0; aWireIndex < aNumPackets; aWireIndex++)
				{
					if (!aWireUsed[aWireIndex] &&
						aSeedPackets[aWireIndex].mId == aLocalPacket.mNetId &&
						(aNativeFusion || (aSeedPackets[aWireIndex].mType == aLocalType &&
						aSeedPackets[aWireIndex].mImitater == aLocalImitater &&
						aSeedPackets[aWireIndex].mOriginLevel == aLocalPacket.mRandomDiscountLevel)))
					{
						aWireUsed[aWireIndex] = true;
						aWireForLocal[aLocalIndex] = aWireIndex;
						break;
					}
				}
			}
			if (aWireForLocal[aLocalIndex] >= 0)
				continue;
			// Fusion aliases can represent normal/extra cards with different
			// runtime state. Their validated stable IDs must never match by alias.
			if (aNativeFusion)
			{
				anIdentityMultisetMatch = false;
				aMismatchLocalIndex = aLocalIndex;
				break;
			}
			for (int aWireIndex = 0; aWireIndex < aNumPackets; aWireIndex++)
			{
				const NetSeedPacketWire& aWire = aSeedPackets[aWireIndex];
				if (!aWireUsed[aWireIndex] && aWire.mType == aLocalType &&
					aWire.mImitater == aLocalImitater &&
					aWire.mOriginLevel == aLocalPacket.mRandomDiscountLevel &&
					aWire.mOwner == aLocalPacket.mNetOwnerSlot)
				{
					aWireUsed[aWireIndex] = true;
					aWireForLocal[aLocalIndex] = aWireIndex;
					break;
				}
			}
			if (aWireForLocal[aLocalIndex] < 0)
			{
				anIdentityMultisetMatch = false;
				aMismatchLocalIndex = aLocalIndex;
				break;
			}
		}
	}

	bool aCanonicalOwnershipSafe = true;
	for (int i = 0; i < aNumPackets; i++)
	{
		if (aSeedPackets[i].mOwner != LawnNet::NET_SHARED_OWNER)
		{
			aCanonicalOwnershipSafe = false;
			break;
		}
	}
	if (aLocalPacketCount < 0 || aLocalPacketCount > SEEDBANK_CAPACITY)
		aCanonicalOwnershipSafe = false;
	for (int i = 0; aCanonicalOwnershipSafe && i < aLocalPacketCount; i++)
	{
		if (mSeedBank->mSeedPackets[i].mNetOwnerSlot != LawnNet::NET_SHARED_OWNER)
			aCanonicalOwnershipSafe = false;
	}
	// Beghouled legitimately creates its four upgrade buttons (and optional
	// crater button) only after the first successful match.  Clients do not run
	// that gameplay callback, so their zero-card bank must be rebuilt from the
	// authoritative shared identities just like a conveyor/slot-machine bank.
	const bool aDynamicBeghouledBank = mApp &&
		(mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED ||
		 mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED_TWIST);
	const bool aCanonicalFallbackMode = HasConveyorBeltSeedBank() ||
		(mApp && mApp->IsSlotMachineLevel()) || aDynamicBeghouledBank;
	// Conveyor array indices are physical slots: RemoveSeed compacts them and mX
	// is index-derived.  Apply the authoritative wire order even when the same
	// identity multiset could otherwise be greedily matched in a stale local order.
	// An initial intro bank (including a late spectator's mixed-owner bank)
	// may need adoption. A valid chosen bank already has viewer-relative order.
	const bool aCanonicalFallback =
		(aNativeFusion && !anIdentityMultisetMatch) ||
		(!anIdentityMultisetMatch && mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO) ||
		(aCanonicalOwnershipSafe &&
		 (HasConveyorBeltSeedBank() || (!anIdentityMultisetMatch && aCanonicalFallbackMode)));
	if (!anIdentityMultisetMatch && !aCanonicalFallback)
	{
		uint8_t aMismatchType = 255;
		uint8_t aMismatchImitater = 255;
		uint8_t aMismatchOwner = 255;
		if (aMismatchLocalIndex >= 0 && aMismatchLocalIndex < aLocalPacketCount)
		{
			const SeedPacket& aMismatch = mSeedBank->mSeedPackets[aMismatchLocalIndex];
			aMismatchType = (uint8_t)aMismatch.mPacketType;
			aMismatchImitater = (uint8_t)aMismatch.mImitaterType;
			aMismatchOwner = aMismatch.mNetOwnerSlot;
		}
		NetTrace("snapshot seed identity mismatch rejected localCount=%d wireCount=%u localIndex=%d identity=(%u,%u,%u) mode=%d fallbackMode=%d shared=%d",
			aLocalPacketCount, aNumPackets, aMismatchLocalIndex, aMismatchType,
			aMismatchImitater, aMismatchOwner, mApp ? (int)mApp->mGameMode : -1,
			aCanonicalFallbackMode ? 1 : 0, aCanonicalOwnershipSafe ? 1 : 0);
		return;
	}
	if (aCanonicalFallback && !anIdentityMultisetMatch)
	{
		NetTrace("snapshot seed canonical fallback mode=%d conveyor=%d slot=%d beghouled=%d localCount=%d wireCount=%u",
			mApp ? (int)mApp->mGameMode : -1, HasConveyorBeltSeedBank() ? 1 : 0,
			mApp && mApp->IsSlotMachineLevel() ? 1 : 0, aDynamicBeghouledBank ? 1 : 0,
			aLocalPacketCount, aNumPackets);
	}

	auto ApplySeedRuntimeState = [&](SeedPacket& thePacket,
		const NetSeedPacketWire& theWire, bool theApplyCanonicalIdentity)
	{
		const bool anIdentityChanged = thePacket.mNetId != theWire.mId;
		thePacket.mNetId = theWire.mId;
		if (theApplyCanonicalIdentity)
		{
			SeedType aType = theWire.mType == 255 ? SeedType::SEED_NONE : (SeedType)theWire.mType;
			SeedType anImitater = theWire.mImitater == 255 ? SeedType::SEED_NONE : (SeedType)theWire.mImitater;
			if (theWire.mType == 255 && theWire.mOriginLevel > 0)
			{
				thePacket.SetMysteryPlaceholder(theWire.mOriginLevel);
			}
			else if (thePacket.mPacketType != aType || thePacket.mImitaterType != anImitater ||
				(HasConveyorBeltSeedBank() && anIdentityChanged))
			{
				// SetPacketType queries plant definitions.  Empty and challenge-only
				// packet ids deliberately live outside that table and use a raw reset.
				if (HasConveyorBeltSeedBank() || aType == SeedType::SEED_NONE ||
					(int)aType >= (int)SeedType::NUM_SEED_TYPES)
				{
					thePacket.mPacketType = aType;
					thePacket.mImitaterType = anImitater;
					thePacket.mRefreshCounter = 0;
					thePacket.mRefreshTime = 0;
					thePacket.mRefreshing = false;
					thePacket.mActive = true;
				}
				else
				{
					thePacket.SetPacketType(aType, anImitater);
				}
			}
		}
		thePacket.mRefreshTime = theWire.mRefreshTime;
		thePacket.mRefreshCounter = theWire.mRefreshTime > 0 ? theWire.mCooldown * theWire.mRefreshTime / 255 : 0;
		thePacket.mRefreshing = theWire.mRefreshing != 0;
		thePacket.mNetOwnerSlot = theWire.mOwner;
		thePacket.mRandomDiscountLevel = theWire.mOriginLevel;
		thePacket.mActive = theWire.mActive != 0;
		thePacket.mOffsetX = theWire.mOffsetX;
		thePacket.mSlotMachineCountDown = theWire.mSlotCountdown;
		thePacket.mSlotMachiningNextSeed = theWire.mSlotNextSeed == 255 ?
			SeedType::SEED_NONE : (SeedType)theWire.mSlotNextSeed;
		thePacket.mSlotMachiningPosition = theWire.mSlotPosition;
	};

	// Apply only after structural validation and the complete identity mapping
	// have succeeded; a malformed or unsafe mismatched section cannot partially
	// update the local seed bank.
	for (int aLocalIndex = 0; aLocalIndex < aNumPackets; aLocalIndex++)
	{
		const int aWireIndex = aCanonicalFallback ? aLocalIndex : aWireForLocal[aLocalIndex];
		ApplySeedRuntimeState(mSeedBank->mSeedPackets[aLocalIndex],
			aSeedPackets[aWireIndex], true);
	}
	mSeedBank->mNumPackets = aNumPackets;
	mSeedBank->Move(aSeedBankX, aSeedBankY);
	mSeedBank->mConveyorBeltCounter = aConveyorCounter;
	mSeedBank->mCutSceneDarken = aSeedBankDarken;
	if (aNativeFusion&&!LawnFusion::IsNativeSourceConveyor308(this)) mSeedBank->SortPacketsForViewer(mApp->mNetManager->GetMySlot());
	if (HasConveyorBeltSeedBank() || aNativeFusion)
	{
		// A client starts with an empty/local bank in several challenge entry
		// paths.  Canonical packet adoption changes the count but historically
		// left every packet's base coordinates at zero, clipping or stacking the
		// cards.  Recompute only geometry; identities/runtime fields above remain
		// authoritative.
		mSeedBank->mWidth = Sexy::IMAGE_SEEDBANK->GetWidth() + GetSeedBankExtraWidth();
		mSeedBank->mHeight = Sexy::IMAGE_SEEDBANK->GetHeight();
		for (int i = 0; i < mSeedBank->mNumPackets; ++i)
		{
			if (aNativeFusion) mSeedBank->mSeedPackets[i].mIndex = i;
			mSeedBank->mSeedPackets[i].mX = GetSeedPacketPositionX(i);
			mSeedBank->mSeedPackets[i].mY = GetSeedPacketPositionY(i);
		}
	}

	// ---- plants ----
	if (b.Remaining() < 2)
	{
		NetTrace("snapshot plant section missing count bytes=%zu", b.Remaining());
		return;
	}
	uint16_t aCount = b.ReadU16Or();
	if (aCount > 1024 || (size_t)aCount > b.Remaining() / NET_PLANT_WIRE_BYTES)
	{
		NetTrace("snapshot plant section truncated count=%u remaining=%zu record=%zu",
			aCount, b.Remaining(), NET_PLANT_WIRE_BYTES);
		return;
	}
	struct NetPlantWire
	{
		uint16_t mId;
		uint8_t mType;
		uint8_t mImitater;
		uint8_t mRow;
		uint8_t mCol;
		uint16_t mHealth;
		uint16_t mMaxHealth;
		uint8_t mState;
		uint8_t mFlags;
                int16_t mPosX;
                int16_t mPosY;
                int32_t mRenderOrder;
                uint8_t mRecentlyEaten;
                uint8_t mEatenFlash;
                uint8_t mBeghouledFlash;
                NetZenPlantState mZen;
                NetZombieAnimState mReanim[NET_PLANT_REANIM_ROLES];
	};
	std::vector<NetPlantWire> aPlantWires;
	aPlantWires.reserve(aCount);
	std::map<unsigned short, bool> aSeenPlants;
	for (int i = 0; i < aCount; i++)
	{
		NetPlantWire aWire = {};
		aWire.mId = b.ReadU16Or();
		aWire.mType = b.ReadU8Or();
		aWire.mImitater = b.ReadU8Or();
		aWire.mRow = b.ReadU8Or();
		aWire.mCol = b.ReadU8Or();
		aWire.mHealth = b.ReadU16Or();
		aWire.mMaxHealth = b.ReadU16Or();
		aWire.mState = b.ReadU8Or();
		aWire.mFlags = b.ReadU8Or();
                aWire.mPosX = b.ReadI16Or();
                aWire.mPosY = b.ReadI16Or();
                aWire.mRenderOrder = b.ReadI32Or();
                aWire.mRecentlyEaten = b.ReadU8Or();
                aWire.mEatenFlash = b.ReadU8Or();
                aWire.mBeghouledFlash = b.ReadU8Or();
                if (!NetDecodeZenPlant(b, aWire.mZen,
                    mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN,
                    (uint8_t)SeedType::NUM_SEED_TYPES, (uint8_t)DrawVariation::NUM_VARIATIONS))
                {
                    NetTrace("snapshot plant section rejected invalid Zen need index=%d id=%u", i, aWire.mId);
                    return;
                }
                for (int aRole = 0; aRole < NET_PLANT_REANIM_ROLES; aRole++)
                {
                        if (!NetDecodeZombieAnimState(b, aWire.mReanim[aRole]))
                        {
                                NetTrace("snapshot plant section rejected malformed animation index=%d id=%u role=%d",
                                        i, aWire.mId, aRole);
                                return;
                        }
                }

		uint8_t aMaxPlantState = (uint8_t)PlantState::STATE_LILYPAD_INVULNERABLE;
#ifdef _DS_MINIGAMES
		aMaxPlantState = (uint8_t)PlantState::STATE_HEAT_WAVE_POWERED;
#endif
		const bool aValid = aWire.mId != 0 &&
			aWire.mType < (uint8_t)SeedType::NUM_SEED_TYPES &&
			(aWire.mImitater == 255 || aWire.mImitater < (uint8_t)SeedType::NUM_SEED_TYPES) &&
			IsValidGridCell(aWire.mCol, aWire.mRow) &&
            NetFusionPlantHealthValid(aNativeFusion?&aFusionFrame:nullptr,aWire.mId,aWire.mHealth,aWire.mMaxHealth) &&
                        aWire.mState <= aMaxPlantState && (aWire.mFlags & ~3) == 0 &&
			aWire.mPosX >= -2048 && aWire.mPosX <= 2048 &&
			aWire.mPosY >= -2048 && aWire.mPosY <= 2048 &&
			aWire.mRenderOrder >= 0 && aWire.mRenderOrder <= 1000000 &&
			aSeenPlants.find(aWire.mId) == aSeenPlants.end();
		if (!aValid)
		{
			NetTrace("snapshot plant section rejected index=%d id=%u type=%u imitater=%u cell=(%u,%u) health=%u/%u state=%u flags=%u pos=(%d,%d) order=%d",
				i, aWire.mId, aWire.mType, aWire.mImitater, aWire.mCol, aWire.mRow,
				aWire.mHealth, aWire.mMaxHealth, aWire.mState, aWire.mFlags,
				aWire.mPosX, aWire.mPosY, aWire.mRenderOrder);
			return;
		}
                aSeenPlants[aWire.mId] = true;
                aPlantWires.push_back(aWire);
        }

        // Existing ghosts provide exact local definitions for every live role.
        // Reject an out-of-range frame window before mutating any plant.  A new
        // ghost is still safe: the codec has bounded fields and application below
        // rechecks against the freshly initialized local definition.
        for (const NetPlantWire& aWire : aPlantWires)
        {
                Plant* anExisting = NetGetGhostPlant(aWire.mId);
                if (!anExisting || anExisting->mSeedType != (SeedType)aWire.mType)
                        continue;
                for (int aRole = 0; aRole < NET_PLANT_REANIM_ROLES; aRole++)
                {
                        if (!aWire.mReanim[aRole].mPresent)
                                continue;
                        Reanimation* aLocal = mApp->ReanimationTryToGet(
                                NetGetPlantReanimID(anExisting, aRole));
                        if (aLocal && !NetZombieAnimStateFitsFrames(aWire.mReanim[aRole],
                                NetGetReanimationFrameCapacity(aLocal)))
                        {
                                NetTrace("snapshot plant animation rejected id=%u type=%u role=%d frames=%u+%u capacity=%u",
                                        aWire.mId, aWire.mType, aRole,
                                        aWire.mReanim[aRole].mFrameStart,
                                        aWire.mReanim[aRole].mFrameCount,
                                        NetGetReanimationFrameCapacity(aLocal));
                                return;
                        }
                }
        }

	// The complete variable-length section is valid before any plant is created,
	// adopted, moved, updated, or deleted.
	std::map<unsigned short, NetZenPlantState> aZenPlantStates;
	for (const NetPlantWire& aWire : aPlantWires)
	{
		SeedType anImitaterType = aWire.mImitater == 255 ? SeedType::SEED_NONE : (SeedType)aWire.mImitater;
		Plant* aPlant = NetGetGhostPlant(aWire.mId);
        bool fusionPlantReused=false;
        if(aNativeFusion&&aPlant)
        {
            const auto incoming=std::find_if(aFusionFrame.plants.begin(),aFusionFrame.plants.end(),[&](const auto& r){return r.id==aWire.mId;});
            const auto* previous=LawnFusion::GetNativePlantData(aPlant);
            fusionPlantReused=previous&&incoming!=aFusionFrame.plants.end()&&previous->instanceToken!=incoming->data.instanceToken;
        }
		if (aPlant && (fusionPlantReused||aPlant->mSeedType != (SeedType)aWire.mType ||
			aPlant->mImitaterType != anImitaterType))
		{
			// A wrapped wire id must never turn an older ghost into a different
			// plant in place: its reanimations and type-specific state belong to
			// the previous entity.  Retire it without gameplay side effects and
			// let the normal creation path build a correctly initialized ghost.
			NetTrace("snapshot plant id reuse id=%u old=(%d,%d) new=(%u,%d)",
				aWire.mId, (int)aPlant->mSeedType, (int)aPlant->mImitaterType,
				aWire.mType, (int)anImitaterType);
			aPlant->mDead = true;
			aPlant->RemoveEffects();
			aPlant = nullptr;
		}
		bool aCreatedOrAdopted = false;
		if (!aPlant)
		{
			// Intro/challenge setup can create the same stock plant locally before
			// the first authoritative snapshot.  Adopt it instead of drawing two
			// plants in the same cell.
			Plant* aLocalPlant = nullptr;
			while (mPlants.IterateNext(aLocalPlant))
			{
				if (!aLocalPlant->mDead && !aLocalPlant->mNetGhost &&
					aLocalPlant->mRow == aWire.mRow && aLocalPlant->mPlantCol == aWire.mCol &&
					aLocalPlant->mSeedType == (SeedType)aWire.mType &&
					aLocalPlant->mImitaterType == anImitaterType)
				{
					aPlant = aLocalPlant;
					aCreatedOrAdopted = true;
					break;
				}
			}
		}
		if (!aPlant)
		{
			gNetSuppressSounds = true;
			aPlant = NewPlant(aWire.mCol, aWire.mRow, (SeedType)aWire.mType, anImitaterType);
			gNetSuppressSounds = false;
			if (!aPlant)
				continue;
			aCreatedOrAdopted = true;
		}
		aPlant->mNetId = aWire.mId;
		aPlant->mNetGhost = true;
		if (aWire.mZen.mSeedType != 255)
			aZenPlantStates[aWire.mId] = aWire.mZen;
		// Native Fusion cell relocation is an instantaneous accepted placement.
		// Commit the render origin with its logical cell, before presentation can
		// observe the new support/light membership at the previous position.
		const bool aNativeCellChanged = aNativeFusion &&
			(aPlant->mRow != aWire.mRow || aPlant->mPlantCol != aWire.mCol);
		aPlant->mRow = aWire.mRow;
		aPlant->mPlantCol = aWire.mCol;
		aPlant->mNetTargetX = (float)aWire.mPosX;
		aPlant->mNetTargetY = (float)aWire.mPosY;
		if (aCreatedOrAdopted || aNativeCellChanged)
		{
			aPlant->mX = aWire.mPosX;
			aPlant->mY = aWire.mPosY;
		}
		aPlant->mRenderOrder = aWire.mRenderOrder;
		PlantState aPreviousState = aPlant->mState;
		aPlant->mPlantHealth = aWire.mHealth;
		aPlant->mPlantMaxHealth = aWire.mMaxHealth;
                aPlant->mState = (PlantState)aWire.mState;
                aPlant->mIsAsleep = (aWire.mFlags & 1) != 0;
                aPlant->mSquished = (aWire.mFlags & 2) != 0;
                aPlant->mRecentlyEatenCountdown = aWire.mRecentlyEaten;
                aPlant->mEatenFlashCountdown = aWire.mEatenFlash;
                aPlant->mBeghouledFlashCountdown = aWire.mBeghouledFlash;
                if (aNativeFusion)
                {
                        // Source identity must exist before resource-only hooks,
                        // including a late first sample of an active ability.
                        const auto record = std::find_if(aFusionFrame.plants.begin(), aFusionFrame.plants.end(),
                                [&](const auto& item) { return item.id == aPlant->mNetId; });
                        if (record == aFusionFrame.plants.end() ||
                                LawnFusion::GetNativeSeedForPlant(record->data.kind) != aPlant->mSeedType ||
                                !LawnFusion::AttachNativePlant(this, aPlant, record->data))
                        {
                                NetTrace("native Fusion plant presentation rejected: metadata binding id=%u", aPlant->mNetId);
                                return;
                        }
                }
                NetApplyPlantPresentation(aPlant, aPreviousState, aCreatedOrAdopted);
                for (int aRole = 0; aRole < NET_PLANT_REANIM_ROLES; aRole++)
                        NetApplyReanimState(mApp, NetGetPlantReanimID(aPlant, aRole),
                                aWire.mReanim[aRole]);
	}
	{
		Plant* aPlant = nullptr;
		while (mPlants.IterateNext(aPlant))
			if (!aPlant->mDead &&
					((aPlant->mNetGhost && aSeenPlants.find(aPlant->mNetId) == aSeenPlants.end()) || !aPlant->mNetGhost))
			{
				// Snapshot omission is authoritative.  Retire presentation objects
				// without client-side challenge callbacks, loot, damage or RNG.
				aPlant->mDead = true;
				aPlant->RemoveEffects();
			}
	}

	// ---- zombies ----
	if (b.Remaining() < 2)
	{
		NetTrace("snapshot zombie section missing count bytes=%zu", b.Remaining());
		return;
	}
	aCount = b.ReadU16Or();
	if (aCount > 1024 || (size_t)aCount > b.Remaining() / NET_ZOMBIE_WIRE_BYTES)
	{
		NetTrace("snapshot zombie section truncated count=%u remaining=%zu record=%zu",
			aCount, b.Remaining(), NET_ZOMBIE_WIRE_BYTES);
		return;
	}
	struct NetZombieWire
	{
		uint16_t mId;
		uint8_t mType;
		uint8_t mRow;
		int16_t mPosX4;
		int16_t mPosY4;
		int16_t mFromWave;
		int32_t mRenderOrder;
		NetZombieHealthState mHealth;
		uint8_t mPhase;
		uint8_t mHeight;
		uint8_t mHelmType;
		uint8_t mShieldType;
		uint8_t mFlags;
		uint8_t mChilled;
		uint8_t mButtered;
		uint8_t mIceTrap;
		uint8_t mFade;
		uint8_t mJustShot;
		float mVelX;
		float mAltitude;
		float mAnimRate;
		NetZombieAnimState mBodyAnim;
		NetZombatarAppearance mZombatar;
	};
	std::vector<NetZombieWire> aZombieWires;
	aZombieWires.reserve(aCount);
	std::map<unsigned short, bool> aSeenZombies;
	for (int i = 0; i < aCount; i++)
	{
		NetZombieWire aWire = {};
		aWire.mId = b.ReadU16Or();
		aWire.mType = b.ReadU8Or();
		aWire.mRow = b.ReadU8Or();
		aWire.mFromWave = b.ReadI16Or();
		aWire.mRenderOrder = b.ReadI32Or();
		aWire.mPosX4 = b.ReadI16Or();
		aWire.mPosY4 = b.ReadI16Or();
		if (!NetDecodeZombieHealthState(b, aWire.mHealth))
		{
			NetTrace("snapshot zombie section rejected malformed health index=%d id=%u",
				i, aWire.mId);
			return;
		}
		aWire.mPhase = b.ReadU8Or();
		aWire.mHeight = b.ReadU8Or();
		aWire.mHelmType = b.ReadU8Or();
		aWire.mShieldType = b.ReadU8Or();
		aWire.mFlags = b.ReadU8Or();
		aWire.mChilled = b.ReadU8Or();
		aWire.mButtered = b.ReadU8Or();
		aWire.mIceTrap = b.ReadU8Or();
		aWire.mFade = b.ReadU8Or();
		aWire.mJustShot = b.ReadU8Or();
		aWire.mVelX = b.ReadF32Or();
		aWire.mAltitude = b.ReadF32Or();
		aWire.mAnimRate = b.ReadF32Or();
		if (!NetDecodeZombieAnimState(b, aWire.mBodyAnim))
		{
			NetTrace("snapshot zombie section rejected malformed body animation index=%d id=%u",
				i, aWire.mId);
			return;
		}
		if (!NetDecodeZombatar(b, aWire.mZombatar) ||
			(aWire.mZombatar.mPresent && aWire.mType != (uint8_t)ZombieType::ZOMBIE_NORMAL &&
			 aWire.mType != (uint8_t)ZombieType::ZOMBIE_FLAG))
		{
			NetTrace("snapshot zombie appearance rejected index=%d id=%u", i, aWire.mId);
			return;
		}

#ifdef _HAS_BLOOM_AND_DOOM_CONTENTS
		const uint8_t aMaxPhase = (uint8_t)ZombiePhase::PHASE_DOG_FREE;
#else
		const uint8_t aMaxPhase = (uint8_t)ZombiePhase::PHASE_ZAMBONI_FLAT;
#endif
		const bool aValid = aWire.mId != 0 &&
			aWire.mType < (uint8_t)ZombieType::NUM_ZOMBIE_TYPES &&
			aWire.mFromWave >= Zombie::ZOMBIE_WAVE_WINNER &&
			aWire.mRenderOrder >= 0 && aWire.mRenderOrder <= 1000000 &&
			aWire.mRow < MAX_GRID_SIZE_Y && aWire.mPhase <= aMaxPhase &&
			aWire.mHeight <= (uint8_t)ZombieHeight::HEIGHT_ZOMBIQUARIUM &&
			aWire.mHelmType <= (uint8_t)HelmType::HELMTYPE_BLACK_FOOTBALL &&
			aWire.mShieldType <= (uint8_t)ShieldType::SHIELDTYPE_TRASHCAN &&
			_finite(aWire.mVelX) &&
			_finite(aWire.mAltitude) && _finite(aWire.mAnimRate) &&
			aWire.mVelX >= -2048.0f && aWire.mVelX <= 2048.0f &&
			aWire.mAltitude >= -8192.0f && aWire.mAltitude <= 8192.0f &&
			aWire.mAnimRate > 0.0f && aWire.mAnimRate <= 1000.0f &&
			aSeenZombies.find(aWire.mId) == aSeenZombies.end();
		if (!aValid)
		{
			NetTrace("snapshot zombie section rejected index=%d id=%u type=%u row=%u phase=%u height=%u helm=%u shield=%u",
				i, aWire.mId, aWire.mType, aWire.mRow, aWire.mPhase,
				aWire.mHeight, aWire.mHelmType, aWire.mShieldType);
			return;
		}
		aSeenZombies[aWire.mId] = true;
		aZombieWires.push_back(aWire);
	}

	// Validate all type-specific animation ranges before creating or mutating a
	// single presentation entity.  Existing ghosts can supply their exact body
	// definition; new ghosts use the preloaded definition for the wire type.
	for (const NetZombieWire& aWire : aZombieWires)
	{
		if (!aWire.mBodyAnim.mPresent)
			continue;
		uint32_t aFrameCapacity = 0;
		Zombie* anExistingZombie = NetGetGhostZombie(aWire.mId);
		if (anExistingZombie && anExistingZombie->mZombieType == (ZombieType)aWire.mType)
		{
			Reanimation* anExistingBody = mApp->ReanimationTryToGet(anExistingZombie->mBodyReanimID);
			aFrameCapacity = NetGetReanimationFrameCapacity(anExistingBody);
		}
		if (aFrameCapacity == 0 && !NetGetZombieTypeFrameCapacity(aWire.mType, aFrameCapacity))
		{
			NetTrace("snapshot zombie body animation rejected: definition unavailable id=%u type=%u",
				aWire.mId, aWire.mType);
			return;
		}
		if (!NetZombieAnimStateFitsFrames(aWire.mBodyAnim, aFrameCapacity))
		{
			NetTrace("snapshot zombie body animation rejected: id=%u type=%u frames=%u+%u capacity=%u",
				aWire.mId, aWire.mType, aWire.mBodyAnim.mFrameStart,
				aWire.mBodyAnim.mFrameCount, aFrameCapacity);
			return;
		}
	}

	// Validate the complete variable-length section before creating, replacing,
	// moving or retiring any presentation entity.
	for (const NetZombieWire& aWire : aZombieWires)
	{
		Zombie* aZombie = NetGetGhostZombie(aWire.mId);
        bool fusionZombieReused=false;
        if(aNativeFusion&&aZombie)
        {
            const auto* context=LawnFusion::GetNativeContext(this);const auto generation=mZombies.DataArrayGetID(aZombie);
            const auto previous=context->zombieInstanceTokens.find(generation);
            const auto incoming=std::find_if(aFusionFrame.zombieIdentities.begin(),aFusionFrame.zombieIdentities.end(),[&](const auto& r){return r.id==aWire.mId;});
            fusionZombieReused=previous!=context->zombieInstanceTokens.end()&&incoming!=aFusionFrame.zombieIdentities.end()&&previous->second!=incoming->data;
        }
		if (aZombie && (fusionZombieReused||aZombie->mZombieType != (ZombieType)aWire.mType))
		{
			NetRetireZombiePresentation(mApp, aZombie);
			aZombie = nullptr;
		}
		if (aZombie && aWire.mBodyAnim.mPresent && !aWire.mBodyAnim.mDead &&
			!mApp->ReanimationTryToGet(aZombie->mBodyReanimID))
		{
			// EffectSystem frees terminal/absent reanimations after a frame.  If
			// the host later publishes a live body again, rebuild the complete
			// presentation object rather than leaving a permanently headless ghost.
			NetRetireZombiePresentation(mApp, aZombie);
			aZombie = nullptr;
		}
		bool aCreated = false;
		if (!aZombie)
		{
			gNetSuppressSounds = true;
			aZombie = AddZombieInRow((ZombieType)aWire.mType, aWire.mRow, 0);
			gNetSuppressSounds = false;
			if (!aZombie)
				continue;
			aCreated = true;
			aZombie->mNetId = aWire.mId;
			aZombie->mNetGhost = true;
			aZombie->mNetAnimKey = 255;
		}
		const int aPreviousRow = aZombie->mRow;
		const bool aWasInPool = aZombie->mInPool;
		const float aTargetX = aWire.mPosX4 / 4.0f;
		const float aTargetY = aWire.mPosY4 / 4.0f;
		const bool aFirstAuthoritativeSample = aCreated || !aZombie->mVisible;
        const auto* previousDay=aNativeFusion?LawnFusion::GetNativeDayZombie273(aZombie):nullptr;
        bool sourceMotionEdge=false;
        if(previousDay)
        {
            const auto incoming=std::find_if(aFusionFrame.dayZombies273.begin(),aFusionFrame.dayZombies273.end(),
                [&](const auto& record){return record.id==aWire.mId;});
            sourceMotionEdge=incoming!=aFusionFrame.dayZombies273.end()&&
                (incoming->data.status!=previousDay->status||incoming->data.shadowEnabled!=previousDay->shadowEnabled);
            if(sourceMotionEdge)
                NetTrace("Fusion source motion edge id=%u status=%u->%u shadow=%d->%d oldPos=(%.3f,%.3f) target=(%.3f,%.3f) coherentSnap=1",
                    aWire.mId,unsigned(previousDay->status),unsigned(incoming->data.status),previousDay->shadowEnabled?1:0,
                    incoming->data.shadowEnabled?1:0,aZombie->mPosX,aZombie->mPosY,aTargetX,aTargetY);
        }
		const bool aPortalJump = !aFirstAuthoritativeSample &&
			(fabsf(aTargetX - aZombie->mPosX) >= 120.0f ||
			 fabsf(aTargetY - aZombie->mPosY) >= 50.0f);
		if (aPreviousRow != aWire.mRow)
			aZombie->SetRow(aWire.mRow);
		aZombie->mNetTargetX = aWire.mPosX4 / 4.0f;
		aZombie->mNetTargetY = aWire.mPosY4 / 4.0f;
		// The authoritative pose/state below changes immediately. A motion
		// transition (including a source landing correction) must use its same
		// position sample rather than the previous phase's smoothed position.
		if (aFirstAuthoritativeSample || aPortalJump || sourceMotionEdge)
		{
			aZombie->mPosX = aZombie->mNetTargetX;
			aZombie->mPosY = aZombie->mNetTargetY;
			aZombie->mX = (int)aZombie->mPosX;
			aZombie->mY = (int)aZombie->mPosY;
			if (aPortalJump)
				NetTrace("portal zombie presentation snap id=%u row=%d->%u pos=(%.2f,%.2f)",
					aWire.mId, aPreviousRow, aWire.mRow,
					aZombie->mPosX, aZombie->mPosY);
		}
		aZombie->mVisible = true;
		aZombie->mFromWave = aWire.mFromWave;
		aZombie->mRenderOrder = aWire.mRenderOrder;
        const bool sourceArtwork=aNativeFusion&&std::any_of(aFusionFrame.sourceZombies.begin(),aFusionFrame.sourceZombies.end(),
            [&](const auto& record){return record.id==aWire.mId;});
		aZombie->ApplyNetZombatar(sourceArtwork?NetZombatarAppearance{}:aWire.mZombatar);
		const HelmType aPreviousHelmet = aZombie->mHelmType;
		const ShieldType aPreviousShield = aZombie->mShieldType;
		const bool aHadHelmet = aZombie->mHelmHealth > 0;
		const bool aHadShield = aZombie->mShieldHealth > 0;
		const int aPreviousHelmetDamage = aZombie->GetHelmDamageIndex();
		const int aPreviousShieldDamage = aZombie->GetShieldDamageIndex();
		aZombie->mBodyHealth = (int)aWire.mHealth.mBody;
		aZombie->mBodyMaxHealth = (int)aWire.mHealth.mBodyMax;
		aZombie->mHelmHealth = (int)aWire.mHealth.mHelm;
		aZombie->mHelmMaxHealth = (int)aWire.mHealth.mHelmMax;
		aZombie->mShieldHealth = (int)aWire.mHealth.mShield;
		aZombie->mShieldMaxHealth = (int)aWire.mHealth.mShieldMax;
		aZombie->mFlyingHealth = (int)aWire.mHealth.mFlying;
		aZombie->mFlyingMaxHealth = (int)aWire.mHealth.mFlyingMax;
		aZombie->mZombiePhase = (ZombiePhase)aWire.mPhase;
		aZombie->mZombieHeight = (ZombieHeight)aWire.mHeight;
		aZombie->mHelmType = (HelmType)aWire.mHelmType;
		aZombie->mShieldType = (ShieldType)aWire.mShieldType;
		aZombie->mIsEating = (aWire.mFlags & 1) != 0;
		aZombie->mInPool = (aWire.mFlags & 2) != 0;
		aZombie->mOnHighGround = (aWire.mFlags & 4) != 0;
		// Flags alone cannot hide the corresponding tracks or spawn the falling
		// limb.  Reuse the stock visual routines once at each authoritative edge;
		// an initial headless/one-armed sample hides tracks without replaying a
		// historical loss.  No damage, loot or challenge callbacks run here.
		const bool aHasHead = (aWire.mFlags & 8) != 0;
		const bool aHasArm = (aWire.mFlags & 16) != 0;
		if (aZombie->mHasArm && !aHasArm)
		{
			aZombie->SetupReanimForLostArm(aFirstAuthoritativeSample ?
				1U << (int)DamageFlags::DAMAGE_DOESNT_LEAVE_BODY : 0U);
		}
		if (aZombie->mHasHead && !aHasHead)
		{
			if (aFirstAuthoritativeSample)
				aZombie->SetupReanimForLostHead();
			else
				aZombie->DropHead(0U);
		}
		aZombie->mHasHead = aHasHead;
		aZombie->mHasArm = aHasArm;
		const bool aHelmetChanged = aCreated || aPreviousHelmet != aZombie->mHelmType ||
			aHadHelmet != (aZombie->mHelmHealth > 0) || aPreviousHelmetDamage != aZombie->GetHelmDamageIndex();
		const bool aShieldChanged = aCreated || aPreviousShield != aZombie->mShieldType ||
			aHadShield != (aZombie->mShieldHealth > 0) || aPreviousShieldDamage != aZombie->GetShieldDamageIndex();
		NetApplyZombieArmorPresentation(aZombie, aPreviousHelmet, aPreviousShield, aHelmetChanged, aShieldChanged);
		aZombie->mHasObject = (aWire.mFlags & 32) != 0;
		aZombie->mVariant = (aWire.mFlags & 64) != 0;
		aZombie->mMindControlled = (aWire.mFlags & 128) != 0;
		aZombie->mChilledCounter = aWire.mChilled;
		aZombie->mButteredCounter = aWire.mButtered;
		aZombie->mIceTrapCounter = aWire.mIceTrap;
		aZombie->mZombieFade = aWire.mFade == 255 ? -1 : aWire.mFade;
		aZombie->mJustGotShotCounter = aWire.mJustShot;
		aZombie->mVelX = aWire.mVelX;
		aZombie->mAltitude = aWire.mAltitude;
		aZombie->mOriginalAnimRate = aWire.mAnimRate;
		// Creation can precede the client's playing scene. Hydrate the stock
		// water tracks from authoritative terrain/state without replaying AI.
		if ((aFirstAuthoritativeSample || aZombieTerrainChanged || aPreviousRow != aZombie->mRow || aWasInPool != aZombie->mInPool) &&
			(mPlantRow[aZombie->mRow] == PLANTROW_POOL || aZombie->mInPool || aZombie->mZombieType == ZOMBIE_DUCKY_TUBE))
			aZombie->SetupPoolReanim();

		Reanimation* aBody = mApp->ReanimationTryToGet(aZombie->mBodyReanimID);
		if (aBody)
		{
			if (!aWire.mBodyAnim.mPresent)
			{
				// A locally initialized track must not keep playing when the host has
				// no body reanimation.  Do not destroy it: a later present snapshot
				// can safely revive the same resource without rebuilding attachments.
				aBody->mAnimRate = 0.0f;
				aBody->mDead = true;
			}
			else
			{
				aBody->mFrameStart = aWire.mBodyAnim.mFrameStart;
				aBody->mFrameCount = aWire.mBodyAnim.mFrameCount;
				aBody->mAnimTime = aWire.mBodyAnim.mAnimTime;
				aBody->mLastFrameTime = aWire.mBodyAnim.mAnimTime;
				aBody->mAnimRate = aWire.mBodyAnim.mAnimRate;
				aBody->mLoopType = (ReanimLoopType)aWire.mBodyAnim.mLoopType;
				aBody->mLoopCount = aWire.mBodyAnim.mLoopCount;
				aBody->mDead = aWire.mBodyAnim.mDead;
			}
		}
		else if (aWire.mBodyAnim.mPresent)
		{
			// Entity state remains usable even if its cosmetic resource could not
			// be allocated.  Crucially, there is no fallback track guess here.
			NetTrace("snapshot zombie body animation unavailable after create id=%u type=%u",
				aWire.mId, aWire.mType);
		}

	}
	{
		Zombie* aZombie = nullptr;
		while (mZombies.IterateNext(aZombie))
			if (aZombie->mNetGhost && !aZombie->mDead && aSeenZombies.find(aZombie->mNetId) == aSeenZombies.end())
				NetRetireZombiePresentation(mApp, aZombie);
	}

	// ---- projectiles ----
	if (b.Remaining() < 2)
	{
		NetTrace("snapshot projectile section missing count bytes=%zu", b.Remaining());
		return;
	}
	aCount = b.ReadU16Or();
	if (aCount > 1024 || (size_t)aCount > b.Remaining() / NET_PROJECTILE_WIRE_BYTES)
	{
		NetTrace("snapshot projectile section truncated count=%u remaining=%zu record=%zu",
			aCount, b.Remaining(), NET_PROJECTILE_WIRE_BYTES);
		return;
	}
	struct NetProjectileWire
	{
		uint16_t mId;
		uint8_t mType;
		uint8_t mRow;
		float mPosX;
		float mPosY;
		float mPosZ;
		float mShadowY;
		int32_t mRenderOrder;
		float mRotation;
        float mRotationSpeed;
		uint8_t mMotion;
		uint8_t mFrame;
		uint8_t mFlags;
	};
	std::vector<NetProjectileWire> aProjectileWires;
	aProjectileWires.reserve(aCount);
	std::map<unsigned short, bool> aSeenProjectiles;
	for (int i = 0; i < aCount; i++)
	{
		NetProjectileWire aWire = {};
		aWire.mId = b.ReadU16Or();
		aWire.mType = b.ReadU8Or();
		aWire.mRow = b.ReadU8Or();
		aWire.mPosX = b.ReadF32Or();
		aWire.mPosY = b.ReadF32Or();
		aWire.mPosZ = b.ReadF32Or();
		aWire.mShadowY = b.ReadF32Or();
		aWire.mRenderOrder = b.ReadI32Or();
		aWire.mRotation = b.ReadF32Or();
		aWire.mMotion = b.ReadU8Or();
		aWire.mFrame = b.ReadU8Or();
		aWire.mFlags = b.ReadU8Or();
        aWire.mRotationSpeed=b.ReadF32Or();

		const bool aFrameValid = NetProjectileFrameIsSafe(aWire.mType, aWire.mFrame);
		const bool aValid = aWire.mId != 0 &&
			aWire.mType < (uint8_t)ProjectileType::NUM_PROJECTILES &&
			aWire.mRow < MAX_GRID_SIZE_Y &&
			aWire.mMotion <= (uint8_t)ProjectileMotion::MOTION_HOMING &&
			(aWire.mFlags & ~1) == 0 && aFrameValid &&
			_finite(aWire.mPosX) && _finite(aWire.mPosY) &&
			_finite(aWire.mPosZ) && _finite(aWire.mShadowY) && _finite(aWire.mRotation) &&
            _finite(aWire.mRotationSpeed)&&aWire.mRotationSpeed>= -100000.f&&aWire.mRotationSpeed<=100000.f&&
			aWire.mPosX >= -1024.0f && aWire.mPosX <= 2048.0f &&
			aWire.mPosY >= -1024.0f && aWire.mPosY <= 2048.0f &&
			aWire.mPosZ >= -4096.0f && aWire.mPosZ <= 4096.0f &&
			aWire.mShadowY >= -1024.0f && aWire.mShadowY <= 2048.0f &&
			aWire.mRotation >= -100000.0f && aWire.mRotation <= 100000.0f &&
			aWire.mRenderOrder >= -1000000 && aWire.mRenderOrder <= 2000000 &&
			aSeenProjectiles.find(aWire.mId) == aSeenProjectiles.end();
		if (!aValid)
		{
			NetTrace("snapshot projectile section rejected index=%d id=%u type=%u row=%u motion=%u frame=%u flags=%u",
				i, aWire.mId, aWire.mType, aWire.mRow, aWire.mMotion, aWire.mFrame, aWire.mFlags);
			return;
		}
		aSeenProjectiles[aWire.mId] = true;
		aProjectileWires.push_back(aWire);
	}

	// The entire variable-length section is valid before any projectile is
	// created, replaced, moved, or deleted.
	for (const NetProjectileWire& aWire : aProjectileWires)
	{
		Projectile* aProjectile = NetGetGhostProjectile(aWire.mId);
        const auto worldAux=aNativeFusion?std::find_if(aFusionFrame.worldRemainder269Aux.begin(),aFusionFrame.worldRemainder269Aux.end(),[&](const auto& r){return r.id==aWire.mId;}):aFusionFrame.worldRemainder269Aux.end();
        const bool typedWorldAux=worldAux!=aFusionFrame.worldRemainder269Aux.end();
        const auto* previousWorldAux=aProjectile?LawnFusion::GetNativeWorldRemainder269AuxState(aProjectile):nullptr;
        const auto obstacle=aNativeFusion?std::find_if(aFusionFrame.sourceObstacles313.begin(),aFusionFrame.sourceObstacles313.end(),[&](const auto& r){return r.id==aWire.mId;}):aFusionFrame.sourceObstacles313.end();
        const auto* oldObstacle=aProjectile?LawnFusion::GetNativeSourceObstacle313(aProjectile):nullptr;
        const bool obstacleChanged=aProjectile&&(bool(oldObstacle)!=(obstacle!=aFusionFrame.sourceObstacles313.end())||
            (oldObstacle&&obstacle!=aFusionFrame.sourceObstacles313.end()&&oldObstacle->instanceToken!=obstacle->data.instanceToken));
        const bool worldAuxChanged=aProjectile&&(bool(previousWorldAux)!=typedWorldAux||
            (previousWorldAux&&typedWorldAux&&previousWorldAux->instanceToken!=worldAux->data.instanceToken));
		if (aProjectile && (worldAuxChanged||obstacleChanged||aProjectile->mProjectileType != (ProjectileType)aWire.mType))
		{
			// Torchwood conversion changes PEA into FIREBALL without changing the
			// network identity.  Recreate the presentation object so FIREBALL gets
			// its required REANIM_FIRE_PEA attachment and the old trail dies once.
			aProjectile->Die();
			aProjectile = nullptr;
		}
		bool aCreated = false;
		if (!aProjectile)
		{
			aProjectile = mProjectiles.DataArrayAlloc();
			if (aProjectile)
				aProjectile->ProjectileInitializeNetGhost((int)aWire.mPosX, (int)aWire.mPosY,
					aWire.mRenderOrder, aWire.mRow, (ProjectileType)aWire.mType);
			if (!aProjectile)
				continue;
			aCreated = true;
			aProjectile->mNetId = aWire.mId;
			aProjectile->mNetGhost = true;
		}
		const int aPreviousRow = aProjectile->mRow;
		const bool aPortalJump = !aCreated &&
			mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_PORTAL_COMBAT &&
			aPreviousRow != aWire.mRow &&
			(fabsf(aWire.mPosX - aProjectile->mPosX) >= 120.0f ||
			 fabsf(aWire.mPosY - aProjectile->mPosY) >= 50.0f);
		aProjectile->mRow = aWire.mRow;
		aProjectile->mNetTargetX = aWire.mPosX;
		aProjectile->mNetTargetY = aWire.mPosY;
		aProjectile->mNetTargetZ = aWire.mPosZ;
		aProjectile->mNetTargetRotation = aWire.mRotation;
        const auto incoming=aNativeFusion?std::find_if(aFusionFrame.projectiles.begin(),aFusionFrame.projectiles.end(),[&](const auto& r){return r.id==aWire.mId;}):aFusionFrame.projectiles.end();
        const bool cannon=incoming!=aFusionFrame.projectiles.end()&&LawnFusion::IsFusionCannon268Way14(incoming->data.kind)&&incoming->data.specialType==14;
        const bool cannonPhaseChanged=cannon&&aProjectile->mRotation!=aWire.mRotation;
        if(aNativeFusion)aProjectile->mRotationSpeed=aWire.mRotationSpeed;
        if(cannon)aProjectile->mRotation=aWire.mRotation;
		if (aCreated || aPortalJump||cannonPhaseChanged||typedWorldAux||obstacle!=aFusionFrame.sourceObstacles313.end())
		{
			aProjectile->mPosX = aWire.mPosX;
			aProjectile->mPosY = aWire.mPosY;
			aProjectile->mPosZ = aWire.mPosZ;
			aProjectile->mRotation = aWire.mRotation;
			aProjectile->mX = (int)aWire.mPosX;
			aProjectile->mY = (int)(aWire.mPosY + aWire.mPosZ);
			if (aPortalJump)
				NetTrace("portal projectile presentation snap id=%u row=%d->%u pos=(%.2f,%.2f,%.2f)",
					aWire.mId, aPreviousRow, aWire.mRow,
					aWire.mPosX, aWire.mPosY, aWire.mPosZ);
		}
		aProjectile->mShadowY = aWire.mShadowY;
		aProjectile->mRenderOrder = aWire.mRenderOrder;
		aProjectile->mMotionType = (ProjectileMotion)aWire.mMotion;
		aProjectile->mFrame = aWire.mFrame;
		aProjectile->mOnHighGround = (aWire.mFlags & 1) != 0;
		aProjectile->SyncFireballPresentationDirection();
	}
	{
		Projectile* aProjectile = nullptr;
		while (mProjectiles.IterateNext(aProjectile))
			if (aProjectile->mNetGhost && !aProjectile->mDead && aSeenProjectiles.find(aProjectile->mNetId) == aSeenProjectiles.end())
				aProjectile->Die();
	}

	// ---- coins ----
	if (b.Remaining() < 2)
	{
		NetTrace("snapshot coin section missing count bytes=%zu", b.Remaining());
		return;
	}
	aCount = b.ReadU16Or();
	if (aCount > 1024 || (size_t)aCount > b.Remaining() / NET_COIN_WIRE_BYTES)
	{
		NetTrace("snapshot coin section truncated count=%u remaining=%zu record=%zu",
			aCount, b.Remaining(), NET_COIN_WIRE_BYTES);
		return;
	}
	struct NetCoinWire
	{
		uint16_t mId;
		uint8_t mType;
		uint8_t mMotion;
		float mPosX;
		float mPosY;
		int32_t mRenderOrder;
		uint16_t mScaleQ;
		uint16_t mCollectionDistanceQ;
		uint8_t mFlags;
		uint8_t mUsableSeed;
		uint8_t mFadeCount;
		uint16_t mDisappearCounter;
        int mCoinAge,mFullDisappearCounter,mCardUses;
		uint8_t mPottedSeed;
		uint8_t mPottedGarden;
		uint8_t mPottedFacing;
		uint8_t mPottedVariation;
		uint8_t mPottedAge;
	};
	std::vector<NetCoinWire> aCoinWires;
	aCoinWires.reserve(aCount);
	std::map<unsigned short, bool> aSeenCoins;
	for (int i = 0; i < aCount; i++)
	{
		const size_t coinRecordStart=b.mReadPos;
        NetCoinWire aWire = {};
		aWire.mId = b.ReadU16Or();
		aWire.mType = b.ReadU8Or();
		aWire.mMotion = b.ReadU8Or();
		aWire.mPosX = b.ReadF32Or();
		aWire.mPosY = b.ReadF32Or();
		aWire.mRenderOrder = b.ReadI32Or();
		aWire.mScaleQ = b.ReadU16Or();
		aWire.mCollectionDistanceQ = b.ReadU16Or();
		aWire.mFlags = b.ReadU8Or();
		aWire.mUsableSeed = b.ReadU8Or();
		aWire.mFadeCount = b.ReadU8Or();
		aWire.mDisappearCounter = b.ReadU16Or();
		const bool aUsablePacket = aWire.mType == (uint8_t)CoinType::COIN_USABLE_SEED_PACKET;
		aWire.mPottedSeed = b.ReadU8Or();
		aWire.mPottedGarden = b.ReadU8Or();
		aWire.mPottedFacing = b.ReadU8Or();
		aWire.mPottedVariation = b.ReadU8Or();
		aWire.mPottedAge = b.ReadU8Or();
        aWire.mCoinAge=b.ReadI32Or();aWire.mFullDisappearCounter=b.ReadI32Or();
        aWire.mCardUses=b.ReadU8Or();
        const auto equipment=aNativeFusion?std::find_if(aFusionFrame.equipmentCoins.begin(),aFusionFrame.equipmentCoins.end(),[&](const auto& r){return r.id==aWire.mId;}):aFusionFrame.equipmentCoins.end();
        const bool typedEquipment=equipment!=aFusionFrame.equipmentCoins.end();
        const auto card=aNativeFusion?std::find_if(aFusionFrame.droppedCards269.begin(),aFusionFrame.droppedCards269.end(),[&](const auto& r){return r.id==aWire.mId;}):aFusionFrame.droppedCards269.end();
        const bool typedCard=card!=aFusionFrame.droppedCards269.end();
        const auto fertilizer=aNativeFusion?std::find_if(aFusionFrame.fertilizeCoins269.begin(),aFusionFrame.fertilizeCoins269.end(),[&](const auto& r){return r.id==aWire.mId;}):aFusionFrame.fertilizeCoins269.end();
        const bool typedFert=fertilizer!=aFusionFrame.fertilizeCoins269.end();
		const bool aPottedPresent = aWire.mType == (uint8_t)CoinType::COIN_PRESENT_PLANT ||
			aWire.mType == (uint8_t)CoinType::COIN_AWARD_PRESENT;
		const bool aValidPottedVisual = aPottedPresent ?
			(aWire.mPottedSeed < (uint8_t)SeedType::NUM_SEED_TYPES &&
			 aWire.mPottedGarden <= (uint8_t)GardenType::GARDEN_AQUARIUM &&
			 aWire.mPottedFacing <= (uint8_t)PottedPlant::FacingDirection::FACING_LEFT &&
			 aWire.mPottedVariation < (uint8_t)DrawVariation::NUM_VARIATIONS &&
			 aWire.mPottedAge <= (uint8_t)PottedPlantAge::PLANTAGE_FULL) :
			(aWire.mPottedSeed == 255 && aWire.mPottedGarden == 255 &&
			 aWire.mPottedFacing == 255 && aWire.mPottedVariation == 255 &&
			 aWire.mPottedAge == 255);
		const bool aValid = aWire.mId != 0 &&
            (!typedEquipment||NetFusionEquipmentCoinRawValid(b,coinRecordStart,equipment->data))&&
            (typedCard?LawnFusion::NativeDroppedCard269CoinStateValid(card->data,aWire.mCoinAge,aWire.mCardUses,aWire.mFullDisappearCounter):aWire.mCardUses==0)&&
            ((typedEquipment||typedFert)?aWire.mType==(uint8_t)COIN_NONE:
            aWire.mType >= (uint8_t)CoinType::COIN_SILVER && aWire.mType <= (uint8_t)CoinType::COIN_PRESENT_SURVIVAL_MODE) &&
            aWire.mCoinAge>=0&&aWire.mFullDisappearCounter>=0&&aWire.mDisappearCounter==(std::min)(65535,aWire.mFullDisappearCounter)&&
			aWire.mMotion <= (uint8_t)CoinMotion::COIN_MOTION_FROM_BOSS &&
			(aWire.mFlags & ~7) == 0 &&
			(aUsablePacket ? aWire.mUsableSeed < (uint8_t)SeedType::NUM_SEED_TYPES : aWire.mUsableSeed == 255) &&
			_finite(aWire.mPosX) && _finite(aWire.mPosY) &&
			aWire.mPosX >= -1024.0f && aWire.mPosX <= 2048.0f &&
			aWire.mPosY >= -1024.0f && aWire.mPosY <= 2048.0f &&
			aWire.mRenderOrder >= -1000000 && aWire.mRenderOrder <= 2000000 &&
			aWire.mScaleQ <= 16384 && aWire.mCollectionDistanceQ <= 8192 &&
			aWire.mFadeCount <= 15 &&
			aValidPottedVisual &&
			aSeenCoins.find(aWire.mId) == aSeenCoins.end();
		if (!aValid)
		{
			NetTrace("snapshot coin section rejected index=%d id=%u type=%u motion=%u flags=%u seed=%u potted=(%u,%u,%u,%u,%u) scale=%u fade=%u",
				i, aWire.mId, aWire.mType, aWire.mMotion, aWire.mFlags,
				aWire.mUsableSeed, aWire.mPottedSeed, aWire.mPottedGarden,
				aWire.mPottedFacing, aWire.mPottedVariation, aWire.mPottedAge,
				aWire.mScaleQ, aWire.mFadeCount);
			return;
		}
		aSeenCoins[aWire.mId] = true;
		aCoinWires.push_back(aWire);
	}

	for (const NetCoinWire& aWire : aCoinWires)
	{
		const bool aPottedPresent = aWire.mType == (uint8_t)CoinType::COIN_PRESENT_PLANT ||
			aWire.mType == (uint8_t)CoinType::COIN_AWARD_PRESENT;
		PottedPlant aPottedVisual = {};
		const PottedPlant* aPottedVisualPtr = nullptr;
		if (aPottedPresent)
		{
			aPottedVisual.mSeedType = (SeedType)aWire.mPottedSeed;
			aPottedVisual.mWhichZenGarden = (GardenType)aWire.mPottedGarden;
			aPottedVisual.mFacing = (PottedPlant::FacingDirection)aWire.mPottedFacing;
			aPottedVisual.mDrawVariation = (DrawVariation)aWire.mPottedVariation;
			aPottedVisual.mPlantAge = (PottedPlantAge)aWire.mPottedAge;
			aPottedVisualPtr = &aPottedVisual;
		}
		Coin* aCoin = NetGetGhostCoin(aWire.mId);
        const auto incoming=aNativeFusion?std::find_if(aFusionFrame.equipmentCoins.begin(),aFusionFrame.equipmentCoins.end(),[&](const auto& r){return r.id==aWire.mId;}):aFusionFrame.equipmentCoins.end();
        const bool typedEquipment=incoming!=aFusionFrame.equipmentCoins.end();
        const auto incomingCard=aNativeFusion?std::find_if(aFusionFrame.droppedCards269.begin(),aFusionFrame.droppedCards269.end(),[&](const auto& r){return r.id==aWire.mId;}):aFusionFrame.droppedCards269.end();
        const bool typedCard=incomingCard!=aFusionFrame.droppedCards269.end();
        const auto incomingFert=aNativeFusion?std::find_if(aFusionFrame.fertilizeCoins269.begin(),aFusionFrame.fertilizeCoins269.end(),[&](const auto& r){return r.id==aWire.mId;}):aFusionFrame.fertilizeCoins269.end();
        const bool typedFert=incomingFert!=aFusionFrame.fertilizeCoins269.end();
        const auto* previousFert=aCoin?LawnFusion::GetNativeFertilize269Data(aCoin):nullptr;
        const bool fertReused=typedFert&&previousFert&&previousFert->instanceToken!=incomingFert->data.instanceToken;
        const auto* previousCard=aCoin?LawnFusion::GetNativeDroppedCard269Data(aCoin):nullptr;
        const bool cardReused=typedCard&&previousCard&&previousCard->instanceToken!=incomingCard->data.instanceToken;
        const auto* previousEquipment=aCoin?LawnFusion::GetNativeEquipmentData(aCoin):nullptr;
        const bool equipmentReused=typedEquipment&&previousEquipment&&previousEquipment->instanceToken!=incoming->data.instanceToken;
		bool aWasBeingCollected = aCoin && aCoin->mIsBeingCollected;
		if (aCoin && (equipmentReused||cardReused||fertReused||aCoin->mType != (CoinType)aWire.mType))
		{
			unsigned int aDataId = mCoins.DataArrayGetID(aCoin);
			if (mCursorObject && mCursorObject->mCoinID == (CoinID)aDataId)
			{
				mCursorObject->mType = SeedType::SEED_NONE;
				mCursorObject->mCursorType = CursorType::CURSOR_TYPE_NORMAL;
				mCursorObject->mCoinID = CoinID::COINID_NULL;
				mApp->SetCursor(CURSOR_POINTER);
			}
			aCoin->Die();
			aCoin = nullptr;
			aWasBeingCollected = false;
		}
		bool aCreated = false;
		if (!aCoin)
		{
			aCoin = mCoins.DataArrayAlloc();
			if (aCoin)
			{
				// This factory creates stock attachments but never Collects, scores,
				// mutates a profile/board, plays sound, or consumes gameplay RNG.
				aCoin->CoinInitializeNetGhost((int)aWire.mPosX, (int)aWire.mPosY,
					(CoinType)aWire.mType, (CoinMotion)aWire.mMotion, aPottedVisualPtr);
				aCoin->mUsableSeedType = aWire.mUsableSeed == 255 ?
					SeedType::SEED_NONE : (SeedType)aWire.mUsableSeed;
			}
			if (!aCoin)
				continue;
			aCreated = true;
			aCoin->mNetId = aWire.mId;
			aCoin->mNetGhost = true;
		}
		const bool aFirstAuthoritativeSample = aCreated || !aCoin->mVisible;
		aCoin->mNetTargetX = aWire.mPosX;
		aCoin->mNetTargetY = aWire.mPosY;
		if (aFirstAuthoritativeSample||typedEquipment||typedCard||typedFert)
		{
			aCoin->mPosX = aWire.mPosX;
			aCoin->mPosY = aWire.mPosY;
		}
		aCoin->mVisible = true;
		// Coin::Draw uses absolute mPos coordinates; mX/mY must stay at the
		// GameObject origin or the renderer translates the coin twice.
		aCoin->mX = 0;
		aCoin->mY = 0;
		aCoin->mRenderOrder = aWire.mRenderOrder;
		aCoin->mScale = aWire.mScaleQ / 4096.0f;
		aCoin->mCollectionDistance = aWire.mCollectionDistanceQ / 4.0f;
		aCoin->mCoinMotion = (CoinMotion)aWire.mMotion;
		aCoin->mIsBeingCollected = (aWire.mFlags & 1) != 0;
		if (!aWasBeingCollected && aCoin->mIsBeingCollected && aCoin->IsMoney())
			ShowCoinBank();
		aCoin->mHitGround = (aWire.mFlags & 2) != 0;
		aCoin->mNeedsBouncyArrow = (aWire.mFlags & 4) != 0;
		aCoin->mUsableSeedType = aWire.mUsableSeed == 255 ? SeedType::SEED_NONE : (SeedType)aWire.mUsableSeed;
		if (aPottedPresent)
		{
			aCoin->mPottedPlantSpec.mSeedType = aPottedVisual.mSeedType;
			aCoin->mPottedPlantSpec.mWhichZenGarden = aPottedVisual.mWhichZenGarden;
			aCoin->mPottedPlantSpec.mFacing = aPottedVisual.mFacing;
			aCoin->mPottedPlantSpec.mDrawVariation = aPottedVisual.mDrawVariation;
			aCoin->mPottedPlantSpec.mPlantAge = aPottedVisual.mPlantAge;
		}
		aCoin->mFadeCount = aWire.mFadeCount;
		aCoin->mDisappearCounter = (typedEquipment||typedCard||typedFert)?aWire.mFullDisappearCounter:aWire.mDisappearCounter;
        if(typedEquipment||typedCard||typedFert)aCoin->mCoinAge=aWire.mCoinAge;
        if(typedCard)aCoin->mTimesDropped=aWire.mCardUses;
		if(!typedEquipment&&!typedCard&&!typedFert)aCoin->NetUpdatePresentation();
	}
	{
		Coin* aCoin = nullptr;
		while (mCoins.IterateNext(aCoin))
			if (aCoin->mNetGhost && !aCoin->mDead && aSeenCoins.find(aCoin->mNetId) == aSeenCoins.end())
			{
				// Once the host omits a coin it is authoritative-dead.  Do not restart
				// StartFade on every snapshot and never let Coin::Update reach ScoreCoin.
				unsigned int aDataId = mCoins.DataArrayGetID(aCoin);
				if (mCursorObject && mCursorObject->mCoinID == (CoinID)aDataId)
				{
					mCursorObject->mType = SeedType::SEED_NONE;
					mCursorObject->mCursorType = CursorType::CURSOR_TYPE_NORMAL;
					mCursorObject->mCoinID = CoinID::COINID_NULL;
					mApp->SetCursor(CURSOR_POINTER);
				}
				aCoin->Die();
			}
	}

	// ---- lawn mowers ----
	// Decode and validate the complete variable-length section before touching
	// any local cleaner.  A bad final record therefore cannot partially move,
	// replace or retire records decoded before it.
	std::vector<NetMowerSnapshotState> aMowerWires;
	if (!NetDecodeMowerSnapshotSection(b, aMowerWires))
	{
		NetTrace("snapshot mower section rejected remaining=%zu record=%zu max-count=%u",
			b.Remaining(), NET_MOWER_WIRE_BYTES, NET_MOWER_MAX_COUNT);
		return;
	}
	std::map<unsigned short, bool> aSeenMowers;
	for (const NetMowerSnapshotState& aWire : aMowerWires)
	{
		aSeenMowers[aWire.mId] = true;
		if (!aWire.mReanim.mPresent)
			continue;

		uint32_t aFrameCapacity = 0;
		LawnMower* anExistingMower = NetGetGhostMower(aWire.mId);
		if (anExistingMower &&
			anExistingMower->mMowerType == (LawnMowerType)aWire.mType)
		{
			Reanimation* anExistingReanim =
				mApp->ReanimationTryToGet(anExistingMower->mReanimID);
			aFrameCapacity = NetGetReanimationFrameCapacity(anExistingReanim);
		}
		if (aFrameCapacity == 0 &&
			!NetGetMowerTypeFrameCapacity(aWire.mType, aFrameCapacity))
		{
			NetTrace("snapshot mower animation rejected: definition unavailable id=%u type=%u",
				aWire.mId, aWire.mType);
			return;
		}
		if (!NetZombieAnimStateFitsFrames(aWire.mReanim, aFrameCapacity))
		{
			NetTrace("snapshot mower animation rejected: id=%u type=%u frames=%u+%u capacity=%u",
				aWire.mId, aWire.mType, aWire.mReanim.mFrameStart,
				aWire.mReanim.mFrameCount, aFrameCapacity);
			return;
		}
	}

	for (const NetMowerSnapshotState& aWire : aMowerWires)
	{
		LawnMower* aMower = NetGetGhostMower(aWire.mId);
		if (!aMower)
		{
			LawnMower* aLocalMower = nullptr;
			while (mLawnMowers.IterateNext(aLocalMower))
			{
				if (!aLocalMower->mDead && !aLocalMower->mNetGhost &&
					aLocalMower->mRow == aWire.mRow)
				{
					aMower = aLocalMower;
					break;
				}
			}
		}

		bool aCreated = false;
		if (!aMower)
		{
			gNetSuppressSounds = true;
			aMower = mLawnMowers.DataArrayAlloc();
			if (aMower)
				aMower->LawnMowerInitialize(aWire.mRow,
					(LawnMowerType)aWire.mType);
			gNetSuppressSounds = false;
			if (!aMower)
				continue;
			aCreated = true;
		}
		else
		{
			Reanimation* anExistingReanim =
				mApp->ReanimationTryToGet(aMower->mReanimID);
			const bool aNeedsLiveReanim = aWire.mReanim.mPresent &&
				!aWire.mReanim.mDead &&
				(!anExistingReanim || anExistingReanim->mDead);
			const bool aNeedsCleanTransform =
				aMower->mMowerState == LawnMowerState::MOWER_SQUISHED &&
				aWire.mState != (uint8_t)LawnMowerState::MOWER_SQUISHED;
			if (aMower->mMowerType != (LawnMowerType)aWire.mType ||
				aNeedsLiveReanim || aNeedsCleanTransform)
			{
				if (anExistingReanim)
					anExistingReanim->ReanimationDie();
				aMower->LawnMowerInitialize(aWire.mRow,
					(LawnMowerType)aWire.mType);
				aCreated = true;
			}
		}

		const int aPreviousRow = aMower->mRow;
		const bool aPortalJump = !aCreated &&
			mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_PORTAL_COMBAT &&
			aPreviousRow != aWire.mRow &&
			(fabsf(aWire.mPosX - aMower->mPosX) >= 120.0f ||
			 fabsf(aWire.mPosY - aMower->mPosY) >= 50.0f);
		aMower->mNetId = aWire.mId;
		aMower->mNetGhost = true;
		aMower->mRow = aWire.mRow;
		aMower->mMowerState = (LawnMowerState)aWire.mState;
		aMower->mVisible = aWire.mVisible;
		aMower->mNetTargetX = aWire.mPosX;
		aMower->mNetTargetY = aWire.mPosY;
		if (aCreated || aPortalJump)
		{
			aMower->mPosX = aWire.mPosX;
			aMower->mPosY = aWire.mPosY;
			if (aPortalJump)
				NetTrace("portal mower presentation snap id=%u row=%d->%u pos=(%.2f,%.2f)",
					aWire.mId, aPreviousRow, aWire.mRow,
					aWire.mPosX, aWire.mPosY);
		}
		aMower->mRenderOrder = aWire.mRenderOrder;
		aMower->mAltitude = aWire.mAltitude;
		aMower->mMowerHeight = (MowerHeight)aWire.mHeight;

		Reanimation* aMowerReanim =
			mApp->ReanimationTryToGet(aMower->mReanimID);
		if (aMowerReanim)
		{
			aMowerReanim->mRenderOrder = aWire.mRenderOrder;
			if (!aWire.mReanim.mPresent)
			{
				aMowerReanim->mAnimRate = 0.0f;
				aMowerReanim->mDead = true;
			}
			else
			{
				aMowerReanim->mFrameStart = aWire.mReanim.mFrameStart;
				aMowerReanim->mFrameCount = aWire.mReanim.mFrameCount;
				aMowerReanim->mAnimTime = aWire.mReanim.mAnimTime;
				aMowerReanim->mLastFrameTime = aWire.mReanim.mAnimTime;
				aMowerReanim->mAnimRate = aWire.mReanim.mAnimRate;
				aMowerReanim->mLoopType =
					(ReanimLoopType)aWire.mReanim.mLoopType;
				aMowerReanim->mLoopCount = aWire.mReanim.mLoopCount;
				aMowerReanim->mDead = aWire.mReanim.mDead;
			}
			if (aMower->mMowerState == LawnMowerState::MOWER_SQUISHED)
			{
				aMowerReanim->OverrideScale(0.85f, 0.22f);
				aMowerReanim->SetPosition(-11.0f, 65.0f);
			}
		}
	}
	{
		LawnMower* aMower = nullptr;
		while (mLawnMowers.IterateNext(aMower))
		{
			if (!aMower->mDead &&
				((aMower->mNetGhost && aSeenMowers.find(aMower->mNetId) == aSeenMowers.end()) || !aMower->mNetGhost))
			{
				Reanimation* aReanim = mApp->ReanimationTryToGet(aMower->mReanimID);
				if (aReanim)
					aReanim->ReanimationDie();
				aMower->mDead = true;
			}
		}
	}

	// ---- grid items ----
	aCount = b.ReadU16Or();
	std::map<unsigned short, bool> aSeenGridItems;
	for (int i = 0; i < aCount; i++)
	{
		static const size_t GRID_ITEM_WIRE_BYTES = 37;
		if (b.Remaining() < GRID_ITEM_WIRE_BYTES)
		{
			NetTrace("snapshot grid section truncated index=%d count=%u remaining=%zu", i, aCount, b.Remaining());
			return;
		}
		uint16_t anId = b.ReadU16Or();
		uint8_t aType = b.ReadU8Or();
		uint8_t aState = b.ReadU8Or();
		uint8_t aX = b.ReadU8Or();
		uint8_t aY = b.ReadU8Or();
		int32_t aCounter = b.ReadI32Or();
		int32_t aRenderOrder = b.ReadI32Or();
		float aPosX = b.ReadF32Or();
		float aPosY = b.ReadF32Or();
		float aGoalX = b.ReadF32Or();
		float aGoalY = b.ReadF32Or();
		uint8_t aZombieType = b.ReadU8Or();
		uint8_t aSeedType = b.ReadU8Or();
		uint8_t aScaryPotType = b.ReadU8Or();
		uint16_t aTransparentCounter = b.ReadU16Or();
		uint8_t aSunCount = b.ReadU8Or();
		uint8_t aFlags = b.ReadU8Or();
		bool aPortal = aType == (uint8_t)GridItemType::GRIDITEM_PORTAL_CIRCLE ||
			aType == (uint8_t)GridItemType::GRIDITEM_PORTAL_SQUARE;
		bool aValidCell = IsValidGridCell(aX, aY) ||
			(!LawnFusion::IsNativeFusionBoard(this) && aPortal && aX == MAX_GRID_SIZE_X && aY < MAX_GRID_SIZE_Y);
		bool aValid = anId != 0 && aType >= (uint8_t)GridItemType::GRIDITEM_GRAVESTONE &&
			aType <= (uint8_t)GridItemType::GRIDITEM_IZOMBIE_BRAIN &&
			NetGridStateMatchesType(aType, aState) && aValidCell &&
			(aZombieType == 255 || aZombieType < (uint8_t)ZombieType::NUM_ZOMBIE_TYPES) &&
			(aSeedType == 255 || aSeedType < (uint8_t)SeedType::NUM_SEED_TYPES) &&
			aScaryPotType <= (uint8_t)ScaryPotType::SCARYPOT_SUN &&
			_finite(aPosX) && _finite(aPosY) && _finite(aGoalX) && _finite(aGoalY);
		if (!aValid)
		{
			NetTrace("snapshot grid rejected id=%u type=%u state=%u cell=(%u,%u) zombie=%u seed=%u pot=%u",
				anId, aType, aState, aX, aY, aZombieType, aSeedType, aScaryPotType);
			continue;
		}
		bool aSeedPot = aScaryPotType == (uint8_t)ScaryPotType::SCARYPOT_SEED;
		bool aZombiePot = aScaryPotType == (uint8_t)ScaryPotType::SCARYPOT_ZOMBIE;
		bool aSunPot = aScaryPotType == (uint8_t)ScaryPotType::SCARYPOT_SUN;
		if (aType == (uint8_t)GridItemType::GRIDITEM_SCARY_POT &&
			(aY >= MAX_GRID_SIZE_Y - 1 || aScaryPotType == (uint8_t)ScaryPotType::SCARYPOT_NONE ||
			 (aSeedPot && (aSeedType == 255 || aZombieType != 255 || aSunCount != 0)) ||
			 (aZombiePot && (aZombieType == 255 || aSeedType != 255 || aSunCount != 0)) ||
			 (aSunPot && (aZombieType != 255 || aSeedType != 255 || aSunCount < 1 || aSunCount > 3)) ||
			 (aState == (uint8_t)GridItemState::GRIDITEM_STATE_SCARY_POT_LEAF && !aSeedPot) ||
			 (aState == (uint8_t)GridItemState::GRIDITEM_STATE_SCARY_POT_ZOMBIE && !aZombiePot) ||
			 aTransparentCounter > 50))
		{
			NetTrace("snapshot scary pot rejected id=%u state=%u content=%u zombie=%u seed=%u",
				anId, aState, aScaryPotType, aZombieType, aSeedType);
			continue;
		}
		if (aSeenGridItems.find(anId) != aSeenGridItems.end())
		{
			NetTrace("snapshot grid rejected duplicate id=%u", anId);
			continue;
		}
		aSeenGridItems[anId] = true;

		GridItem* aGridItem = NetGetGhostGridItem(anId);
		if (aGridItem && aGridItem->mGridItemType != (GridItemType)aType)
		{
			NetTrace("snapshot grid rejected: id=%u changed type old=%d new=%u", anId, (int)aGridItem->mGridItemType, aType);
			continue;
		}
		if (!aGridItem)
		{
			// Adopt the stock client-side item created during local resource/intro
			// setup when possible.  Its random contents are overwritten below.
			GridItem* aLocalItem = nullptr;
			while (mGridItems.IterateNext(aLocalItem))
			{
				if (!aLocalItem->mDead && !aLocalItem->mNetGhost &&
					aLocalItem->mGridItemType == (GridItemType)aType &&
					aLocalItem->mGridX == aX && aLocalItem->mGridY == aY)
				{
					aGridItem = aLocalItem;
					break;
				}
			}
		}
		GridItemState aPreviousState = aGridItem ? aGridItem->mGridItemState : (GridItemState)aState;
		if (!aGridItem)
		{
			gNetSuppressSounds = true;
			aGridItem = mGridItems.DataArrayAlloc();
			if (!aGridItem)
				continue;
			gNetSuppressSounds = false;
		}
		aGridItem->mGridItemType = (GridItemType)aType;
		aGridItem->mGridItemState = (GridItemState)aState;
		aGridItem->mGridX = aX;
		aGridItem->mGridY = aY;
		aGridItem->mGridItemCounter = aCounter;
		aGridItem->mRenderOrder = aRenderOrder;
		aGridItem->mPosX = aPosX;
		aGridItem->mPosY = aPosY;
		aGridItem->mGoalX = aGoalX;
		aGridItem->mGoalY = aGoalY;
		aGridItem->mZombieType = aZombieType == 255 ? ZombieType::ZOMBIE_INVALID : (ZombieType)aZombieType;
		aGridItem->mSeedType = aSeedType == 255 ? SeedType::SEED_NONE : (SeedType)aSeedType;
		aGridItem->mScaryPotType = (ScaryPotType)aScaryPotType;
		aGridItem->mTransparentCounter = aTransparentCounter;
		aGridItem->mSunCount = aSunCount;
		aGridItem->mHighlighted = (aFlags & 1) != 0;
		NetEnsureGridItemVisual(aGridItem, aPreviousState);
		aGridItem->mNetId = anId;
		aGridItem->mNetGhost = true;
	}
	{
		GridItem* aGridItem = nullptr;
		while (mGridItems.IterateNext(aGridItem))
			if (!aGridItem->mDead &&
				((aGridItem->mNetGhost && aSeenGridItems.find(aGridItem->mNetId) == aSeenGridItems.end()) ||
				 (!aGridItem->mNetGhost &&
				  (aGridItem->mGridItemType == GridItemType::GRIDITEM_SCARY_POT ||
				   aGridItem->mGridItemType == GridItemType::GRIDITEM_PORTAL_CIRCLE ||
				   aGridItem->mGridItemType == GridItemType::GRIDITEM_PORTAL_SQUARE))))
				aGridItem->GridItemDie();
	}
	if (b.Remaining() != 0)
	{
		NetTrace("snapshot ignored trailing bytes=%zu", b.Remaining());
		return;
	}
	// The new shared Zen view is committed as one unit only after the entire
	// snapshot has passed validation, including every later entity section.
	if (mApp->mZenGarden)
	{
		mApp->mZenGarden->ApplyNetGardenState(aZenState);
		mApp->mZenGarden->mNetPlantStates.swap(aZenPlantStates);
		if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
		{
			Plant* plant = nullptr;
			while (mPlants.IterateNext(plant))
			{
				if (plant->mDead || mApp->mZenGarden->mNetPlantStates.find(plant->mNetId) == mApp->mZenGarden->mNetPlantStates.end())
					continue;
				Plant* pot = GetTopPlantAt(plant->mPlantCol, plant->mRow, PlantPriority::TOPPLANT_ONLY_UNDER_PLANT);
				const bool darkPot = plant->mState == PlantState::STATE_ZEN_GARDEN_WATERED ||
					plant->mState == PlantState::STATE_ZEN_GARDEN_NEEDY || plant->mState == PlantState::STATE_ZEN_GARDEN_HAPPY;
				if (pot && !Plant::IsAquatic(plant->mSeedType))
					if (Reanimation* body = mApp->ReanimationTryToGet(pot->mBodyReanimID))
						body->SetImageOverride("Pot_top", darkPot ? IMAGE_REANIM_POT_TOP_DARK : nullptr);
				Plant* source = pot ? pot : plant;
				TodParticleSystem* particle = mApp->ParticleTryToGet(source->mParticleID);
				const bool glow = particle && !particle->mDead &&
					(particle->mEffectType == ParticleEffect::PARTICLE_POTTED_ZEN_GLOW ||
					 particle->mEffectType == ParticleEffect::PARTICLE_POTTED_PLANT_GLOW ||
					 particle->mEffectType == ParticleEffect::PARTICLE_POTTED_WATER_PLANT_GLOW);
				if (plant->mState == PlantState::STATE_ZEN_GARDEN_HAPPY && !glow)
					mApp->mZenGarden->AddHappyEffect(plant);
				else if (plant->mState != PlantState::STATE_ZEN_GARDEN_HAPPY && glow)
					particle->ParticleSystemDie();
			}
		}
	}
	if (aNativeFusion && !NetBindNativeFusionFrame(this, aFusionFrame, aFusionSelection))
	{
		NetTrace("native Fusion snapshot missing native allocation epoch=%u", aFusionFrame.epoch);
		return;
	}
    if(aNativeFusion&&LawnFusion::IsNativeSourceConveyor308(this))
    {
        std::string error;
        if(!LawnFusion::AdoptNativeConveyor308(this,aConveyorCursors,&error))
        {NetTrace("Fusion source conveyor adoption invariant failed epoch=%u error=%s",aFusionFrame.epoch,error.c_str());return;}
    }
	if(aNativeFusion)LawnFusion::SyncNativeTools268Cursors(this);
	// Commit the fog cache and weather together only after the complete snapshot
	// validates. Drawing consumes this native grid directly; no local fog warmup.
	mNetDayPhase = aWeather.mDayPhase;
	mNetWeatherTimer = aWeather.mWeatherTimer;
	mNetFogTimer = aWeather.mFogTimer;
	mNetStormTimer = aWeather.mStormTimer;
	mNetNightBlend = aWeather.mNightBlend;
	mNetStormFlashKind = aWeather.mStormFlashKind;
	mNetStormFlashCounter = aWeather.mStormFlashCounter;
	mFogOffset = aFogOffset2 / 2.0f;
	mFogBlownCountDown = aFogBlownCountDown;
	for (size_t x = 0; x < NET_ENDLESS_FOG_COLUMNS; ++x)
		for (size_t y = 0; y < NET_ENDLESS_FOG_ROWS; ++y)
			mGridCelFog[x][y] = aWeather.mFogGrid[x * NET_ENDLESS_FOG_ROWS + y];
	mNetWeatherSnapshotReady = true;
	mTutorialTimer = aTutorialTimer;
	mShowShovel = aShowShovel != 0;
	if (mApp->mMusic)
	{
		// T/ST chooses the new tune at its local silent visual midpoint.
		if (!(IsEndlessT() && (mApp->mGameScene == GameScenes::SCENE_PLAYING || mPaused)))
		{
			if (aMusicTune == MusicTune::MUSIC_TUNE_NONE)
				mApp->mMusic->StopAllMusic();
			else
				mApp->mMusic->MakeSureMusicIsPlaying((MusicTune)aMusicTune);
		}
		if (mApp->mMusic->mPaused != (aMusicPaused != 0))
			mApp->mMusic->GameMusicPause(aMusicPaused != 0);
	}

	// Challenge::Update is deliberately host-only.  Reconstruct the slot-machine
	// handle's cosmetic transitions only after the complete snapshot was accepted.
	// The pull sound arrives once through reliable BOARDEV_SOUND; replaying it here
	// would double the audio on clients.
	if (mApp->IsSlotMachineLevel())
	{
		Reanimation* aHandle = mApp->ReanimationTryToGet(mChallenge->mReanimChallenge);
		if (aHandle && aChallengeState == ChallengeState::STATECHALLENGE_SLOT_MACHINE_ROLLING &&
			(aPreviousChallengeState != ChallengeState::STATECHALLENGE_SLOT_MACHINE_ROLLING ||
			 aChallengeRuntime.mSlotMachineRollCount > aPreviousSlotMachineRollCount))
		{
			aHandle->PlayReanim("anim_pull", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 36.0f);
			NetTrace("slot-machine presentation pull roll=%u previousRoll=%d",
				aChallengeRuntime.mSlotMachineRollCount, aPreviousSlotMachineRollCount);
		}
		else if (aHandle &&
			aPreviousChallengeState == ChallengeState::STATECHALLENGE_SLOT_MACHINE_ROLLING &&
			aChallengeState == ChallengeState::STATECHALLENGE_NORMAL)
		{
			aHandle->PlayReanim("anim_return", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 24.0f);
			NetTrace("slot-machine presentation return roll=%u",
				aChallengeRuntime.mSlotMachineRollCount);
		}
		mNetPresentedSlotMachineState = aChallengeState;
		mNetPresentedSlotMachineRollCount = aChallengeRuntime.mSlotMachineRollCount;
	}
	if (mChallenge)
		mChallenge->mShowBowlingLine = aShowBowlingLine;
	// Commit camera presentation only after every fixed and variable section has
	// validated.  A malformed tail must not move the client viewport.
	const bool aFirstGameplayCameraSample = !mNetGameplayCameraTargets.mValid;
	const bool aCameraDiscontinuity = abs(aGameplayBoardX - mX) >= 160 ||
		abs(aGameplayBoardY - mY) >= 120;
	mNetGameplayCameraTargets.mBoardX = aGameplayBoardX;
	mNetGameplayCameraTargets.mBoardY = aGameplayBoardY;
	mNetGameplayCameraTargets.mTreeX = aGameplayTreeX;
	mNetGameplayCameraTargets.mPoleX = aGameplayPoleX;
	mNetGameplayCameraTargets.mValid = mApp->mGameScene != GameScenes::SCENE_LEVEL_INTRO;
	if (mNetGameplayCameraTargets.mValid && (aFirstGameplayCameraSample || aCameraDiscontinuity))
	{
		Move(aGameplayBoardX, aGameplayBoardY);
		mTreeX = aGameplayTreeX;
		mPoleX = aGameplayPoleX;
	}
	// Intro targets describe a different scene and must never pull a playing
	// client back toward its final cutscene camera sample.
	if (mApp->mGameScene != GameScenes::SCENE_LEVEL_INTRO)
		mNetIntroTargets.mValid = false;
	// Terminal presentation is committed only after the complete snapshot passed
	// every variable-section check.  The host continues sending terminal snapshots
	// in ZOMBIES_WON so a dropped last gameplay packet cannot strand the client.
	if (aZombiesWon && mApp->mGameScene != GameScenes::SCENE_ZOMBIES_WON)
	{
		mApp->mBoardResult = BoardResult::BOARDRESULT_LOST;
		mApp->mGameScene = GameScenes::SCENE_ZOMBIES_WON;
		mCutScene->StartZombiesWon();
		FreezeEffectsForCutscene(true);
		TutorialArrowRemove();
		UpdateCursor();
		NetTrace("terminal loss snapshot applied boardResult=%d", (int)mApp->mBoardResult);
	}
	if (mApp && mApp->CoopSmokeActive())
		mApp->CoopSmokeOnSnapshotApplied(aSmokeSnapshotHash, aSmokeSnapshotBytes);
	if (theApplied) *theApplied = true;
}

// ==========================================================================
// Client ghost per-tick updates
// ==========================================================================

void Board::NetGhostUpdateZombie(Zombie* theZombie)
{
	if (mPaused) return;
	// interpolate position toward the authoritative target
	theZombie->mPosX += (theZombie->mNetTargetX - theZombie->mPosX) * 0.25f;
	theZombie->mPosY += (theZombie->mNetTargetY - theZombie->mPosY) * 0.25f;
	theZombie->mX = (int)theZombie->mPosX;
	theZombie->mY = (int)theZombie->mPosY;

	// cosmetic counters
	if (theZombie->mJustGotShotCounter > 0)
		theZombie->mJustGotShotCounter--;
	if (theZombie->mChilledCounter > 0)
		theZombie->mChilledCounter--;
	if (theZombie->mButteredCounter > 0)
		theZombie->mButteredCounter--;
	if (theZombie->mIceTrapCounter > 0)
		theZombie->mIceTrapCounter--;

	// Do not call Zombie::Animate() on a client ghost: chew callbacks mutate
	// health and can trigger hypno/death gameplay.  Only advance its attached
	// visual tree and keep that tree positioned on the interpolated entity.
	AttachmentUpdateAndMove(theZombie->mAttachmentID, theZombie->mPosX, theZombie->mPosY);
	theZombie->UpdateReanim();
    if(auto* captures=LawnFusion::GetNativeCapturedPlants314(this))
        if(const auto* data=LawnFusion::GetNativeSourceZombieData(theZombie);data&&data->kind==45)
        {LawnFusion::CapturedMatrix314 matrix;if(LawnFusion::GetNativeSourceZombieMatrix313(theZombie,"",matrix))captures->UpdateOwner(theZombie,matrix);}
}

void Board::NetGhostUpdateOthers()
{
	// Host pause freezes all client-side presentation clocks and interpolation.
	if (mPaused)
		return;

	// Projectiles: interpolate every float used by Draw, keep the integer
	// GameObject origin coherent, and advance only the attached visual tree.
	Projectile* aProjectile = nullptr;
	while (mProjectiles.IterateNext(aProjectile))
	{
		if (aProjectile->mDead || !aProjectile->mNetGhost)
			continue;
		aProjectile->mPosX += (aProjectile->mNetTargetX - aProjectile->mPosX) * 0.35f;
		aProjectile->mPosY += (aProjectile->mNetTargetY - aProjectile->mPosY) * 0.35f;
		aProjectile->mPosZ += (aProjectile->mNetTargetZ - aProjectile->mPosZ) * 0.35f;
        if(LawnFusion::IsNativeCannon268Projectile(aProjectile))aProjectile->mRotation=aProjectile->mNetTargetRotation;
        else aProjectile->mRotation += (aProjectile->mNetTargetRotation - aProjectile->mRotation) * 0.35f;
		aProjectile->mX = (int)aProjectile->mPosX;
		aProjectile->mY = (int)(aProjectile->mPosY + aProjectile->mPosZ);
		AttachmentUpdateAndMove(aProjectile->mAttachmentID,
			aProjectile->mPosX, aProjectile->mPosY + aProjectile->mPosZ);
	}

	// Coins: host position is authoritative both while falling and while flying
	// into the bank.  Never call Coin::Update here: it can reach ScoreCoin and
	// mutate sun/profile/gameplay state on a client.
	Coin* aCoin = nullptr;
	while (mCoins.IterateNext(aCoin))
	{
		if (aCoin->mDead || !aCoin->mNetGhost)
			continue;
        if(LawnFusion::IsNativeEquipmentCoin(aCoin)){aCoin->mPosX=aCoin->mNetTargetX;aCoin->mPosY=aCoin->mNetTargetY;}
        else {aCoin->mPosX += (aCoin->mNetTargetX - aCoin->mPosX) * 0.35f;aCoin->mPosY += (aCoin->mNetTargetY - aCoin->mPosY) * 0.35f;}
		aCoin->mX = 0;
		aCoin->mY = 0;
		if (aCoin->mAttachmentID != AttachmentID::ATTACHMENTID_NULL)
		{
			float anOffsetX = 0.0f;
			float anOffsetY = 0.0f;
			if (aCoin->mType == CoinType::COIN_DIAMOND)
			{
				anOffsetX = 18.0f - 18.0f * aCoin->mScale;
				anOffsetY = 13.0f - 13.0f * aCoin->mScale;
			}
			AttachmentUpdateAndMove(aCoin->mAttachmentID,
				aCoin->mPosX + anOffsetX, aCoin->mPosY + anOffsetY);
			AttachmentOverrideColor(aCoin->mAttachmentID, aCoin->GetColor());
			AttachmentOverrideScale(aCoin->mAttachmentID, aCoin->mScale);
		}
	}

	// mowers: interpolate
	LawnMower* aMower = nullptr;
	while (mLawnMowers.IterateNext(aMower))
	{
		if (aMower->mDead || !aMower->mNetGhost)
			continue;
		aMower->mPosX += (aMower->mNetTargetX - aMower->mPosX) * 0.3f;
		aMower->mPosY += (aMower->mNetTargetY - aMower->mPosY) * 0.3f;
		Reanimation* aMowerReanim = mApp->ReanimationTryToGet(aMower->mReanimID);
		if (aMowerReanim && aMowerReanim->mIsAttachment)
			aMowerReanim->Update();
	}

	// Grid-item visuals created from snapshots are attachments in the stock
	// code, so EffectSystem deliberately does not tick them.
	GridItem* aGridItem = nullptr;
	while (mGridItems.IterateNext(aGridItem))
	{
		if (aGridItem->mDead || !aGridItem->mNetGhost)
			continue;
		Reanimation* aGridReanim = mApp->ReanimationTryToGet(aGridItem->mGridItemReanimID);
		if (!aGridReanim || !aGridReanim->mIsAttachment)
			continue;
		aGridReanim->Update();
		if ((aGridItem->mGridItemType == GridItemType::GRIDITEM_PORTAL_CIRCLE ||
			aGridItem->mGridItemType == GridItemType::GRIDITEM_PORTAL_SQUARE) &&
			aGridItem->mGridItemState != GridItemState::GRIDITEM_STATE_PORTAL_CLOSED &&
			aGridReanim->mLoopType == ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD && aGridReanim->mLoopCount > 0)
		{
			aGridReanim->PlayReanim("anim_pulse", ReanimLoopType::REANIM_LOOP, 0, 12.0f);
			if (!mApp->ParticleTryToGet(aGridItem->mGridItemParticleID))
			{
				ParticleEffect anEffect = ParticleEffect::PARTICLE_PORTAL_CIRCLE;
				float aX = aGridItem->mGridX * 80.0f + 13.0f;
				float aY = GridToPixelY(0, aGridItem->mGridY) - 39.0f;
				if (aGridItem->mGridItemType == GridItemType::GRIDITEM_PORTAL_SQUARE)
				{
					anEffect = ParticleEffect::PARTICLE_PORTAL_SQUARE;
					aX -= 8.0f;
					aY += 15.0f;
				}
				TodParticleSystem* aParticle = mApp->AddTodParticle(aX, aY, 0, anEffect);
				if (aParticle)
					aGridItem->mGridItemParticleID = mApp->ParticleGetID(aParticle);
			}
		}
	}
}

void Board::NetUpdateIntroPresentation(bool theSnap)
{
	if (!mNetIntroTargets.mValid)
		return;
	if (!theSnap && (mApp->mGameScene != GameScenes::SCENE_LEVEL_INTRO || mPaused))
		return;

	const int aBoardX = theSnap ? mNetIntroTargets.mBoardX :
		NetApproachPresentationInt(mX, mNetIntroTargets.mBoardX);
	const int aBoardY = theSnap ? mNetIntroTargets.mBoardY :
		NetApproachPresentationInt(mY, mNetIntroTargets.mBoardY);
	Move(aBoardX, aBoardY);

	mTreeX = theSnap ? mNetIntroTargets.mTreeX :
		NetApproachPresentationInt(mTreeX, mNetIntroTargets.mTreeX);
	mPoleX = theSnap ? mNetIntroTargets.mPoleX :
		NetApproachPresentationInt(mPoleX, mNetIntroTargets.mPoleX);
	mSodPosition = theSnap ? mNetIntroTargets.mSodPosition :
		NetApproachPresentationInt(mSodPosition, mNetIntroTargets.mSodPosition);

	const int aFogOffset2 = theSnap ? mNetIntroTargets.mFogOffset2 :
		NetApproachPresentationInt((int)(mFogOffset * 2.0f), mNetIntroTargets.mFogOffset2);
	mFogOffset = aFogOffset2 / 2.0f;

	if (mSeedBank)
	{
		const int aSeedBankX = theSnap ? mNetIntroTargets.mSeedBankX :
			NetApproachPresentationInt(mSeedBank->mX, mNetIntroTargets.mSeedBankX);
		const int aSeedBankY = theSnap ? mNetIntroTargets.mSeedBankY :
			NetApproachPresentationInt(mSeedBank->mY, mNetIntroTargets.mSeedBankY);
		mSeedBank->Move(aSeedBankX, aSeedBankY);
		mSeedBank->mCutSceneDarken = theSnap ? mNetIntroTargets.mSeedBankDarken :
			NetApproachPresentationInt(mSeedBank->mCutSceneDarken,
				mNetIntroTargets.mSeedBankDarken);
	}

	SeedChooserScreen* aChooser = mApp->mSeedChooserScreen;
	if (aChooser)
	{
		const int aWideX = mApp->mDDInterface ? mApp->mDDInterface->mWideScreenOffsetX : 0;
		const int aWideY = mApp->mDDInterface ? mApp->mDDInterface->mWideScreenOffsetY : 0;
		const int aChooserCanonicalX = aChooser->mX - aWideX;
		const int aChooserCanonicalY = aChooser->mY - aWideY;
		const int aChooserX = theSnap ? mNetIntroTargets.mChooserX :
			NetApproachPresentationInt(aChooserCanonicalX, mNetIntroTargets.mChooserX);
		const int aChooserY = theSnap ? mNetIntroTargets.mChooserY :
			NetApproachPresentationInt(aChooserCanonicalY, mNetIntroTargets.mChooserY);
		aChooser->Move(aChooserX + aWideX, aChooserY + aWideY);
		if (aChooser->mMenuButton)
		{
			const int aMenuCanonicalY = aChooser->mMenuButton->mY - aWideY;
			const int aMenuY = theSnap ? mNetIntroTargets.mChooserMenuY :
				NetApproachPresentationInt(aMenuCanonicalY, mNetIntroTargets.mChooserMenuY);
			aChooser->mMenuButton->mY = aMenuY + aWideY;
		}
	}
}

// START_PLAYING is a reliable barrier, so carry the final host camera with it.
// Gameplay snapshots keep streaming the same canonical fields afterwards, but
// waiting for the first snapshot allowed one or more frames at the client's
// constructor/default position (and could leave special challenge cameras
// visibly shifted when the intro transition was skipped).
void Board::NetEncodeCameraBootstrap(NetBuffer& b)
{
	b.WriteI16((int16_t)max(-2048, min(2048, mX)));
	b.WriteI16((int16_t)max(-1024, min(1024, mY)));
	b.WriteI16((int16_t)max(-4096, min(4096, mTreeX)));
	b.WriteI16((int16_t)max(-4096, min(4096, mPoleX)));
	NetTrace("camera bootstrap encoded board=(%d,%d) tree=%d pole=%d",
		mX, mY, mTreeX, mPoleX);
}

bool Board::NetApplyCameraBootstrap(NetBuffer& b)
{
	if (b.Remaining() != 8)
	{
		NetTrace("camera bootstrap rejected: bytes=%zu expected=8", b.Remaining());
		return false;
	}
	const int aBoardX = b.ReadI16Or();
	const int aBoardY = b.ReadI16Or();
	const int aTreeX = b.ReadI16Or();
	const int aPoleX = b.ReadI16Or();
	if (aBoardX < -2048 || aBoardX > 2048 ||
		aBoardY < -1024 || aBoardY > 1024 ||
		aTreeX < -4096 || aTreeX > 4096 ||
		aPoleX < -4096 || aPoleX > 4096)
	{
		NetTrace("camera bootstrap rejected: board=(%d,%d) tree=%d pole=%d",
			aBoardX, aBoardY, aTreeX, aPoleX);
		return false;
	}

	Move(aBoardX, aBoardY);
	mTreeX = aTreeX;
	mPoleX = aPoleX;
	mNetGameplayCameraTargets.mBoardX = aBoardX;
	mNetGameplayCameraTargets.mBoardY = aBoardY;
	mNetGameplayCameraTargets.mTreeX = aTreeX;
	mNetGameplayCameraTargets.mPoleX = aPoleX;
	mNetGameplayCameraTargets.mValid = true;
	// The reliable bootstrap supersedes the last intro interpolation target.
	// LawnApp::StartPlaying must not pull the board back to an older sample.
	mNetIntroTargets.mValid = false;
	NetTrace("camera bootstrap applied board=(%d,%d) tree=%d pole=%d",
		aBoardX, aBoardY, aTreeX, aPoleX);
	return true;
}

void Board::NetEncodeStartPlaying(NetBuffer& b)
{
	if (LawnFusion::IsNativeFusionBoard(this))
	{
		// The existing ordered gameplay barrier carries the same native state
		// as a snapshot, including the final reveal and stable packet identities.
		NetEncodeSnapshot(b);
		return;
	}
	const NetMysteryLimits limits = {(uint8_t)NUM_SEEDS_IN_CHOOSER, (uint8_t)SEED_IMITATER, (uint8_t)SEED_GATLINGPEA};
	NetMysteryStart state;
	state.mBoardX = (int16_t)max(-2048, min(2048, mX));
	state.mBoardY = (int16_t)max(-1024, min(1024, mY));
	state.mTreeX = (int16_t)max(-4096, min(4096, mTreeX));
	state.mPoleX = (int16_t)max(-4096, min(4096, mPoleX));
	if (!mSeedBank || mSeedBank->mNumPackets < 0 || mSeedBank->mNumPackets > SEEDBANK_CAPACITY ||
		mNetMysteryRevealIds.size() > NET_MYSTERY_MAX_CARDS)
	{ NetTrace("START_PLAYING encode rejected invalid bank/reveal count"); return; }
	for (int i = 0; i < mSeedBank->mNumPackets; ++i)
		if (mSeedBank->mSeedPackets[i].IsMysteryPending())
		{ NetTrace("START_PLAYING encode rejected unresolved packet id=%u", mSeedBank->mSeedPackets[i].mNetId); return; }
	for (uint16_t id : mNetMysteryRevealIds)
	{
		const SeedPacket* found = nullptr;
		for (int i = 0; i < mSeedBank->mNumPackets; ++i)
			if (mSeedBank->mSeedPackets[i].mNetId == id)
			{
				if (found) { NetTrace("START_PLAYING encode rejected duplicate bank id=%u", id); return; }
				found = &mSeedBank->mSeedPackets[i];
			}
		if (!found || !found->mRandomDiscountLevel || found->mRandomDiscountLevel > 10)
		{ NetTrace("START_PLAYING encode rejected missing mystery identity id=%u", id); return; }
		NetMysteryReveal reveal;
		reveal.mId = id; reveal.mType = (uint8_t)found->mPacketType; reveal.mImitater = (uint8_t)found->mImitaterType;
		state.mReveals.push_back(reveal);
	}
	if (!NetEncodeMysteryStart(state, b, limits))
	{ NetTrace("START_PLAYING encode rejected invalid reveal state"); return; }
	NetTrace("camera bootstrap encoded board=(%d,%d) tree=%d pole=%d",
		state.mBoardX, state.mBoardY, state.mTreeX, state.mPoleX);
	NetTrace("START_PLAYING encoded mysteryReveals=%zu bytes=%zu", state.mReveals.size(), b.Size());
}

bool Board::NetApplyStartPlaying(NetBuffer& b)
{
	if (const auto* context = LawnFusion::GetNativeContext(this))
	{
		NetBuffer candidate = b, nativeBody;
		LawnFusion::NativeSnapshotFrame frame;
		if (!LawnFusion::DecodeNativeSnapshot(candidate, context->eligible, context->epoch, frame, nativeBody) ||
			frame.selection.phase != LawnFusion::FusionBankPhase::Playing || !frame.selectionCommitted ||
			!NetPreflightNativeFusionBody(this,nativeBody, frame)) return false;
		bool applied = false;
		NetApplySnapshot(b, &applied);
		if (!applied) return false;
		NetBuffer camera;
		camera.WriteI16(mNetGameplayCameraTargets.mBoardX); camera.WriteI16(mNetGameplayCameraTargets.mBoardY);
		camera.WriteI16(mNetGameplayCameraTargets.mTreeX); camera.WriteI16(mNetGameplayCameraTargets.mPoleX);
		if (!NetApplyCameraBootstrap(camera)) return false;
		mCutScene->mSeedChoosing = false;
		NetTrace("native Fusion START_PLAYING applied epoch=%u packets=%zu revealAtomic=1", frame.epoch, frame.packets.size());
		return true;
	}
	const size_t start = b.mReadPos;
	const NetMysteryLimits limits = {(uint8_t)NUM_SEEDS_IN_CHOOSER, (uint8_t)SEED_IMITATER, (uint8_t)SEED_GATLINGPEA};
	NetMysteryStart state;
	if (!NetDecodeMysteryStart(b, state, limits) || !mSeedBank ||
		mSeedBank->mNumPackets < 0 || mSeedBank->mNumPackets > SEEDBANK_CAPACITY)
	{ b.mReadPos = start; NetTrace("START_PLAYING rejected malformed reveal/camera"); return false; }
	std::vector<uint16_t> pending;
	for (int i = 0; i < mSeedBank->mNumPackets; ++i)
		if (mSeedBank->mSeedPackets[i].IsMysteryPending()) pending.push_back(mSeedBank->mSeedPackets[i].mNetId);
	if (!NetMysteryRevealIdsMatch(pending, state.mReveals))
	{ b.mReadPos = start; NetTrace("START_PLAYING rejected pending identity set pending=%zu reveals=%zu", pending.size(), state.mReveals.size()); return false; }
	std::vector<SeedPacket*> packets;
	SeedChooserScreen* chooser = mApp->mSeedChooserScreen;
	for (const NetMysteryReveal& reveal : state.mReveals)
	{
		SeedPacket* packet = nullptr;
		for (int i = 0; i < mSeedBank->mNumPackets; ++i)
			if (mSeedBank->mSeedPackets[i].mNetId == reveal.mId)
			{
				if (packet) { b.mReadPos = start; return false; }
				packet = &mSeedBank->mSeedPackets[i];
			}
		const auto available = [&](SeedType type)
		{
			return mApp->SeedTypeAvailable(type) && (!chooser ||
				(!chooser->SeedNotAllowedToPick(type) && !chooser->SeedNotAllowedDuringTrial(type)));
		};
		if (!packet || packet->mRandomDiscountLevel > 10 || packet->mNetOwnerSlot >= NET_MAX_PLAYERS ||
			!available((SeedType)reveal.mType) ||
			(reveal.mType == (uint8_t)SEED_IMITATER && !available((SeedType)reveal.mImitater)))
		{ b.mReadPos = start; NetTrace("START_PLAYING rejected unavailable reveal id=%u type=%u", reveal.mId, reveal.mType); return false; }
		for (int i = 0; i < mSeedBank->mNumPackets; ++i)
		{
			const SeedPacket& other = mSeedBank->mSeedPackets[i];
			if (other.mNetOwnerSlot == packet->mNetOwnerSlot && !other.IsMysteryPending() && (uint8_t)other.mPacketType == reveal.mType)
			{ b.mReadPos = start; NetTrace("START_PLAYING rejected own fixed-card duplicate id=%u", reveal.mId); return false; }
		}
		for (size_t i = 0; i < packets.size(); ++i)
			if (packets[i]->mNetOwnerSlot == packet->mNetOwnerSlot && state.mReveals[i].mType == reveal.mType)
			{ b.mReadPos = start; NetTrace("START_PLAYING rejected own mystery duplicate id=%u", reveal.mId); return false; }
		packets.push_back(packet);
	}
	NetBuffer camera;
	camera.WriteI16(state.mBoardX); camera.WriteI16(state.mBoardY); camera.WriteI16(state.mTreeX); camera.WriteI16(state.mPoleX);
	if (!NetApplyCameraBootstrap(camera)) { b.mReadPos = start; return false; }
	for (size_t i = 0; i < packets.size(); ++i)
	{
		SeedPacket& packet = *packets[i];
		const uint8_t origin = packet.mRandomDiscountLevel;
		packet.SetPacketType((SeedType)state.mReveals[i].mType, state.mReveals[i].mImitater == 255 ? SEED_NONE : (SeedType)state.mReveals[i].mImitater);
		packet.mRandomDiscountLevel = origin;
	}
	NetTrace("START_PLAYING applied mysteryReveals=%zu identitiesPreserved=1", state.mReveals.size());
	return true;
}

void Board::NetEncodeIntroState(NetBuffer& b)
{
	const int aWideX = mApp->mDDInterface ? mApp->mDDInterface->mWideScreenOffsetX : 0;
	const int aWideY = mApp->mDDInterface ? mApp->mDDInterface->mWideScreenOffsetY : 0;
	SeedChooserScreen* aChooser = mApp->mSeedChooserScreen;

	b.WriteU32(mApp->mNetBoardSeed);
	b.WriteI32(mCutScene->mCutsceneTime);
	b.WriteI16((int16_t)mX);
	b.WriteI16((int16_t)mY);
	b.WriteI16((int16_t)mTreeX);
	b.WriteI16((int16_t)mPoleX);
	b.WriteI16((int16_t)(aChooser ? aChooser->mX - aWideX : 0));
	b.WriteI16((int16_t)(aChooser ? aChooser->mY - aWideY : 0));
	b.WriteI16((int16_t)(aChooser && aChooser->mMenuButton ? aChooser->mMenuButton->mY - aWideY : 0));
	b.WriteI16((int16_t)mSeedBank->mX);
	b.WriteI16((int16_t)mSeedBank->mY);
	b.WriteU16((uint16_t)max(0, min(1000, mSodPosition)));
	b.WriteI16((int16_t)(mFogOffset * 2.0f));
	b.WriteU16((uint16_t)min(65535, max(0, mFogBlownCountDown)));
	b.WriteU8((uint8_t)max(0, min(255, mSeedBank->mCutSceneDarken)));

	uint8_t aFlags = 0;
	if (mCutScene->mSeedChoosing) aFlags |= 1;
	if (mMenuButton && mMenuButton->mBtnNoDraw) aFlags |= 2;
	if (aChooser && aChooser->mMenuButton && aChooser->mMenuButton->mBtnNoDraw) aFlags |= 4;
	if (aChooser && aChooser->mMenuButton && aChooser->mMenuButton->mDisabled) aFlags |= 8;
	if (mEnableGraveStones) aFlags |= 16;
	if (mShowShovel) aFlags |= 32;
	if (aChooser && aChooser->mMouseVisible) aFlags |= 64;
	b.WriteU8(aFlags);

	// Use the same canonical 12-byte presentation payload as the playing-scene
	// DAVE_STATE message; INTRO_STATE applies it only after validating all data.
	NetEncodeDaveState(b);

	uint16_t aCount = 0;
	NetBuffer aPreview;
	Zombie* aZombie = nullptr;
	while (mZombies.IterateNext(aZombie))
	{
		if (aZombie->mDead || aZombie->mFromWave != Zombie::ZOMBIE_WAVE_CUTSCENE)
			continue;
		if (aZombie->mNetId == 0)
			aZombie->mNetId = NetAllocId();
		if (aZombie->mNetId == 0 || aCount == 0xFFFF)
			continue;
		aPreview.WriteU16(aZombie->mNetId);
		aPreview.WriteU8((uint8_t)aZombie->mZombieType);
		aPreview.WriteU8((uint8_t)max(0, min(MAX_GRID_SIZE_Y - 1, aZombie->mRow)));
		aPreview.WriteI16((int16_t)(aZombie->mPosX * 4.0f));
		aPreview.WriteI16((int16_t)(aZombie->mPosY * 4.0f));
		aPreview.WriteI32(aZombie->mRenderOrder);
		aPreview.WriteU8(aZombie->mVariant ? 1 : 0);
		aCount++;
	}
	b.WriteU16(aCount);
	b.WriteBytes(aPreview.Data(), aPreview.Size());
}

void Board::NetApplyIntroState(NetBuffer& b)
{
	static const size_t INTRO_FIXED_BYTES = 48;
	static const size_t INTRO_ZOMBIE_BYTES = 13;
	if (b.Remaining() < INTRO_FIXED_BYTES)
	{
		NetTrace("INTRO_STATE rejected: truncated fixed payload bytes=%zu", b.Remaining());
		return;
	}

	uint32_t aBoardSeed = b.ReadU32Or();
	int aCutsceneTime = b.ReadI32Or();
	int aBoardX = b.ReadI16Or();
	int aBoardY = b.ReadI16Or();
	int aTreeX = b.ReadI16Or();
	int aPoleX = b.ReadI16Or();
	int aChooserX = b.ReadI16Or();
	int aChooserY = b.ReadI16Or();
	int aChooserMenuY = b.ReadI16Or();
	int aSeedBankX = b.ReadI16Or();
	int aSeedBankY = b.ReadI16Or();
	int aSodPosition = b.ReadU16Or();
	int aFogOffset2 = b.ReadI16Or();
	int aFogCountdown = b.ReadU16Or();
	int aSeedBankDarken = b.ReadU8Or();
	uint8_t aFlags = b.ReadU8Or();
	NetDaveWireState aDaveState = {};
	const bool aDaveValid = NetReadDaveWireState(b, aDaveState);
	uint16_t aZombieCount = b.ReadU16Or();

	if (aBoardSeed != mApp->mNetBoardSeed || aSodPosition > 1000 || (aFlags & ~127) != 0 ||
		!aDaveValid ||
		b.Remaining() != (size_t)aZombieCount * INTRO_ZOMBIE_BYTES)
	{
		NetTrace("INTRO_STATE rejected: seed=%u expected=%u sod=%d flags=%u dave=(%u,%u,%d,%u,%d,%d) zombies=%u remaining=%zu",
			aBoardSeed, mApp->mNetBoardSeed, aSodPosition, aFlags,
			aDaveState.mState, aDaveState.mAnimKey, aDaveState.mMessageIndex,
			aDaveState.mAnimTimeQ, aDaveState.mX, aDaveState.mY,
			aZombieCount, b.Remaining());
		return;
	}

	struct PreviewZombie
	{
		uint16_t mId;
		uint8_t mType;
		uint8_t mRow;
		int16_t mX4;
		int16_t mY4;
		int32_t mRenderOrder;
		bool mVariant;
	};
	std::vector<PreviewZombie> aPreview;
	aPreview.reserve(aZombieCount);
	std::map<unsigned short, bool> aSeen;
	for (uint16_t i = 0; i < aZombieCount; i++)
	{
		PreviewZombie aWire = {};
		aWire.mId = b.ReadU16Or();
		aWire.mType = b.ReadU8Or();
		aWire.mRow = b.ReadU8Or();
		aWire.mX4 = b.ReadI16Or();
		aWire.mY4 = b.ReadI16Or();
		aWire.mRenderOrder = b.ReadI32Or();
		aWire.mVariant = b.ReadU8Or() != 0;
		if (aWire.mId == 0 || aWire.mType >= (uint8_t)ZombieType::NUM_ZOMBIE_TYPES ||
			aWire.mRow >= MAX_GRID_SIZE_Y || aSeen.find(aWire.mId) != aSeen.end())
		{
			NetTrace("INTRO_STATE rejected zombie id=%u type=%u row=%u", aWire.mId, aWire.mType, aWire.mRow);
			return;
		}
		aSeen[aWire.mId] = true;
		aPreview.push_back(aWire);
	}

	// All bytes and preview entities have now been validated.  From this point
	// on mutations are presentation-only; the client never advances Dave's
	// dialog state machine or consumes gameplay RNG.
	NetApplyDavePresentation(this, mApp, aDaveState);

	const bool aFirstIntroSample = !mNetIntroTargets.mValid;
	mNetIntroTargets.mBoardX = aBoardX;
	mNetIntroTargets.mBoardY = aBoardY;
	mNetIntroTargets.mTreeX = aTreeX;
	mNetIntroTargets.mPoleX = aPoleX;
	mNetIntroTargets.mChooserX = aChooserX;
	mNetIntroTargets.mChooserY = aChooserY;
	mNetIntroTargets.mChooserMenuY = aChooserMenuY;
	mNetIntroTargets.mSeedBankX = aSeedBankX;
	mNetIntroTargets.mSeedBankY = aSeedBankY;
	mNetIntroTargets.mSodPosition = aSodPosition;
	mNetIntroTargets.mFogOffset2 = aFogOffset2;
	mNetIntroTargets.mSeedBankDarken = aSeedBankDarken;
	mNetIntroTargets.mValid = true;

	mCutScene->mCutsceneTime = max(0, aCutsceneTime);
	mFogBlownCountDown = aFogCountdown;
	NetUpdateIntroPresentation(aFirstIntroSample);
	mEnableGraveStones = (aFlags & 16) != 0;
	mShowShovel = (aFlags & 32) != 0;
	if (mMenuButton)
		mMenuButton->mBtnNoDraw = (aFlags & 2) != 0;

	SeedChooserScreen* aChooser = mApp->mSeedChooserScreen;
	if (aChooser)
	{
		aChooser->mMouseVisible = (aFlags & 64) != 0;
		if (aChooser->mMenuButton)
		{
			aChooser->mMenuButton->mBtnNoDraw = (aFlags & 4) != 0;
			aChooser->mMenuButton->mDisabled = (aFlags & 8) != 0;
		}
		bool aSeedChoosing = (aFlags & 1) != 0;
		if (aSeedChoosing && !mCutScene->mSeedChoosing)
			mCutScene->StartSeedChooser();
		else if (!aSeedChoosing && mCutScene->mSeedChoosing)
		{
			mCutScene->mSeedChoosing = false;
			aChooser->mMouseVisible = false;
			mApp->mWidgetManager->SetFocus(this);
		}
	}

	for (const PreviewZombie& aWire : aPreview)
	{
		Zombie* aZombie = NetGetGhostZombie(aWire.mId);
		if (!aZombie)
		{
			gNetSuppressSounds = true;
			aZombie = AddZombieInRow((ZombieType)aWire.mType, aWire.mRow, Zombie::ZOMBIE_WAVE_CUTSCENE);
			gNetSuppressSounds = false;
			if (!aZombie)
				continue;
			aZombie->mNetId = aWire.mId;
			aZombie->mNetGhost = true;
			aZombie->mNetAnimKey = 255;
		}
		aZombie->mFromWave = Zombie::ZOMBIE_WAVE_CUTSCENE;
		aZombie->mRow = aWire.mRow;
		aZombie->mVariant = aWire.mVariant;
		aZombie->mPosX = aWire.mX4 / 4.0f;
		aZombie->mPosY = aWire.mY4 / 4.0f;
		aZombie->mNetTargetX = aZombie->mPosX;
		aZombie->mNetTargetY = aZombie->mPosY;
		aZombie->mX = (int)aZombie->mPosX;
		aZombie->mY = (int)aZombie->mPosY;
		aZombie->mRenderOrder = aWire.mRenderOrder;
	}

	Zombie* aZombie = nullptr;
	while (mZombies.IterateNext(aZombie))
	{
		if (!aZombie->mDead && aZombie->mFromWave == Zombie::ZOMBIE_WAVE_CUTSCENE &&
			(!aZombie->mNetGhost || aSeen.find(aZombie->mNetId) == aSeen.end()))
		{
			NetRetireZombiePresentation(mApp, aZombie);
		}
	}
	mCutScene->mPlacedZombies = true;
}

void Board::NetClientTick()
{
	// Drain and apply incoming messages first (events then snapshots in order)
	NetDrainMessages();
	if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO)
		NetUpdateIntroPresentation(false);
	else if (mNetGameplayCameraTargets.mValid &&
		(mApp->mGameScene == GameScenes::SCENE_PLAYING ||
		 mApp->mGameScene == GameScenes::SCENE_ZOMBIES_WON))
	{
		Move(NetApproachPresentationInt(mX, mNetGameplayCameraTargets.mBoardX),
			NetApproachPresentationInt(mY, mNetGameplayCameraTargets.mBoardY));
		mTreeX = NetApproachPresentationInt(mTreeX, mNetGameplayCameraTargets.mTreeX);
		mPoleX = NetApproachPresentationInt(mPoleX, mNetGameplayCameraTargets.mPoleX);
	}

	// ghost micro-updates (interpolation, cosmetic counters)
	Zombie* aZombie = nullptr;
	while (mZombies.IterateNext(aZombie))
	{
		if (aZombie->mDead)
			continue;
		if (aZombie->mNetGhost)
		{
			NetGhostUpdateZombie(aZombie);
		}
		else if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO)
		{
			// Preview zombies are created locally by the stock cutscene.  They do
			// not simulate on a client, but their attached reanimations must tick.
			AttachmentUpdateAndMove(aZombie->mAttachmentID, aZombie->mPosX, aZombie->mPosY);
			aZombie->UpdateReanim();
		}
	}

	Plant* aPlant = nullptr;
	while (mPlants.IterateNext(aPlant))
	{
		if (!aPlant->mDead && aPlant->mIsOnBoard)
		{
			if (aPlant->mNetGhost && !mPaused)
			{
				float aDeltaX = aPlant->mNetTargetX - (float)aPlant->mX;
				float aDeltaY = aPlant->mNetTargetY - (float)aPlant->mY;
				if (fabsf(aDeltaX) <= 1.0f)
					aPlant->mX = (int)aPlant->mNetTargetX;
				else
				{
					int aStepX = (int)(aDeltaX * 0.35f);
					aPlant->mX += aStepX != 0 ? aStepX : (aDeltaX > 0.0f ? 1 : -1);
				}
				if (fabsf(aDeltaY) <= 1.0f)
					aPlant->mY = (int)aPlant->mNetTargetY;
				else
				{
					int aStepY = (int)(aDeltaY * 0.35f);
					aPlant->mY += aStepY != 0 ? aStepY : (aDeltaY > 0.0f ? 1 : -1);
				}
			}
			aPlant->Animate();
			aPlant->UpdateReanim();
		}
	}
	NetGhostUpdateOthers();
	if (!mPaused && mFwooshCountDown > 0)
	{
		UpdateFwoosh();
		--mFwooshCountDown;
	}

	// Host Challenge::Update advances the Slot Machine's attachment-based handle.
	// Clients do not run that gameplay update, so tick this presentation only.
	if (!mPaused && mChallenge && mApp->IsSlotMachineLevel())
	{
		Reanimation* aChallengeReanim =
			mApp->ReanimationTryToGet(mChallenge->mReanimChallenge);
		if (aChallengeReanim && aChallengeReanim->mIsAttachment)
			aChallengeReanim->Update();
	}

	// fade counters run locally for visuals
	if (mBoardFadeOutCounter > 0)
		mBoardFadeOutCounter--;
	if (mShakeCounter > 0)
		mShakeCounter--;
	if (mIceTrapCounter > 0)
		mIceTrapCounter--;
	if (mCoinBankFadeCount > 0 && mApp->GetDialog(Dialogs::DIALOG_PURCHASE_PACKET_SLOT) == nullptr)
		mCoinBankFadeCount--;
}

// ==========================================================================
// Remote cursor rendering (board scene)
// ==========================================================================

void Board::NetDrawRemoteCursors(Graphics* g)
{
	NetManager* aNet = mApp->mNetManager;
	if (!aNet || !aNet->IsActive())
		return;

	for (int i = 0; i < NET_MAX_PLAYERS; i++)
	{
		if (i == aNet->GetMySlot() || !aNet->mRemoteCursors[i].mValid)
			continue;
		const NetRemoteCursor& aCursor = aNet->mRemoteCursors[i];
		const NetPlayerInfo& aPlayer = aNet->GetPlayer(i);
		if (!aPlayer.mActive)
			continue;
		int aCursorX = (int)aCursor.mRenderX;
		int aCursorY = (int)aCursor.mRenderY;

		NetColor aColor = GetRelativePlayerColor(aNet->GetMySlot(), i);

		const bool aHoldingPlant =
			aCursor.mHeldSeed < (uint8_t)SeedType::NUM_SEED_TYPES &&
			aCursor.mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_BANK;
		// Match CursorObject::Draw exactly: the cursor coordinates are the widget
		// origin, while every plant has its own height offset.  Tint the actual
		// silhouette instead of placing an unrelated rectangle over it.
		if (aHoldingPlant)
		{
			SeedType aSeedType = (SeedType)aCursor.mHeldSeed;
			SeedType anImitater = aCursor.mHeldImitater != 255 ? (SeedType)aCursor.mHeldImitater : SeedType::SEED_NONE;
                        float anOffsetX = -35.0f;
                        float anOffsetY = PlantDrawHeightOffset(this, nullptr, aSeedType, -1, -1) - 60.0f;
#ifdef _HAS_BLOOM_AND_DOOM_CONTENTS
                        if (aSeedType == SeedType::SEED_STINGER)
                                anOffsetY -= 60.0f;
                        else
#endif
                        if (Plant::IsFlying(aSeedType) || aSeedType == SeedType::SEED_GRAVEBUSTER)
                                anOffsetY += 30.0f;
                        if (mApp->IsIZombieLevel())
                        {
                                anOffsetX -= 55.0f;
                                anOffsetY -= 70.0f;
                                if (aSeedType == SeedType::SEED_ZOMBIE_BUNGEE)
                                        anOffsetY -= 600.0f;
                                else if (aSeedType == SeedType::SEED_ZOMBONI)
                                {
                                        anOffsetX += 10.0f;
                                        anOffsetY -= 25.0f;
                                }
                                else if (aSeedType == SeedType::SEED_ZOMBIE_POLEVAULTER)
                                        anOffsetX -= 50.0f;
                        }
			Graphics aTinted(*g);
			aTinted.SetColorizeImages(true);
			aTinted.SetColor(Color(aColor.r, aColor.g, aColor.b, 150));
			Plant::DrawSeedType(&aTinted, aSeedType, anImitater, VARIATION_NORMAL,
				(float)aCursorX + anOffsetX, (float)aCursorY + anOffsetY);
		}

		// cursor pointer marker in player color
		if (!aHoldingPlant)
		{
			g->SetColor(Color(aColor.r, aColor.g, aColor.b, 255));
			g->FillRect(aCursorX, aCursorY, 9, 9);
		}

		// nick + ping label in small font
		if (aPlayer.mNick[0])
		{
			SexyString aLabel = StringToSexyString(aPlayer.mNick) + _S(" ") + StrFormat(_S("%ums"), aPlayer.mPing);
			int aLabelX = aCursorX + 14;
			int aLabelY = aCursorY + 10;
			g->SetColor(Color(0, 0, 0, 180));
			g->DrawString(aLabel, aLabelX + 1, aLabelY + 1);
			g->SetColor(Color(aColor.r, aColor.g, aColor.b, 255));
			g->DrawString(aLabel, aLabelX, aLabelY);
		}
	}
}

// ==========================================================================
// Tower defense road rendering
// ==========================================================================

void Board::NetDrawTowerDefensePath(Graphics* g)
{
	if (!g || !mApp || mTDPath.empty() ||
		mApp->mGameMode != GameMode::GAMEMODE_COOP_TOWER_DEFENSE)
		return;
	if (!Sexy::IMAGE_BACKGROUND1UNSODDED)
	{
		static bool aMissingRoadImageTraced = false;
		if (!aMissingRoadImageTraced)
		{
			aMissingRoadImageTraced = true;
			NetTrace("TD road draw skipped: DelayLoad_BackgroundUnsodded did not provide IMAGE_BACKGROUND1UNSODDED");
		}
		return;
	}
	if ((mTDPath.size() & 1U) != 0U)
	{
		NetTrace("TD road draw skipped: malformed odd path byteCount=%zu", mTDPath.size());
		return;
	}
	const int anImageWidth = Sexy::IMAGE_BACKGROUND1UNSODDED->GetWidth();
	const int anImageHeight = Sexy::IMAGE_BACKGROUND1UNSODDED->GetHeight();
	if (anImageWidth <= 0 || anImageHeight <= 0)
	{
		NetTrace("TD road draw skipped: invalid image dimensions image=%dx%d cells=%zu",
			anImageWidth, anImageHeight, mTDPath.size() / 2);
		return;
	}

	// cut the matching cell out of the unsodded (dirt) lawn image, aligned the
	// same way DrawBackdrop places the regular background
	const int aBgX = -BOARD_OFFSET + WIDESCREEN_OFFSETX;
	const int aBgY = WIDESCREEN_OFFSETY;

	// Validate the complete draw first.  A malformed late cell must not leave a
	// partially painted road in the current frame.
	for (size_t i = 0; i + 1 < mTDPath.size(); i += 2)
	{
		const int aRow = mTDPath[i] >> 1;
		const int aCol = mTDPath[i + 1];
		if ((mTDPath[i] & 1U) != 0U || aRow < 0 || aRow >= 5 ||
			aCol < 0 || aCol >= MAX_GRID_SIZE_X)
		{
			NetTrace("TD road draw skipped: invalid cell=%zu rowByte=%u row=%d col=%d",
				i / 2, (unsigned)mTDPath[i], aRow, aCol);
			return;
		}
		const int aX = GridToPixelX(aCol, aRow);
		const int aY = GridToPixelY(aCol, aRow);

		const Rect aSrcRect(aX - aBgX, aY - aBgY, 80, 100);
		if (aSrcRect.mX < 0 || aSrcRect.mY < 0 ||
			aSrcRect.mX + aSrcRect.mWidth > anImageWidth ||
			aSrcRect.mY + aSrcRect.mHeight > anImageHeight)
		{
			NetTrace("TD road draw skipped: source out of bounds cell=%zu rect=(%d,%d,%d,%d) image=%dx%d",
				i / 2, aSrcRect.mX, aSrcRect.mY, aSrcRect.mWidth, aSrcRect.mHeight,
				anImageWidth, anImageHeight);
			return;
		}
	}

	const size_t aCellCount = mTDPath.size() / 2;
	for (size_t i = 0; i < mTDPath.size(); i += 2)
	{
		const int aRow = mTDPath[i] >> 1;
		const int aCol = mTDPath[i + 1];
		const int aX = GridToPixelX(aCol, aRow);
		const int aY = GridToPixelY(aCol, aRow);
		const Rect aSrcRect(aX - aBgX, aY - aBgY, 80, 100);
		g->DrawImage(Sexy::IMAGE_BACKGROUND1UNSODDED, aX, aY, aSrcRect);
	}

	// A path packet or a non-null image alone does not prove that the road made
	// it through the renderer.  Emit this only after every cell was drawn.
	if (!mNetTDRoadDrawLogged)
	{
		mNetTDRoadDrawLogged = true;
		const char* aRole = mApp->NetIsHost() ? "host" :
			(mApp->NetIsClient() ? "client" : "solo");
		if (mApp->CoopSmokeActive())
		{
			NetTrace("SMOKE_TD_ROAD_RENDER_COMPLETE role=%s board=%p cells=%zu image=%dx%d",
				aRole, (void*)this, aCellCount, anImageWidth, anImageHeight);
		}
		else
		{
			NetTrace("TD_ROAD_DRAW_COMPLETE role=%s board=%p cells=%zu image=%dx%d",
				aRole, (void*)this, aCellCount, anImageWidth, anImageHeight);
		}
	}
}

// ==========================================================================
// Tower defense + PvP gameplay helpers
// ==========================================================================

bool Board::IsTowerDefense()
{
	return mApp->mGameMode == GameMode::GAMEMODE_COOP_TOWER_DEFENSE;
}

bool Board::IsPvPMode()
{
	return mApp->mGameMode == GameMode::GAMEMODE_COOP_PVP;
}

bool Board::IsSeedPacketVisibleToSlot(int theIndex, int theSlot) const
{
	if (!mSeedBank || theIndex < 0 || theIndex >= mSeedBank->mNumPackets)
		return false;
	if (mApp->mGameMode != GameMode::GAMEMODE_COOP_PVP || !mApp->mNetManager)
		return true;
	if (theSlot < 0 || theSlot >= LawnNet::NET_MAX_PLAYERS)
		return false;
	const NetPlayerInfo& aPlayer = mApp->mNetManager->GetPlayer(theSlot);
	if (!aPlayer.mActive)
		return false;
	const bool aZombiePacket = mChallenge->IsZombieSeedType(mSeedBank->mSeedPackets[theIndex].mPacketType);
	return aZombiePacket == (aPlayer.mTeam == LawnNet::NETTEAM_ZOMBIES);
}

bool Board::CanUsePlantTools(int theSlot) const
{
	if (mApp->mGameMode != GameMode::GAMEMODE_COOP_PVP)
		return true;
	if (!mApp->mNetManager || theSlot < 0 || theSlot >= LawnNet::NET_MAX_PLAYERS)
		return false;
	const NetPlayerInfo& aPlayer = mApp->mNetManager->GetPlayer(theSlot);
	return aPlayer.mActive && !aPlayer.mSpectator && aPlayer.mTeam == LawnNet::NETTEAM_PLANTS;
}

void Board::AssignPvPSeedOwners()
{
	if (!IsPvPMode() || !mSeedBank || !mApp->mNetManager)
		return;

	int aPlantSlots[LawnNet::NET_MAX_PLAYERS];
	int aPlantCount = 0;
	for (int aSlot = 0; aSlot < LawnNet::NET_MAX_PLAYERS; aSlot++)
	{
		const LawnNet::NetPlayerInfo& aPlayer = mApp->mNetManager->GetPlayer(aSlot);
		if (!aPlayer.mActive || aPlayer.mSpectator)
			continue;
		if (aPlayer.mTeam == LawnNet::NETTEAM_PLANTS)
			aPlantSlots[aPlantCount++] = aSlot;
	}

	// PreNewGame rejects an incomplete PvP lobby.  Keep safe fallbacks for
	// malformed/replayed state so every packet still has a valid owner.
	if (aPlantCount == 0)
		aPlantSlots[aPlantCount++] = 0;

	int aPlantRoundRobin = 0;
	for (int i = 0; i < mSeedBank->mNumPackets; i++)
	{
		SeedPacket& aPacket = mSeedBank->mSeedPackets[i];
		if (aPacket.mPacketType == SeedType::SEED_NONE)
			continue;
		if (mChallenge->IsZombieSeedType(aPacket.mPacketType))
		{
			// Every zombie player can use every zombie card.  Brains and cooldowns
			// live on this one host-authoritative shared packet.
			aPacket.mNetOwnerSlot = LawnNet::NET_SHARED_OWNER;
			continue;
		}

		int anOwner = aPacket.mNetOwnerSlot;
		bool anOwnerIsPlant = anOwner >= 0 && anOwner < LawnNet::NET_MAX_PLAYERS &&
			mApp->mNetManager->GetPlayer(anOwner).mActive &&
			!mApp->mNetManager->GetPlayer(anOwner).mSpectator &&
			mApp->mNetManager->GetPlayer(anOwner).mTeam == LawnNet::NETTEAM_PLANTS;
		if (!anOwnerIsPlant)
			aPacket.mNetOwnerSlot = (unsigned char)aPlantSlots[aPlantRoundRobin++ % aPlantCount];
	}
}

bool Board::CanTakeBrains(int theAmount)
{
	return mNetPvPBrains >= theAmount;
}

void Board::TakeBrains(int theAmount)
{
	mNetPvPBrains = max(0, mNetPvPBrains - theAmount);
}

// host-side per-tick updates for the custom modes
void Board::UpdateTowerDefenseGame()
{
	if (!IsTowerDefense() || mApp->NetIsClient())
		return;

	// TD path zombies are driven by Zombie::UpdateTowerDefense
}

void Board::UpdatePvP()
{
	if (!IsPvPMode() || mApp->NetIsClient() || mPaused || mBoardFadeOutCounter >= 0 ||
		mApp->mGameScene != GameScenes::SCENE_PLAYING)
		return;
	UpdatePvPBalance(mNetPvPBalance, mNetPvPKillScore, mNetPvPBrainScore, GetModeDifficulty(),
		!mPaused && !mLevelComplete && !mLevelAwardSpawned && mNetPvPKillScore < 100 && mNetPvPBrainScore < 5);

	// Brains trickle: one basic zombie every ~10 seconds, shared by the team.
	mNetPvPBrainTick++;
	if (mNetPvPBrainTick >= 1000)
	{
		mNetPvPBrainTick = 0;
		mNetPvPBrains += 25;
	}

	// win checks
	if (mNetPvPBrainScore >= 5)
	{
		ZombiesWon(nullptr);
	}
	else if (mNetPvPKillScore >= 100)
	{
		mApp->mBoardResult = BoardResult::BOARDRESULT_WON;
		mLevelAwardSpawned = true;
		FadeOutLevel();
	}
}

// Deaths can happen before or after the zombie update loop. Count at the death
// edge so instant/projectile kills cannot disappear between iterator passes.
void Board::NetPvPZombieKilled(Zombie* theZombie)
{
	if (!IsPvPMode() || mApp->NetIsClient() || !theZombie || theZombie->mNetKillCounted ||
		!theZombie->IsOnBoard() || mApp->mGameScene != GameScenes::SCENE_PLAYING ||
		mBoardFadeOutCounter >= 0 || mLevelComplete || mNetPvPKillScore >= 100 || mNetPvPBrainScore >= 5)
		return;
	theZombie->mNetKillCounted = true;
	++mNetPvPKillScore;
}

// A zombie crossed into the house: brain for the zombie team.
void Board::NetPvPZombieScored(Zombie* theZombie)
{
	if (!IsPvPMode() || mApp->NetIsClient() || !theZombie || theZombie->mBoard != this ||
		theZombie->mNetKillCounted || theZombie->IsDeadOrDying() || !theZombie->IsOnBoard() ||
		!theZombie->mHasHead || theZombie->IsWalkingBackwards() ||
		mApp->mGameScene != GameScenes::SCENE_PLAYING || mBoardFadeOutCounter >= 0 ||
		mLevelComplete || mNetPvPKillScore >= 100 || mNetPvPBrainScore >= 5)
		return;
	mNetPvPBrainScore++;
	// Reaching the house is a score for the invading team, not a plant kill.
	theZombie->mNetKillCounted = true;
	theZombie->DieNoLoot();
	mApp->PlaySample(Sexy::SOUND_CHOMP);
}

// PvP score display
void Board::NetDrawPvPScore(Graphics* g)
{
	if (!IsPvPMode())
		return;

	g->PushState();
	g->SetColor(Color(45, 32, 20, 220));
	g->FillRect(230, BOARD_HEIGHT - 26, 340, 26);
	SexyString aScoreStr = StrFormat(_S("BRAINS %d/5    KILLS %d/100"), mNetPvPBrainScore, mNetPvPKillScore);
	TodDrawString(g, aScoreStr, BOARD_WIDTH / 2, BOARD_HEIGHT - 8, Sexy::FONT_BRIANNETOD12,
		Color(255, 240, 200), DrawStringJustification::DS_ALIGN_CENTER);
	g->PopState();
}

// ==========================================================================
// Zombie: tower defense path steering (the stock walking code supplies speed)
// ==========================================================================

void Zombie::UpdateTowerDefense(float theDistanceWalked)
{
	Board* aBoard = mBoard;
	// Bungees pick a target cell and move vertically through their own state
	// machine.  They must never inherit the road entry/off-screen projection.
	if (!aBoard || mZombieType == ZombieType::ZOMBIE_BUNGEE)
		return;

	int aCells = (int)(aBoard->mTDPath.size() / 2);
	if (aCells < 2 || mDead)
		return;

	auto GetRoadAnchor = [&](int theRow, int theCol, float& theX, float& theY)
	{
		theX = (float)aBoard->GridToPixelX(theCol, theRow) - 25.0f;
		theY = aBoard->GetPosYBasedOnRow(theX + 40.0f, theRow) - 30.0f;
		if (mZombieType == ZombieType::ZOMBIE_BALLOON)
			theY -= 30.0f;
		else if (mZombieType == ZombieType::ZOMBIE_POGO)
			theY -= 16.0f;
	};

	int aStartRow = aBoard->mTDPath[0] >> 1;
	int aStartCol = aBoard->mTDPath[1];
	float aStartX;
	float aStartY;
	GetRoadAnchor(aStartRow, aStartCol, aStartX, aStartY);

	// Spawned zombies walk in from off-screen exactly as in the original game,
	// then enter the first road cell without a teleport.
	if (mTdPathPos <= 0.0f && mPosX > aStartX)
	{
		if (mRow != aStartRow)
			SetRow(aStartRow);
		mPosY = aStartY;
		return;
	}

	mTdPathPos = max(0.0f, mTdPathPos + theDistanceWalked);

	float aDistanceLeft = mTdPathPos;
	for (int anIndex = 0; anIndex < aCells - 1; anIndex++)
	{
		int aRowA = aBoard->mTDPath[anIndex * 2] >> 1;
		int aColA = aBoard->mTDPath[anIndex * 2 + 1];
		int aRowB = aBoard->mTDPath[(anIndex + 1) * 2] >> 1;
		int aColB = aBoard->mTDPath[(anIndex + 1) * 2 + 1];

		float aXA;
		float aYA;
		float aXB;
		float aYB;
		GetRoadAnchor(aRowA, aColA, aXA, aYA);
		GetRoadAnchor(aRowB, aColB, aXB, aYB);
		float aDeltaX = aXB - aXA;
		float aDeltaY = aYB - aYA;
		float aSegmentLength = sqrtf(aDeltaX * aDeltaX + aDeltaY * aDeltaY);

		if (aDistanceLeft <= aSegmentLength)
		{
			float aT = aSegmentLength > 0.0f ? aDistanceLeft / aSegmentLength : 0.0f;
			mPosX = aXA + aDeltaX * aT;
			mPosY = aYA + aDeltaY * aT;
			int aProjectedRow = aT < 0.5f ? aRowA : aRowB;
			if (mRow != aProjectedRow)
				SetRow(aProjectedRow);
			return;
		}

		aDistanceLeft -= aSegmentLength;
	}

	// The original walking distance has reached the end of the complete road.
	aBoard->mNetTDLives--;
	mApp->PlaySample(Sexy::SOUND_LOSEMUSIC);
	if (aBoard->mNetTDLives <= 0)
		aBoard->ZombiesWon(nullptr);
	DieNoLoot();
}
