#include "FusionSourceZombieWireTests.h"
#include "../../Lawn/Fusion/FusionNativeWire.h"
#include "../../Lawn/Fusion/FusionPoisonRules.h"
#include "../../Lawn/Net/NetCommon.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace
{
using namespace LawnFusion;
using LawnNet::NetBuffer;
FusionSourceZombieWireTestResult result;
void Check(bool condition,int line)
{
    ++result.mChecks;
    if (!condition) { ++result.mFailures; std::printf("    Source Zombie wire check failed line=%d\n",line); }
}
#define CHECK(condition) Check(!!(condition),__LINE__)
constexpr uint32_t kEpoch = 19;
constexpr size_t kHeaderBytes = 14, kSourceRecordBytes = 50, kEmptyEquipmentTailBytes = 50; // Includes the FNS26 empty Day family count.
constexpr size_t kDisabledAdventureSessionBytes=1; // FNS26, between temporaryLightLevel and Garlic count.
const std::vector<FusionBankEligibleCard> eligible = {{0,false,100,7.5f},{1,false,50,7.5f}};

NativeSnapshotFrame Example()
{
    FusionSeedBank bank;
    bank.Open(kEpoch,1,eligible,1);
    bank.Choose(kEpoch,0,0,false); bank.Choose(kEpoch,0,1,false);
    bank.SetReady(kEpoch,0,true); bank.CommitIntro(kEpoch); bank.BeginPlaying(kEpoch);
    NativeSnapshotFrame frame;
    frame.epoch = kEpoch; frame.selectionCommitted = true; frame.selection = bank.State();
    for (const auto& packet : frame.selection.packets)
    {
        NativePacketData data;
        data.selectionId=packet.id; data.kind=packet.kind; data.isExtra=packet.isExtra;
        data.owner=packet.owner; data.randomOrigin=packet.randomOrigin;
        data.price=packet.price; data.rechargeSeconds=packet.rechargeSeconds;
        frame.packets.push_back({static_cast<uint16_t>(packet.id+10),data});
    }
    NativeSourceZombieData basic;
    basic.kind=0; basic.instanceToken=0x10029; basic.originSpeed=.9f; basic.health=269.73f;
    NativeSourceZombieData cone=basic;
    cone.kind=2; cone.instanceToken=0x1002a; cone.originSpeed=1.8f; cone.health=270;
    // A bitten parent may retire before the source target lock clears next update.
    cone.targetEntity=NativeZombieTargetEntity::Plant;
    cone.targetId=65535; cone.targetGeneration=0xffffffff;
    frame.sourceZombies={{41,basic},{42,cone}};
    // Source truth and status tags accompany the same native Zombie IDs.
    frame.zombieActions={{41,{0,1.0f,true,true,6,1234,37}},{42,{127,.5f,false,false,0,500,100}}};
    frame.temporaryLightLevel=65535;
    return frame;
}

NetBuffer Body()
{
    NetBuffer body; body.WriteI32(500); body.WriteU16(0x1234); body.WriteU8(0x80);
    return body;
}
NetBuffer Encode(const NativeSnapshotFrame& frame)
{
    auto body=Body(); CHECK(EncodeNativeSnapshot(frame,eligible,body)); return body;
}
uint16_t U16(const NetBuffer& buffer,size_t offset)
{ return buffer.mData[offset] | (static_cast<uint16_t>(buffer.mData[offset+1])<<8); }
void PutU16(NetBuffer& buffer,size_t offset,uint16_t value)
{ buffer.mData[offset]=static_cast<uint8_t>(value); buffer.mData[offset+1]=static_cast<uint8_t>(value>>8); }
void PutU32(NetBuffer& buffer,size_t offset,uint32_t value)
{ for (unsigned byte=0;byte<4;++byte) buffer.mData[offset+byte]=static_cast<uint8_t>(value>>(8*byte)); }
void PutF32(NetBuffer& buffer,size_t offset,float value)
{ std::memcpy(buffer.mData.data()+offset,&value,sizeof(value)); }
size_t SourceCountOffset(const NetBuffer& wire)
{
    // Example has no equipment Coins, equipment Plants or zombie identities.
    // FNS19 adds three state counts and seven parent bytes per source record;
    // FNS20 adds four Companion/World U16 counts, FNS21 Card/Rolling, FNS22 Fert.
    // Each FNS22 source record appends firstArmorStageU8 after dieWithParent.
    return kHeaderBytes+U16(wire,6)-kEmptyEquipmentTailBytes-2-2*kSourceRecordBytes;
}

void RejectWireAtomically(NetBuffer forged)
{
    // Correct the opaque body's checksum after changing it as well. Semantic
    // rejection must precede exposure of either metadata or this valid body.
    if (forged.Size()>=kHeaderBytes)
    {
        const size_t body=kHeaderBytes+U16(forged,6);
        if (body<forged.Size())
        {
            forged.mData[body]^=0x40;
            PutU32(forged,10,LawnNet::NetFnv1a32(forged.Data()+body,forged.Size()-body));
        }
    }
    NetBuffer input; input.WriteU16(0xaabb); input.WriteBytes(forged.Data(),forged.Size()); input.mReadPos=2;
    const auto originalInput=input.mData;
    auto output=Example(); output.epoch=777; output.selection.epoch=777;
    output.money=321; output.sourceZombies[0].data.health=37.75f;output.background=BACKGROUND_1_DAY;
    const auto originalFrame=Encode(output).mData;
    NetBuffer native; native.WriteU16(0x9988); native.mReadPos=1;
    const auto originalBody=native.mData;
    CHECK(!DecodeNativeSnapshot(input,eligible,kEpoch,output,native));
    CHECK(input.mReadPos==2 && input.mData==originalInput);
    CHECK(native.mReadPos==1 && native.mData==originalBody);
    CHECK(Encode(output).mData==originalFrame);
}

void RejectFrame(const NativeSnapshotFrame& frame)
{
    CHECK(!ValidateNativeSnapshotFrame(frame,eligible));
    auto body=Body(); const auto original=body.mData;
    CHECK(!EncodeNativeSnapshot(frame,eligible,body));
    CHECK(body.mReadPos==0 && body.mData==original);
}
}

