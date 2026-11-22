#include "FusionCampaign342Rules.h"
#include "FusionSourceZombieClosure280.h"
#include "FusionHeavyZombie313BRules.h"
#include "FusionJackbox300Rules.h"
#include <algorithm>
#include <limits>

namespace LawnFusion
{
namespace
{
void Add(std::vector<uint16_t>& out,uint16_t kind)
{if(std::find(out.begin(),out.end(),kind)==out.end())out.push_back(kind);}
bool WaterOnly(uint16_t k)
{switch(k){case 14:case 17:case 19:case 27:case 113:case 200:case 214:return true;default:return false;}}
bool WaterAllowed(uint16_t k)
{
    switch(k)
    {
    case 0:case 2:case 4:case 11:case 12:case 13:case 14:case 17:case 19:
    case 25:case 26:case 27:case 29:case 113:case 119:case 200:case 205:case 206:case 214:case 221:return true;
    default:return false;
    }
}
bool AvoidFirstRow(uint16_t k)
{switch(k){case 35:case 36:case 38:case 39:case 43:case 212:case 221:return true;default:return false;}}
bool Pick(const FusionAdventureScheduler273Random& random,std::span<const uint8_t> choices,uint8_t& out)
{
    if(choices.empty()||!random.integer)return false;
    const int draw=random.integer(random.user,0,int(choices.size()));
    if(draw<0||draw>=int(choices.size()))return false;out=choices[size_t(draw)];return true;
}
bool PlanValid(const FusionCampaign342Plan& p)
{
    if(!p.source||FindFusionSourceAdventure272(p.source->level)!=p.source)return false;
    const uint8_t water=p.source->scene==2||p.source->scene==3?uint8_t((1<<2)|(1<<3)):0;
    return p.waterRows==water&&p.night==(p.source->scene==1||p.source->scene==3)&&p.maximumWaves>0;
}
}
bool FusionCampaign342SpawnKinds(const FusionCampaign342Plan& p,uint16_t kind,std::vector<uint16_t>& out)
{
    if(!PlanValid(p))return false;std::vector<uint16_t> next{kind};
    if(p.waterRows&&(kind==0||kind==2||kind==4))Add(next,kind==0?11:kind==2?12:13);
    if(kind==18)Add(next,16); // Source no-ice sentinel35 changes SuperDriver to Driver.
    out=std::move(next);return true;
}
bool BuildFusionCampaign342Plan(int level,uint32_t cycle,FusionCampaign342Plan& out,std::string* error)
{
    if(error)error->clear();const auto reject=[&](const char* why){if(error)*error=why;return false;};
    const auto* s=FindFusionSourceAdventure272(level);int waves=0;
    if(!s||s->level!=level||s->scene>4||s->columns!=10||s->rows!=((s->scene==2||s->scene==3)?6:5)||
       !s->firstCycleWaves||s->initialSun<0||s->initialMoney<0||s->allowedZombieKinds.empty()||
       (s->tutorialId&&s->tutorialId!=s->level)||s->roofPreplantColumns>s->columns||
       !cycle||cycle>uint32_t((std::numeric_limits<int>::max)())||
       !TryGetFusionSourceAdventureWaves272(level,int(cycle),waves))return reject("descriptor: invalid source campaign descriptor/cycle.");
    FusionCampaign342Plan next;next.source=s;next.maximumWaves=waves;
    next.waterRows=s->scene==2||s->scene==3?uint8_t((1<<2)|(1<<3)):0;next.night=s->scene==1||s->scene==3;
    for(auto kind:s->allowedZombieKinds)
    {std::vector<uint16_t> variants;if(!FusionCampaign342SpawnKinds(next,kind,variants))return false;for(auto v:variants)Add(next.possibleKinds,v);}
    std::vector<uint16_t> fallback;if(!FusionCampaign342SpawnKinds(next,0,fallback))return false;
    for(auto v:fallback)Add(next.possibleKinds,v);out=std::move(next);return true;
}
bool NativeSourceAdventureLevelReady342(int level,uint32_t cycle,std::string* error)
{
    if(error)error->clear();
    const auto reject=[&](const char* why){if(error)*error=why;return false;};
    if(!FindFusionSourceAdventure272(level))return reject("descriptor: source campaign level is unavailable.");
    if(!cycle||cycle>uint32_t((std::numeric_limits<int>::max)()))return reject("cycle: invalid source campaign cycle.");
    FusionCampaign342Plan p;if(!BuildFusionCampaign342Plan(level,cycle,p,error))return false;
    if(p.source->conveyor&&level!=27&&level!=36&&level!=45)return reject("controller: source conveyor is unavailable.");
    if(p.source->roofPreplantColumns&&(p.source->scene!=4||p.source->roofPreplantColumns!=(level==37?5:3)))return reject("preplants: source roof descriptor differs.");
    switch(p.source->tutorialId)
    {case 0:case 1:case 4:case 7:case 8:case 10:case 13:case 15:case 22:case 24:case 30:case 35:case 42:break;
     default:return reject("tutorial: source lesson has no native implementation.");}
    std::vector<uint16_t> closure;std::string detail;
    if(!BuildFusionSourceZombieClosure280(p.possibleKinds,closure,&detail))
    {if(error)*error="npc: "+detail;return false;}
    return true;
}
bool ResolveFusionCampaign342Spawn(const FusionCampaign342Plan& p,uint16_t kind,int initialRow,
    std::span<const uint8_t> ice,const FusionAdventureScheduler273Random& random,FusionAdventureScheduler273Spawn& out)
{
    if(!PlanValid(p)||initialRow<0||initialRow>=p.source->rows||ice.size()!=p.source->rows||
       std::any_of(ice.begin(),ice.end(),[](uint8_t v){return v>1;}))return false;
    std::vector<uint8_t> land,water;
    for(uint8_t r=0;r<p.source->rows;++r)((p.waterRows&(1<<r))?water:land).push_back(r);
    uint8_t row=uint8_t(initialRow);const bool wet=(p.waterRows&(1<<row))!=0;
    if(wet&&!WaterAllowed(kind)){if(!Pick(random,land,row))return false;}
    else if(!wet&&WaterOnly(kind)){if(!Pick(random,water,row))return false;}
    if(AvoidFirstRow(kind))
    {
        // Source ordinary !isEndless branch repeats GetRandomLandRow. Bound a
        // broken/custom RNG without changing the valid source draw distribution.
        unsigned attempts=0;while(!row){if(++attempts>1024||!Pick(random,land,row))return false;}
    }
    else if((kind==26||kind==205)&&!row)
    {if(!random.integer)return false;const int draw=random.integer(random.user,1,p.source->rows);if(draw<1||draw>=p.source->rows)return false;row=uint8_t(draw);}
    uint16_t emitted=kind;
    if((p.waterRows&(1<<row))&&(kind==0||kind==2||kind==4))emitted=kind==0?11:kind==2?12:13;
    if(kind==18&&!ice[row])emitted=16;
    out={emitted,row};return true;
}
}
