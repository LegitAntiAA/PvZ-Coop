#include "FusionNativeWireTests.h"
#include "../../Lawn/Fusion/FusionNativeWire.h"
#include "../../Lawn/Fusion/FusionProducerRules.h"
#include "../../Lawn/Fusion/FusionRollingActors269Rules.h"
#include "../../Lawn/Fusion/FusionSplitShooterRules.h"
#include "../../Lawn/Fusion/FusionFireRules.h"
#include "../../Lawn/Fusion/FusionDeathRules.h"
#include "../../Lawn/Fusion/FusionChomperRules.h"
#include "../../Lawn/Fusion/FusionLobberRules.h"
#include "../../Lawn/Fusion/FusionMoneyRules.h"
#include "../../Lawn/Fusion/FusionPoisonRules.h"
#include "../../Lawn/Fusion/FusionEconomyRules.h"
#include "../../Lawn/Fusion/FusionNativeProjectileMetadata.h"
#include "../../Lawn/Fusion/FusionNativeDayZombie273.h"
#include "../../Lawn/Fusion/FusionNativeEquipment268.h"
#include "../../Lawn/Fusion/FusionChomper268Rules.h"
#include "../../Lawn/Fusion/FusionShulk268Rules.h"
#include "../../Lawn/Fusion/FusionExtras268Rules.h"
#include "../../Lawn/Net/NetCommon.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>
#include <source_location>

namespace
{
FusionNativeWireTestResult result;
// These offset fixtures have empty Star/Doom/Companion/World/Card/Rolling/Fert
// vectors. FNS23 adds a16-byte Board singleton and two U16 counts to FNS22.
// FNS28 suffix: six U16 empty graph/water/heavy/extended/obstacle/captured
// vectors, sourcePoints F32, ultimateCount I32, punishTime F32.
constexpr size_t empty313TailBytes=6*2+4+4+4+4;
constexpr size_t empty269FamilyCountBytes=10*2+20+2+2+2+2+empty313TailBytes;
constexpr size_t emptyNight291CountBytes=4+empty313TailBytes; // Jackson/Jackbox counts +313 suffix after Day.
constexpr size_t emptyJackbox306CountBytes=2+empty313TailBytes;
constexpr size_t disabledAdventureSessionBytes=1; // FNS26, after temporaryLightLevel and before Garlic.
void Check(bool condition, int line)
{
    ++result.mChecks;
    if (!condition) { ++result.mFailures; std::printf("    Native Fusion wire check failed line=%d\n", line); }
}
#define CHECK(condition) Check(!!(condition), __LINE__)

const std::vector<LawnFusion::FusionBankEligibleCard> eligible = {
    {0, false, 100, 7.5f}, {0, true, 200, 7.5f}, {1, false, 50, 7.5f}, {1, true, 100, 7.5f}};

LawnFusion::NativeSnapshotFrame Example()
{
    using namespace LawnFusion;
    FusionSeedBank bank;
    bank.Open(19, 1, eligible, 3);
    bank.Choose(19, 0, 0, false); bank.Choose(19, 0, 1, false);
    bank.Choose(19, 1, 0, true); bank.Choose(19, 1, 1, true);
    bank.SetReady(19, 0, true); bank.SetReady(19, 1, true);
    bank.CommitIntro(19); bank.BeginPlaying(19);
    NativeSnapshotFrame frame;
    frame.epoch = 19; frame.selectionCommitted = true; frame.selection = bank.State();
    for (const auto& selected : frame.selection.packets)
    {
        NativePacketData data;
        data.selectionId = selected.id; data.kind = selected.kind; data.isExtra = selected.isExtra;
        data.owner = selected.owner; data.randomOrigin = selected.randomOrigin;
        data.price = selected.price; data.rechargeSeconds = selected.rechargeSeconds;
        frame.packets.push_back({(uint16_t)(selected.id + 10), data});
    }
    NativePlantData plant;plant.instanceToken=0x10001u;
    plant.kind = 1000; plant.firstParent = 0; plant.secondParent = 1;
    plant.packetId = 13; plant.owner = 1; plant.productionTicks = 2500;
    plant.idleSeconds = 2.5f; plant.shootSeconds = 0.2f;
    frame.plants.push_back({21, plant});
    NativeProjectileData projectile; projectile.kind = 8; projectile.damage = 100; projectile.sunOnHit = 5;
    frame.projectiles.push_back({31, projectile}); frame.coins.push_back({41, 5});
    return frame;
}

void RejectAtomically(LawnNet::NetBuffer forged,std::source_location caller=std::source_location::current())
{
    // Semantic rejection must still hold with a modified opaque native body
    // and its correct checksum, and at a nonzero input cursor.
    const size_t metadata = forged.mData[6] | (static_cast<size_t>(forged.mData[7]) << 8);
    const size_t payload = 14 + metadata;
    forged.mData[payload] ^= 0x40;
    const auto hash = LawnNet::NetFnv1a32(forged.Data()+payload,forged.Size()-payload);
    for (int byte=0;byte<4;++byte) forged.mData[10+byte]=static_cast<uint8_t>(hash>>(byte*8));
    LawnNet::NetBuffer prefixed; prefixed.WriteU8(0x77); prefixed.WriteBytes(forged.Data(),forged.Size()); prefixed.mReadPos=1;
    const auto input=prefixed.mData;
    auto previous=Example(); previous.epoch=777; previous.money=321;
    previous.temporaryLightLevel=444;previous.background=BACKGROUND_3_POOL;
    previous.zombieIdentities={{701,0xdeadbeef}};previous.plants[0].data.waterTargetId=702;previous.plants[0].data.waterTargetToken=0xcafebabe;
    LawnFusion::NativeEquipmentData equipment;equipment.instanceToken=33;previous.equipmentCoins={{703,equipment}};
    previous.wheatPlants269={{704,{true,31.5f}}};
    LawnFusion::NativeDayZombie273State day;day.status=3;day.shadowEnabled=false;
    previous.dayZombies273={{705,day}};
    previous.nightZombies281={{706,{}}};previous.nightZombies281[0].data.lookBack=false;
    previous.zombieActions.push_back({700,{42,2.0f,true,true,6,1234,37}});
    previous.garlicZombies.push_back({701,{LawnFusion::GarlicActionPhase::Waiting,true,44,0,32,-1,-1,0,0}});
    LawnNet::NetBuffer native; native.WriteU8(0x99);
    const bool accepted=LawnFusion::DecodeNativeSnapshot(prefixed,eligible,19,previous,native);
    if(accepted)std::printf("    Unexpected accepted malformed vector caller=%u\n",caller.line());
    CHECK(!accepted);
    CHECK(prefixed.mReadPos==1 && prefixed.mData==input && previous.epoch==777 && previous.money==321 &&
        previous.coins.size()==1 && previous.coins[0].data.value==5 && !previous.coins[0].data.money &&
        previous.projectiles[0].data.kind==8 && !previous.projectiles[0].data.isZombieBullet &&
        !previous.projectiles[0].data.hitLatched && !previous.projectiles[0].data.umbrellaed &&
        previous.background==BACKGROUND_3_POOL && previous.zombieActions.size()==1 && previous.zombieActions[0].id==700 &&
        previous.zombieActions[0].data.colorMask==42 && previous.zombieActions[0].data.scale==2.0f &&
        previous.zombieActions[0].data.doomOnDeath && previous.zombieActions[0].data.jalaedOnDeath &&
        previous.zombieActions[0].data.poisonLevel==6 && previous.zombieActions[0].data.poisonTicks==1234 &&
        previous.zombieActions[0].data.poisonDamageTicks==37 && previous.temporaryLightLevel==444 &&
        previous.plants[0].data.batchShotCount==0 && previous.plants[0].data.batchRageTicks==0 &&
        previous.plants[0].data.batchBagCount==0 && previous.plants[0].data.batchMode==0 &&
        previous.garlicZombies.size()==1 && previous.garlicZombies[0].id==701 && previous.garlicZombies[0].data.waitTicks==44 &&
        previous.garlicZombies[0].data.faceTicks==32 && previous.zombieIdentities.size()==1&&previous.zombieIdentities[0].data==0xdeadbeef&&
        previous.plants[0].data.waterTargetId==702&&previous.plants[0].data.waterTargetToken==0xcafebabe&&
        previous.wheatPlants269.size()==1&&previous.wheatPlants269[0].id==704&&previous.wheatPlants269[0].data.isFromWheat&&previous.wheatPlants269[0].data.wheatTime==31.5f&&
        previous.dayZombies273.size()==1&&previous.dayZombies273[0].id==705&&previous.dayZombies273[0].data.status==3&&
        !previous.dayZombies273[0].data.shadowEnabled&&previous.dayZombies273[0].data.shootCountdown==1.5f&&
        previous.nightZombies281.size()==1&&previous.nightZombies281[0].id==706&&!previous.nightZombies281[0].data.lookBack&&
        previous.equipmentCoins.size()==1&&previous.equipmentCoins[0].id==703&&previous.equipmentCoins[0].data.instanceToken==33&&native.Size()==1 && native.mData[0]==0x99);
}
}

