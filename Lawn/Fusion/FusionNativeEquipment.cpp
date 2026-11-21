#include "FusionNativeEquipment.h"
#include "FusionNativeEquipment268.h"
#include "FusionNativeExtras268.h"
#include "FusionNative.h"
#include "FusionNativeHypno269.h"
#include "FusionNativeSummonPlants269.h"
#include "FusionNativeVisual.h"
#include "FusionBasePlantRules.h"
#include "../Board.h"
#include "../Coin.h"
#include "../Plant.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace LawnFusion
{
namespace
{
template<class T> uint32_t LiveId(DataArray<T>& array,const T* entity)
{
    if(!array.mBlock||!entity)return 0;
    const uintptr_t begin=reinterpret_cast<uintptr_t>(array.mBlock),pointer=reinterpret_cast<uintptr_t>(entity);
    const size_t stride=sizeof(typename DataArray<T>::DataArrayItem);
    if(pointer<begin||pointer-begin>=static_cast<size_t>(array.mMaxUsedCount)*stride||(pointer-begin)%stride)return 0;
    const auto& item=array.mBlock[(pointer-begin)/stride];
    return array.DataArrayTryToGet(item.mID)==entity?item.mID:0;
}
bool Current(const Board* b)
{return b&&b->mApp&&b->mApp->mBoard==b&&IsNativeFusionBoard(b);}
bool Host(const Board* b){return Current(b)&&!b->mApp->NetIsClient();}
bool Point(float x,float y)
{return std::isfinite(x)&&std::isfinite(y)&&std::abs(x)<=4096&&std::abs(y)<=4096;}
bool SourcePlant(const Plant* p)
{
    const auto* d=GetNativePlantData(p);const auto* profile=d?FindFusionEquipmentProfile(d->kind):nullptr;
    return p&&Host(p->mBoard)&&!p->mNetGhost&&!p->mDead&&p->mIsOnBoard&&p->mPlantHealth>0&&
        p->mNetId&&d&&d->instanceToken&&profile&&profile->family==FusionEquipmentFamily::Capture;
}
Plant* ResolveSource(Board* b,const NativeEquipmentData& d)
{
    Plant* p=nullptr;
    while(b->mPlants.IterateNext(p))if(p->mNetId==d.sourcePlantId)
        if(const auto* data=GetNativePlantData(p);data&&data->instanceToken==d.sourcePlantToken)return p;
    return nullptr;
}
bool ParentTransform(const Plant* p,std::array<float,6>& matrix)
{
    const auto* d=GetNativePlantData(p);
    if(!d||!GetNativeSourcePlantNodeTransform(p,d->kind==1102?"Blover_petal/Position":d->kind==1166?"Pos":"",matrix))return false;
    for(float value:matrix)if(!std::isfinite(value))return false;
    return std::abs(double(matrix[0])*matrix[3]-double(matrix[1])*matrix[2])>1e-12;
}
bool ParentLocal(const std::array<float,6>& m,float x,float y,float& localX,float& localY)
{
    const double determinant=double(m[0])*m[3]-double(m[1])*m[2];
    if(std::abs(determinant)<=1e-12)return false;
    const double dx=double(x)-m[4],dy=double(y)-m[5];
    const float lx=float((dx*m[3]-dy*m[2])/determinant),ly=float((dy*m[0]-dx*m[1])/determinant);
    if(!Point(lx,ly))return false;localX=lx;localY=ly;return true;
}
void Position(Coin* c,float x,float y)
{
    c->mPosX=x-20;c->mPosY=y-20;
    // Coin::Draw already consumes Board-space mPos; GameObject adds no origin.
    c->mX=0;c->mY=0;
}
struct Bounds
{
    float left,right,top,bottom,landing;
};
Bounds BoardBounds(Board* b)
{
    const double py=GetNativeFusionPixelsY(b),ox=b->GetCellCenterX(0,0),oy=b->GridToPixelY(0,0)+50;
    const double lastY=2.3-(b->GridToPixelY(0,GetNativeFusionRowCount(b)-1)-b->GridToPixelY(0,0))/py;
    const double minY=lastY-.6;
    return {float(ox+(-10+4.8)*kFusionBasePixelsX),
        float(b->GetCellCenterX(b->GetColumnCount()-1,0)+1.85*kFusionBasePixelsX),
        float(oy+(2.3-6.5)*py),float(oy+(2.3-minY)*py),float(oy+(2.3-(minY+2))*py)};
}
bool CanMutate(Coin* c)
{return c&&!c->mDead&&!c->mNetGhost&&Host(c->mBoard)&&GetNativeEquipmentData(c);}
void RecoverOnSourceDie(Board* b,FusionEquipmentType type)
{
    std::vector<uint32_t> supers,ordinary;Plant* p=nullptr;
    while(b->IteratePlants(p))if(!p->mDead&&p->mIsOnBoard&&p->mPlantHealth>0)
        if(const auto* d=GetNativePlantData(p))
        {if(d->kind==1151)supers.push_back(LiveId(b->mPlants,p));else if(d->kind==1129)ordinary.push_back(LiveId(b->mPlants,p));}
    uint16_t kind=0;int amount=0;
    if(!TryGetFusionBucketDeathRecovery(int(type),int(supers.size()),int(ordinary.size()),kind,amount))return;
    for(auto id:kind==1151?supers:ordinary)if((p=b->mPlants.DataArrayTryToGet(id))&&!p->mDead&&p->mPlantHealth>0)
    {
        if(kind==1129){RecoverNativeMachine268(p,amount);continue;}
        p->mPlantHealth=static_cast<int>((std::min)(int64_t(p->mPlantMaxHealth),int64_t(p->mPlantHealth)+amount));
        p->mEatenFlashCountdown=25;NativePlantHealthChanged(p);
    }
}
}

NativeEquipmentData* GetNativeEquipmentData(Coin* c)
{
    auto* context=GetNativeContext(c?c->mBoard:nullptr);if(!context)return nullptr;
    const uint32_t generation=LiveId(c->mBoard->mCoins,c);const auto found=context->equipmentCoins.find(generation);
    return generation&&found!=context->equipmentCoins.end()?&found->second:nullptr;
}
const NativeEquipmentData* GetNativeEquipmentData(const Coin* c)
{return GetNativeEquipmentData(const_cast<Coin*>(c));}
NativeEquipmentPlantState* GetNativeEquipmentPlantState(Plant* p)
{
    auto* context=GetNativeContext(p?p->mBoard:nullptr);const auto* data=GetNativePlantData(p);
    if(!context||!data||!FindFusionEquipmentProfile(data->kind))return nullptr;
    const uint32_t generation=LiveId(p->mBoard->mPlants,p);if(!generation)return nullptr;
    const auto found=context->equipmentPlants.find(generation);
    if(found!=context->equipmentPlants.end())return &found->second;
    if(!Host(p->mBoard)||p->mNetGhost||p->mDead)return nullptr;
    return &context->equipmentPlants.try_emplace(generation).first->second;
}
const NativeEquipmentPlantState* GetNativeEquipmentPlantState(const Plant* p)
{
    const auto* context=GetNativeContext(p?p->mBoard:nullptr);if(!context)return nullptr;
    const uint32_t generation=LiveId(p->mBoard->mPlants,p);const auto found=context->equipmentPlants.find(generation);
    return generation&&found!=context->equipmentPlants.end()?&found->second:nullptr;
}
Coin* ResolveNativeEquipment(Board* b,uint16_t id,uint32_t token)
{
    if(!Current(b)||!id||!token)return nullptr;Coin* c=nullptr;
    while(b->IterateCoins(c))if(!c->mDead&&c->mNetId==id)
        if(const auto* d=GetNativeEquipmentData(c);d&&d->instanceToken==token)return c;
    return nullptr;
}
Coin* GetNativeEquipmentHeldBy(const Plant* p)
{
    const auto* source=GetNativePlantData(p);if(!source||!p->mNetId||!source->instanceToken)return nullptr;
    Coin* c=nullptr;while(p->mBoard->IterateCoins(c))if(!c->mDead)
        if(const auto* d=GetNativeEquipmentData(c);d&&d->sourcePlantId==p->mNetId&&d->sourcePlantToken==source->instanceToken)return c;
    return nullptr;
}
bool CanCreateNativeEquipment(Board* b,FusionEquipmentType type)
{
    return Host(b)&&FusionEquipmentTypeValid(int(type))&&b->mCoins.mBlock&&b->mCoins.mSize<b->mCoins.mMaxSize&&
        CanCreateNativeEquipmentVisual(b,type);
}
Coin* CreateNativeEquipment(Board* b,FusionEquipmentType type,float x,float y,bool disappear)
{
    if(!Point(x,y)||!CanCreateNativeEquipment(b,type))return nullptr;
    auto* c=b->mCoins.DataArrayAlloc();c->mApp=b->mApp;c->mBoard=b;
    // This initializer has no gameplay RNG, no vanilla Coin spawn event and no
    // ordinary fall impulse. Snapshot birth publishes the complete typed Coin.
    c->CoinInitializeNetGhost(int(x-20),int(y-20),COIN_NONE,COIN_MOTION_COIN,nullptr);
    c->mNetGhost=false;c->mNetId=b->NetAllocId();
    if(!c->mNetId){b->mCoins.DataArrayFree(c);return nullptr;}
    NativeEquipmentData data;data.type=type==FusionEquipmentType::Door?FusionEquipmentType::Bucket:type;
    data.factoryType=type;data.instanceToken=LiveId(b->mCoins,c);data.anchorX=x;data.anchorY=y;
    data.gravity=9.8f;data.disappear=disappear;
    // Bucket.Start captures its anchor BEFORE clamping, and always draws VX,
    // including a captured/non-disappearing Bucket. Velocity survives landing.
    const auto bounds=BoardBounds(b);
    Position(c,(std::max)(bounds.left,(std::min)(x,bounds.right)),(std::max)(bounds.top,(std::min)(y,bounds.bottom)));
    c->mVelX=RandRangeFloat(-1.5f,1.5f)*float(kFusionBasePixelsX)*.01f;
    c->mVelY=-4.f*float(GetNativeFusionPixelsY(b))*.01f;c->mGroundY=int(bounds.landing-20);
    GetNativeContext(b)->equipmentCoins.emplace(data.instanceToken,data);
    LawnNet::NetTrace("Fusion equipment birth coin=%u token=%u factory=%u type=%u anchor=%.4f,%.4f pos=%.4f,%.4f vx=%.6f vy=%.6f tick=%d",
        c->mNetId,data.instanceToken,unsigned(type),unsigned(data.type),x,y,c->mPosX+20,c->mPosY+20,c->mVelX,c->mVelY,b->mMainCounter);
    return c;
}
bool BeginNativeEquipmentPull(Coin* c,Plant* p,float x,float y)
{
    if(!CanMutate(c)||!SourcePlant(p)||p->mBoard!=c->mBoard||!Point(x,y))return false;
    std::array<float,6> matrix;if(!ParentTransform(p,matrix))return false;
    auto* d=GetNativeEquipmentData(c);const auto* source=GetNativePlantData(p);
    if(d->phase==NativeEquipmentPhase::Cursor||d->sourcePlantId||GetNativeEquipmentHeldBy(p))return false;
    const float k=GetNativeSourcePixelsPerWorldUnit();
    const double det=double(matrix[0])*matrix[3]-double(matrix[1])*matrix[2];
    d->attachmentBasis={float(matrix[3]*k/det),float(-matrix[1]*k/det),
        float(matrix[2]*k/det),float(-matrix[0]*k/det)};
    d->sourcePlantId=p->mNetId;d->sourcePlantToken=source->instanceToken;d->phase=NativeEquipmentPhase::Pulling;
    d->pullStartX=c->mPosX+20;d->pullStartY=c->mPosY+20;d->pullEndX=x;d->pullEndY=y;
    d->phaseStartAge=c->mCoinAge;d->disappear=false;d->claimSlot=255;c->mHitGround=true;
    return true;
}
Coin* CreateNativeEquipmentForCapture(Plant* p,FusionEquipmentType type,float startX,float startY,float endX,float endY)
{
    if(!SourcePlant(p)||!Point(startX,startY)||!Point(endX,endY)||GetNativeEquipmentHeldBy(p))return nullptr;
    std::array<float,6> matrix;if(!ParentTransform(p,matrix))return nullptr;
    auto* c=CreateNativeEquipment(p->mBoard,type,startX,startY,false);if(!c)return nullptr;
    // Source coroutine's initial world position precedes Bucket.Start's clamp.
    Position(c,startX,startY);
    if(BeginNativeEquipmentPull(c,p,endX,endY))return c;
    RetireNativeEquipment(c,false);return nullptr;
}
bool TryGetNativeEquipmentHoldAnchor(const Plant* p,float& x,float& y)
{
    const auto* d=GetNativePlantData(p);const auto* profile=d?FindFusionEquipmentProfile(d->kind):nullptr;
    return profile&&profile->family==FusionEquipmentFamily::Capture&&
        GetNativeSourcePlantNodePoint(p,d->kind==1102?"Blover_petal/Position":d->kind==1166?"Pos":"Position",x,y)&&Point(x,y);
}
bool PutDownNativeEquipment(Coin* c)
{
    if(!CanMutate(c))return false;auto* d=GetNativeEquipmentData(c);
    float localX=0,localY=0;
    if(d->sourcePlantId)
    {
        auto* source=ResolveSource(c->mBoard,*d);std::array<float,6> matrix;
        if(!source||!ParentTransform(source,matrix)||!ParentLocal(matrix,d->anchorX,d->anchorY,localX,localY))return false;
    }
    ClearNativeEquipmentLease(c->mBoard,c->mNetId,d->instanceToken);
    d->claimSlot=255;d->phase=NativeEquipmentPhase::World;d->phaseStartAge=c->mCoinAge;
    if(d->sourcePlantId){d->pullStartX=localX;d->pullStartY=localY;}
    Position(c,d->anchorX,d->anchorY);c->mHitGround=true;return true;
}
bool DropNativeEquipmentFromPlant(Coin* c)
{
    if(!CanMutate(c))return false;auto* d=GetNativeEquipmentData(c);
    if(auto* source=ResolveSource(c->mBoard,*d))
    {
        std::array<float,6> m;
        if(ParentTransform(source,m))
        {
            const auto b=d->attachmentBasis;const float k=GetNativeSourcePixelsPerWorldUnit();
            d->attachmentBasis={(m[0]*b[0]+m[2]*b[1])/k,-(m[1]*b[0]+m[3]*b[1])/k,
                (m[0]*b[2]+m[2]*b[3])/k,-(m[1]*b[2]+m[3]*b[3])/k};
        }
    }
    NativeEquipmentCoinRemoving268(c);ClearNativeEquipmentLease(c->mBoard,c->mNetId,d->instanceToken);
    d->sourcePlantId=0;d->sourcePlantToken=0;d->claimSlot=255;d->phase=NativeEquipmentPhase::Falling;
    d->phaseStartAge=c->mCoinAge;d->anchorX=c->mPosX+20;d->anchorY=c->mPosY+20;d->disappear=true;
    c->mDisappearCounter=0;c->mHitGround=false; // Source retains velocity and existing flash state.
    return true;
}
void RetireNativeEquipment(Coin* c,bool sourceDieCallback)
{
    if(!CanMutate(c))return;auto* b=c->mBoard;auto* context=GetNativeContext(b);
    const auto data=*GetNativeEquipmentData(c);const uint32_t generation=LiveId(b->mCoins,c);
    ClearNativeEquipmentLease(b,c->mNetId,data.instanceToken);NativeEquipmentCoinRemoving268(c);
    context->equipmentCoins.erase(generation); // One-shot before any recovery callback can recurse.
    c->Die();if(sourceDieCallback)RecoverOnSourceDie(b,data.type);
    LawnNet::NetTrace("Fusion equipment retire coin=%u token=%u sourceDie=%d tick=%d",c->mNetId,data.instanceToken,sourceDieCallback?1:0,b->mMainCounter);
}
bool ConsumeNativeEquipment(Coin* c)
{if(!CanMutate(c))return false;RetireNativeEquipment(c,false);return true;}
bool IsNativeEquipmentCoin(const Coin* c){return GetNativeEquipmentData(c)!=nullptr;}

Plant* TryReplaceNativeEquipmentReceiver(Plant* old,uint16_t kind)
{
    if(!old||!Host(old->mBoard)||old->mNetGhost||old->mDead||!old->mIsOnBoard||old->mPlantHealth<=0||
        !CanApplyNativeFusionGameplay(old->mBoard))return nullptr;
    auto* b=old->mBoard;const auto* prior=GetNativePlantData(old);
    const bool allowed=prior&&((prior->kind==3&&kind==1129)||(prior->kind==1027&&kind==1028)||
        (prior->kind==1092&&(kind==1097||kind==1098||kind==1099)));
    if(!allowed||b->mPlants.DataArrayTryToGet(prior->instanceToken)!=old||
        b->mPlants.mSize>=b->mPlants.mMaxSize||!EnsureNativePlantVisuals(b,kind))return nullptr;
    // Source SetPlant(Default) creates a new actor, not a recipe child. Publish
    // its complete fresh state before retiring the previous layer occupant.
    NativePlantData fresh;fresh.kind=kind;fresh.owner=LawnNet::NET_SHARED_OWNER;
    auto* child=b->AddPlant(old->mPlantCol,old->mRow,GetNativeSeedForPlant(kind),SEED_NONE,&fresh);
    const auto* bound=GetNativePlantData(child);
    if(!child||child->mDead||!bound||!child->mNetId||!bound->instanceToken||bound->kind!=kind||
        b->mPlants.DataArrayTryToGet(bound->instanceToken)!=child)
    {if(child)child->Die(255);return nullptr;}
    old->Die(0);RebuildNativeEquipmentGraph(b);return child;
}

bool TryNativeEquipmentGraphSink(Plant* plant,FusionEquipmentType type,Coin* item)
{
    const auto* data=GetNativePlantData(plant);
    if(data&&data->kind==924)return TryUseNativeHypno269Item(plant,item);
    if(data&&(data->kind==1157||data->kind==1166))return TryUseNativeSummonPlants269Item(plant,type,item);
    // Box246 / HypnoMagnet1157 need their real downstream
    // controllers. Keep the source callback explicit until those are ported.
    return false;
}

bool UpdateNativeEquipmentCoin(Coin* c)
{
    if(UpdateNativeSummonPlants269Coin(c))return true;
    auto* d=GetNativeEquipmentData(c);if(!d)return false;
    // Coin::Update already advanced its sole age; client snapshots alone own
    // every typed field and all motion on remote peers.
    if(!CanMutate(c)||!CanApplyNativeFusionGameplay(c->mBoard))return true;
    if(d->phase==NativeEquipmentPhase::Falling)
    {
        const float py=float(GetNativeFusionPixelsY(c->mBoard));
        c->mVelY+=d->gravity*py*.0001f;
        float x=c->mPosX+20+c->mVelX,y=c->mPosY+20+c->mVelY;
        const float floor=BoardBounds(c->mBoard).landing;
        if(y>d->anchorY+py||y>floor)
        {
            y=(std::min)(y,floor);d->anchorX=x;d->anchorY=y;d->phase=NativeEquipmentPhase::World;
            d->phaseStartAge=c->mCoinAge;c->mHitGround=true;
        }
        Position(c,x,y);
    }
    else if(d->phase==NativeEquipmentPhase::Pulling)
    {
        auto* source=ResolveSource(c->mBoard,*d);std::array<float,6> matrix;
        if(!source){DropNativeEquipmentFromPlant(c);return true;}
        const float t=(std::min)(1.f,float(c->mCoinAge-d->phaseStartAge)/30.f);
        Position(c,d->pullStartX+(d->pullEndX-d->pullStartX)*t,d->pullStartY+(d->pullEndY-d->pullStartY)*t);
        if(t>=1&&ParentTransform(source,matrix)&&ParentLocal(matrix,d->pullEndX,d->pullEndY,d->pullStartX,d->pullStartY))
        {d->phase=NativeEquipmentPhase::Attached;d->phaseStartAge=c->mCoinAge;}
    }
    else if(d->phase==NativeEquipmentPhase::Attached||(d->phase==NativeEquipmentPhase::World&&d->sourcePlantId))
    {
        auto* source=ResolveSource(c->mBoard,*d);std::array<float,6> matrix;
        if(!source){DropNativeEquipmentFromPlant(c);return true;}
        if(ParentTransform(source,matrix))Position(c,matrix[0]*d->pullStartX+matrix[2]*d->pullStartY+matrix[4],
            matrix[1]*d->pullStartX+matrix[3]*d->pullStartY+matrix[5]);
    }
    if(d->disappear)
    {
        if(c->mDisappearCounter<(std::numeric_limits<int>::max)())++c->mDisappearCounter;
        bool flash=false,expire=false;
        TryGetFusionBucketLifetime(float(c->mDisappearCounter)*.01f,true,false,d->phase==NativeEquipmentPhase::Cursor,d->flashing,flash,expire);
        if(flash)d->flashing=true;
        if(expire)RetireNativeEquipment(c,true);
    }
    return true;
}
}
