#include "FusionNativeQing208.h"
#include "FusionNative.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeProjection313.h"
#include "FusionNativeZombies.h"
#include "FusionNativeEffects269.h"
#include "FusionNativePlantDamage.h"
#include "FusionNativeRemainingPlants269.h"
#include "FusionNativeSourceDamage315.h"
#include "FusionAnimation.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Plant.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include <algorithm>
#include <cmath>
namespace LawnFusion { namespace {
bool Host(const Zombie* z){const auto* d=GetNativeSourceZombieData(z);return z&&d&&d->kind==208&&!z->mDead&&!z->mNetGhost&&!z->mApp->NetIsClient()&&IsNativeSourceWorld313(z->mBoard);}
bool Playing(const Zombie* z){return Host(z)&&!z->mBoard->mPaused&&z->mApp->mGameScene==SCENE_PLAYING;}
bool Live(const Plant* p){return p&&!p->mDead&&!p->mSquished&&p->mIsOnBoard&&GetNativePlantData(p)&&NativeSourcePlantColliderEnabled(p);}
bool PlantEligible(Zombie* z,Plant* p,int maximumRowDifference=0){
 if(!Live(p)||std::abs(p->mRow-z->mRow)>maximumRowDifference)return false;
 bool cal=false,nut=false,flying=false,pumpkin=false,pot=false,lily=false;int level=0;
 if(!GetNativeSpecialZombie269PlantTraits(p,cal,nut,level)||!GetNativeRemaining269PlantTags(p,flying,pumpkin,pot,lily))return false;
 PlantsOnLawn cell;z->mBoard->GetPlantsOnLawn(p->mPlantCol,p->mRow,&cell);
 return FusionQing208PlantAllowed(z->mMindControlled,0,cal,flying,pumpkin,cell.mPumpkinPlant!=nullptr);
}
bool Box(const Zombie* z,Sexy::Rect& out){FusionAnimationBoxCollider303 b;float x=0,y=0;
 if(!GetNativeSourceZombieBox313(z,b)||!b.enabled||!b.nodeActive||!GetNativeSourceZombie269NodePoint(z,"",x,y))return false;
 const float scale=60*z->mScaleZombie;const int left=int(std::floor(x+scale*b.aabb[0])),top=int(std::floor(y-scale*b.aabb[3]));
 out={left,top,int(std::ceil(x+scale*b.aabb[2]))-left,int(std::ceil(y-scale*b.aabb[1]))-top};return true;
}
void Flash(Plant* p){p->mEatenFlashCountdown=25;}
bool QuickDamage(Zombie* z,const NativeQing208Contact& ref){
 if(ref.zombie){auto* q=z->mBoard->mZombies.DataArrayTryToGet(ref.uid);if(!q||q->mDead||q->mRow!=z->mRow||q->mMindControlled==z->mMindControlled||!FusionQing208TargetStatusAllowed(GetNativeFusionZombieStatus(q)))return true;
  return ApplyNativeSourceTypedDamage313(q,4,GetNativeSourceZombieBiteDamage(z,500),false);}
 auto* p=z->mBoard->mPlants.DataArrayTryToGet(ref.uid);if(PlantEligible(z,p)){Flash(p);ApplyNativeSourcePlantDamage(p,GetNativeSourceZombieBiteDamage(z,500),0);}return true;
}
bool Contacts(Zombie* z,std::vector<NativeQing208Contact>& out){Sexy::Rect self;if(!Box(z,self))return false;std::vector<NativeQing208Contact> refs;
 Plant* p=nullptr;while(z->mBoard->IteratePlants(p))if(Live(p)&&GetRectOverlap(self,p->GetPlantRect())>0)refs.push_back({z->mBoard->mPlants.DataArrayGetID(p),false});
 Zombie* q=nullptr;while(z->mBoard->IterateZombies(q)){if(q==z||q->mDead||!GetNativeSourceZombieData(q))continue;Sexy::Rect body;if(Box(q,body)&&GetRectOverlap(self,body)>0)refs.push_back({z->mBoard->mZombies.DataArrayGetID(q),true});}
 out.swap(refs);return true;
}
bool OwnerLive(Board* b,LawnApp* app,uint32_t uid,Zombie*& z){if(app->mBoard!=b)return false;z=b->mZombies.DataArrayTryToGet(uid);return Host(z);}
bool Bite(Zombie* z,const NativeQing208Hooks& h){const auto* d=GetNativeSourceZombieData(z);if(!d||d->beforeDying||GetNativeFusionZombieStatus(z)==1)return true;
 if(d->targetEntity==NativeZombieTargetEntity::Plant&&!z->mMindControlled){auto* p=z->mBoard->mPlants.DataArrayTryToGet(d->targetGeneration);if(!Live(p)||p->mNetId!=d->targetId||p->mRow!=z->mRow)return true;
  Flash(p);if(!h.sound(z,uint16_t(RandRangeInt(8,9)),.3f,1))return false;ApplyNativeSourcePlantDamage(p,GetNativeSourceZombieBiteDamage(z,500),0);return true;}
 if(d->targetEntity==NativeZombieTargetEntity::Zombie){auto* q=z->mBoard->mZombies.DataArrayTryToGet(d->targetGeneration);if(!q||q->mDead||q->mNetId!=d->targetId)return true;if(q->mMindControlled==z->mMindControlled){NativeClockOnlyZombieClear309(z);return true;}
  if(!ApplyNativeSourceTypedDamage313(q,4,GetNativeSourceZombieBiteDamage(z,500),false))return false;return h.sound(z,uint16_t(RandRangeInt(8,9)),.3f,1);}
 if(z->mMindControlled)NativeClockOnlyZombieClear309(z);return true;
}
bool Destroy(Zombie* z,NativeQing208State& s,const NativeQing208Hooks& h){if(s.destroyed)return true;
 float ax=0,ay=0,gx=0,gy=0,x=0,y=0;if(!h.destroyOwner||!NativeSourcePoint313(z,"axis",ax,ay)||!NativeSourceGroundAnchor313(z,gx,gy)||!NativeSourcePointToBoard313(z->mBoard,z->mRow,gy,ax,ay+.75f,x,y))return false;
 const int row=z->mRow,damage=GetNativeSourceZombieBiteDamage(z,500);const bool controlled=z->mMindControlled;std::vector<uint32_t> targets;
 if(!controlled){Plant* p=nullptr;while(z->mBoard->IteratePlants(p))if(PlantEligible(z,p,1)){const auto r=p->GetPlantRect();const float dx=(std::clamp(x,float(r.mX),float(r.mX+r.mWidth))-x)/float(kFusionBasePixelsX),dy=(std::clamp(y,float(r.mY),float(r.mY+r.mHeight))-y)/float(GetNativeFusionPixelsY(z->mBoard));if(dx*dx+dy*dy<=4)targets.push_back(z->mBoard->mPlants.DataArrayGetID(p));}}
 float fx=0,fy=0;if(!NativeSourcePointToBoard313(z->mBoard,row,gy,ax,ay+1,fx,fy))return false;
 SpawnNativeSourceEffect269(z->mBoard,36,fx,fy,z->mRenderOrder+1,true);if(!h.sound(z,40,.5f,1))return false;
 auto* b=z->mBoard;auto* app=z->mApp;s.destroyed=true;if(!h.destroyOwner(z))return false;
 // Source Destroy is scheduled before AttackPlants. Stable full generations
 // preserve callback recipients without looking up a deleted source owner.
 for(uint32_t uid:targets){if(app->mBoard!=b)return true;auto* p=b->mPlants.DataArrayTryToGet(uid);if(Live(p)){Flash(p);ApplyNativeSourcePlantDamage(p,damage,2);}}
 return true;
}
} // namespace
bool EnsureNativeQing208Resources(Board* b,const NativeQing208Hooks& h,std::string* e){
 if(!IsNativeSourceWorld313(b)||!h.trailReady||!h.setTrailEmission||!h.triggerQuickMove||!h.triggerShake||!h.sound||!h.destroyOwner)return false;
 if(!EnsureNativeSourceZombieVisuals(b,208,e)||!CanQueryNativeSourceZombieRoot273(b,208)||!EnsureNativeSourceEffect269(b,36,e)||!h.trailReady(b,e))return false;
 for(uint16_t k:{8,9,40,42})if(!LawnNet::NetSourceSound284Ready(b->mApp,k))return false;return true;
}
bool InitializeNativeQing208(Zombie* z,NativeQing208State& s,const NativeQing208Hooks& h){if(!Host(z)||!EnsureNativeQing208Resources(z->mBoard,h))return false;NativeQing208State next;
 next.initialized=true;next.attributeCountdown=RandRangeFloat(4.4f,7.53f);s=std::move(next);return true;}
bool UpdateNativeQing208(Zombie* z,NativeQing208State& s,const NativeQing208Hooks& h){if(!Playing(z))return true;if(!s.initialized||s.failed||!FusionQing208ViewValid(s))return false;if(s.tick==z->mBoard->mMainCounter)return true;
 std::vector<NativeQing208Contact> current;if(!Contacts(z,current))return false;s.tick=z->mBoard->mMainCounter;
 const auto* d=GetNativeSourceZombieData(z);if(s.attributeCountdown>0){s.attributeCountdown=(std::max)(0.f,s.attributeCountdown-.01f);if(s.attributeCountdown==0&&!d->beforeDying&&GetNativeFusionZombieStatus(z)!=1){if(!h.triggerQuickMove(z))return false;s.attributeCountdown=RandRangeFloat(8.8f,15.45f);}}
 std::vector<NativeQing208Contact> edges;for(const auto& r:current)if(std::find(s.contacts.begin(),s.contacts.end(),r)==s.contacts.end())edges.push_back(r);for(const auto& r:s.contacts)if(std::find(current.begin(),current.end(),r)==current.end())edges.push_back(r);s.contacts=std::move(current);
 if(!s.quickMove||d->beforeDying)return true;auto* b=z->mBoard;auto* app=z->mApp;const uint32_t uid=b->mZombies.DataArrayGetID(z);
 for(const auto& edge:edges){if(!OwnerLive(b,app,uid,z))return true;if(!QuickDamage(z,edge))return false;}return true;}
bool ApplyNativeQing208Event(Zombie* z,NativeQing208State& s,NativeQing208Event event,const NativeQing208Hooks& h){if(!Host(z)||!s.initialized||s.failed||!h.sound)return false;
 switch(event){case NativeQing208Event::StartQuickMove:if(!h.sound(z,42,.3f,1.6f)||!h.setTrailEmission(z,true))return false;s.quickMove=true;return true;
 case NativeQing208Event::QuickMoveOver:if(!h.setTrailEmission(z,false))return false;s.quickMove=false;return true;
 case NativeQing208Event::Bite:return Bite(z,h);case NativeQing208Event::DieEvent:return h.triggerShake&&h.triggerShake(z);case NativeQing208Event::Destroy:return Destroy(z,s,h);}return false;}
}
