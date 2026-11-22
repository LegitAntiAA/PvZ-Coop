#include "FusionTravelBuff369Rules.h"
#include <cmath>
#include <limits>
#include <algorithm>
#include <vector>
#include <utility>
namespace LawnFusion {
bool GetFusionTravelPlantAdvanced369(uint16_t kind,FusionTravelPlantAdvanced369& out)
{for(const auto& entry:kFusionTravelPlantAdvanced369)if(entry.kind==kind){out=entry;return true;}return false;}
bool TryFusionTravelPlantAttack369(int base,bool advanced10,bool connected,int& out)
{
 if(base<0)return false;
 float multiplier=1.f;if(advanced10)multiplier+=.2f;if(connected)multiplier+=.5f;
 const float value=float(base)*multiplier;
 if(!std::isfinite(value)||double(value)>double((std::numeric_limits<int>::max)()))return false;
 out=int(value);return true;
}
bool TryFusionTravelConnectedDamage369(int raw,bool connected,int& out)
{if(raw<0)return false;out=!raw||!connected?raw:raw<4?1:raw/2;return true;}
bool TryFusionTravelSun369(int before,int count,int multiplier,bool advanced25,int& out)
{
 if(before<0||multiplier<0)return false;
 const int64_t value=(int64_t(before)+count)*multiplier,cap=advanced25?100000:50000;
 if(value<0)return false;
 out=int(value<cap?value:cap);return true;
}
bool TryFusionTravelMoney369(int before,int value,bool advanced22,int& money,int& sun)
{
 if(before<0||value<0)return false;
 // Source predicate is >=100000, although ordinary credit caps at99999. This
 // branch is reachable only with an explicitly larger source money balance.
 if(advanced22&&before>=100000){money=99999;sun=value/2;}
 else {money=int((int64_t(before)+value)<99999?int64_t(before)+value:99999);sun=0;}
 return true;
}
int FusionTravelCardStep369(bool enabled){return enabled?2:1;}
int FusionTravelHammerStep369(bool enabled){return enabled?10:1;}
float FusionTravelImitaterSeconds369(uint16_t kind,float ordinary,bool enabled,bool playing)
{return kind==245&&enabled&&playing?50.f:ordinary;}
int FusionTravelLight369(bool enabled){return enabled?4:0;}
bool TryFusionTravelProjectileDamage369(int base,uint16_t kind,bool advanced10,bool snowGatling17,int& out)
{
 if(base<=0||snowGatling17&&kind!=18)return false;
 int value=base;if(!TryFusionTravelPlantAttack369(base,advanced10,false,value))return false;
 if(snowGatling17)
 {
  const float boosted=float(value)*1.5f;
  if(!std::isfinite(boosted)||double(boosted)>double((std::numeric_limits<int>::max)()))return false;
  value=int(boosted);
 }
 out=value;return true;
}
bool FusionTravelProjectilePair369(int base,int actual,uint16_t kind,bool allow10,bool allow17,uint8_t fire)
{
 if(base<=0||actual<=0||actual==base)return false;
 // Board.Yellow/Orange/RedFirePea add fixed source values AFTER the existing
 // pea attack. Their +20/+40/+60 never receive the plant's Advanced10 again.
 if(allow10&&fire&&fire<=3&&(kind==0||kind==10)&&int64_t(base)+4==actual)return true;
 for(unsigned mask=1;mask<4;++mask)
 {
  if((mask&1)&&!allow10||(mask&2)&&(!allow17||kind!=18))continue;
  int value=0;if(TryFusionTravelProjectileDamage369(base,kind,(mask&1)!=0,(mask&2)!=0,value)&&value==actual)return true;
 }
 return false;
}
bool FusionTravelRelayedProjectilePair369(int base,int actual,uint16_t kind,bool allow10,bool allow17,bool allow24)
{
 if(base<=0||actual<=base||base>100000000||actual>100000000)return false;
 // Existing umbrella wire admission already bounds both channels to100M.
 // Each inverse interval is tiny in this F32 domain. Retain only candidates
 // whose exact forward source multiplication recreates BOTH current values.
 std::vector<std::pair<int,int>> pending{{base,actual}};
 for(size_t n=0;n<pending.size()&&n<4096;++n)
 {
  const auto [b,a]=pending[n];if(FusionTravelProjectilePair369(b,a,kind,allow10,allow17))return true;
  for(float factor:{1.75f,2.25f})
  {
   if(factor==2.25f&&!allow24)continue;
   const int cb=int(double(b)/factor),ca=int(double(a)/factor);
   for(int pb=(std::max)(1,cb-16);pb<=cb+16&&pb<b;++pb)
   {
    if(int(float(pb)*factor)!=b)continue;
    for(int pa=(std::max)(pb+1,ca-16);pa<=ca+16&&pa<a;++pa)
    {
     if(int(float(pa)*factor)!=a)continue;const std::pair<int,int> pair{pb,pa};
     if(std::find(pending.begin(),pending.end(),pair)==pending.end())pending.push_back(pair);
    }
   }
  }
 }
 return false;
}
}
