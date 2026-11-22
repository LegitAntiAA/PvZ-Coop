#include "FusionNativeDrown313Bridge.h"
#include "FusionDrown343Rules.h"
#include "FusionNativeJackbox309Bridge.h"
#include "FusionNativeObstacle313.h"
#include "FusionNative.h"
#include "FusionNativeProjection313.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativePlantDamage.h"
#include "FusionNativeRemainingPlants269.h"
#include "FusionNativeBatchActions.h"
#include "FusionNativeFireActionBatch.h"
#include "FusionNativeShooterBatch.h"
#include "FusionNativeStar269.h"
#include "FusionWaterBridge321Rules.h"
#include "FusionAnimation.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Projectile.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include <cmath>
namespace LawnFusion
{
namespace
{
bool Traits(const Plant* p,bool& invincible,bool& pumpkin,bool& flying,bool& pot)
{
    if(!p||!GetNativePlantData(p))return false;bool lily=false;
    if(!GetNativeRemaining269PlantTags(p,flying,pumpkin,pot,lily))return false;
    invincible=NativeBatchPlantInvincible(p)||NativeFireActionInvincible(p)||
        NativeShooterBatchInvulnerable(p)||NativeStar269Invulnerable(p)||NativeRemaining269Invulnerable(p);
    return true;
}
bool Shadow(Plant* p,float& x,float& y)
{return p&&!p->mDead&&p->mIsOnBoard&&p->GetShadowCenter(x,y)&&std::isfinite(x)&&std::isfinite(y);}
bool Prepare(Board* b,const FusionSourceObstacle313Spawn& s,std::string* error)
{
    return IsNativeSourceWorld313(b)&&EnsureNativeDrownWeapon313Visuals(b,error,s.variant)&&
        LawnNet::NetSourceSound284Ready(b->mApp,3)&&LawnNet::NetSourceSound284Ready(b->mApp,4)&&
        CanSpawnNativeSourceObstacle313(b,s,error);
}
bool Touches(Projectile* p,Plant* target)
{
    if(!p||!target||target->mRow!=p->mRow||!NativeSourcePlantColliderEnabled(target))return false;
    FusionAnimationBoxCollider303 box;if(!GetNativeSourceObstacle313Box(p,box)||!box.enabled||!box.nodeActive)return false;
    FusionContactQuad321 q;for(size_t i=0;i<4;++i)q[i]={p->mPosX+20+60*box.corners[i][0],p->mPosY+20-60*box.corners[i][1]};
    const auto r=target->GetPlantRect();if(r.mWidth<=0||r.mHeight<=0)return false;
    const float l=float(r.mX),t=float(r.mY),rr=float(r.mX+r.mWidth),bb=float(r.mY+r.mHeight);
    return FusionContactQuads321(q,{{{l,t},{rr,t},{rr,bb},{l,bb}}});
}
bool Ground(Projectile* p,float& output)
{if(!p||!IsNativeSourceWorld313(p->mBoard)||!std::isfinite(p->mShadowY))return false;output=p->mShadowY;return true;}
void Sound(Zombie* z,uint16_t id,float volume)
{if(z&&!z->mNetGhost&&!z->mApp->NetIsClient())LawnNet::NetPlaySourceSound284(z->mApp,id,volume);}
int Random(Board*,int minimum,int exclusiveMaximum){return RandRangeInt(minimum,exclusiveMaximum-1);}
float RandomFloat(Board*,float minimum,float maximum){return RandRangeFloat(minimum,maximum);}
bool Stronger(const Board* b){return NativeUltimateFootballDrownCount343(b)>0;}
bool TouchesBullet(Projectile* p,Projectile* shot)
{
    if(!p||!shot||shot->mDead)return false;
    FusionAnimationBoxCollider303 box;if(!GetNativeSourceObstacle313Box(p,box)||!box.enabled||!box.nodeActive)return false;
    FusionContactQuad321 q;for(size_t i=0;i<4;++i)q[i]={p->mPosX+20+60*box.corners[i][0],p->mPosY+20-60*box.corners[i][1]};
    const auto r=shot->GetProjectileRect();if(r.mWidth<=0||r.mHeight<=0)return false;
    const float l=float(r.mX),t=float(r.mY),rr=float(r.mX+r.mWidth),bb=float(r.mY+r.mHeight);
    return FusionContactQuads321(q,{{{l,t},{rr,t},{rr,bb},{l,bb}}});
}
}
const NativeDrownWeapon313Hooks& GetNativeDrownWeapon313Hooks()
{static const NativeDrownWeapon313Hooks hooks{Traits,Shadow,Prepare,SpawnNativeSourceObstacle313,Touches,Ground,Sound,Random,RandomFloat,TouchesBullet,Stronger,[](Board* b,const NativeJackbox300Explosion& e,std::string* why){return CanEmitNativeSourceZombieExplosion343(b,e,why,0);},[](Board* b,const NativeJackbox300Explosion& e){return EmitNativeSourceZombieExplosion343(b,e,0);}};return hooks;}
bool EnsureNativeDrownWeapon313Visuals(Board* b,std::string* error,uint8_t variant)
{
    auto* c=GetNativeContext(b);if(!c||!IsNativeSourceWorld313(b))return false;
    if(!c->sourceObstacleVisuals313)c->sourceObstacleVisuals313=std::make_unique<NativeSourceObstacle313Visuals>(b);
    std::string why;if(!c->sourceObstacleVisuals313->Ensure(FusionSourceObstacle313Kind::DrownWeapon,variant,why))
    {if(error)*error=why;return false;}return true;
}
bool DrawNativeDrownWeapon313(Sexy::Graphics* g,Projectile* p)
{
    const auto* d=GetNativeSourceObstacle313(p);if(!d||d->kind!=FusionSourceObstacle313Kind::DrownWeapon)return false;
    const auto* c=GetNativeContext(p->mBoard);if(c&&c->sourceObstacleVisuals313)c->sourceObstacleVisuals313->Draw(g,p,*d);return true;
}
}
