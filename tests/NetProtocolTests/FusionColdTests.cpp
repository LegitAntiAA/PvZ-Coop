#include "FusionColdTests.h"
#include "../../Lawn/Fusion/FusionColdRules.h"
#include "../../Lawn/Fusion/FusionStraightProjectileRules.h"
#include "../../Lawn/Fusion/FusionSplitShooterRules.h"
#include "../../Lawn/Fusion/FusionRollingProjectileRules.h"
#include "../../Lawn/Fusion/FusionNativeWire.h"
#include "../../Lawn/Fusion/FusionShooterTiming.h"
#include "../../Lawn/Net/NetCommon.h"
#include <cmath>
#include <cstdio>
#include <limits>

namespace
{
FusionColdTestResult result;
void Check(bool value, int line)
{
    ++result.mChecks;
    if (!value) { ++result.mFailures; std::printf("    Cold check failed line=%d\n",line); }
}
#define CHECK(value) Check(!!(value),__LINE__)
bool Same(const LawnFusion::FusionColdState& a,const LawnFusion::FusionColdState& b)
{ return a.level==b.level && a.threshold==b.threshold && a.chilledTicks==b.chilledTicks && a.frozenTicks==b.frozenTicks; }
const std::vector<LawnFusion::FusionBankEligibleCard> eligible = {
    {0,false,100,7.5f},{0,true,200,7.5f},{1,false,50,7.5f},{1,true,100,7.5f}};
LawnFusion::NativeSnapshotFrame Example()
{
    using namespace LawnFusion;
    FusionSeedBank bank;
    bank.Open(106,1,eligible,3,kNativeFusionBankCapacity);
    bank.Choose(106,0,0,false); bank.Choose(106,0,1,false);
    bank.Choose(106,1,0,true); bank.Choose(106,1,1,true);
    bank.SetReady(106,0,true); bank.SetReady(106,1,true);
    bank.CommitIntro(106); bank.BeginPlaying(106);
    NativeSnapshotFrame frame;
    frame.epoch=106; frame.selectionCommitted=true; frame.selection=bank.State();
    for (const auto& selected:frame.selection.packets)
    {
        NativePacketData packet;
        packet.selectionId=selected.id; packet.kind=selected.kind; packet.isExtra=selected.isExtra;
        packet.owner=selected.owner; packet.price=selected.price; packet.rechargeSeconds=selected.rechargeSeconds;
        frame.packets.push_back({selected.id,packet});
    }
    NativePlantData plant;
    plant.kind=10; plant.instanceToken=0x10001; plant.owner=0; plant.packetId=1;
    frame.plants.push_back({100,plant});
    NativeProjectileData projectile;
    projectile.kind=15; projectile.damage=20;
    frame.projectiles.push_back({101,projectile});
    frame.coldZombies.push_back({200,{250,200,1400,400}});
    return frame;
}
}

