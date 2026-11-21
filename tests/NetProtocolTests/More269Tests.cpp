#include "More269Tests.h"
#include "../../Lawn/Fusion/FusionMorePlants269Rules.h"
#include "../../Lawn/Fusion/FusionMorePlants269BackendRules.h"
#include "../../Lawn/Fusion/FusionCherryShooterRules.h"
#include "../../Lawn/Fusion/FusionSeedBank.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <initializer_list>
#include <limits>

int RunMore269Tests(int& checks)
{
    using namespace LawnFusion;int failures=0;
    const auto check=[&](bool pass,int line){++checks;if(!pass){++failures;std::printf("    More269 line=%d\n",line);}};
#define CHECK(x) check(bool(x),__LINE__)
    // Regression: generic source Start randomizes Speed even without a shot clip.
    for(uint16_t kind:{241,244,248,254,905,911,922,1110,1178,1147,1164,1166,34,937})
    {CHECK(FusionNativeAnimationSpeedValid(kind,1.03583908f));CHECK(FusionNativeAnimationSpeedValid(kind,.9f));CHECK(FusionNativeAnimationSpeedValid(kind,1.1f));CHECK(!FusionNativeAnimationSpeedValid(kind,1.1001f));}
    FusionBankEligibleCard gift;CHECK(TryMakeFusionEligibleCard(250,false,gift));CHECK(gift.baseCost==-100);
    for(int origin=0;origin<=10;++origin)CHECK(FusionBankPacketCost(gift.baseCost,origin)==-100);
    CHECK(TryMakeFusionEligibleCard(250,true,gift));CHECK(gift.baseCost==-200);
    CHECK(TryMakeFusionEligibleCard(250,false,gift));
    FusionSeedBank rewardBank;const std::vector<FusionBankEligibleCard> rewardPool{gift};
    CHECK(rewardBank.Open(1,2,rewardPool,1,1));CHECK(rewardBank.Choose(1,0,250,false));
    CHECK(rewardBank.State().packets.size()==1&&rewardBank.State().packets[0].price==-100);
    CHECK(FusionSeedBank::ValidateSnapshot(rewardBank.State(),rewardPool,1));
    auto malformed=rewardBank.State();malformed.packets[0].price=-99;
    CHECK(!FusionSeedBank::ValidateSnapshot(malformed,rewardPool,1));
    FusionSeedBank forged;gift.kind=0;CHECK(!forged.Open(1,2,{gift},1,1));
    constexpr std::array<uint16_t,6> first={0,0,0,0,39,9};
    constexpr std::array<unsigned,4> sizes={6,9,11,15};
    constexpr std::array<float,4> intervals={3,2.5f,2.5f,2};
    // Four ordinary Peas are four weighted outcomes, not a de-duplicated set.
    for(unsigned stage=0;stage<4;++stage)
    {
        const auto roster=FusionHamburger269Roster(stage);
        CHECK(roster.size()==sizes[stage]);
        CHECK(std::equal(first.begin(),first.end(),roster.begin()));
        CHECK(std::count(roster.begin(),roster.end(),uint16_t(0))==4);
        if(stage==1)CHECK(roster[6]==37&&roster[7]==55&&roster[8]==28);
        if(stage==2)CHECK(roster[6]==59&&roster[7]==86&&roster[8]==37&&roster[9]==55&&roster[10]==28);
        if(stage==3)CHECK(roster[6]==60&&roster[7]==15&&roster[8]==21&&roster[9]==24&&roster[10]==59&&roster[11]==86&&roster[12]==37&&roster[13]==55&&roster[14]==28);
        const auto* timing=FindFusionMorePlants269Timing(243,stage);
        CHECK(timing&&timing->attackIntervalSeconds==intervals[stage]&&timing->eventClipSeconds.size()==8&&
            timing->clipDurationSeconds==1&&timing->stateSpeed==1&&timing->speedParameterActive);
        unsigned events=0;for(unsigned event=0;event<8;++event)events+=FusionHamburger269EventAllowed(stage,event);
        CHECK(events==2*(stage+1));CHECK(!FusionHamburger269EventAllowed(stage,8));
        for(unsigned draw=0;draw<roster.size();++draw)for(int color=0;color<4;++color)
        {
            FusionHamburger269Shot shot{};
            const int base=20+(stage==1?10:stage>=2?20:0);
            const int heat=roster[draw]==0?(color==0?20:color==1?40:color==2?60:0):0;
            CHECK(TryChooseFusionHamburger269Shot(stage,20,int(draw),color,shot)&&shot.kind==roster[draw]&&
                shot.damage==base+heat&&shot.fire==(roster[draw]==0&&color<3?color+1:0));
            CHECK(FusionMore269ProjectileMetadataValid(shot.kind,shot.damage,shot.fire,0,
                FusionMore269ProjectileType(shot.kind,shot.fire),FusionMore269ProjectileMotion(shot.kind),
                shot.fire?uint16_t(17):uint16_t(0),shot.fire?12345U:0U));
        }
    }
    CHECK(FusionHamburger269Roster(4).empty());CHECK(!FindFusionMorePlants269Timing(243,4));
    CHECK(!FindFusionMorePlants269Timing(248));CHECK(!FindFusionMorePlants269Timing(900));
    CHECK(!FusionHamburger269EventAllowed(4,0));
    const auto* present=FindFusionMorePlants269Timing(256);
    CHECK(present&&present->eventClipSeconds.size()==1&&present->eventClipSeconds[0]==1.25);
    // Source Present starts once at Awake, without a periodic launch interval.
    CHECK(present&&present->attackIntervalSeconds==0&&FusionShooterTimingProfileValid(*present));
    if(present)
    {
        for(float speed:{.9f,1.f,1.1f})
        {
            int counter=0,events=0,total=0;
            CHECK(TryGetFusionShooterShotCounter(*present,speed,counter)&&counter>1);
            for(int remaining=counter-1;remaining>=0;--remaining)
            {CHECK(TryGetFusionShooterEventsAtCounter(*present,speed,remaining,events));total+=events;}
            CHECK(total==1&&FusionShooterDuration(*present,speed)>0);
            counter=77;
            CHECK(!TryGetFusionShooterInitialTicks(*present,.5f,counter)&&counter==77);
            CHECK(!TryGetFusionShooterCycleTicks(*present,1.f,counter)&&counter==77);
        }
        auto negativeInterval=*present;negativeInterval.attackIntervalSeconds=-1;
        CHECK(!FusionShooterTimingProfileValid(negativeInterval));
    }
    FusionHamburger269Shot unchanged{777,888,9};
    for(const auto args:std::array<std::array<int,4>,6>{{{4,20,0,0},{0,-1,0,0},{0,20,-1,0},{0,20,6,0},{0,20,0,-1},{0,20,0,4}}})
        CHECK(!TryChooseFusionHamburger269Shot(unsigned(args[0]),args[1],args[2],args[3],unchanged)&&unchanged.kind==777&&unchanged.damage==888&&unchanged.fire==9);
    CHECK(!TryChooseFusionHamburger269Shot(1,(std::numeric_limits<int>::max)(),4,3,unchanged)&&unchanged.damage==888);

    // Canonical way0 straight lobber leaves stay distinct from a real lobbed body.
    for(uint16_t kind:{uint16_t(55),uint16_t(59),uint16_t(60),uint16_t(86)})
    {
        CHECK(FusionMore269PlanarLobber(kind));
        const auto type=FusionMore269ProjectileType(kind,0);
        CHECK(!FusionMore269ProjectileMetadataValid(kind,20,0,13,type,MOTION_STRAIGHT,0,0));
        CHECK(!FusionMore269ProjectileMetadataValid(kind,20,0,0,type,MOTION_LOBBED,0,0));
        CHECK(!FusionMore269ProjectileMetadataValid(kind,20,1,0,type,MOTION_STRAIGHT,17,12345));
    }
    CHECK(!FusionMore269PlanarLobber(39));CHECK(FusionMore269ProjectileMotion(39)==MOTION_STAR);
    CHECK(!FusionMore269Payload(14));CHECK(!FusionMore269Payload(65535));
    CHECK(!FusionMore269ProjectileMetadataValid(0,40,1,0,PROJECTILE_FIREBALL,MOTION_STRAIGHT,0,0));
    CHECK(!FusionMore269ProjectileMetadataValid(0,40,1,0,PROJECTILE_FIREBALL,MOTION_STRAIGHT,17,0));
    CHECK(!FusionMore269ProjectileMetadataValid(0,40,1,0,PROJECTILE_FIREBALL,MOTION_STRAIGHT,0,12345));
    CHECK(!FusionMore269ProjectileMetadataValid(0,40,1,0,PROJECTILE_PEA,MOTION_STRAIGHT,17,12345));
    CHECK(!FusionMore269ProjectileMetadataValid(0,20,0,0,PROJECTILE_PEA,MOTION_STRAIGHT,17,12345));
    CHECK(!FusionMore269ProjectileMetadataValid(0,-1,1,0,PROJECTILE_FIREBALL,MOTION_STRAIGHT,17,12345));
    CHECK(!FusionMore269ProjectileMetadataValid(0,(std::numeric_limits<int>::min)(),3,0,PROJECTILE_FIREBALL,MOTION_STRAIGHT,17,12345));
    CHECK(!FusionMore269ProjectileMetadataValid(0,41,1,0,PROJECTILE_FIREBALL,MOTION_STRAIGHT,17,12345));
    CHECK(!FusionMore269ProjectileMetadataValid(0,40,4,0,PROJECTILE_FIREBALL,MOTION_STRAIGHT,17,12345));
    // Real SquashBullet contact transitions initial way0 into way1 lobbed.
    for(int damage:{20,30,40})
        CHECK(FusionMore269ProjectileMetadataValid(28,damage,0,1,PROJECTILE_PEA,MOTION_LOBBED,0,0));
    CHECK(!FusionMore269ProjectileMetadataValid(28,20,0,1,PROJECTILE_PEA,MOTION_STRAIGHT,0,0));
    CHECK(!FusionMore269ProjectileMetadataValid(28,50,0,1,PROJECTILE_PEA,MOTION_LOBBED,0,0));
    CHECK(!FusionMore269ProjectileMetadataValid(28,20,1,1,PROJECTILE_PEA,MOTION_LOBBED,17,12345));
    CHECK(!FusionMore269ProjectileMetadataValid(28,20,0,1,PROJECTILE_PEA,MOTION_LOBBED,17,12345));

    CHECK(FusionMorePlants269StateValid(243,0,3,0,123,2700,50));
    CHECK(!FusionMorePlants269StateValid(243,0,4,0,123,2700,50));
    CHECK(!FusionMorePlants269StateValid(243,0,3,0,123,2701,50));
    CHECK(!FusionMorePlants269StateValid(243,0,3,0,123,2700,51));
    CHECK(!FusionMorePlants269StateValid(243,1,3,0,123,0,0));
    CHECK(!FusionMorePlants269StateValid(243,0,0,0,0,-1,0));
    CHECK(!FusionMorePlants269StateValid(243,0,0,1,0,0,0));
    CHECK(FusionMorePlants269StateValid(248,1,0,0,(std::numeric_limits<uint32_t>::max)(),1000,0));
    CHECK(!FusionMorePlants269StateValid(248,2,0,0,0,0,0));
    CHECK(!FusionMorePlants269StateValid(248,0,0,0,0,1001,0));
    CHECK(FusionMorePlants269StateValid(256,0,0,0,0,0,0));
    CHECK(!FusionMorePlants269StateValid(256,0,0,0,1,0,0));
    CHECK(!FusionMorePlants269StateValid(900,0,0,0,0,0,0));

    // Every source integer0..32 remains a candidate; support cannot reweight it.
    const auto candidates=FusionPresent269AllCandidates();CHECK(candidates.size()==33);
    for(unsigned i=0;i<candidates.size();++i)CHECK(candidates[i]==i);
    auto drops=PlanFusionSqualour269Drops(false,0);CHECK(drops.self&&!drops.upgraded&&!drops.squash);
    drops=PlanFusionSqualour269Drops(true,1);CHECK(!drops.self&&!drops.upgraded&&drops.squash==1);
    drops=PlanFusionSqualour269Drops(true,2);CHECK(!drops.self&&!drops.upgraded&&drops.squash==2);
    drops=PlanFusionSqualour269Drops(true,3);CHECK(!drops.self&&drops.upgraded&&drops.squash==3);
    drops=PlanFusionSqualour269Drops(true,(std::numeric_limits<unsigned>::max)());CHECK(!drops.self&&drops.upgraded&&drops.squash==3);
    CHECK(FusionMorePlants269SquashTarget(7,false,false));
    CHECK(FusionMorePlants269SquashTarget(0,true,true));
    CHECK(!FusionMorePlants269SquashTarget(0,false,true));
    CHECK(!FusionMorePlants269SquashTarget(0,true,false));
#undef CHECK
    return failures;
}
