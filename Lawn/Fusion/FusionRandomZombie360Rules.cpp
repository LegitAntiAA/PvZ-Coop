#include "FusionRandomZombie360Rules.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace LawnFusion
{
namespace {constexpr uint16_t sourcePrefabs[]={0,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224};
constexpr FusionZombieProfile profiles[]={{105,270,370,0,50},{110,270,720,0,50},{215,20000,1100,0,50}};
}
const FusionZombieProfile* FindFusionRandomZombie360Profile(uint16_t k){for(const auto& p:profiles)if(p.kind==k)return &p;return nullptr;}
std::span<const uint16_t> FusionRandom360SourcePrefabs(){return sourcePrefabs;}
bool FusionRandom360NotRandom(uint16_t k)
{switch(k){case 21:case 22:case 23:case 25:case 26:case 29:case 41:case 42:case 43:case 44:case 46:case 116:case 119:case 205:case 206:case 211:case 212:case 218:case 219:case 220:case 221:case 222:case 223:case 224:return true;default:return false;}}
bool FusionRandom360UselessHypno(uint16_t k)
{if(FusionRandom360NotRandom(k))return true;switch(k){case 3:case 6:case 7:case 17:case 19:case 27:case 28:case 32:case 34:case 113:case 202:return true;default:return false;}}
bool BuildFusionRandom360Pool(uint16_t parent,bool controlled,std::vector<uint16_t>& out,std::string* error)
{
 if(!FusionRandomZombie360Kind(parent)){if(error)*error="Unknown source random parent";return false;}
 std::vector<uint16_t> next;
 for(uint16_t k:sourcePrefabs)if((parent==215?k>=200:k<150)&&!FusionRandom360NotRandom(k)&&(!controlled||!FusionRandom360UselessHypno(k)))next.push_back(k);
 if(next.empty()){if(error)*error="Source random pool empty";return false;}
 out.swap(next);if(error)error->clear();return true;
}
bool ResolveFusionRandom360DirectKind(uint16_t k,bool water,uint16_t& emitted)
{
 if(!std::binary_search(std::begin(sourcePrefabs),std::end(sourcePrefabs),k))return false;
 auto result=k;
 if(water)
 {
  if(k==0)result=11;else if(k==2)result=12;else if(k==4)result=13;
  else switch(k){case 11:case 12:case 13:case 14:case 17:case 19:case 25:case 26:case 27:case 29:case 113:case 119:case 200:case 205:case 206:case 214:case 221:break;default:return false;}
 }
 emitted=result;return true;
}
bool ScaleFusionRandom360Health(const FusionRandom360Health& in,float factor,FusionRandom360Health& out)
{
 if(!std::isfinite(in.health)||!std::isfinite(factor)||factor<=0||in.maximum<0||in.first<0||in.firstMaximum<0||in.second<0||in.secondMaximum<0)return false;
 auto next=in;const float health=in.health*factor;if(!std::isfinite(health))return false;next.health=health;
 const auto scale=[&](int value,int& result){const float f=float(value)*factor;if(!std::isfinite(f)||double(f)>(std::numeric_limits<int>::max)())return false;result=int(f);return true;};
 if(!scale(in.maximum,next.maximum)||!scale(in.first,next.first)||!scale(in.firstMaximum,next.firstMaximum)||!scale(in.second,next.second)||!scale(in.secondMaximum,next.secondMaximum))return false;
 out=next;return true;
}
bool FusionRandom360ScaleFromDraw(uint16_t parent,int draw,float& factor)
{
 float f=1;
 if(parent==105){if(draw<1||draw>=7)return false;f=float(draw)/5;}
 else if(parent==215){if(draw<4||draw>=7)return false;f=float(draw)/5;}
 else if(parent!=110||draw!=0)return false;
 factor=f;return true;
}
bool FusionRandom360ArmorStage(uint16_t k,int hp,int maximum,uint8_t before,uint8_t& out)
{
 if(!FusionRandomZombie360Kind(k)||hp<0||maximum<=0||maximum>(std::numeric_limits<int>::max)()/2||before>2)return false;
 const int twoThirds=maximum*2/3,third=maximum/3;
 uint8_t next=before;
 if(k==215)next=hp>twoThirds?0:hp<third?2:1;
 else{if(hp<twoThirds&&next<1)next=1;if(hp<third&&next<2)next=2;}
 out=next;return true;
}
bool PlanFusionRandom360ArmorDamage(uint16_t k,int armor,int maximum,uint8_t before,int damage,bool dying,FusionRandom360ArmorDamage& out)
{
 if(!FusionRandomZombie360Kind(k)||armor<0||maximum<=0||before>2||damage<0)return false;
 FusionRandom360ArmorDamage p;p.armor=damage>=armor?0:armor-damage;p.stage=before;p.fall=damage>=armor&&!dying;
 if(p.armor&&!FusionRandom360ArmorStage(k,p.armor,maximum,before,p.stage))return false;
 // Random.FirstArmorTakeDamage alwaysabsorbsalloverflow, including lethal hits.
 out=p;return true;
}
}
