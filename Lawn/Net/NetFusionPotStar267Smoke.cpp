#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../Widget/GameButton.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionRecipe.h"
#include "../Fusion/FusionNativeDefender.h"
#include "../Fusion/FusionNativeGloveUI.h"
#include "../Fusion/FusionNativeFireActionBatch.h"
#include "../Fusion/FusionNativeSeaLantern267.h"
#include "../Fusion/FusionSeaLantern267Rules.h"
#include "../Fusion/FusionBasePlantRules.h"
#include "../../SexyAppFramework/DDInterface.h"
#include "../../SexyAppFramework/ScrollbarWidget.h"
#include "../../Sexy.TodLib/Reanimator.h"
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
using namespace LawnFusion;using LawnNet::NetTrace;
constexpr uint16_t kinds[]={1114,1125,1133,28,32,1115,28,32,1115};
constexpr int columns[]={1,1,1,1,1,1,5,5,9},rows[]={0,2,4,0,2,4,0,2,4};
struct Ref{uint16_t id=0;uint32_t token=0;};
struct State
{
    int stage=0,waiting=0,introChecks=0,entry=-1,move=0,moveStep=0,group=-1,birthTick=0,growthDue=0;
    int maxPotButter=0,maxControlButter=0;
    uint32_t epoch=0,since=0,chat=0,wheel=0,introFirst=0,introLast=0,pauseSamples=0,clickAt=0,diagnostic=0;
    bool configured=false,setup=false,discovered=false,pauseQueued=false,resumeQueued=false;
    bool kernel=false,butter=false,enhanced=false,normalMelon=false,damage0=false,damage2=false;
    bool terrain=false,sawGrow=false,sawIdle2=false,shovelPicked=false,shovelSent=false;
    std::array<Ref,9> refs{};std::array<uint16_t,4> targets{};
    std::set<std::string> frames;std::map<std::string,int> draws;
} s;
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool ok,const char* why){if(!ok)a->CoopSmokeFail(why);return ok;}
void Advance(LawnApp* a,int stage){s.stage=stage;s.since=GetTickCount();NetTrace("SMOKE_POTSTAR267_STAGE role=%s epoch=%u stage=%d",Role(a),s.epoch,stage);}
std::string Tag(const char* name){return "PS267_"+std::string(name)+"_"+std::to_string(s.epoch);}
bool Has(LawnApp* a,int slot,const std::string& text){for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==slot&&line.mText==text)return true;return false;}
bool Send(LawnApp* a,const std::string& text){if(Has(a,Slot(a),text))return true;if(s.chat&&GetTickCount()-s.chat<800)return false;s.chat=GetTickCount();a->mNetManager->SendChat(text);return Has(a,Slot(a),text);}
bool Barrier(LawnApp* a,const char* name){const auto text=Tag(name);Send(a,text);return Has(a,0,text)&&Has(a,1,text);}
bool Click(LawnApp* a,float x,float y)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;
    return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"PotStar267 owned SDL click failed");
}
bool BoardClick(LawnApp* a,float x,float y){return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY);}
bool Escape(LawnApp* a){SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"PotStar267 owned SDL Escape failed");}
bool Choice(LawnApp* a,SeedChooserScreen* c,int index)
{
    int x=0,y=0;c->GetSeedPositionInChooser(index,x,y);const float cy=float(y+35-int(c->mScrollbar->mScrollValue));
    if(cy<158||cy>493)
    {
        if(s.wheel&&GetTickCount()-s.wheel<250)return false;s.wheel=GetTickCount();SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=float(c->mX+300);m.motion.y=float(c->mY+300);
        SDL_Event w{};w.type=SDL_EVENT_MOUSE_WHEEL;w.wheel.windowID=m.motion.windowID;w.wheel.y=cy<158?1.f:-1.f;
        Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&w),"PotStar267 owned chooser wheel failed");return false;
    }
    return std::abs(c->mScrollAmount)<=.05f&&Click(a,float(c->mX+x+25),float(c->mY)+cy);
}
bool Capture(LawnApp* a,const char* suffix)
{
    const auto name="fusion_potstar267_"+std::to_string(s.group)+"_"+suffix;if(s.frames.contains(name))return true;
    if(!s.draws.contains(name))s.draws[name]=a->mDrawCount;
    const int requestedDraw=s.draws[name]+2;if(!a->CoopSmokeEnsureFreshFrameAfterDraw(requestedDraw))return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"PotStar267 actual native BMP failed"))return false;
    s.frames.insert(name);NetTrace("SMOKE_POTSTAR267_RENDER role=%s frame=%s freshDraws=%d requestedDraw=%d capturedDraw=%d",Role(a),name.c_str(),a->mDrawCount-s.draws[name],requestedDraw,a->mCoopSmokeFrameDrawCount);return true;
}

