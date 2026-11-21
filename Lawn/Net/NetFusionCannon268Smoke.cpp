#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetCommon.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Projectile.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativeVisual.h"
#include "../Fusion/FusionNativeCannon268.h"
#include "../Fusion/FusionNativeDefender.h"
#include "../Fusion/FusionEconomyRules.h"
#include "../Fusion/FusionRecipe.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../SexyAppFramework/ScrollbarWidget.h"
#include "../../SexyAppFramework/DDInterface.h"
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
    {{1120,1,0},{1122,1,2},{1123,1,4}},
    {{1132,1,1},{915,4,3}},
    {{1139,1,0},{1142,1,2},{914,1,4}}};
struct Ref {uint16_t id=0;uint32_t token=0;};
struct State
{
    int group=0,stage=0,waiting=0,entry=-1,start=-1,introChecks=0,mask=0,ui=0,uiPhase=0;
    int moneyBefore=0,moneyAfter=0,moneyFee=0,moneyTick=0,mcLastRow=-1,mcLastHealth=-1;
    float mcLastX=0,mcLastY=0;
    uint32_t since=0,chat=0,wheel=0,epoch=0,previousEpoch=0,introFirst=0,introLast=0,samples=0,diagnostic=0,uiSince=0;
    bool configured=false,setup=false,pauseRequested=false,resumeRequested=false,baseline=false,idsSent=false,idsParsed=false,idsReady=false;
    bool enhanced=false,cornTrait=false,poison=false,extraShot=false,kernelBeforeAim=false;
    std::array<Ref,8> actors{};
    std::array<bool,8> aimed{},up{},down{},moneyMode{},paid{};
    std::array<bool,2> skillDamage{};
    std::array<int,8> initialHealth{},moneyHealthBefore{},moneyCommitTick{};
    std::vector<uint16_t> zombies;
    std::map<uint16_t,std::set<uint16_t>> payloads;
    std::set<std::string> frames;std::map<std::string,int> draws;
}s;
const auto& Specs(){return groups[s.group];}
int Expected(){return (1<<int(Specs().size()))-1;}
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool okay,const char* why){if(!okay)a->CoopSmokeFail(why);return okay;}
void Advance(LawnApp* a,int stage){s.stage=stage;s.since=GetTickCount();s.diagnostic=0;NetTrace("SMOKE_CANNON268_STAGE role=%s group=%d epoch=%u stage=%d",Role(a),s.group,s.epoch,stage);}
std::string Tag(const char* name){return "C268_"+std::to_string(s.group)+"_"+std::to_string(s.epoch)+"_"+name;}
bool Has(LawnApp* a,int who,const std::string& value){for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==who&&line.mText==value)return true;return false;}
bool Send(LawnApp* a,const std::string& value)
{if(Has(a,Slot(a),value))return true;if(s.chat&&GetTickCount()-s.chat<800)return false;s.chat=GetTickCount();a->mNetManager->SendChat(value);return Has(a,Slot(a),value);}
bool Barrier(LawnApp* a,const char* name){const auto value=Tag(name);Send(a,value);return Has(a,0,value)&&Has(a,1,value);}
bool Click(LawnApp* a,float x,float y)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Cannon268 owned SDL click failed");
}
bool BoardClick(LawnApp* a,float x,float y)
{return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY);}
bool Escape(LawnApp* a)
{SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Cannon268 owned SDL Escape failed");}
bool Choice(LawnApp* a,SeedChooserScreen* c,int index)
{
    int x=0,y=0;c->GetSeedPositionInChooser(index,x,y);const float cy=float(y+35-int(c->mScrollbar->mScrollValue));
    if(cy<158||cy>493)
    {if(s.wheel&&GetTickCount()-s.wheel<250)return false;s.wheel=GetTickCount();SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=float(c->mX+300);m.motion.y=float(c->mY+300);SDL_Event w{};w.type=SDL_EVENT_MOUSE_WHEEL;w.wheel.windowID=m.motion.windowID;w.wheel.y=cy<158?1.f:-1.f;Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&w),"Cannon268 chooser wheel failed");return false;}
    return std::abs(c->mScrollAmount)<=.05f&&Click(a,float(c->mX+x+25),float(c->mY)+cy);
}
bool Capture(LawnApp* a,const char* suffix)
{
    const auto name="fusion_cannon268_"+std::to_string(s.group)+"_"+suffix;if(s.frames.contains(name))return true;
    if(!s.draws.contains(name))s.draws[name]=a->mDrawCount;
    const int requested=s.draws[name]+2;if(!a->CoopSmokeEnsureFreshFrameAfterDraw(requested))return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Cannon268 fresh owned BMP failed"))return false;
    s.frames.insert(name);NetTrace("SMOKE_CANNON268_RENDER role=%s group=%d frame=%s requestedDraw=%d capturedDraw=%d",Role(a),s.group,name.c_str(),requested,a->mCoopSmokeFrameDrawCount);return true;
}
Plant* At(Board* b,int col,int row,int kind)
{Plant* p=nullptr;while(b->IteratePlants(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&d&&d->kind==kind&&p->mPlantCol==col&&p->mRow==row)return p;}return nullptr;}
Plant* Actor(Board* b,int i)
{auto* p=At(b,Specs()[i].col,Specs()[i].row,Specs()[i].kind);const auto* d=GetNativePlantData(p);return p&&d&&p->mNetId==s.actors[i].id&&d->instanceToken==s.actors[i].token?p:nullptr;}
Zombie* ZombieById(Board* b,uint16_t id)
{Zombie* z=nullptr;while(b->IterateZombies(z))if(!z->mDead&&z->mNetId==id)return z;return nullptr;}
Plant* MakePlant(LawnApp* a,uint16_t kind,int col,int row)
{
    NativePlantData data;data.kind=kind;data.owner=1;
    for(std::size_t i=0;i<GetFusionRecipeCount();++i)if(GetFusionRecipes()[i].result==kind){data.firstParent=GetFusionRecipes()[i].existing;data.secondParent=GetFusionRecipes()[i].used;break;}
    if(!IsNativeBasePlantKind(kind)&&data.firstParent==kFusionNoPlant&&data.secondParent==kFusionNoPlant)data.owner=LawnNet::NET_SHARED_OWNER;
    std::string error;if(!Require(a,EnsureNativePlantVisuals(a->mBoard,kind,&error),error.empty()?"Cannon268 actual source model missing":error.c_str()))return nullptr;
    auto* p=a->mBoard->AddPlant(col,row,GetNativeSeedForPlant(kind),SEED_NONE,&data);const auto* bound=GetNativePlantData(p);
    if(!Require(a,p&&bound&&bound->kind==kind&&bound->instanceToken&&p->mNetId&&!p->mDead,"Cannon268 initial real typed factory failed"))return nullptr;return p;
}
Zombie* MakeZombie(LawnApp* a,bool bucket,int col,int row)
{
    auto* b=a->mBoard;auto* z=b->AddZombieInRow(ZOMBIE_NORMAL,row,0);
    auto* reanim=z?a->ReanimationTryToGet(z->mBodyReanimID):nullptr;
    if(!Require(a,z&&z->mNetId&&reanim,"Cannon268 initial native NORMAL control failed"))return nullptr;
    z->mBodyHealth=z->mBodyMaxHealth=32000;
    if(bucket)
    {
        // The real NORMAL reanimation contains the same native helmet track.
        // This is an initial armor-component control, never a source NPC alias.
        if(!Require(a,reanim->TrackExists("anim_bucket"),"Cannon268 native bucket track missing"))return nullptr;
        z->mHelmType=HELMTYPE_BUCKET;z->mHelmHealth=z->mHelmMaxHealth=32000;
        z->ReanimShowPrefix("anim_bucket",RENDER_GROUP_NORMAL);
        z->ReanimShowPrefix("anim_hair",RENDER_GROUP_HIDDEN);
    }
    z->mPosX=float(b->GetCellCenterX(col,row)-60);z->mX=int(z->mPosX);z->mPosY=z->GetPosYBasedOnRow(row);z->mY=int(z->mPosY);
    // Native walking uses reanimation _ground velocity when that track exists.
    z->mVelX=0;z->ApplyAnimRate(0);
    if(!Require(a,z->mZombieType==ZOMBIE_NORMAL&&GetNativeFusionZombieStatus(z)==0,"Cannon268 native control admission differs"))return nullptr;
    s.zombies.push_back(z->mNetId);
    NetTrace("SMOKE_CANNON268_INITIAL_NPC group=%d index=%zu id=%u type=%d status=%d row=%d x=%.9g body=%d helm=%d bucketTrack=%d stationary=1 armorComponentFixture=%d initialOnly=1 sourceAI=NotClaimed",s.group,s.zombies.size()-1,z->mNetId,int(z->mZombieType),GetNativeFusionZombieStatus(z),row,z->mPosX,z->mBodyHealth,z->mHelmHealth,bucket?1:0,bucket?1:0);return z;
}
bool Setup(LawnApp* a)
{
    auto* b=a->mBoard;if(!Require(a,a->NetIsHost()&&!b->mPaused&&!b->mPlants.mSize&&!b->mZombies.mSize,"Cannon268 setup requires initial unpaused empty world"))return false;
    if(s.group==2)
    {
        GetNativeContext(b)->money=6000; // Initial controlled budget for two real3000-fee UI skills; not source income.
        if(!MakePlant(a,1112,1,4)||!MakePlant(a,1114,3,4)||!MakePlant(a,1125,5,4))return false;
    }
    for(const auto& spec:Specs())if(!MakePlant(a,spec.kind,spec.col,spec.row)||!MakeZombie(a,true,8,spec.row))return false;
    if(s.group==1){if(!MakeZombie(a,true,9,0))return false;}
    if(s.group==2){if(!MakePlant(a,914,3,4)||!MakePlant(a,914,5,4)||!MakeZombie(a,true,9,4))return false;}
    auto* mc=MakeZombie(a,false,9,s.group==0?3:s.group==1?4:1);if(!mc)return false;mc->mMindControlled=true;
    s.setup=true;s.pauseRequested=Escape(a);
    NetTrace("SMOKE_CANNON268_SETUP group=%d mainKinds=%zu money=%d initialEconomyControl=%d sourceFactory=1 sourceAI=NotClaimed actorWritesAfterSetup=0",s.group,Specs().size(),GetNativeContext(b)->money,s.group==2?1:0);return true;
}
bool TargetManifest(LawnApp* a)
{
    if(s.idsReady)return true;const auto prefix=Tag("IDS")+" ";
    if(a->NetIsHost())
    {
        std::string value=prefix;for(auto id:s.zombies)value+=std::to_string(id)+" ";value.pop_back();
        if(!Require(a,value.size()<=LawnNet::NET_MAX_CHAT_CHARS,"Cannon268 host UID proof too long"))return false;
        if(!s.idsSent){if(!Send(a,value))return false;s.idsSent=true;NetTrace("SMOKE_CANNON268_TARGET_PROOF role=host group=%d text=%s",s.group,value.c_str());}
        if(!Has(a,1,Tag("IDS_ACK")))return false;
    }
    else
    {
        if(!s.idsParsed)for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==0&&line.mText.starts_with(prefix))
        {std::istringstream input(line.mText.substr(prefix.size()));unsigned id=0;while(input>>id){if(!Require(a,id>0&&id<=65535,"Cannon268 invalid authoritative UID"))return false;s.zombies.push_back(uint16_t(id));}s.idsParsed=true;NetTrace("SMOKE_CANNON268_TARGET_PROOF role=client group=%d text=%s",s.group,line.mText.c_str());break;}
        if(!s.idsParsed||!Send(a,Tag("IDS_ACK")))return false;
    }
    if(!Require(a,s.zombies.size()==std::size_t(s.group==2?5:4),"Cannon268 authoritative target count differs"))return false;
    s.idsReady=true;return true;
}
bool Baseline(LawnApp* a)
{
    auto* b=a->mBoard;if(!TargetManifest(a)||b->mBackground!=BACKGROUND_1_DAY||GetNativeFusionRowCount(b)!=5)return false;
    for(int i=0;i<int(Specs().size());++i)
    {
        auto* p=At(b,Specs()[i].col,Specs()[i].row,Specs()[i].kind);const auto* d=GetNativePlantData(p);if(!p||!d)return false;
        if(!s.actors[i].id){s.actors[i]={p->mNetId,d->instanceToken};auto* z=ZombieById(b,s.zombies[i]);if(!z)return false;s.initialHealth[i]=z->mBodyHealth+z->mHelmHealth+z->mShieldHealth;
            NetTrace("SMOKE_CANNON268_BIRTH role=%s group=%d index=%d id=%u token=%u kind=%u owner=%u hp=%d",Role(a),s.group,i,p->mNetId,d->instanceToken,d->kind,d->owner,p->mPlantHealth);}
    }
    // The snapshot client retains its own intro/main counter. Both peers
    // measure elapsed time from their actual paused cohort baseline.
    if(s.start<0)
    {
        s.start=b->mMainCounter;
        NetTrace("SMOKE_CANNON268_CLOCK role=%s group=%d epoch=%u start=%d paused=%d samples=%u",Role(a),s.group,s.epoch,s.start,b->mPaused?1:0,Samples(a));
    }
    return true;
}
void AimAndMoney(LawnApp* a)
{
    if(!a->NetIsClient()||a->mBoard->mPaused||s.ui>=int(Specs().size()))return;auto* b=a->mBoard;auto* p=Actor(b,s.ui);const auto* d=GetNativePlantData(p);if(!p||!d)return;
    if(s.group<2)
    {
        if(s.group==1&&Specs()[s.ui].kind==915&&b->mMainCounter-s.start<900)return;
        if(s.uiPhase==0&&CanAimNativeCannon268(p))
        {float x=0,y=0;if(p->GetShadowCenter(x,y)&&BoardClick(a,x,y-20)){s.uiPhase=1;NetTrace("SMOKE_CANNON268_UI role=client group=%d index=%d source=%u token=%u intent=arm",s.group,s.ui,p->mNetId,d->instanceToken);}return;}
        if(s.uiPhase==1&&b->mCursorObject->mCursorType==CURSOR_TYPE_COBCANNON_TARGET)
        {if(BoardClick(a,float(b->GetCellCenterX(8,p->mRow)),float(b->GridToPixelY(8,p->mRow)+50))){s.uiPhase=2;NetTrace("SMOKE_CANNON268_UI role=client group=%d index=%d source=%u token=%u intent=target",s.group,s.ui,p->mNetId,d->instanceToken);}return;}
        if(s.uiPhase==2&&d->batchMode==uint8_t(FusionCannon268Phase::Firing)){++s.ui;s.uiPhase=0;}
        return;
    }
    if(s.ui>=2||b->mMainCounter-s.start<700)return;auto* context=GetNativeContext(b);
    if(s.uiPhase==0)
    {
        if(!CanActivateNativeCannon268Money(p))return;
        if(!Require(a,TryGetFusionMoneySkillFee(d->kind,s.moneyFee),"Cannon268 source Money fee missing"))return;
        if(b->mCursorObject->mCursorType!=CURSOR_TYPE_MONEY_SIGN)
        {auto rect=b->GetShovelButtonRect();b->GetZenButtonRect(OBJECT_TYPE_MONEY_SIGN,rect);BoardClick(a,float(rect.mX+rect.mWidth/2),float(rect.mY+rect.mHeight/2));return;}
        s.moneyBefore=context->money;float x=0,y=0;if(p->GetShadowCenter(x,y)&&BoardClick(a,x,y-20))s.uiPhase=1;return;
    }
    if(s.uiPhase==1&&d->batchMode==1)
    {
        if(!Require(a,context->money==s.moneyBefore-s.moneyFee,"Cannon268 Money skill did not debit exact source fee"))return;
        s.moneyAfter=context->money;s.moneyTick=b->mMainCounter;s.paid[s.ui]=true;float x=0,y=0;
        if(p->GetShadowCenter(x,y)&&BoardClick(a,x,y-20))s.uiPhase=2;return;
    }
    if(s.uiPhase==2&&b->mMainCounter-s.moneyTick>=30)
    {if(!Require(a,context->money==s.moneyAfter,"Cannon268 cooldown repeat spent Money again"))return;
     NetTrace("SMOKE_CANNON268_PAID role=client index=%d source=%u token=%u fee=%d before=%d after=%d repeatRejected=1",s.ui,p->mNetId,d->instanceToken,s.moneyFee,s.moneyBefore,s.moneyAfter);++s.ui;s.uiPhase=0;}
}
void Observe(LawnApp* a)
{
    auto* b=a->mBoard;const int elapsed=b->mMainCounter-s.start;
    if(a->mGameScene==SCENE_ZOMBIES_WON){a->CoopSmokeFail("Cannon268 natural cohort reached terminal scene");return;}
    for(int i=0;i<int(Specs().size());++i)
    {
        const auto* d=GetNativePlantData(Actor(b,i));if(d&&s.group<2&&d->batchMode==3)s.aimed[i]=true;
        if(d&&s.group==2&&i<2&&d->batchMode==1&&!s.moneyMode[i])
        {auto* z=ZombieById(b,s.zombies[i]);if(z){s.moneyHealthBefore[i]=z->mBodyHealth+z->mHelmHealth;s.moneyCommitTick[i]=b->mMainCounter;s.moneyMode[i]=true;}}
    }
    bool livePayload=false;
    Projectile* projectile=nullptr;while(b->IterateProjectiles(projectile))
    {
        const auto* d=GetNativeProjectileData(projectile);if(!d||projectile->mDead)continue;
        if(!FindFusionCannon268ProjectileProfile(d->kind)&&!IsFusionCannon268Way14(d->kind)&&d->kind!=85&&d->kind!=61&&d->kind!=62&&d->kind!=63&&d->kind!=64)continue;
        livePayload=true;s.payloads[d->kind].insert(projectile->mNetId);
        if(s.group==1&&d->kind==85&&!s.aimed[1])s.kernelBeforeAim=true;
        if(IsFusionCannon268Way14(d->kind))for(int i=0;i<int(Specs().size());++i)
        {
            const auto* profile=FindFusionCannon268Profile(Specs()[i].kind);if(!profile||profile->projectileKind!=d->kind)continue;
            if(s.group<2&&!Require(a,s.aimed[i],"Cannon268 manual projectile emitted before target commit"))return;
            if(projectile->mRotation==kFusionCannon268UpRotation)s.up[i]=true;
            if(projectile->mRotation==kFusionCannon268DownRotation)s.down[i]=true;
        }
        if(d->kind==72)
        {
            if(d->melonEnhanced)s.enhanced=true;if(d->effectVariant==1)s.cornTrait=true;
            // Main shots from the real MelonPot recipient always capture
            // enhanced=true. Its source ExtraShoot explicitly emits false.
            // Recover constant-VX birth X from the actual native body; this
            // separates that actor from the two distant914 source actors.
            float sx=0,sy=0;auto* melon=At(b,5,4,914);
            if(s.group==2&&!d->melonEnhanced&&melon&&GetNativeSourcePlantNodePoint(melon,"Shoot",sx,sy)&&
                std::abs(projectile->mPosX+20-projectile->mVelX*projectile->mProjectileAge-sx)<25)s.extraShot=true;
        }
    }
    if(s.group<2)
    {
        for(int i=0;i<int(Specs().size());++i)
        {
            auto* z=ZombieById(b,s.zombies[i]);const auto* p=FindFusionCannon268Profile(Specs()[i].kind);const auto* d=GetNativePlantData(Actor(b,i));
            if(!z||!d||!p)continue;bool okay=s.aimed[i]&&s.up[i]&&s.down[i]&&z->mBodyHealth+z->mHelmHealth+z->mShieldHealth<s.initialHealth[i];
            if(p->kind==1120||p->kind==1122||p->kind==1123)okay=okay&&d->batchShotCount==1;
            if(p->kind==1122){const auto* action=GetNativeZombieActionData(z);okay=okay&&action&&action->jalaedOnDeath;}
            if(p->kind==1123)okay=okay&&z->mIceTrapCounter>0;
            if(p->kind==1132)okay=okay&&d->batchShotCount==4&&s.payloads[73].size()>=11;
            if(p->kind==915){auto* global=ZombieById(b,s.zombies[2]);okay=okay&&s.kernelBeforeAim&&!s.payloads[85].empty()&&global&&global->mIceTrapCounter>0&&global->mBodyHealth+global->mHelmHealth<64000;}
            if(okay)s.mask|=1<<i;
        }
    }
    else
    {
        for(int i=0;i<2;++i)
        {
            auto* z=ZombieById(b,s.zombies[i]);
            if(z&&s.moneyMode[i]&&b->mMainCounter-s.moneyCommitTick[i]<=700&&s.moneyHealthBefore[i]-(z->mBodyHealth+z->mHelmHealth)>=(i?600:450))s.skillDamage[i]=true;
            if(z&&s.moneyMode[i]&&s.skillDamage[i]&&!s.payloads[i?83:80].empty()&&(i==0||s.payloads[83].size()>=16))s.mask|=1<<i;
        }
        for(std::size_t i=0;i+1<s.zombies.size();++i){const auto* action=GetNativeZombieActionData(ZombieById(b,s.zombies[i]));if(action&&(action->poisonLevel>0||action->poisonTicks>0))s.poison=true;}
        if(s.enhanced&&s.cornTrait&&s.extraShot&&s.poison&&!s.payloads[63].empty()&&!s.payloads[64].empty()&&(!s.payloads[61].empty()||!s.payloads[62].empty()))s.mask|=4;
    }
    const uint16_t mcId=s.zombies.back();auto* mc=ZombieById(b,mcId);
    if(!mc||!mc->mMindControlled||mc->mBodyHealth!=32000)
    {
        NetTrace("SMOKE_CANNON268_MC_REJECT role=%s group=%d id=%u present=%d controlled=%d hp=%d x=%.9g y=%.9g row=%d lastHp=%d lastX=%.9g lastY=%.9g lastRow=%d elapsed=%d snapshots=%u",Role(a),s.group,mcId,mc?1:0,mc&&mc->mMindControlled?1:0,mc?mc->mBodyHealth:-1,mc?mc->mPosX:s.mcLastX,mc?mc->mPosY:s.mcLastY,mc?mc->mRow:s.mcLastRow,s.mcLastHealth,s.mcLastX,s.mcLastY,s.mcLastRow,elapsed,Samples(a));
        a->CoopSmokeFail(!mc?"Cannon268 mind-controlled control missing":!mc->mMindControlled?"Cannon268 control lost mind-control":"Cannon268 mind-controlled control health changed");return;
    }
    s.mcLastX=mc->mPosX;s.mcLastY=mc->mPosY;s.mcLastRow=mc->mRow;s.mcLastHealth=mc->mBodyHealth;
    AimAndMoney(a);if(s.baseline&&!b->mPaused&&elapsed>20&&livePayload)Capture(a,"flight");
    if(!s.diagnostic||GetTickCount()-s.diagnostic>5000){s.diagnostic=GetTickCount();NetTrace("SMOKE_CANNON268_OBSERVE role=%s group=%d elapsed=%d mask=%02x expected=%02x ui=%d enhanced=%d corn=%d extra=%d poison=%d p73=%zu p83=%zu p85=%zu",Role(a),s.group,elapsed,s.mask,Expected(),s.ui,s.enhanced?1:0,s.cornTrait?1:0,s.extraShot?1:0,s.poison?1:0,s.payloads[73].size(),s.payloads[83].size(),s.payloads[85].size());}
}
bool Complete(LawnApp* a)
{return s.mask==Expected()&&(s.group!=2||!a->NetIsClient()||(s.paid[0]&&s.paid[1]));}

}
namespace LawnNet
{
bool NetSmokeUpdateFusionCannon268(LawnApp* a)
{
    const char* enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_CANNON268"),*native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
    if(!a||!enabled||std::strcmp(enabled,"1")||!native||std::strcmp(native,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
    if(a->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();
    if(GetTickCount()-s.since>150000){char why[160];sprintf_s(why,"Cannon268 bounded deadline group=%d stage=%d mask=%02x expected=%02x ui=%d",s.group,s.stage,s.mask,Expected(),s.ui);a->CoopSmokeFail(why);return true;}
    if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
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
        if(s.group==2){NetTrace("SMOKE_CANNON268_COMPLETE role=%s groups=3 sourcePlants=8 manualUI=1 moneyUI=1 freshFrames=12 initialFactory=1 acquisition=NotExecuted sourceAI=NotClaimed postSetupActorWrites=0",Role(a));a->CoopSmokeWriteResult(true,nullptr);return true;}
        State next;next.group=s.group+1;next.previousEpoch=s.epoch;next.chat=s.chat;s=std::move(next);Advance(a,1);
        if(a->NetIsHost()){std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;
    }
    if(!a->mBoard||!IsNativeFusionBoard(a->mBoard))return true;auto* b=a->mBoard;const auto* context=GetNativeContext(b);
    if(context->epoch<=s.previousEpoch)return true;if(!s.epoch)s.epoch=context->epoch;
    if(!Require(a,context->epoch==s.epoch&&context->validation&&context->visuals&&!a->mFusionRuntime&&b->GetColumnCount()==10,"Cannon268 requires real matching native validation world"))return true;
    const auto& bank=context->selection.State();auto* chooser=a->mSeedChooserScreen;
    if(s.stage==1)
    {
        if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||chooser->mSeedsInFlight||!b->mCutScene->mSeedChoosing)return true;
        int own=0;for(const auto& packet:bank.packets)if(packet.owner==Slot(a))++own;if(own<s.waiting)return true;
        if(own<5){const int wanted=5*Slot(a)+own;int dense=-1;for(std::size_t i=0;i<context->eligible.size();++i)if(context->eligible[i].kind==wanted&&!context->eligible[i].isExtra)dense=int(i);if(!Require(a,dense>=0,"Cannon268 native chooser card missing"))return true;if(Choice(a,chooser,dense))s.waiting=own+1;return true;}
        if(bank.packets.size()==10&&Capture(a,"chooser"))Advance(a,2);
    }
    else if(s.stage==2)
    {
        if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}auto* button=chooser?chooser->mStartButton:nullptr;
        if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;
        const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
        if(!button->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"Cannon268 Ready hover failed");}
        else if(Click(a,x,y))Advance(a,3);
    }
    else if(s.stage==3)
    {
        if(bank.phase==FusionBankPhase::Intro){if(!s.introFirst)s.introFirst=GetTickCount();s.introLast=GetTickCount();++s.introChecks;return true;}
        if(bank.phase!=FusionBankPhase::Playing||chooser||a->mGameScene!=SCENE_PLAYING)return true;
        if(!Require(a,s.introChecks>=10&&s.introLast-s.introFirst>=1000,"Cannon268 full native intro skipped"))return true;
        if(s.entry<0){s.entry=a->mAppCounter;return true;}if(a->mAppCounter<=s.entry)return true;
        if(a->NetIsHost()&&!s.setup&&!Setup(a))return true;if(b->mPaused)Advance(a,4);
    }
    else if(s.stage==4)
    {
        if(!b->mPaused||!Baseline(a))
        {
            if(!s.diagnostic||GetTickCount()-s.diagnostic>5000)
            {s.diagnostic=GetTickCount();int actors=0,states=0;for(const auto& spec:Specs()){auto* p=At(b,spec.col,spec.row,spec.kind);if(p)++actors;if(GetNativePlantData(p))++states;}
             NetTrace("SMOKE_CANNON268_BASELINE_WAIT role=%s group=%d epoch=%u paused=%d background=%d actors=%d/%zu states=%d idsReady=%d targets=%zu samples=%u",Role(a),s.group,s.epoch,b->mPaused?1:0,int(b->mBackground),actors,Specs().size(),states,s.idsReady?1:0,s.zombies.size(),Samples(a));}
            return true;
        }
        Observe(a);if(!s.samples){s.samples=Samples(a);return true;}
        if(Samples(a)-s.samples<2||!Capture(a,"before")||!Barrier(a,"BASELINE"))return true;s.baseline=true;
        if(a->NetIsHost()&&!s.resumeRequested)s.resumeRequested=Escape(a);
        if(s.resumeRequested||a->NetIsClient())Advance(a,5);
    }
    else if(s.stage==5)
    {
        if(b->mPaused)return true;Observe(a);if(a->mCoopSmokeResultWritten)return true;
        const int minimum=s.group==2?2500:1800;
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
        NetBuffer digest;digest.WriteI32(GetNativeContext(b)->money);digest.WriteI32(s.mask);
        for(const auto& spec:Specs())
        {
            auto* p=At(b,spec.col,spec.row,spec.kind);const auto* d=GetNativePlantData(p);
            digest.WriteU16(p?p->mNetId:0);digest.WriteU32(d?d->instanceToken:0);digest.WriteI32(p?p->mPlantHealth:-1);
            digest.WriteU8(d?d->batchMode:255);digest.WriteU32(d?d->batchShotCount:0);digest.WriteI32(d?d->productionTicks:-1);digest.WriteI32(d?d->pendingProductionTicks:-1);
        }
        for(auto id:s.zombies){auto* z=ZombieById(b,id);digest.WriteU16(id);digest.WriteI32(z?z->mBodyHealth:-1);digest.WriteI32(z?z->mHelmHealth:-1);digest.WriteI32(z?z->mShieldHealth:-1);}
        char hp[40];sprintf_s(hp,"HP_%08x",NetFnv1a32(digest.Data(),digest.Size()));if(!Barrier(a,hp)||!Capture(a,"complete")||!Barrier(a,"FINAL"))return true;
        NetTrace("SMOKE_CANNON268_CASE role=%s group=%d epoch=%u kinds=%zu mask=%02x finalSameHP=1 snapshots=%u acquisition=NotExecuted sourceAI=NotClaimed postSetupActorWrites=0",Role(a),s.group,s.epoch,Specs().size(),s.mask,Samples(a));Advance(a,10);
    }
    return true;
}
}
