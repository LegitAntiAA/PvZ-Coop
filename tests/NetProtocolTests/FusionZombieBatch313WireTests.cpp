#include "../../Lawn/Fusion/FusionNativeWire.h"
#include "../../Lawn/Net/NetCommon.h"
#include <cstdio>
#include <limits>
namespace
{
using namespace LawnFusion;using LawnNet::NetBuffer;
constexpr uint32_t epoch=313;
const std::vector<FusionBankEligibleCard> eligible={{0,false,100,7.5f}};
NativeSnapshotFrame Empty()
{
    FusionSeedBank bank;bank.Open(epoch,1,eligible,1);bank.Choose(epoch,0,0,false);
    bank.SetReady(epoch,0,true);bank.CommitIntro(epoch);bank.BeginPlaying(epoch);
    NativeSnapshotFrame f;f.epoch=epoch;f.selectionCommitted=true;f.selection=bank.State();
    for(const auto& p:f.selection.packets)
    {NativePacketData d;d.selectionId=p.id;d.kind=p.kind;d.owner=p.owner;d.price=p.price;d.rechargeSeconds=p.rechargeSeconds;f.packets.push_back({uint16_t(p.id+10),d});}
    return f;
}
NativeSnapshotFrame Actor(uint16_t kind)
{
    auto f=Empty();NativeSourceZombieData d;d.kind=kind;d.instanceToken=0x10029;d.originSpeed=1;d.health=float(FindFusionZombieProfile(kind)->maxHealth);
    f.sourceZombies.push_back({41,d});f.zombieIdentities.push_back({41,d.instanceToken});
    if(!FusionWaterZombie313Kind(kind)&&FindFusionSourceGraph313(kind)){NativeSourceGraph313Snapshot graph;MakeNative313GraphFixture(kind,graph);f.sourceGraphs313.push_back({41,graph});}
    if(FusionWaterZombie313Kind(kind))
    {FusionWaterZombie313View water;InitializeFusionWaterZombie313View(kind,water);if(kind!=20){water.auxiliary.mPresent=true;water.auxiliary.mFrameCount=1;}if(FusionWaterShooter343Kind(kind)){water.shootClock343.mPresent=true;water.shootClock343.mFrameCount=1;}f.waterZombies313.push_back({41,water});f.background=BACKGROUND_3_POOL;}
    if(NativeHeavyZombie313BKind(kind)){NativeHeavyZombie313Snapshot heavy;if(kind==32||kind==34){heavy.actor.status=14;heavy.actor.jumpSpeed=1.2f;}if(kind==45)heavy.actor.status=heavy.actor.bungee.status;f.heavyZombies313.push_back({41,heavy});}
    if(FindFusionExtendedZombie313Profile(kind)){FusionExtendedZombie313View v;if(kind==220)v.attackDamage=200;f.extendedZombies313.push_back({41,v});}
    return f;
}
NetBuffer Encode(const NativeSnapshotFrame& f,bool& okay)
{NetBuffer b;b.WriteU8(0xa3);okay=EncodeNativeSnapshot(f,eligible,b);return b;}
}
int RunFusionZombieBatch313WireTests(int& checks)
{
    int failed=0;const auto check=[&](bool okay,int line){++checks;if(!okay){++failed;std::printf("Batch313Wire:%d failed\n",line);}};
#define C313(x) check(bool(x),__LINE__)
    for(uint16_t kind=0;kind<256;++kind)
    {
        if(!FusionWaterZombie313Kind(kind)&&!NativeHeavyZombie313BKind(kind)&&!FindFusionExtendedZombie313Profile(kind))continue;
        auto f=Actor(kind);C313(ValidateNativeSnapshotFrame(f,eligible));bool okay=false;auto bytes=Encode(f,okay);C313(okay);
        NativeSnapshotFrame restored;NetBuffer body;
        C313(DecodeNativeSnapshot(bytes,eligible,epoch,restored,body)&&body.Size()==1&&body.mData[0]==0xa3);
        bool again=false;auto roundtrip=Encode(restored,again);C313(again&&roundtrip.mData==bytes.mData);
        auto orphan=f;orphan.sourceZombies.clear();C313(!ValidateNativeSnapshotFrame(orphan,eligible));
        if(!f.sourceGraphs313.empty()){auto missing=f;missing.sourceGraphs313.clear();C313(!ValidateNativeSnapshotFrame(missing,eligible));auto dirty=f;dirty.sourceGraphs313[0].data.layers[2].fadeElapsed=std::numeric_limits<float>::quiet_NaN();C313(!ValidateNativeSnapshotFrame(dirty,eligible));}
        if(!f.waterZombies313.empty()){auto duplicate=f;duplicate.waterZombies313.push_back(duplicate.waterZombies313[0]);C313(!ValidateNativeSnapshotFrame(duplicate,eligible));}
    }
    auto capture=Actor(45);CapturedSourcePlant314View view;view.epoch=epoch;view.captureToken=3;view.ownerId=41;view.ownerToken=0x10029;
    view.plantId=50;view.plantToken=0x10032;view.nativeArtwork=true;view.roles[0]=0;
    CapturedNativeReanim314 native;native.type=REANIM_PEASHOOTER;native.clock.mPresent=true;native.clock.mFrameCount=1;CapturedNativeTrack314 track;track.name="anim_idle";native.tracks.push_back(track);view.reanimations.push_back(native);
    FusionAnimationDrawOptions::ExtraState extra;extra.stateName="idle";extra.elapsedSeconds=.1f;extra.baseBlend=FusionAnimationDrawOptions::BaseBlend{"idle",.2f,.5f};view.pose.options.extraStates.push_back(extra);
    capture.capturedPlants314.push_back({41,view});C313(ValidateNativeSnapshotFrame(capture,eligible));
    auto invalidCapture=capture;invalidCapture.capturedPlants314[0].data.pose.options.extraStates[0].baseBlend->incomingWeight=std::numeric_limits<float>::infinity();
    C313(!ValidateNativeSnapshotFrame(invalidCapture,eligible));
    bool okay=false;auto bytes=Encode(capture,okay);C313(okay);
    for(size_t n=0;n<bytes.Size();++n)
    {
        NetBuffer shortPacket;shortPacket.mData.assign(bytes.mData.begin(),bytes.mData.begin()+n);
        auto dest=Empty();dest.sourcePoints=123;NetBuffer body;body.WriteU8(77);
        C313(!DecodeNativeSnapshot(shortPacket,eligible,epoch,dest,body)&&shortPacket.mReadPos==0&&dest.sourcePoints==123&&dest.capturedPlants314.empty()&&body.Size()==1&&body.mData[0]==77);
    }
    for(auto kind:{FusionSourceObstacle313Kind::DrownWeapon,FusionSourceObstacle313Kind::KirovBomb,FusionSourceObstacle313Kind::SuperBall,FusionSourceObstacle313Kind::MachineBall})
    {
        auto f=Empty();FusionSourceObstacle313View obstacle;obstacle.kind=kind;obstacle.instanceToken=0x10040;obstacle.ownerId=41;obstacle.ownerToken=0x10029;
        obstacle.ageTicks=20;obstacle.velocityX=-1;obstacle.velocityY=.5f;
        if(kind==FusionSourceObstacle313Kind::MachineBall){obstacle.clock.mPresent=true;obstacle.clock.mFrameCount=1;obstacle.hitCount=1;obstacle.lastHitAge=19;}
        f.sourceObstacles313.push_back({64,obstacle});C313(ValidateNativeSnapshotFrame(f,eligible));bool okay=false;auto bytes=Encode(f,okay);C313(okay);
        NativeSnapshotFrame restored;NetBuffer body;C313(DecodeNativeSnapshot(bytes,eligible,epoch,restored,body)&&restored.sourceObstacles313[0].data.ageTicks==20&&restored.sourceObstacles313[0].data.velocityY==.5f);
        auto bad=f;bad.sourceObstacles313[0].data.velocityX=std::numeric_limits<float>::quiet_NaN();C313(!ValidateNativeSnapshotFrame(bad,eligible));
        bad=f;bad.sourceObstacles313[0].data.lastHitAge=21;C313(!ValidateNativeSnapshotFrame(bad,eligible));
    }
    // Each genuine Machine family retains its live source owner while its
    // shared MachineBall is in flight. Never validate it only after owner death.
    for(uint16_t ownerKind:{40,210,219,205})
    {
        auto f=Actor(ownerKind);FusionSourceObstacle313View ball;
        ball.kind=FusionSourceObstacle313Kind::MachineBall;ball.instanceToken=0x10040;
        ball.ownerId=41;ball.ownerToken=0x10029;ball.clock.mPresent=true;ball.clock.mFrameCount=1;
        f.sourceObstacles313.push_back({64,ball});
        C313(ValidateNativeSnapshotFrame(f,eligible)==(ownerKind!=205));
        if(ownerKind!=205){bool okay=false;auto bytes=Encode(f,okay);C313(okay);}
    }
    auto money=Empty();money.sourcePoints=1234.5f;money.extendedBoard313={7,100000};bool moneyOkay=false;auto moneyBytes=Encode(money,moneyOkay);C313(moneyOkay);
    NativeSnapshotFrame moneyOut;NetBuffer moneyBody;C313(DecodeNativeSnapshot(moneyBytes,eligible,epoch,moneyOut,moneyBody)&&moneyOut.sourcePoints==1234.5f&&moneyOut.extendedBoard313.ultimateCount==7&&moneyOut.extendedBoard313.punishTime==100000);
    money.sourcePoints=std::numeric_limits<float>::infinity();C313(!ValidateNativeSnapshotFrame(money,eligible));
#undef C313
    return failed;
}
