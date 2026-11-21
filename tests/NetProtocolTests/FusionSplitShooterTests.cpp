#include "FusionSplitShooterTests.h"
#include "../../Lawn/Fusion/FusionSplitShooterRules.h"
#include "../../Lawn/Fusion/FusionStraightProjectileRules.h"
#include "../../Lawn/Fusion/FusionShooterTiming.h"
#include "../../Lawn/Fusion/FusionNativeWire.h"
#include "../../Lawn/Net/NetCommon.h"
#include <cmath>
#include <cstdio>
#include <limits>

namespace
{
FusionSplitShooterTestResult result;
void Check(bool value, int line)
{
    ++result.mChecks;
    if (!value) { ++result.mFailures; std::printf("    SplitShooter check failed line=%d\n",line); }
}
#define CHECK(value) Check(!!(value),__LINE__)
const std::vector<LawnFusion::FusionBankEligibleCard> eligible = {
    {0,false,100,7.5f},{0,true,200,7.5f},{1,false,50,7.5f},{1,true,100,7.5f}};

LawnFusion::NativeSnapshotFrame Example()
{
    using namespace LawnFusion;
    FusionSeedBank bank;
    bank.Open(98,1,eligible,3,kNativeFusionBankCapacity);
    bank.Choose(98,0,0,false); bank.Choose(98,0,1,false);
    bank.Choose(98,1,0,true); bank.Choose(98,1,1,true);
    bank.SetReady(98,0,true); bank.SetReady(98,1,true);
    bank.CommitIntro(98); bank.BeginPlaying(98);
    NativeSnapshotFrame frame;
    frame.epoch=98; frame.selectionCommitted=true; frame.selection=bank.State();
    for (const auto& selected:frame.selection.packets)
    {
        NativePacketData packet;
        packet.selectionId=selected.id; packet.kind=selected.kind; packet.isExtra=selected.isExtra;
        packet.owner=selected.owner; packet.price=selected.price; packet.rechargeSeconds=selected.rechargeSeconds;
        frame.packets.push_back({selected.id,packet});
    }
    NativePlantData plant;
    plant.kind=1090; plant.firstParent=0; plant.secondParent=1030;
    plant.instanceToken=0x10001; plant.owner=0; plant.packetId=1;
    frame.plants.push_back({100,plant});
    NativeProjectileData projectile;
    projectile.kind=0; projectile.damage=20; projectile.specialType=1;
    frame.projectiles.push_back({101,projectile});
    return frame;
}
}

