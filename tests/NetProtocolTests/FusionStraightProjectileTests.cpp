#include "FusionStraightProjectileTests.h"
#include "../../Lawn/Fusion/FusionStraightProjectileRules.h"
#include "../../Lawn/Fusion/FusionCherryShooterRules.h"
#include "../../Lawn/Fusion/FusionGloveRules.h"
#include "../../Lawn/Fusion/FusionNativeWire.h"
#include "../../Lawn/Fusion/FusionShooterTiming.h"
#include "../../Lawn/Net/NetCommon.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace
{
FusionStraightProjectileTestResult result;
void Check(bool value, int line)
{
    ++result.mChecks;
    if (!value) { ++result.mFailures; std::printf("    StraightProjectile check failed line=%d\n", line); }
}
#define CHECK(value) Check(!!(value), __LINE__)
bool Near(double first, double second) { return std::fabs(first-second)<1e-12; }
const std::vector<LawnFusion::FusionBankEligibleCard> eligible = {
    {0,false,100,7.5f},{0,true,200,7.5f},{1,false,50,7.5f},{1,true,100,7.5f}};

LawnFusion::NativeSnapshotFrame Example()
{
    using namespace LawnFusion;
    FusionSeedBank bank;
    bank.Open(78,1,eligible,3,kNativeFusionBankCapacity);
    bank.Choose(78,0,0,false); bank.Choose(78,0,1,false);
    bank.Choose(78,1,0,true); bank.Choose(78,1,1,true);
    bank.SetReady(78,0,true); bank.SetReady(78,1,true);
    bank.CommitIntro(78); bank.BeginPlaying(78);
    NativeSnapshotFrame frame;
    frame.epoch=78; frame.selectionCommitted=true; frame.selection=bank.State();
    for (const auto& selected:frame.selection.packets)
    {
        NativePacketData packet;
        packet.selectionId=selected.id; packet.kind=selected.kind; packet.isExtra=selected.isExtra;
        packet.owner=selected.owner; packet.price=selected.price; packet.rechargeSeconds=selected.rechargeSeconds;
        frame.packets.push_back({selected.id,packet});
    }
    NativePlantData plant;
    plant.kind=1018; plant.firstParent=6; plant.secondParent=0;
    plant.instanceToken=0x10001; plant.owner=0; plant.packetId=1;
    plant.animationSpeed=1.03f; plant.shootSeconds=.1f;
    frame.plants.push_back({100,plant});
    NativeProjectileData projectile;
    projectile.kind=10; projectile.damage=20; projectile.sunOnHit=0;
    frame.projectiles.push_back({101,projectile});
    return frame;
}
}

