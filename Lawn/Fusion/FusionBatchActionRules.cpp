#include "FusionBatchActionRules.h"
#include "FusionBasePlantRules.h"
#include "FusionAction266Rules.h"
#include <algorithm>
#include <cmath>

namespace LawnFusion
{
namespace
{
constexpr FusionBatchActionProfile profiles[] = {
    {8,FusionBatchActionFamily::Hypno,0,false}, {1022,FusionBatchActionFamily::Hypno,0,false},
    {11,FusionBatchActionFamily::Doom,1800,false}, {16,FusionBatchActionFamily::Jalapeno,1800,false},
    {1040,FusionBatchActionFamily::Doom,1800,false}, {1045,FusionBatchActionFamily::Hypno,0,false},
    {13,FusionBatchActionFamily::Squash,1800,false}, {1057,FusionBatchActionFamily::Squash,1800,true},
    {1054,FusionBatchActionFamily::Squash,1800,true},
    {17,FusionBatchActionFamily::Caltrop,20,false}, {1155,FusionBatchActionFamily::Caltrop,20,true},
    {1180,FusionBatchActionFamily::Caltrop,20,false},
    {1060,FusionBatchActionFamily::Caltrop,20,false},
    {1074,FusionBatchActionFamily::Caltrop,20,false},
    {1075,FusionBatchActionFamily::Caltrop,40,true},
    {1070,FusionBatchActionFamily::Gloom,20,false},
    {1071,FusionBatchActionFamily::Gloom,40,true},
    {1072,FusionBatchActionFamily::Gloom,20,false},
    {1170,FusionBatchActionFamily::ForwardFume,20,false},
    {1062,FusionBatchActionFamily::Caltrop,20,true}, {1063,FusionBatchActionFamily::Caltrop,20,false},
    {1064,FusionBatchActionFamily::Caltrop,5,false},
    {9,FusionBatchActionFamily::Scaredy,20,false,.34850003499999993,.469},
    {1038,FusionBatchActionFamily::Scaredy,20,false,.34850003499999993,.469},
    {1036,FusionBatchActionFamily::Contact,0,false},
    {1044,FusionBatchActionFamily::Contact,1800,false},
    {1042,FusionBatchActionFamily::Scaredy,20,false,.34850003499999993,.469},
    {1171,FusionBatchActionFamily::Scaredy,600,false,.34850003499999993,.469},
    {1043,FusionBatchActionFamily::ManualFume,1800,false},
    {1046,FusionBatchActionFamily::ForwardFume,20,false,.718,.69},
    {1024,FusionBatchActionFamily::Scaredy,20,false,.34850003499999993,.469},
    {1025,FusionBatchActionFamily::AdaptiveFume,20,false,.493,.87299997},
    {14,FusionBatchActionFamily::Threepeater,20,false,.31810762000000004,.6304785484000001},
    {1058,FusionBatchActionFamily::Threepeater,20,false,.31810762000000004,.6304785484000001}};
constexpr FusionCaltropStrikeProfile caltropStrikes[] = {
    {17,20,0,false,false}, {1155,20,0,false,false},
    {1062,20,0,true,false}, {1063,20,0,false,false}, {1064,5,0,false,false},
    {1180,20,10,false,false}, {1060,20,0,false,true},
    {1074,20,5,false,true}, {1075,40,0,true,true}};
constexpr double scaredEvents[] = {static_cast<double>(.3f)};
constexpr double threeEvents[] = {.5};
constexpr double fumeEvents[] = {.75};
constexpr double doublePuffEvents[] = {.75,static_cast<double>(1.0833334f)};
constexpr double caltropEvents[] = {static_cast<double>(.33333334f)};
constexpr double threeSpikeEvents[] = {.5};
constexpr double doomFumeEvents[] = {static_cast<double>(.35f)};
constexpr double iceDoomFumeEvents[] = {static_cast<double>(.41666666f),static_cast<double>(.55f),
    static_cast<double>(.68333334f),static_cast<double>(.8333333f)};
const FusionShooterTimingProfile timing[] = {
    {9,1.5f,scaredEvents,1,.5,true,9}, {1024,1.5f,scaredEvents,1,.5,true,13},
    {1038,1.5f,scaredEvents,1,.5,true,21},
    {1042,1.5f,scaredEvents,1,.5,false,22},
    {1171,1.5f,scaredEvents,1,.5,true,92},
    {1043,60,doomFumeEvents,1,static_cast<double>(1.55f),true,0xffff},
    {1046,3,iceDoomFumeEvents,1,1,true,24},
    {14,1.5f,threeEvents,static_cast<double>(1.1f),1,true,0},
    {1058,1.5f,threeEvents,static_cast<double>(1.1f),1,true,0},
    {1025,1.5f,doublePuffEvents,2,static_cast<double>(1.3333334f),true,9},
    {1025,1.5f,fumeEvents,1,static_cast<double>(1.1666666f),true,0xffff,"shoot1"},
    {17,1,caltropEvents,1,static_cast<double>(.6666667f),true,0xffff,"attack"},
    {1155,1,caltropEvents,1,static_cast<double>(.6666667f),true,0xffff,"attack"},
    {1180,1,caltropEvents,1,static_cast<double>(.6666667f),true,0xffff,"attack"},
    {1062,1,caltropEvents,1,static_cast<double>(.6666667f),true,0xffff,"attack"},
    {1063,1.5f,caltropEvents,1,static_cast<double>(.6666667f),true,0xffff,"attack"},
    {1064,1.5f,threeSpikeEvents,1,1,true,0xffff,"attack"}};
bool RectValid(double x, double y, double w, double h)
{
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(w) && std::isfinite(h) &&
        w >= 0 && h >= 0 && std::isfinite(x+w) && std::isfinite(y+h);
}
}
const FusionBatchActionProfile* FindFusionBatchActionProfile(uint16_t kind)
{
    for (const auto& profile : profiles) if (profile.kind == kind) return &profile;
    return nullptr;
}
bool FusionBatchPlantStateValid(uint16_t kind,uint32_t shots,uint16_t rage,uint16_t bag,uint8_t mode)
{ return (kind==1042 ? shots<=14 : shots==0) && rage==0 && bag==0 && (kind==1171 ? mode<=1 : mode==0); }
bool TryGetFusionScaredyFearTiming(uint16_t kind,float speed,FusionScaredyFearTiming& result)
{
    if (kind!=1038 || !std::isfinite(speed) || speed<.9f || speed>1.1f) return false;
    result={static_cast<int>(std::ceil(100.0/speed)),
        static_cast<int>(std::ceil(static_cast<double>(.8333333f)/speed*100)),
        static_cast<int>(std::ceil(static_cast<double>(1.0166667f)/speed*100))};
    return true;
}
unsigned int FusionBatchDamageFlags(uint16_t kind)
{
    if (FindFusionCaltropStrikeProfile(kind)) return 1U; // Shieldless4/IceShieldless5 bypass only the second armor.
    if (kind==13 || kind==1057 || kind==1054) return 18U; // Independent shield then first armor/body; native crushed-body art.
    if (kind==1025) return 2U; // Source NormalAll1.
    return 0U;
}
const FusionCaltropStrikeProfile* FindFusionCaltropStrikeProfile(uint16_t kind)
{
    for (const auto& profile : caltropStrikes) if (profile.kind == kind) return &profile;
    return nullptr;
}
const FusionShooterTimingProfile* FindFusionBatchTimingProfile(uint16_t kind, bool closeRange)
{
    for (const auto& profile : timing)
        if (profile.kind == kind && (kind!=1025 || (profile.projectileKind==0xffff)==closeRange)) return &profile;
    return FindFusionAction266TimingProfile(kind);
}
bool TryGetFusionBatchInstantTicks(uint16_t kind, float speed, int& ticks)
{
    if (!std::isfinite(speed) || speed < .9f || speed > 1.1f || (kind != 11 && kind != 1040 && kind != 16)) return false;
    // Controller speed is serialized single precision; Jalapeno has no Speed parameter.
    const double rate = kind == 16 ? static_cast<double>(.8f) : static_cast<double>(.7f)*speed;
    ticks = static_cast<int>(std::ceil(static_cast<double>(.6666667f)/rate*100));
    return true;
}
bool TryGetFusionCaltropCycleTicks(float jitter, int& ticks)
{ return TryGetFusionCaltropCycleTicks(17,jitter,ticks); }
bool TryGetFusionCaltropCycleTicks(uint16_t kind,float jitter,int& ticks)
{
    const auto* profile=FindFusionBatchActionProfile(kind);
    const auto* clock=FindFusionBatchTimingProfile(kind);
    if (!profile || profile->family!=FusionBatchActionFamily::Caltrop || !clock ||
        !std::isfinite(jitter) || jitter < -.1f || jitter > .1f) return false;
    ticks = static_cast<int>(std::ceil(static_cast<double>(clock->attackIntervalSeconds+jitter)*100));
    return true;
}
bool TryGetFusionScaredyDoomInterval(uint32_t shots,float& interval)
{
    if (shots>14) return false;
    float value=1.5f;
    for (uint32_t i=0;i<shots;++i) value=value>.2f ? value-.1f : .2f;
    interval=value;
    return true;
}
bool TryAdvanceFusionScaredyDoom(uint32_t before,bool scared,uint32_t& after,bool& explode)
{
    float interval=0;
    if (!TryGetFusionScaredyDoomInterval(before,interval)) return false;
    explode=scared && interval==.2f;
    after=scared ? (explode ? before : 0u) : (std::min)(before+1,14u);
    return true;
}
bool TryAdvanceFusionPuffColors(uint8_t before, uint8_t color, uint8_t& after, bool& hypnotize)
{
    if (before > 127 || color > 6) return false;
    after = static_cast<uint8_t>(before | (1u<<color));
    hypnotize = after == 127;
    return true;
}
bool TryGetFusionBatchEntryRows(uint16_t kind,int row,int rows,std::array<int,3>& result,size_t& count)
{
    if (kind!=1058 || rows<1 || rows>6 || row<0 || row>=rows) return false;
    std::array<int,3> candidate{}; size_t used=0;
    if (row>0) candidate[used++]=row-1;
    if (row<rows-1) candidate[used++]=row+1;
    candidate[used++]=row;
    result=candidate; count=used; return true;
}
bool FusionBatchProjectileMetadataValid(uint16_t kind, int damage, int sun, uint8_t phase, uint8_t color, int motion)
{
    if (damage != 20 || sun != 0) return false;
    if (kind == 9) return phase == 0 && color == 0 && motion == 0; // Source long-range way0; not Puff way3.
    if (kind == 13) return phase == 0 && color <= 6 && motion == 0; // Native STRAIGHT.
    return kind == 0 && color == 0 && (phase == kFusionThreeUpper || phase == kFusionThreeLower) && motion == 2;
}
bool TryGetFusionThreeVerticalStep(uint8_t phase, int age, float& pixels)
{
    if ((phase != kFusionThreeUpper && phase != kFusionThreeLower) || age < 0) return false;
    //100Hz native integration of the source Rigidbody velocity assignment.
    const float t = age*.01f;
    const float source = t < .5f ? 7.0f-16.0f*t : 0.0f;
    pixels = static_cast<float>(source*kFusionBasePixelsY*.01)*(phase == kFusionThreeUpper ? -1.0f : 1.0f);
    return true;
}
bool FusionBatchShooterCanAcquire(uint16_t kind, int status, int rowDelta, bool controlled, bool dying, double dx, double vision)
{
    const auto* p = FindFusionBatchActionProfile(kind);
    if (p && p->family==FusionBatchActionFamily::ForwardFume)
        return rowDelta==0 && std::isfinite(dx) && dx>0 && (kind!=1170 || dx<7*kFusionBasePixelsX) && FusionPlantSearchUniqueZombieAllowed(status,controlled,dying);
    if (!p || !std::isfinite(dx) || !std::isfinite(vision) || controlled || dying || status < 0 || status == 1 ||
        status == 3 || status == 7 || status == 9 || status == 10 || status == 12 || status == 16 || status >= 20) return false;
    const bool three = p->family == FusionBatchActionFamily::Threepeater;
    return (three ? rowDelta >= -1 && rowDelta <= 1 : rowDelta == 0) && dx > 0 && dx < vision;
}
bool TryMakeFusionBatchArea(uint16_t kind, double x, double y, bool attack, FusionBatchArea& out)
{
    if (!std::isfinite(x) || !std::isfinite(y)) return false;
    const auto* p = FindFusionBatchActionProfile(kind);
    if (!p) return false;
    double w=0,h=0;
    if (p->family == FusionBatchActionFamily::Caltrop) w=h=1;
    else if (p->family == FusionBatchActionFamily::Squash)
    { w=attack?1:3; h=3; if (!attack) x+=.5*kFusionBasePixelsX; }
    else if (p->family == FusionBatchActionFamily::Scaredy)
    { w=h=kind==1038 && attack ? 3 : 2.5; y-=(kind==1038 && attack ? 1 : .5)*kFusionBasePixelsY; }
    else return false;
    FusionBatchArea candidate{x-w*kFusionBasePixelsX*.5,y-h*kFusionBasePixelsY*.5,w*kFusionBasePixelsX,h*kFusionBasePixelsY};
    if (!RectValid(candidate.x,candidate.y,candidate.width,candidate.height)) return false;
    out=candidate; return true;
}
bool FusionBatchAreaTouchesRect(const FusionBatchArea& a, double x, double y, double w, double h)
{
    return RectValid(a.x,a.y,a.width,a.height) && RectValid(x,y,w,h) &&
        a.x<=x+w && a.x+a.width>=x && a.y<=y+h && a.y+a.height>=y;
}
bool FusionDoomTouchesRect(double cx,double cy,double x,double y,double w,double h)
{ return FusionBatchCircleTouchesRect(cx,cy,5,x,y,w,h); }
bool FusionBatchCircleTouchesRect(double cx,double cy,double radius,double x,double y,double w,double h)
{
    if (!std::isfinite(cx) || !std::isfinite(cy) || !std::isfinite(radius) || radius<0 || !RectValid(x,y,w,h)) return false;
    const double dx=(std::max)({x-cx,0.0,cx-x-w})/kFusionBasePixelsX;
    const double dy=(std::max)({y-cy,0.0,cy-y-h})/kFusionBasePixelsY;
    return dx*dx+dy*dy<=radius*radius+1e-12;
}
bool TryMakeFusionDoomPuffTrigger(size_t collider,double shadowX,double shadowY,FusionBatchArea& out)
{
    // Both enabled root BoxCollider2D triggers, root scale .15. Collider0 is
    // the wide contact volume; collider1 is also a source trigger for this kind.
    constexpr double boxes[][4]={{-.17402363,2.9414606,5.7458153,5.6546946},
        {.8773042,.21457958,1.0584879,5.177252}};
    if (collider>=2 || !std::isfinite(shadowX) || !std::isfinite(shadowY)) return false;
    const auto& b=boxes[collider];
    const double w=b[2]*.15*kFusionBasePixelsX,h=b[3]*.15*kFusionBasePixelsY;
    out={shadowX+(b[0]*.15+.0015)*kFusionBasePixelsX-w*.5,
        shadowY-(b[1]*.15-.069)*kFusionBasePixelsY-h*.5,w,h};
    return true;
}
}
