#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetCommon.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Projectile.h"
#include "../Cutscene.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativeVisual.h"
#include "../Fusion/FusionNativeWater268.h"
#include "../Fusion/FusionNativeChomper268.h"
#include "../Fusion/FusionChomper268Rules.h"
#include "../Fusion/FusionBasePlantRules.h"
#include "../Fusion/FusionNativeCold.h"
#include "../Fusion/FusionRecipe.h"
#include "../../SexyAppFramework/ScrollbarWidget.h"
#include "../../SexyAppFramework/DDInterface.h"
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
#include <utility>
#include <vector>
namespace
{
using namespace LawnFusion;using LawnNet::NetTrace;
struct Spec {uint16_t kind;int col,row;};
const std::vector<Spec> groups[]={
    {{15,1,2},{1049,6,2},{1156,3,3}},
    {{1050,7,2},{1051,1,3},{1066,6,3}},
    {{252,1,2},{1067,6,2},{1068,1,3},{1069,6,3}},
    {{1076,1,2},{1083,3,3},{1085,1,4}},
    {{1013,2,0},{903,2,1},{921,2,2},{33,2,3},{1177,2,4}}};
struct Ref {uint16_t id=0;uint32_t token=0;};
struct State
{
    int group=0,stage=0,waiting=0,entry=-1,start=-1,introChecks=0,mask=0,selectedGroup=-1,completedGroups=0,completedPlants=0;
    uint32_t since=0,chat=0,wheel=0,epoch=0,previousEpoch=0,introFirst=0,introLast=0,samples=0,diagnostic=0,baselineSamples=0,hitDiagnostic=0;
    bool configured=false,setup=false,pauseRequested=false,resumeRequested=false,baseline=false,idsSent=false,idsParsed=false,idsReady=false,selectorRead=false;
    bool grap=false,cold=false,homing=false,bounce=false,leafTurn=false,leafHit=false,sky=false,star42Initial=false,star42Tracking=false;
    std::array<Ref,8> actors{};
    std::array<uint16_t,8> biteTargets{};
    std::array<int,8> phases{},previousHealth{},peakHealth{},previousShots{},grownShots{};
    std::array<bool,8> retired{},overheal{},decay{},damaged{};
    std::array<std::set<uint16_t>,8> captures{};
    std::vector<uint16_t> zombies;
    std::vector<int> initialZombieHealth,initialFlyingHealth;
    std::vector<bool> targetDamaged,targetCaptured,targetRetired;
    std::set<uint16_t> payloads;
    std::set<std::string> frames;std::map<std::string,int> draws;
    std::set<uint16_t> poolBaselineProof,poolRuntimeProof;
}s;
const auto& Specs(){return groups[s.group];}
int Expected(){return (1<<int(Specs().size()))-1;}
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool okay,const char* why){if(!okay)a->CoopSmokeFail(why);return okay;}
void Advance(LawnApp* a,int stage){s.stage=stage;s.since=GetTickCount();s.diagnostic=0;NetTrace("SMOKE_COMBAT268_STAGE role=%s group=%d epoch=%u stage=%d",Role(a),s.group,s.epoch,stage);}
std::string Tag(const char* name){return "C268_"+std::to_string(s.group)+"_"+std::to_string(s.epoch)+"_"+name;}
bool Has(LawnApp* a,int who,const std::string& value){for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==who&&line.mText==value)return true;return false;}
bool Send(LawnApp* a,const std::string& value)
{if(!Require(a,value.size()<=LawnNet::NET_MAX_CHAT_CHARS,"Combat268 chat proof exceeded native limit"))return false;if(Has(a,Slot(a),value))return true;if(s.chat&&GetTickCount()-s.chat<800)return false;s.chat=GetTickCount();a->mNetManager->SendChat(value);return Has(a,Slot(a),value);}
bool Barrier(LawnApp* a,const char* name){const auto value=Tag(name);Send(a,value);return Has(a,0,value)&&Has(a,1,value);}
bool Click(LawnApp* a,float x,float y)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Combat268 owned SDL click failed");
}
bool BoardClick(LawnApp* a,float x,float y)
{return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY);}
bool Escape(LawnApp* a)
{SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Combat268 owned SDL Escape failed");}
bool Choice(LawnApp* a,SeedChooserScreen* c,int index)
{
    int x=0,y=0;c->GetSeedPositionInChooser(index,x,y);const float cy=float(y+35-int(c->mScrollbar->mScrollValue));
    if(cy<158||cy>493)
    {if(s.wheel&&GetTickCount()-s.wheel<250)return false;s.wheel=GetTickCount();SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=float(c->mX+300);m.motion.y=float(c->mY+300);SDL_Event w{};w.type=SDL_EVENT_MOUSE_WHEEL;w.wheel.windowID=m.motion.windowID;w.wheel.y=cy<158?1.f:-1.f;Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&w),"Combat268 chooser wheel failed");return false;}
    return std::abs(c->mScrollAmount)<=.05f&&Click(a,float(c->mX+x+25),float(c->mY)+cy);
}
bool Capture(LawnApp* a,const char* suffix)
{
    const auto name="fusion_combat268_g"+std::to_string(s.group)+"_"+suffix;if(s.frames.contains(name))return true;
    if(!s.draws.contains(name))s.draws[name]=a->mDrawCount;
    const int requested=s.draws[name]+2;if(!a->CoopSmokeEnsureFreshFrameAfterDraw(requested))return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Combat268 fresh owned BMP failed"))return false;
    s.frames.insert(name);NetTrace("SMOKE_COMBAT268_RENDER role=%s group=%d frame=%s requestedDraw=%d capturedDraw=%d",Role(a),s.group,name.c_str(),requested,a->mCoopSmokeFrameDrawCount);return true;
}
Plant* At(Board* b,int col,int row,int kind)
{Plant* p=nullptr;while(b->IteratePlants(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&d&&d->kind==kind&&p->mPlantCol==col&&p->mRow==row)return p;}return nullptr;}
Plant* Actor(Board* b,int i)
{auto* p=At(b,Specs()[i].col,Specs()[i].row,Specs()[i].kind);const auto* d=GetNativePlantData(p);return p&&d&&p->mNetId==s.actors[i].id&&d->instanceToken==s.actors[i].token?p:nullptr;}
Zombie* ZombieById(Board* b,uint16_t id)
{Zombie* z=nullptr;while(b->IterateZombies(z))if(!z->mDead&&z->mNetId==id)return z;return nullptr;}
Plant* MakePlant(LawnApp* a,uint16_t kind,int col,int row)
{
    NativePlantData data;data.kind=kind;data.owner=uint8_t(col%2);
    for(std::size_t i=0;i<GetFusionRecipeCount();++i)if(GetFusionRecipes()[i].result==kind){data.firstParent=GetFusionRecipes()[i].existing;data.secondParent=GetFusionRecipes()[i].used;break;}
    if(!IsNativeBasePlantKind(kind)&&data.firstParent==kFusionNoPlant&&data.secondParent==kFusionNoPlant)data.owner=LawnNet::NET_SHARED_OWNER;
    std::string error;if(!Require(a,EnsureNativePlantVisuals(a->mBoard,kind,&error),error.empty()?"Combat268 actual source model missing":error.c_str()))return nullptr;
    auto* p=a->mBoard->AddPlant(col,row,GetNativeSeedForPlant(kind),SEED_NONE,&data);const auto* bound=GetNativePlantData(p);
    if(!Require(a,p&&bound&&bound->kind==kind&&bound->instanceToken&&p->mNetId&&!p->mDead,"Combat268 initial real typed factory failed"))return nullptr;return p;
}

