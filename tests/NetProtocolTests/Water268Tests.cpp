#include "Water268Tests.h"
#include "../../Lawn/Fusion/FusionWater268Rules.h"
#include "../../Lawn/Fusion/FusionBasePlantRules.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>
Water268TestResult RunWater268Tests()
{
    using namespace LawnFusion;Water268TestResult r;
#define CHECK(...) do{++r.mChecks;if(!(__VA_ARGS__)){++r.mFailures;std::printf("Water268:%d %s\n",__LINE__,#__VA_ARGS__);}}while(0)
    for(int s=0;s<=24;++s)
    {
        const bool retained=s!=1&&s!=7&&s!=12&&s!=16&&!(s>=21&&s<=24);
        const bool selected=retained&&s!=3&&s!=9&&s!=10&&s!=20;
        CHECK(FusionWater268HomingTarget(s,false,true,true,true)==retained);
        CHECK(FusionWater268HomingTarget(s,false,true,true,false)==selected);
        CHECK(!FusionWater268HomingTarget(s,true,true,true,true));
        CHECK(!FusionWater268HomingTarget(s,false,false,true,true));
        CHECK(!FusionWater268HomingTarget(s,false,true,false,true));
        const bool kelp=s!=1&&s!=3&&s!=9&&s!=10&&s!=12&&s!=16&&s<20;
        CHECK(FusionWater268GrabTarget(s,0,false,false)==kelp);
        CHECK(!FusionWater268GrabTarget(s,1,false,false));
        CHECK(!FusionWater268GrabTarget(s,0,true,false));
        CHECK(!FusionWater268GrabTarget(s,0,false,true));
    }
    CHECK(!FusionWater268HomingTarget(-1,false,true,true,true));
    CHECK(!FusionWater268HomingTarget(25,false,true,true,true));
    for(double py:{kFusionBasePixelsY,85.0/1.45})for(double dx:{-100.,0.,100.})for(double dy:{-100.,0.,100.})
    {
        float vx=99,vy=99;CHECK(TryGetFusionWater268HomingStep(dx,dy,py,5,vx,vy));
        const double length=std::sqrt(std::pow(vx/kFusionBasePixelsX,2)+std::pow(vy/py,2));
        CHECK(std::abs(length-(dx||dy?.05:0))<1e-7);
        CHECK((dx==0&&vx==0)||(dx*vx>0));CHECK((dy==0&&vy==0)||(dy*vy>0));
    }
    float vx=7,vy=8;CHECK(!TryGetFusionWater268HomingStep(1,1,0,5,vx,vy)&&vx==7&&vy==8);
    CHECK(!TryGetFusionWater268HomingStep(std::numeric_limits<double>::quiet_NaN(),1,60,5,vx,vy)&&vx==7&&vy==8);
    CHECK(!TryGetFusionWater268HomingStep(1,1,60,-1,vx,vy)&&vx==7&&vy==8);
    for(float speed:{.9f,1.f,1.1f}){int ticks=0;CHECK(TryGetFusionWater268GrabTicks(speed,ticks));CHECK(ticks==int(std::ceil(double(.33333334f)/speed*100)));}
    int unchanged=123;CHECK(!TryGetFusionWater268GrabTicks(0,unchanged)&&unchanged==123);
    for(uint16_t k:{252,1067,1068,1069}){const auto* t=FindFusionWater268Timing(k);CHECK(t&&FusionShooterTimingProfileValid(*t)&&t->eventClipSeconds.size()==2);}
    CHECK(!FindFusionWater268Timing(1175));CHECK(!FindFusionWater268Timing(1056));
    // Land CactusStar shares the projectile controller, not the water placement rule.
    for(uint16_t k:{15,1049,1050,1051,1066,1156,252,1067,1068,1069,1076,1083})CHECK(IsFusionWater268WaterPlant(k));
    CHECK(IsFusionWater268Kind(1085)&&!IsFusionWater268WaterPlant(1085));
    const auto* cactusStar=FindFusionWater268Timing(1085);
    CHECK(cactusStar&&cactusStar->projectileKind==42&&cactusStar->eventClipSeconds.size()==1&&FusionShooterTimingProfileValid(*cactusStar));
    CHECK(cactusStar&&cactusStar->stateSpeed==double(1.1f)&&cactusStar->eventClipSeconds[0]==double(.41666666f));
    CHECK(FusionWater268PlantStateValid(1085,5,0,0,0));
    CHECK(!FusionWater268PlantStateValid(1085,5,0,1,0));
    CHECK(!FusionWater268PlantStateValid(1085,5,0,0,1));
    for(int status=-1;status<=25;++status)
    {
        CHECK(FusionWater268StarTarget(status,false,true,true)==(status==10));
        CHECK(!FusionWater268StarTarget(status,true,true,true));
        CHECK(!FusionWater268StarTarget(status,false,false,true));
        CHECK(!FusionWater268StarTarget(status,false,true,false));
    }
    for(uint8_t way:{2,9})
    {
        for(int age=0;age<=30;++age)CHECK(!FusionWater268StarTrackingDue(way,age));
        CHECK(FusionWater268StarTrackingDue(way,31));
    }
    CHECK(!FusionWater268StarTrackingDue(6,31));CHECK(!FusionWater268StarTrackingDue(10,31));
    for(uint8_t way=0;way<19;++way)CHECK(FusionWater268ProjectileMetadataValid(42,20,way)==(way==2||way==9||way==6));
    CHECK(!FusionWater268ProjectileMetadataValid(42,40,6));

    for(uint16_t active:{1050,1051,1066,1076,1083})CHECK(FusionWater268PlantStateValid(active,0,0,0,0));
    for(uint16_t pending:{1056,1175})CHECK(!FusionWater268PlantStateValid(pending,0,0,0,0));
    for(uint16_t k:{20,37,34,35})CHECK(FusionWater268HomingMetadataValid(k,k==20||k==35?40:20,6));
    CHECK(!FusionWater268HomingMetadataValid(94,80,6));CHECK(!FusionWater268HomingMetadataValid(37,20,7));
    CHECK(!FusionWater268HomingMetadataValid(34,80,6));CHECK(!FusionWater268HomingMetadataValid(35,20,6));
    CHECK(kFusionWater268SinkTicks==50);
    for(uint16_t k:{15,1049,1156})
    {
        CHECK(FusionWater268CaptureStateValid(k,0,0,0,0,0,1));
        CHECK(FusionWater268CaptureStateValid(k,1,34,0,23,0x10017,1));
        CHECK(FusionWater268CaptureStateValid(k,2,0,50,23,0x10017,1));
        CHECK(FusionWater268CaptureStateValid(k,2,0,0,23,0x10017,1));
        CHECK(!FusionWater268CaptureStateValid(k,1,0,0,23,0x10017,1));
        CHECK(!FusionWater268CaptureStateValid(k,1,34,1,23,0x10017,1));
        CHECK(!FusionWater268CaptureStateValid(k,2,0,51,23,0x10017,1));
        CHECK(!FusionWater268CaptureStateValid(k,0,0,0,23,0x10017,1));
        CHECK(!FusionWater268CaptureStateValid(k,1,34,0,0,0x10017,1));
    }
    // Destroyed target leaves the source hand coroutine running; no dangling FK.
    for(uint16_t k:{15,1049,1156,1050,1051,1066})
    {
        CHECK(FusionWater268CaptureStateValid(k,1,34,0,0,0,1));
        CHECK(FusionWater268CaptureStateValid(k,2,0,50,0,0,1));
        CHECK(!FusionWater268CaptureStateValid(k,2,0,50,42,0,1));
        CHECK(!FusionWater268CaptureStateValid(k,2,0,50,0,0x1002a,1));
    }
    CHECK(!FusionWater268CaptureStateValid(1050,3,0,0,0,0,1));
    CHECK(FusionWater268CaptureStateValid(1050,3,0,0,42,0x1002a,1));
    CHECK(!FusionWater268CaptureStateValid(1051,3,0,0,42,0x1002a,1));
    CHECK(!FusionWater268CaptureStateValid(1050,3,1,0,42,0x1002a,1));
    CHECK(!FusionWater268PlantStateValid(1050,0,0,3,0));
    CHECK(!FusionWater268PlantStateValid(1076,1,0,2,0));
    const auto* grown=FindFusionWater268Timing(1076,true,false);
    CHECK(grown&&FusionShooterTimingProfileValid(*grown)&&grown->eventClipSeconds.size()==4&&!grown->speedParameterActive);
    CHECK(FindFusionWater268Timing(1076,false,true)->projectileKind==37);
    CHECK(FindFusionWater268Timing(1083,true)->eventClipSeconds.size()==2);
    for(float speed:{.9f,1.f,1.1f})
    {
        int ticks=0;CHECK(TryGetFusionWater268SearchTicks(speed,ticks));
        int starts=0,crashes=0;
        for(int remaining=ticks;remaining>=0;--remaining)
        {starts+=FusionWater268SearchEventDue(remaining,speed,false);crashes+=FusionWater268SearchEventDue(remaining,speed,true);}
        CHECK(starts==1&&crashes==1);
        CHECK(FusionWater268SearchEventDue(ticks-int(std::ceil(double(.56666666f)/speed*100)),speed,false));
    }
    for(int count:{0,5,6,29,30})
    {
        int ticks=0,next=0;bool percent=false,full=false;
        CHECK(TryAddFusionWater268Grap(170,count,ticks,next,percent,full));
        CHECK(ticks==370&&next==count+1&&percent==(count>5)&&full==(count>=30));
    }
    int ticks=7,next=8;bool percent=false,full=false;
    CHECK(!TryAddFusionWater268Grap((std::numeric_limits<int>::max)()-199,0,ticks,next,percent,full)&&ticks==7&&next==8);
    float angle=0;CHECK(TryGetFusionWater268SkyAngle(1,angle)&&std::abs(angle-.5235987901687622f)<1e-7f);
    CHECK(TryGetFusionWater268SkyAngle(85,angle)&&angle==0);
    CHECK(FusionWater268ProjectileMetadataValid(32,40,255));
    CHECK(!FusionWater268ProjectileMetadataValid(32,40,1));
    CHECK(FusionWater268ProjectileCanHit(7,10,0,false,true,true));
    CHECK(!FusionWater268ProjectileCanHit(7,0,0,false,true,true));
    CHECK(FusionWater268ProjectileCanHit(2,0,1,false,true,true));
    CHECK(!FusionWater268ProjectileCanHit(9,0,1,false,true,true));
    for(double py:{kFusionBasePixelsY,85./1.45})
    {
        FusionRecipientRect rect;
        CHECK(TryMakeFusionWater268ProjectileRect(42,100,100,0,py,rect));
        CHECK(std::abs(rect.width-.32703882*1.7*kFusionBasePixelsX)<1e-9);
        CHECK(std::abs(rect.height-.31575292*1.7*py)<1e-9);
        CHECK(FusionWater268ProjectileTouchesRect(42,100,100,0,py,rect));
        CHECK(TryMakeFusionWater268ProjectileRect(34,100,100,0,py,rect));
        CHECK(std::abs(rect.width-.4993149*1.7*kFusionBasePixelsX)<1e-9);
        CHECK(std::abs(rect.height-.15268683*1.7*py)<1e-9);
        CHECK(FusionWater268ProjectileTouchesRect(34,100,100,0,py,rect));
        CHECK(!FusionWater268ProjectileTouchesRect(34,100,100,0,py,{200,200,1,1}));
        CHECK(TryMakeFusionWater268ProjectileRect(37,100,100,.5235987755982989,py,rect));
        // A rotated thin box's AABB corner is outside its true source SAT box.
        CHECK(!FusionWater268ProjectileTouchesRect(37,100,100,.5235987755982989,py,{rect.x,rect.y,.01,.01}));
    }
#undef CHECK
    return r;
}
