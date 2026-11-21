#include "FusionAdventureScheduler273Tests.h"
#include "../../Lawn/Fusion/FusionAdventureScheduler273.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <set>
#include <string>
#include <vector>

namespace
{
using namespace LawnFusion;
using Result=FusionAdventureScheduler273Result;
FusionAdventureScheduler273TestResult result;
void Check(bool okay,int line)
{++result.mChecks;if(!okay){++result.mFailures;std::printf("    AdventureScheduler273 failed line=%d\n",line);}}
#define CHECK(value) Check(!!(value),__LINE__)
struct Fixture
{
    bool ready=true,capacity=true,spawnOkay=true,badInteger=false,badReal=false,healthOkay=true;
    uint16_t refused=0xffff;
    int weightDraw=1,integers=0,reals=0,healthCalls=0,spawnCalls=0,canSpawnCalls=0,nextRow=0;
    float health=0,jitter=1.f,initialHealth=270.f;
    std::vector<int> rowDraws;
    size_t rowAt=0;
    std::string calls;
    std::vector<uint16_t> readyKinds;
    std::vector<FusionAdventureScheduler273Spawn> spawned;
    static int Integer(void* user,int low,int high)
    {
        auto& f=*static_cast<Fixture*>(user);++f.integers;
        if(f.badInteger)return high;
        if(low==1){f.calls+='W';return f.weightDraw;}
        f.calls+='R';
        if(f.rowAt<f.rowDraws.size())return f.rowDraws[f.rowAt++];
        const int row=f.nextRow%(high-low)+low;++f.nextRow;return row;
    }
    static float Real(void* user,float low,float high)
    {
        auto& f=*static_cast<Fixture*>(user);++f.reals;
        f.calls+=low==3.5f?'T':'J';
        if(f.badReal)return std::numeric_limits<float>::quiet_NaN();
        return low==3.5f?high:f.jitter;
    }
    static bool Ready(void* user,uint16_t kind)
    {auto& f=*static_cast<Fixture*>(user);f.readyKinds.push_back(kind);return f.ready&&kind!=f.refused;}
    static bool Health(void* user,float& out)
    {auto& f=*static_cast<Fixture*>(user);++f.healthCalls;f.calls+='H';out=f.health;return f.healthOkay;}
    static bool Capacity(void* user,std::span<const uint16_t>)
    {auto& f=*static_cast<Fixture*>(user);++f.canSpawnCalls;f.calls+='C';return f.capacity;}
    static bool Spawn(void* user,int,std::span<const FusionAdventureScheduler273Spawn> actors,float& out)
    {
        auto& f=*static_cast<Fixture*>(user);++f.spawnCalls;f.calls+='S';
        f.spawned.assign(actors.begin(),actors.end());out=f.initialHealth;return f.spawnOkay;
    }
    FusionAdventureScheduler273Random Random(){return {this,Integer,Real};}
    FusionAdventureScheduler273Callbacks Callbacks(){return {this,Random(),Health,Capacity,Spawn};}
    Result Init(FusionAdventureScheduler273State& s,int level=1,int cycle=1,int difficulty=2)
    {return InitializeFusionAdventureScheduler273(s,true,level,cycle,difficulty,Random(),this,Ready);}
    void ClearCalls(){integers=reals=healthCalls=spawnCalls=canSpawnCalls=0;calls.clear();spawned.clear();}
};
FusionAdventureScheduler273Input Playing(float dt)
{FusionAdventureScheduler273Input input;input.host=true;input.deltaSeconds=dt;return input;}
bool Near(float a,float b){return std::abs(a-b)<.00001f;}
}
FusionAdventureScheduler273TestResult RunFusionAdventureScheduler273Tests()
{
    result={};std::printf("[ RUN      ] Fusion.AdventureScheduler273\n");
    {
        Fixture f;FusionAdventureScheduler273State s;
        CHECK(f.Init(s)==Result::Okay);CHECK(s.initialized&&s.wave==0&&s.maximumWaves==10&&s.rows==5);
        CHECK(s.countdown==15.f&&s.healthElapsed==0&&s.hold==0&&s.hugeElapsed==0);
        const size_t expected[]={1,3,5,6,8,10,11,13,15,33};size_t total=0;
        for(size_t wave=0;wave<s.waves.size();++wave)
        {CHECK(s.waves[wave].size()==expected[wave]);for(auto kind:s.waves[wave])CHECK(kind==0);total+=s.waves[wave].size();}
        CHECK(total==105);CHECK(f.integers==104&&f.reals==0); // Final guaranteed0 has no RNG draw.
        for(int difficulty=0;difficulty<=5;++difficulty)
        {FusionAdventureScheduler273State other;CHECK(f.Init(other,3,1,difficulty)==Result::Okay);CHECK(other.maximumWaves==10);}
        FusionAdventureScheduler273State later;
        CHECK(f.Init(later,1,2)==Result::Okay&&later.maximumWaves==20);
        CHECK(f.Init(later,1,3)==Result::Okay&&later.maximumWaves==20);
    }
    {
        Fixture f;FusionAdventureScheduler273State s;
        f.refused=21;CHECK(f.Init(s,6)==Result::NotReady);
        CHECK(!s.initialized&&f.integers==0&&f.reals==0); // Includes final-only21 before generation.
        f.refused=0;f.readyKinds.clear();CHECK(f.Init(s,8)==Result::NotReady);
        CHECK(f.readyKinds.size()==1&&f.readyKinds.front()==0&&f.integers==0); // Budget fallback even when0 is not allow-listed.
        f.refused=0xffff;
        CHECK(InitializeFusionAdventureScheduler273(s,false,1,1,2,f.Random(),&f,Fixture::Ready)==Result::NotHost);
        CHECK(f.integers==0);
        CHECK(f.Init(s,0)==Result::Invalid&&f.Init(s,46)==Result::Invalid);
        CHECK(f.Init(s,1,0)==Result::Invalid&&f.Init(s,1,1,6)==Result::Invalid);
        CHECK(f.Init(s,25,2,5)==Result::NotReady&&f.integers==0);
        f.badInteger=true;CHECK(f.Init(s)==Result::RandomFailed&&!s.initialized);
    }
    {
        Fixture f;f.weightDraw=9000;FusionAdventureScheduler273State s;
        CHECK(f.Init(s,3)==Result::Okay);
        CHECK(s.waves[0].size()==1&&s.waves[0][0]==0); // Chosen bucket4 costs4, budget1 emits0.
        const auto& final=s.waves.back();CHECK(final.size()>=3);
        CHECK(final[final.size()-3]==0&&final[final.size()-2]==2&&final.back()==4);
        CHECK(final.size()==11); // Budget32 /cost4 =8, then guaranteed0/2/4.
    }
    {
        Fixture f;FusionAdventureScheduler273State s;CHECK(f.Init(s)==Result::Okay);f.ClearCalls();
        FusionAdventureScheduler273Events e;
        auto input=Playing(1.f);input.host=false;
        CHECK(UpdateFusionAdventureScheduler273(s,input,f.Callbacks(),e)==Result::NotHost);
        CHECK(s.countdown==15.f&&s.healthElapsed==0&&f.calls.empty());
        input.host=true;input.paused=true;
        CHECK(UpdateFusionAdventureScheduler273(s,input,f.Callbacks(),e)==Result::Okay&&f.calls.empty());
        input.paused=false;input.tutorial=true;
        CHECK(UpdateFusionAdventureScheduler273(s,input,f.Callbacks(),e)==Result::Okay);
        CHECK(s.healthElapsed==1.f&&f.healthCalls==0&&f.reals==0&&s.countdown==15.f);
        input.deltaSeconds=.001f;f.health=-11.f;
        CHECK(UpdateFusionAdventureScheduler273(s,input,f.Callbacks(),e)==Result::Okay);
        CHECK(e.healthSampled&&f.healthCalls==1&&s.healthElapsed==0&&s.currentHealth==-11.f&&f.reals==0);
        CHECK(FusionAdventureScheduler273TutorOver(s)&&s.countdown==5.f);
        input.tutorial=false;input.deltaSeconds=.25f;
        for(int gate=0;gate<4;++gate)
        {
            auto gated=input;gated.iceDoom=gate==0;gated.allowSummon=gate!=1;gated.boss=gate==2;gated.scaryPot=gate==3;
            CHECK(UpdateFusionAdventureScheduler273(s,gated,f.Callbacks(),e)==Result::Okay);
            CHECK(s.countdown==5.f&&s.hold==0&&f.reals==0);
        }
        input.over=true;const float elapsed=s.healthElapsed;
        CHECK(UpdateFusionAdventureScheduler273(s,input,f.Callbacks(),e)==Result::Okay&&s.healthElapsed==elapsed);
    }
    {
        Fixture f;FusionAdventureScheduler273State s;CHECK(f.Init(s)==Result::Okay);f.ClearCalls();
        FusionAdventureScheduler273Events e;auto input=Playing(.01f);s.countdown=.01f;
        CHECK(UpdateFusionAdventureScheduler273(s,input,f.Callbacks(),e)==Result::Okay);
        CHECK(s.countdown==0&&s.wave==0&&f.calls=="T"&&f.spawnCalls==0); // Strictly negative, threshold still drawn on wave0.
        f.ClearCalls();CHECK(UpdateFusionAdventureScheduler273(s,input,f.Callbacks(),e)==Result::Okay);
        CHECK(e.firstWave&&!e.finalWave&&!e.hugeWarning&&e.spawnedWave==1&&e.spawnedCount==1);
        CHECK(s.wave==1&&s.countdown==30.f&&Near(s.hold,.02f)&&s.latestWaveHealth==270.f&&s.currentHealth==270.f);
        CHECK(f.calls=="TCRS"); // Whole-wave preflight, all row RNG, then actual birth callback.
        s.countdown=10.f;s.hold=4.2f;s.currentHealth=0;s.latestWaveHealth=270.f;input.deltaSeconds=0;
        f.ClearCalls();CHECK(UpdateFusionAdventureScheduler273(s,input,f.Callbacks(),e)==Result::Okay);
        CHECK(s.hold==4.2f&&s.countdown==10.f&&f.calls=="T");
        input.deltaSeconds=.01f;f.jitter=.95f;f.ClearCalls();
        CHECK(UpdateFusionAdventureScheduler273(s,input,f.Callbacks(),e)==Result::Okay);
        CHECK(Near(s.countdown,.95f*1.8f)&&s.hold==0&&f.calls=="TJ");
    }
    {
        Fixture f;FusionAdventureScheduler273State s;CHECK(f.Init(s)==Result::Okay);f.ClearCalls();
        FusionAdventureScheduler273Events e;auto input=Playing(.1f);s.wave=9;s.countdown=.1f;
        CHECK(UpdateFusionAdventureScheduler273(s,input,f.Callbacks(),e)==Result::Okay&&!e.hugeWarning);
        CHECK(UpdateFusionAdventureScheduler273(s,input,f.Callbacks(),e)==Result::Okay&&e.hugeWarning&&s.hugeWarning);
        CHECK(s.wave==9&&Near(s.hugeElapsed,.1f)&&f.spawnCalls==0);
        s.hugeElapsed=5.f;input.deltaSeconds=0;f.ClearCalls();
        CHECK(UpdateFusionAdventureScheduler273(s,input,f.Callbacks(),e)==Result::Okay);
        CHECK(!e.hugeWarning&&s.wave==9&&f.spawnCalls==0); // Exactly5s cannot spawn.
        input.deltaSeconds=.01f;
        CHECK(UpdateFusionAdventureScheduler273(s,input,f.Callbacks(),e)==Result::Okay);
        CHECK(e.finalWave&&!e.firstWave&&!e.hugeWarning&&e.spawnedWave==10&&e.spawnedCount==33);
        CHECK(s.wave==10&&s.hugeElapsed==0&&!s.hugeWarning&&s.countdown==30.f);
        for(size_t begin=0;begin<f.spawned.size();begin+=5)
        {std::set<uint8_t> rows;for(size_t i=begin;i<(std::min)(begin+5,f.spawned.size());++i)rows.insert(f.spawned[i].row);
         CHECK(rows.size()==(std::min)(size_t(5),f.spawned.size()-begin));}
        const auto first=f.calls.find('R'),last=f.calls.rfind('R'),spawn=f.calls.find('S');
        CHECK(first!=std::string::npos&&last<spawn&&f.calls.substr(first,last-first+1).find_first_not_of('R')==std::string::npos);
        f.ClearCalls();input.deltaSeconds=.01f;
        CHECK(UpdateFusionAdventureScheduler273(s,input,f.Callbacks(),e)==Result::Okay&&f.reals==0&&f.spawnCalls==0);
    }
    {
        Fixture f;FusionAdventureScheduler273State s;CHECK(f.Init(s)==Result::Okay);f.ClearCalls();
        s.wave=2;s.countdown=0;f.rowDraws={1,1,2,2,3,4,0};
        FusionAdventureScheduler273Events e;auto input=Playing(.01f);
        CHECK(UpdateFusionAdventureScheduler273(s,input,f.Callbacks(),e)==Result::Okay);
        const uint8_t expected[]={1,2,3,4,0};
        CHECK(f.spawned.size()==5&&f.integers==7);
        for(size_t i=0;i<f.spawned.size();++i)CHECK(f.spawned[i].row==expected[i]);
    }
    {
        const FusionAdventureScheduler273Health actors[]={{-3.9f,5,6,false},{2.8f,0,0,false},
            {std::numeric_limits<float>::quiet_NaN(),0,0,true}};
        float sum=99;CHECK(TrySumFusionAdventureHealth273(actors,sum)&&sum==10.f);
        const FusionAdventureScheduler273Health dying[]={{-99.f,0,0,false}};
        CHECK(TrySumFusionAdventureHealth273(dying,sum)&&sum==-99.f);
        const FusionAdventureScheduler273Health order[]={{16777216.f,0,0,false},{1.f,0,0,false},{-16777216.f,0,0,false}};
        CHECK(TrySumFusionAdventureHealth273(order,sum)&&sum==1.f);
        CHECK(TrySumFusionAdventureBirthHealth273(order,sum)&&sum==0.f);
        const FusionAdventureScheduler273Health wrapping[]={{2147483520.f,128,0,false}};
        CHECK(TrySumFusionAdventureHealth273(wrapping,sum)&&sum==-2147483648.f);
        const FusionAdventureScheduler273Health invalid[]={{std::numeric_limits<float>::infinity(),0,0,false}};
        sum=99;CHECK(!TrySumFusionAdventureHealth273(invalid,sum)&&sum==99);
    }
    for(int failure=0;failure<4;++failure)
    {
        Fixture f;FusionAdventureScheduler273State s;CHECK(f.Init(s)==Result::Okay);f.ClearCalls();s.countdown=0;
        f.capacity=failure!=0;f.spawnOkay=failure!=1;f.badReal=failure==2;
        if(failure==3)f.badInteger=true;
        FusionAdventureScheduler273Events e;const auto input=Playing(.01f);
        const Result expected[]={Result::NotReady,Result::SpawnFailed,Result::RandomFailed,Result::RandomFailed};
        CHECK(UpdateFusionAdventureScheduler273(s,input,f.Callbacks(),e)==expected[failure]);
        CHECK(s.failed&&s.wave==0&&e.spawnedCount==0);
        const int calls=f.spawnCalls;
        CHECK(UpdateFusionAdventureScheduler273(s,input,f.Callbacks(),e)==Result::Failed&&f.spawnCalls==calls);
        CHECK(!FusionAdventureScheduler273TutorOver(s));
        if(failure==0)CHECK(f.integers==0&&f.spawnCalls==0);
    }
    {
        Fixture f;FusionAdventureScheduler273State s;CHECK(f.Init(s)==Result::Okay);f.ClearCalls();
        s.healthElapsed=1.f;f.healthOkay=false;FusionAdventureScheduler273Events e;
        CHECK(UpdateFusionAdventureScheduler273(s,Playing(.01f),f.Callbacks(),e)==Result::HealthFailed&&s.failed);
        CHECK(f.reals==0&&f.spawnCalls==0);
        FusionAdventureScheduler273State invalid;CHECK(UpdateFusionAdventureScheduler273(invalid,Playing(.01f),f.Callbacks(),e)==Result::Invalid);
    }
    std::printf("[ %s ] Fusion.AdventureScheduler273 checks=%d failures=%d\n",result.mFailures?"FAILED":"OK",result.mChecks,result.mFailures);
    return result;
}
