#include "FusionNativeFinish363.h"
#include "FusionNative.h"
#include "FusionNativeTravelUltimate370.h"
#include "FusionNativeExtendedZombie313Bridge.h"
#include "FusionNativeNextZombie343State.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeSourceLifecycle313.h"
#include "FusionNativeProjection313.h"
#include "FusionNativeZombiePreflight280.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativePlantDamage.h"
#include "FusionNativeRemainingPlants269.h"
#include "FusionNativePlacement.h"
#include "FusionNativeExtras268.h"
#include "FusionNativeDefender.h"
#include "FusionNativeFire.h"
#include "FusionNativeSourceDamage315.h"
#include "FusionNativeSourceObstacleFactory313.h"
#include "FusionNativeObstacle313.h"
#include "FusionNativeEffects269.h"
#include "SpecialZombie269Rules.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../GridItem.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include <algorithm>
#include <cmath>
namespace LawnFusion {namespace {
uint16_t Kind(const Zombie* z){const auto* d=GetNativeSourceZombieData(z);return d?d->kind:0xffff;}
bool Host(const Zombie* z){return z&&!z->mDead&&!z->mNetGhost&&!z->mApp->NetIsClient()&&IsNativeSourceWorld313(z->mBoard);}
bool Live(const Plant* p){return p&&!p->mDead&&p->mIsOnBoard&&p->mPlantHealth>0&&GetNativePlantData(p);}
bool Same(LawnApp* app,Board* b,uint32_t epoch){const auto* c=app->mBoard==b?GetNativeContext(b):nullptr;return c&&c->epoch==epoch;}
bool Failed(Zombie* z,const char* why){if(auto* s=EditNativeExtendedZombie313(z))s->failed=true;LawnNet::NetTrace("Finish363 stopped kind=%u reason=%s",Kind(z),why);return false;}
std::vector<uint32_t> Plants(Board* b,int row,int col=-1)
{std::vector<uint32_t> out;Plant* p=nullptr;while(b->IteratePlants(p))if(Live(p)&&p->mRow==row&&(col<0||p->mPlantCol==col))out.push_back(b->mPlants.DataArrayGetID(p));return out;}
bool FirePlan(Board* b,int row,bool hostile,std::vector<uint32_t>& targets,FusionSourceDeathBudget319& extra)
{
 if(!IsNativeSourceWorld313(b)||b->mApp->NetIsClient()||row<0||row>=GetNativeFusionRowCount(b))return false;
 std::vector<uint16_t> children;Zombie* z=nullptr;while(b->IterateZombies(z))if(!z->mDead&&GetNativeSourceZombieData(z))
 {const auto k=Kind(z);if(hostile){if(!z->mMindControlled&&z->mRow==row&&!GetNativeSourceZombieData(z)->beforeDying&&k==115){targets.push_back(b->mZombies.DataArrayGetID(z));children.push_back(116);}}
  else if((z->mRow==row&&!z->mMindControlled)||k==44||k==46)targets.push_back(b->mZombies.DataArrayGetID(z));}
 std::reverse(targets.begin(),targets.end());
 return EnsureNativeSourceZombieClosure280(b,children)&&GetNativeSourceZombieBirthBudget319(b,children,extra)&&CanRetireNativeSourceZombieBatch313(b,targets,2,extra);
}
bool SquashRequest(Zombie* z,Plant* p,FusionSourceObstacle313Spawn& out)
{
 float x=0,y=0,gx=0,gy=0;
 if(!GetNativeSourceZombie269NodePoint(z,"Zombie_head/SquashPrefab",x,y)||!NativeSourceGroundAnchor313(z,gx,gy))return false;
 FusionSourceObstacle313Spawn s;s.kind=FusionSourceObstacle313Kind::ZombieSquash;s.ownerId=z->mNetId;s.ownerToken=GetNativeSourceZombieData(z)->instanceToken;s.row=z->mRow;s.centerX=x;s.centerY=y;s.groundY=gy;s.variant=Kind(z)==116?1:0;s.squashColumn=uint8_t(p->mPlantCol);s.squashFire=s.variant&&z->mChilledCounter==0&&z->mIceTrapCounter==0;s.renderOrder=Board::MakeRenderOrder(RENDER_LAYER_PROJECTILE,s.row,0);out=s;return true;
}
bool StartSquash(Zombie* z,Plant* p)
{
 auto* b=z->mBoard;auto* app=z->mApp;const uint32_t uid=b->mZombies.DataArrayGetID(z),epoch=GetNativeContext(b)->epoch;
 FusionSourceObstacle313Spawn request;if(!SquashRequest(z,p,request)||!CanSpawnNativeSourceObstacle313(b,request))return false;
 auto* g=EditNativeSourceGraph313(z);const auto* source=FindFusionFinish363Graph(Kind(z));if(!g||!source)return false;
 const uint8_t previous=g->layers[0].state;FusionSourceGraph313Cursor clock;
 const auto& hooks=GetNativeExtendedZombie313Hooks().graph;if(!ReadNativeSourceGraph313Clock(z,0,previous,hooks,clock))return false;
 if(!PlayNativeSourceGraph313(z,*g,0,"die",0,hooks))return false;
 auto& view=g->layers[0];view.outgoing=previous;view.outgoingSeconds=clock.seconds;view.fadeElapsed=0;view.fadeDuration=.01f;view.fadeNormalized=true;g->outgoingCursors[0]=clock;
 auto* item=SpawnNativeSourceObstacle313(b,request);if(!item)return Failed(z,"prepared Squash item factory failed");
 if(!Same(app,b,epoch)||(z=b->mZombies.DataArrayTryToGet(uid))==nullptr||z->mDead)return true;
 auto* d=GetNativeSourceZombieData(z);auto* s=EditNativeExtendedZombie313(z);if(!d||!s)return false;d->beforeDying=s->beforeDying=true;s->squashLaunched363=true;d->health=(std::min)(d->health,float(z->mBodyMaxHealth/3));z->mBodyHealth=NativeSourceZombieLivenessHealth(d->health);return true;
}
}
bool CanApplyNativeSourceFireLine363(Board* b,int row,bool hostile,std::string* error)
{std::vector<uint32_t> targets;FusionSourceDeathBudget319 extra;if(!FirePlan(b,row,hostile,targets,extra)){if(error)*error="Source FireLine whole target/death/conversion closure unavailable";return false;}return LawnNet::NetSourceSound284Ready(b->mApp,42);}
bool ApplyNativeSourceFireLine363(Board* b,int row,bool hostile)
{
 std::vector<uint32_t> targets;FusionSourceDeathBudget319 extra;if(!FirePlan(b,row,hostile,targets,extra))return false;
 auto* app=b->mApp;const uint32_t epoch=GetNativeContext(b)->epoch;
 for(auto uid:targets)
 {
  if(!Same(app,b,epoch))return true;auto* z=b->mZombies.DataArrayTryToGet(uid);if(!z||z->mDead)continue;
  if(hostile){float x=0,y=0;if(!NativeSourcePoint313(z,"axis",x,y)||!RetireNativeSourceZombie313(z,2))return false;if(!Same(app,b,epoch))return true;if(!SpawnNativeExtendedZombieAtAxis313(b,116,row,x,y,false))return false;}
  else if(Kind(z)==16||Kind(z)==18||Kind(z)==201){if(!RetireNativeSourceZombie313(z,2))return false;}
  else{NativeWarmAfterDamage(z);if(!ApplyNativeSourceCharred315(z,1800))return false;}
 }
 if(!Same(app,b,epoch))return true;
 // Source walks the actual cell lists in column order, after zombie callbacks.
 for(int col=0;col<b->GetColumnCount();++col)for(auto uid:Plants(b,row,col))
 {
  if(!Same(app,b,epoch))return true;auto* p=b->mPlants.DataArrayTryToGet(uid);if(!Live(p))continue;const auto k=GetNativePlantData(p)->kind;
  if(k==913||k==925){const int recovery=NativeTravelUltimate370ElementRecovery(p,true,0);if(recovery){p->mPlantHealth=(std::min)(p->mPlantMaxHealth,int((std::min)(int64_t(p->mPlantMaxHealth),int64_t(p->mPlantHealth)+recovery)));NativePlantHealthChanged(p);}continue;}
  if(k==919)continue;
  if(k==1038){p->mPlantHealth=(std::min)(p->mPlantMaxHealth,p->mPlantHealth+3000);NativePlantHealthChanged(p);continue;}
  if(k==1129){NativeMachine268Element(b,row,2);continue;}
  if(hostile)ApplyNativeSourcePlantDamage(p,1000000,0);
 }
 if(!Same(app,b,epoch))return true;
 GridItem* grid=nullptr;while(b->IterateGridItems(grid))if(!grid->mDead&&grid->mGridItemType==GRIDITEM_LADDER&&grid->mGridY==row)grid->GridItemDie();
 std::vector<uint32_t> ice;Projectile* p=nullptr;while(b->IterateProjectiles(p)){const auto* d=GetNativeSourceObstacle313(p);if(!p->mDead&&p->mRow==row&&d&&(d->kind==FusionSourceObstacle313Kind::MachineBall||d->kind==FusionSourceObstacle313Kind::BossBall)&&d->variant==1)ice.push_back(b->mProjectiles.DataArrayGetID(p));}
 for(auto id:ice)if((p=b->mProjectiles.DataArrayTryToGet(id))&&!p->mDead)p->Die();
 b->mIceTimer[row]=(std::min)(b->mIceTimer[row],20);b->DoFwoosh(row);LawnNet::NetBroadcastFwoosh(app,row);LawnNet::NetPlaySourceSound284(app,42,.5f);b->ShakeBoard(3,3);return true;
}
bool EnsureNativeFinish363Resources(Board* b,uint16_t k,std::string* error)
{
 if(!FusionFinish363Kind(k)||!IsNativeSourceWorld313(b)||!EnsureNativeSourceZombieVisuals(b,k,error)||!CanQueryNativeSourceZombieRoot273(b,k))return false;
 if(k==115||k==116){if(!EnsureNativeSourceObstacle313Resources(b,FusionSourceObstacle313Kind::ZombieSquash,k==116?1:0,error))return false;}
 else{const uint16_t child=116;if(!EnsureNativeSourceZombieClosure280(b,std::span(&child,1),error))return false;}
 return LawnNet::NetSourceSound284Ready(b->mApp,42)&&LawnNet::NetSourceSound284Ready(b->mApp,74);
}
bool InitializeNativeFinish363(Zombie* z)
{if(!Host(z)||!FusionFinish363Kind(Kind(z)))return false;auto& p=GetNativeContext(z->mBoard)->nextZombies343[z->mBoard->mZombies.DataArrayGetID(z)];p.initialized=true;if(Kind(z)==117)p.attributeCountdown=RandRangeFloat(13.22f,22.68f);return true;}
bool UpdateNativeFinish363(Zombie* z)
{
 if(!Host(z)||!FusionFinish363Kind(Kind(z))||z->mBoard->mPaused||z->mApp->mGameScene!=SCENE_PLAYING)return true;
 auto* d=GetNativeSourceZombieData(z);auto* c=GetNativeContext(z->mBoard);const uint32_t uid=z->mBoard->mZombies.DataArrayGetID(z);auto& p=c->nextZombies343[uid];if(p.attributeTick==z->mBoard->mMainCounter)return true;p.attributeTick=z->mBoard->mMainCounter;
 if(d->beforeDying)return true;
 if(d->kind==117)
 {p.attributeCountdown-=.01f;if(p.attributeCountdown>0)return true;auto* b=z->mBoard;auto* app=z->mApp;const auto epoch=c->epoch;const int row=z->mRow;const bool hostile=!z->mMindControlled;if(!CanApplyNativeSourceFireLine363(b,row,hostile)||!CanRetireNativeSourceZombie313(z,2))return Failed(z,"117 FireLine preflight refused");p.deathConsumed=true;if(!RetireNativeSourceZombie313(z,2))return false;return !Same(app,b,epoch)||ApplyNativeSourceFireLine363(b,row,hostile);}
 if(z->mMindControlled)return true;std::vector<NativeExtendedZombie313Ref> hits;const auto& h=GetNativeExtendedZombie313Hooks();if(!h.bodyContacts(z,hits))return false;
 for(auto ref:hits)if(!ref.zombie){auto* plant=z->mBoard->mPlants.DataArrayTryToGet(ref.localUID);if(!Live(plant)||plant->mRow!=z->mRow)continue;bool flying=false,pumpkin=false,pot=false,lily=false;if(!GetNativeRemaining269PlantTags(plant,flying,pumpkin,pot,lily))return false;const auto kind=GetNativePlantData(plant)->kind;if(!flying&&!pot&&kind!=251&&kind!=255)return StartSquash(z,plant);}
 return true;
}
bool InitializeNativeSquashItem363(Projectile* p,uint8_t column,bool fire)
{
 auto* d=EditNativeSourceObstacle313(p);if(!d||column>=10)return false;d->squashColumn=column;d->squashFire=fire;d->squashPhase=0;d->squashPhaseAge=0;
 auto* r=p->mApp->ReanimationTryToGet(ReanimationID(d->clockUID));if(!r)return false;r->PlayReanim("anim_walk",REANIM_PLAY_ONCE_AND_HOLD,0,float(r->mFrameCount)/.6666667f);return true;
}
bool NativeSquashItem363Pose(const Projectile* p,const NativeSourceObstacle313Data& d,const char*& state,float& seconds)
{const auto* r=p->mApp->ReanimationTryToGet(ReanimationID(d.clockUID));if(!r||r->mDead)return false;state=d.squashPhase<3?"jump":"jumppop";seconds=r->mAnimTime*(d.squashPhase<3?.6666667f:.16666667f/1.8f);return std::isfinite(seconds);}
bool UpdateNativeSquashItem363(Projectile* p)
{
 auto* d=EditNativeSourceObstacle313(p);if(!d||d->kind!=FusionSourceObstacle313Kind::ZombieSquash)return false;
 auto* b=p->mBoard;auto* app=p->mApp;auto* r=app->ReanimationTryToGet(ReanimationID(d->clockUID));if(!r||r->mDead)return false;
 const float duration=d->squashPhase<3?.6666667f:.16666667f/1.8f;const float before=r->mAnimTime;r->mLastFrameTime=before;r->mAnimTime=(std::min)(1.f,before+.01f/duration);r->mAnimRate=float(r->mFrameCount)/duration;
 if(d->squashPhase==0&&before*.6666667f<.33333334f&&r->mAnimTime*.6666667f>=.33333334f)d->squashPhase=1;
 if(d->squashPhase==2&&uint32_t(p->mProjectileAge)-d->squashPhaseAge>=25){d->squashPhase=3;d->squashPhaseAge=uint32_t(p->mProjectileAge);r->mAnimTime=r->mLastFrameTime=0;}
 if(d->squashPhase==4){if(uint32_t(p->mProjectileAge)-d->squashPhaseAge>=75)p->Die();return true;}
 if(d->squashPhase!=1&&d->squashPhase!=3)return true;
 const float targetX=float(d->squashColumn)-4.8f;float targetY=0;if(!NativeSourceRowLandY313(b,targetX,p->mRow,targetY))return false;if(d->squashPhase==1)targetY+=2;
 float sx=0,sy=0;if(!NativeBoardPointToSource313(b,p->mRow,p->mShadowY,p->mPosX+20,p->mPosY+20,sx,sy))return false;float dx=targetX-sx,dy=targetY-sy,dist=std::sqrt(dx*dx+dy*dy);if(dist>0){const float step=(d->squashPhase==1?5.5f:16.5f)*.01f;sx+=dx/dist*step;sy+=dy/dist*step;}
 float x=0,y=0;if(!NativeSourcePointToBoard313(b,p->mRow,p->mShadowY,sx,sy,x,y))return false;p->mPosX=x-20;p->mPosY=y-20;p->mX=int(p->mPosX);p->mY=int(p->mPosY);p->mVelX=p->mVelY=p->mVelZ=p->mAccZ=0;
 dx=targetX-sx;dy=targetY-sy;if(std::sqrt(dx*dx+dy*dy)>=(std::max)(1.f,b->GetSpeedValue(b->mSpeedMod))*.1f)return true;
 if(d->squashPhase==1){d->squashPhase=2;d->squashPhaseAge=uint32_t(p->mProjectileAge);return true;}
 const int row=p->mRow,col=d->squashColumn;const bool fire=d->squashFire;const uint32_t epoch=GetNativeContext(b)->epoch;if(fire&&!CanApplyNativeSourceFireLine363(b,row,true))return false;d->squashPhase=4;d->squashPhaseAge=uint32_t(p->mProjectileAge);
 for(auto uid:Plants(b,row,col))
 {if(!Same(app,b,epoch))return true;auto* plant=b->mPlants.DataArrayTryToGet(uid);if(!Live(plant))continue;const auto k=GetNativePlantData(plant)->kind;const bool five=(k==903||k==913||k==921||k==925||k==931||k==1151);bool cal=false,nut=false;int level=0;if(!GetNativeSpecialZombie269PlantTraits(plant,cal,nut,level))return false;if(five)ApplyNativeSourcePlantDamage(plant,500,0);else if(!nut||level<3)plant->Die(0);else ApplyNativeSourcePlantDamage(plant,1000,0);}
 if(!Same(app,b,epoch))return true;LawnNet::NetPlaySourceSound284(app,74,.5f);b->ShakeBoard(3,3);return !fire||ApplyNativeSourceFireLine363(b,row,true);
}
}
