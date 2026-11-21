#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Projectile.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../System/PlayerInfo.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativeWire.h"
#include "../Fusion/FusionNativeZombies.h"
#include "../../SexyAppFramework/DDInterface.h"
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
struct Ref { uint16_t id=0; uint32_t token=0; };
struct Tracked
{
    Ref ref{}; uint32_t uid=0; float birthX=0,lastHealth=270,pauseHealth=0,pausePhase=0,pauseX=0;
    int lastHelm=0,pauseHelm=0,lastNut=4000,bites=0;
    bool born=false,moved=false,locked=false,bodyHit=false,helmHit=false,hatGone=false;
    bool headless=false,fractional=false,deferred=false,dying=false,retired=false;
};
struct State
{
    int stage=0,step=0,waiting=0,entry=-1,introChecks=0,sun=600,profileCoins=0;
    uint32_t epoch=0,since=0,chat=0,wheel=0,introFirst=0,introLast=0,pickSamples=0,pauseAt=0,pauseSamples=0;
    bool configured=false,setup=false,picked=false,placed=false,observed=false,spawned=false,pauseRequest=false,resumeRequest=false,resumed=false,pauseLogged=false,diag=false;
    std::array<Ref,5> plants{}; std::array<Tracked,2> zombies{};
    std::set<std::string> frames; std::map<std::string,int> draws; std::set<uint16_t> projectiles;
} s;
struct Buy { uint16_t kind;bool extra;int owner,column,row,cost; };
const std::array<Buy,5> purchases={{{0,false,0,2,1,100},{3,false,0,5,1,50},{0,true,1,2,3,200},{3,true,1,5,3,100},{0,false,0,1,3,100}}};
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
bool InitialWalk2(){const char* flag=std::getenv("PVZ_COOP_SMOKE_FUSION_SOURCE_ZOMBIE_WALK2");return flag&&!std::strcmp(flag,"1");}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool ok,const char* why){if(!ok)a->CoopSmokeFail(why);return ok;}
void Advance(LawnApp* a,int stage){s.stage=stage;s.since=GetTickCount();s.diag=false;NetTrace("SMOKE_SZ260_STAGE role=%s stage=%d epoch=%u",Role(a),stage,s.epoch);}
std::string Tag(const char* name){return "SZ260_"+std::string(name)+"_"+std::to_string(s.epoch);}
bool Has(LawnApp* a,int slot,const std::string& text){for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==slot&&line.mText==text)return true;return false;}
bool Send(LawnApp* a,const std::string& text){if(Has(a,Slot(a),text))return true;if(s.chat&&GetTickCount()-s.chat<800)return false;s.chat=GetTickCount();a->mNetManager->SendChat(text);return Has(a,Slot(a),text);}
bool Barrier(LawnApp* a,const char* name){const auto text=Tag(name);Send(a,text);return Has(a,0,text)&&Has(a,1,text);}
bool Click(LawnApp* a,float x,float y)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Source Zombie owned SDL click failed");
}
bool BoardClick(LawnApp* a,float x,float y){return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY);}
bool Escape(LawnApp* a){SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Source Zombie owned SDL Escape failed");}
bool Choice(LawnApp* a,SeedChooserScreen* c,int index)
{
    int x=0,y=0;c->GetSeedPositionInChooser(index,x,y);const float cy=float(y+35-int(c->mScrollbar->mScrollValue));
    if(cy<158||cy>493){if(s.wheel&&GetTickCount()-s.wheel<250)return false;s.wheel=GetTickCount();SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=float(c->mX+300);m.motion.y=float(c->mY+300);SDL_Event w{};w.type=SDL_EVENT_MOUSE_WHEEL;w.wheel.windowID=m.motion.windowID;w.wheel.y=cy<158?1.f:-1.f;Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&w),"Source Zombie chooser wheel failed");return false;}
    return std::abs(c->mScrollAmount)<=.05f&&Click(a,float(c->mX+x+25),float(c->mY)+cy);
}
bool Capture(LawnApp* a,const std::string& suffix)
{
    const auto name="fusion_source_zombie_"+suffix;if(s.frames.contains(name))return true;
    if(!s.draws.contains(name)){s.draws[name]=a->mDrawCount;return false;}if(a->mDrawCount-s.draws[name]<2)return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Source Zombie renderer capture failed"))return false;
    s.frames.insert(name);NetTrace("SMOKE_SZ260_RENDER role=%s frame=%s freshDraws=%d",Role(a),name.c_str(),a->mDrawCount-s.draws[name]);return true;
}
Plant* FindPlant(Board* b,Ref ref){Plant* p=nullptr;while(b->mPlants.IterateNext(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&p->mIsOnBoard&&p->mNetId==ref.id&&d&&d->instanceToken==ref.token)return p;}return nullptr;}
Plant* At(Board* b,int column,int row){Plant* p=nullptr;while(b->mPlants.IterateNext(p))if(!p->mDead&&p->mIsOnBoard&&p->mPlantCol==column&&p->mRow==row)return p;return nullptr;}
SeedPacket* Card(Board* b,const Buy& buy){for(int i=0;i<b->mSeedBank->mNumPackets;++i){auto* p=&b->mSeedBank->mSeedPackets[i];const auto* d=GetNativePacketData(p);if(d&&d->kind==buy.kind&&d->isExtra==buy.extra&&d->owner==buy.owner)return p;}return nullptr;}
bool Purchase(LawnApp* a)
{
    auto* b=a->mBoard;const auto& buy=purchases[s.step];auto* card=Card(b,buy);const auto* packet=GetNativePacketData(card);
    if(!Require(a,card&&packet&&packet->price==buy.cost&&!packet->randomOrigin,"Source Zombie paid source card/cost missing"))return false;
    if(Slot(a)==buy.owner&&!s.picked){if(!card->mActive||card->mRefreshing)return false;s.pickSamples=Samples(a);s.picked=BoardClick(a,float(b->mSeedBank->mX+card->mX+card->mOffsetX+25),float(b->mSeedBank->mY+card->mY+35));return false;}
    if(Slot(a)==buy.owner&&!s.placed){const auto* cursor=b->mCursorObject;const bool held=cursor&&cursor->mCursorType==CURSOR_TYPE_PLANT_FROM_BANK&&cursor->mSeedBankIndex>=0&&cursor->mSeedBankIndex<b->mSeedBank->mNumPackets&&b->mSeedBank->mSeedPackets[cursor->mSeedBankIndex].mNetId==card->mNetId;if(!held||(a->NetIsClient()&&Samples(a)<=s.pickSamples))return false;if(!Require(a,b->mSunMoney==s.sun&&!At(b,buy.column,buy.row),"Source Zombie held purchase charged/mutated before placement"))return false;s.placed=BoardClick(a,float(b->GetCellCenterX(buy.column,buy.row)),float(b->GridToPixelY(buy.column,buy.row)+50));return false;}
    if(!s.observed)
    {
        auto* p=At(b,buy.column,buy.row);const auto* d=GetNativePlantData(p);if(!d||d->kind!=buy.kind)return false;
        const int hp=buy.kind==3?4000:300,cd=buy.kind==3?3000:750;
        if(!Require(a,d->instanceToken&&d->owner==buy.owner&&d->packetId==card->mNetId&&!d->randomOrigin&&d->firstParent==kFusionNoPlant&&d->secondParent==kFusionNoPlant&&p->mSeedType==GetNativeSeedForPlant(buy.kind)&&p->mPlantHealth==hp&&p->mPlantMaxHealth==hp&&b->mSunMoney==s.sun-buy.cost&&card->mRefreshTime==cd&&(!a->NetIsHost()||card->mTimesUsed==(s.step==4?2:1)),"Source Zombie paid identity/HP/SUN/CD differs"))return false;
        s.plants[s.step]={p->mNetId,d->instanceToken};s.sun-=buy.cost;s.observed=true;
        NetTrace("SMOKE_SZ260_PURCHASE role=%s epoch=%u step=%d kind=%u extra=%d owner=%d id=%u token=%u packet=%u column=%d row=%d cost=%d sun=%d hp=%d cooldown=%d nativeSDL=1",Role(a),s.epoch,s.step,buy.kind,buy.extra?1:0,buy.owner,p->mNetId,d->instanceToken,card->mNetId,buy.column,buy.row,buy.cost,s.sun,hp,cd);
    }
    const auto name="PAID"+std::to_string(s.step);return Barrier(a,name.c_str());
}
bool Ledger(LawnApp* a){return Require(a,a->mBoard->mSunMoney==s.sun&&GetNativeContext(a->mBoard)->money==3000&&a->mPlayerInfo->mCoins==s.profileCoins,"Source Zombie SUN/source money/profile ledger changed");}
float SourceX(LawnApp* a,Zombie* z){return a->NetIsClient()?z->mNetTargetX:z->mPosX;}
bool Discover(LawnApp* a)
{
    auto* b=a->mBoard;Zombie* z=nullptr;int count=0;
    while(b->mZombies.IterateNext(z))if(!z->mDead)
    {
        const auto* d=GetNativeSourceZombieData(z);if(!Require(a,d&&(d->kind==0||d->kind==2),"Source Zombie untyped/native control contaminated activity"))return false;
        const int i=d->kind==2?1:0;auto& t=s.zombies[i];++count;
        if(t.born){if(!Require(a,t.ref.id==z->mNetId&&t.ref.token==d->instanceToken,"Source Zombie duplicate kind/birth identity changed"))return false;continue;}
        const char* state=nullptr;float phase=0;auto* body=a->ReanimationTryToGet(z->mBodyReanimID);
        if(!Require(a,b->mPaused&&d->instanceToken&&d->health==270&&z->mBodyHealth==270&&z->mBodyMaxHealth==270&&z->mHelmHealth==(i?370:0)&&z->mHelmMaxHealth==(i?370:0)&&z->mZombieType==(i?ZOMBIE_TRAFFIC_CONE:ZOMBIE_NORMAL)&&d->originSpeed>=.9f&&d->originSpeed<=1.8f&&d->targetEntity==NativeZombieTargetEntity::None&&GetNativeSourceZombiePresentation(z,state,phase)&&std::string(state)==(d->originSpeed>1.35f?"walk2":"walk")&&body&&body->mAnimRate>0,"Source Zombie initial profile/rolled walk/pose differs"))return false;
        t.born=true;t.ref={z->mNetId,d->instanceToken};t.uid=b->mZombies.DataArrayGetID(z);t.birthX=SourceX(a,z);t.lastHelm=z->mHelmHealth;
        NetTrace("SMOKE_SZ260_BIRTH role=%s epoch=%u kind=%u id=%u token=%u localGeneration=%u speed=%.9g body=270 max=270 helm=%d helmMax=%d x=%.9g row=%d state=%s phase=%.9g ghost=%d",Role(a),s.epoch,d->kind,z->mNetId,d->instanceToken,t.uid,d->originSpeed,z->mHelmHealth,z->mHelmMaxHealth,t.birthX,z->mRow,state,phase,z->mNetGhost?1:0);
    }
    return count==2&&s.zombies[0].born&&s.zombies[1].born;
}
bool ProveSnapshot(LawnApp* a)
{
    auto* b=a->mBoard;LawnNet::NetBuffer wire,body;b->NetEncodeSnapshot(wire);NativeSnapshotFrame frame;
    if(!Require(a,wire.mData.size()>6&&wire.mData[4]==kNativeFusionSnapshotVersion&&DecodeNativeSnapshot(wire,GetNativeContext(b)->eligible,s.epoch,frame,body)&&frame.sourceZombies.size()==2,"Source Zombie real FNS capture/decode failed"))return false;
    for(const auto& t:s.zombies){auto* z=b->mZombies.DataArrayTryToGet(t.uid);const auto* d=GetNativeSourceZombieData(z);bool found=false;for(const auto& record:frame.sourceZombies)if(record.id==t.ref.id){found=d&&record.data.instanceToken==d->instanceToken&&record.data.kind==d->kind&&record.data.health==d->health&&record.data.originSpeed==d->originSpeed&&record.data.targetEntity==d->targetEntity&&record.data.targetId==d->targetId&&record.data.targetGeneration==d->targetGeneration;}if(!Require(a,found,"Source Zombie exact FNS source float/token projection differs"))return false;}
    return true;
}
bool PauseProof(LawnApp* a)
{
    auto* b=a->mBoard;if(!b->mPaused||Samples(a)-s.pauseSamples<2)return false;if(!Discover(a))return false;
    if(!s.pauseAt){s.pauseAt=GetTickCount();s.pauseSamples=Samples(a);for(auto& t:s.zombies){auto* z=b->mZombies.DataArrayTryToGet(t.uid);t.pauseHealth=GetNativeSourceZombieData(z)->health;t.pauseHelm=z->mHelmHealth;t.pauseX=SourceX(a,z);t.pausePhase=a->ReanimationTryToGet(z->mBodyReanimID)->mAnimTime;}return false;}
    for(const auto& t:s.zombies){auto* z=b->mZombies.DataArrayTryToGet(t.uid);const auto* d=GetNativeSourceZombieData(z);auto* body=z?a->ReanimationTryToGet(z->mBodyReanimID):nullptr;if(!Require(a,z&&d&&body&&d->health==t.pauseHealth&&z->mHelmHealth==t.pauseHelm&&SourceX(a,z)==t.pauseX&&body->mAnimTime==t.pausePhase,"Source Zombie source HP/armor/phase/authoritative position advanced while paused"))return false;}
    if(GetTickCount()-s.pauseAt<400||Samples(a)-s.pauseSamples<2)return false;if(!ProveSnapshot(a))return false;
    if(!s.pauseLogged){s.pauseLogged=true;NetTrace("SMOKE_SZ260_PAUSE role=%s epoch=%u stableMs=%u snapshots=%u health=270,270 helm=0,370 exactFNS=1",Role(a),s.epoch,GetTickCount()-s.pauseAt,Samples(a)-s.pauseSamples);}return true;
}
bool Observe(LawnApp* a)
{
    auto* b=a->mBoard;if(!Ledger(a))return false;
    for(int i=0;i<2;++i)
    {
        auto& t=s.zombies[i];auto* z=b->mZombies.DataArrayTryToGet(t.uid);auto* nut=FindPlant(b,s.plants[i?3:1]);
        if(!Require(a,nut&&nut->mPlantHealth>0,"Source Zombie natural target Wallnut was lost"))return false;
        const int loss=t.lastNut-nut->mPlantHealth;if(!Require(a,loss>=0&&loss%50==0,"Source Zombie bite must be exact source50 multiples"))return false;
        if(loss){t.bites+=loss/50;NetTrace("SMOKE_SZ260_BITE role=%s kind=%d id=%u target=%u loss=%d total=%d hp=%d",Role(a),i?2:0,t.ref.id,nut->mNetId,loss,t.bites,nut->mPlantHealth);}t.lastNut=nut->mPlantHealth;
        if(!z||z->mDead)
        {if(!t.retired){if(!Require(a,t.dying,"Source Zombie disappeared before natural death tail was observed"))return false;t.retired=true;NetTrace("SMOKE_SZ260_RETIRED role=%s epoch=%u kind=%d id=%u token=%u localGeneration=%u",Role(a),s.epoch,i?2:0,t.ref.id,t.ref.token,t.uid);}continue;}
        const auto* d=GetNativeSourceZombieData(z);if(!Require(a,d&&d->instanceToken==t.ref.token&&d->kind==(i?2:0)&&z->mZombieType==(i?ZOMBIE_TRAFFIC_CONE:ZOMBIE_NORMAL)&&d->originSpeed>=.9f&&d->originSpeed<=1.8f,"Source Zombie live canonical kind/token/alias changed"))return false;
        const char* state=nullptr;float phase=0;if(!Require(a,GetNativeSourceZombiePresentation(z,state,phase)&&state&&std::isfinite(phase)&&phase>=0&&phase<=1,"Source Zombie source pose/native phase unavailable"))return false;
        if(!t.moved&&SourceX(a,z)<t.birthX-2){t.moved=true;NetTrace("SMOKE_SZ260_MOVED role=%s kind=%u id=%u from=%.9g to=%.9g state=%s phase=%.9g",Role(a),d->kind,z->mNetId,t.birthX,SourceX(a,z),state,phase);}
        if(d->targetEntity==NativeZombieTargetEntity::Plant&&!t.locked){if(!Require(a,d->targetId==nut->mNetId&&d->targetGeneration==s.plants[i?3:1].token,"Source Zombie locked unexpected target/generation"))return false;t.locked=true;NetTrace("SMOKE_SZ260_LOCK role=%s kind=%u id=%u target=%u targetToken=%u state=%s",Role(a),d->kind,z->mNetId,d->targetId,d->targetGeneration,state);}
        if(z->IsDeadOrDying())
        {auto* body=a->ReanimationTryToGet(z->mBodyReanimID);if(!Require(a,z->mZombiePhase==PHASE_ZOMBIE_DYING&&std::string(state)=="die"&&body&&body->mAnimRate>0,"Source Zombie native death animation stalled/changed"))return false;if(!t.dying){t.dying=true;NetTrace("SMOKE_SZ260_DYING role=%s kind=%u id=%u token=%u health=%.9g body=%d rate=%.9g",Role(a),d->kind,z->mNetId,d->instanceToken,d->health,z->mBodyHealth,body->mAnimRate);}Capture(a,"dying"+std::to_string(i));continue;}
        if(!Require(a,z->mBodyHealth==NativeSourceZombieLivenessHealth(d->health),"Source Zombie source float/native liveness projection differs"))return false;
        if(z->mIsEating&&z->mJustGotShotCounter<=0&&z->mHelmetJustGotShotCounter<=0)Capture(a,"contact"+std::to_string(i));
        if(i&&z->mHelmHealth>0&&z->GetHelmDamageIndex()==1&&z->mHelmetJustGotShotCounter<=0)Capture(a,"armor");
        if(i&&z->mHelmHealth<370&&!t.helmHit){t.helmHit=true;NetTrace("SMOKE_SZ260_ARMOR role=%s kind=2 id=%u helm=%d body=%d sourceHP=%.9g",Role(a),z->mNetId,z->mHelmHealth,z->mBodyHealth,d->health);}
        if(i&&z->mHelmHealth==0&&z->mHelmType==HELMTYPE_NONE&&!t.hatGone){t.hatGone=true;NetTrace("SMOKE_SZ260_HAT_GONE role=%s kind=2 id=%u token=%u alias=%d body=%d sourceHP=%.9g",Role(a),z->mNetId,d->instanceToken,int(z->mZombieType),z->mBodyHealth,d->health);}
        if(d->health<270&&!t.bodyHit){t.bodyHit=true;NetTrace("SMOKE_SZ260_BODY role=%s kind=%u id=%u health=%.9g body=%d helm=%d",Role(a),d->kind,z->mNetId,d->health,z->mBodyHealth,z->mHelmHealth);}
        if(!z->mHasHead)
        {
            t.headless=true;if(std::fabs(d->health-std::round(d->health))>.001f&&!t.fractional){t.fractional=true;NetTrace("SMOKE_SZ260_FLOAT_BLEED role=%s kind=%u id=%u health=%.9g body=%d max=%d head=0",Role(a),d->kind,z->mNetId,d->health,z->mBodyHealth,z->mBodyMaxHealth);}
            if(a->NetIsHost()&&d->health>0&&d->health<1){t.deferred=true;NetTrace("SMOKE_SZ260_DEFERRED role=host kind=%u id=%u health=%.9g body=%d sourcePreupdateDeathNext=1",d->kind,z->mNetId,d->health,z->mBodyHealth);}
            if(z->mJustGotShotCounter<=0)Capture(a,"headless"+std::to_string(i));
        }
        if(a->NetIsHost()&&z->mHasHead){const float damage=t.lastHealth-d->health+float(t.lastHelm-z->mHelmHealth);if(damage>0&&!Require(a,std::fabs(damage/20.0f-std::round(damage/20.0f))<.001f,"Source Zombie natural PEA body/armor damage is not20"))return false;}
        t.lastHealth=d->health;t.lastHelm=z->mHelmHealth;
    }
    Projectile* p=nullptr;while(b->mProjectiles.IterateNext(p))if(!p->mDead&&p->mProjectileType==PROJECTILE_PEA&&(p->mRow==1||p->mRow==3)&&s.projectiles.insert(p->mNetId).second)NetTrace("SMOKE_SZ260_PEA role=%s epoch=%u id=%u row=%d type=%d",Role(a),s.epoch,p->mNetId,p->mRow,int(p->mProjectileType));
    return true;
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionSourceZombie(LawnApp* a)
{
    const auto* flag=std::getenv("PVZ_COOP_SMOKE_FUSION_SOURCE_ZOMBIE");const auto* native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
    if(!a||!flag||std::strcmp(flag,"1")||!native||std::strcmp(native,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
    if(a->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();if(GetTickCount()-s.since>120000U){a->CoopSmokeFail("Source Zombie bounded stage deadline");return true;}
    if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
    if(s.stage==0){if(a->NetIsHost()&&!s.configured){a->mNetManager->SetDifficulty(1);s.configured=true;}if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;if(a->NetIsClient()){Advance(a,1);Send(a,"SZ260_BEGIN");}else if(Has(a,1,"SZ260_BEGIN")){Advance(a,1);std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;}
    if(!a->mBoard||!IsNativeFusionBoard(a->mBoard)){if(s.stage==1&&a->NetIsClient())Send(a,"SZ260_BEGIN");return true;}auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);if(!s.epoch)s.epoch=ctx->epoch;
    if(!Require(a,ctx->epoch==s.epoch&&ctx->validation&&!a->mFusionRuntime&&b->GetColumnCount()==10&&b->mBackground==BACKGROUND_1_DAY,"Source Zombie canonical native Day activity missing"))return true;
    const auto& bank=ctx->selection.State();auto* chooser=a->mSeedChooserScreen;
    if(s.stage==1)
    {
        if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||!b->mCutScene->mSeedChoosing||chooser->mSeedsInFlight)return true;if(!Require(a,bank.activeMask==3&&bank.targetCount==10&&bank.quotas[0]==5&&bank.quotas[1]==5,"Source Zombie real fair quotas differ"))return true;
        const std::array<std::pair<uint16_t,bool>,5> choices=Slot(a)==0?std::array<std::pair<uint16_t,bool>,5>{{{0,false},{3,false},{1,false},{2,false},{4,false}}}:std::array<std::pair<uint16_t,bool>,5>{{{0,true},{3,true},{1,true},{2,true},{4,true}}};
        int own=0;for(const auto& p:bank.packets)own+=p.owner==Slot(a);if(own<s.waiting)return true;if(own<5){int index=-1;for(size_t i=0;i<ctx->eligible.size();++i)if(ctx->eligible[i].kind==choices[own].first&&ctx->eligible[i].isExtra==choices[own].second)index=int(i);if(!Require(a,index>=0,"Source Zombie actual0/3 card eligibility missing"))return true;if(Choice(a,chooser,index))s.waiting=own+1;return true;}if(bank.packets.size()!=10)return true;
        int rank=-1;for(int i=0;i<b->mSeedBank->mNumPackets;++i){const int next=NetSeedBankOwnerRank(Slot(a),b->mSeedBank->mSeedPackets[i].mNetOwnerSlot);if(!Require(a,next>=rank,"Source Zombie native own-first order differs"))return true;rank=next;}if(!Capture(a,"chooser"))return true;NetTrace("SMOKE_SZ260_POOL role=%s eligible=%zu target=10 quotas=5,5 ownFirst=1",Role(a),ctx->eligible.size());Advance(a,2);
    }
    else if(s.stage==2){if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}auto* button=chooser?chooser->mStartButton:nullptr;if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;if(!button->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"Source Zombie Ready hover failed");}else if(Click(a,x,y))Advance(a,3);}
    else if(s.stage==3){if(bank.phase==FusionBankPhase::Intro){if(!s.introFirst)s.introFirst=GetTickCount();s.introLast=GetTickCount();++s.introChecks;return true;}if(bank.phase!=FusionBankPhase::Playing||a->mGameScene!=SCENE_PLAYING||chooser)return true;if(!Require(a,s.introChecks>=10&&s.introLast-s.introFirst>=1000&&b->mSunMoney==500&&ctx->money==3000,"Source Zombie fullintro/fresh ledgers differ"))return true;if(s.entry<0){s.entry=a->mAppCounter;return true;}if(a->mAppCounter<=s.entry)return true;s.profileCoins=a->mPlayerInfo->mCoins;Advance(a,4);}
    else if(s.stage==4){if(a->NetIsHost()&&!s.setup){if(!Require(a,!b->mPlants.mSize&&!b->mZombies.mSize,"Source Zombie initial world not naturally empty"))return true;b->mSunCountDown=b->mZombieCountDown=b->mZombieCountDownStart=100000;b->mSunMoney=600;s.setup=true;NetTrace("SMOKE_SZ260_SETUP initialSUN=600 sourceMoney=3000 oneSUNwrite=1 ambientOnly=1 initialActorHPWrites=0");}if(b->mSunMoney==600&&Barrier(a,"INITIAL"))Advance(a,5);}
    else if(s.stage==5){if(s.step==5){if(Capture(a,"planted")&&Barrier(a,"PLANTED"))Advance(a,6);return true;}if(Purchase(a)){++s.step;s.picked=s.placed=s.observed=false;}}
    else if(s.stage==6)
    {
        if(!Ledger(a))return true;
        if(a->NetIsHost()&&!s.spawned)
        {
            for(int i=0;i<2;++i)
            {
                auto* nut=FindPlant(b,s.plants[i?3:1]);auto* z=SpawnNativeSourceZombie(b,i?2:0,i?3:1,0);
                if(!Require(a,nut&&z,"Source Zombie explicit factory spawn failed"))return true;
                if(InitialWalk2())
                {
                    const auto* data=GetNativeSourceZombieData(z);
                    if(!Require(a,data&&z->mZombieAge==0,"Source Zombie controlled origin must be initial setup only"))return true;
                    auto initial=*data;const float generated=initial.originSpeed;initial.originSpeed=1.5f;
                    if(!Require(a,AttachNativeSourceZombieData(b,z,initial)&&StartNativeSourceZombieWalk(z,0),"Source Zombie controlled initial walk2 setup failed"))return true;
                    NetTrace("SMOKE_SZ260_INITIAL_ORIGIN role=host epoch=%u kind=%d id=%u token=%u generated=%.9g used=1.5 age=0 initialOnly=1 factoryDrawConsumed=1",s.epoch,i?2:0,z->mNetId,initial.instanceToken,generated);
                }
                const auto plant=nut->GetPlantRect(),attack=z->GetZombieAttackRect();
                z->mPosX=float(plant.mX+plant.mWidth+8-(attack.mX-z->mX));z->mX=int(z->mPosX);
                NetTrace("SMOKE_SZ260_POSITION kind=%d id=%u initialX=%.9g nativeRowY=%.9g contactGap=8 onlyInitialXWrite=1",i?2:0,z->mNetId,z->mPosX,z->mPosY);
            }
            NetTrace("SMOKE_SZ260_ORIGIN_SETUP role=host epoch=%u controlledInitialWalk2=%d initialOriginWrites=%d naturalRNGClaim=%d",s.epoch,InitialWalk2()?1:0,InitialWalk2()?2:0,InitialWalk2()?0:1);
            s.spawned=true;s.pauseRequest=Escape(a);
        }
        if(b->mPaused){s.pauseSamples=Samples(a);Advance(a,7);}
    }
    else if(s.stage==7){if(!Ledger(a)||!PauseProof(a)||!Capture(a,"birth")||!Barrier(a,"PAUSED"))return true;Advance(a,8);}
    else if(s.stage==8)
    {
        if(!Ledger(a))return true;if(a->NetIsHost()&&!s.resumeRequest){s.resumeRequest=Escape(a);return true;}if(b->mPaused||a->GetDialogCount())return true;
        if(!s.resumed){bool progress=false;for(const auto& t:s.zombies){auto* z=b->mZombies.DataArrayTryToGet(t.uid);auto* body=z?a->ReanimationTryToGet(z->mBodyReanimID):nullptr;progress|=body&&(body->mAnimTime!=t.pausePhase||SourceX(a,z)!=t.pauseX);}if(!progress)return true;s.resumed=true;NetTrace("SMOKE_SZ260_RESUMED role=%s epoch=%u actualPhaseProgress=1",Role(a),s.epoch);}
        if(Barrier(a,"RESUMED")){NetTrace("SMOKE_SZ260_RESUME_ACKS role=%s epoch=%u host=1 client=1",Role(a),s.epoch);Advance(a,9);}
    }
    else if(s.stage==9)
    {
        if(!Observe(a))return true;bool complete=ctx->sourceZombies.empty()&&s.frames.contains("fusion_source_zombie_armor");for(int i=0;i<2;++i){const auto& t=s.zombies[i];complete&=t.moved&&t.locked&&t.bites>0&&t.bodyHit&&t.headless&&t.fractional&&t.dying&&t.retired&&(!i||(t.helmHit&&t.hatGone))&&(!a->NetIsHost()||t.deferred)&&s.frames.contains("fusion_source_zombie_contact"+std::to_string(i))&&s.frames.contains("fusion_source_zombie_headless"+std::to_string(i))&&s.frames.contains("fusion_source_zombie_dying"+std::to_string(i));}
        if(complete&&!s.projectiles.empty()&&Capture(a,"complete")&&Barrier(a,"COMPLETE")){for(int i=0;i<2;++i){const auto& t=s.zombies[i];NetTrace("SMOKE_SZ260_OUTCOME role=%s epoch=%u kind=%d id=%u token=%u bites=%d moved=1 locked=1 bodyHit=1 headless=1 floatBleed=1 dying=1 retired=1 hatGone=%d deferredHost=%d",Role(a),s.epoch,i?2:0,t.ref.id,t.ref.token,t.bites,t.hatGone?1:0,t.deferred?1:0);}NetTrace("SMOKE_SZ260_COMPLETE role=%s epoch=%u sourceKinds=0,2 purchases=5 cost=550 sun=50 money=3000 profileUnchanged=1 sourcePreset=2 pause=1 resumedBoth=1 postSetupActorWrites=0 initialXWrites=2 MC=NotExecuted shrink=NotExecuted secondWorld=0",Role(a),s.epoch);Advance(a,10);a->CoopSmokeWriteResult(true,nullptr);}
        else if(!s.diag&&GetTickCount()-s.since>10000){s.diag=true;for(int i=0;i<2;++i){const auto& t=s.zombies[i];NetTrace("SMOKE_SZ260_WAIT role=%s kind=%d moved=%d locked=%d bites=%d armor=%d body=%d headless=%d fraction=%d dying=%d retired=%d deferred=%d",Role(a),i?2:0,t.moved,t.locked,t.bites,t.helmHit,t.bodyHit,t.headless,t.fractional,t.dying,t.retired,t.deferred);}}
    }
    return true;
}
}
