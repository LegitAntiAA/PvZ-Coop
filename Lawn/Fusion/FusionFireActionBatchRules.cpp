#include "FusionFireActionBatchRules.h"
#include "../../ConstEnums.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace LawnFusion
{
namespace
{
constexpr FusionFireActionProfile kProfiles[] = {
    {20,FusionFireActionFamily::Light,300,true},
    {22,FusionFireActionFamily::Blover,300,true},
    {30,FusionFireActionFamily::Light,1000,false}, // Hostile source adapter deferred.
    {1052,FusionFireActionFamily::Torch,300,true},
    {1053,FusionFireActionFamily::Torch,300,true},
    {1055,FusionFireActionFamily::Torch,300,true},
    {1059,FusionFireActionFamily::Torch,300,true},
    {902,FusionFireActionFamily::Torch,300,true},
    {932,FusionFireActionFamily::Torch,300,true},
    {1061,FusionFireActionFamily::FireGround,300,true},
    {1062,FusionFireActionFamily::Caltrop,300,true},
    {1063,FusionFireActionFamily::Caltrop,300,true},
    {1064,FusionFireActionFamily::Caltrop,300,true},
    {1080,FusionFireActionFamily::Blover,300,true},
    {1091,FusionFireActionFamily::BloverShell,4000,true},
    {1173,FusionFireActionFamily::Obsidian,32000,true},
    {1178,FusionFireActionFamily::Torch,300,false}, // Real CherryLittleFly dependency.
    {1179,FusionFireActionFamily::CherryRunner,300,true},
    {1182,FusionFireActionFamily::Torch,300,true},
};
}
const FusionFireActionProfile* FindFusionFireActionProfile(uint16_t kind)
{
    for (const auto& p:kProfiles) if (p.kind==kind) return &p;
    return nullptr;
}
bool IsFusionFireActionSupported(uint16_t kind)
{ const auto* p=FindFusionFireActionProfile(kind);return p&&p->enabled; }
bool FusionFireActionInvincible(uint16_t kind) { return kind==22||kind==1080||kind==1179; }
bool FusionFireTorchKind(uint16_t kind)
{
    return kind==18||kind==902||kind==1052||kind==1053||kind==1055||kind==1059||kind==1061||kind==1178||kind==1182;
}
bool FusionFireActionProjectileMetadataValid(uint16_t kind,int damage,int sun,uint8_t special,
    uint8_t fire,int type,int motion)
{
    if(sun||fire||type!=PROJECTILE_PEA)return false;
    if(kind==12)return damage==5&&((!special&&motion==MOTION_STRAIGHT)||
        ((special==3||special==4)&&motion==MOTION_THREEPEATER));
    return kind==28&&damage==40&&((!special&&motion==MOTION_STRAIGHT)||
        (special==1&&motion==MOTION_LOBBED));
}
float FusionFireActionMaximumDuration(uint16_t kind,float speed)
{
    if(!std::isfinite(speed)||speed<=0||!IsFusionFireActionSupported(kind))return 0;
    if(kind==22||kind==1080)return 2.0f+.48333332f/speed+.02f;
    if(kind==1064)return 1.0f/speed+.02f;
    if(kind==1173)return .6666667f/(.8f*speed)+.02f;
    if(kind==1179)return 20.0f;
    return 0;
}
bool TryGetFusionFireActionTicks(double seconds,float speed,int& ticks)
{
    if(!std::isfinite(seconds)||seconds<0||!std::isfinite(speed)||speed<=0)return false;
    const double value=std::ceil(seconds/static_cast<double>(speed)*100.0);
    if(value>(std::numeric_limits<int>::max)())return false;
    ticks=static_cast<int>(value);return true;
}
bool TryAdvanceFusionTorchCount(uint16_t kind,int before,bool success,int& after,bool& attempt)
{
    if((kind!=1059&&kind!=1061)||before<0||before==(std::numeric_limits<int>::max)())return false;
    const int value=before+1,limit=kind==1059?50:100;
    attempt=value>limit;
    after=attempt&&(kind==1061||success)?0:value;
    return true;
}
bool FusionFireLightCellAffected(int lightColumn,int lightRow,int column,int row)
{
    if(lightColumn<0||lightRow<0||column<0||row<0)return false;
    return std::abs(static_cast<long long>(lightColumn)-column)<=1&&
        std::abs(static_cast<long long>(lightRow)-row)<=1;
}
bool TryGetFusionFireKnockBack(double axis,double right,double distance,double& result)
{
    if(!std::isfinite(axis)||!std::isfinite(right)||!std::isfinite(distance)||distance<0)return false;
    const double edge=right-2.2;
    result=axis>edge?axis:(std::min)(axis+distance,edge);
    return true;
}
bool FusionFireCircleTouchesRect(double cx,double cy,double radius,double x,double y,double w,double h)
{
    if(!std::isfinite(cx+cy+radius+x+y+w+h)||radius<=0||w<0||h<0)return false;
    const double px=(std::clamp)(cx,x,x+w),py=(std::clamp)(cy,y,y+h);
    const double dx=(px-cx)/(72.0/1.35),dy=(py-cy)/(100.0/1.67);
    return dx*dx+dy*dy<=radius*radius;
}
int FusionFireCherryNutHealOverlapCount(double cx,double cy,int blastRow,
    double cellX,double cellY,int plantRow,double radius)
{
    if(!std::isfinite(cx+cy+cellX+cellY+radius)||radius<=0||blastRow<0||blastRow>=5||
        plantRow<0||plantRow>=5||std::abs(blastRow-plantRow)>1)return 0;
    constexpr double px=72.0/1.35,py=100.0/1.67;
    // The two enabled source1003 BoxCollider2D entries, relative to its Shadow.
    struct Box {double x,y,width,height;};
    constexpr Box boxes[]={{1.1920929e-7,2.147199,3.2122653,3.7848506},
        {.36352903,2.214995,1.8463796,4.1849174}};
    int count=0;
    for(const auto& box:boxes)
    {
        const double w=box.width*.33*px,h=box.height*.33*py;
        const double x=cellX+(box.x+.01)*.33*px;
        const double y=cellY-(.3+(box.y-.47)*.33)*py;
        if(FusionFireCircleTouchesRect(cx,cy,radius,x-w*.5,y-h*.5,w,h))++count;
    }
    return count;
}
bool TryStepFusionSquashProjectile(float z,float velocity,float acceleration,float ground,
    float& nextZ,float& nextVelocity,bool& bounced)
{
    if(!std::isfinite(z)||!std::isfinite(velocity)||!std::isfinite(acceleration)||
        !std::isfinite(ground)||acceleration<0)return false;
    const float nz=z+velocity,nv=velocity+acceleration;
    if(!std::isfinite(nz)||!std::isfinite(nv))return false;
    nextZ=nz;nextVelocity=nv;bounced=nz>ground&&nv>0;
    if(bounced)nextVelocity=-nv; // Source does not clamp the crossed position.
    return true;
}
}
