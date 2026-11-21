#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Coin.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../Widget/GameButton.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionEconomyRules.h"
#include "../Fusion/FusionProducerRules.h"
#include "../Fusion/FusionNativeEconomy.h"
#include "../Fusion/FusionNativeMoneyAction.h"
#include "../../SexyAppFramework/DDInterface.h"
#include "../../SexyAppFramework/ScrollbarWidget.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <sstream>
#include <string>

namespace
{
using namespace LawnFusion;
using LawnNet::NetTrace;
constexpr uint16_t kinds[]={1031,1184,1136,1137,1188,1145};
constexpr int columns[]={1,3,5,7,9,1},rows[]={0,1,2,3,4,4};
struct Ref {uint16_t id=0;uint32_t token=0;};
struct State
{
    int stage=0,waiting=0,introChecks=0,entry=-1,birthTick=0,pauseTick=0;
    int growthTickSum=0,previousGrow=-1,previousTick=0,lastChargeTick=0,previousCharges=0,chargeEvents=0;
    int skillTick=0,previousLargeBirth=0,largeSun=0,refundPot=0,refundSkill=0,refundGarlic=0;
    uint32_t epoch=0,since=0,chat=0,wheel=0,introFirst=0,introLast=0,pauseSamples=0,diagnostic=0,clickAt=0;
    bool configured=false,setup=false,discovered=false,pauseQueued=false,resumeQueued=false;
    bool smallSun=false,potSun=false,silverMoney=false,goldMoney=false,grown=false,bigSun=false;
    bool potUpgrade=false,skill=false,garlicUpgrade=false,skillDrain=false;
    std::array<Ref,6> actors{};std::array<bool,3> clicks{};
    std::array<uint32_t,3> toolSamples{};
    std::set<uint16_t> coins;std::set<std::string> frames;std::map<std::string,int> draws;
} s;
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool ok,const char* why){if(!ok)a->CoopSmokeFail(why);return ok;}
void Advance(LawnApp* a,int stage){s.stage=stage;s.since=GetTickCount();NetTrace("SMOKE_ECONOMY266_STAGE role=%s epoch=%u stage=%d",Role(a),s.epoch,stage);}
std::string Tag(const char* name){return "EC266_"+std::string(name)+"_"+std::to_string(s.epoch);}
bool Has(LawnApp* a,int slot,const std::string& text){for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==slot&&line.mText==text)return true;return false;}
bool Send(LawnApp* a,const std::string& text){if(Has(a,Slot(a),text))return true;if(s.chat&&GetTickCount()-s.chat<800)return false;s.chat=GetTickCount();a->mNetManager->SendChat(text);return Has(a,Slot(a),text);}
bool Barrier(LawnApp* a,const char* name){const auto text=Tag(name);Send(a,text);return Has(a,0,text)&&Has(a,1,text);}
bool Click(LawnApp* a,float x,float y)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;
    return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Economy266 owned SDL click failed");
}
bool BoardClick(LawnApp* a,float x,float y){return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY);}
bool Escape(LawnApp* a){SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Economy266 owned SDL Escape failed");}
bool Choice(LawnApp* a,SeedChooserScreen* c,int index)
{
    int x=0,y=0;c->GetSeedPositionInChooser(index,x,y);const float cy=float(y+35-int(c->mScrollbar->mScrollValue));
    if(cy<158||cy>493)
    {
        if(s.wheel&&GetTickCount()-s.wheel<250)return false;s.wheel=GetTickCount();SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=float(c->mX+300);m.motion.y=float(c->mY+300);
        SDL_Event w{};w.type=SDL_EVENT_MOUSE_WHEEL;w.wheel.windowID=m.motion.windowID;w.wheel.y=cy<158?1.f:-1.f;
        Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&w),"Economy266 owned chooser wheel failed");return false;
    }
    return std::abs(c->mScrollAmount)<=.05f&&Click(a,float(c->mX+x+25),float(c->mY)+cy);
}
bool Capture(LawnApp* a,const char* suffix)
{
    const auto name="fusion_economy266_"+std::string(suffix);if(s.frames.contains(name))return true;
    if(!s.draws.contains(name)){s.draws[name]=a->mDrawCount;return false;}if(a->mDrawCount-s.draws[name]<2)return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Economy266 actual native BMP failed"))return false;
    s.frames.insert(name);NetTrace("SMOKE_ECONOMY266_RENDER role=%s frame=%s freshDraws=%d",Role(a),name.c_str(),a->mDrawCount-s.draws[name]);return true;
}
Plant* Cell(Board* b,int index)
{Plant* p=nullptr;while(b->mPlants.IterateNext(p))if(!p->mDead&&p->mIsOnBoard&&p->mPlantCol==columns[index]&&p->mRow==rows[index])return p;return nullptr;}
Plant* Actor(Board* b,int index)
{auto* p=Cell(b,index);const auto* d=GetNativePlantData(p);return p&&d&&p->mNetId==s.actors[index].id&&d->instanceToken==s.actors[index].token?p:nullptr;}
bool Setup(LawnApp* a)
{
    auto* b=a->mBoard;auto* context=GetNativeContext(b);
    if(!Require(a,!b->mPaused&&!b->mPlants.mSize&&!b->mZombies.mSize&&context->money==3000&&b->mSunMoney==500,"Economy266 factory needs fresh unpaused native activity"))return false;
    context->money=10000;b->mSunCountDown=b->mZombieCountDown=b->mZombieCountDownStart=100000;s.birthTick=b->mMainCounter;
    const uint16_t ancestry[][2]={{6,1},{27,1},{27,31},{1136,kFusionNoPlant},{1187,kFusionNoPlant},{29,31}};
    for(int i=0;i<6;++i)
    {
        NativePlantData data;data.kind=kinds[i];data.owner=1;data.firstParent=ancestry[i][0];data.secondParent=ancestry[i][1];
        auto* p=b->AddPlant(columns[i],rows[i],GetNativeSeedForPlant(kinds[i]),SEED_NONE,&data);const auto* d=GetNativePlantData(p);
        if(!Require(a,p&&d&&p->mNetId&&d->instanceToken&&!p->mDead&&p->mPlantHealth==p->mPlantMaxHealth,"Economy266 typed native factory failed"))return false;
        s.actors[i]={p->mNetId,d->instanceToken};
        if(i==0&&!Require(a,p->mStateCountdown==12001&&!d->batchMode,"Economy266 SunShroom natural12001 clock missing"))return false;
        if(i==4&&!Require(a,p->mStateCountdown==501&&!d->batchShotCount&&d->productionTicks==501&&!d->pendingProductionTicks,"Economy266 Gold initial501 clock missing"))return false;
        NetTrace("SMOKE_ECONOMY266_FACTORY role=host epoch=%u kind=%u id=%u token=%u owner=1 parents=%u,%u state=%d producer=%d pending=%d hp=%d initialOnly=1",s.epoch,d->kind,p->mNetId,d->instanceToken,d->firstParent,d->secondParent,p->mStateCountdown,d->productionTicks,d->pendingProductionTicks,p->mPlantHealth);
    }
    // This negative is confined to initial setup, before any snapshot/resume.
    // A damaged Gold actor makes a free heal observable as well as fee/coins.
    auto* silver=Actor(b,2);auto* gold=Actor(b,4);const auto silverBefore=*GetNativePlantData(silver),goldBefore=*GetNativePlantData(gold);
    const int originalFade=b->mBoardFadeOutCounter,originalHealth=gold->mPlantHealth;
    gold->mPlantHealth=originalHealth-1;const int damagedHealth=gold->mPlantHealth;
    const auto plantCount=b->mPlants.mSize,coinCount=b->mCoins.mSize;const int money=context->money,sun=b->mSunMoney;
    b->mBoardFadeOutCounter=0;
    const bool silverAccepted=TryNativeMoneyPlantAction(silver,1);
    const bool goldMoneyAccepted=TryNativeMoneyPlantAction(gold,1);
    const bool goldSkillAccepted=TryActivateNativeEconomySkill(gold);
    const auto* silverAfter=GetNativePlantData(silver);const auto* goldAfter=GetNativePlantData(gold);
    const bool unchanged=Actor(b,2)==silver&&Actor(b,4)==gold&&!silver->mDead&&!gold->mDead&&
        silverAfter&&goldAfter&&silverAfter->kind==silverBefore.kind&&silverAfter->instanceToken==silverBefore.instanceToken&&
        goldAfter->kind==goldBefore.kind&&goldAfter->instanceToken==goldBefore.instanceToken&&
        silver->mPlantHealth==silver->mPlantMaxHealth&&gold->mPlantHealth==damagedHealth&&
        goldAfter->batchShotCount==goldBefore.batchShotCount&&goldAfter->batchMode==goldBefore.batchMode&&
        goldAfter->productionTicks==goldBefore.productionTicks&&goldAfter->pendingProductionTicks==goldBefore.pendingProductionTicks&&
        b->mPlants.mSize==plantCount&&b->mCoins.mSize==coinCount&&context->money==money&&b->mSunMoney==sun;
    b->mBoardFadeOutCounter=originalFade;gold->mPlantHealth=originalHealth;
    if(!Require(a,!silverAccepted&&!goldMoneyAccepted&&!goldSkillAccepted&&unchanged,
        "Economy266 initial terminal gameplay gate mutated actor/fee/coin state"))return false;
    NetTrace("SMOKE_ECONOMY266_GATE_REJECT role=host epoch=%u MoneySilver=1 MoneyGold=1 EconomySkill=1 UIDunchanged=1 HPunchanged=1 money=10000 coins=%zu fadeRestored=1 initialOnly=1 actualTerminalUITransition=NotExecuted",s.epoch,size_t(coinCount));
    NetTrace("SMOKE_ECONOMY266_INITIAL role=host money=10000 sun=500 naturalClocks=1 cohort=5 auxPaidGarlic=1 initialOnly=1");s.setup=true;return true;
}
bool Discover(LawnApp* a)
{
    if(s.discovered)return true;auto* b=a->mBoard;
    if(GetNativeContext(b)->money!=10000||b->mSunMoney!=500)return false;
    for(int i=0;i<6;++i)
    {
        auto* p=Cell(b,i);const auto* d=GetNativePlantData(p);if(!d||d->kind!=kinds[i])return false;
        if(!Require(a,p->mNetId&&d->instanceToken&&d->owner==1&&p->mPlantHealth==p->mPlantMaxHealth,"Economy266 paused native identity/HP differs"))return false;
        s.actors[i]={p->mNetId,d->instanceToken};
    }
    auto* shroom=Actor(b,0);auto* gold=Actor(b,4);const auto* sd=GetNativePlantData(shroom);const auto* gd=GetNativePlantData(gold);
    if(!Require(a,sd&&!sd->batchMode&&gd&&!gd->batchShotCount&&!gd->batchMode&&!gd->pendingProductionTicks,
        "Economy266 baseline source stages changed before first charge"))return false;
    int birth=s.birthTick,tick=b->mMainCounter,growth=shroom->mStateCountdown,charge=gold->mStateCountdown;
    const auto prefix=Tag("CLOCK")+" ";
    if(a->NetIsHost())
    {
        const int age=tick-birth;
        if(!Require(a,age>=0&&age<kFusionGoldSunflowerInitialTicks&&
            growth==kFusionSunShroomGrowthTicks-age&&charge==kFusionGoldSunflowerInitialTicks-age&&
            gd->productionTicks==charge,"Economy266 host baseline clocks do not share exact native age"))return false;
        std::ostringstream proof;proof<<prefix<<birth<<' '<<tick<<' '<<growth<<' '<<charge<<' '
            <<shroom->mNetId<<' '<<sd->instanceToken<<' '<<gold->mNetId<<' '<<gd->instanceToken;
        if(!Send(a,proof.str()))return false;
    }
    else
    {
        // Native ghosts do not simulate or receive mStateCountdown, and the
        // presentation Board counter is not a host clock. Bind their actual
        // source metadata to the host's paused clock proof instead.
        bool found=false;
        for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==0&&line.mText.starts_with(prefix))
        {
            unsigned shroomId=0,shroomToken=0,goldId=0,goldToken=0;
            std::istringstream proof(line.mText.substr(prefix.size()));
            if(!Require(a,bool(proof>>birth>>tick>>growth>>charge>>shroomId>>shroomToken>>goldId>>goldToken)&&
                shroomId==shroom->mNetId&&shroomToken==sd->instanceToken&&goldId==gold->mNetId&&goldToken==gd->instanceToken,
                "Economy266 paused host clock proof has different actor generations"))return false;
            found=true;break;
        }
        if(!found)return false;
        const int age=tick-birth;
        if(!Require(a,age>=0&&age<kFusionGoldSunflowerInitialTicks&&
            growth==kFusionSunShroomGrowthTicks-age&&charge==kFusionGoldSunflowerInitialTicks-age&&gd->productionTicks==charge,
            "Economy266 client source metadata differs from exact paused host clock proof"))return false;
    }
    s.birthTick=birth;s.pauseTick=tick;s.previousTick=tick;s.previousGrow=growth;s.growthTickSum=birth+kFusionSunShroomGrowthTicks;
    NetTrace("SMOKE_ECONOMY266_BASELINE role=%s epoch=%u hostTick=%d birth=%d nativeAge=%d growth=%d charge=%d growthDue=%d chargeDue=%d presentationTick=%d clockProof=hostPausedSnapshotUIDToken money=10000 sun=500 pause=1 nativeActors=6",
        Role(a),s.epoch,tick,birth,tick-birth,growth,charge,s.growthTickSum,birth+kFusionGoldSunflowerInitialTicks,b->mMainCounter);s.discovered=true;return true;
}
void MoneyClick(LawnApp* a,int index,int step)
{
    if(!a->NetIsClient()||s.clicks[step]||a->mBoard->mPaused||a->GetDialogCount())return;
    auto* b=a->mBoard;if(s.clickAt&&GetTickCount()-s.clickAt<300)return;s.clickAt=GetTickCount();
    if(!b->mCursorObject||b->mCursorObject->mCursorType!=CURSOR_TYPE_MONEY_SIGN)
    {s.toolSamples[step]=0;auto rect=b->GetShovelButtonRect();b->GetZenButtonRect(OBJECT_TYPE_MONEY_SIGN,rect);BoardClick(a,float(rect.mX+rect.mWidth/2),float(rect.mY+rect.mHeight/2));return;}
    if(!s.toolSamples[step]){s.toolSamples[step]=Samples(a);return;}
    if(Samples(a)-s.toolSamples[step]<2)return;
    NetTrace("SMOKE_ECONOMY266_TOOL_ECHO role=client step=%d heldMoney=1 appliedSinceSelect=%u typedAuthorityUnchanged=1",step,Samples(a)-s.toolSamples[step]);
    auto* p=Actor(b,index);const auto* d=GetNativePlantData(p);if(!p||!d)return;
    s.clicks[step]=BoardClick(a,float(b->GetCellCenterX(columns[index],rows[index])),float(b->GridToPixelY(columns[index],rows[index])+50));
    NetTrace("SMOKE_ECONOMY266_MONEY_CLICK role=client step=%d kind=%u id=%u token=%u ownedSDL=1",step,d->kind,p->mNetId,d->instanceToken);
}
bool Observe(LawnApp* a)
{
    auto* b=a->mBoard;auto* context=GetNativeContext(b);
    for(int index:{2,5})
    {
        const bool done=index==2?s.potUpgrade:s.garlicUpgrade;const uint16_t expected=index==2?1137:1146;
        auto* p=Cell(b,index);const auto* d=GetNativePlantData(p);
        if(!done&&d&&d->kind==expected)
        {
            if(!Require(a,p->mNetId!=s.actors[index].id&&d->instanceToken!=s.actors[index].token&&d->owner==1&&d->firstParent==kinds[index]&&d->secondParent==kFusionNoPlant,"Economy266 paid upgrade generation/ancestry differs"))return false;
            s.actors[index]={p->mNetId,d->instanceToken};if(index==2)s.potUpgrade=true;else s.garlicUpgrade=true;
            NetTrace("SMOKE_ECONOMY266_UPGRADE role=%s oldKind=%u kind=%u id=%u token=%u ledger=%d paidClientCommand=1",Role(a),kinds[index],d->kind,p->mNetId,d->instanceToken,context->money);
        }
    }
    for(int i=0;i<6;++i)
    {auto* p=Actor(b,i);if(!Require(a,p&&p->mPlantHealth==p->mPlantMaxHealth,"Economy266 live native generation/HP lost"))return false;}
    auto* shroom=Actor(b,0);const auto* sd=GetNativePlantData(shroom);auto* gold=Actor(b,4);const auto* gd=GetNativePlantData(gold);
    if(a->NetIsHost()&&!s.grown&&b->mMainCounter!=s.previousTick)
    {
        if(!Require(a,shroom->mStateCountdown==(std::max)(0,s.growthTickSum-b->mMainCounter),"Economy266 growth clock did not advance with real Board ticks"))return false;
        s.previousTick=b->mMainCounter;s.previousGrow=shroom->mStateCountdown;
    }
    if(!s.grown&&sd->batchMode)
    {
        if(!Require(a,a->NetIsClient()||(shroom->mStateCountdown==0&&b->mMainCounter==s.growthTickSum),"Economy266 source growth missed exact native deadline"))return false;
        s.grown=true;NetTrace("SMOKE_ECONOMY266_GROW role=%s epoch=%u id=%u token=%u hostDue=%d presentationTick=%d naturalAge=12001 sourceModeSnapshot=1 hostCounterAsserted=%d",Role(a),s.epoch,shroom->mNetId,sd->instanceToken,s.growthTickSum,b->mMainCounter,a->NetIsHost()?1:0);
    }
    if(!s.skill&&gd->batchShotCount>static_cast<uint32_t>(s.previousCharges))
    {
        if(a->NetIsHost())
        {
            const int elapsed=b->mMainCounter-(s.lastChargeTick?s.lastChargeTick:s.birthTick);
            if(!Require(a,gd->batchShotCount==static_cast<uint32_t>(s.previousCharges+1)&&
                elapsed==(s.lastChargeTick?kFusionGoldSunflowerCycleTicks:kFusionGoldSunflowerInitialTicks),"Economy266 natural Gold501/1001 cadence differs"))return false;
        }
        ++s.chargeEvents;s.previousCharges=int(gd->batchShotCount);s.lastChargeTick=b->mMainCounter;
        NetTrace("SMOKE_ECONOMY266_CHARGE role=%s count=%u deadline=%d tick=%d natural=1",Role(a),gd->batchShotCount,gd->productionTicks,b->mMainCounter);
    }
    if(!s.skill&&context->money==5000)
    {s.skill=true;s.skillTick=b->mMainCounter;NetTrace("SMOKE_ECONOMY266_SKILL role=%s id=%u token=%u ledger=5000 fee=1000 charges=%u pending=%d",Role(a),gold->mNetId,gd->instanceToken,gd->batchShotCount,gd->pendingProductionTicks);}
    Coin* coin=nullptr;
    while(b->mCoins.IterateNext(coin))
    {
        if(coin->mDead||!coin->mNetId||!s.coins.insert(coin->mNetId).second)continue;
        const bool money=IsNativeMoneyCoin(coin);const int value=GetNativeCoinValue(coin,coin->IsSun()?coin->GetSunValue():0);
        int nearest=-1;double distance=1e20;
        for(int i=0;i<6;++i){auto* p=Actor(b,i);const double dx=coin->mPosX-p->mX,dy=coin->mPosY-p->mY;const double d=dx*dx+dy*dy;if(d<distance){distance=d;nearest=i;}}
        if(!Require(a,nearest>=0&&distance<120*120,"Economy266 emitted coin lacks nearby native source actor"))return false;
        if(money)
        {
            if(!Require(a,value==25&&coin->mType==COIN_SILVER&&(nearest==2||nearest==3),"Economy266 pot emitted wrong Money type/value"))return false;
            if(nearest==2&&!s.potUpgrade)s.silverMoney=true;if(nearest==3)s.goldMoney=true;
        }
        else
        {
            if(!Require(a,FusionSunCoinMetadataValid(coin->mType,value),"Economy266 SUN native type/value differs"))return false;
            if(nearest==0){if(value==15&&!s.grown)s.smallSun=true;if(value==25&&s.grown)s.bigSun=true;}
            if(nearest==1&&value==15)s.potSun=true;
            if(nearest==2&&value==25)++s.refundPot;
            if(nearest==4&&value==25)++s.refundSkill;
            if(nearest==5&&value==25)++s.refundGarlic;
            if(nearest==4&&value==50)
            {
                if(a->NetIsHost())
                {
                    const int birth=b->mMainCounter-coin->mCoinAge;
                    NetTrace("SMOKE_ECONOMY266_SUN50_BIRTH role=host id=%u birth=%d observed=%d age=%d previousBirth=%d interval=%d",coin->mNetId,birth,b->mMainCounter,coin->mCoinAge,s.previousLargeBirth,s.previousLargeBirth?birth-s.previousLargeBirth:0);
                    if(s.previousLargeBirth&&!Require(a,birth-s.previousLargeBirth==10,"Economy266 Gold SUN coroutine did not wait10 Board ticks"))return false;
                    s.previousLargeBirth=birth;
                }
                ++s.largeSun;
            }
        }
        NetTrace("SMOKE_ECONOMY266_COIN role=%s id=%u nearIndex=%d kind=%u value=%d money=%d type=%d tick=%d",Role(a),coin->mNetId,nearest,GetNativePlantData(Actor(b,nearest))->kind,value,money?1:0,int(coin->mType),b->mMainCounter);
    }
    if(s.skill&&!gd->batchMode&&!gd->batchShotCount)s.skillDrain=true;
    if(s.smallSun&&s.potSun&&s.silverMoney&&s.goldMoney)Capture(a,"production");
    if(!s.diagnostic||GetTickCount()-s.diagnostic>10000)
    {s.diagnostic=GetTickCount();NetTrace("SMOKE_ECONOMY266_OBSERVE role=%s stage=%d tick=%d growth=%d grown=%d small=%d big=%d pot15=%d pots25=%d,%d charges=%u events=%d money=%d upgrades=%d,%d skill=%d drain=%d large=%d refunds=%d,%d,%d",Role(a),s.stage,b->mMainCounter,shroom->mStateCountdown,s.grown,s.smallSun,s.bigSun,s.potSun,s.silverMoney,s.goldMoney,gd->batchShotCount,s.chargeEvents,context->money,s.potUpgrade,s.garlicUpgrade,s.skill,s.skillDrain,s.largeSun,s.refundPot,s.refundSkill,s.refundGarlic);}
    return true;
}
bool NaturalReady(){return s.smallSun&&s.potSun&&s.silverMoney&&s.goldMoney&&s.previousCharges>=3;}
bool Done(){return s.smallSun&&s.potSun&&s.silverMoney&&s.goldMoney&&s.grown&&s.bigSun&&s.potUpgrade&&s.garlicUpgrade&&s.skill&&s.skillDrain&&s.largeSun>=3&&s.refundPot==8&&s.refundSkill==2&&s.refundGarlic==2;}
}

