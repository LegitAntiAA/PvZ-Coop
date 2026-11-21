#include "FusionShooter267Tests.h"
#include "../../Lawn/Fusion/FusionShooter267Rules.h"
#include "../../Lawn/Fusion/FusionBasePlantRules.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>
FusionShooter267TestResult RunFusionShooter267Tests()
{
    using namespace LawnFusion;FusionShooter267TestResult r;
    const auto check=[&](bool okay,int line){++r.mChecks;if(!okay){++r.mFailures;std::printf("FusionShooter267Tests:%d failed\n",line);}};
#define CHECK(x) check(bool(x),__LINE__)
    for(uint16_t k:{1172,1158,1007,919,902,1103,927,904,1047,932})CHECK(IsFusionShooter267Kind(k));
    CHECK(!IsFusionShooter267Kind(909));CHECK(!IsFusionShooter267Kind(1174));
    float interval=99;CHECK(TryGetFusionPeaMineInterval(.999f,interval)&&interval==.5f);
    CHECK(TryGetFusionPeaMineInterval(1,interval)&&interval==.5f);
    CHECK(TryGetFusionPeaMineInterval(3.5f,interval)&&interval==1);
    CHECK(TryGetFusionPeaMineInterval(6,interval)&&interval==1.5f);
    CHECK(TryGetFusionPeaMineInterval(100,interval)&&interval==1.5f);
    CHECK(!TryGetFusionPeaMineInterval((std::numeric_limits<float>::infinity)(),interval)&&interval==1.5f);
    float rate=0,velocity=0;int damage=0;
    CHECK(TryGetFusionUltimateGatling(8000,8000,rate,velocity,damage)&&rate==1&&velocity==6&&damage==120);
    CHECK(TryGetFusionUltimateGatling(5333,8000,rate,velocity,damage)&&rate==1.5f&&velocity==9&&damage==120);
    CHECK(TryGetFusionUltimateGatling(2666,8000,rate,velocity,damage)&&rate==1.5f&&damage==120);
    CHECK(TryGetFusionUltimateGatling(2665,8000,rate,velocity,damage)&&rate==2&&velocity==12&&damage>=120);
    CHECK(TryGetFusionUltimateGatling(0,8000,rate,velocity,damage)&&damage==480);
    CHECK(!TryGetFusionUltimateGatling(-1,8000,rate,velocity,damage));
    for(int first:{2,4,7,8})CHECK(FusionSourceZombieWithIron(first,0,-1));
    CHECK(!FusionSourceZombieWithIron(3,0,0));CHECK(FusionSourceZombieWithIron(3,1,0));
    CHECK(FusionSourceZombieWithIron(0,0,119));CHECK(!FusionSourceZombieWithIron(0,0,120));
    CHECK(TryGetFusionMagnetCactusDamage(20,10,0,damage)&&damage==500);
    CHECK(TryGetFusionMagnetCactusDamage(20,2,0,damage)&&damage==40);
    CHECK(TryGetFusionMagnetCactusDamage(20,1,0,damage)&&damage==20);
    // The counter includes the established final sentinel tick. Source .3f
    // is slightly above .3, so its first 100 Hz event deadline is31, not30.
    struct ShotDeadline {uint16_t kind;int counter,eventTick;};
    for(const auto row:{ShotDeadline{1172,36,35},ShotDeadline{1158,36,35},
        ShotDeadline{1007,35,34},ShotDeadline{904,32,31},ShotDeadline{19,32,31}})
    {
        const auto* t=row.kind==19?FindFusionBaseTimingProfile(19):FindFusionShooter267Timing(row.kind);
        int ticks=0;CHECK(t&&TryGetFusionShooterShotCounter(*t,1,ticks)&&ticks==row.counter);
        if(!t)continue;
        int emissions=0;
        for(int elapsed=1;elapsed<=ticks;++elapsed)
        {
            int events=0;CHECK(TryGetFusionShooterEventsAtCounter(*t,1,ticks-elapsed,events));
            CHECK(events==(elapsed==row.eventTick?1:0));emissions+=events;
        }
        CHECK(emissions==1);
    }
    // Source904 transition exit .75 plus fixed .25s; shooting loops at the
    // exact source clip length. These are native phase deadlines, not events.
    struct BeamPhase {float speed;int start,loop,end;};
    for(const auto row:{BeamPhase{.9f,55,63,34},BeamPhase{1,52,57,33},
        BeamPhase{1.1f,49,52,32},BeamPhase{.985463858f,52,58,33}})
    {
        int ticks=0;
        CHECK(TryGetFusionShooter267PhaseTicks(904,1,row.speed,0,ticks)&&ticks==row.start);
        CHECK(TryGetFusionShooter267PhaseTicks(904,2,row.speed,0,ticks)&&ticks==row.loop);
        CHECK(TryGetFusionShooter267PhaseTicks(904,3,row.speed,0,ticks)&&ticks==row.end);
        const float duration=FusionShooterDuration(*FindFusionShooter267Timing(904),row.speed);
        // Activation frame keeps countdown==total and elapsed0. A live phase
        // has countdown>=1; transition occurs before the total-th sample.
        CHECK(float(row.loop-1)*.01f<duration&&float(row.loop)*.01f>=duration);
    }
    struct GatlingPhase {uint16_t tier;int shoot,loop;};
    for(const auto row:{GatlingPhase{0,125,17},GatlingPhase{1,84,12},GatlingPhase{2,63,9}})
    {
        int ticks=0;
        CHECK(TryGetFusionShooter267PhaseTicks(927,0,.9f,row.tier,ticks)&&ticks==267);
        CHECK(TryGetFusionShooter267PhaseTicks(927,1,1.1f,row.tier,ticks)&&ticks==0);
        CHECK(TryGetFusionShooter267PhaseTicks(927,2,1,row.tier,ticks)&&ticks==row.shoot);
        CHECK(TryGetFusionShooter267PhaseTicks(927,3,1,row.tier,ticks)&&ticks==row.loop);
    }
    int phaseTicks=77;
    CHECK(!TryGetFusionShooter267PhaseTicks(904,2,1,1,phaseTicks)&&phaseTicks==77);
    CHECK(!TryGetFusionShooter267PhaseTicks(927,4,1,0,phaseTicks)&&phaseTicks==77);
    CHECK(!TryGetFusionShooter267PhaseTicks(927,2,1,3,phaseTicks)&&phaseTicks==77);
    CHECK(!TryGetFusionShooter267PhaseTicks(904,2,0,0,phaseTicks)&&phaseTicks==77);
    CHECK(FusionBaseCanAcquire(19,0,true,false,false,4.5*kFusionBasePixelsX-.001));
    CHECK(!FusionBaseCanAcquire(19,0,true,false,false,4.5*kFusionBasePixelsX));
    CHECK(FusionShooter267StateValid(927,150,0,2,3));CHECK(!FusionShooter267StateValid(927,150,0,3,3));
    CHECK(FusionShooter267StateValid(904,0,0,0,3));CHECK(!FusionShooter267StateValid(904,0,0,1,3));
    CHECK(FusionShooter267StateValid(932,0,0,0,0));
    CHECK(!FusionShooter267StateValid(932,1,0,0,0)); // Actual62h invalid parent state after one passive conversion.
    CHECK(!FusionShooter267StateValid(932,0,0,0,1));
    CHECK(FusionShooter267StateValid(902,21,0,0,0)); // UltimateTorch has its own genuine source conversion counter.
    CHECK(FusionShooter267ProjectileMetadataValid(93,60,0,0,0,PROJECTILE_PEA,MOTION_STRAIGHT));
    CHECK(FusionShooter267ProjectileMetadataValid(93,0,0,0,0,PROJECTILE_PEA,MOTION_STRAIGHT));
    CHECK(!FusionShooter267ProjectileMetadataValid(93,481,0,0,0,PROJECTILE_PEA,MOTION_STRAIGHT));
    CHECK(FusionShooter267ProjectileMetadataValid(87,160,4,0,0,PROJECTILE_FIREBALL,MOTION_THREEPEATER));
    CHECK(FusionShooter267ProjectileMetadataValid(87,320,0,1,0,PROJECTILE_FIREBALL,MOTION_STRAIGHT));
    CHECK(!FusionShooter267ProjectileMetadataValid(87,320,0,0,0,PROJECTILE_FIREBALL,MOTION_STRAIGHT));
    CHECK(FusionShooter267ProjectileMetadataValid(36,20,255,0,0,PROJECTILE_PEA,MOTION_LOBBED));
    CHECK(!FusionShooter267ProjectileMetadataValid(36,20,1,0,0,PROJECTILE_PEA,MOTION_LOBBED));
    CHECK(FusionShooter267ProjectileMetadataValid(28,40,5,0,0,PROJECTILE_PEA,MOTION_THREEPEATER));
    CHECK(FusionShooter267ProjectileMetadataValid(99,300,0,0,0,PROJECTILE_PEA,MOTION_STRAIGHT));
    CHECK(FusionShooter267ProjectileMetadataValid(98,300,4,0,1,PROJECTILE_PEA,MOTION_THREEPEATER));
    CHECK(!FusionShooter267ProjectileMetadataValid(99,300,0,1,0,PROJECTILE_FIREBALL,MOTION_STRAIGHT));
    FusionRecipientRect rect;CHECK(TryMakeFusionShooter267ProjectileRect(93,0,0,rect));
    CHECK(std::abs(rect.width-1.12081*.34*kFusionBasePixelsX)<1e-9);
    CHECK(TryMakeFusionShooter267ProjectileRect(98,0,0,rect));
    CHECK(std::abs(rect.width-.33396614*1.174088*kFusionBasePixelsX)<1e-9);
    CHECK(TryMakeFusionShooter267ProjectileRect(7,0,0,rect));
    CHECK(std::abs(rect.width-1.37*.25500003*kFusionBasePixelsX)<1e-9);
    CHECK(!TryMakeFusionShooter267ProjectileRect(49,0,0,rect));
    // Source world boxes retain their X geometry while each scene projects Y
    // independently. These include the rotated PeaMine and existing heated11.
    constexpr double poolPixelsY=85./1.45;
    for(uint16_t kind:{7,11,47,87,93,98,99})
    {
        FusionRecipientRect day,pool;
        CHECK(TryMakeFusionShooter267ProjectileRect(kind,10,20,day));
        CHECK(TryMakeFusionShooter267ProjectileRect(kind,10,20,pool,poolPixelsY));
        CHECK(std::abs(day.x-pool.x)<1e-9&&std::abs(day.width-pool.width)<1e-9);
        CHECK(std::abs((pool.y-20)-(day.y-20)*poolPixelsY/kFusionBasePixelsY)<1e-9&&
            std::abs(pool.height-day.height*poolPixelsY/kFusionBasePixelsY)<1e-9);
    }
    CHECK(TryMakeFusionShooter267ProjectileRect(93,0,0,rect,poolPixelsY));
    CHECK(std::abs(rect.height-2.1532648*.34*poolPixelsY)<1e-9&&
        std::abs(rect.y-(-.5128448*-.34*poolPixelsY-rect.height*.5))<1e-9);
    constexpr double angle=.78539816339744830962;
    for(double pixelsY:{kFusionBasePixelsY,poolPixelsY})
    {
        CHECK(TryMakeFusionShooter267ProjectileRect(7,10,20,rect,pixelsY,angle));
        const double worldExtent=(1.37+1.4)*.25500003/std::sqrt(2.);
        CHECK(std::abs(rect.width-worldExtent*kFusionBasePixelsX)<1e-9&&
            std::abs(rect.height-worldExtent*pixelsY)<1e-9);
        const FusionRecipientRect center{10,20,.001,.001};
        const FusionRecipientRect corner{rect.x+rect.width-.001,rect.y+rect.height-.001,.0005,.0005};
        CHECK(FusionShooter267RotatedProjectileTouchesRect(7,10,20,angle,center,pixelsY));
        CHECK(!FusionShooter267RotatedProjectileTouchesRect(7,10,20,angle,corner,pixelsY));
    }
    const double nan=std::numeric_limits<double>::quiet_NaN(),inf=std::numeric_limits<double>::infinity();
    for(double invalid:{0.,-1.,nan,inf})
    {
        rect={11,22,33,44};
        CHECK(!TryMakeFusionShooter267ProjectileRect(93,0,0,rect,invalid)&&rect.x==11&&rect.y==22&&rect.width==33&&rect.height==44);
        CHECK(!FusionShooter267RotatedProjectileTouchesRect(7,0,0,angle,{0,0,1,1},invalid));
    }
    float vertical=99;
    CHECK(TryGetFusionShooter267ThreeVerticalStep(4,0,vertical)&&std::abs(vertical+4.191616535186768)<1e-6);
    CHECK(TryGetFusionShooter267ThreeVerticalStep(4,0,vertical,85)&&std::abs(vertical+3.5628740787506104)<1e-6);
    CHECK(TryGetFusionShooter267ThreeVerticalStep(5,25,vertical)&&std::abs(vertical-1.7964072227478027)<1e-6);
    CHECK(TryGetFusionShooter267ThreeVerticalStep(5,25,vertical,85)&&std::abs(vertical-1.5269461870193481)<1e-6);
    CHECK(TryGetFusionShooter267ThreeVerticalStep(4,50,vertical,85)&&vertical==0);
    vertical=99;
    CHECK(!TryGetFusionShooter267ThreeVerticalStep(3,0,vertical,85)&&vertical==99);
    CHECK(!TryGetFusionShooter267ThreeVerticalStep(4,-1,vertical,85)&&vertical==99);
    for(double invalid:{0.,-1.,nan,inf})CHECK(!TryGetFusionShooter267ThreeVerticalStep(4,0,vertical,invalid)&&vertical==99);
    uint8_t way=99;int row=99;
    CHECK(!TryGetFusionGoldTorchReturn(4,1,50,0,5,way,row)&&way==99&&row==99);
    CHECK(TryGetFusionGoldTorchReturn(4,1,51,0,5,way,row)&&way==5&&row==1);
    CHECK(!TryGetFusionGoldTorchReturn(5,0,51,1,5,way,row));
    CHECK(TryGetFusionGoldTorchReturn(5,1,51,5,6,way,row)&&way==4&&row==4);
#undef CHECK
    return r;
}
