#include "FusionSnowZombie363Rules.h"
#include <cmath>
namespace LawnFusion { namespace {
struct Key {double t,x,inSlope,outSlope;};
struct Curve {const Key* keys;unsigned count;double scale,duration;bool loop;};
static const Key k47_1[]={{0.0,2.386,0.0,-0.5539999},{0.5,2.109,-0.646,-0.646},{0.6666667,1.986,-0.7824377,-0.7824375},{1.2,1.545,-0.55968714,-0.55968714},{1.3333334,1.506,-0.36724988,-0.3672498},{1.8333334,1.285,-0.22100008,-0.22100008},{2.0,1.285,-0.37333336,-0.3733333},{2.75,0.725,-0.7466666,0.0}};
static const Key k48_1[]={{0.0,2.386,0.0,-0.5539999},{0.5,2.109,-0.646,-0.646},{0.6666667,1.986,-0.7824377,-0.7824375},{1.2,1.545,-0.55968714,-0.55968714},{1.3333334,1.506,-0.36724988,-0.3672498},{1.8333334,1.285,-0.22100008,-0.22100008},{2.0,1.285,-0.37333336,-0.3733333},{2.75,0.725,-0.7466666,0.0}};
double Sample(const Curve& c,double t){const auto* k=c.keys;if(t<=k[0].t)return k[0].x;for(unsigned i=1;i<c.count;++i){const auto& a=k[i-1];const auto& b=k[i];if(t<=b.t){if(t==b.t)return b.x;if(!std::isfinite(a.outSlope)||!std::isfinite(b.inSlope))return a.x;const double d=b.t-a.t,u=(t-a.t)/d,u2=u*u,u3=u2*u;return (2*u3-3*u2+1)*a.x+(u3-2*u2+u)*d*a.outSlope+(-2*u3+3*u2)*b.x+(u3-u2)*d*b.inSlope;}}return k[c.count-1].x;}
bool CurveFor(uint16_t kind,uint8_t state,Curve& c){switch((unsigned(kind)<<8)|state){
case 12033: c={k47_1,8,1.2,2.75,true};return true;
case 12289: c={k48_1,8,1.2,2.75,true};return true;
default:return false;}}
}
bool SourceRootDeltaSnow363(uint16_t kind,uint8_t state,FusionSourceGraph313Cursor a,FusionSourceGraph313Cursor b,double& out){const auto* g=FindFusionSnow363Graph(kind);if(!g||state>=g->stateCount||!FusionSourceGraph313CursorValid(g->states[state],a)||!FusionSourceGraph313CursorValid(g->states[state],b)||b.loop<a.loop||(b.loop==a.loop&&b.seconds<a.seconds))return false;Curve c;if(!CurveFor(kind,state,c)){out=0;return true;}const double d=(double(b.loop-a.loop)*(Sample(c,c.duration)-Sample(c,0))+Sample(c,b.seconds)-Sample(c,a.seconds))*c.scale;if(!std::isfinite(d))return false;out=d;return true;}
}
