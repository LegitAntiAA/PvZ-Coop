#include "FusionMorePlants269BackendRules.h"
namespace LawnFusion
{
bool FusionMore269Payload(uint16_t k)
{switch(k){case 0:case 9:case 15:case 21:case 24:case 28:case 37:case 39:case 55:case 59:case 60:case 86:return true;default:return false;}}
bool FusionMore269PlanarLobber(uint16_t k){return k==55||k==59||k==60||k==86;}
ProjectileType FusionMore269ProjectileType(uint16_t k,uint8_t fire)
{if(k==0&&fire)return PROJECTILE_FIREBALL;switch(k){case 9:return PROJECTILE_PUFF;case 37:return PROJECTILE_SPIKE;
 case 39:return PROJECTILE_STAR;case 55:return PROJECTILE_BUTTER;case 59:case 86:return PROJECTILE_MELON;
 case 60:return PROJECTILE_WINTERMELON;default:return PROJECTILE_PEA;}}
ProjectileMotion FusionMore269ProjectileMotion(uint16_t k){return k==39?MOTION_STAR:MOTION_STRAIGHT;}
bool FusionMore269ProjectileMetadataValid(uint16_t k,int damage,uint8_t fire,uint8_t way,ProjectileType type,
 ProjectileMotion motion,uint16_t sourceId,uint32_t token)
{const bool squashAfterContact=k==28&&way==1&&motion==MOTION_LOBBED;
 if(damage<0||!FusionMore269Payload(k)||(way&&!squashAfterContact)||fire>3||type!=FusionMore269ProjectileType(k,fire)||
    (!squashAfterContact&&motion!=FusionMore269ProjectileMotion(k)))return false;
 if(k!=0&&fire)return false;if((sourceId==0)!=(token==0))return false;
 if(fire){if(!sourceId)return false;damage-=fire==1?20:fire==2?40:60;}else if(sourceId)return false;
 return damage==20||damage==30||damage==40;}
}
