#include "FusionNativeEquipment268.h"
#include "FusionNativeEquipment.h"
#include "FusionNative.h"
#include "FusionNativeDefender.h"
#include "FusionNativeProducer.h"
#include "FusionNativeFireActionBatch.h"
#include "FusionNativeGarlic.h"
#include "FusionNativeWater268.h"
#include "FusionNativeHypno269.h"
#include "FusionNativeSummonPlants269.h"
#include "FusionZombieRules.h"
#include "FusionBasePlantRules.h"
#include "../Board.h"
#include "../Coin.h"
#include "../GridItem.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../../SexyAppFramework/Image.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
namespace LawnFusion
{
namespace
{
bool Current(const Plant* p)
{return p&&p->mBoard&&p->mApp&&p->mApp->mBoard==p->mBoard&&IsNativeFusionBoard(p->mBoard)&&GetNativePlantData(p);}
bool Live(const Plant* p){return Current(p)&&!p->mDead&&p->mIsOnBoard&&p->mPlantHealth>0;}
bool Host(const Plant* p)
{return Current(p)&&!p->mNetGhost&&!p->mApp->NetIsClient()&&!p->mBoard->mPaused&&p->mApp->mGameScene==SCENE_PLAYING;}
int Ticks(float seconds){return static_cast<int>(std::ceil(double(seconds)*100));}
bool Target(Zombie* z)
{return z&&!z->mDead&&!z->IsDeadOrDying()&&z->mBodyHealth>0&&!z->mMindControlled&&z->mHasHead;}
bool PumpkinTarget(Zombie* z)
{return z&&!z->mDead&&!z->mNetGhost&&!z->IsDeadOrDying()&&z->mBodyHealth>0&&!z->mMindControlled&&
    !z->IsFlying()&&z->EffectedByDamage(1U)&&!NativeWater268ZombieCaptured(z);}
int First(const Zombie* z){if(const auto* d=GetNativeSourceZombieData(z)){if(d->kind==111)return 6;if(d->kind==109)return 7;}return z->mHelmType==HELMTYPE_BUCKET?2:z->mHelmType==HELMTYPE_FOOTBALL?4:0;}
int Second(const Zombie* z){return z->mShieldType==SHIELDTYPE_DOOR?1:z->mShieldType==SHIELDTYPE_LADDER?3:0;}
int Unique(Zombie* z)
{
    if(z->mZombiePhase==PHASE_JACK_IN_THE_BOX_RUNNING)return 1;
    if(z->mZombieType==ZOMBIE_POGO&&z->IsBouncingPogo())return 2;
    if(z->mZombieType==ZOMBIE_DIGGER&&z->mHasObject)return 3;
    return 0;
}
bool Circle(float x,float y,const Sexy::Rect& rect,double radius,double pixelsY)
{
    const double dx=((std::max)(double(rect.mX),(std::min)(double(x),double(rect.mX+rect.mWidth)))-x)/kFusionBasePixelsX;
    const double dy=((std::max)(double(rect.mY),(std::min)(double(y),double(rect.mY+rect.mHeight)))-y)/pixelsY;
    return dx*dx+dy*dy<=radius*radius;
}
bool CaptureProfile(Plant* p,FusionMagnetCaptureProfile& out)
{
    auto* state=GetNativeEquipmentPlantState(p);const auto* d=GetNativePlantData(p);
    if(!state||!d||!TryGetFusionMagnetCaptureProfile(d->kind,GetNativeFireLightLevel(p),state->seaGrown,state->retainedRowRange,out))return false;
    state->retainedRowRange=static_cast<uint8_t>(out.rowRange);return true;
}
bool NativeArmorPoint(Plant* p,Zombie* z,FusionEquipmentType type,float& x,float& y)
{
    const int helm=z->GetHelmDamageIndex(),shield=z->GetShieldDamageIndex();Sexy::Image* image=nullptr;
    if(type==FusionEquipmentType::Bucket)
    {image=helm==0?IMAGE_REANIM_ZOMBIE_BUCKET1:helm==1?IMAGE_REANIM_ZOMBIE_BUCKET2:IMAGE_REANIM_ZOMBIE_BUCKET3;z->GetTrackPosition("anim_bucket",x,y);x-=IMAGE_REANIM_ZOMBIE_BUCKET1->GetWidth()/2;y-=IMAGE_REANIM_ZOMBIE_BUCKET1->GetHeight()/2;}
    else if(type==FusionEquipmentType::Helmet)
    {image=helm==0?IMAGE_REANIM_ZOMBIE_FOOTBALL_HELMET:helm==1?IMAGE_REANIM_ZOMBIE_FOOTBALL_HELMET2:IMAGE_REANIM_ZOMBIE_FOOTBALL_HELMET3;z->GetTrackPosition("zombie_football_helmet",x,y);x+=37;y-=60;}
    else if(type==FusionEquipmentType::Door)
    {image=shield==0?IMAGE_REANIM_ZOMBIE_SCREENDOOR1:shield==1?IMAGE_REANIM_ZOMBIE_SCREENDOOR2:IMAGE_REANIM_ZOMBIE_SCREENDOOR3;z->GetTrackPosition("anim_screendoor",x,y);x-=IMAGE_REANIM_ZOMBIE_SCREENDOOR1->GetWidth()/2;y-=IMAGE_REANIM_ZOMBIE_SCREENDOOR1->GetHeight()/2;}
    else if(type==FusionEquipmentType::Ladder)
    {image=shield==0?IMAGE_REANIM_ZOMBIE_LADDER_1:shield==1?IMAGE_REANIM_ZOMBIE_LADDER_1_DAMAGE1:IMAGE_REANIM_ZOMBIE_LADDER_1_DAMAGE2;x=z->mPosX+31-IMAGE_REANIM_ZOMBIE_LADDER_5->GetWidth()/2;y=z->mPosY+20;}
    else if(type==FusionEquipmentType::Jackbox)
    {image=IMAGE_REANIM_ZOMBIE_JACKBOX_BOX;z->GetTrackPosition("Zombie_jackbox_box",x,y);x-=image->GetWidth()/2;y-=image->GetHeight()/2;}
    else if(type==FusionEquipmentType::Jumper)
    {image=IMAGE_ZOMBIEPOGO;z->GetTrackPosition("Zombie_pogo_stick",x,y);x+=40-IMAGE_REANIM_ZOMBIE_LADDER_5->GetWidth()/2;y+=84;}
    else if(type==FusionEquipmentType::Pickaxe)
    {image=IMAGE_REANIM_ZOMBIE_DIGGER_PICKAXE;z->GetTrackPosition("Zombie_digger_pickaxe",x,y);x-=image->GetWidth()/2;}
    if(!image)return false;
    // Plant.DrawMagnetItems uses scale.8. TodDrawImageCelScaledF centres its
    // transform at topLeft + celSize*.4; height offset is supplied by the plant.
    x+=image->GetCelWidth()*.4f;y+=image->GetCelHeight()*.4f+PlantDrawHeightOffset(p->mBoard,p,p->mSeedType,p->mPlantCol,p->mRow);
    return std::isfinite(x)&&std::isfinite(y);
}
void Captured(Plant* p,Coin* coin,const FusionMagnetCaptureProfile& profile,int row,float x,float y,FusionEquipmentType capturedType)
{
    p->mTargetX=Ticks(profile.cooldownSeconds);auto* visual=GetNativePlantData(p);
    if(visual->kind!=1102){visual->batchMode=kEquipmentAttract;visual->shootSeconds=0;}
    auto* state=GetNativeEquipmentPlantState(p);const auto* item=GetNativeEquipmentData(coin);
    state->lastCapturedType=capturedType;state->targetItemId=coin->mNetId;state->targetItemToken=item->instanceToken;
    if(profile.lightBurst)
    {
        std::vector<ZombieID> victims;Zombie* z=nullptr;
        while(p->mBoard->IterateZombies(z))if(Target(z)&&z->mRow==row&&Circle(x,y,z->GetZombieRect(),1,GetNativeFusionPixelsY(p->mBoard)))victims.push_back(p->mBoard->ZombieGetID(z));
        for(auto id:victims)if((z=p->mBoard->ZombieTryToGet(id))&&Target(z))z->TakeDamage(200,1U);
    }
    LawnNet::NetTrace("Fusion equipment capture plant=%u kind=%u coin=%u captureType=%u payloadType=%u cooldown=%d row=%d tick=%d",p->mNetId,GetNativePlantData(p)->kind,coin->mNetId,unsigned(capturedType),unsigned(item->type),p->mTargetX,row,p->mBoard->mMainCounter);
}
bool CaptureZombie(Plant* p,Zombie* z,const FusionMagnetCaptureProfile& profile)
{
    if(!Target(z))return false;FusionEquipmentCapturePlan plan;
    if(!TryGetFusionEquipmentArmorCapture(0,First(z),plan)&&!TryGetFusionEquipmentArmorCapture(1,Second(z),plan)&&!TryGetFusionEquipmentArmorCapture(2,Unique(z),plan))return false;
    if(plan.preserveReturnedBucket||!CanCreateNativeEquipment(p->mBoard,plan.type))return false;
    float x=0,y=0,endX=0,endY=0;
    if(!GetNativeEquipmentPlantState(p)||!NativeArmorPoint(p,z,plan.type,x,y)||!TryGetNativeEquipmentHoldAnchor(p,endX,endY))return false;
    auto* coin=CreateNativeEquipmentForCapture(p,plan.type,x,y,endX,endY);if(!coin)return false;
    // This known native leaf cannot allocate/fail. All authoritative carrier
    // and visual admission was completed before touching the actual armor.
    p->MagnetShroomAttactItem(z);p->mMagnetItems[0].mItemType=MAGNET_ITEM_NONE;
    p->mState=STATE_READY;p->mStateCountdown=0;
    Captured(p,coin,profile,z->mRow,x,y,plan.type);return true;
}
bool CaptureLadder(Plant* p,GridItem* item,const FusionMagnetCaptureProfile& profile,float x,float y)
{
    float endX=0,endY=0;if(!TryGetNativeEquipmentHoldAnchor(p,endX,endY))return false;
    auto* coin=CreateNativeEquipmentForCapture(p,FusionEquipmentType::Ladder,x,y,endX,endY);if(!coin)return false;
    const int row=item->mGridY;item->GridItemDie();Captured(p,coin,profile,row,x,y,FusionEquipmentType::Ladder);return true;
}
void Search(Plant* p,const FusionMagnetCaptureProfile& profile)
{
    if(GetNativeEquipmentHeldBy(p))return;float x=0,y=0;if(!p->GetShadowCenter(x,y))return;
    const double py=GetNativeFusionPixelsY(p->mBoard);Zombie* z=nullptr;
    // Source Physics2D order has no native equivalent; real DataArray order is
    // the deterministic native adapter, without a parallel target collection.
    while(p->mBoard->IterateZombies(z))if(Target(z)&&(profile.wholeWorld||
        (std::abs(z->mRow-p->mRow)<=profile.rowRange&&Circle(x,y-float(.5*py),z->GetZombieRect(),profile.radius,py))))
        if(CaptureZombie(p,z,profile))return;
    if(profile.wholeWorld)return; // MagnetBlover override scans zombieList only.
    GridItem* item=nullptr;
    while(p->mBoard->IterateGridItems(item))if(!item->mDead&&item->mGridItemType==GRIDITEM_LADDER&&std::abs(item->mGridY-p->mRow)<=profile.rowRange)
    {
        const float lx=float(p->mBoard->GetCellCenterX(item->mGridX,item->mGridY)),ly=float(p->mBoard->GridToPixelY(item->mGridX,item->mGridY)+50);
        if(Circle(x,y-float(.5*py),Sexy::Rect(int(lx),int(ly),0,0),profile.radius,py)&&CaptureLadder(p,item,profile,lx,ly))return;
    }
}
std::vector<Plant*> Component(Plant* p)
{
    std::vector<Plant*> all;std::vector<FusionMagnetGraphNode> nodes;Plant* other=nullptr;std::size_t own=0;
    while(p->mBoard->IteratePlants(other))if(Live(other)&&FusionEquipmentMagnetMember(GetNativePlantData(other)->kind))
    {if(other==p)own=all.size();all.push_back(other);nodes.push_back({GetNativePlantData(other)->kind,other->mPlantCol,other->mRow,true});}
    if(all.empty()||own>=all.size()||all[own]!=p)return {};
    std::vector<bool> reached(all.size(),false);reached[own]=true;bool changed=true;
    while(changed){changed=false;for(std::size_t i=0;i<all.size();++i)if(!reached[i])for(std::size_t j=0;j<all.size();++j)
        if(reached[j]&&FusionEquipmentGraphAdjacent(nodes[i].column,nodes[i].row,nodes[j].column,nodes[j].row)){reached[i]=true;changed=true;break;}}
    std::vector<Plant*> result;for(std::size_t i=0;i<all.size();++i)if(reached[i])result.push_back(all[i]);return result;
}
bool Sink(Plant* target,FusionEquipmentType type,Coin* item)
{
    const bool accepted=TryNativeEquipmentGraphSink(target,type,item);
    if(!accepted)LawnNet::NetTrace("Fusion equipment graph dependency kind=%u type=%u tick=%d",GetNativePlantData(target)->kind,unsigned(type),target->mBoard->mMainCounter);
    return accepted;
}
void Rearm(Plant* p)
{
    auto* state=GetNativeEquipmentPlantState(p);if(!state)return;const auto type=state->lastCapturedType;
    auto* visual=GetNativePlantData(p);
    if(visual->kind!=1102){visual->batchMode=kEquipmentIdle;visual->shootSeconds=-1;visual->idleSeconds=0;}
    auto* coin=ResolveNativeEquipment(p->mBoard,state->targetItemId,state->targetItemToken);
    state->targetItemId=0;state->targetItemToken=0;
    if(coin&&!ConsumeNativeEquipment(coin))return;
    auto group=Component(p);Plant* box=nullptr;Plant* machine=nullptr;std::vector<Plant*> hypno,sameRow;
    for(auto* q:group)
    {const auto k=GetNativePlantData(q)->kind;if(k==246&&!box)box=q;if(k==924&&!machine)machine=q;
     if(k==1157){hypno.push_back(q);if(q->mRow==p->mRow)sameRow.push_back(q);}}
    if(box)Sink(box,type,nullptr);else if(machine)Sink(machine,type,nullptr);
    else {auto& eligible=sameRow.empty()?hypno:sameRow;if(!eligible.empty())Sink(eligible[RandRangeInt(0,int(eligible.size())-1)],type,nullptr);}
    if(GetNativePlantData(p)->kind==1101)
    {
        if(state->seaCallbacks<static_cast<uint32_t>((std::numeric_limits<int>::max)()))++state->seaCallbacks;
        if(state&&state->seaCallbacks>=3&&!state->seaGrown){state->seaGrown=true;visual->batchMode=kEquipmentGrow;visual->shootSeconds=0;}
    }
    LawnNet::NetTrace("Fusion equipment rearm plant=%u type=%u tick=%d",p->mNetId,unsigned(type),p->mBoard->mMainCounter);
}
float SourceSpeed(Zombie* z)
{
    if(const auto* source=GetNativeSourceZombieData(z))
    {
        float factor=1;const auto* garlic=GetNativeZombieGarlicData(z);
        if(TryGetFusionZombieStatusSpeedFactor(source->kind,z->mChilledCounter>0,z->mIceTrapCounter>0,z->mButteredCounter>0,garlic&&FusionGarlicStopsWalking(*garlic),factor))return source->originSpeed*factor;
        return 0;
    }
    // Classic controls have no Unity originSpeed; their native status factor is
    // explicit. Initial fixture velocity0 is not an animation-speed override.
    return z->IsImmobilizied()?0.f:z->IsMovingAtChilledSpeed()?.5f:1.f;
}
void Pumpkin(Plant* p)
{
    RebuildNativeEquipmentGraph(p->mBoard);const auto* own=GetNativeEquipmentPlantState(p);float x=0,y=0;
    if(!own||!p->GetShadowCenter(x,y))return;const double py=GetNativeFusionPixelsY(p->mBoard);
    const Sexy::Rect area(int(std::floor(x-1.5*kFusionBasePixelsX)),int(std::floor(y-1.7*py)),int(std::ceil(3*kFusionBasePixelsX)),int(std::ceil(3*py)));
    std::vector<ZombieID> targets;Zombie* z=nullptr;
    while(p->mBoard->IterateZombies(z))if(PumpkinTarget(z)&&std::abs(z->mRow-p->mRow)<=1&&area.Intersects(z->GetZombieRect()))
    {const auto* source=GetNativeSourceZombieData(z);if(FusionMagnetPumpkinMetalTarget(First(z),Second(z),Unique(z),source?source->kind:-1))targets.push_back(p->mBoard->ZombieGetID(z));}
    const auto* definition=FusionFindPlant(GetNativePlantData(p)->kind);if(!definition)return;
    for(auto id:targets)if((z=p->mBoard->ZombieTryToGet(id))&&PumpkinTarget(z))
    {
        const int col=p->mBoard->PixelToGridX(z->mPosX+60,z->mPosY);const bool water=p->mBoard->IsValidGridCell(col,z->mRow)&&p->mBoard->GetGridSquareTypeAt(col,z->mRow)==GRIDSQUARE_POOL;
        int damage=0;if(!TryGetFusionMagnetPumpkinDamage(definition->damage,own->componentSize,SourceSpeed(z),water,damage))continue;
        z->TakeDamage(damage,1U);LawnNet::NetTrace("Fusion equipment Pumpkin plant=%u target=%u damage=%d graph=%u water=%d tick=%d",p->mNetId,z->mNetId,damage,own->componentSize,water?1:0,p->mBoard->mMainCounter);
    }
}
bool ProduceItem(Plant* p)
{
    auto* state=GetNativeEquipmentPlantState(p);if(!state)return false;
    if(state->pendingItemDraw<0)state->pendingItemDraw=static_cast<int8_t>(RandRangeInt(0,9));
    int itemKind=0;FusionEquipmentType type=FusionEquipmentType::None;
    if(!TryGetFusionSunMagnetWeightedItem(state->pendingItemDraw,itemKind,type))return false;
    auto* b=p->mBoard;Plant* right=nullptr;
    while(b->IteratePlants(right))if(Live(right)&&right->mRow==p->mRow&&right->mPlantCol==p->mPlantCol+1)
    {
        const auto k=GetNativePlantData(right)->kind;
        if(k==1157){Sink(right,type,nullptr);return true;}
        if(k==924){Sink(right,FusionEquipmentType::Bucket,nullptr);return true;}
        if(k==1166)
        {float x=0,y=0;if(!right->GetShadowCenter(x,y))return false;auto* coin=CreateNativeEquipment(b,type,x,y);if(!coin)return false;Sink(right,FusionEquipmentType::Bucket,coin);return true;}
    }
    for(auto* q:Component(p))if(GetNativePlantData(q)->kind==246){Sink(q,type,nullptr);return true;}
    float x=0,y=0;if(!p->GetShadowCenter(x,y))return false;auto* coin=CreateNativeEquipment(b,type,x,y);
    if(coin)LawnNet::NetTrace("Fusion equipment SunMagnet item plant=%u coin=%u type=%u item=%d tick=%d",p->mNetId,coin->mNetId,unsigned(type),itemKind,b->mMainCounter);
    return coin!=nullptr;
}
}
bool IsNativeEquipmentPlant(const Plant* p){const auto* d=GetNativePlantData(p);return d&&FindFusionEquipmentProfile(d->kind);}
int GetNativeEquipmentGraphCount(const Plant* target)
{
    if(!Live(target)||!FusionEquipmentMagnetMember(GetNativePlantData(target)->kind))return 0;
    std::vector<Plant*> plants;std::vector<FusionMagnetGraphNode> nodes;Plant* plant=nullptr;
    while(target->mBoard->IteratePlants(plant))if(Live(plant)&&FusionEquipmentMagnetMember(GetNativePlantData(plant)->kind))
    {plants.push_back(plant);nodes.push_back({GetNativePlantData(plant)->kind,plant->mPlantCol,plant->mRow,true});}
    std::vector<uint32_t> sizes;if(!TryGetFusionMagnetComponentSizes(nodes.data(),nodes.size(),sizes))return 0;
    const auto found=std::find(plants.begin(),plants.end(),target);
    return found==plants.end()?0:int(sizes[std::distance(plants.begin(),found)]);
}
void RebuildNativeEquipmentGraph(Board* b)
{
    if(!b||!b->mApp||b->mApp->mBoard!=b||b->mApp->NetIsClient()||!IsNativeFusionBoard(b))return;
    std::vector<Plant*> plants;std::vector<FusionMagnetGraphNode> nodes;Plant* p=nullptr;
    while(b->IteratePlants(p))if(Live(p)&&FusionEquipmentMagnetMember(GetNativePlantData(p)->kind))
    {plants.push_back(p);nodes.push_back({GetNativePlantData(p)->kind,p->mPlantCol,p->mRow,true});}
    std::vector<uint32_t> sizes;if(!TryGetFusionMagnetComponentSizes(nodes.data(),nodes.size(),sizes))return;
    for(std::size_t i=0;i<plants.size();++i)if(auto* state=GetNativeEquipmentPlantState(plants[i]))state->componentSize=static_cast<uint16_t>(sizes[i]);
}
void InitializeNativeEquipmentPlant(Plant* p)
{
    if(!Host(p)||!IsNativeEquipmentPlant(p))return;auto* d=GetNativePlantData(p);const auto* profile=FindFusionEquipmentProfile(d->kind);
    auto* state=GetNativeEquipmentPlantState(p);if(!state)return;*state={};
    p->mPlantHealth=p->mPlantMaxHealth=profile->maximumHealth;p->SetSleeping(false);p->mWakeUpCounter=0;
    d->animationSpeed=RandRangeFloat(.9f,1.1f);d->idleSeconds=0;d->shootSeconds=-1;d->batchMode=0;d->batchBagCount=d->batchRageTicks=0;d->batchShotCount=0;
    p->mTargetX=Ticks(profile->initialAttributeSeconds);p->mTargetY=0;p->mShootingCounter=p->mStateCountdown=p->mDoSpecialCountdown=0;
    d->productionTicks=d->pendingProductionTicks=0;p->mLaunchCounter=0;p->mState=STATE_READY;
    if(profile->family==FusionEquipmentFamily::Defender||profile->family==FusionEquipmentFamily::Pumpkin)InitializeNativeDefender(p);
    if(d->kind==1154)InitializeNativeProducer(p);
    if(d->kind==1102){p->mDoSpecialCountdown=Ticks(.38333333f/d->animationSpeed);d->batchMode=kEquipmentBlow;d->shootSeconds=0;}
    RebuildNativeEquipmentGraph(p->mBoard);
}
bool UpdateNativeEquipmentPlant(Plant* p)
{
    if(!IsNativeEquipmentPlant(p))return false;if(!Host(p)||!Live(p)||p->mSquished)return true;
    auto* d=GetNativePlantData(p);const auto* profile=FindFusionEquipmentProfile(d->kind);
    if(d->kind==1102&&p->mTargetY>0&&!--p->mTargetY){p->Die();return true;}
    if(d->kind==1154)
    {
        UpdateNativeProducer(p);
        if(p->mTargetY>0&&!--p->mTargetY)
        {if(ProduceItem(p))GetNativeEquipmentPlantState(p)->pendingItemDraw=-1;else {p->mTargetY=1;return true;}}
    }
    if(p->mTargetX>0&&!--p->mTargetX)
    {
        if(profile->family==FusionEquipmentFamily::Capture)Rearm(p);
        else if(profile->family==FusionEquipmentFamily::Pumpkin){p->mTargetX=Ticks(6);Pumpkin(p);}
        else if(d->kind==1154){p->mTargetX=Ticks(30);p->mTargetY=50;p->mEatenFlashCountdown=(std::max)(p->mEatenFlashCountdown,50);}
    }
    if(profile->family==FusionEquipmentFamily::Capture&&!p->mTargetX)
    {FusionMagnetCaptureProfile capture;if(CaptureProfile(p,capture))Search(p,capture);}
    return true;
}
void UpdateNativeEquipmentPlantPhase(Plant* p)
{
    if(!IsNativeEquipmentPlant(p)||!Host(p)||!Live(p))return;auto* d=GetNativePlantData(p);d->idleSeconds+=.01f;
    if(d->shootSeconds>=0)d->shootSeconds+=.01f;
    if(d->batchMode==kEquipmentBlow&&d->shootSeconds>=1.2666667f/d->animationSpeed)
    {d->batchMode=kEquipmentBlowLoop;d->shootSeconds=0;}
    else if(d->batchMode==kEquipmentAttract||d->batchMode==kEquipmentGrow)
    {
        const float clip=d->batchMode==kEquipmentGrow?1.f:d->kind==1101?.6333333f:.68333334f;
        if(d->shootSeconds>=clip/d->animationSpeed)
        {d->batchMode=d->batchMode==kEquipmentGrow?kEquipmentIdle:kEquipmentHolding;d->shootSeconds=-1;d->idleSeconds=0;}
    }
}
bool ApplyNativeEquipmentPlantSpecial(Plant* p)
{
    if(!IsNativeEquipmentPlant(p))return false;if(GetNativePlantData(p)->kind!=1102)return true;
    if(!Host(p)||!Live(p)||p->mTargetY)return true;
    Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))if(Target(z)&&z->IsFlying())z->mBlowingAway=true;
    p->mBoard->mFogBlownCountDown=4000;p->mTargetY=1000;p->mApp->PlaySample(Sexy::SOUND_BLOVER);return true;
}
void NativeEquipmentPlantRemoving(Plant* p,int)
{
    if(!Current(p)||p->mNetGhost||p->mApp->NetIsClient()||!IsNativeEquipmentPlant(p))return;
    if(auto* coin=GetNativeEquipmentHeldBy(p)){if(GetNativePlantData(p)->kind==1102)ConsumeNativeEquipment(coin);else DropNativeEquipmentFromPlant(coin);}
    RebuildNativeEquipmentGraph(p->mBoard);
}
void NativeEquipmentCoinRemoving268(Coin* coin)
{
    const auto* data=GetNativeEquipmentData(coin);if(!data||!coin->mBoard||coin->mApp->NetIsClient())return;
    Plant* p=nullptr;while(coin->mBoard->IteratePlants(p))if(auto* state=GetNativeEquipmentPlantState(p))
        if(state->targetItemId==coin->mNetId&&state->targetItemToken==data->instanceToken){state->targetItemId=0;state->targetItemToken=0;}
}
bool CanUseNativeEquipmentOnPlant(const Plant* p,FusionEquipmentType type)
{FusionBucketReceiverEffect effect;const auto* d=GetNativePlantData(p);return Live(p)&&d&&
    (d->kind==1157||d->kind==1166?CanUseNativeSummonPlants269Item(p,type):d->kind==924?(FusionEquipmentTypeValid(int(type))&&CanUseNativeHypno269Item(p)):
     TryGetFusionBucketReceiverEffect(d->kind,int(type),p->mPlantHealth,p->mPlantMaxHealth,effect));}
