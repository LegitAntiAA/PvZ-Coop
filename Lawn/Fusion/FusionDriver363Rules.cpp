#include "FusionDriver363Rules.h"
#include <bit>
#include <cmath>
#include <algorithm>
namespace LawnFusion {
bool FusionDriver363BodyStage(uint16_t k,float hp,int max,uint8_t previous,uint8_t& next)
{
 if(!FusionDriver363Kind(k)||!std::isfinite(hp)||hp<=0||max<=0||hp>float(max)||previous>2)return false;
 uint8_t stage=previous;
 //2013576C0 includes low equality in its middle band.222360050 uses
 // source ValueBetween strictlow<HP<=high; both low bands are strict<max/3.
 if(k==201){if(hp>=float(max)/3.f&&hp<(float(max)+float(max))/3.f)stage=1;}
 else if(float(max)*.33333334f<hp&&hp<=float(max)*.6666667f)stage=1;
 if(hp<float(max)/3.f)stage=2;next=stage;return true;
}
bool FusionDriver363Velocity(bool right,bool stopped,bool beforeIce,float& vx)
{vx=stopped?0.f:(right?1.f:-1.f)*(beforeIce?.5f:.8f);return true;}
bool StepFusionDriver363Turn(int32_t before,int32_t& next,bool& point,bool& flipped)
{
 // Exact unchecked C# I32 increment. Modulo uses the resulting signed value;
 // quaternion parity remains defined through max->min and -1->0 wraps.
 const uint32_t bits=uint32_t(before)+1U;const int32_t after=std::bit_cast<int32_t>(bits);
 next=after;point=after%9==0;flipped=(bits&1U)!=0;return true;
}
bool FusionDriver363RawDamage(uint16_t k,int damage,int& resolved)
{if(!FusionDriver363Kind(k)||damage<0)return false;resolved=k==222?(std::min)(5000,damage):damage;return true;}
}
