#include "../Lawn/Fusion/FusionNativeEquipment268.h"
#include "../Lawn/Fusion/FusionNativeWire.h"
#include "../Lawn/Net/NetCommon.h"
#include <cstdio>
// Pure family-state admission and the shared FNS18 metadata codec.
int RunFusionNativeEquipment268Tests(int& checks)
{
    using namespace LawnFusion;int failures=0;checks=0;
    const auto check=[&](bool value,int line){++checks;if(!value){++failures;std::printf("FusionNativeEquipment268Tests:%d failed\n",line);}};
#define CHECK(x) check(bool(x),__LINE__)
    NativeEquipmentPlantState s;
    CHECK(NativeEquipmentPlantStateValid(25,s));CHECK(!NativeEquipmentPlantStateValid(26,s));
    s.componentSize=0;CHECK(!NativeEquipmentPlantStateValid(1092,s));s.componentSize=1024;CHECK(NativeEquipmentPlantStateValid(1092,s));
    s.componentSize=1025;CHECK(!NativeEquipmentPlantStateValid(1092,s));s.componentSize=1;
    s.retainedRowRange=5;CHECK(NativeEquipmentPlantStateValid(1100,s));CHECK(!NativeEquipmentPlantStateValid(25,s));s.retainedRowRange=2;
    s.targetItemId=7;CHECK(!NativeEquipmentPlantStateValid(25,s));s.targetItemToken=0x10007;
    CHECK(NativeEquipmentPlantStateValid(25,s));CHECK(!NativeEquipmentPlantStateValid(1154,s));
    s.targetItemId=0;s.targetItemToken=0;s.lastCapturedType=FusionEquipmentType::None;CHECK(!NativeEquipmentPlantStateValid(25,s));
    s.lastCapturedType=FusionEquipmentType::Helmet;CHECK(NativeEquipmentPlantStateValid(25,s));CHECK(!NativeEquipmentPlantStateValid(1092,s));
    s.lastCapturedType=FusionEquipmentType::Bucket;s.seaCallbacks=2;CHECK(NativeEquipmentPlantStateValid(1101,s));
    s.seaCallbacks=3;CHECK(!NativeEquipmentPlantStateValid(1101,s));s.seaGrown=true;CHECK(NativeEquipmentPlantStateValid(1101,s));
    CHECK(!NativeEquipmentPlantStateValid(25,s));s.seaCallbacks=0;s.seaGrown=false;
    s.pendingItemDraw=0;CHECK(NativeEquipmentPlantStateValid(1154,s));CHECK(!NativeEquipmentPlantStateValid(25,s));
    s.pendingItemDraw=9;CHECK(NativeEquipmentPlantStateValid(1154,s));s.pendingItemDraw=10;CHECK(!NativeEquipmentPlantStateValid(1154,s));

    const std::vector<FusionBankEligibleCard> eligible={{0,false,100,7.5f}};
    FusionSeedBank bank;
    CHECK(bank.Open(268,1,eligible,1,1) && bank.Choose(268,0,0,false) && bank.SetReady(268,0,true) &&
        bank.CommitIntro(268) && bank.BeginPlaying(268));
    NativeSnapshotFrame frame;frame.epoch=268;frame.selectionCommitted=true;frame.selection=bank.State();
    for(const auto& selected:frame.selection.packets)
    {
        NativePacketData packet;packet.selectionId=selected.id;packet.kind=selected.kind;packet.isExtra=selected.isExtra;
        packet.owner=selected.owner;packet.randomOrigin=selected.randomOrigin;packet.price=selected.price;packet.rechargeSeconds=selected.rechargeSeconds;
        frame.packets.push_back({100,packet});
    }
    // Real equipment Pumpkins retain the common Defender thirds stage in the
    // same native Plant record; component metadata must not force stage zero.
    for(uint16_t kind:{uint16_t(1092),uint16_t(1097),uint16_t(1098),uint16_t(1099),uint16_t(935)})
    {
        NativePlantData plant;plant.kind=kind;plant.instanceToken=0x10000u+kind;plant.owner=LawnNet::NET_SHARED_OWNER;
        frame.plants={{201,plant}};frame.equipmentPlants={{201,NativeEquipmentPlantState{}}};
        for(uint8_t stage:{uint8_t(0),uint8_t(1),uint8_t(2)})
        {
            frame.plants[0].data.damageStage=stage;
            CHECK(ValidateNativeSnapshotFrame(frame,eligible));
            LawnNet::NetBuffer wire;wire.WriteU16(0xa55a);
            CHECK(EncodeNativeSnapshot(frame,eligible,wire));
            NativeSnapshotFrame decoded;LawnNet::NetBuffer body;
            CHECK(DecodeNativeSnapshot(wire,eligible,268,decoded,body));
            CHECK(wire.Remaining()==0 && decoded.plants.size()==1 && decoded.plants[0].data.kind==kind &&
                decoded.plants[0].data.damageStage==stage && decoded.equipmentPlants.size()==1 &&
                decoded.equipmentPlants[0].data.componentSize==1 && body.Size()==2 && body.mData[0]==0x5a && body.mData[1]==0xa5);
        }
        frame.plants[0].data.damageStage=3;
        CHECK(!ValidateNativeSnapshotFrame(frame,eligible));
        LawnNet::NetBuffer unchanged;unchanged.WriteU16(0xa55a);const auto original=unchanged.mData;
        CHECK(!EncodeNativeSnapshot(frame,eligible,unchanged) && unchanged.mReadPos==0 && unchanged.mData==original);
    }
#undef CHECK
    std::printf("FusionNativeEquipment268Tests checks=%d failures=%d\n",checks,failures);return failures;
}
