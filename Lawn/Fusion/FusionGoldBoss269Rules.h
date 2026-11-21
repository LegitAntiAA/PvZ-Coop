#ifndef LAWN_FUSION_GOLD_BOSS269_RULES_H
#define LAWN_FUSION_GOLD_BOSS269_RULES_H
#include <cstdint>
namespace LawnFusion
{
// Source223 is UltimatePaperZombie:Zombie, NOT ZombieBoss or vanilla Zomboss.
struct FusionGoldBoss269Profile {uint16_t kind=223;int body=1500,firstArmor=12000,secondArmor=1500,bite=50;};
const FusionGoldBoss269Profile* FindFusionGoldBoss269Profile(uint16_t kind);
struct FusionGoldBoss269Armor {int status=4,first=12000,firstMaximum=12000,second=1500,secondMaximum=1500;float body=1500;};
struct FusionGoldBoss269Damage
{
    FusionGoldBoss269Armor after;int sourceType=0,damage=0;
    int bodyDamage=0;bool firstLost=false,secondLost=false,directDie=false,charred=false,cold=false;
};
// Pass Zombie.GetDamage's already resolved value; source223 caps that at5000.
// The shared raw override3563E0 allows base body damage ONLY in status6.
bool TryPlanFusionGoldBoss269Damage(const FusionGoldBoss269Armor&,int resolvedDamage,int sourceType,FusionGoldBoss269Damage&);
//203 inherits ordinary GetDamage (no223 cap), coerces toNormal only in4,
// and shares the status6-only body tail-jump to Zombie.BodyTakeDamage.
bool TryPlanFusionGoldPaper269Damage(uint16_t kind,const FusionGoldBoss269Armor&,int resolvedDamage,int sourceType,FusionGoldBoss269Damage&);
int FusionGoldBoss269ArmorStage(int remaining,int maximum,int previousStage);
int FusionGoldPaper269ArmorStage(uint16_t kind,int remaining,int maximum,int previousStage);
bool FusionGoldBoss269RejectStatus(int status,bool mindControl,bool knockback,bool freezeOrButter);
struct FusionGoldBoss269Attribute {int count=0;bool shoot=false,aimedShoot=false;};
bool TryStepFusionGoldBoss269Attribute(int status,int count,bool targetExists,FusionGoldBoss269Attribute&);
struct FusionGoldBoss269Shot {uint16_t kind=3;int damage=300;uint8_t way=9;bool hostile=true,aimed=false;float sourceYOffset=-.1f,velocityMultiplier=2;};
bool TryPlanFusionGoldBoss269Shot(bool aimed,bool friendly,bool towardsRight,bool dead,bool beforeDying,bool isChangingRow,FusionGoldBoss269Shot&);
struct FusionGoldBoss269Award {int sourceCoinType=0,sourceCoinCount=0,sourceItemType=0,sourceItemCount=0;uint16_t card=0xffff;int points=0;};
bool TryPlanFusionGoldBoss269Award(int awardLevel,FusionGoldBoss269Award&);
struct FusionGoldBoss269Health {float body=0;int bodyMaximum=0,first=0,firstMaximum=0,second=0,secondMaximum=0;};
bool TryScaleFusionGoldBoss269Health(float body,int first,int second,int survivalRound,int difficulty,FusionGoldBoss269Health&);
// Lawnf.CalculateProjectileParameters3FAF40 with identical stationary target
// samples at1/2s and source flightTime1.5. Output is native pixels per tick.
bool TryMakeFusionGoldBoss269AimedShot(float sourceX,float sourceY,float targetX,float targetY,
    float pixelsY,float& velocityX,float& velocityY,float& gravityY);
struct FusionGoldPaper269Rect {double x=0,y=0,width=0,height=0;};
bool TryMakeFusionGoldPaper269Collider(uint16_t kind,double rootX,double rootY,double nativeScale,
    bool right,double pixelsX,double pixelsY,FusionGoldPaper269Rect&);
}
#endif
