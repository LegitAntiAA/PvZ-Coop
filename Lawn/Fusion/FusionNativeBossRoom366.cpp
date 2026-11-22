#include "FusionNativeBossRoom366.h"
#include "FusionAnimation.h"
#include "../../third_party/nlohmann/json.hpp"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include "FusionNativeBoss361Bridge.h"
#include "FusionNative.h"
#include "FusionNativeZombies.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeSourceLifecycle313.h"
#include "FusionNativeSourceDamage315.h"
#include "FusionNativeZombieVisual.h"
#include "FusionNativePlantDamage.h"
#include "FusionNativePlacement.h"
#include "FusionNativeVisual.h"
#include "FusionNativeCold.h"
#include "FusionNativeStar269.h"
#include "FusionNativeFireActionBatch.h"
#include "FusionNativeBatchActions.h"
#include "FusionNativeShooterBatch.h"
#include "FusionNativeDroppedCards269.h"
#include "FusionNativeConveyor308.h"
#include "FusionNativeTravelConsumers369.h"
#include "FusionWire.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../Plant.h"
#include "../Coin.h"
#include "../CursorObject.h"
#include "../GridItem.h"
#include "../LawnMower.h"
#include "../Net/NetManager.h"
#include "../Net/NetPresentation.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/Attachment.h"
#include "../../SexyAppFramework/Graphics.h"
#include <algorithm>
#include <cmath>
#include <vector>
namespace LawnFusion
{
namespace
{
bool Current(const Board* b){return b&&b->mApp&&b->mApp->mBoard==b&&IsNativeFusionBoard(b)&&GetNativeContext(b)&&!GetNativeContext(b)->adventureTeardown;}
bool Host(const Board* b){return Current(b)&&!b->mApp->NetIsClient();}
bool Playing(Board* b){return Current(b)&&!b->mPaused&&b->mApp->mGameScene==SCENE_PLAYING&&!b->mApp->GetDialogCount()&&GetNativeContext(b)->selection.State().phase==FusionBankPhase::Playing;}
bool Active(Board* b,int slot)
{const auto* c=GetNativeContext(b);if(!c||slot<0||slot>=4||!(c->selection.State().activeMask&(1u<<slot)))return false;
 return !b->mApp->NetActive()?slot==0:b->mApp->mNetManager&&b->mApp->mNetManager->GetPlayer(slot).mActive&&!b->mApp->mNetManager->GetPlayer(slot).mSpectator;}
bool Invincible(const Plant* p){return NativeStar269Invulnerable(p)||NativeBatchPlantInvincible(p)||NativeFireActionInvincible(p)||NativeShooterBatchInvulnerable(p)||IsNativeIceShroom(p);}
int Local(Board* b){return b->mApp->NetActive()?b->mApp->mNetManager->GetMySlot():0;}
CursorObject* Cursor(Board* b,int slot){return slot==Local(b)?b->mCursorObject:slot>=0&&slot<4?b->mNetPlayerCursors[slot]:nullptr;}
std::vector<uint32_t> Plants(Board* b)
{std::vector<uint32_t> ids;Plant* p=nullptr;while(b->mPlants.IterateNext(p))if(!p->mDead&&p->mIsOnBoard&&GetNativePlantData(p))ids.push_back(b->mPlants.DataArrayGetID(p));return ids;}
bool Same(LawnApp* app,Board* b,uint32_t epoch){return app->mBoard==b&&Current(b)&&GetNativeContext(b)->epoch==epoch;}
void Sound(Board* b,uint16_t id){LawnNet::NetPlaySourceSound284(b->mApp,id,.5f);}
bool RestartBoss2(Board* b)
{
 auto* room=EditNativeBossRoom366(b);if(!room||!Host(b))return false;
 const auto ids=Plants(b);const int count=5*GetNativeFusionRowCount(b);
 // Fresh Pots and Boss coexist with old actors until native cleanup; no dead credit.
 FusionSourceDeathBudget319 peak;peak.zombies=1;peak.reanimations=3+count;peak.attachments=2;
 if(uint64_t(b->mPlants.mMaxSize-b->mPlants.mSize)<uint64_t(count)||!CanAllocateNativeSourceBudget319(b,peak)||!EnsureNativeVisuals(b,std::vector<uint16_t>{27},{},nullptr)||!CanSpawnNativeBoss361Actor(b,46))return false;
 auto* app=b->mApp;const auto epoch=GetNativeContext(b)->epoch;
 if(auto* trophy=b->mCoins.DataArrayTryToGet(room->trophyToken))if(trophy->mNetId==room->trophyId)trophy->Die();
 room->boss2=true;room->won=false;room->trophyId=0;room->trophyToken=0;room->trophyClaimed=false;
 NativeSourceZombieSpawnRequest r;r.kind=46;r.row=0;r.fromWave=0;r.positioned=true;r.shadowX=float(b->GetCellCenterX(0,0))+4.8f*float(kFusionBasePixelsX);
 if(!SpawnNativeSourceZombie(b,r))return false;
 for(auto uid:ids){if(!Same(app,b,epoch))return true;if(auto* p=b->mPlants.DataArrayTryToGet(uid))if(!p->mDead)p->Die(0);}
 if(!Same(app,b,epoch))return true;
 if(IsNativeSourceConveyor308(b)){auto next=GetNativeContext(b)->selection;auto state=next.State();state.packets.clear();++state.revision;if(!next.ApplySnapshot(state))return false;GetNativeContext(b)->selection=std::move(next);if(!SyncNativeConveyor308(b))return false;if(auto* conveyor=GetNativeContext(b)->conveyor308.get())conveyor->feed={};}
 for(int col=0;col<5;++col)for(int row=0;row<GetNativeFusionRowCount(b);++row){NativePlantData incoming;incoming.kind=27;incoming.owner=LawnNet::NET_SHARED_OWNER;if(!TrySpawnNativeFusionPlant(b,col,row,incoming))return false;}
 room=EditNativeBossRoom366(b);if(!room)return false;room->hammerGranted=room->gloveGranted=false;
 LawnNet::NetTrace("Boss366 source fake trophy restart ->46 fresh Pots=%d profileCommit=0",count);return true;
}
}
NativeBossRoom366* EditNativeBossRoom366(Board* b){return Current(b)?&GetNativeContext(b)->bossRoom366:nullptr;}
const NativeBossRoom366* GetNativeBossRoom366(const Board* b){return EditNativeBossRoom366(const_cast<Board*>(b));}
bool EnsureNativeBossRoom366(Board* b,std::string* error)
{if(!Current(b))return false;ReanimatorEnsureDefinitionLoaded(REANIM_HAMMER,false);
 for(uint16_t id:{43,64,84,93})if(!LawnNet::NetSourceSound284Ready(b->mApp,id)){if(error)*error="Boss room genuine source audio unavailable";return false;}
 auto* context=GetNativeContext(b);if(!context->bossRoomVisuals366)context->bossRoomVisuals366=std::make_unique<NativeBossRoomVisuals366>();
 std::string why;if(!context->bossRoomVisuals366->Ensure(b->mApp,why)){if(error)*error=why;return false;}
 return true;}
bool BeginNativeBossRoom366(Board* b,bool stronger,std::string* error)
{
 if(!Host(b)||GetNativeFusionRowCount(b)!=5||!IsNativeSourceWorld313(b))return false;
 auto* context=GetNativeContext(b);auto* room=EditNativeBossRoom366(b);
 if(!room||context->adventure.enabled||room->spawnPending375||room->trophyId)return false;
 Plant* plant=nullptr;while(b->mPlants.IterateNext(plant))if(!plant->mDead)return false;
 Zombie* zombie=nullptr;while(b->mZombies.IterateNext(zombie))if(!zombie->mDead)return false;
 const auto phase=context->selection.State().phase;
 if(phase!=FusionBankPhase::Choosing&&phase!=FusionBankPhase::Intro&&phase!=FusionBankPhase::Playing)return false;
 // This resource/capacity preflight occurs before scene policy or source RNG.
 const uint16_t kind=44;FusionSourceDeathBudget319 peak;
 if(!CanSpawnNativeBoss361Actor(b,kind,error)||!GetNativeSourceZombieBirthBudget319(b,std::span(&kind,1),peak,error)||
    !CanAllocateNativeSourceBudget319(b,peak,error))return false;
 if(!ConfigureNativeFusionBackground(b,BACKGROUND_6_BOSS))return false;
 *room={};room->enabled=true;room->fakeTrophyNext=!stronger;
 room->spawnPending375=true;room->strongerStart375=stronger;
 // InitBoard.MoveOverEvent omits CreateMower for either real source Boss tag.
 LawnMower* mower=nullptr;while(b->mLawnMowers.IterateNext(mower))if(!mower->mDead)mower->Die();
 return true;
}
bool GrantNativeBossTools366(Board* b,int band)
{auto* room=EditNativeBossRoom366(b);if(!room||!Host(b)||band<1||band>2)return false;room->hammerGranted=true;if(band==2)room->gloveGranted=true;return true;}
bool NativeBossRoomGloveAllowed366(const Board* b)
{const auto* room=GetNativeBossRoom366(b);return !room||!room->enabled||room->gloveGranted;}
void UpdateNativeBossRoom366(Board* b)
{
 auto* r=EditNativeBossRoom366(b);if(!r||!r->enabled||!Host(b)||b->mPaused||!Playing(b)||r->lastTick==b->mMainCounter)return;
 r->lastTick=b->mMainCounter;
 if(r->spawnPending375)
 {
  const uint16_t kind=44;FusionSourceDeathBudget319 peak;
  if(!CanSpawnNativeBoss361Actor(b,kind)||!GetNativeSourceZombieBirthBudget319(b,std::span(&kind,1),peak)||!CanAllocateNativeSourceBudget319(b,peak))return;
  NativeSourceZombieSpawnRequest request;request.kind=44;request.row=0;request.fromWave=0;request.positioned=true;
  request.shadowX=float(b->GetCellCenterX(0,0))+4.8f*float(kFusionBasePixelsX);
  const bool stronger=r->strongerStart375;auto* app=b->mApp;const auto epoch=GetNativeContext(b)->epoch;
  r->spawnPending375=false; // Consume before the real factory/source callbacks.
  auto* made=SpawnNativeSourceZombie(b,request);
  if(!Same(app,b,epoch))return;
  if(!made){LawnNet::NetTrace("Boss375 native room factory failed after consumed start; no replay");return;}
  const auto uid=b->mZombies.DataArrayGetID(made);auto* context=GetNativeContext(b);auto found=context->sourceZombies.find(uid);
  if(found==context->sourceZombies.end())return;
  if(stronger)
  {
   // Source InitBoard45D3C0 calls Lawnf.SetZombieHealth(5) synchronously
   // after fresh SetZombie44 and before its first source Update.
   found->second.health*=5;made->mBodyMaxHealth*=5;made->mBodyHealth=NativeSourceZombieLivenessHealth(found->second.health);
  }
 }
 if(r->hammerCooldownTicks<6000)r->hammerCooldownTicks=uint16_t((std::min)(6000,int(r->hammerCooldownTicks)+GetNativeTravelHammerStep369(b)));
}
bool CanCreateNativeBossTrophy366(Board* b,std::string* error)
{FusionSourceDeathBudget319 peak;peak.coins=1;return Host(b)&&CanAllocateNativeSourceBudget319(b,peak,error);}
bool CreateNativeBossTrophy366(Zombie* z)
{
 if(!z||!GetNativeSourceZombieData(z)||!CanCreateNativeBossTrophy366(z->mBoard))return false;auto* b=z->mBoard;auto* r=EditNativeBossRoom366(b);if(!r||r->trophyId)return r&&r->trophyId;
 float x=0,y=0;if(!GetNativeSourceZombie269NodePoint(z,"",x,y))return false;y-=float(GetNativeFusionPixelsY(b));x=(std::clamp)(x,.2f*float(b->mWidth),.8f*float(b->mWidth));y=(std::clamp)(y,.2f*float(b->mHeight),.8f*float(b->mHeight));
 auto* c=b->mCoins.DataArrayAlloc();c->mApp=b->mApp;c->mBoard=b;c->CoinInitializeNetGhost(int(x),int(y),COIN_TROPHY,COIN_MOTION_COIN,nullptr);c->mNetGhost=false;c->mNetId=b->NetAllocId();if(!c->mNetId){b->mCoins.DataArrayFree(c);return false;}
 c->mPosX=x-c->mWidth*.5f;c->mPosY=y-c->mHeight*.5f;c->mVelX=-1.5f*float(kFusionBasePixelsX)*.01f;c->mVelY=-4*float(GetNativeFusionPixelsY(b))*.01f;c->mCoinAge=0;c->mHitGround=false;c->mIsBeingCollected=false;
 r->trophyId=c->mNetId;r->trophyToken=b->mCoins.DataArrayGetID(c);r->trophyStartX=c->mPosX;r->trophyStartY=c->mPosY;r->trophyFake=GetNativeSourceZombieData(z)->kind==44&&r->fakeTrophyNext;r->trophyClaimed=false;r->trophyAnimationBaseAge=0;return true;
}
bool IsNativeBossTrophy366(const Coin* c)
{const auto* r=c?GetNativeBossRoom366(c->mBoard):nullptr;return r&&r->enabled&&r->trophyId&&c->mNetId==r->trophyId&&c->mType==COIN_TROPHY&&(c->mNetGhost||c->mBoard->mCoins.DataArrayGetID(const_cast<Coin*>(c))==r->trophyToken);}
bool UpdateNativeBossTrophy366(Coin* c)
{
 if(!IsNativeBossTrophy366(c))return false;auto* b=c->mBoard;auto* r=EditNativeBossRoom366(b);if(!Host(b)||c->mNetGhost||c->mDead||b->mPaused||!Playing(b))return true;
 ++c->mCoinAge;
 if(!r->trophyClaimed){if(!c->mHitGround){c->mVelY+=float(9.8*GetNativeFusionPixelsY(b)*.0001);c->mPosX+=c->mVelX;c->mPosY+=c->mVelY;if(c->mPosY>=r->trophyStartY+.5f*float(GetNativeFusionPixelsY(b))){c->mHitGround=true;c->mVelX=c->mVelY=0;}}return true;}
 // Native Coin age is the source MoveAndScale coroutine's single clock.
 const float t=(std::min)(1.f,float(c->mCoinAge)/125.f);const float targetX=float(b->GetCellCenterX(0,0))+(10+4.8f)*float(kFusionBasePixelsX)-c->mWidth*.5f;
 const float targetY=float(b->GridToPixelY(0,0)+50)+(2.3f+3.f)*float(GetNativeFusionPixelsY(b))-c->mHeight*.5f;
 c->mPosX=c->mCollectX+(targetX-c->mCollectX)*t;c->mPosY=c->mCollectY+(targetY-c->mCollectY)*t;c->mScale=1-.5f*t;
 if(c->mCoinAge>=125){if(r->trophyFake){if(!RestartBoss2(b))LawnNet::NetTrace("Boss366 second-room restart refused after trophy transition");}else{r->won=true;c->Die();}}
 return true;
}
bool HandleNativeBossRoom366Command(Board* b,int slot,const FusionCommand& command)
{
 auto* r=EditNativeBossRoom366(b);if(!Host(b)||!r||!r->enabled||!Active(b,slot)||command.epoch!=GetNativeContext(b)->epoch||b->mPaused||!Playing(b))return false;
 if(command.action==FusionCommandKind::BossTrophy366)
 {if(!r->trophyId||r->trophyClaimed||command.sourceId!=r->trophyId||command.sourceToken!=r->trophyToken)return false;Coin* coin=nullptr;while(b->mCoins.IterateNext(coin))if(IsNativeBossTrophy366(coin)&&!coin->mDead)break;if(!coin)return false;
  if(r->trophyFake&&!CanSpawnNativeBoss361Actor(b,46))return false;r->trophyClaimed=true;r->trophyAnimationBaseAge=uint32_t(coin->mCoinAge);coin->mCollectX=coin->mPosX;coin->mCollectY=coin->mPosY;coin->mCoinAge=0;coin->mHitGround=true;coin->mVelX=coin->mVelY=0;if(!r->trophyFake)Sound(b,84);
  // Source ClearTheBoard uses real source removals; no Classic completion/profile hook.
  const auto ids=Plants(b);auto* app=b->mApp;const auto epoch=GetNativeContext(b)->epoch;for(auto uid:ids){if(!Same(app,b,epoch))return true;if(auto* p=b->mPlants.DataArrayTryToGet(uid))if(!p->mDead)p->Die(0);}return true;}
 if(command.action!=FusionCommandKind::BossHammer366||!r->hammerGranted||r->hammerCooldownTicks<6000||command.boardTargetX<0||command.boardTargetX>b->mWidth||command.boardTargetY<0||command.boardTargetY>b->mHeight)return false;
 std::vector<uint32_t> targets;Zombie* z=nullptr;const float px=float(kFusionBasePixelsX),py=float(GetNativeFusionPixelsY(b));
 while(b->mZombies.IterateNext(z))if(!z->mDead&&GetNativeSourceZombieData(z)&&GetNativeFusionZombieStatus(z)!=1){float x=0,y=0;if(!GetNativeSourceZombie269NodePoint(z,"Shadow",x,y))return false;const float dx=(x-command.boardTargetX)/px,dy=(y-command.boardTargetY)/py;if(dx*dx+dy*dy<=.25f)targets.push_back(b->mZombies.DataArrayGetID(z));}
 // All callback leaves preflight as one batch before the million-damage hits.
 if(!CanRetireNativeSourceZombieBatch313(b,targets,1))return false;Sound(b,93);auto* app=b->mApp;const auto epoch=GetNativeContext(b)->epoch;
 for(auto uid:targets){if(!Same(app,b,epoch))return true;if(auto* target=b->mZombies.DataArrayTryToGet(uid))if(!target->mDead&&!ApplyNativeSourceTypedDamage313(target,4,1000000,false))return false;}
 if(!targets.empty()&&Same(app,b,epoch)){EditNativeBossRoom366(b)->hammerCooldownTicks=0;Sound(b,64);}return true;
}
bool ToggleNativeBossHammer366(Board* b)
{
 auto* r=EditNativeBossRoom366(b);if(!r||!r->enabled||!r->hammerGranted||r->hammerCooldownTicks<6000||!b->mCursorObject)return false;auto* cursor=b->mCursorObject;
 if(cursor->mCursorType==CURSOR_TYPE_HAMMER){cursor->mCursorType=CURSOR_TYPE_NORMAL;return true;}
 if(cursor->mCursorType!=CURSOR_TYPE_NORMAL)return false;
 if(!b->mApp->ReanimationTryToGet(cursor->mReanimCursorID)){ReanimatorEnsureDefinitionLoaded(REANIM_HAMMER,false);FusionSourceDeathBudget319 peak;peak.reanimations=1;if(!CanAllocateNativeSourceBudget319(b,peak))return false;
  auto* hammer=b->mApp->AddReanimation(-25,16,0,REANIM_HAMMER);if(!hammer)return false;hammer->mIsAttachment=true;hammer->PlayReanim("anim_whack_zombie",REANIM_PLAY_ONCE_AND_HOLD,0,24);hammer->mAnimTime=1;cursor->mReanimCursorID=b->mApp->ReanimationGetID(hammer);}
 cursor->mCursorType=CURSOR_TYPE_HAMMER;return true;
}
bool HandleNativeBossRoom366Pointer(Board* b,int button,int x,int y)
{
 const auto* r=GetNativeBossRoom366(b);if(!r||!r->enabled||!button||!b->mCursorObject)return false;
 if(button>0&&x>=10&&x<78&&y>=95&&y<127&&r->hammerGranted)return ToggleNativeBossHammer366(b);
 FusionCommand c;
 if(b->mCursorObject->mCursorType==CURSOR_TYPE_HAMMER){if(button<0){b->mCursorObject->mCursorType=CURSOR_TYPE_NORMAL;return true;}c.action=FusionCommandKind::BossHammer366;c.boardTargetX=int16_t(x);c.boardTargetY=int16_t(y);b->mCursorObject->mCursorType=CURSOR_TYPE_NORMAL;return b->mApp->SubmitNativeFusionCommand(c);}
 Coin* coin=nullptr;while(b->mCoins.IterateNext(coin))if(!coin->mDead&&IsNativeBossTrophy366(coin)&&x>=coin->mPosX&&x<=coin->mPosX+coin->mWidth&&y>=coin->mPosY&&y<=coin->mPosY+coin->mHeight)
 {c.action=FusionCommandKind::BossTrophy366;c.sourceId=r->trophyId;c.sourceToken=r->trophyToken;return b->mApp->SubmitNativeFusionCommand(c);}return false;
}
void DrawNativeBossTools366(Board* b,Sexy::Graphics* g)
{const auto* r=GetNativeBossRoom366(b);if(!r||!r->enabled||!r->hammerGranted||!g)return;g->SetColor(Sexy::Color(75,50,25,220));g->FillRect(10,95,68,32);g->SetColor(Sexy::Color(220,190,80));g->FillRect(14,119,int(60u*r->hammerCooldownTicks/6000u),4);
 // Existing real native hammer art; its body clock is the cursor's real R.
 auto* cursor=b->mCursorObject;if(cursor)if(auto* hammer=b->mApp->ReanimationTryToGet(cursor->mReanimCursorID)){Sexy::Graphics local(*g);local.Translate(40,99);hammer->Draw(&local);}}

bool NativeSourcePlantInvincible366(const Plant* p){return p&&Invincible(p);}
bool PreflightNativeBossCrush366(Zombie* boss,const FusionBoss361View& view,bool rv,std::string* error)
{
 if(!boss||!Host(boss->mBoard)||!FusionBoss361ViewValid(view))return false;
 if(rv&&view.kind==46&&!CanFallNativeConveyorCards366(boss->mBoard,error))return false;
 return true; // Existing arbitrary recursive plant callback budget boundary.
}
bool ApplyNativeSourcePlantCrashed366(Plant* p,Zombie* boss)
{
 if(!p||!Host(p->mBoard)||p->mDead||Invincible(p))return true;auto* d=GetNativePlantData(p);if(!d)return false;if(d->crushFlags366&1)return true;
 const auto kind=d->kind;d->crushFlags366|=1;
 const auto* bossData=GetNativeSourceZombieData(boss);const bool isBoss=bossData&&FusionBoss361Kind(bossData->kind);
 const bool immune=kind==13||kind==15||kind==1049||kind==1050||kind==1051||kind==1054||kind==1057||kind==1066;
 const bool rock=kind==906||kind==1060||kind==1074||kind==1075;
 if(!isBoss&&immune)return true;if(!isBoss&&rock){ApplyNativeSourcePlantDamage(p,50,0);return true;}
 if(kind==2||kind==1002||kind==16||kind==1017||kind==11||kind==928||kind==1040||((kind==4||kind==1007||kind==1009)&&p->mState!=PlantState::STATE_NOTREADY))
 {p->DoSpecial();return true;}
 if(kind==10){float x=0,y=0;if(!p->GetShadowCenter(x,y)||!ApplyNativeColdBoardFreeze(p->mBoard,x,y))return false;p->Die(0);return true;}
 if(kind==1003||kind==1010||kind==1015||kind==1052||kind==1053){p->Die(0);return true;}
 auto* b=p->mBoard;auto* app=p->mApp;const uint32_t uid=b->mPlants.DataArrayGetID(p),epoch=GetNativeContext(b)->epoch;
 // Source destroys the Plant component now and keeps only its 3s flattened
 // GameObject. Native's existing squished Plant is that render-only tail.
 // Real Die(0) runs all current source/native removal callbacks at this point.
 float shadowX=0,shadowY=0;if(!p->GetShadowCenter(shadowX,shadowY))return false;
 p->Die(0);
 if(!Same(app,b,epoch))return true;p=b->mPlants.DataArrayTryToGet(uid);d=GetNativePlantData(p);if(!p||!d)return true;
 d->crushFlags366|=2;d->crushShadowX366=shadowX;d->crushShadowY366=shadowY;p->mDead=false;p->mSquished=true;p->mDisappearCountdown=300;
 if(auto* body=app->ReanimationTryToGet(p->mBodyReanimID))body->mAnimRate=0;
 return true;
}
bool ApplyNativeBossCrush366(Zombie* boss,const FusionBoss361View& v,bool rv)
{
 if(!PreflightNativeBossCrush366(boss,v,rv))return false;auto* b=boss->mBoard;auto* app=boss->mApp;const auto epoch=GetNativeContext(b)->epoch;
 const auto ids=Plants(b);bool crushed=false;
 for(auto it=ids.rbegin();it!=ids.rend();++it){if(!Same(app,b,epoch))return true;auto* p=b->mPlants.DataArrayTryToGet(*it);if(!p||p->mDead||Invincible(p))continue;
  const bool match=rv?FusionBoss361RvCell(v.targetRow,v.targetColumn,p->mRow,p->mPlantCol):FusionBoss361StompCell(v.targetRow,p->mRow,p->mPlantCol,true);
  if(!match)continue;crushed=true;if(!ApplyNativeSourcePlantCrashed366(p,rv?nullptr:boss))return false;}
 if(!Same(app,b,epoch))return true;if(crushed)Sound(b,uint16_t(RandRangeInt(8,9)));
 if(rv){Sound(b,120);if(v.kind==46){std::vector<uint32_t> cards;Coin* c=nullptr;while(b->mCoins.IterateNext(c))if(!c->mDead&&IsNativeDroppedCard269(c))cards.push_back(b->mCoins.DataArrayGetID(c));for(auto it=cards.rbegin();it!=cards.rend();++it)if(auto* q=b->mCoins.DataArrayTryToGet(*it))if(!FallNativeDroppedCardByBoss366(q))return false;return FallNativeConveyorCards366(b);}}
 else{b->ShakeBoard(15,-15);Sound(b,74);}return true;
}
struct NativeBossRoomVisuals366::Impl{std::unique_ptr<FusionAnimation> trophy,fake;LawnApp* app=nullptr;};
NativeBossRoomVisuals366::NativeBossRoomVisuals366():impl(std::make_unique<Impl>()){}
NativeBossRoomVisuals366::~NativeBossRoomVisuals366()=default;
bool NativeBossRoomVisuals366::Ensure(LawnApp* app,std::string& error)
{
 if(impl->app==app&&impl->trophy&&impl->fake)return true;
 try{std::wstring exe(32768,L'\0');auto n=GetModuleFileNameW(nullptr,exe.data(),DWORD(exe.size()));if(!n||n>=exe.size())return false;exe.resize(n);
 auto root=std::filesystem::path(exe).parent_path()/"fusion"/"boss366";
 std::unique_ptr<FusionAnimation> models[2];int i=0;
 for(const char* name:{"TrophyPrefab","FakeTrophyPrefab"}){const auto path=root/name/"animation.json";std::ifstream input(path);auto json=nlohmann::json::parse(input);
  if(json.at("sourcePrefab")!=std::string("Resources/Board/Award/")+name+".prefab"||json.at("nodes").size()!=8||json.at("sprites").size()!=3)throw std::runtime_error("Boss source Prize model identity differs");
  std::vector<int64_t> ps;for(const auto& component:json.at("nonRenderingComponents"))if(component.at("typeId")==198||component.at("typeId")==199)ps.push_back(component.at("fileId").get<int64_t>());
  // Explicit temporary adaptation: preserve exactly these four original PS
  // pairs without spawning them while strict Sprite-canvas export is pending.
  // Their complete serialized source metadata and node hierarchy stay intact.
  const std::vector<int64_t> audited={198025901758691103,198161607090078389,198313026266067498,198977153420339851,199002669891404536,199336590748365959,199621510681728303,199985305603660343};std::sort(ps.begin(),ps.end());
  if(ps!=audited)throw std::runtime_error("Boss source Prize particle component identities differ");models[i]=std::make_unique<FusionAnimation>();if(!models[i++]->Load(app,path.string(),error,ps))return false;}
 impl->trophy=std::move(models[0]);impl->fake=std::move(models[1]);impl->app=app;error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}
}
bool NativeBossRoomVisuals366::Draw(const Coin* c,Sexy::Graphics* g)const
{
 if(!IsNativeBossTrophy366(c)||!g)return false;const auto* r=GetNativeBossRoom366(c->mBoard);const auto* model=r->trophyFake?impl->fake.get():impl->trophy.get();if(!model)return true;
 FusionAnimationDrawOptions options;options.useBoardBasis=true;options.boardBasis={float(kFusionBasePixelsX),0,0,-float(GetNativeFusionPixelsY(c->mBoard))};
 options.nodeActivityOverrides.push_back({"Particle System",r->trophyClaimed});
 model->DrawPose(g,c->mPosX+c->mWidth*.5f,c->mPosY+c->mHeight*.5f,c->mScale,"flash",float(r->trophyAnimationBaseAge+uint32_t(c->mCoinAge))*.01f,nullptr,-1,1,&options);return true;
}
bool DrawNativeBossTrophy366(const Coin* c,Sexy::Graphics* g)
{const auto* context=c?GetNativeContext(c->mBoard):nullptr;return context&&context->bossRoomVisuals366&&context->bossRoomVisuals366->Draw(c,g);}
}
