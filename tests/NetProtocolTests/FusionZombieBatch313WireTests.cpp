#include "../../Lawn/Fusion/FusionLateZombie363Rules.h"
#include "../../Lawn/Fusion/FusionNativeWire.h"
#include "../../Lawn/Net/NetCommon.h"
#include <cstdio>
#include <limits>
#include <algorithm>
#include <cstring>
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
    if(NativeHeavyZombie313BKind(kind)){NativeHeavyZombie313Snapshot heavy;if(kind==32||FusionSuperPogo363Kind(kind)){heavy.actor.status=14;heavy.actor.jumpSpeed=1.2f;}if(kind==45)heavy.actor.status=heavy.actor.bungee.status;f.heavyZombies313.push_back({41,heavy});}
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
        auto f=Actor(kind);if(!ValidateNativeSnapshotFrame(f,eligible))std::printf("Batch313 invalid canonical fixture kind=%u\n",unsigned(kind));C313(ValidateNativeSnapshotFrame(f,eligible));bool okay=false;auto bytes=Encode(f,okay);C313(okay);
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
    // Boss uses the same canonical source actor/three-layer snapshot. These
    // are pure codec fixtures, not claimed native body/driver/RV frame spans.
    for(uint16_t kind:{uint16_t(44),uint16_t(46)})
    {
        const auto* profile=FindFusionZombieProfile(kind);C313(profile!=nullptr);if(!profile)continue;
        C313(FindFusionSourceGraph313(kind)==FindFusionBoss361Graph(kind)&&FindFusionSourceGraph313(kind)!=nullptr);
        auto f=Actor(kind);f.background=BACKGROUND_6_BOSS;
        C313(f.sourceGraphs313.size()==1&&f.sourceGraphs313[0].data.layerCount==3);
        FusionBoss361View view;view.kind=kind; // Same source defaults as native Initialize.
        C313(FusionBoss361ViewValid(view));
        FusionBoss361Plan plan;uint64_t main=0,driver=0;
        C313(PlanFusionBoss361Skill(view,float(profile->maxHealth)/4,profile->maxHealth,plan));
        C313(ResolveFusionBoss361Plan(plan,3,2,0,view,main,driver));
        C313(view.phase==FusionBoss361Phase::Rv&&view.targetRow==3&&view.targetColumn==2);
        view.summonCount=7;view.spawnCount=3;view.summonBallCount=5;
        view.summonTime=2.25f;view.ballCountdown=.75f;view.ballType=1;view.eyeMask=3;
        C313(PlanFusionBoss361BodyStage(kind,profile->maxHealth,float(profile->maxHealth)/2,0,view.bodyStage));
        view.bodyHistoryMask=3;view.rvOffsetX=1.25f;view.rvOffsetY=-2.5f;view.rvAligned=true;
        view.ballBusy=true;view.rvVisible=true;
        if(kind==46){view.dying=true;view.sourceStatus=1;view.eyeBlackSeconds=.5f;}
        f.sourceZombies[0].data.health=float(profile->maxHealth)/2;
        const auto* graph=FindFusionBoss361Graph(kind);C313(graph!=nullptr);if(!graph||f.sourceGraphs313.empty())continue;
        bool rvState=false;
        for(uint8_t state=0;state<graph->stateCount;++state)
            if(FusionSourceGraph313OwnsState(*graph,0,state)&&std::strcmp(graph->states[state].name,"RV_1")==0)
            {f.sourceGraphs313[0].data.layers[0].state=f.sourceGraphs313[0].data.layers[0].outgoing=state;rvState=true;break;}
        C313(rvState);
        f.bossZombies361={{41,view}};
        auto& room=f.bossRoom366;room.enabled=true;room.boss2=kind==46;room.fastBoard=true;
        room.hammerGranted=true;room.gloveGranted=true;room.fakeTrophyNext=kind==44;
        room.hammerCooldownTicks=1234;room.enemyCount=1;room.lastTick=7654321;
        C313(ValidateNativeSnapshotFrame(f,eligible));
        bool okay=false;const auto bytes=Encode(f,okay);C313(okay);if(!okay)continue;
        auto input=bytes;NativeSnapshotFrame decoded;NetBuffer body;
        const bool read=DecodeNativeSnapshot(input,eligible,epoch,decoded,body);C313(read);
        C313(read&&input.Remaining()==0&&body.Size()==1&&body.mData[0]==0xa3&&decoded.bossZombies361.size()==1);
        if(read&&decoded.bossZombies361.size()==1)
        {
            const auto& v=decoded.bossZombies361[0].data;const auto& r=decoded.bossRoom366;
            C313(decoded.bossZombies361[0].id==41&&v.kind==kind&&v.phase==view.phase&&v.sourceStatus==view.sourceStatus&&
                v.targetRow==3&&v.targetColumn==2&&v.ballType==1&&v.bodyStage==view.bodyStage&&v.eyeMask==3&&
                v.summonCount==7&&v.spawnCount==3&&v.summonBallCount==5&&v.summonTime==2.25f&&v.ballCountdown==.75f&&
                v.bodyHistoryMask==3&&v.rvOffsetX==1.25f&&v.rvOffsetY==-2.5f&&v.rvAligned&&v.eyeBlackSeconds==(kind==46?.5f:0.f)&&
                v.ballBusy&&!v.colliderEnabled&&v.rvVisible&&v.dying==(kind==46));
            C313(r.enabled&&r.boss2==(kind==46)&&r.fastBoard&&r.hammerGranted&&r.gloveGranted&&r.fakeTrophyNext==(kind==44)&&
                !r.won&&!r.trophyFake&&!r.trophyClaimed&&r.hammerCooldownTicks==1234&&r.enemyCount==1&&
                !r.trophyId&&!r.trophyToken&&!r.trophyAnimationBaseAge&&r.trophyStartX==0&&r.trophyStartY==0&&r.lastTick==-1);
            C313(decoded.sourceZombies[0].data.instanceToken==0x10029&&decoded.sourceGraphs313.size()==1&&
                decoded.sourceGraphs313[0].data.layerCount==3&&decoded.sourceGraphs313[0].data.layers[0].state==f.sourceGraphs313[0].data.layers[0].state);
            bool again=false;C313(Encode(decoded,again).mData==bytes.mData&&again);
        }
        // Whole decode transaction at a nonzero cursor: every canonical field,
        // opaque body and private destination sentinel must remain unchanged.
        const auto rejectWire=[&](NetBuffer malformed)
        {
            NetBuffer prefixed;prefixed.WriteU8(0x77);prefixed.WriteBytes(malformed.Data(),malformed.Size());prefixed.mReadPos=1;
            const auto original=prefixed.mData;auto previous=f;previous.sourcePoints=123;previous.bossRoom366.lastTick=4242;
            previous.bossRoom366.hammerCooldownTicks=4321;bool saved=false;const auto before=Encode(previous,saved);C313(saved);
            NetBuffer raw;raw.WriteU8(0x55);C313(!DecodeNativeSnapshot(prefixed,eligible,epoch,previous,raw));
            bool after=false;const auto preserved=Encode(previous,after);
            C313(after&&preserved.mData==before.mData&&previous.bossRoom366.lastTick==4242&&
                prefixed.mReadPos==1&&prefixed.mData==original&&raw.Size()==1&&raw.mData[0]==0x55);
        };
        const auto rejectFrame=[&](const NativeSnapshotFrame& bad)
        {NetBuffer raw;raw.WriteU8(0x55);C313(!ValidateNativeSnapshotFrame(bad,eligible));C313(!EncodeNativeSnapshot(bad,eligible,raw)&&raw.Size()==1&&raw.mData[0]==0x55);};
        auto bad=f;bad.bossZombies361.clear();rejectFrame(bad);
        bad=f;bad.bossRoom366=NativeBossRoom366{};rejectFrame(bad);
        bad=f;bad.bossZombies361[0].data.kind=kind==44?46:44;rejectFrame(bad);
        bad=f;bad.sourceGraphs313.clear();rejectFrame(bad);
        bad=f;bad.sourceGraphs313[0].id=42;rejectFrame(bad);
        bad=f;bad.sourceGraphs313[0].data.layerCount=2;rejectFrame(bad);
        bad=f;bad.bossZombies361[0].data.summonTime=std::numeric_limits<float>::quiet_NaN();rejectFrame(bad);
        bad=f;bad.bossRoom366.trophyStartX=std::numeric_limits<float>::quiet_NaN();rejectFrame(bad);
        const size_t end=14+bytes.mData[6]+(size_t(bytes.mData[7])<<8);
        constexpr size_t bossRecordBytes=49,roomBytes=33,emptyTravelBytes=1,graphRecordBytes=80;
        const size_t boss=end-emptyTravelBytes-roomBytes-bossRecordBytes,roomAt=boss+bossRecordBytes;
        const size_t graphAt=boss-2-16-5*2-graphRecordBytes;
        C313(bytes.mData[boss]==41&&bytes.mData[boss+2]==kind&&bytes.mData[graphAt]==41&&bytes.mData[graphAt+2]==3);
        auto forged=bytes;forged.mData[boss+2]=uint8_t(kind==44?46:44);rejectWire(forged);
        forged=bytes;forged.mData[boss+4]=255;rejectWire(forged);
        forged=bytes;forged.mData[boss+46]=2;rejectWire(forged);
        forged=bytes;forged.mData[graphAt+2]=2;rejectWire(forged);
        forged=bytes;forged.mData[graphAt]=42;rejectWire(forged);
        forged=bytes;std::fill(forged.mData.begin()+roomAt,forged.mData.begin()+roomAt+roomBytes,0);
        forged.mData[roomAt+9]=0x70;forged.mData[roomAt+10]=0x17;rejectWire(forged);
        for(size_t at:{boss+23,roomAt+25})
        {forged=bytes;const uint32_t nan=0x7fc00000;for(unsigned n=0;n<4;++n)forged.mData[at+n]=uint8_t(nan>>(8*n));rejectWire(forged);}
        // Remove the Boss record without truncating metadata or corrupting the
        // unchanged native-body checksum: only the missing semantic view fails.
        forged=bytes;forged.mData[boss-2]=0;forged.mData.erase(forged.mData.begin()+boss,forged.mData.begin()+boss+bossRecordBytes);
        const size_t smaller=end-14-bossRecordBytes;forged.mData[6]=uint8_t(smaller);forged.mData[7]=uint8_t(smaller>>8);rejectWire(forged);
        auto privateTick=f;privateTick.bossRoom366.lastTick=-100;bool same=false;C313(Encode(privateTick,same).mData==bytes.mData&&same);
        // Post-death room retains the host's opaque trophy generation. No Boss
        // actor or proxy client UID is required to preserve that canonical view.
        auto trophy=Empty();trophy.background=BACKGROUND_6_BOSS;auto& t=trophy.bossRoom366;
        t.enabled=true;t.boss2=kind==46;t.trophyId=77;t.trophyToken=0xf123004d;t.trophyAnimationBaseAge=42;
        t.trophyStartX=312.5f;t.trophyStartY=204.25f;t.trophyFake=kind==44;t.trophyClaimed=true;t.won=kind==46;
        bool trophyOkay=false;auto trophyBytes=Encode(trophy,trophyOkay);C313(trophyOkay);NativeSnapshotFrame trophyOut;NetBuffer trophyBody;
        C313(DecodeNativeSnapshot(trophyBytes,eligible,epoch,trophyOut,trophyBody)&&trophyOut.bossZombies361.empty()&&
            trophyOut.bossRoom366.trophyToken==0xf123004d&&trophyOut.bossRoom366.trophyId==77&&
            trophyOut.bossRoom366.trophyStartX==312.5f&&trophyOut.bossRoom366.trophyStartY==204.25f&&
            trophyOut.bossRoom366.trophyAnimationBaseAge==42&&trophyOut.bossRoom366.trophyClaimed&&
            trophyOut.bossRoom366.won==(kind==46)&&trophyOut.bossRoom366.trophyFake==(kind==44));
        bad=trophy;bad.bossRoom366.trophyToken=0;rejectFrame(bad);
    }
    auto money=Empty();money.sourcePoints=1234.5f;money.extendedBoard313={7,100000};bool moneyOkay=false;auto moneyBytes=Encode(money,moneyOkay);C313(moneyOkay);
    NativeSnapshotFrame moneyOut;NetBuffer moneyBody;C313(DecodeNativeSnapshot(moneyBytes,eligible,epoch,moneyOut,moneyBody)&&moneyOut.sourcePoints==1234.5f&&moneyOut.extendedBoard313.ultimateCount==7&&moneyOut.extendedBoard313.punishTime==100000);
    money.sourcePoints=std::numeric_limits<float>::infinity();C313(!ValidateNativeSnapshotFrame(money,eligible));
#undef C313
    return failed;
}
