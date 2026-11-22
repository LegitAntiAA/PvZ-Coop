#include "WorldRemainder269StateTests.h"
#include "../../Lawn/Fusion/FusionNativeWire.h"
#include "../../Lawn/Fusion/FusionNativeWorldRemainder269.h"
#include "../../Lawn/Fusion/FusionCatalog.h"
#include "../../Lawn/Net/NetCommon.h"
#include <cstdio>
#include <limits>

namespace
{
const std::vector<LawnFusion::FusionBankEligibleCard> eligible={
    {0,false,100,7.5f},{0,true,200,7.5f},{1,false,50,7.5f},{1,true,100,7.5f}};
LawnFusion::NativeSnapshotFrame Example()
{
    using namespace LawnFusion;FusionSeedBank bank;bank.Open(19,1,eligible,3);
    bank.Choose(19,0,0,false);bank.Choose(19,0,1,false);
    bank.Choose(19,1,0,true);bank.Choose(19,1,1,true);
    bank.SetReady(19,0,true);bank.SetReady(19,1,true);bank.CommitIntro(19);bank.BeginPlaying(19);
    NativeSnapshotFrame frame;frame.epoch=19;frame.selectionCommitted=true;frame.selection=bank.State();
    for(const auto& selected:frame.selection.packets)
    {
        NativePacketData d;d.selectionId=selected.id;d.kind=selected.kind;d.isExtra=selected.isExtra;
        d.owner=selected.owner;d.randomOrigin=selected.randomOrigin;d.price=selected.price;d.rechargeSeconds=selected.rechargeSeconds;
        frame.packets.push_back({uint16_t(selected.id+10),d});
    }
    return frame;
}
}
int RunWorldRemainder269StateTests(int& checks)
{
    using namespace LawnFusion;int failures=0;
    const auto check=[&](bool pass,int line){++checks;if(!pass){++failures;std::printf("    WorldRemainder269State line=%d\n",line);}};
#define CHECK(x) check(bool(x),__LINE__)
    NativeWorldRemainder269BoardState board;CHECK(NativeWorldRemainder269BoardStateValid(board));
    CHECK(FusionWorldRemainder269ClockValid(909,0,253,1,-1));
    CHECK(!FusionWorldRemainder269ClockValid(909,0,254,1,-1));
    CHECK(!FusionWorldRemainder269ClockValid(926,0,1,1,-1));
    CHECK(!FusionWorldRemainder269ClockValid(918,0,1,1,-1));
    board.passiveDeadline=1;CHECK(!NativeWorldRemainder269BoardStateValid(board));
    board.initialized=true;board.passiveDeadline=(std::numeric_limits<int>::max)();
    board.activeDeadline=70000;board.ultimateDeadline=90000;board.pending={{80000,0},{80001,1},{80002,2},{80003,5}};
    CHECK(NativeWorldRemainder269BoardStateValid(board));
    board.pending[0].type=3;CHECK(!NativeWorldRemainder269BoardStateValid(board));board.pending[0].type=0;
    board.pending[0].deadline=-1;CHECK(!NativeWorldRemainder269BoardStateValid(board));board.pending[0].deadline=80000;
    auto many=board;many.pending.resize(1024,{80000,0});CHECK(NativeWorldRemainder269BoardStateValid(many));
    many.pending.push_back({80000,0});CHECK(!NativeWorldRemainder269BoardStateValid(many));
    NativeWorldRemainder269AuxState meteor;meteor.family=NativeWorldRemainder269Family::Meteorite;meteor.instanceToken=0x10001;
    meteor.type=5;meteor.sun=true;meteor.targetY=250;CHECK(NativeWorldRemainder269AuxStateValid(meteor));
    auto bad=meteor;bad.type=4;CHECK(!NativeWorldRemainder269AuxStateValid(bad));
    bad=meteor;bad.parent={1,2};CHECK(!NativeWorldRemainder269AuxStateValid(bad));
    bad=meteor;bad.targetX=1;CHECK(!NativeWorldRemainder269AuxStateValid(bad));
    bad=meteor;bad.ageTicks=-1;CHECK(!NativeWorldRemainder269AuxStateValid(bad));
    bad=meteor;bad.targetY=(std::numeric_limits<float>::quiet_NaN)();CHECK(!NativeWorldRemainder269AuxStateValid(bad));
    NativeWorldRemainder269AuxState fly;fly.instanceToken=0x10002;fly.parent={401,0x20001};fly.parentDamage=20;
    CHECK(NativeWorldRemainder269AuxStateValid(fly));
    fly.ready=true;fly.ageTicks=100;fly.nextShotAge=110;fly.shots=57;CHECK(NativeWorldRemainder269AuxStateValid(fly));
    bad=fly;bad.nextShotAge=111;CHECK(!NativeWorldRemainder269AuxStateValid(bad));
    bad=fly;bad.parent.token=0;CHECK(!NativeWorldRemainder269AuxStateValid(bad));
    bad=fly;bad.shots=58;CHECK(!NativeWorldRemainder269AuxStateValid(bad));
    bad=fly;bad.parentDamage=(std::numeric_limits<int>::max)()/3+1;CHECK(!NativeWorldRemainder269AuxStateValid(bad));
    bad=fly;bad.ageTicks=(std::numeric_limits<int>::max)();bad.nextShotAge=bad.ageTicks;CHECK(NativeWorldRemainder269AuxStateValid(bad));
    NativeWorldRemainder269ShotState shot{{401,0x20001},42};CHECK(NativeWorldRemainder269ShotStateValid(shot));
    shot.parent.token=0;CHECK(!NativeWorldRemainder269ShotStateValid(shot));shot.parent.token=0x20001;
    shot.ageTicks=-1;CHECK(!NativeWorldRemainder269ShotStateValid(shot));shot.ageTicks=42;

    auto frame=Example();CHECK(ValidateNativeSnapshotFrame(frame,eligible));frame.worldRemainder269Board=board;frame.goldHypnoSummoned=true;
    frame.worldRemainder269Aux={{501,meteor},{502,fly}};
    NativeProjectileData bullet;bullet.kind=91;bullet.damage=FusionFindPlant(926)->damage;bullet.specialType=0;
    frame.projectiles={{503,bullet}};frame.worldRemainder269Shots={{503,shot}};
    // Source ownerDamage and parent reference survive a retired managed owner.
    CHECK(ValidateNativeSnapshotFrame(frame,eligible));
    auto invalid=frame;invalid.worldRemainder269Aux[1].data.instanceToken=meteor.instanceToken;
    CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
    invalid=frame;invalid.worldRemainder269Aux[0].id=503;CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
    invalid=frame;invalid.worldRemainder269Shots[0].id=504;CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
    invalid=frame;invalid.worldRemainder269Shots.clear();CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
    invalid=frame;invalid.projectiles[0].data.kind=0;CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
    NativePlantData parent;parent.kind=0;parent.owner=LawnNet::NET_SHARED_OWNER;parent.instanceToken=0x20001;
    invalid=frame;invalid.plants={{401,parent}};CHECK(!ValidateNativeSnapshotFrame(invalid,eligible)); // Live owner must be actual926.
    parent.kind=926;auto live=frame;live.plants={{401,parent}};CHECK(ValidateNativeSnapshotFrame(live,eligible));
    live.plants[0].data.instanceToken=0x20002;CHECK(!ValidateNativeSnapshotFrame(live,eligible));
    auto summon=Example();parent.kind=1104;parent.pendingProductionTicks=253;summon.plants={{401,parent}};
    CHECK(ValidateNativeSnapshotFrame(summon,eligible));summon.plants[0].data.pendingProductionTicks=254;CHECK(!ValidateNativeSnapshotFrame(summon,eligible));
    summon.plants[0].data.kind=1093;summon.plants[0].data.pendingProductionTicks=1;CHECK(!ValidateNativeSnapshotFrame(summon,eligible));

    LawnNet::NetBuffer body;body.WriteU8(0x42);CHECK(EncodeNativeSnapshot(frame,eligible,body));
    NativeSnapshotFrame decoded;LawnNet::NetBuffer raw;CHECK(DecodeNativeSnapshot(body,eligible,19,decoded,raw));
    CHECK(decoded.goldHypnoSummoned&&decoded.worldRemainder269Board.pending.size()==4&&
        decoded.worldRemainder269Board.passiveDeadline==(std::numeric_limits<int>::max)()&&
        decoded.worldRemainder269Aux.size()==2&&decoded.worldRemainder269Aux[1].data.parent.token==0x20001&&
        decoded.worldRemainder269Aux[1].data.nextShotAge==110&&decoded.worldRemainder269Shots[0].data.ageTicks==42&&raw.Size()==1&&raw.mData[0]==0x42);
    auto empty=Example();LawnNet::NetBuffer emptyWire;CHECK(EncodeNativeSnapshot(empty,eligible,emptyWire));
    const size_t meta=size_t(emptyWire.mData[6])|(size_t(emptyWire.mData[7])<<8),end=14+meta;
    // Locate this semantic Bool by a single-field encoded probe. Later FNS
    // vectors/board fields may be appended without changing this assertion.
    auto goldProbe=empty;goldProbe.goldHypnoSummoned=true;LawnNet::NetBuffer goldWire;
    CHECK(EncodeNativeSnapshot(goldProbe,eligible,goldWire));CHECK(goldWire.Size()==emptyWire.Size());
    size_t boolOffset=end,differences=0;
    for(size_t i=14;i<end;++i)if(goldWire.mData[i]!=emptyWire.mData[i]){boolOffset=i;++differences;}
    CHECK(differences==1&&boolOffset<end&&emptyWire.mData[boolOffset]==0&&goldWire.mData[boolOffset]==1);
    if(differences!=1||boolOffset>=end)return failures;
    auto forged=emptyWire;forged.mData[boolOffset]=2;
    NativeSnapshotFrame retained;retained.epoch=777;LawnNet::NetBuffer retainedBody;retainedBody.WriteU8(0x99);
    CHECK(!DecodeNativeSnapshot(forged,eligible,19,retained,retainedBody)&&forged.mReadPos==0&&retained.epoch==777&&retainedBody.mData[0]==0x99);
#undef CHECK
    return failures;
}
