#include "FusionNativeQuickJackson367.h"
#include "FusionNativeNightZombie291Bridge.h"
#include "FusionNativeZombies.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeZombiePreflight280.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeProjection313.h"
#include "FusionNativeEffects269.h"
#include "FusionNativeGarlic.h"
#include "FusionNativeWater268.h"
#include "FusionNative.h"
#include "../Zombie.h"
#include "../Board.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
namespace LawnFusion {namespace {
bool Typed(const Zombie* z){const auto* d=GetNativeSourceZombieData(z);return d&&d->kind==207;}
bool Host(const Zombie* z){return Typed(z)&&!z->mDead&&!z->mNetGhost&&!z->mApp->NetIsClient()&&IsNativeSourceWorld313(z->mBoard);}
}
bool EnsureNativeQuickJackson367Resources(Board* b,std::string* error){const uint16_t children[]={6};
 return IsNativeSourceWorld313(b)&&EnsureNativeSourceZombieVisuals(b,207,error)&&CanQueryNativeSourceZombieRoot273(b,207)&&
  EnsureNativeSourceZombieClosure280(b,children,error)&&EnsureNativeSourceEffect269(b,11,error)&&LawnNet::NetSourceSound284Ready(b->mApp,69);}
bool InitializeNativeQuickJackson367Presentation(Zombie* z){return !Typed(z)||(Host(z)&&SetNativeSourceZombieEmission360(z,"Trail_1/Trail_1",true));}
bool FinishNativeQuickJackson367MoonWalk(Zombie* z){if(!Typed(z))return true;if(!Host(z)||!SetNativeSourceZombieEmission360(z,"Trail_1/Trail_1",false))return false;z->mVelX=0;return true;}
bool StepNativeQuickJackson367Position(Zombie* z,bool& stop){stop=false;if(!Typed(z))return true;if(!Host(z))return false;
 const auto* d=GetNativeSourceZombieData(z);const auto* row=GetNativeZombieGarlicData(z);float factor=0,delta=0,x=0,y=0;
 if(!d||!TryGetFusionZombieStatusSpeedFactor(d->kind,z->mChilledCounter>0,z->mIceTrapCounter>0,z->mButteredCounter>0,row&&FusionGarlicStopsWalking(*row),factor)||
  !FusionQuickJackson367Motion(d->originSpeed*factor*GetNativeWater268GrapSpeedFactor(z),.01f,delta)||!NativeSourcePoint313(z,"axis",x,y)||!SetNativeSourceAxis313(z,x+delta,y))return false;
 // Source PositionUpdate tests the ACTUAL Shadow after its extra root move.
 if(!NativeSourcePoint313(z,"Shadow",x,y))return false;stop=x<0;return true;}
bool ResolveNativeQuickJackson367RawDamage(const Zombie* z,int raw,bool fix,int& out){if(!Typed(z))return false;const auto* state=GetNativeJackson291(z);if(!state||!state->initialized||state->failed)return false;out=FusionQuickJackson367RawDamage(raw,fix,state->moonWalkFinished);return true;}
}
