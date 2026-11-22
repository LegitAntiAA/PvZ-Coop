#pragma once
#include "FusionZombieRules.h"
#include "FusionExtendedZombie313Rules.h"
#include "FusionSourceGraph313.h"
#include <vector>
#include <span>
namespace LawnFusion
{
const FusionZombieProfile* FindFusionExtendedZombie347Profile(uint16_t);
bool FusionExtendedZombie347Kind(uint16_t);
std::span<const uint16_t> FusionExtended347Dependencies(uint16_t);
enum class FusionExtended347Family:uint8_t {Football,Airship,Machine,Spider,Jackbox};
bool TryGetFusionExtended347Family(uint16_t,FusionExtended347Family&);
bool FusionExtended347ViewValid(uint16_t,const FusionExtendedZombie313View&);
bool FusionExtended347OwnBody(uint16_t);
bool FusionExtended347BaseLimbs(uint16_t);
bool TryPlanFusionExtended347Effect(uint16_t,int,FusionExtendedEffect313,float,FusionExtendedEffectAction313&,float&);
// Virtual GetDamage order is source-specific:220/224 cap RAW before base;
//218/221 cap RESOLVED after base;219 also consumes its actual .2s source timer.
struct FusionExtended347DamagePlan {int raw=0,maximumResolved=0x7fffffff;float attributeCountdown=0;bool ignored=false;};
bool TryPlanFusionExtended347Damage(uint16_t,int status,bool controlled,int raw,float attributeCountdown,FusionExtended347DamagePlan&);
bool TryPlanFusionExtended347Body(uint16_t,float health,int maximum,int damage,uint8_t previousBodyStage,int status,FusionExtendedBody313Plan&);
uint8_t FusionExtended347ArmorStage(uint16_t,int health,int maximum,uint8_t previous);
struct FusionExtended347Birth {uint16_t kind=0;int row=0,column=-1;float sourceX=9.9f;bool controlled=false,popImmediately=false;};
// Genuine complete row/cell loops; never filter a source row out by terrain.
bool PlanFusionExtended347Births(uint16_t,int sourceStatus,int sourceLevel,int rows,int columns,bool controlled,float axisX,std::vector<FusionExtended347Birth>&);
struct FusionExtended347AttributePlan {float seconds=0,originSpeed=0;int attackDamage=0,firstArmorHealth=0;bool pop=false,shoot=false;};
bool TryPlanFusionExtended347Attribute(uint16_t,int status,float originSpeed,int attackDamage,int firstArmorHealth,int firstArmorMaximum,FusionExtended347AttributePlan&);
bool TryNormalizeFusionExtended347Death(uint16_t,int sourceReason,int&);
bool FusionSpider347HitsGround(float sourceY,float landY);
const FusionSourceGraph313* FindFusionSourceGraph347(uint16_t);
bool SourceRootDelta347(uint16_t,uint8_t,FusionSourceGraph313Cursor,FusionSourceGraph313Cursor,double&);
}