Plant* Actor(Board* b,int index)
{Plant* p=nullptr;while(b->mPlants.IterateNext(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&p->mIsOnBoard&&p->mNetId==s.refs[index].id&&d&&d->instanceToken==s.refs[index].token)return p;}return nullptr;}
Plant* At(Board* b,int index)
{Plant* p=nullptr;while(b->mPlants.IterateNext(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&p->mIsOnBoard&&p->mPlantCol==columns[index]&&p->mRow==rows[index]&&d&&d->kind==kinds[index])return p;}return nullptr;}
Zombie* Target(Board* b,int index)
{Zombie* z=nullptr;while(b->mZombies.IterateNext(z))if(z->mNetId==s.targets[index]&&!z->mDead)return z;return nullptr;}
bool Setup(LawnApp* a)
{
    auto* b=a->mBoard;if(!Require(a,!b->mPaused&&!b->mPlants.mSize&&!b->mZombies.mSize,"PotStar267 needs fresh unpaused factory"))return false;
    b->mSunCountDown=b->mZombieCountDown=b->mZombieCountDownStart=100000;
    for(int index=0;index<9;++index)
    {
        NativePlantData data;data.kind=kinds[index];data.owner=index<3?0:1;
        for(size_t n=0;n<GetFusionRecipeCount();++n){const auto& r=GetFusionRecipes()[n];if(r.result==data.kind){data.firstParent=r.existing;data.secondParent=r.used;break;}}
        auto* p=b->AddPlant(columns[index],rows[index],GetNativeSeedForPlant(data.kind),SEED_NONE,&data);const auto* actual=GetNativePlantData(p);
        if(!Require(a,p&&actual&&p->mNetId&&actual->instanceToken&&p->mPlantHealth==p->mPlantMaxHealth,"PotStar267 typed factory failed"))return false;
        s.refs[index]={p->mNetId,actual->instanceToken};
        NetTrace("SMOKE_POTSTAR267_FACTORY role=host index=%d kind=%u id=%u token=%u owner=%u parents=%u,%u HP=%d initialOnly=1",index,actual->kind,p->mNetId,actual->instanceToken,actual->owner,actual->firstParent,actual->secondParent,p->mPlantHealth);
    }
    for(int index=0;index<4;++index)
    {
        const int row=index<2?index*2:4;auto* z=b->AddZombieInRow(ZOMBIE_NORMAL,row,0);
        if(!Require(a,z&&z->mNetId&&a->ReanimationTryToGet(z->mBodyReanimID),"PotStar267 native control allocation failed"))return false;
        z->mBodyHealth=z->mBodyMaxHealth=1000000;z->mPosY=z->GetPosYBasedOnRow(row);z->mY=int(z->mPosY);
        if(index<2){float x=0,y=0;z->GetShadowCenter(x,y);z->mPosX+=float(b->GetCellCenterX(9,row))-x;z->mVelX=0;z->ApplyAnimRate(0);}
        else
        {
            auto* umbrella=Actor(b,index==2?5:8);const auto pr=umbrella->GetPlantRect(),zr=z->GetZombieRect();
            z->mPosX+=float(pr.mX+pr.mWidth+70-zr.mX);z->mVelX=.5f;z->UpdateAnimSpeed();
        }
        z->mX=int(z->mPosX);s.targets[index]=z->mNetId;
        NetTrace("SMOKE_POTSTAR267_TARGET role=host index=%d id=%u row=%d HP=1000000 nativeNORMAL=1 sourceNPCParity=NotExecuted initialOnly=1",index,z->mNetId,row);
    }
    float potX=0,potY=0,controlX=0,controlY=0;
    auto* potUmbrella=Actor(b,5);auto* controlUmbrella=Actor(b,8);auto* potTarget=Target(b,2);
    if(!Require(a,potUmbrella&&controlUmbrella&&potTarget&&potUmbrella->GetShadowCenter(potX,potY)&&
        controlUmbrella->GetShadowCenter(controlX,controlY),"PotStar267 initial Umbrella isolation anchors missing"))return false;
    // Source radius2 selection can admit a target whose right edge extends
    // another collider width. Its pot knockback3 must remain left of the
    // control's radius2 area: separation > (2+3+2)*PX + targetWidth.
    const int targetWidth=potTarget->GetZombieRect().mWidth;
    const double required=7*kFusionBasePixelsX+targetWidth,separation=controlX-potX;
    if(!Require(a,separation>required,"PotStar267 source Umbrella knockback can contaminate control"))return false;
    NetTrace("SMOKE_POTSTAR267_ISOLATION role=host group=0 potCol=1 controlCol=9 separation=%.9g requiredExclusive=%.9g targetWidth=%d sourceRadii=2,2 knockback=3 initialOnly=1",separation,required,targetWidth);
    s.setup=true;return true;
}
bool Matrix(LawnApp* a)
{
    auto* b=a->mBoard;
    for(int index=0;index<9;++index)
    {
        auto* p=Actor(b,index);const auto* d=GetNativePlantData(p);
        if(!Require(a,p&&d&&d->kind==kinds[index]&&d->owner==(index<3?0:1),"PotStar267 canonical actor/owner lost"))return false;
    }
    for(int index=0;index<3;++index)
    {
        auto* p=Actor(b,index+3);const bool moved=p->mPlantCol==3;
        if(!Require(a,p->mRow==rows[index]&&(p->mPlantCol==1||moved)&&
            GetNativeDefenderPotKind(p)==(moved?kFusionNoPlant:kinds[index])&&
            GetNativeDefenderPotKind(Actor(b,index+6))==kFusionNoPlant,"PotStar267 live support cell lookup differs"))return false;
    }
    if(!Require(a,GetNativeDefenderCornDenominator(Actor(b,3),4)==(Actor(b,3)->mPlantCol==1?2:4)&&
        GetNativeDefenderCornDenominator(Actor(b,6),4)==4&&
        GetNativeDefenderMelonEnhanced(Actor(b,4))==(Actor(b,4)->mPlantCol==1)&&
        !GetNativeDefenderMelonEnhanced(Actor(b,7)),"PotStar267 Corn/Melon live modifiers differ"))return false;
    return true;
}
bool Discover(LawnApp* a)
{
    if(s.discovered)return true;auto* b=a->mBoard;
    for(int index=0;index<9;++index)
    {
        auto* p=At(b,index);const auto* d=GetNativePlantData(p);if(!p||!d)return false;
        if(!Require(a,p->mNetId&&d->instanceToken&&d->owner==(index<3?0:1)&&p->mPlantHealth==p->mPlantMaxHealth,"PotStar267 paused actor baseline differs"))return false;
        s.refs[index]={p->mNetId,d->instanceToken};
    }
    if(a->NetIsHost())
    {
        std::string proof=Tag("TARGETS");for(auto id:s.targets)proof+=' '+std::to_string(id);if(!Send(a,proof))return false;
    }
    else
    {
        const auto prefix=Tag("TARGETS")+" ";bool found=false;
        for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==0&&line.mText.starts_with(prefix))
        {unsigned ids[4]={};if(!Require(a,sscanf_s(line.mText.c_str()+prefix.size(),"%u %u %u %u",&ids[0],&ids[1],&ids[2],&ids[3])==4,"PotStar267 target proof malformed"))return false;for(int i=0;i<4;++i)s.targets[i]=static_cast<uint16_t>(ids[i]);found=true;break;}
        if(!found)return false;
    }
    for(int index=0;index<4;++index){auto* z=Target(b,index);if(!z)return false;if(!Require(a,z->mBodyHealth==1000000,"PotStar267 target baseline advanced"))return false;}
    s.discovered=Matrix(a);return s.discovered;
}
bool Observe(LawnApp* a)
{
    if(!Matrix(a))return false;auto* b=a->mBoard;
    Projectile* p=nullptr;while(b->mProjectiles.IterateNext(p))
    {
        const auto* d=GetNativeProjectileData(p);if(p->mDead||!d)continue;
        if(p->mRow==0){if(d->kind==54)s.kernel=true;if(d->kind==55)s.butter=true;}
        if(p->mRow==2&&d->kind==59){if(d->melonEnhanced)s.enhanced=true;else s.normalMelon=true;}
    }
    for(int i=0;i<4;++i)
    {
        auto* z=Target(b,i);if(!Require(a,z&&z->mBodyHealth>0,"PotStar267 native control retired unexpectedly"))return false;
        if(i==0&&z->mBodyHealth<1000000)s.damage0=true;if(i==1&&z->mBodyHealth<1000000)s.damage2=true;
        if(i>=2)if(const auto* d=GetNativeZombieButterData(z))
        {if(i==2)s.maxPotButter=(std::max)(s.maxPotButter,d->ticks);else s.maxControlButter=(std::max)(s.maxControlButter,d->ticks);}
    }
    if(!Require(a,s.maxPotButter<=600&&s.maxControlButter<=400,"PotStar267 Umbrella butter exceeded source modifiers"))return false;
    if(!s.diagnostic||GetTickCount()-s.diagnostic>10000)
    {s.diagnostic=GetTickCount();NetTrace("SMOKE_POTSTAR267_OBSERVE role=%s stage=%d kernel=%d butter=%d enhanced=%d normal=%d damage=%d,%d potButterMax=%d controlButterMax=%d move=%d postSetupActorWrites=0",Role(a),s.stage,s.kernel,s.butter,s.enhanced,s.normalMelon,s.damage0,s.damage2,s.maxPotButter,s.maxControlButter,s.move);}
    return true;
}
bool NaturalReady(){return s.kernel&&s.butter&&s.enhanced&&s.normalMelon&&s.damage0&&s.damage2&&s.maxPotButter>400&&s.maxControlButter>0;}
bool Move(LawnApp* a)
{
    if(s.move>=3)return true;auto* b=a->mBoard;auto* context=GetNativeContext(b);auto* p=Actor(b,3+s.move);
    if(!Require(a,p,"PotStar267 glove actor lost"))return false;
    if(p->mPlantCol==3)
    {
        if(context->gloveHolds[1].plantId)return false;
        const auto tag="MOVED"+std::to_string(s.move);if(!Matrix(a)||!Barrier(a,tag.c_str()))return false;
        NetTrace("SMOKE_POTSTAR267_MOVE role=%s kind=%u id=%u token=%u from=1 to=3 row=%d sourcePot=%u owner=1 identityPreserved=1 liveOldSupportRemoved=1 ownedClientSDL=1 postSetupActorWrites=0",Role(a),kinds[3+s.move],p->mNetId,s.refs[3+s.move].token,p->mRow,kinds[s.move]);
        ++s.move;s.moveStep=0;s.clickAt=0;return s.move>=3;
    }
    if(!a->NetIsClient()||b->mPaused||a->GetDialogCount())return false;
    if(context->gloveCooldownTicks&&!context->gloveHolds[1].plantId)return false;
    if(s.clickAt&&GetTickCount()-s.clickAt<300)return false;s.clickAt=GetTickCount();
    if(!s.moveStep)
    {
        if(b->mCursorObject->mCursorType!=CURSOR_TYPE_GLOVE){Sexy::Rect r=b->GetShovelButtonRect();b->GetZenButtonRect(OBJECT_TYPE_GLOVE,r);BoardClick(a,float(r.mX+r.mWidth/2),float(r.mY+r.mHeight/2));return false;}
        if(BoardClick(a,float(b->GetCellCenterX(1,p->mRow)),float(b->GridToPixelY(1,p->mRow)+35)))s.moveStep=1;return false;
    }
    if(context->gloveHolds[1].plantId!=p->mNetId||context->gloveHolds[1].instanceToken!=s.refs[3+s.move].token||GetNativeGloveHeldPlant(b,1)!=p)return false;
    if(s.moveStep==1&&BoardClick(a,float(b->GetCellCenterX(3,p->mRow)),float(b->GridToPixelY(3,p->mRow)+50)))s.moveStep=2;
    return false;
}
constexpr int poolColumns[]={2,2,0,9},poolRows[]={2,1,0,1};
bool PoolSetup(LawnApp* a)
{
    auto* b=a->mBoard;if(!Require(a,!b->mPaused&&!b->mPlants.mSize&&!b->mZombies.mSize&&
        b->StageHas6Rows()&&b->GetGridSquareTypeAt(2,2)==GRIDSQUARE_POOL&&b->GetGridSquareTypeAt(7,3)==GRIDSQUARE_POOL,
        "PotStar267 Pool requires fresh real aquatic terrain"))return false;
    b->mSunCountDown=b->mZombieCountDown=b->mZombieCountDownStart=100000;s.birthTick=b->mMainCounter;
    for(int index=0;index<4;++index)
    {
        NativePlantData data;data.kind=index?3:1078;data.owner=1;
        for(size_t n=0;n<GetFusionRecipeCount();++n){const auto& r=GetFusionRecipes()[n];if(r.result==data.kind){data.firstParent=r.existing;data.secondParent=r.used;break;}}
        auto* p=b->AddPlant(poolColumns[index],poolRows[index],GetNativeSeedForPlant(data.kind),SEED_NONE,&data);const auto* d=GetNativePlantData(p);
        if(!Require(a,p&&d&&p->mNetId&&d->instanceToken&&p->mPlantHealth==p->mPlantMaxHealth,"PotStar267 Pool typed actor failed"))return false;
        s.refs[index]={p->mNetId,d->instanceToken};
        NetTrace("SMOKE_POTSTAR267_FACTORY role=host group=1 kind=%u id=%u token=%u row=%d col=%d owner=1 initialOnly=1",d->kind,p->mNetId,d->instanceToken,p->mRow,p->mPlantCol);
    }
    auto* lamp=Actor(b,0);const auto* d=GetNativePlantData(lamp);
    if(!Require(a,lamp->mStateCountdown==kFusionSeaLantern267GrowthTicks&&d->productionTicks==kFusionSeaLantern267GrowthTicks&&
        !d->batchMode&&NativeSeaLantern267LightRadius(lamp)==1,"PotStar267 Pool initial natural3001 clock missing"))return false;
    s.growthDue=s.birthTick+kFusionSeaLantern267GrowthTicks;s.setup=true;return true;
}
bool PoolLight(LawnApp* a,int nearLight,int farLight,int newLight)
{
    auto* b=a->mBoard;const int expected[]={nearLight,farLight,newLight};
    for(int i=0;i<3;++i)
    {
        auto* p=Actor(b,i+1);const auto* d=GetNativePlantData(p);
        if(!Require(a,p&&d&&d->kind==3&&d->owner==1&&p->mPlantCol==poolColumns[i+1]&&p->mRow==poolRows[i+1]&&
            p->mPlantHealth==p->mPlantMaxHealth&&GetNativeFireLightLevel(p)==expected[i],
            "PotStar267 Pool actual probe light/identity/HP differs"))return false;
    }
    return true;
}
bool PoolDiscover(LawnApp* a)
{
    if(s.discovered)return true;auto* b=a->mBoard;
    if(a->NetIsClient())for(int index=0;index<4;++index)
    {
        Plant* p=nullptr;while(b->mPlants.IterateNext(p))
        {const auto* d=GetNativePlantData(p);if(!p->mDead&&d&&d->kind==(index?3:1078)&&p->mPlantCol==poolColumns[index]&&p->mRow==poolRows[index]){s.refs[index]={p->mNetId,d->instanceToken};break;}}
        if(!s.refs[index].id)return false;
    }
    auto* lamp=Actor(b,0);const auto* d=GetNativePlantData(lamp);if(!lamp||!d)return false;
    if(!Require(a,d->owner==1&&!d->batchMode&&lamp->mPlantHealth==lamp->mPlantMaxHealth&&NativeSeaLantern267LightRadius(lamp)==1,
        "PotStar267 Pool paused lamp baseline differs"))return false;
    int birth=s.birthTick,tick=b->mMainCounter,remaining=lamp->mStateCountdown;
    const auto prefix=Tag("CLOCK")+" ";
    if(a->NetIsHost())
    {std::ostringstream proof;proof<<prefix<<birth<<' '<<tick<<' '<<remaining<<' '<<lamp->mNetId<<' '<<d->instanceToken;if(!Send(a,proof.str()))return false;}
    else
    {
        bool found=false;for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==0&&line.mText.starts_with(prefix))
        {unsigned id=0,token=0;std::istringstream proof(line.mText.substr(prefix.size()));if(!Require(a,bool(proof>>birth>>tick>>remaining>>id>>token)&&id==lamp->mNetId&&token==d->instanceToken,"PotStar267 Pool host clock generation differs"))return false;found=true;break;}if(!found)return false;
    }
    const int age=tick-birth;
    if(!Require(a,age>=0&&age<kFusionSeaLantern267GrowthTicks&&remaining==kFusionSeaLantern267GrowthTicks-age&&d->productionTicks==remaining,
        "PotStar267 Pool exact native birth age differs")||!PoolLight(a,1,0,0))return false;
    s.birthTick=birth;s.growthDue=birth+kFusionSeaLantern267GrowthTicks;
    NetTrace("SMOKE_POTSTAR267_BASELINE role=%s group=1 birth=%d hostTick=%d age=%d remaining=%d due=%d light=1,0,0 hostPausedUIDProof=1",
        Role(a),birth,tick,age,remaining,s.growthDue);s.discovered=true;return true;
}
bool PoolObserve(LawnApp* a)
{
    auto* b=a->mBoard;auto* lamp=Actor(b,0);const auto* d=GetNativePlantData(lamp);
    if(!Require(a,lamp&&d&&d->kind==1078&&d->owner==1&&lamp->mPlantHealth==lamp->mPlantMaxHealth&&
        d->batchMode<=1,"PotStar267 Pool lamp identity/owner/HP lost"))return false;
    if(a->NetIsHost()&&!d->batchMode&&!Require(a,lamp->mStateCountdown==s.growthDue-b->mMainCounter&&
        d->productionTicks==lamp->mStateCountdown,"PotStar267 Pool native growth deadline drifted"))return false;
    const char* pose=nullptr;float seconds=0;
    if(!Require(a,GetNativeSeaLantern267VisualPose(lamp,pose,seconds),"PotStar267 Pool source visual pose missing"))return false;
    if(!d->batchMode)return Require(a,NativeSeaLantern267LightRadius(lamp)==1&&!std::strcmp(pose,"idle"),"PotStar267 Pool pre-grow source pose differs")&&PoolLight(a,1,0,0);
    if(!s.sawGrow)
    {
        if(a->NetIsHost()&&!Require(a,b->mMainCounter==s.growthDue&&lamp->mStateCountdown==0&&d->productionTicks==0,
            "PotStar267 Pool natural grow missed exact3001 deadline"))return false;
        if(!Require(a,!std::strcmp(pose,"grow"),"PotStar267 Pool grow transition was not observed"))return false;
        s.sawGrow=true;NetTrace("SMOKE_POTSTAR267_GROW role=%s group=1 id=%u token=%u due=%d tick=%d natural3001=1 radius=2",Role(a),lamp->mNetId,d->instanceToken,s.growthDue,b->mMainCounter);
    }
    if(!std::strcmp(pose,"grow"))Capture(a,"grown");else if(!std::strcmp(pose,"idle2"))s.sawIdle2=true;
    else return Require(a,false,"PotStar267 Pool grown source pose differs");
    return Require(a,NativeSeaLantern267LightRadius(lamp)==2&&!d->productionTicks,"PotStar267 Pool grown radius/state differs")&&
        (lamp->mPlantCol==2?PoolLight(a,1,1,0):PoolLight(a,0,0,1));
}
bool PoolMove(LawnApp* a)
{
    auto* b=a->mBoard;auto* lamp=Actor(b,0);const auto* d=GetNativePlantData(lamp);const auto* context=GetNativeContext(b);
    if(!lamp||!d)return false;
    if(lamp->mPlantCol==7&&lamp->mRow==3)
    {
        if(context->gloveHolds[1].plantId||!PoolLight(a,0,0,1))return false;
        const int expectedX=b->GetPlantOriginX(7,3),expectedY=b->GridToPixelY(7,3);
        float shadowX=0,shadowY=0;
        if(!Require(a,lamp->mX==expectedX&&lamp->mY==expectedY&&lamp->GetShadowCenter(shadowX,shadowY)&&
            std::isfinite(shadowX)&&std::isfinite(shadowY),"PotStar267 accepted cell move retained a stale visual origin"))return false;
        NetTrace("SMOKE_POTSTAR267_VISUAL_ORIGIN role=%s group=1 id=%u token=%u col=7 row=3 x=%d y=%d expectedX=%d expectedY=%d shadow=%.9g,%.9g canonicalHeld=0 renderOriginCoherent=1",Role(a),lamp->mNetId,d->instanceToken,lamp->mX,lamp->mY,expectedX,expectedY,shadowX,shadowY);
        NetTrace("SMOKE_POTSTAR267_MOVE role=%s group=1 kind=1078 id=%u token=%u from=2,2 to=7,3 radius=2 light=0,0,1 owner=1 sameInstance=1 grownPreserved=1 normalizedLiveCoords=1 ownedClientSDL=1",Role(a),lamp->mNetId,d->instanceToken);return true;
    }
    if(!a->NetIsClient()||b->mPaused||a->GetDialogCount())return false;
    if(s.clickAt&&GetTickCount()-s.clickAt<300)return false;s.clickAt=GetTickCount();
    if(!s.moveStep)
    {
        if(b->mCursorObject->mCursorType!=CURSOR_TYPE_GLOVE){Sexy::Rect r=b->GetShovelButtonRect();b->GetZenButtonRect(OBJECT_TYPE_GLOVE,r);BoardClick(a,float(r.mX+r.mWidth/2),float(r.mY+r.mHeight/2));return false;}
        if(BoardClick(a,float(b->GetCellCenterX(2,2)),float(b->GridToPixelY(2,2)+35)))s.moveStep=1;return false;
    }
    if(context->gloveHolds[1].plantId!=lamp->mNetId||context->gloveHolds[1].instanceToken!=d->instanceToken||GetNativeGloveHeldPlant(b,1)!=lamp)return false;
    if(s.moveStep==1&&BoardClick(a,float(b->GetCellCenterX(7,3)),float(b->GridToPixelY(7,3)+50)))s.moveStep=2;
    return false;
}
bool PoolShovel(LawnApp* a)
{
    auto* b=a->mBoard;if(!Actor(b,0))return PoolLight(a,0,0,0)&&Require(a,!GetNativeContext(b)->gloveHolds[1].plantId,
        "PotStar267 Pool removed lamp retained glove identity");
    if(!a->NetIsClient()||b->mPaused||a->GetDialogCount())return false;
    if(!s.shovelPicked){const auto r=b->GetShovelButtonRect();s.shovelPicked=BoardClick(a,float(r.mX+r.mWidth/2),float(r.mY+r.mHeight/2));return false;}
    if(!s.shovelSent&&b->mCursorObject->mCursorType==CURSOR_TYPE_SHOVEL)s.shovelSent=BoardClick(a,float(b->GetCellCenterX(7,3)),float(b->GridToPixelY(7,3)+50));
    return false;
}
}
namespace LawnNet
{
bool HandleFusionPotStar267Smoke(LawnApp* a)
{
    const auto* flag=std::getenv("PVZ_COOP_SMOKE_FUSION_POTSTAR267"),*native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
    if(!a||!flag||std::strcmp(flag,"1")||!native||std::strcmp(native,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
    if(s.group<0){const auto* group=std::getenv("PVZ_COOP_SMOKE_FUSION_POTSTAR267_GROUP");s.group=group?std::atoi(group):0;
        if(!Require(a,s.group==0||s.group==1,"PotStar267 group must be0/1"))return true;}
    if(a->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();
    if(GetTickCount()-s.since>120000U){char why[120];sprintf_s(why,"PotStar267 deadline stage=%d move=%d butter=%d,%d",s.stage,s.move,s.maxPotButter,s.maxControlButter);a->CoopSmokeFail(why);return true;}
    if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
    if(s.stage==0)
    {
        if(a->NetIsHost()&&!s.configured){a->mNetManager->SetDifficulty(1);s.configured=true;}
        if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
        if(a->NetIsClient()){Advance(a,1);Send(a,"PS267_BEGIN");}else if(Has(a,1,"PS267_BEGIN")){Advance(a,1);std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;
    }
    if(!a->mBoard||!IsNativeFusionBoard(a->mBoard)){if(s.stage==1&&a->NetIsClient())Send(a,"PS267_BEGIN");return true;}
    auto* b=a->mBoard;const auto* context=GetNativeContext(b);if(!s.epoch)s.epoch=context->epoch;
    if(s.group==1&&a->NetIsHost()&&!s.terrain){if(!Require(a,ConfigureNativeFusionBackground(b,BACKGROUND_3_POOL),"PotStar267 Pool terrain setup failed"))return true;s.terrain=true;}
    if(s.group==1&&b->mBackground!=BACKGROUND_3_POOL)return true;
    if(!Require(a,context->epoch==s.epoch&&context->validation&&context->visuals&&!a->mFusionRuntime&&b->GetColumnCount()==10&&
        b->mBackground==(s.group?BACKGROUND_3_POOL:BACKGROUND_1_DAY)&&GetNativeFusionRowCount(b)==(s.group?6:5),"PotStar267 native group geometry differs"))return true;
    const auto& bank=context->selection.State();auto* chooser=a->mSeedChooserScreen;
    if(s.stage==1)
    {
        if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||chooser->mSeedsInFlight||!b->mCutScene->mSeedChoosing)return true;
        if(!Require(a,bank.activeMask==3&&bank.targetCount==10&&bank.quotas[0]==5&&bank.quotas[1]==5,"PotStar267 chooser quotas differ"))return true;
        int own=0;for(const auto& p:bank.packets)own+=p.owner==Slot(a);if(own<s.waiting)return true;
        if(own<5){const int wanted=5*Slot(a)+own;int dense=-1;for(size_t i=0;i<context->eligible.size();++i)if(context->eligible[i].kind==wanted&&!context->eligible[i].isExtra)dense=int(i);if(!Require(a,dense>=0,"PotStar267 native chooser card missing"))return true;if(Choice(a,chooser,dense))s.waiting=own+1;return true;}
        if(bank.packets.size()!=10)return true;int rank=-1;for(int i=0;i<b->mSeedBank->mNumPackets;++i){const int next=NetSeedBankOwnerRank(Slot(a),b->mSeedBank->mSeedPackets[i].mNetOwnerSlot);if(!Require(a,next>=rank,"PotStar267 own-first bank differs"))return true;rank=next;}
        if(Capture(a,"chooser"))Advance(a,2);
    }
    else if(s.stage==2)
    {
        if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}auto* button=chooser?chooser->mStartButton:nullptr;if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;
        const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
        if(!button->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"PotStar267 Ready hover failed");}else if(Click(a,x,y))Advance(a,3);
    }
    else if(s.stage==3)
    {
        if(bank.phase==FusionBankPhase::Intro){if(!s.introFirst)s.introFirst=GetTickCount();s.introLast=GetTickCount();++s.introChecks;return true;}
        if(bank.phase!=FusionBankPhase::Playing||a->mGameScene!=SCENE_PLAYING||chooser)return true;
        if(!Require(a,s.introChecks>=10&&s.introLast-s.introFirst>=1000&&b->mSunMoney==500&&context->money==3000,"PotStar267 full intro/initial ledger differs"))return true;
        if(s.entry<0){s.entry=a->mAppCounter;return true;}if(a->mAppCounter>s.entry)Advance(a,4);
    }
    else if(s.stage==4)
    {
        if(a->NetIsHost()&&!s.setup){if(!(s.group?PoolSetup(a):Setup(a)))return true;s.pauseQueued=Escape(a);}
        if(!b->mPaused)return true;if(!s.pauseSamples){s.pauseSamples=Samples(a);return true;}if(Samples(a)-s.pauseSamples<2)return true;
        if((s.group?PoolDiscover(a):Discover(a))&&Capture(a,"before")&&Barrier(a,"BEFORE")){s.resumeQueued=false;Advance(a,5);}
    }
    else if(s.stage==5)
    {if(a->NetIsHost()&&!s.resumeQueued){s.resumeQueued=Escape(a);return true;}if(b->mPaused||a->GetDialogCount())return true;if((s.group?PoolObserve(a):Observe(a))&&Barrier(a,"RESUMED"))Advance(a,6);}
    else if(s.stage==6)
    {if(!(s.group?PoolObserve(a):Observe(a)))return true;if((s.group?(s.sawGrow&&s.sawIdle2&&s.frames.contains("fusion_potstar267_1_grown")):NaturalReady())&&
        (s.group||Capture(a,"flight"))&&Barrier(a,"NATURAL"))Advance(a,7);}
    else if(s.stage==7)
    {if(!(s.group?PoolObserve(a):Observe(a)))return true;if((s.group?PoolMove(a):Move(a))&&Capture(a,"moved")&&Barrier(a,"MOVED_ALL"))
        {s.pauseQueued=false;s.pauseSamples=0;Advance(a,s.group?10:8);}}
    else if(s.stage==10)
    {if(PoolShovel(a)&&Barrier(a,"REMOVED")){NetTrace("SMOKE_POTSTAR267_REMOVAL role=%s group=1 kind=1078 id=%u token=%u nativeClientShovel=1 light=0,0,0 noStaleOwnerOrGlove=1",Role(a),s.refs[0].id,s.refs[0].token);s.pauseQueued=false;s.pauseSamples=0;Advance(a,8);}}
    else if(s.stage==8)
    {
        if(a->NetIsHost()&&!s.pauseQueued){s.pauseQueued=Escape(a);return true;}if(!b->mPaused)return true;
        if(!s.pauseSamples){s.pauseSamples=Samples(a);return true;}if(Samples(a)-s.pauseSamples<2)return true;
        if(!(s.group?(!Actor(b,0)&&PoolLight(a,0,0,0)):Observe(a))||!Require(a,(s.group?(s.sawGrow&&s.sawIdle2):(NaturalReady()&&s.move==3))&&
            b->mSunMoney==500&&context->money==3000,"PotStar267 final evidence/ledger differs"))return true;
        if(Capture(a,"complete")&&Barrier(a,"FINAL"))
        {
            if(s.group)NetTrace("SMOKE_POTSTAR267_CASE role=%s kind=1078 group=1 natural3001Growth=1 radius1to2=1 nativeClientWaterGlove=1 nativeClientShovel=1 sourceFX40=Pending fullSourceCampaign=NotExecuted",Role(a));
            else for(int i=0;i<3;++i)NetTrace("SMOKE_POTSTAR267_CASE role=%s kind=%u group=0 factory=1 liveModifiers=1 nativeClientGloveMove=1 fullSourceCampaign=NotExecuted",Role(a),kinds[i]);
            NetTrace("SMOKE_POTSTAR267_COMPLETE role=%s group=%d count=%d postSetupActorWrites=0 source1089Storage=PendingDependency",Role(a),s.group,s.group?1:3);
            Advance(a,9);a->CoopSmokeWriteResult(true,nullptr);
        }
    }
    return true;
}
}
