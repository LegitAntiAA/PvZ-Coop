#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Projectile.h"
#include "../Coin.h"
#include "../SeedPacket.h"
#include "../Cutscene.h"
#include "../Widget/GameButton.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativeVisual.h"
#include "../Fusion/FusionNativeShooterBatch.h"
#include "../Fusion/FusionNativeBasePlants.h"
#include "../Fusion/FusionRecipe.h"
#include "../../Sexy.TodLib/Reanimator.h"
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
#include <vector>

namespace
{
using namespace LawnFusion;
using LawnNet::NetTrace;
constexpr uint16_t kinds[3][6]={{1020,1152,1107,1096,1183,0},{1161,1165,1168,1109,1167,929},{1191,1192,0,0,0,0}};
constexpr int counts[]={5,6,2};
constexpr int targetHP=20000000;
struct Ref { uint16_t id=0;uint32_t token=0; };
struct State
{
    int group=0,stage=0,waiting=0,playingFrame=-1,baselineTick=0,introChecks=0;
    uint32_t epoch=0,previousEpoch=0,since=0,chat=0,wheel=0,diagnostic=0,pauseSample=0,introFirst=0,introLast=0;
    bool configured=false,setup=false,pauseRequested=false,resumed=false,finalPause=false,finalLogged=false;
    std::array<Ref,6> actors{};
    std::array<bool,6> phaseSeen{},rageSeen{};
    std::array<uint32_t,6> sequence{};
    std::array<int,6> callbacks{};
    std::vector<uint16_t> targets;
    std::set<uint16_t> payloads,projectiles;
    std::set<std::string> frames;
    std::map<std::string,int> draws;
    bool tier101=false,tier301=false,skill=false,skillBullet=false,chomp=false,pit=false,sun=false;
    std::string ids;
} s;
int Count(){return counts[s.group];}
int Column(int i){return s.group==1&&i==5?0:s.group==2&&i==1?3:2;}
int Row(int i){return s.group==2?(i?4:0):(std::min)(i,4);}
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool ok,const char* why){if(!ok)a->CoopSmokeFail(why);return ok;}
void Advance(LawnApp* a,int next){s.stage=next;s.since=GetTickCount();NetTrace("SMOKE_SHOOTER40_STAGE role=%s group=%d epoch=%u stage=%d",Role(a),s.group,s.epoch,next);}
std::string Tag(const char* name){return "S264_"+std::to_string(s.group)+"_"+std::to_string(s.epoch)+"_"+name;}
bool Has(LawnApp* a,int slot,const std::string& text){for(const auto& l:a->mNetManager->mChatLines)if(l.mSlot==slot&&l.mText==text)return true;return false;}
bool Send(LawnApp* a,const std::string& text)
{if(Has(a,Slot(a),text))return true;if(s.chat&&GetTickCount()-s.chat<800)return false;s.chat=GetTickCount();a->mNetManager->SendChat(text);return Has(a,Slot(a),text);}
bool Barrier(LawnApp* a,const char* name){const auto tag=Tag(name);Send(a,tag);return Has(a,0,tag)&&Has(a,1,tag);}
bool Click(LawnApp* a,float x,float y)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;
    return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Shooter40 owned SDL click failed");
}
bool Key(LawnApp* a)
{SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Shooter40 owned SDL Escape failed");}
bool Choice(LawnApp* a,SeedChooserScreen* c,int index)
{
    int x=0,y=0;c->GetSeedPositionInChooser(index,x,y);const float cy=float(y+35-int(c->mScrollbar->mScrollValue));
    if(cy<158||cy>493)
    {
        if(s.wheel&&GetTickCount()-s.wheel<250)return false;s.wheel=GetTickCount();
        SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=float(c->mX+300);m.motion.y=float(c->mY+300);
        SDL_Event w{};w.type=SDL_EVENT_MOUSE_WHEEL;w.wheel.windowID=m.motion.windowID;w.wheel.y=cy<158?1.f:-1.f;
        Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&w),"Shooter40 owned SDL chooser scroll failed");return false;
    }
    return std::abs(c->mScrollAmount)<=.05f&&Click(a,float(c->mX+x+25),float(c->mY)+cy);
}
bool Capture(LawnApp* a,const char* suffix)
{
    const auto name="fusion_shooter40_"+std::to_string(s.group)+"_"+suffix;
    if(s.frames.contains(name))return true;
    if(!s.draws.contains(name)){s.draws[name]=a->mDrawCount;return false;}
    if(a->mDrawCount-s.draws[name]<2)return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Shooter40 actual native renderer capture failed"))return false;
    s.frames.insert(name);NetTrace("SMOKE_SHOOTER40_RENDER role=%s group=%d epoch=%u frame=%s freshDraws=%d",Role(a),s.group,s.epoch,name.c_str(),a->mDrawCount-s.draws[name]);return true;
}
Plant* Actor(Board* b,int index)
{
    Plant* p=nullptr;while(b->mPlants.IterateNext(p))
    {
        const auto* d=GetNativePlantData(p);
        if(!p->mDead&&p->mIsOnBoard&&d&&d->kind==kinds[s.group][index]&&p->mPlantCol==Column(index)&&p->mRow==Row(index))return p;
    }
    return nullptr;
}
Zombie* Target(Board* b,uint16_t id)
{Zombie* z=nullptr;while(b->mZombies.IterateNext(z))if(!z->mDead&&z->mNetId==id)return z;return nullptr;}
bool SpawnTarget(LawnApp* a,int row,float center,int health)
{
    auto* b=a->mBoard;auto* z=b->AddZombieInRow(ZOMBIE_NORMAL,row,0);
    if(!Require(a,z&&z->mNetId,"Shooter40 native normal target factory failed"))return false;
    z->mBodyHealth=z->mBodyMaxHealth=health;z->mHelmHealth=z->mHelmMaxHealth=z->mShieldHealth=z->mShieldMaxHealth=0;
    z->mPosY=z->GetPosYBasedOnRow(row);z->mY=int(z->mPosY);const auto r=z->GetZombieRect();
    z->mPosX+=center-(r.mX+r.mWidth*.5f);z->mX=int(z->mPosX);z->mVelX=0;z->ApplyAnimRate(0);
    s.targets.push_back(z->mNetId);
    NetTrace("SMOKE_SHOOTER40_TARGET_SETUP group=%d epoch=%u index=%zu id=%u row=%d hp=%d x=%.6g y=%.6g initialOnly=1 nativeNormalControl=1",s.group,s.epoch,s.targets.size()-1,z->mNetId,row,health,z->mPosX,z->mPosY);
    return true;
}
bool Setup(LawnApp* a)
{
    auto* b=a->mBoard;
    if(a->NetIsHost()&&!s.setup)
    {
        if(!Require(a,!b->mPaused&&!b->mPlants.mSize&&!b->mZombies.mSize,"Shooter40 setup requires fresh unpaused native world"))return false;
        if (s.group==0)
        {
            constexpr uint16_t batchKinds[]={1036,1037,1038,1040,1023,1042,1043,1044,1045,1046,1171,1054,1020,1152,1107,1109,1161,1165,1167,1168,1096,1183,1191,929,1192,20,22,1091,1052,1053,1055,1059,1061,1062,1063,1064,1173,1080,1179,1182};
            std::string error;
            if(!Require(a,EnsureNativeVisuals(b,batchKinds,{},&error),error.c_str()))return false;
            NetTrace("SMOKE_SHOOTER40_PREFLIGHT models=40 projectileDependencies=all nativeLoad=1");
        }
        b->mSunCountDown=b->mZombieCountDown=b->mZombieCountDownStart=100000;
        for(int i=0;i<Count();++i)
        {
            NativePlantData typed;typed.kind=kinds[s.group][i];typed.owner=uint8_t(i%2);
            for(size_t n=0;n<GetFusionRecipeCount();++n)
            {
                const auto& recipe=GetFusionRecipes()[n];
                if(recipe.result==typed.kind){typed.firstParent=recipe.existing;typed.secondParent=recipe.used;break;}
            }
            std::string error;
            if(!Require(a,EnsureNativePlantVisuals(b,typed.kind,&error),"Shooter40 source visual admission failed"))return false;
            // Explicit initial fixture factory. Paid acquisition, recipes and
            // the three source standalone unlocks are not acceptance claims.
            auto* p=b->AddPlant(Column(i),Row(i),GetNativeSeedForPlant(typed.kind),SEED_NONE,&typed);
            const auto* d=GetNativePlantData(p);const auto* definition=FusionFindPlant(typed.kind);
            if(!Require(a,p&&d&&definition&&p->mNetId&&d->instanceToken&&d->kind==typed.kind&&p->mPlantHealth==definition->maxHealth&&p->mPlantMaxHealth==definition->maxHealth,"Shooter40 actual typed source factory bind failed"))return false;
            NetTrace("SMOKE_SHOOTER40_PLANT_SETUP group=%d epoch=%u kind=%u id=%u token=%u owner=%u parents=%u,%u row=%d column=%d hp=%d launch=%d state=%d shooting=%d factoryOnly=1 acquisitionNotClaimed=1",s.group,s.epoch,d->kind,p->mNetId,d->instanceToken,d->owner,d->firstParent,d->secondParent,p->mRow,p->mPlantCol,p->mPlantHealth,p->mLaunchCounter,p->mStateCountdown,p->mShootingCounter);
        }
        for(int row=0;row<(s.group==2?1:5);++row)
        {
            const float center=float(b->GetCellCenterX(s.group==1?6:2,row)+(s.group==1?0:125));
            if(!SpawnTarget(a,row,center,targetHP))return false;
        }
        if(s.group==0&&!SpawnTarget(a,2,float(b->GetCellCenterX(2,2)-140),targetHP))return false;
        if(s.group==2&&!SpawnTarget(a,4,float(b->GetCellCenterX(3,4)+90),270))return false;
        std::ostringstream ids;ids<<Tag("IDS");for(auto id:s.targets)ids<<' '<<id;s.ids=ids.str();s.setup=true;
        s.pauseRequested=Key(a);
    }
    if(a->NetIsHost()){Send(a,s.ids);}
    else if(!s.setup)
    {
        const auto prefix=Tag("IDS")+" ";
        for(const auto& l:a->mNetManager->mChatLines)if(l.mSlot==0&&l.mText.rfind(prefix,0)==0)
        {std::istringstream ids(l.mText.substr(prefix.size()));unsigned id=0;while(ids>>id){if(!Require(a,id>0&&id<=65535,"Shooter40 malformed target ID barrier"))return false;s.targets.push_back(uint16_t(id));}s.setup=true;break;}
    }
    if(!s.setup||!b->mPaused||s.targets.size()!=size_t(s.group==0?6:s.group==1?5:2))return false;
    for(auto id:s.targets)if(!Target(b,id))return false;
    for(int i=0;i<Count();++i)
    {
        auto* p=Actor(b,i);const auto* d=GetNativePlantData(p);if(!p||!d)return false;
        if(!s.actors[i].id)
        {
            s.actors[i]={p->mNetId,d->instanceToken};
            NetTrace("SMOKE_SHOOTER40_BOUND role=%s group=%d epoch=%u kind=%u id=%u token=%u owner=%u parents=%u,%u row=%d column=%d hp=%d ghost=%d",Role(a),s.group,s.epoch,d->kind,p->mNetId,d->instanceToken,d->owner,d->firstParent,d->secondParent,p->mRow,p->mPlantCol,p->mPlantHealth,p->mNetGhost?1:0);
        }
    }
    return true;
}
bool Observe(LawnApp* a)
{
    auto* b=a->mBoard;
    for(int i=0;i<Count();++i)
    {
        auto* p=Actor(b,i);const auto* d=GetNativePlantData(p);
        if(!Require(a,p&&d&&p->mNetId==s.actors[i].id&&d->instanceToken==s.actors[i].token&&p->mPlantHealth==p->mPlantMaxHealth,"Shooter40 actor identity/health changed outside controlled combat"))return false;
        if(!s.phaseSeen[i]&&d->shootSeconds>=0)
        {s.phaseSeen[i]=true;NetTrace("SMOKE_SHOOTER40_PHASE role=%s group=%d epoch=%u kind=%u id=%u sourceSeconds=%.6g mode=%u",Role(a),s.group,s.epoch,d->kind,p->mNetId,d->shootSeconds,d->batchMode);}
        if(d->kind==1161)
        {
            if(!s.tier101&&d->batchShotCount>=101){s.tier101=true;NetTrace("SMOKE_SHOOTER40_TIER role=%s count=101 actual=%u stateRemaining=%d shootingRemaining=%d",Role(a),d->batchShotCount,p->mStateCountdown,p->mShootingCounter);}
            if(!s.tier301&&d->batchShotCount>=301){s.tier301=true;NetTrace("SMOKE_SHOOTER40_TIER role=%s count=301 actual=%u stateRemaining=%d shootingRemaining=%d",Role(a),d->batchShotCount,p->mStateCountdown,p->mShootingCounter);}
            if(s.tier301)Capture(a,"tier301");
        }
        if((d->kind==1165||d->kind==1168)&&d->batchRageTicks&&!s.rageSeen[i])
        {s.rageSeen[i]=true;NetTrace("SMOKE_SHOOTER40_RAGE role=%s kind=%u id=%u ticks=%u invulnerable=%d naturalRNG=1",Role(a),d->kind,p->mNetId,d->batchRageTicks,NativeShooterBatchInvulnerable(p)?1:0);}
        if(d->kind==1109||d->kind==1167)
        {
            if(d->batchShotCount!=s.sequence[i])
            {++s.callbacks[i];s.sequence[i]=d->batchShotCount;NetTrace("SMOKE_SHOOTER40_SNIPER role=%s kind=%u id=%u sequence=%u observedCallbacks=%d",Role(a),d->kind,p->mNetId,d->batchShotCount,s.callbacks[i]);}
        }
        if(d->kind==1191&&d->batchMode)
        {if(!s.skill)NetTrace("SMOKE_SHOOTER40_SKILL role=%s kind=1191 id=%u bag=%u naturalBag=1",Role(a),p->mNetId,d->batchBagCount);s.skill=true;}
        if(d->kind==1192&&(p->mState==STATE_CHOMPER_BITING_GOT_ONE||p->mState==STATE_CHOMPER_DIGESTING||p->mState==STATE_CHOMPER_SWALLOWING))
        {if(!s.chomp)NetTrace("SMOKE_SHOOTER40_SWALLOW role=%s kind=1192 id=%u digestRemaining=%d",Role(a),p->mNetId,p->mLaunchCounter);s.chomp=true;}
    }
    Projectile* p=nullptr;bool anyFlight=false;
    while(b->mProjectiles.IterateNext(p))
    {
        const auto* d=GetNativeProjectileData(p);if(p->mDead||!d)continue;anyFlight=true;
        if(s.projectiles.insert(p->mNetId).second)
        {s.payloads.insert(d->kind);NetTrace("SMOKE_SHOOTER40_PROJECTILE role=%s group=%d epoch=%u id=%u payload=%u way=%u damage=%d fire=%u effect=%u type=%d motion=%d x=%.6g y=%.6g",Role(a),s.group,s.epoch,p->mNetId,d->kind,d->specialType,d->damage,d->fireState,d->effectVariant,int(p->mProjectileType),int(p->mMotionType),p->mPosX,p->mPosY);}
        if(d->kind==102&&d->specialType==18)s.skillBullet=true;
    }
    if(anyFlight)Capture(a,"flight");
    Coin* coin=nullptr;while(b->mCoins.IterateNext(coin))if(!coin->mDead&&coin->mType==COIN_SMALLSUN)s.sun=true;
    if(s.group==2&&b->GetCraterAt(4,4))s.pit=true;
    return true;
}
bool Complete(LawnApp* a,bool log)
{
    bool done=true;for(int i=0;i<Count();++i)done=done&&s.phaseSeen[i];
    if(s.group==0)
    {
        for(uint16_t k:{11,90,15,46,39})done=done&&s.payloads.contains(k);
    }
    else if(s.group==1)
    {
        done=done&&s.tier101&&s.tier301&&s.rageSeen[1]&&s.rageSeen[2]&&s.callbacks[3]>=6&&s.callbacks[4]>=6&&s.sun&&s.payloads.contains(95);
    }
    else done=done&&s.skill&&s.skillBullet&&s.chomp&&s.pit&&!Target(a->mBoard,s.targets[1]);
    for(size_t i=0;i<s.targets.size();++i)
    {
        if(s.group==2&&i==1)continue;
        auto* z=Target(a->mBoard,s.targets[i]);
        if(!Require(a,z&&z->mBodyMaxHealth==targetHP&&z->mHelmHealth==0&&z->mShieldHealth==0,"Shooter40 controlled normal target lost unexpectedly"))return false;
        done=done&&z->mBodyHealth<targetHP;
        if(log)NetTrace("SMOKE_SHOOTER40_TARGET_OUTCOME role=%s group=%d epoch=%u index=%zu id=%u hp=%d initial=%d damage=%d",Role(a),s.group,s.epoch,i,z->mNetId,z->mBodyHealth,targetHP,targetHP-z->mBodyHealth);
    }
    return done;
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionShooter40(LawnApp* a)
{
    const auto* enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_SHOOTER40"),*native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
    if(!a||!enabled||std::strcmp(enabled,"1")||!native||std::strcmp(native,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
    if(a->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();
    if(GetTickCount()-s.since>150000){char why[120];sprintf_s(why,"Shooter40 deadline group=%d stage=%d tier301=%d rage=%d,%d skill=%d chomp=%d",s.group,s.stage,s.tier301?1:0,s.rageSeen[1]?1:0,s.rageSeen[2]?1:0,s.skill?1:0,s.chomp?1:0);a->CoopSmokeFail(why);return true;}
    if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
    if(s.stage==0)
    {
        if(a->NetIsHost()&&!s.configured){a->mNetManager->SetDifficulty(1);s.configured=true;}
        if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
        if(a->NetIsClient()){Advance(a,1);Send(a,"S264_BEGIN");}
        else if(Has(a,1,"S264_BEGIN")){Advance(a,1);std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;
    }
    if(s.stage==10)
    {
        if(!Barrier(a,"DONE"))return true;
        if(s.group==2){NetTrace("SMOKE_SHOOTER40_COMPLETE role=%s activities=3 sourcePlants=13 factoryOnly=1 nativeCombat=1 postSetupActorWrites=0 paidAcquisition=NotExecuted sourceNPCParity=NotExecuted",Role(a));a->CoopSmokeWriteResult(true,nullptr);return true;}
        State next;next.group=s.group+1;next.previousEpoch=s.epoch;next.chat=s.chat;s=std::move(next);Advance(a,1);
        if(a->NetIsHost()){std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;
    }
    if(!a->mBoard||!IsNativeFusionBoard(a->mBoard)){if(s.stage==1&&a->NetIsClient()&&!s.previousEpoch)Send(a,"S264_BEGIN");return true;}
    auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);if(ctx->epoch<=s.previousEpoch)return true;
    if(!s.epoch){s.epoch=ctx->epoch;NetTrace("SMOKE_SHOOTER40_EPOCH role=%s group=%d epoch=%u previous=%u",Role(a),s.group,s.epoch,s.previousEpoch);}
    if(!Require(a,ctx->epoch==s.epoch&&ctx->validation&&!a->mFusionRuntime&&b->GetColumnCount()==10,"Shooter40 actual native validation epoch differs"))return true;
    const auto& bank=ctx->selection.State();auto* chooser=a->mSeedChooserScreen;
    if(!s.diagnostic||GetTickCount()-s.diagnostic>=10000)
    {s.diagnostic=GetTickCount();NetTrace("SMOKE_SHOOTER40_GATE role=%s group=%d stage=%d epoch=%u tick=%d samples=%u paused=%d projectiles=%zu tier301=%d rage=%d,%d callbacks=%d,%d skill=%d chomp=%d pit=%d",Role(a),s.group,s.stage,s.epoch,b->mMainCounter,Samples(a),b->mPaused?1:0,s.projectiles.size(),s.tier301?1:0,s.rageSeen[1]?1:0,s.rageSeen[2]?1:0,s.callbacks[3],s.callbacks[4],s.skill?1:0,s.chomp?1:0,s.pit?1:0);}
    if(s.stage==1)
    {
        if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||chooser->mSeedsInFlight||!b->mCutScene->mSeedChoosing)return true;
        if(!Require(a,bank.activeMask==3&&bank.targetCount==10&&bank.quotas[0]==5&&bank.quotas[1]==5,"Shooter40 shared native chooser quota differs"))return true;
        int own=0;for(const auto& p:bank.packets)own+=p.owner==Slot(a);if(own<s.waiting)return true;
        if(own<5)
        {
            const int wanted=5*Slot(a)+own;int dense=-1;
            for(size_t i=0;i<ctx->eligible.size();++i)if(ctx->eligible[i].kind==wanted&&!ctx->eligible[i].isExtra)dense=int(i);
            if(!Require(a,dense>=0,"Shooter40 baseline chooser card missing"))return true;if(Choice(a,chooser,dense))s.waiting=own+1;return true;
        }
        if(bank.packets.size()!=10)return true;int rank=-1;
        for(int i=0;i<b->mSeedBank->mNumPackets;++i){const int next=NetSeedBankOwnerRank(Slot(a),b->mSeedBank->mSeedPackets[i].mNetOwnerSlot);if(!Require(a,next>=rank,"Shooter40 own-first native bank order differs"))return true;rank=next;}
        if(Capture(a,"chooser")){NetTrace("SMOKE_SHOOTER40_POOL role=%s group=%d epoch=%u target=10 quotas=5,5 ownFirst=1 nativeChooser=1",Role(a),s.group,s.epoch);Advance(a,2);}
    }
    else if(s.stage==2)
    {
        if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}
        auto* button=chooser?chooser->mStartButton:nullptr;if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;
        const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
        if(!button->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"Shooter40 owned Ready hover failed");}
        else if(Click(a,x,y))Advance(a,3);
    }
    else if(s.stage==3)
    {
        if(bank.phase==FusionBankPhase::Intro)
        {if(!s.introFirst)s.introFirst=GetTickCount();s.introLast=GetTickCount();++s.introChecks;return true;}
        if(bank.phase!=FusionBankPhase::Playing||chooser||a->mGameScene!=SCENE_PLAYING)return true;
        if(!Require(a,s.introChecks>=10&&s.introLast-s.introFirst>=1000,"Shooter40 actual native full intro not observed"))return true;
        if(s.playingFrame<0){s.playingFrame=a->mAppCounter;return true;}if(a->mAppCounter<=s.playingFrame)return true;
        if(a->NetIsHost()&&!Setup(a))return true;
        Advance(a,4);
    }
    else if(s.stage==4)
    {
        if(!Setup(a))return true;if(!s.pauseSample){s.pauseSample=Samples(a);return true;}
        if(Samples(a)-s.pauseSample<2||!Capture(a,"before")||!Barrier(a,"BEFORE"))return true;
        s.baselineTick=b->mMainCounter;Advance(a,5);
    }
    else if(s.stage==5)
    {
        if(a->NetIsHost()&&!s.resumed){s.resumed=Key(a);return true;}
        if(!b->mPaused)Advance(a,6);
    }
    else if(s.stage==6)
    {
        if(!Observe(a))return true;
        if(Complete(a,false)&&s.frames.contains("fusion_shooter40_"+std::to_string(s.group)+"_flight")&&Barrier(a,"OUTCOME"))Advance(a,7);
    }
    else if(s.stage==7)
    {
        if(!Observe(a))return true;
        if(a->NetIsHost()&&!s.finalPause){s.finalPause=Key(a);return true;}
        if(b->mPaused){s.pauseSample=0;Advance(a,8);}
    }
    else if(s.stage==8)
    {
        if(!s.pauseSample){s.pauseSample=Samples(a);return true;}if(Samples(a)-s.pauseSample<2)return true;
        if(!Complete(a,false))return true;
        if(!s.finalLogged){if(!Complete(a,true))return true;s.finalLogged=true;}
        if(!Capture(a,"complete")||!Barrier(a,"FINAL"))return true;
        NetTrace("SMOKE_SHOOTER40_CASE role=%s group=%d epoch=%u plants=%d nativeCombat=1 snapshots=%u elapsedTicks=%d tier101=%d tier301=%d naturalRage=%d,%d sniperCallbacks=%d,%d skill=%d skillBullet=%d swallow=%d crater=%d sun5=%d postSetupActorWrites=0 factoryOnly=1",Role(a),s.group,s.epoch,Count(),Samples(a),b->mMainCounter-s.baselineTick,s.tier101?1:0,s.tier301?1:0,s.rageSeen[1]?1:0,s.rageSeen[2]?1:0,s.callbacks[3],s.callbacks[4],s.skill?1:0,s.skillBullet?1:0,s.chomp?1:0,s.pit?1:0,s.sun?1:0);Advance(a,10);
    }
    return true;
}
}
