#include "FusionNativeRollingActors269.h"
#include "FusionNative.h"
#include "FusionNativeWater268.h"
#include "FusionBasePlantRules.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include <algorithm>
#include <cmath>
#include <utility>
namespace LawnFusion
{
namespace
{
bool Host(const GameObject* p)
{return p&&p->mApp&&p->mBoard&&p->mApp->mBoard==p->mBoard&&!p->mNetGhost&&!p->mApp->NetIsClient()&&IsNativeFusionBoard(p->mBoard);}
bool Playing(const GameObject* p)
{return Host(p)&&!p->mBoard->mPaused&&p->mApp->mGameScene==SCENE_PLAYING;}
bool Live(Zombie* z)
{return z&&!z->mDead&&!z->IsDeadOrDying()&&GetNativeFusionZombieHealth(z)>0&&!NativeWater268ZombieCaptured(z);}
void Contact(Plant* p,NativeRollingActor269State& s)
{
    const auto* profile=FindFusionRollingActor269Profile(s.kind);
    const double py=GetNativeFusionPixelsY(p->mBoard);
    const float x=float((p->mX+s.rootOffsetX-p->mBoard->GetCellCenterX(0,0))/kFusionBasePixelsX-4.8);
    const float y=float(2.3-(p->mY+s.rootOffsetY-p->mBoard->GridToPixelY(0,0)-50)/py);
    std::vector<uint32_t> current;Zombie* z=nullptr;
    while(p->mBoard->IterateZombies(z))
    {
        if(!Live(z))continue;const auto rect=z->GetZombieRect();
        const float left=float((rect.mX-p->mBoard->GetCellCenterX(0,0))/kFusionBasePixelsX-4.8);
        const float bottom=float(2.3-(rect.mY+rect.mHeight-p->mBoard->GridToPixelY(0,0)-50)/py);
        if(!FusionRollingActor269Touches(x,y,left,bottom,float(rect.mWidth/kFusionBasePixelsX),float(rect.mHeight/py)))continue;
        const uint32_t generation=p->mBoard->mZombies.DataArrayGetID(z);current.push_back(generation);
        const bool enter=std::find(s.touching.begin(),s.touching.end(),generation)==s.touching.end();
        const int status=GetNativeFusionZombieStatus(z);
        if(enter)
        {
            if(FusionRollingActor269Enter(p->mRow,z->mRow,status,z->mMindControlled,GetNativeFusionZombieKind(z)))
            {s.enterCount=FusionRollingActor269DamageSum(s.enterCount,1);NativeRollingActor269Entered(p);}
            continue; // Unity's Enter frame precedes subsequent Stay callbacks.
        }
        if(!FusionRollingActor269Contact(p->mRow,z->mRow,status,z->mMindControlled))continue;
        // Source DmgType.Shieldless4 maps to the canonical native shield bypass.
        z->TakeDamage(profile->damage,1U);
        auto found=std::find_if(s.damageHistory.begin(),s.damageHistory.end(),[&](const auto& h){return h.zombieGeneration==generation;});
        if(found==s.damageHistory.end())s.damageHistory.push_back({generation,profile->damage});
        else found->damage=FusionRollingActor269DamageSum(found->damage,profile->damage);
    }
    s.touching=std::move(current);
}
}
Plant* SpawnNativeRollingActor269(Board* b,uint16_t kind,int column,int row)
{
    if(!b||!b->mApp||b->mApp->mBoard!=b||b->mApp->NetIsClient()||!IsNativeFusionBoard(b)||
       !FindFusionRollingActor269Profile(kind)||column<0||column>=b->GetColumnCount()||row<0||row>=GetNativeFusionRowCount(b)||
       !CanCreateNativeRollingActor269(b,kind))return nullptr;
    auto* p=CreateNativeRollingActor269Plant(b,kind,column,row);
    if(!p||p->mDead||!p->mIsOnBoard)return nullptr;
    if(!IsNativeRollingActor269(p)&&!InitializeNativeRollingActor269(p,kind))return nullptr;
    return p;
}
bool InitializeNativeRollingActor269(Plant* p,uint16_t factory)
{
    if(!p||!FindFusionRollingActor269Profile(factory)||!GetNativePlantData(p))return false;
    if(!Host(p))return true;if(IsNativeRollingActor269(p))return true;
    auto* data=GetNativePlantData(p);if(!p->mNetId||!data->instanceToken||(data->kind!=factory&&data->kind!=255))return false;
    // Query the real native ground anchor before installing our shadow hook.
    float x=0,y=0;if(!p->GetShadowCenter(x,y))return false;
    NativeRollingActor269State state;state.factoryKind=factory;state.kind=FusionRollingActor269PhysicalKind(factory);
    state.instanceToken=data->instanceToken;state.bornTick=p->mBoard->mMainCounter;
    state.originX=float(p->mX);state.originY=float(p->mY);state.rootOffsetX=x-p->mX;
    state.rootOffsetY=y-float(.88*GetNativeFusionPixelsY(p->mBoard))-p->mY;
    auto* s=EditNativeRollingActor269State(p,true);if(!s)return false;*s=state;
    data=GetNativePlantData(p);data->kind=255;data->damageStage=0;data->idleSeconds=0;data->shootSeconds=-1;
    data->animationSpeed=1; // BigWallNut.Update never copies randomized Plant.Speed into its Animator.
    data->productionTicks=data->pendingProductionTicks=0;data->batchShotCount=data->batchRageTicks=data->batchBagCount=data->batchMode=0;
    p->mPlantMaxHealth=p->mPlantHealth=4000;p->mState=STATE_READY;p->mIsAsleep=false;p->mSquished=false;
    p->mLaunchCounter=p->mDoSpecialCountdown=p->mStateCountdown=p->mShootingCounter=0;p->mRenderOrder=p->CalcRenderOrder();
    NativeRollingActor269Started(p);NativeRollingActor269Achievement(p->mBoard,6); // Actual physical kind255, also factory251.
    LawnNet::NetTrace("Fusion RollingActor269 birth factory=%u physical=%u id=%u token=%u row=%d col=%d born=%d",factory,state.kind,p->mNetId,state.instanceToken,p->mRow,p->mPlantCol,state.bornTick);return true;
}
bool IsNativeRollingActor269(const Plant* p){return GetNativeRollingActor269State(p)!=nullptr;}
bool UpdateNativeRollingActor269(Plant* p)
{
    auto* s=EditNativeRollingActor269State(p);if(!s)return false;if(!Playing(p)||p->mDead||!p->mIsOnBoard)return true;
    const int now=p->mBoard->mMainCounter;if(now<s->bornTick||s->lastTick==now)return true;
    s->lastTick=now;const int age=now-s->bornTick;
    // Round is an accumulating root-motion clip, not a periodically reset X.
    p->mX=int(std::lround(s->originX+float(4*kFusionBasePixelsX*.01)*age));p->mY=int(std::lround(s->originY));
    p->mRenderOrder=p->CalcRenderOrder();p->mAnimCounter=age;
    if(auto* data=GetNativePlantData(p)){data->idleSeconds=age*.01f;data->shootSeconds=-1;}
    if(now%2)return true; // Shared Board fixed-step phase, source physics50Hz.
    const float shadowX=float((p->mX+s->rootOffsetX-p->mBoard->GetCellCenterX(0,0))/kFusionBasePixelsX-4.8);
    const float maxX=float(-4.8+1.35*(p->mBoard->GetColumnCount()-1)+1.85);
    if(FusionRollingActor269Expired(shadowX,maxX))
    {
        if(s->enterCount>10)NativeRollingActor269Achievement(p->mBoard,10);
        if(std::any_of(s->damageHistory.begin(),s->damageHistory.end(),[](const auto& h){return h.damage>20000;}))NativeRollingActor269Achievement(p->mBoard,11);
        LawnNet::NetTrace("Fusion RollingActor269 retire factory=%u kind=%u id=%u age=%d enters=%d history=%zu hp=%d",s->factoryKind,s->kind,p->mNetId,age,s->enterCount,s->damageHistory.size(),p->mPlantHealth);
        // Source directly Destroy(gameObject), bypassing its no-op Die override.
        s->directDestroy=true;DestroyNativeRollingActor269Plant(p);return true;
    }
    Contact(p,*s);return true;
}
bool NativeRollingActor269SuppressSourceDie(const Plant* p,int reason)
{const auto* s=GetNativeRollingActor269State(p);return s&&!s->directDestroy&&reason!=255;} // Source Die/Crashed are empty, including shovel.
bool ApplyNativeRollingActor269Damage(Plant* p,int input,int sourceType)
{
    if(!IsNativeRollingActor269(p))return false;if(!Playing(p)||p->mDead||!p->mIsOnBoard)return true;
    int actual=0;if(!ResolveNativeRollingActor269IncomingDamage(p,input,sourceType,actual)||actual<=0)return true;
    const int before=p->mPlantHealth;
    p->mPlantHealth=FusionRollingActor269DamageSum(before,-actual);p->mRecentlyEatenCountdown=50;
    NativePlantHealthChanged(p);
    LawnNet::NetTrace("Fusion RollingActor269 damage id=%u type=%d input=%d actual=%d hp=%d->%d tick=%d",p->mNetId,sourceType,input,actual,before,p->mPlantHealth,p->mBoard->mMainCounter);
    return true; // Source TakeDamage subtracts; no automatic death/clamp at0.
}
bool GetNativeRollingActor269Rect(const Plant* p,Sexy::Rect& rect)
{
    const auto* s=GetNativeRollingActor269State(p);if(!s||!p->mBoard||p->mDead)return false;
    const auto* profile=FindFusionRollingActor269Profile(s->kind);const double x=p->mX+s->rootOffsetX,y=p->mY+s->rootOffsetY;
    const double hw=profile->colliderWidth*kFusionBasePixelsX*.5,hh=profile->colliderHeight*GetNativeFusionPixelsY(p->mBoard)*.5;
    const int left=int(std::floor(x-hw)),top=int(std::floor(y-hh));rect=Sexy::Rect(left,top,int(std::ceil(x+hw))-left,int(std::ceil(y+hh))-top);return true;
}
bool GetNativeRollingActor269VisualPose(const Plant* p,const char*& state,float& seconds)
{const auto* s=GetNativeRollingActor269State(p);if(!s||!p->mBoard||p->mBoard->mMainCounter<s->bornTick)return false;state="Round";seconds=float(p->mBoard->mMainCounter-s->bornTick)*.01f;return std::isfinite(seconds);}
bool GetNativeRollingActor269ShadowCenter(const Plant* p,float& x,float& y)
{const auto* s=GetNativeRollingActor269State(p);if(!s||!p->mBoard)return false;x=p->mX+s->rootOffsetX;y=p->mY+s->rootOffsetY+float(.88*GetNativeFusionPixelsY(p->mBoard));return true;}
uint16_t GetNativeRollingActor269ArtworkKind(const Plant* p)
{const auto* s=GetNativeRollingActor269State(p);return s?s->factoryKind:65535;}
bool InitializeNativeWheat269(Plant* p)
{
    const auto* d=GetNativePlantData(p);if(!d||d->kind!=253)return false;
    if(!Host(p))return true;if(!NativeWheat269ReplacementReady(p->mBoard))return false;
    auto* s=EditNativeWheat269State(p,true);if(!s)return false;
    *s={};s->isFromWheat=true;s->wheatTime=FusionWheat269InitialTime(NativeWheat269SpecialMode(p->mBoard));return true;
}
bool MarkNativeWheat269Replacement(Plant* p)
{
    if(!Host(p)||p->mDead||!GetNativePlantData(p))return false;
    auto* s=EditNativeWheat269State(p,true);if(!s)return false;
    *s={};s->isFromWheat=true;NativeWheat269ApplyMaterial(p);return true;
}
bool UpdateNativeWheat269(Plant* p)
{
    auto* s=EditNativeWheat269State(p);if(!s||!s->isFromWheat)return false;
    if(!Playing(p)||p->mDead||!p->mIsOnBoard||p->mPlantHealth<=0||s->lastTick==p->mBoard->mMainCounter)return false;
    s->lastTick=p->mBoard->mMainCounter;
    float after=s->wheatTime;bool transform=after>31.f;
    if(!transform)
    {
        if(!TryStepFusionWheat269(s->wheatTime,.01f,NativeWheat269CanAdvance(p),true,after,transform))return false;
        s->wheatTime=after;
    }
    if(!transform||!NativeWheat269ReplacementReady(p->mBoard))return false;
    // A failed resource/transient preflight retains the flag and its bounded
    // due time. The shared transaction marks each actual offspring only once.
    LawnNet::NetTrace("Fusion Wheat269 transform kind=%u id=%u token=%u time=%.9g",GetNativePlantData(p)->kind,p->mNetId,GetNativePlantData(p)->instanceToken,after);
    (void)NativeWheat269Replace(p);
    return p->mDead; // A real card fallback also consumes the original actor. Ships no fake success.
}
}
