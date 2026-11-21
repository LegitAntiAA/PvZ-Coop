#include "FusionStar269Tests.h"
#include "../../Lawn/Fusion/FusionNativeStar269.h"
#include "../../Lawn/Fusion/FusionStar269Rules.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
FusionStar269TestResult RunFusionStar269Tests()
{
    using namespace LawnFusion;FusionStar269TestResult r;
    const auto check=[&](bool v,int line){++r.mChecks;if(!v){++r.mFailures;std::printf("Star269 failed line=%d\n",line);}};
#define CHECK(x) check(bool(x),__LINE__)
    for(uint16_t kind:{1082,1084,1089,1093,1094,1095,1104,920})CHECK(FindFusionStar269Profile(kind));
    CHECK(!FindFusionStar269Profile(23)&&!FindFusionStar269Profile(65535));
    CHECK(!FusionStar269PlantTakesDamage(1095)&&FusionStar269PlantTakesDamage(1093));
    for(uint16_t kind:{uint16_t(1084),uint16_t(920)})
    {
        const auto* p=FindFusionStar269Profile(kind);std::array<uint32_t,240> refs{};std::size_t index=999;
        for(int i=0;i<p->capacity;++i)refs[i]=uint32_t(i+1);
        CHECK(!TryFindFusionStar269FreeSlot(kind,false,refs.data(),p->storageSlots,index)&&index==999);
        CHECK(TryFindFusionStar269FreeSlot(kind,true,refs.data(),p->storageSlots,index)&&index==p->capacity);
        refs[3]=0;CHECK(TryFindFusionStar269FreeSlot(kind,false,refs.data(),p->storageSlots,index)&&index==3);
        CHECK(!TryFindFusionStar269FreeSlot(kind,true,refs.data(),p->storageSlots-1,index));
        float after=77;bool release=true;
        CHECK(TryAdvanceFusionStar269Hold(kind,std::nextafter(p->storageSeconds,0.f),.01f,after,release)&&!release&&after>p->storageSeconds);
        CHECK(TryAdvanceFusionStar269Hold(kind,p->storageSeconds,.01f,after,release)&&release&&after==p->storageSeconds);
        after=77;CHECK(!TryAdvanceFusionStar269Hold(kind,-1,.01f,after,release)&&after==77);
        FusionStar269Dispatch d;CHECK(TryMakeFusionStar269Dispatch(kind,false,20,6,d)&&d.damage==20*p->dispatchDamageMultiplier&&d.trackSpeed==12&&d.way==6&&d.colliderEnabled);
        CHECK(TryMakeFusionStar269Dispatch(kind,true,20,6,d)&&d.damage==20&&d.trackSpeed==6);
        const auto before=d;CHECK(!TryMakeFusionStar269Dispatch(kind,false,(std::numeric_limits<int>::max)(),6,d)&&d.damage==before.damage);
    }
    for(int bits=0;bits<32;++bits)CHECK(FusionStar269CanCapture(bits&1,bits&2,bits&4,bits&8?10:2,bits&16)==((bits&1)&&(bits&2)&&!(bits&4)&&!(bits&8)&&(bits&16)));
    FusionStar269Point after;bool moving=true;float angle=77;
    CHECK(TryAdvanceFusionStar269Approach({0,0,0},{2,0,0},.1f,after,moving,angle)&&std::abs(after.x-1.4f)<1e-6&&moving&&angle==77);
    CHECK(TryAdvanceFusionStar269Approach({0,0,0},{.8f,0,0},.01f,after,moving,angle)&&!moving&&angle==0);
    float speed=-5.999f;angle=0;after.z=2;
    CHECK(TryAdvanceFusionStar269Orbit({0,0,0},.01f,speed,angle,after)&&speed==-6&&angle<0&&after.z==2);
    const FusionStar269Target targets[]={{true,false,1,0},{true,true,5,0},{true,false,-1,0}};std::size_t selected=99;
    CHECK(TrySelectFusionStar269Target(targets,3,0,0,false,selected)&&selected==0);
    CHECK(TrySelectFusionStar269Target(targets,3,0,0,true,selected)&&selected==1);
    CHECK(!TrySelectFusionStar269Target(nullptr,1,0,0,true,selected)&&selected==1);
    float vx=77,vy=88;bool collider=true,reacquire=false;
    CHECK(TryMakeFusionStar269HomingVelocity(false,false,0,0,0,0,8,vx,vy,collider,reacquire)&&vx==-2&&vy==0&&!collider&&reacquire);
    vx=77;vy=88;collider=true;CHECK(TryMakeFusionStar269HomingVelocity(true,false,0,0,0,0,8,vx,vy,collider,reacquire)&&vx==77&&vy==88&&collider&&reacquire);
    CHECK(TryMakeFusionStar269HomingVelocity(true,true,0,0,3,4,10,vx,vy,collider,reacquire)&&vx==6&&vy==8&&collider&&!reacquire);
    CHECK(FusionStar269CactusCanAcquire(10,false,true,0,1,2));
    CHECK(!FusionStar269CactusCanAcquire(10,true,true,0,1,2)&&!FusionStar269CactusCanAcquire(0,false,true,0,1,2));
    CHECK(!FusionStar269CactusCanAcquire(10,false,true,0,0,2)&&!FusionStar269CactusCanAcquire(10,false,true,0,2,2));
    CHECK(FusionStar269StartupBlowTarget(10)&&!FusionStar269StartupBlowTarget(0));
    CHECK(FusionStar269MeteoriteDue(4,0)&&FusionStar269MeteoriteDue(4,-.01f)&&!FusionStar269MeteoriteDue(4,.01f)&&!FusionStar269MeteoriteDue(3,0));
    FusionStar269Shot shot;
    for(int light=0;light<=4;++light){CHECK(TryMakeFusionStar269Shot(1104,light,3,20,shot)&&shot.projectile==(light?48:39)&&shot.special==(light>=3?4:0)&&shot.damage==23&&shot.requestMeteorite==(light>=4));}
    CHECK(TryMakeFusionStar269Shot(1104,1,1000,20,shot)&&shot.damage==120);
    CHECK(!TryMakeFusionStar269Shot(1104,1,(std::numeric_limits<int>::max)(),(std::numeric_limits<int>::max)(),shot)&&shot.damage==120);
    for(const auto row:{std::array<int,2>{1093,45},{1094,43},{1095,44},{1082,37}})CHECK(TryMakeFusionStar269Shot(uint16_t(row[0]),0,0,20,shot)&&shot.projectile==row[1]);
    FusionStar269Hit hit;
    CHECK(TryMakeFusionStar269Hit(39,9,20,0,0,hit)&&hit.sourceDamageType==4);
    CHECK(TryMakeFusionStar269Hit(48,9,20,0,0,hit)&&hit.sourceDamageType==4);
    CHECK(TryMakeFusionStar269Hit(44,9,20,0,0,hit)&&hit.sourceDamageType==0&&hit.damage==20);
    CHECK(TryMakeFusionStar269Hit(43,2,20,0,0,hit)&&hit.area&&hit.radius==1.5f&&hit.sourceDamageType==10);
    CHECK(FindFusionStar269TimingProfile(1094)->attackIntervalSeconds==3&&FindFusionStar269TimingProfile(1082)->eventClipSeconds.size()==2);
    CHECK(!FindFusionStar269TimingProfile(1089)&&!FindFusionStar269TimingProfile(0));
    for(int kind=39;kind<=49;++kind)CHECK(FusionStar269ProjectileClass(uint16_t(kind))==(kind!=47));
    CHECK(!FusionStar269ProjectileClass(38)&&!FusionStar269ProjectileClass(50));
    CHECK(FusionStar269CanHitStatus(8,10)&&!FusionStar269CanHitStatus(8,0)&&FusionStar269CanHitStatus(6,10)&&!FusionStar269CanHitStatus(6,7));
    CHECK(FusionStar269CanHitStatus(1,7)&&!FusionStar269CanHitStatus(1,10)&&!FusionStar269CanHitStatus(2,10));
    for(int status:{3,9,12,13,16,20,21})for(uint8_t way:{0,2,6,8,10,14})CHECK(!FusionStar269CanHitStatus(way,status));
    NativeStar269ProjectileState ps;CHECK(!NativeStar269ProjectileStateValid(39,2,ps));
    ps.instanceToken=7;CHECK(NativeStar269ProjectileStateValid(39,2,ps));
    CHECK(!NativeStar269ProjectileStateValid(47,2,ps)&&!NativeStar269ProjectileStateValid(39,10,ps));
    ps.ownerId=10;ps.ownerToken=20;ps.accelerated=true;ps.colliderEnabled=false;ps.ownerSlot=239;
    CHECK(NativeStar269ProjectileStateValid(39,10,ps));ps.ownerSlot=240;CHECK(!NativeStar269ProjectileStateValid(39,10,ps));ps.ownerSlot=0;
    ps.colliderEnabled=true;CHECK(!NativeStar269ProjectileStateValid(39,10,ps));ps.colliderEnabled=false;
    ps.elapsed=6.01f;CHECK(NativeStar269ProjectileStateValid(39,10,ps));ps.elapsed=6.03f;CHECK(!NativeStar269ProjectileStateValid(39,10,ps));ps.elapsed=0;
    ps.angularSpeed=-6;CHECK(NativeStar269ProjectileStateValid(39,10,ps));ps.angularSpeed=-6.01f;CHECK(!NativeStar269ProjectileStateValid(39,10,ps));ps.angularSpeed=-3;
    ps.targetZombieId=1;CHECK(!NativeStar269ProjectileStateValid(39,10,ps));ps.targetZombieToken=2;CHECK(NativeStar269ProjectileStateValid(39,10,ps));
    NativeStar269PlantState plantState;CHECK(NativeStar269PlantStateValid(1084,plantState)&&!NativeStar269PlantStateValid(1093,plantState));
    plantState.slots[0]={1,2};CHECK(!NativeStar269PlantStateValid(1084,plantState));plantState.attracting=true;CHECK(NativeStar269PlantStateValid(1084,plantState));
    plantState.slots[1]={3,2};CHECK(!NativeStar269PlantStateValid(1084,plantState));plantState.slots[1]={};plantState.slots[80]={3,4};
    CHECK(!NativeStar269PlantStateValid(1084,plantState)&&NativeStar269PlantStateValid(920,plantState));
    CHECK(FusionStar269CaptureTouches({0,0,0},{3.1f,0,0},0)&&!FusionStar269CaptureTouches({0,0,0},{3.4f,0,0},0));
    CHECK(!FusionStar269CaptureTouches({0,0,0},{0,0,0},(std::numeric_limits<float>::quiet_NaN)()));
    for(int first=0;first<=8;++first)for(int second=0;second<=3;++second)
        CHECK(TryMakeFusionStar269Hit(45,9,20,first,second,hit)&&hit.damage==((first==2||first==4||first==7||first==8||second==1||second==3)?40:20)&&hit.sourceDamageType==0);
    int level=77;bool destroy=false;
    for(int kind:{26,205,206}){CHECK(TryAdvanceFusionUltimateStar269Status(kind,8,level,destroy)&&level==9&&!destroy);CHECK(TryAdvanceFusionUltimateStar269Status(kind,9,level,destroy)&&level==10&&destroy);}
    CHECK(TryAdvanceFusionUltimateStar269Status(25,9,level,destroy)&&level==9&&!destroy);
    CHECK(!FusionStar269HomingOutsideX(-10,-10,10)&&!FusionStar269HomingOutsideX(10,-10,10)&&FusionStar269HomingOutsideX(10.01f,-10,10));
#undef CHECK
    std::printf("FusionStar269 checks=%d failures=%d\n",r.mChecks,r.mFailures);return r;
}
