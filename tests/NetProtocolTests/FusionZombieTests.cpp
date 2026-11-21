#include "FusionZombieTests.h"
#include "../../Lawn/Fusion/FusionZombieRules.h"
#include <bit>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>

namespace
{
FusionZombieTestResult result;
void Check(bool value,int line)
{ ++result.mChecks; if(!value){++result.mFailures;std::printf("    SourceZombie check failed line=%d\n",line);} }
#define CHECK(value) Check(!!(value),__LINE__)
}

FusionZombieTestResult RunFusionZombieTests()
{
    using namespace LawnFusion;
    result={};
    std::printf("[ RUN      ] Fusion.SourceZombieOrdinaryRuleFoundation\n");
    const float nan=(std::numeric_limits<float>::quiet_NaN)(),inf=(std::numeric_limits<float>::infinity)();
    const FusionZombieSpawn sentinel{65000,77,88,99,111,1.25f,true};
    for(uint16_t kind:{uint16_t(0),uint16_t(2)})
    {
        const auto* profile=FindFusionZombieProfile(kind);
        CHECK(profile && profile->maxHealth==270 && profile->firstArmor==(kind==2?370:0) && profile->secondArmor==0 && profile->biteDamage==50);
        FusionZombieSpawn spawn=sentinel;
        for(float speed:{.9f,1.f,1.35f,std::nextafter(1.35f,2.f),1.8f})
        {CHECK(TryPrepareFusionZombieSpawn(kind,2,speed,spawn) && spawn.kind==kind && spawn.maxHealth==270 && spawn.firstArmor==(kind==2?370:0) && spawn.biteDamage==50 && spawn.originSpeed==speed && spawn.walk2==(speed>1.35f));}
        for(float speed:{std::nextafter(.9f,0.f),std::nextafter(1.8f,2.f),-1.f,nan,inf})
        {spawn=sentinel;CHECK(!TryPrepareFusionZombieSpawn(kind,2,speed,spawn) && spawn==sentinel);}
        for(int preset:{0,1,3,4,5,10})
        {spawn=sentinel;CHECK(!TryPrepareFusionZombieSpawn(kind,preset,1.f,spawn) && spawn==sentinel);}
        for(int mask=0;mask<16;++mask)
        {
            float factor=77;const bool cold=mask&1,freeze=mask&2,butter=mask&4,garlic=mask&8;
            CHECK(TryGetFusionZombieStatusSpeedFactor(kind,cold,freeze,butter,garlic,factor) && factor==(freeze||butter||garlic?0.f:cold?.5f:1.f));
        }
        uint32_t events=77;const float first=.33333334f,second=1.1f,end=1.35f;
        CHECK(TryCountFusionZombieAttackEvents(kind,{0,0},{0,std::nextafter(first,0.f)},events) && events==0);
        CHECK(TryCountFusionZombieAttackEvents(kind,{0,std::nextafter(first,0.f)},{0,first},events) && events==1);
        CHECK(TryCountFusionZombieAttackEvents(kind,{0,first},{0,first},events) && events==0);
        CHECK(TryCountFusionZombieAttackEvents(kind,{0,first},{0,second},events) && events==1);
        CHECK(TryCountFusionZombieAttackEvents(kind,{0,second},{0,end},events) && events==0);
        CHECK(TryCountFusionZombieAttackEvents(kind,{0,0},{0,end},events) && events==2);
        CHECK(TryCountFusionZombieAttackEvents(kind,{3,second},{4,first},events) && events==1);
        CHECK(TryCountFusionZombieAttackEvents(kind,{3,end},{4,0},events) && events==0);
        CHECK(TryCountFusionZombieAttackEvents(kind,{3,0},{6,end},events) && events==8);
        CHECK(TryCountFusionZombieAttackEvents(kind,{9,.2f},{9,.2f},events) && events==0);
        for(auto position:{FusionZombieClipPosition{0,-.1f},FusionZombieClipPosition{0,nan},FusionZombieClipPosition{0,inf},FusionZombieClipPosition{0,std::nextafter(end,2.f)}})
        {events=77;CHECK(!TryCountFusionZombieAttackEvents(kind,{0,0},position,events) && events==77);events=77;CHECK(!TryCountFusionZombieAttackEvents(kind,position,{1,0},events) && events==77);}
        events=77;CHECK(!TryCountFusionZombieAttackEvents(kind,{2,.5f},{2,.4f},events) && events==77);
        CHECK(!TryCountFusionZombieAttackEvents(kind,{2,0},{1,end},events) && events==77);
        CHECK(!TryCountFusionZombieAttackEvents(kind,{0,0},{(std::numeric_limits<uint32_t>::max)(),end},events) && events==77);
        CHECK(TryCountFusionZombieAttackEvents(kind,{(std::numeric_limits<uint32_t>::max)(),0},{(std::numeric_limits<uint32_t>::max)(),end},events) && events==2);

        FusionZombieHealthStep step{77,88,true};
        CHECK(TryStepFusionZombieHealth(kind,270,270,true,.01f,step) && std::bit_cast<uint32_t>(step.health)==0x4386dd71U && step.integerHealthProjection==269 && !step.deathAtUpdateStart);
        float remaining=77;int mirror=88;
        CHECK(TryApplyFusionZombieBodyDamage(kind,step.health,50,remaining,mirror) && std::bit_cast<uint32_t>(remaining)==0x435bbae2U && mirror==219);
        CHECK(TryStepFusionZombieHealth(kind,remaining,270,true,.01f,step) && std::bit_cast<uint32_t>(step.health)==0x435b75c3U && step.integerHealthProjection==219 && !step.deathAtUpdateStart);
        CHECK(TryStepFusionZombieHealth(kind,1.1f,270,true,.01f,step) && std::bit_cast<uint32_t>(step.health)==0x3f547ae2U && step.integerHealthProjection==0 && !step.deathAtUpdateStart);
        CHECK(TryStepFusionZombieHealth(kind,step.health,270,true,.01f,step) && std::bit_cast<uint32_t>(step.health)==0x3f0f5c2aU && step.integerHealthProjection==0 && step.deathAtUpdateStart);
        CHECK(TryStepFusionZombieHealth(kind,1,270,false,.01f,step) && step.health==1 && step.integerHealthProjection==1 && !step.deathAtUpdateStart);
        CHECK(TryStepFusionZombieHealth(kind,std::nextafter(1.f,0.f),270,false,.01f,step) && step.integerHealthProjection==0 && step.deathAtUpdateStart);
        CHECK(TryStepFusionZombieHealth(kind,-.1f,270,false,0,step) && step.health==-.1f && step.integerHealthProjection==0 && step.deathAtUpdateStart);
        CHECK(TryStepFusionZombieHealth(kind,270,270,true,0,step) && step.health==270 && step.integerHealthProjection==270 && !step.deathAtUpdateStart);
        // This tiny HP exposes source dt*.1*max rounding; max*.1*dt differs by one ULP.
        CHECK(TryStepFusionZombieHealth(kind,.27f,270,true,.01f,step) && step.health==0 && step.integerHealthProjection==0 && step.deathAtUpdateStart);
        const FusionZombieHealthStep preserved{77,88,true};
        for(float health:{nan,inf,-inf,2147483648.f})
        {step=preserved;CHECK(!TryStepFusionZombieHealth(kind,health,270,true,.01f,step) && step==preserved);remaining=77;mirror=88;CHECK(!TryApplyFusionZombieBodyDamage(kind,health,1,remaining,mirror) && remaining==77 && mirror==88);}
        for(float delta:{-.01f,nan,inf,(std::numeric_limits<float>::max)()})
        {step=preserved;CHECK(!TryStepFusionZombieHealth(kind,270,270,true,delta,step) && step==preserved);}
        step=preserved;CHECK(!TryStepFusionZombieHealth(kind,270,0,true,.01f,step) && step==preserved);
        CHECK(!TryStepFusionZombieHealth(kind,270,-270,true,.01f,step) && step==preserved);
        remaining=77;mirror=88;CHECK(!TryApplyFusionZombieBodyDamage(kind,270,-1,remaining,mirror) && remaining==77 && mirror==88);
        CHECK(!TryApplyFusionZombieBodyDamage(kind,-2147483648.f,256,remaining,mirror) && remaining==77 && mirror==88);
        CHECK(TryApplyFusionZombieBodyDamage(kind,-2147483648.f,1,remaining,mirror) && remaining==-2147483648.f && mirror==(std::numeric_limits<int>::min)());

        float arm=77,head=88;
        CHECK(TryGetFusionZombieLimbThresholds(kind,270,arm,head) &&
            std::bit_cast<uint32_t>(arm)==0x43340000U && std::bit_cast<uint32_t>(head)==0x42b40000U);
        CHECK(179.9f<arm && !(std::ceil(179.9f)<arm) && 89.9f<head && !(std::ceil(89.9f)<head));
        CHECK(std::nextafter(arm,0.f)<arm && !(arm<arm) && !(std::nextafter(arm,inf)<arm));
        CHECK(std::nextafter(head,0.f)<head && !(head<head) && !(std::nextafter(head,inf)<head));
        CHECK(TryGetFusionZombieLimbThresholds(kind,271,arm,head) &&
            std::bit_cast<uint32_t>(arm)==0x4334aaabU && std::bit_cast<uint32_t>(head)==0x42b4aaabU);
        // Integer floor thresholds and a ceil liveness mirror both miss these.
        CHECK(180.5f<arm && !(180.5f<static_cast<float>(2*271/3)) && !(std::ceil(180.5f)<arm));
        CHECK(90.2f<head && !(90.2f<static_cast<float>(271/3)) && !(std::ceil(90.2f)<head));
        CHECK(std::nextafter(arm,0.f)<arm && !(arm<arm) && !(std::nextafter(arm,inf)<arm));
        CHECK(std::nextafter(head,0.f)<head && !(head<head) && !(std::nextafter(head,inf)<head));
        struct LimbGolden { int maximum; uint32_t armBits,headBits; };
        for (const auto& golden:{LimbGolden{1,0x3f2aaaabU,0x3eaaaaabU},
            LimbGolden{3,0x40000000U,0x3f800000U},
            LimbGolden{1073741823,0x4e2aaaabU,0x4daaaaabU},
            LimbGolden{1073741824,0xce2aaaabU,0x4daaaaabU},
            LimbGolden{(std::numeric_limits<int>::max)(),0xbf2aaaabU,0x4e2aaaabU}})
            CHECK(TryGetFusionZombieLimbThresholds(kind,golden.maximum,arm,head) &&
                std::bit_cast<uint32_t>(arm)==golden.armBits && std::bit_cast<uint32_t>(head)==golden.headBits);
        for(int maximum:{0,-1,(std::numeric_limits<int>::min)()})
        { arm=77;head=88;CHECK(!TryGetFusionZombieLimbThresholds(kind,maximum,arm,head) && arm==77 && head==88); }

        FusionZombieShrinkHealth shrink{77,88,99};
        CHECK(TryShrinkFusionZombieHealth(kind,270,270,shrink) && shrink.health==135 && shrink.maximumHealth==135 && shrink.livenessHealth==135);
        CHECK(TryShrinkFusionZombieHealth(kind,271,271,shrink) && shrink.health==135.5f && shrink.maximumHealth==135 && shrink.livenessHealth==136);
        CHECK(FusionZombieHealthFitsMaximum(shrink.health,shrink.maximumHealth));
        CHECK(TryShrinkFusionZombieHealth(kind,277,277,shrink) && shrink.health==138.5f && shrink.maximumHealth==138 && shrink.livenessHealth==139);
        CHECK(TryShrinkFusionZombieHealth(kind,269.75f,270,shrink) && shrink.health==134.875f && shrink.maximumHealth==135 && shrink.livenessHealth==135);
        CHECK(TryShrinkFusionZombieHealth(kind,135.5f,135,shrink) && shrink.health==67.75f && shrink.maximumHealth==67 && shrink.livenessHealth==68);
        CHECK(TryShrinkFusionZombieHealth(kind,1,1,shrink) && shrink.health==.5f && shrink.maximumHealth==0 && shrink.livenessHealth==1);
        CHECK(TryStepFusionZombieHealth(kind,shrink.health,shrink.maximumHealth,false,0,step) && step.deathAtUpdateStart);
        CHECK(TryShrinkFusionZombieHealth(kind,-.1f,270,shrink) && shrink.health==-.1f && shrink.maximumHealth==270 && shrink.livenessHealth==1);
        CHECK(FusionZombieHealthFitsMaximum(std::nextafter(136.f,0.f),135));
        CHECK(!FusionZombieHealthFitsMaximum(136,135) && !FusionZombieHealthFitsMaximum(0,-1));
        CHECK(FusionZombieHealthFitsMaximum(.5f,0) && !FusionZombieHealthFitsMaximum(1,0));
        const FusionZombieShrinkHealth savedShrink{77,88,99};
        for(float health:{nan,inf,-inf,271.f})
        {shrink=savedShrink;CHECK(!TryShrinkFusionZombieHealth(kind,health,270,shrink) && shrink==savedShrink);}
        shrink=savedShrink;CHECK(!TryShrinkFusionZombieHealth(kind,1,-1,shrink) && shrink==savedShrink);
    }
    CHECK(FindFusionZombieProfile(4)&&FindFusionZombieProfile(4)->firstArmor==1100);
    CHECK(FindFusionZombieProfile(8)&&FindFusionZombieProfile(8)->secondArmor==1100);
    CHECK(FindFusionZombieProfile(11)&&FindFusionZombieProfile(11)->waterOnly);
    CHECK(FindFusionZombieProfile(41)&&FindFusionZombieProfile(41)->noAttack);
    for(uint16_t kind:{uint16_t(1),uint16_t(65000),uint16_t(65535)})
    {
        CHECK(!FindFusionZombieProfile(kind));FusionZombieSpawn spawn=sentinel;CHECK(!TryPrepareFusionZombieSpawn(kind,2,1.f,spawn) && spawn==sentinel);
        float factor=77;CHECK(!TryGetFusionZombieStatusSpeedFactor(kind,true,false,false,false,factor) && factor==77);
        uint32_t events=77;CHECK(!TryCountFusionZombieAttackEvents(kind,{0,0},{0,1.35f},events) && events==77);
        FusionZombieHealthStep step{77,88,true};const auto saved=step;CHECK(!TryStepFusionZombieHealth(kind,270,270,true,.01f,step) && step==saved);
        float remaining=77;int mirror=88;CHECK(!TryApplyFusionZombieBodyDamage(kind,270,50,remaining,mirror) && remaining==77 && mirror==88);
        float arm=77,head=88;CHECK(!TryGetFusionZombieLimbThresholds(kind,270,arm,head) && arm==77 && head==88);
        FusionZombieShrinkHealth shrink{77,88,99};const auto savedShrink=shrink;
        CHECK(!TryShrinkFusionZombieHealth(kind,270,270,shrink) && shrink==savedShrink);
    }
    std::printf("[ %s ] Fusion.SourceZombieOrdinaryRuleFoundation checks=%d failures=%d\n",result.mFailures?"FAILED":"OK",result.mChecks,result.mFailures);
    return result;
}
