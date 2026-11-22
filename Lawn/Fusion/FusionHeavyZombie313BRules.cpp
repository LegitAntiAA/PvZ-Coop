#include "FusionHeavyZombie313BRules.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <string_view>

namespace LawnFusion
{
namespace
{
bool Finite(float x){return std::isfinite(x);}
struct RootKey {float time,value,inSlope,outSlope;};
struct RootCurve {uint16_t kind;const char* state;float duration,scale;bool loop;std::span<const RootKey> keys;};
#include "FusionHeavyZombie313BRoots.inc"
constexpr FusionZombieProfile profiles[]={
    {28,720,0,0,50},{31,850,0,0,500},{32,720,0,0,50},{33,720,0,720,50},
    {34,270,2200,0,50},{45,720,0,0,50},{203,1500,0,1500,400},{223,1500,12000,1500,50}};
float Sample(std::span<const RootKey> keys,float t)
{
    if(t<=keys.front().time)return keys.front().value;if(t>=keys.back().time)return keys.back().value;
    size_t i=1;while(keys[i].time<=t)++i;const auto& a=keys[i-1];const auto& b=keys[i];
    if(!Finite(a.outSlope)||!Finite(b.inSlope))return a.value;
    const float h=b.time-a.time,u=(t-a.time)/h,u2=u*u,u3=u2*u;
    return (2*u3-3*u2+1)*a.value+(u3-2*u2+u)*h*a.outSlope+(-2*u3+3*u2)*b.value+(u3-u2)*h*b.inSlope;
}
}
const FusionZombieProfile* FindFusionHeavyZombie313BProfile(uint16_t kind)
{for(const auto& profile:profiles)if(profile.kind==kind)return &profile;return nullptr;}
bool FusionCatapult313BPlantAllowed(const FusionHeavy313BPlant& p,int row,float axisX)
{return p.uid&&p.alive&&p.row==row&&!p.flying&&!p.caltrop&&p.kind!=1095&&Finite(p.shadowX)&&p.shadowX<=axisX;}
bool SelectFusionCatapult313BTarget(std::span<const FusionHeavy313BPlant> plants,int row,float axisX,float shadowX,uint32_t& uid)
{
    if(!Finite(axisX)||!Finite(shadowX))return false;
    uint32_t selected=0;float distance=-(std::numeric_limits<float>::max)();
    for(const auto& p:plants)if(FusionCatapult313BPlantAllowed(p,row,axisX))
    {const float d=std::abs(p.shadowX-shadowX);if(distance<d){distance=d;selected=p.uid;}}
    uid=selected;return true;
}
bool FusionCatapult313BCanSearch(bool controlled,float x,float maximum,uint8_t ammo)
{return !controlled&&Finite(x)&&Finite(maximum)&&x<maximum-2&&ammo>0&&ammo<=25;}
bool FusionCatapult313BBroken(float health,int maximum)
{return Finite(health)&&maximum>0&&health<float(maximum/2);}
uint8_t FusionCatapult313BHiddenBallChildren(uint8_t ammo)
{return ammo<=4?uint8_t((1u<<(std::min)(4,5-int(ammo)))-1):0;}
FusionMiner313BRise FusionMiner313BCheckRise(uint8_t status,float x,bool axe,bool eve)
{
    if(status!=12||!Finite(x))return FusionMiner313BRise::None;
    if(x<(eve?.5f:-5.f))return FusionMiner313BRise::Natural;
    return axe?FusionMiner313BRise::None:FusionMiner313BRise::Surprised;
}
bool FusionMiner313BCanCold(uint8_t status){return status!=12;}
bool FusionMiner313BPlantSurprises(uint16_t kind,std::span<const uint16_t> contents)
{
    if(kind==925||kind==935||kind==1099)return true;
    if(kind!=911&&kind!=922)return false;
    return std::any_of(contents.begin(),contents.end(),[](uint16_t x){return x==935||x==1099;});
}
bool FusionPogo313BViewValid(uint16_t kind,const FusionPogo313BView& s)
{
    if((kind!=32&&kind!=34)||s.wait>2)return false;
    if(s.status!=0&&s.status!=1&&s.status!=14&&(kind!=34||(s.status!=19&&s.status!=20)))return false;
    if(kind==32&&(s.jumped||s.lostJumper))return false;
    return s.status==1||s.shadowEnabled==(s.status!=20);
}
bool FusionPogo313BCanPause(const FusionPogo313BView& s,bool controlled,int row,float x,const FusionHeavy313BPlant& p)
{return s.status==14&&!controlled&&!s.wait&&p.uid&&p.alive&&p.row==row&&!p.flying&&!p.caltrop&&p.shadowX<x;}
bool FusionPogo313BLandSound(FusionPogo313BView& s)
{
    if(s.wait>2)return false;
    auto next=s;
    if(next.paused&&next.started){next.paused=false;next.wait=2;}
    next.started=true;if(next.wait)--next.wait;s=next;return true;
}
bool FusionPogo313BJumpBlocked(const FusionPogo313BView& s,bool dying,int row,float x,const FusionHeavy313BPlant& p)
{return s.wait==1&&!s.paused&&!dying&&p.uid&&p.alive&&p.row==row&&p.shadowX<x&&p.tallNut;}
bool FusionPogo313BFreezeBecomesCold(uint8_t status,bool dying){return status!=0||dying;}
float FusionPogo313BHorizontalSpeed(uint8_t wait,float speed,bool right)
{return wait<=2&&Finite(speed)?(right?1.f:-1.f)*speed*(wait?1.7f:.8f):0.f;}
void FusionPogo313BLoseJumper(FusionPogo313BView& s,uint16_t kind,bool dying,int reason)
{
    if(kind!=32&&kind!=34)return;
    if(!dying)s.status=0;
    s.shadowEnabled=true;
    if(kind==34&&reason==0)s.lostJumper=true;
}
bool StepFusionSuperPogo313B(const FusionPogo313BView& s,float wait,float dt,float x,float jumpX,float maximum,bool controlled,FusionSuperPogo313BStep& out)
{
    if(!FusionPogo313BViewValid(34,s)||!Finite(wait)||wait<0||!Finite(dt)||dt<0||!Finite(x)||!Finite(jumpX)||!Finite(maximum))return false;
    FusionSuperPogo313BStep next;next.after=s;next.waitSeconds=wait;
    if(!controlled&&x<jumpX&&x>1.5f&&wait<5&&!s.jumped)
    {
        next.waitSeconds=wait+dt;if(!Finite(next.waitSeconds))return false;
        if(s.status!=0&&s.status!=1)next.after.status=19;
        if(next.waitSeconds>=5){next.after.jumped=true;next.startJump=true;next.velocityX=-4.25f;next.velocityY=6;}
    }
    else if(x>=maximum-2.5f&&!s.jumped&&s.status!=0&&s.status!=1)next.after.status=14;
    out=next;return true;
}
bool StepFusionSuperPogo313BMotion(float x,float y,float vx,float vy,float dt,float land,FusionSuperPogo313BMotion& out)
{
    if(!Finite(x)||!Finite(y)||!Finite(vx)||!Finite(vy)||!Finite(dt)||dt<0||!Finite(land))return false;
    // PositionUpdate writes the real Rigidbody velocity and then observes the
    // current axis. Physics integration is a separate native engine operation.
    FusionSuperPogo313BMotion next{x,y,vy-dt*10,false};
    if(!Finite(next.x)||!Finite(next.y)||!Finite(next.velocityY))return false;
    if(next.velocityY<0&&next.y<land){next.y=land;next.velocityY=0;next.landed=true;}
    out=next;return true;
}
bool FusionLadder313BCanSet(uint8_t status,bool controlled,bool dying,int row,const FusionHeavy313BPlant& p)
{return status==18&&!controlled&&!dying&&p.uid&&p.alive&&p.row==row&&!p.ladder&&p.nut&&!p.bigNut;}
FusionLadder313BSet FusionLadder313BSetResult(bool alive,bool ladder)
{return alive&&!ladder?FusionLadder313BSet::Place:FusionLadder313BSet::Retry;}
uint8_t FusionLadder313BArmorStage(int health,int maximum)
{if(maximum<=0)return 0;return float(health)<float(maximum)/3?2:float(health)<float(maximum)*2/3?1:0;}
bool FusionBungee313BStealable(const FusionHeavy313BPlant& p,std::span<const FusionHeavy313BPlant> cell)
{
    if(!p.uid||!p.alive||p.flying||p.doubleBox||(p.pot&&p.goldPot))return false;
    if(!p.lily)return true;
    for(const auto& other:cell)if(other.uid&&other.alive&&other.column==p.column&&other.row==p.row&&!other.lily)return false;
    return true;
}
bool BuildFusionBungee313BCandidates(std::span<const FusionHeavy313BPlant> plants,std::vector<uint32_t>& out)
{
    std::vector<uint32_t> result;unsigned sunflowers=0;
    for(const auto& p:plants)if(FusionBungee313BStealable(p,plants))
    {result.push_back(p.uid);if(p.kind==1||p.kind==1033)++sunflowers;}
    if(sunflowers==1)
    {
        result.clear();for(const auto& p:plants)if(p.kind!=1&&p.kind!=1033&&FusionBungee313BStealable(p,plants))result.push_back(p.uid);
    }
    out.swap(result);return true;
}
bool FusionBungee313BViewValid(const FusionBungee313BView& s,int columns,int rows)
{
    return (s.status==0||s.status==1||s.status==22||s.status==23||s.status==24)&&columns>0&&rows>0&&
        s.targetColumn>=0&&s.targetColumn<columns&&s.targetRow>=0&&s.targetRow<rows&&
        Finite(s.targetX)&&Finite(s.targetY)&&Finite(s.markerY)&&Finite(s.cordScaleX)&&Finite(s.cordScaleY)&&Finite(s.cordScaleZ)&&
        s.cordScaleX>=.1f&&s.cordScaleX<=1000&&std::abs(s.cordScaleY)<=1000&&std::abs(s.cordScaleZ)<=1000&&
        std::abs(s.targetX)<=1024&&std::abs(s.targetY)<=1024&&std::abs(s.markerY)<=1024;
}
bool StepFusionBungee313BMotion(const FusionBungee313BView& s,float y,float dt,float maximum,FusionBungee313BMotion& out)
{
    if((s.status!=0&&s.status!=1&&s.status!=22&&s.status!=23&&s.status!=24)||!Finite(y)||!Finite(dt)||dt<0||!Finite(maximum)||
        !Finite(s.targetY)||!Finite(s.cordScaleX)||!Finite(s.cordScaleY)||!Finite(s.cordScaleZ)||s.cordScaleX<.1f)return false;
    FusionBungee313BMotion next;next.after=s;next.rootY=y;
    if(s.status==23)
    {
        next.rootY-=dt*12*(s.setZombie?1.2f:.9f);const float delta=dt*(s.setZombie?2.4f:1.8f);
        next.after.cordScaleX+=delta;next.after.cordScaleY+=delta;next.after.cordScaleZ+=delta;
        if(next.rootY<s.targetY+(s.setZombie?0.f:.5f))
        {next.after.status=s.setZombie?24:0;next.putZombie=s.setZombie;next.land=!s.setZombie;}
        else if(next.rootY<s.targetY+2)next.checkUmbrella=true;
    }
    else if(s.status==24)
    {next.rootY+=dt*12*1.3f;const float delta=dt*2.6f;next.after.cordScaleX-=delta;next.after.cordScaleY-=delta;next.after.cordScaleZ-=delta;next.retire=next.rootY>maximum;}
    // Source clamps X only; until that moment the genuine anisotropic Y/Z
    // offsets survive, including a negative Z after sufficiently long ascent.
    if(next.after.cordScaleX<.1f)next.after.cordScaleX=next.after.cordScaleY=next.after.cordScaleZ=.1f;
    if(!Finite(next.rootY)||!Finite(next.after.cordScaleX)||!Finite(next.after.cordScaleY)||!Finite(next.after.cordScaleZ))return false;
    out=next;return true;
}
bool StepFusionBungee313BMarker(float y,float target,float dt,float& out,bool& landed)
{
    if(!Finite(y)||!Finite(target)||!Finite(dt)||dt<0)return false;
    float next=y;bool hit=false;
    if(y>target){next=y-18*dt;if(!Finite(next))return false;if(next<target){next=target;hit=true;}}
    out=next;landed=hit;return true;
}
float FusionBungee313BInitialMarkerY(int row)
{return 14.f-row*1.5f+.68f*(.9529412f-2.11f);}
bool TryGetFusionHeavy313BRootDelta(uint16_t kind,const char* state,FusionZombieClipPosition a,FusionZombieClipPosition b,bool right,double& out)
{
    if(!state)return false;
    for(const auto& curve:rootCurves)if(curve.kind==kind&&std::string_view(curve.state)==state)
    {
        if(!Finite(a.seconds)||!Finite(b.seconds)||a.seconds<0||b.seconds<0||a.seconds>curve.duration||b.seconds>curve.duration||
            b.loop<a.loop||(a.loop==b.loop&&b.seconds<a.seconds)||(!curve.loop&&(a.loop||b.loop)))return false;
        const double delta=(double(b.loop-a.loop)*(double(Sample(curve.keys,curve.duration))-Sample(curve.keys,0))+
            Sample(curve.keys,b.seconds)-Sample(curve.keys,a.seconds))*curve.scale;
        if(!std::isfinite(delta))return false;out=right?-delta:delta;return true;
    }
    return false;
}
}
