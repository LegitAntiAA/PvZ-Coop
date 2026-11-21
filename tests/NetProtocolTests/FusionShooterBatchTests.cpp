#include "FusionShooterBatchTests.h"
#include "../../Lawn/Fusion/FusionShooterBatchRules.h"
#include "../../Lawn/Fusion/FusionSplitShooterRules.h"
#include <cmath>
#include <cstdio>
#include <limits>
#include <initializer_list>

FusionShooterBatchTestResult RunFusionShooterBatchTests()
{
    using namespace LawnFusion;
    FusionShooterBatchTestResult result{};
    auto check=[&](bool okay,int line){++result.mChecks;if(!okay){++result.mFailures;std::printf("FusionShooterBatchTests:%d failed\n",line);}};
#define CHECK(x) check(bool(x),__LINE__)
    constexpr uint16_t kinds[]={1020,1152,1096,929,1107,1109,1161,1165,1167,1168,1183,1191,1192};
    for(uint16_t kind:kinds)CHECK(IsFusionShooterBatchKind(kind));
    for(uint16_t old:{0,6,7,1001,1085,1104,1170,1174,65000})CHECK(!IsFusionShooterBatchKind(old));
    CHECK(!FindFusionShooterBatchTimingProfile(1192)); // Uses the existing shared Chomper controller.
    CHECK(!FindFusionShooterBatchTimingProfile(1020,"shoot2"));
    CHECK(FindFusionShooterBatchTimingProfile(1191,"shoot2"));
    CHECK(FindFusionShooterBatchTimingProfile(1161,"shoot_loop"));
    CHECK(!FindFusionShooterBatchTimingProfile(1161,"bogus"));
    CHECK(!FindFusionShooterBatchTimingProfile(1020,nullptr));

    // Independent source callback/frame goldens at Speed1. Coincident emissions
    // are dispatched by one callback; timing never invents another event.
    struct Timing { uint16_t kind; int firstCounter; int callbacks; };
    constexpr Timing timing[]={{1020,35,1},{1152,40,1},{1096,39,1},{929,86,4},
        {1107,51,2},{1109,39,1},{1167,39,1},{1161,118,6},{1165,118,6},
        {1168,118,6},{1183,26,1},{1191,33,1}};
    for(const auto& t:timing)
    {
        const auto* p=FindFusionShooterBatchTimingProfile(t.kind);CHECK(p&&FusionShooterTimingProfileValid(*p));
        int counter=-3;CHECK(p&&TryGetFusionShooterShotCounter(*p,1,counter)&&counter==t.firstCounter);
        int fired=0;for(int tick=counter-1;tick>=0;--tick)
        {int count=-1;CHECK(TryGetFusionShooterEventsAtCounter(*p,1,tick,count));fired+=count;}
        CHECK(fired==t.callbacks);
    }
    const auto* split=FindFusionShooterBatchTimingProfile(1107);
    bool due=false;CHECK(TryGetFusionShooterEventAtCounter(*split,1,17,0,due)&&due);
    CHECK(TryGetFusionShooterEventAtCounter(*split,1,1,1,due)&&due);
    const auto emissions=GetFusionSplitEmissionSpecs(1107);
    CHECK(emissions.size()==3);
    CHECK(emissions[0].eventIndex==0&&emissions[0].projectileKind==15&&emissions[0].specialType==1);
    CHECK(emissions[1].eventIndex==0&&emissions[1].specialType==0);
    CHECK(emissions[2].eventIndex==1&&emissions[2].specialType==1);
    for(const auto& e:emissions)CHECK(FusionSplitEmissionValid(1107,e));

    int ticks=777;CHECK(TryGetFusionShooterBatchCycleTicks(1109,0,ticks)&&ticks==301);
    CHECK(TryGetFusionShooterBatchCycleTicks(1191,0,ticks)&&ticks==151);
    CHECK(TryGetFusionShooterBatchCycleTicks(1020,1,ticks)&&ticks==151);
    ticks=777;CHECK(!TryGetFusionShooterBatchCycleTicks(1109,.11f,ticks)&&ticks==777);
    CHECK(!TryGetFusionShooterBatchCycleTicks(1020,0,ticks)&&ticks==777);
    CHECK(!TryGetFusionShooterBatchCycleTicks(65000,1,ticks)&&ticks==777);
    CHECK(!TryGetFusionShooterBatchCycleTicks(1109,(std::numeric_limits<float>::quiet_NaN)(),ticks)&&ticks==777);

    uint32_t sequence=0,next=999;int damage=-9;
    for(int shot=1;shot<=12;++shot)
    {
        CHECK(TryAdvanceFusionSniperShot(1109,sequence,next,damage));sequence=next;
        CHECK(damage==(shot%6?500:1000000));CHECK(sequence==uint32_t(shot%6));
    }
    CHECK(TryAdvanceFusionSniperShot(1167,0,next,damage)&&next==1&&damage==300);
    next=999;damage=-9;
    CHECK(!TryAdvanceFusionSniperShot(1109,6,next,damage)&&next==999&&damage==-9);
    CHECK(!TryAdvanceFusionSniperShot(1191,0,next,damage)&&next==999&&damage==-9);
    uint16_t bag=20,after=999;bool special=false;
    for(int i=20;i>0;--i)
    {CHECK(TryDrawFusionDoomCactusBag(bag,0,after,special)&&!special&&after==i-1);bag=after;}
    CHECK(TryDrawFusionDoomCactusBag(0,0,after,special)&&special&&after==20);
    CHECK(TryDrawFusionDoomCactusBag(20,20,after,special)&&special&&after==20);
    after=999;special=true;
    CHECK(!TryDrawFusionDoomCactusBag(21,0,after,special)&&after==999&&special);
    CHECK(!TryDrawFusionDoomCactusBag(20,21,after,special)&&after==999&&special);
    CHECK(!TryDrawFusionDoomCactusBag(20,-1,after,special)&&after==999&&special);
    float speed=0,rate=0;
    for(const auto c:{0U,100U,101U,300U,301U,1000000U})
    {
        CHECK(TryGetFusionGatlingMilestone(c,speed,rate));
        CHECK(speed==(c>300?12.f:c>100?9.f:6.f)&&rate==(c>300?2.f:c>100?1.5f:1.f));
        const auto* p=FindFusionBigGatlingTimingProfile(c,false);
        CHECK(p&&!p->speedParameterActive&&p->stateSpeed==rate);
        CHECK(FindFusionBigGatlingTimingProfile(c,true)->eventClipSeconds.size()==1);
    }
    CHECK(!FindFusionBigGatlingTimingProfile(0xffffffffU,false));
    speed=rate=-9;CHECK(!TryGetFusionGatlingMilestone(0xffffffffU,speed,rate)&&speed==-9&&rate==-9);

    for(const auto k:kinds)CHECK(FusionShooterBatchStateValid(k,0,0,k==1191?20:0,0));
    CHECK(FusionShooterBatchStateValid(1109,5,0,0,0));
    CHECK(!FusionShooterBatchStateValid(1167,6,0,0,0));
    CHECK(FusionShooterBatchStateValid(1161,301,0,0,3));
    CHECK(!FusionShooterBatchStateValid(1161,0xffffffffU,0,0,0));
    CHECK(FusionShooterBatchStateValid(1161,0,0,0,5));
    CHECK(FusionShooterBatchStateValid(1161,0,0,0,6));
    CHECK(!FusionShooterBatchStateValid(1161,0,0,0,7));
    CHECK(FusionShooterBatchStateValid(1165,0,500,0,1));
    CHECK(FusionShooterBatchStateValid(1168,0,1,0,1));
    CHECK(!FusionShooterBatchStateValid(1165,0,501,0,1));
    CHECK(!FusionShooterBatchStateValid(1165,0,1,0,0));
    CHECK(!FusionShooterBatchStateValid(1168,0,0,0,1));
    CHECK(!FusionShooterBatchStateValid(1168,1,0,0,0));
    CHECK(FusionShooterBatchStateValid(1191,0,0,20,1));
    CHECK(!FusionShooterBatchStateValid(1191,0,0,21,0));
    CHECK(FusionShooterBatchStateValid(65000,0,0,0,0));
    CHECK(!FusionShooterBatchStateValid(65000,1,0,0,0));
    CHECK(!FusionShooterBatchStateValid(1020,0,0,1,0));
    CHECK(FusionShooterBatchInvulnerable(1165,1)&&FusionShooterBatchInvulnerable(1168,500));
    CHECK(!FusionShooterBatchInvulnerable(1168,0)&&!FusionShooterBatchInvulnerable(1168,501));
    CHECK(!FusionShooterBatchInvulnerable(1161,500));
    CHECK(FusionShooterBatchMaximumDuration(1161,1)>=2.5f);
    CHECK(TryGetFusionBigGatlingPhaseTicks(0,1,0,ticks)&&ticks==211);
    CHECK(TryGetFusionBigGatlingPhaseTicks(1,1,0,ticks)&&ticks==258);
    CHECK(TryGetFusionBigGatlingPhaseTicks(2,1,0,ticks)&&ticks==126);
    CHECK(TryGetFusionBigGatlingPhaseTicks(4,1,0,ticks)&&ticks==63);
    CHECK(TryGetFusionBigGatlingPhaseTicks(6,1,0,ticks)&&ticks==84);
    int shotCounter=-1,stateCounter=-1;float elapsed=-1;
    CHECK(TryRephaseFusionBigGatling(2,1,101,0,shotCounter,stateCounter,elapsed));
    CHECK(shotCounter==56&&stateCounter==63&&std::abs(elapsed-.23f)<1e-6f);
    CHECK(TryRephaseFusionBigGatling(2,1,301,0,shotCounter,stateCounter,elapsed));
    CHECK(shotCounter==43&&stateCounter==48&&std::abs(elapsed-.17f)<1e-6f);
    CHECK(TryRephaseFusionBigGatling(3,1,101,0,shotCounter,stateCounter,elapsed));
    CHECK(shotCounter==1&&stateCounter==6&&std::abs(elapsed-.06f)<1e-6f);
    CHECK(TryRephaseFusionBigGatling(3,1,301,0,shotCounter,stateCounter,elapsed));
    CHECK(shotCounter==1&&stateCounter==4&&std::abs(elapsed-.05f)<1e-6f);
    shotCounter=stateCounter=-1;elapsed=-1;
    CHECK(!TryRephaseFusionBigGatling(1,1,101,0,shotCounter,stateCounter,elapsed)&&shotCounter==-1&&stateCounter==-1&&elapsed==-1);
    CHECK(!TryRephaseFusionBigGatling(2,1,101,6,shotCounter,stateCounter,elapsed)&&shotCounter==-1&&stateCounter==-1&&elapsed==-1);
    CHECK(!TryRephaseFusionBigGatling(2,0,101,0,shotCounter,stateCounter,elapsed)&&shotCounter==-1&&stateCounter==-1&&elapsed==-1);
    CHECK(FusionShooterBatchMaximumDuration(1191,.9f)>3.39f);
    CHECK(FusionShooterBatchMaximumDuration(1191,0)==0);

    CHECK(FusionSniperCanAcquire(0,false,false,100,200,800));
    CHECK(!FusionSniperCanAcquire(0,false,false,100,100,800));
    CHECK(!FusionSniperCanAcquire(0,false,false,100,800,800));
    CHECK(!FusionSniperCanAcquire(7,false,false,100,200,800));
    CHECK(!FusionSniperCanAcquire(0,true,false,100,200,800));
    CHECK(!FusionSniperCanAcquire(0,false,true,100,200,800));
    CHECK(TryGetFusionIronDamage(80,370,damage)&&damage==450);
    CHECK(TryGetFusionIronDamage(40,1100,damage)&&damage==1140);
    damage=-9;CHECK(!TryGetFusionIronDamage(80,-1,damage)&&damage==-9);
    CHECK(!TryGetFusionIronDamage((std::numeric_limits<int>::max)(),1,damage)&&damage==-9);
    CHECK(FusionShooterBatchProjectileMetadataValid(11,80,0,0,PROJECTILE_PEA,MOTION_STRAIGHT));
    CHECK(FusionShooterBatchProjectileMetadataValid(90,80,0,0,PROJECTILE_PEA,MOTION_STRAIGHT));
    CHECK(FusionShooterBatchProjectileMetadataValid(46,40,0,9,PROJECTILE_STAR,MOTION_STAR));
    CHECK(FusionShooterBatchProjectileMetadataValid(95,20,0,0,PROJECTILE_PEA,MOTION_STRAIGHT));
    for(int d:{20,60,600})CHECK(FusionShooterBatchProjectileMetadataValid(92,d,0,3,PROJECTILE_PEA,MOTION_PUFF));
    CHECK(!FusionShooterBatchProjectileMetadataValid(92,600,0,0,PROJECTILE_PEA,MOTION_PUFF));
    CHECK(!FusionShooterBatchProjectileMetadataValid(92,600,0,3,PROJECTILE_PEA,MOTION_STRAIGHT));
    CHECK(!FusionShooterBatchProjectileMetadataValid(46,40,0,0,PROJECTILE_STAR,MOTION_STAR));
    CHECK(!FusionShooterBatchProjectileMetadataValid(11,81,0,0,PROJECTILE_PEA,MOTION_STRAIGHT));
    CHECK(!FusionShooterBatchProjectileMetadataValid(11,80,5,0,PROJECTILE_PEA,MOTION_STRAIGHT));
    for(uint8_t way:{16,17,18})CHECK(FusionShooterBatchProjectileMetadataValid(0,20,0,way,PROJECTILE_PEA,MOTION_STRAIGHT));
    CHECK(!FusionShooterBatchProjectileMetadataValid(0,20,0,15,PROJECTILE_PEA,MOTION_STRAIGHT));
    CHECK(FusionShooterBatchProjectileMetadataValid(0,20,0,15,PROJECTILE_PEA,MOTION_STAR));
    CHECK(FusionShooterBatchProjectileMetadataValid(15,20,0,15,PROJECTILE_PEA,MOTION_STAR));
    CHECK(FusionShooterBatchProjectileMetadataValid(97,20,0,15,PROJECTILE_PEA,MOTION_STAR));
    CHECK(FusionShooterBatchProjectileMetadataValid(102,40,0,18,PROJECTILE_SPIKE,MOTION_STAR));
    CHECK(!FusionShooterBatchProjectileMetadataValid(102,40,0,18,PROJECTILE_SPIKE,MOTION_STRAIGHT));
    FusionRecipientRect rect{1,2,3,4};
    CHECK(TryMakeFusionShooterBatchProjectileRect(92,100,200,rect));
    CHECK(std::abs(rect.width-.32*(72.0/1.35))<1e-10);
    CHECK(std::abs(rect.y-(200+(.08048099-.43096197*.5)*(100.0/1.67)))<1e-10);
    CHECK(TryMakeFusionShooterBatchProjectileRect(11,100,200,rect));
    CHECK(std::abs(rect.width-1.1409948*.4*(72.0/1.35))<1e-10);
    rect={1,2,3,4};CHECK(!TryMakeFusionShooterBatchProjectileRect(65000,100,200,rect)&&rect.x==1&&rect.y==2);
    CHECK(!TryMakeFusionShooterBatchProjectileRect(92,(std::numeric_limits<double>::infinity)(),200,rect)&&rect.x==1);
    // The broad AABB includes corner space outside the actual rotated box.
    constexpr double quarterTurn=.7853981633974483;
    for(uint16_t kind:{46,102})
    {
        CHECK(TryMakeFusionShooterBatchRotatedRect(kind,100,200,quarterTurn,rect));
        CHECK(FusionShooterBatchRotatedProjectileTouchesRect(kind,100,200,quarterTurn,{99,199,2,2}));
        CHECK(!FusionShooterBatchRotatedProjectileTouchesRect(kind,100,200,quarterTurn,{rect.x+.01,rect.y+.01,.01,.01}));
        CHECK(!FusionShooterBatchRotatedProjectileTouchesRect(kind,100,200,quarterTurn,{1000,1000,10,10}));
        CHECK(!FusionShooterBatchRotatedProjectileTouchesRect(kind,100,200,quarterTurn,{100,200,0,1}));
    }
    CHECK(TryMakeFusionShooterBatchRotatedRect(102,100,200,0,rect));
    CHECK(std::abs(rect.width-.26838857*1.7*(72.0/1.35))<1e-10);
    CHECK(TryMakeFusionShooterBatchRotatedRect(46,100,200,0,rect));
    CHECK(std::abs(rect.width-.32703882*1.7*(72.0/1.35))<1e-10);
    rect={1,2,3,4};
    CHECK(!TryMakeFusionShooterBatchRotatedRect(11,100,200,0,rect)&&rect.x==1&&rect.y==2);
    CHECK(!TryMakeFusionShooterBatchRotatedRect(46,100,200,(std::numeric_limits<double>::quiet_NaN)(),rect)&&rect.x==1);
    float angle=-1;
    CHECK(TryGetFusionDoomCactusAngle(0,angle)&&angle==0);
    CHECK(TryGetFusionDoomCactusAngle(.125f,angle)&&std::abs(angle-.5553603768348694f)<1e-7f);
    CHECK(TryGetFusionDoomCactusAngle(.25f,angle)&&std::abs(angle-.7853981852531433f)<1e-7f);
    CHECK(TryGetFusionDoomCactusAngle(1000.125f,angle)&&std::abs(angle-.5553591251373291f)<1e-7f);
    CHECK(TryGetFusionDoomCactusAngle(10000.125f,angle)&&std::abs(angle-.5565558671951294f)<1e-7f);
    angle=-1;CHECK(!TryGetFusionDoomCactusAngle(-1,angle)&&angle==-1);
    CHECK(!TryGetFusionDoomCactusAngle((std::numeric_limits<float>::infinity)(),angle)&&angle==-1);
#undef CHECK
    return result;
}
