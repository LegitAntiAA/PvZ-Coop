#include "FusionExtendedZombie347Rules.h"
#include "FusionSourceGraph313.h"
#include <cmath>
namespace LawnFusion { const FusionSourceGraph313* FindFusionSourceGraph347(uint16_t); namespace {
struct Key {double t,x,inSlope,outSlope;};
struct Curve {const Key* keys;unsigned count;double scale,duration;bool loop;};
static const Key k202_1[]={{0.0,1.9616,0.0,0.0},{3.0,1.5680001,-1.1175871e-08,0.0}};
static const Key k202_2[]={{0.0,5.131,0.0,0.0},{0.35,5.0848,-0.23107746,-0.23107749},{0.65,4.9807997,-0.3419996,-0.34199956},{1.0166667,4.8568,3.7498587e-08,0.0},{1.3,4.9272,-1.02942316e-07,0.0},{1.6833333,4.8287997,-0.2923634,-0.29236338},{2.0333333,4.7128,-0.33726257,-0.33726266},{2.3166666,4.6152,-0.28676912,-0.2867691},{2.6833334,4.5264,4.0772832e-08,0.0},{2.8333333,4.5344,0.10666531,0.106665306},{2.9833333,4.5592,0.0064011076,0.0064010634},{3.3333333,4.56,4.191303e-10,0.0}};
static const Key k204_3[]={{0.0,6.73,0.0,-0.24000025},{2.0,6.2499995,-0.24000025,0.0}};
static const Key k210_1[]={{0.0,0.0,0.0,-6.0},{2.0,-12.0,-6.0,0.0}};
static const Key k218_3[]={{0.0,0.0,0.0,0.0},{2.5,0.0,0.0,0.0}};
static const Key k218_4[]={{0.0,0.0,0.0,-0.44210523},{3.1666667,-1.4,-0.44210508,0.0}};
static const Key k218_6[]={{0.0,0.0,0.0,-0.44210523},{3.1666667,-1.4,-0.44210508,0.0}};
static const Key k219_1[]={{0.0,0.0,0.0,-6.0},{2.0,-12.0,-6.0,0.0}};
static const Key k220_3[]={{0.0,6.73,0.0,-0.24000025},{2.0,6.2499995,-0.24000025,0.0}};
static const Key k221_1[]={{0.0,38.05,0.0,-2.0484},{4.1666665,29.515,-2.0483994,0.0}};
static const Key k221_2[]={{4.1666665,38.05,0.0,0.0}};
static const Key k221_3[]={{0.0,38.05,0.0,-0.8888004},{4.1666665,34.346664,-0.8888004,0.0}};
static const Key k224_2[]={{0.0,0.0,0.0,0.0},{0.75,0.0,-0.63800013,-0.638},{1.75,-1.276,-1.276,0.0}};
double Sample(const Curve& c,double t){const auto* k=c.keys;if(t<=k[0].t)return k[0].x;for(unsigned i=1;i<c.count;++i){const auto& a=k[i-1];const auto& b=k[i];if(t<=b.t){if(t==b.t)return b.x;if(!std::isfinite(a.outSlope)||!std::isfinite(b.inSlope))return a.x;const double d=b.t-a.t,u=(t-a.t)/d,u2=u*u,u3=u2*u;return (2*u3-3*u2+1)*a.x+(u3-2*u2+u)*d*a.outSlope+(-2*u3+3*u2)*b.x+(u3-u2)*d*b.inSlope;}}return k[c.count-1].x;}
bool CurveFor(uint16_t kind,uint8_t state,Curve& c){switch((unsigned(kind)<<8)|state){
case 51713: c={k202_1,2,1.25,3.0,true};return true;
case 51714: c={k202_2,12,1.25,3.3333333,true};return true;
case 52227: c={k204_3,2,1.2,2.0,true};return true;
case 53761: c={k210_1,2,0.25,2.0,true};return true;
case 55811: c={k218_3,2,0.5,2.5,false};return true;
case 55812: c={k218_4,2,0.5,3.1666667,true};return true;
case 55814: c={k218_6,2,0.5,3.1666667,true};return true;
case 56065: c={k219_1,2,0.4,2.0,true};return true;
case 56323: c={k220_3,2,1.5,2.0,true};return true;
case 56577: c={k221_1,2,0.3,4.1666665,true};return true;
case 56578: c={k221_2,1,0.3,4.1666665,false};return true;
case 56579: c={k221_3,2,0.3,4.1666665,true};return true;
case 57346: c={k224_2,3,0.4,2.5166667,true};return true;
default:return false;}}
}
bool SourceRootDelta347(uint16_t kind,uint8_t state,FusionSourceGraph313Cursor a,FusionSourceGraph313Cursor b,double& out){const auto* g=FindFusionSourceGraph347(kind);if(!g||state>=g->stateCount||!FusionSourceGraph313CursorValid(g->states[state],a)||!FusionSourceGraph313CursorValid(g->states[state],b)||b.loop<a.loop||(b.loop==a.loop&&b.seconds<a.seconds))return false;Curve c;if(!CurveFor(kind,state,c)){out=0;return true;}const double d=(double(b.loop-a.loop)*(Sample(c,c.duration)-Sample(c,0))+Sample(c,b.seconds)-Sample(c,a.seconds))*c.scale;if(!std::isfinite(d))return false;out=d;return true;}
}
