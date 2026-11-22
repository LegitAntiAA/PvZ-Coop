#include "FusionNextZombie343Rules.h"
#include <cmath>
namespace LawnFusion { namespace {
struct Key {double t,x,inSlope,outSlope;};
struct Curve {const Key* keys;unsigned count;double scale,duration;bool loop;};
static const Key k112_1[]={{0.0,4.531,0.0,-2.4624698},{2.0,-0.39393938,-2.4624689,0.0}};
static const Key k112_4[]={{0.0,4.531,0.0,-2.4624698},{2.0,-0.39393938,-2.4624689,0.0}};
static const Key k112_6[]={{0.0,4.531,0.0,-2.4624698},{2.0,-0.39393938,-2.4624689,0.0}};
static const Key k216_2[]={{0.0,4.531,0.0,-2.4624698},{2.0,-0.39393938,-2.4624689,0.0}};
static const Key k216_3[]={{0.0,4.531,0.0,-2.4624698},{2.0,-0.39393938,-2.4624689,0.0}};
static const Key k200_0[]={{0.0,-1.97375,0.0,-INFINITY},{0.5,-1.97375,0.0,-INFINITY},{1.0,-1.97375,0.0,0.0}};
static const Key k200_1[]={{0.0,-0.18,0.0,-2.4950004},{0.5,-1.4275002,-2.4950008,-2.4950001},{1.0,-2.6750002,-2.4949996,0.0}};
static const Key k211_0[]={{0.0,-1.97375,0.0,0.0},{1.0,-1.97375,0.0,0.0}};
static const Key k211_1[]={{0.0,-1.97375,0.0,0.0},{1.0,-1.97375,0.0,0.0}};
static const Key k217_1[]={{0.0,1.9616,0.0,0.0},{3.0,1.5680001,-1.1175871e-08,0.0}};
static const Key k217_2[]={{0.0,4.1048,0.0,0.0},{0.75,4.0376,-0.1792005,-0.17920049},{1.5,3.5416,-0.88177794,-0.8817779},{3.0,1.2688,1.1920929e-07,-INFINITY},{3.5,1.2688,0.0,0.0}};
static const Key k217_3[]={{0.0,5.131,0.0,0.0},{0.35,5.0512,-0.26923028,-0.26923034},{0.65,4.9560003,-0.3155994,-0.31559947},{1.0166667,4.8408003,4.5058783e-08,0.0},{1.3,4.8888,2.6227122e-08,0.0},{1.6833333,4.7784,-0.303273,-0.303273},{2.0333333,4.6664,-0.35494748,-0.35494754},{2.3166666,4.5536,-0.36685672,-0.3668567},{2.5,4.4952,-0.24436378,-0.24436378},{2.6833334,4.4639997,-5.983824e-10,0.0},{2.9833333,4.4751997,0.046769667,0.046769667},{3.3333333,4.4944,-1.0605386e-08,0.0}};
double Sample(const Curve& c,double t){const auto* k=c.keys;if(t<=k[0].t)return k[0].x;for(unsigned i=1;i<c.count;++i){const auto& a=k[i-1];const auto& b=k[i];if(t<=b.t){if(t==b.t)return b.x;if(!std::isfinite(a.outSlope)||!std::isfinite(b.inSlope))return a.x;const double d=b.t-a.t,u=(t-a.t)/d,u2=u*u,u3=u2*u;return (2*u3-3*u2+1)*a.x+(u3-2*u2+u)*d*a.outSlope+(-2*u3+3*u2)*b.x+(u3-u2)*d*b.inSlope;}}return k[c.count-1].x;}
bool CurveFor(uint16_t kind,uint8_t state,Curve& c){switch((unsigned(kind)<<8)|state){
case 28673: c={k112_1,2,0.33,2.0,true};return true;
case 28676: c={k112_4,2,0.33,2.0,true};return true;
case 28678: c={k112_6,2,0.33,2.0,true};return true;
case 55298: c={k216_2,2,0.3,2.0,true};return true;
case 55299: c={k216_3,2,0.3,2.0,true};return true;
case 51200: c={k200_0,3,0.8,1.0166667,true};return true;
case 51201: c={k200_1,3,0.8,1.0166667,true};return true;
case 54016: c={k211_0,2,0.7,1.0,true};return true;
case 54017: c={k211_1,2,0.7,1.0,true};return true;
case 55553: c={k217_1,2,1.25,3.0,true};return true;
case 55554: c={k217_2,5,1.25,3.5,false};return true;
case 55555: c={k217_3,12,1.25,3.3333333,true};return true;
default:return false;}}
}
bool SourceRootDelta343(uint16_t kind,uint8_t state,FusionSourceGraph313Cursor a,FusionSourceGraph313Cursor b,double& out){const auto* g=FindFusionSourceGraph343(kind);if(!g||state>=g->stateCount||!FusionSourceGraph313CursorValid(g->states[state],a)||!FusionSourceGraph313CursorValid(g->states[state],b)||b.loop<a.loop||(b.loop==a.loop&&b.seconds<a.seconds))return false;Curve c;if(!CurveFor(kind,state,c)){out=0;return true;}const double d=(double(b.loop-a.loop)*(Sample(c,c.duration)-Sample(c,0))+Sample(c,b.seconds)-Sample(c,a.seconds))*c.scale;if(!std::isfinite(d))return false;out=d;return true;}
}
