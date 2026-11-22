#include "FusionLateZombie363Rules.h"
namespace LawnFusion {
bool TryPlanFusionLate363Death(uint16_t k,int reason,bool mc,bool lost,int status,bool consumed,FusionLate363Death& out)
{
 if((k!=201&&k!=209&&k!=222)||reason<0||reason>2||status<0)return false;
 FusionLate363Death p;
 if(!consumed){
  if(k==209){if(reason==0&&!lost&&status!=20){p.explosion=true;p.count=1;p.children[0]=30;}}
  else{p.effect36=true;if(!mc){p.count=3;p.children=k==201?std::array<uint16_t,3>{16,18,10}:std::array<uint16_t,3>{201,201,207};p.offsets={-1,1,0};}}
 }
 out=p;return true;
}
bool FusionSuperPogo363Armor(int hp,int maximum,uint8_t previous,uint8_t& next)
{
 if(hp<=0||maximum<=0||hp>maximum||previous>2)return false;
 uint8_t stage=previous;
 //373B50 nonlethal virtual FirstArmorBroken; source ValueBetween low<h<=high.
 const float low=float(maximum)*.33333334f,high=float(maximum)*.6666667f;
 if(low<float(hp)&&float(hp)<=high)stage=1;
 if(float(hp)<float(maximum)/3.f)stage=2;
 next=stage;return true;
}
}
