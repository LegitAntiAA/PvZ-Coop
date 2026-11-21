#include "FusionShooter267Rules.h"
#include "FusionBasePlantRules.h"
#include "FusionBatchActionRules.h"
#include "FusionShooterBatchRules.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
namespace LawnFusion
{
namespace
{
constexpr double fume[]={static_cast<double>(.35f)},pea[]={static_cast<double>(.6666667f)},three[]={.5};
constexpr double cactus[]={static_cast<double>(.31666666f)},beam[]={static_cast<double>(.3f)};
constexpr double gatling[]={static_cast<double>(.33333334f),.5,static_cast<double>(.6666667f),
    static_cast<double>(.8333333f),1,static_cast<double>(1.1666666f)};
constexpr double loop[]={static_cast<double>(.083333336f)};
constexpr FusionShooterTimingProfile profiles[]={
    {1172,1.5f,fume,1,1,true,0xffff},{1158,1.5f,fume,1,1,true,0xffff},
    {1007,1.5f,pea,2,1,false,7},{919,1.5f,three,static_cast<double>(1.1f),1,true,87},
    {1103,1.5f,cactus,1,.5,true,47,"shoot1"},
    {927,.5f,gatling,1,1.25,true,93},{927,.5f,loop,1,static_cast<double>(.16666667f),true,93,"shoot_loop"},
    {904,.5f,beam,1,static_cast<double>(.56666666f),true,0xffff,"shooting"},
    {1047,3,three,static_cast<double>(1.1f),1,true,28}};
}
bool IsFusionShooter267Kind(uint16_t k)
{return k==902||k==932||FindFusionShooter267Timing(k);}
const FusionShooterTimingProfile* FindFusionShooter267Timing(uint16_t k)
{for(const auto& p:profiles)if(p.kind==k)return &p;return nullptr;}
const FusionShooterTimingProfile* FindFusionShooter267Timing(uint16_t k,const char* state)
{if(state)for(const auto& p:profiles)if(p.kind==k&&!std::strcmp(state,p.visualState))return &p;return nullptr;}
bool FusionShooter267StateValid(uint16_t k,uint32_t shots,uint16_t rage,uint16_t bag,uint8_t mode)
{
    if(!IsFusionShooter267Kind(k)||shots>static_cast<uint32_t>((std::numeric_limits<int>::max)())||rage)return false;
    if(k==927)return mode<=3&&bag<=2;
    if(k==904)return mode<=3&&!bag;
    if(k==932)return !shots&&!bag&&!mode; // Passive source conversion has no parent sequence counter.
    return !bag&&!mode;
}
bool TryGetFusionShooter267PhaseTicks(uint16_t kind,uint8_t mode,float speed,uint16_t tier,int& ticks)
{
    if((kind!=904&&kind!=927)||mode>3||!std::isfinite(speed)||speed<.9f||speed>1.1f||
        tier>2||(kind==904&&tier))return false;
    double seconds=0;
    if(kind==904)
    {
        if(mode==1)seconds=double(.35f)*.75/speed+.25;
        else if(mode==2)seconds=double(.56666666f)/speed;
        else if(mode==3)seconds=double(.1f)*.75/speed+.25;
    }
    else
    {
        const double rate=tier==0?1.:tier==1?1.5:2.;
        if(mode==0)seconds=double(2.6666667f);
        else if(mode==2)seconds=1.25/rate;
        else if(mode==3)seconds=double(.16666667f)/rate;
    }
    ticks=int(std::ceil(seconds*100));return true;
}
bool TryGetFusionPeaMineInterval(float x,float& seconds)
{
    if(!std::isfinite(x))return false;
    seconds=x>6?1.5f:x<1?.5f:1.5f-(6.f-x)/5.f;
    return true;
}
bool TryGetFusionUltimateGatling(int hp,int maximum,float& rate,float& velocity,int& damage)
{
    if(hp<0||maximum<=0||hp>maximum)return false;
    const int third=maximum/3;
    rate=hp>static_cast<int64_t>(maximum)*2/3?1.f:hp>=third?1.5f:2.f;
    velocity=rate==1?6.f:rate==1.5f?9.f:12.f;
    float value=120;
    if(hp<third&&third>0)value+=360.f*(1.f-static_cast<float>(hp)/static_cast<float>(third));
    damage=static_cast<int>(value);return true;
}
bool FusionSourceZombieWithIron(int first,int second,int kind)
{
    if(first==2||first==4||first==7||first==8||second==1||second==3)return true;
    switch(kind){case 14:case 16:case 18:case 29:case 38:case 39:case 40:case 108:case 119:
    case 200:case 201:case 210:case 219:case 221:case 222:return true;default:return false;}
}
bool TryGetFusionMagnetCactusDamage(int damage,int first,int second,int& out)
{
    if(damage<0||damage>(std::numeric_limits<int>::max)()/10)return false;
    out=first==10?(std::max)(500,damage*10):
        first==2||first==4||first==7||first==8||second==1||second==3?damage*2:damage;
    return true;
}
bool TryGetFusionGoldTorchReturn(uint8_t way,uint8_t flag,int age,int row,int rows,uint8_t& next,int& nextRow)
{
    if(flag!=1||age<51||(way!=4&&way!=5)||rows<1||row<0||row>=rows)return false;
    next=way==4?5:4;nextRow=way==4?(std::min)(row+1,rows-1):(std::max)(row-1,0);return true;
}
bool FusionShooter267ProjectileMetadataValid(uint16_t k,int damage,uint8_t way,uint8_t fire,uint8_t variant,int type,int motion)
{
    if(damage<0||damage>100000000||fire>1)return false;
    if(k==11)return damage==320&&!fire&&variant==1&&type==PROJECTILE_PEA&&motion==MOTION_STRAIGHT&&way==0;
    if(k==98||k==99)return variant<=1&&!fire&&type==PROJECTILE_PEA&&
        ((motion==MOTION_STRAIGHT&&way==0)||(motion==MOTION_THREEPEATER&&(way==4||way==5)));
    if(variant)return false;
    const bool straight=motion==MOTION_STRAIGHT&&way==0;
    const bool three=motion==MOTION_THREEPEATER&&(way==4||way==5);
    if(k==7)return damage==20&&type==(fire?PROJECTILE_FIREBALL:PROJECTILE_PEA)&&
        (straight||(way==2&&motion==MOTION_STAR));
    if(k==47)return damage==20&&!fire&&type==PROJECTILE_SPIKE&&straight;
    if(k==87)return damage==(fire?320:160)&&type==PROJECTILE_FIREBALL&&(straight||three);
    if(k==93)return damage<=480&&!fire&&type==PROJECTILE_PEA&&straight;
    if(k==28||k==36)return !fire&&type==PROJECTILE_PEA&&damage==(k==28?40:20)&&
        (straight||three||((way==255||(k==28&&way==1))&&motion==MOTION_LOBBED));
    return false;
}
bool TryGetFusionShooter267ThreeVerticalStep(uint8_t way,int age,float& pixels,double rowPitch)
{
    if((way!=4&&way!=5)||!std::isfinite(rowPitch)||rowPitch<=0)return false;
    float dayPixels=0;if(!TryGetFusionThreeVerticalStep(way==4?kFusionThreeUpper:kFusionThreeLower,age,dayPixels))return false;
    pixels=static_cast<float>(double(dayPixels)*rowPitch/100.);return true;
}
bool TryMakeFusionShooter267ProjectileRect(uint16_t k,double x,double y,FusionRecipientRect& out,double pixelsY,double angle)
{
    if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(pixelsY)||pixelsY<=0||!std::isfinite(angle))return false;
    if(k==7||k==11)
    {
        FusionRecipientRect rect;
        if(k==7?!TryMakeFusionShooterBatchRotatedRect(k,x,y,angle,rect):!TryMakeFusionShooterBatchProjectileRect(k,x,y,rect))return false;
        const double ratio=pixelsY/kFusionBasePixelsY;
        rect.y=y+(rect.y-y)*ratio;rect.height*=ratio;out=rect;return true;
    }
    double scale=1,ox=0,oy=0,w=0,h=0;
    switch(k)
    {
    case 47:scale=1.7;ox=-.015957542;oy=-4.4703484e-8;w=.26838857;h=.16374153;break;
    case 87:scale=1.7;w=.31117183;h=.33539104;break;
    case 93:scale=.34;oy=-.5128448;w=1.12081;h=2.1532648;break;
    case 98:case 99:scale=1.174088;w=.33396614;h=.3410837;break;
    default:return false;
    }
    w*=scale*kFusionBasePixelsX;h*=scale*pixelsY;
    out={x+ox*scale*kFusionBasePixelsX-w*.5,y-oy*scale*pixelsY-h*.5,w,h};return true;
}
bool FusionShooter267RotatedProjectileTouchesRect(uint16_t kind,double x,double y,double angle,
    const FusionRecipientRect& target,double pixelsY)
{
    if(kind!=7||!std::isfinite(pixelsY)||pixelsY<=0)return false;
    // Reuse the source-box SAT in its Day projection. Only the scene's Y
    // projection changes; anisotropic rotation and source box remain shared.
    const double ratio=kFusionBasePixelsY/pixelsY;
    const FusionRecipientRect dayTarget{target.x,y+(target.y-y)*ratio,target.width,target.height*ratio};
    return FusionShooterBatchRotatedProjectileTouchesRect(kind,x,y,angle,dayTarget);
}
}
