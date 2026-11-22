#include "FusionNativeFinish363.h"
#include <cmath>
namespace LawnFusion { namespace {
struct Key {double t,x,inSlope,outSlope;};
struct Curve {const Key* keys;unsigned count;double scale,duration;bool loop;};
static const Key k115_2[]={{0.0,3.1064515,0.0,-1.9102385},{0.38333333,2.3741934,-1.7032251,-1.7032256},{0.6666667,1.9709678,-9.8540255e-08,0.0},{0.81666666,2.0225806,-2.9481981e-08,0.0},{0.96666664,1.9612902,-0.865276,-0.8652759},{1.1,1.6935483,-2.5161293,-2.5161285},{1.3333334,1.0387096,-2.1977112,-2.1977108},{1.6166667,0.55806446,-1.6963955,0.0}};
static const Key k116_2[]={{0.0,3.1064515,0.0,-1.9102385},{0.38333333,2.3741934,-1.7032251,-1.7032256},{0.6666667,1.9709678,-9.8540255e-08,0.0},{0.81666666,2.0225806,-2.9481981e-08,0.0},{0.96666664,1.9612902,-0.865276,-0.8652759},{1.1,1.6935483,-2.5161293,-2.5161285},{1.3333334,1.0387096,-2.1977112,-2.1977108},{1.6166667,0.55806446,-1.6963955,0.0}};
static const Key k117_3[]={{0.0,-0.81666666,0.0,-0.7999996},{0.16666667,-0.9499999,-1.5999992,-1.5999992},{0.33333334,-1.39,-2.3400002,-2.3400002},{0.5,-1.7299999,-1.57,-1.5699999},{0.6666667,-1.9133333,-0.3599997,-0.35999972},{0.8333333,-1.9433333,4.0829256e-08,-INFINITY},{1.0,-1.9433333,0.0,0.0},{1.1666666,-2.4266665,-2.9500003,-2.95},{1.3333334,-2.9266667,-2.1599998,-2.1599998},{1.5,-3.1466665,-1.5299997,-1.5299996},{1.6666666,-3.4366665,2.6702745e-07,0.0}};
static const Key k117_5[]={{0.0,3.1064515,0.0,-1.9102385},{0.38333333,2.3741934,-1.7032251,-1.7032256},{0.6666667,1.9709678,-9.8540255e-08,0.0},{0.81666666,2.0225806,-2.9481981e-08,0.0},{0.96666664,1.9612902,-0.865276,-0.8652759},{1.1,1.6935483,-2.5161293,-2.5161285},{1.3333334,1.0387096,-2.1977112,-2.1977108},{1.6166667,0.55806446,-1.6963955,0.0}};
double Sample(const Curve& c,double t){const auto* k=c.keys;if(t<=k[0].t)return k[0].x;for(unsigned i=1;i<c.count;++i){const auto& a=k[i-1];const auto& b=k[i];if(t<=b.t){if(t==b.t)return b.x;if(!std::isfinite(a.outSlope)||!std::isfinite(b.inSlope))return a.x;const double d=b.t-a.t,u=(t-a.t)/d,u2=u*u,u3=u2*u;return (2*u3-3*u2+1)*a.x+(u3-2*u2+u)*d*a.outSlope+(-2*u3+3*u2)*b.x+(u3-u2)*d*b.inSlope;}}return k[c.count-1].x;}
bool CurveFor(uint16_t kind,uint8_t state,Curve& c){switch((unsigned(kind)<<8)|state){
case 29442: c={k115_2,8,0.31,1.9166666,true};return true;
case 29698: c={k116_2,8,0.31,1.9166666,true};return true;
case 29955: c={k117_3,11,0.31,1.9166666,true};return true;
case 29957: c={k117_5,8,0.31,1.9166666,true};return true;
default:return false;}}
}
bool SourceRootDeltaFinish363(uint16_t kind,uint8_t state,FusionSourceGraph313Cursor a,FusionSourceGraph313Cursor b,double& out){const auto* g=FindFusionFinish363Graph(kind);if(!g||state>=g->stateCount||!FusionSourceGraph313CursorValid(g->states[state],a)||!FusionSourceGraph313CursorValid(g->states[state],b)||b.loop<a.loop||(b.loop==a.loop&&b.seconds<a.seconds))return false;Curve c;if(!CurveFor(kind,state,c)){out=0;return true;}const double d=(double(b.loop-a.loop)*(Sample(c,c.duration)-Sample(c,0))+Sample(c,b.seconds)-Sample(c,a.seconds))*c.scale;if(!std::isfinite(d))return false;out=d;return true;}
}
