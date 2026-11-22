#include "FusionExtendedZombie313Rules.h"
#include "FusionExtendedZombie347Rules.h"
#include "FusionNextZombie343Rules.h"
#include "FusionGroundShooter343Rules.h"
#include "FusionSnowZombie363Rules.h"
#include <algorithm>
#include <cmath>
namespace LawnFusion
{
bool FusionExtendedZombie313ViewValid(uint16_t kind,const FusionExtendedZombie313View& v)
{
    if(v.secondArmorStage343>2||(kind!=114&&v.secondArmorStage343))return false;
    if(FusionExtendedZombie347Kind(kind))return FusionExtended347ViewValid(kind,v);
    if(v.attributeCountdown||v.throwTime||v.isThrow||v.attackDamage)return false;
    if(!FindFusionExtendedZombie313Profile(kind)||v.bodyStage>2||v.weapon>2||v.machineType>2||v.firstArmorStage>2||
        !std::isfinite(v.impVerticalSpeed)||std::abs(v.impVerticalSpeed)>4096||!std::isfinite(v.impMaximumVx)||(v.impMaximumVx!= -3&&v.impMaximumVx!= -2)||
        !std::isfinite(v.uniqueSpeed)||v.uniqueSpeed<=0||v.uniqueSpeed>4096)return false;
    if(v.sourceStatus!=0&&v.sourceStatus!=1)
    {if((kind==35||kind==36||kind==43||kind==212)?v.sourceStatus!=15:
        kind==37?v.sourceStatus!=16&&v.sourceStatus!=17:kind==213?v.sourceStatus!=16:(FusionBalloon343Kind(kind)||kind==26)?v.sourceStatus!=10:true)return false;}
    if(kind!=35&&kind!=36&&(v.weapon||v.carriedImpRemoved))return false;
    if(kind!=43&&kind!=212&&v.ballRemoved)return false;
    if(kind!=26&&v.bombPresent)return false;
    if(kind!=40&&(v.machineType||v.uniqueSpeed!=1))return false;
    if(kind!=29&&kind!=43&&kind!=212&&kind!=114&&kind!=118&&kind!=48&&v.firstArmorStage)return false;
    if(kind==38||kind==39){if(v.uniqueType!=0&&v.uniqueType!=(kind==38?4:5))return false;}else if(v.uniqueType)return false;
    if(!FusionBalloon343Kind(kind)&&v.fallen)return false;
    if(kind!=37&&(!v.visible||v.adjustingImp||v.impVerticalSpeed!=1||v.impMaximumVx!= -3))return false;
    if(FusionExtendedZombie313OwnBodyDamage(kind)&&v.beforeDying)return false;
    return true;
}
namespace
{
constexpr FusionZombieProfile profiles[]={
 {35,3000,0,0,50,true,false,true},{36,6000,0,0,50,true,false,true},{38,3000,0,0,50,true,false,true},{39,6000,0,0,50,true,false,true},
 {43,8000,24000,0,50,true,false,true},{37,270,0,0,50,true},{40,1350,0,0,50,true,false,true},
 {212,8000,24000,0,50,true,false,true},{213,1500,0,0,200,true},
 {25,270,20,0,50,true},{29,270,500,0,50,true},{26,1350,0,0,500,true,false,true}};
bool Garg(uint16_t k){return k==35||k==36||k==43||k==212;}
bool Iron(uint16_t k){return k==38||k==39;}
uint8_t GargBody(float hp,int maximum,uint8_t previous)
{
    if(hp<float(maximum)/3)return 2;
    if(float(maximum)*.33333334f<hp&&hp<=float(maximum)*.6666667f)return 1;
    return previous;
}
}
const FusionZombieProfile* FindFusionExtendedZombie313Profile(uint16_t k)
{for(const auto& p:profiles)if(p.kind==k)return &p;return FusionGroundShooter343Kind(k)||FusionSnowZombie363Kind(k)?FindFusionNextZombie343Profile(k):FindFusionExtendedZombie347Profile(k);}
bool TryGetFusionExtendedZombie313Family(uint16_t k,FusionExtendedZombie313Family& output)
{
    FusionExtendedZombie313Family family;
    if(Garg(k))family=FusionExtendedZombie313Family::Gargantuar;
    else if(Iron(k))family=FusionExtendedZombie313Family::IronGargantuar;
    else if(k==37||k==213)family=FusionExtendedZombie313Family::Imp;
    else if(k==40)family=FusionExtendedZombie313Family::Machine;
    else if(FusionBalloon343Kind(k)||k==26)family=FusionExtendedZombie313Family::Flying;
    else return false;
    output=family;return true;
}
bool FusionExtendedZombie313OwnBodyDamage(uint16_t k){return Garg(k)||Iron(k)||k==40||k==26||FusionExtended347OwnBody(k);}
bool FusionExtendedZombie313BaseLimbCallbacks(uint16_t k){return k==37||k==213||FusionBalloon343Kind(k)||FusionGroundShooter343Kind(k)||FusionSnowZombie363Kind(k)||FusionExtended347BaseLimbs(k);}
bool TryPlanFusionExtendedEffect313(uint16_t k,int status,FusionExtendedEffect313 effect,float seconds,FusionExtendedEffectAction313& output,float& resulting)
{
    if(FusionExtendedZombie347Kind(k))return TryPlanFusionExtended347Effect(k,status,effect,seconds,output,resulting);
    if(!FindFusionExtendedZombie313Profile(k)||status<0||uint8_t(effect)>uint8_t(FusionExtendedEffect313::MindControl)||!std::isfinite(seconds))return false;
    auto action=FusionExtendedEffectAction313::Normal;float duration=seconds;
    if(Iron(k))
    {
        if(effect==FusionExtendedEffect313::MindControl)action=FusionExtendedEffectAction313::CustomMindControl;
        else if(effect!=FusionExtendedEffect313::Knockback)action=FusionExtendedEffectAction313::Suppress;
    }
    if(k==43||k==212)
    {
        if(effect==FusionExtendedEffect313::MindControl||effect==FusionExtendedEffect313::Knockback)action=FusionExtendedEffectAction313::Suppress;
        if((effect==FusionExtendedEffect313::Cold||effect==FusionExtendedEffect313::Freeze||effect==FusionExtendedEffect313::Butter)&&seconds>0)duration=seconds*.5f;
    }
    if(k==40&&(effect==FusionExtendedEffect313::Butter||effect==FusionExtendedEffect313::Poison||effect==FusionExtendedEffect313::Garlic))action=FusionExtendedEffectAction313::Suppress;
    if(status==16&&((k==37&&(effect==FusionExtendedEffect313::Freeze||effect==FusionExtendedEffect313::Butter))||
       (k==213&&effect==FusionExtendedEffect313::Freeze)))action=FusionExtendedEffectAction313::Suppress;
    if(effect==FusionExtendedEffect313::Freeze&&(k==26||(FusionBalloon343Kind(k)&&status==10)))
    {action=FusionExtendedEffectAction313::ColdInstead;duration=seconds+10;}
    if(!std::isfinite(duration))return false;output=action;resulting=duration;return true;
}
bool TryCapFusionExtendedZombie313Damage(uint16_t k,int incoming,int& output)
{
    if(!FindFusionExtendedZombie313Profile(k)||incoming<0)return false;
    output=(k==43||k==212)?(std::min)(incoming,5000):incoming;return true;
}
bool TryPlanFusionExtendedBody313(uint16_t k,float hp,int maximum,int damage,uint8_t previous,uint8_t unique,int status,bool controlled,float shadowX,FusionExtendedBody313Plan& output)
{
    if(FusionExtendedZombie347Kind(k))return TryPlanFusionExtended347Body(k,hp,maximum,damage,previous,status,output);
    if(!FusionExtendedZombie313OwnBodyDamage(k)||!std::isfinite(hp)||maximum<=0||hp>float(maximum)||damage<0||previous>2||status<0||!std::isfinite(shadowX))return false;
    FusionExtendedBody313Plan plan;plan.health=hp-float(damage);plan.bodyStage=previous;plan.uniqueType=unique;plan.sourceStatus=status;
    if(!std::isfinite(plan.health))return false;
    if(Garg(k))
    {
        plan.bodyStage=GargBody(plan.health,maximum,previous);
        if(plan.health<float(maximum)*.5f&&status==15&&shadowX>3&&!controlled)
        {plan.sourceStatus=0;plan.triggerThrow=true;}
    }
    else if(Iron(k))
    {
        if(unique!=0&&unique!=4&&unique!=5)return false;
        if(unique==4||unique==5)
        {
            plan.bodyStage=GargBody(plan.health,maximum,previous);
            if(plan.health<float(maximum)/3){plan.removeIronHead=true;plan.uniqueType=0;}
        }
    }
    else if(k==26)
    {
        if(plan.health<float(maximum)/3)plan.bodyStage=2;
        else if(plan.health>=float(maximum)/3&&plan.health<float(int32_t(uint32_t(maximum)*2U))/3)plan.bodyStage=1;
    }
    else if(k==40)
    {
        if(plan.health<float(maximum)/3)plan.bodyStage=2;
        else if(float(maximum)/3<plan.health&&plan.health<=float(int32_t(uint32_t(maximum)*2U))/3)plan.bodyStage=1;
    }
    output=plan;return true;
}
bool TryPlanFusionFlightFall313(uint16_t k,bool fallen,bool water,int maximum,FusionFlightArmor313Plan& output)
{
    if(!FusionBalloon343Kind(k)||maximum<0)return false;
    FusionFlightArmor313Plan plan;plan.maximum=maximum;plan.fallen=fallen;plan.sourceStatus=fallen?0:10;
    if(fallen){output=plan;return true;}
    plan.fallen=true;plan.fallTriggered=true;plan.sourceStatus=0;plan.die2=water;
    if(!water&&k==29){plan.health=plan.maximum=2200;plan.sourceArmorType=8;}
    else if(!water){plan.clearArmor=true;plan.sourceArmorType=0;}
    output=plan;return true;
}
bool TryPlanFusionFlightArmor313(uint16_t k,int status,bool fallen,bool water,int hp,int maximum,int damage,FusionFlightArmor313Plan& output)
{
    if(!FusionBalloon343Kind(k)||status<0||hp<0||maximum<hp||damage<0)return false;
    FusionFlightArmor313Plan plan;plan.health=hp;plan.maximum=maximum;plan.fallen=fallen;plan.sourceStatus=status;
    plan.sourceArmorType=status==10?(k==29||k==119?10:9):(k==29?8:0);
    if(status==10)
    {
        if(hp<=damage){if(!TryPlanFusionFlightFall313(k,fallen,water,maximum,plan))return false;}
        else plan.health=hp-damage;
        plan.overflow=0; // Source flying first armor consumes the entire hit.
    }
    else if(k==29)
    {
        if(hp<=damage){plan.health=0;plan.clearArmor=true;plan.overflow=damage-hp;}
        else {plan.health=hp-damage;plan.sourceArmorType=8;}
    }
    else {plan.health=0;plan.clearArmor=true;plan.overflow=damage;}
    output=plan;return true;
}
bool TryNormalizeFusionExtendedDeath313(uint16_t k,int reason,int& output)
{
    if(FusionExtendedZombie347Kind(k))return TryNormalizeFusionExtended347Death(k,reason,output);
    if(!FindFusionExtendedZombie313Profile(k)||reason<0)return false;
    output=k==40?(reason==1?1:2):k==26&&reason==0?2:reason;return true;
}
bool TryGetFusionGarg313SpawnPlan(uint16_t k,bool hammer,FusionGarg313SpawnPlan& output)
{
    if(!Garg(k))return false;FusionGarg313SpawnPlan plan;
    if(hammer)
    {
        if(k!=43&&k!=212)return false;
        plan.childKind=k==212?213:37;plan.minimumCount=1;plan.maximumCount=3;
        plan.rootOffsetX=-3;plan.rootOffsetY=1;plan.jitterMin=-1;plan.jitterMax=1;
    }
    else if(k==35||k==36)
    {plan.childKind=37;plan.minimumCount=plan.maximumCount=1;plan.thrown=true;plan.defaultFactoryXBeforeReposition=true;}
    else return false; //43/212 throw a genuine separate ball, not a fake direct NPC.
    output=plan;return true;
}
bool FusionGarg313Target313(int status,bool row,bool opposite)
{return status>=0&&status!=10&&status!=12&&row&&opposite;}
bool FusionKirov313Target313(int status,bool row,bool hostile)
{return status>=0&&status!=1&&status!=10&&status!=12&&!(status>=20&&status<=24)&&row&&hostile;}
bool TryGetFusionKirov313Velocity(float speed,bool right,bool travel,bool slope,float rootX,float& x,float& y)
{
    if(!std::isfinite(speed)||!std::isfinite(rootX))return false;
    const float factor=travel?.5f:.1f;const float direction=right?1.f:-1.f;
    const float vx=direction*factor*speed,vy=slope&&rootX<1.5f?direction*factor*.22f*speed:0;
    if(!std::isfinite(vx)||!std::isfinite(vy))return false;x=vx;y=vy;return true;
}
bool TryStepFusionImp313Flight(float currentVx,float currentVy,float maximumVx,float y,float land,bool beforeDying,float dt,FusionImp313FlightStep& output)
{
    if(!std::isfinite(currentVx)||!std::isfinite(currentVy)||!std::isfinite(maximumVx)||!std::isfinite(y)||!std::isfinite(land)||!std::isfinite(dt)||dt<0)return false;
    FusionImp313FlightStep next{currentVx,currentVy,false};
    if(!beforeDying)
    {if(y>land){next.vx=maximumVx;next.vy=currentVy-dt*4.5f;}else next.triggerLand=true;}
    if(!std::isfinite(next.vy))return false;output=next;return true;
}
}
