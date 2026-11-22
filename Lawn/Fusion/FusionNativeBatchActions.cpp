#include "FusionNativeBatchActions.h"
#include "FusionNativeExtras268.h"
#include "FusionNativeSummonPlants269.h"
#include "FusionNativeLastPlants269.h"
#include "FusionNative.h"
#include "FusionNativeZombie313Dispatch.h"
#include "FusionNativeProjectileMetadata.h"
#include "FusionNativeShooterTiming.h"
#include "FusionNativePlacement.h"
#include "FusionNativeBasePlants.h"
#include "FusionNativeFire.h"
#include "FusionNativeCold.h"
#include "FusionNativeWater268.h"
#include "FusionNativeShooterBatch.h"
#include "FusionNativeFireActionBatch.h"
#include "FusionNativeAction266.h"
#include "NativeSpecialZombie269.h"
#include "FusionNativeZombieVisual.h"
#include "FusionStraightProjectileRules.h"
#include "FusionNativeDefender.h"
#include "FusionBasePlantRules.h"
#include "FusionCherryShooterRules.h"
#include "../Board.h"
#include "../GridItem.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../Net/NetCommon.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace LawnFusion
{
namespace
{
bool Host(const GameObject* object)
{
    return object && object->mApp && object->mBoard && object->mApp->mBoard == object->mBoard &&
        !object->mNetGhost && !object->mApp->NetIsClient() && IsNativeFusionBoard(object->mBoard) &&
        NativeFusionBackgroundValid(static_cast<uint8_t>(object->mBoard->mBackground)) && !object->mBoard->mPaused &&
        object->mApp->mGameScene == SCENE_PLAYING;
}
bool Normal(Zombie* zombie)
{
    return Host(zombie) && !zombie->mDead && !zombie->IsDeadOrDying() &&
        IsNativeFusionGroundZombie(zombie) && !zombie->mMindControlled &&
        !zombie->IsFlying() && (!GetNativeSourceZombieData(zombie) || !zombie->mOnHighGround) &&
        NativeFusionGroundTerrainAllowed(static_cast<uint8_t>(zombie->mBoard->mBackground),zombie->mRow,zombie->mInPool);
}
bool Ground(Plant* plant,float& x,float& y) { return plant && plant->GetShadowCenter(x,y); }
bool Touches(Plant* plant,Zombie* zombie,bool attack)
{
    float x=0,y=0; FusionBatchArea area;
    const auto* data=GetNativePlantData(plant);
    if (!data || !Ground(plant,x,y) || !TryMakeFusionBatchArea(data->kind,x,y,attack,area)) return false;
    const auto rect=zombie->GetZombieRect();
    return FusionBatchAreaTouchesRect(area,rect.mX,rect.mY,rect.mWidth,rect.mHeight);
}
void Play(Plant* plant,const char* state,int ticks,bool loop=false)
{
    auto* body=plant->mApp->ReanimationTryToGet(plant->mBodyReanimID);
    if (!body || !body->TrackExists(state)) return;
    int start=0,count=0; body->GetFramesForLayer(state,start,count);
    plant->PlayBodyReanim(state,loop?REANIM_LOOP:REANIM_PLAY_ONCE_AND_HOLD,0,
        (std::max)(count,1)*100.0f/(std::max)(ticks,1));
}
const FusionBatchActionProfile* Profile(const Plant* plant)
{
    const auto* data=GetNativePlantData(plant);
    return data?FindFusionBatchActionProfile(data->kind):nullptr;
}
const FusionShooterTimingProfile* Timing(const Plant* plant)
{
    const auto* data=GetNativePlantData(plant);
    return data?FindFusionBatchTimingProfile(data->kind,data->kind==1025 && plant->mState==STATE_KERNELPULT_BUTTER):nullptr;
}
void Particle(Board* board,float x,float y,ParticleEffect effect)
{
    board->mApp->AddTodParticle(x,y,static_cast<int>(RENDER_LAYER_TOP),effect);
    LawnNet::NetBroadcastParticle(board->mApp,x,y,static_cast<int>(RENDER_LAYER_TOP),effect);
}
void FireRow(Board* board,int row,int damage=1800,bool fix=false)
{
    if (row<0 || row>=GetNativeFusionRowCount(board) || damage!=1800) return;
    NativeDefenderElementPulse(board,FusionDefenderElement::Fire,row);
    NativeMachine268Element(board,row,2);
    // Admission is made once by the source event. Last-kill awards do not cancel
    // its remaining victims, ladder cleanup, or adjacent source continuations.
    std::vector<ZombieID> victims; Zombie* zombie=nullptr;
    while (board->IterateZombies(zombie))
        if (Normal(zombie) && zombie->mRow==row) victims.push_back(board->ZombieGetID(zombie));
    for (auto id:victims) if ((zombie=board->ZombieTryToGet(id)) && Normal(zombie))
    { NativeWarmAfterDamage(zombie); ApplyNativeFusionCherryDamage(zombie); }
    GridItem* item=nullptr;
    while (board->IterateGridItems(item))
        if (!item->mDead && item->mGridItemType==GRIDITEM_LADDER && item->mGridY==row) item->GridItemDie();
    board->DoFwoosh(row);
    LawnNet::NetBroadcastFwoosh(board->mApp,row);
    board->mIceTimer[row]=(std::min)(board->mIceTimer[row],20);
    board->mApp->PlayFoley(FOLEY_JALAPENO_IGNITE);
    // Source fix reaches Charred's fallback TakeDamage. In bounded sourceD2,
    // it changes no multiplier; the existing1800 Charred helper is identical.
    LawnNet::NetTrace("Fusion native FireRow row=%d damage=%d fix=%d tick=%d",row,damage,fix?1:0,board->mMainCounter);
}
Zombie* Acquire(Plant* plant,bool close)
{
    float px=0,py=0; if (!Ground(plant,px,py)) return nullptr;
    const auto kind=GetNativePlantData(plant)->kind;
    const double vision=plant->mBoard->GetCellCenterX(plant->mBoard->GetColumnCount()-1,plant->mRow)+1.85*kFusionBasePixelsX;
    Zombie* zombie=nullptr; Zombie* nearest=nullptr; float nearestX=0;
    while (plant->mBoard->IterateZombies(zombie))
    {
        float x=0,y=0;
        if (!Normal(zombie) || !zombie->GetShadowCenter(x,y) ||
            !(IsFusionAction266Gloom(kind)?NativeAction266GloomAcquire(plant,zombie):
            FusionBatchShooterCanAcquire(kind,GetNativeFusionZombieStatus(zombie),zombie->mRow-plant->mRow,false,false,x-px,vision-px)) ||
            (close && x-px>=7*kFusionBasePixelsX)) continue;
        if (!nearest || x<nearestX) { nearest=zombie; nearestX=x; }
    }
    return nearest;
}
bool NearScaredy(Plant* plant)
{
    Zombie* zombie=nullptr;
    while (plant->mBoard->IterateZombies(zombie))
        if (Normal(zombie) && (GetNativePlantData(plant)->kind!=1171 || zombie->mRow==plant->mRow) && Touches(plant,zombie,false)) return true;
    return false;
}
void FreezeScaredyArea(Plant* plant)
{
    float x=0,y=0;
    if (!Ground(plant,x,y)) return;
    std::vector<ZombieID> targets; Zombie* zombie=nullptr;
    while (plant->mBoard->IterateZombies(zombie))
        if (Normal(zombie) && std::abs(zombie->mRow-plant->mRow)<=1 && Touches(plant,zombie,true))
            targets.push_back(plant->mBoard->ZombieGetID(zombie));
    bool hit=false;
    for (auto id:targets)
        if ((zombie=plant->mBoard->ZombieTryToGet(id)) && Normal(zombie) && ApplyNativeColdFreeze(zombie,400))
        { zombie->TakeDamage(20,2U); hit=true; }
    Particle(plant->mBoard,x,y,PARTICLE_ICE_TRAP);
    if (hit) plant->mApp->PlayFoley(FOLEY_FROZEN);
    LawnNet::NetTrace("Fusion native Scaredy freeze plant=%u targets=%zu tick=%d",plant->mNetId,targets.size(),plant->mBoard->mMainCounter);
}
bool UpdateScaredyFear(Plant* plant,bool nearby)
{
    const auto* data=GetNativePlantData(plant);
    FusionScaredyFearTiming timing;
    if (!TryGetFusionScaredyFearTiming(data->kind,data->animationSpeed,timing)) return false;
    if (plant->mState==STATE_SCAREDYSHROOM_LOWERING)
    {
        if (plant->mStateCountdown==timing.lowering-timing.event) FreezeScaredyArea(plant);
        if (!plant->mStateCountdown)
        { plant->mState=STATE_SCAREDYSHROOM_SCARED; Play(plant,"anim_scaredidle",100,true); }
    }
    else if (plant->mState==STATE_SCAREDYSHROOM_SCARED)
    {
        if (!nearby)
        { plant->mState=STATE_SCAREDYSHROOM_RAISING; plant->mStateCountdown=timing.raising; Play(plant,"anim_raise",timing.raising); }
    }
    else if (plant->mState==STATE_SCAREDYSHROOM_RAISING)
    {
        if (!plant->mStateCountdown) { plant->mState=STATE_NOTREADY; plant->PlayIdleAnim(15); }
    }
    else if (nearby)
    { plant->mState=STATE_SCAREDYSHROOM_LOWERING; plant->mStateCountdown=timing.lowering; Play(plant,"anim_lower",timing.lowering); }
    return true;
}
bool Emit(Plant* plant,uint16_t kind,int row,uint8_t phase=0)
{
    auto* board=plant->mBoard;
    if (row<0 || row>=GetNativeFusionRowCount(board) || board->mProjectiles.mSize>=board->mProjectiles.mMaxSize) return false;
    float x=0,y=0; if (!Ground(plant,x,y)) return false;
    const auto* profile=Profile(plant); if (!profile) return false;
    // Full source hierarchy nominal-resting Shoot minus Shadow, plus the
    // callback's explicitX+.1. Animated mouth blending remains native art.
    x+=static_cast<float>(profile->muzzleXFromShadow*kFusionBasePixelsX);
    y-=static_cast<float>(profile->muzzleYFromShadow*GetNativeFusionPixelsY(board));
    const auto* straight=FindFusionStraightProjectileProfile(kind);
    const ProjectileType carrier=straight ? straight->nativeType : kind==0 ? PROJECTILE_PEA : PROJECTILE_PUFF;
    auto* p=board->AddProjectile(static_cast<int>(x-20),static_cast<int>(y-20),plant->mRenderOrder-1,row,carrier);
    if (!p) return false;
    p->mMotionType=phase?MOTION_THREEPEATER:MOTION_STRAIGHT;
    p->mDamageRangeFlags=plant->GetDamageRangeFlags(WEAPON_PRIMARY);
    p->mVelX=kFusionPuffPixelsPerTick; p->mVelY=p->mVelZ=p->mAccZ=0;
    p->mShadowY=static_cast<float>(board->GridToPixelY(plant->mPlantCol,row)+50);
    NativeProjectileData data; data.kind=kind; data.damage=20; data.specialType=phase;
    if (kind==13) data.effectVariant=static_cast<uint8_t>(RandRangeInt(0,6));
    if (!AttachNativeProjectile(board,p,data)) { p->Die(); return false; }
    LawnNet::NetTrace("Fusion native Batch projectile plant=%u kind=%u id=%u payload=%u row=%d phase=%u variant=%u damage=20",
        plant->mNetId,GetNativePlantData(plant)->kind,p->mNetId,kind,row,phase,data.effectVariant);
    return true;
}
void HitArea(Plant* plant,bool squash)
{
    const uint16_t kind=GetNativePlantData(plant)->kind;
    const auto* strike=squash?nullptr:FindFusionCaltropStrikeProfile(kind);
    if (!squash && !strike) return;
    const int damage=squash?1800:strike->damage;
    Zombie* zombie=nullptr; std::vector<ZombieID> targets;
    while (plant->mBoard->IterateZombies(zombie))
        if (Normal(zombie) && zombie->mRow==plant->mRow && Touches(plant,zombie,true)) targets.push_back(plant->mBoard->ZombieGetID(zombie));
    if(kind==17&&!squash)
    {
        // Caltrop.AnimAttack invokes its real KillCar before Damage4. Capture
        // ordinary targets first: native Die retires metadata immediately,
        // whereas Unity's method continues after its deferred Destroy.
        float x=0,y=0;
        // Both source overlap passes use Plant.shadow (+0x50), not self root.
        if(Ground(plant,x,y))
        {
            std::vector<ZombieID> drivers;Zombie* driver=nullptr;
            const double px=kFusionBasePixelsX,py=GetNativeFusionPixelsY(plant->mBoard);
            while(plant->mBoard->IterateZombies(driver))
                if(Normal(driver)&&driver->mRow==plant->mRow)
                {
                    bool touches=IsNativeSpecialZombie269Vehicle(driver)&&
                        NativeSpecialZombie269VehicleTouchesRect(driver,GetNativeSourceZombie269NodePoint,x-px*.5,y-py*.5,px,py);
                    const auto* source=GetNativeSourceZombieData(driver);
                    if(source&&(source->kind==16||source->kind==31))
                    {
                        // The shared typed rect uses the actual303 source box,
                        // never a NORMAL/ZAMBONI carrier rectangle.
                        const auto body=driver->GetZombieRect();
                        touches=body.mWidth>0&&body.mHeight>0&&body.mX<=x+px*.5&&body.mX+body.mWidth>=x-px*.5&&
                            body.mY<=y+py*.5&&body.mY+body.mHeight>=y-py*.5;
                    }
                    if(touches)drivers.push_back(plant->mBoard->ZombieGetID(driver));
                }
            for(auto driverId:drivers)if((driver=plant->mBoard->ZombieTryToGet(driverId))&&Normal(driver))
            {
                if(GetNativeSourceZombieData(driver)->kind==18)HandleNativeSpecialZombie269DriverCaltrop(driver);
                else NativeZombie313Caltrop(driver);
                // Source14's inherited KillByCaltrop is empty; the spike still
                // consumes itself and continues its already captured Damage4.
                plant->mApp->PlayFoley(FOLEY_TIRE_POP);
                LawnNet::NetTrace("Fusion source Caltrop KillCar plant=%u kind=%u driver=%u source=%u tick=%d",
                    plant->mNetId,kind,driver->mNetId,GetNativeSourceZombieData(driver)->kind,plant->mBoard->mMainCounter);
                if(!plant->mDead)plant->Die();
            }
        }
    }
    for (auto id:targets) if ((zombie=plant->mBoard->ZombieTryToGet(id)) && Normal(zombie))
    {
        // IceShieldless5 bypasses the shield, then sets slow before helmet/body.
        // The Caltrop callback has no projectile-style frozen damage multiplier.
        if (strike && strike->coldLevelsAfterDamage>0) ApplyNativeColdSlow(zombie);
        zombie->TakeDamage(damage,FusionBatchDamageFlags(kind));
        // This admitted hit may have killed the still-allocated generation.
        if (strike && strike->coldLevelsAfterDamage>0) AddNativeColdLevel(zombie,strike->coldLevelsAfterDamage);
        if (strike && strike->warmAfterDamage) NativeWarmAfterDamage(zombie);
        LawnNet::NetTrace("Fusion native Batch area plant=%u kind=%u target=%u damage=%d shield=%d helm=%d body=%d",
            plant->mNetId,kind,zombie->mNetId,damage,zombie->mShieldHealth,zombie->mHelmHealth,zombie->mBodyHealth);
    }
    if (!targets.empty()) plant->mApp->PlayFoley(squash?FOLEY_THUMP:FOLEY_SPLAT);
}
void HitFume(Plant* plant,bool unbounded=false,int damage=20)
{
    float px=0,py=0; if (!Ground(plant,px,py)) return;
    std::vector<ZombieID> targets; Zombie* zombie=nullptr;
    while (plant->mBoard->IterateZombies(zombie))
    {
        float x=0,y=0;
        if (Normal(zombie) && zombie->mRow==plant->mRow && zombie->GetShadowCenter(x,y) &&
            x>=px && (unbounded || x<=px+7*kFusionBasePixelsX) &&
            (!unbounded || FusionPlantSearchUniqueZombieAllowed(GetNativeFusionZombieStatus(zombie),false,!zombie->mHasHead))) targets.push_back(plant->mBoard->ZombieGetID(zombie));
    }
    for (auto it=targets.rbegin();it!=targets.rend();++it)
        if ((zombie=plant->mBoard->ZombieTryToGet(*it)) && Normal(zombie)) zombie->TakeDamage(damage,2U);
    Particle(plant->mBoard,px,py-static_cast<float>(kFusionBasePixelsY*.5),PARTICLE_FUMECLOUD);
    plant->mApp->PlayFoley(FOLEY_FUME);
}
void SmallBlast(Plant* plant,bool potato)
{
    float x=0,y=0;
    if (!Ground(plant,x,y)) return;
    std::vector<ZombieID> targets; Zombie* target=nullptr;
    while (plant->mBoard->IterateZombies(target))
    {
        if (!Normal(target) || target->mRow!=plant->mRow) continue;
        const auto r=target->GetZombieRect();
        if (FusionBatchCircleTouchesRect(x,y,1,r.mX,r.mY,r.mWidth,r.mHeight)) targets.push_back(plant->mBoard->ZombieGetID(target));
    }
    for (auto id:targets)
        if ((target=plant->mBoard->ZombieTryToGet(id)) && Normal(target))
        {
            if (potato) target->TakeDamage(600,18U); // Source SmallBombPotato Explode10, normalD2.
            else ApplyNativeFusionCherryDamage(target);
        }
    LawnNet::NetTrace("Fusion native Batch smallblast id=%u kind=%u targets=%zu damage=%d tick=%d",
        plant->mNetId,GetNativePlantData(plant)->kind,targets.size(),potato?600:1800,plant->mBoard->mMainCounter);
}
int ScaledTicks(double seconds,float speed) { return static_cast<int>(std::ceil(seconds/speed*100)); }
void Squash(Plant* plant)
{
    auto* data=GetNativePlantData(plant); const float speed=data->animationSpeed;
    if (plant->mState==STATE_NOTREADY)
    {
        Zombie* z=nullptr; Zombie* closest=nullptr; double distance=0; float px=0,py=0;
        if (!Ground(plant,px,py)) return;
        while (plant->mBoard->IterateZombies(z))
        {
            float x=0,y=0;
            if (!Normal(z) || z->mRow!=plant->mRow || !Touches(plant,z,false) || !z->GetShadowCenter(x,y)) continue;
            if (!closest || std::abs(x-px)<distance) { closest=z; distance=std::abs(x-px); }
        }
        if (!closest) return;
        float x=0,y=0; closest->GetShadowCenter(x,y);
        plant->mTargetZombieID=plant->mBoard->ZombieGetID(closest);
        plant->mTargetX=static_cast<int>(x); plant->mTargetY=plant->mBoard->mMainCounter;
        plant->mState=STATE_SQUASH_LOOK; plant->mStateCountdown=ScaledTicks(1.5*static_cast<double>(.8333333f)/static_cast<double>(1.2f),speed);
        Play(plant,x<px?"anim_lookleft":"anim_lookright",plant->mStateCountdown);
        plant->mApp->PlayFoley(FOLEY_SQUASH_HMM); data->shootSeconds=0;
    }
    else if (plant->mState==STATE_SQUASH_LOOK && !plant->mStateCountdown)
    {
        plant->mState=STATE_SQUASH_PRE_LAUNCH; plant->mStateCountdown=ScaledTicks(static_cast<double>(.55f),speed);
        Play(plant,"anim_jumpup",ScaledTicks(static_cast<double>(1.1666666f)*.875,speed)); data->shootSeconds=0;
    }
    else if (plant->mState==STATE_SQUASH_PRE_LAUNCH && !plant->mStateCountdown)
    {
        float x=static_cast<float>(plant->mTargetX),y=0;
        auto* target=plant->mBoard->ZombieTryToGet(plant->mTargetZombieID);
        if (Normal(target) && target->GetShadowCenter(x,y))
        {
            const float time=(plant->mBoard->mMainCounter-plant->mTargetY)*.01f;
            if (time>0) x+=(x-plant->mTargetX)/time*.5f; // Source average observed motion, not new aim prediction.
        }
        float px=0,py=0; Ground(plant,px,py);
        plant->mTargetX=static_cast<int>(x-(px-plant->mX)); plant->mTargetY=plant->mY;
        plant->mLaunchCounter=plant->mX;
        plant->mState=STATE_SQUASH_RISING;
        plant->mStateCountdown=ScaledTicks(static_cast<double>(1.1666666f)*.875-static_cast<double>(.55f),speed);
        data->shootSeconds=0;
    }
    else if (plant->mState==STATE_SQUASH_RISING)
    {
        const int duration=ScaledTicks(static_cast<double>(1.1666666f)*.875-static_cast<double>(.55f),speed);
        const double dx=(plant->mTargetX-plant->mLaunchCounter)/kFusionBasePixelsX;
        const double distance=std::sqrt(dx*dx+1.75*1.75);
        const double travelSeconds=distance/(8.0*(distance>2?distance*.5:1.0));
        const float t=static_cast<float>((std::min)(1.0,(duration-plant->mStateCountdown)*.01/travelSeconds));
        const float ease=t<.5f?2*t*t:1-2*(1-t)*(1-t);
        plant->mX=static_cast<int>(plant->mLaunchCounter+(plant->mTargetX-plant->mLaunchCounter)*ease);
        plant->mY=plant->mTargetY-static_cast<int>(1.75*kFusionBasePixelsY*ease);
        if (!plant->mStateCountdown)
        {
            plant->mState=STATE_SQUASH_FALLING; plant->mStateCountdown=ScaledTicks(static_cast<double>(.33333334f)/static_cast<double>(1.8f),speed);
            Play(plant,"anim_jumpdown",plant->mStateCountdown); data->shootSeconds=0;
        }
    }
    else if (plant->mState==STATE_SQUASH_FALLING)
    {
        const int duration=ScaledTicks(static_cast<double>(.33333334f)/static_cast<double>(1.8f),speed);
        plant->mY=plant->mTargetY-static_cast<int>(1.75*kFusionBasePixelsY*plant->mStateCountdown/(std::max)(duration,1));
        if (!plant->mStateCountdown)
        {
            HitArea(plant,true);
            if (data->kind==1054) FireRow(plant->mBoard,plant->mRow);
            plant->mBoard->ShakeBoard(1,4);
            Particle(plant->mBoard,plant->mX+40.0f,plant->mY+80.0f,PARTICLE_DUST_SQUASH);
            plant->mState=STATE_SQUASH_DONE_FALLING;
            plant->mStateCountdown=ScaledTicks((1.5-static_cast<double>(.33333334f))/static_cast<double>(1.8f),speed);
        }
    }
    else if (plant->mState==STATE_SQUASH_DONE_FALLING && !plant->mStateCountdown) plant->Die();
}
}

bool IsNativeBatchAction(const Plant* plant) { return Profile(plant)!=nullptr; }
const FusionShooterTimingProfile* GetNativeBatchTimingProfile(const Plant* plant) { return Timing(plant); }
void InitializeNativeBatchAction(Plant* plant)
{
    const auto* profile=Profile(plant);
    if (!profile || !Host(plant) || plant->mDead) return;
    auto* data=GetNativePlantData(plant);
    plant->mState=STATE_NOTREADY; plant->mDoSpecialCountdown=0; plant->mShootingCounter=0;
    if (const auto* timing=Timing(plant))
    {
        if (profile->family==FusionBatchActionFamily::Caltrop)
        {
            data->animationSpeed=RandRangeFloat(.9f,1.1f);
            plant->mLaunchRate=static_cast<int>(timing->attackIntervalSeconds*100);
            const float seconds=RandRangeFloat(.2f,.8f)*timing->attackIntervalSeconds;
            plant->mLaunchCounter=static_cast<int>(std::ceil(static_cast<double>(seconds)*100));
        }
        else InitializeNativeShooterTiming(plant,*timing,true);
    }
    else data->animationSpeed=RandRangeFloat(.9f,1.1f);
    if (profile->family==FusionBatchActionFamily::Hypno || profile->family==FusionBatchActionFamily::Doom ||
        profile->family==FusionBatchActionFamily::Contact)
    { plant->SetSleeping(false); plant->mWakeUpCounter=0; }
    if (profile->family==FusionBatchActionFamily::Doom || profile->family==FusionBatchActionFamily::Jalapeno)
    {
        TryGetFusionBatchInstantTicks(data->kind,data->animationSpeed,plant->mDoSpecialCountdown);
        Play(plant,"anim_explode",plant->mDoSpecialCountdown); data->shootSeconds=0;
    }
    if (profile->family==FusionBatchActionFamily::ManualFume) plant->mLaunchCounter=0;
    if (data->kind==1042) data->batchShotCount=0;
    if (data->kind==1171) { plant->mDoSpecialCountdown=2000; data->batchMode=0; }
    LawnNet::NetTrace("Fusion native Batch init id=%u kind=%u speed=%.9g launch=%d special=%d",plant->mNetId,data->kind,data->animationSpeed,plant->mLaunchCounter,plant->mDoSpecialCountdown);
}
void NativeBatchPlantPlaced(Plant* plant)
{
    const auto* data=GetNativePlantData(plant);
    if (!data || !Host(plant) || plant->mDead || plant->mApp->IsIZombieLevel()) return;
    std::array<int,3> rows{}; size_t count=0;
    if (!TryGetFusionBatchEntryRows(data->kind,plant->mRow,GetNativeFusionRowCount(plant->mBoard),rows,count)) return;
    for (size_t i=0;i<count;++i) FireRow(plant->mBoard,rows[i]);
    LawnNet::NetTrace("Fusion native Batch entry id=%u kind=%u rows=%zu moving=0",plant->mNetId,data->kind,count);
}
bool UpdateNativeBatchAction(Plant* plant)
{
    const auto* profile=Profile(plant); if (!profile) return false;
    if (!Host(plant) || plant->mDead) return true;
    const auto family=profile->family;
    if (family==FusionBatchActionFamily::Contact)
    {
        if (UpdateNativeColdContact(plant)) return true;
        if (profile->kind==1044)
        {
            float x=0,y=0;
            if (!Ground(plant,x,y)) return true;
            Zombie* target=nullptr;
            while (plant->mBoard->IterateZombies(target))
            {
                if (!Normal(target) || target->mRow!=plant->mRow || !target->mHasHead ||
                    GetNativeFusionZombieStatus(target)==10) continue;
                const auto r=target->GetZombieRect();
                bool touch=false;
                for (size_t i=0;i<2;++i)
                { FusionBatchArea a; if (TryMakeFusionDoomPuffTrigger(i,x,y,a) && FusionBatchAreaTouchesRect(a,r.mX,r.mY,r.mWidth,r.mHeight)) touch=true; }
                if (!touch) continue;
                Particle(plant->mBoard,x,y,PARTICLE_DOOM);
                plant->mBoard->ShakeBoard(1,4); plant->mApp->PlaySample(Sexy::SOUND_DOOMSHROOM);
                SmallBlast(plant,false);
                // Source contact attacks before setting exploded, then Die.
                plant->mState=STATE_DOINGSPECIAL;
                plant->Die();
                break;
            }
        }
        return true;
    }
    if (family==FusionBatchActionFamily::ManualFume)
    { if (plant->mLaunchCounter>0) --plant->mLaunchCounter; return true; }
    if (family==FusionBatchActionFamily::Squash) { Squash(plant); return true; }
    if (family==FusionBatchActionFamily::Doom || family==FusionBatchActionFamily::Jalapeno || family==FusionBatchActionFamily::Hypno) return true;
    auto* data=GetNativePlantData(plant);
    if (family==FusionBatchActionFamily::Threepeater && plant->mState==STATE_READY && !plant->mStateCountdown)
    { Emit(plant,0,plant->mRow); plant->mState=STATE_NOTREADY; }
    if (family==FusionBatchActionFamily::Scaredy)
    {
        const bool nearby=NearScaredy(plant);
        if (data->kind==1042 && nearby)
        {
            uint32_t after=0; bool explode=false;
            if (!TryAdvanceFusionScaredyDoom(data->batchShotCount,true,after,explode)) return true;
            data->batchShotCount=after;
            if (explode)
            {
                float x=0,y=0; if (!Ground(plant,x,y)) return true;
                ApplyNativeBatchDoomAt(plant->mBoard,x,y,plant->mPlantCol,plant->mRow);
                plant->Die(); return true;
            }
        }
        if (UpdateScaredyFear(plant,nearby)) { }
        else if (nearby)
        {
            if (plant->mState!=STATE_SCAREDYSHROOM_SCARED)
            { plant->mState=STATE_SCAREDYSHROOM_SCARED; Play(plant,"anim_scaredidle",100,true); }
        }
        else if (plant->mState==STATE_SCAREDYSHROOM_SCARED)
        { plant->mState=STATE_NOTREADY; plant->PlayIdleAnim(15); }
    }
    if (--plant->mLaunchCounter>0) return true;
    if (family==FusionBatchActionFamily::Caltrop)
    {
        TryGetFusionCaltropCycleTicks(data->kind,RandRangeFloat(-.1f,.1f),plant->mLaunchCounter);
        Zombie* z=nullptr; bool found=false;
        while (plant->mBoard->IterateZombies(z)) if (Normal(z) && z->mRow==plant->mRow && Touches(plant,z,false)) { found=true; break; }
        if (found && data->shootSeconds<0)
        { BeginNativeShooterShot(plant,*Timing(plant)); Play(plant,"anim_attack",static_cast<int>(std::ceil(FusionShooterDuration(*Timing(plant),data->animationSpeed)*100))); }
        return true;
    }
    // ScaredFume intentionally resets to the exact CSV interval, without the
    // ordinary PlantShootUpdate multiplicative jitter.
    if (family==FusionBatchActionFamily::AdaptiveFume) plant->mLaunchCounter=151;
    else if (data->kind==1042)
    {
        auto clock=*Timing(plant);
        if (!TryGetFusionScaredyDoomInterval(data->batchShotCount,clock.attackIntervalSeconds)) return true;
        ResetNativeShooterLaunchCounter(plant,clock);
    }
    else ResetNativeShooterLaunchCounter(plant,*Timing(plant));
    if (family==FusionBatchActionFamily::Scaredy && (plant->mState==STATE_SCAREDYSHROOM_SCARED ||
        plant->mState==STATE_SCAREDYSHROOM_LOWERING || plant->mState==STATE_SCAREDYSHROOM_RAISING)) return true;
    if (data->shootSeconds>=0) return true;
    if (family==FusionBatchActionFamily::AdaptiveFume)
    {
        const bool close=Acquire(plant,true)!=nullptr;
        if (!close && !Acquire(plant,false)) return true;
        plant->mState=close?STATE_KERNELPULT_BUTTER:STATE_NOTREADY;
    }
    else if (!Acquire(plant,false)) return true;
    BeginNativeShooterShot(plant,*Timing(plant));
    Play(plant,"anim_shooting",static_cast<int>(std::ceil(FusionShooterDuration(*Timing(plant),data->animationSpeed)*100)));
    return true;
}
void UpdateNativeBatchActionPhase(Plant* plant)
{
    const auto* profile=Profile(plant); if (!profile || !Host(plant) || plant->mDead) return;
    auto* data=GetNativePlantData(plant);
    // Presentation mirrors the host counter; clients never own a second timer.
    if (data->kind==1171) data->batchMode=plant->mDoSpecialCountdown==0?1:0;
    if (const auto* timing=Timing(plant))
    {
        UpdateNativeShooterPhase(plant,*timing);
        if (profile->family==FusionBatchActionFamily::AdaptiveFume && data->shootSeconds<0) plant->mState=STATE_NOTREADY;
    }
    else { data->idleSeconds+=.01f; if (data->shootSeconds>=0) data->shootSeconds+=.01f; }
}
bool DispatchNativeBatchShot(Plant* plant)
{
    const auto* profile=Profile(plant); if (!profile || !Timing(plant)) return false;
    if (!Host(plant) || plant->mDead) return true;
    const auto* data=GetNativePlantData(plant); const auto* timing=Timing(plant);
    for (size_t i=0;i<timing->eventClipSeconds.size();++i)
    {
        bool due=false; if (!TryGetFusionShooterEventAtCounter(*timing,data->animationSpeed,plant->mShootingCounter,i,due) || !due) continue;
        if (profile->family==FusionBatchActionFamily::Caltrop)
        { if (!ApplyNativeFireCaltropStrike(plant)) HitArea(plant,false); }
        else if (profile->family==FusionBatchActionFamily::Gloom) ApplyNativeAction266GloomAttack(plant);
        else if (profile->family==FusionBatchActionFamily::ManualFume) HitFume(plant,true,profile->damage);
        else if (profile->family==FusionBatchActionFamily::AdaptiveFume && plant->mState==STATE_KERNELPULT_BUTTER) HitFume(plant);
        else if (profile->family==FusionBatchActionFamily::Threepeater)
        {
            Emit(plant,0,plant->mRow);
            if (plant->mRow<GetNativeFusionRowCount(plant->mBoard)-1) Emit(plant,0,plant->mRow+1,kFusionThreeLower);
            if (plant->mRow>0) Emit(plant,0,plant->mRow-1,kFusionThreeUpper);
            if (plant->mRow==0 || plant->mRow==GetNativeFusionRowCount(plant->mBoard)-1)
            { plant->mState=STATE_READY; plant->mStateCountdown=21; } // This frame's common-- leaves20; Invoke is wall-time, independent of Speed.
        }
        else if (data->kind==1170) LaunchNativePotatoPuff(plant,20);
        else if (data->kind==1171)
        {
            const bool prepared=plant->mDoSpecialCountdown>0;
            if (LaunchNativePotatoPuff(plant,prepared?600:60) && prepared)
            { plant->mPlantHealth-=75; plant->mEatenFlashCountdown=25; NativePlantHealthChanged(plant); if (plant->mPlantHealth<=0) plant->Die(); }
        }
        else if (Emit(plant,timing->projectileKind,plant->mRow) && data->kind==1042)
        {
            uint32_t after=0; bool explode=false;
            if (TryAdvanceFusionScaredyDoom(data->batchShotCount,false,after,explode)) GetNativePlantData(plant)->batchShotCount=after;
        }
    }
    return true;
}
bool ApplyNativeBatchSpecial(Plant* plant)
{
    const auto* profile=Profile(plant);
    if (profile && profile->kind==1171) return true; // Common native countdown reached source preparation20s.
    if (!profile || (profile->family!=FusionBatchActionFamily::Doom && profile->family!=FusionBatchActionFamily::Jalapeno)) return false;
    if (!Host(plant) || plant->mDead) return true;
    auto* board=plant->mBoard; const int col=plant->mPlantCol,row=plant->mRow; float x=0,y=0; Ground(plant,x,y);
    if (profile->family==FusionBatchActionFamily::Jalapeno)
    { FireRow(board,row); plant->Die(); return true; }
    // Source Doom removes itself before SetDoom.
    plant->Die();
    ApplyNativeBatchDoomAt(board,x,y,col,row,profile->damage,true,profile->kind==1040);
    return true;
}
bool TryActivateNativeDoomFume(Plant* plant)
{
    const auto* data=GetNativePlantData(plant);
    if (!data || data->kind!=1043) return false;
    if (!Host(plant) || plant->mDead || plant->mLaunchCounter!=0) return true;
    const auto* clock=Timing(plant);
    if (!clock || !BeginNativeShooterShot(plant,*clock)) return true;
    plant->mLaunchCounter=6000;
    Play(plant,"anim_shooting",static_cast<int>(std::ceil(FusionShooterDuration(*clock,data->animationSpeed)*100)));
    LawnNet::NetTrace("Fusion native DoomFume activate id=%u cooldown=%d tick=%d",plant->mNetId,plant->mLaunchCounter,plant->mBoard->mMainCounter);
    return true;
}
namespace
{
bool CanonicalBoard(Board* board)
{
    const auto* context=GetNativeContext(board);
    return context && board->mApp && board->mApp->mBoard==board && !board->mApp->NetIsClient() &&
        board->mApp->mGameScene==SCENE_PLAYING && context->selection.State().phase==FusionBankPhase::Playing &&
        NativeFusionBackgroundValid(static_cast<uint8_t>(board->mBackground)) && board->mBoardFadeOutCounter<0;
}
}
void ApplyNativeBatchFireRow(Board* board,int row,int damage,bool fix)
{
    if (CanonicalBoard(board)) FireRow(board,row,damage,fix);
}
void ApplyNativeBatchDoomAt(Board* board,float x,float y,int col,int row,int damage,bool setPit,bool iceDoom)
{
    if (!CanonicalBoard(board) || damage!=1800 || row<0 || row>=GetNativeFusionRowCount(board) ||
        !std::isfinite(x) || !std::isfinite(y) || (setPit && !board->IsValidGridCell(col,row))) return;
    // Source default position is relative to the cell's logical Shadow anchor.
    x-=.3f*static_cast<float>(kFusionBasePixelsX); y-=.2f*static_cast<float>(kFusionBasePixelsY);
    std::vector<ZombieID> targets; Zombie* zombie=nullptr;
    while (board->IterateZombies(zombie))
    {
        if (!Normal(zombie)) continue; const auto r=zombie->GetZombieRect();
        if (iceDoom || FusionDoomTouchesRect(x,y,r.mX,r.mY,r.mWidth,r.mHeight)) targets.push_back(board->ZombieGetID(zombie));
    }
    for (auto id:targets) if ((zombie=board->ZombieTryToGet(id)) && Normal(zombie))
    {
        if (iceDoom) ApplyNativeColdFreeze(zombie,kFusionColdMaximumFreezeTicks);
        ApplyNativeFusionCherryDamage(zombie);
    }
    GridItem* item=nullptr;
    while (board->IterateGridItems(item))
    {
        if (item->mDead || item->mGridItemType!=GRIDITEM_LADDER || !board->IsValidGridCell(item->mGridX,item->mGridY)) continue;
        const double w=.98*.7*kFusionBasePixelsX,h=2.09*.7*kFusionBasePixelsY;
        if (FusionDoomTouchesRect(x,y,board->GetCellCenterX(item->mGridX,item->mGridY)+.3*kFusionBasePixelsX-w*.5,
            board->GridToPixelY(item->mGridX,item->mGridY)+50-.6*kFusionBasePixelsY-h*.5,w,h)) item->GridItemDie();
    }
    if (setPit)
    {
        Plant* other=nullptr; std::vector<uint32_t> plants;
        while (board->IteratePlants(other)) if (!other->mDead && other->mPlantCol==col && other->mRow==row) plants.push_back(board->mPlants.DataArrayGetID(other));
        for (auto id:plants) if ((other=board->mPlants.DataArrayTryToGet(id)) && !other->mDead) other->Die();
        if (board->mGridItems.mSize<board->mGridItems.mMaxSize)
            if (auto* crater=board->AddACrater(col,row)) crater->mGridItemCounter=18000; // Source GridItem.CraterUpdate90/180s.
    }
    board->mApp->PlaySample(Sexy::SOUND_DOOMSHROOM); board->ShakeBoard(3,-4); Particle(board,x,y,PARTICLE_DOOM);
    LawnNet::NetTrace("Fusion native Doom col=%d row=%d pit=%d ice=%d damage=%d tick=%d",col,row,setPit?1:0,iceDoom?1:0,damage,board->mMainCounter);
}

void NativeBatchPlantRemoving(Plant* plant,int reason)
{
    if (!plant || reason<0 || reason>7 || !CanonicalBoard(plant->mBoard) || plant->mNetGhost ||
        !plant->mIsOnBoard || !plant->mDead) return;
    const auto* data=GetNativePlantData(plant);
    if (!data || (data->kind!=1044 && data->kind!=1171)) return;
    const auto kind=data->kind;
    float x=0,y=0;
    if (!Ground(plant,x,y)) return;
    const bool potato=kind==1171;
    if (!potato && plant->mState==STATE_DOINGSPECIAL) return; // Already emitted by contact.
    SmallBlast(plant,potato);
    if (potato)
    { Particle(plant->mBoard,x,y-static_cast<float>(.2*kFusionBasePixelsY),PARTICLE_POTATO_MINE); plant->mApp->PlaySample(Sexy::SOUND_POTATO_MINE); }
    LawnNet::NetTrace("Fusion native Batch death id=%u kind=%u reason=%d damage=%d tick=%d",
        plant->mNetId,kind,reason,potato?600:1800,plant->mBoard->mMainCounter);
}
void NativeBatchZombieDying(Zombie* zombie)
{
    if (!zombie || zombie->mNetGhost || !CanonicalBoard(zombie->mBoard)) return;
    auto* action=GetNativeZombieActionData(zombie);
    if (!action || (!action->doomOnDeath && !action->jalaedOnDeath)) return;
    float x=0,y=0;
    if (!zombie->GetShadowCenter(x,y)) return;
    const bool doom=action->doomOnDeath,jalaed=action->jalaedOnDeath;
    action->doomOnDeath=false; action->jalaedOnDeath=false; // Consume before recursive damage.
    LawnNet::NetTrace("Fusion native Doom tag consumed zombie=%u tick=%d",zombie->mNetId,zombie->mBoard->mMainCounter);
    if (doom) ApplyNativeBatchDoomAt(zombie->mBoard,x,y,-1,zombie->mRow,1800,false,false);
    if (jalaed) ApplyNativeFireSourceExplosion(zombie,100,true);
}
bool HandleNativeBatchPlantEaten(Zombie* zombie,Plant* plant)
{
    if(HandleNativeLastPlants269Eaten(zombie,plant))return true;
    if(HandleNativeSummonPlants269Eaten(zombie,plant))return true;
    const auto* data=GetNativePlantData(plant);
    if (!data || (data->kind!=8 && data->kind!=1022 && data->kind!=1024 && data->kind!=1045)) return false;
    if (!Normal(zombie) || !Host(plant) || plant->mDead || zombie->mBoard!=plant->mBoard) return true;
    const auto before=*data; auto* board=plant->mBoard; const int col=plant->mPlantCol,row=plant->mRow;
    if (before.kind==1045)
    {
        auto* action=GetOrCreateNativeZombieActionData(zombie);
        if (!action) return true;
        action->doomOnDeath=true;
    }
    zombie->StopEating(); zombie->StartMindControlled(); plant->Die();
    if (before.kind==1022)
    {
        if (IsNativeSourceZombie(zombie))
        {
            if (!TryShrinkNativeSourceZombie(zombie))
            {
                LawnNet::NetTrace("Fusion source Zombie shrink rejected id=%u",zombie->mNetId);
                return true;
            }
        }
        else
        {
            zombie->mBodyHealth/=2; zombie->mBodyMaxHealth/=2;
            if (zombie->mHelmMaxHealth>0) { zombie->mHelmHealth/=2; zombie->mHelmMaxHealth/=2; }
            if (zombie->mShieldMaxHealth>0) { zombie->mShieldHealth/=2; zombie->mShieldMaxHealth/=2; }
            zombie->mScaleZombie*=.5f; zombie->UpdateAnimSpeed();
        }
        if (auto* action=GetOrCreateNativeZombieActionData(zombie)) action->scale=zombie->mScaleZombie;
    }
    if (before.kind==1024)
    {
        NativePlantData replacement; replacement.kind=9; replacement.packetId=before.packetId;
        replacement.owner=before.owner; replacement.randomOrigin=before.randomOrigin;
        TrySpawnNativeFusionPlant(board,col,row,replacement);
    }
    Particle(board,zombie->mPosX+60,zombie->mPosY+40,PARTICLE_MIND_CONTROL);
    zombie->TrySpawnLevelAward();
    LawnNet::NetTrace("Fusion native Batch hypnotize kind=%u zombie=%u shrink=%d replacement=%d",before.kind,zombie->mNetId,before.kind==1022?1:0,before.kind==1024?9:-1);
    return true;
}
bool NativeBatchPlantUntargetable(const Plant* plant)
{
    const auto* p=Profile(plant);
    return p && p->family==FusionBatchActionFamily::Squash && plant->mState>=STATE_SQUASH_LOOK && plant->mState<=STATE_SQUASH_DONE_FALLING;
}
bool NativeBatchPlantInvincible(const Plant* plant)
{
    const auto* p=Profile(plant);
    return p && (p->family==FusionBatchActionFamily::Doom || p->family==FusionBatchActionFamily::Jalapeno || NativeBatchPlantUntargetable(plant));
}
bool GetNativeBatchContactPlantRect(const Plant* plant,Sexy::Rect& rect)
{
    const auto* profile=Profile(plant);
    if (!profile || profile->family!=FusionBatchActionFamily::Contact) return false;
    float x=0,y=0;
    if (!const_cast<Plant*>(plant)->GetShadowCenter(x,y)) return false;
    FusionBatchArea area;
    if (const auto* cold=FindFusionColdContactProfile(profile->kind))
    {
        area.width=cold->width*kFusionBasePixelsX;
        area.height=cold->height*kFusionBasePixelsY;
        area.x=x+cold->centerXFromShadow*kFusionBasePixelsX-area.width*.5;
        area.y=y-cold->centerYFromShadow*kFusionBasePixelsY-area.height*.5;
    }
    else
    {
        if (profile->kind!=1044 || !TryMakeFusionDoomPuffTrigger(0,x,y,area)) return false;
        FusionBatchArea second;
        if (!TryMakeFusionDoomPuffTrigger(1,x,y,second)) return false;
        const double right=(std::max)(area.x+area.width,second.x+second.width);
        const double bottom=(std::max)(area.y+area.height,second.y+second.height);
        area.x=(std::min)(area.x,second.x);area.y=(std::min)(area.y,second.y);
        area.width=right-area.x;area.height=bottom-area.y;
    }
    const int left=static_cast<int>(std::floor(area.x)),top=static_cast<int>(std::floor(area.y));
    rect=Sexy::Rect(left,top,static_cast<int>(std::ceil(area.x+area.width))-left,
        static_cast<int>(std::ceil(area.y+area.height))-top);
    return true;
}
bool GetNativeBatchVisualPose(const Plant* plant,const char*& state,float& elapsed)
{
    const auto* p=Profile(plant); if (!p) return false;
    const auto* d=GetNativePlantData(plant); const char* next="idle";
    FusionScaredyFearTiming fear;
    if (TryGetFusionScaredyFearTiming(d->kind,d->animationSpeed,fear) &&
        (plant->mState==STATE_SCAREDYSHROOM_LOWERING || plant->mState==STATE_SCAREDYSHROOM_RAISING))
    {
        const bool lower=plant->mState==STATE_SCAREDYSHROOM_LOWERING;
        state=lower?"scared":"scaredgrow";
        elapsed=(std::max)(0,(lower?fear.lowering:fear.raising)-plant->mStateCountdown)*.01f;
        return true;
    }
    if (p->family==FusionBatchActionFamily::Doom || p->family==FusionBatchActionFamily::Jalapeno) next="explode";
    else if (p->family==FusionBatchActionFamily::Scaredy && plant->mState==STATE_SCAREDYSHROOM_SCARED) next="scaredidle";
    else if (p->family==FusionBatchActionFamily::ManualFume && d->shootSeconds<0 && plant->mLaunchCounter>0) next="rest";
    else if (p->family==FusionBatchActionFamily::Squash)
    {
        if (plant->mState==STATE_SQUASH_LOOK) next=plant->mTargetX<plant->mX?"lookleft":"lookright";
        else if (plant->mState==STATE_SQUASH_PRE_LAUNCH || plant->mState==STATE_SQUASH_RISING) next="jump";
        else if (plant->mState==STATE_SQUASH_FALLING || plant->mState==STATE_SQUASH_DONE_FALLING) next="jumppop";
    }
    else if (d->shootSeconds>=0) { const auto* t=Timing(plant); if (t) next=t->visualState; }
    state=next; elapsed=d->shootSeconds>=0?d->shootSeconds:d->idleSeconds;
    if (p->family==FusionBatchActionFamily::Squash && plant->mState==STATE_SQUASH_RISING)
        elapsed+=.55f/d->animationSpeed; // AnimMove does not restart the source jump clip.
    return std::isfinite(elapsed) && elapsed>=0;
}
bool IsNativeBatchProjectile(const Projectile* p)
{
    const auto* d=GetNativeProjectileData(p); if (!d) return false;
    const bool batch=d->kind==13 || (d->kind==9 && p->mMotionType==MOTION_STRAIGHT) ||
        (d->kind==0 && (d->specialType==kFusionThreeUpper || d->specialType==kFusionThreeLower));
    return batch && NativeProjectileMetadataValid(*d,p->mProjectileType,p->mMotionType);
}
bool GetNativeBatchProjectileCenter(const Projectile* p,float& x,float& y)
{
    if (!IsNativeBatchProjectile(p)) return false;
    x=p->mPosX+20; y=p->mPosY+p->mPosZ+20;
    return std::isfinite(x) && std::isfinite(y);
}
bool MoveNativeBatchProjectile(Projectile* p)
{
    if (!IsNativeBatchProjectile(p)) return false;
    if (!Host(p) || p->mDead) return true;
    if (NativeBatchProjectileExpired(p)) { p->Die(); return true; }
    auto* d=GetNativeProjectileData(p); float dy=0;
    if (d->specialType && TryGetFusionThreeVerticalStep(d->specialType,(std::max)(p->mProjectileAge-1,0),dy))
    {
        const int from=(std::max)(0,(std::min)(p->mRow,GetNativeFusionRowCount(p->mBoard)-2));
        const int pitch=p->mBoard->GridToPixelY(0,from+1)-p->mBoard->GridToPixelY(0,from);
        dy=static_cast<float>(double(dy)*pitch/100.);
    }
    p->mVelY=dy; p->mPosX+=p->mVelX; p->mPosY+=dy; p->mShadowY+=dy;
    return true;
}
bool ApplyNativeBatchProjectileImpact(Projectile* p,Zombie* z)
{
    if (!IsNativeBatchProjectile(p)) return false;
    if (!Host(p) || p->mDead || (z && (!Normal(z) || NativeWater268ZombieCaptured(z) || z->mRow!=p->mRow))) return true;
    if (IsNativeHotProjectile(p)) return ApplyNativeFireImpact(p,z);
    const auto data=*GetNativeProjectileData(p);
    if (z && data.kind==13)
    {
        auto* action=GetOrCreateNativeZombieActionData(z); uint8_t after=0; bool control=false;
        if (!action || !TryAdvanceFusionPuffColors(action->colorMask,data.effectVariant,after,control)) return true;
        action->colorMask=after;
        if (control) { z->StopEating(); z->StartMindControlled(); z->TrySpawnLevelAward(); }
        else z->TakeDamage(data.damage,0U);
        LawnNet::NetTrace("Fusion native Batch color projectile=%u target=%u color=%u mask=%u hypnotize=%d",p->mNetId,z->mNetId,data.effectVariant,after,control?1:0);
    }
    else if (z)
    {
        const int body=z->mBodyHealth,helm=z->mHelmHealth,shield=z->mShieldHealth;
        z->TakeDamage(data.damage,0U);
        LawnNet::NetTrace("Fusion native Batch impact projectile=%u kind=%u target=%u row=%d pool=%d damage=%d body=%d->%d helm=%d->%d shield=%d->%d tick=%d",
            p->mNetId,data.kind,z->mNetId,z->mRow,z->mInPool?1:0,data.damage,body,z->mBodyHealth,helm,z->mHelmHealth,shield,z->mShieldHealth,p->mBoard->mMainCounter);
    }
    p->PlayImpactSound(z); Particle(p->mBoard,p->mPosX+20,p->mPosY+p->mPosZ+20,data.kind?PARTICLE_PUFF_SPLAT:PARTICLE_PEA_SPLAT); p->Die();
    return true;
}
bool GetNativeBatchProjectileRect(const Projectile* p,Sexy::Rect& rect)
{
    if (!IsNativeBatchProjectile(p)) return false;
    if (GetNativeProjectileData(p)->kind==0) return false; // Reuse the existing native Pea collision rectangle.
    // Native collider adaptation; the existing logical40x40 centre is retained.
    FusionBaseRect r;
    if (!TryMakeFusionPuffRect(p->mPosX+20,p->mPosY+p->mPosZ+20,r)) { rect=Sexy::Rect(); return true; }
    const int x=static_cast<int>(std::floor(r.x)),y=static_cast<int>(std::floor(r.y));
    rect=Sexy::Rect(x,y,static_cast<int>(std::ceil(r.x+r.width))-x,static_cast<int>(std::ceil(r.y+r.height))-y); return true;
}
bool NativeBatchProjectileCanHit(const Projectile* p,Zombie* z)
{ return IsNativeBatchProjectile(p) && Normal(z) && !NativeWater268ZombieCaptured(z) && z->mBoard==p->mBoard && z->mRow==p->mRow; }
bool NativeBatchProjectileExpired(const Projectile* p)
{
    if (!IsNativeBatchProjectile(p) || !Host(p)) return false;
    auto* b=p->mBoard;
    const double x=(p->mPosX+20-b->GetCellCenterX(0,0))/kFusionBasePixelsX-4.8;
    const double pixelsY=GetNativeFusionPixelsY(b);
    const double y=2.3-(p->mPosY+p->mPosZ+20-(b->GridToPixelY(0,0)+50))/pixelsY;
    const double bottom=2.3-(b->GridToPixelY(0,GetNativeFusionRowCount(b)-1)-b->GridToPixelY(0,0))/pixelsY-.6;
    return FusionCherryProjectileExpired(x,y,p->mProjectileAge,-10,1.35*(b->GetColumnCount()-1)-4.8+1.85,bottom,6.5);
}
}
