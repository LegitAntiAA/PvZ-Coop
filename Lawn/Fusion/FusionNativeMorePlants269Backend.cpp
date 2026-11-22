#include "FusionNativeProjectileBuff369.h"
#include "FusionNativeMorePlants269Backend.h"
#include "FusionNativeMorePlants269.h"
#include "FusionNative.h"
#include "FusionNativeVisual.h"
#include "FusionNativeStar269.h"
#include "FusionNativeProjectileMetadata.h"
#include "FusionNativePlacement.h"
#include "FusionNativeRemainingPlants269.h"
#include "FusionNativeLobber.h"
#include "FusionNativeWater268.h"
#include "FusionBasePlantRules.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include <cmath>
#include <initializer_list>
namespace LawnFusion
{
namespace
{
bool Current(const Board* b){return b&&b->mApp&&b->mApp->mBoard==b&&IsNativeFusionBoard(b);}
bool Host(const Board* b){return Current(b)&&!b->mApp->NetIsClient();}
NativeProjectileData Metadata(uint16_t kind,int damage,uint8_t fire,const Plant* parent=nullptr)
{NativeProjectileData d;d.kind=kind;d.damage=damage;d.fireState=fire;
 if(fire&&parent){d.lastTorchId=parent->mNetId;d.lastTorchToken=GetNativePlantData(parent)->instanceToken;}return d;}
}
bool NativeMore269SourceWaterPlant(uint16_t k)
{switch(k){case 12:case 15:case 19:case 252:case 1049:case 1050:case 1051:case 1056:case 1066:
 case 1067:case 1068:case 1069:case 1076:case 1077:case 1078:case 1083:case 1086:case 1101:case 1156:case 1175:return true;default:return false;}}
bool NativeMore269PresentWaterCell(const Plant* p)
{if(!p||!Current(p->mBoard)||p->mRow<0||p->mRow>=GetNativeFusionRowCount(p->mBoard)||p->mBoard->mPlantRow[p->mRow]!=PLANTROW_POOL)return false;
 PlantsOnLawn cell{};p->mBoard->GetPlantsOnLawn(p->mPlantCol,p->mRow,&cell);return !cell.mUnderPlant;}
bool NativeMore269PresentSuperRandom(const Plant*){return false;} // No native source SuperRandom world yet.
bool NativeMore269SqualourTravel(const Plant*){return false;} // OrdinaryD2, source Travel flag absent.
bool CanSpawnNativeMore269Plant(const Board* b,uint16_t kind)
{
 const auto* def=FusionFindPlant(kind);if(!Host(b)||!def||!def->hasStats||GetNativeSeedForPlant(kind)==SEED_NONE)return false;
 std::string modelError,portraitError;
 const bool model=EnsureNativePlantVisuals(const_cast<Board*>(b),kind,&modelError);
 const bool portrait=EnsureNativePortraitVisuals(const_cast<Board*>(b),kind,&portraitError);
 if(!model||!portrait)LawnNet::NetTrace("Fusion More269 factory preflight kind=%u model=%d portrait=%d modelError=%s portraitError=%s",kind,model,portrait,modelError.c_str(),portraitError.c_str());
 return model&&portrait;
}
bool CanEmitNativeMore269HamburgerShot(const Board* b,uint16_t kind,uint8_t fire)
{if(!Host(b)||!FusionMore269Payload(kind)||fire>3||(fire&&kind!=0)||b->mProjectiles.mSize>=b->mProjectiles.mMaxSize)return false;
 // Actual package only. Optional firechild is already the shared native source
 // Fire overlay; a missing package never becomes a different projectilekind.
 if(!EnsureNativeProjectileVisuals(const_cast<Board*>(b),kind))return false;
 Plant* parent=nullptr;while(const_cast<Board*>(b)->IteratePlants(parent))if(!parent->mDead)
  if(const auto* pd=GetNativePlantData(parent))if(pd->kind==243)break;
 if(!parent)return false;
 for(int base:{20,30,40}){const auto d=Metadata(kind,base+(fire==1?20:fire==2?40:fire==3?60:0),fire,parent);
  if(!NativeProjectileMetadataValid(d,FusionMore269ProjectileType(kind,fire),FusionMore269ProjectileMotion(kind)))return false;}
 return true;}
bool EmitNativeMore269HamburgerShot(Plant* p,const FusionHamburger269Shot& shot,float x,float y)
{
 if(!p||!Host(p->mBoard)||p->mDead||!GetNativePlantData(p)||GetNativePlantData(p)->kind!=243||
    !std::isfinite(x+y)||!CanEmitNativeMore269HamburgerShot(p->mBoard,shot.kind,shot.fire))return false;
 auto* b=p->mBoard;const auto type=FusionMore269ProjectileType(shot.kind,shot.fire);const auto motion=FusionMore269ProjectileMotion(shot.kind);
 const auto d=Metadata(shot.kind,shot.damage,shot.fire,p);
 if(!FusionMore269ProjectileMetadataValid(d.kind,d.damage,d.fireState,d.specialType,type,motion,d.lastTorchId,d.lastTorchToken)||
    !NativeProjectileMetadataValid(d,type,motion))return false;
 auto* q=b->AddProjectile(int(x-20),int(y-20),p->mRenderOrder-1,p->mRow,type==PROJECTILE_STAR?PROJECTILE_PEA:type);if(!q)return false;
 q->mProjectileType=type;q->mMotionType=motion;q->mPosX=x-20;q->mPosY=y-20;q->mPosZ=0;
 q->mVelX=float(6*kFusionBasePixelsX*.01);q->mVelY=q->mVelZ=q->mAccZ=0;q->mShadowY=y;q->mRotation=q->mRotationSpeed=0;
 if(shot.kind==28)
 {q->mVelX=RandRangeFloat(1.5f,1.9f)*.8f*float(kFusionBasePixelsX*.01);
  q->mVelZ=-RandRangeFloat(4.8f,5.2f)*.8f*float(GetNativeFusionPixelsY(b)*.01);
  q->mAccZ=float(9.8*GetNativeFusionPixelsY(b)*.0001);q->mShadowY=y+float(10*.061242823*GetNativeFusionPixelsY(b));}
 q->mX=int(q->mPosX);q->mY=int(q->mPosY);
 if(!AttachNativeTravelProjectile369(p,q,d)){q->Die();return false;}
 if(shot.kind==39&&!NativeStar269ProjectileCreated(q)){q->Die();return false;}
 return true;
}
bool IsNativeMore269Projectile(const Projectile* q)
{const auto* d=GetNativeProjectileData(q);return d&&!d->sunOnHit&&!d->effectVariant&&!d->melonEnhanced&&!d->isZombieBullet&&!d->hitLatched&&!d->umbrellaed&&
 FusionMore269ProjectileMetadataValid(d->kind,NativeProjectileValidationDamage369(*d),d->fireState,d->specialType,q->mProjectileType,q->mMotionType,d->lastTorchId,d->lastTorchToken);}
bool MoveNativeMore269PlanarLobber(Projectile* q)
{
 const auto* d=GetNativeProjectileData(q);if(!d||!FusionMore269PlanarLobber(d->kind)||q->mMotionType!=MOTION_STRAIGHT||d->specialType)return false;
 if(!Host(q->mBoard)||q->mNetGhost||q->mDead||q->mBoard->mPaused)return true;
 q->mPosX+=q->mVelX;q->mPosY+=q->mVelY;q->mShadowY+=q->mVelY;
 if(d->kind==55||d->kind==86)q->mRotation-=float(3.14159265358979323846/36.);
 if(q->mProjectileAge>2000||q->mPosX+20>q->mBoard->GetCellCenterX(q->mBoard->GetColumnCount()-1,q->mRow)+float(2.85*kFusionBasePixelsX))q->Die();
 q->mX=int(q->mPosX);q->mY=int(q->mPosY);return true;
}
bool UpdateNativeMore269SquashMotion(Plant* p)
{if(!p||!GetNativePlantData(p)||GetNativePlantData(p)->kind!=248)return false;
 if(!Host(p->mBoard)||p->mNetGhost||p->mDead||p->mBoard->mPaused)return true;
 // Use real stock acquisition, target full-generation, easing and two native
 // motion states. Replace only its typed impact and terminal removal leaves.
 if(p->mState==STATE_SQUASH_FALLING&&p->mStateCountdown==0){FinishNativeSqualour269(p);return true;}
 p->UpdateSquash();return true;}
bool GetNativeMore269SquashVisualPose(const Plant* p,const char*& state,float& seconds)
{if(!p||!GetNativePlantData(p)||GetNativePlantData(p)->kind!=248)return false;const auto* d=GetNativePlantData(p);seconds=0;
 switch(p->mState){case STATE_NOTREADY:state="idle";seconds=d->idleSeconds;break;
 case STATE_SQUASH_LOOK:state=p->mTargetX<p->mX?"lookleft":"lookright";seconds=(80-p->mStateCountdown)*.01f;break;
 case STATE_SQUASH_PRE_LAUNCH:state="jump";seconds=(30-p->mStateCountdown)*.01f;break;
 case STATE_SQUASH_RISING:state="jump";seconds=.3f+(50-p->mStateCountdown)*.01f;break;
 case STATE_SQUASH_FALLING:state="jumppop";seconds=(10-p->mStateCountdown)*.01f;break;
 default:return false;}return true;}
}
