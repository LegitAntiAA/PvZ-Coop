#include "FusionNativeProjectileBuff369.h"
#include "FusionNativeLobber.h"
#include "FusionNative.h"
#include "FusionNativeMorePlants269Backend.h"
#include "FusionNativeShooterTiming.h"
#include "FusionNativeCold.h"
#include "FusionNativePoison.h"
#include "FusionNativeFire.h"
#include "FusionNativeButter.h"
#include "FusionNativeMoney.h"
#include "FusionNativeDefender.h"
#include "FusionNativeVisual.h"
#include "FusionNativeProjectileMetadata.h"
#include "FusionNativePlantDamage.h"
#include "FusionNativeUmbrellaBatch.h"
#include "FusionNativeWater268.h"
#include "FusionNativeCannon268.h"
#include "FusionNativeSolar269.h"
#include "FusionNativeHostileProjectile.h"
#include "FusionButterRules.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../Coin.h"
#include "../Net/NetCommon.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include <cmath>
#include <vector>
#include <limits>

namespace LawnFusion
{
namespace
{
bool Host(const GameObject* object)
{
    return object && object->mApp && object->mBoard && object->mApp->mBoard == object->mBoard &&
        !object->mNetGhost && !object->mApp->NetIsClient() && IsNativeFusionBoard(object->mBoard) &&
        NativeFusionBackgroundValid(static_cast<uint8_t>(object->mBoard->mBackground));
}
bool LivePlant(Plant* plant)
{ return IsNativeLobber(plant) && Host(plant) && !plant->mDead && plant->mIsOnBoard && plant->mPlantHealth > 0; }
bool Normal(Zombie* target)
{
    return target && !target->mDead && !target->mNetGhost && IsNativeFusionGroundZombie(target) &&
        !target->IsDeadOrDying() && target->mBodyHealth > 0 && !target->mMindControlled && !target->IsFlying() &&
        !NativeWater268ZombieCaptured(target);
}
FusionLobberRect TargetRect(Zombie* target)
{
    const auto r = target->GetZombieRect();
    return {static_cast<double>(r.mX),static_cast<double>(r.mY),static_cast<double>(r.mWidth),static_cast<double>(r.mHeight)};
}
bool Overlaps(const Sexy::Rect& a, const FusionLobberRect& b)
{ return a.mX <= b.x+b.width && a.mX+a.mWidth >= b.x && a.mY <= b.y+b.height && a.mY+a.mHeight >= b.y; }
bool Frozen(const Zombie* target) { return target->mIceTrapCounter > 0; }
Plant* RedirectUmbrella(Plant* source,float firstTargetX)
{
    Plant* p=nullptr;Plant* first=nullptr;
    while(source->mBoard->IteratePlants(p))
    {
        const auto* d=GetNativePlantData(p);float x=0,y=0;
        // Thrower.AnimShoot scans the actual cell columns from its own column;
        // source+1B0 is the captured zombie B0.x, not the shooter's root X.
        const bool relay=d&&(d->kind==1128||d->kind==916||(IsNativeSolar269(source)&&FusionSolar269RelayKind(d->kind)));
        if(p->mDead||!p->mIsOnBoard||p->mPlantHealth<=0||!relay||p->mRow!=source->mRow||
            p->mPlantCol<source->mPlantCol||!p->GetShadowCenter(x,y)||x>=firstTargetX)continue;
        if(!first||p->mPlantCol<first->mPlantCol)first=p;
    }
    return first;
}
void Retire(Projectile* projectile)
{
    if (auto* context = GetNativeContext(projectile->mBoard))
    {
        const auto id = projectile->mBoard->mProjectiles.DataArrayGetID(projectile);
        context->rollingHits.erase(id); context->lobberPierceLimits.erase(id);context->lobberPlantTargets.erase(id);
    }
    projectile->Die();
}
int CapturedLimit(Projectile* projectile, const FusionLobberProjectileTraits& traits)
{
    if (traits.fixedPierceLimit == 1) return 1;
    const auto* context = GetNativeContext(projectile->mBoard);
    if (!context) return 0;
    const auto found = context->lobberPierceLimits.find(projectile->mBoard->mProjectiles.DataArrayGetID(projectile));
    if (found == context->lobberPierceLimits.end() || found->second < 2 || (traits.kind != 63 && found->second > 6) ||
        (traits.kind != 63 && traits.fixedPierceLimit && found->second != traits.fixedPierceLimit)) return 0;
    return found->second;
}
void EmitFiveSun(Projectile* projectile, bool ground)
{
    if (projectile->mBoard->mCoins.mSize >= projectile->mBoard->mCoins.mMaxSize)
    {
        LawnNet::NetTrace("Fusion native Lobber sun capacity omitted projectile=%u ground=%d value=5",projectile->mNetId,ground?1:0);
        return; // Explicit native capacity adaptation: consume this callback, never write the ledger.
    }
    auto* coin = projectile->mBoard->AddCoin(static_cast<int>(projectile->mPosX),
        static_cast<int>(projectile->mPosY+projectile->mPosZ),COIN_SMALLSUN,COIN_MOTION_FROM_PLANT,SEED_NONE,5);
    if (coin) LawnNet::NetTrace("Fusion native Lobber sun projectile=%u coin=%u value=%d ground=%d tick=%d",
        projectile->mNetId,coin->mNetId,GetNativeCoinValue(coin,0),ground?1:0,projectile->mBoard->mMainCounter);
}
}

namespace
{
Projectile* EmitLobberChild(Board* board,int row,float baseline,const FusionLobberLaunch& launch,
    uint16_t kind,int damage,bool enhanced,uint8_t variant=0)
{
    const auto* profile=FindFusionLobberProjectileProfile(kind);
    if(!profile || board->mProjectiles.mSize>=board->mProjectiles.mMaxSize || !EnsureNativeProjectileVisuals(board,kind))return nullptr;
    auto* p=board->AddProjectile(int(launch.spawn.x-20),int(launch.spawn.y-20),Board::MakeRenderOrder(RENDER_LAYER_PROJECTILE,row,0),row,profile->nativeType);
    if(!p)return nullptr;
    NativeProjectileData d;d.kind=kind;d.damage=damage;d.melonEnhanced=enhanced;d.effectVariant=variant;
    p->mMotionType=MOTION_LOBBED;
    if(!AttachNativeProjectile(board,p,d)){p->Die();return nullptr;}
    p->mPosX=float(launch.spawn.x-20);p->mPosY=baseline-20;p->mPosZ=float(launch.spawn.y-baseline);
    p->mVelX=float(launch.velocityX);p->mVelY=0;p->mVelZ=float(launch.velocityY);p->mAccZ=float(launch.accelerationY);
    p->mShadowY=float(baseline+.1*GetNativeFusionPixelsY(board));p->mRotation=p->mRotationSpeed=0;
    p->mX=int(p->mPosX);p->mY=int(p->mPosY+p->mPosZ);
    int limit=0;
    if(TryGetFusionLobberPierceLimit(kind,0,limit)&&limit>1)GetNativeContext(board)->lobberPierceLimits[board->mProjectiles.DataArrayGetID(p)]=limit;
    return p;
}
void EmitNativeSuperMelonChildren(Projectile* parent,float x,float y)
{
    const auto data=*GetNativeProjectileData(parent);
    const float baseline=parent->mPosY+20;
    FusionLobberLaunch launch;launch.spawn={x,y-.3*GetNativeFusionPixelsY(parent->mBoard)};launch.accelerationY=parent->mAccZ;
    launch.velocityX=parent->mVelX*.5f;launch.velocityY=-std::abs(parent->mVelZ)*.5f;
    EmitLobberChild(parent->mBoard,parent->mRow,baseline,launch,70,100,data.melonEnhanced);
    const bool butter=RandRangeInt(0,data.effectVariant?1:3)==1;
    launch.velocityX=parent->mVelX*.3f;launch.velocityY=-std::abs(parent->mVelZ)*.3f;
    EmitLobberChild(parent->mBoard,parent->mRow,baseline,launch,butter?67:66,butter?120:100,data.melonEnhanced);
    LawnNet::NetTrace("Fusion native SuperMelon split projectile=%u second=%d p=%d tick=%d",parent->mNetId,butter?67:66,data.effectVariant?2:4,parent->mBoard->mMainCounter);
}
}
Projectile* EmitNativeLobberChild(Board* board,int row,float baseline,const FusionLobberLaunch& launch,
    uint16_t kind,int damage,bool enhanced,uint8_t variant)
{
    return EmitLobberChild(board,row,baseline,launch,kind,damage,enhanced,variant);
}
bool TryActivateNativeGoldCabbage(Plant* plant)
{
    if(!LivePlant(plant)||GetNativePlantData(plant)->kind!=1135||plant->mShootingCounter>0)return false;
    auto* data=GetNativePlantData(plant);
    if(!BeginNativeShooterShot(plant,*FindFusionExtendedLobberTimingProfile(1135,"supershoot")))return false;
    data->batchMode=1;return true;
}
void FireNativeGoldCabbageVolley(Plant* plant)
{
    if(!LivePlant(plant)||GetNativePlantData(plant)->kind!=1135)return;
    float x=0,y=0;if(!plant->GetShadowCenter(x,y))return;
    const auto* profile=FindFusionLobberProfile(1135);
    const double pixelsY=GetNativeFusionPixelsY(plant->mBoard);
    const FusionLobberPoint muzzle{x+profile->muzzleX*kFusionLobberPixelsX,y-profile->muzzleY*pixelsY};
    Zombie* target=nullptr;int count=0;
    while(plant->mBoard->IterateZombies(target))
    {
        if(!Normal(target)||GetNativeFusionZombieStatus(target)==16||!FusionLobberSplashCanAffect(GetNativeFusionZombieStatus(target),0,false))continue;
        const auto rect=TargetRect(target);
        const FusionLobberPoint current{rect.x+rect.width*.5,rect.y};
        const FusionLobberPoint sample{current.x+(target->IsImmobilizied()||target->mIsEating?0:target->mVelX),current.y}; // Actual native leftward velocity, one tick sample.
        FusionLobberLaunch launch;
        if(TryMakeFusionLobberLaunch(muzzle,sample,current,.01,nullptr,launch,pixelsY)&&
            EmitLobberChild(plant->mBoard,target->mRow,y,launch,75,160,false,1))++count;
    }
    LawnNet::NetTrace("Fusion native GoldCabbage volley plant=%u count=%d miusa=1 damage=160 tick=%d",plant->mNetId,count,plant->mBoard->mMainCounter);
}

bool IsNativeLobber(const Plant* plant)
{
    const auto* data = GetNativePlantData(plant);
    return data && FindFusionLobberProfile(data->kind) && GetNativeSeedForPlant(data->kind) != SEED_NONE &&
        GetNativeSeedForPlant(data->kind) == plant->mSeedType;
}
const FusionShooterTimingProfile* GetNativeLobberTimingProfile(const Plant* plant)
{
    if (!IsNativeLobber(plant)) return nullptr;
    const auto kind = GetNativePlantData(plant)->kind;
    if(kind==934&&GetNativePlantData(plant)->batchMode)return FindFusionSolar269Timing("super");
    if(kind==1135 && GetNativePlantData(plant)->batchMode)return FindFusionExtendedLobberTimingProfile(kind,"supershoot");
    if((kind==1139||kind==1142)&&GetNativePlantData(plant)->batchMode)
        return FindFusionCannon268LobberTiming(kind,kind==1139?"shoot3":"shoot2");
    return FindFusionShooterTimingProfile(kind,FusionLobberCornDenominator(kind) && plant->mState == STATE_KERNELPULT_BUTTER ? "shoot2" : "shoot");
}
void InitializeNativeLobber(Plant* plant)
{
    if (!LivePlant(plant)) return;
    const auto* data = GetNativePlantData(plant);
    const auto* timing = FindFusionShooterTimingProfile(data->kind);
    if (!timing) return;
    auto initial = *timing;
    initial.attackIntervalSeconds = data->kind==934?GetNativeSolar269LobberInterval(plant):GetNativeDefenderLobberInterval(plant,timing->attackIntervalSeconds);
    if (!InitializeNativeShooterTiming(plant,initial,false)) return;
    if (FusionLobberCornDenominator(data->kind)) plant->mState = STATE_NOTREADY;
    if(data->kind==1126)plant->mLaunchCounter=0;
    if(data->kind==934){auto* d=GetNativePlantData(plant);d->batchMode=0;d->batchRageTicks=d->batchBagCount=0;plant->mTargetX=0;}
    if(FindFusionCannon268LobberProfile(data->kind))
    {
        plant->mStateCountdown=0;auto* state=GetNativePlantData(plant);
        state->batchMode=0;state->batchShotCount=0;state->batchRageTicks=state->batchBagCount=0;
    }
    LawnNet::NetTrace("Fusion native Lobber init id=%u kind=%u speed=%.9g launch=%d tick=%d",
        plant->mNetId,data->kind,data->animationSpeed,plant->mLaunchCounter,plant->mBoard->mMainCounter);
}
bool ResetNativeLobberLaunchCounter(Plant* plant)
{
    const auto* data = GetNativePlantData(plant);
    const auto* timing = data ? FindFusionShooterTimingProfile(data->kind) : nullptr;
    if (!LivePlant(plant) || !timing) return false;
    const float interval = data->kind==934?GetNativeSolar269LobberInterval(plant):GetNativeDefenderLobberInterval(plant,timing->attackIntervalSeconds);
    return TryGetFusionLobberCycleTicks(data->kind,RandRangeFloat(-.1f,.1f),plant->mLaunchCounter,interval);
}
void BeginNativeLobberShot(Plant* plant, Zombie* target)
{
    if (!LivePlant(plant) || !Normal(target) || target->mBoard != plant->mBoard) return;
    if(GetNativePlantData(plant)->batchMode&&(GetNativePlantData(plant)->kind==1135||
        GetNativePlantData(plant)->kind==1139||GetNativePlantData(plant)->kind==1142))return;
    const auto* data = GetNativePlantData(plant);
    const auto* timing = FindFusionShooterTimingProfile(data->kind);
    const auto rect = TargetRect(target);
    if (!timing || rect.width <= 0 || rect.height <= 0) return;
    const int denominator = GetNativeDefenderCornDenominator(plant,FusionLobberCornDenominator(data->kind));
    if (denominator)
    {
        const auto* alternate = FindFusionShooterTimingProfile(data->kind,"shoot2"); int counter = 0;
        if (!alternate || !TryGetFusionShooterShotCounter(*timing,data->animationSpeed,counter) ||
            !TryGetFusionShooterShotCounter(*alternate,data->animationSpeed,counter)) return;
        bool chosen = false;
        if (!TryChooseFusionLobberAlternate(data->kind,RandRangeInt(0,denominator-1),chosen)) return;
        plant->mState = chosen ? STATE_KERNELPULT_BUTTER : STATE_NOTREADY;
        timing = chosen ? alternate : timing;
    }
    if (!BeginNativeShooterShot(plant,*timing)) return;
    NativeLobberAim aim;
    aim.targetGeneration = static_cast<uint32_t>(plant->mBoard->ZombieGetID(target));
    aim.sampleTick = plant->mBoard->mMainCounter;
    aim.targetCenterX = static_cast<float>(rect.x+rect.width*.5);
    aim.targetTopY = static_cast<float>(rect.y);
    if(data->kind==934)plant->mTargetZombieID=plant->mBoard->ZombieGetID(target);
    GetNativeContext(plant->mBoard)->lobberAims[data->instanceToken] = aim;
    LawnNet::NetTrace("Fusion native Lobber trigger plant=%u kind=%u target=%u generation=%u sampleTick=%d b0=%.6f,%.6f shootCounter=%d speed=%.9g state=%s",
        plant->mNetId,data->kind,target->mNetId,aim.targetGeneration,aim.sampleTick,aim.targetCenterX,aim.targetTopY,
        plant->mShootingCounter,data->animationSpeed,timing->visualState);
}
void UpdateNativeLobberPhase(Plant* plant)
{
    if (!LivePlant(plant)) return;
    if (const auto* timing = GetNativeLobberTimingProfile(plant))
    {
        UpdateNativeShooterPhase(plant,*timing);
        if(GetNativePlantData(plant)->kind==1135 && GetNativePlantData(plant)->shootSeconds<0)GetNativePlantData(plant)->batchMode=0;
        const auto* data = GetNativePlantData(plant);
        if (FusionLobberCornDenominator(data->kind) && data->shootSeconds < 0 && !plant->mShootingCounter) plant->mState = STATE_NOTREADY;
    }
}
Zombie* FindNativeLobberTarget(Plant* plant, int row)
{
    if (!LivePlant(plant)) return nullptr;
    float x = 0, y = 0;
    if (!plant->GetShadowCenter(x,y)) return nullptr;
    const double maximum = plant->mBoard->GetCellCenterX(plant->mBoard->GetColumnCount()-1,row)+1.85*kFusionLobberPixelsX;
    Zombie* target = nullptr; Zombie* closest = nullptr; float distance = 0;
    while (plant->mBoard->IterateZombies(target))
    {
        float tx = 0, ty = 0;
        if (!Normal(target) || !target->GetShadowCenter(tx,ty) ||
            !FusionLobberCanAcquire(GetNativeFusionZombieStatus(target),target->mRow==row,target->mMindControlled,target->IsDeadOrDying(),x,tx,maximum)) continue;
        if (!closest || tx-x < distance) { closest = target; distance = tx-x; }
    }
    return closest;
}
bool FireNativeLobberProjectile(Plant* plant)
{
    if (!IsNativeLobber(plant)) return false;
    if (!LivePlant(plant)) return true;
    if(HandleNativeSolar269LobberEvent(plant))return true;
    if(FireNativeCannon268Money(plant))return true;
    if(GetNativePlantData(plant)->kind==1135 && GetNativePlantData(plant)->batchMode)
    {
        FireNativeGoldCabbageVolley(plant);return true;
    }
    auto* context = GetNativeContext(plant->mBoard);
    const auto* data = GetNativePlantData(plant);
    const auto found = context->lobberAims.find(data->instanceToken);
    if (found == context->lobberAims.end()) return true;
    NativeLobberAim aim = found->second;
    const bool solar=data->kind==934;
    if(solar){int initial=0;if(TryGetFusionShooterShotCounter(*FindFusionSolar269Timing(),data->animationSpeed,initial))GetNativePlantData(plant)->shootSeconds=(initial-plant->mShootingCounter)*.01f;}
    if(!solar)context->lobberAims.erase(found); // Legacy one-event ownership remains unchanged.
    const auto* profile = FindFusionLobberProfile(data->kind,FusionLobberCornDenominator(data->kind) && plant->mState == STATE_KERNELPULT_BUTTER);
    const uint16_t payload=solar?FusionSolar269Projectile(NativeSolar269TravelUltimate22(plant)):profile?profile->projectileKind:0xffff;
    const auto* traits = profile ? FindFusionLobberProjectileTraits(payload) : nullptr;
    int pierceLimit = 0;
    if (!traits || !TryGetFusionLobberPierceLimit(payload,plant->mBoard->mSunMoney,pierceLimit)) return true;
    float sx = 0, sy = 0;
    if (!plant->GetShadowCenter(sx,sy)) return true;
    const double pixelsY=GetNativeFusionPixelsY(plant->mBoard);
    // Source gives a live CabbageUmbrella first refusal at AnimShoot. Both
    // samples are its current Shadow+.75, so glove movement is not predicted.
    if(auto* receiver=RedirectUmbrella(plant,aim.targetCenterX))
    {
        float x=0,y=0;receiver->GetShadowCenter(x,y);
        aim.targetPlant=true;aim.targetGeneration=plant->mBoard->mPlants.DataArrayGetID(receiver);
        aim.targetCenterX=x;aim.targetTopY=y-float(.75*pixelsY);
    }
    FusionLobberPoint a{sx+profile->muzzleX*kFusionLobberPixelsX,sy-profile->muzzleY*pixelsY};
    if(FindFusionCannon268LobberProfile(data->kind)||solar)
    {float x=0,y=0;if(!GetNativeSourcePlantNodePoint(plant,"Shoot",x,y))return true;a={x,y};}
    const FusionLobberPoint b0{aim.targetCenterX,aim.targetTopY};
    FusionLobberPoint b1 = b0; FusionLobberRect rect; const FusionLobberRect* liveRect = nullptr;
    // Do not recheck HP, status, row or target eligibility at emission. Only
    // the exact full-generation actor/collider identity can update B1.
    Zombie* target = aim.targetPlant?nullptr:solar?plant->mBoard->ZombieTryToGet(plant->mTargetZombieID):plant->mBoard->mZombies.DataArrayTryToGet(aim.targetGeneration);
    if (target && target->mBoard == plant->mBoard && !NativeWater268ZombieCaptured(target))
    {
        rect = TargetRect(target);
        if (rect.width > 0 && rect.height > 0) { b1 = {rect.x+rect.width*.5,rect.y}; liveRect = &rect; }
    }
    const double elapsed = (static_cast<int64_t>(plant->mBoard->mMainCounter)-aim.sampleTick)*.01;
    FusionLobberLaunch launch;
    bool solved=false;
    if(solar)
    {
        // Solar owns source flight1 and reads actual current native velocity;
        // relay/fallback use the proved Vector2.zero, never global mouse input.
        FusionLobberPoint velocity{};
        if(target&&!target->IsImmobilizied()&&!target->mIsEating)
            velocity.x=(target->IsWalkingBackwards()?1:-1)*target->mVelX*100*(target->IsMovingAtChilledSpeed()?CHILLED_SPEED_FACTOR:1)*plant->mBoard->GetPvPZombieSpeedMultiplier()*GetNativeWater268GrapSpeedFactor(target);
        solved=TryMakeFusionLobberVelocityLaunch(a,b1,velocity,1,liveRect,launch,pixelsY);
    }
    else solved=TryMakeFusionLobberLaunch(a,b0,b1,elapsed,liveRect,launch,pixelsY);
    if (!solved ||
        plant->mBoard->mProjectiles.mSize >= plant->mBoard->mProjectiles.mMaxSize) return true;
    Projectile* projectile = plant->mBoard->AddProjectile(static_cast<int>(launch.spawn.x-20),
        static_cast<int>(launch.spawn.y-20),Board::MakeRenderOrder(RENDER_LAYER_PROJECTILE,plant->mRow,0),plant->mRow,profile->nativeType);
    if (!projectile) return true;
    NativeProjectileData bullet; bullet.kind = payload; bullet.damage = profile->damage; bullet.sunOnHit = traits->sunReward ? 5 : 0;
    if(solar)bullet.specialType=13;
    if(data->kind==1126)
    {
        bullet.effectVariant=GetNativeDefenderCornDenominator(plant,4)==2?1:0;
        if(RandRangeInt(0,99)<42)plant->mLaunchCounter=11;
    }
    bullet.melonEnhanced = traits->splash && GetNativeDefenderMelonEnhanced(plant); // Source trait captured on emission, not impact.
    projectile->mMotionType = MOTION_LOBBED;
    if (!AttachNativeTravelProjectile369(plant,projectile,bullet)) { projectile->Die(); return true; }
    if(aim.targetPlant)context->lobberPlantTargets[plant->mBoard->mProjectiles.DataArrayGetID(projectile)]=aim.targetGeneration;
    if (pierceLimit > 1) context->lobberPierceLimits[plant->mBoard->mProjectiles.DataArrayGetID(projectile)] = pierceLimit;
    projectile->mPosX = static_cast<float>(launch.spawn.x-20);
    projectile->mPosY = sy-20; // Constant source Shadow baseline, reused native height field.
    projectile->mPosZ = static_cast<float>(launch.spawn.y-sy);
    projectile->mVelX = static_cast<float>(launch.velocityX); projectile->mVelY = 0;
    projectile->mVelZ = static_cast<float>(launch.velocityY); projectile->mAccZ = static_cast<float>(launch.accelerationY);
    projectile->mShadowY = static_cast<float>(sy+.1*pixelsY);
    if (traits->sourceRotation) projectile->mRotation = projectile->mRotationSpeed = 0;
    projectile->mX = static_cast<int>(projectile->mPosX); projectile->mY = static_cast<int>(projectile->mPosY+projectile->mPosZ);
    if(aim.targetPlant)LawnNet::NetTrace("Fusion native Lobber plant aim plant=%u projectile=%u targetGeneration=%u b0=%.6f,%.6f b1=%.6f,%.6f pixelsY=%.9g",plant->mNetId,projectile->mNetId,aim.targetGeneration,b0.x,b0.y,b1.x,b1.y,pixelsY);
    if(FindFusionCannon268LobberProfile(data->kind))++GetNativePlantData(plant)->batchShotCount;
    NativeCannon268LobberEmitted(plant,projectile);
    if(solar){++GetNativePlantData(plant)->batchShotCount;auto* next=FindNativeLobberTarget(plant,plant->mRow);plant->mTargetZombieID=next?plant->mBoard->ZombieGetID(next):ZOMBIEID_NULL;}
    plant->mApp->PlayFoley(FOLEY_THROW);
    LawnNet::NetTrace("Fusion native Lobber fire plant=%u kind=%u projectile=%u payload=%u damage=%d targetGeneration=%u tick=%d sampleTick=%d targetAvailable=%d a=%.6f,%.6f b0=%.6f,%.6f b1=%.6f,%.6f spawn=%.6f,%.6f vx=%.9g vz=%.9g acc=%.9g baseline=%.6f pierceLimit=%d wallet=%d",
        plant->mNetId,data->kind,projectile->mNetId,bullet.kind,bullet.damage,aim.targetGeneration,plant->mBoard->mMainCounter,aim.sampleTick,
        liveRect ? 1 : 0,a.x,a.y,b0.x,b0.y,b1.x,b1.y,launch.spawn.x,launch.spawn.y,projectile->mVelX,projectile->mVelZ,projectile->mAccZ,sy,pierceLimit,plant->mBoard->mSunMoney);
    return true;
}
bool IsNativeLobberProjectile(const Projectile* projectile)
{
    const auto* data = GetNativeProjectileData(projectile);
    if(!data||data->isZombieBullet||data->hitLatched||!FindFusionLobberProjectileTraits(data->kind))return false;
    if(FusionMore269PlanarLobber(data->kind)&&IsNativeMore269Projectile(projectile))return true;
    if((data->kind==59&&data->specialType==13)||data->umbrellaed)
        return NativeProjectileMetadataValid(*data,projectile->mProjectileType,projectile->mMotionType);
    if((data->kind==53||data->kind==75)&&data->specialType==13)
        return NativeProjectileMetadataValid(*data,projectile->mProjectileType,projectile->mMotionType);
    return data && !data->specialType && !data->fireState && !data->lastTorchId && !data->lastTorchToken &&
        FusionLobberProjectileMetadataValid(data->kind,NativeProjectileValidationDamage369(*data),data->sunOnHit,projectile->mProjectileType);
}
bool SetNativeLobberRetarget(Projectile* p,int row,float vx,float vy,float gravity)
{
    if(!IsNativeLobberProjectile(p)||!Host(p)||p->mDead||!std::isfinite(vx)||!std::isfinite(vy)||
        !std::isfinite(gravity)||gravity<=0||row<0||row>=(p->mBoard->StageHas6Rows()?6:5))return false;
    const bool planar=IsNativeSourcePlanarProjectile(p);
    const float shift=float(p->mBoard->GridToPixelY(0,row)-p->mBoard->GridToPixelY(0,p->mRow));
    // The world root remains PosY+PosZ; only its existing ground decomposition
    // moves to the retargeted lane. Planar bodies already store absolute PosY.
    if(planar)p->mVelY=vy;
    else {p->mPosY+=shift;p->mPosZ-=shift;p->mVelZ=vy;}
    p->mShadowY+=shift;p->mVelX=vx;p->mAccZ=gravity;p->mRow=row;
    p->mX=int(p->mPosX);p->mY=int(p->mPosY+p->mPosZ);
    return true;
}
bool GetNativeLobberProjectileCenter(const Projectile* projectile, float& x, float& y)
{
    if (!IsNativeLobberProjectile(projectile)) return false;
    const float nx = projectile->mPosX+20, ny = projectile->mPosY+projectile->mPosZ+20;
    if (!std::isfinite(nx) || !std::isfinite(ny)) return false;
    x = nx; y = ny; return true;
}
bool GetNativeLobberProjectileRect(const Projectile* projectile, Sexy::Rect& rect)
{
    if (!IsNativeLobberProjectile(projectile)) return false;
    float x = 0, y = 0; FusionLobberRect r;
    if (!GetNativeLobberProjectileCenter(projectile,x,y) || !TryMakeFusionLobberRect(GetNativeProjectileData(projectile)->kind,x,y,r,GetNativeFusionPixelsY(projectile->mBoard)))
    { rect = Sexy::Rect(0,0,0,0); return true; }
    const int left = static_cast<int>(std::floor(r.x)), top = static_cast<int>(std::floor(r.y));
    rect = Sexy::Rect(left,top,static_cast<int>(std::ceil(r.x+r.width))-left,static_cast<int>(std::ceil(r.y+r.height))-top);
    return true;
}
bool UpdateNativeLobberMotion(Projectile* projectile)
{
    if (!IsNativeLobberProjectile(projectile)) return false;
    if(MoveNativeMore269PlanarLobber(projectile))return true;
    if(IsNativeSourcePlanarProjectile(projectile))return false; // Actual source-planar58/59 owns its movement step.
    if (!Host(projectile) || projectile->mDead) return true;
    float x = 0, y = 0;
    if (!AdvanceFusionLobberMotion(projectile->mVelX,projectile->mAccZ,projectile->mPosX,projectile->mPosZ,projectile->mVelZ) ||
        !GetNativeLobberProjectileCenter(projectile,x,y)) { Retire(projectile); return true; }
    const auto* traits = FindFusionLobberProjectileTraits(GetNativeProjectileData(projectile)->kind);
    UpdateNativeCannon268LittleGoldRotation(projectile);
    if (GetNativeProjectileData(projectile)->kind!=80&&traits->sourceRotation) projectile->mRotation -= static_cast<float>(3.14159265358979323846/36.0); //−500°/s, once per native tick.
    const auto* board = projectile->mBoard;
    const double sourceX = (x-projectile->mBoard->GetCellCenterX(0,0))/kFusionLobberPixelsX-4.8;
    const double maximum = 1.35*(board->GetColumnCount()-1)-4.8+1.85+1;
    if (FusionLobberExpired(sourceX,projectile->mProjectileAge,-10,maximum)) { Retire(projectile); return true; }
    Sexy::Rect rect; GetNativeLobberProjectileRect(projectile,rect);
    auto* context=GetNativeContext(projectile->mBoard);
    const auto generation=projectile->mBoard->mProjectiles.DataArrayGetID(projectile);
    if(context)
    {
        const auto goal=context->lobberPlantTargets.find(generation);
        if(goal!=context->lobberPlantTargets.end())
        {
            auto* receiver=projectile->mBoard->mPlants.DataArrayTryToGet(goal->second);
            if(!receiver||receiver->mDead||!receiver->mIsOnBoard)context->lobberPlantTargets.erase(goal);
            else if(NativeSourcePlantColliderEnabled(receiver)&&rect.Intersects(receiver->GetPlantRect())&&
                TryEnhanceNativeUmbrellaProjectile(projectile,receiver))
            {
                // A source Emerald relay may have replaced the actual full-generation
                // target during this callback. Preserve that new reservation.
                const auto next=context->lobberPlantTargets.find(generation);
                if(next!=context->lobberPlantTargets.end()&&next->second==projectile->mBoard->mPlants.DataArrayGetID(receiver))context->lobberPlantTargets.erase(next);
                LawnNet::NetTrace("Fusion native Lobber plant contact projectile=%u target=%u generation=%u tick=%d",projectile->mNetId,receiver->mNetId,projectile->mBoard->mPlants.DataArrayGetID(receiver),projectile->mBoard->mMainCounter);
                return true;
            }
        }
    }
    if (traits->fixedPierceLimit != 1)
    {
        if (!CapturedLimit(projectile,*traits)) { Retire(projectile); return true; }
        std::vector<ZombieID> targets; Zombie* target = nullptr;
        while (projectile->mBoard->IterateZombies(target))
            if (Normal(target) && target->mRow == projectile->mRow && Overlaps(rect,TargetRect(target)))
                targets.push_back(projectile->mBoard->ZombieGetID(target));
        for (const auto id : targets)
            if (!projectile->mDead)
                if (auto* target = projectile->mBoard->ZombieTryToGet(id)) projectile->DoImpact(target);
        if (!projectile->mDead && FusionLobberGroundHit(projectile->mPosZ,projectile->mVelZ,GetNativeFusionPixelsY(projectile->mBoard))) projectile->DoImpact(nullptr);
        return true;
    }
    Zombie* target = nullptr; Zombie* hit = nullptr;
    while (projectile->mBoard->IterateZombies(target))
        if (Normal(target) && target->mRow == projectile->mRow && Overlaps(rect,TargetRect(target)) &&
            (!hit || target->mPosX < hit->mPosX)) hit = target;
    // Source colliders are active while ascending too. Vanilla lobbed landing
    // thresholds and descending-only collision must not run a second time.
    if (hit || FusionLobberGroundHit(projectile->mPosZ,projectile->mVelZ,GetNativeFusionPixelsY(projectile->mBoard))) projectile->DoImpact(hit);
    return true;
}
bool ApplyNativeLobberImpact(Projectile* projectile, Zombie* primary)
{
    if (!IsNativeLobberProjectile(projectile)) return false;
    if (!Host(projectile) || projectile->mDead) return true;
    if (primary && (!Normal(primary) || primary->mBoard != projectile->mBoard || primary->mRow != projectile->mRow)) return true;
    if(ApplyNativeCannon268LittleGoldImpact(projectile))return true;
    const auto* data = GetNativeProjectileData(projectile);
    const uint16_t kind = data->kind; const int base = data->damage;
    const auto& traits = *FindFusionLobberProjectileTraits(kind); const bool cold = traits.cold;
    const int limit = CapturedLimit(projectile,traits); int hits = 0;
    if (!limit) { Retire(projectile); return true; }
    if(primary && kind==63)
    {
        auto* c=GetNativeContext(projectile->mBoard);
        const auto found=c->rollingHits.find(projectile->mBoard->mProjectiles.DataArrayGetID(projectile));
        if(found!=c->rollingHits.end()&&found->second.count(projectile->mBoard->ZombieGetID(primary)))return true;
    }
    if(primary && kind==63 && GetNativePoisonLevel(primary)>0)
    {
        auto* c=GetNativeContext(projectile->mBoard);
        auto& captured=c->lobberPierceLimits[projectile->mBoard->mProjectiles.DataArrayGetID(projectile)];
        if(captured<(std::numeric_limits<int>::max)())++captured;
    }
    const int activeLimit=kind==63?CapturedLimit(projectile,traits):limit;
    if (primary && activeLimit > 1)
    {
        auto* context = GetNativeContext(projectile->mBoard);
        const uint32_t id = projectile->mBoard->mProjectiles.DataArrayGetID(projectile);
        if (!context || FusionProjectileHitCount(context->rollingHits,id) >= static_cast<std::size_t>(activeLimit)) { Retire(projectile); return true; }
        if (!RememberFusionRollingHit(context->rollingHits,id,projectile->mBoard->ZombieGetID(primary))) return true;
        hits = static_cast<int>(FusionProjectileHitCount(context->rollingHits,id)); // Source increments BEFORE callback.
    }
    float x = 0, y = 0;
    if (!GetNativeLobberProjectileCenter(projectile,x,y)) { Retire(projectile); return true; }
    projectile->PlayImpactSound(primary);
    if (traits.moneyItem)
    {
        const bool emitted = EmitNativeMoneyItem(projectile->mBoard,x,y,traits.moneyItem);
        LawnNet::NetTrace("Fusion native Lobber money projectile=%u payload=%u item=%u ground=%d emitted=%d beforeDamage=1 tick=%d",
            projectile->mNetId,kind,traits.moneyItem,primary?0:1,emitted?1:0,projectile->mBoard->mMainCounter);
    }
    const FusionColdDamageContext context{60};
    if (primary)
    {
        int damage = base;
        if (cold) TryGetFusionLobberColdDamage(base,Frozen(primary),damage);
        if(kind>=61&&kind<=64)TryGetFusionPoisonLobberDamage(kind,base,GetNativePoisonLevel(primary),false,damage);
        if(kind==75&&data->effectVariant && static_cast<float>(primary->mHelmHealth)+GetNativeFusionZombieHealth(primary)<=300.f)damage=1000000;
        primary->TakeDamage(damage,kind==64?2U:cold ? 2U : 1U,cold ? &context : nullptr);
        if(kind>=61&&kind<=64&&kind!=62)AddNativePoisonLevel(primary);
        if(kind==86)NativeWarmAfterDamage(primary);
        if (cold) AddNativeColdLevel(primary,50); // AFTER damage, even a lethal hit on an allocated actor.
        if (traits.butter) ApplyNativeFusionButter(primary,400,kind!=62); // Source direct butter follows primary damage.
        if(kind==62)SetNativePoison(primary,1000);
        LawnNet::NetTrace("Fusion native Lobber primary projectile=%u payload=%u target=%u damage=%d flags=%u coldAfter=%d tick=%d",
            projectile->mNetId,kind,primary->mNetId,damage,cold ? 2U : 1U,cold ? 50 : 0,projectile->mBoard->mMainCounter);
    }
    std::vector<ZombieID> targets;
    const double pixelsY=GetNativeFusionPixelsY(projectile->mBoard);
    const auto areaCenter = FusionLobberSplashCenter(x,y,pixelsY);
    if (traits.splash)
    {
        Zombie* target = nullptr;
        while (projectile->mBoard->IterateZombies(target))
            if (target != primary && Normal(target) && FusionLobberSplashCanAffect(0,target->mRow-projectile->mRow,target->mMindControlled) &&
                FusionLobberAreaTouchesRect(areaCenter.x,areaCenter.y,TargetRect(target),pixelsY)) targets.push_back(projectile->mBoard->ZombieGetID(target));
    }
    int splash = 0;
    int butterTicks = 0;
    if (traits.splash && traits.butter && !targets.empty()) TryGetFusionButterSplashTicks(static_cast<int>(targets.size()),butterTicks);
    if (!targets.empty() && TryGetFusionLobberSplashDamage(base,static_cast<int>(targets.size()),splash,data->melonEnhanced))
        for (const auto id : targets)
            if (auto* target = projectile->mBoard->ZombieTryToGet(id))
            {
                if (!Normal(target)) continue;
                if (cold) AddNativeColdLevel(target,5); // BEFORE current-hit frozen multiplier.
                int damage = splash;
                if (cold) TryGetFusionLobberColdDamage(splash,Frozen(target),damage);
                if(kind==64)TryGetFusionPoisonLobberDamage(kind,splash,GetNativePoisonLevel(target),true,damage);
                if(!ApplyNativeCannon268SplashDamage(projectile,target,damage))
                    target->TakeDamage(damage,2U,cold ? &context : nullptr);
                if(kind==64)AddNativePoisonLevel(target);
                if(kind==86)NativeWarmAfterDamage(target);
                if (butterTicks) ApplyNativeFusionButter(target,butterTicks); // NormalAll damage BEFORE distributed butter.
                LawnNet::NetTrace("Fusion native Lobber secondary projectile=%u payload=%u target=%u count=%zu base=%d damage=%d flags=2 coldBefore=%d tick=%d",
                    projectile->mNetId,kind,target->mNetId,targets.size(),splash,damage,cold ? 5 : 0,projectile->mBoard->mMainCounter);
            }
    if (primary && activeLimit > 1) TryApplyFusionLobberBounce(kind,hits,projectile->mVelX,projectile->mVelZ);
    if (traits.sunReward) EmitFiveSun(projectile,primary == nullptr);
    const auto effect = !traits.splash ? PARTICLE_CABBAGE_SPLAT : cold ? PARTICLE_WINTERMELON : PARTICLE_MELONSPLASH;
    projectile->mApp->AddTodParticle(x,y,projectile->mRenderOrder+1,effect);
    LawnNet::NetBroadcastParticle(projectile->mApp,x,y,projectile->mRenderOrder+1,effect);
    LawnNet::NetTrace("Fusion native Lobber impact projectile=%u payload=%u primary=%u count=%zu splash=%d age=%d center=%.6f,%.6f tick=%d areaCenter=%.6f,%.6f hits=%d limit=%d vx=%.9g vz=%.9g butter=%d sun=%d enhanced=%d",
        projectile->mNetId,kind,primary ? primary->mNetId : 0,targets.size(),splash,projectile->mProjectileAge,x,y,projectile->mBoard->mMainCounter,areaCenter.x,areaCenter.y,
        hits,limit,projectile->mVelX,projectile->mVelZ,butterTicks,traits.sunReward?5:0,data->melonEnhanced?1:0);
    if(primary && kind==71)EmitNativeSuperMelonChildren(projectile,x,y);
    NativeCannon268LobberChildren(projectile,primary);
    if (!primary || activeLimit == 1 || hits >= activeLimit) Retire(projectile);
    return true; // No post-primary award gate; finish this accepted splash once.
}
void NativeLobberPlantRemoved(Plant* plant)
{
    if (!plant) return;
    auto* context = GetNativeContext(plant->mBoard); const auto* data = GetNativePlantData(plant);
    if (context && data && FindFusionLobberProfile(data->kind)) context->lobberAims.erase(data->instanceToken);
}
}
