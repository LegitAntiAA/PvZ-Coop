#include "FusionNativeTravelConsumers369.h"
#include "FusionNativeCompanions269.h"
#include "FusionNative.h"
#include "FusionNativePlacement.h"
#include "FusionNativeShooterTiming.h"
#include "FusionNativeEquipment268.h"
#include "FusionEquipmentRules.h"
#include "FusionNativeDefender.h"
#include "FusionNativeProducer.h"
#include "FusionNativeFire.h"
#include "FusionNativeStar269.h"
#include "FusionNativeWater268.h"
#include "FusionNativeProjectileMetadata.h"
#include "FusionNativeVisual.h"
#include "FusionRecipientShooterRules.h"
#include "FusionBasePlantRules.h"
#include "FusionFireActionBatchRules.h"
#include "FusionDeathRules.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include "../../Resources.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace LawnFusion
{
namespace
{
bool Host(const GameObject* p)
{return p&&p->mApp&&p->mBoard&&p->mApp->mBoard==p->mBoard&&!p->mNetGhost&&!p->mApp->NetIsClient()&&IsNativeFusionBoard(p->mBoard);}
bool Playing(const GameObject* p)
{return Host(p)&&!p->mBoard->mPaused&&p->mApp->mGameScene==SCENE_PLAYING;}
bool Live(Plant* p){return p&&!p->mDead&&p->mIsOnBoard&&p->mPlantHealth>0;}
bool Live(Zombie* z){return z&&!z->mDead&&!z->IsDeadOrDying()&&GetNativeFusionZombieHealth(z)>0&&!NativeWater268ZombieCaptured(z);}
FusionCompanion269Point Source(Board* b,float x,float y)
{return {float((x-b->GetCellCenterX(0,0))/kFusionBasePixelsX-4.8),float(2.3-(y-b->GridToPixelY(0,0)-50)/GetNativeFusionPixelsY(b))};}
void Position(Projectile* p,FusionCompanion269Point v)
{
    p->mPosX=float(p->mBoard->GetCellCenterX(0,0)+(v.x+4.8)*kFusionBasePixelsX-20);
    p->mPosY=float(p->mBoard->GridToPixelY(0,0)+50+(2.3-v.y)*GetNativeFusionPixelsY(p->mBoard)-20);p->mPosZ=0;
    p->mShadowY=p->mPosY+20+float(1.5*GetNativeFusionPixelsY(p->mBoard));p->mX=int(p->mPosX);p->mY=int(p->mPosY);
}
Plant* Parent(Projectile* p,const NativeCompanion269State& s)
{
    Plant* v=nullptr;while(p->mBoard->mPlants.IterateNext(v))
    {const auto* d=GetNativePlantData(v);if(Live(v)&&d&&v->mNetId==s.parentId&&d->instanceToken==s.parentToken)return v;}return nullptr;
}
Projectile* Child(Plant* p,NativeCompanion269Reference ref)
{
    Projectile* q=nullptr;while(p->mBoard->IterateProjectiles(q))if(!q->mDead&&q->mNetId==ref.id)
    {const auto* d=GetNativeCompanion269State(q);if(d&&d->instanceToken==ref.token)return q;}return nullptr;
}
Zombie* Target(Projectile* p,NativeCompanion269State& s)
{
    Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))if(z->mNetId==s.targetId&&Live(z)&&p->mBoard->mZombies.DataArrayGetID(z)==s.targetToken)return z;
    s.targetId=0;s.targetToken=0;return nullptr;
}
bool Eligible(Zombie* z,const NativeCompanion269State& s)
{return Live(z)&&FusionCompanion269CanTarget(s.family,GetNativeFusionZombieStatus(z),z->mMindControlled,s.smallVariant);}
Zombie* Acquire(Projectile* p,NativeCompanion269State& s)
{
    const auto point=Source(p->mBoard,p->mPosX+20,p->mPosY+20);Zombie* best=nullptr;float distance=0;
    for(int pass=0;pass<(s.family==FusionCompanion269Family::LittleFly?1:2);++pass)
    {
        Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))
        {
            if(!Eligible(z,s)||(pass==0&&GetNativeFusionZombieStatus(z)!=10))continue;
            float x=0,y=0;if(!z->GetShadowCenter(x,y))continue;const auto shadow=Source(p->mBoard,x,y);const float d=std::hypot(shadow.x-point.x,shadow.y-point.y);
            if(!best||d<distance){best=z;distance=d;}
        }
        if(best)break;
    }
    s.targetId=best?best->mNetId:0;s.targetToken=best?p->mBoard->mZombies.DataArrayGetID(best):0;return best;
}
bool SpawnChild(Plant* p,size_t index,bool supplement=false)
{
    auto* s=EditNativeCompanions269PlantState(p);const auto* profile=FindFusionCompanions269Profile(GetNativePlantData(p)->kind);
    if(!s||!profile||index>=s->children.size())return false;
    if(Child(p,s->children[index]))return true;s->children[index]={};
    if(!CanCreateNativeCompanion269Carrier(p->mBoard,profile->childFamily))return false;
    float x=0,y=0;if(!p->GetShadowCenter(x,y))return false;
    const bool torch=GetNativePlantData(p)->kind==1178;
    if(torch){if(!GetNativeSourcePlantNodePoint(p,"Shoot",x,y))return false;}
    else {x-=float(.5*kFusionBasePixelsX);y-=RandRangeFloat(1.3f,1.5f)*float(GetNativeFusionPixelsY(p->mBoard));}
    NativeCompanion269State state;state.family=profile->childFamily;state.parentId=p->mNetId;state.parentToken=GetNativePlantData(p)->instanceToken;
    state.smallVariant=torch;state.positionOffset=torch?float(index)*.2f:float(index)*.3f;
    state.offsetX=RandRangeFloat(-.1f,.1f);state.offsetY=RandRangeFloat(-.1f,.1f);
    auto* q=CreateNativeCompanion269Carrier(p->mBoard,state,torch?0:p->mRow,x,y);
    const auto* actual=GetNativeCompanion269State(q);
    if(!q||q->mDead||!actual||!q->mNetId||!actual->instanceToken)return false;
    // Carrier allocation may rehash Root's maps. Reacquire the actual parent state.
    s=EditNativeCompanions269PlantState(p);if(!s||index>=s->children.size()){q->Die();return false;}
    s->children[index]={q->mNetId,actual->instanceToken};
    LawnNet::NetTrace("Fusion Companion269 birth parent=%u token=%u child=%u token=%u family=%u slot=%zu small=%d supplement=%d",p->mNetId,state.parentToken,q->mNetId,actual->instanceToken,unsigned(state.family),index,state.smallVariant?1:0,supplement?1:0);return true;
}
void CreateFlies(Plant* p,bool supplement=false)
{
    const auto kind=GetNativePlantData(p)->kind;
    if(kind==1178)return;
    const auto* profile=FindFusionCompanions269Profile(kind);if(!profile||!profile->childSlots)return;
    if(!supplement)SpawnChild(p,0);
    if((kind==911||kind==922)&&(supplement||HasNativeCompanions269Trait(p,NativeCompanions269Trait::Ultimate24)))
    {SpawnChild(p,1,supplement);SpawnChild(p,2,supplement);}
}
Plant* WorldPlant(Plant* p,int column,int row,uint16_t kind)
{
    if(kind==251)return SpawnNativeRollingActor269(p->mBoard,251,column,row);
    NativePlantData incoming;incoming.kind=kind;incoming.owner=LawnNet::NET_SHARED_OWNER;
    return TrySpawnNativeFusionPlant(p->mBoard,column,row,incoming);
}
void CloneConnectedPumpkin(Plant* p)
{
    // Reuse the source MagnetSystem membership and adjacency policy over actual
    // current actors; these are transient query references, not another world.
    std::vector<Plant*> nodes;Plant* v=nullptr;
    while(p->mBoard->IteratePlants(v)){const auto* d=GetNativePlantData(v);if(Live(v)&&d&&FusionEquipmentMagnetMember(d->kind))nodes.push_back(v);}
    const auto first=std::find(nodes.begin(),nodes.end(),p);if(first==nodes.end())return;
    std::vector<bool> connected(nodes.size());connected[size_t(first-nodes.begin())]=true;bool changed=true;
    while(changed){changed=false;for(size_t i=0;i<nodes.size();++i)if(connected[i])for(size_t j=0;j<nodes.size();++j)
        if(!connected[j]&&FusionEquipmentGraphAdjacent(nodes[i]->mPlantCol,nodes[i]->mRow,nodes[j]->mPlantCol,nodes[j]->mRow)){connected[j]=true;changed=true;}}
    for(size_t i=0;i<nodes.size();++i)if(connected[i]&&GetNativePlantData(nodes[i])->kind==935&&NativeCompanions269HasLayerNeighbour(nodes[i]))
    {if(auto* child=ReplaceNativeCompanions269Plant(nodes[i],GetNativePlantData(p)->kind))NativeCompanions269BirthEffect(child,child->mRow);return;}
}
void Attribute(Plant* p)
{
    const auto kind=GetNativePlantData(p)->kind;if(kind==922)return; // Actual empty override.
    const int rows=GetNativeFusionRowCount(p->mBoard),columns=p->mBoard->GetColumnCount();
    const uint16_t output=kind==911?912:1082;
    if(kind==911)
    {
        CloneConnectedPumpkin(p);
        for(int c=(std::max)(0,p->mPlantCol-1);c<=(std::min)(columns-1,p->mPlantCol+1);++c)
            for(int r=(std::max)(0,p->mRow-1);r<=(std::min)(rows-1,p->mRow+1);++r)
            {Plant* v=nullptr;while(p->mBoard->IteratePlants(v))if(Live(v)&&v->mPlantCol==c&&v->mRow==r&&GetNativePlantData(v)&&GetNativePlantData(v)->kind==1082)
                {if(auto* child=ReplaceNativeCompanions269Plant(v,912))NativeCompanions269BirthEffect(child,p->mRow);return;}}
    }
    for(int c=(std::max)(0,p->mPlantCol-1);c<=(std::min)(columns-1,p->mPlantCol+1);++c)
        for(int r=(std::max)(0,p->mRow-1);r<=(std::min)(rows-1,p->mRow+1);++r)
            if(auto* child=WorldPlant(p,c,r,output)){NativeCompanions269BirthEffect(child,p->mRow);return;}
}
void Ray(Projectile* p,const NativeCompanion269State& s,Zombie* target)
{
    float x=0,y=0;if(!GetNativeCompanion269NodePoint(p,"Shoot",x,y))return;
    const auto origin=Source(p->mBoard,x,y);std::vector<std::pair<float,ZombieID>> hits;Zombie* z=nullptr;
    while(p->mBoard->IterateZombies(z))
    {
        if(!Live(z)||!FusionCompanion269RayTarget(GetNativeFusionZombieStatus(z),z->mMindControlled,s.smallVariant)||(target&&z->mRow!=target->mRow))continue;
        const auto rect=z->GetZombieRect();const auto low=Source(p->mBoard,float(rect.mX),float(rect.mY+rect.mHeight));float distance=0;
        if(FusionCompanion269RayTouches(origin,low.x,low.y,float(rect.mWidth/kFusionBasePixelsX),float(rect.mHeight/GetNativeFusionPixelsY(p->mBoard)),&distance))hits.push_back({distance,p->mBoard->ZombieGetID(z)});
    }
    std::stable_sort(hits.begin(),hits.end(),[](const auto& a,const auto& b){return a.first<b.first;});
    auto* parent=Parent(p,s);const int damage=FusionCompanion269Damage(parent&&HasNativeCompanions269Trait(parent,NativeCompanions269Trait::Ultimate25));
    for(const auto& hit:hits)if((z=p->mBoard->ZombieTryToGet(hit.second))&&Live(z))z->TakeDamage(damage,0U); // Source NormalAll1.
    p->mApp->PlayFoley(FOLEY_FUME);
    LawnNet::NetTrace("Fusion Companion269 ray child=%u targets=%zu damage=%d targetRow=%d",p->mNetId,hits.size(),damage,target?target->mRow:-1);
}
void Shoot(Projectile* p,NativeCompanion269State& s)
{
    auto* target=Target(p,s);
    if(s.family!=FusionCompanion269Family::LittleFly){Ray(p,s,target);return;}
    if(!target)return;float x=0,y=0;if(!GetNativeCompanion269NodePoint(p,"Shoot",x,y)||!EnsureNativeProjectileVisuals(p->mBoard,37))return;
    NativeProjectileData data;data.kind=37;data.damage=20;data.specialType=12;
    if(!NativeProjectileMetadataValid(data,PROJECTILE_SPIKE,MOTION_STAR)||p->mBoard->mProjectiles.mSize>=p->mBoard->mProjectiles.mMaxSize)return;
    auto* q=p->mBoard->AddProjectile(int(x-20),int(y-20),p->mRenderOrder-1,target->mRow,PROJECTILE_SPIKE);if(!q)return;
    q->mMotionType=MOTION_STAR;q->mPosX=x-20;q->mPosY=y-20;q->mPosZ=0;q->mShadowY=y;
    q->mVelX=float(6*kFusionBasePixelsX*.01);q->mVelY=q->mVelZ=q->mAccZ=0;q->mRotation=q->mRotationSpeed=0;
    q->mX=int(q->mPosX);q->mY=int(q->mPosY);if(!AttachNativeProjectile(p->mBoard,q,data))q->Die();
    (void)RandRangeInt(3,4);p->mApp->PlayFoley(FOLEY_THROW);
}
}
bool IsNativeCompanions269Plant(const Plant* p)
{const auto* d=GetNativePlantData(p);return d&&FindFusionCompanions269Profile(d->kind);}
bool InitializeNativeCompanions269Plant(Plant* p)
{
    if(!IsNativeCompanions269Plant(p))return false;if(!Host(p))return true;const auto kind=GetNativePlantData(p)->kind;
    if(kind==1169){if(!IsNativeEquipmentPlant(p))return false;InitializeNativeEquipmentPlant(p);return true;}
    auto* s=EditNativeCompanions269PlantState(p,true);if(!s)return false;
    const auto* profile=FindFusionCompanions269Profile(kind);*s={};s->children.resize(profile->childSlots);
    auto* d=GetNativePlantData(p);d->idleSeconds=0;d->shootSeconds=-1;d->animationSpeed=kind==912?1.f:RandRangeFloat(.9f,1.1f);
    d->productionTicks=d->pendingProductionTicks=0;d->batchShotCount=d->batchRageTicks=d->batchBagCount=d->batchMode=0;
    p->mState=STATE_READY;p->mShootingCounter=p->mDoSpecialCountdown=p->mStateCountdown=0;p->mTargetX=0;
    p->mLaunchCounter=FusionCompanions269AttributeTicks(profile->initialAttributeSeconds);
    if(kind==912)
    {
        if(!InitializeNativeShooterTiming(p,*FindFusionCompanions269Timing(kind),false))return false;
        Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))if(!z->mDead&&FusionStar269StartupBlowTarget(GetNativeFusionZombieStatus(z)))z->mBlowingAway=true;
        p->mBoard->mFogBlownCountDown=4000;p->mApp->PlaySample(Sexy::SOUND_BLOVER);
    }
    else CreateFlies(p);return true;
}
bool UpdateNativeCompanions269Plant(Plant* p)
{
    if(!IsNativeCompanions269Plant(p))return false;if(!Playing(p)||!Live(p))return true;const auto kind=GetNativePlantData(p)->kind;
    if(kind==1169)return UpdateNativeEquipmentPlant(p);
    auto* state=EditNativeCompanions269PlantState(p);if(!state)return true;
    bool recreate=false;for(auto it=state->recreateDeadlines.begin();it!=state->recreateDeadlines.end();)
        if(*it<=p->mBoard->mMainCounter){it=state->recreateDeadlines.erase(it);recreate=true;}else ++it;
    if(recreate)CreateFlies(p);
    if(kind==912)
    {
        if(--p->mLaunchCounter<=0)
        {
            ResetNativeShooterLaunchCounter(p,*FindFusionCompanions269Timing(kind));Zombie* z=nullptr;
            while(p->mBoard->IterateZombies(z))if(Live(z)&&z->mRow==p->mRow&&GetNativeFusionZombieStatus(z)==10&&!z->mMindControlled)
                {if(GetNativePlantData(p)->shootSeconds<0)BeginNativeShooterShot(p,*FindFusionCompanions269Timing(kind));break;}
        }
        return true;
    }
    if(kind==1178)
    {
        HandleNativeTorchwood(p);const auto area=p->GetPlantAttackRect(WEAPON_PRIMARY);Projectile* q=nullptr;
        while(p->mBoard->IterateProjectiles(q))
        {
            const auto* d=GetNativeProjectileData(q);
            if(q->mDead||!d||d->kind!=3||d->isZombieBullet||q->mRow!=p->mRow||
                (d->lastTorchId==p->mNetId&&d->lastTorchToken==GetNativePlantData(p)->instanceToken)||GetRectOverlap(area,q->GetProjectileRect())<10)continue;
            if(TrySummonNativeCherryTorch269(p))q->Die();
        }
        return true;
    }
    if(p->mLaunchCounter>0&&!--p->mLaunchCounter)
    {
        Attribute(p);if(p->mDead)return true;
        const auto* profile=FindFusionCompanions269Profile(kind);
        p->mLaunchCounter=FusionCompanions269AttributeTicks(kind==911&&NativeCompanions269ShootingBoard(p->mBoard)?5.f:profile->attributeSeconds);
    }
    return true;
}
void UpdateNativeCompanions269PlantPhase(Plant* p)
{
    if(!Playing(p)||!IsNativeCompanions269Plant(p)||!Live(p))return;
    if(GetNativePlantData(p)->kind==1169){UpdateNativeEquipmentPlantPhase(p);return;}
    NativeDefenderHealthChanged(p);
    if(const auto* timing=FindFusionCompanions269Timing(GetNativePlantData(p)->kind))UpdateNativeShooterPhase(p,*timing);
    else GetNativePlantData(p)->idleSeconds+=.01f;
}
bool DispatchNativeCompanions269Events(Plant* p)
{
    if(!IsNativeCompanions269Plant(p))return false;if(!Playing(p)||!Live(p)||GetNativePlantData(p)->kind!=912)return true;
    const auto* timing=FindFusionCompanions269Timing(912);int count=0;
    if(!TryGetFusionShooterEventsAtCounter(*timing,GetNativePlantData(p)->animationSpeed,p->mShootingCounter,count))return true;
    for(int event=0;event<count;++event)
    {
        const int base=GetNativeTravelPlantAttack369(p,FusionFindPlant(912)->damage),damage=HasNativeCompanions269Trait(p,NativeCompanions269Trait::Ultimate25)?base*3:base;
        Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))if(Live(z)&&z->mRow==p->mRow&&GetNativeFusionZombieStatus(z)==10&&!z->mMindControlled)z->TakeDamage(damage,1U);
        ++GetNativePlantData(p)->batchShotCount;p->mApp->PlayFoley(FOLEY_FUME);
    }
    return true;
}
void NativeCompanions269PlantMoved(Plant* p)
{
    if(!Host(p)||!IsNativeCompanions269Plant(p))return;auto* s=EditNativeCompanions269PlantState(p);if(!s||s->children.empty()||GetNativePlantData(p)->kind==1178)return;
    // Source SuperPumpkin.OnMove changes ONLY littleFly, not its extra fields.
    if(auto* q=Child(p,s->children[0]))q->mRow=p->mRow;
}
void NativeCompanions269PlantRemoving(Plant* p,int reason)
{
    if(!Host(p)||!IsNativeCompanions269Plant(p))return;
    auto* s=EditNativeCompanions269PlantState(p);if(s)
    {const auto children=s->children;s->recreateDeadlines.clear();s->children.assign(children.size(),{});for(const auto ref:children)if(auto* q=Child(p,ref))q->Die();}
    if(GetNativePlantData(p)->kind==1178&&FusionDeathReasonHasEffects(reason))
    {float x=0,y=0;if(p->GetShadowCenter(x,y))ApplyNativeCompanions269PlantCherryExplosion(p,x,y-float(.5*GetNativeFusionPixelsY(p->mBoard)),p->mRow);}
}
bool AdjustNativeCompanions269Damage(Plant* p,int& damage,int sourceType)
{
    if(!Playing(p)||!Live(p)||!IsNativeCompanions269Plant(p)||damage<=0)return false;
    const auto kind=GetNativePlantData(p)->kind;
    if(kind==905){EmitNativeSourceSun(p,2,false);damage=FusionSuperSun269IncomingDamage(HasNativeCompanions269Trait(p,NativeCompanions269Trait::Common5));}
    if(kind==1169&&sourceType==2){RecoverNativeCompanions269Plant(p,damage);return true;}
    return sourceType==2&&(kind==1110||kind==911||kind==922);
}
bool TryActivateNativeSuperSun269(Plant* p)
{
    if(!Playing(p)||!Live(p)||!GetNativePlantData(p)||GetNativePlantData(p)->kind!=905||!CanCreateNativeRollingActor269(p->mBoard,251))return false;
    const int cost=FusionSuperSun269SummonCost(HasNativeCompanions269Trait(p,NativeCompanions269Trait::Advanced4));
    if(!TrySpendNativeSuperSun269Sun(p,cost))return false;RecoverNativeCompanions269Plant(p,p->mPlantMaxHealth);
    if(auto* child=WorldPlant(p,p->mPlantCol+1,p->mRow,251))NativeCompanions269BirthEffect(child,p->mRow);return true;
}
bool TrySupplementNativeCompanions269(Plant* p)
{if(!Playing(p)||!Live(p)||!GetNativePlantData(p)||(GetNativePlantData(p)->kind!=911&&GetNativePlantData(p)->kind!=922))return false;CreateFlies(p,true);return true;}
bool TrySummonNativeCherryTorch269(Plant* p)
{
    if(!Playing(p)||!Live(p)||!GetNativePlantData(p)||GetNativePlantData(p)->kind!=1178)return false;
    auto* s=EditNativeCompanions269PlantState(p);if(!s)return false;
    for(size_t i=0;i<s->children.size();++i)if(!Child(p,s->children[i]))if(SpawnChild(p,i)){p->mTargetX=0;GetNativePlantData(p)->batchShotCount=0;return true;}
    return false;
}
void NativeCherryTorch269ConversionCommitted(Plant* p)
{
    if(!Playing(p)||!GetNativePlantData(p)||GetNativePlantData(p)->kind!=1178)return;
    if(p->mTargetX<(std::numeric_limits<int>::max)())++p->mTargetX;
    GetNativePlantData(p)->batchShotCount=uint32_t(p->mTargetX);if(p->mTargetX>30)TrySummonNativeCherryTorch269(p);
}
bool IsNativeCompanion269(const Projectile* p){return GetNativeCompanion269State(p)!=nullptr;}
bool UpdateNativeCompanion269(Projectile* p)
{
    auto* s=EditNativeCompanion269State(p);if(!s)return false;if(!Playing(p)||p->mDead)return true;
    if(s->ageTicks==p->mProjectileAge)return true; // One callback per real native age, including both clip events.
    s->ageTicks=p->mProjectileAge;auto* parent=Parent(p,*s);if(!parent){p->Die();return true;}
    auto point=Source(p->mBoard,p->mPosX+20,p->mPosY+20);auto* target=Target(p,*s);
    if(target)
    {
        if(!Eligible(target,*s)){s->targetId=s->targetToken=0;target=nullptr;}
        else
        {
            const auto rect=target->GetZombieRect();const float x=s->family==FusionCompanion269Family::CherryLittleFly?float(rect.mX+rect.mWidth*.5):float(rect.mX);
            const auto anchor=Source(p->mBoard,x,float(rect.mY+rect.mHeight*.5));
            const FusionCompanion269Point desired{anchor.x+s->offsetX-1,anchor.y+s->offsetY+.5f};
            TryMoveFusionCompanion269(point,desired,.01f,point);Position(p,point);
            TryRotateFusionCompanion269(p->mRotation,std::atan2(anchor.y-point.y,anchor.x-point.x),.01f,p->mRotation);
            s->onTarget=std::hypot(desired.x-point.x,desired.y-point.y)<.3f;
        }
    }
    else
    {
        Acquire(p,*s);s->onTarget=false;
        if(point.x>s->offsetX*5-6)
        {float x=0,y=0;if(parent->GetShadowCenter(x,y)){auto hold=Source(p->mBoard,x,y);hold.x+=s->positionOffset-.5f;hold.y+=1.4f;TryMoveFusionCompanion269(point,hold,.01f,point);Position(p,point);TryRotateFusionCompanion269(p->mRotation,0,.01f,p->mRotation);}}
    }
    if(s->family==FusionCompanion269Family::CherryLittleFly&&s->onTarget&&target)
    {const int row=target->mRow,variant=s->smallVariant?2:0;p->Die();ApplyNativeCompanion269CherryExplosion(p,row,variant);return true;}
    if(s->family!=FusionCompanion269Family::CherryLittleFly)
    {
        bool trigger=false;float after=0;if(TryStepFusionCompanion269Shoot(s->shootCountdown,.01f,s->onTarget,after,trigger))s->shootCountdown=after;
        if(trigger){s->shootStartAge=p->mProjectileAge;s->shotEmitted=false;}
        if(const auto* timing=FindFusionCompanion269Animation(s->family);s->shootStartAge>=0&&timing)
        {
            const int elapsed=p->mProjectileAge-s->shootStartAge;
            for(const double clipEvent:timing->eventClipSeconds)
                if(elapsed==int(std::ceil(clipEvent/timing->stateSpeed*100)))
                {s->shotEmitted=true;Shoot(p,*s);s=EditNativeCompanion269State(p);if(!s||p->mDead)return true;}
            if(elapsed>=int(std::ceil(FusionShooterDuration(*timing,1)*100))){s->shootStartAge=-1;s->shotEmitted=false;}
        }
    }
    return true;
}
void NativeCompanion269Removing(Projectile* p,bool sourceDie)
{
    if(!Host(p))return;auto* s=EditNativeCompanion269State(p);if(!s||!s->parentId)return;
    auto* parent=Parent(p,*s);const auto token=s->instanceToken;const bool cherry=s->family==FusionCompanion269Family::CherryLittleFly;
    s->parentId=0;s->parentToken=0;s->targetId=0;s->targetToken=0; // Consume before recursive source callbacks.
    if(parent)if(auto* state=EditNativeCompanions269PlantState(parent))
    {
        for(auto& ref:state->children)if(ref.id==p->mNetId&&ref.token==token)ref={};
        if(sourceDie&&cherry&&GetNativePlantData(parent)->kind!=1178)
        {const int ticks=NativeCompanions269ShootingBoard(p->mBoard)?300:3000;if(p->mBoard->mMainCounter<=(std::numeric_limits<int>::max)()-ticks)state->recreateDeadlines.push_back(p->mBoard->mMainCounter+ticks);}
    }
}
bool GetNativeCompanion269VisualPose(const Projectile* p,const char*& state,float& seconds)
{const auto* s=GetNativeCompanion269State(p);if(!s)return false;state=s->shootStartAge>=0?"shoot":"idle";seconds=float(s->shootStartAge>=0?s->ageTicks-s->shootStartAge:s->ageTicks)*.01f;return seconds>=0&&std::isfinite(seconds);}
bool GetNativeCompanion269ShadowCenter(const Projectile* p,float& x,float& y)
{if(!IsNativeCompanion269(p))return false;x=p->mPosX+20;y=p->mPosY+p->mPosZ+20+float(1.5*GetNativeFusionPixelsY(p->mBoard));return true;}
bool IsNativeCompanion269Shot(const Projectile* p)
{const auto* d=GetNativeProjectileData(p);return d&&d->kind==37&&d->specialType==12&&p->mProjectileType==PROJECTILE_SPIKE&&p->mMotionType==MOTION_STAR;}
bool UpdateNativeCompanion269Shot(Projectile* p)
{
    if(!IsNativeCompanion269Shot(p))return false;if(!Playing(p)||p->mDead)return true;
    p->mPosX+=p->mVelX;p->mPosY+=p->mVelY;p->mX=int(p->mPosX);p->mY=int(p->mPosY);
    const auto point=Source(p->mBoard,p->mPosX+20,p->mPosY+20);
    if(point.x< -12||point.x>float(1.35*(p->mBoard->GetColumnCount()-1)-4.8+3.85)||point.y>6.5f||point.y<float(2.3-(GetNativeFusionRowCount(p->mBoard)-1)*(GetNativeFusionRowCount(p->mBoard)==6?1.45:1.67)-.6))p->Die();
    return true;
}
bool HandleNativeCompanion269ShotCollision(Projectile* p)
{
    if(!IsNativeCompanion269Shot(p))return false;if(!Playing(p)||p->mDead)return true;
    FusionRecipientRect area;if(!TryMakeFusionRecipientProjectileRect(37,p->mPosX+20,p->mPosY+20,p->mVelX,p->mVelY,area))return true;
    const double centerY=p->mPosY+20,ratio=kFusionBasePixelsY/GetNativeFusionPixelsY(p->mBoard);
    Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))
    {
        if(!Live(z)||!FusionStar269CanHitStatus(12,GetNativeFusionZombieStatus(z))||z->mMindControlled)continue;
        const auto r=z->GetZombieRect();if(FusionRecipientProjectileTouchesRect(37,p->mPosX+20,centerY,p->mVelX,p->mVelY*ratio,{double(r.mX),centerY+(r.mY-centerY)*ratio,double(r.mWidth),r.mHeight*ratio}))
        {z->TakeDamage(20,0U);p->PlayImpactSound(z);p->Die();break;}
    }
    return true;
}
}
