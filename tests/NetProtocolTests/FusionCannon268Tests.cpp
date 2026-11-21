#include "FusionCannon268Tests.h"
#include "../../Lawn/Fusion/FusionCannon268Rules.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>
FusionCannon268TestResult RunFusionCannon268Tests()
{
    using namespace LawnFusion;FusionCannon268TestResult result{};
#define CHECK(...) do{++result.mChecks;if(!(__VA_ARGS__)){++result.mFailures;std::printf("Cannon268:%d %s\n",__LINE__,#__VA_ARGS__);}}while(0)
    for(auto kind:{1120,1122,1123,1132,1139,1142,914,915})CHECK(FindFusionCannon268Profile(kind));
    CHECK(!FindFusionCannon268Profile(0)&&!FindFusionCannon268Profile(1121));
    CHECK(FindFusionCannon268Profile(1122)->projectileKind==68&&FindFusionCannon268Profile(1123)->projectileKind==69);
    CHECK(!FindFusionCannon268Profile(1139)->manual&&!FindFusionCannon268Profile(1142)->manual&&!FindFusionCannon268Profile(914)->manual);
    for(int kind:{1120,1122,1123,1132,915})for(float speed:{.9f,1.f,1.1f})
    {
        const auto* shot=FindFusionCannon268Timing(kind);int initial=0;
        CHECK(shot&&FusionShooterTimingProfileValid(*shot)&&TryGetFusionCannon268ShotCounter(*shot,speed,initial));
        int emissions=0;for(int remaining=initial-1;remaining>=0;--remaining)
        {int count=0;for(size_t event=0;event<shot->eventClipSeconds.size();++event){bool due=false;CHECK(TryGetFusionCannon268EventAtCounter(*shot,speed,remaining,event,due));count+=due?1:0;}emissions+=count;}
        CHECK(emissions==(kind==1132?4:1));
        if(kind==1132)CHECK(!FindFusionCannon268Timing(kind,true));
        else CHECK(FindFusionCannon268Timing(kind,true)->stateSpeed==1.2);
    }
    CHECK(FusionCannon268StateValid(1120,0,501,0,0,0));
    CHECK(FusionCannon268StateValid(1120,1,0,0,0,0)&&FusionCannon268StateValid(1120,2,0,0,0,0));
    CHECK(FusionCannon268StateValid(1120,3,3601,0,0,0));
    CHECK(!FusionCannon268StateValid(1120,3,0,0,0,0)&&!FusionCannon268StateValid(1120,2,1,0,0,0));
    CHECK(!FusionCannon268StateValid(1120,0,3602,0,0,0)&&!FusionCannon268StateValid(1120,0,501,1,0,0));
    CHECK(!FusionCannon268StateValid(1120,0,501,0,1,0)&&!FusionCannon268StateValid(1120,0,501,0,0,1));
    for(float speed:{.9f,1.f,1.1f})
    {
        for(int kind:{1120,1122,1123,915})
        {
            const auto* charge=FindFusionCannon268Timing(kind,true);int full=0,generic=0;
            CHECK(TryGetFusionCannon268ShotCounter(*charge,speed,full)&&TryGetFusionShooterShotCounter(*charge,speed,generic));
            CHECK(full>generic);int events=0;
            for(int remaining=full-1;remaining>=0;--remaining){bool due=false;CHECK(TryGetFusionCannon268EventAtCounter(*charge,speed,remaining,0,due));events+=due?1:0;}
            CHECK(events==1);
        }
        const auto* sub=FindFusionCannon268SubTiming();int full=0;CHECK(TryGetFusionCannon268ShotCounter(*sub,speed,full)&&full==118);
        int events=0;for(int remaining=full-1;remaining>=0;--remaining)for(size_t i=0;i<2;++i)
        {bool due=false;CHECK(TryGetFusionCannon268EventAtCounter(*sub,speed,remaining,i,due));events+=due?1:0;}
        CHECK(events==2);
        for(auto kind:{1139,1142})
        {
            const auto* skill=FindFusionCannon268LobberTiming(kind,kind==1139?"shoot3":"shoot2");int initial=0;
            CHECK(skill&&TryGetFusionShooterShotCounter(*skill,speed,initial));events=0;
            for(int remaining=initial-1;remaining>=0;--remaining){int count=0;CHECK(TryGetFusionShooterEventsAtCounter(*skill,speed,remaining,count));events+=count;}
            CHECK(events==4);
        }
    }
    CHECK(FusionCannon268StateValid(915,2,0,117,0,0));
    CHECK(!FusionCannon268StateValid(915,2,0,118,0,0));
    CHECK(FusionCannon268StateValid(1139,1,0,0,201,0)&&FusionCannon268StateValid(1142,0,0,0,1,0));
    CHECK(!FusionCannon268StateValid(1139,1,1,0,201,0)&&!FusionCannon268StateValid(1142,1,0,0,202,0));
    CHECK(FusionCannon268StateValid(914,0,0,0,0,0)&&!FusionCannon268StateValid(914,1,0,0,0,0));
    CHECK(FusionCannon268SplitCount(73,1)==10&&FusionCannon268SplitCount(83,2)==15);
    CHECK(!FusionCannon268SplitCount(65,1)&&!FusionCannon268SplitCount(73,3));
    int doom=7;CHECK(TryGetFusionCannon268DoomDamage(1800,100,100,doom)&&doom==1860);
    CHECK(TryGetFusionCannon268DoomDamage(1800,99,0,doom)&&doom==1829);
    doom=7;CHECK(!TryGetFusionCannon268DoomDamage(1800,-1,0,doom)&&doom==7);
    CHECK(!TryGetFusionCannon268DoomDamage(1800,std::numeric_limits<float>::infinity(),0,doom)&&doom==7);
    for(uint16_t kind:{65,68,69,73,83,84})
    {
        int damage=kind==68?3600:kind==73?200:kind==83?120:1800;
        CHECK(FusionCannon268ProjectileMetadataValid(kind,damage,0,14,0,false,PROJECTILE_COBBIG,MOTION_LOBBED));
        CHECK(!FusionCannon268ProjectileMetadataValid(kind,damage,0,13,0,false,PROJECTILE_COBBIG,MOTION_LOBBED));
        CHECK(!FusionCannon268ProjectileMetadataValid(kind,damage,0,14,0,true,PROJECTILE_COBBIG,MOTION_LOBBED));
    }
    CHECK(FusionCannon268ProjectileMetadataValid(73,200,0,14,1,false,PROJECTILE_COBBIG,MOTION_LOBBED));
    CHECK(FusionCannon268ProjectileMetadataValid(83,120,0,14,2,false,PROJECTILE_COBBIG,MOTION_LOBBED));
    CHECK(!FusionCannon268ProjectileMetadataValid(83,120,0,14,1,false,PROJECTILE_COBBIG,MOTION_LOBBED));
    CHECK(FusionCannon268ProjectileMetadataValid(72,120,0,0,1,true,PROJECTILE_MELON,MOTION_LOBBED));
    CHECK(FusionCannon268ProjectileMetadataValid(80,450,0,0,0,true,PROJECTILE_MELON,MOTION_LOBBED));
    CHECK(FusionCannon268ProjectileMetadataValid(85,360,0,0,0,false,PROJECTILE_KERNEL,MOTION_STRAIGHT));
    CHECK(!FusionCannon268ProjectileMetadataValid(85,360,0,14,0,false,PROJECTILE_KERNEL,MOTION_STRAIGHT));
    for(int rows:{5,6})for(uint16_t kind:{65,68,69,73,83,84})
    {
        const double py=rows==6?85/1.45:100/1.67;
        float x=120,y=50,v=0,rotation=0;bool hit=true;
        CHECK(TryStepFusionCannon268(kind,99,rows,py,500,300,x,y,v,rotation,hit));
        CHECK(x==120&&rotation==kFusionCannon268UpRotation&&v<0&&!hit);
        x=120;y=50;
        CHECK(TryStepFusionCannon268(kind,100,rows,py,500,300,x,y,v,rotation,hit));
        CHECK(x==500&&rotation==kFusionCannon268DownRotation&&v>0&&!hit);
        CHECK(std::fabs(v-float(rows*3*.01*py))<1e-5f);
        CHECK(FusionCannon268RotationValid(rotation)&&!FusionCannon268RotationValid(0));
    }
    float x=1,y=100,v=3,rot=4;bool hit=false;
    CHECK(TryStepFusionCannon268(73,100,5,100/1.67,200,100,x,y,v,rot,hit)&&!hit);
    y=100.001f;CHECK(TryStepFusionCannon268(73,100,5,100/1.67,200,100,x,y,v,rot,hit)&&hit&&y==100.001f);
    x=1;y=2;v=3;rot=4;hit=false;
    CHECK(!TryStepFusionCannon268(80,100,5,100/1.67,200,100,x,y,v,rot,hit)&&x==1&&y==2&&v==3&&rot==4&&!hit);
    CHECK(!TryStepFusionCannon268(65,-1,5,100/1.67,200,100,x,y,v,rot,hit));
    CHECK(!TryStepFusionCannon268(65,100,4,100/1.67,200,100,x,y,v,rot,hit));
    CHECK(!TryStepFusionCannon268(65,100,5,std::numeric_limits<double>::quiet_NaN(),200,100,x,y,v,rot,hit));
    CHECK(FusionCannon268AreaTouches(0,0,100,0,1,1,100/1.67));
    CHECK(!FusionCannon268AreaTouches(0,0,110,0,1,1,100/1.67));
#undef CHECK
    return result;
}
