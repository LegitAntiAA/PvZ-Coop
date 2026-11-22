#include "FusionNativeFireActionBatch.h"
#include "FusionNativeCompanions269.h"
#include "FusionNativeShooter267.h"
#include "FusionShooter267Rules.h"
#include "FusionNative.h"
#include "FusionNativeZombie313Dispatch.h"
#include "FusionNativeMorePlants269Backend.h"
#include "FusionNativeUmbrellaBatch.h"
#include "FusionNativeSeaLantern267.h"
#include "FusionNativeFire.h"
#include "FusionNativeCold.h"
#include "FusionNativeBatchActions.h"
#include "FusionNativeBasePlants.h"
#include "FusionNativePlacement.h"
#include "FusionNativeVisual.h"
#include "FusionNativeDeath.h"
#include "FusionNativeAction266.h"
#include "FusionDefenderRules.h"
#include "FusionNativeDefender.h"
#include "FusionNativeProjectileMetadata.h"
#include "FusionStraightProjectileRules.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../Board.h"
#include "../Net/NetCommon.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "../../Sexy.TodLib/TodParticle.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <vector>

namespace LawnFusion
{
namespace
{
constexpr double PX=72.0/1.35,PY=100.0/1.67;
constexpr double SquashScale=.061242823;
constexpr double SquashImageY=2.25*SquashScale;
constexpr double SquashShadowY=10.0*SquashScale;
bool Current(const GameObject* o)
{ return o&&o->mApp&&o->mBoard&&o->mApp->mBoard==o->mBoard&&IsNativeFusionBoard(o->mBoard)&&NativeFusionBackgroundValid(static_cast<uint8_t>(o->mBoard->mBackground)); }
bool Host(const GameObject* o)
{ return Current(o)&&!o->mApp->NetIsClient()&&!o->mNetGhost; }
bool Playing(const GameObject* o)
{ return Host(o)&&!o->mBoard->mPaused&&o->mApp->mGameScene==SCENE_PLAYING; }
const FusionFireActionProfile* Profile(const Plant* p)
{
    const auto* d=GetNativePlantData(p);
    const auto* profile=d?FindFusionFireActionProfile(d->kind):nullptr;
    return Current(p)&&profile&&profile->enabled&&GetNativeSeedForPlant(d->kind)==p->mSeedType?profile:nullptr;
}
bool Target(Zombie* z)
{
    return Host(z)&&!z->mDead&&!z->IsDeadOrDying()&&IsNativeFusionGroundZombie(z)&&
        !z->mMindControlled&&!z->IsFlying()&&(!GetNativeSourceZombieData(z)||(!z->mInPool&&!z->mOnHighGround));
}
int Ticks(double key,float speed)
{ int ticks=0;return TryGetFusionFireActionTicks(key,speed,ticks)?ticks:0; }
void Ground(Plant* p,float& x,float& y)
{
    x=static_cast<float>(p->mBoard->GetCellCenterX(p->mPlantCol,p->mRow));
    y=static_cast<float>(p->mBoard->GridToPixelY(p->mPlantCol,p->mRow)+50);
    p->GetShadowCenter(x,y);
}
void Particle(Board* b,float x,float y,ParticleEffect effect)
{
    b->mApp->AddTodParticle(x,y,RENDER_LAYER_TOP,effect);
    LawnNet::NetBroadcastParticle(b->mApp,x,y,RENDER_LAYER_TOP,effect);
}
bool Area(Plant* p,Zombie* z,double width)
{
    if(!Target(z)||z->mBoard!=p->mBoard||z->mRow!=p->mRow)return false;
    float x=0,y=0;Ground(p,x,y);const auto r=z->GetZombieRect();
    return r.mX<=x+width*.5*PX&&r.mX+r.mWidth>=x-width*.5*PX&&
        r.mY<=y+width*.5*PY&&r.mY+r.mHeight>=y-width*.5*PY;
}
void KnockBack(Zombie* z,double distance)
{
    bool source313=false;if(NativeZombie313StatusTarget(z,FusionExtendedEffect313::Knockback,source313)&&!source313)return;
    float x=0,y=0;if(!GetNativeBaseZombieGroundAnchor(z,x,y))return;
    auto* b=z->mBoard;
    const double origin=b->GetCellCenterX(0,z->mRow);
    const double sourceX=(x-origin)/PX-4.8;
    // Zombie.Awake copies Board.zombieMaxX, not the plant vision boundary.
    const double right=1.35*(b->GetColumnCount()-1)-4.8+4.0;
    double moved=sourceX;
    if(!TryGetFusionFireKnockBack(sourceX,right,distance,moved))return;
    z->mPosX+=static_cast<float>((moved-sourceX)*PX);z->mX=static_cast<int>(z->mPosX);
}
bool SpawnRight(Plant* p,uint16_t kind,int first)
{
    auto provenance=*GetNativePlantData(p);provenance.kind=kind;
    for(int c=first;c<p->mBoard->GetColumnCount();++c)
        if(TrySpawnNativeFusionPlant(p->mBoard,c,p->mRow,provenance))return true;
    return false;
}
Projectile* Emit(Plant* p,uint16_t kind,int damage,int row,uint8_t special,
    float x,float y,uint8_t fire=0)
{
    auto* b=p->mBoard;
    NativeProjectileData data;data.kind=kind;data.damage=damage;data.specialType=special;data.fireState=fire;
    if(fire){data.lastTorchId=p->mNetId;data.lastTorchToken=GetNativePlantData(p)->instanceToken;data.effectVariant=kind==0?1:0;}
    const auto type=fire?PROJECTILE_FIREBALL:PROJECTILE_PEA;
    const auto motion=special==3||special==4?MOTION_THREEPEATER:MOTION_STRAIGHT;
    if(!NativeProjectileMetadataValid(data,type,motion))return nullptr;
    auto* shot=b->AddProjectile(static_cast<int>(x-20),static_cast<int>(y-20),p->mRenderOrder+1,row,type);
    if(!shot)return nullptr;
    shot->mPosX=x-20;shot->mPosY=y-20;shot->mX=static_cast<int>(shot->mPosX);shot->mY=static_cast<int>(shot->mPosY);
    shot->mVelX=3.2f;shot->mVelY=0;shot->mPosZ=0;shot->mShadowY=y;
    shot->mMotionType=motion;
    if(!shot->mNetId)shot->mNetId=b->NetAllocId();
    if(!shot->mNetId||!AttachNativeProjectile(b,shot,data)){shot->Die();return nullptr;}
    return shot;
}
bool EmitThree(Plant* p,uint16_t kind,int damage,bool hot)
{
    if(!Playing(p)||p->mDead||!p->mNetId||p->mBoard->mProjectiles.mSize+3>p->mBoard->mProjectiles.mMaxSize||
        !EnsureNativeProjectileVisuals(p->mBoard,kind))return false;
    float x=0,y=0;Ground(p,x,y);
    // Native-art muzzle adaptation: reuse this actor's native shadow and the
    // established Threepeater head height, not an editor prefab root position.
    y-=static_cast<float>(.8*PY);
    struct Shot { int row;uint8_t phase;float x; };
    Shot specs[3]={{p->mRow,0,x},
        {p->mRow? p->mRow-1:p->mRow,static_cast<uint8_t>(p->mRow?3:0),x},
        {p->mRow+1<kNativeFusionRows?p->mRow+1:p->mRow,static_cast<uint8_t>(p->mRow+1<kNativeFusionRows?4:0),x}};
    // Source ThreeTorch emits edge-lane extras immediately at X+.5. ThreeSpike
    // instead queues its one missing edge shot for.2s in the native field.
    if(hot)
    {
        if(!p->mRow)specs[1].x+=static_cast<float>(.5*PX);
        if(p->mRow+1==kNativeFusionRows)specs[2].x+=static_cast<float>(.5*PX);
    }
    std::vector<Projectile*> staged;
    for(int i=0;i<3;++i)
    {
        if(!hot&&i&&specs[i].phase==0){p->mDoSpecialCountdown=21;continue;}
        auto* shot=Emit(p,kind,damage,specs[i].row,specs[i].phase,specs[i].x,y,hot?1:0);
        if(!shot){for(auto* old:staged)old->Die();return false;}staged.push_back(shot);
    }
    LawnNet::NetTrace("Fusion native FireAction three plant=%u kind=%u payload=%u count=%zu hot=%d tick=%d",
        p->mNetId,GetNativePlantData(p)->kind,kind,staged.size(),hot?1:0,p->mBoard->mMainCounter);
    return true;
}
void EmitSquash(Plant* p)
{
    if(p->mBoard->mProjectiles.mSize>=p->mBoard->mProjectiles.mMaxSize||!EnsureNativeProjectileVisuals(p->mBoard,28))return;
    NativeProjectileData admission;admission.kind=28;admission.damage=40;
    if(!NativeProjectileMetadataValid(admission,PROJECTILE_PEA,MOTION_STRAIGHT))return;
    float x=0,y=0;Ground(p,x,y);y-=static_cast<float>(.5*PY);
    // Resource/capacity admission precedes the source two velocity draws.
    const float vx=RandRangeFloat(1.5f,1.9f)*.8f,vy=RandRangeFloat(4.8f,5.2f)*.8f;
    auto* shot=Emit(p,28,40,p->mRow,0,x,y);if(!shot)return;
    shot->mVelX=vx*static_cast<float>(PX*.01);
    shot->mVelZ=-vy*static_cast<float>(PY*.01);
    shot->mAccZ=static_cast<float>(9.8*PY*.0001);
    shot->mShadowY=y+static_cast<float>(SquashShadowY*PY);
    LawnNet::NetTrace("Fusion native FireAction squash born plant=%u projectile=%u vx=%.9g vz=%.9g acc=%.9g tick=%d",
        p->mNetId,shot->mNetId,shot->mVelX,shot->mVelZ,shot->mAccZ,p->mBoard->mMainCounter);
}
void Burst(Board* b,float x,float y,int row,int damage,double radius,bool explosive=true,bool sameRow=false)
{
    std::vector<ZombieID> ids;Zombie* z=nullptr;
    while(b->IterateZombies(z))
    {
        if(!Target(z)||(sameRow?z->mRow!=row:std::abs(z->mRow-row)>1))continue;const auto r=z->GetZombieRect();
        if(FusionFireCircleTouchesRect(x,y,radius,r.mX,r.mY,r.mWidth,r.mHeight))ids.push_back(b->ZombieGetID(z));
    }
    for(auto id:ids)if((z=b->ZombieTryToGet(id))&&Target(z))
    {if(explosive)ApplyNativeFusionPotatoDamage(z,damage);else z->TakeDamage(damage,2U);}
    Particle(b,x,y,PARTICLE_POTATO_MINE);
}
bool AuxTarget(Plant* p)
{
    float x=0,y=0;Ground(p,x,y);const double max=p->mBoard->GetCellCenterX(p->mBoard->GetColumnCount()-1,p->mRow)+1.85*PX;
    Zombie* z=nullptr;
    while(p->mBoard->IterateZombies(z))
    {
        float zx=0,zy=0;
        if(Target(z)&&std::abs(z->mRow-p->mRow)<=1&&GetNativeBaseZombieGroundAnchor(z,zx,zy)&&zx>x&&zx<max)return true;
    }
    return false;
}
}

bool IsNativeFireAction(const Plant* p){return Profile(p)!=nullptr;}
void InitializeNativeFireAction(Plant* p)
{
    if(IsNativeShooter267(p)){InitializeNativeShooter267(p);return;}
    const auto* profile=Profile(p);if(!profile||!Host(p))return;
    auto* d=GetNativePlantData(p);p->mPlantMaxHealth=p->mPlantHealth=profile->maximumHealth;
    p->mIsAsleep=false;p->mWakeUpCounter=0;p->mDoSpecialCountdown=0;p->mDisappearCountdown=0;
    p->mTargetX=p->mTargetY=0;
    if(profile->family==FusionFireActionFamily::Caltrop){InitializeNativeBatchAction(p);return;}
    if(profile->family==FusionFireActionFamily::BloverShell)InitializeNativeDefender(p);
    d->animationSpeed=RandRangeFloat(.9f,1.1f);
    p->mLaunchCounter=0;p->mStateCountdown=0;p->mState=STATE_NOTREADY;d->shootSeconds=-1;
    if(profile->family==FusionFireActionFamily::Blover)
    {p->mDoSpecialCountdown=Ticks(static_cast<double>(.48333332f),d->animationSpeed);d->shootSeconds=0;}
    else if(profile->family==FusionFireActionFamily::FireGround)
        p->mLaunchCounter=Ticks(static_cast<double>(RandRangeFloat(.2f,.8f)),1);
    else if(profile->family==FusionFireActionFamily::Obsidian)p->mLaunchCounter=100;
    else if(profile->family==FusionFireActionFamily::CherryRunner)
    {p->mStateCountdown=100;p->mTargetX=p->mX;}
    else if(profile->family==FusionFireActionFamily::BloverShell)p->mLaunchCounter=3000;
    LawnNet::NetTrace("Fusion native FireAction init id=%u token=%u kind=%u hp=%d launch=%d special=%d speed=%.9g",
        p->mNetId,d->instanceToken,d->kind,p->mPlantHealth,p->mLaunchCounter,p->mDoSpecialCountdown,d->animationSpeed);
}
void UpdateNativeFireLight(Board* b)
{
    auto* c=GetNativeContext(b);if(!c||!b->mApp||b->mApp->NetIsClient()||b->mPaused)return;
    std::erase_if(c->temporaryLightDeadlines,[&](int until){return until<=b->mMainCounter;});
    c->temporaryLightLevel=static_cast<uint16_t>(c->temporaryLightDeadlines.size());
}
bool AddNativeFireLight(Board* b,int ticks)
{
    auto* c=GetNativeContext(b);
    if(!c||!b->mApp||b->mApp->NetIsClient()||b->mPaused||ticks<=0||
        b->mMainCounter>(std::numeric_limits<int>::max)()-ticks||c->temporaryLightDeadlines.size()>=65535)return false;
    c->temporaryLightDeadlines.push_back(b->mMainCounter+ticks);
    c->temporaryLightLevel=static_cast<uint16_t>(c->temporaryLightDeadlines.size());
    LawnNet::NetTrace("Fusion native FireAction light count=%u until=%d tick=%d",c->temporaryLightLevel,b->mMainCounter+ticks,b->mMainCounter);
    return true;
}
int GetNativeFireLightLevel(const Plant* p)
{
    if(!Current(p))return 0;const auto* c=GetNativeContext(p->mBoard);if(!c)return 0;
    int level=c->temporaryLightLevel+GetNativeAction266LightContribution(p)+GetNativeUmbrella267LightContribution(p);
    Plant* other=nullptr;
    while(p->mBoard->IteratePlants(other))
    {
        const auto* d=GetNativePlantData(other);
        const int radius=d&&d->kind==20?1:NativeSeaLantern267LightRadius(other);
        if(radius>0&&!other->mDead&&other->mIsOnBoard&&other->mPlantHealth>0&&
            std::abs(other->mPlantCol-p->mPlantCol)<=radius&&std::abs(other->mRow-p->mRow)<=radius)++level;
    }
    return level;
}
bool EmitNativeFireThree(Plant* p){return EmitThree(p,0,30,true);}
void NativeFireConversionCommitted(Plant* p)
{
    NativeCherryTorch269ConversionCommitted(p);
    const auto* d=GetNativePlantData(p);if(!d||!Playing(p)||(d->kind!=1059&&d->kind!=1061&&d->kind!=902))return;
    if(d->kind==902)
    {
        if(p->mTargetX<(std::numeric_limits<int>::max)())++p->mTargetX;
        GetNativePlantData(p)->batchShotCount=static_cast<uint32_t>(p->mTargetX);
        if(p->mTargetX>20&&SpawnRight(p,1054,p->mPlantCol+1))p->mTargetX=0;
        return;
    }
    const int before=p->mTargetX;int after=before;bool attempt=false;
    if(!TryAdvanceFusionTorchCount(d->kind,before,false,after,attempt))return;
    p->mTargetX=after;
    if(attempt)
    {
        const bool made=SpawnRight(p,d->kind==1059?1057:1155,p->mPlantCol+1);
        if(d->kind==1059&&made)p->mTargetX=0;
        LawnNet::NetTrace("Fusion native FireAction spawn plant=%u kind=%u count=%d success=%d tick=%d",p->mNetId,d->kind,before+1,made?1:0,p->mBoard->mMainCounter);
    }
}
bool UpdateNativeFireAction(Plant* p)
{
    if(IsNativeShooter267(p))return UpdateNativeShooter267(p);
    const auto* profile=Profile(p);if(!profile)return false;
    const bool caltrop=profile->family==FusionFireActionFamily::Caltrop;
    if(!Playing(p)||p->mDead)return !caltrop;
    auto* d=GetNativePlantData(p);
    if(caltrop)
    {
        if(d->kind!=1064)return false;
        if(p->mTargetY>0)
        {
            --p->mTargetY;
            const int full=Ticks(1.0,d->animationSpeed)+1,event=Ticks(.5,d->animationSpeed);
            if(p->mTargetY==full-event)EmitThree(p,12,5,false);
            if(!p->mTargetY)p->mState=STATE_NOTREADY;
        }
        if(--p->mTargetX<0)
        {
            p->mTargetX=static_cast<int>(std::floor(static_cast<double>(1.5f+RandRangeFloat(-.1f,.1f))*100));
            if(AuxTarget(p)){p->mState=STATE_READY;p->mTargetY=Ticks(1.0,d->animationSpeed)+1;}
        }
        return false;
    }
    if(FusionFireTorchKind(d->kind))HandleNativeTorchwood(p);
    if(d->kind==1052)
    {
        if(p->mLaunchCounter>0)--p->mLaunchCounter;
        else
        {
            p->mLaunchCounter=150;std::vector<ZombieID> ids;Zombie* z=nullptr;
            while(p->mBoard->IterateZombies(z))if(Area(p,z,4))ids.push_back(p->mBoard->ZombieGetID(z));
            for(auto id:ids)if((z=p->mBoard->ZombieTryToGet(id))&&Target(z)){z->TakeDamage(40,2U);NativeWarmAfterDamage(z);}
            if(!ids.empty())LawnNet::NetTrace("Fusion native FireAction aura plant=%u hits=%zu damage=40 tick=%d",p->mNetId,ids.size(),p->mBoard->mMainCounter);
        }
    }
    else if(profile->family==FusionFireActionFamily::FireGround)
    {
        if(p->mLaunchCounter>0&&!--p->mLaunchCounter)
        {
            p->mLaunchCounter=100;Zombie* z=nullptr;
            while(p->mBoard->IterateZombies(z))if(Area(p,z,1)){z->TakeDamage(20,2U);NativeWarmAfterDamage(z);}
        }
    }
    else if(profile->family==FusionFireActionFamily::Blover)
    {if(p->mState==STATE_DOINGSPECIAL&&!p->mStateCountdown)p->Die();}
    else if(profile->family==FusionFireActionFamily::BloverShell)
    {if(!--p->mLaunchCounter){SpawnRight(p,22,p->mPlantCol);p->mLaunchCounter=3000;}}
    else if(profile->family==FusionFireActionFamily::Obsidian)
    {
        if(!--p->mLaunchCounter)
        {
            p->mLaunchCounter=100;p->mPlantHealth-=p->mPlantMaxHealth/5;
            p->mEatenFlashCountdown=(std::max)(p->mEatenFlashCountdown,25);
            LawnNet::NetTrace("Fusion native FireAction obsidian plant=%u hp=%d tick=%d",p->mNetId,p->mPlantHealth,p->mBoard->mMainCounter);
        }
        if(p->mPlantHealth>0&&p->mPlantHealth<p->mPlantMaxHealth/5&&p->mState!=STATE_DOINGSPECIAL)
        {
            p->mState=STATE_DOINGSPECIAL;
            p->mDoSpecialCountdown=Ticks(static_cast<double>(.6666667f)/static_cast<double>(.8f),d->animationSpeed)+1;
            p->mDisappearCountdown=p->mDoSpecialCountdown+1;
        }
    }
    else if(profile->family==FusionFireActionFamily::CherryRunner)
    {
        if(p->mState!=STATE_DOINGSPECIAL&&!p->mStateCountdown)
        {
            p->mState=STATE_DOINGSPECIAL;
            // The runner dies at the source right boundary. Keep the vanilla
            // STATE_DOINGSPECIAL cleanup from retiring it on the next update.
            p->mDisappearCountdown=2000;
        }
        if(p->mState==STATE_DOINGSPECIAL)
        {
            ++p->mLaunchCounter;p->mX=p->mTargetX+static_cast<int>(p->mLaunchCounter*.1*PX);
            const double limit=p->mBoard->GetCellCenterX(p->mBoard->GetColumnCount()-1,p->mRow)+1.85*PX;
            float x=0,y=0;Ground(p,x,y);if(x>limit){p->Die();return true;}
            if((p->mBoard->mMainCounter&1)==0)
            {
                std::vector<ZombieID> ids;Zombie* z=nullptr;const auto body=p->GetPlantRect();
                while(p->mBoard->IterateZombies(z))if(Target(z)&&z->mRow==p->mRow&&body.Intersects(z->GetZombieRect()))ids.push_back(p->mBoard->ZombieGetID(z));
                for(auto id:ids)if((z=p->mBoard->ZombieTryToGet(id))&&Target(z))
                {
                    float zx=0,zy=0;GetNativeBaseZombieGroundAnchor(z,zx,zy);
                    Burst(p->mBoard,zx,zy,p->mRow,300,1.5);
                    HealNativeCherryNutFromCircle(p->mBoard,zx,zy,p->mRow,100,1.5);
                    KnockBack(z,.8);
                }
            }
        }
    }
    return true;
}
bool ApplyNativeFireActionSpecial(Plant* p)
{
    const auto* profile=Profile(p);if(!profile)return false;
    if(!Playing(p)||p->mDead)return true;
    const auto* d=GetNativePlantData(p);
    if(profile->family==FusionFireActionFamily::Blover)
    {
        if(p->mState==STATE_DOINGSPECIAL)return true;
        p->mState=STATE_DOINGSPECIAL;p->mStateCountdown=200;p->mDisappearCountdown=201;
        Zombie* z=nullptr;
        while(p->mBoard->IterateZombies(z))
            if(!z->mDead&&!z->IsDeadOrDying()&&!z->mMindControlled&&z->IsFlying())z->mBlowingAway=true;
        // The existing native flying actor owns its blow-away motion. No source
        // Flying NPC AI or water/fog background is admitted by this day slice.
        p->mBoard->mFogBlownCountDown=4000;p->mApp->PlaySample(Sexy::SOUND_BLOVER);
        if(d->kind==1080)AddNativeFireLight(p->mBoard);
        LawnNet::NetTrace("Fusion native FireAction blow plant=%u kind=%u disappear=200 tick=%d",p->mNetId,d->kind,p->mBoard->mMainCounter);
        return true;
    }
    if(d->kind==1064)
    {
        float x=0,y=0;Ground(p,x,y);y-=static_cast<float>(.8*PY);
        if(p->mBoard->mProjectiles.mSize<p->mBoard->mProjectiles.mMaxSize&&EnsureNativeProjectileVisuals(p->mBoard,12))
            Emit(p,12,5,p->mRow,0,x,y);
        return true;
    }
    if(profile->family==FusionFireActionFamily::Obsidian){p->Die();return true;}
    return false;
}
void UpdateNativeFireActionPhase(Plant* p)
{
    if(IsNativeShooter267(p)){UpdateNativeShooter267Phase(p);return;}
    const auto* profile=Profile(p);if(!profile||!Host(p)||p->mDead)return;
    auto* d=GetNativePlantData(p);
    // Caltrop's shared phase already advances this same native idle clock.
    if(profile->family!=FusionFireActionFamily::Caltrop)d->idleSeconds+=.01f;
    if(profile->family==FusionFireActionFamily::Blover)
    {
        const int event=Ticks(static_cast<double>(.48333332f),d->animationSpeed);
        d->shootSeconds=p->mState==STATE_DOINGSPECIAL?
            .48333332f/d->animationSpeed+(200-p->mStateCountdown)*.01f:
            (event-p->mDoSpecialCountdown)*.01f;
    }
    else if(d->kind==1064&&p->mState==STATE_READY)
        d->shootSeconds=(Ticks(1.0,d->animationSpeed)+1-p->mTargetY)*.01f;
    else if(profile->family==FusionFireActionFamily::CherryRunner)
        d->shootSeconds=p->mState==STATE_DOINGSPECIAL?p->mLaunchCounter*.01f:-1;
    else if(profile->family==FusionFireActionFamily::Obsidian)
        d->shootSeconds=p->mState==STATE_DOINGSPECIAL?
            (Ticks(static_cast<double>(.6666667f)/static_cast<double>(.8f),d->animationSpeed)-p->mDoSpecialCountdown)*.01f:-1;
}
void NativeFireActionPlantRemoving(Plant* p,int reason)
{
    NativeShooter267PlantRemoving(p,reason);
    const auto* profile=Profile(p);
    if(!profile||!Host(p)||!FusionDeathReasonHasEffects(reason))return;
    const auto kind=GetNativePlantData(p)->kind;
    if(kind==1052||kind==1053)ApplyNativeBatchFireRow(p->mBoard,p->mRow,1800,false);
    else if(kind==1173)
    {
        float x=0,y=0;Ground(p,x,y);
        ApplyNativeBatchFireRow(p->mBoard,p->mRow,1800,true);
        ApplyNativeColdBoardFreeze(p->mBoard,x,y);
    }
}
bool NativeFireActionUntargetable(const Plant* p)
{
    const auto* d=GetNativePlantData(p);
    return Profile(p)&&d->kind==1179&&p->mState==STATE_DOINGSPECIAL;
}
bool NativeFireActionInvincible(const Plant* p)
{ const auto* d=GetNativePlantData(p);return Profile(p)&&FusionFireActionInvincible(d->kind); }
void RestoreNativeFireActionPresentation(Plant* p)
{
    const auto* d=GetNativePlantData(p);
    // RemoveFromList means no source grid occupancy. The native actor must
    // remain on board to receive UpdateAbilities and authoritative snapshots;
    // NativeFireActionUntargetable excludes it from grid/target queries.
    if(Profile(p)&&d->kind==1179)p->mIsOnBoard=true;
}
void NativeFireActionHealthChanged(Plant* p)
{
    const auto* d=GetNativePlantData(p);if(!d||d->kind!=1091||!Host(p)||p->mDead)return;
    auto* state=GetNativePlantData(p);uint8_t next=state->damageStage;
    if(TryGetFusionDefenderDamageStage(p->mPlantHealth,p->mPlantMaxHealth,next,next))state->damageStage=next;
}
bool GetNativeFireActionVisualPose(const Plant* p,const char*& state,float& elapsed)
{
    if(IsNativeShooter267(p))return GetNativeShooter267VisualPose(p,state,elapsed);
    const auto* profile=Profile(p);if(!profile)return false;
    const auto* d=GetNativePlantData(p);state="idle";elapsed=d->idleSeconds;
    if(profile->family==FusionFireActionFamily::Caltrop&&d->kind!=1064)return false;
    if(profile->family==FusionFireActionFamily::Blover)
    {
        const float duration=(d->kind==1080?.7f:.68333334f)/d->animationSpeed;
        if(d->shootSeconds<duration){state="blow";elapsed=(std::max)(d->shootSeconds,0.0f);}
        else{state="blowloop";elapsed=d->shootSeconds-duration;}
    }
    else if(d->kind==1064)
    {
        if(p->mState!=STATE_READY)return false;state="summon";elapsed=(std::max)(d->shootSeconds,0.0f);
    }
    else if(d->kind==1053)state="idle_advanced";
    else if(d->kind==1059)state="idle_squash";
    else if(d->kind==1173&&p->mState==STATE_DOINGSPECIAL){state="explode";elapsed=(std::max)(d->shootSeconds,0.0f);}
    else if(d->kind==1179&&p->mState==STATE_DOINGSPECIAL){state="fire";elapsed=(std::max)(d->shootSeconds,0.0f);}
    return std::isfinite(elapsed)&&elapsed>=0;
}
bool ApplyNativeFireCaltropStrike(Plant* p)
{
    const auto* d=GetNativePlantData(p);
    if(!d||(d->kind!=1062&&d->kind!=1063))return false;
    if(!Playing(p)||p->mDead)return true;
    std::vector<ZombieID> ids;Zombie* z=nullptr;
    while(p->mBoard->IterateZombies(z))if(Area(p,z,1))ids.push_back(p->mBoard->ZombieGetID(z));
    for(auto id:ids)if((z=p->mBoard->ZombieTryToGet(id))&&Target(z))
    {
        z->TakeDamage(20,1U);
        if(d->kind==1062)NativeWarmAfterDamage(z);
        else KnockBack(z,.2);
    }
    if(d->kind==1063&&!ids.empty())EmitSquash(p);
    LawnNet::NetTrace("Fusion native FireAction caltrop plant=%u kind=%u hits=%zu damage=20 tick=%d",p->mNetId,d->kind,ids.size(),p->mBoard->mMainCounter);
    return true;
}
bool IsNativeFireActionProjectile(const Projectile* p)
{
    const auto* extended=GetNativeProjectileData(p);
    if(extended&&extended->kind==28&&IsNativeMore269Projectile(p))return true;
    if(extended&&(extended->kind==36||extended->kind==28&&(extended->specialType==4||extended->specialType==5||extended->specialType==255)))
        return NativeProjectileMetadataValid(*extended,p->mProjectileType,p->mMotionType);
    const auto* d=GetNativeProjectileData(p);
    return Current(p)&&d&&FusionFireActionProjectileMetadataValid(d->kind,d->damage,d->sunOnHit,
        d->specialType,d->fireState,p->mProjectileType,p->mMotionType)&&!d->lastTorchId&&!d->lastTorchToken;
}
bool GetNativeFireActionProjectileCenter(const Projectile* p,float& x,float& y)
{
    if(!IsNativeFireActionProjectile(p))return false;
    x=p->mPosX+20;y=p->mPosY+p->mPosZ+20;
    return std::isfinite(x)&&std::isfinite(y);
}
bool GetNativeFireActionProjectileRect(const Projectile* p,Sexy::Rect& result)
{
    if(!IsNativeFireActionProjectile(p))return false;
    const auto kind=GetNativeProjectileData(p)->kind;
    float x=0,y=0;if(!GetNativeFireActionProjectileCenter(p,x,y))return false;
    const double scale=kind==12?.3:SquashScale;
    const double ox=(kind==12?.7128571:-.062675)*scale;
    const double oy=(kind==12?1.6495185:kind==36?2.3980184:1.7461257)*scale;
    const double w=(kind==12?2.0941527:3.4891138)*scale*PX;
    const double py=GetNativeFusionPixelsY(p->mBoard);
    const double h=(kind==12?1.1823587:kind==36?14.196761:13.317838)*scale*py;
    result=Sexy::Rect(static_cast<int>(std::floor(x+ox*PX-w*.5)),static_cast<int>(std::floor(y-oy*py-h*.5)),
        static_cast<int>(std::ceil(w)),static_cast<int>(std::ceil(h)));
    return true;
}
bool MoveNativeFireActionProjectile(Projectile* p)
{
    if(!IsNativeFireActionProjectile(p))return false;
    if(!Playing(p)||p->mDead)return true;
    const auto* d=GetNativeProjectileData(p);auto* b=p->mBoard;
    const double py=GetNativeFusionPixelsY(b);
    const double pitch=GetNativeFusionRowCount(b)==6?85.:100.;
    const double sx=(p->mPosX+20-b->GetCellCenterX(0,p->mRow))/PX-4.8;
    const double sy=2.3-(p->mPosY+20-(b->GridToPixelY(0,0)+50))/py;
    if(p->mProjectileAge>2000||sx>1.35*(b->GetColumnCount()-1)-4.8+1.85+3.85||sx< -8.65||
        sy<2.3-pitch/py*(GetNativeFusionRowCount(b)-1)-.6-2.85||sy>9.35){p->Die();return true;}
    p->mPosX+=p->mVelX;
    if(d->kind==12)
    {
        float dy=0;if(d->specialType)TryGetFusionThreeVerticalStep(d->specialType,(std::max)(p->mProjectileAge-1,0),dy);
        dy=static_cast<float>(dy*pitch/100.);
        p->mVelY=dy;p->mPosY+=dy;p->mShadowY+=dy;
    }
    else if(d->specialType==4||d->specialType==5)
    {
        float dy=0;
        TryGetFusionShooter267ThreeVerticalStep(d->specialType,(std::max)(p->mProjectileAge-1,0),dy,pitch);
        p->mVelY=dy;p->mPosY+=dy;p->mShadowY+=dy;
    }
    else if(d->specialType==1||d->specialType==255||(d->kind==28&&d->specialType==0))
    {
        float z=p->mPosZ,v=p->mVelZ;bool bounce=false;
        const float ground=static_cast<float>((SquashImageY+SquashShadowY-.15)*py);
        if(!TryStepFusionSquashProjectile(p->mPosZ,p->mVelZ,p->mAccZ,ground,z,v,bounce)){p->Die();return true;}
        p->mPosZ=z;p->mVelZ=v;
        if(bounce)
        {
            if(d->kind==36)
            {
                // CherrySquashBullet overrides only the landing callback;
                // BombCherry defaults its own1800, radius2 and source row.
                Burst(p->mBoard,p->mPosX+20,p->mPosY+20,p->mRow,1800,2,true);
                HealNativeCherryNutFromCircle(p->mBoard,p->mPosX+20,p->mPosY+20,p->mRow,100,2);
            }
            else Burst(p->mBoard,p->mPosX+20,p->mPosY+20,p->mRow,d->damage,.5,false,true);
            LawnNet::NetTrace("Fusion native Squash267 landing id=%u payload=%u damage=%d row=%d tick=%d",p->mNetId,d->kind,d->damage,p->mRow,p->mBoard->mMainCounter);
        }
    }
    return true;
}
bool HandleNativeFireActionProjectileCollision(Projectile* p)
{
    if(!IsNativeFireActionProjectile(p))return false;
    if(!Playing(p)||p->mDead)return true;
    const auto* d=GetNativeProjectileData(p);if((d->kind==28||d->kind==36)&&(d->specialType==1||d->specialType==255))return true;
    Sexy::Rect rect;if(!GetNativeFireActionProjectileRect(p,rect))return true;
    auto* c=GetNativeContext(p->mBoard);if(!c)return true;
    const auto generation=p->mBoard->mProjectiles.DataArrayGetID(p);
    std::unordered_set<uint32_t> touching;
    Zombie* z=nullptr;
    while(p->mBoard->IterateZombies(z))
    {
        if(!Target(z)||z->mRow!=p->mRow||!rect.Intersects(z->GetZombieRect()))continue;
        const auto target=p->mBoard->mZombies.DataArrayGetID(z);touching.insert(target);
        const auto previous=c->fireContacts.find(generation);
        if(previous!=c->fireContacts.end()&&previous->second.contains(target))continue;
        p->DoImpact(z);
        if(p->mDead||d->kind==28||d->kind==36)break;
    }
    // Only current contact episodes are retained: exiting and entering again
    // is a new Unity OnTriggerEnter, unlike the rolling lifetime hit history.
    if(!p->mDead)c->fireContacts[generation]=std::move(touching);
    return true;
}
bool ApplyNativeFireActionProjectileImpact(Projectile* p,Zombie* z)
{
    if(!IsNativeFireActionProjectile(p))return false;
    if(!Playing(p)||p->mDead||!Target(z)||z->mBoard!=p->mBoard||z->mRow!=p->mRow)return true;
    auto* d=GetNativeProjectileData(p);p->PlayImpactSound(z);
    if(d->kind==12)z->TakeDamage(5,1U);
    else if(d->specialType!=1&&d->specialType!=255)
    {
        z->TakeDamage(d->damage,0U);
        const bool extended=d->kind==36||d->specialType==4||d->specialType==5||p->mRotationSpeed>0;
        d->specialType=extended?255:1;p->mMotionType=MOTION_LOBBED;p->mVelZ*= -.75f;
        if(extended){p->mVelX=p->mRotationSpeed;p->mVelY=0;}
    }
    LawnNet::NetTrace("Fusion native FireAction impact projectile=%u kind=%u target=%u phase=%u damage=%d tick=%d",
        p->mNetId,d->kind,z->mNetId,d->specialType,d->damage,p->mBoard->mMainCounter);
    return true;
}
}
