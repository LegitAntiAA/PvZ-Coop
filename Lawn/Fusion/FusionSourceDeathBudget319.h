#pragma once
#include <cstdint>
namespace LawnFusion
{
// Whole synchronous operation's mandatory native peak. No parent/dead object
// releases are credited until its real DataArray/holder actually frees them.
struct FusionSourceDeathBudget319
{
    uint64_t zombies=0,projectiles=0,reanimations=0,attachments=0,coins=0,droppedCards=0,particleSystems=0,emitters=0;
    bool operator==(const FusionSourceDeathBudget319&)const=default;
};
bool AddFusionSourceDeathBudget319(const FusionSourceDeathBudget319&,const FusionSourceDeathBudget319&,FusionSourceDeathBudget319&);
bool FusionSourceDeathBudgetFits319(const FusionSourceDeathBudget319& used,const FusionSourceDeathBudget319& peak,const FusionSourceDeathBudget319& capacity);
// Source-owned mandatory callback leaves only. Cosmetic acknowledged drops and
// already committed children provide neither a new cost nor deletion credit.
struct FusionSourceDeathPending319
{
 uint16_t kind=0;int reason=2;bool consumed=false,controlled=false,changingRow=false;
 uint8_t machineType=0,awardLevel=0;bool pointsCommitted=false,deathCommitted=false,rewardCommitted=false;
 bool boxPresent=false,countdownStarted=false;
};
bool PlanFusionSourceDeathLeaves319(const FusionSourceDeathPending319&,FusionSourceDeathBudget319& out);
}
