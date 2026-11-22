#include "FusionNativeDriver363.h"
#include "FusionNativeLateZombie363.h"
#include "FusionNativeExtendedZombie313Bridge.h"
#include "FusionNativeNextZombie343State.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeSourceLifecycle313.h"
#include "FusionNativeSourceDamage315.h"
#include "FusionNativeProjection313.h"
#include "FusionNativeZombiePreflight280.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeEffects269.h"
#include "FusionNative.h"
#include "SpecialZombie269Rules.h"
#include "../Zombie.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include <cmath>
#include <limits>
namespace LawnFusion {namespace {
uint16_t Kind(const Zombie* z){const auto* d=GetNativeSourceZombieData(z);return d?d->kind:0xffff;}
bool Host(const Zombie* z){return z&&FusionDriver363Kind(Kind(z))&&!z->mDead&&!z->mNetGhost&&!z->mApp->NetIsClient()&&IsNativeSourceWorld313(z->mBoard);}
bool Live(const Plant* p){return p&&!p->mDead&&p->mIsOnBoard&&p->mPlantHealth>0&&GetNativePlantData(p);}
bool Same(LawnApp* app,Board* b,uint32_t epoch){return app->mBoard==b&&GetNativeContext(b)&&GetNativeContext(b)->epoch==epoch;}
bool Failed(Zombie* z,const char* why){if(auto* s=EditNativeExtendedZombie313(z))s->failed=true;LawnNet::NetTrace("Driver363 stopped kind=%u reason=%s",Kind(z),why);return false;}
bool PrepareSummon(Zombie* z,std::vector<int>& rows,FusionSourceDeathBudget319& peak,std::string* error)
{
 if(!Host(z)||Kind(z)!=222)return false;
 rows.clear();const int count=GetNativeFusionRowCount(z->mBoard);if(z->mRow>0)rows.push_back(z->mRow-1);if(z->mRow+1<count)rows.push_back(z->mRow+1);rows.push_back(z->mRow);rows.push_back(z->mRow);
 std::vector<uint16_t> children;for(int row:rows)if(z->mBoard->mPlantRow[row]!=PLANTROW_POOL){children.push_back(207);children.push_back(217);}
 if(!GetNativeSourceZombieBirthBudget319(z->mBoard,children,peak,error)||!CanAllocateNativeSourceBudget319(z->mBoard,peak,error))return false;
 return EnsureNativeSourceEffect269(z->mBoard,11,error);
}
}
bool EnsureNativeDriver363Resources(Board* b,uint16_t kind,std::string* error)
{
 if(!b||!FusionDriver363Kind(kind)||!IsNativeSourceWorld313(b)||!FindFusionDriver363Graph(kind)||!EnsureNativeSourceZombieVisuals(b,kind,error)||!CanQueryNativeSourceZombieRoot273(b,kind))return false;
 const uint16_t ordinary[]={16,18,10},ultimate[]={201,207,217};
 if(!EnsureNativeSourceZombieClosure280(b,kind==201?std::span<const uint16_t>(ordinary):std::span<const uint16_t>(ultimate),error)||!EnsureNativeSourceEffect269(b,36,error))return false;
 for(int sound:{8,9,43,76,69})if((kind==222||sound!=69)&&!LawnNet::NetSourceSound284Ready(b->mApp,uint16_t(sound)))return false;return true;
}
bool InitializeNativeDriver363(Zombie* z)
{if(!Host(z))return false;auto* s=EditNativeExtendedZombie313(z);if(!s)return false;s->beforeDying=GetNativeSourceZombieData(z)->beforeDying;auto& p=GetNativeContext(z->mBoard)->nextZombies343[z->mBoard->mZombies.DataArrayGetID(z)];p.initialized=true;const auto& h=GetNativeExtendedZombie313Hooks();h.sound(z,76,1);if(Kind(z)==222)h.sound(z,69,.5f);return true;}
bool NativeDriver363ColliderEnabled(const Zombie* z)
{const auto* d=GetNativeSourceZombieData(z);return d&&FusionDriver363Kind(d->kind)&&!z->mDead&&!d->beforeDying;}
bool UpdateNativeDriver363PendingDeath(Zombie* z)
{
 if(!FusionDriver363Kind(Kind(z)))return false;if(!Host(z)||z->mBoard->mPaused||z->mApp->mGameScene!=SCENE_PLAYING)return true;
 if(Kind(z)==201&&GetNativeSourceZombieData(z)->beforeDying&&z->mSummonCounter>0&&z->mZombieAge>=z->mSummonCounter)
 {if(!RetireNativeSourceZombie313(z,2))Failed(z,"prepared2s Invoke retirement refused");}return true;
}
bool NativeDriver363Caltrop(Zombie* z)
{
 if(!Host(z))return false;const auto* d=GetNativeSourceZombieData(z);auto* s=EditNativeExtendedZombie313(z);auto* g=EditNativeSourceGraph313(z);if(!s||!g)return false;
 if(Kind(z)==222||d->health>float(z->mBodyMaxHealth)*.5f)return ApplyNativeSourceTypedDamage313(z,0,int(float(z->mBodyMaxHealth)*.5f));
 if(d->beforeDying)return true;if(z->mZombieAge>(std::numeric_limits<int>::max)()-200||!CanRetireNativeLate363(z,2))return false;
 s->bodyStage=2;s->beforeDying=true;GetNativeSourceZombieData(z)->beforeDying=true;
 g->parameters.triggers|=FusionSourceGraph313Trigger(FusionSourceGraph313Param::Shake)|FusionSourceGraph313Trigger(FusionSourceGraph313Param::Die);
 z->mVelX=z->mVelZ=0;z->mSummonCounter=z->mZombieAge+200;return true;
}
bool ApplyNativeDriver363Body(Zombie* z,int damage)
{
 if(!Host(z)||damage<0)return false;auto* d=GetNativeSourceZombieData(z);auto* s=EditNativeExtendedZombie313(z);auto* g=EditNativeSourceGraph313(z);if(!d||!s||!g)return false;
 const float hp=d->health-float(damage);if(!std::isfinite(hp))return false;
 if(hp<=0&&!CanRetireNativeSourceZombie313(z,2))return Failed(z,"source201/222 death closure refused before subtraction");
 d->health=hp;z->mBodyHealth=NativeSourceZombieLivenessHealth(hp);z->mJustGotShotCounter=25;
 if(hp<=0)return RetireNativeSourceZombie313(z,2);
 uint8_t stage=s->bodyStage;if(!FusionDriver363BodyStage(d->kind,hp,z->mBodyMaxHealth,s->bodyStage,stage))return false;s->bodyStage=stage;
 if(d->kind==201&&hp<float(z->mBodyMaxHealth)/3)g->parameters.triggers|=FusionSourceGraph313Trigger(FusionSourceGraph313Param::Shake);return true;
}
bool UpdateNativeDriver363(Zombie* z)
{
 if(!Host(z)||z->mBoard->mPaused||z->mApp->mGameScene!=SCENE_PLAYING)return true;
 auto* b=z->mBoard;auto* app=z->mApp;const uint32_t uid=b->mZombies.DataArrayGetID(z),epoch=GetNativeContext(b)->epoch;
 auto& privateState=GetNativeContext(b)->nextZombies343[uid];if(privateState.attributeTick==b->mMainCounter)return true;privateState.attributeTick=b->mMainCounter;if(privateState.failed)return false;
 UpdateNativeDriver363PendingDeath(z);if(!Same(app,b,epoch)||(z=b->mZombies.DataArrayTryToGet(uid))==nullptr||z->mDead)return true;
 const auto* d=GetNativeSourceZombieData(z);auto* s=EditNativeExtendedZombie313(z);if(!d||!s)return false;
 // Inherited358BC0 reads actual root.GetChild(4), including the different
 // Ultimate prefab hierarchy; do not substitute a convenient wheel name.
 float front=0,y=0;if(!GetNativeSourceZombie269NodePoint(z,Kind(z)==201?"Zombie_zamboni_wheel_1":"Credits_speaker_overlay_1",front,y))return false;
 const float boundary=b->mIceTimer[z->mRow]>0?float(b->mIceMinX[z->mRow]):float(b->GetCellCenterX(0,z->mRow)+(35+4.8)*kFusionBasePixelsX);
 float vx=0,axisX=0,axisY=0;FusionDriver363Velocity(s->rootFlip,d->beforeDying,front<boundary,vx);
 if(!NativeSourcePoint313(z,"axis",axisX,axisY))return false;
 const float vy=b->StageHasRoof()&&axisX<1.5f?vx*.22f:0;
 const auto& h=GetNativeExtendedZombie313Hooks();if(!h.setVelocity(z,vx,vy))return false;
 if(!NativeDriver363ColliderEnabled(z)||b->mMainCounter%2)return true;
 std::vector<NativeExtendedZombie313Ref> contacts;if(!h.bodyContacts(z,contacts))return false;
 for(const auto& ref:contacts)
 {
  if(!Same(app,b,epoch)||(z=b->mZombies.DataArrayTryToGet(uid))==nullptr||z->mDead)return true;if(!NativeDriver363ColliderEnabled(z))break;
  if(ref.zombie){auto* enemy=b->mZombies.DataArrayTryToGet(ref.localUID);if(enemy&&!enemy->mDead&&enemy->mRow==z->mRow&&enemy->mMindControlled!=z->mMindControlled&&SpecialZombie269CrashZombieAllowed(true,false,GetNativeFusionZombieStatus(enemy))&&!ApplyNativeSourceTypedDamage313(enemy,4,20))return false;continue;}
  auto* p=b->mPlants.DataArrayTryToGet(ref.localUID);if(z->mMindControlled||!Live(p)||p->mRow!=z->mRow)continue;
  bool nut=false,big=false,cal=false,umbrella=false,bite=false;int level=0;SpecialZombie269CrashPlan plan;
  if(!h.plantTraits(p,nut,big,level,cal,umbrella,bite)||!TryPlanSpecialZombie269Crash(18,GetNativePlantData(p)->kind,cal,nut,level,plan))return false;
  if(plan.action==SpecialZombie269CrashAction::Ignore)continue;
  if(plan.killByCaltrop&&!NativeDriver363Caltrop(z))return false;
  if(!Same(app,b,epoch)||(z=b->mZombies.DataArrayTryToGet(uid))==nullptr||z->mDead)return true;
  p=b->mPlants.DataArrayTryToGet(ref.localUID);if(!Live(p))continue;
  if(plan.action==SpecialZombie269CrashAction::Crush){if(!h.crush(z,p,false,RandRangeInt(8,9)))return false;}
  else{if(!h.plantDamage(p,plan.damage,0))return false;if(!Same(app,b,epoch)||(z=b->mZombies.DataArrayTryToGet(uid))==nullptr||z->mDead)return true;if(plan.knockRight&&!h.translateRoot(z,1,0))return false;}
 }
 return true;
}
bool CanDriver363GraphEvent(Zombie* z,FusionSourceGraph313Event e,std::string* error)
{
 if(!Host(z))return false;if(Kind(z)!=222)return true;
 if(e==FusionSourceGraph313Event::JacksonSummon363){if(z->mMindControlled)return true;std::vector<int> rows;FusionSourceDeathBudget319 peak;return PrepareSummon(z,rows,peak,error);}return true;
}
bool Driver363GraphEvent(Zombie* z,FusionSourceGraph313Event e)
{
 if(!CanDriver363GraphEvent(z,e))return false;if(Kind(z)!=222)return true;
 if(e==FusionSourceGraph313Event::JacksonTurn363)
 {auto* d=GetNativeSourceZombieData(z);int32_t next=0;bool point=false,flip=false;StepFusionDriver363Turn(d->sourceStatusLevel,next,point,flip);d->sourceStatusLevel=next;auto* g=EditNativeSourceGraph313(z);if(!g)return false;if(point)g->layerParameters361[1].triggers|=FusionSourceGraph313Trigger(FusionSourceGraph313Param::JacksonPoint363);return true;}
 if(e!=FusionSourceGraph313Event::JacksonSummon363)return true;
 if(z->mMindControlled)return true;
 std::vector<int> rows;FusionSourceDeathBudget319 peak;if(!PrepareSummon(z,rows,peak,nullptr))return false;
 auto* b=z->mBoard;auto* app=z->mApp;const uint32_t uid=b->mZombies.DataArrayGetID(z),epoch=GetNativeContext(b)->epoch;float x=0,y=0;if(!NativeSourcePoint313(z,"axis",x,y))return false;
 for(size_t i=0;i<rows.size();++i)
 {
  const int row=rows[i];if(b->mPlantRow[row]==PLANTROW_POOL)continue;const float sourceOffset=i+2>=rows.size()?(i+1==rows.size()?-2.f:2.f):0.f;
  const float childX=x+sourceOffset;float land=0;if(!NativeSourceRowLandY313(b,childX,row,land))return Failed(z,"prepared child land projection failed");
  auto* child=SpawnNativeExtendedZombieAtAxis313(b,207,row,childX,land,false);if(!child)return Failed(z,"prepared8NPC summon partially emitted");
  float cx=0,cy=0;if(!GetNativeSourceZombie269NodePoint(child,"Shadow",cx,cy))return Failed(z,"prepared207 actualShadow failed");
  SpawnNativeSourceEffect269(b,11,cx,cy-float(.5*GetNativeFusionPixelsY(b)),child->mRenderOrder,true);
  if(!Same(app,b,epoch)||(z=b->mZombies.DataArrayTryToGet(uid))==nullptr||z->mDead)return true;
  if(!NativeSourceRowLandY313(b,childX+.5f,row,land)||!SpawnNativeExtendedZombieAtAxis313(b,217,row,childX+.5f,land,false))return Failed(z,"prepared217 summon partially emitted");
  if(!Same(app,b,epoch)||(z=b->mZombies.DataArrayTryToGet(uid))==nullptr||z->mDead)return true;
 }
 return true;
}
bool Driver363GraphParameters(Zombie* z,uint8_t layer,FusionSourceGraph313Parameters& out)
{
 const auto* d=GetNativeSourceZombieData(z);auto* s=EditNativeExtendedZombie313(z);auto* g=EditNativeSourceGraph313(z);if(!d||!s||!g||!FusionDriver363Kind(d->kind)||layer>1)return false;
 auto p=layer?g->layerParameters361[1]:g->parameters;
 if(!GetNativeExtendedZombie313Hooks().graph.parameters(z,p))return false;p.attacking=false;out=p;return true;
}
const NativeExtendedZombie313Hooks& GetNativeDriver363Hooks()
{static const auto h=[](){auto value=GetNativeExtendedZombie313Hooks();value.graph.parametersForLayer361=Driver363GraphParameters;return value;}();return h;}
}