FusionSplitShooterTestResult RunFusionSplitShooterTests()
{
    using namespace LawnFusion;
    result={};
    std::printf("[ RUN      ] Fusion.SharedSplitEmissionsAndReturn\n");
    for (uint16_t unknown:{uint16_t(0),uint16_t(1001),uint16_t(1106),uint16_t(65535)})
        CHECK(GetFusionSplitEmissionSpecs(unknown).empty());
    struct Row { uint16_t plant,payload; float rearX,rearY,frontX,frontY; };
    for (const auto row:{Row{1090,0,-.9703125f,.600305796f,.514f,.6003f},
        Row{1108,1,-.9703125f,.600305796f,.414f,.6003f},Row{1153,10,-.2985f,.306f,.204f,.08175f},
        Row{1107,15,-.8624999724f,.66355f,.367999977f,.66355f}})
    {
        const auto specs=GetFusionSplitEmissionSpecs(row.plant);
        const auto* profile=FindFusionShooterTimingProfile(row.plant);
        CHECK(specs.size()==3 && profile && profile->eventClipSeconds.size()==2);
        if (specs.size()!=3 || !profile || profile->eventClipSeconds.size()!=2) continue;
        CHECK(GetNativeSeedForPlant(row.plant)==SEED_PEASHOOTER && profile->projectileKind==row.payload);
        CHECK(specs[0].eventIndex==0 && specs[1].eventIndex==0 && specs[2].eventIndex==1);
        CHECK(specs[0].specialType==1 && specs[1].specialType==0 && specs[2].specialType==1);
        CHECK(specs[0].muzzleX==row.rearX && specs[0].muzzleY==row.rearY &&
            specs[1].muzzleX==row.frontX && specs[1].muzzleY==row.frontY);
        for (const auto& spec:specs) CHECK(spec.projectileKind==row.payload && FusionSplitEmissionValid(row.plant,spec));
        auto forged=specs[0]; forged.muzzleX+=1;
        CHECK(!FusionSplitEmissionValid(row.plant,forged));
        forged=specs[0]; forged.specialType=2;
        CHECK(!FusionSplitEmissionValid(row.plant,forged));
        CHECK(!FusionSplitEmissionValid(row.plant==1107 ? 1090 : 1107,specs[0]));
        for (float speed:{.9f,1.0f,1.1f})
        {
            const int speedIndex=speed==.9f ? 0 : speed==1 ? 1 : 2;
            const int peaDeadlines[3][2]={{38,56},{34,50},{31,46}};
            const int puffDeadlines[3][2]={{43,71},{39,64},{35,58}};
            const int* deadlines=row.plant==1153 ? puffDeadlines[speedIndex] : peaDeadlines[speedIndex];
            int initial=0;
            CHECK(TryGetFusionShooterShotCounter(*profile,speed,initial) && initial==deadlines[1]+1);
            int total=0;
            for (int remaining=initial-1;remaining>=0;--remaining)
            {
                int oldCount=-1, indexedCount=0;
                CHECK(TryGetFusionShooterEventsAtCounter(*profile,speed,remaining,oldCount));
                for (std::size_t event=0;event<2;++event)
                {
                    bool due=false;
                    CHECK(TryGetFusionShooterEventAtCounter(*profile,speed,remaining,event,due));
                    CHECK(due==(remaining==initial-deadlines[event]));
                    if (!due) continue;
                    ++indexedCount;
                    for (const auto& spec:specs) if (spec.eventIndex==event) ++total;
                }
                CHECK(indexedCount==oldCount);
            }
            //1153 has3 named callbacks but only2unique deadlines. Never5 shots.
            CHECK(total==3);
        }
        bool unchanged=true;
        CHECK(!TryGetFusionShooterEventAtCounter(*profile,1,1,2,unchanged) && unchanged);
        CHECK(!TryGetFusionShooterEventAtCounter(*profile,1,-1,0,unchanged) && unchanged);
    }
    const double coincidentEvents[]={.001,.001};
    const FusionShooterTimingProfile coincident{65534,1.5f,coincidentEvents,1,.01,true,0};
    int initial=0,count=0;
    CHECK(TryGetFusionShooterShotCounter(coincident,1,initial) && initial==2);
    CHECK(TryGetFusionShooterEventsAtCounter(coincident,1,1,count) && count==2);
    for (std::size_t event=0;event<2;++event)
    {
        bool due=false;
        CHECK(TryGetFusionShooterEventAtCounter(coincident,1,1,event,due) && due);
    }
    for (uint16_t payload:{uint16_t(0),uint16_t(1),uint16_t(10)})
    {
        const auto* profile=FindFusionStraightProjectileProfile(payload);
        CHECK(profile);
        if (!profile) continue;
        CHECK(FusionSplitProjectilePhaseValid(payload,0,MOTION_STRAIGHT));
        CHECK(FusionSplitProjectilePhaseValid(payload,1,MOTION_BACKWARDS));
        CHECK(FusionSplitProjectilePhaseValid(payload,2,MOTION_STRAIGHT));
        CHECK(!FusionSplitProjectilePhaseValid(payload,0,MOTION_BACKWARDS));
        CHECK(!FusionSplitProjectilePhaseValid(payload,1,MOTION_STRAIGHT));
        CHECK(!FusionSplitProjectilePhaseValid(payload,3,MOTION_STRAIGHT));
        unsigned int flags=99;
        CHECK(TryGetFusionSplitProjectileDamageFlags(payload,1,flags) && flags==(payload==10 ? 0U : 1U));
        CHECK(TryGetFusionSplitProjectileDamageFlags(payload,0,flags) && flags==0);
        CHECK(TryGetFusionSplitProjectileDamageFlags(payload,2,flags) && flags==0);
        flags=99;
        CHECK(!TryGetFusionSplitProjectileDamageFlags(payload,3,flags) && flags==99);
        for (uint8_t phase:{uint8_t(0),uint8_t(1),uint8_t(2)})
        {
            const double right=9.2+(phase==0 ? 2.0 : 1.0);
            CHECK(!FusionStraightProjectileExpired(*profile,right,0,2000,-10,9.2,-4.98,6.5,phase));
            CHECK(FusionStraightProjectileExpired(*profile,std::nextafter(right,20.0),0,0,-10,9.2,-4.98,6.5,phase));
            CHECK(FusionStraightProjectileExpired(*profile,0,0,2001,-10,9.2,-4.98,6.5,phase));
            CHECK(FusionStraightProjectileExpired(*profile,0,6.50001,0,-10,9.2,-4.98,6.5,phase));
            CHECK(FusionStraightProjectileExpired(*profile,-100,0,76,-10,9.2,-4.98,6.5,phase)==(phase==0));
        }
        CHECK(!FusionStraightProjectileExpired(*profile,-12,0,76,-10,9.2,-4.98,6.5));
        CHECK(FusionStraightProjectileExpired(*profile,-12.000001,0,76,-10,9.2,-4.98,6.5));
    }
    uint8_t phase=99;
    CHECK(TryAdvanceFusionSplitProjectilePhase(-10,-10,1,phase) && phase==1);
    CHECK(TryAdvanceFusionSplitProjectilePhase(std::nextafter(-10.0,-20.0),-10,1,phase) && phase==2);
    CHECK(TryAdvanceFusionSplitProjectilePhase(-100,-10,2,phase) && phase==2);
    CHECK(TryAdvanceFusionSplitProjectilePhase(-100,-10,0,phase) && phase==0);
    phase=99;
    CHECK(!TryAdvanceFusionSplitProjectilePhase(0,-10,3,phase) && phase==99);
    CHECK(!TryAdvanceFusionSplitProjectilePhase(std::numeric_limits<double>::quiet_NaN(),-10,1,phase) && phase==99);
    for (int status:{1,3,7,9,10,12,16,20,21,22,23,24})
        CHECK(!FusionSplitShooterCanTarget(status,true,false,300,-8,9.2));
    CHECK(FusionSplitShooterCanTarget(0,true,false,300,-20,9.2));
    CHECK(FusionSplitShooterCanTarget(0,true,false,300,9.19999,9.2));
    CHECK(!FusionSplitShooterCanTarget(0,true,false,300,9.2,9.2));
    CHECK(!FusionSplitShooterCanTarget(0,false,false,300,0,9.2));
    CHECK(!FusionSplitShooterCanTarget(0,true,true,300,0,9.2));
    CHECK(!FusionSplitShooterCanTarget(0,true,false,0,0,9.2));

    const uint16_t parents[][3]={{1090,0,1030},{1090,1030,0},{1108,0,1008},{1108,2,1090},
        {1108,1001,1030},{1108,1008,0},{1108,1030,1001},{1108,1090,2},{1153,6,1090},{1153,1018,1030},{1153,1019,0}};
    for (const auto& row:parents)
    {
        auto frame=Example();
        frame.plants[0].data.kind=row[0]; frame.plants[0].data.firstParent=row[1]; frame.plants[0].data.secondParent=row[2];
        CHECK(ValidateNativeSnapshotFrame(frame,eligible));
        frame.plants[0].data.firstParent=65534;
        CHECK(!ValidateNativeSnapshotFrame(frame,eligible));
    }
    for (uint16_t kind:{uint16_t(0),uint16_t(1),uint16_t(10)})
        for (uint8_t special:{uint8_t(0),uint8_t(1),uint8_t(2)})
        {
            auto frame=Example();
            auto& projectile=frame.projectiles[0].data;
            projectile.kind=kind; projectile.damage=kind==1 ? 60 : 20; projectile.specialType=special;
            CHECK(ValidateNativeSnapshotFrame(frame,eligible));
            LawnNet::NetBuffer body; body.WriteU8(0x98);
            CHECK(EncodeNativeSnapshot(frame,eligible,body));
            NativeSnapshotFrame decoded;
            LawnNet::NetBuffer native;
            CHECK(DecodeNativeSnapshot(body,eligible,98,decoded,native) && body.Remaining()==0);
            CHECK(decoded.projectiles.size()==1 && decoded.projectiles[0].data.specialType==special &&
                decoded.projectiles[0].data.kind==kind && native.Size()==1 && native.mData[0]==0x98);
            projectile.specialType=5; CHECK(!ValidateNativeSnapshotFrame(frame,eligible)); //3/4 now represent Threepeater lanes.
            projectile.specialType=special; projectile.damage+=1; CHECK(!ValidateNativeSnapshotFrame(frame,eligible));
            --projectile.damage; projectile.sunOnHit=5; CHECK(!ValidateNativeSnapshotFrame(frame,eligible));
        }
    for (uint16_t kind:{uint16_t(2),uint16_t(8),uint16_t(9)})
    {
        auto frame=Example();
        auto& projectile=frame.projectiles[0].data;
        projectile.kind=kind; projectile.damage=kind==8 ? 100 : 20; projectile.sunOnHit=kind==8 ? 5 : 0;
        CHECK(!ValidateNativeSnapshotFrame(frame,eligible)); // Rear phase is straight-profile only.
        CHECK(!FusionSplitProjectilePhaseValid(kind,1,MOTION_BACKWARDS));
    }
    std::printf("[ %s ] Fusion.SharedSplitEmissionsAndReturn checks=%d failures=%d\n",
        result.mFailures ? "  FAILED  " : "       OK ",result.mChecks,result.mFailures);
    return result;
}
