#include "SpecialZombie269Rules.h"
#include "FusionNextZombie343Rules.h"
#include "FusionCatalog.h"
#include <cmath>
#include <limits>
namespace LawnFusion
{
namespace
{
constexpr FusionZombieProfile profiles[]={
    {5,270,0,200,50,true},{14,1350,0,0,500,true,true,true},{15,720,0,1100,400,true},
    {18,2700,0,0,500,true,false,true},{104,1500,0,200,50,true}};
struct RootKey {float time,x,inSlope,outSlope;};
// Actual npc900-models/{15,104}: walk and walk2 have the SAME root curve.
constexpr RootKey paperRoot[]={
    {0,1.12f,0,-.69777775f},{.16666667f,1.0037037f,-1.329101f,-1.3291006f},
    {.35f,.3037037f,-3.9259264f,-3.9259262f},{.56666666f,-.56666666f,-2.3247857f,-2.3247857f},
    {.78333336f,-.81851846f,-.8502418f,-.85024196f},{.95f,-.9f,-.85024226f,-.850242f},
    {1.1666666f,-1.2111111f,-2.1623929f,-2.162393f},{1.3833333f,-1.837037f,-2.4362125f,-2.4362137f},
    {1.6166667f,-2.3074074f,5.0790953e-7f,0},{1.7833333f,-2.1259258f,1.08889f,0}};

bool Position(FusionZombieClipPosition p,float duration)
{return std::isfinite(p.seconds)&&p.seconds>=0&&p.seconds<=duration;}
}
bool SpecialZombie269PaperKind(uint16_t kind){return kind==5||kind==15||kind==104;}
const FusionZombieProfile* FindSpecialZombie269Profile(uint16_t kind)
{if(kind==200||kind==211)return FindFusionNextZombie343Profile(kind);for(const auto& p:profiles)if(p.kind==kind)return &p;return nullptr;}
bool SpecialZombie269RejectEffect(uint16_t kind,int status,SpecialZombie269StatusEffect effect)
{return kind==18||(kind==15&&status==5&&effect!=SpecialZombie269StatusEffect::Cold);}
uint32_t SpecialZombie269RequiredCapabilities(uint16_t kind)
{
    switch(kind)
    {
    case 14:case 200:case 211:return Special269Model|Special269Crash|Special269BodyParts|Special269DeathEffect|
        Special269VehicleMotion|Special269VehicleContacts|Special269IncomingDamage|Special269Water;
    case 5:case 15:return Special269Model|Special269PaperTransition|Special269PaperMotion|Special269PaperBite;
    case 18:return Special269Model|Special269Crash|Special269BodyParts|Special269DeathEffect|Special269DriverTerrain|
        Special269VehicleMotion|Special269VehicleContacts|Special269IncomingDamage|Special269DeathClock;
    case 104:return Special269Model|Special269PaperTransition|Special269PaperShot|Special269PaperMotion|Special269PaperBite;
    default:return 0;
    }
}
bool SpecialZombie269CapabilitiesReady(uint16_t kind,uint32_t actual)
{const auto required=SpecialZombie269RequiredCapabilities(kind);return required&&(actual&required)==required;}
bool TryGetSpecialZombie269BodyStage(uint16_t kind,float hp,int maximum,uint8_t& stage)
{
    if((!FusionSubmarine343Kind(kind)&&kind!=18)||!std::isfinite(hp)||maximum<=0||hp>maximum)return false;
    // Source14 doubles I32 before converting; source18 adds two F32 maxima.
    // Equality retains the higher-health image in both actual source bodies.
    const uint32_t twice=uint32_t(maximum)*2U;
    const int64_t wrapped=(twice&0x80000000U)?int64_t(twice)-0x100000000LL:twice;
    const volatile float numerator=FusionSubmarine343Kind(kind)?float(wrapped):float(maximum)+float(maximum);
    stage=hp<float(maximum)/3?2:hp<numerator/3?1:0;return true;
}
bool TryGetSpecialZombie269PaperClip(uint16_t kind,int status,bool eating,SpecialZombie269PaperClip& out)
{
    if(!SpecialZombie269PaperKind(kind)||(status!=4&&status!=5&&status!=6))return false;
    SpecialZombie269PaperClip p;
    if(status==5)p={"losePaper",2,1};
    else if(eating)p={status==6?"attack2":"attack",2,status==6?(kind==15?6.f:3.f):2.f};
    else if(status==6)p={"walk2",kind!=104?2.0166667f:2.f,kind==15?4.f:1.f};
    else p={"walk",2,.4f};
    out=p;return true;
}
bool SpecialZombie269PaperAngryDue(float phase)
{return std::isfinite(phase)&&phase>=1.9166666f/2&&phase<=1;}
bool TrySampleSpecialZombie269PaperRoot(uint16_t kind,float seconds,float& out)
{
    if(!SpecialZombie269PaperKind(kind)||!std::isfinite(seconds)||seconds<0||seconds>2.0166667f)return false;
    if(seconds<=paperRoot[0].time){out=paperRoot[0].x;return true;}
    if(seconds>=paperRoot[9].time){out=paperRoot[9].x;return true;}
    unsigned i=1;while(paperRoot[i].time<=seconds)++i;
    const auto& a=paperRoot[i-1];const auto& b=paperRoot[i];
    const float h=b.time-a.time,t=(seconds-a.time)/h,t2=t*t,t3=t2*t;
    out=(2*t3-3*t2+1)*a.x+(t3-2*t2+t)*h*a.outSlope+(-2*t3+3*t2)*b.x+(t3-t2)*h*b.inSlope;return true;
}
bool TryGetSpecialZombie269PaperRootDelta(uint16_t kind,int status,FusionZombieClipPosition before,
    FusionZombieClipPosition after,bool right,double scale,double& out)
{
    SpecialZombie269PaperClip clip;
    if(!TryGetSpecialZombie269PaperClip(kind,status,false,clip)||status==5||!Position(before,clip.duration)||
       !Position(after,clip.duration)||after.loop<before.loop||(after.loop==before.loop&&after.seconds<before.seconds)||
       !std::isfinite(scale)||scale<0)return false;
    float x0=0,x1=0;if(!TrySampleSpecialZombie269PaperRoot(kind,before.seconds,x0)||!TrySampleSpecialZombie269PaperRoot(kind,after.seconds,x1))return false;
    const double dx=double(after.loop-before.loop)*(double(paperRoot[9].x)-paperRoot[0].x)+double(x1)-x0;
    const double result=(right?-dx:dx)*scale;if(!std::isfinite(result))return false;out=result;return true;
}
bool TryCountSpecialZombie269PaperBites(uint16_t kind,FusionZombieClipPosition before,FusionZombieClipPosition after,uint32_t& out)
{
    if(!SpecialZombie269PaperKind(kind)||!Position(before,2)||!Position(after,2)||after.loop<before.loop||
       (after.loop==before.loop&&after.seconds<before.seconds))return false;
    constexpr float event=1.0833334f;
    const uint64_t b=uint64_t(before.loop)+(before.seconds>=event),a=uint64_t(after.loop)+(after.seconds>=event);
    if(a<b||a-b>(std::numeric_limits<uint32_t>::max)())return false;out=uint32_t(a-b);return true;
}
bool StepSpecialZombie269PaperShootCounter(int& ticks,bool angry,bool stopped,bool headless)
{
    if(ticks<0||ticks>75||!angry||stopped||headless)return false;
    if(!ticks){ticks=75;return true;}--ticks;return false; // Strict source<0 after.01.
}
bool TryPlanSpecialZombie269Crash(uint16_t sourceKind,uint16_t plantKind,bool caltrop,bool isNut,int level,SpecialZombie269CrashPlan& out)
{
    if((!FusionSubmarine343Kind(sourceKind)&&sourceKind!=18&&sourceKind!=40)||level<0)return false;
    SpecialZombie269CrashPlan p;
    if(!caltrop)
    {
        // Zombie.CrashEntity381060, mask0x40401 over kinds903..921.
        if(plantKind==903||plantKind==913||plantKind==921||plantKind==925||plantKind==1151)
            p={SpecialZombie269CrashAction::Damage,500,true,false};
        else if(plantKind==931||(plantKind==1048&&!FusionSubmarine343Kind(sourceKind)))
            p={SpecialZombie269CrashAction::Damage,500,true,true};
        else if(isNut&&level>=3)p={SpecialZombie269CrashAction::Damage,1000,true,false};
        else p.action=SpecialZombie269CrashAction::Crush;
    }
    out=p;return true;
}

bool TryGetSpecialZombie269SourcePlantTraits(uint16_t kind,bool& caltrop,bool& nut,int& level)
{
    if(!FusionFindPlant(kind))return false;
    bool c=false,n=false;
    switch(kind)
    {
    case 17:case 906:case 1060:case 1061:case 1062:case 1063:case 1064:
    case 1074:case 1075:case 1155:case 1180:case 1196:c=true;break;
    }
    // GetPlantTag40C770 writes IsNut40D3F0 to Plant+113 (tag base10C+7).
    // SpickRock is a DIFFERENT tag, at11B. All281 source prefabs start level0.
    switch(kind)
    {
    case 3:case 24:case 243:case 251:case 255:case 903:case 905:case 911:case 913:
    case 921:case 922:case 925:case 931:case 935:case 1003:case 1004:case 1006:
    case 1010:case 1012:case 1013:case 1021:case 1027:case 1028:case 1029:case 1039:
    case 1048:case 1073:case 1087:case 1088:case 1089:case 1091:case 1092:case 1097:
    case 1098:case 1099:case 1110:case 1129:case 1147:case 1151:case 1164:case 1169:
    case 1189:case 1190:n=true;break;
    }
    caltrop=c;nut=n;level=0;return true;
}
bool TryGetSpecialZombie269VehicleClip(uint16_t kind,bool pending,SpecialZombie269PaperClip& out,bool severe)
{
    if(!FusionSubmarine343Kind(kind)&&kind!=18)return false;
    if(kind==200){out={"walk",1.0166667f,.5f};return true;}if(kind==211){out={"walk",1.f,.5f};return true;}
    out=kind==14?SpecialZombie269PaperClip{"walk",1.6833333f,1}:
        pending?SpecialZombie269PaperClip{"die",1,.9f}:severe?SpecialZombie269PaperClip{"shake",.15f,1}:SpecialZombie269PaperClip{"idle",2,2};
    return true;
}
bool TryGetSpecialZombie269SubmarineRootDelta(FusionZombieClipPosition before,FusionZombieClipPosition after,
    bool right,double projection,double& out)
{
    constexpr float duration=1.6833333f;
    if(!Position(before,duration)||!Position(after,duration)||after.loop<before.loop||
        (after.loop==before.loop&&after.seconds<before.seconds)||!std::isfinite(projection)||projection<0)return false;
    // Actual source14 walk root-X:7.32->6.925->6.53, then a1/60s hold.
    const auto sample=[](float t)
    {
        constexpr RootKey keys[]={{0,7.32f,0,-.47399998f},{.8333333f,6.925f,-.47399998f,-.47399998f},
            {1.6666666f,6.53f,-.47399998f,0}};
        if(t>=keys[2].time)return keys[2].x;
        const auto& a=keys[t<keys[1].time?0:1];const auto& b=keys[t<keys[1].time?1:2];
        const float h=b.time-a.time,u=(t-a.time)/h,u2=u*u,u3=u2*u;
        return (2*u3-3*u2+1)*a.x+(u3-2*u2+u)*h*a.outSlope+(-2*u3+3*u2)*b.x+(u3-u2)*h*b.inSlope;
    };
    const double dx=double(after.loop-before.loop)*double(6.53f-7.32f)+sample(after.seconds)-sample(before.seconds);
    const double result=dx*(right?-projection:projection);if(!std::isfinite(result))return false;out=result;return true;
}
namespace
{
struct VehiclePoint {double x,y;};
bool VehiclePolygon(uint16_t kind,double x,double y,double scale,bool right,double px,double py,
    std::array<VehiclePoint,8>& out,unsigned& count)
{
    if((!FusionSubmarine343Kind(kind)&&kind!=18)||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(scale)||scale<=0||
        !std::isfinite(px)||!std::isfinite(py)||px<=0||py<=0)return false;
    std::array<VehiclePoint,8> polygon{};
    if(kind==14)
    {
        polygon={{{.06792784,1.9781685},{-1.0962214,1.6720707},{-1.9631891,.6282803},
            {-.35003608,.34576535},{1.9259735,.5903076},{1.091661,1.7571964},
            {1.3697743,2.3108172},{.849164,2.18384}}};count=8;
    }
    else
    {
        constexpr double ox=1.0021281,oy=5.9179745,w=10.208747,h=7.6357136;
        polygon[0]={ox-w*.5,oy-h*.5};polygon[1]={ox+w*.5,oy-h*.5};
        polygon[2]={ox+w*.5,oy+h*.5};polygon[3]={ox-w*.5,oy+h*.5};count=4;
    }
    const double factor=scale*(kind==14?.9:.25);
    for(unsigned i=0;i<count;++i)
    {polygon[i]={x+polygon[i].x*px*factor*(right?-1:1),y-polygon[i].y*py*factor};
     if(!std::isfinite(polygon[i].x)||!std::isfinite(polygon[i].y))return false;}
    out=polygon;return true;
}
bool SegmentRect(VehiclePoint a,VehiclePoint b,double l,double t,double r,double bottom)
{
    double lo=0,hi=1;
    const auto clip=[&](double p,double q)
    {if(p==0)return q>=0;const double u=q/p;if(p<0){if(u>hi)return false;if(u>lo)lo=u;}else{if(u<lo)return false;if(u<hi)hi=u;}return true;};
    return clip(a.x-b.x,a.x-l)&&clip(b.x-a.x,r-a.x)&&clip(a.y-b.y,a.y-t)&&clip(b.y-a.y,bottom-a.y);
}
}
bool SpecialZombie269VehicleTouchesRect(uint16_t kind,double rootX,double rootY,double scale,bool right,
    double px,double py,double left,double top,double width,double height)
{
    if(!std::isfinite(left)||!std::isfinite(top)||!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0)return false;
    std::array<VehiclePoint,8> polygon;unsigned count=0;
    if(!VehiclePolygon(kind,rootX,rootY,scale,right,px,py,polygon,count))return false;
    const double r=left+width,b=top+height;if(!std::isfinite(r)||!std::isfinite(b))return false;
    bool inside=false;
    for(unsigned i=0,j=count-1;i<count;j=i++)
    {
        const auto& a=polygon[j];const auto& p=polygon[i];
        if((p.x>=left&&p.x<=r&&p.y>=top&&p.y<=b)||SegmentRect(a,p,left,top,r,b))return true;
        if(((p.y>top)!=(a.y>top))&&left<(a.x-p.x)*(top-p.y)/(a.y-p.y)+p.x)inside=!inside;
    }
    return inside; // Rectangle is wholly inside a possibly concave source polygon.
}

bool SpecialZombie269CrashZombieAllowed(bool sameRow,bool sameSide,int status)
{return sameRow&&!sameSide&&status!=10&&status!=12;}
bool TryPlanSpecialZombie269DriverCaltrop(float hp,int maximum,SpecialZombie269CaltropPlan& out)
{
    if(!std::isfinite(hp)||maximum<=0||hp<=0||hp>maximum)return false;
    out=hp>float(maximum)*.5f?SpecialZombie269CaltropPlan{int(float(maximum)*.5f),false}:
        SpecialZombie269CaltropPlan{0,true};return true;
}
bool TryGetSpecialZombie269DriverVelocity(bool controlled,bool stopped,bool behind,float speed,float& vx)
{
    if(!std::isfinite(speed)||speed<0)return false;
    vx=stopped?0:(controlled?1.f:-1.f)*(behind?.2f:speed);return true;
}
}
