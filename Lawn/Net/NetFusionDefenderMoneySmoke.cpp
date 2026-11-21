#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetSmokeCoinClick.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Projectile.h"
#include "../Coin.h"
#include "../SeedPacket.h"
#include "../Cutscene.h"
#include "../System/PlayerInfo.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativeWire.h"
#include "../Fusion/FusionRecipe.h"
#include "../Fusion/FusionDefenderRules.h"
#include "../Fusion/FusionNativeDefender.h"
#include "../Fusion/FusionMoneyRules.h"
#include "../Fusion/FusionNativeBatchActions.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../SexyAppFramework/DDInterface.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <iterator>
#include <initializer_list>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
using namespace LawnFusion;
struct Actor { uint16_t id=0; uint32_t token=0; int hp=0,stages=0; };
struct Producer { int previous=0,pending=0,callbacks=0,cycles=0; };
struct State
{
    int stage=0,waiting=0,introChecks=0,initialMoney=0,profileCoins=0,setupTick=0,pulseTick=0;
    uint32_t since=0,start=0,chat=0,introFirst=0,introLast=0,creditedAt=0,diagnostic=0,matrixDiagnostic=0;
    bool configured=false,setup=false,combat=false,pulses=false,reflect=false,cactusReflect=false,coldTall=false,coldNut=false,warm=false,covered=false;
    bool clicked=false,credited=false,cabbageCycle=false;
    uint16_t coin=0,ice=0,fire=0;
    int iceEnd=0,fireEnd=0;
    std::map<uint16_t,Actor> actors;
    std::map<uint16_t,uint16_t> biters;
    std::map<uint16_t,int> targetHealth,damageKinds;
    std::map<uint16_t,Producer> producers;
    std::map<uint16_t,int> coinValues;
    std::set<uint16_t> payloads,projectiles;
    std::set<std::string> frames;
    std::map<std::string,int> draw;
    std::string ids;
    LawnNet::NetSmokeCoinClickState settled;
} s;
constexpr uint16_t moneyKinds[]={31,1140,1134,1138,1141};
const char* Role(LawnApp* a) { return a->NetIsHost()?"host":"client"; }
int Slot(LawnApp* a) { return a->mNetManager->GetMySlot(); }
bool Require(LawnApp* a,bool ok,const char* why) { if(!ok)a->CoopSmokeFail(why); return ok; }
void Advance(LawnApp* a,int stage)
{ s.stage=stage;s.since=GetTickCount();LawnNet::NetTrace("SMOKE_DM196_STAGE role=%s stage=%d",Role(a),stage); }
bool Has(LawnApp* a,int slot,const std::string& text)
{ for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==slot&&line.mText==text)return true;return false; }
bool Send(LawnApp* a,const std::string& text)
{
    if(Has(a,Slot(a),text))return true;
    if(s.chat&&GetTickCount()-s.chat<800)return false;
    s.chat=GetTickCount();a->mNetManager->SendChat(text);return Has(a,Slot(a),text);
}
bool Barrier(LawnApp* a,const std::string& text) { Send(a,text);return Has(a,0,text)&&Has(a,1,text); }
bool Click(LawnApp* a,float x,float y)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;
    return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"DefenderMoney owned SDL click failed");
}
bool Capture(LawnApp* a,const std::string& name)
{
    if(s.frames.contains(name))return true;
    if(!s.draw.contains(name)){s.draw[name]=a->mDrawCount;return false;}
    if(a->mDrawCount-s.draw[name]<2)return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"DefenderMoney renderer capture failed"))return false;
    s.frames.insert(name);LawnNet::NetTrace("SMOKE_DM196_RENDER role=%s frame=%s freshDraws=%d",Role(a),name.c_str(),a->mDrawCount-s.draw[name]);return true;
}
Plant* Kind(Board* b,uint16_t kind)
{ Plant* p=nullptr;while(b->mPlants.IterateNext(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&p->mIsOnBoard&&d&&d->kind==kind)return p;}return nullptr; }
Zombie* Enemy(Board* b,uint16_t id)
{ Zombie* z=nullptr;while(b->mZombies.IterateNext(z))if(z->mNetId==id)return z;return nullptr; }
NativePlantData Provenance(uint16_t kind)
{
    NativePlantData d;d.kind=kind;
    if(IsNativeBasePlantKind(kind)||kind==1029)return d;
    if(kind==1028){d.firstParent=1027;return d;}
    const FusionRecipe* best=nullptr;
    for(size_t i=0;i<GetFusionRecipeCount();++i)
    {
        const auto& r=GetFusionRecipes()[i];if(r.result!=kind)continue;
        if(!best)best=&r;
        if(r.existing!=kind&&r.used!=kind){best=&r;break;}
    }
    if(best){d.firstParent=best->existing;d.secondParent=best->used;}
    return d;
}
void Position(uint16_t kind,int& column,int& row)
{
    if(kind==1029){column=0;row=1;return;}
    for(size_t i=0;i<FusionDefenderProfileCount();++i)if(FusionDefenderProfileAt(i)->kind==kind)
    {
        if(i<7){column=int(i)+2;row=0;return;}
        if(i==7){column=0;row=1;return;}
        if(i>=8&&i<=10){column=0;row=int(i)-6;return;}
        if(i==11||i==12){column=int(i)-10;row=1;return;}
        column=int(i)-7;row=1;return;
    }
    if(kind==31||kind==1140){column=kind==31?0:1;row=0;return;}
    column=0;row=kind==1134?2:kind==1138?3:4;
}
bool Matrix(LawnApp* a)
{
    auto* b=a->mBoard;std::vector<uint16_t> kinds;
    for(size_t i=0;i<FusionDefenderProfileCount();++i)kinds.push_back(FusionDefenderProfileAt(i)->kind);
    kinds.insert(kinds.end(),std::begin(moneyKinds),std::end(moneyKinds));
    for(const auto kind:kinds)
    {
        auto* p=Kind(b,kind);const auto* d=GetNativePlantData(p);const auto* definition=FusionFindPlant(kind);
        if(!p||!d)return false;
        int col=0,row=0;Position(kind,col,row);PlantsOnLawn cell{};b->GetPlantsOnLawn(col,row,&cell);
        const auto* profile=FindFusionDefenderProfile(kind);
        Plant* actual=profile&&profile->layer==FusionDefenderLayer::Under?cell.mUnderPlant:
            profile&&profile->layer==FusionDefenderLayer::Pumpkin?cell.mPumpkinPlant:cell.mNormalPlant;
        const auto expected=Provenance(kind);
        if(!Require(a,definition&&definition->hasStats&&p==actual&&p->mSeedType==GetNativeSeedForPlant(kind)&&
            p->mPlantHealth==definition->maxHealth&&p->mPlantMaxHealth==definition->maxHealth&&p->mPlantCol==col&&p->mRow==row&&
            p->mNetId&&d->instanceToken&&d->owner==255&&!d->packetId&&!d->randomOrigin&&
            d->firstParent==expected.firstParent&&d->secondParent==expected.secondParent&&d->damageStage==0,
            "DefenderMoney actual canonical HP/layer/ancestry differs"))return false;
        if(!s.actors.contains(kind))
        {
            s.actors[kind]={p->mNetId,d->instanceToken,p->mPlantHealth,1};
            LawnNet::NetTrace("SMOKE_DM196_ACTOR role=%s kind=%u id=%u token=%u HP=%d layer=%d column=%d row=%d parents=%u,%u pending=%u",
                Role(a),kind,p->mNetId,d->instanceToken,p->mPlantHealth,profile?int(profile->layer):0,col,row,d->firstParent,d->secondParent,profile?profile->pendingDependencies:0);
        }
    }
    return true;
}
bool InitialMetadata(LawnApp* a)
{
    auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);NativeSnapshotFrame frame;
    frame.epoch=ctx->epoch;frame.selectionCommitted=ctx->selectionCommitted;frame.selection=ctx->selection.State();
    frame.money=ctx->money;frame.gloveHolds=ctx->gloveHolds;frame.gloveCooldownTicks=ctx->gloveCooldownTicks;
    for(int i=0;i<b->mSeedBank->mNumPackets;++i)
    {
        const auto& packet=b->mSeedBank->mSeedPackets[i];const auto* data=GetNativePacketData(&packet);
        if(!Require(a,data!=nullptr,"DefenderMoney initial fixture packet metadata missing"))return false;
        frame.packets.push_back({packet.mNetId,*data});
    }
    if(!Require(a,ValidateNativeSnapshotFrame(frame,ctx->eligible),"DefenderMoney initial fixture bank metadata rejected"))return false;
    Plant* plant=nullptr;
    while(b->mPlants.IterateNext(plant))if(!plant->mDead&&plant->mIsOnBoard)
    {
        const auto* data=GetNativePlantData(plant);
        if(!Require(a,data!=nullptr,"DefenderMoney initial fixture plant metadata missing"))return false;
        frame.plants.push_back({plant->mNetId,*data});
        if(!ValidateNativeSnapshotFrame(frame,ctx->eligible))
        {
            LawnNet::NetTrace("SMOKE_DM196_INVALID_FIXTURE kind=%u id=%u token=%u parents=%u,%u production=%d pending=%d stage=%u speed=%.9g",
                data->kind,plant->mNetId,data->instanceToken,data->firstParent,data->secondParent,data->productionTicks,data->pendingProductionTicks,data->damageStage,data->animationSpeed);
            return Require(a,false,"DefenderMoney initial fixture plant ancestry/metadata rejected by production snapshot policy");
        }
    }
    // This exercises the actual metadata codec, without sending a snapshot or
    // claiming that an empty synthetic entity body tests native-body preflight.
    LawnNet::NetBuffer metadata;
    if(!Require(a,EncodeNativeSnapshot(frame,ctx->eligible,metadata),"DefenderMoney initial fixture metadata encode failed"))return false;
    LawnNet::NetTrace("SMOKE_DM196_INITIAL_METADATA actors=%zu packets=%zu productionEncoder=1 metadataOnly=1 nativeBodyPreflight=0",frame.plants.size(),frame.packets.size());
    return true;
}
bool SetupPlants(LawnApp* a)
{
    if(!a->NetIsHost()||s.setup)return true;
    auto* b=a->mBoard;if(!Require(a,!b->mPlants.mSize&&!b->mZombies.mSize,"DefenderMoney initial setup requires empty native world"))return false;
    b->mSunCountDown=b->mZombieCountDown=b->mZombieCountDownStart=100000;
    for(size_t i=0;i<FusionDefenderProfileCount()+5;++i)
    {
        const auto kind=i<FusionDefenderProfileCount()?FusionDefenderProfileAt(i)->kind:moneyKinds[i-FusionDefenderProfileCount()];
        int col=0,row=0;Position(kind,col,row);const auto data=Provenance(kind);
        auto* p=b->AddPlant(col,row,GetNativeSeedForPlant(kind),SEED_NONE,&data);
        if(!Require(a,p&&GetNativePlantData(p),"DefenderMoney real factory allocation failed"))return false;
        NativeBatchPlantPlaced(p);
    }
    if(!InitialMetadata(a))return false;s.setup=true;
    LawnNet::NetTrace("SMOKE_DM196_SETUP actors=22 sourceDefenders=17 moneyKinds=5 initialFactoryOnly=1 paidCrafts=0 actorHealthTimerWrites=0 RNGReseeds=0 ambientSuppressionSetupOnly=1");return true;
}
bool SetupCombat(LawnApp* a)
{
    if(!a->NetIsHost()||s.combat)return true;
    auto* b=a->mBoard;std::ostringstream out;out<<"DM196_IDS";
    for(int row=2;row<=4;++row)
    {
        auto* z=b->AddZombieInRow(ZOMBIE_NORMAL,row,0);if(!Require(a,z!=nullptr,"DefenderMoney target allocation failed"))return false;
        z->mBodyHealth=z->mBodyMaxHealth=50000;z->mVelX=0;const auto r=z->GetZombieRect();
        z->mPosX+=(row==3?760:580)-(r.mX+r.mWidth*.5f);z->mX=int(z->mPosX);z->ApplyAnimRate(0);
        s.targetHealth[z->mNetId]=50000;out<<' '<<z->mNetId;
    }
    auto* secondary=b->AddZombieInRow(ZOMBIE_NORMAL,4,0);if(!Require(a,secondary!=nullptr,"DefenderMoney secondary allocation failed"))return false;
    secondary->mBodyHealth=secondary->mBodyMaxHealth=50000;secondary->mVelX=0;const auto rect=secondary->GetZombieRect();
    secondary->mPosX+=620-(rect.mX+rect.mWidth*.5f);secondary->mX=int(secondary->mPosX);secondary->ApplyAnimRate(0);
    s.targetHealth[secondary->mNetId]=50000;out<<' '<<secondary->mNetId;
    for(uint16_t kind:{uint16_t(1029),uint16_t(1048),uint16_t(1189),uint16_t(1039),uint16_t(1073),uint16_t(24),uint16_t(1088)})
    {
        auto* p=Kind(b,kind);auto* z=p?b->AddZombieInRow(ZOMBIE_NORMAL,p->mRow,0):nullptr;
        auto* anim=z?a->ReanimationTryToGet(z->mBodyReanimID):nullptr;
        if(!Require(a,p&&z&&anim&&anim->mAnimRate>0,"DefenderMoney real biter allocation failed"))return false;
        z->mBodyHealth=z->mBodyMaxHealth=kind==1048?1:kind==1088?3:50000;z->SetAnimRate(anim->mAnimRate);z->mVelX=0;
        z->mPosY=z->GetPosYBasedOnRow(p->mRow);z->mY=int(z->mPosY);bool found=false;
        for(int offset=80;offset>=-80;--offset){z->mPosX=float(p->mX+offset);z->mX=int(z->mPosX);if(z->FindPlantTarget(ATTACKTYPE_CHEW)==p){found=true;break;}}
        if(!Require(a,found,"DefenderMoney initial native chew target unavailable"))return false;
        if(kind==1039||kind==1073)
        {
            auto* cold=GetOrCreateNativeZombieColdData(z);if(!Require(a,cold!=nullptr,"DefenderMoney initial cold prerequisite failed"))return false;
            cold->level=kind==1039?90:50;cold->threshold=kind==1039?100:200;
            if(kind==1073)z->SetColdCounters(1000,0);
        }
        s.biters[kind]=z->mNetId;out<<' '<<kind<<' '<<z->mNetId;
    }
    s.ids=out.str();s.combat=true;s.setupTick=b->mMainCounter;
    LawnNet::NetTrace("SMOKE_DM196_COMBAT_SETUP targets=4 biters=7 initialPositionHealthColdAndRateOnly=1 healthWritesAfterSetup=0 timerWritesAfterSetup=0 sourceZombieAIUnported=1 tick=%d",s.setupTick);return true;
}
bool ReadCombat(LawnApp* a)
{
    if(a->NetIsHost())return s.combat;
    for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==0&&line.mText.rfind("DM196_IDS ",0)==0)
    {
        std::istringstream in(line.mText.substr(10));unsigned id=0,kind=0;
        for(int i=0;i<4;++i){if(!(in>>id)||!id||id>65535)return false;s.targetHealth.try_emplace(uint16_t(id),50000);}
        for(int i=0;i<7;++i){if(!(in>>kind>>id)||!id||id>65535)return false;s.biters[uint16_t(kind)]=uint16_t(id);}
        s.combat=true;return true;
    }return false;
}
bool Observe(LawnApp* a)
{
    auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);
    for(auto& [kind,record]:s.actors)
    {
        auto* p=Kind(b,kind);if(!p)continue;const auto* d=GetNativePlantData(p);
        if(!Require(a,p->mNetId==record.id&&d->instanceToken==record.token&&p->mPlantHealth<=p->mPlantMaxHealth,"DefenderMoney actor changed generation or exceeded maxHP"))return false;
        if(const auto* profile=FindFusionDefenderProfile(kind);profile&&profile->damageNodes[0])
        {
            const int low=profile->maxHealth/3,high=int(int64_t(profile->maxHealth)*2/3);int expected=p->mPlantHealth>high?0:p->mPlantHealth>low&&p->mPlantHealth<high?1:p->mPlantHealth<low?2:-1;
            if(!Require(a,expected<0||d->damageStage==expected,"DefenderMoney real damage-stage cache mismatches natural health"))return false;
            if(!(record.stages&(1<<d->damageStage)))
            {record.stages|=1<<d->damageStage;LawnNet::NetTrace("SMOKE_DM196_CRACK role=%s kind=%u id=%u stage=%u HP=%d max=%d tick=%d",Role(a),kind,p->mNetId,d->damageStage,p->mPlantHealth,p->mPlantMaxHealth,b->mMainCounter);}
            if(d->damageStage==2&&(kind==1029||kind==1073||kind==24))Capture(a,kind==1029?"fusion_defendermoney_body_crack":kind==1073?"fusion_defendermoney_tall_crack":"fusion_defendermoney_cover_crack");
        }
        record.hp=p->mPlantHealth;
    }
    for(const auto kind:{uint16_t(31),uint16_t(1140)})if(auto* p=Kind(b,kind))
    {
        const auto* d=GetNativePlantData(p);auto& rec=s.producers[kind];
        if(rec.previous>0&&p->mLaunchCounter>rec.previous+2)++rec.cycles;
        if(rec.pending>0&&!d->pendingProductionTicks)++rec.callbacks;
        rec.previous=p->mLaunchCounter;rec.pending=d->pendingProductionTicks;
    }
    if(s.combat)
    {
        auto* reflect=Kind(b,1048);auto* coldTall=Kind(b,1039);auto* coldNut=Kind(b,1189);auto* warm=Kind(b,1073);
        auto* r=Enemy(b,s.biters[1048]);auto* t=Enemy(b,s.biters[1039]);auto* n=Enemy(b,s.biters[1189]);auto* w=Enemy(b,s.biters[1073]);
        if(!s.reflect&&reflect&&reflect->mPlantHealth==3996&&(!r||r->mBodyHealth<=0||r->IsDeadOrDying()))
        {s.reflect=true;LawnNet::NetTrace("SMOKE_DM196_BITE role=%s kind=1048 plant=%u target=%u lethalReflect=2 victimDamage=4 originalBiteContinued=1",Role(a),reflect->mNetId,s.biters[1048]);}
        auto* cactus=Kind(b,1088);auto* c=Enemy(b,s.biters[1088]);
        if(!s.cactusReflect&&cactus&&cactus->mPlantHealth==3996&&(!c||c->mBodyHealth<=0||c->IsDeadOrDying()))
        {s.cactusReflect=true;LawnNet::NetTrace("SMOKE_DM196_BITE role=%s kind=1088 plant=%u target=%u lethalReflect=4 victimDamage=4 originalBiteContinued=1",Role(a),cactus->mNetId,s.biters[1088]);}
        if(!s.coldTall&&coldTall&&coldTall->mPlantHealth<8000&&t)
        {const auto* d=GetNativeZombieColdData(t);if(d&&d->threshold>=200&&t->mIceTrapCounter>0&&(!a->NetIsHost()||coldTall->mPlantHealth==7996)){s.coldTall=true;LawnNet::NetTrace("SMOKE_DM196_BITE role=%s kind=1039 plant=%u target=%u threshold=%d level=%d frozen=%d victimHP=%d",Role(a),coldTall->mNetId,t->mNetId,d->threshold,d->level,t->mIceTrapCounter,coldTall->mPlantHealth);}}
        if(!s.coldNut&&coldNut&&coldNut->mPlantHealth<4000&&n)
        {const auto* d=GetNativeZombieColdData(n);if(d&&d->level>=10&&n->mChilledCounter>0){s.coldNut=true;LawnNet::NetTrace("SMOKE_DM196_BITE role=%s kind=1189 plant=%u target=%u level=%d slow=%d",Role(a),coldNut->mNetId,n->mNetId,d->level,n->mChilledCounter);}}
        if(!s.warm&&warm&&warm->mPlantHealth<8000&&w)
        {const auto* d=GetNativeZombieColdData(w);if(d&&d->threshold==200&&!d->level&&!w->mChilledCounter&&!w->mIceTrapCounter&&w->mBodyHealth<50000){s.warm=true;LawnNet::NetTrace("SMOKE_DM196_BITE role=%s kind=1073 plant=%u target=%u threshold=200 cleared=1 HP=%d",Role(a),warm->mNetId,w->mNetId,w->mBodyHealth);}}
        auto* body=Kind(b,1029);auto* pot=Kind(b,27);
        if(!s.covered&&body&&pot&&body->mPlantHealth<8000&&pot->mPlantHealth==300)
        {s.covered=true;LawnNet::NetTrace("SMOKE_DM196_POT role=%s under=%u body=%u underHP=300 bodyHP=%d biteOrderProtected=1 nonbiteImmunityRuntime=0",Role(a),pot->mNetId,body->mNetId,body->mPlantHealth);}
        Projectile* p=nullptr;
        while(b->mProjectiles.IterateNext(p))if(!p->mDead)if(const auto* d=GetNativeProjectileData(p);d&&(d->kind==74||d->kind==76||d->kind==78||d->kind==81))
        {
            s.payloads.insert(d->kind);
            if(s.projectiles.insert(p->mNetId).second)
            {if(!Require(a,d->damage==(d->kind==74||d->kind==78?60:d->kind==76?40:100)&&!d->sunOnHit&&(d->kind==81?d->melonEnhanced:!d->melonEnhanced),"DefenderMoney Silver projectile metadata differs"))return false;
             LawnNet::NetTrace("SMOKE_DM196_PROJECTILE role=%s id=%u kind=%u damage=%d enhanced=%d sun=0",Role(a),p->mNetId,d->kind,d->damage,d->melonEnhanced?1:0);}
        }
        if(a->NetIsHost())for(auto& [id,last]:s.targetHealth)if(auto* z=Enemy(b,id))
        {
            const int delta=last-z->mBodyHealth;
            if(delta>0){if(delta==60||delta==40||delta==100||delta==66){++s.damageKinds[delta];LawnNet::NetTrace("SMOKE_DM196_DAMAGE target=%u row=%d delta=%d HP=%d tick=%d",id,z->mRow,delta,z->mBodyHealth,b->mMainCounter);}last=z->mBodyHealth;}
        }
        if(a->NetIsHost())if(auto* p=Kind(b,1134))
        {
            auto& cycle=s.producers[1134];
            if(cycle.previous>0&&p->mLaunchCounter>cycle.previous+2)
            {
                if(!Require(a,p->mLaunchCounter>=190&&p->mLaunchCounter<=210&&GetNativeDefenderPotKind(p)==1112,
                    "DefenderMoney CabbagePot actual natural cycle differs"))return false;
                s.cabbageCycle=true;
                LawnNet::NetTrace("SMOKE_DM196_POT_EFFECT kind=1134 under=1112 naturalCycle=%d interval=2 tick=%d",p->mLaunchCounter,b->mMainCounter);
            }
            cycle.previous=p->mLaunchCounter;
        }
    }
    Coin* coin=nullptr;
    while(b->mCoins.IterateNext(coin))if(!coin->mDead&&IsNativeMoneyCoin(coin))
    {
        const int value=GetNativeCoinValue(coin,0);if(value>0){if(!Require(a,FusionMoneyCoinMetadataValid(coin->mType,value),"DefenderMoney actual cash type/value invalid"))return false;s.coinValues[coin->mNetId]=value;}
        if(coin->mIsBeingCollected&&s.coinValues.contains(coin->mNetId))
        {if(!Require(a,s.coinValues.at(coin->mNetId)==25&&(!s.coin||s.coin==coin->mNetId),"DefenderMoney unexpected cash coin collected"))return false;s.coin=coin->mNetId;}
    }
    if(!s.credited&&ctx->money!=s.initialMoney)
    {if(!Require(a,s.coin&&ctx->money==s.initialMoney+25,"DefenderMoney credit lacks exactly one observed Money25"))return false;s.credited=true;s.creditedAt=GetTickCount();LawnNet::NetTrace("SMOKE_DM196_CREDIT role=%s id=%u delta=25 initial=%d money=%d sun=500 profileUnchanged=1",Role(a),s.coin,s.initialMoney,ctx->money);}
    return Require(a,ctx->money==s.initialMoney+(s.credited?25:0)&&b->mSunMoney==500&&a->mPlayerInfo->mCoins==s.profileCoins,"DefenderMoney SUN/profile/cash ledger separation failed");
}
bool Pulses(LawnApp* a)
{
    if(!a->NetIsHost()||s.pulses)return true;
    if(!s.reflect||!s.cactusReflect||!s.coldTall||!s.coldNut||!s.warm)return false;
    auto* b=a->mBoard;auto cold=Provenance(10),fire=Provenance(16);
    auto* ice=b->AddPlant(4,1,GetNativeSeedForPlant(10),SEED_NONE,&cold);
    auto* flame=b->AddPlant(2,0,GetNativeSeedForPlant(16),SEED_NONE,&fire);
    if(!Require(a,ice&&flame,"DefenderMoney natural pulse actor allocation failed"))return false;
    NativeBatchPlantPlaced(ice);NativeBatchPlantPlaced(flame);s.ice=ice->mNetId;s.fire=flame->mNetId;s.pulses=true;s.pulseTick=b->mMainCounter;
    LawnNet::NetTrace("SMOKE_DM196_PULSE_SETUP ice=%u fire=%u tick=%d sourceCountdownOnly=1 directEffectCalls=0",s.ice,s.fire,s.pulseTick);return true;
}
bool Collect(LawnApp* a)
{
    if(!a->NetIsClient()||s.clicked)return true;
    Coin* coin=nullptr;int x=0,y=0;auto* b=a->mBoard;
    while(b->mCoins.IterateNext(coin))if(!coin->mDead&&IsNativeMoneyCoin(coin)&&GetNativeCoinValue(coin,0)==25&&
        LawnNet::NetSmokeSettledCoinPoint(b,coin,a->mCoopSmokeSnapshotsApplied,s.settled,x,y))
    {
        s.coin=coin->mNetId;s.clicked=Click(a,float(x+b->mX+a->mDDInterface->mWideScreenOffsetX),float(y+b->mY+a->mDDInterface->mWideScreenOffsetY));
        LawnNet::NetTrace("SMOKE_DM196_COIN_CLICK role=client candidate=%u value=25 point=%d,%d singleSDL=1 settledSnapshots=%u margin=8",s.coin,x,y,a->mCoopSmokeSnapshotsApplied-s.settled.coins[s.coin].firstSnapshot);return s.clicked;
    }return true;
}
void Diagnostic(LawnApp* a)
{
    std::ostringstream payloads;for(const auto kind:s.payloads)payloads<<kind<<',';
    const auto* ctx=GetNativeContext(a->mBoard);
    LawnNet::NetTrace("SMOKE_DM196_GATE role=%s stage=%d ageMs=%u reflect=%d cactusReflect=%d coldTall=%d coldNut=%d warm=%d pot=%d payloads=%s callbacks31=%d callbacks1140=%d cabbageCycle=%d masks1029=%d masks1073=%d masks24=%d masks1039=%d clicked=%d credited=%d coin=%u money=%d damage40=%d damage60=%d damage100=%d damage66=%d iceEnd=%d fireEnd=%d",
        Role(a),s.stage,GetTickCount()-s.start,s.reflect?1:0,s.cactusReflect?1:0,s.coldTall?1:0,s.coldNut?1:0,s.warm?1:0,s.covered?1:0,payloads.str().c_str(),
        s.producers[31].callbacks,s.producers[1140].callbacks,s.cabbageCycle?1:0,s.actors[1029].stages,s.actors[1073].stages,s.actors[24].stages,s.actors[1039].stages,
        s.clicked?1:0,s.credited?1:0,s.coin,ctx?ctx->money:-1,s.damageKinds[40],s.damageKinds[60],s.damageKinds[100],s.damageKinds[66],s.iceEnd,s.fireEnd);
    s.diagnostic=GetTickCount();
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionDefenderMoney(LawnApp* a)
{
    const auto* enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_DEFENDER_MONEY");const auto* native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
    if(!a||!enabled||std::strcmp(enabled,"1")||!native||std::strcmp(native,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
    if(a->mCoopSmokeResultWritten)return true;if(!s.start)s.start=GetTickCount();
    if(GetTickCount()-s.start>230000){Diagnostic(a);a->CoopSmokeFail("DefenderMoney bounded230s fixture deadline");return true;}
    if(s.stage>=7&&s.stage<=9&&(!s.diagnostic||GetTickCount()-s.diagnostic>=15000))Diagnostic(a);
    if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
    if(s.stage==0)
    {
        if(a->NetIsHost()&&!s.configured){a->mNetManager->SetDifficulty(1);s.configured=true;}
        if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
        if(a->NetIsClient()){Send(a,"DM196_START");Advance(a,1);}
        else if(Has(a,1,"DM196_START")){std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());Advance(a,1);}return true;
    }
    if(!a->mBoard||!IsNativeFusionBoard(a->mBoard)){if(s.stage==1&&a->NetIsClient())Send(a,"DM196_START");return true;}
    auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);auto* chooser=a->mSeedChooserScreen;const auto& bank=ctx->selection.State();const int slot=Slot(a);
    if(!Require(a,ctx->validation&&ctx->visuals&&!a->mFusionRuntime&&b->GetColumnCount()==10,"DefenderMoney lost real native world"))return true;
    if(s.stage>=6&&!Observe(a))return true;
    if(s.stage==1)
    {
        if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||!b->mCutScene->mSeedChoosing||chooser->mSeedsInFlight)return true;
        int own=0;for(const auto& packet:bank.packets)own+=packet.owner==slot;if(own<s.waiting)return true;
        if(own<bank.quotas[slot]){int x=0,y=0;chooser->GetSeedPositionInChooser(slot+2*own,x,y);if(Click(a,float(chooser->mX+x+25),float(chooser->mY+y+35-int(chooser->mScrollbar->mScrollValue))))s.waiting=own+1;return true;}
        if(bank.packets.size()==bank.targetCount)Advance(a,2);
    }
    else if(s.stage==2)
    {
        if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}
        auto* button=chooser?chooser->mStartButton:nullptr;if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;
        const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
        if(!button->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"DefenderMoney Ready hover failed");}
        else if(Click(a,x,y))Advance(a,3);
    }
    else if(s.stage==3)
    {
        if(bank.phase==FusionBankPhase::Intro){if(!s.introFirst)s.introFirst=GetTickCount();s.introLast=GetTickCount();++s.introChecks;return true;}
        if(bank.phase!=FusionBankPhase::Playing||a->mGameScene!=SCENE_PLAYING||chooser)return true;
        if(!Require(a,s.introChecks>=10&&s.introLast-s.introFirst>=1000&&b->mSunMoney==500&&ctx->money==3000,"DefenderMoney skipped full intro/fresh ledgers"))return true;
        s.initialMoney=ctx->money;s.profileCoins=a->mPlayerInfo->mCoins;Advance(a,4);
    }
    else if(s.stage==4){if(!Barrier(a,"DM196_INITIAL"))return true;if(SetupPlants(a))Advance(a,5);}
    else if(s.stage==5)
    {
        if(!s.matrixDiagnostic||GetTickCount()-s.matrixDiagnostic>=15000)
        {
            uint16_t missing=0;int present=0;
            for(size_t i=0;i<FusionDefenderProfileCount()+5;++i)
            {const auto kind=i<FusionDefenderProfileCount()?FusionDefenderProfileAt(i)->kind:moneyKinds[i-FusionDefenderProfileCount()];if(Kind(b,kind))++present;else if(!missing)missing=kind;}
            NetTrace("SMOKE_DM196_MATRIX_GATE role=%s present=%d missingKind=%u recorded=%zu sent=%u applied=%u hostMatrix=%d clientMatrix=%d",
                Role(a),present,missing,s.actors.size(),a->mCoopSmokeSnapshotsSent,a->mCoopSmokeSnapshotsApplied,Has(a,0,"DM196_MATRIX")?1:0,Has(a,1,"DM196_MATRIX")?1:0);
            s.matrixDiagnostic=GetTickCount();
        }
        if(!Matrix(a)||!Capture(a,"fusion_defendermoney_matrix")||!Barrier(a,"DM196_MATRIX"))return true;Advance(a,6);
    }
    else if(s.stage==6)
    {
        if(a->NetIsHost()){if(!SetupCombat(a)||!Send(a,s.ids))return true;}
        if(!ReadCombat(a)||!Barrier(a,"DM196_COMBAT"))return true;Advance(a,7);
    }
    else if(s.stage==7)
    {
        if(a->NetIsHost()&&!Pulses(a))return true;
        if(a->NetIsHost()&&s.pulses)
        {
            if(!Kind(b,16)&&!s.fireEnd){s.fireEnd=b->mMainCounter;NetTrace("SMOKE_DM196_PULSE_END kind=16 elapsed=%d",s.fireEnd-s.pulseTick);}
            if(!Kind(b,10)&&!s.iceEnd){s.iceEnd=b->mMainCounter;NetTrace("SMOKE_DM196_PULSE_END kind=10 elapsed=%d",s.iceEnd-s.pulseTick);}
            if(s.fireEnd&&s.iceEnd&&!Send(a,"DM196_PULSES"))return true;
        }
        if(!Has(a,0,"DM196_PULSES")||!s.reflect||!s.cactusReflect||!s.coldTall||!s.coldNut||!s.warm||!s.covered)return true;
        if(!Capture(a,"fusion_defendermoney_effects")||!Barrier(a,"DM196_EFFECTS"))return true;Advance(a,8);
    }
    else if(s.stage==8)
    {
        if(s.payloads.size()<4||!Capture(a,"fusion_defendermoney_silver")||!Capture(a,"fusion_defendermoney_money")||!Barrier(a,"DM196_MONEY"))return true;
        if(!Collect(a))return true;if(!s.credited)return true;Advance(a,9);
    }
    else if(s.stage==9)
    {
        if((s.actors[1029].stages&6)!=6||(s.actors[1073].stages&6)!=6||(s.actors[24].stages&6)!=6||!(s.actors[1039].stages&2)||
            !s.frames.contains("fusion_defendermoney_body_crack")||!s.frames.contains("fusion_defendermoney_tall_crack")||!s.frames.contains("fusion_defendermoney_cover_crack"))return true;
        if(a->NetIsHost())
        {
            if(s.producers[31].callbacks<2||s.producers[1140].callbacks<2||!s.cabbageCycle||s.damageKinds[60]<1||s.damageKinds[40]<1||s.damageKinds[100]<1||s.damageKinds[66]<1)return true;
            if(!Send(a,"DM196_NATURAL_DONE"))return true;
        }
        if(!Has(a,0,"DM196_NATURAL_DONE")||GetTickCount()-s.creditedAt<800||!Capture(a,"fusion_defendermoney_complete")||!Barrier(a,"DM196_COMPLETE"))return true;
        NetTrace("SMOKE_DM196_COMPLETE role=%s defenders=17 moneyKinds=5 canonicalActors=22 naturalFamilies=3 coldBiters=2 warm=1 reflect=2 potBiteProtection=1 credit25=1 money=%d sun=500 profileUnchanged=1 controlledFactory=1 paidCrafts=0 postSetupHealthTimerWrites=0 equipmentAndRecipientExtrasDeferred=1 sourceZombieAIUnported=1",Role(a),ctx->money);
        Advance(a,10);a->CoopSmokeWriteResult(true,nullptr);
    }
    return true;
}
}