FusionSourceZombieWireTestResult RunFusionSourceZombieWireTests()
{
    result={};
    std::printf("[ RUN      ] Fusion.SourceZombieWireTransactional\n");
    const auto frame=Example(); CHECK(ValidateNativeSnapshotFrame(frame,eligible));
    const auto wire=Encode(frame); CHECK(U16(wire,4)==kNativeFusionSnapshotVersion && wire.mData[18]==BACKGROUND_1_DAY && wire.mData[19]==1);
    for (uint8_t background : {uint8_t(BACKGROUND_1_DAY),uint8_t(BACKGROUND_2_NIGHT),uint8_t(BACKGROUND_3_POOL),uint8_t(BACKGROUND_4_FOG),uint8_t(BACKGROUND_5_ROOF),uint8_t(BACKGROUND_6_BOSS)})
    {
        auto terrain=frame;terrain.background=background;
        auto terrainWire=Encode(terrain);
        CHECK(terrainWire.mData[18]==background && terrainWire.mData[19]==1);
        NativeSnapshotFrame restored;NetBuffer nativeTerrain;
        CHECK(DecodeNativeSnapshot(terrainWire,eligible,kEpoch,restored,nativeTerrain));
        CHECK(restored.background==background && restored.sourceZombies.size()==terrain.sourceZombies.size() && nativeTerrain.mData==Body().mData);
        if(!restored.sourceZombies.empty())CHECK(restored.sourceZombies[0].data.health==frame.sourceZombies[0].data.health);
        CHECK(Encode(restored).mData==terrainWire.mData);
    }
    {auto invalid=frame;invalid.sourceZombies[0].data.kind=11;RejectFrame(invalid);} // A source duck cannot inhabit Day.
    for (uint8_t background : {uint8_t(6),uint8_t(7),uint8_t(255)})
    {
        auto bad=wire;bad.mData[18]=background;RejectWireAtomically(bad);
        auto invalid=frame;invalid.background=background;RejectFrame(invalid);
    }
    for (uint8_t invalidBoolean : {uint8_t(2),uint8_t(255)})
    {auto bad=wire;bad.mData[19]=invalidBoolean;RejectWireAtomically(bad);}
    const size_t sourceCount=SourceCountOffset(wire), first=sourceCount+2, second=first+kSourceRecordBytes;
    CHECK(U16(wire,sourceCount)==2 && U16(wire,first)==41 && U16(wire,first+2)==0);
    CHECK(U16(wire,second)==42 && U16(wire,second+2)==2);
    const size_t equipmentTail=second+kSourceRecordBytes;
    CHECK(equipmentTail+kEmptyEquipmentTailBytes==kHeaderBytes+U16(wire,6));
    CHECK(U16(wire,equipmentTail)==0 && U16(wire,equipmentTail+2)==0 && U16(wire,equipmentTail+4)==0);
    NetBuffer input; input.WriteU32(0x12345678); input.WriteBytes(wire.Data(),wire.Size()); input.mReadPos=4;
    NativeSnapshotFrame decoded; NetBuffer native;
    CHECK(DecodeNativeSnapshot(input,eligible,kEpoch,decoded,native));
    CHECK(input.mReadPos==input.Size() && native.mReadPos==0 && native.mData==Body().mData);
    CHECK(decoded.sourceZombies.size()==2);
    CHECK(decoded.temporaryLightLevel==65535 && decoded.zombieActions.size()==2 &&
        decoded.zombieActions[0].id==41 && decoded.zombieActions[0].data.doomOnDeath && decoded.zombieActions[0].data.jalaedOnDeath &&
        decoded.zombieActions[0].data.poisonLevel==6 && decoded.zombieActions[0].data.poisonTicks==1234 &&
        decoded.zombieActions[0].data.poisonDamageTicks==37 && decoded.zombieActions[1].id==42 &&
        !decoded.zombieActions[1].data.doomOnDeath && !decoded.zombieActions[1].data.jalaedOnDeath &&
        decoded.zombieActions[1].data.poisonLevel==0 && decoded.zombieActions[1].data.poisonTicks==500 &&
        decoded.zombieActions[1].data.poisonDamageTicks==100 && decoded.zombieActions[1].data.scale==.5f);
    if (decoded.sourceZombies.size()==2)
    {
        const auto& basic=decoded.sourceZombies[0]; const auto& cone=decoded.sourceZombies[1];
        CHECK(basic.id==41 && basic.data.kind==0 && basic.data.instanceToken==0x10029 &&
            basic.data.originSpeed==.9f && basic.data.health==269.73f &&
            basic.data.targetEntity==NativeZombieTargetEntity::None && !basic.data.targetId && !basic.data.targetGeneration);
        CHECK(cone.id==42 && cone.data.kind==2 && cone.data.instanceToken==0x1002a &&
            cone.data.originSpeed==1.8f && cone.data.health==270 &&
            cone.data.targetEntity==NativeZombieTargetEntity::Plant && cone.data.targetId==65535 && cone.data.targetGeneration==0xffffffff);
        CHECK(Encode(decoded).mData==wire.mData);
    }
    auto dangling=frame;
    dangling.sourceZombies[0].data.targetEntity=NativeZombieTargetEntity::Zombie;
    dangling.sourceZombies[0].data.targetId=65000; dangling.sourceZombies[0].data.targetGeneration=0xffee1234;
    CHECK(ValidateNativeSnapshotFrame(dangling,eligible));
    auto danglingWire=Encode(dangling);
    CHECK(DecodeNativeSnapshot(danglingWire,eligible,kEpoch,decoded,native));
    CHECK(decoded.sourceZombies[0].data.targetId==65000 && decoded.sourceZombies[0].data.targetGeneration==0xffee1234);

    for (float health : {0.0f,-.27f,-12.5f,static_cast<float>((std::numeric_limits<int>::min)()),
        std::nextafter(static_cast<float>((std::numeric_limits<int>::max)()),0.0f)})
    {
        auto boundary=frame; boundary.sourceZombies[0].data.health=health;
        auto encoded=Encode(boundary);
        CHECK(DecodeNativeSnapshot(encoded,eligible,kEpoch,decoded,native));
        CHECK(decoded.sourceZombies[0].data.health==health);
    }
    // All byte truncations, including the appended record's final target token.
    for (size_t length=0;length<wire.Size();++length)
    { auto truncated=wire; truncated.mData.resize(length); RejectWireAtomically(truncated); }
    for (uint16_t version : {uint16_t(13),uint16_t(14),uint16_t(15),uint16_t(16),uint16_t(17),uint16_t(18),uint16_t(19),uint16_t(20),uint16_t(21),uint16_t(22),uint16_t(23),uint16_t(24),uint16_t(25),uint16_t(27)})
    { auto wrongVersion=wire; PutU16(wrongVersion,4,version); RejectWireAtomically(wrongVersion); }
    // FNS18 action record id2+mask1+scale4+Doom/JalaedBool2+poisonI32*3+waterGrapI32*2 precedes the unchanged
    // coin/glove/money/light/Garlic/source tail. Noncanonical Bool must reject
    // without exposing an otherwise valid sourceZombie/body update.
    constexpr size_t actionRecordBytes=29;
    const size_t firstAction=sourceCount-2-kDisabledAdventureSessionBytes-2-4-(kFusionBankPlayers*6+4)-2-frame.zombieActions.size()*actionRecordBytes;
    const size_t secondAction=firstAction+actionRecordBytes;
    CHECK(wire.mData[sourceCount-2-kDisabledAdventureSessionBytes]==0&&U16(wire,sourceCount-2)==0);
    CHECK(U16(wire,firstAction)==41 && wire.mData[firstAction+7]==1 && wire.mData[firstAction+8]==1 &&
        U16(wire,secondAction)==42 && wire.mData[secondAction+7]==0 && wire.mData[secondAction+8]==0);
    CHECK(wire.mData[firstAction+9]==6 && wire.mData[firstAction+10]==0 &&
        wire.mData[firstAction+13]==0xd2 && wire.mData[firstAction+14]==4 && wire.mData[firstAction+17]==37 &&
        wire.mData[secondAction+9]==0 && wire.mData[secondAction+13]==0xf4 &&
        wire.mData[secondAction+14]==1 && wire.mData[secondAction+17]==100);
    CHECK(U16(wire,firstAction+21)==0 && U16(wire,firstAction+25)==0 &&
        U16(wire,secondAction+21)==0 && U16(wire,secondAction+25)==0);
    for(uint8_t invalid : {uint8_t(2),uint8_t(255)})
        for(size_t offset : {firstAction+7,firstAction+8,secondAction+7,secondAction+8})
        { auto bad=wire;bad.mData[offset]=invalid;RejectWireAtomically(bad); }
    for(int variant=0;variant<9;++variant)
    {
        auto bad=wire;
        if(variant==0)PutU32(bad,firstAction+9,0xffffffffu);
        if(variant==1)PutU32(bad,firstAction+9,static_cast<uint32_t>(kFusionPoisonMaximumLevel)+1);
        if(variant==2)PutU32(bad,firstAction+13,0xffffffffu);
        if(variant==3)PutU32(bad,firstAction+13,0); // Timed level6 cannot become untimed.
        if(variant==4)PutU32(bad,firstAction+17,0);
        if(variant==5)PutU32(bad,firstAction+17,101);
        if(variant==6)PutU32(bad,firstAction+17,0xffffffffu);
        if(variant==7){PutU32(bad,secondAction+9,6);PutU32(bad,secondAction+13,0);}
        if(variant==8)PutU32(bad,secondAction+17,0x7fffffffu);
        RejectWireAtomically(bad);
    }

    // Record layout: id2, kind2, token4, origin4, health4, entity1, target2, generation4.
    for (uint16_t kind : {uint16_t(1),uint16_t(65000)})
    { auto bad=wire; PutU16(bad,first+2,kind); RejectWireAtomically(bad); }
    for (float speed : {0.0f,std::nextafter(.9f,0.0f),std::nextafter(1.8f,2.0f),
        (std::numeric_limits<float>::infinity)(),-(std::numeric_limits<float>::infinity)(),
        (std::numeric_limits<float>::quiet_NaN)()})
    { auto bad=wire; PutF32(bad,first+8,speed); RejectWireAtomically(bad); }
    for (float health : {static_cast<float>((std::numeric_limits<int>::max)()),
        std::nextafter(static_cast<float>((std::numeric_limits<int>::min)()),-(std::numeric_limits<float>::infinity)()),
        (std::numeric_limits<float>::infinity)(),-(std::numeric_limits<float>::infinity)(),
        (std::numeric_limits<float>::quiet_NaN)()})
    { auto bad=wire; PutF32(bad,first+12,health); RejectWireAtomically(bad); }
    { auto bad=wire; PutU16(bad,first,0); RejectWireAtomically(bad); }
    { auto bad=wire; PutU32(bad,first+4,0); RejectWireAtomically(bad); }
    { auto bad=wire; PutU16(bad,second,41); RejectWireAtomically(bad); }
    { auto bad=wire; PutU32(bad,second+4,0x10029); RejectWireAtomically(bad); }
    { auto bad=wire; bad.mData[first+16]=3; RejectWireAtomically(bad); }
    { auto bad=wire; bad.mData[first+16]=255; RejectWireAtomically(bad); }
    { auto bad=wire; PutU16(bad,first+17,11); RejectWireAtomically(bad); } // None with dangling identity.
    { auto bad=wire; PutU32(bad,first+19,1); RejectWireAtomically(bad); }
    { auto bad=wire; PutU16(bad,second+17,0); RejectWireAtomically(bad); } // Plant with missing identity.
    { auto bad=wire; PutU32(bad,second+19,0); RejectWireAtomically(bad); }
    { auto bad=wire; bad.mData[second+16]=2; PutU16(bad,second+17,42); RejectWireAtomically(bad); } // Self Zombie.
    for (uint16_t count : {uint16_t(1),uint16_t(3),uint16_t(1025),uint16_t(65535)})
    { auto bad=wire; PutU16(bad,sourceCount,count); RejectWireAtomically(bad); }

    auto maximum=frame; maximum.sourceZombies.clear();
    for (uint16_t index=1;index<=1024;++index)
    {
        NativeSourceZombieData data; data.kind=index%2?0:2; data.instanceToken=0x10000+index;
        maximum.sourceZombies.push_back({index,data});
    }
    CHECK(ValidateNativeSnapshotFrame(maximum,eligible));
    auto maximumWire=Encode(maximum);
    CHECK(DecodeNativeSnapshot(maximumWire,eligible,kEpoch,decoded,native));
    CHECK(decoded.sourceZombies.size()==1024);
    maximum.sourceZombies.push_back({1025,frame.sourceZombies[0].data}); RejectFrame(maximum);
    auto duplicate=frame; duplicate.sourceZombies[1].id=41; RejectFrame(duplicate);
    duplicate=frame; duplicate.sourceZombies[1].data.instanceToken=0x10029; RejectFrame(duplicate);
    auto unknown=frame; unknown.sourceZombies[0].data.kind=1; RejectFrame(unknown);
    auto empty=frame; empty.sourceZombies.clear(); auto emptyWire=Encode(empty);
    CHECK(DecodeNativeSnapshot(emptyWire,eligible,kEpoch,decoded,native) && decoded.sourceZombies.empty());

    // FNS19: real parent and stored projectile generations survive one transaction.
    auto linked=frame;
    NativePlantData parent;parent.kind=1084;parent.instanceToken=0x20001;parent.batchMode=1;
    linked.plants.push_back({60,parent});
    linked.sourceZombies[0].data.sourceParentId=60;
    linked.sourceZombies[0].data.sourceParentToken=parent.instanceToken;
    linked.sourceZombies[0].data.dieWithParent=true;
    NativeStar269PlantState storage;storage.attracting=true;storage.slots[79]={61,0x30001};
    linked.star269Plants.push_back({60,storage});
    NativeProjectileData star;star.kind=43;star.damage=80;star.specialType=10;
    linked.projectiles.push_back({61,star});
    NativeStar269ProjectileState flight;flight.instanceToken=0x30001;flight.ownerId=60;
    flight.ownerToken=parent.instanceToken;flight.ownerSlot=79;flight.accelerated=true;flight.colliderEnabled=false;
    linked.star269Projectiles.push_back({61,flight});
    NativePlantData doom;doom.kind=930;doom.instanceToken=0x20002;
    linked.plants.push_back({62,doom});
    NativeDoom269PlantState doomState;doomState.interval=.2f;doomState.ordinal=3;doomState.queuedTrigger=true;doomState.lastUpdateTick=999;
    linked.doom269Plants.push_back({62,doomState});
    auto linkedWire=Encode(linked);
    CHECK(DecodeNativeSnapshot(linkedWire,eligible,kEpoch,decoded,native));
    CHECK(decoded.star269Plants.size()==1&&decoded.star269Plants[0].data.slots[79].token==0x30001);
    CHECK(decoded.star269Projectiles.size()==1&&decoded.star269Projectiles[0].data.ownerToken==parent.instanceToken);
    CHECK(decoded.sourceZombies[0].data.sourceParentToken==parent.instanceToken&&decoded.sourceZombies[0].data.dieWithParent);
    CHECK(decoded.doom269Plants.size()==1&&decoded.doom269Plants[0].data.queuedTrigger&&decoded.doom269Plants[0].data.lastUpdateTick==-1);
    for(int fault=0;fault<10;++fault)
    {
        auto bad=linked;
        if(fault==0)++bad.sourceZombies[0].data.sourceParentToken;
        if(fault==1)++bad.star269Plants[0].data.slots[79].token;
        if(fault==2)++bad.star269Projectiles[0].data.ownerToken;
        if(fault==3)bad.star269Projectiles[0].data.ownerSlot=78;
        if(fault==4)bad.star269Projectiles[0].data.colliderEnabled=true;
        if(fault==5)bad.star269Plants[0].data.slots[80]={63,0x30002};
        if(fault==6)bad.star269Plants.clear();
        if(fault==7)bad.doom269Plants.clear();
        if(fault==8)bad.doom269Plants[0].data.interval=100;
        if(fault==9)bad.projectiles[0].data.specialType=6;
        RejectFrame(bad);
    }
    auto released=linked;released.star269Plants[0].data.slots[79]={};
    released.projectiles[0].data.specialType=6;
    auto& releasedFlight=released.star269Projectiles[0].data;
    releasedFlight.ownerId=releasedFlight.ownerSlot=0;releasedFlight.ownerToken=0;releasedFlight.colliderEnabled=true;releasedFlight.trackSpeed=10;
    CHECK(ValidateNativeSnapshotFrame(released,eligible));
    released.star269Projectiles[0].data.targetZombieId=70;
    released.star269Projectiles[0].data.targetZombieToken=0x40001;
    released.zombieIdentities={{70,0x40001},{41,0x10029},{42,0x1002a}};
    ProjectNativeStar269LiveTargets(released);
    CHECK(released.star269Projectiles[0].data.targetZombieToken==0x40001&&ValidateNativeSnapshotFrame(released,eligible));
    released.zombieIdentities[0].data=0x50001; // Same network ID, another actual generation.
    CHECK(!ValidateNativeSnapshotFrame(released,eligible));
    ProjectNativeStar269LiveTargets(released);
    CHECK(!released.star269Projectiles[0].data.targetZombieId&&!released.star269Projectiles[0].data.targetZombieToken&&ValidateNativeSnapshotFrame(released,eligible));
    // Source111 persists the actual head sprite stage independently of current
    // armor HP. Other source kinds cannot borrow this family-specific byte.
    for(uint8_t stage:{uint8_t(0),uint8_t(1),uint8_t(2)})
    {
        auto armor=frame;auto& actor=armor.sourceZombies[0].data;actor.kind=111;actor.firstArmorStage=stage;
        CHECK(NativeSourceZombieDataValid(actor,270));
        auto encoded=Encode(armor);NativeSnapshotFrame copy;NetBuffer opaque;
        CHECK(DecodeNativeSnapshot(encoded,eligible,kEpoch,copy,opaque));
        CHECK(copy.sourceZombies.size()==2&&copy.sourceZombies[0].data.kind==111&&copy.sourceZombies[0].data.firstArmorStage==stage&&opaque.mData==Body().mData);
        const size_t firstArmor=SourceCountOffset(encoded)+2;
        CHECK(encoded.mData[firstArmor+30]==stage);
        for(uint8_t invalid:{uint8_t(3),uint8_t(255)})
        {auto bad=encoded;bad.mData[firstArmor+30]=invalid;RejectWireAtomically(bad);}
        auto badBool=encoded;badBool.mData[firstArmor+29]=2;RejectWireAtomically(badBool); // Adjacent dieWithParent remains canonical Bool.
    }
    for(uint8_t stage:{uint8_t(1),uint8_t(2),uint8_t(255)})
    {auto bad=frame;bad.sourceZombies[0].data.firstArmorStage=stage;CHECK(!NativeSourceZombieDataValid(bad.sourceZombies[0].data,270));RejectFrame(bad);}
    // Attached source104 head uses the actual native reanimation snapshot.
    // A corrupt optional field must not expose any partial actor/body update.
    auto paper=frame;auto& actor=paper.sourceZombies[0].data;actor.kind=104;
    actor.paperShootTicks=75;actor.paperShootEvent=1;
    auto& head=actor.paperHead;head.mPresent=true;head.mFrameStart=5;head.mFrameCount=12;
    head.mAnimTime=.625f;head.mAnimRate=36;head.mLoopType=3;head.mLoopCount=0;
    auto paperWire=Encode(paper);NativeSnapshotFrame paperCopy;NetBuffer paperBody;
    CHECK(DecodeNativeSnapshot(paperWire,eligible,kEpoch,paperCopy,paperBody));
    CHECK(paperCopy.sourceZombies[0].data.paperShootTicks==75&&paperCopy.sourceZombies[0].data.paperShootEvent==1&&
        paperCopy.sourceZombies[0].data.paperHead.mPresent&&paperCopy.sourceZombies[0].data.paperHead.mAnimTime==.625f&&
        paperCopy.sourceZombies[0].data.paperHead.mFrameStart==5&&Encode(paperCopy).mData==paperWire.mData);
    const size_t paperRecord=SourceCountOffset(paperWire)+2;
    for(int fault=0;fault<11;++fault)
    {
        auto bad=paperWire;
        if(fault==0)bad.mData[paperRecord+31]=76;
        if(fault==1)bad.mData[paperRecord+32]=3;
        if(fault==2)bad.mData[paperRecord+33]=2;
        if(fault==3)bad.mData[paperRecord+34]=2;
        if(fault==4)PutU16(bad,paperRecord+37,0);
        if(fault==5)PutF32(bad,paperRecord+39,(std::numeric_limits<float>::quiet_NaN)());
        if(fault==6)PutF32(bad,paperRecord+39,1.01f);
        if(fault==7)PutF32(bad,paperRecord+43,(std::numeric_limits<float>::infinity)());
        if(fault==8)bad.mData[paperRecord+47]=6;
        if(fault==9)bad.mData[paperRecord+33]=0; // Noncanonical absent head.
        if(fault==10)PutU16(bad,paperRecord+2,0); // Other kind cannot borrow104's head.
        RejectWireAtomically(bad);
    }
    auto stoppedVehicle=frame;stoppedVehicle.sourceZombies[0].data.kind=18;
    stoppedVehicle.sourceZombies[0].data.vehicleStopped=true;
    auto stoppedWire=Encode(stoppedVehicle);CHECK(DecodeNativeSnapshot(stoppedWire,eligible,kEpoch,paperCopy,paperBody));
    CHECK(paperCopy.sourceZombies[0].data.vehicleStopped);
    auto corruptStopped=stoppedWire;corruptStopped.mData[SourceCountOffset(stoppedWire)+2+49]=2;RejectWireAtomically(corruptStopped);
    corruptStopped=stoppedWire;PutU16(corruptStopped,SourceCountOffset(stoppedWire)+2+2,0);RejectWireAtomically(corruptStopped);
    std::printf("[ %s ] Fusion.SourceZombieWireTransactional checks=%d failures=%d\n",
        result.mFailures?"  FAILED  ":"       OK ",result.mChecks,result.mFailures);
    return result;
}
