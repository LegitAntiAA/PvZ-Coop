#include "FusionQing208Rules.h"
#include <cmath>
namespace LawnFusion {
bool FusionQing208ViewValid(const FusionQing208View& v)
{return std::isfinite(v.attributeCountdown)&&v.attributeCountdown>=0&&v.attributeCountdown<=15.45f;}
bool FusionQing208TargetStatusAllowed(int s)
{switch(s){case 3:case 7:case 9:case 10:case 12:case 16:case 20:case 21:case 22:case 23:case 24:return false;default:return true;}}
bool FusionQing208PlantAllowed(bool controlled,int rows,bool caltrop,bool flying,bool pumpkin,bool covered)
{return !controlled&&rows==0&&!caltrop&&!flying&&(!covered||pumpkin);}
bool FusionQing208Interval(bool initial,float sample,float& out)
{if(!std::isfinite(sample)||sample<0||sample>1)return false;out=initial?4.4f+(7.53f-4.4f)*sample:8.8f+(15.45f-8.8f)*sample;return true;}
}