FusionColdTestResult RunFusionColdTests()
{
    using namespace LawnFusion;
    result={};
    std::printf("[ RUN      ] Fusion.SourceColdStateAndPierce\n");
    CHECK(kFusionIceShroomFuseTicks==151 && kFusionColdFreezeTicks==400 &&
        kFusionColdSlowTicks==1000 && kFusionColdFrozenSlowTicks==1400);
    for (int threshold:{100,200,300,400})
    {
        CHECK(FusionColdStateValid(0,threshold,0,0));
        CHECK(FusionColdStateValid(500,threshold,1400,400));
        CHECK(FusionColdStateValid((std::numeric_limits<int>::max)(),threshold,0,0));
    }
    CHECK(!FusionColdStateValid(-1,100,0,0));
    for (int bad:{0,99,101,500}) CHECK(!FusionColdStateValid(0,bad,0,0));
    CHECK(!FusionColdStateValid(0,100,-1,0) && !FusionColdStateValid(0,100,1501,0));
    CHECK(!FusionColdStateValid(0,100,0,-1) && !FusionColdStateValid(0,100,0,501));
    FusionColdState state, next;
    CHECK(TryAddFusionColdLevels({50,100,1000,0},50,next) && Same(next,{0,200,1400,400}));
    CHECK(TryAddFusionColdLevels({95,100,17,0},5,next) && Same(next,{0,200,1400,400}));
    CHECK(TryAddFusionColdLevels({95,100,17,100},50,next) && Same(next,{145,100,17,100}));
    const auto unchanged = next;
    CHECK(!TryAddFusionColdLevels({(std::numeric_limits<int>::max)()-4,100,0,0},5,next) && Same(next,unchanged));
    CHECK(!TryAddFusionColdLevels(state,0,next) && Same(next,unchanged));
    CHECK(!TryAddFusionColdLevels(state,-1,next) && Same(next,unchanged));
    CHECK(FusionColdAppliesAfterShield(60,false) && FusionColdAppliesAfterShield(60,true));
    CHECK(!FindFusionColdProjectileProfile(60)); // Distinct before/after level ordering.
    int damage=0;
    for (int hit=1;hit<=9;++hit)
    {
        CHECK(TryPrepareFusionColdHit(15,state,20,next,damage));
        CHECK(next.level==hit*10 && next.threshold==100 && next.frozenTicks==0 && damage==20);
        CHECK(TrySetFusionColdSlow(next,state));
    }
    CHECK(TryPrepareFusionColdHit(15,state,20,next,damage));
    CHECK(next.level==0 && next.threshold==200 && next.frozenTicks==400 && next.chilledTicks==1400 && damage==80);
    CHECK(!FusionColdAppliesAfterShield(15,false)); // Fully absorbed: new14s remains.
    CHECK(FusionColdAppliesAfterShield(15,true));
    CHECK(TrySetFusionColdSlow(next,state) && state.chilledTicks==1000 && state.frozenTicks==400);
    state={250,200,17,100};
    CHECK(TryPrepareFusionColdHit(15,state,20,next,damage) && next.level==260 && next.threshold==200 &&
        next.frozenTicks==100 && next.chilledTicks==17 && damage==80); // No threshold reset while frozen.
    CHECK(TrySetFusionColdFreeze(state,400,next) && next.frozenTicks==400 && next.chilledTicks==17);
    CHECK(TrySetFusionColdFreeze(next,200,state) && state.frozenTicks==400 && state.chilledTicks==17);
    CHECK(TrySetFusionColdSlow(state,next) && next.chilledTicks==1000 && next.frozenTicks==400);
    state={20,300,0,0};
    CHECK(TrySetFusionColdFreeze(state,400,next) && next.level==20 && next.threshold==300 && next.chilledTicks==1400);
    state={250,200,600,0}; // Actual native counter has reached0; only sourcelevel resets.
    CHECK(TryEndFusionColdFreeze(state,next) && next.level==0 && next.threshold==200 && next.chilledTicks==600 && next.frozenTicks==0);
    state={390,400,0,0};
    CHECK(TryPrepareFusionColdHit(15,state,20,next,damage) && next.threshold==400 && next.level==0 && damage==80);
    state={95,100,0,0};
    CHECK(TryPrepareFusionColdHit(18,state,20,next,damage) && next.level==0 && next.threshold==200 && damage==80);
    CHECK(FusionColdAppliesAfterShield(18,false) && FusionColdAppliesAfterShield(18,true));
    CHECK(!FusionColdAppliesAfterShield(0,true));
    const FusionColdState sentinel{7,300,8,9};
    next=sentinel; damage=99;
    state={(std::numeric_limits<int>::max)(),100,0,0};
    CHECK(!TryPrepareFusionColdHit(15,state,20,next,damage) && Same(next,sentinel) && damage==99);
    state={0,100,0,1};
    CHECK(!TryPrepareFusionColdHit(15,state,(std::numeric_limits<int>::max)(),next,damage) && Same(next,sentinel) && damage==99);
    CHECK(!TryPrepareFusionColdHit(17,state,20,next,damage) && Same(next,sentinel));
    CHECK(!TrySetFusionColdFreeze(state,0,next) && Same(next,sentinel));

    for (uint16_t kind:{uint16_t(15),uint16_t(18)})
    {
        const auto* cold=FindFusionColdProjectileProfile(kind);
        const auto* straight=FindFusionStraightProjectileProfile(kind);
        CHECK(cold && straight);
        if (!cold || !straight) continue;
        CHECK(cold->levelPerHit==(kind==15 ? 10 : 5) && cold->maximumHits==(kind==15 ? 1 : 3));
        CHECK(straight->damage==20 && straight->supportsSplitPhases==(kind==15) && straight->sourceCollider && straight->pixelsPerTick==3.2f);
        CHECK(FusionSplitProjectilePhaseValid(kind,0,MOTION_STRAIGHT));
        CHECK(FusionSplitProjectilePhaseValid(kind,1,MOTION_BACKWARDS)==(kind==15) &&
            FusionSplitProjectilePhaseValid(kind,2,MOTION_STRAIGHT)==(kind==15));
        CHECK(!FusionSplitProjectilePhaseValid(kind,1,MOTION_STRAIGHT) && !FusionSplitProjectilePhaseValid(kind,2,MOTION_BACKWARDS));
        unsigned int flags=99;
        CHECK(TryGetFusionSplitProjectileDamageFlags(kind,0,flags) && flags==(kind==15 ? 0U : 2U));
        if(kind==15) CHECK(TryGetFusionSplitProjectileDamageFlags(kind,1,flags) && flags==(1U<<DAMAGE_BYPASSES_SHIELD));
        else CHECK(!TryGetFusionSplitProjectileDamageFlags(kind,1,flags));
        CHECK(FusionStraightProjectileMetadataValid(kind,PROJECTILE_PEA,20,0));
        CHECK(!FusionStraightProjectileMetadataValid(kind,PROJECTILE_SNOWPEA,20,0));
        for (int age:{75,76,2000}) CHECK(!FusionStraightProjectileExpired(*straight,0,0,age,-10,9.2,-4.98,6.5));
        CHECK(FusionStraightProjectileExpired(*straight,0,0,2001,-10,9.2,-4.98,6.5));
        CHECK(FusionStraightProjectileExpired(*straight,0,0,0,-10,9.2,-4.98,6.5,1)==(kind!=15));
        CHECK(straight->width>20 && straight->width<26 && straight->height>9 && straight->height<39);
        CHECK(std::fabs(straight->width-(kind==15 ? 25.70586112 : 20.785342558942613))<1e-10);
        CHECK(std::fabs(straight->height-(kind==15 ? 38.86922909625845 : 9.39119309620066))<1e-10);
        CHECK(std::fabs(straight->offsetY-(kind==15 ? 7.258724100884551 : .47998997766669466))<1e-10);
    }
    for (uint16_t kind:{uint16_t(0),uint16_t(1),uint16_t(10)})
        CHECK(FindFusionStraightProjectileProfile(kind)->supportsSplitPhases && FusionSplitProjectilePhaseValid(kind,1,MOTION_BACKWARDS));
    FusionRollingHitHistory history;
    CHECK(FusionProjectileHitCount(history,0x10001)==0);
    CHECK(RememberFusionRollingHit(history,0x10001,0x10002));
    CHECK(!RememberFusionRollingHit(history,0x10001,0x10002));
    CHECK(RememberFusionRollingHit(history,0x10001,0x20002)); // Reused target slot/new generation.
    CHECK(RememberFusionRollingHit(history,0x10001,0x10003));
    CHECK(FusionProjectileHitCount(history,0x10001)==3);
    CHECK(FusionProjectileHitCount(history,0x20001)==0); // Reused projectile slot/new generation.

    for (uint16_t plant:{uint16_t(1034),uint16_t(1105),uint16_t(1106),uint16_t(1035),uint16_t(908)})
    {
        const auto* timing=FindFusionShooterTimingProfile(plant);
        CHECK(timing && timing->projectileKind==(plant==1035 || plant==908 ? 18 : 15));
        float x=0,y=0;
        CHECK(TryGetFusionColdMuzzle(plant,x,y) && x>0 && y>0);
    }
    auto frame=Example();
    CHECK(ValidateNativeSnapshotFrame(frame,eligible));
    LawnNet::NetBuffer encoded; encoded.WriteU8(0xC6);
    CHECK(EncodeNativeSnapshot(frame,eligible,encoded));
    NativeSnapshotFrame decoded;
    LawnNet::NetBuffer body;
    CHECK(DecodeNativeSnapshot(encoded,eligible,106,decoded,body) && encoded.Remaining()==0);
    CHECK(decoded.coldZombies.size()==1 && decoded.coldZombies[0].data.level==250 &&
        decoded.coldZombies[0].data.threshold==200 && decoded.coldZombies[0].data.chilledTicks==1400 &&
        decoded.coldZombies[0].data.frozenTicks==400);
    frame.coldZombies[0].data.level=-1; CHECK(!ValidateNativeSnapshotFrame(frame,eligible));
    frame=Example(); frame.coldZombies[0].data.threshold=150; CHECK(!ValidateNativeSnapshotFrame(frame,eligible));
    frame=Example(); frame.coldZombies[0].data.chilledTicks=1501; CHECK(!ValidateNativeSnapshotFrame(frame,eligible));
    frame=Example(); frame.coldZombies[0].data.frozenTicks=501; CHECK(!ValidateNativeSnapshotFrame(frame,eligible));
    frame=Example(); frame.projectiles[0].data.specialType=255; CHECK(!ValidateNativeSnapshotFrame(frame,eligible));
    // Explicit non-projectile IceAll does not impersonate a bullet ID.
    CHECK(!FusionColdDeliveryAppliesAfterShield(FusionColdDamageDelivery::Ice,false));
    CHECK(FusionColdDeliveryAppliesAfterShield(FusionColdDamageDelivery::Ice,true));
    CHECK(FusionColdDeliveryAppliesAfterShield(FusionColdDamageDelivery::IceAll,false));
    CHECK(FusionColdDeliveryAppliesAfterShield(FusionColdDamageDelivery::IceShieldless,false));
    CHECK(!FusionColdDeliveryAppliesAfterShield(FusionColdDamageDelivery::ProjectileProfile,true));
    CHECK(kFusionColdMaximumFreezeTicks==500 && kFusionColdMaximumSlowTicks==1500);
    CHECK(TrySetFusionColdFreeze({31,300,20,0},500,next) && Same(next,{31,300,1500,500}));
    CHECK(TrySetFusionColdSlow(next,state) && state.chilledTicks==1000 && state.frozenTicks==500);
    CHECK(TrySetFusionColdFreeze({31,300,17,1},500,next) && next.chilledTicks==17 && next.frozenTicks==500);
    next=sentinel;
    CHECK(!TrySetFusionColdFreeze({},501,next) && Same(next,sentinel));
    CHECK(FindFusionColdProjectileProfile(21) && FindFusionColdProjectileProfile(21)->maximumHits==1);
    CHECK(FindFusionColdProjectileProfile(24) && FindFusionColdProjectileProfile(24)->maximumHits==3);
    CHECK(TryPrepareFusionColdHit(21,{90,100,0,0},20,next,damage) && damage==80 && Same(next,{0,200,1400,400}));
    CHECK(TryPrepareFusionColdHit(24,{95,100,0,0},20,next,damage) && damage==80 && FusionColdAppliesAfterShield(24,false));
    CHECK(TryPrepareFusionColdHit(97,{},20,next,damage) && damage==40 && next.level==20);
    CHECK(TryPrepareFusionColdHit(97,{80,100,0,0},20,next,damage) && damage==160 && next.threshold==200);
    next=sentinel;damage=99;
    CHECK(!TryPrepareFusionColdHit(97,{0,100,0,1},(std::numeric_limits<int>::max)()/4,next,damage) && Same(next,sentinel) && damage==99);
    const auto* touch=FindFusionColdContactProfile(1036);
    CHECK(touch && touch->freezeTicks==400 && std::abs(touch->width-.48337584)<1e-12);
    CHECK(FusionColdContactCanTrigger(1036,0,true,false,false,0));
    CHECK(!FusionColdContactCanTrigger(1036,0,true,false,false,1));
    CHECK(!FusionColdContactCanTrigger(1036,0,true,false,true,0));
    CHECK(!FusionColdContactCanTrigger(1036,0,false,false,false,0));
    CHECK(!FusionColdContactCanTrigger(1036,0,true,true,false,0));
    CHECK(!FusionColdContactCanTrigger(6,0,true,false,false,0));
    for (int status:{1,7,10,12,16,20,21,22,23,24}) CHECK(!FusionColdBurstCanDamage(status,false,0,1));
    CHECK(FusionColdBurstCanDamage(0,false,-1,1) && FusionColdBurstCanDamage(0,false,1,1));
    CHECK(!FusionColdBurstCanDamage(0,false,2,1) && !FusionColdBurstCanDamage(0,true,0,1));
    std::printf("[ %s ] Fusion.SourceColdStateAndPierce checks=%d failures=%d\n",
        result.mFailures ? "  FAILED  " : "       OK ",result.mChecks,result.mFailures);
    return result;
}
