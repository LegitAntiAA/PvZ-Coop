#include "FusionGoldConversion313B.h"
#include "FusionSourceZombieTypes313.h"
#include <cmath>
#include <unordered_set>
namespace LawnFusion
{
bool BuildFusionGoldConversion313B(std::span<const FusionGoldConversion313BActor> ordered,
    std::vector<FusionGoldConversion313BActor>& output)
{
    std::vector<FusionGoldConversion313BActor> result;result.reserve(30);
    std::unordered_set<uint32_t> seen;
    for(const auto& actor:ordered)if(actor.exists&&(!actor.localUID||!std::isfinite(actor.axisX)||
        actor.row>5||actor.rowType>2||!seen.insert(actor.localUID).second))return false;
    for(auto i=ordered.rbegin();i!=ordered.rend()&&result.size()<30;++i)
    {
        const auto& actor=*i;
        if(!actor.exists||actor.kind==203||actor.rowType==1||FusionSourceZombieIsAir313(actor.kind)||
            FusionSourceZombieIsDriver313(actor.kind)||FusionSourceZombieIsGargantuar313(actor.kind))continue;
        result.push_back(actor);
    }
    output.swap(result);return true;
}
}
