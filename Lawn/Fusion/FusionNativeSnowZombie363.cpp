#include "FusionNativeSnowZombie363.h"
#include "FusionNativeExtendedZombie313Bridge.h"
#include "FusionNativeNextZombie343State.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeZombies.h"
#include "FusionNativeEffects269.h"
#include "FusionNative.h"
#include "../Zombie.h"
#include "../Board.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include <cmath>
namespace LawnFusion {namespace {
uint16_t Kind(const Zombie* z){const auto* d=GetNativeSourceZombieData(z);return d?d->kind:0xffff;}
uint32_t UID(Zombie* z){return z->mBoard->mZombies.DataArrayGetID(z);}
bool Host(const Zombie* z){return z&&!z->mDead&&!z->mNetGhost&&!z->mApp->NetIsClient()&&FusionSnowZombie363Kind(Kind(z))&&IsNativeSourceWorld313(z->mBoard);}
void Fail(Zombie* z,const char* why){if(auto* s=EditNativeExtendedZombie313(z))s->failed=true;LawnNet::NetTrace("Fusion Snow363 stopped kind=%u uid=%u reason=%s",unsigned(Kind(z)),UID(z),why);}
}
bool EnsureNativeSnowZombie363Resources(Board* b,uint16_t kind,std::string* error)
{return b&&FusionSnowZombie363Kind(kind)&&IsNativeSourceWorld313(b)&&FindFusionSnow363Graph(kind)&&EnsureNativeSourceZombieVisuals(b,kind,error)&&CanQueryNativeSourceZombieRoot273(b,kind);}
bool InitializeNativeSnowZombie363(Zombie* z)
{
 if(!Host(z)||!EnsureNativeSnowZombie363Resources(z->mBoard,Kind(z)))return false;auto* c=GetNativeContext(z->mBoard);if(!c)return false;
 auto& ai=c->nextZombies343[UID(z)];ai={};ai.initialized=true;
 if(Kind(z)==47)ai.attributeCountdown=RandRangeFloat(kFusionSnow363MinimumTurn,kFusionSnow363MaximumTurn);
 return std::isfinite(ai.attributeCountdown)&&ai.attributeCountdown>=0;
}
bool UpdateNativeSnowZombie363Attribute(Zombie* z)
{
 if(!Host(z)||z->mBoard->mPaused||z->mApp->mGameScene!=SCENE_PLAYING)return true;auto* c=GetNativeContext(z->mBoard);if(!c)return false;
 auto it=c->nextZombies343.find(UID(z));if(it==c->nextZombies343.end()||!it->second.initialized||it->second.failed)return false;auto& ai=it->second;
 if(ai.attributeTick==z->mBoard->mMainCounter)return true;ai.attributeTick=z->mBoard->mMainCounter;
 if(Kind(z)!=47||ai.attributeCountdown<=0)return true;ai.attributeCountdown-=.01f;if(ai.attributeCountdown>0)return true;
 ai.attributeCountdown=0;auto* state=EditNativeExtendedZombie313(z);float oldX=0,oldY=0,newX=0,newY=0;
 if(!state||!GetNativeSourceZombie269NodePoint(z,"",oldX,oldY))return false;
 const bool previous=state->rootFlip;state->rootFlip=true;
 if(!GetNativeSourceZombie269NodePoint(z,"",newX,newY)||!std::isfinite(newX+newY)){state->rootFlip=previous;return false;}
 //35B9E0 sets the real Transform.worldRotation around its existing ROOT,
 //not the keep-Shadow facing transaction used by mind control. The shared
 //sampler is anchored on Shadow, so compensate its real before/after ROOT.
 z->mPosX+=oldX-newX;z->mPosY+=oldY-newY;z->mX=int(z->mPosX);z->mY=int(z->mPosY);
 NativeClockOnlyZombieClear309(z);return true;
}
bool ApplyNativeSnow363FirstArmor(Zombie* z,int damage,int& remaining)
{
 if(!Host(z)||Kind(z)!=48||damage<0)return false;auto* state=EditNativeExtendedZombie313(z);if(!state)return false;
 const int hp=z->mHelmHealth;if(hp<=0){remaining=damage;return true;}
 if(damage>=hp)
 {
  if(!PlayNativeSourceZombieArmor284(z,true)){remaining=0;Fail(z,"Prepared source48 LoseCone refused");return true;}
  remaining=damage-hp;z->mHelmHealth=0;z->mHelmType=HELMTYPE_NONE;
 }
 else
 {uint8_t stage=0;if(!TryGetFusionSnow363ArmorStage(hp-damage,z->mHelmMaxHealth,stage)){remaining=0;Fail(z,"Source48 armor boundary invalid");return true;}
  z->mHelmHealth=hp-damage;state->firstArmorStage=stage;remaining=0;}
 return true;
}
}
