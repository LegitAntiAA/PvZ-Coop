#include "FusionRollingProjectileTests.h"
#include "../../Lawn/Fusion/FusionRollingProjectileRules.h"
#include "../../Lawn/Fusion/FusionShooterTiming.h"
#include "../../Lawn/Fusion/FusionCherryShooterRules.h"
#include "../../Lawn/Fusion/FusionNativeWire.h"
#include "../../Lawn/Net/NetCommon.h"
#include <cmath>
#include <cstdio>
#include <limits>
#include <utility>

namespace
{
FusionRollingProjectileTestResult result;
void Check(bool value, int line)
{
    ++result.mChecks;
    if (!value) { ++result.mFailures; std::printf("    RollingProjectile check failed line=%d\n", line); }
}
#define CHECK(value) Check(!!(value), __LINE__)
bool Near(float a, float b) { return std::fabs(a-b)<.00001f; }
}

FusionRollingProjectileTestResult RunFusionRollingProjectileTests()
{
    using namespace LawnFusion;
    result={};
    std::printf("[ RUN      ] Fusion.RollingPierceSourceRulesAndWire\n");
    CHECK(GetNativeSeedForPlant(1004)==SEED_PEASHOOTER);
    const auto* definition=FusionFindPlant(1004);
    CHECK(definition && definition->hasStats && definition->maxHealth==4000);
    const auto* clock=FindFusionShooterTimingProfile(1004);
    CHECK(clock && !clock->speedParameterActive && clock->projectileKind==2);
    for (float speed:{.9f,1.0f,1.1f})
    {
        int counter=0;
        CHECK(clock && TryGetFusionShooterShotCounter(*clock,speed,counter) && counter==76);
        CHECK(clock && FusionShooterDuration(*clock,speed)==1);
        CHECK(FusionNativeAnimationSpeedValid(1004,speed));
    }
    for (const auto sample:{std::pair{4000,0},std::pair{2666,0},std::pair{2665,1},std::pair{1334,1},
        std::pair{1333,0},std::pair{1332,2},std::pair{1,2},std::pair{0,2}})
        CHECK(FusionNutShooterDamageStage(sample.first)==sample.second);

    float x=0,z=static_cast<float>(-(.69888-.1)*kFusionRollingPixelsY),velocity=kFusionRollingStartVelocity,rotation=0;
    CHECK(Near(z,-35.86107635498047f) && Near(velocity,.1173652708530426f));
    CHECK(AdvanceFusionRollingMotion(kFusionNutRollingMotion,x,z,velocity,rotation));
    CHECK(Near(x,1.28f) && Near(z,-35.685028076171875f) && Near(velocity,.1760479062795639f));
    CHECK(Near(rotation,-.18849556148052216f));
    CHECK(AdvanceFusionRollingMotion(kFusionNutRollingMotion,x,z,velocity,rotation));
    CHECK(Near(z,-35.45029830932617f) && Near(velocity,.2347305417060852f));
    for (int tick=3;tick<=32;++tick) CHECK(AdvanceFusionRollingMotion(kFusionNutRollingMotion,x,z,velocity,rotation));
    CHECK(Near(z,-1.1209537982940674f) && velocity>0);
    CHECK(AdvanceFusionRollingMotion(kFusionNutRollingMotion,x,z,velocity,rotation) && z==0 && velocity==0);
    CHECK(Near(rotation,-6.220355033874512f));
    CHECK(AdvanceFusionRollingMotion(kFusionNutRollingMotion,x,z,velocity,rotation) && z==0 && velocity==0);
    // Exact equality alone does not land: preserve positive velocity until a
    // following step crosses below the floor. Collider rotation is independent.
    x=0;z=-1;velocity=1;rotation=0;
    const FusionRollingMotionProfile equality{1,0,0};
    CHECK(AdvanceFusionRollingMotion(equality,x,z,velocity,rotation) && z==0 && velocity==1);
    CHECK(AdvanceFusionRollingMotion(equality,x,z,velocity,rotation) && z==0 && velocity==0);
    const float nan=(std::numeric_limits<float>::quiet_NaN)();
    z=1;const float savedX=x;
    CHECK(!AdvanceFusionRollingMotion(kFusionNutRollingMotion,x,z,velocity,rotation) && x==savedX && z==1);
    FusionRollingRect rect;
    CHECK(TryMakeFusionRollingRect(100,100,rect));
    CHECK(std::abs(rect.width-19.093333333333334)<1e-9 && std::abs(rect.height-24.97005988023952)<1e-9);
    CHECK(!TryMakeFusionRollingRect(nan,100,rect));
    for (double boundary:{-12.0,11.2}) CHECK(!FusionRollingProjectileExpired(boundary,0,2000,-10,9.2,-4.98,6.5));
    CHECK(!FusionRollingProjectileExpired(0,-4.98,2000,-10,9.2,-4.98,6.5));
    CHECK(!FusionRollingProjectileExpired(0,6.5,2000,-10,9.2,-4.98,6.5));
    CHECK(FusionRollingProjectileExpired(0,0,2001,-10,9.2,-4.98,6.5));
    CHECK(FusionRollingProjectileExpired(11.20001,0,0,-10,9.2,-4.98,6.5));
    CHECK(FusionRollingProjectileCanHit(0,true,false,false,270));
    CHECK(FusionRollingProjectileCanHit(1,true,false,false,270)); // Source enum1 is not native IsDeadOrDying.
    CHECK(FusionRollingProjectileCanHit(7,true,false,false,270)); // Source movingWay1 rule; adapter still normal0-only.
    for (int status:{3,9,10,12,13,16,20,21}) CHECK(!FusionRollingProjectileCanHit(status,true,false,false,270));
    CHECK(!FusionRollingProjectileCanHit(0,false,false,false,270));
    CHECK(!FusionRollingProjectileCanHit(0,true,true,false,270));
    CHECK(!FusionRollingProjectileCanHit(0,true,false,true,270));
    CHECK(!FusionRollingProjectileCanHit(0,true,false,false,0));
    CHECK(!FusionRollingProjectileCanHit(0,true,false,false,nan));

    FusionRollingHitHistory history;
    constexpr uint32_t p1=0x10001,p2=0x20001,z1=0x10002,z1Reused=0x20002;
    CHECK(!RememberFusionRollingHit(history,0,z1) && history.empty());
    CHECK(RememberFusionRollingHit(history,p1,z1));
    CHECK(!RememberFusionRollingHit(history,p1,z1));
    CHECK(RememberFusionRollingHit(history,p1,0x10003) && RememberFusionRollingHit(history,p1,0x10004));
    CHECK(history[p1].size()==3); // No stock two-entry piercing cap.
    CHECK(RememberFusionRollingHit(history,p1,z1Reused)); // Same slot, new zombie generation.
    CHECK(RememberFusionRollingHit(history,p2,z1)); // Same projectile slot, new projectile generation.
    CHECK(history[p1].size()==4 && history[p2].size()==1);
    history.erase(p1);
    CHECK(history.size()==1 && history[p2].contains(z1)); // Retire only that projectile.

    const std::vector<FusionBankEligibleCard> eligible={{0,false,100,7.5f},{0,true,200,7.5f},{3,false,50,30},{3,true,100,30}};
    FusionSeedBank bank;CHECK(bank.Open(66,1,eligible,3,kNativeFusionBankCapacity));
    CHECK(bank.Choose(66,0,0,false) && bank.Choose(66,0,3,false));
    CHECK(bank.Choose(66,1,0,true) && bank.Choose(66,1,3,true));
    CHECK(bank.SetReady(66,0,true) && bank.SetReady(66,1,true) && bank.CommitIntro(66) && bank.BeginPlaying(66));
    NativeSnapshotFrame frame;frame.epoch=66;frame.selectionCommitted=true;frame.selection=bank.State();
    for (const auto& selected:frame.selection.packets)
    {
        NativePacketData p;p.selectionId=selected.id;p.kind=selected.kind;p.isExtra=selected.isExtra;
        p.owner=selected.owner;p.price=selected.price;p.rechargeSeconds=selected.rechargeSeconds;
        frame.packets.push_back({selected.id,p});
    }
    NativePlantData plant;plant.instanceToken=0x10001u;plant.kind=1004;plant.firstParent=0;plant.secondParent=3;plant.packetId=2;plant.owner=0;
    plant.animationSpeed=1.03f;plant.shootSeconds=.1f;frame.plants.push_back({100,plant});
    NativeProjectileData bullet;bullet.kind=2;frame.projectiles.push_back({101,bullet});
    CHECK(ValidateNativeSnapshotFrame(frame,eligible));
    auto reverse=frame;reverse.plants[0].data.firstParent=3;reverse.plants[0].data.secondParent=0;reverse.plants[0].data.packetId=1;
    CHECK(ValidateNativeSnapshotFrame(reverse,eligible));
    auto bad=frame;bad.plants[0].data.secondParent=0;CHECK(!ValidateNativeSnapshotFrame(bad,eligible));
    bad=frame;bad.plants[0].data.firstParent=kFusionNoPlant;bad.plants[0].data.secondParent=kFusionNoPlant;CHECK(!ValidateNativeSnapshotFrame(bad,eligible));
    bad=frame;bad.projectiles[0].data.damage=60;CHECK(!ValidateNativeSnapshotFrame(bad,eligible));
    bad=frame;bad.projectiles[0].data.sunOnHit=5;CHECK(!ValidateNativeSnapshotFrame(bad,eligible));
    bad=frame;bad.projectiles[0].data.kind=3;CHECK(!ValidateNativeSnapshotFrame(bad,eligible));
    bad=frame;bad.plants[0].data.shootSeconds=1;CHECK(!ValidateNativeSnapshotFrame(bad,eligible));
    LawnNet::NetBuffer wire;wire.WriteU32(66);CHECK(EncodeNativeSnapshot(frame,eligible,wire));
    CHECK(wire.mData[4]==28 && wire.mData[5]==0); // Exact source status; impact history stays host-only.
    NativeSnapshotFrame decoded;LawnNet::NetBuffer native,input=wire;
    CHECK(DecodeNativeSnapshot(input,eligible,66,decoded,native));
    CHECK(decoded.plants[0].data.kind==1004 && decoded.projectiles[0].data.kind==2 && decoded.projectiles[0].data.damage==20);
    std::printf(result.mFailures ? "[  FAILED  ] Fusion.RollingPierceSourceRulesAndWire\n" : "[       OK ] Fusion.RollingPierceSourceRulesAndWire\n");
    return result;
}
