#include "FusionDrown343Rules.h"
#include <algorithm>
#include <cmath>
namespace LawnFusion
{
bool TryMakeFusionDrown343HorizontalVelocity(float draw,bool big,float& vx,float& vy)
{if(!std::isfinite(draw)||draw<12||draw>14)return false;vx=-draw;vy=big?-2.f:-1.f;return true;}
float FusionDrown343FlashRemaining(int age,uint32_t origin,bool longFlash)
{
 if(age<0||!origin||origin>uint32_t(age))return 0;
 const int ticks=(age-int(origin))/2;float t=longFlash?5.f:.3f;
 if(ticks>251)return 0;for(int i=0;i<ticks&&t>0;++i)t-=.02f;return(std::max)(0.f,t);
}
float FusionDrown343Brightness(int age,uint32_t origin,bool longFlash)
{
 if(age<0||!origin||origin>uint32_t(age))return 1;
 const int steps=(std::min)((age-int(origin))/2,251);float t=longFlash?5.f:.3f;
 // Fixed375E80 applies the final material BEFORE clamping its timer to0.
 // A stopped flash retains that last material; no invented reset-to-white.
 for(int i=0;i<steps&&t>0;++i)t-=.02f;
 const float cosine=std::cos(t*4.f*3.1415927f);return std::clamp(float(125-int(cosine*75.f))*.02f,1.f,4.f);
}
bool FusionDrown343BigBulletContact(uint8_t way,bool hostile,bool sameRow)
{return way==2||way==15||way==18||(!hostile&&sameRow&&way!=13);}
bool FusionDrown343OwnerVariant(uint16_t owner,uint8_t v)
{return (owner==20||owner==202)?v==0:owner==216?v==Drown343Horizontal:owner==218?v==(Drown343Horizontal|Drown343Big):false;}
}
