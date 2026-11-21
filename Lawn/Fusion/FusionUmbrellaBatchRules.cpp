#include "FusionUmbrellaBatchRules.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace LawnFusion
{
bool IsFusionUmbrellaBatchKind(uint16_t k)
{constexpr uint16_t kinds[]={30,916,923,1148,1115,1127,1128,1131,1143,1144,1185,1186};for(auto id:kinds)if(k==id)return true;return false;}
bool FusionUmbrellaBatchPlantStateValid(uint16_t k,uint32_t shots,uint16_t rage,uint16_t energy,uint8_t mode)
{return IsFusionUmbrellaBatchKind(k)&&(k==1131?shots<=1000000:shots==0)&&(FusionUmbrellaBatchMoneySkillFee(k)>0?rage<=750:rage==0)&&(k==1185?energy<=2000:energy==0)&&mode<=7;}
float FusionUmbrellaBatchMaximumDuration(uint16_t k,float speed)
{return IsFusionUmbrellaBatchKind(k)&&std::isfinite(speed)&&speed>=.9f&&speed<=1.1f?(FusionUmbrellaBatchMoneySkillFee(k)>0?1.0f:1.0f/1.2f)/speed+.02f:0;}
int FusionUmbrellaBatchMoneySkillFee(uint16_t k){return k==1144?2000:k==1148?3000:k==916||k==923?1000:0;}
int FusionUmbrellaSelfDamage267(int count){return count<1?0:count==1?100:count<=5?150:250;}
bool TryApplyFusionLanternUmbrella267Damage(int incoming,uint16_t energy,int& applied,uint16_t& after)
{if(incoming<0||energy>2000)return false;applied=(std::max)(incoming-int(energy)/5,0);after=static_cast<uint16_t>((std::max)(int64_t(energy)-int64_t(incoming)*5,int64_t(0)));return true;}
bool TryGetFusionUmbrella267Reinforcement(int incoming,bool travel,int& after)
{if(incoming<=0)return false;const float n=static_cast<float>(incoming)*(travel?2.25f:1.75f);if(!std::isfinite(n)||n>=float((std::numeric_limits<int>::max)()))return false;after=static_cast<int>(n);return true;}
bool TryMakeFusionUmbrella267Retarget(double x,double y,double tx,double ty,double py,float& vx,float& vy,float& g)
{
    if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(tx)||!std::isfinite(ty)||!std::isfinite(py)||py<=0)return false;
    // AdjustBulletSpeed485370: identical target samples (t1=1,t2=2), flight=.5.
    const float gravity=float(14.715*py*.0001),velocityX=float((tx-x)/50),velocityY=float((ty-y)/50-double(gravity)*25);
    if(!std::isfinite(gravity)||!std::isfinite(velocityX)||!std::isfinite(velocityY))return false;
    vx=velocityX;vy=velocityY;g=gravity;return true;
}
}