namespace LawnNet
{
bool HandleFusionEconomy266Smoke(LawnApp* a)
{
    const auto* flag=std::getenv("PVZ_COOP_SMOKE_FUSION_ECONOMY266"),*native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
    if(!a||!flag||std::strcmp(flag,"1")||!native||std::strcmp(native,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
    if(a->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();
    if(GetTickCount()-s.since>210000U){char why[160];sprintf_s(why,"Economy266 bounded deadline stage=%d grown=%d big=%d skill=%d upgrades=%d,%d",s.stage,s.grown,s.bigSun,s.skill,s.potUpgrade,s.garlicUpgrade);a->CoopSmokeFail(why);return true;}
    if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
    if(s.stage==0)
    {
        if(a->NetIsHost()&&!s.configured){a->mNetManager->SetDifficulty(1);s.configured=true;}
        if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
        if(a->NetIsClient()){Advance(a,1);Send(a,"EC266_BEGIN");}
        else if(Has(a,1,"EC266_BEGIN")){Advance(a,1);std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;
    }
    if(!a->mBoard||!IsNativeFusionBoard(a->mBoard)){if(s.stage==1&&a->NetIsClient())Send(a,"EC266_BEGIN");return true;}
    auto* b=a->mBoard;const auto* context=GetNativeContext(b);if(!s.epoch)s.epoch=context->epoch;
    if(!Require(a,context->epoch==s.epoch&&context->validation&&context->visuals&&!a->mFusionRuntime&&b->GetColumnCount()==10&&b->mBackground==BACKGROUND_1_DAY,"Economy266 native Day5 validation world differs"))return true;
    const auto& bank=context->selection.State();auto* chooser=a->mSeedChooserScreen;
    if(s.stage==1)
    {
        if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||chooser->mSeedsInFlight||!b->mCutScene->mSeedChoosing)return true;
        if(!Require(a,bank.activeMask==3&&bank.targetCount==10&&bank.quotas[0]==5&&bank.quotas[1]==5,"Economy266 native chooser quotas differ"))return true;
        int own=0;for(const auto& p:bank.packets)own+=p.owner==Slot(a);if(own<s.waiting)return true;
        if(own<5){const int wanted=5*Slot(a)+own;int dense=-1;for(size_t i=0;i<context->eligible.size();++i)if(context->eligible[i].kind==wanted&&!context->eligible[i].isExtra)dense=int(i);if(!Require(a,dense>=0,"Economy266 baseline native chooser card missing"))return true;if(Choice(a,chooser,dense))s.waiting=own+1;return true;}
        if(bank.packets.size()!=10)return true;int rank=-1;for(int i=0;i<b->mSeedBank->mNumPackets;++i){const int next=NetSeedBankOwnerRank(Slot(a),b->mSeedBank->mSeedPackets[i].mNetOwnerSlot);if(!Require(a,next>=rank,"Economy266 own-first native bank differs"))return true;rank=next;}
        if(Capture(a,"chooser"))Advance(a,2);
    }
    else if(s.stage==2)
    {
        if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}auto* button=chooser?chooser->mStartButton:nullptr;if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;
        const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
        if(!button->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"Economy266 owned Ready hover failed");}else if(Click(a,x,y))Advance(a,3);
    }
    else if(s.stage==3)
    {
        if(bank.phase==FusionBankPhase::Intro){if(!s.introFirst)s.introFirst=GetTickCount();s.introLast=GetTickCount();++s.introChecks;return true;}
        if(bank.phase!=FusionBankPhase::Playing||a->mGameScene!=SCENE_PLAYING||chooser)return true;
        if(!Require(a,s.introChecks>=10&&s.introLast-s.introFirst>=1000&&b->mSunMoney==500&&context->money==3000,"Economy266 full native intro/initial ledgers differ"))return true;
        if(s.entry<0){s.entry=a->mAppCounter;return true;}if(a->mAppCounter>s.entry)Advance(a,4);
    }
    else if(s.stage==4)
    {
        if(a->NetIsHost()&&!s.setup){if(!Setup(a))return true;s.pauseQueued=Escape(a);}
        if(!b->mPaused)return true;if(!s.pauseSamples){s.pauseSamples=Samples(a);return true;}if(Samples(a)-s.pauseSamples<2)return true;
        if(Discover(a)&&Capture(a,"before")&&Barrier(a,"BEFORE")){s.resumeQueued=false;Advance(a,5);}
    }
    else if(s.stage==5||s.stage==8)
    {
        if(a->NetIsHost()&&!s.resumeQueued){s.resumeQueued=Escape(a);return true;}if(b->mPaused||a->GetDialogCount())return true;
        if(!Observe(a))return true;if(Barrier(a,s.stage==5?"RESUMED":"ACTIONS_RESUMED"))Advance(a,s.stage==5?6:9);
    }
    else if(s.stage==6)
    {
        if(!Observe(a))return true;if(!Require(a,context->money==10000&&b->mSunMoney==500,"Economy266 ledger changed before client Money actions"))return true;
        if(NaturalReady()&&Barrier(a,"NATURAL")){s.pauseQueued=false;s.pauseSamples=0;Advance(a,7);}
    }
    else if(s.stage==7)
    {
        if(a->NetIsHost()&&!s.pauseQueued){s.pauseQueued=Escape(a);return true;}if(!b->mPaused)return true;
        if(!s.pauseSamples){s.pauseSamples=Samples(a);return true;}if(Samples(a)-s.pauseSamples<2)return true;
        if(Observe(a)&&Capture(a,"production")&&Barrier(a,"PRODUCTION")){s.resumeQueued=false;Advance(a,8);}
    }
    else if(s.stage==9)
    {
        if(!Observe(a))return true;
        if(!s.potUpgrade)MoneyClick(a,2,0);
        else if(!s.skill)MoneyClick(a,4,1);
        else if(!s.garlicUpgrade)MoneyClick(a,5,2);
        if(s.largeSun>=1&&s.skill&&s.refundSkill>=2)Capture(a,"skill");
        if(s.grown)Capture(a,"grown");
        if(Done()&&Capture(a,"skill")&&Capture(a,"grown")&&Barrier(a,"OBSERVED")){s.pauseQueued=false;s.pauseSamples=0;Advance(a,10);}
    }
    else if(s.stage==10)
    {
        if(a->NetIsHost()&&!s.pauseQueued){s.pauseQueued=Escape(a);return true;}if(!b->mPaused)return true;
        if(!s.pauseSamples){s.pauseSamples=Samples(a);return true;}if(Samples(a)-s.pauseSamples<2)return true;
        if(!Observe(a)||!Require(a,Done()&&context->money==4000&&b->mSunMoney==500,"Economy266 final paid ledger/production evidence differs"))return true;
        if(Capture(a,"complete")&&Barrier(a,"FINAL"))
        {
            NetTrace("SMOKE_ECONOMY266_CASE role=%s epoch=%u kinds=1031,1184,1136,1137,1188 cohort=5 aux1146Paid=1 naturalGrowth12001=1 preSUN15=1 postSUN25=1 potSUN15=1 bothPotsMoney25=1 GoldSUN50=%d refunds=8,2,2 money=4000 frames=6 paused=1 postSetupActorWrites=0",Role(a),s.epoch,s.largeSun);
            NetTrace("SMOKE_ECONOMY266_COMPLETE role=%s activities=1 sourcePlants=5 paidClientActions=3 boundedFactory=1 nativeCounters=1 sourceCampaign=NotExecuted sourceNPCParity=NotExecuted",Role(a));a->CoopSmokeWriteResult(true,nullptr);Advance(a,11);
        }
    }
    return true;
}
}
