#include "FusionNativeHeavyZombie313B.h"
#include <cmath>
namespace LawnFusion
{
bool NativeHeavyZombie313BKind(uint16_t k){return k==28||k==31||k==32||k==33||k==34||k==45||k==203||k==223;}
bool ValidateFusionHeavyZombie313BView(uint16_t k,const FusionHeavyZombie313BView& s)
{
 if(!NativeHeavyZombie313BKind(k)||s.ammo>25||s.armorStage>2||s.pogoWait>2||!std::isfinite(s.jumpSpeed))return false;
 FusionHeavyZombie313BView canonical;
 if(k==28){if(s.status!=0&&s.status!=1&&s.status!=12&&s.status!=13)return false;canonical.status=s.status;canonical.axePresent=s.axePresent;canonical.lookBack=s.lookBack;}
 else if(k==31){if(s.status||s.armorStage>1)return false;canonical.ammo=s.ammo;canonical.shooting=s.shooting;canonical.caltropDying=s.caltropDying;canonical.armorStage=s.armorStage;}
 else if(k==33){if(s.status!=0&&s.status!=1&&s.status!=18)return false;if(s.status==18&&!s.ladderVisible)return false;canonical.status=s.status;canonical.ladderVisible=s.ladderVisible;canonical.armorStage=s.armorStage;}
 else if(k==32||k==34)
 {
  if(!FusionPogo313BViewValid(k,{s.status,s.pogoWait,s.pogoPaused,s.pogoStarted,s.jumped,s.shadowEnabled,s.lostJumper})||s.jumpSpeed<1.1f||s.jumpSpeed>1.4f)return false;
  canonical.status=s.status;canonical.pogoWait=s.pogoWait;canonical.pogoPaused=s.pogoPaused;canonical.pogoStarted=s.pogoStarted;
  canonical.jumped=s.jumped;canonical.lostJumper=s.lostJumper;canonical.shadowEnabled=s.shadowEnabled;canonical.jumpSpeed=s.jumpSpeed;
  if(k==34)canonical.armorStage=s.armorStage;
 }
 else if(k==45){if(s.status!=s.bungee.status||!FusionBungee313BViewValid(s.bungee,10,6)||s.bungee.setZombie)return false;canonical.status=s.status;canonical.bungee=s.bungee;}
 // Gold source4/5/6/status/armor/award uses gold269, validated separately.
 return canonical==s;
}
}
