#include "FusionCampaign342Tests.h"
#include "../../Lawn/Fusion/FusionCampaign342Rules.h"
#include "../../Lawn/Fusion/FusionZombieRules.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>

using namespace LawnFusion;
namespace
{
struct Random
{
    uint32_t value=12345;int calls=0;bool bad=false;
    static int Integer(void* user,int minimum,int maximum)
    {
        auto& r=*static_cast<Random*>(user);++r.calls;if(r.bad)return maximum;
        r.value=r.value*1664525u+1013904223u;return minimum+int(r.value%uint32_t(maximum-minimum));
    }
    static float Real(void* user,float minimum,float maximum)
    {auto& r=*static_cast<Random*>(user);++r.calls;r.value=r.value*1664525u+1013904223u;return minimum+(maximum-minimum)*float(r.value&65535)/65535.f;}
    FusionAdventureScheduler273Random API(){return {this,Integer,Real};}
};
bool AllReady(void*,uint16_t){return true;}
int PickFirst(void*,int minimum,int){return minimum;}
int PickLast(void*,int,int maximum){return maximum-1;}
}
FusionCampaign342TestResults RunFusionCampaign342Tests()
{
    FusionCampaign342TestResults result;
#define CHECK(value) do {++result.mChecks;if(!(value)){++result.mFailures;std::printf("Campaign342 line%d failed: %s\n",__LINE__,#value);}}while(0)
    std::array<int,45> supported{};for(int i=0;i<45;++i)supported[size_t(i)]=i+1;
    int totalWaves=0,conveyors=0,preplants=0;
    for(int level=1;level<=45;++level)
    {
        FusionCampaign342Plan p;CHECK(BuildFusionCampaign342Plan(level,1,p));
        CHECK(p.source==FindFusionSourceAdventure272(level)&&p.source->level==level);
        const int scene=(level-1)/9;CHECK(p.source->scene==scene);
        CHECK(p.source->rows==(scene==2||scene==3?6:5)&&p.source->columns==10);
        CHECK(p.waterRows==(scene==2||scene==3?12:0));CHECK(p.night==(scene==1||scene==3));
        CHECK(p.source->conveyor==(level==27||level==36||level==45));conveyors+=p.source->conveyor;
        preplants+=p.source->rows*p.source->roofPreplantColumns;
        CHECK(p.source->initialMoney==3000);
        CHECK(p.source->initialSun==(scene==0?500:scene==1||scene==3?150:300));
        CHECK(std::find(p.possibleKinds.begin(),p.possibleKinds.end(),0)!=p.possibleKinds.end());
        for(auto kind:p.source->allowedZombieKinds)
            CHECK(std::find(p.possibleKinds.begin(),p.possibleKinds.end(),kind)!=p.possibleKinds.end());
        std::string reason;
        CHECK(NativeSourceAdventureLevelReady280(level,1,&reason)==
            (std::find(supported.begin(),supported.end(),level)!=supported.end()));
        CHECK(reason.empty()==NativeSourceAdventureLevelReady280(level,1));
        CHECK(!NativeSourceAdventureLevelReady280(level,0,&reason)&&reason.starts_with("cycle:"));
        CHECK(NativeSourceAdventureLevelReady280(level,2,&reason));
        FusionCampaign342Plan repeated;CHECK(BuildFusionCampaign342Plan(level,2,repeated,&reason));
        CHECK(repeated.maximumWaves==p.maximumWaves+10&&repeated.possibleKinds==p.possibleKinds);
        std::vector<uint8_t> ice(p.source->rows,0);
        for(auto kind:p.source->allowedZombieKinds)for(int row=0;row<p.source->rows;++row)
        {
            Random random;FusionAdventureScheduler273Spawn birth{65535,255};
            CHECK(ResolveFusionCampaign342Spawn(p,kind,row,ice,random.API(),birth));
            CHECK(birth.row<p.source->rows);
            CHECK(std::find(p.possibleKinds.begin(),p.possibleKinds.end(),birth.kind)!=p.possibleKinds.end());
            const bool water=(p.waterRows&(1<<birth.row))!=0;
            if(water)CHECK(birth.kind==11||birth.kind==12||birth.kind==13||birth.kind==14||
                birth.kind==17||birth.kind==19||birth.kind==25||birth.kind==26||birth.kind==27||birth.kind==29);
            if(kind==14||kind==17||kind==19||kind==27)CHECK(water);
            if(kind==18)CHECK(!water&&birth.kind==16);
            if(kind==35||kind==38||kind==40||kind==45||kind==31||kind==32||kind==33||kind==34)
                CHECK(!water);
            if(kind==35||kind==38)CHECK(birth.row!=0);
        }
        // Execute the real shared wave generator for all descriptors and all6
        // source difficulties: exact wave count/final tail, no registry filtering.
        for(int difficulty=0;difficulty<6;++difficulty)
        {
            Random random;FusionAdventureScheduler273State scheduler;
            CHECK(InitializeFusionAdventureScheduler273(scheduler,true,level,1,difficulty,random.API(),nullptr,AllReady)==FusionAdventureScheduler273Result::Okay);
            CHECK(scheduler.maximumWaves==p.maximumWaves&&scheduler.rows==p.source->rows);
            CHECK(scheduler.waves.size()==size_t(p.maximumWaves));
            const auto& final=scheduler.waves.back();CHECK(final.size()>=p.source->allowedZombieKinds.size());
            CHECK(std::equal(p.source->allowedZombieKinds.begin(),p.source->allowedZombieKinds.end(),final.end()-p.source->allowedZombieKinds.size()));
            for(const auto& wave:scheduler.waves)for(auto kind:wave)
                CHECK(kind==0||FusionSourceAdventureAllowsZombie272(level,kind));
            totalWaves+=scheduler.maximumWaves;
        }
    }
    CHECK(conveyors==3&&preplants==145);CHECK(totalWaves>0);
    FusionCampaign342Plan pool;CHECK(BuildFusionCampaign342Plan(26,1,pool));
    std::vector<uint8_t> ice(6,0);FusionAdventureScheduler273Spawn out{65432,254};Random bad;bad.bad=true;
    CHECK(!ResolveFusionCampaign342Spawn(pool,20,2,ice,bad.API(),out)&&out.kind==65432&&out.row==254&&bad.calls==1);
    CHECK(!ResolveFusionCampaign342Spawn(pool,0,-1,ice,bad.API(),out)&&out.kind==65432);
    ice[0]=2;CHECK(!ResolveFusionCampaign342Spawn(pool,18,0,ice,bad.API(),out));ice[0]=1;
    FusionAdventureScheduler273Random last{nullptr,PickLast,nullptr};
    CHECK(ResolveFusionCampaign342Spawn(pool,18,0,ice,last,out)&&out.kind==18&&out.row==0);
    ice[0]=0;CHECK(ResolveFusionCampaign342Spawn(pool,18,0,ice,last,out)&&out.kind==16);
    for(auto [kind,expected]:std::array<std::array<uint16_t,2>,3>{{{0,11},{2,12},{4,13}}})
    {CHECK(ResolveFusionCampaign342Spawn(pool,kind,2,ice,last,out)&&out.kind==expected&&out.row==2);}
    FusionCampaign342Plan roof;CHECK(BuildFusionCampaign342Plan(43,1,roof));
    std::vector<uint8_t> dry(5,0);FusionAdventureScheduler273Random first{nullptr,PickFirst,nullptr};out={65432,254};
    CHECK(!ResolveFusionCampaign342Spawn(roof,35,0,dry,first,out)&&out.kind==65432&&out.row==254);
    FusionCampaign342Plan untouched=pool;CHECK(!BuildFusionCampaign342Plan(46,1,untouched)&&untouched.source==pool.source&&untouched.possibleKinds==pool.possibleKinds);
    CHECK(!BuildFusionCampaign342Plan(1,0,untouched)&&untouched.source==pool.source);
    CHECK(!BuildFusionCampaign342Plan(1,(std::numeric_limits<uint32_t>::max)(),untouched));
#undef CHECK
    return result;
}