FusionNativeWireTestResult RunFusionNativeWireTests()
{
    using namespace LawnFusion;
    using LawnNet::NetBuffer;
    result = {};
    std::printf("[ RUN      ] Fusion.NativeSnapshotExtensionTransactional\n");
    // Exact native tile intervals. A real Pool-row5 SDL click previously
    // produced row4 because input retained the Day100px divisor.
    constexpr int dayEdges[]={80,180,280,380,480,580};
    constexpr int poolEdges[]={80,165,250,335,420,505,590};
    for(uint8_t bg:{uint8_t(BACKGROUND_1_DAY),uint8_t(BACKGROUND_3_POOL),uint8_t(BACKGROUND_4_FOG)})
    {
        const bool pool=bg!=BACKGROUND_1_DAY;
        const int* edges=pool?poolEdges:dayEdges;
        const int count=pool?6:5;
        for(int row=0;row<count;++row)
            for(int y:{edges[row],(edges[row]+edges[row+1])/2,edges[row+1]-1})
            {
                CHECK(NativeFusionPixelToRow(bg,y)==row);
                CHECK(NativeFusionPixelToRow(bg,y,true)==row);
            }
        for(int y:{(std::numeric_limits<int>::min)(),79})
        {CHECK(NativeFusionPixelToRow(bg,y)==-1);CHECK(NativeFusionPixelToRow(bg,y,true)==0);}
        for(int y:{edges[count],(std::numeric_limits<int>::max)()})
        {CHECK(NativeFusionPixelToRow(bg,y)==-1);CHECK(NativeFusionPixelToRow(bg,y,true)==count-1);}
    }
    CHECK(NativeFusionPixelToRow(BACKGROUND_3_POOL,555)==5);
    CHECK(NativeFusionPoolEntryRight(9)==680&&NativeFusionPoolEntryRight(10)==752);
    CHECK(NativeFusionPixelToRow(BACKGROUND_2_NIGHT,555)==4);
    // A source1181 callback can start Garlic on a native Pool swimmer.
    // Keep Day/swimmer, high-ground, missing-head and MC refusals intact.
    for(uint8_t bg:{uint8_t(BACKGROUND_1_DAY),uint8_t(BACKGROUND_3_POOL),uint8_t(BACKGROUND_4_FOG)})
        for(int row=0;row<6;++row)
        {
            CHECK(NativeGarlicRawFlagsValid(bg,row,8)==(row<NativeFusionRowsForBackground(bg)));
            CHECK(NativeGarlicRawFlagsValid(bg,row,10)==(bg!=BACKGROUND_1_DAY&&(row==2||row==3)));
            CHECK(!NativeGarlicRawFlagsValid(bg,row,0));
            CHECK(!NativeGarlicRawFlagsValid(bg,row,12));
            CHECK(!NativeGarlicRawFlagsValid(bg,row,136));
        }
    CHECK(!NativeGarlicRawFlagsValid(BACKGROUND_2_NIGHT,2,10));
    CHECK(!NativeGarlicRawFlagsValid(BACKGROUND_3_POOL,6,10));
    // Passive267 torches have no shooting timing profile. They still need
    // their own state validator, including902's actual conversion counter.
    for (uint16_t kind : {uint16_t(902),uint16_t(932)})
        for (float speed : {.9f,1.f,1.1f})
        {
            auto torch=Example();auto& data=torch.plants[0].data;
            data.kind=kind;data.firstParent=kind==902?1052:1055;
            data.secondParent=kind==902?1054:1150;
            data.packetId=0;data.productionTicks=data.pendingProductionTicks=0;
            data.shootSeconds=-1;data.animationSpeed=speed;
            data.batchShotCount=kind==902?21:0;
            CHECK(ValidateNativeSnapshotFrame(torch,eligible));
            NetBuffer wire;wire.WriteU32(0x267);
            CHECK(EncodeNativeSnapshot(torch,eligible,wire));
            NativeSnapshotFrame decoded;NetBuffer body;
            CHECK(DecodeNativeSnapshot(wire,eligible,19,decoded,body));
            CHECK(decoded.plants.size()==1&&decoded.plants[0].data.kind==kind&&
                decoded.plants[0].data.batchShotCount==data.batchShotCount&&decoded.plants[0].data.animationSpeed==speed);
            if(kind==932)
            {
                data.batchShotCount=1;NetBuffer untouched;untouched.WriteU32(0x267);
                const auto previous=untouched.mData;
                CHECK(!EncodeNativeSnapshot(torch,eligible,untouched)&&untouched.mData==previous);
            }
        }
    // The native bank stays10 wide while its eligible pool grows beyond10.
    std::vector<FusionBankEligibleCard> largePool;
    for (uint16_t kind=0;kind<8;++kind)
        for (bool extra : {false,true}) largePool.push_back({kind,extra,extra?100:50,7.5f});
    FusionSeedBank nativeBank;
    CHECK(nativeBank.Open(31,1,largePool,3,kNativeFusionBankCapacity));
    CHECK(nativeBank.State().targetCount==10 && nativeBank.State().quotas[0]==5 && nativeBank.State().quotas[1]==5);
    NativeSnapshotFrame nativeChoosing;
    nativeChoosing.epoch=31; nativeChoosing.selection=nativeBank.State();
    CHECK(ValidateNativeSnapshotFrame(nativeChoosing,largePool));
    auto oversizedBank=nativeBank.State();
    ++oversizedBank.revision; oversizedBank.targetCount=14; oversizedBank.quotas[0]=oversizedBank.quotas[1]=7;
    CHECK(FusionSeedBank::ValidateSnapshot(oversizedBank,largePool)); // Source reference permits14.
    CHECK(!nativeBank.ApplySnapshot(oversizedBank));
    CHECK(nativeBank.State().targetCount==10 && nativeBank.State().revision==1);
    nativeChoosing.selection=oversizedBank;
    CHECK(!ValidateNativeSnapshotFrame(nativeChoosing,largePool));
    CHECK(!nativeBank.Open(32,1,largePool,3,0) && nativeBank.State().epoch==31);
    CHECK(!nativeBank.Open(32,1,largePool,3,kFusionBankCapacity+1) && nativeBank.State().epoch==31);
    const auto source = Example();
    CHECK(ValidateNativeSnapshotFrame(source, eligible));
    NetBuffer body; body.WriteU32(0x12345678); body.WriteU8(0x42);
    const auto original = body.mData;
    CHECK(EncodeNativeSnapshot(source, eligible, body));
    CHECK(body.Size() > original.size());
    constexpr std::array<uint8_t,28> empty313Golden={0,0,0,0,0,0,0,0,0,0,0,0,0,0,250,67,0,0,0,0,0,0,0,0,0,0,0,0}; //500.f,0,0,0.f.
    const size_t defaultMetadataEnd=14+body.mData[6]+(size_t(body.mData[7])<<8);
    CHECK(empty313TailBytes==empty313Golden.size()&&std::equal(empty313Golden.begin(),empty313Golden.end(),body.mData.begin()+defaultMetadataEnd-empty313TailBytes));
    NativeSnapshotFrame decoded;
    NetBuffer native;
    NetBuffer wire = body;
    CHECK(DecodeNativeSnapshot(wire, eligible, 19, decoded, native));
    CHECK(wire.Remaining() == 0 && native.mData == original);
    CHECK(decoded.plants.size() == 1 && decoded.plants[0].data.kind == 1000);
    CHECK(decoded.plants[0].data.producerRewardFlags == 0 && decoded.plants[0].data.deathEventFlags == 0 &&
        body.mData[4] == kNativeFusionSnapshotVersion && body.mData[5] == 0 && body.mData[18]==BACKGROUND_1_DAY && body.mData[19]==1);
    CHECK(decoded.packets[0].id != decoded.packets[0].data.selectionId);
    CHECK(decoded.plants[0].data.firstParent == 0 && decoded.plants[0].data.secondParent == 1);
    CHECK(decoded.coins.size() == 1 && decoded.coins[0].data.value == 5 && !decoded.coins[0].data.money);
    CHECK(decoded.money == kFusionMoneyInitial);
    CHECK(!decoded.projectiles[0].data.fireState && !decoded.projectiles[0].data.lastTorchId && !decoded.projectiles[0].data.lastTorchToken &&
        !decoded.projectiles[0].data.isZombieBullet && !decoded.projectiles[0].data.hitLatched && !decoded.projectiles[0].data.umbrellaed);
    NetBuffer roundTrip = native;
    CHECK(EncodeNativeSnapshot(decoded, eligible, roundTrip));
    CHECK(roundTrip.mData == body.mData);

    // Background is authoritative metadata, including the native row geometry.
    for (uint8_t background : {uint8_t(BACKGROUND_1_DAY),uint8_t(BACKGROUND_2_NIGHT),uint8_t(BACKGROUND_3_POOL),uint8_t(BACKGROUND_4_FOG),uint8_t(BACKGROUND_5_ROOF),uint8_t(BACKGROUND_6_BOSS)})
    {
        auto terrain=source;terrain.background=background;
        NetBuffer encoded;encoded.mData=original;
        CHECK(EncodeNativeSnapshot(terrain,eligible,encoded));
        CHECK(encoded.mData[18]==background && encoded.mData[19]==1);
        NativeSnapshotFrame restored;NetBuffer terrainBody;
        CHECK(DecodeNativeSnapshot(encoded,eligible,19,restored,terrainBody));
        CHECK(restored.background==background && NativeFusionRowsForBackground(restored.background)==((background==BACKGROUND_3_POOL||background==BACKGROUND_4_FOG)?6:5));
        CHECK(EncodeNativeSnapshot(restored,eligible,terrainBody) && terrainBody.mData==encoded.mData);
    }
    for (uint8_t background : {uint8_t(6),uint8_t(7),uint8_t(255)})
    {
        auto forged=body;forged.mData[18]=background;RejectAtomically(forged);
        auto invalidTerrain=source;invalidTerrain.background=background;
        NetBuffer preserved;preserved.mData=original;
        CHECK(!EncodeNativeSnapshot(invalidTerrain,eligible,preserved) && preserved.mData==original);
    }
    for (uint8_t invalidBoolean : {uint8_t(2),uint8_t(255)})
    {auto forged=body;forged.mData[19]=invalidBoolean;RejectAtomically(forged);}

    // FNS13 appends source Garlic state after all FNS12 fields. The body
    // remains the only position/HP authority; no client clock is introduced.
    auto garlicFrame = source;
    GarlicActionState waiting{GarlicActionPhase::Waiting,true,100,0,50,-1,-1,0,0};
    GarlicActionState movingDown{GarlicActionPhase::Moving,false,0,99,0,1,2,150,250};
    GarlicActionState movingUp{GarlicActionPhase::Moving,true,0,0,150,4,3,450,350};
    GarlicActionState recovering; recovering.faceTicks=50;
    garlicFrame.garlicZombies={{51,waiting},{52,movingDown},{53,movingUp},{54,recovering},{55,{}}};
    CHECK(NativeGarlicRowY(0)==50 && NativeGarlicRowY(4)==450);
    CHECK(NativeGarlicActionStateValid(waiting) && NativeGarlicActionStateValid(movingDown) && NativeGarlicActionStateValid(movingUp));
    auto impossibleCertain = movingDown;
    impossibleCertain.certainRow = true; impossibleCertain.targetRow = 0; impossibleCertain.targetY = 50;
    CHECK(!NativeGarlicActionStateValid(impossibleCertain));
    CHECK(FusionGarlicLogicalRow(movingUp,4)==4 && FusionGarlicSortingRow(movingUp,4)==3);
    CHECK(FusionGarlicPreventsEating(waiting) && FusionGarlicPreventsEating(movingDown) && !FusionGarlicPreventsEating(recovering));
    NetBuffer garlicWire; garlicWire.mData=original;
    CHECK(EncodeNativeSnapshot(garlicFrame,eligible,garlicWire));
    NativeSnapshotFrame garlicDecoded; NetBuffer garlicBody;
    CHECK(DecodeNativeSnapshot(garlicWire,eligible,19,garlicDecoded,garlicBody));
    CHECK(garlicDecoded.garlicZombies.size()==5 && garlicBody.mData==original);
    for (size_t index=0;index<garlicFrame.garlicZombies.size() && index<garlicDecoded.garlicZombies.size();++index)
    {
        const auto& before=garlicFrame.garlicZombies[index];const auto& after=garlicDecoded.garlicZombies[index];
        CHECK(before.id==after.id && before.data.phase==after.data.phase && before.data.certainRow==after.data.certainRow &&
            before.data.waitTicks==after.data.waitTicks && before.data.moveElapsedTicks==after.data.moveElapsedTicks &&
            before.data.faceTicks==after.data.faceTicks && before.data.moveDurationTicks==after.data.moveDurationTicks && before.data.originRow==after.data.originRow &&
            before.data.targetRow==after.data.targetRow && before.data.startY==after.data.startY && before.data.targetY==after.data.targetY);
    }
    CHECK(EncodeNativeSnapshot(garlicDecoded,eligible,garlicBody) && garlicBody.mData==garlicWire.mData);
    GarlicActionState poolEdge{GarlicActionPhase::Moving,false,0,99,0,5,4,475,390};
    CHECK(NativeGarlicRowY(4,6)==390 && NativeGarlicRowY(5,6)==475 &&
        NativeGarlicActionStateValid(poolEdge,6) && !NativeGarlicActionStateValid(poolEdge,5));
    for (uint8_t background : {uint8_t(BACKGROUND_3_POOL),uint8_t(BACKGROUND_4_FOG)})
    {
        auto pool=source;pool.background=background;pool.garlicZombies={{51,poolEdge}};
        NetBuffer encoded;encoded.mData=original;
        CHECK(EncodeNativeSnapshot(pool,eligible,encoded));
        NativeSnapshotFrame restored;NetBuffer poolBody;
        CHECK(DecodeNativeSnapshot(encoded,eligible,19,restored,poolBody));
        CHECK(restored.background==background && restored.garlicZombies.size()==1 &&
            restored.garlicZombies[0].data.originRow==5 && restored.garlicZombies[0].data.targetRow==4 &&
            restored.garlicZombies[0].data.startY==475 && restored.garlicZombies[0].data.targetY==390);
        CHECK(EncodeNativeSnapshot(restored,eligible,poolBody) && poolBody.mData==encoded.mData);
        auto day=encoded;day.mData[18]=BACKGROUND_1_DAY;RejectAtomically(day);
        const size_t end=14+(encoded.mData[6] | (static_cast<size_t>(encoded.mData[7])<<8));
        const size_t rowRecord=end-empty269FamilyCountBytes-8-22; // Empty source/equipment/269 counts follow this sole Garlic record.
        for (int variant=0;variant<4;++variant)
        {
            auto forged=encoded;
            if(variant==0)forged.mData[rowRecord+12]=6;
            if(variant==1)forged.mData[rowRecord+13]=5;
            const uint32_t wrongY=variant==2?0x44098000u:0x43e10000u; // Day row5 Y550 / row4 Y450.
            if(variant>=2)for(int byte=0;byte<4;++byte)forged.mData[rowRecord+(variant==2?14:18)+byte]=static_cast<uint8_t>(wrongY>>(byte*8));
            RejectAtomically(forged);
        }
    }
    const size_t garlicMetadata=garlicWire.mData[6] | (static_cast<size_t>(garlicWire.mData[7])<<8);
    const size_t baseMetadata=body.mData[6] | (static_cast<size_t>(body.mData[7])<<8);
    constexpr size_t garlicRecordBytes=22; // id2 + phase/bool2 + fourU16 + rows2 + twoF32.
    const size_t garlicCount=14+garlicMetadata-empty269FamilyCountBytes-10-garlicFrame.garlicZombies.size()*garlicRecordBytes;
    const size_t firstGarlic=garlicCount+2;
    CHECK(garlicMetadata==baseMetadata+5*garlicRecordBytes && garlicCount==14+baseMetadata-empty269FamilyCountBytes-10);
    CHECK(std::equal(body.mData.begin()+14,body.mData.begin()+garlicCount,garlicWire.mData.begin()+14));
    CHECK(garlicWire.mData[garlicCount]==5 && garlicWire.mData[garlicCount+1]==0 &&
        garlicWire.mData[firstGarlic]==51 && garlicWire.mData[firstGarlic+2]==1 && garlicWire.mData[firstGarlic+3]==1 &&
        garlicWire.mData[firstGarlic+4]==100 && garlicWire.mData[firstGarlic+10]==100 && garlicWire.mData[firstGarlic+11]==0 &&
        garlicWire.mData[firstGarlic+12]==255 && garlicWire.mData[firstGarlic+13]==255);
    for(uint16_t duration:{uint16_t(1),uint16_t(250),uint16_t(10000)})
    {
        auto timed=source;auto moving=movingDown;moving.moveDurationTicks=duration;moving.moveElapsedTicks=0;
        timed.garlicZombies={{52,moving}};CHECK(NativeGarlicActionStateValid(moving));
        NetBuffer encoded;encoded.mData=original;CHECK(EncodeNativeSnapshot(timed,eligible,encoded));
        NativeSnapshotFrame restored;NetBuffer opaque;CHECK(DecodeNativeSnapshot(encoded,eligible,19,restored,opaque));
        CHECK(restored.garlicZombies.size()==1&&restored.garlicZombies[0].data.moveDurationTicks==duration&&restored.garlicZombies[0].data.moveElapsedTicks==0);
        const size_t end=14+encoded.mData[6]+(size_t(encoded.mData[7])<<8),record=end-empty269FamilyCountBytes-8-garlicRecordBytes;
        CHECK(encoded.mData[record+10]==uint8_t(duration)&&encoded.mData[record+11]==uint8_t(duration>>8)&&encoded.mData[record+12]==1&&encoded.mData[record+13]==2);
    }
    for (int variant=0;variant<23;++variant)
    {
        NetBuffer forged=garlicWire;
        const size_t move=firstGarlic+garlicRecordBytes;
        const auto write32=[&](size_t offset,uint32_t value)
        {for(int byte=0;byte<4;++byte)forged.mData[offset+byte]=static_cast<uint8_t>(value>>(byte*8));};
        if(variant==0)forged.mData[firstGarlic]=0;
        if(variant==1)forged.mData[move]=51;
        if(variant==2){forged.mData[garlicCount]=1;forged.mData[garlicCount+1]=4;} //1025, rejected before allocation.
        if(variant==3)forged.mData[firstGarlic+2]=3;
        if(variant==4)forged.mData[firstGarlic+3]=2;
        if(variant==5)forged.mData[firstGarlic+4]=0;
        if(variant==6)forged.mData[firstGarlic+4]=101;
        if(variant==7)forged.mData[firstGarlic+6]=1;
        if(variant==8)forged.mData[firstGarlic+8]=151;
        if(variant==9)forged.mData[move+6]=100;
        if(variant==10)forged.mData[move+4]=1;
        if(variant==11)forged.mData[move+12]=255;
        if(variant==12)forged.mData[move+13]=5;
        if(variant==13)write32(move+14,0x7fc00000u); // NaN.
        if(variant==14)write32(move+18,0x7f800000u); // Infinity.
        if(variant==15)write32(move+14,0x43170000u); //151 instead of source-normal row1 Y150.
        if(variant==16)write32(move+18,0x437b0000u); //251 instead of row2 Y250.
        if(variant==17)forged.mData[move+13]=1; // Same row is not a Moving action.
        if(variant==18)forged.mData[move+13]=3; // Non-adjacent rows.
        if(variant==19)write32(firstGarlic+14,0x3f800000u); // Waiting has no captured Ys.
        if(variant==20)forged.mData[firstGarlic+4*garlicRecordBytes+3]=1; // Idle cannot retain certainRow.
        if(variant==21)forged.mData[move+10]=forged.mData[move+11]=0; // Duration must be positive.
        if(variant==22){forged.mData[move+10]=0x11;forged.mData[move+11]=0x27;} //10001 exceeds source maximum.
        RejectAtomically(forged);
    }
    for (int variant=0;variant<6;++variant)
    {
        auto invalidGarlic=garlicFrame;
        if(variant==0)invalidGarlic.garlicZombies.push_back(invalidGarlic.garlicZombies[0]);
        if(variant==1)invalidGarlic.garlicZombies[0].id=0;
        if(variant==2)invalidGarlic.garlicZombies[0].data.waitTicks=101;
        if(variant==3)invalidGarlic.garlicZombies[1].data.startY=std::numeric_limits<float>::quiet_NaN();
        if(variant==4)invalidGarlic.garlicZombies[1].data.targetY=350;
        if(variant==5)invalidGarlic.garlicZombies.resize(1025,{60,waiting});
        NetBuffer preserved;preserved.mData=original;const auto bytes=preserved.mData;
        CHECK(!EncodeNativeSnapshot(invalidGarlic,eligible,preserved) && preserved.mData==bytes && preserved.mReadPos==0);
    }
    for (size_t length=0;length<garlicWire.Size();++length)
    {
        NetBuffer cut;cut.WriteU8(0x77);cut.WriteBytes(garlicWire.Data(),length);cut.mReadPos=1;
        const auto input=cut.mData;auto previous=garlicFrame;previous.epoch=777;
        NetBuffer oldBody;oldBody.WriteU8(0x99);
        CHECK(!DecodeNativeSnapshot(cut,eligible,19,previous,oldBody));
        CHECK(cut.mReadPos==1 && cut.mData==input && previous.epoch==777 && previous.garlicZombies.size()==5 &&
            previous.garlicZombies[1].data.moveElapsedTicks==99 && oldBody.Size()==1 && oldBody.mData[0]==0x99);
    }
    auto garlicChoosing=nativeChoosing;
    garlicChoosing.selection=nativeBank.State();garlicChoosing.garlicZombies={{51,waiting}};
    CHECK(!ValidateNativeSnapshotFrame(garlicChoosing,largePool));

    // New base recipients use the shared timeline and canonical native payload
    // identity; unused traits must still reject before reader/world admission.
    for (uint16_t kind : {uint16_t(21),uint16_t(23),uint16_t(29)})
    {
        auto recipient = source;
        auto& actor = recipient.plants[0].data;
        actor.kind = kind; actor.firstParent = actor.secondParent = kFusionNoPlant;
        actor.productionTicks = actor.pendingProductionTicks = 0;
        actor.shootSeconds = kind == 29 ? -1.0f : .2f;
        recipient.projectiles.clear();
        if (kind != 29)
        {
            NativeProjectileData shot; shot.kind = kind == 21 ? 37 : 39;
            shot.damage = 20; shot.specialType = kind == 21 ? 0 : 9;
            recipient.projectiles.push_back({31,shot});
            CHECK(NativeProjectileMetadataValid(shot,kind == 21 ? PROJECTILE_SPIKE : PROJECTILE_STAR,
                kind == 21 ? MOTION_STRAIGHT : MOTION_STAR));
            CHECK(!NativeProjectileMetadataValid(shot,PROJECTILE_PEA,
                kind == 21 ? MOTION_STRAIGHT : MOTION_STAR));
        }
        NetBuffer encoded; encoded.WriteU8(0x44);
        CHECK(EncodeNativeSnapshot(recipient,eligible,encoded));
        NativeSnapshotFrame copy; NetBuffer restored,input = encoded;
        CHECK(DecodeNativeSnapshot(input,eligible,19,copy,restored) && copy.plants[0].data.kind == kind &&
            copy.plants[0].data.firstParent == kFusionNoPlant && copy.plants[0].data.secondParent == kFusionNoPlant &&
            restored.Size() == 1 && restored.mData[0] == 0x44);
        if (kind != 29)
            for (int trait = 0; trait < 5; ++trait)
            {
                auto invalid = recipient; auto& shot = invalid.projectiles[0].data;
                if (trait == 0) shot.specialType = 1;
                if (trait == 1) shot.melonEnhanced = true;
                if (trait == 2) shot.effectVariant = 1;
                if (trait == 3) shot.sunOnHit = 5;
                if (trait == 4) { shot.lastTorchId = 21; shot.lastTorchToken = 0x10001u; }
                NetBuffer preserved; preserved.WriteU8(0x55);
                CHECK(!EncodeNativeSnapshot(invalid,eligible,preserved) && preserved.Size() == 1 && preserved.mData[0] == 0x55);
            }
        else
        {
            for (uint8_t stage : {uint8_t(0),uint8_t(1),uint8_t(2)})
            { recipient.plants[0].data.damageStage = stage; CHECK(ValidateNativeSnapshotFrame(recipient,eligible)); }
            recipient.plants[0].data.kind = 1130;
            recipient.plants[0].data.firstParent = 27; recipient.plants[0].data.secondParent = 29;
            recipient.plants[0].data.damageStage = 0; CHECK(ValidateNativeSnapshotFrame(recipient,eligible));
            recipient.plants[0].data.damageStage = 1; CHECK(!ValidateNativeSnapshotFrame(recipient,eligible));
        }
    }

    // Chomper variants use the same wire fields and cache native phase time.
    for (uint16_t kind : {uint16_t(1012),uint16_t(1014),uint16_t(1016)})
    {
        auto family = source;
        auto& actor = family.plants[0].data;
        actor.kind = kind; actor.firstParent = kind == 1012 ? 3 : kind == 1014 ? 1 : 2;
        actor.secondParent = 5; actor.productionTicks = actor.pendingProductionTicks = 0;
        family.gloveHolds[1] = {family.plants[0].id,actor.instanceToken};
        for (float elapsed : {-1.0f,0.0f,.65f,1.92f,39.99f,40.0f})
            for (float speed : {.9f,1.0f,1.1f})
            {
                actor.shootSeconds = elapsed; actor.animationSpeed = speed;
                actor.damageStage = kind == 1012 ? 2 : 0;
                NetBuffer encoded = native;
                CHECK(EncodeNativeSnapshot(family,eligible,encoded));
                NativeSnapshotFrame copy; NetBuffer restored;
                CHECK(DecodeNativeSnapshot(encoded,eligible,19,copy,restored));
                CHECK(copy.plants[0].data.kind == kind && copy.plants[0].data.shootSeconds == elapsed &&
                    copy.plants[0].data.animationSpeed == speed && copy.plants[0].data.instanceToken == actor.instanceToken &&
                    copy.plants[0].data.damageStage == actor.damageStage);
                CHECK(copy.gloveHolds[1].instanceToken == actor.instanceToken && restored.mData == native.mData);
            }
        for (float invalid : {-.5f,40.01f})
        {
            actor.shootSeconds = invalid;
            CHECK(!ValidateNativeSnapshotFrame(family,eligible));
        }
        actor.shootSeconds = 0; actor.productionTicks = 1;
        CHECK(!ValidateNativeSnapshotFrame(family,eligible));
        actor.productionTicks = 0; actor.pendingProductionTicks = 1;
        CHECK(!ValidateNativeSnapshotFrame(family,eligible));
        actor.pendingProductionTicks = 0; actor.firstParent = 0;
        CHECK(!ValidateNativeSnapshotFrame(family,eligible));
        actor.firstParent = kind == 1012 ? 3 : kind == 1014 ? 1 : 2;
        actor.damageStage = kind == 1012 ? 3 : 1;
        CHECK(!ValidateNativeSnapshotFrame(family,eligible));
    }

    // Lobbers use existing native body motion plus bounded source metadata.
    // Their captured target observations remain host-only, outside this frame.
    struct LobberBinding { uint16_t kind, firstParent, secondParent; };
    for (const auto& binding : {LobberBinding{26,kFusionNoPlant,kFusionNoPlant},
        LobberBinding{32,kFusionNoPlant,kFusionNoPlant}, LobberBinding{1116,32,10},
        LobberBinding{28,kFusionNoPlant,kFusionNoPlant}, LobberBinding{1111,26,1},
        LobberBinding{1113,26,28}, LobberBinding{1121,32,28}, LobberBinding{1124,26,32},
        LobberBinding{1134,31,26}, LobberBinding{1138,31,28}, LobberBinding{1141,31,32}})
    for (bool alternate : {false,true})
    {
        auto lobbers = source;
        const uint16_t kind = binding.kind;
        const auto* profile = FindFusionLobberProfile(kind,alternate);
        if (alternate && !profile) continue;
        CHECK(profile != nullptr);
        if (!profile) continue;
        auto& plant = lobbers.plants[0].data;
        plant.kind = kind; plant.productionTicks = plant.pendingProductionTicks = 0;
        plant.firstParent = binding.firstParent;
        plant.secondParent = binding.secondParent;
        plant.animationSpeed = .9f; plant.shootSeconds = .3f;
        auto& payload = lobbers.projectiles[0].data;
        payload = {}; payload.kind = profile->projectileKind; payload.damage = profile->damage;
        payload.sunOnHit = payload.kind == 53 ? 5 : 0;
        NetBuffer encoded = native;
        CHECK(EncodeNativeSnapshot(lobbers,eligible,encoded));
        NativeSnapshotFrame copy; NetBuffer restored;
        CHECK(DecodeNativeSnapshot(encoded,eligible,19,copy,restored));
        CHECK(copy.plants.size() == 1 && copy.projectiles.size() == 1 && copy.plants[0].data.kind == kind && copy.plants[0].data.shootSeconds == .3f &&
            copy.projectiles[0].data.kind == payload.kind && copy.projectiles[0].data.damage == payload.damage &&
            copy.projectiles[0].data.sunOnHit == payload.sunOnHit);
        CHECK(restored.mData == native.mData && copy.plants.size() == 1 && copy.plants[0].data.instanceToken == plant.instanceToken);
        CHECK(!FusionFireProjectileMetadataValid(payload.kind,payload.damage,payload.sunOnHit,0,0,0,PROJECTILE_PEA));
        for (int field = 0; field < 5; ++field)
        {
            auto invalid = lobbers;
            auto& data = invalid.projectiles[0].data;
            if (field == 0) ++data.damage;
            if (field == 1) data.sunOnHit = payload.sunOnHit == 5 ? 0 : 5;
            if (field == 2) data.specialType = 1;
            if (field == 3) data.fireState = 1;
            if (field == 4) { data.lastTorchId = 21; data.lastTorchToken = plant.instanceToken; }
            NetBuffer untouched = native;
            CHECK(!EncodeNativeSnapshot(invalid,eligible,untouched));
            CHECK(untouched.mData == native.mData && untouched.mReadPos == native.mReadPos);
        }
    }

    // Source producers share the native frame; pure producers carry a copy of
    // their native counter, and only SunNut owns persistent threshold bits.
    auto producers = source;
    for (uint16_t kind : {uint16_t(1), uint16_t(1033), uint16_t(1006)})
    {
        NativePlantData data;
        data.kind = kind; data.owner = LawnNet::NET_SHARED_OWNER;
        data.instanceToken = 0x10002u + static_cast<uint32_t>(producers.plants.size());
        if (kind != 1) { data.firstParent = 1; data.secondParent = kind == 1033 ? 1 : 3; }
        CHECK(TryGetFusionProducerCycleTicks(*FindFusionProducerProfile(kind), 2, data.productionTicks));
        CHECK(data.productionTicks == (kind == 1 ? 2701 : kind == 1033 ? 2201 : 1701));
        data.pendingProductionTicks = 50;
        data.producerRewardFlags = kind == 1006 ? 3 : 0;
        producers.plants.push_back({static_cast<uint16_t>(21 + producers.plants.size()), data});
    }
    NetBuffer producerWire; producerWire.mData = original;
    CHECK(EncodeNativeSnapshot(producers, eligible, producerWire));
    NativeSnapshotFrame producerDecoded; NetBuffer producerBody;
    CHECK(DecodeNativeSnapshot(producerWire, eligible, 19, producerDecoded, producerBody));
    CHECK(producerDecoded.plants.size() == 4 && producerBody.mData == original);
    CHECK(producerDecoded.plants[2].data.kind == 1033 && producerDecoded.plants[2].data.productionTicks == 2201 &&
        producerDecoded.plants[2].data.pendingProductionTicks == 50 && producerDecoded.plants[2].data.producerRewardFlags == 0);
    CHECK(producerDecoded.plants[3].data.kind == 1006 && producerDecoded.plants[3].data.firstParent == 1 &&
        producerDecoded.plants[3].data.secondParent == 3 && producerDecoded.plants[3].data.productionTicks == 1701 &&
        producerDecoded.plants[3].data.producerRewardFlags == 3);
    for (uint8_t flags = 0; flags <= 3; ++flags)
    {
        auto frame = producers; frame.plants[3].data.producerRewardFlags = flags;
        CHECK(ValidateNativeSnapshotFrame(frame, eligible));
    }
    for (size_t i = 0; i < producers.plants.size(); ++i)
    {
        for (int field = 0; field < 3; ++field)
        {
            auto invalidProducer = producers;
            auto& data = invalidProducer.plants[i].data;
            if (field == 0) ++data.productionTicks;
            else if (field == 1) data.pendingProductionTicks = 51;
            else data.producerRewardFlags = data.kind == 1006 ? 4 : 1;
            // The original1000 example is below its maximum, so move this
            // negative explicitly past its source25+2 strict deadline.
            if (field == 0 && i == 0) data.productionTicks = 2702;
            NetBuffer preserved; preserved.mData = original;
            CHECK(!EncodeNativeSnapshot(invalidProducer, eligible, preserved));
            CHECK(preserved.mData == original && preserved.mReadPos == 0);
        }
    }
    auto zeroProducer = producers;
    zeroProducer.plants[1].data.productionTicks = zeroProducer.plants[1].data.pendingProductionTicks = 0;
    CHECK(ValidateNativeSnapshotFrame(zeroProducer, eligible));
    for (uint16_t kind : {uint16_t(31),uint16_t(1140)})
    {
        auto moneyProducer=source;
        auto& data=moneyProducer.plants[0].data;
        data.kind=kind; data.firstParent=data.secondParent=kind==31?kFusionNoPlant:31;
        data.productionTicks=FusionProducerMaximumTicks(kind); data.pendingProductionTicks=50;
        data.animationSpeed=1; data.shootSeconds=-1;
        NetBuffer encoded; encoded.mData=original;
        CHECK(data.productionTicks==(kind==31?2700:2200) && EncodeNativeSnapshot(moneyProducer,eligible,encoded));
        NativeSnapshotFrame copy; NetBuffer restored;
        CHECK(DecodeNativeSnapshot(encoded,eligible,19,copy,restored));
        CHECK(copy.plants.size()==1 && copy.plants[0].data.kind==kind &&
            copy.plants[0].data.productionTicks==data.productionTicks && copy.plants[0].data.pendingProductionTicks==50 &&
            copy.money==3000 && restored.mData==original);
        for (int field=0;field<3;++field)
        {
            auto invalid=moneyProducer;
            if (field==0) ++invalid.plants[0].data.productionTicks;
            if (field==1) invalid.plants[0].data.pendingProductionTicks=51;
            if (field==2) invalid.plants[0].data.producerRewardFlags=1;
            NetBuffer untouched; untouched.mData=original;
            CHECK(!EncodeNativeSnapshot(invalid,eligible,untouched) && untouched.mData==original && untouched.mReadPos==0);
        }
    }

    // Producer field order is unchanged: flags follow the twoI32 deadlines. Altered
    // metadata must reject even with a changed opaque body and valid checksum.
    constexpr size_t headerBytes = 14, bankStateBytes = 22, selectedPacketBytes = 21, nativePacketBytes = 17;
    const size_t firstPlant = headerBytes + 6 + bankStateBytes + 2 + source.selection.packets.size()*selectedPacketBytes +
        2 + source.packets.size()*nativePacketBytes + 2;
    constexpr size_t plantBytes = 56, flagsAfterRecordId = 20, deathFlagsAfterRecordId = 21;
    // These live recipes preserve the Puff kind but replace its generation.
    // Refusing the resulting ancestry drops every snapshot until the plant dies.
    for (const uint16_t incoming : {7, 9})
    {
        auto puff = source;
        puff.plants.front().data = NativePlantData{};
        auto& actor = puff.plants.front().data;
        actor.kind = actor.firstParent = 6;
        actor.secondParent = incoming;
        actor.instanceToken = 0x20001;
        actor.owner = 1;
        actor.packetId = 13;
        NetBuffer encoded; encoded.mData = original;
        CHECK(EncodeNativeSnapshot(puff, eligible, encoded));
        NativeSnapshotFrame copy; NetBuffer restored;
        const bool decodedOk = DecodeNativeSnapshot(encoded, eligible, puff.epoch, copy, restored);
        CHECK(decodedOk);
        if (decodedOk)
            CHECK(copy.plants.size() == 1 && copy.plants.front().data.kind == 6 &&
                copy.plants.front().data.firstParent == 6 && copy.plants.front().data.secondParent == incoming &&
                copy.plants.front().data.instanceToken == actor.instanceToken && restored.mData == original);
        std::swap(actor.firstParent, actor.secondParent);
        CHECK(!ValidateNativeSnapshotFrame(puff, eligible));
    }

    // FNS15 carries the batch state on the same native Plant generation. Use
    // actual directional ancestry for the entire40, including standalone elite
    // forms with no incoming source recipe (their acquisition is separate).
    const uint16_t batchAncestry[][3] = {
        {1036,6,10},{1037,7,10},{1038,9,10},{1040,10,11},{1023,7,8},
        {1042,9,11},{1043,7,11},{1044,6,11},{1045,8,11},{1046,7,1040},
        {1171,4,9},{1054,13,16},{1020,kFusionNoPlant,kFusionNoPlant},
        {1152,6,1020},{1107,0,1105},{1109,1168,0},{1161,kFusionNoPlant,kFusionNoPlant},
        {1165,10,1168},{1167,16,1109},{1168,1109,0},{1096,kFusionNoPlant,kFusionNoPlant},
        {1183,6,23},{1191,11,21},{929,908,1},{1192,5,11},
        {20,kFusionNoPlant,kFusionNoPlant},{22,kFusionNoPlant,kFusionNoPlant},
        {1091,24,22},{1052,16,1053},{1053,16,18},{1055,14,18},{1059,13,18},
        {1061,17,18},{1062,16,17},{1063,13,17},{1064,14,17},{1173,10,16},
        {1080,20,22},{1179,2,16},{1182,10,18}
    };
    for (const auto& ancestry : batchAncestry)
    {
        auto batch=source;auto& actor=batch.plants[0].data;
        actor.kind=ancestry[0];actor.firstParent=ancestry[1];actor.secondParent=ancestry[2];
        actor.productionTicks=actor.pendingProductionTicks=0;actor.shootSeconds=-1;
        const float fireDuration=FusionFireActionMaximumDuration(actor.kind,actor.animationSpeed);
        if(fireDuration>0)
        {
            actor.shootSeconds=fireDuration-.01f;CHECK(ValidateNativeSnapshotFrame(batch,eligible));
            actor.shootSeconds=fireDuration;CHECK(!ValidateNativeSnapshotFrame(batch,eligible));
            actor.shootSeconds=-1;
        }
        if(actor.kind==1042)actor.batchShotCount=14;
        if(actor.kind==1109||actor.kind==1167)actor.batchShotCount=5;
        if(actor.kind==1161){actor.batchShotCount=0x7fffffffu;actor.batchMode=6;}
        if(actor.kind==1165||actor.kind==1168){actor.batchRageTicks=500;actor.batchMode=1;}
        if(actor.kind==1191){actor.batchBagCount=20;actor.batchMode=1;}
        NetBuffer encoded;encoded.mData=original;
        const bool encodedOk=EncodeNativeSnapshot(batch,eligible,encoded);CHECK(encodedOk);
        if(!encodedOk)continue;
        CHECK(encoded.mData[firstPlant+39]==static_cast<uint8_t>(actor.batchShotCount) &&
            encoded.mData[firstPlant+43]==static_cast<uint8_t>(actor.batchRageTicks) &&
            encoded.mData[firstPlant+45]==actor.batchBagCount && encoded.mData[firstPlant+47]==actor.batchMode);
        NativeSnapshotFrame copy;NetBuffer restored;
        const bool decodedOk=DecodeNativeSnapshot(encoded,eligible,19,copy,restored);CHECK(decodedOk);
        if(decodedOk)
        {
            CHECK(copy.plants.size()==1 && copy.plants[0].data.kind==actor.kind &&
                copy.plants[0].data.firstParent==actor.firstParent && copy.plants[0].data.secondParent==actor.secondParent &&
                copy.plants[0].data.instanceToken==actor.instanceToken &&
                copy.plants[0].data.batchShotCount==actor.batchShotCount && copy.plants[0].data.batchRageTicks==actor.batchRageTicks &&
                copy.plants[0].data.batchBagCount==actor.batchBagCount && copy.plants[0].data.batchMode==actor.batchMode && restored.mData==original);
            CHECK(EncodeNativeSnapshot(copy,eligible,restored) && restored.mData==encoded.mData);
        }
        for(int field=0;field<4;++field)
        {
            NetBuffer forged=encoded;
            const size_t offset=firstPlant+(field==0?39:field==1?43:field==2?45:47);
            uint32_t value=1;
            if(field==0)value=actor.kind==1042?15:actor.kind==1109||actor.kind==1167?6:actor.kind==1161?0x80000000u:1;
            if(field==1&&(actor.kind==1165||actor.kind==1168))value=501;
            if(field==2&&actor.kind==1191)value=21;
            if(field==3)value=actor.kind==1161?7:actor.kind==1165||actor.kind==1168||actor.kind==1191||actor.kind==1171?2:1;
            const int width=field==0?4:field==3?1:2;
            for(int byte=0;byte<width;++byte)forged.mData[offset+byte]=static_cast<uint8_t>(value>>(byte*8));
            RejectAtomically(forged);
        }
    }
    // Source1171 visual readiness is a host-counter mirror, not a ghost clock.
    for(uint8_t mode : {uint8_t(0),uint8_t(1)})
    {
        auto potato=source;auto& actor=potato.plants[0].data;
        actor.kind=1171;actor.firstParent=4;actor.secondParent=9;
        actor.productionTicks=actor.pendingProductionTicks=0;actor.shootSeconds=-1;actor.batchMode=mode;
        NetBuffer encoded;encoded.mData=original;CHECK(EncodeNativeSnapshot(potato,eligible,encoded));
        NativeSnapshotFrame copy;NetBuffer restored;
        CHECK(DecodeNativeSnapshot(encoded,eligible,19,copy,restored) && copy.plants[0].data.batchMode==mode && restored.mData==original);
    }
    // Batch266 retains source directional recipes, typed Money upgrades and
    // shared source actors separately. These are codec cases, not acquisition
    // or gameplay acceptance for the corresponding plants.
    const uint16_t batch266Ancestry[][3]={
        {1117,28,29},{1118,26,29},{1119,29,32},{1149,16,32},{1126,26,1121},
        {1079,20,21},{1081,20,23},{1159,kFusionNoPlant,kFusionNoPlant},{1160,kFusionNoPlant,kFusionNoPlant},
        {1011,0,5},{1135,1134,kFusionNoPlant},{1005,2,1001},{901,1005,1017},{907,11,1032},{933,904,21},
        {1002,1,2},{1009,1,4},{1015,4,5},{1060,17,1060},{1070,7,1070},{1071,1070,16},
        {1072,1037,1070},{1074,1060,10},{1075,1060,16},{1087,24,20},{1145,29,31},
        {1146,1145,kFusionNoPlant},{1163,4,16},{1170,6,4},{1190,24,4},
        {1057,kFusionNoPlant,kFusionNoPlant},{1155,kFusionNoPlant,kFusionNoPlant},
        {1048,3,17},{1189,3,10},{1039,1027,10},{1031,6,1},{1184,27,1},{1136,27,31},
        {1137,1136,kFusionNoPlant},{1188,1187,kFusionNoPlant}
    };
    CHECK(sizeof(batch266Ancestry)/sizeof(batch266Ancestry[0])==40);
    for(const auto& ancestry:batch266Ancestry)
    {
        auto frame=source;auto& actor=frame.plants[0].data;
        actor.kind=ancestry[0];actor.firstParent=ancestry[1];actor.secondParent=ancestry[2];
        actor.productionTicks=actor.pendingProductionTicks=0;actor.shootSeconds=-1;
        if(actor.firstParent==kFusionNoPlant){actor.owner=LawnNet::NET_SHARED_OWNER;actor.packetId=0;}
        if(actor.kind==1015)actor.batchMode=2;
        if(actor.kind==907)actor.batchShotCount=15;
        if(actor.kind==933||actor.kind==1170||actor.kind==1031)actor.batchMode=1;
        if(actor.kind==1188)
        {
            actor.batchShotCount=kFusionGoldSunflowerMaximumCharges;actor.batchBagCount=0;actor.batchMode=1;
            actor.productionTicks=kFusionGoldSunflowerCycleTicks;actor.pendingProductionTicks=10;
        }
        NetBuffer encoded;encoded.mData=original;
        const bool encodedOk=EncodeNativeSnapshot(frame,eligible,encoded);CHECK(encodedOk);
        if(!encodedOk)continue;
        NativeSnapshotFrame copy;NetBuffer restored;
        const bool decodedOk=DecodeNativeSnapshot(encoded,eligible,19,copy,restored);CHECK(decodedOk);
        if(decodedOk)
        {
            const auto& actual=copy.plants[0].data;
            CHECK(actual.kind==actor.kind&&actual.firstParent==actor.firstParent&&actual.secondParent==actor.secondParent&&
                actual.owner==actor.owner&&actual.packetId==actor.packetId&&actual.instanceToken==actor.instanceToken&&
                actual.batchShotCount==actor.batchShotCount&&actual.batchRageTicks==actor.batchRageTicks&&
                actual.batchBagCount==actor.batchBagCount&&actual.batchMode==actor.batchMode&&
                actual.productionTicks==actor.productionTicks&&actual.pendingProductionTicks==actor.pendingProductionTicks&&restored.mData==original);
            CHECK(EncodeNativeSnapshot(copy,eligible,restored)&&restored.mData==encoded.mData);
        }
        if(actor.kind==1188)
        {
            CHECK(actor.batchMode==1&&actor.pendingProductionTicks==10);
            for(int pending:{0,1,10})
            {
                auto gold=frame;gold.plants[0].data.batchMode=pending?1:0;
                gold.plants[0].data.pendingProductionTicks=pending;
                NetBuffer positive;positive.mData=original;CHECK(EncodeNativeSnapshot(gold,eligible,positive));
                NativeSnapshotFrame retained;NetBuffer nativeBody;
                CHECK(DecodeNativeSnapshot(positive,eligible,19,retained,nativeBody)&&
                    retained.plants[0].data.batchMode==(pending?1:0)&&
                    retained.plants[0].data.pendingProductionTicks==pending&&nativeBody.mData==original);
            }
            for(int mismatch=0;mismatch<2;++mismatch)
            {
                auto invalid=frame;
                if(mismatch==0)invalid.plants[0].data.batchMode=0;
                else invalid.plants[0].data.pendingProductionTicks=0;
                CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
                NetBuffer preserved;preserved.mData=original;
                CHECK(!EncodeNativeSnapshot(invalid,eligible,preserved)&&preserved.mData==original&&!preserved.mReadPos);
                auto forged=encoded;
                if(mismatch==0)forged.mData[firstPlant+47]=0;
                else for(int byte=0;byte<4;++byte)forged.mData[firstPlant+16+byte]=0;
                RejectAtomically(forged);
            }
        }
        for(int field=0;field<5;++field)
        {
            auto forged=encoded;
            const size_t offset=firstPlant+(field==0?39:field==1?43:field==2?45:field==3?47:4);
            const uint32_t value=field==0?(actor.kind==1188?201:actor.kind==907?16:1):field==1?1:
                field==2?1:field==3?(actor.kind==1015?3:actor.kind==933||actor.kind==1170||actor.kind==1031||actor.kind==1188||actor.kind==1135?2:1):65534;
            const int width=field==0?4:field==3?1:2;
            for(int byte=0;byte<width;++byte)forged.mData[offset+byte]=static_cast<uint8_t>(value>>(byte*8));
            RejectAtomically(forged);
        }
        if(actor.firstParent==kFusionNoPlant)
        {
            auto invalid=frame;invalid.plants[0].data.owner=1;invalid.plants[0].data.packetId=13;
            NetBuffer unchanged;unchanged.mData=original;
            CHECK(!EncodeNativeSnapshot(invalid,eligible,unchanged)&&unchanged.mData==original);
        }
        if(IsFusionMoneyUpgradeAncestry(actor.firstParent,actor.secondParent,actor.kind))
        {
            auto invalid=frame;invalid.plants[0].data.secondParent=0;
            NetBuffer unchanged;unchanged.mData=original;
            CHECK(!EncodeNativeSnapshot(invalid,eligible,unchanged)&&unchanged.mData==original);
        }
    }
    // All7 BigGatling visual modes and the two coupled native Rage states
    // survive snapshots; malformed combinations must never partially commit.
    for(uint8_t mode=0;mode<=6;++mode)
    {
        auto batch=source;auto& actor=batch.plants[0].data;
        actor.kind=1161;actor.firstParent=actor.secondParent=kFusionNoPlant;
        actor.productionTicks=actor.pendingProductionTicks=0;actor.shootSeconds=-1;actor.batchMode=mode;
        NetBuffer encoded;encoded.mData=original;CHECK(EncodeNativeSnapshot(batch,eligible,encoded));
        NativeSnapshotFrame copy;NetBuffer restored;
        CHECK(DecodeNativeSnapshot(encoded,eligible,19,copy,restored) && copy.plants[0].data.batchMode==mode && restored.mData==original);
    }
    for(uint16_t kind : {uint16_t(1165),uint16_t(1168)})
    {
        auto batch=source;auto& actor=batch.plants[0].data;actor.kind=kind;
        actor.firstParent=kind==1165?10:1109;actor.secondParent=kind==1165?1168:0;
        actor.productionTicks=actor.pendingProductionTicks=0;actor.shootSeconds=-1;
        CHECK(ValidateNativeSnapshotFrame(batch,eligible));
        actor.batchRageTicks=1;CHECK(!ValidateNativeSnapshotFrame(batch,eligible));
        actor.batchMode=1;CHECK(ValidateNativeSnapshotFrame(batch,eligible));
        actor.batchRageTicks=0;CHECK(!ValidateNativeSnapshotFrame(batch,eligible));
    }
    const size_t sunNutFlags = firstPlant + 3*plantBytes + flagsAfterRecordId;
    CHECK(sunNutFlags < producerWire.Size() && producerWire.mData[sunNutFlags] == 3);
    NativeSnapshotFrame preservedFrame; preservedFrame.epoch = 777;
    NetBuffer preservedBody; preservedBody.WriteU8(0x99);
    for (int field = 0; field < 4; ++field)
    {
        NetBuffer forgedProducer = producerWire; forgedProducer.mReadPos = 0;
        if (field == 0) forgedProducer.mData[sunNutFlags] = 4;
        else if (field == 1) forgedProducer.mData[firstPlant+flagsAfterRecordId] = 1;
        else
        {
            const size_t offset = firstPlant + plantBytes + (field == 2 ? 12 : 16);
            const uint32_t value = field == 2 ? 2702 : 51;
            for (int byte = 0; byte < 4; ++byte) forgedProducer.mData[offset+byte] = static_cast<uint8_t>(value >> (byte*8));
        }
        const size_t metadataBytes = forgedProducer.mData[6] | (static_cast<size_t>(forgedProducer.mData[7]) << 8);
        const size_t opaqueBody = headerBytes + metadataBytes;
        forgedProducer.mData[opaqueBody] ^= 0x80;
        const auto changedHash = LawnNet::NetFnv1a32(forgedProducer.Data()+opaqueBody, forgedProducer.Size()-opaqueBody);
        for (int byte = 0; byte < 4; ++byte) forgedProducer.mData[10+byte] = static_cast<uint8_t>(changedHash >> (byte*8));
        CHECK(!DecodeNativeSnapshot(forgedProducer, eligible, 19, preservedFrame, preservedBody));
        CHECK(forgedProducer.mReadPos == 0 && preservedFrame.epoch == 777 && preservedBody.Size() == 1 && preservedBody.mData[0] == 0x99);
    }
    for (uint16_t version : {uint16_t(2), uint16_t(3), uint16_t(4), uint16_t(5), uint16_t(6), uint16_t(7), uint16_t(8), uint16_t(9), uint16_t(10), uint16_t(11), uint16_t(12), uint16_t(13), uint16_t(14), uint16_t(15), uint16_t(16), uint16_t(17), uint16_t(18), uint16_t(19), uint16_t(20), uint16_t(21), uint16_t(22),uint16_t(23),uint16_t(24),uint16_t(25),uint16_t(26),uint16_t(27),uint16_t(28),uint16_t(29),uint16_t(kNativeFusionSnapshotVersion+1)})
    {
        NetBuffer wrongVersion = producerWire; wrongVersion.mReadPos = 0;
        wrongVersion.mData[4] = static_cast<uint8_t>(version); wrongVersion.mData[5] = 0;
        CHECK(!DecodeNativeSnapshot(wrongVersion, eligible, 19, preservedFrame, preservedBody));
        CHECK(wrongVersion.mReadPos == 0 && preservedFrame.epoch == 777 && preservedBody.mData[0] == 0x99);
    }

    // Death bits belong to a real plant identity, not its cell or glove owner.
    // The opaque body markers below are not a complete Board snapshot: this
    // verifies codec retention across body changes, not native MovePlant.
    auto deaths = source;
    struct DeathPlant { uint16_t kind, first, second; };
    for (const auto entry : {DeathPlant{1003,2,3}, DeathPlant{1010,3,4}, DeathPlant{1162,2,4}})
    {
        NativePlantData data;
        data.kind = entry.kind; data.firstParent = entry.first; data.secondParent = entry.second;
        data.owner = 1; data.packetId = 13;
        data.instanceToken = 0x20001u + static_cast<uint32_t>(deaths.plants.size());
        deaths.plants.push_back({static_cast<uint16_t>(21 + deaths.plants.size()), data});
    }
    constexpr size_t potatoNutIndex = 2;
    const auto potatoNutId = deaths.plants[potatoNutIndex].id;
    const auto potatoNutToken = deaths.plants[potatoNutIndex].data.instanceToken;
    deaths.gloveHolds[0] = {potatoNutId, potatoNutToken}; // Cross-owner hold is allowed.
    deaths.gloveCooldownTicks = 333;
    NetBuffer deathWire;
    for (uint8_t flags = 0; flags <= 3; ++flags)
    {
        deaths.plants[potatoNutIndex].data.deathEventFlags = flags;
        CHECK(ValidateNativeSnapshotFrame(deaths, eligible));
        NetBuffer deathBody; deathBody.mData = original;
        deathBody.WriteI32(724); deathBody.WriteI32(330 + flags);
        const auto expectedBody = deathBody.mData;
        CHECK(EncodeNativeSnapshot(deaths, eligible, deathBody));
        const size_t potatoFlags = firstPlant + potatoNutIndex*plantBytes + deathFlagsAfterRecordId;
        CHECK(deathBody.mData[potatoFlags] == flags && deathBody.mData[potatoFlags-1] == 0);
        deathWire = deathBody;
        NativeSnapshotFrame deathDecoded; NetBuffer decodedDeathBody;
        CHECK(DecodeNativeSnapshot(deathBody, eligible, 19, deathDecoded, decodedDeathBody));
        CHECK(decodedDeathBody.mData == expectedBody && deathDecoded.plants.size() == 4);
        const auto& record = deathDecoded.plants[potatoNutIndex];
        CHECK(record.id == potatoNutId && record.data.instanceToken == potatoNutToken &&
            record.data.kind == 1010 && record.data.firstParent == 3 && record.data.secondParent == 4 &&
            record.data.owner == 1 && record.data.packetId == 13 && record.data.randomOrigin == 0 &&
            record.data.deathEventFlags == flags && record.data.producerRewardFlags == 0);
        CHECK(deathDecoded.gloveHolds[0].plantId == potatoNutId &&
            deathDecoded.gloveHolds[0].instanceToken == potatoNutToken && deathDecoded.gloveCooldownTicks == 333);
        CHECK(deathDecoded.plants[0].data.productionTicks == source.plants[0].data.productionTicks &&
            deathDecoded.plants[0].data.pendingProductionTicks == source.plants[0].data.pendingProductionTicks &&
            deathDecoded.plants[0].data.idleSeconds == source.plants[0].data.idleSeconds &&
            deathDecoded.plants[0].data.shootSeconds == source.plants[0].data.shootSeconds &&
            deathDecoded.plants[1].data.deathEventFlags == 0 && deathDecoded.plants[3].data.deathEventFlags == 0);
        CHECK(EncodeNativeSnapshot(deathDecoded, eligible, decodedDeathBody) && decodedDeathBody.mData == deathWire.mData);
    }
    struct BadDeathFlags { size_t plant; uint8_t flags; };
    for (const auto bad : {BadDeathFlags{potatoNutIndex,4}, BadDeathFlags{potatoNutIndex,128},
        BadDeathFlags{0,1}, BadDeathFlags{1,1}, BadDeathFlags{3,2}})
    {
        auto invalidDeath = deaths;
        invalidDeath.plants[bad.plant].data.deathEventFlags = bad.flags;
        NetBuffer untouched; untouched.mData = original;
        CHECK(!EncodeNativeSnapshot(invalidDeath, eligible, untouched));
        CHECK(untouched.mData == original && untouched.mReadPos == 0);

        NetBuffer forgedDeath = deathWire;
        forgedDeath.mData[firstPlant + bad.plant*plantBytes + deathFlagsAfterRecordId] = bad.flags;
        const size_t metadataBytes = forgedDeath.mData[6] | (static_cast<size_t>(forgedDeath.mData[7]) << 8);
        const size_t opaqueBody = headerBytes + metadataBytes;
        forgedDeath.mData[opaqueBody] ^= 0x80;
        const auto changedHash = LawnNet::NetFnv1a32(forgedDeath.Data()+opaqueBody, forgedDeath.Size()-opaqueBody);
        for (int byte = 0; byte < 4; ++byte) forgedDeath.mData[10+byte] = static_cast<uint8_t>(changedHash >> (byte*8));
        NetBuffer prefixed; prefixed.WriteU8(0xee); prefixed.WriteBytes(forgedDeath.Data(),forgedDeath.Size());
        prefixed.mReadPos = 1;
        auto output = deaths;
        output.epoch = 777; output.gloveCooldownTicks = 777;
        output.plants[potatoNutIndex].data.deathEventFlags = 1;
        NetBuffer outputBody; outputBody.WriteU8(0x99); outputBody.WriteU8(0x88); outputBody.mReadPos = 1;
        CHECK(!DecodeNativeSnapshot(prefixed, eligible, 19, output, outputBody));
        CHECK(prefixed.mReadPos == 1 && output.epoch == 777 && output.gloveCooldownTicks == 777 &&
            output.plants[potatoNutIndex].id == potatoNutId && output.plants[potatoNutIndex].data.instanceToken == potatoNutToken &&
            output.plants[potatoNutIndex].data.firstParent == 3 && output.plants[potatoNutIndex].data.secondParent == 4 &&
            output.plants[potatoNutIndex].data.deathEventFlags == 1 && output.gloveHolds[0].plantId == potatoNutId &&
            output.plants[0].data.productionTicks == source.plants[0].data.productionTicks);
        CHECK(outputBody.Size() == 2 && outputBody.mReadPos == 1 && outputBody.mData[0] == 0x99 && outputBody.mData[1] == 0x88);
    }

    // Exact cold counters extend the same native Zombie records. These are
    // opaque native record prefixes, not a complete Board snapshot; the real
    // Board preflight additionally checks IDs and capped counters before apply.
    auto cold = source;
    cold.coldZombies.push_back({51, {0, 400, 0, 0}}); // Warm preserves the raised threshold.
    cold.coldZombies.push_back({52, {750, 200, 1400, 400}}); // Frozen buildup may exceed its threshold.
    NetBuffer coldPrefixes;
    for (const auto& record : cold.coldZombies)
    {
        const size_t begin = coldPrefixes.Size();
        coldPrefixes.WriteU16(record.id); coldPrefixes.WriteU8(ZOMBIE_NORMAL); coldPrefixes.WriteU8(2);
        coldPrefixes.WriteI16(0); coldPrefixes.WriteI32(10000); coldPrefixes.WriteI16(800); coldPrefixes.WriteI16(1000);
        LawnNet::NetZombieHealthState health{};
        health.mBody = health.mBodyMax = 270;
        CHECK(LawnNet::NetEncodeZombieHealthState(health, coldPrefixes));
        for (int i = 0; i < 5; ++i) coldPrefixes.WriteU8(0);
        CHECK(coldPrefixes.Size() - begin == 51);
        coldPrefixes.WriteU8(static_cast<uint8_t>(record.data.chilledTicks > 255 ? 255 : record.data.chilledTicks));
        coldPrefixes.WriteU8(0); // Butter remains in the ordinary body.
        coldPrefixes.WriteU8(static_cast<uint8_t>(record.data.frozenTicks > 255 ? 255 : record.data.frozenTicks));
        coldPrefixes.WriteU8(255); coldPrefixes.WriteU8(0);
        coldPrefixes.WriteF32(0.5f); coldPrefixes.WriteF32(0); coldPrefixes.WriteF32(12);
        CHECK(coldPrefixes.Size() - begin == 68);
    }
    const auto coldOriginal = coldPrefixes.mData;
    NetBuffer coldWire = coldPrefixes;
    CHECK(EncodeNativeSnapshot(cold, eligible, coldWire));
    NativeSnapshotFrame coldDecoded; NetBuffer coldBody;
    CHECK(DecodeNativeSnapshot(coldWire, eligible, 19, coldDecoded, coldBody));
    CHECK(coldBody.mData == coldOriginal && coldDecoded.coldZombies.size() == 2);
    CHECK(coldDecoded.coldZombies[0].id == 51 && coldDecoded.coldZombies[0].data.level == 0 &&
        coldDecoded.coldZombies[0].data.threshold == 400 && !coldDecoded.coldZombies[0].data.chilledTicks && !coldDecoded.coldZombies[0].data.frozenTicks);
    CHECK(coldDecoded.coldZombies[1].id == 52 && coldDecoded.coldZombies[1].data.level == 750 &&
        coldDecoded.coldZombies[1].data.threshold == 200 && coldDecoded.coldZombies[1].data.chilledTicks == 1400 &&
        coldDecoded.coldZombies[1].data.frozenTicks == 400 && coldBody.mData[68+51] == 255 && coldBody.mData[68+53] == 255);
    NetBuffer coldRoundTrip = coldBody;
    CHECK(EncodeNativeSnapshot(coldDecoded, eligible, coldRoundTrip) && coldRoundTrip.mData == coldWire.mData);
    for (int variant = 0; variant < 10; ++variant)
    {
        auto invalidCold = cold;
        auto& data = invalidCold.coldZombies[0].data;
        if (variant == 0) data.level = -1;
        if (variant == 1) data.threshold = 0;
        if (variant == 2) data.threshold = 101;
        if (variant == 3) data.threshold = 500;
        if (variant == 4) data.chilledTicks = -1;
        if (variant == 5) data.chilledTicks = 1501;
        if (variant == 6) data.frozenTicks = -1;
        if (variant == 7) data.frozenTicks = 501;
        if (variant == 8) invalidCold.coldZombies[1].id = invalidCold.coldZombies[0].id;
        if (variant == 9) invalidCold.coldZombies[0].id = 0;
        NetBuffer previous; previous.mData = original;
        CHECK(!EncodeNativeSnapshot(invalidCold, eligible, previous));
        CHECK(previous.mReadPos == 0 && previous.mData == original);
    }
    auto maximumCold = source;
    for (uint16_t id = 1000; id < 2024; ++id) maximumCold.coldZombies.push_back({id, {}});
    CHECK(ValidateNativeSnapshotFrame(maximumCold, eligible));
    NetBuffer maximumColdWire; maximumColdWire.mData = original;
    CHECK(EncodeNativeSnapshot(maximumCold, eligible, maximumColdWire));
    NativeSnapshotFrame maximumColdDecoded; NetBuffer maximumColdBody;
    CHECK(DecodeNativeSnapshot(maximumColdWire, eligible, 19, maximumColdDecoded, maximumColdBody));
    CHECK(maximumColdDecoded.coldZombies.size() == 1024 && maximumColdDecoded.coldZombies.front().id == 1000 &&
        maximumColdDecoded.coldZombies.back().id == 2023 && maximumColdBody.mData == original);
    maximumCold.coldZombies.push_back({2024, {}});
    CHECK(!ValidateNativeSnapshotFrame(maximumCold, eligible));
    NetBuffer rejectedCount; rejectedCount.mData = original;
    CHECK(!EncodeNativeSnapshot(maximumCold, eligible, rejectedCount) && rejectedCount.mData == original);

    constexpr size_t projectileMetadataBytes = 26, coldRecordBytes = 18; // FNS20 appends goldAttracted Bool.
    const size_t coldCount = firstPlant + source.plants.size()*plantBytes + 2 + source.projectiles.size()*projectileMetadataBytes;
    const size_t firstCold = coldCount + 2;
    // FNS12 appends source projectile traits, action masks, typed cash and the
    // separate money ledger. These are metadata for existing native actors.
    auto moneyFrame=source;
    moneyFrame.projectiles[0].data={};
    moneyFrame.projectiles[0].data.kind=59; moneyFrame.projectiles[0].data.damage=80;
    moneyFrame.projectiles[0].data.melonEnhanced=true;
    moneyFrame.coldZombies={{51,{0,100,1400,400}}};
    moneyFrame.butterZombies={{52,{400,true}}};
    moneyFrame.zombieActions={{51,{0,1.0f,true,true,6,1234,37}},{52,{127,1.0f,false,false,5,0,100}}};
    moneyFrame.coins={{60,{5,false}},{61,{25,false}},{62,{25,true}},{63,{100,true}},{64,{300,true}},{65,{1000,true}}};
    moneyFrame.money=99999;
    moneyFrame.temporaryLightLevel=65535;
    moneyFrame.gloveHolds[1]={moneyFrame.plants[0].id,moneyFrame.plants[0].data.instanceToken};
    moneyFrame.gloveCooldownTicks=1000;
    NetBuffer moneyWire; moneyWire.mData=original;
    CHECK(EncodeNativeSnapshot(moneyFrame,eligible,moneyWire));
    NativeSnapshotFrame moneyDecoded; NetBuffer moneyBody;
    CHECK(DecodeNativeSnapshot(moneyWire,eligible,19,moneyDecoded,moneyBody));
    CHECK(moneyDecoded.money==99999 && moneyDecoded.projectiles[0].data.melonEnhanced && !moneyDecoded.projectiles[0].data.effectVariant);
    CHECK(moneyDecoded.zombieActions.size()==2 && moneyDecoded.zombieActions[0].data.colorMask==0 && moneyDecoded.zombieActions[1].data.colorMask==127 &&
        moneyDecoded.zombieActions[0].data.scale==1 && moneyDecoded.zombieActions[1].data.scale==1 &&
        moneyDecoded.zombieActions[0].data.doomOnDeath && !moneyDecoded.zombieActions[1].data.doomOnDeath &&
        moneyDecoded.zombieActions[0].data.jalaedOnDeath && !moneyDecoded.zombieActions[1].data.jalaedOnDeath &&
        moneyDecoded.zombieActions[0].data.poisonLevel==6 && moneyDecoded.zombieActions[0].data.poisonTicks==1234 &&
        moneyDecoded.zombieActions[0].data.poisonDamageTicks==37 && moneyDecoded.zombieActions[1].data.poisonLevel==5 &&
        moneyDecoded.zombieActions[1].data.poisonTicks==0 && moneyDecoded.zombieActions[1].data.poisonDamageTicks==100 &&
        moneyDecoded.temporaryLightLevel==65535);
    CHECK(moneyDecoded.coldZombies[0].data.frozenTicks==400 && moneyDecoded.butterZombies[0].data.ticks==400 && moneyDecoded.butterZombies[0].data.visible);
    CHECK(moneyDecoded.gloveCooldownTicks==1000 && moneyDecoded.gloveHolds[1].instanceToken==moneyFrame.plants[0].data.instanceToken && moneyBody.mData==original);
    CHECK(moneyDecoded.coins.size()==moneyFrame.coins.size());
    for (size_t i=0;i<moneyDecoded.coins.size() && i<moneyFrame.coins.size();++i)
        CHECK(moneyDecoded.coins[i].id==moneyFrame.coins[i].id && moneyDecoded.coins[i].data.value==moneyFrame.coins[i].data.value &&
            moneyDecoded.coins[i].data.money==moneyFrame.coins[i].data.money);
    CHECK(EncodeNativeSnapshot(moneyDecoded,eligible,moneyBody) && moneyBody.mData==moneyWire.mData);
    const size_t moneyButterCount=coldCount+2+moneyFrame.coldZombies.size()*coldRecordBytes;
    const size_t actionCount=moneyButterCount+2+moneyFrame.butterZombies.size()*7;
    const size_t firstAction=actionCount+2;
    constexpr size_t actionRecordBytes=29; // ID2 + mask1 + scaleF32 + Doom/Jalaed Bool + three poisonI32 + independent Grap2I32.
    const size_t coinCount=firstAction+moneyFrame.zombieActions.size()*actionRecordBytes;
    const size_t firstCoin=coinCount+2;
    const size_t moneyOffset=firstCoin+moneyFrame.coins.size()*7+moneyFrame.gloveHolds.size()*6+4;
    const size_t moneyMetadata=moneyWire.mData[6] | (static_cast<size_t>(moneyWire.mData[7])<<8);
    CHECK(moneyOffset+16+disabledAdventureSessionBytes+empty269FamilyCountBytes==headerBytes+moneyMetadata && moneyWire.mData[actionCount]==2 && moneyWire.mData[coinCount]==6);
    CHECK(moneyWire.mData[moneyOffset+6]==0); // Canonical disabled source Adventure Session marker.
    CHECK(moneyWire.mData[firstAction+7]==1 && moneyWire.mData[firstAction+actionRecordBytes+7]==0 &&
        moneyWire.mData[moneyOffset+4]==255 && moneyWire.mData[moneyOffset+5]==255);
    CHECK(moneyWire.mData[firstAction+8]==1 && moneyWire.mData[firstAction+actionRecordBytes+8]==0 &&
        moneyWire.mData[firstAction+9]==6 && moneyWire.mData[firstAction+10]==0 &&
        moneyWire.mData[firstAction+13]==0xd2 && moneyWire.mData[firstAction+14]==4 &&
        moneyWire.mData[firstAction+17]==37 && moneyWire.mData[firstAction+18]==0 &&
        moneyWire.mData[firstAction+actionRecordBytes+9]==5 && moneyWire.mData[firstAction+actionRecordBytes+17]==100);
    const size_t firstProjectile=firstPlant+source.plants.size()*plantBytes+2;
    CHECK(body.mData[firstProjectile+22]==0 && body.mData[firstProjectile+23]==0 && body.mData[firstProjectile+24]==0 && body.mData[firstProjectile+25]==0);
    for (size_t flag : {size_t(22),size_t(23),size_t(24),size_t(25)})
        for (uint8_t invalidBoolean : {uint8_t(2),uint8_t(255)})
        {auto forged=body;forged.mData[firstProjectile+flag]=invalidBoolean;RejectAtomically(forged);}
    // FNS17 keeps source allegiance, hit latch and one-shot Umbrella upgrade
    // on the existing native projectile identity; way13 alone is not allegiance.
    struct PlanarMetadata {uint16_t kind;int damage;bool hostile,umbrellaed;};
    for(bool latched:{false,true})
    {
        auto pendingFrame=source;auto& shot=pendingFrame.projectiles[0].data;shot={};
        shot.kind=3;shot.damage=300;shot.specialType=9;shot.isZombieBullet=true;shot.hitLatched=latched;
        NetBuffer pendingWire;CHECK(EncodeNativeSnapshot(pendingFrame,eligible,pendingWire));
        NativeSnapshotFrame restored;NetBuffer ordinary;CHECK(DecodeNativeSnapshot(pendingWire,eligible,source.epoch,restored,ordinary));
        CHECK(restored.projectiles.size()==1&&restored.projectiles[0].data.hitLatched==latched&&restored.projectiles[0].data.isZombieBullet);
    }
    const PlanarMetadata planarMetadata[]={
        {58,60,true,false},{59,80,false,false},{59,160,false,false},
        {59,140,false,true},{59,280,false,true}};
    for(const auto& spec:planarMetadata)for(bool latched:{false,true})
    {
        auto frame=source;auto& shot=frame.projectiles[0].data;shot={};
        shot.kind=spec.kind;shot.damage=spec.damage;shot.specialType=13;
        shot.isZombieBullet=spec.hostile;shot.hitLatched=latched;shot.umbrellaed=spec.umbrellaed;
        const auto type=spec.hostile?PROJECTILE_BASKETBALL:PROJECTILE_MELON;
        CHECK(NativeProjectileMetadataType(shot)==type&&NativeProjectileMetadataMotion(shot)==MOTION_LOBBED);
        CHECK(NativeProjectileMetadataValid(shot,type,MOTION_LOBBED));
        CHECK(!NativeProjectileMetadataValid(shot,type==PROJECTILE_MELON?PROJECTILE_BASKETBALL:PROJECTILE_MELON,MOTION_LOBBED));
        CHECK(!NativeProjectileMetadataValid(shot,type,MOTION_STRAIGHT));
        NetBuffer encoded;encoded.mData=original;
        const bool encodedOk=EncodeNativeSnapshot(frame,eligible,encoded);CHECK(encodedOk);
        if(!encodedOk)continue;
        CHECK(encoded.mData[firstProjectile+22]==spec.hostile&&encoded.mData[firstProjectile+23]==latched&&
            encoded.mData[firstProjectile+24]==spec.umbrellaed);
        NativeSnapshotFrame copy;NetBuffer restored;
        const bool decodedOk=DecodeNativeSnapshot(encoded,eligible,19,copy,restored);CHECK(decodedOk);
        if(decodedOk)
        {
            const auto& actual=copy.projectiles[0].data;
            CHECK(copy.projectiles[0].id==frame.projectiles[0].id&&actual.kind==spec.kind&&actual.damage==spec.damage&&
                actual.specialType==13&&actual.isZombieBullet==spec.hostile&&actual.hitLatched==latched&&
                actual.umbrellaed==spec.umbrellaed&&restored.mData==original);
            CHECK(EncodeNativeSnapshot(copy,eligible,restored)&&restored.mData==encoded.mData);
        }
        for(int field=0;field<10;++field)
        {
            auto forged=encoded;
            if(field==0)forged.mData[firstProjectile+22]=!spec.hostile;
            if(field==1)forged.mData[firstProjectile+12]=12;
            if(field==2)forged.mData[firstProjectile+4]=static_cast<uint8_t>(spec.damage+1);
            if(field==3)forged.mData[firstProjectile+24]=!spec.umbrellaed;
            if(field==4)forged.mData[firstProjectile+8]=1;
            if(field==5)forged.mData[firstProjectile+13]=1;
            if(field==6)forged.mData[firstProjectile+14]=1;
            if(field==7)forged.mData[firstProjectile+16]=1;
            if(field==8)forged.mData[firstProjectile+20]=1;
            if(field==9)forged.mData[firstProjectile+21]=1;
            RejectAtomically(forged);
        }
        for(size_t flag:{size_t(22),size_t(23),size_t(24)})for(uint8_t invalid:{uint8_t(2),uint8_t(255)})
        {auto forged=encoded;forged.mData[firstProjectile+flag]=invalid;RejectAtomically(forged);}
    }
    // Existing friendly lobbers retain their full original metadata after the
    // exact source floor(damage*1.75) inversion; arbitrary damage cannot enter.
    auto umbrellaCabbage=source;auto& upgraded=umbrellaCabbage.projectiles[0].data;upgraded={};
    upgraded.kind=52;upgraded.damage=70;upgraded.umbrellaed=true;
    CHECK(NativeProjectileMetadataValid(upgraded,PROJECTILE_CABBAGE,MOTION_LOBBED));
    NetBuffer umbrellaWire;umbrellaWire.mData=original;
    const bool umbrellaEncoded=EncodeNativeSnapshot(umbrellaCabbage,eligible,umbrellaWire);CHECK(umbrellaEncoded);
    if(umbrellaEncoded)
    {
        NativeSnapshotFrame copy;NetBuffer restored;
        const bool decodedOk=DecodeNativeSnapshot(umbrellaWire,eligible,19,copy,restored);CHECK(decodedOk);
        if(decodedOk)CHECK(copy.projectiles[0].data.kind==52&&copy.projectiles[0].data.damage==70&&
            copy.projectiles[0].data.umbrellaed&&!copy.projectiles[0].data.hitLatched&&!copy.projectiles[0].data.isZombieBullet);
        for(int field=0;field<5;++field)
        {
            auto forged=umbrellaWire;
            if(field<2)forged.mData[firstProjectile+4]=static_cast<uint8_t>(field==0?69:71);
            if(field==2)forged.mData[firstProjectile+22]=1;
            if(field==3)forged.mData[firstProjectile+23]=1;
            if(field==4)forged.mData[firstProjectile+12]=13;
            RejectAtomically(forged);
        }
    }
    auto fractionalUpgrade=source;auto& sunCabbage=fractionalUpgrade.projectiles[0].data;sunCabbage={};
    sunCabbage.kind=53;sunCabbage.damage=87;sunCabbage.sunOnHit=5;sunCabbage.umbrellaed=true; // floor(50*1.75), not88.
    CHECK(NativeProjectileMetadataValid(sunCabbage,PROJECTILE_CABBAGE,MOTION_LOBBED));
    NetBuffer fractionalWire;fractionalWire.mData=original;
    const bool fractionalEncoded=EncodeNativeSnapshot(fractionalUpgrade,eligible,fractionalWire);CHECK(fractionalEncoded);
    if(fractionalEncoded)
    {
        NativeSnapshotFrame copy;NetBuffer restored;
        const bool decodedOk=DecodeNativeSnapshot(fractionalWire,eligible,19,copy,restored);CHECK(decodedOk);
        if(decodedOk)CHECK(copy.projectiles[0].data.damage==87&&copy.projectiles[0].data.sunOnHit==5&&
            copy.projectiles[0].data.umbrellaed&&EncodeNativeSnapshot(copy,eligible,restored)&&restored.mData==fractionalWire.mData);
        auto bad=fractionalWire;bad.mData[firstProjectile+4]=88;RejectAtomically(bad);
        bad=fractionalWire;bad.mData[firstProjectile+8]=0;RejectAtomically(bad);
    }
    for(size_t flag:{size_t(22),size_t(23),size_t(24),size_t(25)})
    {auto foreign=body;foreign.mData[firstProjectile+flag]=1;RejectAtomically(foreign);}
    CHECK(moneyWire.mData[firstProjectile+20]==1 && moneyWire.mData[firstProjectile+21]==0 &&
        moneyWire.mData[firstAction+actionRecordBytes+2]==127 && moneyWire.mData[firstCoin+6]==0 && moneyWire.mData[firstCoin+2*7+6]==1);
    CHECK(NativeProjectileMetadataValid(moneyFrame.projectiles[0].data,PROJECTILE_MELON,MOTION_LOBBED));
    CHECK(!NativeProjectileMetadataValid(moneyFrame.projectiles[0].data,PROJECTILE_PEA,MOTION_LOBBED));
    CHECK(!NativeProjectileMetadataValid(moneyFrame.projectiles[0].data,PROJECTILE_MELON,MOTION_STRAIGHT));
    // The renderer's new payloads must also enter through the same central
    // type/motion contract used by body preflight and snapshot validation.
    struct Batch266Payload { uint16_t kind;int damage;uint8_t way,variant;ProjectileType type;ProjectileMotion motion; };
    const Batch266Payload batch266Payloads[]={
        {3,300,0,0,PROJECTILE_PEA,MOTION_STRAIGHT},{23,300,0,0,PROJECTILE_PEA,MOTION_STRAIGHT},
        {23,1800,0,1,PROJECTILE_PEA,MOTION_STRAIGHT},
        {4,80,1,0,PROJECTILE_PEA,MOTION_STRAIGHT},{5,80,1,0,PROJECTILE_PEA,MOTION_STRAIGHT},
        {38,20,0,0,PROJECTILE_SPIKE,MOTION_STRAIGHT},{39,40,2,0,PROJECTILE_STAR,MOTION_STAR},
        {39,40,9,0,PROJECTILE_STAR,MOTION_STAR},{40,40,2,0,PROJECTILE_STAR,MOTION_STAR},
        {40,40,9,1,PROJECTILE_STAR,MOTION_STAR},{40,40,6,1,PROJECTILE_STAR,MOTION_STAR},
        {61,20,0,0,PROJECTILE_KERNEL,MOTION_LOBBED},{62,40,0,0,PROJECTILE_BUTTER,MOTION_LOBBED},
        {63,40,0,0,PROJECTILE_CABBAGE,MOTION_LOBBED},{64,80,0,0,PROJECTILE_MELON,MOTION_LOBBED},
        {86,160,0,0,PROJECTILE_MELON,MOTION_LOBBED},{71,140,0,0,PROJECTILE_MELON,MOTION_LOBBED},
        {75,80,0,0,PROJECTILE_CABBAGE,MOTION_LOBBED},{75,160,0,1,PROJECTILE_CABBAGE,MOTION_LOBBED},
        {100,300,0,0,PROJECTILE_SPIKE,MOTION_STRAIGHT},{100,300,18,0,PROJECTILE_SPIKE,MOTION_STAR}
    };
    for(const auto& spec:batch266Payloads)
    {
        auto frame=source;auto& shot=frame.projectiles[0].data;shot={};
        shot.kind=spec.kind;shot.damage=spec.damage;shot.specialType=spec.way;shot.effectVariant=spec.variant;
        CHECK(NativeProjectileMetadataType(shot)==spec.type&&NativeProjectileMetadataMotion(shot)==spec.motion);
        CHECK(NativeProjectileMetadataValid(shot,spec.type,spec.motion));
        CHECK(!NativeProjectileMetadataValid(shot,spec.type==PROJECTILE_PEA?PROJECTILE_SPIKE:PROJECTILE_PEA,spec.motion));
        CHECK(!NativeProjectileMetadataValid(shot,spec.type,spec.motion==MOTION_STRAIGHT?MOTION_LOBBED:MOTION_STRAIGHT));
        NetBuffer encoded;encoded.mData=original;
        const bool encodedOk=EncodeNativeSnapshot(frame,eligible,encoded);CHECK(encodedOk);
        if(!encodedOk)continue;
        NativeSnapshotFrame copy;NetBuffer restored;
        const bool decodedOk=DecodeNativeSnapshot(encoded,eligible,19,copy,restored);CHECK(decodedOk);
        if(decodedOk)
        {
            const auto& actual=copy.projectiles[0].data;
            CHECK(actual.kind==shot.kind&&actual.damage==shot.damage&&actual.specialType==shot.specialType&&
                actual.effectVariant==shot.effectVariant&&!actual.sunOnHit&&!actual.fireState&&!actual.lastTorchId&&
                !actual.lastTorchToken&&!actual.melonEnhanced&&restored.mData==original);
            CHECK(EncodeNativeSnapshot(copy,eligible,restored)&&restored.mData==encoded.mData);
        }
        for(int field=0;field<5;++field)
        {
            auto forged=encoded;
            if(field==0)forged.mData[firstPlant+plantBytes+2+4]=static_cast<uint8_t>(spec.damage+1);
            if(field==1)forged.mData[firstPlant+plantBytes+2+8]=1;
            if(field==2)forged.mData[firstPlant+plantBytes+2+12]=255;
            if(field==3)forged.mData[firstPlant+plantBytes+2+21]=255;
            if(field==4)forged.mData[firstPlant+plantBytes+2+14]=1; // Incomplete generation stamp.
            RejectAtomically(forged);
        }
    }
    for (int variant=0;variant<31;++variant)
    {
        NetBuffer forged=moneyWire;
        auto write32=[&](size_t offset,uint32_t value)
        { for (int byte=0;byte<4;++byte) forged.mData[offset+byte]=static_cast<uint8_t>(value>>(byte*8)); };
        if (variant==0) forged.mData[firstProjectile+20]=2; // Noncanonical Bool.
        if (variant==1) forged.mData[firstProjectile+21]=1; // Color variant belongs to payload13 only.
        if (variant==2) forged.mData[firstAction+2]=128;
        if (variant==3) { forged.mData[firstAction+actionRecordBytes]=51; forged.mData[firstAction+actionRecordBytes+1]=0; } // Duplicate action generation ID.
        if (variant==4) { forged.mData[firstAction]=0; forged.mData[firstAction+1]=0; }
        if (variant==5) forged.mData[firstCoin+6]=2; // Noncanonical typed-coin Bool.
        if (variant==6) forged.mData[firstCoin+6]=1; // Sun5 is never a Money value.
        if (variant==7) write32(firstCoin+2*7+2,0); // Claimed tombstones are not live snapshot coins.
        if (variant==8) { forged.mData[firstCoin+7]=60; forged.mData[firstCoin+8]=0; }
        if (variant==9) write32(moneyOffset,100000);
        if (variant==10) write32(moneyOffset,0xffffffffu);
        if (variant==11)
        {
            forged.mData[firstProjectile+2]=8; write32(firstProjectile+4,100); write32(firstProjectile+8,5);
        } // Otherwise valid kind8 cannot carry a Melon enhancement.
        if (variant==12) { forged.mData[actionCount]=1; forged.mData[actionCount+1]=4; } // Bounded vector rejects1025 before allocation.
        if (variant==13) write32(firstAction+3,0); // Zero scale.
        if (variant==14) write32(firstAction+3,0xbf800000u); // Negative scaleР Р†РІвЂљВ¬РІР‚в„ў1.
        if (variant==15) write32(firstAction+3,0x7fc00000u); // NaN.
        if (variant==16) write32(firstAction+3,0x41100000u); // Scale9 exceeds8.
        if (variant==17) write32(firstAction+3,0x7f800000u); // Infinity.
        if (variant==18) forged.mData[firstAction+7]=2; // Noncanonical Doom-on-death Bool.
        if (variant==19) forged.mData[firstAction+actionRecordBytes+7]=255;
        if (variant==20) forged.mData[firstAction+8]=2;
        if (variant==21) forged.mData[firstAction+actionRecordBytes+8]=255;
        if (variant==22) write32(firstAction+9,0xffffffffu); // Negative poison level.
        if (variant==23) write32(firstAction+9,static_cast<uint32_t>(kFusionPoisonMaximumLevel)+1);
        if (variant==24) write32(firstAction+13,0xffffffffu); // Negative duration.
        if (variant==25) write32(firstAction+17,0); // Pulse interval is1..100 even while inactive.
        if (variant==26) write32(firstAction+17,101);
        if (variant==27) write32(firstAction+17,0xffffffffu);
        if (variant==28) write32(firstAction+13,0); // Level6 cannot remain untimed.
        if (variant==29) write32(firstAction+actionRecordBytes+9,6);
        if (variant==30) write32(firstAction+actionRecordBytes+17,0x7fffffffu);
        RejectAtomically(forged);
    }
    // Poison's partial untimed stacks and warmed timed level0 are real source
    // states. The pulse remainder survives all of them, including INT32 clocks.
    const FusionPoisonState poisonStates[]={
        {0,0,100},{1,0,37},{5,0,1},{0,1234,37},{5,500,100},{6,1000,1},
        {kFusionPoisonMaximumLevel,(std::numeric_limits<int>::max)(),100}
    };
    for(const auto state:poisonStates)
    {
        auto frame=moneyFrame;auto& status=frame.zombieActions[0].data;
        status.poisonLevel=state.level;status.poisonTicks=state.ticks;status.poisonDamageTicks=state.damageTicks;
        NetBuffer encoded;encoded.mData=original;CHECK(EncodeNativeSnapshot(frame,eligible,encoded));
        NativeSnapshotFrame copy;NetBuffer restored;
        CHECK(DecodeNativeSnapshot(encoded,eligible,19,copy,restored));
        if(copy.zombieActions.size()==2)
        {
            const auto& actual=copy.zombieActions[0].data;
            CHECK(actual.poisonLevel==state.level&&actual.poisonTicks==state.ticks&&actual.poisonDamageTicks==state.damageTicks&&
                actual.doomOnDeath&&actual.jalaedOnDeath&&restored.mData==original);
            CHECK(EncodeNativeSnapshot(copy,eligible,restored)&&restored.mData==encoded.mData);
        }
        else CHECK(false);
    }
    for(int variant=0;variant<8;++variant)
    {
        auto frame=moneyFrame;auto& status=frame.zombieActions[0].data;
        if(variant==0)status.poisonLevel=-1;
        if(variant==1)status.poisonLevel=kFusionPoisonMaximumLevel+1;
        if(variant==2)status.poisonTicks=-1;
        if(variant==3)status.poisonTicks=0;
        if(variant==4)status.poisonDamageTicks=0;
        if(variant==5)status.poisonDamageTicks=101;
        if(variant==6)status.poisonDamageTicks=-1;
        if(variant==7)status.poisonDamageTicks=(std::numeric_limits<int>::max)();
        NetBuffer untouched;untouched.WriteU8(0x77);untouched.WriteU8(0x99);untouched.mReadPos=1;
        const auto bytes=untouched.mData;
        CHECK(!ValidateNativeSnapshotFrame(frame,eligible));
        CHECK(!EncodeNativeSnapshot(frame,eligible,untouched)&&untouched.mData==bytes&&untouched.mReadPos==1);
    }
    for(int value:{15,50})
    {
        auto frame=moneyFrame;frame.coins[0].data={value,false};
        NetBuffer encoded;encoded.mData=original;CHECK(EncodeNativeSnapshot(frame,eligible,encoded));
        NativeSnapshotFrame copy;NetBuffer restored;
        CHECK(DecodeNativeSnapshot(encoded,eligible,19,copy,restored)&&copy.coins[0].data.value==value&&
            !copy.coins[0].data.money&&restored.mData==original);
        frame.coins[0].data.money=true;
        NetBuffer untouched;untouched.mData=original;
        CHECK(!EncodeNativeSnapshot(frame,eligible,untouched)&&untouched.mData==original);
    }
    // The source lease count occupies the fullU16 range; no narrower semantic
    // bound is imposed. The existing truncation matrix covers missing bytes.
    for(uint16_t light : {uint16_t(0),uint16_t(1),uint16_t(3000),uint16_t(65535)})
    {
        auto room=moneyFrame;room.temporaryLightLevel=light;
        NetBuffer encoded;encoded.mData=original;CHECK(EncodeNativeSnapshot(room,eligible,encoded));
        NativeSnapshotFrame copy;NetBuffer restored;
        CHECK(DecodeNativeSnapshot(encoded,eligible,19,copy,restored) && copy.temporaryLightLevel==light &&
            copy.zombieActions[0].data.doomOnDeath && restored.mData==original);
    }
    // HypnoPuff scales the actual native actor. This exact F32 is metadata,
    // not an inference from HP, remaining armour, or a local ghost generation.
    for (float scale : {.5f,1.0f,2.0f,8.0f})
    {
        auto scaled=moneyFrame;
        scaled.zombieActions[0].data.scale=scale;
        scaled.zombieActions[1].data.scale=scale;
        NetBuffer encoded;encoded.mData=original;
        CHECK(EncodeNativeSnapshot(scaled,eligible,encoded));
        NativeSnapshotFrame copy;NetBuffer restored;
        CHECK(DecodeNativeSnapshot(encoded,eligible,19,copy,restored));
        CHECK(copy.zombieActions.size()==2 && copy.zombieActions[0].id==51 && copy.zombieActions[0].data.colorMask==0 &&
            copy.zombieActions[1].id==52 && copy.zombieActions[1].data.colorMask==127 &&
            copy.zombieActions[0].data.scale==scale && copy.zombieActions[1].data.scale==scale && restored.mData==original);
        CHECK(EncodeNativeSnapshot(copy,eligible,restored) && restored.mData==encoded.mData);
    }
    for (float scale : {0.0f,-1.0f,(std::numeric_limits<float>::quiet_NaN)(),8.001f,(std::numeric_limits<float>::infinity)()})
    {
        auto invalid=moneyFrame;invalid.zombieActions[0].data.scale=scale;
        NetBuffer previous;previous.WriteU8(0x77);previous.WriteU8(0x99);previous.mReadPos=1;
        const auto input=previous.mData;
        CHECK(!EncodeNativeSnapshot(invalid,eligible,previous));
        CHECK(previous.mData==input && previous.mReadPos==1);
    }
    for (int amount : {0,3000,99999})
    {
        auto frame=moneyFrame; frame.money=amount; NetBuffer encoded; encoded.mData=original;
        CHECK(EncodeNativeSnapshot(frame,eligible,encoded));
        NativeSnapshotFrame copy; NetBuffer restored;
        CHECK(DecodeNativeSnapshot(encoded,eligible,19,copy,restored) && copy.money==amount && restored.mData==original);
    }
    for (int variant=0;variant<12;++variant)
    {
        auto invalid=moneyFrame;
        if (variant==0) invalid.money=-1;
        if (variant==1) invalid.money=100000;
        if (variant==2) invalid.zombieActions[0].data.colorMask=128;
        if (variant==3) invalid.zombieActions.push_back(invalid.zombieActions[0]);
        if (variant==4) invalid.zombieActions[0].id=0;
        if (variant==5) invalid.coins[2].data.value=5;
        if (variant==6) invalid.coins[3].data.money=false;
        if (variant==7) invalid.coins[3].data.value=-1;
        if (variant==8) invalid.coins[3].data.value=1001;
        if (variant==9) invalid.coins.push_back(invalid.coins[0]);
        if (variant==10) invalid.coins[0].id=0;
        if (variant==11) invalid.projectiles[0].data.effectVariant=7;
        NetBuffer preserved; preserved.mData=original;
        CHECK(!EncodeNativeSnapshot(invalid,eligible,preserved) && preserved.mData==original && preserved.mReadPos==0);
    }
    for (uint16_t version : {uint16_t(12),uint16_t(13),uint16_t(14),uint16_t(15),uint16_t(16),uint16_t(17)})
    {
        NetBuffer wrong=moneyWire; wrong.mData[4]=static_cast<uint8_t>(version); wrong.mData[5]=0;
        RejectAtomically(wrong);
    }
    for (uint8_t color : {uint8_t(0),uint8_t(3),uint8_t(6)})
    {
        auto colored=source; auto& data=colored.projectiles[0].data;
        data={}; data.kind=13; data.damage=20; data.effectVariant=color;
        CHECK(NativeProjectileMetadataValid(data,PROJECTILE_PUFF,MOTION_STRAIGHT));
        CHECK(!NativeProjectileMetadataValid(data,PROJECTILE_PEA,MOTION_STRAIGHT));
        CHECK(!NativeProjectileMetadataValid(data,PROJECTILE_PUFF,MOTION_LOBBED));
        NetBuffer encoded; encoded.mData=original;
        CHECK(EncodeNativeSnapshot(colored,eligible,encoded));
        NativeSnapshotFrame copy; NetBuffer restored;
        CHECK(DecodeNativeSnapshot(encoded,eligible,19,copy,restored));
        CHECK(copy.projectiles.size()==1 && copy.projectiles[0].data.kind==13 &&
            copy.projectiles[0].data.effectVariant==color && !copy.projectiles[0].data.melonEnhanced && restored.mData==original);
        if (encoded.Size()>firstProjectile+21)
        { encoded.mData[firstProjectile+21]=7; RejectAtomically(encoded); }
    }
    for (uint8_t phase : {uint8_t(3),uint8_t(4)})
    {
        auto three=source; auto& data=three.projectiles[0].data;
        data={}; data.kind=0; data.damage=20; data.specialType=phase;
        CHECK(NativeProjectileMetadataValid(data,PROJECTILE_PEA,MOTION_THREEPEATER));
        CHECK(!NativeProjectileMetadataValid(data,PROJECTILE_PEA,MOTION_STRAIGHT));
        CHECK(!NativeProjectileMetadataValid(data,PROJECTILE_PUFF,MOTION_THREEPEATER));
        NetBuffer encoded; encoded.mData=original;
        CHECK(EncodeNativeSnapshot(three,eligible,encoded));
        NativeSnapshotFrame copy; NetBuffer restored;
        CHECK(DecodeNativeSnapshot(encoded,eligible,19,copy,restored));
        CHECK(copy.projectiles.size()==1 && copy.projectiles[0].data.specialType==phase &&
            copy.projectiles[0].data.kind==0 && restored.mData==original);
        if (encoded.Size()>firstProjectile+12)
        { encoded.mData[firstProjectile+12]=5; RejectAtomically(encoded); }
    }
    // The same source payload9 supports the previously accepted short Puff
    // native motion and the long-range Scaredy straight adapter. A new family
    // must not remove the older valid native tuple during body preflight.
    NativeProjectileData puff;puff.kind=9;puff.damage=20;
    CHECK(NativeProjectileMetadataValid(puff,PROJECTILE_PUFF,MOTION_PUFF));
    CHECK(NativeProjectileMetadataValid(puff,PROJECTILE_PUFF,MOTION_STRAIGHT));
    CHECK(!NativeProjectileMetadataValid(puff,PROJECTILE_PEA,MOTION_PUFF));
    CHECK(!NativeProjectileMetadataValid(puff,PROJECTILE_PUFF,MOTION_BACKWARDS));
    // Butter mirrors the actual native counter beyond the raw body's255 cap.
    // Visibility remains independent: a new.5s effect can be hidden, while a
    // previous visible effect survives an overwrite to the same50 ticks.
    const size_t butterCount = coldCount + 2; // Source example has no cold records.
    const size_t firstButter = butterCount + 2;
    NetBuffer butterWire;
    for (int ticks : {0,1,50,100,200,255,256,400,599,600})
        for (bool visible : {false,true})
        {
            if (!ticks && visible) continue;
            auto butter = source;
            butter.butterZombies.push_back({700,{ticks,visible}});
            NetBuffer encoded = native;
            CHECK(EncodeNativeSnapshot(butter,eligible,encoded));
            CHECK(encoded.mData[butterCount] == 1 && encoded.mData[firstButter] == (700 & 255));
            NativeSnapshotFrame copy; NetBuffer restored;
            CHECK(DecodeNativeSnapshot(encoded,eligible,19,copy,restored));
            CHECK(copy.butterZombies.size() == 1 && copy.butterZombies[0].id == 700 &&
                copy.butterZombies[0].data.ticks == ticks && copy.butterZombies[0].data.visible == visible);
            CHECK(restored.mData == native.mData);
            CHECK(EncodeNativeSnapshot(copy,eligible,restored) && restored.mData == encoded.mData);
            if (ticks == 400 && visible) butterWire = encoded;
        }
    for (int variant = 0; variant < 4; ++variant)
    {
        auto invalid = source;
        invalid.butterZombies.push_back({700,{400,true}});
        if (variant == 0) invalid.butterZombies[0].data.ticks = 601;
        if (variant == 1) invalid.butterZombies[0].data.ticks = -1;
        if (variant == 2) invalid.butterZombies[0].data.ticks = 0;
        if (variant == 3) invalid.butterZombies[0].id = 0;
        NetBuffer untouched = native;
        CHECK(!EncodeNativeSnapshot(invalid,eligible,untouched) && untouched.mData == native.mData);
    }
    auto duplicateButter = source;
    duplicateButter.butterZombies = {{700,{400,true}},{700,{50,false}}};
    CHECK(!ValidateNativeSnapshotFrame(duplicateButter,eligible));
    for (int variant = 0; variant < 4; ++variant)
    {
        NetBuffer forged = butterWire;
        if (variant == 0) { forged.mData[firstButter+2] = 0x59; forged.mData[firstButter+3] = 2; } //601.
        if (variant == 1) forged.mData[firstButter+6] = 2; // Invalid serialized Bool.
        if (variant == 2) for (int byte = 0; byte < 4; ++byte) forged.mData[firstButter+2+byte] = 0; //0,true.
        if (variant == 3) { forged.mData[firstButter] = 0; forged.mData[firstButter+1] = 0; }
        const size_t bytes = forged.mData[6] | (static_cast<size_t>(forged.mData[7]) << 8);
        const size_t payload = headerBytes + bytes;
        forged.mData[payload] ^= 0x40;
        const auto hash = LawnNet::NetFnv1a32(forged.Data()+payload,forged.Size()-payload);
        for (int byte = 0; byte < 4; ++byte) forged.mData[10+byte] = static_cast<uint8_t>(hash >> (byte*8));
        NetBuffer prefixed; prefixed.WriteU8(0x77); prefixed.WriteBytes(forged.Data(),forged.Size()); prefixed.mReadPos = 1;
        NativeSnapshotFrame preserved; preserved.epoch = 777;
        NetBuffer preservedNative; preservedNative.WriteU8(0x99);
        CHECK(!DecodeNativeSnapshot(prefixed,eligible,19,preserved,preservedNative));
        CHECK(prefixed.mReadPos == 1 && preserved.epoch == 777 && preserved.butterZombies.empty() &&
            preservedNative.Size() == 1 && preservedNative.mData[0] == 0x99);
    }
    CHECK(coldWire.mData[coldCount] == 2 && coldWire.mData[firstCold] == 51);
    for (int variant = 0; variant < 7; ++variant)
    {
        NetBuffer forged = coldWire; forged.mReadPos = 0;
        if (variant < 4)
        {
            const int values[] = {-1, 101, 1501, 501};
            const uint32_t value = static_cast<uint32_t>(values[variant]);
            const size_t offset = firstCold + 2 + variant*4;
            for (int byte = 0; byte < 4; ++byte) forged.mData[offset+byte] = static_cast<uint8_t>(value >> (byte*8));
        }
        if (variant == 4) forged.mData[firstCold+coldRecordBytes] = 51; // Duplicate ID.
        if (variant == 5) { forged.mData[coldCount] = 1; forged.mData[coldCount+1] = 4; } //1025.
        if (variant == 6) { forged.mData[firstCold] = 0; forged.mData[firstCold+1] = 0; }
        const size_t metadataBytes = forged.mData[6] | (static_cast<size_t>(forged.mData[7]) << 8);
        const size_t payload = headerBytes + metadataBytes;
        forged.mData[payload+10] ^= 0x80;
        const auto hash = LawnNet::NetFnv1a32(forged.Data()+payload, forged.Size()-payload);
        for (int byte = 0; byte < 4; ++byte) forged.mData[10+byte] = static_cast<uint8_t>(hash >> (byte*8));
        CHECK(!DecodeNativeSnapshot(forged, eligible, 19, preservedFrame, preservedBody));
        CHECK(forged.mReadPos == 0 && preservedFrame.epoch == 777 && preservedFrame.coldZombies.empty() &&
            preservedBody.Size() == 1 && preservedBody.mData[0] == 0x99);
    }
    for (uint16_t kind : {uint16_t(15), uint16_t(18)})
    {
        auto coldProjectile = source;
        coldProjectile.projectiles[0].data = {};
        coldProjectile.projectiles[0].data.kind = kind;
        CHECK(ValidateNativeSnapshotFrame(coldProjectile, eligible));
        for (uint8_t phase : {uint8_t(1), uint8_t(2)})
        {
            coldProjectile.projectiles[0].data.specialType = phase;
            NetBuffer previous; previous.mData = original;
            if(kind==15)
            {
                CHECK(EncodeNativeSnapshot(coldProjectile, eligible, previous));
                NativeSnapshotFrame decodedCold;NetBuffer restoredColdBody;
                CHECK(DecodeNativeSnapshot(previous,eligible,19,decodedCold,restoredColdBody) &&
                    decodedCold.projectiles[0].data.kind==15 && decodedCold.projectiles[0].data.specialType==phase && restoredColdBody.mData==original);
            }
            else CHECK(!EncodeNativeSnapshot(coldProjectile, eligible, previous) && previous.mData == original);
        }
        coldProjectile.projectiles[0].data.specialType=5;
        NetBuffer previous;previous.mData=original;
        CHECK(!EncodeNativeSnapshot(coldProjectile,eligible,previous) && previous.mData==original);
    }
    auto baseIce = source;
    baseIce.plants[0].data = {};
    baseIce.plants[0].data.instanceToken = 0x10001u;
    baseIce.plants[0].data.kind = 10;
    CHECK(ValidateNativeSnapshotFrame(baseIce, eligible));
    baseIce.plants[0].data.firstParent = 0;
    CHECK(!ValidateNativeSnapshotFrame(baseIce, eligible));

    // Protocol53 native projectile record is35bytes: motion stays byte28; velocity
    // and age are host fields, not replicated fields. The codec body is opaque
    // here; Board's Fusion preflight checks the same phase against this motion.
    NetBuffer splitWire;
    for (uint16_t kind : {uint16_t(0), uint16_t(1), uint16_t(10)})
        for (uint8_t phase = 0; phase <= 2; ++phase)
        {
            auto frame = source;
            frame.projectiles[0].data = {};
            auto& data = frame.projectiles[0].data;
            data.kind = kind; data.damage = kind == 1 ? 60 : 20; data.specialType = phase;
            const auto motion = phase == 1 ? MOTION_BACKWARDS : MOTION_STRAIGHT;
            NetBuffer record;
            record.WriteU16(frame.projectiles[0].id); record.WriteU8(PROJECTILE_PEA); record.WriteU8(2);
            record.WriteF32(200); record.WriteF32(250); record.WriteF32(0); record.WriteF32(280);
            record.WriteI32(10000); record.WriteF32(0);
            record.WriteU8(static_cast<uint8_t>(motion)); record.WriteU8(0); record.WriteU8(0);record.WriteF32(0);
            CHECK(record.Size() == 35 && record.mData[28] == motion);
            const auto originalRecord = record.mData;
            CHECK(ValidateNativeSnapshotFrame(frame, eligible) && EncodeNativeSnapshot(frame, eligible, record));
            NativeSnapshotFrame restored; NetBuffer restoredRecord;
            CHECK(DecodeNativeSnapshot(record, eligible, 19, restored, restoredRecord));
            CHECK(restored.projectiles.size() == 1 && restored.projectiles[0].id == frame.projectiles[0].id &&
                restored.projectiles[0].data.kind == kind && restored.projectiles[0].data.damage == data.damage &&
                restored.projectiles[0].data.specialType == phase && !restored.projectiles[0].data.sunOnHit);
            CHECK(restoredRecord.mData == originalRecord && FusionSplitProjectilePhaseValid(kind, phase,
                static_cast<ProjectileMotion>(restoredRecord.mData[28])));
            CHECK(!FusionSplitProjectilePhaseValid(kind, phase, phase == 1 ? MOTION_STRAIGHT : MOTION_BACKWARDS));
            if (kind == 10 && phase == 2) { splitWire = record; splitWire.mReadPos = 0; }
        }
    for (int variant = 0; variant < 6; ++variant)
    {
        auto frame = source;
        auto& data = frame.projectiles[0].data;
        data = {}; data.kind = 10; data.damage = 20; data.specialType = 1;
        if (variant == 0) data.specialType = 3;
        if (variant == 1) { data.kind = 8; data.damage = 100; }
        if (variant == 2) data.kind = 9;
        if (variant == 3) data.kind = 2;
        if (variant == 4) data.damage = 60;
        if (variant == 5) data.sunOnHit = 5;
        NetBuffer previous; previous.mData = original;
        CHECK(!EncodeNativeSnapshot(frame, eligible, previous));
        CHECK(previous.mData == original && previous.mReadPos == 0);
    }
    // Invalid metadata still rejects if an altered native payload has a valid
    // checksum; no input read position or pre-existing output may be consumed.
    const size_t projectileMetadata = firstPlant + source.plants.size()*plantBytes + 2;
    CHECK(projectileMetadata+12 < splitWire.Size() && splitWire.mData[projectileMetadata+12] == 2);
    for (int variant = 0; variant < 6; ++variant)
    {
        NetBuffer bad = splitWire;
        if (variant == 0) bad.mData[projectileMetadata+12] = 3;
        if (variant >= 1 && variant <= 3)
        {
            bad.mData[projectileMetadata+2] = variant == 1 ? 8 : variant == 2 ? 9 : 2;
            if (variant == 1) bad.mData[projectileMetadata+4] = 100;
        }
        if (variant == 4) bad.mData[projectileMetadata+4] = 60;
        if (variant == 5) bad.mData[projectileMetadata+8] = 5;
        const size_t metadataBytes = bad.mData[6] | (static_cast<size_t>(bad.mData[7]) << 8);
        const size_t payload = headerBytes + metadataBytes;
        bad.mData[payload] ^= 0x80;
        const auto hash = LawnNet::NetFnv1a32(bad.Data()+payload, bad.Size()-payload);
        for (int byte = 0; byte < 4; ++byte) bad.mData[10+byte] = static_cast<uint8_t>(hash >> (byte*8));
        CHECK(!DecodeNativeSnapshot(bad, eligible, 19, preservedFrame, preservedBody));
        CHECK(bad.mReadPos == 0 && preservedFrame.epoch == 777 && preservedBody.Size() == 1 && preservedBody.mData[0] == 0x99);
    }

    // Yellow hot state does not change sourcekind0. A retired/moved Torch is
    // an opaque identity, so no matching live Plant record is required here.
    NetBuffer hotWire;
    for (uint8_t phase = 0; phase <= 2; ++phase)
        for (bool stamped : {false, true})
        {
            auto hot = source;
            auto& data = hot.projectiles[0].data;
            data = {}; data.kind = 0; data.damage = 40; data.fireState = 1; data.specialType = phase;
            data.lastTorchId = stamped ? 65000 : 0;
            data.lastTorchToken = stamped ? 0xf1234567u : 0;
            const auto motion = phase == 1 ? MOTION_BACKWARDS : MOTION_STRAIGHT;
            NetBuffer record;
            record.WriteU16(hot.projectiles[0].id); record.WriteU8(PROJECTILE_FIREBALL); record.WriteU8(2);
            record.WriteF32(200); record.WriteF32(250); record.WriteF32(0); record.WriteF32(280);
            record.WriteI32(10000); record.WriteF32(0);
            record.WriteU8(static_cast<uint8_t>(motion)); record.WriteU8(0); record.WriteU8(0);record.WriteF32(0);
            CHECK(record.Size() == 35 && record.mData[2] == PROJECTILE_FIREBALL && record.mData[28] == motion);
            CHECK(FusionFireProjectileMetadataValid(data.kind, data.damage, data.sunOnHit, data.fireState,
                data.lastTorchId, data.lastTorchToken, static_cast<ProjectileType>(record.mData[2])));
            CHECK(!FusionFireProjectileMetadataValid(data.kind, data.damage, data.sunOnHit, data.fireState,
                data.lastTorchId, data.lastTorchToken, PROJECTILE_PEA));
            CHECK(ValidateNativeSnapshotFrame(hot, eligible));
            const auto originalRecord = record.mData;
            CHECK(EncodeNativeSnapshot(hot, eligible, record));
            NativeSnapshotFrame restored; NetBuffer restoredBody;
            CHECK(DecodeNativeSnapshot(record, eligible, 19, restored, restoredBody));
            CHECK(restored.projectiles.size() == 1 && restored.projectiles[0].data.kind == 0 &&
                restored.projectiles[0].data.damage == 40 && restored.projectiles[0].data.fireState == 1 &&
                restored.projectiles[0].data.specialType == phase && restored.projectiles[0].data.lastTorchId == data.lastTorchId &&
                restored.projectiles[0].data.lastTorchToken == data.lastTorchToken && restoredBody.mData == originalRecord);
            CHECK(FusionSplitProjectilePhaseValid(0, phase, static_cast<ProjectileMotion>(restoredBody.mData[28])));
            hotWire = record; hotWire.mReadPos = 0;
        }
    // ThreeTorch small is a kind0 hot marker, distinct from colorfulPuff13's
    // palette. Heating and IceTorch replacement preserve the native way/motion
    // and the opaque lastTorch generation, including the Rage way15.
    for(uint8_t phase : {kFusionThreeUpper,kFusionThreeLower,uint8_t(15)})
        for(uint16_t kind : {uint16_t(15),uint16_t(97)})
        {
            auto coldShot=source;auto& shot=coldShot.projectiles[0].data;
            shot={};shot.kind=kind;shot.specialType=phase;shot.lastTorchId=65000;shot.lastTorchToken=0xf1234567u;
            const auto motion=phase==15?MOTION_STAR:MOTION_THREEPEATER;
            CHECK(NativeProjectileMetadataMotion(shot)==motion && NativeProjectileMetadataType(shot)==PROJECTILE_PEA);
            CHECK(NativeProjectileMetadataValid(shot,PROJECTILE_PEA,motion));
            NetBuffer encoded;encoded.mData=original;
            const bool encodedOk=EncodeNativeSnapshot(coldShot,eligible,encoded);CHECK(encodedOk);
            if(!encodedOk)continue;
            NativeSnapshotFrame copy;NetBuffer restored;
            CHECK(DecodeNativeSnapshot(encoded,eligible,19,copy,restored) && copy.projectiles[0].data.kind==kind &&
                copy.projectiles[0].data.specialType==phase && copy.projectiles[0].data.lastTorchId==65000 &&
                copy.projectiles[0].data.lastTorchToken==0xf1234567u && restored.mData==original);
            auto forged=encoded;forged.mData[projectileMetadata+21]=1;RejectAtomically(forged);
            forged=encoded;forged.mData[projectileMetadata+14]=forged.mData[projectileMetadata+15]=0;RejectAtomically(forged);
        }
    for(uint8_t phase : {kFusionThreeUpper,kFusionThreeLower})
        for(const auto& tier : {std::pair<uint8_t,int>{1,30},{2,55},{3,70},{3,95}})
        {
            auto hot=source;auto& shot=hot.projectiles[0].data;
            shot={};shot.kind=0;shot.specialType=phase;shot.fireState=tier.first;shot.damage=tier.second;
            shot.effectVariant=1;shot.lastTorchId=65000;shot.lastTorchToken=0xf1234567u;
            CHECK(NativeProjectileMetadataValid(shot,PROJECTILE_FIREBALL,MOTION_THREEPEATER));
            NetBuffer encoded;encoded.mData=original;
            const bool encodedOk=EncodeNativeSnapshot(hot,eligible,encoded);CHECK(encodedOk);
            if(!encodedOk)continue;
            NativeSnapshotFrame copy;NetBuffer restored;
            CHECK(DecodeNativeSnapshot(encoded,eligible,19,copy,restored) && copy.projectiles[0].data.effectVariant==1 &&
                copy.projectiles[0].data.damage==tier.second && copy.projectiles[0].data.specialType==phase && restored.mData==original);
            auto forged=encoded;forged.mData[projectileMetadata+21]=2;RejectAtomically(forged);
            forged=encoded;forged.mData[projectileMetadata+21]=0;RejectAtomically(forged);
        }
    auto melted = source;
    auto& meltedData = melted.projectiles[0].data;
    meltedData = {}; meltedData.kind = 0; meltedData.lastTorchId = 65000; meltedData.lastTorchToken = 0xf1234567u;
    CHECK(ValidateNativeSnapshotFrame(melted, eligible));
    CHECK(!FusionFireProjectileMetadataValid(0, 20, 0, 0, 65000, 0xf1234567u, PROJECTILE_FIREBALL));
    NetBuffer meltedWire; meltedWire.mData = original;
    CHECK(EncodeNativeSnapshot(melted, eligible, meltedWire));
    NativeSnapshotFrame meltedDecoded; NetBuffer meltedBody;
    CHECK(DecodeNativeSnapshot(meltedWire, eligible, 19, meltedDecoded, meltedBody));
    CHECK(!meltedDecoded.projectiles[0].data.fireState && meltedDecoded.projectiles[0].data.damage == 20 &&
        meltedDecoded.projectiles[0].data.lastTorchId == 65000 && meltedDecoded.projectiles[0].data.lastTorchToken == 0xf1234567u);
    CHECK(hotWire.mData[projectileMetadata+13] == 1 && hotWire.mData[projectileMetadata+14] == (65000 & 255));
    // Source PuffTorch1061 now legitimately heats SplitPuff payload10 while
    // retaining its native phase and opaque Torch generation.
    auto hotPuff=source;auto& hotPuffData=hotPuff.projectiles[0].data;
    hotPuffData={};hotPuffData.kind=10;hotPuffData.damage=40;hotPuffData.fireState=1;hotPuffData.specialType=2;
    hotPuffData.lastTorchId=65000;hotPuffData.lastTorchToken=0xf1234567u;
    NetBuffer hotPuffWire;hotPuffWire.mData=original;
    CHECK(EncodeNativeSnapshot(hotPuff,eligible,hotPuffWire));
    NativeSnapshotFrame hotPuffDecoded;NetBuffer hotPuffBody;
    CHECK(DecodeNativeSnapshot(hotPuffWire,eligible,19,hotPuffDecoded,hotPuffBody) &&
        hotPuffDecoded.projectiles[0].data.kind==10 && hotPuffDecoded.projectiles[0].data.fireState==1 &&
        hotPuffDecoded.projectiles[0].data.damage==40 && hotPuffDecoded.projectiles[0].data.specialType==2 &&
        hotPuffDecoded.projectiles[0].data.lastTorchId==65000 && hotPuffDecoded.projectiles[0].data.lastTorchToken==0xf1234567u && hotPuffBody.mData==original);
    for (int variant = 0; variant < 10; ++variant)
    {
        NetBuffer forged = hotWire;
        auto write32 = [&](size_t offset, uint32_t value)
        {
            for (int byte = 0; byte < 4; ++byte) forged.mData[offset+byte] = static_cast<uint8_t>(value >> (byte*8));
        };
        if (variant == 0) forged.mData[projectileMetadata+13] = 2;
        if (variant == 1) { forged.mData[projectileMetadata+14] = 0; forged.mData[projectileMetadata+15] = 0; }
        if (variant == 2) write32(projectileMetadata+16, 0);
        if (variant == 3) forged.mData[projectileMetadata+2] = 1;
        if (variant == 4) forged.mData[projectileMetadata+2] = 15; // Coldkind15 cannot retain hot state1.
        if (variant == 5) write32(projectileMetadata+4, 20);
        if (variant == 6) write32(projectileMetadata+8, 5);
        if (variant == 7) forged.mData[projectileMetadata+13] = 0; // Ordinarykind0 cannot deal40.
        if (variant == 8)
        {
            forged.mData[projectileMetadata+13] = 0;
            forged.mData[projectileMetadata+2] = 1;
            write32(projectileMetadata+4, 60); // OrdinaryCherry still cannot carry a Torch stamp.
        }
        if (variant == 9) forged.mData[projectileMetadata+2] = 25; // Visualslot25 is not this sourcekind.
        const size_t metadataBytes = forged.mData[6] | (static_cast<size_t>(forged.mData[7]) << 8);
        const size_t payload = headerBytes + metadataBytes;
        forged.mData[payload+4] ^= 0x80;
        const auto hash = LawnNet::NetFnv1a32(forged.Data()+payload, forged.Size()-payload);
        for (int byte = 0; byte < 4; ++byte) forged.mData[10+byte] = static_cast<uint8_t>(hash >> (byte*8));
        CHECK(!DecodeNativeSnapshot(forged, eligible, 19, preservedFrame, preservedBody));
        CHECK(forged.mReadPos == 0 && preservedFrame.epoch == 777 && preservedBody.Size() == 1 && preservedBody.mData[0] == 0x99);
    }

    // A9-column peer must not accept or apply a10-column native world.
    NetBuffer oldGeometry = body;
    oldGeometry.mData[4] = 1; oldGeometry.mData[5] = 0;
    NativeSnapshotFrame oldFrame; oldFrame.epoch = 777;
    NetBuffer oldBody; oldBody.WriteU8(0x99);
    CHECK(!DecodeNativeSnapshot(oldGeometry, eligible, 19, oldFrame, oldBody));
    CHECK(oldGeometry.mReadPos == 0 && oldFrame.epoch == 777 && oldBody.Size() == 1 && oldBody.mData[0] == 0x99);

    // Choosing already owns real native packets. Committed describes the
    // selection phase, not whether any SeedPacket objects have been created.
    FusionSeedBank choosingBank;
    CHECK(choosingBank.Open(19, 1, eligible, 3));
    NativeSnapshotFrame choosing;
    choosing.epoch = 19; choosing.selection = choosingBank.State();
    CHECK(ValidateNativeSnapshotFrame(choosing, eligible));
    CHECK(choosingBank.Choose(19, 0, 0, false));
    choosing.selection = choosingBank.State();
    choosing.packets.push_back(source.packets[0]);
    CHECK(ValidateNativeSnapshotFrame(choosing, eligible));
    NetBuffer choosingWire; choosingWire.WriteU8(0x42);
    CHECK(EncodeNativeSnapshot(choosing, eligible, choosingWire));
    NativeSnapshotFrame choosingDecoded; NetBuffer choosingBody;
    CHECK(DecodeNativeSnapshot(choosingWire, eligible, 19, choosingDecoded, choosingBody));
    CHECK(!choosingDecoded.selectionCommitted && choosingDecoded.selection.phase == FusionBankPhase::Choosing && choosingDecoded.packets.size() == 1);
    choosing.selectionCommitted = true;
    CHECK(!ValidateNativeSnapshotFrame(choosing, eligible));
    auto uncommittedPlaying = source;
    uncommittedPlaying.selectionCommitted = false;
    CHECK(!ValidateNativeSnapshotFrame(uncommittedPlaying, eligible));
    NetBuffer falseCommitWire = body;
    falseCommitWire.mData[19] = 0;
    NativeSnapshotFrame previousCommit; previousCommit.epoch = 777;
    NetBuffer previousCommitBody; previousCommitBody.WriteU8(0x99);
    CHECK(!DecodeNativeSnapshot(falseCommitWire, eligible, 19, previousCommit, previousCommitBody));
    CHECK(falseCommitWire.mReadPos == 0 && previousCommit.epoch == 777 && previousCommitBody.Size() == 1 && previousCommitBody.mData[0] == 0x99);

    // Native level setup/free cards have no source bank packet. Their shared
    // owner is legal; an unowned purchased packet or mystery origin is not.
    auto freePlant = source;
    freePlant.plants[0].data.kind = 0;
    freePlant.plants[0].data.firstParent = freePlant.plants[0].data.secondParent = kFusionNoPlant;
    freePlant.plants[0].data.packetId = 0;
    freePlant.plants[0].data.owner = LawnNet::NET_SHARED_OWNER;
    CHECK(ValidateNativeSnapshotFrame(freePlant, eligible));
    freePlant.plants[0].data.randomOrigin = 1;
    CHECK(!ValidateNativeSnapshotFrame(freePlant, eligible));
    freePlant.plants[0].data.randomOrigin = 0;
    freePlant.plants[0].data.packetId = source.plants[0].data.packetId;
    CHECK(!ValidateNativeSnapshotFrame(freePlant, eligible));
    freePlant.plants[0].data.packetId = 0;
    freePlant.plants[0].data.owner = 4;
    CHECK(!ValidateNativeSnapshotFrame(freePlant, eligible));

    for (size_t length = 0; length < body.Size(); ++length)
    {
        NetBuffer truncated; truncated.mData.assign(body.mData.begin(), body.mData.begin() + length);
        NativeSnapshotFrame previous; previous.epoch = 777;
        NetBuffer previousBody; previousBody.WriteU8(0x99);
        CHECK(!DecodeNativeSnapshot(truncated, eligible, 19, previous, previousBody));
        CHECK(truncated.mReadPos == 0 && previous.epoch == 777 && previousBody.Size() == 1 && previousBody.mData[0] == 0x99);
    }
    for (size_t offset : {size_t(0), size_t(4), size_t(6), size_t(8), size_t(10), body.Size() - 1})
    {
        NetBuffer bad = body; bad.mData[offset] ^= 0x40;
        NativeSnapshotFrame previous; previous.epoch = 777;
        NetBuffer previousBody;
        CHECK(!DecodeNativeSnapshot(bad, eligible, 19, previous, previousBody));
        CHECK(bad.mReadPos == 0 && previous.epoch == 777 && previousBody.Size() == 0);
    }
    NetBuffer wrongEpoch = body;
    CHECK(!DecodeNativeSnapshot(wrongEpoch, eligible, 20, decoded, native));
    CHECK(wrongEpoch.mReadPos == 0);
    NetBuffer trailing = body; trailing.WriteU8(0);
    CHECK(!DecodeNativeSnapshot(trailing, eligible, 19, decoded, native));
    NetBuffer badBoolean = body; badBoolean.mData[19] = 2;
    CHECK(!DecodeNativeSnapshot(badBoolean, eligible, 19, decoded, native));

    auto invalid = source;
    invalid.plants[0].data.kind = 1032;
    NetBuffer untouched; untouched.WriteU8(0x7a);
    CHECK(!EncodeNativeSnapshot(invalid, eligible, untouched));
    CHECK(untouched.Size() == 1 && untouched.mData[0] == 0x7a);
    invalid = source; invalid.plants[0].data.firstParent = 1;
    CHECK(!EncodeNativeSnapshot(invalid, eligible, untouched));
    invalid = source; invalid.plants[0].data.idleSeconds = std::numeric_limits<float>::infinity();
    CHECK(!EncodeNativeSnapshot(invalid, eligible, untouched));
    invalid = source; invalid.packets[0].data.selectionId = 1234;
    CHECK(!EncodeNativeSnapshot(invalid, eligible, untouched));
    invalid = source; ++invalid.packets[0].data.price;
    CHECK(!EncodeNativeSnapshot(invalid, eligible, untouched));
    invalid = source; invalid.packets.push_back(invalid.packets[0]);
    CHECK(!EncodeNativeSnapshot(invalid, eligible, untouched));
    invalid = source; invalid.coins[0].data = 16;
    CHECK(!EncodeNativeSnapshot(invalid, eligible, untouched));
    invalid = source; invalid.projectiles[0].data.damage = 20;
    CHECK(!EncodeNativeSnapshot(invalid, eligible, untouched));
    NetBuffer oversized; oversized.mData.resize(LawnNet::NET_MAX_FRAME - 8, 0x44);
    CHECK(!EncodeNativeSnapshot(source, eligible, oversized));
    CHECK(oversized.Size() == LawnNet::NET_MAX_FRAME - 8 && oversized.mData[0] == 0x44);

    // Public mystery placeholders retain only origin/ownership through Intro.
    FusionSeedBank pendingBank;
    CHECK(pendingBank.Open(25, 1, eligible, 1));
    CHECK(pendingBank.ToggleMystery(25, 0, true));
    CHECK(pendingBank.SetReady(25, 0, true)); CHECK(pendingBank.CommitIntro(25));
    NativeSnapshotFrame pending;
    pending.epoch = 25; pending.selectionCommitted = true; pending.selection = pendingBank.State();
    for (const auto& selected : pending.selection.packets)
    {
        NativePacketData data;
        data.selectionId = selected.id; data.kind = kFusionNoPlant; data.owner = selected.owner; data.randomOrigin = selected.randomOrigin;
        pending.packets.push_back({(uint16_t)(selected.id + 10), data});
    }
    CHECK(ValidateNativeSnapshotFrame(pending, eligible));
    pending.selectionCommitted = false;
    CHECK(!ValidateNativeSnapshotFrame(pending, eligible));
    pending.selectionCommitted = true;
    pending.packets[0].data.isExtra = true;
    CHECK(!ValidateNativeSnapshotFrame(pending, eligible));
    // FNS18 extends existing real actors; source kinds with several movingWays
    // retain every old tuple while their new paths carry exact metadata.
    const Batch266Payload batch268Payloads[]={
        {20,40,6,0,PROJECTILE_PEA,MOTION_HOMING},{34,20,6,0,PROJECTILE_SPIKE,MOTION_HOMING},
        {35,40,6,0,PROJECTILE_SPIKE,MOTION_HOMING},{37,20,6,0,PROJECTILE_SPIKE,MOTION_HOMING},
        {37,20,7,0,PROJECTILE_SPIKE,MOTION_STAR},{37,20,0,0,PROJECTILE_SPIKE,MOTION_STRAIGHT},
        {29,20,0,0,PROJECTILE_PEA,MOTION_STRAIGHT},{29,20,4,0,PROJECTILE_PEA,MOTION_THREEPEATER},
        {29,20,5,0,PROJECTILE_PEA,MOTION_THREEPEATER},{32,40,0,0,PROJECTILE_PEA,MOTION_STRAIGHT},
        {32,40,4,0,PROJECTILE_PEA,MOTION_THREEPEATER},{32,40,5,0,PROJECTILE_PEA,MOTION_THREEPEATER},
        {32,40,255,0,PROJECTILE_PEA,MOTION_LOBBED},{41,20,2,0,PROJECTILE_STAR,MOTION_STAR},{41,20,9,0,PROJECTILE_STAR,MOTION_STAR},
        {65,1800,14,0,PROJECTILE_COBBIG,MOTION_LOBBED},{68,3600,14,0,PROJECTILE_COBBIG,MOTION_LOBBED},
        {69,1800,14,0,PROJECTILE_COBBIG,MOTION_LOBBED},{73,200,14,0,PROJECTILE_COBBIG,MOTION_LOBBED},
        {73,200,14,1,PROJECTILE_COBBIG,MOTION_LOBBED},{83,120,14,0,PROJECTILE_COBBIG,MOTION_LOBBED},
        {83,120,14,2,PROJECTILE_COBBIG,MOTION_LOBBED},{84,1800,14,0,PROJECTILE_COBBIG,MOTION_LOBBED},
        {85,360,0,0,PROJECTILE_KERNEL,MOTION_STRAIGHT},{72,120,0,0,PROJECTILE_MELON,MOTION_LOBBED},
        {72,120,0,1,PROJECTILE_MELON,MOTION_LOBBED},{77,60,0,0,PROJECTILE_KERNEL,MOTION_LOBBED},
        {79,80,0,0,PROJECTILE_BUTTER,MOTION_LOBBED},{80,450,0,0,PROJECTILE_MELON,MOTION_LOBBED},
        {82,120,0,0,PROJECTILE_MELON,MOTION_LOBBED},{6,80,1,0,PROJECTILE_PEA,MOTION_STRAIGHT},
        {89,300,0,0,PROJECTILE_PEA,MOTION_STRAIGHT},{94,10,16,0,PROJECTILE_PEA,MOTION_HOMING},{94,10,2,0,PROJECTILE_PEA,MOTION_STAR}};
    for(const auto& spec:batch268Payloads)
    {
        auto frame=source;auto& shot=frame.projectiles[0].data;shot={};shot.kind=spec.kind;shot.damage=spec.damage;shot.specialType=spec.way;shot.effectVariant=spec.variant;
        CHECK(NativeProjectileMetadataType(shot)==spec.type&&NativeProjectileMetadataMotion(shot)==spec.motion);
        CHECK(NativeProjectileMetadataValid(shot,spec.type,spec.motion));
        CHECK(!NativeProjectileMetadataValid(shot,spec.type==PROJECTILE_PEA?PROJECTILE_SPIKE:PROJECTILE_PEA,spec.motion));
        CHECK(!NativeProjectileMetadataValid(shot,spec.type,spec.motion==MOTION_STRAIGHT?MOTION_LOBBED:MOTION_STRAIGHT));
        NetBuffer encoded;encoded.mData=original;const bool ok=EncodeNativeSnapshot(frame,eligible,encoded);CHECK(ok);if(!ok)continue;
        NativeSnapshotFrame restored;NetBuffer opaque;CHECK(DecodeNativeSnapshot(encoded,eligible,19,restored,opaque));
        CHECK(restored.projectiles.size()==1&&restored.projectiles[0].data.kind==spec.kind&&restored.projectiles[0].data.specialType==spec.way&&opaque.mData==original);
        for(int corrupt=0;corrupt<4;++corrupt)
        {
            auto forged=encoded;
            if(corrupt==0)forged.mData[firstProjectile+4]=static_cast<uint8_t>(spec.damage+1);
            if(corrupt==1)forged.mData[firstProjectile+12]=254;
            if(corrupt==2)forged.mData[firstProjectile+21]=7;
            if(corrupt==3)forged.mData[firstProjectile+22]=1;
            RejectAtomically(forged);
        }
    }
    for(uint16_t kind:{uint16_t(61),uint16_t(62),uint16_t(63),uint16_t(64),uint16_t(72),uint16_t(80),uint16_t(82)})
    {
        auto frame=source;auto& shot=frame.projectiles[0].data;shot={};shot.kind=kind;shot.melonEnhanced=true;
        const auto* p=FindFusionLobberProjectileProfile(kind);CHECK(p!=nullptr);if(!p)continue;shot.damage=p->damage;
        CHECK(NativeProjectileMetadataValid(shot,p->nativeType,MOTION_LOBBED));NetBuffer encoded;encoded.mData=original;CHECK(EncodeNativeSnapshot(frame,eligible,encoded));
    }
    // Free typed actors use the existing native level-factory ancestry rule;
    // actual purchase/owner/placement qualification belongs to grouped runtime.
    for(uint16_t kind:{uint16_t(25),uint16_t(1100),uint16_t(1101),uint16_t(1102),uint16_t(1092),uint16_t(1097),uint16_t(1098),uint16_t(1099),uint16_t(935),uint16_t(1027),uint16_t(1028),uint16_t(1029),uint16_t(1154),
        uint16_t(15),uint16_t(1049),uint16_t(1050),uint16_t(1051),uint16_t(1066),uint16_t(1156),uint16_t(252),uint16_t(1067),uint16_t(1068),uint16_t(1069),uint16_t(1076),uint16_t(1083),
        uint16_t(1120),uint16_t(1122),uint16_t(1123),uint16_t(1132),uint16_t(1139),uint16_t(1142),uint16_t(914),uint16_t(915),uint16_t(1013),uint16_t(903),uint16_t(921),uint16_t(33),uint16_t(1177),uint16_t(1129),uint16_t(1162)})
    {
        auto frame=source;auto& actor=frame.plants[0].data;actor={};actor.kind=kind;actor.instanceToken=0x10001;actor.owner=LawnNet::NET_SHARED_OWNER;
        if(const auto* cannon=FindFusionCannon268Profile(kind);cannon&&cannon->manual)actor.productionTicks=kind==1120?501:51;
        if(kind==1102){actor.batchMode=kEquipmentBlow;actor.shootSeconds=0;}
        if(kind==1076||kind==1083)actor.productionTicks=kind==1076?12001:6001;
        if(kind==1013){actor.batchBagCount=6;actor.productionTicks=4000;}
        if(FindFusionStrongChomperProfile(kind))actor.pendingProductionTicks=101;
        if(IsFusionShulk268Kind(kind))actor.productionTicks=51;
        NetBuffer encoded;encoded.mData=original;const bool ok=EncodeNativeSnapshot(frame,eligible,encoded);CHECK(ok);if(!ok)continue;
        NativeSnapshotFrame restored;NetBuffer opaque;CHECK(DecodeNativeSnapshot(encoded,eligible,19,restored,opaque));
        CHECK(restored.plants.size()==1&&restored.plants[0].data.kind==kind&&opaque.mData==original);
        if(!IsFusionExtras268Kind(kind))for(float speed:{.9f,1.1f}){auto edge=frame;edge.plants[0].data.animationSpeed=speed;CHECK(ValidateNativeSnapshotFrame(edge,eligible));}
        {auto bad=frame;bad.plants[0].data.animationSpeed=1.1001f;CHECK(!ValidateNativeSnapshotFrame(bad,eligible));}
        for(int invalid=0;invalid<3;++invalid)
        {auto forged=encoded;if(invalid==0)forged.mData[firstPlant+47]=255;if(invalid==1)forged.mData[firstPlant+43]=255;if(invalid==2)forged.mData[firstPlant+48]=1;RejectAtomically(forged);}
    }
    // Grap count survives duration expiry; unbounded checked source stacking is
    // represented without inventing a narrower cap or a second cold clock.
    for(const auto pair:{std::pair{0,0},std::pair{0,31},std::pair{200,1},std::pair{1200,6},std::pair{(std::numeric_limits<int>::max)(),(std::numeric_limits<int>::max)()}})
    {
        auto frame=moneyFrame;frame.zombieActions[0].data.waterGrapTicks=pair.first;frame.zombieActions[0].data.waterGrapCount=pair.second;
        NetBuffer encoded;encoded.mData=original;const bool ok=EncodeNativeSnapshot(frame,eligible,encoded);CHECK(ok);if(!ok)continue;
        NativeSnapshotFrame restored;NetBuffer opaque;CHECK(DecodeNativeSnapshot(encoded,eligible,19,restored,opaque));
        CHECK(restored.zombieActions[0].data.waterGrapTicks==pair.first&&restored.zombieActions[0].data.waterGrapCount==pair.second);
        for(int field=0;field<2;++field){auto forged=encoded;for(int byte=0;byte<4;++byte)forged.mData[firstAction+(field?25:21)+byte]=255;RejectAtomically(forged);}
        if(pair.first>0){auto forged=encoded;for(int byte=0;byte<4;++byte)forged.mData[firstAction+25+byte]=0;RejectAtomically(forged);}
    }
    {
        auto frame=source;auto& plant=frame.plants[0].data;plant={};plant.kind=15;plant.instanceToken=0x10001;plant.owner=LawnNet::NET_SHARED_OWNER;
        plant.batchMode=1;plant.productionTicks=20;plant.waterTargetId=555;plant.waterTargetToken=0x10003;frame.zombieIdentities={{555,0x10003}};
        NetBuffer encoded;encoded.mData=original;const bool ok=EncodeNativeSnapshot(frame,eligible,encoded);CHECK(ok);
        if(ok)
        {
            CHECK(encoded.mData[firstPlant+48]==43&&encoded.mData[firstPlant+49]==2&&encoded.mData[firstPlant+50]==3);
            for(int invalid=0;invalid<4;++invalid){auto forged=encoded;if(invalid==0){forged.mData[firstPlant+50]=forged.mData[firstPlant+52]=0;}if(invalid==1)forged.mData[firstPlant+47]=0;if(invalid==2)forged.mData[firstPlant+12]=255;if(invalid==3)forged.mData[firstPlant+50]^=1;RejectAtomically(forged);}
            auto duplicate=frame;auto copy=duplicate.plants[0];copy.id=22;copy.data.instanceToken=0x10002;duplicate.plants.push_back(copy);CHECK(!ValidateNativeSnapshotFrame(duplicate,eligible));
            copy.data.kind=1050;copy.data.batchMode=3;copy.data.productionTicks=0;copy.data.shootSeconds=0;duplicate.plants.back()=copy;CHECK(ValidateNativeSnapshotFrame(duplicate,eligible));
            auto missing=frame;missing.zombieIdentities.clear();CHECK(!ValidateNativeSnapshotFrame(missing,eligible));
        }
    }
    {
        auto frame=source;auto& plant=frame.plants[0].data;plant={};plant.kind=25;plant.instanceToken=0x10001;plant.owner=LawnNet::NET_SHARED_OWNER;
        NativeEquipmentData item;item.type=FusionEquipmentType::Bucket;item.factoryType=FusionEquipmentType::Door;
        item.phase=NativeEquipmentPhase::Attached;item.instanceToken=0x10007;item.sourcePlantId=21;item.sourcePlantToken=plant.instanceToken;
        item.anchorX=200;item.anchorY=250;item.gravity=9.8f;item.disappear=false;item.attachmentBasis={2,1,-1,.5f};
        NativeEquipmentPlantState magnet;magnet.targetItemId=501;magnet.targetItemToken=item.instanceToken;magnet.lastCapturedType=FusionEquipmentType::Door;
        frame.equipmentCoins={{501,item}};frame.equipmentPlants={{21,magnet}};
        NetBuffer encoded;encoded.mData=original;const bool ok=EncodeNativeSnapshot(frame,eligible,encoded);CHECK(ok);
        if(ok)
        {
            NativeSnapshotFrame restored;NetBuffer opaque;CHECK(DecodeNativeSnapshot(encoded,eligible,19,restored,opaque));
            CHECK(restored.equipmentCoins.size()==1&&restored.equipmentPlants.size()==1&&restored.equipmentCoins[0].data.type==FusionEquipmentType::Bucket&&
                restored.equipmentCoins[0].data.factoryType==FusionEquipmentType::Door&&restored.equipmentCoins[0].data.attachmentBasis==item.attachmentBasis&&opaque.mData==original);
            const size_t end=14+(encoded.mData[6]|(size_t(encoded.mData[7])<<8));
            constexpr size_t equipmentBytes=67,equipmentPlantBytes=18; // FNS20 weapon Bool precedes basis.
            const size_t firstEquipment=end-empty269FamilyCountBytes-2-2-equipmentPlantBytes-equipmentBytes,
                firstEquipmentPlant=end-empty269FamilyCountBytes-2-equipmentPlantBytes;
            CHECK(encoded.mData[firstEquipment]==245&&encoded.mData[firstEquipment+2]==0&&encoded.mData[firstEquipment+3]==10&&
                encoded.mData[firstEquipment+4]==3&&encoded.mData[firstEquipment+48]==0&&encoded.mData[firstEquipment+50]==0&&encoded.mData[firstEquipmentPlant+8]==245);
            for(int invalid=0;invalid<19;++invalid)
            {
                auto forged=encoded;auto write=[&](size_t offset,uint32_t value){for(int byte=0;byte<4;++byte)forged.mData[offset+byte]=uint8_t(value>>(byte*8));};
                if(invalid==0)forged.mData[firstEquipment+2]=255;
                if(invalid==1)forged.mData[firstEquipment+3]=255;
                if(invalid==2)forged.mData[firstEquipment+4]=255;
                if(invalid==3)write(firstEquipment+5,0);
                if(invalid==4)forged.mData[firstEquipment+9]=forged.mData[firstEquipment+10]=0;
                if(invalid==5)write(firstEquipment+11,0);
                if(invalid==6)forged.mData[firstEquipment+15]=0;
                if(invalid==7)write(firstEquipment+16,0x7fc00000);
                if(invalid==8)write(firstEquipment+40,0);
                if(invalid==9)write(firstEquipment+44,0xffffffff);
                if(invalid==10)forged.mData[firstEquipment+48]=2;
                if(invalid==11)forged.mData[firstEquipment+49]=255;
                if(invalid==12)write(firstEquipment+51,0x7fc00000);
                if(invalid==13)for(int m=0;m<4;++m)write(firstEquipment+51+m*4,0);
                if(invalid==14)write(firstEquipment+11,0x10002);
                if(invalid==15)write(firstEquipmentPlant+10,0x10008);
                if(invalid==16)forged.mData[firstEquipmentPlant+16]=2;
                if(invalid==17)forged.mData[firstEquipmentPlant+17]=0; // Only1154 owns pending RNG draw.
                if(invalid==18)forged.mData[firstEquipment+50]=2; // Noncanonical weapon Bool.
                RejectAtomically(forged);
            }
            for(int invalid=0;invalid<5;++invalid)
            {
                auto bad=frame;
                if(invalid==0)bad.coins.push_back({501,{25,false}});
                if(invalid==1){auto copy=bad.equipmentCoins[0];copy.id=502;copy.data.instanceToken=0x10008;bad.equipmentCoins.push_back(copy);}
                if(invalid==2)bad.equipmentPlants[0].id=22;
                if(invalid==3)bad.equipmentCoins[0].data.sourcePlantToken=0x10002;
                if(invalid==4)bad.equipmentCoins[0].data.factoryType=FusionEquipmentType::Helmet;
                NetBuffer untouched;untouched.mData=original;CHECK(!EncodeNativeSnapshot(bad,eligible,untouched)&&untouched.mData==original);
            }
        }
        for(int type=0;type<=10;++type)for(auto phase:{NativeEquipmentPhase::Falling,NativeEquipmentPhase::World,NativeEquipmentPhase::Cursor})
        {
            auto world=source;NativeEquipmentData v;v.type=v.factoryType=static_cast<FusionEquipmentType>(type);v.phase=phase;v.instanceToken=0x10007;
            v.gravity=9.8f;v.claimSlot=phase==NativeEquipmentPhase::Cursor?0:255;world.equipmentCoins={{501,v}};
            NetBuffer encoded;encoded.mData=original;const bool ok=EncodeNativeSnapshot(world,eligible,encoded);CHECK(ok);if(!ok)continue;
            NativeSnapshotFrame restored;NetBuffer opaque;CHECK(DecodeNativeSnapshot(encoded,eligible,19,restored,opaque));CHECK(restored.equipmentCoins[0].data.type==v.type&&opaque.mData==original);
            if(phase==NativeEquipmentPhase::Cursor)
            {auto duplicate=world;auto copy=duplicate.equipmentCoins[0];copy.id=502;copy.data.instanceToken=0x10008;duplicate.equipmentCoins.push_back(copy);CHECK(!ValidateNativeSnapshotFrame(duplicate,eligible));}
        }
    }
    {
        auto frame=source;frame.zombieIdentities={{555,0x10003}};
        NetBuffer encoded;encoded.mData=original;const bool ok=EncodeNativeSnapshot(frame,eligible,encoded);CHECK(ok);
        if(ok)
        {
            NativeSnapshotFrame restored;NetBuffer opaque;CHECK(DecodeNativeSnapshot(encoded,eligible,19,restored,opaque));
            CHECK(restored.zombieIdentities.size()==1&&restored.zombieIdentities[0].id==555&&restored.zombieIdentities[0].data==0x10003&&opaque.mData==original);
            const size_t end=14+(encoded.mData[6]|(size_t(encoded.mData[7])<<8)),first=end-empty269FamilyCountBytes-6;
            for(int invalid=0;invalid<2;++invalid){auto forged=encoded;if(invalid==0)forged.mData[first]=forged.mData[first+1]=0;else for(int byte=0;byte<4;++byte)forged.mData[first+2+byte]=0;RejectAtomically(forged);}
        }
        for(int invalid=0;invalid<3;++invalid)
        {auto bad=frame;if(invalid==0)bad.zombieIdentities.push_back({556,0x10003});if(invalid==1)bad.zombieIdentities.push_back({555,0x10004});if(invalid==2)bad.zombieIdentities[0].data=0;CHECK(!ValidateNativeSnapshotFrame(bad,eligible));}
        frame.zombieIdentities.clear();for(int i=1;i<=1024;++i)frame.zombieIdentities.push_back({uint16_t(i),uint32_t(0x10000+i)});
        NetBuffer maximum;maximum.mData=original;CHECK(EncodeNativeSnapshot(frame,eligible,maximum));
        frame.zombieIdentities.push_back({1025,0x10401});CHECK(!ValidateNativeSnapshotFrame(frame,eligible));
    }
    for(uint16_t kind:{uint16_t(915),uint16_t(1139),uint16_t(1129)})
    {
        auto frame=source;auto& actor=frame.plants[0].data;actor={};actor.kind=kind;actor.instanceToken=0x10001;actor.owner=LawnNet::NET_SHARED_OWNER;
        if(kind==915){actor.batchMode=2;actor.pendingProductionTicks=117;}
        if(kind==1139){actor.batchMode=1;actor.batchRageTicks=201;actor.shootSeconds=0;}
        if(kind==1129){actor.batchMode=2;actor.damageStage=2;}
        NetBuffer encoded;encoded.mData=original;const bool ok=EncodeNativeSnapshot(frame,eligible,encoded);CHECK(ok);if(!ok)continue;
        NativeSnapshotFrame restored;NetBuffer opaque;CHECK(DecodeNativeSnapshot(encoded,eligible,19,restored,opaque));
        CHECK(restored.plants[0].data.kind==kind&&restored.plants[0].data.batchMode==actor.batchMode&&restored.plants[0].data.pendingProductionTicks==actor.pendingProductionTicks);
        auto forged=encoded;
        if(kind==915)forged.mData[firstPlant+16]=118;
        if(kind==1139)forged.mData[firstPlant+12]=1;
        if(kind==1129)forged.mData[firstPlant+47]=3;
        RejectAtomically(forged);
    }
    // Protocol57 keeps native clock fields exact across the old U16 lifetime
    // boundary. These predicates are also used by real Board raw preflight.
    {
        NativeEquipmentData d;d.phaseStartAge=0;
        for(const auto sample:{std::pair{0,0},std::pair{31,30},std::pair{65535,65535},std::pair{65536,65536},std::pair{(std::numeric_limits<int>::max)(),(std::numeric_limits<int>::max)()}})
        {
            const uint16_t prefix=uint16_t((std::min)(65535,sample.second));
            CHECK(NativeEquipmentRawClockValid(d,sample.first,sample.second,prefix));
            NetBuffer coin;coin.WriteU16(501);coin.WriteU8(COIN_NONE);coin.WriteU8(COIN_MOTION_COIN);coin.WriteF32(200);coin.WriteF32(250);
            coin.WriteI32(10000);coin.WriteU16(4096);coin.WriteU16(0);coin.WriteU8(0);coin.WriteU8(255);coin.WriteU8(0);coin.WriteU16(prefix);
            for(int i=0;i<5;++i)coin.WriteU8(255);coin.WriteI32(sample.first);coin.WriteI32(sample.second);coin.WriteU8(0); // Actual mTimesDropped.
            CHECK(coin.Size()==39&&coin.mData[2]==COIN_NONE&&coin.mData[23]==uint8_t(prefix));
            coin.mReadPos=30;const int age=coin.ReadI32Or(),life=coin.ReadI32Or();uint8_t uses=255;CHECK(coin.ReadU8(&uses)&&!uses&&age==sample.first&&life==sample.second&&NativeEquipmentRawClockValid(d,age,life,prefix));
            CHECK(!NativeEquipmentRawClockValid(d,sample.first,sample.second,uint16_t(prefix^1)));
        }
        CHECK(!NativeEquipmentRawClockValid(d,-1,0,0)&&!NativeEquipmentRawClockValid(d,0,-1,0)&&!NativeEquipmentRawClockValid(d,1,2,2));
        d.phaseStartAge=2;CHECK(!NativeEquipmentRawClockValid(d,1,0,0));d.phaseStartAge=-1;CHECK(!NativeEquipmentRawClockValid(d,1,0,0));
    }
    for(uint16_t kind:{uint16_t(65),uint16_t(68),uint16_t(69),uint16_t(73),uint16_t(83),uint16_t(84)})
    {
        NativeProjectileData d;d.kind=kind;d.specialType=14;
        for(float rotation:{kFusionCannon268UpRotation,kFusionCannon268DownRotation})
        {
            CHECK(NativeProjectileRawRotationValid(d,PROJECTILE_COBBIG,MOTION_LOBBED,rotation,0));
            CHECK(!NativeProjectileRawRotationValid(d,PROJECTILE_KERNEL,MOTION_LOBBED,rotation,0));
            CHECK(!NativeProjectileRawRotationValid(d,PROJECTILE_COBBIG,MOTION_STRAIGHT,rotation,0));
            CHECK(!NativeProjectileRawRotationValid(d,PROJECTILE_COBBIG,MOTION_LOBBED,rotation,1));
        }
        CHECK(!NativeProjectileRawRotationValid(d,PROJECTILE_COBBIG,MOTION_LOBBED,0,0));
        CHECK(!NativeProjectileRawRotationValid(d,PROJECTILE_COBBIG,MOTION_LOBBED,(std::numeric_limits<float>::quiet_NaN)(),0));
    }
    for(uint8_t way:{uint8_t(16),uint8_t(2)})
    {
        NativeProjectileData d;d.kind=94;d.specialType=way;const auto motion=way==16?MOTION_HOMING:MOTION_STAR;
        for(float phase:{-6.283186f,0.f,6.283186f})CHECK(NativeProjectileRawRotationValid(d,PROJECTILE_PEA,motion,.5f,phase));
        CHECK(!NativeProjectileRawRotationValid(d,PROJECTILE_PEA,motion,.5f,6.2832f));
        CHECK(!NativeProjectileRawRotationValid(d,PROJECTILE_PEA,motion,.5f,(std::numeric_limits<float>::infinity)()));
        CHECK(!NativeProjectileRawRotationValid(d,PROJECTILE_SPIKE,motion,.5f,0));
    }
    // Shared ground-terrain admission must keep real Pool/Fog lanes hittable,
    // while a stray Day swimmer or an out-of-lawn row remains inadmissible.
    for(uint8_t background:{uint8_t(BACKGROUND_1_DAY),uint8_t(BACKGROUND_3_POOL),uint8_t(BACKGROUND_4_FOG)})
        for(int row=-1;row<=6;++row)
        {
            const bool valid=row>=0&&row<NativeFusionRowsForBackground(background);
            CHECK(NativeFusionGroundTerrainAllowed(background,row,false)==valid);
            CHECK(NativeFusionGroundTerrainAllowed(background,row,true)==
                (background!=BACKGROUND_1_DAY&&(row==2||row==3)));
        }
    CHECK(!NativeFusionGroundTerrainAllowed(255,2,false));
    CHECK(!NativeFusionGroundTerrainAllowed(255,2,true));
    // Source41 root ray and independently integrated child angle share the
    // canonical raw record. A late join never reconstructs it from local age.
    for(uint8_t way:{uint8_t(2),uint8_t(9)})
    {
        NativeProjectileData d;d.kind=41;d.damage=20;d.specialType=way;
        for(float angle:{0.f,1.5707964f,6.283186f})
            CHECK(NativeProjectileRawRotationValid(d,PROJECTILE_STAR,MOTION_STAR,.5f,angle));
        for(float angle:{-.00001f,6.2832f,(std::numeric_limits<float>::quiet_NaN)(),(std::numeric_limits<float>::infinity)()})
            CHECK(!NativeProjectileRawRotationValid(d,PROJECTILE_STAR,MOTION_STAR,.5f,angle));
        CHECK(!NativeProjectileRawRotationValid(d,PROJECTILE_PEA,MOTION_STAR,.5f,0));
        CHECK(!NativeProjectileRawRotationValid(d,PROJECTILE_STAR,MOTION_STRAIGHT,.5f,0));
        CHECK(!NativeProjectileRawRotationValid(d,PROJECTILE_STAR,MOTION_STAR,(std::numeric_limits<float>::quiet_NaN)(),0));
        auto bad=d;bad.damage=21;CHECK(!NativeProjectileRawRotationValid(bad,PROJECTILE_STAR,MOTION_STAR,.5f,0));
        bad=d;bad.specialType=0;CHECK(!NativeProjectileRawRotationValid(bad,PROJECTILE_STAR,MOTION_STAR,.5f,0));
        bad=d;bad.isZombieBullet=true;CHECK(!NativeProjectileRawRotationValid(bad,PROJECTILE_STAR,MOTION_STAR,.5f,0));
        bad=d;bad.effectVariant=1;CHECK(!NativeProjectileRawRotationValid(bad,PROJECTILE_STAR,MOTION_STAR,.5f,0));
    }
    for(uint8_t way:{uint8_t(2),uint8_t(9),uint8_t(6)})
    {
        NativeProjectileData d;d.kind=42;d.damage=20;d.specialType=way;
        const auto motion=way==6?MOTION_HOMING:MOTION_STAR;
        CHECK(NativeProjectileRawRotationValid(d,PROJECTILE_STAR,motion,.5f,0));
        CHECK(NativeProjectileRawRotationValid(d,PROJECTILE_STAR,motion,.5f,6.283186f));
        CHECK(!NativeProjectileRawRotationValid(d,PROJECTILE_STAR,motion,.5f,-.01f));
        CHECK(!NativeProjectileRawRotationValid(d,PROJECTILE_STAR,motion,.5f,6.2832f));
        CHECK(!NativeProjectileRawRotationValid(d,PROJECTILE_STAR,motion,.5f,(std::numeric_limits<float>::quiet_NaN)()));
        CHECK(!NativeProjectileRawRotationValid(d,PROJECTILE_PEA,motion,.5f,0));
        CHECK(!NativeProjectileRawRotationValid(d,PROJECTILE_STAR,MOTION_STRAIGHT,.5f,0));
    }
    {NativeProjectileData d;d.kind=85;CHECK(NativeProjectileRawRotationValid(d,PROJECTILE_KERNEL,MOTION_STRAIGHT,0,0));}
    // Source null-target coroutine keeps its natural grab/sink clocks. Only
    // the complete nullable reference detaches; mixed pairs remain malformed.
    for(uint8_t mode:{uint8_t(1),uint8_t(2)})
    {
        auto frame=source;auto& actor=frame.plants[0].data;actor={};actor.kind=15;actor.instanceToken=0x10001;actor.owner=LawnNet::NET_SHARED_OWNER;
        actor.batchMode=mode;actor.productionTicks=mode==1?20:0;actor.pendingProductionTicks=mode==2?25:0;
        NetBuffer encoded;encoded.mData=original;const bool ok=EncodeNativeSnapshot(frame,eligible,encoded);CHECK(ok);if(!ok)continue;
        NativeSnapshotFrame restored;NetBuffer opaque;CHECK(DecodeNativeSnapshot(encoded,eligible,19,restored,opaque));
        CHECK(restored.plants[0].data.batchMode==mode&&!restored.plants[0].data.waterTargetId&&!restored.plants[0].data.waterTargetToken);
        auto mixed=encoded;mixed.mData[firstPlant+48]=1;RejectAtomically(mixed);
    }
    {
        // SetParent(Board,true) retains its current world basis after Drop.
        auto frame=source;NativeEquipmentData dropped;dropped.type=dropped.factoryType=FusionEquipmentType::Bucket;
        dropped.phase=NativeEquipmentPhase::World;dropped.instanceToken=0x10009;dropped.gravity=9.8f;dropped.attachmentBasis={2,1,-1,.5f};frame.equipmentCoins={{501,dropped}};
        NetBuffer encoded;encoded.mData=original;const bool ok=EncodeNativeSnapshot(frame,eligible,encoded);CHECK(ok);
        if(ok){NativeSnapshotFrame restored;NetBuffer opaque;CHECK(DecodeNativeSnapshot(encoded,eligible,19,restored,opaque));CHECK(restored.equipmentCoins[0].data.attachmentBasis==dropped.attachmentBasis&&!restored.equipmentCoins[0].data.sourcePlantId);}
    }
    // FNS21 cards bind a real Coin identity, with canonical serialized Bools
    // and one authenticated cursor lease shared with equipment/glove tools.
    {
        NativeDroppedCard269Data card;card.kind=0;card.instanceToken=0x20001;card.heldSlot=1;card.landed=true;
        auto frame=source;frame.droppedCards269={{501,card}};
        NetBuffer encoded;encoded.mData=original;const bool ok=EncodeNativeSnapshot(frame,eligible,encoded);CHECK(ok);
        if(ok)
        {
            NativeSnapshotFrame decoded;NetBuffer opaque;CHECK(DecodeNativeSnapshot(encoded,eligible,19,decoded,opaque));
            CHECK(decoded.droppedCards269.size()==1&&decoded.droppedCards269[0].id==501&&decoded.droppedCards269[0].data.instanceToken==card.instanceToken&&decoded.droppedCards269[0].data.heldSlot==1&&decoded.droppedCards269[0].data.landed&&!decoded.droppedCards269[0].data.flashing&&opaque.mData==original);
            const size_t end=14+(encoded.mData[6]|(size_t(encoded.mData[7])<<8)),firstCard=end-emptyNight291CountBytes-24-4-15;
            CHECK(encoded.mData[firstCard]==(501&255)&&encoded.mData[firstCard+12]==1&&encoded.mData[firstCard+13]==1&&encoded.mData[firstCard+14]==0);
            for(size_t flag:{firstCard+13,firstCard+14})for(uint8_t invalid:{uint8_t(2),uint8_t(255)})
            {auto forged=encoded;forged.mData[flag]=invalid;RejectAtomically(forged);}
        }
        for(int invalid=0;invalid<5;++invalid)
        {
            auto bad=frame;
            if(invalid==0)bad.droppedCards269[0].data.landed=false;
            if(invalid==1)bad.droppedCards269[0].data.heldSlot=2; // Inactive slot.
            if(invalid==2){auto duplicate=bad.droppedCards269[0];duplicate.id=502;bad.droppedCards269.push_back(duplicate);} // Duplicate token and lease.
            if(invalid==3){auto duplicate=bad.droppedCards269[0];duplicate.id=502;++duplicate.data.instanceToken;bad.droppedCards269.push_back(duplicate);} // Distinct Coin, same cursor.
            if(invalid==4)bad.coins.push_back({501,{5,false}}); // One raw Coin cannot be both card and sun.
            CHECK(!ValidateNativeSnapshotFrame(bad,eligible));
        }
        auto gloveOnly=source;gloveOnly.gloveHolds[1]={gloveOnly.plants[0].id,gloveOnly.plants[0].data.instanceToken};
        CHECK(ValidateNativeSnapshotFrame(gloveOnly,eligible));gloveOnly.droppedCards269=frame.droppedCards269;CHECK(!ValidateNativeSnapshotFrame(gloveOnly,eligible));
        auto equipmentOnly=source;NativeEquipmentData held;held.type=held.factoryType=FusionEquipmentType::Bucket;held.gravity=9.8f;held.instanceToken=0x20003;held.phase=NativeEquipmentPhase::Cursor;held.claimSlot=1;
        equipmentOnly.equipmentCoins={{503,held}};CHECK(ValidateNativeSnapshotFrame(equipmentOnly,eligible));
        equipmentOnly.droppedCards269=frame.droppedCards269;CHECK(!ValidateNativeSnapshotFrame(equipmentOnly,eligible));
        for(uint16_t kind:{uint16_t(241),uint16_t(244),uint16_t(245),uint16_t(250),uint16_t(1175),uint16_t(255),uint16_t(242),uint16_t(249),uint16_t(254),uint16_t(34),uint16_t(1174)})
        {auto available=frame;available.droppedCards269[0].data.kind=kind;CHECK(GetNativeSeedForPlant(kind)!=SEED_NONE&&ValidateNativeSnapshotFrame(available,eligible));}
        auto wheatCard=frame;wheatCard.droppedCards269[0].data.kind=253;CHECK(GetNativeSeedForPlant(253)==SEED_SUNFLOWER&&ValidateNativeSnapshotFrame(wheatCard,eligible));
        auto emperorCard=frame;emperorCard.droppedCards269[0].data.kind=900;CHECK(GetNativeSeedForPlant(900)==SEED_HYPNOSHROOM&&ValidateNativeSnapshotFrame(emperorCard,eligible));
        auto unavailable=frame;unavailable.droppedCards269[0].data.kind=938;CHECK(GetNativeSeedForPlant(938)==SEED_NONE&&!ValidateNativeSnapshotFrame(unavailable,eligible));
        auto uses=card;uses.heldSlot=255;CHECK(NativeDroppedCard269CoinStateValid(uses,20,0,0));CHECK(!NativeDroppedCard269CoinStateValid(uses,20,1,0));
        uses.kind=6;CHECK(NativeDroppedCard269CoinStateValid(uses,20,2,0));CHECK(!NativeDroppedCard269CoinStateValid(uses,20,3,0));
    }
    // One real native Plant carries physical255 and an independent factory/art
    // identity. Signed source HP survives even though the legacy raw prefix isU16.
    for(uint16_t factory:{uint16_t(251),uint16_t(255)})for(int health:{4000,0,-25})
    {
        auto frame=source;auto& plant=frame.plants[0].data;plant={};plant.kind=255;plant.owner=LawnNet::NET_SHARED_OWNER;plant.instanceToken=0x10001;
        NativeRollingActor269Snapshot rolling;rolling.factoryKind=factory;rolling.instanceToken=plant.instanceToken;rolling.bornTick=123;
        rolling.originX=100;rolling.originY=200;rolling.rootOffsetX=40;rolling.rootOffsetY=5;rolling.health=health;
        rolling.lastTick=456;rolling.directDestroy=true;rolling.touching={0x10002};rolling.damageHistory={{0x10002,600}}; // Host-only, not wire-authored.
        frame.rollingPlants269={{frame.plants[0].id,rolling}};
        NetBuffer encoded;encoded.mData=original;const bool ok=EncodeNativeSnapshot(frame,eligible,encoded);CHECK(ok);if(!ok)continue;
        NativeSnapshotFrame restored;NetBuffer opaque;CHECK(DecodeNativeSnapshot(encoded,eligible,19,restored,opaque));
        CHECK(restored.rollingPlants269.size()==1&&restored.rollingPlants269[0].id==frame.plants[0].id&&restored.rollingPlants269[0].data.factoryKind==factory&&restored.rollingPlants269[0].data.kind==255&&restored.rollingPlants269[0].data.health==health&&restored.rollingPlants269[0].data.instanceToken==plant.instanceToken&&opaque.mData==original);
        if(restored.rollingPlants269.size()==1)CHECK(restored.rollingPlants269[0].data.lastTick==-1&&!restored.rollingPlants269[0].data.directDestroy&&restored.rollingPlants269[0].data.touching.empty()&&restored.rollingPlants269[0].data.damageHistory.empty());
        for(int invalid=0;invalid<5;++invalid)
        {
            auto bad=frame;
            if(invalid==0)bad.rollingPlants269.clear();
            if(invalid==1)++bad.rollingPlants269[0].id;
            if(invalid==2)++bad.rollingPlants269[0].data.instanceToken;
            if(invalid==3)bad.rollingPlants269[0].data.factoryKind=253;
            if(invalid==4)bad.plants.clear();
            CHECK(!ValidateNativeSnapshotFrame(bad,eligible));
        }
        const size_t end=14+(encoded.mData[6]|(size_t(encoded.mData[7])<<8)),firstRolling=end-emptyNight291CountBytes-24-2-34;
        auto forged=encoded;forged.mData[firstRolling+6]^=1;RejectAtomically(forged); // Mismatched opaque actor token.
    }
    // FNS22 Fert extends a real Coin. Its nullable target is an opaque current
    // Plant generation; cursor ownership shares the existing tool lease rules.
    {
        auto frame=source;NativeFertilize269Data item;item.instanceToken=0x70001;
        item.targetId=frame.plants[0].id;item.targetToken=frame.plants[0].data.instanceToken;
        item.phase=NativeFertilize269Phase::Using;item.phaseStartAge=10;item.anchorX=125;item.anchorY=250;
        frame.fertilizeCoins269={{541,item}};CHECK(NativeFertilize269DataValid(item));
        NetBuffer encoded;encoded.mData=original;const bool ok=EncodeNativeSnapshot(frame,eligible,encoded);CHECK(ok);
        if(ok)
        {
            NativeSnapshotFrame copy;NetBuffer opaque;CHECK(DecodeNativeSnapshot(encoded,eligible,19,copy,opaque));
            CHECK(copy.fertilizeCoins269.size()==1&&copy.fertilizeCoins269[0].id==541&&copy.fertilizeCoins269[0].data.instanceToken==item.instanceToken&&copy.fertilizeCoins269[0].data.targetId==item.targetId&&copy.fertilizeCoins269[0].data.targetToken==item.targetToken&&copy.fertilizeCoins269[0].data.phase==NativeFertilize269Phase::Using&&copy.fertilizeCoins269[0].data.phaseStartAge==10&&copy.fertilizeCoins269[0].data.anchorX==125&&opaque.mData==original);
            const size_t end=14+(encoded.mData[6]|(size_t(encoded.mData[7])<<8)),firstFert=end-emptyNight291CountBytes-24-26;
            CHECK(encoded.mData[firstFert]==(541&255)&&encoded.mData[firstFert+12]==3&&encoded.mData[firstFert+13]==255);
            auto bad=encoded;bad.mData[firstFert+12]=4;RejectAtomically(bad); // Unknown phase cannot masquerade as a Bool.
        }
        auto wrongGeneration=frame;++wrongGeneration.fertilizeCoins269[0].data.targetToken;CHECK(!ValidateNativeSnapshotFrame(wrongGeneration,eligible));
        auto missingTarget=frame;++missingTarget.fertilizeCoins269[0].data.targetId;CHECK(!ValidateNativeSnapshotFrame(missingTarget,eligible));
        auto cursor=source;item.targetId=0;item.targetToken=0;item.phase=NativeFertilize269Phase::Cursor;item.heldSlot=1;item.phaseStartAge=0;
        cursor.fertilizeCoins269={{541,item}};CHECK(ValidateNativeSnapshotFrame(cursor,eligible));
        auto inactiveSlot=cursor;inactiveSlot.fertilizeCoins269[0].data.heldSlot=2;CHECK(!ValidateNativeSnapshotFrame(inactiveSlot,eligible));
        auto gloveOnly=source;gloveOnly.gloveHolds[1]={source.plants[0].id,source.plants[0].data.instanceToken};CHECK(ValidateNativeSnapshotFrame(gloveOnly,eligible));
        gloveOnly.fertilizeCoins269=cursor.fertilizeCoins269;CHECK(!ValidateNativeSnapshotFrame(gloveOnly,eligible));
    }
    {
        auto ordinary=source;NativePlantData base;base.kind=0;base.instanceToken=0x71001;ordinary.plants[0].data=base;
        CHECK(ValidateNativeSnapshotFrame(ordinary,eligible));
        ordinary.wheatPlants269={{ordinary.plants[0].id,{true,0}}};
        CHECK(!ValidateNativeSnapshotFrame(ordinary,eligible)); // Isolate the exact pool-membership guard.
        auto maximum=source;maximum.plants.clear();
        for(unsigned i=0;i<1024;++i)
        {auto plant=source.plants[0];plant.id=uint16_t(i+1);plant.data.instanceToken=0x80000+i;maximum.plants.push_back(plant);maximum.wheatPlants269.push_back({plant.id,{true,0}});}
        CHECK(ValidateNativeSnapshotFrame(maximum,eligible));
        maximum.wheatPlants269.push_back({1025,{true,0}});CHECK(!ValidateNativeSnapshotFrame(maximum,eligible));
    }
    // FNS24 appends only the source flag/float on an existing native plant.
    // The canonical source pool is shared with production; no host lastTick.
    CHECK(FusionEve269Candidates().size()==138);
    CHECK(FusionEve269Candidate(1000)&&FusionEve269Candidate(1121)&&FusionEve269Candidate(1192));
    CHECK(!FusionEve269Candidate(253)&&!FusionEve269Candidate(1018)&&!FusionEve269Candidate(65535));
    for(float time:{0.f,30.9906139f,31.01f,32.f})
    {
        auto frame=source;frame.wheatPlants269={{frame.plants[0].id,{true,time}}};
        NetBuffer encoded;encoded.mData=original;const bool ok=EncodeNativeSnapshot(frame,eligible,encoded);CHECK(ok);if(!ok)continue;
        NativeSnapshotFrame restored;NetBuffer opaque;
        CHECK(DecodeNativeSnapshot(encoded,eligible,19,restored,opaque));
        CHECK(restored.wheatPlants269.size()==1&&restored.wheatPlants269[0].id==frame.plants[0].id&&
            restored.wheatPlants269[0].data.isFromWheat&&restored.wheatPlants269[0].data.wheatTime==time&&opaque.mData==original);
        const size_t end=14+(encoded.mData[6]|(size_t(encoded.mData[7])<<8)),count=end-emptyNight291CountBytes-11,record=end-emptyNight291CountBytes-9;
        CHECK(encoded.mData[count]==1&&encoded.mData[count+1]==0&&encoded.mData[record+2]==1);
        for(uint8_t badBool:{uint8_t(0),uint8_t(2),uint8_t(255)})
        {auto bad=encoded;bad.mData[record+2]=badBool;RejectAtomically(bad);}
        for(float invalidTime:{-.01f,32.01f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()})
        {
            auto bad=encoded;std::memcpy(bad.mData.data()+record+3,&invalidTime,4);RejectAtomically(bad);
            auto invalidFrame=frame;invalidFrame.wheatPlants269[0].data.wheatTime=invalidTime;
            NetBuffer preserved;preserved.WriteU8(0xa5);const auto bytes=preserved.mData;
            CHECK(!ValidateNativeSnapshotFrame(invalidFrame,eligible)&&!EncodeNativeSnapshot(invalidFrame,eligible,preserved)&&preserved.mData==bytes);
        }
        auto orphan=encoded;orphan.mData[record]=0xfe;orphan.mData[record+1]=0xff;RejectAtomically(orphan);
        for(int fault=0;fault<5;++fault)
        {
            auto invalid=frame;
            if(fault==0)invalid.wheatPlants269[0].data.isFromWheat=false;
            if(fault==1)invalid.wheatPlants269.push_back(invalid.wheatPlants269[0]);
            if(fault==2)invalid.plants.clear();
            if(fault==3)invalid.plants[0].data.kind=0; // Valid ordinary kind, outside exact Eve pool.
            if(fault==4)invalid.wheatPlants269.resize(1025,invalid.wheatPlants269[0]);
            NetBuffer preserved;preserved.WriteU8(0xa5);const auto bytes=preserved.mData;
            CHECK(!ValidateNativeSnapshotFrame(invalid,eligible)&&!EncodeNativeSnapshot(invalid,eligible,preserved)&&preserved.mData==bytes);
        }
        // A duplicate wire entry still has a valid framing/body checksum.
        auto duplicate=encoded;duplicate.mData.insert(duplicate.mData.begin()+end-emptyNight291CountBytes-2,encoded.mData.begin()+record,encoded.mData.begin()+record+7);
        duplicate.mData[count]=2;const size_t metadata=end-14+7;duplicate.mData[6]=uint8_t(metadata);duplicate.mData[7]=uint8_t(metadata>>8);RejectAtomically(duplicate);
        for(uint16_t badVersion:{uint16_t(23),uint16_t(24),uint16_t(25),uint16_t(26),uint16_t(27),uint16_t(28),uint16_t(29),uint16_t(kNativeFusionSnapshotVersion+1)})
        {auto bad=encoded;bad.mData[4]=uint8_t(badVersion);bad.mData[5]=0;RejectAtomically(bad);}
        auto cleared=source;NetBuffer clearWire;clearWire.mData=original;
        CHECK(EncodeNativeSnapshot(cleared,eligible,clearWire)&&DecodeNativeSnapshot(clearWire,eligible,19,restored,opaque)&&restored.wheatPlants269.empty());
    }
    // FNS26 Day state extends the SAME source/native Zombie identity. Profiles
    // being understood by the codec does not admit their runtime factories.
    const auto dayFrame=[&](uint16_t kind,unsigned variant)
    {
        auto frame=source;NativeSourceZombieData zombie;zombie.kind=kind;
        zombie.instanceToken=0x27301;zombie.health=270;
        frame.sourceZombies={{601,zombie}};frame.zombieIdentities={{601,zombie.instanceToken}};
        NativeDayZombie273State state;
        if(kind==3){state.status=variant==0?0:variant==1?2:3;state.shadowEnabled=state.status!=3;}
        if(FusionDayZombie273Doll(kind)||kind==106||kind==107)state.armorStage=uint8_t(variant);
        if(FusionDayZombie273Shooter(kind))
        {state.shootCountdown=.625f+variant*.125f;state.shootEvent=uint8_t(variant);}
        if(variant&&(FusionDayZombie273Shooter(kind)||FusionDayZombie273Doll(kind)))
        {
            auto& head=state.head;head.mPresent=true;head.mDead=variant==2;
            head.mFrameStart=123;head.mFrameCount=20;head.mAnimTime=21845.f/65535.f;
            head.mAnimRate=variant==1?1.25f:-1.25f;head.mLoopType=2;head.mLoopCount=7;
        }
        frame.dayZombies273={{601,state}};return frame;
    };
    const auto rejectDayFrame=[&](const NativeSnapshotFrame& frame)
    {
        NetBuffer preserved;preserved.mData=original;const auto before=preserved.mData;
        CHECK(!ValidateNativeSnapshotFrame(frame,eligible));
        CHECK(!EncodeNativeSnapshot(frame,eligible,preserved)&&preserved.mData==before);
    };
    for(uint16_t kind:{uint16_t(3),uint16_t(21),uint16_t(22),uint16_t(23),uint16_t(100),uint16_t(106),uint16_t(107),uint16_t(108)})
        for(unsigned variant=0;variant<3;++variant)
        {
            auto frame=dayFrame(kind,variant);const auto& expected=frame.dayZombies273[0].data;
            CHECK(ValidateNativeSnapshotFrame(frame,eligible));
            NetBuffer encoded;encoded.mData=original;
            const bool ok=EncodeNativeSnapshot(frame,eligible,encoded);CHECK(ok);if(!ok)continue;
            const size_t end=14+(encoded.mData[6]|(size_t(encoded.mData[7])<<8));
            constexpr size_t dayRecordBytes=2+8+LawnNet::NET_ZOMBIE_ANIM_WIRE_BYTES;
            const size_t record=end-emptyNight291CountBytes-dayRecordBytes,count=record-2;
            CHECK(dayRecordBytes==21&&encoded.mData[count]==1&&encoded.mData[count+1]==0&&
                encoded.mData[record]==(601&255)&&encoded.mData[record+1]==(601>>8)&&
                encoded.mData[record+6]==expected.status&&encoded.mData[record+7]==expected.armorStage&&
                encoded.mData[record+8]==expected.shootEvent&&encoded.mData[record+9]==expected.shadowEnabled);
            NetBuffer headWire;CHECK(LawnNet::NetEncodeZombieAnimState(expected.head,headWire));
            CHECK(headWire.Size()==11&&std::equal(headWire.mData.begin(),headWire.mData.end(),encoded.mData.begin()+record+10));
            NativeSnapshotFrame restored;NetBuffer opaque;
            const bool read=DecodeNativeSnapshot(encoded,eligible,19,restored,opaque);CHECK(read);
            CHECK(read&&restored.dayZombies273.size()==1&&restored.sourceZombies.size()==1&&opaque.mData==original);
            if(read&&restored.dayZombies273.size()==1&&restored.sourceZombies.size()==1)
            {
                const auto& actual=restored.dayZombies273[0].data;const auto& a=actual.head;const auto& e=expected.head;
                CHECK(restored.dayZombies273[0].id==601&&restored.sourceZombies[0].id==601&&
                    restored.sourceZombies[0].data.kind==kind&&restored.sourceZombies[0].data.instanceToken==0x27301&&
                    actual.shootCountdown==expected.shootCountdown&&actual.status==expected.status&&
                    actual.armorStage==expected.armorStage&&actual.shootEvent==expected.shootEvent&&actual.shadowEnabled==expected.shadowEnabled);
                CHECK(a.mPresent==e.mPresent&&a.mDead==e.mDead&&a.mFrameStart==e.mFrameStart&&a.mFrameCount==e.mFrameCount&&
                    a.mAnimTime==e.mAnimTime&&a.mAnimRate==e.mAnimRate&&a.mLoopType==e.mLoopType&&a.mLoopCount==e.mLoopCount);
                CHECK(!actual.dollFalling&&actual.poleEventFrame==-1&&!actual.poleEventValid&&
                    actual.poleEvent.loop==0&&actual.poleEvent.seconds==0&&!actual.headDropPending&&!actual.headDropDamageFlags);
                CHECK(EncodeNativeSnapshot(restored,eligible,opaque)&&opaque.mData==encoded.mData);
            }
            // These are host event-consumption/allocation guards, never peer clocks.
            auto hostOnly=frame;auto& privateState=hostOnly.dayZombies273[0].data;
            privateState.dollFalling=true;privateState.poleEventFrame=12345;
            privateState.poleEvent={77,std::numeric_limits<float>::quiet_NaN()};privateState.poleEventValid=true;
            privateState.headDropPending=true;privateState.headDropDamageFlags=0xffffffff;
            NetBuffer privateWire;privateWire.mData=original;
            CHECK(ValidateNativeSnapshotFrame(hostOnly,eligible)&&EncodeNativeSnapshot(hostOnly,eligible,privateWire)&&privateWire.mData==encoded.mData);
        }
    for(int fault=0;fault<18;++fault)
    {
        auto bad=dayFrame(3,0);auto& state=bad.dayZombies273[0].data;
        if(fault==0)bad.sourceZombies.clear();
        if(fault==1)bad.sourceZombies[0].data.kind=0; // Known ordinary NPC cannot acquire Day state.
        if(fault==2)++bad.dayZombies273[0].id;
        if(fault==3)bad.dayZombies273[0].id=0;
        if(fault==4)bad.dayZombies273.push_back(bad.dayZombies273[0]);
        if(fault==5)bad.dayZombies273.clear(); // Known Pole3 still REQUIRES its typed state.
        if(fault==6)++bad.zombieIdentities[0].data;
        if(fault==7)state.status=1;
        if(fault==8)state.shadowEnabled=false;
        if(fault==9)state.armorStage=1;
        if(fault==10)state.shootEvent=1;
        if(fault==11)state.shootCountdown=0;
        if(fault==12){state.head.mPresent=true;state.head.mFrameCount=1;}
        if(fault==13)state.head.mFrameCount=1; // Noncanonical absent head.
        if(fault==14)state.shootCountdown=std::numeric_limits<float>::quiet_NaN();
        if(fault==15)state.shootCountdown=-.011f;
        if(fault==16)state.shootCountdown=1.601f;
        if(fault==17)bad.dayZombies273.resize(1025,bad.dayZombies273[0]);
        rejectDayFrame(bad);
    }
    // Reject actual-kind mismatches, even when BOTH source kinds are known.
    {auto bad=dayFrame(3,2);bad.sourceZombies[0].data.kind=21;rejectDayFrame(bad);}
    {auto bad=dayFrame(100,1);bad.sourceZombies[0].data.kind=106;rejectDayFrame(bad);}
    for(float countdown:{-.01f,0.f,1.5f,1.6f})
    {auto frame=dayFrame(100,1);frame.dayZombies273[0].data.shootCountdown=countdown;CHECK(ValidateNativeSnapshotFrame(frame,eligible));}
    for(int fault=0;fault<9;++fault)
    {
        auto bad=dayFrame(100,1);auto& state=bad.dayZombies273[0].data;
        if(fault==0)state.armorStage=1;
        if(fault==1)state.shootEvent=3;
        if(fault==2)state.head.mPresent=false;
        if(fault==3)state.head.mFrameCount=0;
        if(fault==4){state.head.mFrameStart=65530;state.head.mFrameCount=6;}
        if(fault==5)state.head.mAnimTime=1.01f;
        if(fault==6)state.head.mAnimRate=std::numeric_limits<float>::infinity();
        if(fault==7)state.head.mLoopType=6;
        if(fault==8)state.shadowEnabled=false;
        rejectDayFrame(bad);
    }
    // Wire faults retain a valid opaque body/checksum and must be transactional.
    {
        auto frame=dayFrame(100,1);NetBuffer encoded;encoded.mData=original;
        const bool ok=EncodeNativeSnapshot(frame,eligible,encoded);CHECK(ok);
        if(ok)
        {
            const size_t end=14+(encoded.mData[6]|(size_t(encoded.mData[7])<<8)),record=end-emptyNight291CountBytes-21,count=record-2;
            for(int fault=0;fault<12;++fault)
            {
                auto forged=encoded;
                if(fault==0)forged.mData[record]=0; // Unknown ID in existing source vector.
                if(fault==1)forged.mData[record+6]=2; // Shooter status must be0.
                if(fault==2)forged.mData[record+7]=3;
                if(fault==3)forged.mData[record+8]=3;
                if(fault==4)forged.mData[record+9]=2; // Noncanonical Bool.
                if(fault==5)forged.mData[record+9]=0;
                if(fault==6){float n=std::numeric_limits<float>::quiet_NaN();std::memcpy(forged.mData.data()+record+2,&n,4);}
                if(fault==7)forged.mData[record+20]=4; // Unknown head flags.
                if(fault==8)forged.mData[record+20]=0; // Absent head with stale frame/time bytes.
                if(fault==9)forged.mData[record+12]=forged.mData[record+13]=0; // Present head, empty frame span.
                if(fault==10)forged.mData[record+18]=6;
                if(fault==11){forged.mData[count]=1;forged.mData[count+1]=4;} // Count1025.
                RejectAtomically(forged);
            }
            auto duplicate=encoded;
            duplicate.mData.insert(duplicate.mData.begin()+end-emptyNight291CountBytes,encoded.mData.begin()+record,encoded.mData.begin()+end-emptyNight291CountBytes);
            duplicate.mData[count]=2;const size_t larger=end-14+21;
            duplicate.mData[6]=uint8_t(larger);duplicate.mData[7]=uint8_t(larger>>8);RejectAtomically(duplicate);
            auto missing=encoded;missing.mData.erase(missing.mData.begin()+record,missing.mData.begin()+end-emptyNight291CountBytes);missing.mData[count]=0;
            const size_t smaller=end-14-21;missing.mData[6]=uint8_t(smaller);missing.mData[7]=uint8_t(smaller>>8);RejectAtomically(missing);
            // Truncate every head suffix while preserving the original native body.
            for(size_t retained=0;retained<11;++retained)
            {
                auto truncated=encoded;const size_t cut=record+10+retained;
                truncated.mData.erase(truncated.mData.begin()+cut,truncated.mData.begin()+end-emptyNight291CountBytes);
                const size_t size=cut-14+emptyNight291CountBytes;truncated.mData[6]=uint8_t(size);truncated.mData[7]=uint8_t(size>>8);RejectAtomically(truncated);
            }
        }
    }
    // Day0/11 use the existing native projectile identity. Reflected friendly
    // shots may double damage; a consumed hit latch belongs only to hostile shots.
    for(uint16_t kind:{uint16_t(0),uint16_t(11)})for(uint8_t way:{uint8_t(0),uint8_t(9)})
        for(bool hostile:{false,true})for(int multiplier:{1,2})for(bool latched:{false,true})
        {
            auto frame=source;auto& shot=frame.projectiles[0].data;shot={};shot.kind=kind;
            shot.damage=(kind==0?20:80)*multiplier;shot.specialType=way;shot.isZombieBullet=hostile;shot.hitLatched=latched;
            const auto motion=way==9?MOTION_BACKWARDS:MOTION_STRAIGHT;
            const bool valid=(!hostile||multiplier==1)&&(!latched||hostile);
            CHECK(NativeDay273ProjectileMetadataValid(shot,PROJECTILE_PEA,motion)==valid);
            CHECK(NativeProjectileMetadataType(shot)==PROJECTILE_PEA&&NativeProjectileMetadataMotion(shot)==motion);
            CHECK(!NativeDay273ProjectileMetadataValid(shot,PROJECTILE_SPIKE,motion));
            CHECK(!NativeDay273ProjectileMetadataValid(shot,PROJECTILE_PEA,MOTION_LOBBED));
            if(!valid){rejectDayFrame(frame);continue;}
            CHECK(NativeProjectileMetadataValid(shot,PROJECTILE_PEA,motion));
            NetBuffer encoded;encoded.mData=original;
            const bool ok=EncodeNativeSnapshot(frame,eligible,encoded);CHECK(ok);if(!ok)continue;
            NativeSnapshotFrame restored;NetBuffer opaque;
            const bool read=DecodeNativeSnapshot(encoded,eligible,19,restored,opaque);CHECK(read);
            if(read&&restored.projectiles.size()==1)
            {
                const auto& actual=restored.projectiles[0].data;
                CHECK(restored.projectiles[0].id==frame.projectiles[0].id&&actual.kind==kind&&actual.damage==shot.damage&&
                    actual.specialType==way&&actual.isZombieBullet==hostile&&actual.hitLatched==latched&&
                    !actual.sunOnHit&&!actual.fireState&&!actual.lastTorchId&&!actual.lastTorchToken&&!actual.melonEnhanced&&
                    !actual.effectVariant&&!actual.umbrellaed&&!actual.goldAttracted&&opaque.mData==original);
                CHECK(EncodeNativeSnapshot(restored,eligible,opaque)&&opaque.mData==encoded.mData);
            }
            else CHECK(false);
            if(hostile&&way==9&&!latched)
            {
                for(size_t offset:{size_t(8),size_t(13),size_t(14),size_t(16),size_t(20),size_t(21),size_t(24),size_t(25)})
                {auto forged=encoded;forged.mData[firstProjectile+offset]=1;RejectAtomically(forged);}
                for(int fault=0;fault<8;++fault)
                {
                    auto bad=frame;auto& data=bad.projectiles[0].data;
                    if(fault==0)data.sunOnHit=1;if(fault==1)data.fireState=1;
                    if(fault==2)data.lastTorchId=1;if(fault==3)data.lastTorchToken=1;
                    if(fault==4)data.melonEnhanced=true;if(fault==5)data.effectVariant=1;
                    if(fault==6)data.umbrellaed=true;if(fault==7)data.goldAttracted=true;
                    CHECK(!NativeDay273ProjectileMetadataValid(data,PROJECTILE_PEA,motion));rejectDayFrame(bad);
                }
            }
        }
    // The exact Day helper must not claim another family's otherwise valid tuple.
    {NativeProjectileData foreign;foreign.kind=3;foreign.damage=300;foreign.specialType=9;foreign.isZombieBullet=true;
        CHECK(!NativeDay273ProjectileMetadataValid(foreign,PROJECTILE_PEA,MOTION_BACKWARDS));
        CHECK(NativeProjectileMetadataValid(foreign,PROJECTILE_PEA,MOTION_BACKWARDS));}
    // FNS27: one Jackson view for the same real Zombie. Private controller
    // references/cursors are excluded, and malformed records reject atomically.
    {
        using Phase=FusionNightZombie281Phase;
        const auto nightFrame=[](const FusionNightZombie281View& view)
        {
            auto frame=Example();NativeSourceZombieData source;
            source.kind=10;source.instanceToken=0x29101;source.health=720;
            frame.sourceZombies={{601,source}};frame.zombieIdentities={{601,source.instanceToken}};
            frame.nightZombies281={{601,view}};return frame;
        };
        const auto rejectNight=[&](const NativeSnapshotFrame& frame)
        {
            NetBuffer body;body.mData=original;const auto bytes=body.mData;
            CHECK(!ValidateNativeSnapshotFrame(frame,eligible));
            CHECK(!EncodeNativeSnapshot(frame,eligible,body)&&body.mData==bytes);
        };
        const auto roundtripNight=[&](const FusionNightZombie281View& view)
        {
            auto frame=nightFrame(view);CHECK(ValidateNativeSnapshotFrame(frame,eligible));
            NetBuffer encoded;encoded.mData=original;CHECK(EncodeNativeSnapshot(frame,eligible,encoded));
            NativeSnapshotFrame restored;NetBuffer body;
            const bool read=DecodeNativeSnapshot(encoded,eligible,19,restored,body);
            CHECK(read&&restored.nightZombies281.size()==1&&body.mData==original);
            if(read&&restored.nightZombies281.size()==1)
            {
                const auto& actual=restored.nightZombies281[0].data;
                CHECK(actual.phase==view.phase&&actual.outgoing==view.outgoing&&actual.lookBack==view.lookBack&&
                    actual.moonWalkFinished==view.moonWalkFinished&&actual.canAttack==view.canAttack&&actual.loseDancer==view.loseDancer&&
                    actual.outgoingSourceSeconds==view.outgoingSourceSeconds&&actual.transitionElapsed==view.transitionElapsed&&actual.transitionTotal==view.transitionTotal);
                CHECK(EncodeNativeSnapshot(restored,eligible,body)&&body.mData==encoded.mData);
            }
            return encoded;
        };
        for(unsigned phase=0;phase<7;++phase)
        {
            FusionNightZombie281View view;view.phase=Phase(phase);
            view.moonWalkFinished=phase!=0;view.canAttack=phase>=2;
            roundtripNight(view);
        }
        const std::pair<Phase,Phase> edges[]={
            {Phase::Moonwalk,Phase::Point},{Phase::Point,Phase::ArmRaise},
            {Phase::Walk,Phase::ArmRaise},{Phase::Walk,Phase::EatFromWalk},{Phase::Walk,Phase::Point},
            {Phase::ArmRaise,Phase::Walk},{Phase::ArmRaise,Phase::EatFromRaise},
            {Phase::EatFromWalk,Phase::Walk},{Phase::EatFromRaise,Phase::ArmRaise},
            {Phase::Moonwalk,Phase::Death},{Phase::Point,Phase::Death},{Phase::Walk,Phase::Death},
            {Phase::ArmRaise,Phase::Death},{Phase::EatFromWalk,Phase::Death},{Phase::EatFromRaise,Phase::Death}};
        for(const auto& [from,to]:edges)
        {
            FusionNightZombie281Clip clip;CHECK(TryGetFusionNightZombie281Clip(from,clip));
            FusionNightZombie281View view;view.phase=to;view.outgoing=from;
            view.moonWalkFinished=true;view.canAttack=to!=Phase::Point;
            view.outgoingSourceSeconds=.5f*clip.seconds;
            view.transitionTotal=from==Phase::ArmRaise&&to==Phase::Walk?.02f:.25f;
            view.transitionElapsed=.5f*view.transitionTotal;roundtripNight(view);
        }
        FusionNightZombie281View good;good.phase=Phase::Point;good.outgoing=Phase::Moonwalk;
        good.moonWalkFinished=true;good.outgoingSourceSeconds=1;good.transitionTotal=.25f;good.transitionElapsed=.125f;
        const auto encoded=roundtripNight(good);
        const size_t metadata=size_t(encoded.mData[6])+(size_t(encoded.mData[7])<<8);
        const size_t first=14+metadata-17-emptyJackbox306CountBytes; // id U16, two phases/flags U8,3F32.
        for(unsigned fault=0;fault<14;++fault)
        {
            auto bad=nightFrame(good);auto& view=bad.nightZombies281[0].data;
            if(fault==0)bad.nightZombies281.clear();
            if(fault==1)bad.sourceZombies.clear();
            if(fault==2)bad.sourceZombies[0].data.kind=0;
            if(fault==3)bad.nightZombies281.push_back(bad.nightZombies281[0]);
            if(fault==4)bad.nightZombies281[0].id=602;
            if(fault==5)view.phase=Phase(7);
            if(fault==6)view.outgoing=Phase(7);
            if(fault==7)view.moonWalkFinished=false,view.canAttack=true;
            if(fault==8)view.outgoingSourceSeconds=-.1f;
            if(fault==9)view.outgoingSourceSeconds=2.1f;
            if(fault==10)view.transitionElapsed=-.1f;
            if(fault==11)view.transitionElapsed=.3f;
            if(fault==12)view.transitionTotal=-.25f;
            if(fault==13)view.transitionTotal=0;
            rejectNight(bad);
        }
        for(size_t at:{first+2,first+3,first+4})
        {auto bad=encoded;bad.mData[at]=255;RejectAtomically(bad);}
        for(size_t at:{first+5,first+9,first+13})
        {
            auto bad=encoded;const uint32_t bits=0x7fc00000;
            for(unsigned i=0;i<4;++i)bad.mData[at+i]=uint8_t(bits>>(i*8));RejectAtomically(bad);
        }
        NativeNightZombie281State state;
        static_cast<FusionNightZombie281View&>(state)=good;
        state.dancers={1,2,3,0xffffffff};state.tick=123;state.moonWalkElapsed=2.5f;state.eventPosition={7,.5f};
        auto privateCopy=nightFrame(static_cast<const FusionNightZombie281View&>(state));
        NetBuffer canonical;canonical.mData=original;
        CHECK(EncodeNativeSnapshot(privateCopy,eligible,canonical)&&canonical.mData==encoded.mData);
    }
    // FNS28: the same real Jackbox generation owns one canonical view.
    // Its runtime speed may double only with the source box already absent.
    {
        using Phase=FusionJackbox300Phase;
        const auto makeJackbox=[](uint16_t kind,const FusionJackbox300View& view,float speed=1.f)
        {
            auto frame=Example();NativeSourceZombieData source;
            source.kind=kind;source.instanceToken=0x30001;source.health=720;source.originSpeed=speed;
            frame.sourceZombies={{701,source}};frame.zombieIdentities={{701,source.instanceToken}};
            frame.jackboxZombies300={{701,view}};return frame;
        };
        const auto rejectJackbox=[&](const NativeSnapshotFrame& frame)
        {
            NetBuffer body;body.mData=original;
            CHECK(!ValidateNativeSnapshotFrame(frame,eligible));
            CHECK(!EncodeNativeSnapshot(frame,eligible,body)&&body.mData==original);
        };
        const auto roundtripJackbox=[&](const NativeSnapshotFrame& frame)
        {
            NetBuffer encoded;encoded.mData=original;
            CHECK(ValidateNativeSnapshotFrame(frame,eligible));
            const bool written=EncodeNativeSnapshot(frame,eligible,encoded);CHECK(written);
            NativeSnapshotFrame actual;NetBuffer input=encoded,body;
            const bool read=written&&DecodeNativeSnapshot(input,eligible,frame.epoch,actual,body);
            CHECK(read&&actual.jackboxZombies300.size()==1&&body.mData==original);
            if(read)CHECK(EncodeNativeSnapshot(actual,eligible,body)&&body.mData==encoded.mData);
            return encoded;
        };
        FusionJackbox300View good;good.phase=Phase::Eat;good.outgoing=Phase::Walk;
        good.countdown=3;good.originalCountdown=4.4f;good.outgoingSeconds=.75f;
        good.transitionTotal=.25f;good.transitionElapsed=.125f;
        const auto encoded=roundtripJackbox(makeJackbox(24,good));
        roundtripJackbox(makeJackbox(30,good));
        for(unsigned phase=0;phase<4;++phase)
        {
            FusionJackbox300View view;view.phase=Phase(phase);
            if(view.phase==Phase::Pop){view.countdown=0;view.popTriggered=true;}
            roundtripJackbox(makeJackbox(24,view));
        }
        auto disarmed=good;disarmed.boxPresent=false;disarmed.audioPlaying=false;
        roundtripJackbox(makeJackbox(30,disarmed,3.6f));
        rejectJackbox(makeJackbox(30,good,3.6f));
        rejectJackbox(makeJackbox(24,disarmed,3.6f));
        for(unsigned fault=0;fault<16;++fault)
        {
            auto bad=makeJackbox(30,good);auto& view=bad.jackboxZombies300[0].data;
            if(fault==0)bad.jackboxZombies300.clear();
            if(fault==1)bad.sourceZombies.clear();
            if(fault==2)bad.sourceZombies[0].data.kind=0;
            if(fault==3)bad.jackboxZombies300.push_back(bad.jackboxZombies300[0]);
            if(fault==4)bad.jackboxZombies300[0].id=702;
            if(fault==5)view.phase=Phase(255);
            if(fault==6)view.outgoing=Phase(255);
            if(fault==7)view.originalCountdown=8;
            if(fault==8)view.countdown=-1;
            if(fault==9)view.boxPresent=false,view.audioPlaying=true;
            if(fault==10)view.popTriggered=true;
            if(fault==11)view.transitionTotal=.5f;
            if(fault==12)view.transitionElapsed=.3f;
            if(fault==13)view.outgoingSeconds=2;
            if(fault==14)view.transitionElapsed=std::numeric_limits<float>::quiet_NaN();
            if(fault==15)view.countdown=std::numeric_limits<float>::infinity();
            rejectJackbox(bad);
        }
        const size_t metadata=size_t(encoded.mData[6])+(size_t(encoded.mData[7])<<8);
        const size_t first=14+metadata-empty313TailBytes-25; // id2, phase/outgoing/flags3, five F32.
        for(size_t at:{first+2,first+3,first+4})
        {auto bad=encoded;bad.mData[at]=255;RejectAtomically(bad);}
        for(size_t at:{first+5,first+9,first+13,first+17,first+21})
        {
            auto bad=encoded;const uint32_t bits=0x7fc00000;
            for(unsigned i=0;i<4;++i)bad.mData[at+i]=uint8_t(bits>>(8*i));RejectAtomically(bad);
        }
        NativeJackbox300State privateState;static_cast<FusionJackbox300View&>(privateState)=good;
        privateState.localUID=0xffffffff;privateState.tick=234;privateState.incomingPosition={9,.75f};
        privateState.outgoingPosition={7,.2f};privateState.initialized=true;
        NetBuffer canonical;canonical.mData=original;
        CHECK(EncodeNativeSnapshot(makeJackbox(24,static_cast<const FusionJackbox300View&>(privateState)),eligible,canonical)&&canonical.mData==encoded.mData);
    }
    std::printf(result.mFailures ? "[  FAILED  ] Fusion.NativeWire\n" : "[       OK ] Fusion.NativeWire\n");
    // Actual FNS codec: stacked source Invoke deadlines, no separate phase.
    {
        auto disabled=source;disabled.plants[0].data.disable363={1,250,500};
        NetBuffer encoded;encoded.WriteU32(0x12345678);CHECK(EncodeNativeSnapshot(disabled,eligible,encoded));
        NativeSnapshotFrame restored;NetBuffer raw;CHECK(DecodeNativeSnapshot(encoded,eligible,19,restored,raw));
        CHECK(restored.plants.size()==disabled.plants.size()&&restored.plants[0].data.disable363==disabled.plants[0].data.disable363);
        for(auto bad:{std::vector<uint16_t>{0},std::vector<uint16_t>{501},std::vector<uint16_t>{500,1}})
        {auto invalid=source;invalid.plants[0].data.disable363=bad;NetBuffer untouched;untouched.WriteU8(0x55);const auto original=untouched.mData;CHECK(!EncodeNativeSnapshot(invalid,eligible,untouched)&&untouched.mData==original);}
        NetBuffer bad;bad.WriteU32(0x12345678);CHECK(EncodeNativeSnapshot(disabled,eligible,bad));
        // Current fixed prefix ends54bytes after this sole metadata record ID.
        const size_t ttl=firstPlant+54+2;bad.mData[ttl]=0;bad.mData[ttl+1]=0;RejectAtomically(bad);
    }
    return result;
}
