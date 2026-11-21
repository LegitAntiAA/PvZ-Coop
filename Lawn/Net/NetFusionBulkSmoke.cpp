#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetSmokeCoinClick.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Coin.h"
#include "../GridItem.h"
#include "../SeedPacket.h"
#include "../Cutscene.h"
#include "../System/PlayerInfo.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativePlacement.h"
#include "../Fusion/FusionRecipe.h"
#include "../Fusion/FusionNativeBatchActions.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../SexyAppFramework/DDInterface.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
using namespace LawnFusion;
struct Identity { uint16_t id=0; uint32_t token=0; };
struct State
{
    int stage=0,waitingOwn=0,introChecks=0,initialMoney=0,profileCoins=0,pauseTick=0;
    uint32_t since=0,lastChat=0,introFirst=0,introLast=0,pauseSince=0,creditSince=0,clickSince=0;
    uint16_t collected=0; int credit=0,maxFires=0; bool setup=false,recorded=false,clicked=false,credited=false,configured=false,resumeSent=false,gateLogged=false;
    std::vector<uint16_t> kinds; std::map<uint16_t,Identity> actors; std::map<uint16_t,int> coinValues;
    std::set<std::string> frames; std::map<std::string,int> firstDraw;
} s;
const char* Role(LawnApp* a) { return a->NetIsHost()?"host":"client"; }
int Slot(LawnApp* a) { return a->mNetManager->GetMySlot(); }
bool Require(LawnApp* a,bool ok,const char* why) { if(!ok)a->CoopSmokeFail(why); return ok; }
void Advance(LawnApp* a,int stage)
{ s.stage=stage; s.since=GetTickCount(); LawnNet::NetTrace("SMOKE_FUSION_BULK_STAGE role=%s stage=%d",Role(a),stage); }
bool Has(LawnApp* a,int who,const std::string& text)
{ for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==who&&line.mText==text)return true; return false; }
bool Send(LawnApp* a,const std::string& text)
{
    if(Has(a,Slot(a),text))return true;
    if(s.lastChat&&GetTickCount()-s.lastChat<800)return false;
    s.lastChat=GetTickCount(); a->mNetManager->SendChat(text); return Has(a,Slot(a),text);
}
bool Barrier(LawnApp* a,const std::string& text) { Send(a,text); return Has(a,0,text)&&Has(a,1,text); }
bool Key(LawnApp* a,SDL_Keycode key)
{
    SDL_Event d{}; d.type=SDL_EVENT_KEY_DOWN; d.key.windowID=SDL_GetWindowID(a->mSDLWindow); d.key.key=key; d.key.down=true;
    SDL_Event u=d; u.type=SDL_EVENT_KEY_UP; u.key.down=false;
    return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Bulk owned SDL key failed");
}
bool Click(LawnApp* a,float x,float y)
{
    SDL_Event m{}; m.type=SDL_EVENT_MOUSE_MOTION; m.motion.windowID=SDL_GetWindowID(a->mSDLWindow); m.motion.x=x; m.motion.y=y;
    SDL_Event d{}; d.type=SDL_EVENT_MOUSE_BUTTON_DOWN; d.button.windowID=m.motion.windowID; d.button.button=SDL_BUTTON_LEFT; d.button.clicks=1; d.button.down=true; d.button.x=x; d.button.y=y;
    SDL_Event u=d; u.type=SDL_EVENT_MOUSE_BUTTON_UP; u.button.down=false;
    return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Bulk owned SDL click failed");
}
bool Capture(LawnApp* a,const std::string& name)
{
    if(s.frames.contains(name))return true;
    if(!s.firstDraw.contains(name)){s.firstDraw[name]=a->mDrawCount;return false;}
    if(a->mDrawCount-s.firstDraw[name]<2)return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Bulk actual renderer capture failed"))return false;
    s.frames.insert(name); LawnNet::NetTrace("SMOKE_FUSION_BULK_RENDER role=%s frame=%s freshDraws=%d",Role(a),name.c_str(),a->mDrawCount-s.firstDraw[name]); return true;
}
bool ReadKinds(LawnApp* a)
{
    if(!s.kinds.empty())return true;
    const char* raw=std::getenv("PVZ_COOP_SMOKE_FUSION_BULK_KINDS");
    std::istringstream in(raw?raw:""); std::string part; std::set<uint16_t> unique;
    while(std::getline(in,part,','))
    {
        char* end=nullptr; const long kind=std::strtol(part.c_str(),&end,10);
        if(!Require(a,!part.empty()&&end&&!*end&&kind>=0&&kind<65535&&FusionFindPlant(int(kind))&&GetNativeSeedForPlant(uint16_t(kind))!=SEED_NONE&&unique.insert(uint16_t(kind)).second,"Bulk scope contains unknown/duplicate native source ID"))return false;
        s.kinds.push_back(uint16_t(kind));
    }
    return Require(a,s.kinds.size()==40,"Bulk requires exactly40 explicit scope IDs");
}
NativePlantData Provenance(uint16_t kind)
{
    NativePlantData data; data.kind=kind;
    if(IsNativeBasePlantKind(kind)||kind==1029||kind==1057||kind==1155)return data;
    if(kind==1028){data.firstParent=1027;return data;}
    const FusionRecipe* best=nullptr;
    for(size_t i=0;i<GetFusionRecipeCount();++i)
    {
        const auto& recipe=GetFusionRecipes()[i]; if(recipe.result!=kind)continue;
        if(!best)best=&recipe;
        if(recipe.existing!=kind&&recipe.used!=kind){best=&recipe;break;}
    }
    if(best){data.firstParent=best->existing;data.secondParent=best->used;}
    return data;
}
Plant* Kind(Board* board,uint16_t kind)
{
    Plant* plant=nullptr;
    while(board->mPlants.IterateNext(plant)){const auto* data=GetNativePlantData(plant);if(!plant->mDead&&plant->mIsOnBoard&&data&&data->kind==kind)return plant;}
    return nullptr;
}
int Fires(LawnApp* a)
{
    int count=0; Reanimation* reanim=nullptr;
    while(a->mEffectSystem->mReanimationHolder->mReanimations.IterateNext(reanim))if(!reanim->mDead&&reanim->mReanimationType==REANIM_JALAPENO_FIRE)++count;
    return count;
}
bool Matrix(LawnApp* a,bool record)
{
    auto* b=a->mBoard; std::set<uint16_t> ids; std::set<uint32_t> tokens;
    int count=0; Plant* live=nullptr; while(b->mPlants.IterateNext(live))count+=!live->mDead&&live->mIsOnBoard;
    if(count!=40)return false;
    for(size_t i=0;i<s.kinds.size();++i)
    {
        const auto kind=s.kinds[i]; auto* plant=Kind(b,kind); const auto* data=GetNativePlantData(plant); const auto* definition=FusionFindPlant(kind); const auto expected=Provenance(kind);
        if(!Require(a,plant&&data&&definition&&definition->hasStats&&plant->mNetId&&data->instanceToken&&ids.insert(plant->mNetId).second&&tokens.insert(data->instanceToken).second&&
            plant->mSeedType==GetNativeSeedForPlant(kind)&&plant->mPlantMaxHealth==definition->maxHealth&&plant->mPlantHealth==definition->maxHealth&&
            data->firstParent==expected.firstParent&&data->secondParent==expected.secondParent&&data->owner==255&&!data->packetId&&!data->randomOrigin&&
            plant->mPlantCol==int(i%10)&&plant->mRow==int(i/10)&&!plant->mIsAsleep,
            "Bulk actual native source identity/alias/HP/ancestry/cell differs"))return false;
        if(record)
        {
            s.actors[kind]={plant->mNetId,data->instanceToken};
            LawnNet::NetTrace("SMOKE_FUSION_BULK_ACTOR role=%s index=%zu kind=%u id=%u token=%u alias=%d HP=%d max=%d owner=%u packet=%u origin=%u parents=%u,%u column=%d row=%d",Role(a),i,kind,plant->mNetId,data->instanceToken,int(plant->mSeedType),plant->mPlantHealth,plant->mPlantMaxHealth,data->owner,data->packetId,data->randomOrigin,data->firstParent,data->secondParent,plant->mPlantCol,plant->mRow);
        }
        else if(!Require(a,s.actors.contains(kind)&&s.actors.at(kind).id==plant->mNetId&&s.actors.at(kind).token==data->instanceToken,"Bulk native actor generation changed across pause"))return false;
    }
    constexpr std::array<uint16_t,3> rejectedRecipes[]={{27,1,1184},{27,31,1136},{24,4,1190}};
    if(record)
        for(const auto& probe : rejectedRecipes)
        {
            auto* parent=Kind(b,probe[0]); const auto* before=GetNativePlantData(parent);
            const auto* recipe=FindFusionRecipe(probe[0],probe[1]);
            if(!Require(a,parent&&before&&recipe&&recipe->result==probe[2]&&GetNativeSeedForPlant(probe[2])==SEED_NONE,
                "Bulk negative recipe probe no longer names an unsupported source result"))return false;
            const uint16_t id=parent->mNetId; const uint32_t token=before->instanceToken; const int hp=parent->mPlantHealth;
            Plant* target=parent;
            const bool accepted=ResolveNativeFusionPlacementTarget(b,parent->mPlantCol,parent->mRow,probe[1],target);
            if(!Require(a,!accepted&&target==parent&&!parent->mDead&&parent->mNetId==id&&
                GetNativePlantData(parent)->instanceToken==token&&parent->mPlantHealth==hp,
                "Bulk unsupported source recipe fell through to ordinary placement or changed its parent"))return false;
            LawnNet::NetTrace("SMOKE_FUSION_BULK_RECIPE_REJECT role=%s parent=%u incoming=%u result=%u id=%u token=%u unchanged=1 readOnlyPolicy=1 paidCraft=0",
                Role(a),probe[0],probe[1],probe[2],id,token);
        }
    return true;
}
bool Setup(LawnApp* a)
{
    if(!a->NetIsHost()||s.setup)return true;
    auto* b=a->mBoard;
    if(!Require(a,b->mPlants.mSize==0&&!b->mPaused&&a->GetDialogCount()==0,"Bulk initial matrix requires empty unpaused native field"))return false;
    for(size_t i=0;i<s.kinds.size();++i)
    {
        const auto data=Provenance(s.kinds[i]); auto* plant=b->AddPlant(int(i%10),int(i/10),GetNativeSeedForPlant(data.kind),SEED_NONE,&data);
        if(!Require(a,plant&&GetNativePlantData(plant),"Bulk real native factory failed"))return false;
        NativeBatchPlantPlaced(plant); // Actual fresh-placement callback, never replayed for a ghost.
    }
    s.setup=true;
    LawnNet::NetTrace("SMOKE_FUSION_BULK_SETUP actors=40 initialFactoryOnly=1 paidCrafts=0 postSetupActorWrites=0 entryCallbacks=1 RNGReseeds=0");
    return Key(a,SDLK_ESCAPE);
}
bool ObserveCredit(LawnApp* a)
{
    auto* b=a->mBoard; const auto* ctx=GetNativeContext(b); Coin* coin=nullptr;
    while(b->mCoins.IterateNext(coin))if(IsNativeMoneyCoin(coin)&&coin->IsMoney())
    {
        const int value=GetNativeCoinValue(coin,0); if(value>0)s.coinValues[coin->mNetId]=value;
        if(coin->mIsBeingCollected&&s.coinValues.contains(coin->mNetId))
        {
            if(!Require(a,!s.collected||s.collected==coin->mNetId,"Bulk collected more than one real money coin"))return false;
            s.collected=coin->mNetId; s.credit=s.coinValues.at(coin->mNetId);
        }
    }
    if(!s.credited&&ctx->money!=s.initialMoney)
    {
        if(!Require(a,s.collected&&s.credit>0&&ctx->money==s.initialMoney+s.credit,"Bulk currency changed without exactly one observed real money credit"))return false;
        s.credited=true;s.creditSince=GetTickCount();
        LawnNet::NetTrace("SMOKE_FUSION_BULK_CREDIT role=%s id=%u delta=%d money=%d profileUnchanged=1 sunUnchanged=1",Role(a),s.collected,s.credit,ctx->money);
    }
    return Require(a,ctx->money==s.initialMoney+(s.credited?s.credit:0)&&a->mPlayerInfo->mCoins==s.profileCoins&&b->mSunMoney==500,"Bulk money/profile/sun ledger changed unexpectedly");
}
bool Collect(LawnApp* a)
{
    if(!a->NetIsClient()||s.clicked||!s.frames.contains("fusion_bulk_money"))return true;
    static LawnNet::NetSmokeCoinClickState stable;
    auto* b=a->mBoard; Coin* coin=nullptr;int x=0,y=0;
    while(b->mCoins.IterateNext(coin))if(!coin->mDead&&coin->mHitGround&&!coin->mIsBeingCollected&&IsNativeMoneyCoin(coin)&&GetNativeCoinValue(coin,0)>0)
        if(LawnNet::NetSmokeSettledCoinPoint(b,coin,a->mCoopSmokeSnapshotsApplied,stable,x,y))
        {
            s.clicked=Click(a,float(x+b->mX+a->mDDInterface->mWideScreenOffsetX),float(y+b->mY+a->mDDInterface->mWideScreenOffsetY));
            s.clickSince=GetTickCount();
            LawnNet::NetTrace("SMOKE_FUSION_BULK_COIN_CLICK role=client candidate=%u point=%d,%d singleSDL=1 settledSnapshots=%u margin=8 ghost=%.4f,%.4f target=%.4f,%.4f",coin->mNetId,x,y,a->mCoopSmokeSnapshotsApplied-stable.coins.at(coin->mNetId).firstSnapshot,coin->mPosX,coin->mPosY,coin->mNetTargetX,coin->mNetTargetY);return s.clicked;
        }
    return true;
}
void GateDiagnostic(LawnApp* a)
{
    if(s.gateLogged)return;s.gateLogged=true;auto* b=a->mBoard;
    const auto at=std::find(s.kinds.begin(),s.kinds.end(),11)-s.kinds.begin();auto* crater=b->GetCraterAt(int(at%10),int(at/10));
    LawnNet::NetTrace("SMOKE_FUSION_BULK_GATE role=%s elapsedMs=%u doomAlive=%d jalaAlive=%d crater=%d craterTicks=%d fires=%d maxFires=%d marker=%d clicked=%d collected=%u credited=%d delta=%d money=%d expected=%d creditAgeMs=%u moneyFrame=%d completeFrame=%d ackHost=%d ackClient=%d",
        Role(a),GetTickCount()-s.since,Kind(b,11)?1:0,Kind(b,16)?1:0,crater&&!crater->mDead?1:0,crater?crater->mGridItemCounter:0,Fires(a),s.maxFires,b->mFwooshCountDown,s.clicked?1:0,s.collected,s.credited?1:0,s.credit,GetNativeContext(b)->money,s.initialMoney+(s.credited?s.credit:0),s.creditSince?GetTickCount()-s.creditSince:0,s.frames.contains("fusion_bulk_money")?1:0,s.frames.contains("fusion_bulk_complete")?1:0,Has(a,0,"FB182_COMPLETE")?1:0,Has(a,1,"FB182_COMPLETE")?1:0);
    int shown=0;Reanimation* r=nullptr;
    while(a->mEffectSystem->mReanimationHolder->mReanimations.IterateNext(r))if(!r->mDead&&r->mReanimationType==REANIM_JALAPENO_FIRE&&shown++<3)
        LawnNet::NetTrace("SMOKE_FUSION_BULK_FIRE role=%s loop=%d rate=%.6g time=%.6g frames=%d attached=%d",Role(a),int(r->mLoopType),r->mAnimRate,r->mAnimTime,r->mFrameCount,r->mIsAttachment?1:0);
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionBulk(LawnApp* a)
{
    const char* enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_BULK");const char* native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
    if(!a||!enabled||std::strcmp(enabled,"1")||!native||std::strcmp(native,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
    if(a->mCoopSmokeResultWritten)return true;
    if(!s.since)s.since=GetTickCount();
    if(GetTickCount()-s.since>90000){if(s.stage==7)GateDiagnostic(a);char why[96];sprintf_s(why,"Bulk deadline stage=%d role=%s",s.stage,Role(a));a->CoopSmokeFail(why);return true;}
    if(!ReadKinds(a)||!a->mNetManager||!a->mNetManager->IsActive())return true;
    a->mCoopSmokePeerReady=true;
    if(s.stage==0)
    {
        if(a->NetIsHost()&&!s.configured){a->mNetManager->SetDifficulty(1);s.configured=true;}
        if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
        if(a->NetIsClient()){Advance(a,1);Send(a,"FB182_START");}
        else if(Has(a,1,"FB182_START")){Advance(a,1);std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}
        return true;
    }
    if(!a->mBoard||!IsNativeFusionBoard(a->mBoard)){if(s.stage==1&&a->NetIsClient())Send(a,"FB182_START");return true;}
    auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);auto* chooser=a->mSeedChooserScreen;const auto& bank=ctx->selection.State();const int slot=Slot(a);
    if(!Require(a,ctx->validation&&!a->mFusionRuntime&&ctx->visuals&&b->GetColumnCount()==10,"Bulk requires actual native world/source resource initialization"))return true;
    if(s.stage>=5)s.maxFires=(std::max)(s.maxFires,Fires(a));
    if(s.stage==1)
    {
        if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||!b->mCutScene->mSeedChoosing||chooser->mSeedsInFlight)return true;
        if(!Require(a,bank.activeMask==3&&bank.targetCount==10&&bank.quotas[0]==5&&bank.quotas[1]==5&&ctx->eligible.size()>=10,"Bulk live native chooser quota missing"))return true;
        int own=0;for(const auto& packet:bank.packets)own+=packet.owner==slot;
        if(own<s.waitingOwn)return true;
        if(own<bank.quotas[slot]){int x=0,y=0;chooser->GetSeedPositionInChooser(slot+2*own,x,y);if(Click(a,float(chooser->mX+x+25),float(chooser->mY+y+35-int(chooser->mScrollbar->mScrollValue))))s.waitingOwn=own+1;return true;}
        if(bank.packets.size()==bank.targetCount)Advance(a,2);
    }
    else if(s.stage==2)
    {
        if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}
        auto* button=chooser?chooser->mStartButton:nullptr;if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;
        const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
        if(!button->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"Bulk real Ready hover failed");}
        else if(Click(a,x,y))Advance(a,3);
    }
    else if(s.stage==3)
    {
        if(bank.phase==FusionBankPhase::Intro){if(!s.introFirst)s.introFirst=GetTickCount();s.introLast=GetTickCount();++s.introChecks;return true;}
        if(bank.phase!=FusionBankPhase::Playing||a->mGameScene!=SCENE_PLAYING||chooser)return true;
        if(!Require(a,s.introChecks>=10&&s.introLast-s.introFirst>=1000&&b->mSunMoney==500,"Bulk bypassed actual full native intro"))return true;
        s.initialMoney=ctx->money;s.profileCoins=a->mPlayerInfo->mCoins;Advance(a,4);
    }
    else if(s.stage==4)
    {
        if(!Barrier(a,"FB182_SETUP"))return true;
        if(!Setup(a))return true;Advance(a,5);
    }
    else if(s.stage==5)
    {
        if(!b->mPaused)return true;
        if(!s.pauseSince){s.pauseSince=GetTickCount();s.pauseTick=b->mMainCounter;}
        if(!Require(a,b->mMainCounter==s.pauseTick,"Bulk real Escape pause advanced world clock"))return true;
        if(!Matrix(a,!s.recorded))return true;s.recorded=true;
        if(!ObserveCredit(a)||!Capture(a,"fusion_bulk_matrix_paused"))return true;
        if(Barrier(a,"FB182_MATRIX")){NetTrace("SMOKE_FUSION_BULK_MATRIX role=%s actors=40 nativeBoard=1 secondWorld=0 resourcesInitialized=1 paused=1 localTick=%d stableMs=%u fires=%d",Role(a),b->mMainCounter,GetTickCount()-s.pauseSince,Fires(a));Advance(a,6);}
    }
    else if(s.stage==6)
    {
        if(a->NetIsHost()&&!s.resumeSent){s.resumeSent=Key(a,SDLK_ESCAPE);return true;}
        if(b->mPaused||a->GetDialogCount())return true;
        if(!Matrix(a,false)||!Capture(a,"fusion_bulk_matrix_live"))return true;
        NetTrace("SMOKE_FUSION_BULK_LIVE role=%s actors=40 sameGenerations=1 instantKindsStillAlive=1 fires=%d",Role(a),Fires(a));Advance(a,7);
    }
    else if(s.stage==7)
    {
        if(!ObserveCredit(a))return true;
        if(s.clicked&&!s.credited&&GetTickCount()-s.clickSince>4000){GateDiagnostic(a);a->CoopSmokeFail("Bulk single settled SDL coin click received no credit within4s");return true;}
        if(GetTickCount()-s.since>10000)GateDiagnostic(a);
        if(!s.coinValues.empty()&&!Capture(a,"fusion_bulk_money"))return true;
        if(!Collect(a))return true;
        if(Kind(b,11)||Kind(b,16))return true;
        const auto at=std::find(s.kinds.begin(),s.kinds.end(),11)-s.kinds.begin();auto* crater=b->GetCraterAt(int(at%10),int(at/10));
        if(!Require(a,crater&&!crater->mDead&&crater->mGridItemCounter>0,"Bulk natural Doom death lacks actual native crater"))return true;
        // The stock host uses mFwooshCountDown as an enabled marker and never
        // decrements it. The real owned effect IDs define completion.
        if(Fires(a)!=0||!s.credited||GetTickCount()-s.creditSince<800)return true;
        if(!Require(a,s.maxFires>=12,"Bulk never observed actual native row fire presentation"))return true;
        for(const auto kind:s.kinds)if(kind!=11&&kind!=16)
        {auto* p=Kind(b,kind);const auto* data=GetNativePlantData(p);if(!Require(a,p&&data&&p->mNetId==s.actors.at(kind).id&&data->instanceToken==s.actors.at(kind).token&&p->mPlantHealth==p->mPlantMaxHealth,"Bulk survivor generation/HP changed without target NPCs"))return true;}
        GateDiagnostic(a);
        if(!Capture(a,"fusion_bulk_complete")||!Barrier(a,"FB182_COMPLETE"))return true;
        NetTrace("SMOKE_FUSION_BULK_COMPLETE role=%s actors=40 survivors=38 naturalInstantDeaths=2 crater=1 rowEffectsRetired=1 moneyCoin=%u moneyCredit=%d initialMoney=%d finalMoney=%d sourceZombieAIUnported=1 paidCrafts=0 postSetupActorWrites=0 all40GameplayProven=0",Role(a),s.collected,s.credit,s.initialMoney,ctx->money);
        Advance(a,8);a->CoopSmokeWriteResult(true,nullptr);
    }
    return true;
}
}
