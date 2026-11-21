#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetCommon.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../Cutscene.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionRecipe.h"
#include "../Fusion/FusionNativeVisual.h"
#include "../Fusion/FusionNativeFireActionBatch.h"
#include "../Fusion/FusionNativeAction266.h"
#include "../Fusion/FusionNativeCold.h"
#include "../Fusion/FusionNativeProjectileMetadata.h"
#include "../../SexyAppFramework/ScrollbarWidget.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>

namespace
{
using namespace LawnFusion;
using LawnNet::NetTrace;
struct Spec {uint16_t kind;int col,row;};
constexpr std::array<Spec,20> specs={{{1002,0,0},{1009,3,0},{1015,6,0},{1060,1,1},
    {1070,1,2},{1071,4,2},{1072,6,2},{1074,4,1},{1075,7,1},{1087,0,3},
    {1145,3,3},{1146,5,3},{1163,9,4},{1170,8,3},{1190,6,4},{1057,0,4},
    {1155,3,4},{1048,9,0},{1189,9,1},{1039,9,2}}};
struct Ref {uint16_t id=0;uint32_t token=0;int x=0;};
struct State
{
    int stage=0,waiting=0,entry=-1,start=0,introChecks=0,mask=0;
    uint32_t since=0,chat=0,wheel=0,epoch=0,introFirst=0,introLast=0,pauseSamples=0,diag=0;
    bool configured=false,setup=false,pauseRequested=false,resumeRequested=false,baseline=false;
    std::array<Ref,20> actors{};std::array<uint16_t,20> zombies{};
    std::array<int,20> initialHP{};std::array<int,20> initialZombieHP{};
    std::set<std::string> frames;std::map<std::string,int> draws;
} s;
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool ok,const char* why){if(!ok)a->CoopSmokeFail(why);return ok;}
void Advance(LawnApp* a,int n){s.stage=n;s.since=GetTickCount();s.diag=0;NetTrace("SMOKE_ACTION266_STAGE role=%s epoch=%u stage=%d",Role(a),s.epoch,n);}
std::string Tag(const char* name){return "A266_"+std::string(name)+"_"+std::to_string(s.epoch);}
bool Has(LawnApp* a,int who,const std::string& value){for(const auto& l:a->mNetManager->mChatLines)if(l.mSlot==who&&l.mText==value)return true;return false;}
bool Send(LawnApp* a,const std::string& value){if(Has(a,Slot(a),value))return true;if(s.chat&&GetTickCount()-s.chat<800)return false;s.chat=GetTickCount();a->mNetManager->SendChat(value);return Has(a,Slot(a),value);}
bool Barrier(LawnApp* a,const char* name){const auto tag=Tag(name);Send(a,tag);return Has(a,0,tag)&&Has(a,1,tag);}
bool Click(LawnApp* a,float x,float y)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Action266 scoped SDL click failed");
}
bool Escape(LawnApp* a)
{
    SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;
    SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Action266 scoped SDL Escape failed");
}
bool Choice(LawnApp* a,SeedChooserScreen* c,int i)
{
    int x=0,y=0;c->GetSeedPositionInChooser(i,x,y);const float cy=float(y+35-int(c->mScrollbar->mScrollValue));
    if(cy<158||cy>493)
    {
        if(s.wheel&&GetTickCount()-s.wheel<250)return false;s.wheel=GetTickCount();
        SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=float(c->mX+300);m.motion.y=float(c->mY+300);
        SDL_Event w{};w.type=SDL_EVENT_MOUSE_WHEEL;w.wheel.windowID=m.motion.windowID;w.wheel.y=cy<158?1.f:-1.f;
        Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&w),"Action266 scoped SDL chooser wheel failed");return false;
    }
    return std::abs(c->mScrollAmount)<=.05f&&Click(a,float(c->mX+x+25),float(c->mY)+cy);
}
bool Capture(LawnApp* a,const char* suffix)
{
    const std::string name="fusion_action266_0_"+std::string(suffix);if(s.frames.contains(name))return true;
    if(!s.draws.contains(name)){s.draws[name]=a->mDrawCount;return false;}if(a->mDrawCount-s.draws[name]<2)return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Action266 actual native BMP capture failed"))return false;
    s.frames.insert(name);NetTrace("SMOKE_ACTION266_RENDER role=%s frame=%s freshDraws=%d",Role(a),name.c_str(),a->mDrawCount-s.draws[name]);return true;
}
Plant* At(Board* b,int col,int row,int kind)
{
    Plant* p=nullptr;while(b->IteratePlants(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&d&&d->kind==kind&&p->mPlantCol==col&&p->mRow==row)return p;}return nullptr;
}
Plant* Actor(Board* b,int i)
{
    auto* p=At(b,specs[i].col,specs[i].row,specs[i].kind);const auto* d=GetNativePlantData(p);
    return d&&p->mNetId==s.actors[i].id&&d->instanceToken==s.actors[i].token?p:nullptr;
}
Zombie* Target(Board* b,int i){Zombie* z=nullptr;while(b->IterateZombies(z))if(!z->mDead&&z->mNetId==s.zombies[i])return z;return nullptr;}
Plant* MakePlant(LawnApp* a,int kind,int col,int row)
{
    auto* b=a->mBoard;NativePlantData d;d.kind=uint16_t(kind);d.owner=uint8_t(col%2);
    for(size_t i=0;i<GetFusionRecipeCount();++i)
    {const auto& recipe=GetFusionRecipes()[i];if(recipe.result==kind){d.firstParent=recipe.existing;d.secondParent=recipe.used;break;}}
    if(!IsNativeBasePlantKind(d.kind)&&d.firstParent==kFusionNoPlant&&d.secondParent==kFusionNoPlant)
    {
        // Unpaid world fixture, with no fabricated recipe ancestry.
        d.owner=LawnNet::NET_SHARED_OWNER;d.packetId=0;d.randomOrigin=0;
    }
    std::string error;const bool loaded=EnsureNativePlantVisuals(b,uint16_t(kind),&error);
    if(!Require(a,loaded,error.empty()?"Action266 source model missing":error.c_str()))return nullptr;
    auto* p=b->AddPlant(col,row,GetNativeSeedForPlant(uint16_t(kind)),SEED_NONE,&d);const auto* bound=GetNativePlantData(p);
    if(!Require(a,p&&bound&&bound->kind==kind&&bound->instanceToken&&p->mNetId&&!p->mDead,"Action266 initial native typed factory failed"))return nullptr;
    return p;
}
bool Setup(LawnApp* a)
{
    auto* b=a->mBoard;
    if(!Require(a,a->NetIsHost()&&!b->mPaused&&!b->mPlants.mSize&&!b->mZombies.mSize,"Action266 setup requires empty unpaused native world"))return false;
    for(const auto& spec:specs)if(!MakePlant(a,spec.kind,spec.col,spec.row))return false;
    if(!MakePlant(a,1009,2,0)||!MakePlant(a,20,1,3))return false;
    At(b,0,3,1087)->mPlantHealth=3700;
    At(b,1,1,1060)->mPlantHealth=350;At(b,4,1,1074)->mPlantHealth=200;
    if(!Require(a,HandleNativeAction266Crush(At(b,1,1,1060))&&HandleNativeAction266Crush(At(b,4,1,1074)),"Action266 initial source NULL crush refused"))return false;
    for(int index:{1,12,14})At(b,specs[index].col,specs[index].row,specs[index].kind)->mPlantHealth=1;
    for(int i=0;i<20;++i)
    {
        if(i==9||i==13)continue;
        const auto& spec=specs[i];
        auto* z=b->AddZombieInRow(ZOMBIE_NORMAL,spec.row,0);
        if(!Require(a,z&&z->mNetId&&a->ReanimationTryToGet(z->mBodyReanimID),"Action266 initial NPC failed"))return false;
        const bool fragile=i==0||i==2;
        z->mBodyHealth=z->mBodyMaxHealth=fragile?200:32000;
        z->mHelmHealth=z->mHelmMaxHealth=0;
        int column=spec.col;
        if(i==0||i==2||i==15)++column;
        if(i==11)column=7;
        z->mPosX=float(b->GetCellCenterX(column,spec.row)-60);z->mX=int(z->mPosX);
        z->mPosY=z->GetPosYBasedOnRow(z->mRow);z->mY=int(z->mPosY);
        z->mVelX=0;
        if(i==8)
        {z->SetColdCounters(1000,0);NativeZombieColdData cold;cold.level=10;cold.chilledTicks=1000;
         if(!Require(a,AttachNativeZombieColdData(b,z,cold),"Action266 initial FireRock cold target failed"))return false;}
        s.zombies[i]=z->mNetId;s.initialZombieHP[i]=z->mBodyHealth;
        NetTrace("SMOKE_ACTION266_INITIAL_NPC index=%d sourceKind=%u id=%u row=%d x=%.9g body=%d initialOnly=1",i,spec.kind,z->mNetId,z->mRow,z->mPosX,z->mBodyHealth);
    }
    if(!Require(a,TryActivateNativeAction266Skill(At(b,5,3,1146)),"Action266 initial GoldGarlic skill refused"))return false;
    s.setup=true;s.start=b->mMainCounter;
    NetTrace("SMOKE_ACTION266_SETUP kinds=20 actors=22 initialLowHP=1009,1163,1190 initialGoldSkill=1 initialNULLcrush=1060,1074 nativeFactory=1 acquisition=NotExecuted initialOnly=1");
    return true;
}
bool Baseline(LawnApp* a)
{
    auto* b=a->mBoard;
    for(int i=0;i<20;++i)
    {
        const auto& spec=specs[i];auto* p=At(b,spec.col,spec.row,spec.kind);const auto* d=GetNativePlantData(p);
        if(!d)return false;
        if(!Require(a,p->mNetId&&d->instanceToken&&p->mIsOnBoard,"Action266 source identity differs"))return false;
        if(!s.actors[i].id)
        {
            s.actors[i]={p->mNetId,d->instanceToken,p->mX};s.initialHP[i]=p->mPlantHealth;
            NetTrace("SMOKE_ACTION266_BIRTH role=%s index=%d kind=%u id=%u token=%u col=%d row=%d hp=%d max=%d speed=%.9g alias=%d ghost=%d",Role(a),i,spec.kind,p->mNetId,d->instanceToken,spec.col,spec.row,p->mPlantHealth,p->mPlantMaxHealth,d->animationSpeed,p->mSeedType,p->mNetGhost?1:0);
        }
    }
    if(a->NetIsClient())
    {
        for(int i=0;i<20;++i)
        {
            if(i==9||i==13)continue;const auto& spec=specs[i];Zombie* z=nullptr;float best=10000;
            int column=spec.col;if(i==0||i==2||i==15)++column;if(i==11)column=7;
            const float wanted=float(b->GetCellCenterX(column,spec.row)-60);
            while(b->IterateZombies(z))if(!z->mDead&&z->mRow==spec.row&&std::abs(z->mPosX-wanted)<best)
            {best=std::abs(z->mPosX-wanted);s.zombies[i]=z->mNetId;s.initialZombieHP[i]=z->mBodyHealth;}
            if(!s.zombies[i])return false;
        }
    }
    return At(b,2,0,1009)&&At(b,1,3,20);
}
void Observe(LawnApp* a)
{
    auto* b=a->mBoard;const int elapsed=b->mMainCounter-s.start;
    for(int i=0;i<20;++i)
    {
        auto* p=Actor(b,i);auto* z=Target(b,i);bool passed=false;
        const auto* d=GetNativePlantData(p);
        switch(i)
        {
        case 0:passed=!p&&(!z||z->mBodyHealth<200);break;
        case 1:passed=!p;break;
        case 2:passed=At(b,7,0,4)!=nullptr;break;
        case 3:case 4:case 16:
            passed=z&&z->mBodyHealth<s.initialZombieHP[i];break;
        case 5:{const auto* action=GetNativeZombieActionData(z);passed=z&&z->mBodyHealth<32000&&action&&action->jalaedOnDeath;break;}
        case 6:case 7:{const auto* cold=GetNativeZombieColdData(z);passed=z&&z->mBodyHealth<32000&&cold&&(cold->level>0||cold->chilledTicks>0||cold->frozenTicks>0);break;}
        case 8:{const auto* cold=GetNativeZombieColdData(z);passed=z&&z->mBodyHealth<32000&&cold&&cold->level==0&&z->mChilledCounter==0;break;}
        case 9:passed=p&&p->mPlantHealth>s.initialHP[i];break;
        case 10:case 11:{const auto* garlic=GetNativeZombieGarlicData(z);passed=z&&(z->mRow!=specs[i].row||(garlic&&FusionGarlicActionActive(*garlic)));break;}
        case 12:passed=!p&&At(b,8,4,4);break;
        case 13:{int count=0;Plant* child=nullptr;while(b->IteratePlants(child)){const auto* cd=GetNativePlantData(child);if(!child->mDead&&cd&&cd->kind==1170&&child->mPlantCol==8&&child->mRow==3)++count;}passed=count==3&&d&&d->batchMode==1;break;}
        case 14:passed=!p;break;
        case 15:passed=!p&&z&&z->mBodyHealth<32000;break;
        case 17:passed=p&&p->mPlantHealth<s.initialHP[i]&&z&&z->mBodyHealth<32000;break;
        case 18:case 19:{const auto* cold=GetNativeZombieColdData(z);passed=p&&p->mPlantHealth<s.initialHP[i]&&cold&&(cold->level>0||cold->chilledTicks>0||cold->frozenTicks>0);break;}
        }
        if(passed)s.mask|=1<<i;
    }
    if(elapsed>60&&elapsed<250)Capture(a,"flight");
    if(!s.diag||GetTickCount()-s.diag>5000)
    {s.diag=GetTickCount();NetTrace("SMOKE_ACTION266_OBSERVE role=%s elapsed=%d mask=%05x expected=fffff",Role(a),elapsed,s.mask);}
}
}
namespace LawnNet
{
bool NetSmokeUpdateFusionAction266(LawnApp* a)
{
    const auto* enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_ACTION266"),*native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
    if(!a||!enabled||std::strcmp(enabled,"1")||!native||std::strcmp(native,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
    if(a->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();
    if(GetTickCount()-s.since>200000){char why[120];sprintf_s(why,"Action266 deadline stage=%d mask=%05x expected=fffff",s.stage,s.mask);a->CoopSmokeFail(why);return true;}
    if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
    if(s.stage==0)
    {
        if(a->NetIsHost()&&!s.configured){a->mNetManager->SetDifficulty(1);s.configured=true;}
        if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
        if(a->NetIsClient()){Advance(a,1);Send(a,"A266_BEGIN");}
        else if(Has(a,1,"A266_BEGIN")){Advance(a,1);std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;
    }
    if(!a->mBoard||!IsNativeFusionBoard(a->mBoard)){if(s.stage==1&&a->NetIsClient())Send(a,"A266_BEGIN");return true;}
    auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);if(!s.epoch)s.epoch=ctx->epoch;
    if(!Require(a,ctx->epoch==s.epoch&&ctx->validation&&ctx->visuals&&!a->mFusionRuntime&&b->GetColumnCount()==10,"Action266 requires native validation world"))return true;
    const auto& bank=ctx->selection.State();auto* chooser=a->mSeedChooserScreen;
    if(s.stage==1)
    {
        if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||chooser->mSeedsInFlight||!b->mCutScene->mSeedChoosing)return true;
        int own=0;for(const auto& p:bank.packets)if(p.owner==Slot(a))++own;
        if(own<s.waiting)return true;
        if(own<5)
        {
            const int wanted=5*Slot(a)+own;int dense=-1;for(size_t i=0;i<ctx->eligible.size();++i)if(ctx->eligible[i].kind==wanted&&!ctx->eligible[i].isExtra)dense=int(i);
            if(!Require(a,dense>=0,"Action266 native chooser source base missing"))return true;if(Choice(a,chooser,dense))s.waiting=own+1;return true;
        }
        if(bank.packets.size()==10&&Capture(a,"chooser"))Advance(a,2);
    }
    else if(s.stage==2)
    {
        if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}
        auto* button=chooser?chooser->mStartButton:nullptr;if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;
        const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
        if(!button->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"Action266 Ready hover failed");}
        else if(Click(a,x,y))Advance(a,3);
    }
    else if(s.stage==3)
    {
        if(bank.phase==FusionBankPhase::Intro){if(!s.introFirst)s.introFirst=GetTickCount();s.introLast=GetTickCount();++s.introChecks;return true;}
        if(bank.phase!=FusionBankPhase::Playing||chooser||a->mGameScene!=SCENE_PLAYING)return true;
        if(!Require(a,s.introChecks>=10&&s.introLast-s.introFirst>=1000,"Action266 full native intro was skipped"))return true;
        if(s.entry<0){s.entry=a->mAppCounter;return true;}if(a->mAppCounter<=s.entry)return true;
        if(a->NetIsHost()&&!s.setup){if(!Setup(a))return true;s.pauseRequested=Escape(a);}
        if(b->mPaused)Advance(a,4);
    }
    else if(s.stage==4)
    {
        if(!b->mPaused){if(s.baseline){s.pauseRequested=false;s.start=b->mMainCounter;Advance(a,5);}return true;}
        if(!Baseline(a))return true;if(!s.pauseSamples){s.pauseSamples=Samples(a);return true;}if(Samples(a)-s.pauseSamples<2)return true;
        if(!Capture(a,"before")||!Barrier(a,"BASELINE"))return true;s.baseline=true;
        if(a->NetIsHost()&&!s.resumeRequested)s.resumeRequested=Escape(a);
    }
    else if(s.stage==5)
    {
        Observe(a);if(b->mMainCounter-s.start<7000)return true;
        if(!Require(a,s.mask==0xfffff,"Action266 grouped effects/expiry acceptance incomplete"))return true;
        if(!Barrier(a,"EFFECTS"))return true;
        if(a->NetIsHost()&&!s.pauseRequested)s.pauseRequested=Escape(a);
        if(b->mPaused){s.pauseSamples=Samples(a);Advance(a,6);}
    }
    else if(s.stage==6)
    {
        if(!b->mPaused||Samples(a)-s.pauseSamples<2||!Capture(a,"complete")||!Barrier(a,"FINAL"))return true;
        NetTrace("SMOKE_ACTION266_CASE role=%s group=0 epoch=%u kinds=20 mask=%05x postSetupActorWrites=0",Role(a),s.epoch,s.mask);
        NetTrace("SMOKE_ACTION266_COMPLETE role=%s epoch=%u kinds=20 mask=%05x sourceAcquisition=NotExecuted controlNPCParity=NotExecuted initialActors=22 initialLowHP=1009,1163,1190 initialGoldSkill=1 postSetupActorWrites=0 snapshots=%u",Role(a),s.epoch,s.mask,Samples(a));
        a->CoopSmokeWriteResult(true,nullptr);
    }
    return true;
}
}
