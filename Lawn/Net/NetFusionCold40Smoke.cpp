#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../SeedPacket.h"
#include "../Cutscene.h"
#include "../Widget/GameButton.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionRecipe.h"
#include "../Fusion/FusionNativeBasePlants.h"
#include "../Fusion/FusionBasePlantRules.h"
#include "../Fusion/FusionBatchActionRules.h"
#include "../Fusion/FusionColdRules.h"
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
#include <string>
#include <utility>

namespace
{
using namespace LawnFusion;
using LawnNet::NetTrace;
constexpr uint16_t kinds[3][4]={{1036,1037,1038,1042},{1040,1043,1044,1046},{1023,1045,1171,1054}};
constexpr int columns[3][4]={{1,1,1,1},{0,7,0,7},{1,4,1,6}};
constexpr int rows[3][4]={{0,1,3,4},{0,1,4,3},{0,1,3,4}};
struct Ref {uint16_t id=0;uint32_t token=0;};
struct State
{
    int group=0,stage=0,waiting=0,entry=-1,introChecks=0,birthTick=0,pauseTick=0;
    uint32_t epoch=0,oldEpoch=0,since=0,chat=0,wheel=0,introFirst=0,introLast=0,pauseSamples=0,diagnostic=0;
    bool configured=false,setup=false,pauseQueued=false,resumeQueued=false,manualQueued=false,discovered=false;
    std::array<Ref,4> plants{};std::array<uint16_t,4> targets{};
    std::array<bool,4> observed{},retired{};std::array<int,4> maxFreeze{};
    bool shot21=false,shot22=false,shot24=false,shot92=false,doomTag=false,doomControlled=false,doomDeath=false,doomBlast=false;
    uint16_t opponent=0;int opponentPrevious=20000;
    Ref potatoAux{};uint16_t potatoWalker=0;
    bool potatoBefore=false,potatoAfter=false,potatoPlain=false;
    std::set<uint16_t> shots,hypnoTargets;std::set<std::string> frames;std::map<std::string,int> draws;
} s;
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool ok,const char* why){if(!ok)a->CoopSmokeFail(why);return ok;}
void Advance(LawnApp* a,int stage){s.stage=stage;s.since=GetTickCount();s.diagnostic=0;NetTrace("SMOKE_COLD40_STAGE role=%s group=%d epoch=%u stage=%d",Role(a),s.group,s.epoch,stage);}
std::string Tag(const char* value){return "C40_"+std::string(value)+"_"+std::to_string(s.group)+"_"+std::to_string(s.epoch);}
bool Has(LawnApp* a,int slot,const std::string& text){for(const auto& l:a->mNetManager->mChatLines)if(l.mSlot==slot&&l.mText==text)return true;return false;}
bool Send(LawnApp* a,const std::string& text){if(Has(a,Slot(a),text))return true;if(s.chat&&GetTickCount()-s.chat<800)return false;s.chat=GetTickCount();a->mNetManager->SendChat(text);return Has(a,Slot(a),text);}
bool Barrier(LawnApp* a,const char* value){const auto text=Tag(value);Send(a,text);return Has(a,0,text)&&Has(a,1,text);}
bool Escape(LawnApp* a){SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Cold40 owned SDL Escape failed");}
bool Click(LawnApp* a,float x,float y)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Cold40 owned SDL click failed");
}
bool Choice(LawnApp* a,SeedChooserScreen* c,int index)
{
    int x=0,y=0;c->GetSeedPositionInChooser(index,x,y);const float cy=float(y+35-int(c->mScrollbar->mScrollValue));
    if(cy<158||cy>493){if(s.wheel&&GetTickCount()-s.wheel<250)return false;s.wheel=GetTickCount();SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=float(c->mX+300);m.motion.y=float(c->mY+300);SDL_Event w{};w.type=SDL_EVENT_MOUSE_WHEEL;w.wheel.windowID=m.motion.windowID;w.wheel.y=cy<158?1.f:-1.f;Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&w),"Cold40 owned chooser wheel failed");return false;}
    return std::abs(c->mScrollAmount)<=.05f&&Click(a,float(c->mX+x+25),float(c->mY)+cy);
}
bool Capture(LawnApp* a,const char* suffix)
{
    const auto name="fusion_cold40_"+std::to_string(s.group)+"_"+suffix;if(s.frames.contains(name))return true;
    if(!s.draws.contains(name)){s.draws[name]=a->mDrawCount;return false;}if(a->mDrawCount-s.draws[name]<2)return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Cold40 actual native BMP failed"))return false;
    s.frames.insert(name);NetTrace("SMOKE_COLD40_RENDER role=%s group=%d frame=%s freshDraws=%d",Role(a),s.group,name.c_str(),a->mDrawCount-s.draws[name]);return true;
}
Plant* Actor(Board* b,int i)
{Plant* p=nullptr;while(b->mPlants.IterateNext(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&p->mNetId==s.plants[i].id&&d&&d->instanceToken==s.plants[i].token)return p;}return nullptr;}
Zombie* Target(Board* b,uint16_t id)
{Zombie* z=nullptr;while(b->mZombies.IterateNext(z))if(z->mNetId==id&&(!z->mNetGhost||z->mVisible))return z;return nullptr;}
Zombie* Spawn(LawnApp* a,int row,float shadowX,int health=20000,int maximum=20000,bool stationary=true)
{
    auto* b=a->mBoard;auto* z=b->AddZombieInRow(ZOMBIE_NORMAL,row,0);auto* body=z?a->ReanimationTryToGet(z->mBodyReanimID):nullptr;
    if(!Require(a,z&&z->mNetId&&body&&body->mAnimRate>0,"Cold40 initial native NORMAL control allocation failed"))return nullptr;
    z->mBodyHealth=health;z->mBodyMaxHealth=maximum;z->mPosY=z->GetPosYBasedOnRow(row);z->mY=int(z->mPosY);
    float x=0,y=0;if(!Require(a,z->GetShadowCenter(x,y),"Cold40 target shadow missing"))return nullptr;
    z->mPosX+=shadowX-x;z->mX=int(z->mPosX);if(stationary){z->mVelX=0;z->ApplyAnimRate(0);}
    NetTrace("SMOKE_COLD40_TARGET role=host group=%d id=%u generation=%u row=%d hp=%d max=%d x=%.9g stationary=%d initialOnly=1",s.group,z->mNetId,b->mZombies.DataArrayGetID(z),row,health,maximum,z->mPosX,stationary?1:0);
    return z;
}
bool InitialContactApproach(LawnApp* a,Plant* p,Zombie* z,uint16_t kind)
{
    float px=0,py=0;if(!Require(a,GetNativeBasePlantGroundAnchor(p,px,py),"Cold40 initial contact shadow missing"))return false;
    std::array<FusionBatchArea,2> areas{};size_t count=0;
    if(const auto* contact=FindFusionColdContactProfile(kind))
    {
        const double w=contact->width*kFusionBasePixelsX,h=contact->height*kFusionBasePixelsY;
        areas[count++]={px+contact->centerXFromShadow*kFusionBasePixelsX-w*.5,
            py-contact->centerYFromShadow*kFusionBasePixelsY-h*.5,w,h};
    }
    else if(kind==1044)
        for(size_t i=0;i<2;++i){if(!Require(a,TryMakeFusionDoomPuffTrigger(i,px,py,areas[count]),"Cold40 DoomPuff source trigger missing"))return false;++count;}
    if(!Require(a,count>0,"Cold40 initial contact source geometry unavailable"))return false;
    double right=areas[0].x+areas[0].width;for(size_t i=1;i<count;++i)right=(std::max)(right,areas[i].x+areas[i].width);
    const auto before=z->GetZombieRect();z->mPosX+=static_cast<float>(right+48-before.mX);z->mX=int(z->mPosX);
    z->mVelX=1.0f;z->UpdateAnimSpeed();const auto after=z->GetZombieRect();bool outside=true;
    for(size_t i=0;i<count;++i)outside&=!FusionBatchAreaTouchesRect(areas[i],after.mX,after.mY,after.mWidth,after.mHeight);
    if(!Require(a,outside&&after.mX-right>=47&&z->mVelX>0,"Cold40 contact NPC starts inside actual source trigger"))return false;
    NetTrace("SMOKE_COLD40_CONTACT_INITIAL role=host group=%d kind=%u plant=%u target=%u outside=1 sourceColliders=%zu triggerRight=%.9g targetLeft=%d gap=%.9g velocity=%.9g initialOnly=1",s.group,kind,p->mNetId,z->mNetId,count,right,after.mX,after.mX-right,z->mVelX);
    return true;
}
bool Setup(LawnApp* a)
{
    auto* b=a->mBoard;if(!Require(a,!b->mPaused&&!b->mPlants.mSize&&!b->mZombies.mSize,"Cold40 factory requires fresh unpaused native activity"))return false;
    b->mSunCountDown=b->mZombieCountDown=b->mZombieCountDownStart=100000;s.birthTick=b->mMainCounter;
    for(int i=0;i<4;++i)
    {
        const uint16_t kind=kinds[s.group][i];NativePlantData data;data.kind=kind;data.owner=kind==1043?1:0;data.packetId=0;
        const auto* recipes=GetFusionRecipes();for(size_t n=0;n<GetFusionRecipeCount();++n)if(recipes[n].result==kind){data.firstParent=recipes[n].existing;data.secondParent=recipes[n].used;break;}
        if(!Require(a,data.firstParent!=kFusionNoPlant&&data.secondParent!=kFusionNoPlant,"Cold40 source directional ancestry missing"))return false;
        auto* p=b->AddPlant(columns[s.group][i],rows[s.group][i],GetNativeSeedForPlant(kind),SEED_NONE,&data);const auto* actual=GetNativePlantData(p);
        if(!Require(a,p&&p->mNetId&&actual&&actual->instanceToken&&actual->kind==kind&&!p->mDead,"Cold40 real typed native plant factory failed"))return false;
        s.plants[i]={p->mNetId,actual->instanceToken};float px=0,py=0;if(!Require(a,GetNativeBasePlantGroundAnchor(p,px,py),"Cold40 plant shadow missing"))return false;
        const bool contact=(s.group==0&&i==0)||(s.group==1&&i==2)||(s.group==2&&i==3);
        const float ahead=contact?15.f:(s.group==0&&i>=2)?250.f:(s.group==2&&i==2)?250.f:80.f;
        const bool biter=s.group==2&&i==1;
        const bool approach=kind==1036||kind==1044;
        auto* z=Spawn(a,p->mRow,px+ahead,s.group==2&&i==0?200:biter?270:20000,biter?270:20000,!(biter||approach));if(!z)return false;s.targets[i]=z->mNetId;
        if(approach&&!InitialContactApproach(a,p,z,kind))return false;
        if(s.group==2&&i==0){s.hypnoTargets.insert(z->mNetId);for(int n=0;n<3;++n){auto* extra=Spawn(a,p->mRow,px+ahead+n*3,200,20000);if(!extra)return false;s.hypnoTargets.insert(extra->mNetId);}}
        if(s.group==2&&i==1)
        {
            bool found=false;
            for(int dx=80;dx>=-80;--dx){z->mPosX=float(p->mX+dx);z->mX=int(z->mPosX);if(z->FindPlantTarget(ATTACKTYPE_CHEW)==p){found=true;break;}}
            if(!Require(a,found,"Cold40 initial DoomHypno real chew geometry failed"))return false;
            float zx=0,zy=0;z->GetShadowCenter(zx,zy);auto* opponent=Spawn(a,p->mRow,zx+100,20000,20000,false);if(!opponent)return false;s.opponent=opponent->mNetId;
        }
        NetTrace("SMOKE_COLD40_FACTORY role=host group=%d epoch=%u kind=%u id=%u token=%u parents=%u,%u hp=%d special=%d launch=%d target=%u initialOnly=1",s.group,s.epoch,kind,p->mNetId,actual->instanceToken,actual->firstParent,actual->secondParent,p->mPlantHealth,p->mDoSpecialCountdown,p->mLaunchCounter,s.targets[i]);
    }
    if(s.group==2)
    {
        // The existing1171 proves600/self75. A second source-identical actor
        // stays untargeted past20s, then meets an initial walking control NPC.
        NativePlantData data;data.kind=1171;data.firstParent=4;data.secondParent=9;data.owner=0;
        auto* p=b->AddPlant(7,2,GetNativeSeedForPlant(1171),SEED_NONE,&data);const auto* d=GetNativePlantData(p);
        if(!Require(a,p&&d&&d->instanceToken&&p->mPlantHealth==300,"Cold40 auxiliary1171 native factory failed"))return false;
        s.potatoAux={p->mNetId,d->instanceToken};
        const float vision=float(b->GetCellCenterX(b->GetColumnCount()-1,2)+1.85*(72.0/1.35));
        auto* walker=Spawn(a,2,vision+500.f,20000,20000,false);if(!walker)return false;
        walker->mVelX=.25f;walker->UpdateAnimSpeed();s.potatoWalker=walker->mNetId;
        NetTrace("SMOKE_COLD40_POTATO_INITIAL role=host epoch=%u id=%u token=%u target=%u shadowX=%.9g vision=%.9g velocity=.25 initialOnly=1",s.epoch,p->mNetId,d->instanceToken,walker->mNetId,vision+500.f,vision);
    }
    s.setup=true;return true;
}
bool Discover(LawnApp* a)
{
    if(s.discovered)return true;auto* b=a->mBoard;
    for(int i=0;i<4;++i)
    {
        Plant* found=nullptr;Plant* p=nullptr;while(b->mPlants.IterateNext(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&d&&d->kind==kinds[s.group][i]&&p->mPlantCol==columns[s.group][i]&&p->mRow==rows[s.group][i])found=p;}
        const auto* d=GetNativePlantData(found);if(!d)return false;s.plants[i]={found->mNetId,d->instanceToken};
        if(!Require(a,d->instanceToken&&d->owner==(d->kind==1043?1:0)&&found->mPlantHealth==found->mPlantMaxHealth,"Cold40 paused plant generation/owner/HP differs"))return false;
        Zombie* z=nullptr;Zombie* target=nullptr;while(b->mZombies.IterateNext(z))if(!z->mDead&&z->mRow==rows[s.group][i]&&(!z->mNetGhost||z->mVisible))
        {
            if(s.group==2&&i==1){if(z->mBodyMaxHealth==270)target=z;else s.opponent=z->mNetId;}
            else{if(!target||z->mNetId<target->mNetId)target=z;if(s.group==2&&i==0)s.hypnoTargets.insert(z->mNetId);}
        }
        if(!target)return false;s.targets[i]=target->mNetId;
    }
    if(s.group==2)
    {
        Plant* p=nullptr;while(b->mPlants.IterateNext(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&d&&d->kind==1171&&p->mPlantCol==7&&p->mRow==2)s.potatoAux={p->mNetId,d->instanceToken};}
        Zombie* z=nullptr;while(b->mZombies.IterateNext(z))if(!z->mDead&&z->mRow==2&&(!z->mNetGhost||z->mVisible))s.potatoWalker=z->mNetId;
        if(!s.potatoAux.id||!s.potatoWalker)return false;
    }
    s.birthTick=b->mMainCounter;s.pauseTick=b->mMainCounter;s.discovered=true;
    NetTrace("SMOKE_COLD40_BASELINE role=%s group=%d epoch=%u mainKinds=4 targets=%zu paused=1 tick=%d sun=%d money=%d",Role(a),s.group,s.epoch,b->mZombies.mSize,b->mMainCounter,b->mSunMoney,GetNativeContext(b)->money);return true;
}
bool Observe(LawnApp* a)
{
    auto* b=a->mBoard;bool flight=false;
    for(int i=0;i<4;++i)
    {
        auto* p=Actor(b,i);auto* z=Target(b,s.targets[i]);if(!p)s.retired[i]=true;
        if(z)s.maxFreeze[i]=(std::max)(s.maxFreeze[i],z->mIceTrapCounter);
        if(s.group==0)
        {
            if(i==0)s.observed[i]|=s.retired[i]&&s.maxFreeze[i]>0;
            if(i==1){const auto* cold=GetNativeZombieColdData(z);s.observed[i]|=z&&z->mBodyHealth<20000&&cold&&cold->level>0;}
            if(i==2)s.observed[i]|=s.shot21&&z&&z->mBodyHealth<20000;
            if(i==3){const auto* d=GetNativePlantData(p);s.observed[i]|=s.shot22&&d&&d->batchShotCount>0;}
        }
        else if(s.group==1)
        {
            if(i==0)s.observed[i]|=s.retired[i]&&b->GetCraterAt(columns[1][i],rows[1][i])&&s.maxFreeze[i]>=450&&z&&z->mBodyHealth<=18200;
            if(i==1){const auto* d=GetNativePlantData(p);s.observed[i]|=s.manualQueued&&p&&d&&z&&z->mBodyHealth<=16400;}
            if(i==2)s.observed[i]|=s.retired[i]&&!b->GetCraterAt(columns[1][i],rows[1][i])&&z&&z->mBodyHealth<=16400;
            if(i==3)s.observed[i]|=s.shot24&&z&&z->mBodyHealth<18200&&s.maxFreeze[i]>0;
        }
        else
        {
            if(i==0){for(auto id:s.hypnoTargets){auto* target=Target(b,id);s.observed[i]|=target&&target->mMindControlled;}}
            if(i==1)
            {
                const auto* action=GetNativeZombieActionData(z);s.doomTag|=action&&action->doomOnDeath;s.doomControlled|=z&&z->mMindControlled;
                s.doomDeath|=!z||z->mDead||z->IsDeadOrDying();auto* opponent=Target(b,s.opponent);
                if(opponent){s.doomBlast|=s.opponentPrevious-opponent->mBodyHealth>=1800;s.opponentPrevious=opponent->mBodyHealth;}
                s.observed[i]|=s.retired[i]&&s.doomTag&&s.doomControlled&&s.doomDeath&&s.doomBlast&&!b->GetCraterAt(columns[2][i],rows[2][i]);
            }
            if(i==2)s.observed[i]|=s.shot92&&p&&p->mPlantHealth<=225;
            if(i==3)s.observed[i]|=s.retired[i]&&z&&z->mBodyHealth<=16400&&!b->GetCraterAt(columns[2][i],rows[2][i]);
        }
    }
    if(s.group==2)
    {
        Plant* p=nullptr;Plant* auxiliary=nullptr;
        while(b->mPlants.IterateNext(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&d&&p->mNetId==s.potatoAux.id&&d->instanceToken==s.potatoAux.token)auxiliary=p;}
        const auto* d=GetNativePlantData(auxiliary);
        if(!Require(a,d&&auxiliary->mPlantHealth==300,"Cold40 auxiliary1171 retired/self-damaged before plain phase"))return false;
        if(!s.potatoBefore&&d->batchMode==0&&Capture(a,"potato_light2"))
        {s.potatoBefore=true;NetTrace("SMOKE_COLD40_POTATO20 role=%s epoch=%u id=%u token=%u phase=0 hp=300 light2Frame=1",Role(a),s.epoch,s.potatoAux.id,s.potatoAux.token);}
        if(!s.potatoAfter&&d->batchMode==1&&Capture(a,"potato_light1"))
        {s.potatoAfter=true;NetTrace("SMOKE_COLD40_POTATO20 role=%s epoch=%u id=%u token=%u phase=1 hp=300 light1Frame=1 hostCounter=%d",Role(a),s.epoch,s.potatoAux.id,s.potatoAux.token,a->NetIsHost()?auxiliary->mDoSpecialCountdown:-1);}
    }
    Projectile* shot=nullptr;while(b->mProjectiles.IterateNext(shot))
    {
        const auto* d=GetNativeProjectileData(shot);if(shot->mDead||!d)continue;
        if(d->kind==21)s.shot21=true;if(d->kind==22)s.shot22=true;if(d->kind==24)s.shot24=true;if(d->kind==92&&d->damage==600)s.shot92=true;
        if(s.group==2&&d->kind==92&&shot->mRow==2)
        {
            if(!Require(a,d->damage==60&&s.potatoAfter,"Cold40 delayed1171 acquired target before source20s readiness"))return false;
            s.potatoPlain=true;
        }
        const bool relevant=(s.group==0&&(d->kind==21||d->kind==22))||(s.group==1&&d->kind==24)||(s.group==2&&d->kind==92);
        if(relevant){flight=true;if(s.shots.insert(shot->mNetId).second)NetTrace("SMOKE_COLD40_PROJECTILE role=%s group=%d epoch=%u id=%u kind=%u damage=%d row=%d age=%d",Role(a),s.group,s.epoch,shot->mNetId,d->kind,d->damage,shot->mRow,shot->mProjectileAge);}
    }
    if(flight)Capture(a,"flight");
    if(s.group==1&&a->NetIsClient()&&!s.manualQueued&&s.retired[0]&&!b->mPaused)
    {auto* p=Actor(b,1);if(p)s.manualQueued=Click(a,float(p->mX+40+b->mX+a->mDDInterface->mWideScreenOffsetX),float(p->mY+40+b->mY+a->mDDInterface->mWideScreenOffsetY));}
    if(s.group==1&&a->NetIsHost())
    {auto* p=Actor(b,1);const auto* d=GetNativePlantData(p);s.manualQueued|=p&&d&&(d->shootSeconds>=0||p->mLaunchCounter>0);}
    if(!s.diagnostic||GetTickCount()-s.diagnostic>5000){s.diagnostic=GetTickCount();NetTrace("SMOKE_COLD40_OBSERVE role=%s group=%d epoch=%u elapsed=%d checks=%d,%d,%d,%d shots=%d,%d,%d,%d freeze=%d,%d,%d,%d tag=%d MC=%d death=%d blast=%d",Role(a),s.group,s.epoch,b->mMainCounter-s.birthTick,s.observed[0],s.observed[1],s.observed[2],s.observed[3],s.shot21,s.shot22,s.shot24,s.shot92,s.maxFreeze[0],s.maxFreeze[1],s.maxFreeze[2],s.maxFreeze[3],s.doomTag,s.doomControlled,s.doomDeath,s.doomBlast);}
    return (s.group!=2||(s.potatoBefore&&s.potatoAfter&&s.potatoPlain))&&std::all_of(s.observed.begin(),s.observed.end(),[](bool value){return value;})&&s.frames.contains("fusion_cold40_"+std::to_string(s.group)+"_flight");
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionCold40(LawnApp* a)
{
    const auto* flag=std::getenv("PVZ_COOP_SMOKE_FUSION_COLD40"),*native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
    if(!a||!flag||std::strcmp(flag,"1")||!native||std::strcmp(native,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
    if(a->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();if(GetTickCount()-s.since>90000U){char why[160];sprintf_s(why,"Cold40 bounded deadline group=%d stage=%d checks=%d%d%d%d",s.group,s.stage,s.observed[0],s.observed[1],s.observed[2],s.observed[3]);a->CoopSmokeFail(why);return true;}
    if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
    if(s.stage==0){if(a->NetIsHost()&&!s.configured){a->mNetManager->SetDifficulty(1);s.configured=true;}if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;if(a->NetIsClient()){Advance(a,1);Send(a,"C40_BEGIN");}else if(Has(a,1,"C40_BEGIN")){Advance(a,1);std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;}
    if(s.stage==9)
    {
        if(!Barrier(a,"DONE"))return true;
        if(s.group==2){NetTrace("SMOKE_COLD40_COMPLETE role=%s activities=3 factoryKinds=12 boundedChecks=1 actualPurchases=0 postSetupActorWrites=0 manualOwner=client sourceAI=NotExecuted sourceCampaign=NotExecuted potatoAfter20s=1 potatoChildToggleFrames=2 potatoAuxActors=1 JalaSquashFreeze=NotApplicable",Role(a));a->CoopSmokeWriteResult(true,nullptr);return true;}
        const auto old=s.epoch;State next;next.group=s.group+1;next.oldEpoch=old;next.chat=s.chat;s=std::move(next);Advance(a,1);if(a->NetIsHost()){std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;
    }
    if(!a->mBoard||!IsNativeFusionBoard(a->mBoard)){if(s.stage==1&&a->NetIsClient()&&!s.oldEpoch)Send(a,"C40_BEGIN");return true;}
    auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);if(ctx->epoch<=s.oldEpoch)return true;if(!s.epoch)s.epoch=ctx->epoch;
    if(!Require(a,ctx->epoch==s.epoch&&ctx->validation&&ctx->visuals&&!a->mFusionRuntime&&b->GetColumnCount()==10&&b->mBackground==BACKGROUND_1_DAY,"Cold40 real native Day5 validation world missing"))return true;
    const auto& bank=ctx->selection.State();auto* chooser=a->mSeedChooserScreen;
    if(s.stage==1)
    {
        if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||chooser->mSeedsInFlight||!b->mCutScene->mSeedChoosing)return true;
        if(!Require(a,bank.activeMask==3&&bank.targetCount==10&&bank.quotas[0]==5&&bank.quotas[1]==5,"Cold40 native fair chooser quotas differ"))return true;
        const std::array<uint16_t,5> choices=Slot(a)?std::array<uint16_t,5>{4,5,6,7,10}:std::array<uint16_t,5>{0,1,2,3,8};
        int own=0;for(const auto& p:bank.packets)if(p.owner==Slot(a)){if(!Require(a,own<5&&p.kind==choices[own]&&!p.isExtra,"Cold40 actual selected source card identity differs"))return true;++own;}
        if(own<s.waiting)return true;if(own<5){int dense=-1;for(size_t i=0;i<ctx->eligible.size();++i)if(ctx->eligible[i].kind==choices[own]&&!ctx->eligible[i].isExtra)dense=int(i);if(!Require(a,dense>=0,"Cold40 native base chooser entry missing"))return true;if(Choice(a,chooser,dense))s.waiting=own+1;return true;}
        if(bank.packets.size()!=10)return true;int rank=-1;for(int i=0;i<b->mSeedBank->mNumPackets;++i){const int next=NetSeedBankOwnerRank(Slot(a),b->mSeedBank->mSeedPackets[i].mNetOwnerSlot);if(!Require(a,next>=rank,"Cold40 own-first native bank order differs"))return true;rank=next;}
        if(Capture(a,"chooser"))Advance(a,2);
    }
    else if(s.stage==2)
    {
        if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}auto* button=chooser?chooser->mStartButton:nullptr;if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;
        const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;if(!button->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"Cold40 native Ready hover failed");}else if(Click(a,x,y))Advance(a,3);
    }
    else if(s.stage==3)
    {
        if(bank.phase==FusionBankPhase::Intro){if(!s.introFirst)s.introFirst=GetTickCount();s.introLast=GetTickCount();++s.introChecks;return true;}
        if(bank.phase!=FusionBankPhase::Playing||a->mGameScene!=SCENE_PLAYING||chooser)return true;
        if(!Require(a,s.introChecks>=10&&s.introLast-s.introFirst>=1000&&b->mSunMoney==500&&ctx->money==3000,"Cold40 full intro/fresh source ledgers differ"))return true;
        if(s.entry<0){s.entry=a->mAppCounter;return true;}if(a->mAppCounter<=s.entry)return true;Advance(a,4);
    }
    else if(s.stage==4)
    {
        if(a->NetIsHost()&&!s.setup){if(!Setup(a))return true;s.pauseQueued=Escape(a);}
        if(!b->mPaused)return true;if(!s.pauseSamples){s.pauseSamples=Samples(a);return true;}if(Samples(a)-s.pauseSamples<2)return true;
        if(Discover(a)&&Capture(a,"before")&&Barrier(a,"BEFORE"))Advance(a,5);
    }
    else if(s.stage==5)
    {
        if(a->NetIsHost()&&!s.resumeQueued){s.resumeQueued=Escape(a);return true;}if(b->mPaused||a->GetDialogCount())return true;
        Observe(a); // Capture natural first-tick contacts while resume ACKs travel.
        if(Barrier(a,"RESUMED")){NetTrace("SMOKE_COLD40_RESUMED role=%s group=%d epoch=%u hostACK=1 clientACK=1",Role(a),s.group,s.epoch);Advance(a,6);}
    }
    else if(s.stage==6)
    {
        if(!Require(a,b->mSunMoney==500&&ctx->money==3000,"Cold40 native ledger changed outside actions"))return true;
        const bool done=Observe(a);if(done&&Barrier(a,"OBSERVED")){s.pauseQueued=false;s.pauseSamples=0;Advance(a,7);}
    }
    else if(s.stage==7)
    {
        if(a->NetIsHost()&&!s.pauseQueued){s.pauseQueued=Escape(a);return true;}if(!b->mPaused)return true;
        if(!s.pauseSamples){s.pauseSamples=Samples(a);return true;}if(Samples(a)-s.pauseSamples<2)return true;
        if(!Require(a,b->mSunMoney==500&&ctx->money==3000,"Cold40 final ledger differs"))return true;
        if(Capture(a,"complete")&&Barrier(a,"FINAL"))
        {
            for(int i=0;i<4;++i)NetTrace("SMOKE_COLD40_KIND role=%s group=%d epoch=%u kind=%u id=%u token=%u observed=%d retired=%d maximumFreeze=%d",Role(a),s.group,s.epoch,kinds[s.group][i],s.plants[i].id,s.plants[i].token,s.observed[i],s.retired[i],s.maxFreeze[i]);
            NetTrace("SMOKE_COLD40_CASE role=%s group=%d epoch=%u kinds=4 boundedFactory=1 pause=1 resumedBoth=1 frames=4 postSetupActorWrites=0",Role(a),s.group,s.epoch);Advance(a,9);
        }
    }
    return true;
}
}