Zombie* MakeZombie(LawnApp* a,int row,float colliderCenterX,int health=20000000,bool walking=false,ZombieType kind=ZOMBIE_NORMAL)
{
    auto* b=a->mBoard;auto* z=b->AddZombieInRow(kind,row,0);
    if(!Require(a,z&&z->mNetId&&a->ReanimationTryToGet(z->mBodyReanimID),"Combat268 initial native NPC factory failed"))return nullptr;
    z->mBodyHealth=z->mBodyMaxHealth=health;
    if(kind==ZOMBIE_BALLOON)z->mFlyingHealth=z->mFlyingMaxHealth=20000000;
    const auto rect=z->GetZombieRect();z->mPosX+=colliderCenterX-float(rect.mX+rect.mWidth*.5);z->mX=int(z->mPosX);
    z->mPosY=z->GetPosYBasedOnRow(row);z->mY=int(z->mPosY);
    // Warm-start the stock entry endpoint before applying initial control speed.
    // CheckForPool owns terrain/edge/flying admission; no synthetic update ticks.
    z->CheckForPool();z->CompletePoolEntry();
    if(!walking){z->mVelX=0;z->ApplyAnimRate(0);}
    s.zombies.push_back(z->mNetId);
    NetTrace("SMOKE_COMBAT268_INITIAL_NPC group=%d index=%zu id=%u type=%d row=%d x=%.9g body=%d pool=%d altitude=%.9g height=%d walking=%d initialOnly=1 sourceAI=NotClaimed",s.group,s.zombies.size()-1,z->mNetId,int(kind),row,z->mPosX,health,z->mInPool?1:0,z->mAltitude,int(z->mZombieHeight),walking?1:0);
    return z;
}
float ShadowX(Board* b,int index)
{float x=0,y=0;At(b,Specs()[index].col,Specs()[index].row,Specs()[index].kind)->GetShadowCenter(x,y);return x;}
bool Setup(LawnApp* a)
{
    auto* b=a->mBoard;
    if(!Require(a,a->NetIsHost()&&!b->mPaused&&!b->mPlants.mSize&&!b->mZombies.mSize,"Combat268 setup requires initial unpaused empty world")||
       !Require(a,ConfigureNativeFusionBackground(b,s.group==4?BACKGROUND_1_DAY:BACKGROUND_3_POOL),"Combat268 source terrain refused"))return false;
    for(const auto& p:Specs())if(!MakePlant(a,p.kind,p.col,p.row))return false;
    if(s.group==0)
    {for(int i=0;i<3;++i)if(!MakeZombie(a,Specs()[i].row,ShadowX(b,i)+60))return false;}
    else if(s.group==1)
    {
        // Three independent real targets let SquashKelp execute all three natural captures.
        for(int i=0;i<3;++i)if(!MakeZombie(a,2,ShadowX(b,0)+60+i*2))return false;
        for(int i=1;i<3;++i)if(!MakeZombie(a,3,ShadowX(b,i)+220,20000000,true))return false;
        for(int row:{2,3,4})if(!MakeZombie(a,row,float(b->GetCellCenterX(9,row))))return false;
    }
    else if(s.group==2)
    {
        // Native swim depth changes nearest-target distances. Finite initial
        // receivers let real homing retire the nearer pair and acquire the rest.
        for(int i=0;i<4;++i)if(!MakeZombie(a,Specs()[i].row,ShadowX(b,i)+100,2700))return false;
    }
    else if(s.group==3)
    {
        if(!MakeZombie(a,2,float(b->GetCellCenterX(2,2)))||
           !MakeZombie(a,3,float(b->GetCellCenterX(5,3)))||
           !MakeZombie(a,1,float(b->GetCellCenterX(3,1)))||
           !MakeZombie(a,5,float(b->GetCellCenterX(3,5)))||
           !MakeZombie(a,3,float(b->GetCellCenterX(1,3))))return false;
        // Same source1076 is an auxiliary air receiver cohort, not a new accepted plant.
        if(!MakePlant(a,1076,7,3)||!MakeZombie(a,3,float(b->GetCellCenterX(9,3)),20000000,false,ZOMBIE_BALLOON))return false;
        // Only source42 can damage this row4 flying receiver: sky1076 is same-row3,
        // ground41 rejects air. This keeps the new leaf hit independent of37.
        if(!MakeZombie(a,4,float(b->GetCellCenterX(5,4)),20000000,false,ZOMBIE_BALLOON))return false;
    }
    else
    {
        if(!MakeZombie(a,0,ShadowX(b,0)+75,400)||!MakeZombie(a,0,ShadowX(b,0)+100,5000000))return false;
        for(int i=1;i<3;++i)for(int n=0;n<4;++n)if(!MakeZombie(a,i,ShadowX(b,i)+75+n,2000))return false;
        for(int i=0;i<3;++i)if(!MakeZombie(a,i,float(b->GetCellCenterX(i==0?0:8,i))))return false;
        for(int i=3;i<5;++i)if(!MakeZombie(a,i,ShadowX(b,i)+60,2000))return false;
        // Outside the melee/search box, but nearest for the real cross-row
        // leaf homing after the initial close receiver retires.
        if(!MakeZombie(a,4,float(b->GetCellCenterX(4,4))))return false;
    }
    s.setup=true;s.pauseRequested=Escape(a);
    NetTrace("SMOKE_COMBAT268_SETUP group=%d kinds=%zu targets=%zu sourceFactory=1 acquisition=NotExecuted runtimeIncomingCap=NotExecuted sourceAI=NotClaimed postSetupActorWrites=0",s.group,Specs().size(),s.zombies.size());return true;
}
int TargetCount(){return s.group==0?3:s.group==1?8:s.group==2?4:s.group==3?7:16;}
bool TargetManifest(LawnApp* a)
{
    if(s.idsReady)return true;const auto prefix=Tag("IDS")+" ";
    if(a->NetIsHost())
    {
        std::string text=prefix;for(auto id:s.zombies)text+=std::to_string(id)+" ";text.pop_back();
        if(!Require(a,text.size()<=LawnNet::NET_MAX_CHAT_CHARS,"Combat268 target UID manifest too long"))return false;
        if(!s.idsSent){if(!Send(a,text))return false;s.idsSent=true;NetTrace("SMOKE_COMBAT268_TARGETS role=host group=%d manifest=%s",s.group,text.c_str());}
        if(!Has(a,1,Tag("IDS_ACK")))return false;
    }
    else
    {
        if(!s.idsParsed)for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==0&&line.mText.starts_with(prefix))
        {std::istringstream input(line.mText.substr(prefix.size()));unsigned id=0;while(input>>id){if(!Require(a,id&&id<=65535,"Combat268 target UID invalid"))return false;s.zombies.push_back(uint16_t(id));}s.idsParsed=true;break;}
        if(!s.idsParsed||!Send(a,Tag("IDS_ACK")))return false;
    }
    if(!Require(a,s.zombies.size()==std::size_t(TargetCount()),"Combat268 host/client target manifest count differs"))return false;
    s.initialZombieHealth.resize(s.zombies.size());s.initialFlyingHealth.resize(s.zombies.size());s.targetDamaged.resize(s.zombies.size());s.targetCaptured.resize(s.zombies.size());s.targetRetired.resize(s.zombies.size());
    for(size_t i=0;i<s.zombies.size();++i){auto* z=ZombieById(a->mBoard,s.zombies[i]);if(!z)return false;s.initialZombieHealth[i]=z->mBodyHealth;s.initialFlyingHealth[i]=z->mFlyingHealth;}
    s.idsReady=true;return true;
}
bool PoolPresentation(LawnApp* a,bool baseline)
{
    auto* b=a->mBoard;if(!b->StageHasPool())return true;
    auto& proved=baseline?s.poolBaselineProof:s.poolRuntimeProof;
    for(const auto id:s.zombies)
    {
        auto* z=ZombieById(b,id);
        if(!z||z->mZombieType!=ZOMBIE_NORMAL||z->IsDeadOrDying()||z->IsFlying()||z->mIsEating||
            NativeWater268ZombieCaptured(z)||z->IsTangleKelpTarget()||z->mZombieHeight!=HEIGHT_ZOMBIE_NORMAL)continue;
        const bool square=b->IsPoolSquare(b->PixelToGridX(z->mX+75,z->mY),z->mRow)&&
            b->IsPoolSquare(b->PixelToGridX(z->mX+45,z->mY),z->mRow)&&z->mX<680;
        if(!square&&!z->mInPool)continue;
        auto* body=a->ReanimationTryToGet(z->mBodyReanimID);int first=0,count=0,duck=RENDER_GROUP_HIDDEN;
        if(body&&body->TrackExists("anim_swim"))body->GetFramesForLayer("anim_swim",first,count);
        if(body&&body->TrackExists("Zombie_duckytube"))duck=body->GetTrackInstanceByName("Zombie_duckytube")->mRenderGroup;
        const bool okay=z->mInPool&&std::abs(z->mAltitude-float(int(-40*z->mScaleZombie)))<.01f&&
            body&&count>0&&body->mFrameStart==first&&body->mFrameCount==count&&duck==RENDER_GROUP_NORMAL;
        if(!okay||proved.insert(id).second)
            NetTrace("SMOKE_COMBAT268_POOL_PRESENTATION role=%s group=%d phase=%s id=%u pool=%d altitude=%.9g expectedAltitude=%d height=%d swim=%d/%d actual=%d/%d duckGroup=%d okay=%d",Role(a),s.group,baseline?"baseline":"runtime",id,z->mInPool?1:0,z->mAltitude,int(-40*z->mScaleZombie),int(z->mZombieHeight),first,count,body?body->mFrameStart:-1,body?body->mFrameCount:-1,duck,okay?1:0);
        if(!Require(a,okay,"Combat268 native Pool altitude/swim/duck presentation differs"))return false;
    }
    return true;
}
bool Baseline(LawnApp* a)
{
    auto* b=a->mBoard;if(!TargetManifest(a)||GetNativeFusionRowCount(b)!=(s.group==4?5:6))return false;
    for(int i=0;i<int(Specs().size());++i)
    {
        auto* p=At(b,Specs()[i].col,Specs()[i].row,Specs()[i].kind);const auto* d=GetNativePlantData(p);if(!p||!d)return false;
        if(!s.actors[i].id){s.actors[i]={p->mNetId,d->instanceToken};s.previousHealth[i]=s.peakHealth[i]=p->mPlantHealth;NetTrace("SMOKE_COMBAT268_BIRTH role=%s group=%d index=%d kind=%u id=%u token=%u health=%d",Role(a),s.group,i,d->kind,p->mNetId,d->instanceToken,p->mPlantHealth);}
    }
    if(!s.baselineSamples)s.baselineSamples=Samples(a);return PoolPresentation(a,true);
}
bool Observe(LawnApp* a)
{
    auto* b=a->mBoard;const int elapsed=s.start<0?0:b->mMainCounter-s.start;
    if(!Require(a,a->mGameScene!=SCENE_ZOMBIES_WON,"Combat268 controls reached terminal world"))return false;
    if(s.baseline&&!PoolPresentation(a,false))return false;
    for(int i=0;i<int(Specs().size());++i)
    {
        auto* p=Actor(b,i);const auto* d=GetNativePlantData(p);const auto k=Specs()[i].kind;
        if(!p)
        {
            const bool expected=s.group<2&&(s.phases[i]&(1<<1))&&(s.phases[i]&(1<<2))&&(!s.captures[i].empty());
            if(!Require(a,expected,"Combat268 source actor disappeared without observed grab and sink"))return false;
            if(!s.retired[i])NetTrace("SMOKE_COMBAT268_RETIRE role=%s group=%d index=%d kind=%u id=%u token=%u captures=%zu",Role(a),s.group,i,k,s.actors[i].id,s.actors[i].token,s.captures[i].size());
            s.retired[i]=true;continue;
        }
        s.phases[i]|=1<<d->batchMode;
        if(s.group==4&&i<3)
        {
            if(auto* target=b->ZombieTryToGet(p->mTargetZombieID))s.biteTargets[i]=target->mNetId;
            // The generic host-local target field is not sent for Strong. Bind the same
            // first real forward collider while the observed bite animation is pending.
            if(!s.biteTargets[i]&&d->batchMode==1)
            {
                float px=0,py=0;p->GetShadowCenter(px,py);Zombie* candidate=nullptr;Zombie* z=nullptr;
                while(b->IterateZombies(z))if(!z->mDead&&z->mRow==p->mRow&&!z->mMindControlled)
                {const auto r=z->GetZombieRect();if(r.mX<px+2.25*kFusionBasePixelsX&&r.mX+r.mWidth>px+.75*kFusionBasePixelsX&&(!candidate||z->mPosX<candidate->mPosX))candidate=z;}
                if(candidate)s.biteTargets[i]=candidate->mNetId;
            }
            if(p->mPlantHealth>s.previousHealth[i]&&s.biteTargets[i])
            {for(size_t j=0;j<s.zombies.size();++j)if(s.zombies[j]==s.biteTargets[i]){s.targetDamaged[j]=true;NetTrace("SMOKE_COMBAT268_BITE_OUTCOME role=%s plant=%u target=%u recovery=%d",Role(a),p->mNetId,s.zombies[j],p->mPlantHealth-s.previousHealth[i]);}s.biteTargets[i]=0;}
        }
        if(d->waterTargetId){s.captures[i].insert(d->waterTargetId);for(size_t j=0;j<s.zombies.size();++j)if(s.zombies[j]==d->waterTargetId&&d->batchMode!=3)s.targetCaptured[j]=true;}
        if(p->mPlantHealth<s.previousHealth[i])
        {
            s.damaged[i]=true;
            if(s.group==4&&i<3&&s.previousHealth[i]>p->mPlantMaxHealth&&!p->mSquished)s.decay[i]=true;
            NetTrace("SMOKE_COMBAT268_HP role=%s group=%d id=%u kind=%u before=%d after=%d pending=%d",Role(a),s.group,p->mNetId,k,s.previousHealth[i],p->mPlantHealth,d->pendingProductionTicks);
        }
        if(p->mPlantHealth>p->mPlantMaxHealth)s.overheal[i]=true;
        s.peakHealth[i]=(std::max)(s.peakHealth[i],p->mPlantHealth);s.previousHealth[i]=p->mPlantHealth;
        if(s.group==0){}
        else if(s.group==1){if(d->batchShotCount>s.previousShots[i])s.previousShots[i]=d->batchShotCount;}
        else if(s.group==2){if(d->batchShotCount>=2)s.mask|=1<<i;}
        else if(s.group==3)
        {
            if(d->batchBagCount&&d->batchMode!=2&&d->batchShotCount>s.previousShots[i])s.grownShots[i]+=d->batchShotCount-s.previousShots[i];
            s.previousShots[i]=d->batchShotCount;
        }
        else
        {
            if(i<3&&s.overheal[i]&&s.decay[i]&&d->batchShotCount>0&&(s.phases[i]&(1<<1)))s.mask|=1<<i;
            if(i>=3&&(s.phases[i]&15)==15)s.mask|=1<<i;
        }
    }
    for(size_t i=0;i<s.zombies.size();++i)
    {
        auto* z=ZombieById(b,s.zombies[i]);
        if(!z)
        {
            if(!Require(a,s.targetCaptured[i]||s.targetDamaged[i],"Combat268 target vanished without observed capture or actual damage"))return false;
            if(!s.targetRetired[i])NetTrace("SMOKE_COMBAT268_NPC_RETIRED role=%s group=%d index=%zu id=%u priorDamage=%d priorCapture=%d",Role(a),s.group,i,s.zombies[i],s.targetDamaged[i]?1:0,s.targetCaptured[i]?1:0);
            s.targetRetired[i]=true;continue;
        }
        if(z->mBodyHealth<s.initialZombieHealth[i]||z->mFlyingHealth<s.initialFlyingHealth[i])s.targetDamaged[i]=true;
        const auto* action=GetNativeZombieActionData(z);if(action&&action->waterGrapCount>0&&action->waterGrapTicks>0)s.grap=true;
        const auto* cold=GetNativeZombieColdData(z);if(cold&&cold->level>0)s.cold=true;
    }
    Projectile* q=nullptr;
    while(b->IterateProjectiles(q))
    {
        const auto* d=GetNativeProjectileData(q);if(!d||q->mDead)continue;s.payloads.insert(d->kind);
        if(d->specialType==6&&q->mMotionType==MOTION_HOMING&&std::abs(q->mRotation)>.0001f)s.homing=true;
        if(d->kind==32&&d->specialType==255)s.bounce=true;
        if(d->kind==37&&d->specialType==7)s.sky=true;
        if(d->kind==42){s.star42Initial|=d->specialType==2||d->specialType==9;s.star42Tracking|=d->specialType==6&&q->mMotionType==MOTION_HOMING;}
        if(d->kind==94){s.leafTurn|=d->specialType==16&&q->mMotionType==MOTION_HOMING;s.leafHit|=d->specialType==2&&q->mMotionType==MOTION_STAR;}
    }
    if(s.group<2)
    {
        for(int i=0;i<int(Specs().size());++i)
        {
            bool capturedGone=true;for(auto id:s.captures[i])if(ZombieById(b,id))capturedGone=false;
            const bool shots=s.group==0||i==0||s.previousShots[i]>0;
            if(s.retired[i]&&capturedGone&&shots&&(s.group!=1||i!=0||s.captures[i].size()>=3))s.mask|=1<<i;
        }
    }
    if(s.group==3)
    {if(s.grownShots[0]>=4&&(s.phases[0]&(1<<2))&&s.payloads.contains(9)&&s.sky)s.mask|=1;if(s.grownShots[1]>=10&&(s.phases[1]&(1<<2))&&s.payloads.contains(41))s.mask|=2;if(auto* p=Actor(b,2);p&&GetNativePlantData(p)->batchShotCount>=5&&s.star42Initial&&s.star42Tracking&&s.targetDamaged.size()==7&&s.targetDamaged[6])s.mask|=4;}
    if(!b->mPaused&&s.start>=0&&elapsed>=20&&!Capture(a,"action"))return false;
    const int late=s.group==3?6500:s.group==4?500:100;
    if(!b->mPaused&&s.start>=0&&elapsed>=late&&!Capture(a,"late"))return false;
    if(!s.diagnostic||GetTickCount()-s.diagnostic>=10000)
    {
        s.diagnostic=GetTickCount();NetTrace("SMOKE_COMBAT268_OBSERVE role=%s group=%d elapsed=%d mask=%02x expected=%02x grap=%d cold=%d homing=%d bounce=%d sky=%d leaf=%d/%d star42=%d/%d runtimeIncomingCap=NotExecuted",Role(a),s.group,elapsed,s.mask,Expected(),s.grap?1:0,s.cold?1:0,s.homing?1:0,s.bounce?1:0,s.sky?1:0,s.leafTurn?1:0,s.leafHit?1:0,s.star42Initial?1:0,s.star42Tracking?1:0);
        for(int i=0;i<int(Specs().size());++i){auto* p=Actor(b,i);const auto* d=GetNativePlantData(p);NetTrace("SMOKE_COMBAT268_ACTOR role=%s group=%d index=%d kind=%u id=%u token=%u live=%d phases=%x shots=%u captures=%zu hp=%d peak=%d grownShots=%d",Role(a),s.group,i,Specs()[i].kind,s.actors[i].id,s.actors[i].token,p?1:0,s.phases[i],d?d->batchShotCount:0,s.captures[i].size(),p?p->mPlantHealth:-1,s.peakHealth[i],s.grownShots[i]);}
    }
    return true;
}
bool Complete(LawnApp* a)
{
    if(s.mask!=Expected()||Samples(a)-s.baselineSamples<8)return false;
    if(s.group==1&&(!s.grap||!s.bounce||!s.payloads.contains(29)||!s.payloads.contains(32)))return false;
    if(s.group==2&&(!s.homing||!s.cold||!s.payloads.contains(20)||!s.payloads.contains(37)||!s.payloads.contains(34)||!s.payloads.contains(35)))return false;
    if(s.group==4&&(!s.leafTurn||!s.leafHit||!s.payloads.contains(89)||!s.payloads.contains(3)||!s.payloads.contains(6)||(!s.payloads.contains(4)&&!s.payloads.contains(5))))return false;
    if(s.group>=2)
    {
        std::string unhit;
        for(size_t i=0;i<s.zombies.size();++i)if(!s.targetDamaged[i]&&!s.targetCaptured[i])
        {if(!unhit.empty())unhit+=",";unhit+=std::to_string(s.zombies[i]);}
        if(!unhit.empty())
        {
            if(!s.hitDiagnostic||GetTickCount()-s.hitDiagnostic>=5000)
            {s.hitDiagnostic=GetTickCount();NetTrace("SMOKE_COMBAT268_WAIT_HIT role=%s group=%d missingUIDs=%s",Role(a),s.group,unhit.c_str());}
            return false;
        }
    }
    return s.frames.contains("fusion_combat268_g"+std::to_string(s.group)+"_action")&&s.frames.contains("fusion_combat268_g"+std::to_string(s.group)+"_late");
}
bool FinalDigest(LawnApp* a)
{
    LawnNet::NetBuffer digest;digest.WriteU16(uint16_t(Specs().size()));
    for(int i=0;i<int(Specs().size());++i)
    {auto* p=Actor(a->mBoard,i);const auto* d=GetNativePlantData(p);digest.WriteU16(s.actors[i].id);digest.WriteU32(s.actors[i].token);digest.WriteU8(p?1:0);digest.WriteI32(p?p->mPlantHealth:-1);digest.WriteI32(d?d->batchMode:-1);digest.WriteI32(d?d->batchShotCount:-1);}
    digest.WriteU16(uint16_t(s.zombies.size()));
    for(auto id:s.zombies)
    {auto* z=ZombieById(a->mBoard,id);digest.WriteU16(id);digest.WriteU8(z?1:0);digest.WriteI32(z?z->mBodyHealth:-1);digest.WriteI32(z?z->mHelmHealth:-1);digest.WriteI32(z?z->mShieldHealth:-1);digest.WriteI32(z?z->mFlyingHealth:-1);NetTrace("SMOKE_COMBAT268_FINAL_NPC role=%s group=%d id=%u present=%d body=%d helm=%d shield=%d",Role(a),s.group,id,z?1:0,z?z->mBodyHealth:-1,z?z->mHelmHealth:-1,z?z->mShieldHealth:-1);}
    char hash[32];sprintf_s(hash,"HP_%08x",LawnNet::NetFnv1a32(digest.Data(),digest.Size()));return Barrier(a,hash);
}
}
namespace LawnNet
{
bool NetSmokeUpdateFusionCombat268(LawnApp* a)
{
    const char* enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_COMBAT268"),*native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
    if(!a||!enabled||std::strcmp(enabled,"1")||!native||std::strcmp(native,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
    if(a->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();
    if(GetTickCount()-s.since>200000){char why[160];sprintf_s(why,"Combat268 bounded deadline group=%d stage=%d mask=%02x expected=%02x ui=%d",s.group,s.stage,s.mask,Expected(),0);a->CoopSmokeFail(why);return true;}
    if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
    if(!s.selectorRead)
    {
        s.selectorRead=true;const char* selected=std::getenv("PVZ_COOP_SMOKE_FUSION_COMBAT268_GROUP");
        if(selected&&*selected)
        {
            if(!Require(a,selected[0]>='0'&&selected[0]<='4'&&!selected[1],"Combat268 group selector must be one digit0..4"))return true;
            s.selectedGroup=s.group=selected[0]-'0';
        }
        NetTrace("SMOKE_COMBAT268_SCOPE role=%s selectedGroup=%d groupsExpected=%d sourcePlantsExpected=%zu",Role(a),s.selectedGroup,s.selectedGroup<0?5:1,s.selectedGroup<0?std::size_t(18):Specs().size());
    }
    if(s.stage==0)
    {
        if(a->NetIsHost()&&!s.configured){a->mNetManager->SetDifficulty(1);s.configured=true;}
        if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
        if(a->NetIsClient()){Advance(a,1);Send(a,"C268_BEGIN");}
        else if(Has(a,1,"C268_BEGIN")){Advance(a,1);std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;
    }
    if(s.stage==10)
    {
        if(!Barrier(a,"NEXT"))return true;
        ++s.completedGroups;s.completedPlants+=int(Specs().size());
        if(s.selectedGroup>=0||s.group==4){NetTrace("SMOKE_COMBAT268_COMPLETE role=%s groups=%d sourcePlants=%d freshFrames=%d selectedGroup=%d initialFactory=1 acquisition=NotExecuted sourceAI=NotClaimed runtimeIncomingCap=NotExecuted postSetupActorWrites=0",Role(a),s.completedGroups,s.completedPlants,s.completedGroups*4,s.selectedGroup);a->CoopSmokeWriteResult(true,nullptr);return true;}
        State next;next.group=s.group+1;next.previousEpoch=s.epoch;next.chat=s.chat;next.selectorRead=true;next.selectedGroup=s.selectedGroup;next.completedGroups=s.completedGroups;next.completedPlants=s.completedPlants;s=std::move(next);Advance(a,1);
        if(a->NetIsHost()){std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;
    }
    if(!a->mBoard||!IsNativeFusionBoard(a->mBoard))return true;auto* b=a->mBoard;const auto* context=GetNativeContext(b);
    if(context->epoch<=s.previousEpoch)return true;if(!s.epoch)s.epoch=context->epoch;
    if(!Require(a,context->epoch==s.epoch&&context->validation&&context->visuals&&!a->mFusionRuntime&&b->GetColumnCount()==10,"Combat268 requires real matching native validation world"))return true;
    const auto& bank=context->selection.State();auto* chooser=a->mSeedChooserScreen;
    if(s.stage==1)
    {
        if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||chooser->mSeedsInFlight||!b->mCutScene->mSeedChoosing)return true;
        int own=0;for(const auto& packet:bank.packets)if(packet.owner==Slot(a))++own;if(own<s.waiting)return true;
        if(own<5){const int wanted=5*Slot(a)+own;int dense=-1;for(std::size_t i=0;i<context->eligible.size();++i)if(context->eligible[i].kind==wanted&&!context->eligible[i].isExtra)dense=int(i);if(!Require(a,dense>=0,"Combat268 native chooser card missing"))return true;if(Choice(a,chooser,dense))s.waiting=own+1;return true;}
        if(bank.packets.size()==10)Advance(a,2);
    }
    else if(s.stage==2)
    {
        if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}auto* button=chooser?chooser->mStartButton:nullptr;
        if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;
        const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
        if(!button->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"Combat268 Ready hover failed");}
        else if(Click(a,x,y))Advance(a,3);
    }
    else if(s.stage==3)
    {
        if(bank.phase==FusionBankPhase::Intro){if(!s.introFirst)s.introFirst=GetTickCount();s.introLast=GetTickCount();++s.introChecks;return true;}
        if(bank.phase!=FusionBankPhase::Playing||chooser||a->mGameScene!=SCENE_PLAYING)return true;
        if(!Require(a,s.introChecks>=10&&s.introLast-s.introFirst>=1000,"Combat268 full native intro skipped"))return true;
        if(s.entry<0){s.entry=a->mAppCounter;return true;}if(a->mAppCounter<=s.entry)return true;
        if(a->NetIsHost()&&!s.setup&&!Setup(a))return true;if(b->mPaused)Advance(a,4);
    }
    else if(s.stage==4)
    {
        if(!b->mPaused||!Baseline(a))
        {
            if(!s.diagnostic||GetTickCount()-s.diagnostic>5000){s.diagnostic=GetTickCount();NetTrace("SMOKE_COMBAT268_BASELINE_WAIT role=%s group=%d ids=%zu ready=%d paused=%d",Role(a),s.group,s.zombies.size(),s.idsReady?1:0,b->mPaused?1:0);}
            return true;
        }
        if(!Observe(a))return true;if(!s.samples){s.samples=Samples(a);return true;}
        if(Samples(a)-s.samples<2||!Capture(a,"before")||!Barrier(a,"BASELINE"))return true;
        if(s.start<0)
        {s.start=b->mMainCounter;NetTrace("SMOKE_COMBAT268_BASELINE_CLOCK role=%s group=%d localStart=%d paused=1 snapshots=%u",Role(a),s.group,s.start,Samples(a));}
        s.baseline=true;
        if(a->NetIsHost()&&!s.resumeRequested)s.resumeRequested=Escape(a);
        if(s.resumeRequested||a->NetIsClient())Advance(a,5);
    }
    else if(s.stage==5)
    {
        if(b->mPaused)return true;if(!Observe(a))return true;if(a->mCoopSmokeResultWritten)return true;
        const int minimum=s.group==3?12300:s.group==4?1500:300;
        if(b->mMainCounter-s.start<minimum||!Complete(a)||!Barrier(a,"EFFECTS"))return true;
        s.pauseRequested=false;Advance(a,6);
    }
    else if(s.stage==6)
    {
        if(a->NetIsHost()&&!s.pauseRequested)s.pauseRequested=Escape(a);
        if(b->mPaused){s.samples=Samples(a);Advance(a,7);}
    }
    else if(s.stage==7)
    {
        if(!b->mPaused||Samples(a)-s.samples<2||!Complete(a))return true;
        if(!Observe(a)||!FinalDigest(a)||!Capture(a,"complete")||!Barrier(a,"FINAL"))return true;
        NetTrace("SMOKE_COMBAT268_CASE role=%s group=%d epoch=%u kinds=%zu mask=%02x finalSameHP=1 snapshots=%u acquisition=NotExecuted sourceAI=NotClaimed postSetupActorWrites=0",Role(a),s.group,s.epoch,Specs().size(),s.mask,Samples(a));Advance(a,10);
    }
    return true;
}
}
