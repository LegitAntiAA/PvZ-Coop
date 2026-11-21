#include "FusionSolar269Tests.h"
#include "../../Lawn/Fusion/FusionSolar269Rules.h"
#include "../../Lawn/Fusion/FusionLobberRules.h"
#include <cmath>
#include <cstdio>
#include <limits>
#include <initializer_list>
FusionSolar269TestResult RunFusionSolar269Tests()
{
    using namespace LawnFusion;FusionSolar269TestResult r;
#define CHECK(...) do{++r.mChecks;if(!(__VA_ARGS__)){++r.mFailures;std::printf("Solar269:%d %s\n",__LINE__,#__VA_ARGS__);}}while(0)
    CHECK(FusionSolar269Interval(false)==2&&FusionSolar269Interval(true)==1.5f);
    CHECK(FusionSolar269Projectile(false)==75&&FusionSolar269Projectile(true)==53);
    CHECK(FusionSolar269RelayKind(1128)&&FusionSolar269RelayKind(916)&&!FusionSolar269RelayKind(30));
    CHECK(FindFusionLobberProfile(934)&&FindFusionLobberProfile(934)->damage==300&&FusionLobberCornDenominator(934)==0);
    CHECK(!FindFusionLobberProfile(934,true));
    CHECK(FusionLobberProjectileMetadataValid(75,80,0,PROJECTILE_CABBAGE)&&FusionLobberProjectileMetadataValid(75,160,0,PROJECTILE_CABBAGE));
    CHECK(FusionLobberProjectileMetadataValid(75,300,0,PROJECTILE_CABBAGE)&&!FusionLobberProjectileMetadataValid(75,600,0,PROJECTILE_CABBAGE));
    CHECK(FusionLobberProjectileMetadataValid(53,50,5,PROJECTILE_CABBAGE)&&FusionLobberProjectileMetadataValid(53,300,5,PROJECTILE_CABBAGE));
    CHECK(!FusionLobberProjectileMetadataValid(53,300,0,PROJECTILE_CABBAGE)&&!FusionLobberProjectileMetadataValid(75,300,5,PROJECTILE_CABBAGE));
    const auto* shot=FindFusionSolar269Timing();const auto* super=FindFusionSolar269Timing("super");
    CHECK(shot&&shot->eventClipSeconds.size()==5&&shot->stateSpeed==double(1.3f)&&shot->speedParameterActive);
    CHECK(super&&super->eventClipSeconds.size()==2&&!super->speedParameterActive&&super->clipDurationSeconds==double(1.6666666f));
    CHECK(!FindFusionSolar269Timing("fake"));
    for(float speed:{.9f,1.f,1.1f})
    {
        int counter=0;CHECK(TryGetFusionShooterShotCounter(*shot,speed,counter));int emitted=0;
        for(int c=counter-1;c>=0;--c){int count=0;CHECK(TryGetFusionShooterEventsAtCounter(*shot,speed,c,count));emitted+=count;}
        // Counter ends after the last event, then the existing phase owns the
        // visual tail through the real1.6666666-second clip end.
        CHECK(emitted==5);CHECK(TryGetFusionShooterShotCounter(*super,speed,counter)&&counter==135&&FusionShooterDuration(*super,speed)>counter*.01f);
        int first=0,last=0;bool due=false;
        for(int c=counter-1;c>=0;--c)
        {if(TryGetFusionShooterEventAtCounter(*super,speed,c,0,due)&&due)first=counter-c;if(TryGetFusionShooterEventAtCounter(*super,speed,c,1,due)&&due)last=counter-c;}
        CHECK(first==34&&last==134); // Super ignores Speed parameter.
    }
    float after=77;CHECK(TryClampFusionSolar269Countdown(3,after)&&after==.5f);
    CHECK(TryClampFusionSolar269Countdown(.1f,after)&&after==.1f&&TryClampFusionSolar269Countdown(-.1f,after)&&after==-.1f);
    CHECK(!TryClampFusionSolar269Countdown(std::numeric_limits<float>::infinity(),after)&&after==-.1f);
    FusionSolar269SkillPlan skill;
    CHECK(!TryPlanFusionSolar269Skill(0,FusionSolar269WorldBranch::Unavailable,skill));
    CHECK(TryPlanFusionSolar269Skill(0,FusionSolar269WorldBranch::Countdown,skill)&&!skill.superAnimation&&skill.cooldownTicks==200);
    CHECK(TryPlanFusionSolar269Skill(0,FusionSolar269WorldBranch::ActiveSolar,skill)&&skill.superAnimation);
    CHECK(!TryPlanFusionSolar269Skill(1,FusionSolar269WorldBranch::ActiveSolar,skill)&&!TryPlanFusionSolar269Skill(-1,FusionSolar269WorldBranch::Countdown,skill));
    CHECK(FusionSolar269PlantStateValid(5,200,13,1)&&FusionSolar269PlantStateValid(0,0,0,0));
    CHECK(!FusionSolar269PlantStateValid(0,201,0,0)&&!FusionSolar269PlantStateValid(0,0,1,0)&&!FusionSolar269PlantStateValid(0,0,0,2));
    const FusionLobberPoint origin{0,0},sample{100,20},current{80,20},velocity{-40,0};
    FusionLobberLaunch legacy,explicitLegacy,oneSecond;
    CHECK(TryMakeFusionLobberLaunch(origin,sample,current,.5,nullptr,legacy));
    CHECK(TryMakeFusionLobberVelocityLaunch(origin,current,velocity,1.5,nullptr,explicitLegacy));
    CHECK(legacy.velocityX==explicitLegacy.velocityX&&legacy.velocityY==explicitLegacy.velocityY&&legacy.accelerationY==explicitLegacy.accelerationY);
    for(double py:{kFusionLobberPixelsY,85./1.45})
    {
        CHECK(TryMakeFusionLobberVelocityLaunch(origin,current,velocity,1,nullptr,oneSecond,py));
        float x=0,z=0,vy=float(oneSecond.velocityY);bool valid=true;
        for(int i=0;i<100;++i)valid&=AdvanceFusionLobberMotion(float(oneSecond.velocityX),float(oneSecond.accelerationY),x,z,vy);
        CHECK(valid&&std::abs(x-40)<.001&&std::abs(z-20)<.02); // Actual1s moving-target intercept.
    }
    oneSecond.velocityX=77;CHECK(!TryMakeFusionLobberVelocityLaunch(origin,current,velocity,0,nullptr,oneSecond)&&oneSecond.velocityX==77);
    CHECK(!TryMakeFusionLobberVelocityLaunch(origin,current,velocity,-1,nullptr,oneSecond));
    CHECK(!TryMakeFusionLobberVelocityLaunch(origin,current,{std::numeric_limits<double>::quiet_NaN(),0},1,nullptr,oneSecond));
    FusionLobberRect nearby{1,3,10,20};CHECK(TryMakeFusionLobberVelocityLaunch(origin,current,velocity,1,&nearby,oneSecond)&&oneSecond.spawn.x==1&&oneSecond.spawn.y==3+.1*kFusionLobberPixelsY);
#undef CHECK
    return r;
}