FusionStraightProjectileTestResult RunFusionStraightProjectileTests()
{
    using namespace LawnFusion;
    result={};
    std::printf("[ RUN      ] Fusion.SharedStraightProjectileProfiles\n");
    const auto* pea=FindFusionStraightProjectileProfile(0);
    const auto* cherry=FindFusionStraightProjectileProfile(1);
    const auto* puffPea=FindFusionStraightProjectileProfile(10);
    CHECK(pea && cherry && puffPea);
    if (!pea || !cherry || !puffPea) return result;
    for (uint16_t unknown:{uint16_t(2),uint16_t(8),uint16_t(9),uint16_t(11),uint16_t(65535)})
        CHECK(!FindFusionStraightProjectileProfile(unknown) &&
            !FusionStraightProjectileMetadataValid(unknown,PROJECTILE_PEA,20,0));
    for (const auto* profile:{pea,cherry,puffPea})
    {
        CHECK(profile->nativeType==PROJECTILE_PEA && profile->pixelsPerTick==3.2f && profile->maximumAgeTicks==2000);
        CHECK(FusionStraightProjectileMetadataValid(profile->kind,PROJECTILE_PEA,profile->damage,0));
        CHECK(!FusionStraightProjectileMetadataValid(profile->kind,PROJECTILE_PUFF,profile->damage,0));
        CHECK(!FusionStraightProjectileMetadataValid(profile->kind,PROJECTILE_PEA,profile->damage+1,0));
        CHECK(!FusionStraightProjectileMetadataValid(profile->kind,PROJECTILE_PEA,profile->damage,5));
        for (int age:{0,75,76,2000})
            CHECK(!FusionStraightProjectileExpired(*profile,0,0,age,-10,9.2,-4.98,6.5));
        CHECK(FusionStraightProjectileExpired(*profile,0,0,2001,-10,9.2,-4.98,6.5));
        for (double x:{-12.000001,-12.0,0.0,11.2,11.200001})
            for (double y:{-4.980001,-4.98,0.0,6.5,6.500001})
                for (int age:{-1,0,2000,2001})
                    CHECK(FusionStraightProjectileExpired(*profile,x,y,age,-10,9.2,-4.98,6.5)==
                        FusionCherryProjectileExpired(x,y,age,-10,9.2,-4.98,6.5));
    }
    CHECK(pea->damage==20 && cherry->damage==60 && puffPea->damage==20 && !pea->sourceCollider);
    FusionStraightProjectileRect rect{1,2,3,4};
    CHECK(!TryMakeFusionStraightProjectileRect(*pea,100,200,rect) && rect.x==1 && rect.height==4);
    // Exact legacy Cherry doubles survive the shared profile extraction.
    for (double x:{-1000.0,-.1,0.0,100.0,4096.0})
        for (double y:{-100.0,0.0,.1,200.0})
        {
            FusionCherryProjectileRect old;
            CHECK(TryMakeFusionCherryProjectileRect(x,y,old) && TryMakeFusionStraightProjectileRect(*cherry,x,y,rect));
            CHECK(rect.x==old.x && rect.y==old.y && rect.width==old.width && rect.height==old.height);
        }
    CHECK(TryMakeFusionStraightProjectileRect(*puffPea,100,200,rect));
    CHECK(Near(rect.x,93.56655281066666) && Near(rect.y,192.59833808383234));
    CHECK(Near(rect.width,12.946372799999999) && Near(rect.height,21.02247784431138));
    CHECK(Near(puffPea->offsetX,.03973921066666666) && Near(puffPea->offsetY,3.109577005988024));
    CHECK(static_cast<int>(std::floor(rect.x))==93 && static_cast<int>(std::floor(rect.y))==192 &&
        static_cast<int>(std::ceil(rect.x+rect.width))-93==14 && static_cast<int>(std::ceil(rect.y+rect.height))-192==22);
    const double nan=(std::numeric_limits<double>::quiet_NaN)();
    const auto previous=rect;
    CHECK(!TryMakeFusionStraightProjectileRect(*puffPea,nan,0,rect) && rect.x==previous.x && rect.width==previous.width);
    CHECK(FusionStraightProjectileExpired(*puffPea,nan,0,0,-10,9.2,-4.98,6.5));
    CHECK(FusionStraightProjectileExpired(*puffPea,0,0,0,9.2,-10,-4.98,6.5));
    for (int status=0;status<=24;++status)
        CHECK(FusionStraightProjectileCanHit(status,true,false,270)==FusionCherryProjectileCanHit(status,true,false,270));
    for (int status:{3,7,9,10,12,13,16,20,21}) CHECK(!FusionStraightProjectileCanHit(status,true,false,270));
    CHECK(FusionStraightProjectileCanHit(0,true,false,270));
    CHECK(FusionStraightProjectileCanHit(1,true,false,270)); // Source1 is not a native dead phase.
    CHECK(!FusionStraightProjectileCanHit(0,false,false,270) && !FusionStraightProjectileCanHit(0,true,true,270));
    CHECK(!FusionStraightProjectileCanHit(0,true,false,0));

    struct PlantVector { uint16_t kind; size_t events; float muzzleX,muzzleY; };
    for (const auto v:{PlantVector{1018,1,.2695f,.084f},PlantVector{1019,2,.27025f,.084f},PlantVector{1065,4,.28802f,.09911f}})
    {
        CHECK(GetNativeSeedForPlant(v.kind)==SEED_PEASHOOTER);
        const auto* profile=FindFusionShooterTimingProfile(v.kind);
        const auto* definition=FusionFindPlant(v.kind);
        CHECK(profile && profile->projectileKind==10 && profile->eventClipSeconds.size()==v.events && profile->speedParameterActive);
        CHECK(definition && definition->hasStats && definition->damage==20 && definition->maxHealth==300);
        CHECK(profile && std::strcmp(profile->visualState,v.kind==1065 ? "shoot_g" : "shoot")==0);
        if (v.kind==1065) CHECK(profile && profile->stateSpeed==static_cast<double>(1.3f));
        float x=0,y=0;
        CHECK(TryGetFusionPuffPeaMuzzle(v.kind,x,y) && x==v.muzzleX && y==v.muzzleY);
        CHECK(FusionGloveSuccessCooldown(v.kind)==334);
    }
    for (uint16_t kind:{uint16_t(6),uint16_t(19)}) CHECK(FusionGloveSuccessCooldown(kind)==334);
    for (uint16_t kind:{uint16_t(0),uint16_t(1),uint16_t(1000),uint16_t(1001),uint16_t(1004),uint16_t(1030),uint16_t(1032)})
        CHECK(FusionGloveSuccessCooldown(kind)==1000);

    const auto frame=Example();
    const uint16_t ancestry[][3]={{1018,6,0},{1019,1018,0},{1019,6,1030},
        {1065,1019,1030},{1065,6,1032},{1065,1018,1090},{1065,1153,0}};
    for (const auto& parents:ancestry)
    {
        auto sample=frame;
        sample.plants[0].data.kind=parents[0]; sample.plants[0].data.firstParent=parents[1];
        sample.plants[0].data.secondParent=parents[2];
        CHECK(ValidateNativeSnapshotFrame(sample,eligible));
        auto invalid=sample;
        invalid.plants[0].data.secondParent=kFusionNoPlant;
        CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
        invalid=sample; invalid.projectiles[0].data.damage=60;
        CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
        invalid=sample; invalid.projectiles[0].data.sunOnHit=5;
        CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
        invalid=sample; invalid.projectiles[0].data.kind=11;
        CHECK(!ValidateNativeSnapshotFrame(invalid,eligible));
    }
    auto reversed=frame;
    reversed.plants[0].data.firstParent=0; reversed.plants[0].data.secondParent=6;
    CHECK(!ValidateNativeSnapshotFrame(reversed,eligible));
    LawnNet::NetBuffer wire; wire.WriteU8(0x78);
    CHECK(EncodeNativeSnapshot(frame,eligible,wire));
    NativeSnapshotFrame decoded;
    LawnNet::NetBuffer native,input=wire;
    CHECK(DecodeNativeSnapshot(input,eligible,78,decoded,native) && input.Remaining()==0);
    CHECK(decoded.plants[0].data.kind==1018 && decoded.projectiles[0].data.kind==10 &&
        decoded.projectiles[0].data.damage==20 && decoded.projectiles[0].data.sunOnHit==0);
    CHECK(native.Size()==1 && native.mData[0]==0x78);
    std::printf("[ %s ] Fusion.SharedStraightProjectileProfiles checks=%d failures=%d\n",
        result.mFailures ? "  FAILED  " : "       OK ",result.mChecks,result.mFailures);
    return result;
}
