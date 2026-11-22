#include "FusionLateZombie363Rules.h"
#include <cmath>
namespace LawnFusion { namespace {
struct Key {double t,x,inSlope,outSlope;};
struct Curve {const Key* keys;unsigned count;double scale,duration;bool loop;};
static const Key k209_1[]={{0.0,1.3935484,0.0,0.0},{0.9166667,0.23548387,-0.2024813,-0.20248128},{1.0833334,0.18064517,-0.62672824,-0.6267281},{1.2666667,-0.31612903,-2.7834108,-2.783411},{1.4333333,-0.7935484,-1.7294487,-1.7294487},{1.7833333,-1.2096775,4.3211617e-07,0.0}};
double Sample(const Curve& c,double t){const auto* k=c.keys;if(t<=k[0].t)return k[0].x;for(unsigned i=1;i<c.count;++i){const auto& a=k[i-1];const auto& b=k[i];if(t<=b.t){if(t==b.t)return b.x;if(!std::isfinite(a.outSlope)||!std::isfinite(b.inSlope))return a.x;const double d=b.t-a.t,u=(t-a.t)/d,u2=u*u,u3=u2*u;return (2*u3-3*u2+1)*a.x+(u3-2*u2+u)*d*a.outSlope+(-2*u3+3*u2)*b.x+(u3-u2)*d*b.inSlope;}}return k[c.count-1].x;}
bool CurveFor(uint16_t kind,uint8_t state,Curve& c){switch((unsigned(kind)<<8)|state){
case 53505: c={k209_1,6,0.31,2.0,true};return true;
default:return false;}}
}
bool SourceRootDeltaLate363(uint16_t kind,uint8_t state,FusionSourceGraph313Cursor a,FusionSourceGraph313Cursor b,double& out){const auto* g=FindFusionLate363Graph(kind);if(!g||state>=g->stateCount||!FusionSourceGraph313CursorValid(g->states[state],a)||!FusionSourceGraph313CursorValid(g->states[state],b)||b.loop<a.loop||(b.loop==a.loop&&b.seconds<a.seconds))return false;Curve c;if(!CurveFor(kind,state,c)){out=0;return true;}const double d=(double(b.loop-a.loop)*(Sample(c,c.duration)-Sample(c,0))+Sample(c,b.seconds)-Sample(c,a.seconds))*c.scale;if(!std::isfinite(d))return false;out=d;return true;}
}
