#include "FusionSourceRootQing369.h"
#include <cmath>
namespace LawnFusion {
const FusionSourceGraph313* FindFusionSourceGraph208(uint16_t);
namespace {
struct Key {double t,x,inSlope,outSlope;};
struct Curve {const Key* keys;unsigned count;double scale,duration;};
static const Key k208_1[]={{0.0,3.988604,0.0,-1.4871169},{2.0,1.0143702,-1.4871174,0.0}};
static const Key k208_2[]={{0.5833333,-13.399707,0.0,0.0},{1.0,-25.98541,8.641916e-06,0.0}};
double Sample(const Curve& c,double t){const auto* k=c.keys;if(t<=k[0].t)return k[0].x;for(unsigned i=1;i<c.count;++i){const auto& a=k[i-1];const auto& b=k[i];if(t<=b.t){if(t==b.t)return b.x;if(!std::isfinite(a.outSlope)||!std::isfinite(b.inSlope))return a.x;const double d=b.t-a.t,u=(t-a.t)/d,u2=u*u,u3=u2*u;return (2*u3-3*u2+1)*a.x+(u3-2*u2+u)*d*a.outSlope+(-2*u3+3*u2)*b.x+(u3-u2)*d*b.inSlope;}}return k[c.count-1].x;}
bool CurveFor(uint8_t state,Curve& c){switch(state){case 1: c={k208_1,2,0.31941,2.0};return true;
case 2: c={k208_2,2,0.31941,1.1666666};return true; default:return false;}}
}
bool SourceRootDeltaQing369(uint16_t kind,uint8_t state,FusionSourceGraph313Cursor a,FusionSourceGraph313Cursor b,double& out)
{
 const auto* g=FindFusionSourceGraph208(kind);
 if(!g||state>=g->stateCount||!FusionSourceGraph313CursorValid(g->states[state],a)||!FusionSourceGraph313CursorValid(g->states[state],b)||b.loop<a.loop||(b.loop==a.loop&&b.seconds<a.seconds))return false;
 Curve c;if(!CurveFor(state,c)){out=0;return true;}
 // Same native source-root projection policy as SourceRootGround343/Snow363.
 // State offsets are never teleported into the real carrier; only local delta.
 const double delta=(double(b.loop-a.loop)*(Sample(c,c.duration)-Sample(c,0))+Sample(c,b.seconds)-Sample(c,a.seconds))*c.scale;
 if(!std::isfinite(delta))return false;out=delta;return true;
}
}
