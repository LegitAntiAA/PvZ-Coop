#include "FusionNativeBossBall366.h"
#include "FusionNativeObstacle313.h"
#include "FusionNative.h"
#include "FusionNativeZombies.h"
#include "FusionNativeEffects269.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Projectile.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include <cmath>
namespace LawnFusion
{
namespace
{
bool Owner(Zombie* z)
{
 const auto* d=GetNativeSourceZombieData(z);const auto* c=z?GetNativeContext(z->mBoard):nullptr;
 return z&&z->mApp&&z->mApp->mBoard==z->mBoard&&!z->mDead&&!z->mNetGhost&&!z->mApp->NetIsClient()&&c&&!c->adventureTeardown&&
  d&&(d->kind==44||d->kind==46)&&d->instanceToken==z->mBoard->mZombies.DataArrayGetID(z)&&z->mNetId;
}
}
bool MakeNativeBossBall366Request(Zombie* z,uint8_t variant,int row,float mx,float my,float ground,bool fast,int order,FusionSourceObstacle313Spawn& out,std::string* error)
{
 if(!Owner(z)||variant>1||row<0||row>=GetNativeFusionRowCount(z->mBoard)||!std::isfinite(mx)||!std::isfinite(my)||!std::isfinite(ground)||order<0)return false;
 const auto* d=GetNativeSourceZombieData(z);FusionSourceObstacle313Spawn s;
 s.kind=FusionSourceObstacle313Kind::BossBall;s.ownerId=z->mNetId;s.ownerToken=d->instanceToken;s.row=row;s.variant=variant;
 s.centerX=mx-float(kFusionBasePixelsX);s.centerY=s.groundY=ground;s.renderOrder=z->mRenderOrder+1;
 s.sourceBirthVelocityX366=fast?-1.5f:-.75f;s.growthRenderOrder366=order;
 if(!CanSpawnNativeSourceObstacle313(z->mBoard,s,error)||!EnsureNativeSourceEffect269(z->mBoard,uint16_t(76+variant),error)||!LawnNet::NetSourceSound284Ready(z->mApp,121))return false;
 out=s;return true;
}
bool EmitNativeBossBall366(Zombie* z,const FusionSourceObstacle313Spawn& s,float mx,float my,std::string* error)
{
 if(!Owner(z)||s.kind!=FusionSourceObstacle313Kind::BossBall||s.ownerId!=z->mNetId||s.ownerToken!=GetNativeSourceZombieData(z)->instanceToken||
  !std::isfinite(mx)||!std::isfinite(my)||!CanSpawnNativeSourceObstacle313(z->mBoard,s,error)||
  !EnsureNativeSourceEffect269(z->mBoard,uint16_t(76+s.variant),error)||!LawnNet::NetSourceSound284Ready(z->mApp,121))return false;
 auto* b=z->mBoard;auto* app=z->mApp;const uint32_t epoch=GetNativeContext(b)->epoch,uid=b->mZombies.DataArrayGetID(z);
 // Actual source AnimPutBall: sound121 -> CreateParticle76/77 -> item32/33.
 if(!LawnNet::NetPlaySourceSound284(app,121,.5f))return false;
 SpawnNativeSourceEffect269(b,uint16_t(76+s.variant),mx-float(kFusionBasePixelsX)*.5f,my-float(GetNativeFusionPixelsY(b))*.1f,
  Board::MakeRenderOrder(RENDER_LAYER_COIN_BANK,0,30000),true);
 if(app->mBoard!=b||!GetNativeContext(b)||GetNativeContext(b)->epoch!=epoch)return false;
 z=b->mZombies.DataArrayTryToGet(uid);if(!Owner(z))return false;
 return SpawnNativeSourceObstacle313(b,s)!=nullptr;
}
}
