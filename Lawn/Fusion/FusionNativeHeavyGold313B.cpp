#include "FusionNativeHeavy313BInternal.h"
#include "FusionGoldConversion313B.h"
#include "FusionNative.h"
#include "FusionNativeProjection313.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeSourceLifecycle313.h"
#include "FusionSourceDeathBudget319.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeVisual.h"
#include "FusionNativeSourceDamage315.h"
#include "FusionNativeCharred311.h"
#include "FusionNativeDroppedCards269.h"
#include "FusionNativeMoney.h"
#include "FusionMoneyRules.h"
#include "FusionProducerRules.h"
#include "FusionNativeEffects269.h"
#include "FusionNativeCold.h"
#include "FusionNativeProjectileMetadata.h"
#include "FusionNativePlantDamage.h"
#include "FusionNativePlacement.h"
#include "SpecialZombie269Rules.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "FusionBasePlantRules.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Projectile.h"
#include "../Coin.h"
#include "../Net/NetCommon.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include <algorithm>
#include <cmath>
namespace LawnFusion::Heavy313B
{
namespace
{
using Event=FusionSourceGraph313Event;using Param=FusionSourceGraph313Param;
// BEGIN PURE GOLD PEAK313B
struct GoldPeak {int coins=0,reanimations=0,attachments=0,cards=0;float points=0;};
bool GoldPendingPeak(uint16_t kind,uint8_t award,bool pointsCommitted,bool deathCommitted,bool rewardCommitted,int64_t maximumSum,GoldPeak& out)
{
 if((kind!=203&&kind!=223)||award>2||maximumSum<0)return false;GoldPeak p;
 if(!pointsCommitted)p.points=float(maximumSum)/12000.f;
 if(kind==223&&!deathCommitted)p.coins=p.reanimations=p.attachments=1;
 if(award&&!rewardCommitted){FusionGoldBoss269Award a;if(!TryPlanFusionGoldBoss269Award(award,a))return false;
  p.reanimations+=a.sourceCoinCount+a.sourceItemCount;p.attachments=p.reanimations;
  p.cards=a.card!=0xffff?1:0;p.coins+=a.sourceCoinCount+a.sourceItemCount+p.cards;p.points+=a.points;}
 out=p;return true;
}
// END PURE GOLD PEAK313B
bool CoinClocksReady()
{
 for(const auto type:{REANIM_SUN,REANIM_COIN_SILVER,REANIM_COIN_GOLD,REANIM_DIAMOND})
 {ReanimatorEnsureDefinitionLoaded(type,false);if(!gReanimatorDefArray||unsigned(type)>=gReanimatorDefCount||!gReanimatorDefArray[type].mTracks.tracks||gReanimatorDefArray[type].mTracks.count<=0)return false;}
 return true;
}
bool CardsFit(Board* b,int requested)
{int used=0;Coin* c=nullptr;while(b->mCoins.IterateNext(c))if(!c->mDead&&GetNativeDroppedCard269Data(c))++used;return requested>=0&&used<=128&&requested<=128-used;}
bool CreditReady(Board* b,float amount)
{const auto* c=GetNativeContext(b);return c&&!b->mApp->NetIsClient()&&std::isfinite(amount)&&amount>=0&&std::isfinite(c->sourcePoints)&&c->sourcePoints>=0&&std::isfinite(c->sourcePoints+amount);}
bool Credit(Board* b,float value)
{if(!CreditReady(b,value))return false;GetNativeContext(b)->sourcePoints+=value;return true;}
bool Node(Zombie* z,const char* path,float& x,float& y){return GetNativeSourceZombie269NodePoint(z,path,x,y);}
bool AwardReady(Zombie* z,const FusionGoldBoss269Award& a)
{
 CoinType type;int value=0;return TryGetFusionMoneyItem(uint16_t(a.sourceItemType),type,value)&&TryGetFusionSourceSunValue(uint16_t(a.sourceCoinType),value)&&
  CoinClocksReady()&&CardsFit(z->mBoard,a.card!=0xffff?1:0)&&
  Pool(z->mBoard,0,a.sourceCoinCount+a.sourceItemCount,a.sourceCoinCount+a.sourceItemCount,0,a.sourceCoinCount+a.sourceItemCount+(a.card!=0xffff))&&
  (a.card==0xffff||CanCreateNativeDroppedCard269(z->mBoard,a.card,0))&&EnsureNativeSourceEffect269(z->mBoard,62)&&CreditReady(z->mBoard,float(a.points));
}
bool Award(Zombie* z,const FusionGoldBoss269Award& a)
{
 if(!AwardReady(z,a))return false;float x=0,y=0;if(!Node(z,"Shadow",x,y))return false;
 auto* b=z->mBoard;int value=0;if(!TryGetFusionSourceSunValue(uint16_t(a.sourceCoinType),value))return false;
 // Tod may drop an admitted cosmetic effect at its own bounded capacity.
 SpawnNativeSourceEffect269(b,62,x,y,z->mRenderOrder+1,true);
 for(int i=0;i<a.sourceCoinCount;++i){auto* c=b->AddCoin(int(x-20),int(y-20),value==50?COIN_LARGESUN:COIN_SUN,COIN_MOTION_FROM_PLANT,SEED_NONE,value);if(!c||!AttachNativeCoinValue(b,c,value))return false;}
 for(int i=0;i<a.sourceItemCount;++i)if(!EmitNativeMoneyItem(b,x,y,uint16_t(a.sourceItemType)))return false;
 if(a.card!=0xffff&&!CreateNativeDroppedCard269(b,a.card,0,x,y))return false;
 if(a.points&&!Credit(b,float(a.points)))return false;
 LawnNet::NetPlaySourceSound284(z->mApp,66,.5f);return true;
}
bool FirstFall(Zombie* z)
{if(!PlayNativeSourceZombieArmor284(z,true))return false;z->mHelmType=HELMTYPE_NONE;return true;}
bool BeginFall(Zombie* z)
{
 auto* s=EditNativeHeavyZombie313B(z);auto* g=EditNativeSourceGraph313(z);if(!s||!g||!PlayNativeSourceZombieArmor284(z,false))return false;
 NativeClockOnlyZombieClear309(z);s->gold.status=5;s->status=0;z->mShieldType=SHIELDTYPE_NONE;z->mHasObject=false;
 g->parameters.triggers|=FusionSourceGraph313Trigger(Param::LosePaper);LawnNet::NetPlaySourceSound284(z->mApp,44,.5f);return true;
}
bool Children(Board* b,std::span<const uint16_t> kinds,std::string* error)
{
 for(auto k:kinds)if(k!=203||!CanSpawnNativeHeavyZombie313B(b,k,error))return false;
 return Pool(b,int(kinds.size()),int(kinds.size()*2),int(kinds.size()),0,0);
}
bool RetireBatch(Zombie* owner,std::span<const uint32_t> ids,std::string* error)
{
 FusionSourceDeathBudget319 births;births.zombies=ids.size();births.reanimations=2*ids.size();births.attachments=ids.size();
 if(!CanRetireNativeSourceZombieBatch313(owner->mBoard,ids,2,births,error))return false;
 // Every selected virtual Die2 is preflighted before parent paper/armor mutation.
 // Native death effects may allocate R/coins themselves; their family preflight
 // must account for the complete batch, not count slots of actors being killed.
 int rewardCoins=0;float points=0;
 for(auto uid:ids){auto* z=owner->mBoard->mZombies.DataArrayTryToGet(uid);if(!z||!CanRetireNativeSourceZombie313(z,2,error))return false;
  if(const auto* s=GetNativeHeavyZombie313B(z);s&&(Kind(z)==203||Kind(z)==223)&&!s->gold.rewardCommitted&&s->gold.awardLevel){FusionGoldBoss269Award a;if(!TryPlanFusionGoldBoss269Award(s->gold.awardLevel,a))return false;rewardCoins+=a.sourceCoinCount+a.sourceItemCount+(a.card!=0xffff)+1;points+=a.points;}
 }
 return Pool(owner->mBoard,0,0,0,0,rewardCoins)&&CreditReady(owner->mBoard,points);
}
void RetireOld(Zombie* z)
{if(!RetireNativeSourceZombie313(z,2))Failed(z->mBoard,"Gold313B prepared source Die2 failed");}
const NativeGoldConversion313BHooks& ConversionHooks()
{
 static const NativeGoldConversion313BHooks h{GetNativeSourceZombieBirthOrder313,
 [](const Zombie* z,float& x,float& y){return NativeSourcePoint313(z,"axis",x,y);},Children,RetireBatch,BeginFall,RetireOld};return h;
}
Plant* Target(Zombie* z)
{
 float axis=0,y=0,shadow=0,sy=0;if(!NativeSourcePoint313(z,"axis",axis,y)||!NativeSourcePoint313(z,"Shadow",shadow,sy))return nullptr;
 Plant* selected=nullptr;float farthest=-1;Plant* p=nullptr;
 while(z->mBoard->mPlants.IterateNext(p)){if(p->mDead||!p->mIsOnBoard||p->mRow!=z->mRow)continue;const auto* d=GetNativePlantData(p);bool spike=false,nut=false;int level=0;
  if(!d||d->kind==1095||IsFusionSourceFlyingKind(d->kind)||!TryGetSpecialZombie269SourcePlantTraits(d->kind,spike,nut,level)||spike)continue;
  float px=0,py=0,sx=0,sy2=0;if(!p->GetShadowCenter(px,py)||!NativeBoardPointToSource313(z->mBoard,z->mRow,py,px,py,sx,sy2))continue;
  const float distance=std::abs(sx-shadow);if(sx<axis&&distance>farthest){farthest=distance;selected=p;}}
 return selected;
}
bool Shot(Zombie* z,bool aimed)
{
 auto* s=EditNativeHeavyZombie313B(z);const auto* data=GetNativeSourceZombieData(z);if(!s||!data)return false;
 if(s->gold.status==1||data->beforeDying||GetNativeZombieGarlicData(z)&&GetNativeZombieGarlicData(z)->phase==GarlicActionPhase::Moving)return true;
 FusionGoldBoss269Shot plan;const bool right=z->mMindControlled!=s->lookBack;
 if(Kind(z)==223){if(!TryPlanFusionGoldBoss269Shot(aimed,z->mMindControlled,right,z->mDead,data->beforeDying,false,plan))return true;}
 else {plan.way=right?0:9;plan.hostile=!z->mMindControlled;plan.velocityMultiplier=1;}
 Plant* target=aimed?Target(z):nullptr;if(aimed&&!target)return true;
 float x=0,y=0,ty=0;const char* path=Kind(z)==203?"Zombie_head/Shoot":aimed?"gun/Shoot2":"Head/paokou3_mid";
 if(!Node(z,path,x,y)||!EnsureNativeProjectileVisuals(z->mBoard,3)||!Pool(z->mBoard,0,0,0,1,0))return false;
 const float py=float(GetNativeFusionPixelsY(z->mBoard));float vx=float((plan.way==9?-1:1)*.06*kFusionBasePixelsX)*plan.velocityMultiplier,vy=0,gravity=0;
 if(aimed){float tx=0;if(!target->GetShadowCenter(tx,ty)||!TryMakeFusionGoldBoss269AimedShot(x,y,tx,ty,py,vx,vy,gravity))return false;}
 NativeProjectileData d;d.kind=3;d.damage=plan.damage;d.specialType=plan.way;d.isZombieBullet=plan.hostile;
 const auto motion=aimed?MOTION_LOBBED:plan.way==9?MOTION_BACKWARDS:MOTION_STRAIGHT;
 if(!NativeProjectileMetadataValid(d,PROJECTILE_PEA,motion))return false;y-=plan.sourceYOffset*py;
 auto* p=z->mBoard->AddProjectile(int(x-20),int(y-20),z->mRenderOrder-1,z->mRow,PROJECTILE_PEA);if(!p)return false;
 p->mMotionType=motion;p->mPosX=x-20;p->mPosY=y-20;p->mPosZ=0;p->mX=int(p->mPosX);p->mY=int(p->mPosY);p->mVelX=vx;p->mVelY=vy;p->mVelZ=0;p->mAccZ=gravity;p->mShadowY=aimed?ty:y;p->mRotation=p->mRotationSpeed=0;
 if(!AttachNativeProjectile(z->mBoard,p,d)){p->Die();return false;}
 LawnNet::NetPlaySourceSound284(z->mApp,uint16_t(RandRangeInt(aimed?0:3,aimed?2:4)),.5f);return true;
}
struct DamagePlan
{FusionGoldBoss269Damage damage;NativeGoldConversion313BPlan conversion;SourceCharred311Birth charred;bool hasConversion=false,hasCharred=false;};
bool PlanDamage(Zombie* z,int resolved,int type,DamagePlan& out,std::string* error)
{
 if(!Playing(z)||resolved<0)return false;const auto* s=GetNativeHeavyZombie313B(z);const auto* d=GetNativeSourceZombieData(z);if(!s||!d)return false;
 FusionGoldBoss269Armor in{s->gold.status,z->mHelmHealth,z->mHelmMaxHealth,z->mShieldHealth,z->mShieldMaxHealth,d->health};DamagePlan p;
 if(!TryPlanFusionGoldPaper269Damage(Kind(z),in,resolved,type,p.damage))return false;
 if(p.damage.secondLost&&Kind(z)==223){if(!PrepareNativeGoldConversion313B(z,ConversionHooks(),p.conversion,error))return false;p.hasConversion=true;}
 if((p.damage.directDie||p.damage.charred)&&!GoldCanRetire(z,error))return false;
 if(p.damage.charred){const auto variant=SourceCharred311SourceVariant(Kind(z),p.damage.after.status,true,z->mInPool,d->beforeDying);
  if(variant){if(variant!=1||!TryMakeNativeSourceCharred311(z,p.damage.after.status,z->mInPool,d->beforeDying,z->mMindControlled!=s->lookBack,p.charred,error)||!EnsureNativeSourceCharred311(z->mBoard,1,error))return false;p.hasCharred=true;}}
 out=std::move(p);return true;
}
}
bool GoldResources(Board* b,uint16_t kind,std::string* error)
{
 return FindFusionGoldBoss269Profile(kind)&&CoinClocksReady()&&EnsureNativeProjectileVisuals(b,3)&&EnsureNativeSourceEffect269(b,62,error)&&
  GetNativeSeedForPlant(256)!=SEED_NONE&&EnsureNativePortraitVisuals(b,256)&&
  (b->mApp->NetIsClient()||CreditReady(b,1252.f));
}
bool GoldUpdate(Zombie* z,NativeHeavyZombie313BState& s)
{
 if(!Playing(z)||z->IsDeadOrDying())return true;
 if(Kind(z)==223&&!CrashContacts(z))return false;
 if(s.gold.status!=6)return true;
 if(--z->mSummonCounter>=0)return true;
 if(Kind(z)==203)
 {z->mSummonCounter=50;auto* graph=EditNativeSourceGraph313(z);return graph&&PlayNativeSourceGraph313(z,*graph,1,"shoot",0,GetNativeHeavyZombie313BGraphHooks());}
 z->mSummonCounter=10;FusionGoldBoss269Attribute step;if(!TryStepFusionGoldBoss269Attribute(s.gold.status,s.gold.attributeCount,Target(z)!=nullptr,step))return false;
 s.gold.attributeCount=step.count;auto* g=EditNativeSourceGraph313(z);if(!g)return false;
 if(step.shoot)g->parameters.triggers|=FusionSourceGraph313Trigger(Param::Shoot);
 if(step.aimedShoot)g->parameters.triggers|=FusionSourceGraph313Trigger(Param::Shoot2);return true;
}
bool GoldPrepareEvent(Zombie* z,Event event,std::string* error)
{
 if(event==Event::Destroy)return CanRetireNativeSourceZombie313(z,2,error);
 if(event==Event::Shoot||event==Event::Shoot2)return EnsureNativeProjectileVisuals(z->mBoard,3)&&Pool(z->mBoard,0,0,0,1,0);
 return true;
}
bool GoldEvent(Zombie* z,NativeHeavyZombie313BState& s,Event event)
{
 switch(event)
 {
 case Event::Shoot:return Shot(z,false);
 case Event::Shoot2:return Shot(z,true);
 case Event::Angry:case Event::Status6:s.gold.status=6;return true;
 case Event::AngrySound:s.gold.status=6;LawnNet::NetPlaySourceSound284(z->mApp,uint16_t(RandRangeInt(45,46)),.5f);return true;
 case Event::Bite:NativeClockOnlyZombieBite313(z);return true;
 case Event::FallSound:z->mApp->PlayFoley(FOLEY_ZOMBIE_FALLING);return true;
 case Event::Destroy:return RetireNativeSourceZombie313(z,2);
 default:return false;
 }
}
bool GoldCanRetire(Zombie* z,std::string* error)
{
 if(!Host(z))return false;const auto* s=GetNativeHeavyZombie313B(z);if(!s)return false;
 GoldPeak peak;if(!GoldPendingPeak(Kind(z),s->gold.awardLevel,s->gold.pointsCommitted,s->gold.deathVisualCommitted,s->gold.rewardCommitted,
  int64_t(z->mBodyMaxHealth)+z->mHelmMaxHealth+z->mShieldMaxHealth,peak)||!CoinClocksReady()||!CardsFit(z->mBoard,peak.cards)||
  !Pool(z->mBoard,0,peak.reanimations,peak.attachments,0,peak.coins)||!CreditReady(z->mBoard,peak.points))return false;
 if(!s->gold.rewardCommitted&&s->gold.awardLevel){FusionGoldBoss269Award a;if(!TryPlanFusionGoldBoss269Award(s->gold.awardLevel,a)||!AwardReady(z,a))return false;}
 (void)error;return true;
}
bool GoldBeforeRetire(Zombie* z)
{
 if(!GoldCanRetire(z,nullptr))return false;auto* s=EditNativeHeavyZombie313B(z);if(!s)return false;
 if(!s->gold.pointsCommitted){s->gold.pointsCommitted=true;if(!Credit(z->mBoard,float(int64_t(z->mBodyMaxHealth)+z->mHelmMaxHealth+z->mShieldMaxHealth)/12000.f))return false;}
 if(Kind(z)==223&&!s->gold.deathVisualCommitted){float x=0,y=0;if(!Node(z,"Shadow",x,y))return false;s->gold.deathVisualCommitted=true;if(!EmitNativeMoneyItem(z->mBoard,x,y-.6f*float(GetNativeFusionPixelsY(z->mBoard)),34))return false;LawnNet::NetPlaySourceSound284(z->mApp,43,.5f);}
 if(!s->gold.rewardCommitted&&s->gold.awardLevel){FusionGoldBoss269Award a;if(!TryPlanFusionGoldBoss269Award(s->gold.awardLevel,a))return false;s->gold.rewardCommitted=true;if(!Award(z,a))return false;}
 z->mDroppedLoot=true;return true;
}
bool GoldDamage(Zombie* z,int resolved,int type,bool dryRun,std::string* error)
{
 if(const auto* s=GetNativeHeavyZombie313B(z);s&&s->gold.status==1)return true;
 DamagePlan p;if(!PlanDamage(z,resolved,type,p,error))return false;if(dryRun)return true;
 auto* s=EditNativeHeavyZombie313B(z);if(!s)return false;
 if(p.damage.firstLost&&!FirstFall(z))return false;
 if(p.damage.secondLost)
 {if(p.hasConversion){if(!CommitNativeGoldConversion313B(z,ConversionHooks(),p.conversion,s->failed,error))return false;}
  else if(!BeginFall(z))return false;}
 if(z->mDead)return true;
 z->mHelmHealth=p.damage.after.first;z->mShieldHealth=p.damage.after.second;s->gold.status=uint8_t(p.damage.after.status);
 if(!p.damage.firstLost&&z->mHelmHealth>0)s->gold.firstStage=uint8_t(FusionGoldBoss269ArmorStage(z->mHelmHealth,z->mHelmMaxHealth,s->gold.firstStage));
 if(!p.damage.secondLost&&z->mShieldHealth>0)s->gold.secondStage=uint8_t(FusionGoldPaper269ArmorStage(Kind(z),z->mShieldHealth,z->mShieldMaxHealth,s->gold.secondStage));
 if(p.damage.bodyDamage){z->TakeBodyDamage(p.damage.bodyDamage,0U);if(auto* d=GetNativeSourceZombieData(z);d&&d->health<0){d->health=0;z->mBodyHealth=1;}}
 if(p.damage.cold)ApplyNativeColdSlow(z);
 if(p.damage.charred||p.damage.directDie){auto* app=z->mApp;auto* b=z->mBoard;if(!RetireNativeSourceZombie313(z,2))return false;if(p.hasCharred&&app->mBoard==b)return StartNativeSourceCharred311(b,p.charred);}
 return true;
}
bool GoldArmorDamage(Zombie* z,bool first,int damage,int& overflow)
{
 if(!Playing(z)||damage<0){overflow=0;return true;}auto* s=EditNativeHeavyZombie313B(z);if(!s){overflow=0;return true;}
 int& hp=first?z->mHelmHealth:z->mShieldHealth;const int maximum=first?z->mHelmMaxHealth:z->mShieldMaxHealth;
 if(!hp){overflow=damage;return true;}
 const int after=(std::max)(0,hp-damage);NativeGoldConversion313BPlan plan;std::string error;
 if(!after&&!first&&Kind(z)==223&&!PrepareNativeGoldConversion313B(z,ConversionHooks(),plan,&error)){overflow=0;return true;}
 if(!after)
 {if(first){if(!FirstFall(z)){overflow=0;return true;}}
  else if(Kind(z)==223){if(!CommitNativeGoldConversion313B(z,ConversionHooks(),plan,s->failed,&error)){overflow=0;return true;}}
  else if(!BeginFall(z)){overflow=0;return true;}}
 overflow=(std::max)(0,damage-hp);hp=after;
 if(after){auto& stage=first?s->gold.firstStage:s->gold.secondStage;stage=uint8_t(first?FusionGoldBoss269ArmorStage(hp,maximum,stage):FusionGoldPaper269ArmorStage(Kind(z),hp,maximum,stage));}
 return true;
}
bool GoldDirectFall(Zombie* z)
{
 if(!Playing(z))return false;if(Kind(z)==203)return BeginFall(z);
 auto* s=EditNativeHeavyZombie313B(z);NativeGoldConversion313BPlan p;std::string error;
 return s&&PrepareNativeGoldConversion313B(z,ConversionHooks(),p,&error)&&CommitNativeGoldConversion313B(z,ConversionHooks(),p,s->failed,&error);
}
}
namespace LawnFusion
{
bool ApplyNativeHeavyGold313BPost(Zombie* z,int award,int round,int difficulty)
{
 if(!Heavy313B::Host(z)||z->mDead||(Heavy313B::Kind(z)!=203&&Heavy313B::Kind(z)!=223)||award<1||award>2)return false;
 auto* s=EditNativeHeavyZombie313B(z);auto* d=GetNativeSourceZombieData(z);FusionGoldBoss269Health next;
 if(!s||!d||!TryScaleFusionGoldBoss269Health(d->health,z->mHelmHealth,z->mShieldHealth,round,difficulty,next)||!Heavy313B::GoldResources(z->mBoard,d->kind,nullptr))return false;
 d->health=next.body;z->mBodyHealth=NativeSourceZombieLivenessHealth(next.body);z->mBodyMaxHealth=next.bodyMaximum;
 z->mHelmHealth=next.first;z->mHelmMaxHealth=next.firstMaximum;z->mShieldHealth=next.second;z->mShieldMaxHealth=next.secondMaximum;s->gold.awardLevel=uint8_t(award);return true;
}
}
