#include "FusionCompanions269Rules.h"
#include "FusionNativeCompanions269.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace LawnFusion
{
namespace
{
const FusionCompanions269Profile profiles[]={
    {905,0,FusionCompanion269Family::LittleFly,0,0},
    {911,3,FusionCompanion269Family::UltimateLittleFly,10,30},
    {912,0,FusionCompanion269Family::LittleFly,0,0},
    {922,3,FusionCompanion269Family::CherryLittleFly,10,0},
    {1110,1,FusionCompanion269Family::LittleFly,10,30},
    {1169,0,FusionCompanion269Family::LittleFly,6,6},
    {1178,3,FusionCompanion269Family::CherryLittleFly,0,0}};
constexpr double events[]={double(.45f),double(.75f)};
const FusionShooterTimingProfile flyTiming{912,1.5f,events,double(1.2f),double(.9f),true,65535,"shoot"};
constexpr double littleEvents[]={double(.45f),double(.75f)},ultimateEvents[]={double(.5f)};
const FusionShooterTimingProfile auxiliaryTiming[]={
    {65535,1,littleEvents,double(1.2f),double(.9f),false,65535,"shoot"},
    {65535,1,ultimateEvents,double(1.2f),double(.6f),true,65535,"shoot"}};
bool Point(FusionCompanion269Point p){return std::isfinite(p.x)&&std::isfinite(p.y);}
}
const FusionCompanions269Profile* FindFusionCompanions269Profile(uint16_t kind)
{for(const auto& p:profiles)if(p.kind==kind)return &p;return nullptr;}
const FusionShooterTimingProfile* FindFusionCompanions269Timing(uint16_t kind)
{return kind==912?&flyTiming:nullptr;}
const FusionShooterTimingProfile* FindFusionCompanion269Animation(FusionCompanion269Family family)
{
    if(family==FusionCompanion269Family::LittleFly)return &auxiliaryTiming[0];
    return family==FusionCompanion269Family::UltimateLittleFly||family==FusionCompanion269Family::CherryLittleFly?&auxiliaryTiming[1]:nullptr;
}
bool FusionCompanion269CanTarget(FusionCompanion269Family family,int status,bool controlled,bool smallVariant)
{
    if(controlled||status<0||status>25)return false;
    if(family==FusionCompanion269Family::LittleFly)return status==10;
    if(family!=FusionCompanion269Family::UltimateLittleFly&&family!=FusionCompanion269Family::CherryLittleFly)return false;
    if(status==1||status==3||status==9||status==12||(status>=20&&status<=24))return false;
    return !smallVariant||(status!=7&&status!=10);
}
bool FusionCompanion269RayTarget(int status,bool controlled,bool smallVariant)
{return FusionCompanion269CanTarget(FusionCompanion269Family::UltimateLittleFly,status,controlled,smallVariant);}
bool TryStepFusionCompanion269Shoot(float before,float delta,bool onTarget,float& after,bool& trigger)
{
    if(!std::isfinite(before)||before<0||before>1||!std::isfinite(delta)||delta<0||delta>1)return false;
    float result=before;bool due=false;
    if(onTarget&&before>0){result-=delta;if(result<=0){result=1;due=true;}}
    after=result;trigger=due;return true;
}
bool TryMoveFusionCompanion269(FusionCompanion269Point current,FusionCompanion269Point target,float delta,FusionCompanion269Point& after)
{
    if(!Point(current)||!Point(target)||!std::isfinite(delta)||delta<0||delta>1)return false;
    const float dx=target.x-current.x,dy=target.y-current.y,distance=std::hypot(dx,dy),step=6*delta;
    if(!std::isfinite(distance))return false;
    after=distance==0||distance<=step?target:FusionCompanion269Point{current.x+dx/distance*step,current.y+dy/distance*step};return Point(after);
}
bool TryRotateFusionCompanion269(float current,float target,float delta,float& after)
{
    if(!std::isfinite(current)||!std::isfinite(target)||!std::isfinite(delta)||delta<0||delta>1)return false;
    constexpr float tau=6.2831853071795864769f;
    const float result=current+std::remainder(target-current,tau)*(std::min)(1.f,delta*5);
    if(!std::isfinite(result))return false;after=result;return true;
}
bool FusionCompanion269RayTouches(FusionCompanion269Point origin,float x,float y,float width,float height,float* distance)
{
    if(!Point(origin)||!Point({x,y})||!std::isfinite(width)||!std::isfinite(height)||width<0||height<0||!std::isfinite(x+width)||!std::isfinite(y+height))return false;
    // Exact source infinite RaycastAll direction(.9,-.4). Normalization cannot
    // change intersection membership or ascending hit order along the ray.
    float entry=0,exit=(std::numeric_limits<float>::max)();
    const auto slab=[&](float start,float direction,float low,float high)
    {float a=(low-start)/direction,b=(high-start)/direction;if(a>b)std::swap(a,b);entry=(std::max)(entry,a);exit=(std::min)(exit,b);return entry<=exit;};
    if(!slab(origin.x,.9f,x,x+width)||!slab(origin.y,-.4f,y,y+height))return false;
    if(distance)*distance=entry;return true;
}
int FusionCompanions269AttributeTicks(float seconds)
{
    if(!std::isfinite(seconds)||seconds<=0||seconds>30)return 0;
    int ticks=0;while(seconds>0){seconds-=.01f;++ticks;}return ticks;
}
int FusionCompanion269Damage(bool ultimate){return ultimate?600:200;}
int FusionSuperSun269IncomingDamage(bool common){return common?10:50;}
int FusionSuperSun269SummonCost(bool advanced){return advanced?200:500;}
bool NativeCompanion269StateValid(const NativeCompanion269State& v)
{
    if(static_cast<unsigned>(v.family)>static_cast<unsigned>(FusionCompanion269Family::CherryLittleFly)||
       !v.instanceToken||!v.parentId||!v.parentToken||((v.targetId==0)!=(v.targetToken==0))||
       !std::isfinite(v.offsetX)||v.offsetX<-.1f||v.offsetX>.1f||!std::isfinite(v.offsetY)||v.offsetY<-.1f||v.offsetY>.1f||
       !std::isfinite(v.positionOffset)||v.positionOffset<0||v.positionOffset>.6f||
       !std::isfinite(v.shootCountdown)||v.shootCountdown<0||v.shootCountdown>1||v.ageTicks<0||
       v.shootStartAge< -1||v.shootStartAge>v.ageTicks||(v.shootStartAge<0&&v.shotEmitted))return false;
    return !v.smallVariant||v.family!=FusionCompanion269Family::LittleFly;
}
bool NativeCompanions269PlantStateValid(uint16_t kind,const NativeCompanions269PlantState& v)
{
    const auto* p=FindFusionCompanions269Profile(kind);if(!p||v.children.size()!=static_cast<size_t>(p->childSlots))return false;
    for(size_t i=0;i<v.children.size();++i)
    {
        const auto ref=v.children[i];if((ref.id==0)!=(ref.token==0))return false;
        if(ref.id)for(size_t j=0;j<i;++j)if(v.children[j].id==ref.id||v.children[j].token==ref.token)return false;
    }
    return true; // recreateDeadlines is host-only and is never decoded from FNS.
}
}
