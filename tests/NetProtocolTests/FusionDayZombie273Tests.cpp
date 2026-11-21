#include "FusionDayZombie273Tests.h"
#include "../../Lawn/Fusion/FusionDayZombie273Rules.h"
#include <cmath>
#include <cstdio>
#include <limits>

int RunFusionDayZombie273Tests(int& checks)
{
    using namespace LawnFusion;int failures=0;
    const auto check=[&](bool okay,int line){++checks;if(!okay){++failures;std::printf("FusionDayZombie273Tests:%d failed\n",line);}};
#define CHECK(x) check(bool(x),__LINE__)
    CHECK(FindFusionDayZombie273Profile(3)->maxHealth==720);
    CHECK(FindFusionDayZombie273Profile(21)->maxHealth==2000&&FindFusionDayZombie273Profile(21)->firstArmor==4400);
    CHECK(FindFusionDayZombie273Profile(22)->firstArmor==2200&&FindFusionDayZombie273Profile(23)->firstArmor==1100);
    CHECK(FindFusionDayZombie273Profile(100)->maxHealth==270&&FindFusionDayZombie273Profile(108)->maxHealth==1350);
    CHECK(FindFusionDayZombie273Profile(106)->firstArmor==2200&&FindFusionDayZombie273Profile(107)->firstArmor==1100);
    for(uint16_t kind:{3,21,22,23,100,106,107,108})
    {
        const auto* p=FindFusionDayZombie273Profile(kind);CHECK(p&&p->biteDamage==50&&!p->nativeLocomotion);
        const auto needed=FusionDayZombie273RequiredCapabilities(kind);
        CHECK(needed&&FusionDayZombie273CapabilitiesReady(kind,needed));
        for(uint32_t bit=1;bit<=Day273BucketItem;bit<<=1)if(needed&bit)CHECK(!FusionDayZombie273CapabilitiesReady(kind,needed&~bit));
    }
    CHECK(!FindFusionDayZombie273Profile(0)&&!FusionDayZombie273CapabilitiesReady(0,0xffffffffU));
    const auto deps=FusionDayZombie273NpcDependencies(21);
    CHECK(deps.size()==3&&deps[0]==22&&deps[1]==23&&deps[2]==2);
    CHECK(FusionDayZombie273NpcDependencies(22).size()==2&&FusionDayZombie273NpcDependencies(23).front()==2);
    CHECK(FusionDayZombie273NpcDependencies(100).empty());
    constexpr uint16_t onlyCone[]={2},coneAndSilver[]={2,23},dollClosure[]={2,22,23};
    CHECK(!FusionDayZombie273Ready(21,0xffffffffU,onlyCone));
    CHECK(!FusionDayZombie273Ready(21,0xffffffffU,coneAndSilver));
    CHECK(FusionDayZombie273Ready(21,0xffffffffU,dollClosure));
    CHECK(FusionDayZombie273Ready(22,0xffffffffU,coneAndSilver));
    CHECK(FusionDayZombie273Ready(23,0xffffffffU,onlyCone));
    CHECK(!FusionDayZombie273Ready(23,0,onlyCone));
    uint16_t child=999;
    CHECK(TryGetFusionDayZombie273DollChild(21,child)&&child==22);
    CHECK(TryGetFusionDayZombie273DollChild(22,child)&&child==23);
    CHECK(TryGetFusionDayZombie273DollChild(23,child)&&child==2);
    CHECK(!TryGetFusionDayZombie273DollChild(2,child)&&child==2);
    CHECK(FusionDayZombie273DollCanFall(98,100)&&!FusionDayZombie273DollCanFall(99,100));
    CHECK(!FusionDayZombie273DollCanFall(-1,100)&&!FusionDayZombie273DollCanFall(0,1));
    FusionDayZombie273Clip clip;
    CHECK(TryGetFusionDayZombie273Clip(3,2,false,clip)&&clip.seconds==3&&clip.multiplier==2.5f&&clip.loop);
    CHECK(TryGetFusionDayZombie273Clip(3,3,false,clip)&&clip.seconds==3.5f&&clip.multiplier==2&&!clip.loop);
    CHECK(TryGetFusionDayZombie273Clip(3,0,true,clip)&&clip.seconds==2.25f&&clip.multiplier==1);
    CHECK(TryGetFusionDayZombie273Clip(3,0,false,clip)&&clip.seconds==3.3333333f&&clip.multiplier==.5f);
    CHECK(TryGetFusionDayZombie273Clip(3,1,false,clip)&&clip.seconds==2.1666667f&&clip.multiplier==1.8f&&!clip.loop);
    CHECK(TryGetFusionDayZombie273Clip(108,1,false,clip)&&clip.seconds==1.3333334f&&clip.multiplier==1&&!clip.loop);
    CHECK(!TryGetFusionDayZombie273Clip(21,0,false,clip));
    for(uint16_t kind:{100,106,107,108})CHECK(TryGetFusionDayZombie273Clip(kind,0,false,clip)&&clip.seconds==1.9166666f&&clip.multiplier==.5f);
    double delta=99;
    CHECK(TryGetFusionDayZombie273RootDelta(3,2,{0,0},{1,0},false,1,delta)&&std::abs(delta-double(1.5680001f-1.9616f))<1e-6);
    CHECK(TryGetFusionDayZombie273RootDelta(3,2,{0,0},{1,0},true,1,delta)&&delta>0);
    CHECK(TryGetFusionDayZombie273RootDelta(3,0,{0,0},{1,0},false,1,delta)&&std::abs(delta-double(4.56f-5.131f))<1e-6);
    CHECK(TryGetFusionDayZombie273RootDelta(3,3,{0,0},{0,3.5f},false,1,delta)&&std::abs(delta-double(2.2784f-4.1048f))<1e-6);
    CHECK(TryGetFusionDayZombie273RootDelta(3,3,{0,0},{1,3.5f},false,1,delta)&&delta<0);
    CHECK(!TryGetFusionDayZombie273RootDelta(3,3,{0,0},{1,0},false,1,delta));
    CHECK(!TryGetFusionDayZombie273RootDelta(3,2,{1,0},{0,1},false,1,delta));
    CHECK(!TryGetFusionDayZombie273RootDelta(3,2,{0,1},{0,.5f},false,1,delta));
    CHECK(TryGetFusionDayZombie273RootDelta(100,0,{0,0},{1,0},false,1,delta)&&std::abs(delta-double(.55806446f-3.1064515f))<1e-6);
    CHECK(TryGetFusionDayZombie273RootDelta(108,0,{0,0},{0,0},false,1,delta)&&delta==0);
    uint8_t events=255;
    CHECK(TryGetFusionDayZombie273PoleEvents({0,0},{0,.75f},events)&&events==Day273JumpSound1);
    CHECK(TryGetFusionDayZombie273PoleEvents({0,.75f},{0,.75f},events)&&events==0);
    CHECK(TryGetFusionDayZombie273PoleEvents({0,0},{0,3.5f},events)&&events==15);
    CHECK(TryGetFusionDayZombie273PoleEvents({0,2},{1,3.5f},events)&&events==Day273JumpOver);
    CHECK(!TryGetFusionDayZombie273PoleEvents({0,0},{1,0},events));
    uint32_t bites=99;
    CHECK(TryCountFusionDayZombie273Bites(3,{0,0},{0,.98333335f},bites)&&bites==1);
    CHECK(TryCountFusionDayZombie273Bites(3,{0,.98333335f},{0,.98333335f},bites)&&bites==0);
    CHECK(TryCountFusionDayZombie273Bites(3,{0,0},{1,0},bites)&&bites==2);
    CHECK(TryCountFusionDayZombie273Bites(3,{0,2.25f},{1,0},bites)&&bites==0);
    CHECK(!TryCountFusionDayZombie273Bites(3,{1,0},{0,1},bites));
    FusionDayZombie273PlantTraits traits;
    CHECK(FusionDayZombie273PoleCanStart(2,false,false,true,true,traits));
    CHECK(!FusionDayZombie273PoleCanStart(2,true,false,true,true,traits));
    CHECK(!FusionDayZombie273PoleCanStart(2,false,true,true,true,traits));
    CHECK(!FusionDayZombie273PoleCanStart(2,false,false,false,true,traits));
    traits.caltrop=true;CHECK(!FusionDayZombie273PoleCanStart(2,false,false,true,true,traits));traits.caltrop=false;
    traits.flying=true;CHECK(!FusionDayZombie273PoleCanStart(2,false,false,true,true,traits));traits.flying=false;
    traits.tallNut=true;CHECK(FusionDayZombie273PoleCanStart(2,false,false,true,true,traits)&&FusionDayZombie273PoleBlocksLanding(true,traits));
    traits.ladder=true;CHECK(!FusionDayZombie273PoleCanStart(2,false,false,true,true,traits)&&!FusionDayZombie273PoleBlocksLanding(true,traits));
    float after=99;bool play=true;
    CHECK(TryStepFusionDayZombie273Shoot(100,.01f,1.5f,.01f,0,false,false,0,after,play)&&after==0&&!play);
    CHECK(TryStepFusionDayZombie273Shoot(100,0,1.5f,.01f,0,false,false,-.1f,after,play)&&after==1.4f&&play);
    CHECK(TryStepFusionDayZombie273Shoot(108,0,1.5f,.01f,0,false,false,.1f,after,play)&&after==1.6f&&play);
    CHECK(TryStepFusionDayZombie273Shoot(100,.25f,1.5f,.01f,1,false,false,0,after,play)&&after==.25f&&!play);
    CHECK(TryStepFusionDayZombie273Shoot(100,.25f,1.5f,.01f,0,true,false,0,after,play)&&after==.25f&&!play);
    CHECK(TryStepFusionDayZombie273Shoot(100,.25f,1.5f,.01f,0,false,true,0,after,play)&&after==.25f&&!play);
    CHECK(!TryStepFusionDayZombie273Shoot(3,0,1.5f,.01f,0,false,false,0,after,play));
    CHECK(!TryStepFusionDayZombie273Shoot(100,0,1.5f,.01f,0,false,false,.11f,after,play));
    CHECK(!TryStepFusionDayZombie273Shoot(100,(std::numeric_limits<float>::quiet_NaN)(),1.5f,.01f,0,false,false,0,after,play));
    FusionDayZombie273Shot shot;
    CHECK(TryMakeFusionDayZombie273Shot(100,0,false,false,0,false,shot)&&shot.kind==0&&shot.damage==20&&shot.way==9&&shot.hostile&&shot.offsetX==-.4f);
    CHECK(TryMakeFusionDayZombie273Shot(108,0,false,false,1,true,shot)&&shot.kind==11&&shot.damage==80&&shot.way==0&&!shot.hostile&&shot.offsetX==.4f);
    CHECK(!TryMakeFusionDayZombie273Shot(100,1,false,false,0,false,shot));
    CHECK(!TryMakeFusionDayZombie273Shot(100,0,true,false,0,false,shot));
    CHECK(!TryMakeFusionDayZombie273Shot(100,0,false,true,0,false,shot));
    CHECK(FusionDayZombie273ProjectileTuple(0,20,9,true)&&FusionDayZombie273ProjectileTuple(11,80,9,true));
    CHECK(!FusionDayZombie273ProjectileTuple(11,160,9,true)&&FusionDayZombie273ProjectileTuple(11,160,0,false));
    CHECK(!FusionDayZombie273ProjectileTuple(0,80,0,false)&&!FusionDayZombie273ProjectileTuple(11,80,13,true));
    for(int status:{1,3,7,9,10,12,13,16,20,21})CHECK(!FusionDayZombie273ProjectileCanHit(status));
    for(int status:{0,2,4,5,6,8,11,14,15,17,18,19,22,23,24})CHECK(FusionDayZombie273ProjectileCanHit(status));
    FusionDayZombie273ZombieHit hit;
    CHECK(TryPlanFusionDayZombie273ZombieHit(0,9,20,200,true,hit)&&hit.damage==20&&hit.sourceDamageType==4&&hit.knockback==0);
    CHECK(TryPlanFusionDayZombie273ZombieHit(0,0,20,200,false,hit)&&hit.sourceDamageType==0);
    CHECK(TryPlanFusionDayZombie273ZombieHit(11,9,80,200,true,hit)&&hit.damage==280&&hit.sourceDamageType==0&&hit.knockback==-.2f);
    CHECK(TryPlanFusionDayZombie273ZombieHit(11,0,80,0,false,hit)&&hit.damage==80&&hit.knockback==.2f);
    CHECK(TryPlanFusionDayZombie273ZombieHit(11,0,80,(std::numeric_limits<int>::max)(),false,hit)&&hit.damage==(std::numeric_limits<int>::min)()+79);
    uint8_t stage=0;int sprite=99;
    CHECK(TryGetFusionDayZombie273ArmorStage(106,1466,2200,0,stage,sprite)&&stage==0&&sprite==-1); // Integer threshold, equality intact.
    CHECK(TryGetFusionDayZombie273ArmorStage(106,1465,2200,0,stage,sprite)&&stage==1&&sprite==14);
    CHECK(TryGetFusionDayZombie273ArmorStage(106,733,2200,1,stage,sprite)&&stage==1);
    CHECK(TryGetFusionDayZombie273ArmorStage(106,732,2200,1,stage,sprite)&&stage==2&&sprite==15);
    CHECK(TryGetFusionDayZombie273ArmorStage(107,733,1100,0,stage,sprite)&&stage==1&&sprite==16); // Floating threshold differs from106.
    CHECK(TryGetFusionDayZombie273ArmorStage(107,366,1100,0,stage,sprite)&&stage==2&&sprite==17);
    CHECK(TryGetFusionDayZombie273ArmorStage(21,100,4400,0,stage,sprite)&&stage==2&&sprite==45);
    CHECK(TryGetFusionDayZombie273ArmorStage(22,100,2200,0,stage,sprite)&&stage==2&&sprite==47);
    CHECK(TryGetFusionDayZombie273ArmorStage(23,100,1100,0,stage,sprite)&&stage==2&&sprite==49);
    CHECK(TryGetFusionDayZombie273ArmorStage(21,4400,4400,2,stage,sprite)&&stage==2); // No invented heal/reset.
    CHECK(!TryGetFusionDayZombie273ArmorStage(106,-1,2200,0,stage,sprite));
    int armor=99,overkill=99;
    CHECK(TryStepFusionDayZombie273Armor(106,2200,2199,armor,overkill)&&armor==1&&overkill==0);
    CHECK(TryStepFusionDayZombie273Armor(106,2200,2200,armor,overkill)&&armor==0&&overkill==0);
    CHECK(TryStepFusionDayZombie273Armor(107,1100,1111,armor,overkill)&&armor==0&&overkill==11);
    CHECK(!TryStepFusionDayZombie273Armor(108,100,1,armor,overkill));
#undef CHECK
    return failures;
}
