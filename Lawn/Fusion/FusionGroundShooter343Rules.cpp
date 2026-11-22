#include "FusionGroundShooter343Rules.h"
namespace LawnFusion
{
bool TryPlanFusionGround343Shot(uint16_t k,int status,bool dying,bool row,bool right,bool controlled,bool muzzle,FusionGround343Shot& out)
{
 if(!FusionGroundShooter343Kind(k)||status<0||status>24)return false;
 FusionGround343Shot s;s.bullet=k==118?0:11;s.damage=k==118?20:80;s.way=right?0:9;s.hostile=!controlled;
 s.sourceOffsetX=k==119?0:right?.4f:-.4f;
 s.emit=muzzle&&(k==118||(!dying&&!row&&status!=1));out=s;return true;
}
bool TryGetFusionGround343Attribute(uint16_t k,bool initial,float& out)
{if(!FusionGroundShooter343Kind(k))return false;out=k==118?.1f:initial&&k==114?.5f:1.5f;return true;}
uint8_t FusionGround343ArmorStage(uint16_t k,int hp,int maximum,uint8_t previous)
{
 if((k!=114&&k!=118)||hp<=0||maximum<=0||previous>2)return previous;
 const int low=maximum/3,high=int((int64_t(maximum)*2)/3);
 if(k==114){if(hp<low)return 2;if(hp<high&&previous<1)return 1;return previous;}
 //356940 uses the same strict-open low boundary as216, not float thirds.
 return hp<low?2:hp>low&&hp<=high?1:previous;
}
}
