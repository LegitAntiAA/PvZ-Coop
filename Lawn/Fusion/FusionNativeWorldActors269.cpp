#include "FusionNativeWorldActors269.h"
#include "FusionNativeLaserConnections269.h"
#include "FusionNative.h"
#include "FusionNativeVisual.h"
#include "FusionNativeShooterTiming.h"
#include "FusionNativeFireActionBatch.h"
#include "FusionNativeWater268.h"
#include "FusionBasePlantRules.h"
#include "FusionFireActionBatchRules.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../Net/NetCommon.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "../../SexyAppFramework/Graphics.h"
#include "../../SexyAppFramework/Point.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
namespace LawnFusion
{
namespace
{
const FusionShooterTimingProfile& PrismTiming()
{return *FindFusionWorldActors269Timing(34);}
bool Host(const GameObject* p)
{return p&&p->mApp&&p->mBoard&&p->mApp->mBoard==p->mBoard&&!p->mNetGhost&&!p->mApp->NetIsClient()&&IsNativeFusionBoard(p->mBoard);}
bool Playing(const GameObject* p)
{return Host(p)&&!p->mBoard->mPaused&&CanApplyNativeFusionGameplay(p->mBoard);}
bool Live(Zombie* z)
{return z&&!z->mDead&&!z->IsDeadOrDying()&&!z->mMindControlled&&GetNativeFusionZombieHealth(z)>0&&!NativeWater268ZombieCaptured(z);}
FusionWorld269Reference Ref(Plant* p)
{const auto* d=GetNativePlantData(p);return d?FusionWorld269Reference{p->mNetId,d->instanceToken}:FusionWorld269Reference{};}
FusionWorld269Reference Ref(Zombie* z)
{
    const uint32_t gen=z->mBoard->ZombieGetID(z);const auto* c=GetNativeContext(z->mBoard);
    const auto it=c->zombieInstanceTokens.find(gen);
    return {z->mNetId,it==c->zombieInstanceTokens.end()?gen:it->second};
}
Plant* ResolvePlant(Board* b,FusionWorld269Reference r)
{if(!r.id||!r.token)return nullptr;Plant* p=nullptr;while(b->IteratePlants(p))if(!p->mDead&&p->mNetId==r.id&&Ref(p).token==r.token)return p;return nullptr;}
Zombie* ResolveZombie(Board* b,FusionWorld269Reference r)
{if(!r.id||!r.token)return nullptr;Zombie* z=nullptr;while(b->IterateZombies(z))if(z->mNetId==r.id&&Ref(z).token==r.token&&Live(z))return z;return nullptr;}
bool Touches(Plant* p,float x,float y,double radius,Zombie* z)
{
    const auto r=z->GetZombieRect();const double ratio=kFusionBasePixelsY/GetNativeFusionPixelsY(p->mBoard);
    return FusionFireCircleTouchesRect(x,y,radius,r.mX,y+(r.mY-y)*ratio,r.mWidth,r.mHeight*ratio);
}
bool Muzzle(const Plant* p,float& x,float& y)
{return GetNativeSourcePlantNodePoint(p,GetNativePlantData(p)->kind==34?"body/Ball0":"fightball",x,y);}
bool PrismAllowed(Plant* p,Zombie* z,int rows)
{return Live(z)&&std::abs(z->mRow-p->mRow)<=rows&&FusionPlantSearchUniqueZombieAllowed(GetNativeFusionZombieStatus(z),false,false);}
bool EmitLine(Plant* p,float x,float y,float endX,float endY,float extra,int row,bool endpoint=true)
{
    if(!CanCreateNativeWorld269Effects(p,false))return false;
    NativeWorld269BeamState state;state.ownerId=p->mNetId;state.ownerToken=GetNativePlantData(p)->instanceToken;
    state.startX=x;state.startY=y;state.endX=endX;state.endY=endY;state.width=.2f+extra;state.sortingRow=row;
    // No damage or reference mutation follows a failed actual actor birth.
    auto* beam=CreateNativeWorld269BeamCarrier(p->mBoard,state);if(!beam)return false;
    if(!endpoint||CreateNativeWorld269Effect(p,99,endX,endY,row))return true;
    beam->Die();return false;
}
void AcquireLaser(Plant* p,NativeWorld269PlantState& s)
{
    const int light=GetNativeFireLightLevel(p),limit=s.connected?6:4;
    for(int pass=0;pass<2;++pass)
    {
        Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))
        {
            if(!FusionLaser269CanAcquire(GetNativeFusionZombieStatus(z),z->mMindControlled,Live(z),z->mRow-p->mRow,light,s.connected,pass==0))continue;
            const auto ref=Ref(z);bool found=false;for(auto held:s.zombies)if(held.id==ref.id&&held.token==ref.token)found=true;
            if(found)continue;
            int slot=0;while(slot<limit&&s.zombies[slot].id)++slot;if(slot==limit)return;s.zombies[slot]=ref;
        }
    }
}
void PulseLaser(Plant* p,NativeWorld269PlantState& s)
{
    const int base=FusionFindPlant(937)->damage;
    for(auto& ref:s.zombies)
    {
        auto* z=ResolveZombie(p->mBoard,ref);if(!z){ref={};continue;}
        int damage=0;if(TryGetFusionLaser269Damage(base,s.connected,HasNativeWorld269Trait(p,NativeWorld269Trait::Advanced47),GetNativeFusionZombieStatus(z),damage))z->TakeDamage(damage,0U);
        if(!Live(z))ref={};
    }
    AcquireLaser(p,s); // Source attacks previous references BEFORE filling slots.
}
void Energy(Plant* p,NativeWorld269PlantState& s,int cost)
{
    bool depleted=false;int after=0;if(!TryAdjustFusionLaser269Energy(s.energy,s.maximumEnergy,cost,after,depleted))return;
    s.energy=after;if(depleted)p->mDoSpecialCountdown=500;
}
void HealLaser(Plant* p,NativeWorld269PlantState& s)
{
    if(s.energy>s.maximumEnergy/10)
        for(auto& ref:s.plants)
        {
            auto* target=ResolvePlant(p->mBoard,ref);if(!target){ref={};continue;}
            RecoverNativeCompanions269Plant(target,FusionLaser269Healing(target->mPlantMaxHealth));
            Energy(p,s,s.maximumEnergy/10);if(target->mPlantHealth>=target->mPlantMaxHealth)ref={};
        }
    Plant* target=nullptr;while(p->mBoard->IteratePlants(target))
    {
        if(target->mDead||target->mPlantHealth>=target->mPlantMaxHealth||std::abs(target->mPlantCol-p->mPlantCol)>1||std::abs(target->mRow-p->mRow)>1)continue;
        const auto ref=Ref(target);if(!ref.id)continue;bool found=false;for(auto held:s.plants)if(held.id==ref.id&&held.token==ref.token)found=true;
        if(found)continue;auto slot=std::find_if(s.plants.begin(),s.plants.end(),[](auto r){return !r.id;});if(slot==s.plants.end())return;*slot=ref;
    }
}
void AcquirePrism(Plant* p,NativeWorld269PlantState& s)
{
    float x=0,y=0;if(!Muzzle(p,x,y))return;Zombie* best=nullptr;double nearest=(std::numeric_limits<double>::max)();
    Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))
    {
        float zx=0,zy=0;if(!PrismAllowed(p,z,2)||!Touches(p,x,y,4,z)||!z->GetShadowCenter(zx,zy))continue;
        const double distance=std::hypot((zx-x)/kFusionBasePixelsX,(zy-.8*GetNativeFusionPixelsY(p->mBoard)-y)/GetNativeFusionPixelsY(p->mBoard));
        if(distance<nearest){nearest=distance;best=z;}
    }
    if(!best)return;s.zombies[0]=Ref(best);s.receiver={};s.contributors=0;
    BeginNativeShooterShot(p,PrismTiming());
    Plant* other=nullptr;while(p->mBoard->IteratePlants(other))
    {
        const auto* d=GetNativePlantData(other);if(!d||d->kind!=34||other==p||other->mDead||d->shootSeconds>=0||std::abs(other->mPlantCol-p->mPlantCol)>2||std::abs(other->mRow-p->mRow)>2)continue;
        auto* support=EditNativeWorld269PlantState(other);if(!support)continue;
        support->receiver=Ref(p);support->zombies={};support->contributors=0;other->mLaunchCounter=140;BeginNativeShooterShot(other,PrismTiming());++s.contributors;
    }
}
}
bool IsNativeWorld269Plant(const Plant* p)
{const auto* d=GetNativePlantData(p);return d&&(d->kind==34||d->kind==937)&&IsNativeFusionBoard(p->mBoard);}
const FusionShooterTimingProfile* GetNativeWorld269Timing(const Plant* p)
{const auto* d=GetNativePlantData(p);return d&&d->kind==34?&PrismTiming():nullptr;}
bool InitializeNativeWorld269Plant(Plant* p)
{
    if(!IsNativeWorld269Plant(p))return false;if(!Host(p))return true;if(!CanCreateNativeWorld269Effects(p,false))return false;auto* s=EditNativeWorld269PlantState(p,true);if(!s)return false;*s={};auto* d=GetNativePlantData(p);
    p->mState=STATE_NOTREADY;p->mStateCountdown=p->mDoSpecialCountdown=p->mShootingCounter=0;d->animationSpeed=RandRangeFloat(.9f,1.1f);d->shootSeconds=-1;d->idleSeconds=0;
    if(d->kind==34)p->mLaunchCounter=150;
    else {s->maximumEnergy=HasNativeWorld269Trait(p,NativeWorld269Trait::Advanced46)?20000:2000;s->energy=s->maximumEnergy;p->mLaunchCounter=0;p->mDoSpecialCountdown=100;}
    return true;
}
bool UpdateNativeWorld269Plant(Plant* p)
{
    if(!IsNativeWorld269Plant(p))return false;if(!Playing(p)||p->mDead)return true;auto* s=EditNativeWorld269PlantState(p);if(!s)return true;
    if(GetNativePlantData(p)->kind==34){if(--p->mLaunchCounter<=0){p->mLaunchCounter=150;if(GetNativePlantData(p)->shootSeconds<0&&CanCreateNativeWorld269Effects(p,false))AcquirePrism(p,*s);}return true;}
    if(s->connected&&!ResolvePlant(p->mBoard,s->connectedPlant))DisconnectNativeLaser269(p);
    if(p->mDoSpecialCountdown<=0){p->mDoSpecialCountdown=100;const int light=GetNativeFireLightLevel(p);if(light>0)Energy(p,*s,-light*(HasNativeWorld269Trait(p,NativeWorld269Trait::Advanced46)?500:25));}
    if(p->mStateCountdown<=0){p->mStateCountdown=201;HealLaser(p,*s);}
    if(!s->connected||s->main)
    {
        if(--p->mLaunchCounter<=0){p->mLaunchCounter=int(std::floor(FusionFindPlant(937)->attackIntervalSeconds*RandRangeFloat(.95f,1.05f)*100))+1;PulseLaser(p,*s);}
        if(s->connected&&p->mBoard->mMainCounter>=s->explosionDeadline)
        {
            const bool advanced=HasNativeWorld269Trait(p,NativeWorld269Trait::Advanced47);s->explosionDeadline=p->mBoard->mMainCounter+int(std::floor(RandRangeFloat(.95f,1.05f)*(advanced?200:600)))+1;
            std::vector<Zombie*> held;for(auto ref:s->zombies)if(auto* z=ResolveZombie(p->mBoard,ref))held.push_back(z);
            if(held.size()==6&&CanCreateNativeWorld269Effects(p,true))
            {
                auto* center=held[RandRangeInt(0,5)];float x=0,y=0;if(center->GetShadowCenter(x,y)&&CreateNativeWorld269Effect(p,100,x,y,center->mRow))
                {std::vector<ZombieID> hit;Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))if(Live(z)&&std::abs(z->mRow-center->mRow)<2&&Touches(p,x,y,2,z))hit.push_back(p->mBoard->ZombieGetID(z));for(auto id:hit)if((z=p->mBoard->ZombieTryToGet(id))&&Live(z))ApplyNativeSourceExplosionDamage(z,FusionFindPlant(937)->damage*(advanced?10:3));}
            }
        }
    }
    return true;
}
void UpdateNativeWorld269PlantPhase(Plant* p)
{if(IsNativeWorld269Plant(p)&&Playing(p)){if(GetNativePlantData(p)->kind==34)UpdateNativeShooterPhase(p,PrismTiming());else GetNativePlantData(p)->idleSeconds+=.01f;}}
bool DispatchNativeWorld269Shot(Plant* p)
{
    if(!IsNativeWorld269Plant(p))return false;if(GetNativePlantData(p)->kind!=34||!Playing(p))return true;
    bool due=false;if(!TryGetFusionShooterEventAtCounter(PrismTiming(),GetNativePlantData(p)->animationSpeed,p->mShootingCounter,0,due)||!due)return true;
    auto* s=EditNativeWorld269PlantState(p);if(!s)return true;float x=0,y=0;if(!Muzzle(p,x,y))return true;
    if(auto* receiver=ResolvePlant(p->mBoard,s->receiver)){float rx=0,ry=0;if(Muzzle(receiver,rx,ry))EmitLine(p,x,y,rx,ry,0,(std::max)(p->mRow,receiver->mRow),false);s->receiver={};return true;}
    auto* target=ResolveZombie(p->mBoard,s->zombies[0]);if(!target){s->zombies[0]={};return true;}float tx=0,ty=0;if(!target->GetShadowCenter(tx,ty))return true;ty-=float(.8*GetNativeFusionPixelsY(p->mBoard));
    int damage=0;if(!TryGetFusionPrism269Damage(FusionFindPlant(34)->damage,s->contributors,damage)||!EmitLine(p,x,y,tx,ty,(std::min)(.25f,s->contributors*.05f),(std::max)(p->mRow,target->mRow)))return true;
    std::vector<ZombieID> others;Zombie* z=nullptr;while(p->mBoard->IterateZombies(z))if(z!=target&&PrismAllowed(p,z,5)&&std::abs(z->mRow-target->mRow)<2&&Touches(p,tx,ty,2,z)){others.push_back(p->mBoard->ZombieGetID(z));if(others.size()==3)break;}
    const int targetRow=target->mRow;target->TakeDamage(damage,0U);
    for(auto id:others)if((z=p->mBoard->ZombieTryToGet(id))&&Live(z)){float zx=0,zy=0;if(z->GetShadowCenter(zx,zy)&&EmitLine(p,tx,ty,zx,zy-float(.8*GetNativeFusionPixelsY(p->mBoard)),0,(std::max)(targetRow,z->mRow)))z->TakeDamage(damage/2,0U);}
    s->zombies[0]={};s->receiver={};return true;
}
bool ConnectNativeLaser269(Plant* p,Plant* other)
{
    // Structural pairing also runs when an owned glove is cancelled while
    // paused. Its host-only refs do not apply damage or advance gameplay.
    if(!Host(p)||p->mDead||!p->mIsOnBoard||!IsNativeWorld269Plant(p)||!IsNativeWorld269Plant(other)||other->mDead||!other->mIsOnBoard||p==other||GetNativePlantData(p)->kind!=937||GetNativePlantData(other)->kind!=937||p->mBoard!=other->mBoard||!CanCreateNativeWorld269Effects(p,true))return false;
    auto* a=EditNativeWorld269PlantState(p);auto* b=EditNativeWorld269PlantState(other);if(!a||!b||a->connected||b->connected||!CreateNativeLaser269Connection(p,other))return false;
    a->connected=b->connected=true;a->main=true;b->main=false;a->connectedPlant=Ref(other);b->connectedPlant=Ref(p);a->explosionDeadline=p->mBoard->mMainCounter;return true;
}
void DisconnectNativeLaser269(Plant* p)
{
    auto* s=EditNativeWorld269PlantState(p);if(!s)return;auto* other=ResolvePlant(p->mBoard,s->connectedPlant);
    RemoveNativeLaser269Connection(p);s->connected=s->main=false;s->connectedPlant={};s->explosionDeadline=0;s->zombies[4]=s->zombies[5]={};
    if(other)if(auto* peer=EditNativeWorld269PlantState(other)){peer->connected=peer->main=false;peer->connectedPlant={};peer->explosionDeadline=0;peer->zombies[4]=peer->zombies[5]={};}
}
bool AdjustNativeLaser269Damage(Plant* p,int& damage)
{
    if(!Host(p)||!IsNativeWorld269Plant(p)||GetNativePlantData(p)->kind!=937||damage<0)return false;auto* s=EditNativeWorld269PlantState(p);if(!s)return false;
    const int before=s->energy;const int64_t spent=int64_t(damage)*5;Energy(p,*s,int(std::min<int64_t>(spent,(std::numeric_limits<int>::max)())));damage=(std::max)(0,damage-before/5);return true;
}
bool GetNativeWorld269VisualPose(const Plant* p,const char*& state,float& seconds)
{if(!IsNativeWorld269Plant(p))return false;const auto* d=GetNativePlantData(p);const auto* s=GetNativeWorld269PlantState(p);if(d->kind==34){state=d->shootSeconds>=0?"shoot":"idle";seconds=d->shootSeconds>=0?d->shootSeconds:d->idleSeconds;}else{state=s&&s->connected?"connect":s&&std::any_of(s->zombies.begin(),s->zombies.end(),[](auto r){return r.id!=0;})?"shoot":"idle";seconds=d->idleSeconds;}return true;}
bool GetNativeLaser269Segment(const Plant* p,unsigned index,float& x,float& y,float& tx,float& ty,int& row)
{
    const auto* d=GetNativePlantData(p);const auto* s=GetNativeWorld269PlantState(p);if(!d||d->kind!=937||!s||index>=8||(s->connected&&!s->main))return false;
    int originRow=0;if(s->connected){if(!GetNativeLaserConnections269BallPoint(p,x,y,originRow))return false;}else if(!Muzzle(p,x,y))return false;
    row=p->mRow;if(index<6){auto* z=ResolveZombie(p->mBoard,s->zombies[index]);if(!z)return false;const auto r=z->GetZombieRect();tx=r.mX+r.mWidth*.5f;ty=r.mY+r.mHeight*.5f;row=(std::max)(row,z->mRow);return true;}
    auto* target=ResolvePlant(p->mBoard,s->plants[index-6]);if(!target||!target->GetShadowCenter(tx,ty))return false;row=(std::max)(row,target->mRow);return true;
}
bool IsNativeWorld269Beam(const Projectile* p){return p&&IsNativeFusionBoard(p->mBoard)&&GetNativeWorld269BeamState(p);}
bool UpdateNativeWorld269Beam(Projectile* p)
{if(!IsNativeWorld269Beam(p))return false;if(Host(p)&&FusionPrism269Expired(p->mProjectileAge))p->Die();return true;}
bool DrawNativeWorld269Beam(Sexy::Graphics* g,Projectile* p)
{
    const auto* s=GetNativeWorld269BeamState(p);if(!g||!s)return false;const float alpha=FusionPrism269Alpha(p->mProjectileAge);if(alpha<=0)return true;
    // Serialized PrismLine width=.2, two RGBA gradient keys. CPU ribbon keeps
    // source geometry/color; URP material bloom/MSAA remain a visual limit.
    const double dx=s->endX-s->startX,dy=s->endY-s->startY,py=GetNativeFusionPixelsY(p->mBoard),len=std::hypot(dx/kFusionBasePixelsX,dy/py);if(len<=1e-6)return true;
    const double half=s->width*.5,nx=-(dy/py)/len*half*kFusionBasePixelsX,ny=(dx/kFusionBasePixelsX)/len*half*py;
    g->PushState();for(int i=0;i<16;++i){const double t0=i/16.,t1=(i+1)/16.;Sexy::Point pts[4]={{int(s->startX+dx*t0+nx),int(s->startY+dy*t0+ny)},{int(s->startX+dx*t1+nx),int(s->startY+dy*t1+ny)},{int(s->startX+dx*t1-nx),int(s->startY+dy*t1-ny)},{int(s->startX+dx*t0-nx),int(s->startY+dy*t0-ny)}};const double t=(t0+t1)*.5;g->SetColor(Sexy::Color(255,int(255*(.9701796+(.96862745-.9701796)*t)),int(255*(.6650944+(.6666667-.6650944)*t)),int(alpha*255)));g->PolyFillAA(pts,4,true);}g->PopState();return true;
}
}
