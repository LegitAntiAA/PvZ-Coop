#include "../../LawnApp.h"
#include "NetManager.h"
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
constexpr std::array<Spec,15> specs={{{20,0,0},{22,4,0},{1091,1,0},{1052,5,1},{1053,2,1},
    {1055,2,2},{1059,7,1},{1061,2,3},{1062,5,2},{1063,7,2},{1064,9,2},
    {1173,9,0},{1080,6,0},{1179,0,4},{1182,3,4}}};
struct Ref {uint16_t id=0;uint32_t token=0;int x=0;};
struct State
{
    int stage=0,waiting=0,entry=-1,start=0,introChecks=0,mask=0,seenProjectiles=0;
    uint32_t since=0,chat=0,wheel=0,epoch=0,introFirst=0,introLast=0,pauseSamples=0,diag=0;
    bool configured=false,setup=false,pauseRequested=false,resumeRequested=false,baseline=false;
    bool lightLease=false,lightSurvived=false,freeze=false,smallShot=false,orange=false,red=false,cold=false,bounce=false,runner=false,healed=false,spawnSquash=false,spawnCaltrop=false,spawnBlover=false;
    std::array<Ref,15> actors{};std::array<uint16_t,5> zombies{};std::array<float,5> zombieX{};
    std::set<std::string> frames;std::map<std::string,int> draws;
} s;
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool ok,const char* why){if(!ok)a->CoopSmokeFail(why);return ok;}
void Advance(LawnApp* a,int n){s.stage=n;s.since=GetTickCount();s.diag=0;NetTrace("SMOKE_FIRE40_STAGE role=%s epoch=%u stage=%d",Role(a),s.epoch,n);}
std::string Tag(const char* name){return "F264_"+std::string(name)+"_"+std::to_string(s.epoch);}
bool Has(LawnApp* a,int who,const std::string& value){for(const auto& l:a->mNetManager->mChatLines)if(l.mSlot==who&&l.mText==value)return true;return false;}
bool Send(LawnApp* a,const std::string& value){if(Has(a,Slot(a),value))return true;if(s.chat&&GetTickCount()-s.chat<800)return false;s.chat=GetTickCount();a->mNetManager->SendChat(value);return Has(a,Slot(a),value);}
bool Barrier(LawnApp* a,const char* name){const auto tag=Tag(name);Send(a,tag);return Has(a,0,tag)&&Has(a,1,tag);}
bool Click(LawnApp* a,float x,float y)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Fire40 scoped SDL click failed");
}
bool Escape(LawnApp* a)
{
    SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;
    SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Fire40 scoped SDL Escape failed");
}
bool Choice(LawnApp* a,SeedChooserScreen* c,int i)
{
    int x=0,y=0;c->GetSeedPositionInChooser(i,x,y);const float cy=float(y+35-int(c->mScrollbar->mScrollValue));
    if(cy<158||cy>493)
    {
        if(s.wheel&&GetTickCount()-s.wheel<250)return false;s.wheel=GetTickCount();
        SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=float(c->mX+300);m.motion.y=float(c->mY+300);
        SDL_Event w{};w.type=SDL_EVENT_MOUSE_WHEEL;w.wheel.windowID=m.motion.windowID;w.wheel.y=cy<158?1.f:-1.f;
        Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&w),"Fire40 scoped SDL chooser wheel failed");return false;
    }
    return std::abs(c->mScrollAmount)<=.05f&&Click(a,float(c->mX+x+25),float(c->mY)+cy);
}
bool Capture(LawnApp* a,const char* suffix)
{
    const std::string name="fusion_fire40_0_"+std::string(suffix);if(s.frames.contains(name))return true;
    if(!s.draws.contains(name)){s.draws[name]=a->mDrawCount;return false;}if(a->mDrawCount-s.draws[name]<2)return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Fire40 actual native BMP capture failed"))return false;
    s.frames.insert(name);NetTrace("SMOKE_FIRE40_RENDER role=%s frame=%s freshDraws=%d",Role(a),name.c_str(),a->mDrawCount-s.draws[name]);return true;
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
    std::string error;if(!Require(a,EnsureNativePlantVisuals(b,uint16_t(kind),&error),error.empty()?"Fire40 source model missing":error.c_str()))return nullptr;
    auto* p=b->AddPlant(col,row,GetNativeSeedForPlant(uint16_t(kind)),SEED_NONE,&d);const auto* bound=GetNativePlantData(p);
    if(!Require(a,p&&bound&&bound->kind==kind&&bound->instanceToken&&p->mNetId&&!p->mDead,"Fire40 initial native typed factory failed"))return nullptr;
    return p;
}
bool MakeShot(LawnApp* a,int kind,int col,int row,Plant* initialContact=nullptr)
{
    auto* b=a->mBoard;
    const auto contact=initialContact?initialContact->GetPlantAttackRect(WEAPON_PRIMARY):Sexy::Rect();
    const float x=initialContact?contact.mX+contact.mWidth*.5f:float(b->GetCellCenterX(col,row));
    const float y=initialContact?contact.mY+contact.mHeight*.5f:float(b->GridToPixelY(col,row)+10);
    NativeProjectileData d;d.kind=uint16_t(kind);d.damage=20;
    if(!Require(a,EnsureNativeProjectileVisuals(b,uint16_t(kind))&&NativeProjectileMetadataValid(d,PROJECTILE_PEA,MOTION_STRAIGHT),"Fire40 initial source projectile unavailable"))return false;
    auto* p=b->AddProjectile(int(x-20),int(y-20),RENDER_LAYER_PROJECTILE,row,PROJECTILE_PEA);
    if(!Require(a,p!=nullptr,"Fire40 initial projectile capacity failed"))return false;
    p->mPosX=x-20;p->mPosY=y-20;p->mX=int(p->mPosX);p->mY=int(p->mPosY);p->mPosZ=0;p->mShadowY=y;
    p->mVelX=3.2f;p->mVelY=p->mVelZ=p->mAccZ=0;p->mMotionType=MOTION_STRAIGHT;
    if(!p->mNetId)p->mNetId=b->NetAllocId();
    if(!Require(a,p->mNetId&&AttachNativeProjectile(b,p,d),"Fire40 initial source projectile bind failed"))return false;
    const auto collider=p->GetProjectileRect();const auto* target=GetNativePlantData(initialContact);
    NetTrace("SMOKE_FIRE40_INITIAL_SHOT id=%u uid=%u kind=%u type=%d motion=%d row=%d center=%.9g,%.9g velocity=%.9g,%.9g collider=%d,%d,%d,%d contactId=%u contactKind=%u contactRect=%d,%d,%d,%d initialOnly=1",
        p->mNetId,b->mProjectiles.DataArrayGetID(p),d.kind,p->mProjectileType,p->mMotionType,row,x,y,p->mVelX,p->mVelY,
        collider.mX,collider.mY,collider.mWidth,collider.mHeight,initialContact?initialContact->mNetId:0,target?target->kind:0,
        contact.mX,contact.mY,contact.mWidth,contact.mHeight);
    return true;
}
bool Setup(LawnApp* a)
{
    auto* b=a->mBoard;if(!Require(a,a->NetIsHost()&&!b->mPaused&&!b->mPlants.mSize&&!b->mZombies.mSize,"Fire40 setup requires empty unpaused native world"))return false;
    for(const auto& spec:specs)if(!MakePlant(a,spec.kind,spec.col,spec.row))return false;
    if(!MakePlant(a,1182,6,4))return false;
    auto* nut=MakePlant(a,1003,8,4);if(!nut)return false;nut->mPlantHealth=3000;
    // Explicit initial-only boundary fixtures. No actor state is overwritten
    // after the first authoritative setup frame or while the scenario runs.
    At(b,7,1,1059)->mTargetX=50;At(b,2,3,1061)->mTargetX=100;
    constexpr std::array<std::array<int,2>,5> cells={{{6,1},{5,2},{7,2},{9,2},{8,4}}};
    for(int i=0;i<5;++i)
    {
        auto* z=b->AddZombieInRow(ZOMBIE_NORMAL,cells[i][1],0);auto* body=z?a->ReanimationTryToGet(z->mBodyReanimID):nullptr;
        if(!Require(a,z&&z->mNetId&&body,"Fire40 initial native control NPC allocation failed"))return false;
        z->mBodyHealth=z->mBodyMaxHealth=32000;z->mHelmHealth=z->mHelmMaxHealth=0;
        z->mPosX=float(b->GetCellCenterX(cells[i][0],cells[i][1])-60);if(i==4)z->mPosX-=65;
        z->mX=int(z->mPosX);z->mPosY=z->GetPosYBasedOnRow(z->mRow);z->mY=int(z->mPosY);z->mVelX=0;z->ApplyAnimRate(0);
        s.zombies[i]=z->mNetId;s.zombieX[i]=z->mPosX;
        NetTrace("SMOKE_FIRE40_INITIAL_NPC index=%d id=%u row=%d x=%.9g body=32000 speed=0 sourceParity=0 initialOnly=1",i,z->mNetId,z->mRow,z->mPosX);
    }
    for(const auto& shot:std::array<std::array<int,3>,3>{{{0,0,1},{0,0,2},{10,0,3}}})if(!MakeShot(a,shot[0],shot[1],shot[2]))return false;
    // Start these two ordinary payloads at the real contact rectangle. The
    // former generic muzzle let a control NPC intercept the SquashTorch shot
    // and placed the cold-chain shot above the native Torchwood trigger.
    // Both actual IceTorches still perform their own0->15->97 conversions.
    if(!MakeShot(a,0,7,1,At(b,7,1,1059))||!MakeShot(a,0,3,4,At(b,3,4,1182)))return false;
    s.setup=true;s.start=b->mMainCounter;
    NetTrace("SMOKE_FIRE40_SETUP kinds=15 actors=17 controlNPCs=5 sourceProjectiles=5 count1059=50 count1061=100 nutHP=3000 nativeFactory=1 acquisition=NotExecuted initialOnly=1");
    return true;
}
bool Baseline(LawnApp* a)
{
    auto* b=a->mBoard;
    for(int i=0;i<15;++i)
    {
        const auto& spec=specs[i];auto* p=At(b,spec.col,spec.row,spec.kind);const auto* d=GetNativePlantData(p);if(!d)return false;
        if(!Require(a,p->mNetId&&d->instanceToken&&p->mPlantMaxHealth==FindFusionFireActionProfile(spec.kind)->maximumHealth&&p->mIsOnBoard,"Fire40 peer initial source identity/HP differs"))return false;
        if(!s.actors[i].id)
        {
            s.actors[i]={p->mNetId,d->instanceToken,p->mX};
            NetTrace("SMOKE_FIRE40_BIRTH role=%s index=%d kind=%u id=%u token=%u col=%d row=%d hp=%d speed=%.9g nativeAlias=%d ghost=%d",Role(a),i,spec.kind,p->mNetId,d->instanceToken,spec.col,spec.row,p->mPlantHealth,d->animationSpeed,p->mSeedType,p->mNetGhost?1:0);
        }
    }
    if(a->NetIsClient())
    {
        constexpr std::array<int,5> rows={1,2,2,2,4};
        for(int i=0;i<5;++i)
        {
            Zombie* z=nullptr;float best=10000;
            while(b->IterateZombies(z))if(!z->mDead&&z->mRow==rows[i])
            {
                const int col=i==0?6:i==1?5:i==2?7:i==3?9:8;
                const float wanted=float(b->GetCellCenterX(col,rows[i])-60-(i==4?65:0));
                const float distance=std::abs(z->mPosX-wanted);if(distance<best){best=distance;s.zombies[i]=z->mNetId;s.zombieX[i]=z->mPosX;}
            }
            if(!s.zombies[i])return false;
        }
    }
    return At(b,6,4,1182)&&At(b,8,4,1003)&&GetNativeFireLightLevel(Actor(b,0))>=1;
}
void Observe(LawnApp* a)
{
    auto* b=a->mBoard;const int elapsed=b->mMainCounter-s.start;const auto* ctx=GetNativeContext(b);
    if(ctx->temporaryLightLevel>0)s.lightLease=true;
    if(s.lightLease&&!Actor(b,12)&&ctx->temporaryLightLevel>0)s.lightSurvived=true;
    Projectile* shot=nullptr;while(b->IterateProjectiles(shot))
    {
        const auto* d=GetNativeProjectileData(shot);if(!d||shot->mDead)continue;
        s.orange|=d->kind==0&&d->fireState==2;s.red|=d->kind==0&&d->fireState==3;
        s.smallShot|=d->kind==0&&d->fireState>0&&d->effectVariant==1;
        s.cold|=d->kind==97&&d->lastTorchId&&d->lastTorchToken;
        s.bounce|=d->kind==28&&d->specialType==1;
        if(d->kind==12)s.seenProjectiles|=1;
    }
    s.spawnSquash|=At(b,8,1,1057)!=nullptr;s.spawnCaltrop|=At(b,3,3,1155)!=nullptr;
    s.spawnBlover|=At(b,1,0,22)!=nullptr;
    auto* runner=Actor(b,13);if(runner&&runner->mX>s.actors[13].x+20)
    {
        PlantsOnLawn cell{};b->GetPlantsOnLawn(0,4,&cell);
        if(cell.mNormalPlant!=runner&&runner->mIsOnBoard&&NativeFireActionUntargetable(runner))s.runner=true;
    }
    if(auto* nut=At(b,8,4,1003))s.healed|=nut->mPlantHealth>3000;
    for(int i=0;i<5;++i)if(auto* z=Target(b,i))if(z->mIceTrapCounter>0)s.freeze=true;
    if(Actor(b,0)&&GetNativeFireLightLevel(Actor(b,0))>=1)s.mask|=1<<0;
    if(!Actor(b,1)&&elapsed>250)s.mask|=1<<1;
    if(s.spawnBlover)s.mask|=1<<2;
    if(s.red)s.mask|=1<<3;
    if(s.orange)s.mask|=1<<4;
    if(s.smallShot)s.mask|=1<<5;
    if(s.spawnSquash)s.mask|=1<<6;
    if(s.spawnCaltrop)s.mask|=1<<7;
    if(auto* z=Target(b,1))if(z->mBodyHealth<32000)s.mask|=1<<8;
    if(auto* z=Target(b,2))if(z->mBodyHealth<32000&&z->mPosX>s.zombieX[2]+1&&s.bounce)s.mask|=1<<9;
    if(s.seenProjectiles)s.mask|=1<<10;
    if(!Actor(b,11)&&s.freeze)s.mask|=1<<11;
    if(!Actor(b,12)&&s.lightSurvived)s.mask|=1<<12;
    if(s.runner&&s.healed&&!Actor(b,13))s.mask|=1<<13;
    if(s.cold)s.mask|=1<<14;
    if(elapsed>60&&elapsed<250)Capture(a,"flight");
    if(s.spawnBlover)Capture(a,"blover_spawn");
    if(!s.diag||GetTickCount()-s.diag>5000)
    {
        s.diag=GetTickCount();NetTrace("SMOKE_FIRE40_OBSERVE role=%s tick=%d elapsed=%d mask=%04x light=%u lease=%d survived=%d freeze=%d small=%d orange=%d red=%d cold97=%d bounce=%d runner=%d heal=%d spawn1057=%d spawn1155=%d spawn22=%d",Role(a),b->mMainCounter,elapsed,s.mask,ctx->temporaryLightLevel,s.lightLease,s.lightSurvived,s.freeze,s.smallShot,s.orange,s.red,s.cold,s.bounce,s.runner,s.healed,s.spawnSquash,s.spawnCaltrop,s.spawnBlover);
    }
}
}
namespace LawnNet
{
bool NetSmokeUpdateFusionFire40(LawnApp* a)
{
    const auto* enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_FIRE40"),*native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
    if(!a||!enabled||std::strcmp(enabled,"1")||!native||std::strcmp(native,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
    if(a->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();
    if(GetTickCount()-s.since>150000){char why[120];sprintf_s(why,"Fire40 deadline stage=%d mask=%04x expected=7fff",s.stage,s.mask);a->CoopSmokeFail(why);return true;}
    if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
    if(s.stage==0)
    {
        if(a->NetIsHost()&&!s.configured){a->mNetManager->SetDifficulty(1);s.configured=true;}
        if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
        if(a->NetIsClient()){Advance(a,1);Send(a,"F264_BEGIN");}
        else if(Has(a,1,"F264_BEGIN")){Advance(a,1);std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;
    }
    if(!a->mBoard||!IsNativeFusionBoard(a->mBoard)){if(s.stage==1&&a->NetIsClient())Send(a,"F264_BEGIN");return true;}
    auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);if(!s.epoch)s.epoch=ctx->epoch;
    if(!Require(a,ctx->epoch==s.epoch&&ctx->validation&&ctx->visuals&&!a->mFusionRuntime&&b->GetColumnCount()==10,"Fire40 requires native validation world"))return true;
    const auto& bank=ctx->selection.State();auto* chooser=a->mSeedChooserScreen;
    if(s.stage==1)
    {
        if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||chooser->mSeedsInFlight||!b->mCutScene->mSeedChoosing)return true;
        int own=0;for(const auto& p:bank.packets)if(p.owner==Slot(a))++own;
        if(own<s.waiting)return true;
        if(own<5)
        {
            const int wanted=5*Slot(a)+own;int dense=-1;for(size_t i=0;i<ctx->eligible.size();++i)if(ctx->eligible[i].kind==wanted&&!ctx->eligible[i].isExtra)dense=int(i);
            if(!Require(a,dense>=0,"Fire40 native chooser source base missing"))return true;if(Choice(a,chooser,dense))s.waiting=own+1;return true;
        }
        if(bank.packets.size()==10&&Capture(a,"chooser"))Advance(a,2);
    }
    else if(s.stage==2)
    {
        if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}
        auto* button=chooser?chooser->mStartButton:nullptr;if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;
        const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
        if(!button->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"Fire40 Ready hover failed");}
        else if(Click(a,x,y))Advance(a,3);
    }
    else if(s.stage==3)
    {
        if(bank.phase==FusionBankPhase::Intro){if(!s.introFirst)s.introFirst=GetTickCount();s.introLast=GetTickCount();++s.introChecks;return true;}
        if(bank.phase!=FusionBankPhase::Playing||chooser||a->mGameScene!=SCENE_PLAYING)return true;
        if(!Require(a,s.introChecks>=10&&s.introLast-s.introFirst>=1000,"Fire40 full native intro was skipped"))return true;
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
        Observe(a);if(b->mMainCounter-s.start<3300)return true;
        if(!Require(a,s.mask==0x7fff&&s.lightSurvived&&ctx->temporaryLightLevel==0,"Fire40 grouped effects/expiry acceptance incomplete"))return true;
        if(!Barrier(a,"EFFECTS"))return true;
        if(a->NetIsHost()&&!s.pauseRequested)s.pauseRequested=Escape(a);
        if(b->mPaused){s.pauseSamples=Samples(a);Advance(a,6);}
    }
    else if(s.stage==6)
    {
        if(!b->mPaused||Samples(a)-s.pauseSamples<2||!Capture(a,"complete")||!Barrier(a,"FINAL"))return true;
        NetTrace("SMOKE_FIRE40_CASE role=%s group=0 epoch=%u kinds=15 mask=%04x postSetupActorWrites=0",Role(a),s.epoch,s.mask);
        NetTrace("SMOKE_FIRE40_COMPLETE role=%s epoch=%u kinds=15 mask=%04x sourceAcquisition=NotExecuted controlNPCParity=NotExecuted initialActors=17 countBoundaryOverride=50,100 natural1091Period=3000 lightLeaseExpiry=3000 postSetupActorWrites=0 snapshots=%u",Role(a),s.epoch,s.mask,Samples(a));
        a->CoopSmokeWriteResult(true,nullptr);
    }
    return true;
}
}
