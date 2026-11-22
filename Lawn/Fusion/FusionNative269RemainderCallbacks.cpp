#include "FusionNativeTravelConsumers369.h"
#include "FusionNative269RemainderCallbacks.h"
#include "FusionBasePlantRules.h"
#include "FusionNative.h"
#include "FusionNativeEffects269.h"
#include "FusionNativeFire.h"
#include "FusionNativeSolar269.h"
#include "FusionNativeVisual.h"
#include "FusionNativeWater268.h"
#include "FusionNativeProjectileMetadata.h"
#include "FusionProducerRules.h"
#include "FusionNativeDroppedCards269.h"
#include "FusionNativePlacement.h"
#include "FusionNativeRollingActors269.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../Coin.h"
#include "../Net/NetCommon.h"
#include "../Net/NetPresentation.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "../../Sexy.TodLib/TodParticle.h"
#include <cmath>

namespace LawnFusion
{
namespace
{
bool BoardValid(const Board* b)
{return b&&b->mApp&&b->mApp->mBoard==b&&IsNativeFusionBoard(b)&&GetNativeContext(b);}
bool Host(Board* b)
{return BoardValid(b)&&!b->mApp->NetIsClient();}
bool Point(float x,float y)
{return std::isfinite(x)&&std::isfinite(y)&&std::abs(x)<=4096&&std::abs(y)<=4096;}
}
bool EnsureNativeWorldRemainder269Assets(Board* b,NativeWorldRemainder269Family family,uint8_t type)
{
    if(!BoardValid(b)||!EnsureNativeWorldRemainder269AuxVisual(b,family,type))return false;
    if(family==NativeWorldRemainder269Family::LourFly)
        return type==0&&EnsureNativeProjectileVisuals(b,91);
    if(family!=NativeWorldRemainder269Family::Meteorite||(type!=0&&type!=1&&type!=2&&type!=5))return false;
    int value=0;
    if(!TryGetFusionSourceSunValue(0,value)||value!=25)return false; // Native SUN image/value, not projectile0.
    const uint16_t kind=type==1?48:type==2||type==5?49:39;
    const uint16_t effect=type==2?51:type==5?98:49;
    return EnsureNativeProjectileVisuals(b,kind)&&EnsureNativeSourceEffect269(b,effect);
}
bool QueryNativeWorldRemainder269Solar(const Board* b,bool& present)
{
    if(!BoardValid(b))return false;
    // Normal-D2 has no Solar creator: the only native super-creation branch
    // is explicitly unavailable. This is known absence on this mode's board.
    if(QueryNativeSolar269WorldBranch(b)!=FusionSolar269WorldBranch::Unavailable||CanCreateNativeSolar269SuperParticle(b))return false;
    present=false;return true;
}
bool CreateNativeWorldRemainder269ImpactEffect(Board* b,uint16_t effect,float x,float y,int row)
{
    if(!Host(b)||!Point(x,y)||row<0||row>=GetNativeFusionRowCount(b)||(effect!=49&&effect!=51&&effect!=98)||
        !EnsureNativeSourceEffect269(b,effect))return false;
    return SpawnNativeSourceEffect269(b,effect,x,y,Board::MakeRenderOrder(RENDER_LAYER_PARTICLE,row,0),true)!=nullptr;
}
bool EmitNativeWorldRemainder269MeteoriteSun(Board* b,float x,float y,int count)
{
    int value=0;
    if(!Host(b)||!CanApplyNativeFusionGameplay(b)||!Point(x,y)||(count!=10&&count!=50)||
       !TryGetFusionSourceSunValue(0,value)||value!=25||
       !FusionProducerCanFitCallback(b->mCoins.mSize,b->mCoins.mMaxSize,count))return false;
    // BigStar.Crash453E60: both independent Range(-2,+2) draws precede SetCoin0.
    // ASM454063/4543C3, DLL1816AEBE8=-2 and1816AEA20=2; native motion keeps its
    // own cosmetic RNG. These are source birth positions, not coin fall jitter.
    for(int i=0;i<count;++i)
    {
        const float sx=x+RandRangeFloat(-2,2)*float(kFusionBasePixelsX);
        const float sy=y-RandRangeFloat(-2,2)*float(GetNativeFusionPixelsY(b));
        auto* c=b->AddCoin(int(sx-20),int(sy-20),COIN_SUN,COIN_MOTION_FROM_PLANT,SEED_NONE,value);
        if(!c||!AttachNativeCoinValue(b,c,value))return false;
    }
    LawnNet::NetTrace("Fusion World269 sourceSun item=0 count=%d value=25 x=%.9g y=%.9g tick=%d",count,x,y,b->mMainCounter);
    return true;
}
// Travel, shooting-level and balanced-event selections have no active source
// mode in native normal-D2. They are not inferred from vanilla challenge IDs.
bool HasNativeWorldRemainder269Trait(const Board* b,NativeWorldRemainder269Trait trait)
{
 switch(trait){case NativeWorldRemainder269Trait::Ultimate8:return HasNativeTravelUltimate369(b,8);
 case NativeWorldRemainder269Trait::Ultimate9:return HasNativeTravelUltimate369(b,9);
 case NativeWorldRemainder269Trait::Advanced19:return HasNativeTravelAdvanced369(b,19);
 case NativeWorldRemainder269Trait::Advanced34:return HasNativeTravelAdvanced369(b,34);
 case NativeWorldRemainder269Trait::Advanced35:return HasNativeTravelAdvanced369(b,35);
 case NativeWorldRemainder269Trait::Advanced45:return HasNativeTravelAdvanced369(b,45);
 default:return false;} // Debuff19 cannot be granted by Random.UnlockTravelBuff.
}
bool NativeWorldRemainder269ShootingBoard(const Board*){return false;}
bool NativeLastPlants269Advanced(const Plant* p,unsigned index){return p&&GetNativePlantData(p)&&HasNativeTravelAdvanced369(p->mBoard,index);}
bool NativeLastPlants269Shooting(const Board*){return false;}
bool NativeLastPlants269Balanced(const Board*){return false;}
bool NativeLastPlants269EmperorFastStart(const Board*){return false;}
bool NativeLastPlants269GoldSummoned(const Board* b)
{const auto* c=BoardValid(b)?GetNativeContext(b):nullptr;return c&&c->goldHypnoSummoned;}
void MarkNativeLastPlants269GoldSummoned(Board* b)
{if(Host(b))GetNativeContext(b)->goldHypnoSummoned=true;}
void NotifyNativeLastPlants269GoldAlreadySummoned(Plant* p)
{
    if(p&&Host(p->mBoard)&&!p->mNetGhost)
        p->mBoard->DisplayAdvice(_S("Золотой гипногриб уже был использован."),MESSAGE_STYLE_HINT_FAST,ADVICE_NONE);
}
bool CanApplyNativeLastPlants269GoldPost(Board*,uint16_t)
{
    // Source awardLevel2/1 and SetHealthInTravel are not a native source-wave
    // award. Reject the complete Gold transaction before any RNG/flag/Die.
    return false;
}
void ApplyNativeLastPlants269GoldPost(Zombie* z,int awardLevel)
{if(z)LawnNet::NetTrace("Fusion Last269 unexpected blocked GoldPost npc=%u awardLevel=%d",z->mNetId,awardLevel);}
bool EnsureNativeLastPlants269BirthEffect(Board* b)
{
    if(!BoardValid(b)||!b->mApp->mEffectSystem)return false;
    const auto* holder=b->mApp->mEffectSystem->mParticleHolder;
    return holder&&holder->mParticleSystems.mSize<holder->mParticleSystems.mMaxSize&&holder->mEmitters.mSize<holder->mEmitters.mMaxSize;
}
void CreateNativeLastPlants269BirthEffect(Board* b,float x,float y,int row)
{
    if(!Host(b)||!Point(x,y)||row<0||row>=GetNativeFusionRowCount(b)||!EnsureNativeLastPlants269BirthEffect(b))return;
    const int order=Board::MakeRenderOrder(RENDER_LAYER_PARTICLE,row,0);
    // Explicit compatible birth presentation; source11 Unity appearance is debt.
    if(b->mApp->AddTodParticle(x,y,order,PARTICLE_PLANTING))
        LawnNet::NetBroadcastParticle(b->mApp,x,y,order,PARTICLE_PLANTING);
}
bool TryConvertNativeLastPlants269Torch(Plant* p,Projectile* q,bool& counted)
{
    counted=false;const auto* data=GetNativePlantData(p);const auto* shot=GetNativeProjectileData(q);
    if(!data||data->kind!=1056||!q||q->mBoard!=p->mBoard||!Host(p->mBoard))return false;
    if(shot&&!FusionLastPlants269TorchContactAllowed(shot->specialType,q->mRow==p->mRow,shot->isZombieBullet,
        shot->lastTorchId==p->mNetId&&shot->lastTorchToken==data->instanceToken))return false;
    const uint16_t kind=shot?shot->kind:0;
    if(kind!=0&&kind!=10&&kind!=15)return false;
    // KelpTorch470180 calls YellowFirePea, FirePuffPea or void FireSnowPea;
    // shared commit retains the REAL1056 UID/token without mutating its kind.
    const bool committed=TryConvertNativeTorchProjectile(p,q,1056,counted);
    if(kind==15)counted=true; // Source counts its void callback even on a no-op.
    return committed||kind==15;
}
bool EnsureNativeLastPlants269TorchConversions(Board* b)
{
    if(!BoardValid(b)||GetNativeSeedForPlant(1156)==SEED_NONE||!IsNativeWater268ImplementedKind(1156)||
       !EnsureNativePlantVisuals(b,1156)||!EnsureNativeProjectileVisuals(b,30))return false;
    // Actual1156 shared world factory resets unsupported inherited packet data;
    // canonical30 leaf must be admitted before its original29 is retired.
    NativeProjectileData shot;shot.kind=30;shot.damage=40;shot.fireState=1;shot.lastTorchId=1;shot.lastTorchToken=1;
    if(!NativeProjectileMetadataValid(shot,PROJECTILE_PEA,MOTION_STRAIGHT))return false;
    shot.specialType=2;return NativeProjectileMetadataValid(shot,PROJECTILE_PEA,MOTION_STAR);
}
bool NativeLastPlants269WheatFastStart(const Board*){return false;}
bool NativeLastPlants269WheatClockRunning(const Plant* p)
{
    const auto* d=GetNativePlantData(p);
    return p&&d&&d->kind==253&&Host(p->mBoard)&&!p->mDead&&!p->mNetGhost&&p->mIsOnBoard&&
        NativeWheat269CanAdvance(p)&&!p->mBoard->mTimeStopCounter&&d->animationSpeed>0; // Source Board+0xDC is enermyCount, NOT theWave.
}
namespace
{
// Original GameAPP.LoadPlant nonnull runtime slots, minus Board.WheatLimit.
// 193 source slots1000..1192 ->138 choices. Never filtered by native support.
const auto eve269Pool=FusionEve269Candidates();
bool Eve269Actor(const Plant* p)
{
    const auto* d=GetNativePlantData(p);
    return p&&d&&Host(p->mBoard)&&!p->mDead&&!p->mNetGhost&&p->mIsOnBoard&&
        p->mBoard->IsValidGridCell(p->mPlantCol,p->mRow)&&
        (d->kind==253||GetNativeWheat269State(p));
}
}
bool NativeEve269PoolReady(Board* b)
{
    // These are transient native conditions, not immutable resource failures.
    // Never cache pause, game phase, particle capacity or full dropped-card bank.
    if(!Host(b)||b->mPaused||!CanApplyNativeFusionGameplay(b)||!GetNativeContext(b)||
       !b->mPlants.mBlock||!b->mCoins.mBlock||b->mCoins.mSize>=b->mCoins.mMaxSize||
       !EnsureNativeLastPlants269BirthEffect(b))return false;
    unsigned cards=0;Coin* coin=nullptr;while(b->IterateCoins(coin))if(!coin->mDead&&GetNativeDroppedCard269Data(coin))++cards;
    if(cards>=128)return false;
    auto* context=GetNativeContext(b);
    if(context->eve269ResourceAudit)return context->eve269ResourceAudit==1;
    unsigned errors=0;
    for(const uint16_t kind:eve269Pool)
    {
        // A missing implementation/resource gates the ENTIRE source pool; it
        // never removes that outcome or silently rerolls a selected kind.
        std::string modelError,portraitError;
        const bool registered=FusionFindPlant(kind)&&GetNativeSeedForPlant(kind)!=SEED_NONE;
        const bool model=registered&&EnsureNativePlantVisuals(b,kind,&modelError);
        const bool portrait=registered&&EnsureNativePortraitVisuals(b,kind,&portraitError);
        if(!model||!portrait)
        {++errors;LawnNet::NetTrace("Fusion Eve269 fullPool blocked kind=%u registered=%d model=%d portrait=%d modelError=%s portraitError=%s",kind,registered,model,portrait,modelError.c_str(),portraitError.c_str());}
        ReleaseUnusedNativeVisuals(b); // Bound32-bit native GPU residency.
    }
    context->eve269ResourceAudit=errors?2:1;
    LawnNet::NetTrace("Fusion Eve269 fullPool resourceAudit=%u count=138 errors=%u hostLocal=1 noFNS=1",unsigned(context->eve269ResourceAudit),errors);
    return errors==0;
}

Plant* ReplaceNativeEve269Plant(Plant* p,bool& committed)
{
    committed=false;
    if(!Eve269Actor(p)||!NativeEve269PoolReady(p->mBoard))return nullptr;
    Board* b=p->mBoard;const int col=p->mPlantCol,row=p->mRow;
    const auto* data=GetNativePlantData(p);const uint16_t oldId=p->mNetId,oldKind=data->kind;const uint32_t oldToken=data->instanceToken;
    float x=0,y=0;if(!p->GetShadowCenter(x,y))return nullptr;
    // Plant.WheatUpdate: virtual Die(1), source11 at Shadow+Y.5, THEN SetEve
    // makes its one original uniform RNG draw. Death callbacks keep RNG order.
    p->Die(1);committed=true;
    CreateNativeLastPlants269BirthEffect(b,x,y-float(.5*GetNativeFusionPixelsY(b)),row);
    const int count=int(eve269Pool.size());
    const int draw=RandRangeInt(0,count-1);const uint16_t kind=eve269Pool[draw];
    NativePlantData incoming;incoming.kind=kind;incoming.owner=LawnNet::NET_SHARED_OWNER;
    Plant* child=TrySpawnNativeFusionPlant(b,col,row,incoming);
    if(child)
    {
        if(!MarkNativeWheat269Replacement(child))
            LawnNet::NetTrace("Fusion Eve269 unexpected Wheat mark failure child=%u kind=%u",child->mNetId,kind);
        const auto* identity=GetNativePlantData(child);
        LawnNet::NetTrace("Fusion Eve269 replace source=%u token=%u sourceKind=%u draw=%d pool=138 kind=%u child=%u childToken=%u fromWheat=1 col=%d row=%d",oldId,oldToken,oldKind,draw,kind,child->mNetId,identity?identity->instanceToken:0,col,row);
        return child;
    }
    // Mouse.GetBoxXFromColumn/GetLandY, not the removed actor's visual centre.
    const float cardX=float(b->GetCellCenterX(col,row)),cardY=float(b->GridToPixelY(col,row)+50);
    Coin* card=CreateNativeDroppedCard269(b,kind,0,cardX,cardY);
    const auto* identity=GetNativeDroppedCard269Data(card);
    LawnNet::NetTrace("Fusion Eve269 fallback source=%u token=%u draw=%d pool=138 kind=%u card=%u cardToken=%u cost=0 col=%d row=%d success=%d",oldId,oldToken,draw,kind,card?card->mNetId:0,identity?identity->instanceToken:0,col,row,card?1:0);
    return nullptr;
}
bool CanReplaceNativeLastPlants269Wheat(const Plant* p)
{return Eve269Actor(p)&&NativeEve269PoolReady(p->mBoard);}
bool ReplaceNativeLastPlants269Wheat(Plant* p)
{bool committed=false;(void)ReplaceNativeEve269Plant(p,committed);return committed;}
}
