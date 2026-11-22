#include "FusionGroundShooter343Rules.h"
#include <cmath>
namespace LawnFusion { namespace {
struct Key {double t,x,inSlope,outSlope;};
struct Curve {const Key* keys;unsigned count;double scale,duration;bool loop;};
static const Key k114_3[]={{0.0,3.1064515,0.0,-1.9102385},{0.38333333,2.3741934,-1.7032251,-1.7032256},{0.6666667,1.9709678,-9.8540255e-08,0.0},{0.81666666,2.0225806,-2.9481981e-08,0.0},{0.96666664,1.9612902,-0.865276,-0.8652759},{1.1,1.6935483,-2.5161293,-2.5161285},{1.3333334,1.0387096,-2.1977112,-2.1977108},{1.6166667,0.55806446,-1.6963955,0.0}};
static const Key k114_5[]={{0.0,3.1064515,0.0,-1.9102385},{0.38333333,2.3741934,-1.7032251,-1.7032256},{0.6666667,1.9709678,-9.8540255e-08,0.0},{0.81666666,2.0225806,-2.9481981e-08,0.0},{0.96666664,1.9612902,-0.865276,-0.8652759},{1.1,1.6935483,-2.5161293,-2.5161285},{1.3333334,1.0387096,-2.1977112,-2.1977108},{1.6166667,0.55806446,-1.6963955,0.0}};
static const Key k118_3[]={{0.0,6.73,0.0,-0.24000025},{2.0,6.2499995,-0.24000025,0.0}};
static const Key k119_1[]={{0.0,7.2322583,0.0,-1.3741937},{2.0,4.483871,-1.3741937,0.0}};
static const Key k119_3[]={{0.0,7.2322583,0.0,-1.431452},{0.8,6.0870967,-0.7399195,-0.73991954},{1.0,6.0774193,-0.6633069,-0.6633065},{1.8,5.0548387,-1.2782259,0.0}};
static const Key k119_5[]={{0.0,7.2322583,0.0,0.0}};
double Sample(const Curve& c,double t){const auto* k=c.keys;if(t<=k[0].t)return k[0].x;for(unsigned i=1;i<c.count;++i){const auto& a=k[i-1];const auto& b=k[i];if(t<=b.t){if(t==b.t)return b.x;if(!std::isfinite(a.outSlope)||!std::isfinite(b.inSlope))return a.x;const double d=b.t-a.t,u=(t-a.t)/d,u2=u*u,u3=u2*u;return (2*u3-3*u2+1)*a.x+(u3-2*u2+u)*d*a.outSlope+(-2*u3+3*u2)*b.x+(u3-u2)*d*b.inSlope;}}return k[c.count-1].x;}
bool CurveFor(uint16_t kind,uint8_t state,Curve& c){switch((unsigned(kind)<<8)|state){
case 29187: c={k114_3,8,0.31,1.9166666,true};return true;
case 29189: c={k114_5,8,0.31,1.9166666,true};return true;
case 30211: c={k118_3,2,1.2,2.0,true};return true;
case 30465: c={k119_1,2,0.31,2.0,true};return true;
case 30467: c={k119_3,4,0.31,2.0166667,true};return true;
case 30469: c={k119_5,1,0.31,1.0,false};return true;
default:return false;}}
}
bool SourceRootDeltaGround343(uint16_t kind,uint8_t state,FusionSourceGraph313Cursor a,FusionSourceGraph313Cursor b,double& out){const auto* g=FindFusionGround343Graph(kind);if(!g||state>=g->stateCount||!FusionSourceGraph313CursorValid(g->states[state],a)||!FusionSourceGraph313CursorValid(g->states[state],b)||b.loop<a.loop||(b.loop==a.loop&&b.seconds<a.seconds))return false;Curve c;if(!CurveFor(kind,state,c)){out=0;return true;}const double d=(double(b.loop-a.loop)*(Sample(c,c.duration)-Sample(c,0))+Sample(c,b.seconds)-Sample(c,a.seconds))*c.scale;if(!std::isfinite(d))return false;out=d;return true;}
}
