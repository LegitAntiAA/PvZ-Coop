#include "FusionNativeProjectileBuff369.h"
#include "FusionNativeGoldBoss269Adapter.h"
#include "FusionNative.h"
#include "FusionNativeZombies.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativeVisual.h"
#include "FusionNativeCold.h"
#include "FusionNativeGarlic.h"
#include "FusionNativeWater268.h"
#include "FusionNativeDroppedCards269.h"
#include "FusionNativeMoney.h"
#include "FusionMoneyRules.h"
#include "FusionProducerRules.h"
#include "FusionNativeEffects269.h"
#include "FusionNativeHostileProjectile.h"
#include "FusionNativeShooterBatch.h"
#include "FusionHostileProjectileRules.h"
#include "FusionNativeProjectileMetadata.h"
#include "FusionNativePlantDamage.h"
#include "FusionNativePlacement.h"
#include "SpecialZombie269Rules.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Coin.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/Attachment.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>
namespace LawnFusion
{
namespace
{
bool Host(const Board* b){return b&&b->mApp&&b->mApp->mBoard==b&&!b->mApp->NetIsClient()&&IsNativeFusionBoard(b);}
bool Live(const Zombie* z){return z&&Host(z->mBoard)&&!z->mNetGhost&&!z->mDead&&!z->mBoard->mPaused&&CanApplyNativeFusionGameplay(z->mBoard);}
Reanimation* Body(const Zombie* z){return z?z->mApp->ReanimationTryToGet(z->mBodyReanimID):nullptr;}
Reanimation* Head(const Zombie* z,bool aimed){return z?z->mApp->ReanimationTryToGet(aimed?z->mBossFireBallReanimID:z->mSpecialHeadReanimID):nullptr;}
float Factor(Zombie* z)
{float factor=1;const auto* garlic=GetNativeZombieGarlicData(z);TryGetFusionZombieStatusSpeedFactor(0,z->mChilledCounter>0,z->mIceTrapCounter>0,z->mButteredCounter>0,garlic&&FusionGarlicStopsWalking(*garlic),factor);return factor*GetNativeWater268GrapSpeedFactor(z);}
bool AddClock(Zombie* z,bool aimed)
{
 if(Head(z,aimed))return true;auto* body=Body(z);if(!body||!body->TrackExists("anim_head1"))return false;
 auto* head=z->mApp->AddReanimation(0,0,0,REANIM_PEASHOOTER);if(!head)return false;
 const auto id=z->mApp->ReanimationGetID(head);head->PlayReanim("anim_shooting",REANIM_PLAY_ONCE_AND_HOLD,0,0);head->mAnimTime=head->mLastFrameTime=1;head->mLoopCount=1;
 if(!AttachReanim(body->GetTrackInstanceByName("anim_head1")->mAttachmentID,head,0,0)){z->mApp->RemoveReanimation(id);return false;}
 if(aimed)z->mBossFireBallReanimID=id;else z->mSpecialHeadReanimID=id;return true;
}
bool ConversionCandidate(const Zombie* z)
{
 const auto* d=GetNativeSourceZombieData(z);if(!z||z->mDead||!d||d->kind==203||z->mRow<0||z->mRow>=GetNativeFusionRowCount(z->mBoard)||z->mBoard->mPlantRow[z->mRow]==PLANTROW_POOL)return false;
 // Every currently implemented kind is covered by actual TypeMgr classes.
 // Gargantuar212 and drivers14/18 are excluded; no unsupported NPC is aliased.
 switch(d->kind){case 0:case 2:case 4:case 8:case 15:case 104:case 109:case 111:case 223:return true;default:return false;}
}
std::vector<uint32_t> ConversionTargets(Zombie* z)
{std::vector<uint32_t> out;Zombie* q=nullptr;while(z->mBoard->IterateZombies(q))if(ConversionCandidate(q))out.push_back(z->mBoard->mZombies.DataArrayGetID(q));std::sort(out.rbegin(),out.rend());if(out.size()>30)out.resize(30);return out;}
void BeginFall(Zombie* z)
{auto* s=EditNativeGoldBoss269State(z);if(!s)return;z->StopEating();s->status=5;z->mZombiePhase=PHASE_NEWSPAPER_MADDENING;z->mShieldType=SHIELDTYPE_NONE;z->mHasObject=false;z->PlayZombieReanim("anim_gasp",REANIM_PLAY_ONCE_AND_HOLD,0,0);UpdateNativeGoldPaper269Animation(z);}
bool AwardExact(const FusionGoldBoss269Award& a)
{FusionGoldBoss269Award expected;const int level=a.points?2:1;return TryPlanFusionGoldBoss269Award(level,expected)&&a.sourceCoinType==expected.sourceCoinType&&a.sourceCoinCount==expected.sourceCoinCount&&a.sourceItemType==expected.sourceItemType&&a.sourceItemCount==expected.sourceItemCount&&a.card==expected.card&&a.points==expected.points;}
bool PointsOnDeath(Zombie* z)
{auto* s=EditNativeGoldBoss269State(z);if(!s)return false;if(s->pointsCommitted)return true;const float points=float(int64_t(z->mBodyMaxHealth)+z->mHelmMaxHealth+z->mShieldMaxHealth)/12000.f;if(!CanCreditNativeGoldPaper269SourcePoints(z->mBoard,points))return false;s->pointsCommitted=true;return CreditNativeGoldPaper269SourcePoints(z->mBoard,points);}
}
bool IsNativeGoldPaper269(const Zombie* z)
{const auto* d=GetNativeSourceZombieData(z);return z&&d&&(d->kind==203||d->kind==223)&&IsNativeFusionBoard(z->mBoard);}
bool CanCreateNativeGoldPaper269(Board* b,uint16_t kind)
{
 if(!b||!FindFusionGoldBoss269Profile(kind)||!EnsureNativeSourceZombieVisuals(b,kind)||!EnsureNativeProjectileVisuals(b,3)||
    !EnsureNativeSourceEffect269(b,62)||GetNativeSeedForPlant(256)==SEED_NONE||!EnsureNativePortraitVisuals(b,256))return false;
 // Ghost adoption preflights the identical real resources, never a host coin
 // allocation or a host-only points transaction.
 return !Host(b)||(CanCreateNativeDroppedCard269(b,256,0)&&CanCreditNativeGoldPaper269SourcePoints(b,1252.f));
}
bool EnsureNativeGoldBoss269Dependencies(Board* b)
{return CanCreateNativeGoldPaper269(b,203)&&CanCreateNativeGoldPaper269(b,223);}
bool GetNativeGoldBoss269SourcePoint(const Zombie* z,const char* field,float& x,float& y)
{
 if(!z||!field)return false;const auto* d=GetNativeSourceZombieData(z);if(!d)return false;
 const char* path=!std::strcmp(field,"axis")||!std::strcmp(field,"shadow")?"Shadow":
  !std::strcmp(field,"shoot2")?"gun/Shoot2":!std::strcmp(field,"shoot")?(d->kind==223?"Head/paokou3_mid":"Zombie_head/Shoot"):field;
 return GetNativeSourceZombie269NodePoint(z,path,x,y);
}
bool NativeGoldBoss269TargetPlantAllowed(const Plant* p)
{const auto* d=GetNativePlantData(p);bool caltrop=false,nut=false;int level=0;return p&&d&&!IsFusionSourceFlyingKind(d->kind)&&TryGetSpecialZombie269SourcePlantTraits(d->kind,caltrop,nut,level)&&!caltrop;}
bool InitializeNativeGoldPaper269(Zombie* z)
{
 if(!IsNativeGoldPaper269(z))return false;const auto kind=GetNativeSourceZombieData(z)->kind;
 if(!CanCreateNativeGoldPaper269(z->mBoard,kind)||!AddClock(z,false)||(kind==223&&!AddClock(z,true)))return false;
 if(z->mNetGhost||z->mApp->NetIsClient())return true;
 if(kind==223){if(!InitializeNativeGoldBoss269(z))return false;}else {auto* s=EditNativeGoldBoss269State(z,true);if(!s)return false;*s={};z->mSummonCounter=0;z->mZombiePhase=PHASE_NEWSPAPER_READING;}
 z->mBossHeadCounter=0;return StartNativeGoldPaper269Walk(z,0);
}
bool UpdateNativeGoldPaper269Animation(Zombie* z)
{
 if(!IsNativeGoldPaper269(z))return false;if(!Live(z)||z->IsDeadOrDying())return true;auto* body=Body(z);const auto* s=GetNativeGoldBoss269State(z);if(!body||!s)return true;
 const bool ultimate=GetNativeSourceZombieData(z)->kind==223;
 const float speed=s->status==5?1.f:z->mIsEating?(s->status==6?6.f:2.f):s->status==6?4.f:ultimate?.3f:.4f;
 z->mOriginalAnimRate=body->mFrameCount*speed*GetNativeSourceZombieData(z)->originSpeed*Factor(z)/2.f;body->mAnimRate=z->mOriginalAnimRate;return true;
}
bool StartNativeGoldPaper269Walk(Zombie* z,int blend)
{
 if(!IsNativeGoldPaper269(z))return false;if(!Live(z)||z->IsDeadOrDying())return true;const auto* s=GetNativeGoldBoss269State(z);if(!s||s->status==5)return true;
 z->PlayZombieReanim(s->status==6?"anim_walk_nopaper":"anim_walk",REANIM_LOOP,blend,0);UpdateNativeGoldPaper269Animation(z);return true;
}
void TriggerNativeGoldBoss269Animation(Zombie* z,bool aimed)
{
 if(!Live(z)||!IsNativeGoldPaper269(z)||!AddClock(z,aimed))return;auto* s=EditNativeGoldBoss269State(z);auto* head=Head(z,aimed);if(!s||!head)return;
 const bool ultimate=GetNativeSourceZombieData(z)->kind==223;const float duration=ultimate?.33333334f:1.8333334f;
 head->PlayReanim("anim_shooting",REANIM_PLAY_ONCE_AND_HOLD,0,head->mFrameCount*(aimed?2.f:3.f)*GetNativeSourceZombieData(z)->originSpeed*Factor(z)/duration);
 if(aimed)s->aimedEvent=1;else z->mBossHeadCounter=1;
}
bool UpdateNativeGoldPaper269(Zombie* z)
{
 if(!IsNativeGoldPaper269(z))return false;if(!Live(z)||z->IsDeadOrDying())return true;auto* s=EditNativeGoldBoss269State(z);if(!s||s->lastAnimationTick==z->mBoard->mMainCounter)return true;s->lastAnimationTick=z->mBoard->mMainCounter;
 const bool ultimate=GetNativeSourceZombieData(z)->kind==223;
 if(ultimate)UpdateNativeGoldBoss269(z);
 else if(s->status==5){auto* body=Body(z);if(body&&body->mAnimTime>=.9759711f){s->status=6;z->mZombiePhase=PHASE_NEWSPAPER_MAD;StartNativeGoldPaper269Walk(z,0);}}
 else if(s->status==6&&!z->mBoard->mTimeStopCounter&&z->mHasHead){if(--z->mSummonCounter<0){z->mSummonCounter=75;TriggerNativeGoldBoss269Animation(z,false);}}
 //223's original body has no Angry event. Do not invent a5->6 timer: its
 // real SecondArmorFall may convert the223 actor itself into203.
 UpdateNativeGoldPaper269Animation(z);
 for(bool aimed:{false,true})
 {
  auto* head=Head(z,aimed);if(!head)continue;int event=aimed?s->aimedEvent:z->mBossHeadCounter;if(!event)continue;
  const float duration=ultimate?.33333334f:1.8333334f;head->mAnimRate=head->mFrameCount*(aimed?2.f:3.f)*GetNativeSourceZombieData(z)->originSpeed*Factor(z)/duration;
  constexpr float standard[]={.6666667f,1.f,1.3333334f,1.6666666f};const int count=ultimate?1:4;
  while(event<=count&&head->mAnimTime*duration>=(ultimate?.16666667f:standard[event-1]))
  {if(ultimate)NativeGoldBoss269AnimShoot(z,aimed,!z->mHasHead,z->mZombieHeight!=HEIGHT_ZOMBIE_NORMAL,z->mMindControlled);else {FusionGoldBoss269Shot shot;shot.way=z->mMindControlled?0:9;shot.hostile=!z->mMindControlled;shot.velocityMultiplier=1;CreateNativeGoldBoss269Cherry(z,shot,nullptr);}++event;}
  if(head->mLoopCount>0)event=0;if(aimed)s->aimedEvent=uint8_t(event);else z->mBossHeadCounter=event;
 }
 return true;
}
bool GetNativeGoldPaper269Status(const Zombie* z,int& out)
{if(!IsNativeGoldPaper269(z))return false;const auto* s=GetNativeGoldBoss269State(z);if(!s)return false;out=z->mDead||const_cast<Zombie*>(z)->IsDeadOrDying()?1:s->status;return true;}
bool GetNativeGoldPaper269Presentation(const Zombie* z,const char*& clip,float& phase)
{if(!IsNativeGoldPaper269(z))return false;const auto* s=GetNativeGoldBoss269State(z);const auto* body=Body(z);if(!s||!body)return false;clip=const_cast<Zombie*>(z)->IsDeadOrDying()?"die":s->status==5?"losePaper":z->mIsEating?(s->status==6?"attack2":"attack"):s->status==6?"walk2":"walk";phase=body->mAnimTime;return std::isfinite(phase)&&phase>=0&&phase<=1;}
bool GetNativeGoldPaper269AddonPose(const Zombie* z,bool aimed,const char*& clip,float& phase)
{if(!IsNativeGoldPaper269(z))return false;const auto* s=GetNativeGoldBoss269State(z);const auto* h=Head(z,aimed);if(!s||!h||!(aimed?s->aimedEvent:z->mBossHeadCounter))return false;clip=aimed?"shootback":"shoot";phase=h->mAnimTime;return std::isfinite(phase)&&phase>=0&&phase<=1;}
bool GetNativeGoldPaper269Collider(const Zombie* z,FusionGoldPaper269Rect& out)
{float x=0,y=0;return IsNativeGoldPaper269(z)&&GetNativeSourceZombie269NodePoint(z,"",x,y)&&
 TryMakeFusionGoldPaper269Collider(GetNativeSourceZombieData(z)->kind,x,y,z->mScaleZombie,z->mMindControlled,kFusionBasePixelsX,GetNativeFusionPixelsY(z->mBoard),out);}
bool CanApplyNativeGoldBoss269ShieldFall(const Zombie* z)
{if(!z||!Host(z->mBoard)||!CanCreateNativeGoldPaper269(z->mBoard,203))return false;const auto targets=ConversionTargets(const_cast<Zombie*>(z));return int(targets.size())<=z->mBoard->mZombies.mMaxSize-z->mBoard->mZombies.mSize;}
void ApplyNativeGoldBoss269ShieldFall(Zombie* z)
{
 if(!Live(z)||!CanApplyNativeGoldBoss269ShieldFall(z))return;BeginFall(z);auto* b=z->mBoard;const auto targets=ConversionTargets(z);
 for(auto generation:targets)if(auto* old=b->mZombies.DataArrayTryToGet(generation);old&&ConversionCandidate(old))
 {float x=0,y=0;if(!old->GetShadowCenter(x,y))continue;NativeSourceZombieSpawnRequest request;request.kind=203;request.row=old->mRow;request.positioned=true;request.shadowX=x;
  auto* child=SpawnNativeSourceZombie(b,request);auto* d=GetNativeSourceZombieData(child);if(!child||!d){LawnNet::NetTrace("Fusion GoldPaper270 conversion allocation failed old=%u generation=%u",old->mNetId,generation);continue;}
  d->health*=5.f;child->mBodyHealth=NativeSourceZombieLivenessHealth(d->health);child->mBodyMaxHealth*=5;child->mHelmHealth*=5;child->mHelmMaxHealth*=5;child->mShieldMaxHealth*=5;child->mShieldHealth=0;BeginFall(child);
  LawnNet::NetTrace("Fusion GoldPaper270 convert source=%u old=%u oldToken=%u child=%u childToken=%u kind=203 scaleHP=5",z->mNetId,old->mNetId,generation,child->mNetId,d->instanceToken);
  if(IsNativeGoldPaper269(old))RetireNativeGoldBoss269(old,2);else {CreditNativeGoldPaper269SourcePoints(b,float(int64_t(old->mBodyMaxHealth)+old->mHelmMaxHealth+old->mShieldMaxHealth)/12000.f);old->mDroppedLoot=true;old->DieNoLoot();}
 }
}
bool BeginNativeGoldPaper269Fall(Zombie* z)
{if(!IsNativeGoldPaper269(z))return false;if(!Live(z))return true;if(GetNativeSourceZombieData(z)->kind==223){if(!CanApplyNativeGoldBoss269ShieldFall(z))return true;ApplyNativeGoldBoss269ShieldFall(z);}else BeginFall(z);return true;}
void RefreshNativeGoldBoss269ArmorVisuals(Zombie* z)
{if(!IsNativeGoldPaper269(z))return;if(auto* d=GetNativeSourceZombieData(z);d)if(const auto* s=GetNativeGoldBoss269State(z))d->firstArmorStage=GetNativeSourceZombieData(z)->kind==203?s->secondStage:s->firstStage;}
bool NativeGoldPaper269Damage(Zombie* z,int damage,int type)
{
 if(!IsNativeGoldPaper269(z))return false;if(GetNativeSourceZombieData(z)->kind==223)return NativeGoldBoss269Damage(z,damage,type);if(!Live(z))return true;
 auto* s=EditNativeGoldBoss269State(z);const auto* d=GetNativeSourceZombieData(z);if(!s||!d)return true;FusionGoldBoss269Damage p;FusionGoldBoss269Armor a{s->status,0,0,z->mShieldHealth,z->mShieldMaxHealth,d->health};if(!TryPlanFusionGoldPaper269Damage(203,a,damage,type,p))return true;
 z->mShieldHealth=p.after.second;if(p.secondLost)BeginFall(z);else if(z->mShieldHealth>0)s->secondStage=uint8_t(FusionGoldPaper269ArmorStage(203,z->mShieldHealth,z->mShieldMaxHealth,s->secondStage));RefreshNativeGoldBoss269ArmorVisuals(z);
 if(p.bodyDamage){z->TakeBodyDamage(p.bodyDamage,0U);if(auto* body=GetNativeSourceZombieData(z);body&&body->health<0){body->health=0;z->mBodyHealth=1;}}
 if(p.cold)ApplyNativeColdSlow(z);if(p.charred)SetNativeGoldBoss269Charred(z);if(p.directDie)RetireNativeGoldBoss269(z,2);return true;
}
bool NativeGoldPaper269BodyDamage(const Zombie* z){const auto* s=GetNativeGoldBoss269State(z);return IsNativeGoldPaper269(z)&&s&&s->status!=6;}
bool NativeGoldPaper269RejectStatus(const Zombie* z,bool mc,bool knock,bool freezeButter)
{const auto* s=GetNativeGoldBoss269State(z);return IsNativeGoldPaper269(z)&&s&&
 (GetNativeSourceZombieData(z)->kind==223?FusionGoldBoss269RejectStatus(s->status,mc,knock,freezeButter):freezeButter&&s->status==5);}
bool ApplyNativeGoldPaper269Post(Zombie* z,int award,int round,int difficulty)
{
 if(!Live(z)||!IsNativeGoldPaper269(z)||award<1||award>2)return false;auto* d=GetNativeSourceZombieData(z);auto* s=EditNativeGoldBoss269State(z);FusionGoldBoss269Health next;
 if(!d||!s||!TryScaleFusionGoldBoss269Health(d->health,z->mHelmHealth,z->mShieldHealth,round,difficulty,next))return false;
 d->health=next.body;z->mBodyHealth=NativeSourceZombieLivenessHealth(next.body);z->mBodyMaxHealth=next.bodyMaximum;z->mHelmHealth=z->mHelmMaxHealth=next.first;z->mShieldHealth=z->mShieldMaxHealth=next.second;s->awardLevel=uint8_t(award);return true;
}
bool CanCreateNativeGoldBoss269Award(const Zombie* z,const FusionGoldBoss269Award& a)
{CoinType type;int value=0;return z&&Host(z->mBoard)&&AwardExact(a)&&TryGetFusionMoneyItem(uint16_t(a.sourceItemType),type,value)&&TryGetFusionSourceSunValue(uint16_t(a.sourceCoinType),value)&&
 FusionProducerCanFitCallback(z->mBoard->mCoins.mSize,z->mBoard->mCoins.mMaxSize,a.sourceCoinCount+a.sourceItemCount+(a.card!=0xffff))&&
 (a.card==0xffff||CanCreateNativeDroppedCard269(z->mBoard,a.card,0))&&EnsureNativeSourceEffect269(z->mBoard,62)&&CanCreditNativeGoldPaper269SourcePoints(z->mBoard,float(a.points));}
bool CreateNativeGoldBoss269Award(Zombie* z,const FusionGoldBoss269Award& a)
{
 if(!CanCreateNativeGoldBoss269Award(z,a))return false;float x=0,y=0;if(!GetNativeGoldBoss269SourcePoint(z,"axis",x,y))return false;auto* b=z->mBoard;int sun=0;if(!TryGetFusionSourceSunValue(uint16_t(a.sourceCoinType),sun))return false;
 if(!SpawnNativeSourceEffect269(b,62,x,y,z->mRenderOrder+1,true))return false;
 for(int i=0;i<a.sourceCoinCount;++i){auto* c=b->AddCoin(int(x-20),int(y-20),sun==50?COIN_LARGESUN:COIN_SUN,COIN_MOTION_FROM_PLANT,SEED_NONE,sun);if(!c||!AttachNativeCoinValue(b,c,sun))return false;}
 for(int i=0;i<a.sourceItemCount;++i)if(!EmitNativeMoneyItem(b,x,y,uint16_t(a.sourceItemType)))return false;
 if(a.card!=0xffff&&!CreateNativeDroppedCard269(b,a.card,0,x,y))return false;
 if(a.points&&!CreditNativeGoldPaper269SourcePoints(b,float(a.points)))return false;
 // Native compatible cue; sourceSound66 resource parity is not asserted.
 z->mApp->PlayFoley(FOLEY_COIN);if(a.points)b->DisplayAdvice(_S("+1250 очков за золотого зомби"),MESSAGE_STYLE_HINT_FAST,ADVICE_NONE);
 LawnNet::NetTrace("Fusion GoldPaper270 sourceAward id=%u level=%d sunType=%d sunCount=%d item=%d itemCount=%d card=%u points=%d",z->mNetId,a.points?2:1,a.sourceCoinType,a.sourceCoinCount,a.sourceItemType,a.sourceItemCount,a.card,a.points);return true;
}
bool CreateNativeGoldBoss269DeathVisual(Zombie* z)
{float x=0,y=0;if(!z||!Host(z->mBoard)||!GetNativeGoldBoss269SourcePoint(z,"shadow",x,y)||!EmitNativeMoneyItem(z->mBoard,x,y-float(.6*GetNativeFusionPixelsY(z->mBoard)),34))return false;z->mApp->PlayFoley(FOLEY_CHERRYBOMB);return true;}
bool NativeGoldPaper269Dying(Zombie* z)
{
 if(!IsNativeGoldPaper269(z)||!Host(z->mBoard)||!PointsOnDeath(z))return false;
 if(GetNativeSourceZombieData(z)->kind==223){const bool okay=NativeGoldBoss269Dying(z);if(okay)z->mDroppedLoot=true;return okay;}
 auto* s=EditNativeGoldBoss269State(z);if(!s)return false;if(!s->awardLevel||s->rewardCommitted){z->mDroppedLoot=true;return true;}FusionGoldBoss269Award a;if(!TryPlanFusionGoldBoss269Award(s->awardLevel,a)||!CanCreateNativeGoldBoss269Award(z,a))return false;s->rewardCommitted=true;const bool okay=CreateNativeGoldBoss269Award(z,a);if(okay)z->mDroppedLoot=true;return okay;
}
bool UpdateNativeGoldPaper269Death(Zombie* z)
{if(!IsNativeGoldPaper269(z)||!Live(z)||z->IsDeadOrDying()||GetNativeSourceZombieData(z)->health>=1)return false;
 if(GetNativeSourceZombieData(z)->kind==223){RetireNativeGoldBoss269(z,2);return true;}return !NativeGoldPaper269Dying(z);}
void RetireNativeGoldBoss269(Zombie* z,int reason)
{if(!IsNativeGoldPaper269(z)||!Host(z->mBoard)||!NativeGoldPaper269Dying(z))return;z->mDroppedLoot=true;LawnNet::NetTrace("Fusion GoldPaper270 sourceDie id=%u reason=%d vanillaLoot=0",z->mNetId,reason);z->DieNoLoot();}
void SetNativeGoldBoss269Charred(Zombie* z)
{if(!IsNativeGoldPaper269(z)||!Live(z)||!NativeGoldPaper269Dying(z))return;z->mDroppedLoot=true;z->ApplyBurn(true);}
void NativeGoldPaper269Removing(Zombie* z)
{if(!IsNativeGoldPaper269(z))return;z->mApp->RemoveReanimation(z->mSpecialHeadReanimID);z->mSpecialHeadReanimID=REANIMATIONID_NULL;if(GetNativeSourceZombieData(z)->kind==223){z->mApp->RemoveReanimation(z->mBossFireBallReanimID);z->mBossFireBallReanimID=REANIMATIONID_NULL;}}
Projectile* CreateNativeGoldBoss269Cherry(Zombie* z,const FusionGoldBoss269Shot& shot,Plant* target)
{
 if(!Live(z)||!IsNativeGoldPaper269(z)||!EnsureNativeProjectileVisuals(z->mBoard,3)||z->mBoard->mProjectiles.mSize>=z->mBoard->mProjectiles.mMaxSize)return nullptr;
 float x=0,y=0;if(!GetNativeGoldBoss269SourcePoint(z,shot.aimed?"shoot2":"shoot",x,y))return nullptr;auto* b=z->mBoard;const float py=float(GetNativeFusionPixelsY(b));
 float vx=float((shot.way==9?-1:1)*.06*kFusionBasePixelsX)*shot.velocityMultiplier,vy=0,g=0,ty=0;
 if(shot.aimed){float tx=0;if(!target||target->mDead||!target->GetShadowCenter(tx,ty)||!TryMakeFusionGoldBoss269AimedShot(x,y,tx,ty,py,vx,vy,g))return nullptr;}
 NativeProjectileData d;d.kind=3;d.damage=shot.damage;d.specialType=shot.way;d.isZombieBullet=shot.hostile;
 const auto motion=shot.aimed?MOTION_LOBBED:shot.way==9?MOTION_BACKWARDS:MOTION_STRAIGHT;if(!NativeProjectileMetadataValid(d,PROJECTILE_PEA,motion))return nullptr;
 y-=shot.sourceYOffset*py;auto* p=b->AddProjectile(int(x-20),int(y-20),z->mRenderOrder-1,z->mRow,PROJECTILE_PEA);if(!p)return nullptr;
 p->mMotionType=motion;p->mPosX=x-20;p->mPosY=y-20;p->mPosZ=0;p->mX=int(p->mPosX);p->mY=int(p->mPosY);p->mVelX=vx;p->mVelY=vy;p->mVelZ=0;p->mAccZ=g;p->mShadowY=shot.aimed?ty:y;p->mRotation=p->mRotationSpeed=0;
 if(!AttachNativeProjectile(b,p,d)){p->Die();return nullptr;}(void)RandRangeInt(shot.aimed?0:3,shot.aimed?2:4);z->mApp->PlayFoley(FOLEY_THROW);
 LawnNet::NetTrace("Fusion GoldPaper270 actualShot source=%u kind=%u projectile=%u payload=3 damage=%d way=%u aimed=%d vx=%.9g vy=%.9g g=%.9g",z->mNetId,GetNativeSourceZombieData(z)->kind,p->mNetId,shot.damage,shot.way,shot.aimed,vx,vy,g);return p;
}
bool IsNativeGoldPaper269AimedProjectile(const Projectile* p)
{const auto* d=GetNativeProjectileData(p);return d&&d->kind==3&&NativeProjectileValidationDamage369(*d)==60&&d->specialType==13&&d->isZombieBullet&&p->mProjectileType==PROJECTILE_PEA&&p->mMotionType==MOTION_LOBBED&&NativeProjectileMetadataValid(*d,p->mProjectileType,p->mMotionType);}
bool UpdateNativeGoldPaper269AimedProjectile(Projectile* p)
{
 if(!IsNativeGoldPaper269AimedProjectile(p))return false;if(p->mDead||p->mNetGhost||!Host(p->mBoard)||!CanApplyNativeFusionGameplay(p->mBoard))return true;if(UpdateNativeSuperCherryStart(p))return true;
 FusionHostilePlanarShot s{p->mPosX+20.,p->mPosY+20.,p->mVelX,p->mVelY,p->mAccZ};if(!AdvanceFusionHostilePlanarShot(s)){p->Die();return true;}
 p->mPosX=float(s.x-20);p->mPosY=float(s.y-20);p->mVelY=float(s.velocityY);p->mX=int(p->mPosX);p->mY=int(p->mPosY);
 FusionRecipientRect r;if(!TryMakeFusionShooterBatchProjectileRect(3,s.x,s.y,r)){p->Die();return true;}const double ratio=GetNativeFusionPixelsY(p->mBoard)/kFusionBasePixelsY;r.y=s.y+(r.y-s.y)*ratio;r.height*=ratio;
 Plant* target=nullptr;while(p->mBoard->IteratePlants(target))if(!target->mDead&&target->mIsOnBoard&&GetNativePlantData(target)&&FusionSuperCherryPlantCollides(GetNativePlantData(target)->kind,p->mRow,target->mRow,NativeSourcePlantColliderEnabled(target)))
 {const auto q=target->GetPlantRect();if(r.x<=q.mX+q.mWidth&&r.x+r.width>=q.mX&&r.y<=q.mY+q.mHeight&&r.y+r.height>=q.mY){QueueNativeSuperCherryStart(p,SelectNativeSuperCherryPlantTarget(p,target),true);return true;}}
 Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))if(!z->mDead&&!z->IsDeadOrDying()&&z->mMindControlled&&z->mRow==p->mRow&&!NativeWater268ZombieCaptured(z))
 {const auto q=z->GetZombieRect();if(r.x<=q.mX+q.mWidth&&r.x+r.width>=q.mX&&r.y<=q.mY+q.mHeight&&r.y+r.height>=q.mY){QueueNativeSuperCherryStart(p,nullptr,false);return true;}}
 if(s.velocityY>0&&s.y>p->mShadowY-.5*GetNativeFusionPixelsY(p->mBoard))QueueNativeSuperCherryStart(p,nullptr,false);return true;
}
bool ApplyNativeGoldBoss269CrashContact(Zombie* z,Plant* p)
{
 if(!Live(z)||!IsNativeGoldBoss269(z)||!p||p->mDead||p->mRow!=z->mRow||z->mMindControlled)return false;const auto* d=GetNativePlantData(p);bool spike=false,nut=false;int level=0;
 if(!d||!TryGetSpecialZombie269SourcePlantTraits(d->kind,spike,nut,level)||spike)return false;
 const bool tough=d->kind==903||d->kind==913||d->kind==921||d->kind==925||d->kind==1151;const bool slippery=d->kind==931||d->kind==1048;
 if(slippery){auto* s=EditNativeGoldBoss269State(z);if(s&&s->status==4){z->mShieldHealth=1;NativeGoldBoss269Damage(z,2,0);if(z->mDead)return true;}}
 if(tough||slippery||nut&&level>=3){ApplyNativeSourcePlantDamage(p,tough||slippery?500:1000,0);z->mPosX+=float(kFusionBasePixelsX);z->mX=int(z->mPosX);}else p->Squish();return true;
}
bool UpdateNativeGoldPaper269Attack(Zombie* z)
{
 if(!IsNativeGoldPaper269(z))return false;const auto kind=GetNativeSourceZombieData(z)->kind;if(!Live(z)||z->IsDeadOrDying())return true;
 if(kind==223)
 {
  if(z->mBoard->mMainCounter%2)return true;FusionGoldPaper269Rect r;if(!GetNativeGoldPaper269Collider(z,r))return true;
  const auto touches=[&](const Sexy::Rect& q){return r.x<=q.mX+q.mWidth&&r.x+r.width>=q.mX&&r.y<=q.mY+q.mHeight&&r.y+r.height>=q.mY;};
  Plant* p=nullptr;while(z->mBoard->IteratePlants(p))if(!p->mDead&&p->mRow==z->mRow&&NativeSourcePlantColliderEnabled(p)&&touches(p->GetPlantRect())){ApplyNativeGoldBoss269CrashContact(z,p);if(z->mDead)return true;}
  Zombie* q=nullptr;while(z->mBoard->IterateZombies(q))if(q!=z&&!q->mDead&&!q->IsDeadOrDying()&&q->mRow==z->mRow&&q->mMindControlled!=z->mMindControlled&&!NativeWater268ZombieCaptured(q)&&!q->IsFlying()&&GetNativeFusionZombieStatus(q)!=12&&touches(q->GetZombieRect()))q->TakeDamage(20,1U);
  return true;
 }
 // Source203 inherits the ordinary paper's one PlayEatSound at1.0833334/2s.
 auto* context=GetNativeContext(z->mBoard);auto* body=Body(z);auto* d=GetNativeSourceZombieData(z);const auto* state=GetNativeGoldBoss269State(z);if(!context||!body||!d||!state||state->status==5)return true;auto& c=context->sourceZombieCursors[z->mBoard->mZombies.DataArrayGetID(z)];
 Plant* plant=d->targetEntity==NativeZombieTargetEntity::Plant?z->mBoard->mPlants.DataArrayTryToGet(d->targetGeneration):nullptr;Zombie* enemy=d->targetEntity==NativeZombieTargetEntity::Zombie?z->mBoard->mZombies.DataArrayTryToGet(d->targetGeneration):nullptr;
 if(plant&&(plant->mDead||plant->mNetId!=d->targetId||!NativeSourcePlantColliderEnabled(plant)||GetRectOverlap(z->GetZombieAttackRect(),plant->GetPlantRect())<=0))plant=nullptr;
 if(enemy&&(enemy->mDead||enemy->IsDeadOrDying()||enemy->mNetId!=d->targetId||enemy->mMindControlled==z->mMindControlled||NativeWater268ZombieCaptured(enemy)||GetRectOverlap(z->GetZombieAttackRect(),enemy->GetZombieRect())<=0))enemy=nullptr;
 if(!plant&&!enemy)
 {if(z->mIsEating)z->StopEating();d->targetEntity=NativeZombieTargetEntity::None;d->targetId=0;d->targetGeneration=0;c.attackValid=false;
  enemy=z->FindZombieTarget();if(!enemy&&!z->mMindControlled)plant=z->FindPlantTarget(ATTACKTYPE_CHEW);
  if(!enemy&&!plant)return true;d->targetEntity=enemy?NativeZombieTargetEntity::Zombie:NativeZombieTargetEntity::Plant;d->targetId=enemy?enemy->mNetId:plant->mNetId;d->targetGeneration=enemy?z->mBoard->mZombies.DataArrayGetID(enemy):z->mBoard->mPlants.DataArrayGetID(plant);z->StartEating();UpdateNativeGoldPaper269Animation(z);}
 const FusionZombieClipPosition now{uint32_t((std::max)(0,body->mLoopCount)),body->mAnimTime*2.f};
 if(c.attackValid&&c.attackFrameStart==body->mFrameStart&&now.loop>=c.attack.loop)
 {const uint64_t before=uint64_t(c.attack.loop)+(c.attack.seconds>=1.0833334f),after=uint64_t(now.loop)+(now.seconds>=1.0833334f);
  if(after>before&&z->mHasHead&&!z->IsImmobilizied()){if(plant)z->EatPlant(plant);else if(enemy)z->EatZombie(enemy);}}
 c.attack=now;c.attackFrameStart=body->mFrameStart;c.attackValid=true;return true;
}
}
