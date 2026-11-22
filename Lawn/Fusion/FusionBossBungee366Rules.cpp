#include "FusionBossBungee366Rules.h"
namespace LawnFusion
{
bool BuildFusionBossBungee366RowPool(std::span<const FusionHeavy313BPlant> plants,int column,int count,std::vector<int>& out)
{
 if(column<1||column>3||count<1||count>6)return false;std::vector<int> rows;
 for(const auto& p:plants)if(p.alive&&p.column==column){if(p.row<0||p.row>=count)return false;rows.push_back(p.row);}
 out=std::move(rows);return true;
}
}