bool TryUseNativeEquipmentOnPlant(Plant* p,FusionEquipmentType type,Coin* item)
{
    if(!Host(p)||!CanUseNativeEquipmentOnPlant(p,type))return false;
    if(item){const auto* data=GetNativeEquipmentData(item);if(!data||data->type!=type||item->mDead||item->mBoard!=p->mBoard)return false;}
    if(GetNativePlantData(p)->kind==924)return TryUseNativeHypno269Item(p,item);
    if(GetNativePlantData(p)->kind==1157||GetNativePlantData(p)->kind==1166)return TryUseNativeSummonPlants269Item(p,type,item);
    FusionBucketReceiverEffect effect;TryGetFusionBucketReceiverEffect(GetNativePlantData(p)->kind,int(type),p->mPlantHealth,p->mPlantMaxHealth,effect);
    const uint16_t old=p->mNetId;auto* b=p->mBoard;
    if(effect.replacementKind!=65535){if(!TryReplaceNativeEquipmentReceiver(p,effect.replacementKind))return false;}
    else if(effect.sunValue){if(!EmitNativeSourceSun(p,0))return false;}
    else {p->mPlantHealth=int((std::min)(int64_t(p->mPlantMaxHealth),int64_t(p->mPlantHealth)+effect.recoverHealth));p->mEatenFlashCountdown=25;NativePlantHealthChanged(p);}
    if(item&&!ConsumeNativeEquipment(item))return false;
    LawnNet::NetTrace("Fusion equipment receiver plant=%u type=%u replacement=%u recover=%d sun=%d tick=%d",old,unsigned(type),effect.replacementKind,effect.recoverHealth,effect.sunValue,b->mMainCounter);return true;
}
bool GetNativeEquipmentPlantVisualPose(const Plant* p,const char*& state,float& elapsed)
{
    if(!IsNativeEquipmentPlant(p))return false;const auto* d=GetNativePlantData(p);const auto* own=GetNativeEquipmentPlantState(p);
    state="idle";elapsed=d->idleSeconds;
    if(d->batchMode==kEquipmentBlow){state="blow";elapsed=d->shootSeconds;}
    else if(d->batchMode==kEquipmentBlowLoop){state="blowloop";elapsed=d->shootSeconds;}
    else if(d->batchMode==kEquipmentGrow){state="grow";elapsed=d->shootSeconds;}
    else if(d->batchMode==kEquipmentAttract){state="attrack";elapsed=d->shootSeconds;}
    else if(d->batchMode==kEquipmentHolding||(d->kind==1101&&own&&own->seaGrown))state="idle2";
    return std::isfinite(elapsed)&&elapsed>=0;
}
}
