#include "FusionSourceDeathBudget319.h"
#include "FusionGoldBoss269Rules.h"
#include <array>
#include <limits>
namespace LawnFusion
{
namespace
{
constexpr std::array<uint64_t FusionSourceDeathBudget319::*,8> fields={&FusionSourceDeathBudget319::zombies,&FusionSourceDeathBudget319::projectiles,&FusionSourceDeathBudget319::reanimations,&FusionSourceDeathBudget319::attachments,&FusionSourceDeathBudget319::coins,&FusionSourceDeathBudget319::droppedCards,&FusionSourceDeathBudget319::particleSystems,&FusionSourceDeathBudget319::emitters};
}
bool AddFusionSourceDeathBudget319(const FusionSourceDeathBudget319& a,const FusionSourceDeathBudget319& b,FusionSourceDeathBudget319& out)
{auto sum=a;for(auto f:fields){if(b.*f>(std::numeric_limits<uint64_t>::max)()-a.*f)return false;sum.*f+=b.*f;}out=sum;return true;}
bool FusionSourceDeathBudgetFits319(const FusionSourceDeathBudget319& used,const FusionSourceDeathBudget319& peak,const FusionSourceDeathBudget319& capacity)
{for(auto f:fields)if(used.*f>capacity.*f||peak.*f>capacity.*f-used.*f)return false;return true;}
bool PlanFusionSourceDeathLeaves319(const FusionSourceDeathPending319& p,FusionSourceDeathBudget319& out)
{
 if(p.reason<0||p.reason>2||p.machineType>2||p.awardLevel>2)return false;
 FusionSourceDeathBudget319 peak;
 if(p.kind==40&&!p.consumed)
 {
  if(!p.machineType)peak.coins=1;
  else if(!p.changingRow){peak.zombies=5;peak.reanimations=5;if(!p.controlled){peak.projectiles=1;peak.reanimations++;peak.attachments=1;}}
 }
 if(p.kind==30&&!p.consumed&&p.reason==0&&p.boxPresent&&p.countdownStarted){peak.zombies=1;peak.reanimations=1;}
 if(p.kind==203||p.kind==223)
 {
  if(p.kind==223&&!p.deathCommitted)peak.coins=peak.reanimations=peak.attachments=1;
  if(p.awardLevel&&!p.rewardCommitted)
  {
   FusionGoldBoss269Award award;if(!TryPlanFusionGoldBoss269Award(p.awardLevel,award))return false;
   const auto animated=uint64_t(award.sourceCoinCount)+uint64_t(award.sourceItemCount);
   peak.coins+=animated;peak.reanimations+=animated;peak.attachments+=animated;
   if(award.card!=0xffff){peak.coins++;peak.droppedCards=1;}
  }
 }
 out=peak;return true;
}
}
