#include "FusionWorldParticle269Rules.h"
#include <algorithm>
#include <cmath>
#include <utility>
namespace LawnFusion
{
bool SampleFusionWorldParticle269Curve(std::span<const FusionWorldParticle269Key> keys,float t,float& value)
{
    if(keys.empty()||keys.size()>128||!std::isfinite(t)||t<0||t>1)return false;
    for(std::size_t i=0;i<keys.size();++i)
        if(!std::isfinite(keys[i].time)||!std::isfinite(keys[i].value)||!std::isfinite(keys[i].inSlope)||!std::isfinite(keys[i].outSlope)||keys[i].time<0||keys[i].time>1||(i&&keys[i].time<=keys[i-1].time))return false;
    if(t<=keys.front().time){value=keys.front().value;return true;}
    if(t>=keys.back().time){value=keys.back().value;return true;}
    for(std::size_t i=1;i<keys.size();++i)if(t<=keys[i].time)
    {
        const auto& a=keys[i-1];const auto& b=keys[i];const double h=b.time-a.time,u=(t-a.time)/h,u2=u*u,u3=u2*u;
        const double result=(2*u3-3*u2+1)*a.value+(u3-2*u2+u)*h*a.outSlope+(-2*u3+3*u2)*b.value+(u3-u2)*h*b.inSlope;
        if(!std::isfinite(result))return false;value=float(result);return true;
    }
    return false;
}
bool BuildFusionWorldParticle269Friction(std::span<const FusionWorldParticle269Key> keys,int ticks,std::vector<FusionWorldParticle269Sample>& output)
{
    if(ticks<2||ticks>10001)return false;
    std::vector<FusionWorldParticle269Sample> values;values.reserve(ticks);float previous=0;
    if(!SampleFusionWorldParticle269Curve(keys,0,previous)||previous<0||previous>1.000001f)return false;
    values.push_back({0,previous==0?1.f:0.f,previous==0?1.f:0.f});
    for(int i=1;i<ticks;++i)
    {
        const float t=float(i)/float(ticks-1);float current=0;
        if(!SampleFusionWorldParticle269Curve(keys,t,current)||current< -1e-6f||current>previous+1e-6f)return false;
        current=std::max(0.f,current);const float coefficient=previous>0?1-current/previous:1;
        values.push_back({t,std::clamp(coefficient,0.f,1.f),std::clamp(coefficient,0.f,1.f)});previous=current;
    }
    output=std::move(values);return true;
}
bool TryGetFusionWorldParticle269Orbit(float velocity,float step,float& circle,float& away)
{
    if(!std::isfinite(velocity)||!std::isfinite(step)||step<=0||step>1||std::abs(double(velocity)*step)>=1)return false;
    const double angle=double(velocity)*step,a=std::tan(angle)/step,b=(std::cos(angle)-1)/step;
    if(!std::isfinite(a)||!std::isfinite(b))return false;circle=float(a);away=float(b);return true;
}
bool ComposeFusionWorldParticle269LocalBasis(std::span<const FusionWorldParticle269Transform> chain,std::array<float,9>& output)
{
    if(chain.empty()||chain.size()>64)return false;
    std::array<double,9> matrix={1,0,0,0,1,0,0,0,1};
    for(const auto& t:chain)
    {
        for(float p:t.position)if(!std::isfinite(p)||std::abs(p)>10000)return false;
        for(float s:t.scale)if(!std::isfinite(s)||s<=0||s>1000)return false;
        double norm=0;for(float q:t.rotation){if(!std::isfinite(q))return false;norm+=double(q)*q;}
        if(std::abs(norm-1)>1e-4)return false;
        const double length=std::sqrt(norm),x=t.rotation[0]/length,y=t.rotation[1]/length,z=t.rotation[2]/length,w=t.rotation[3]/length;
        const std::array<double,9> r={1-2*(y*y+z*z),2*(x*y-w*z),2*(x*z+w*y),
            2*(x*y+w*z),1-2*(x*x+z*z),2*(y*z-w*x),2*(x*z-w*y),2*(y*z+w*x),1-2*(x*x+y*y)};
        std::array<double,9> next={};
        for(int row=0;row<3;++row)for(int col=0;col<3;++col)for(int k=0;k<3;++k)next[row*3+col]+=matrix[row*3+k]*r[k*3+col];
        matrix=next;
    }
    std::array<float,9> result;
    for(int row=0;row<3;++row)for(int col=0;col<3;++col)result[row*3+col]=float(matrix[row*3+col]*chain.back().scale[col]);
    output=result;return true;
}
bool ProjectFusionWorldParticle269Rectangle(const std::array<float,9>& b,const std::array<float,2>& half,
    float px,float py,FusionWorldParticle269Rectangle& output)
{
    if(!std::isfinite(px)||!std::isfinite(py)||px<=0||py<=0||px>10000||py>10000)return false;
    for(float h:half)if(!std::isfinite(h)||h<=0||h>1000)return false;
    for(float v:b)if(!std::isfinite(v)||std::abs(v)>1000)return false;
    double length[3]={};for(int c=0;c<3;++c){for(int r=0;r<3;++r)length[c]+=double(b[r*3+c])*b[r*3+c];if(length[c]<1e-12)return false;}
    for(int a=0;a<3;++a)for(int c=a+1;c<3;++c)
    {double dot=0;for(int r=0;r<3;++r)dot+=double(b[r*3+a])*b[r*3+c];if(std::abs(dot)>1e-5*std::sqrt(length[a]*length[c]))return false;}
    const double determinant=double(b[0])*(double(b[4])*b[8]-double(b[5])*b[7])-double(b[1])*(double(b[3])*b[8]-double(b[5])*b[6])+double(b[2])*(double(b[3])*b[7]-double(b[4])*b[6]);
    if(determinant<=0)return false;
    const double ux=double(b[0])*half[0]*px,uy=-double(b[3])*half[0]*py,
        vx=double(b[1])*half[1]*px,vy=-double(b[4])*half[1]*py;
    const double epsilon=1e-5*(std::max)({1.,std::abs(ux),std::abs(uy),std::abs(vx),std::abs(vy)});
    const auto axis=[=](double x,double y){return std::abs(x)<=epsilon?(std::abs(y)<=epsilon?0:2):std::abs(y)<=epsilon?1:-1;};
    const int u=axis(ux,uy),v=axis(vx,vy);
    if(u<0||v<0||(u&&u==v))return false;
    const double nx=double(b[2])*px,ny=-double(b[5])*py,speed=std::hypot(nx,ny);
    FusionWorldParticle269Rectangle result;
    result.halfX=float((u==1?std::abs(ux):0)+(v==1?std::abs(vx):0));
    result.halfY=float((u==2?std::abs(uy):0)+(v==2?std::abs(vy):0));
    if(speed>1e-5)
    {
        result.launchDegrees=float(std::atan2(nx,ny)*180/3.14159265358979323846);
        // Native Tod interprets the constant angle0 as a random direction.
        if(result.launchDegrees==0)result.launchDegrees=360;
        result.launchSpeedScale=float(speed);
    }
    output=result;return true;
}
}
