#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionBatchActionRules.h"
#include "../Fusion/FusionRecipe.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../SexyAppFramework/DDInterface.h"
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
constexpr int kColumn=4,kRow=2;
struct Ref { uint16_t id=0; uint32_t token=0; };
struct Sample { int body=0,helm=0,shield=0,level=0,threshold=100,slow=0,freeze=0; };
struct State
{
    int stage=0,waitingOwn=0,introChecks=0,playingTick=0,selectedOwn=0,choiceDense=-1,money=500;
    uint32_t since=0,chat=0,wheel=0,introFirst=0,introLast=0,epoch=0,pickSamples=0;
    bool configured=false,ambient=false,picked=false,placed=false,bought=false,setup=false,baseline=false;
    bool initialPause=false,initialResume=false,resumedOne=false,freezePause=false,freezeSeen=false;
    bool freezeReady=false,freezeResume=false,resumedTwo=false,finalPause=false,finalSeen=false,finalLogged=false;
    bool lethalRetired=false,lethalProgress=false,pendingLogged=false;
    int lethalTick=0,lethalRetiredTick=0;float lethalAnim=0;
    uint32_t pauseSince=0,pauseSamples=0,finalSamples=0;int pauseChecks=0;
    Ref parent{},plant{};std::array<uint16_t,2> packets{};std::array<uint16_t,5> targets{};
    std::array<bool,3> first{};std::array<Sample,2> paused{};
    std::map<std::string,int> draw;std::set<std::string> frames;std::string ids;
} s;
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool ok,const char* why){if(!ok)a->CoopSmokeFail(why);return ok;}
void Advance(LawnApp* a,int stage)
{
    s.stage=stage;s.since=GetTickCount();s.picked=s.placed=s.bought=false;
    NetTrace("SMOKE_FIC251_STAGE role=%s stage=%d epoch=%u",Role(a),stage,s.epoch);
}
std::string Tag(const char* name){return "FIC251_"+std::string(name)+"_"+std::to_string(s.epoch);}
bool Has(LawnApp* a,int who,const std::string& text){for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==who&&line.mText==text)return true;return false;}
bool Send(LawnApp* a,const std::string& text)
{
    if(Has(a,Slot(a),text))return true;
    if(s.chat&&GetTickCount()-s.chat<800)return false;s.chat=GetTickCount();a->mNetManager->SendChat(text);
    return Has(a,Slot(a),text);
}
bool Barrier(LawnApp* a,const char* name){const auto text=Tag(name);Send(a,text);return Has(a,0,text)&&Has(a,1,text);}
bool Click(LawnApp* a,float x,float y)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;
    return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"IceCaltrop owned SDL click failed");
}
bool BoardClick(LawnApp* a,float x,float y){return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY);}
bool Escape(LawnApp* a)
{
    SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;
    SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;
    return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"IceCaltrop owned SDL Escape failed");
}
bool Choice(LawnApp* a,SeedChooserScreen* chooser,int index)
{
    int x=0,y=0;chooser->GetSeedPositionInChooser(index,x,y);const float cy=float(y+35-int(chooser->mScrollbar->mScrollValue));
    s.choiceDense=index;
    if(cy<158||cy>493)
    {
        if(s.wheel&&GetTickCount()-s.wheel<250)return false;s.wheel=GetTickCount();
        SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=float(chooser->mX+300);m.motion.y=float(chooser->mY+300);
        SDL_Event w{};w.type=SDL_EVENT_MOUSE_WHEEL;w.wheel.windowID=m.motion.windowID;w.wheel.y=cy<158?1.f:-1.f;
        Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&w),"IceCaltrop owned chooser wheel failed");return false;
    }
    if(std::abs(chooser->mScrollAmount)>.05f)return false;
    return Click(a,float(chooser->mX+x+25),float(chooser->mY)+cy);
}
bool Ready(LawnApp* a,GameButton* button)
{
    if(!button||button->mDisabled||button->mBtnNoDraw)return false;const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
    if(!button->IsMouseOver())
    {SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"IceCaltrop Ready hover failed");return false;}
    NetTrace("SMOKE_FIC251_READY role=%s nativeSDL=1 hoverObserved=1",Role(a));return Click(a,x,y);
}
bool Capture(LawnApp* a,const char* suffix)
{
    const std::string name="fusion_icecaltrop_"+std::string(suffix);
    if(s.frames.contains(name))return true;
    if(!s.draw.contains(name)){s.draw[name]=a->mDrawCount;return false;}
    if(a->mDrawCount-s.draw[name]<2)return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"IceCaltrop actual renderer capture failed"))return false;
    s.frames.insert(name);NetTrace("SMOKE_FIC251_RENDER role=%s frame=%s freshDraws=%d",Role(a),name.c_str(),a->mDrawCount-s.draw[name]);return true;
}
Plant* Actor(Board* b,Ref ref)
{Plant* p=nullptr;while(b->mPlants.IterateNext(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&p->mNetId==ref.id&&d&&d->instanceToken==ref.token)return p;}return nullptr;}
Plant* At(Board* b,uint16_t kind)
{Plant* p=nullptr;while(b->mPlants.IterateNext(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&p->mIsOnBoard&&p->mPlantCol==kColumn&&p->mRow==kRow&&d&&d->kind==kind)return p;}return nullptr;}
SeedPacket* Packet(Board* b,int owner,int kind)
{for(int i=0;i<b->mSeedBank->mNumPackets;++i){auto* p=&b->mSeedBank->mSeedPackets[i];const auto* d=GetNativePacketData(p);if(d&&d->owner==owner&&d->kind==kind&&!d->isExtra)return p;}return nullptr;}
bool Holding(Board* b,SeedPacket* packet)
{auto* c=b->mCursorObject;return c&&packet&&c->mCursorType==CURSOR_TYPE_PLANT_FROM_BANK&&c->mSeedBankIndex>=0&&c->mSeedBankIndex<b->mSeedBank->mNumPackets&&b->mSeedBank->mSeedPackets[c->mSeedBankIndex].mNetId==packet->mNetId;}
Zombie* Target(Board* b,uint16_t id)
{Zombie* z=nullptr;while(b->mZombies.IterateNext(z))if(z->mNetId==id&&!z->mDead&&(!z->mNetGhost||z->mVisible))return z;return nullptr;}
Sample Read(Zombie* z)
{const auto* d=GetNativeZombieColdData(z);return {z->mBodyHealth,z->mHelmHealth,z->mShieldHealth,d?d->level:0,d?d->threshold:100,z->mChilledCounter,z->mIceTrapCounter};}
bool Business(LawnApp* a,int uses)
{
    for(int i=0;i<2;++i)
    {
        auto* p=Packet(a->mBoard,i,i?10:17);const auto* d=GetNativePacketData(p);const bool used=uses>i;
        const int full=used?(i?5000:750):0;
        const bool ok=p&&d&&p->mNetId==s.packets[i]&&d->price==(i?75:100)&&d->owner==i&&!d->isExtra&&!d->randomOrigin&&p->mRefreshTime==full&&(!a->NetIsHost()||p->mTimesUsed==(used?1:0));
        if(!ok)NetTrace("SMOKE_FIC251_BUSINESS role=%s index=%d id=%u expected=%u price=%d owner=%d refresh=%d expectedRefresh=%d uses=%d",Role(a),i,p?p->mNetId:0,s.packets[i],d?d->price:-1,d?d->owner:-1,p?p->mRefreshTime:-1,full,p?p->mTimesUsed:-1);
        if(!Require(a,ok,"IceCaltrop canonical card identity/price/cooldown differs"))return false;
    }
    return Require(a,a->mBoard->mSunMoney==s.money,"IceCaltrop wallet changed outside real purchase");
}
bool Purchase(LawnApp* a,int step)
{
    auto* b=a->mBoard;const int owner=step,kind=step?10:17,result=step?1180:17,before=step?400:500,after=step?325:400;
    auto* packet=Packet(b,owner,kind);
    if(Slot(a)==owner&&!s.picked)
    {
        if(!packet||!packet->mActive||packet->mRefreshing)return false;
        auto* bank=b->mSeedBank;s.pickSamples=Samples(a);
        s.picked=BoardClick(a,float(bank->mX+packet->mX+packet->mOffsetX+packet->mWidth/2),float(bank->mY+packet->mY+packet->mHeight/2));return false;
    }
    if(Slot(a)==owner&&!s.placed)
    {
        if(!Holding(b,packet)||(a->NetIsClient()&&Samples(a)<=s.pickSamples))return false;
        if(!Require(a,b->mSunMoney==before,"IceCaltrop pickup charged currency before placement"))return false;
        s.placed=BoardClick(a,float(b->GetCellCenterX(kColumn,kRow)),float(b->GridToPixelY(kColumn,kRow)+50));return false;
    }
    if(!s.bought)
    {
        auto* p=At(b,uint16_t(result));const auto* d=GetNativePlantData(p);if(!p||!d)return false;
        const bool parents=step?(d->firstParent==17&&d->secondParent==10):(d->firstParent==kFusionNoPlant&&d->secondParent==kFusionNoPlant);
        if(!Require(a,packet&&d->owner==owner&&d->packetId==packet->mNetId&&!d->randomOrigin&&d->instanceToken&&parents&&p->mPlantHealth==300&&!p->mIsAsleep&&b->mSunMoney==after,"IceCaltrop paid source/provenance/HP/price differs"))return false;
        if(step)
        {if(!Require(a,p->mNetId!=s.parent.id&&d->instanceToken!=s.parent.token&&!Actor(b,s.parent),"IceCaltrop did not atomically retire its actual parent"))return false;s.plant={p->mNetId,d->instanceToken};}
        else s.parent={p->mNetId,d->instanceToken};
        s.money=after;s.bought=true;
        NetTrace("SMOKE_FIC251_PURCHASE role=%s step=%d incoming=%d kind=%d id=%u token=%u parentId=%u parentToken=%u owner=%d packet=%u cost=%d sun=%d HP=300 cooldown=%d nativeSDL=1",Role(a),step,kind,result,p->mNetId,d->instanceToken,step?s.parent.id:0,step?s.parent.token:0,owner,packet->mNetId,step?75:100,after,step?5000:750);
    }
    return Business(a,step+1)&&Barrier(a,step?"CRAFT":"PLANTED");
}
bool ReadIds(LawnApp* a)
{
    if(a->NetIsHost())return s.setup;const auto prefix=Tag("IDS")+" ";
    for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==0&&line.mText.rfind(prefix,0)==0)
    {
        std::istringstream in(line.mText.substr(prefix.size()));std::set<unsigned> seen;
        for(auto& id:s.targets){unsigned value=0;if(!Require(a,bool(in>>value)&&value&&value<=65535&&seen.insert(value).second,"IceCaltrop canonical target ID message invalid"))return false;id=uint16_t(value);}
        std::string extra;if(!Require(a,!(in>>extra),"IceCaltrop target ID message trailing data"))return false;s.ids=line.mText;s.setup=true;return true;
    }
    return false;
}
bool Setup(LawnApp* a)
{
    if(!a->NetIsHost()||s.setup)return true;auto* b=a->mBoard;auto* plant=Actor(b,s.plant);float x=0,y=0;FusionBatchArea area;
    if(!Require(a,b->mPaused&&plant&&plant->GetShadowCenter(x,y)&&TryMakeFusionBatchArea(1180,x,y,true,area)&&!b->mZombies.mSize,"IceCaltrop initial native cohort setup unavailable"))return false;
    for(int i=0;i<5;++i)
    {
        const int row=i==3?kRow+1:kRow;auto* z=b->AddZombieInRow(ZOMBIE_NORMAL,row,0);auto* body=z?a->ReanimationTryToGet(z->mBodyReanimID):nullptr;const float rate=body?body->mAnimRate:0;
        if(!Require(a,z&&z->mNetId&&body&&std::isfinite(rate)&&rate>0,"IceCaltrop native target factory/walking rate unavailable"))return false;
        if(i==0){z->mHelmType=HELMTYPE_BUCKET;z->AttachHelmet();z->mShieldType=SHIELDTYPE_DOOR;z->AttachShield();}
        z->mBodyHealth=z->mBodyMaxHealth=i==2?20:500;z->mHelmHealth=z->mHelmMaxHealth=i==0?100:0;z->mShieldHealth=z->mShieldMaxHealth=i==0?200:0;
        z->mPosY=z->GetPosYBasedOnRow(row);z->mY=int(z->mPosY);const auto r=z->GetZombieRect();const float wanted=x+(i==4?180.f:float(i*3));
        z->mPosX+=wanted-(r.mX+r.mWidth*.5f);z->mX=int(z->mPosX);z->mVelX=0;
        if(i<3)
        {
            auto* cold=GetOrCreateNativeZombieColdData(z);if(!Require(a,cold!=nullptr,"IceCaltrop initial cold control admission failed"))return false;
            *cold={i==2?90:0,100,i==2?0:1400,i==1?400:0};z->SetColdCounters(cold->chilledTicks,cold->frozenTicks);
        }
        z->SetAnimRate(rate);z->ApplyAnimRate(0);s.targets[i]=z->mNetId;
        const auto actual=z->GetZombieRect();const bool touches=FusionBatchAreaTouchesRect(area,actual.mX,actual.mY,actual.mWidth,actual.mHeight);
        if(!Require(a,(i<3?touches:true)&&(i!=4||!touches)&&!z->FindPlantTarget(ATTACKTYPE_CHEW),"IceCaltrop controlled contact/outside/bite geometry differs"))return false;
        NetTrace("SMOKE_FIC251_SETUP index=%d id=%u generation=%u row=%d body=%d helm=%d shield=%d level=%d threshold=100 slow=%d freeze=%d contact=%d nativeNORMAL=1 fixtureInitialOnly=1",i,z->mNetId,uint32_t(b->ZombieGetID(z)),row,z->mBodyHealth,z->mHelmHealth,z->mShieldHealth,i==2?90:0,z->mChilledCounter,z->mIceTrapCounter,touches?1:0);
    }
    std::ostringstream out;out<<Tag("IDS");for(auto id:s.targets)out<<' '<<id;s.ids=out.str();s.setup=true;return true;
}
bool Baseline(LawnApp* a)
{
    if(!ReadIds(a))return false;
    for(int i=0;i<5;++i)
    {
        auto* z=Target(a->mBoard,s.targets[i]);if(!z)return false;const auto q=Read(z);
        if(!Require(a,q.body==(i==2?20:500)&&q.helm==(i==0?100:0)&&q.shield==(i==0?200:0)&&q.level==(i==2?90:0)&&q.threshold==100&&q.slow==(i<2?1400:0)&&q.freeze==(i==1?400:0),"IceCaltrop paused initial full-snapshot controls differ"))return false;
    }
    if(!s.baseline){s.baseline=true;NetTrace("SMOKE_FIC251_BASELINE role=%s ids=%u,%u,%u,%u,%u paused=1 visibleFullSnapshot=1 sun=325",Role(a),s.targets[0],s.targets[1],s.targets[2],s.targets[3],s.targets[4]);}
    return true;
}
bool Observe(LawnApp* a)
{
    if(!s.baseline)return true;auto* b=a->mBoard;auto* plant=Actor(b,s.plant);const auto* d=GetNativePlantData(plant);
    if(!Require(a,plant&&d&&d->kind==1180&&plant->mPlantHealth==300,"IceCaltrop actual source actor disappeared or changed HP"))return false;
    if(!b->mPaused&&d->shootSeconds>=.05f&&d->shootSeconds<.5f)Capture(a,"attack");
    for(int i=0;i<5;++i)
    {
        auto* z=Target(b,s.targets[i]);
        if(i==2)
        {
            if(z&&(z->IsDeadOrDying()||z->mBodyHealth<=0))
            {
                const auto q=Read(z);auto* body=a->ReanimationTryToGet(z->mBodyReanimID);
                if(!s.first[2])
                {
                    if(a->NetIsHost()&&!Require(a,q.body==0&&q.level==0&&q.threshold==200&&q.freeze>=397&&q.freeze<=400&&q.slow>=1397&&q.slow<=1400&&body&&body->mAnimRate>0,"IceCaltrop lethal postdamage cold commit froze death animation or lost threshold"))return false;
                    s.first[2]=true;s.lethalTick=b->mMainCounter;s.lethalAnim=body?body->mAnimTime:0;
                    NetTrace("SMOKE_FIC251_FIRST role=%s index=2 id=%u body=%d helm=%d shield=%d level=%d threshold=%d slow=%d freeze=%d rate=%.9g hostExact=%d",Role(a),s.targets[2],q.body,q.helm,q.shield,q.level,q.threshold,q.slow,q.freeze,body?body->mAnimRate:-1,a->NetIsHost()?1:0);
                }
                if(body&&body->mAnimRate>0&&body->mAnimTime!=s.lethalAnim)s.lethalProgress=true;
            }
            else if(!z&&!s.lethalRetired)
            {
                if(a->NetIsHost()&&!Require(a,s.first[2]&&s.lethalProgress&&b->mMainCounter-s.lethalTick<400,"IceCaltrop lethal actor failed natural animated retirement within4active seconds"))return false;
                s.first[2]=true;s.lethalRetired=true;s.lethalRetiredTick=b->mMainCounter;
                NetTrace("SMOKE_FIC251_RETIRED role=%s id=%u firstTick=%d retiredTick=%d progress=%d hostBounded=%d canonicalOmission=1",Role(a),s.targets[2],s.lethalTick,s.lethalRetiredTick,s.lethalProgress?1:0,a->NetIsHost()?1:0);
            }
            continue;
        }
        if(!Require(a,z&&!z->IsDeadOrDying(),"IceCaltrop healthy controlled actor unexpectedly retired"))return false;const auto q=Read(z);
        if(i>=3)
        {
            if(q.body!=500||q.helm||q.shield||q.level||q.slow||q.freeze)NetTrace("SMOKE_FIC251_CONTROL_DAMAGE role=%s index=%d id=%u body=%d level=%d slow=%d freeze=%d",Role(a),i,z->mNetId,q.body,q.level,q.slow,q.freeze);
            if(!Require(a,q.body==500&&!q.helm&&!q.shield&&!q.level&&!q.slow&&!q.freeze,"IceCaltrop adjacent/outside control changed"))return false;
        }
        else
        {
            const int loss=(i==0?100-q.helm:0)+500-q.body;
            if(!Require(a,loss>=0&&loss%20==0&&q.shield==(i==0?200:0),"IceCaltrop damage not20 shield-bypass increments"))return false;
            if(loss&&!s.first[i])
            {
                if(a->NetIsHost()&&!Require(a,loss==20&&q.body==(i==0?500:480)&&q.helm==(i==0?80:0)&&q.level==10&&q.threshold==100&&q.slow>=997&&q.slow<=1000&&(i==0?q.freeze==0:q.freeze>0),"IceCaltrop first natural strike damage/armor/cold order differs"))return false;
                s.first[i]=true;NetTrace("SMOKE_FIC251_FIRST role=%s index=%d id=%u body=%d helm=%d shield=%d level=%d threshold=%d slow=%d freeze=%d rate=0 hostExact=%d",Role(a),i,z->mNetId,q.body,q.helm,q.shield,q.level,q.threshold,q.slow,q.freeze,a->NetIsHost()?1:0);
            }
        }
    }
    return true;
}
bool PauseProof(LawnApp* a)
{
    auto* b=a->mBoard;if(!b->mPaused)return false;
    if(!s.freezeSeen)
    {
        auto* frozen=Target(b,s.targets[1]);if(!Require(a,frozen&&frozen->mIceTrapCounter>0,"IceCaltrop freeze pause missed positive native timer"))return false;
        s.freezeSeen=true;s.pauseSince=GetTickCount();s.pauseSamples=Samples(a);
        for(int i=0;i<2;++i)s.paused[i]=Read(Target(b,s.targets[i]));
    }
    for(int i=0;i<2;++i)
    {
        auto* z=Target(b,s.targets[i]);if(!Require(a,z!=nullptr,"IceCaltrop paused target missing"))return false;const auto now=Read(z),old=s.paused[i];
        if(!Require(a,now.body==old.body&&now.helm==old.helm&&now.shield==old.shield&&now.level==old.level&&now.threshold==old.threshold&&now.slow==old.slow&&now.freeze==old.freeze,"IceCaltrop paused native cold/health changed"))return false;
    }
    ++s.pauseChecks;
    if(s.pauseChecks<8||Samples(a)-s.pauseSamples<2||GetTickCount()-s.pauseSince<600||!Capture(a,"paused"))return false;
    if(!s.freezeReady){s.freezeReady=true;NetTrace("SMOKE_FIC251_PAUSE role=%s slow=%d freeze=%d checks=%d snapshots=%u elapsedMs=%u exactOwnCounters=1",Role(a),s.paused[1].slow,s.paused[1].freeze,s.pauseChecks,Samples(a)-s.pauseSamples,GetTickCount()-s.pauseSince);}
    return Barrier(a,"FROZEN_PAUSED");
}
}
namespace LawnNet
{
bool NetSmokeUpdateFusionIceCaltrop(LawnApp* a)
{
    const char* enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_ICE_CALTROP"),*native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
    if(!a||!enabled||std::strcmp(enabled,"1")||!native||std::strcmp(native,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
    if(a->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();
    if(GetTickCount()-s.since>45000U){char why[140];sprintf_s(why,"IceCaltrop deadline stage=%d role=%s first=%d,%d,%d retired=%d",s.stage,Role(a),s.first[0]?1:0,s.first[1]?1:0,s.first[2]?1:0,s.lethalRetired?1:0);a->CoopSmokeFail(why);return true;}
    if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
    if(s.stage==0)
    {
        if(a->NetIsHost()&&!s.configured){a->mNetManager->SetDifficulty(1);s.configured=true;}
        if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
        if(a->NetIsClient()){Advance(a,1);Send(a,"FIC251_BEGIN");}
        else if(Has(a,1,"FIC251_BEGIN")){Advance(a,1);std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}
        return true;
    }
    if(!a->mBoard||!IsNativeFusionBoard(a->mBoard)){if(s.stage==1&&a->NetIsClient())Send(a,"FIC251_BEGIN");return true;}
    auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);auto* chooser=a->mSeedChooserScreen;const int slot=Slot(a);
    if(!Require(a,ctx&&ctx->validation&&!a->mFusionRuntime&&b->GetColumnCount()==10&&b->mBackground==BACKGROUND_1_DAY,"IceCaltrop lost same native10x5/day Board"))return true;
    if(!s.epoch)s.epoch=ctx->epoch;if(!Require(a,ctx->epoch==s.epoch,"IceCaltrop activity epoch changed"))return true;
    const auto& bank=ctx->selection.State();
    // Only fixture ground-track pose is frozen. Cold/death callbacks keep their
    // real timers, and dying actors are deliberately never assigned rate0.
    if(a->NetIsHost()&&s.setup)for(auto id:s.targets)if(auto* z=Target(b,id))if(!z->IsDeadOrDying())z->ApplyAnimRate(0);
    if(!Observe(a))return true;
    if(s.stage==1)
    {
        if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||!b->mCutScene->mSeedChoosing||chooser->mSeedsInFlight)return true;
        if(!Require(a,bank.activeMask==3&&bank.targetCount==10&&bank.quotas[0]==5&&bank.quotas[1]==5,"IceCaltrop real chooser quota differs"))return true;
        if(!Capture(a,"chooser"))return true;
        std::vector<int> choices;for(size_t i=0;i<ctx->eligible.size();++i){const auto& e=ctx->eligible[i];if(slot==0?(!e.isExtra&&e.kind!=10):(e.isExtra||e.kind==10))choices.push_back(int(i));}
        auto priority=[&](int i){const auto& e=ctx->eligible[i];return slot==0?(e.kind==17?0:10+e.kind):(!e.isExtra?0:10+e.kind);};
        std::stable_sort(choices.begin(),choices.end(),[&](int x,int y){return priority(x)<priority(y);});
        if(!Require(a,choices.size()>=5&&ctx->eligible[choices[0]].kind==(slot?10:17)&&!ctx->eligible[choices[0]].isExtra,"IceCaltrop actual required base card missing"))return true;
        int own=0;for(const auto& p:bank.packets)own+=p.owner==slot;
        if(own>s.selectedOwn){s.selectedOwn=own;NetTrace("SMOKE_FIC251_CHOICE role=%s own=%d dense=%d acknowledged=1",Role(a),own,s.choiceDense);}
        if(own<s.waitingOwn)return true;
        if(own<5){if(Choice(a,chooser,choices[own]))s.waitingOwn=own+1;return true;}
        if(bank.packets.size()!=10)return true;
        for(int i=0;i<2;++i){auto* p=Packet(b,i,i?10:17);if(!Require(a,p&&p->mNetId,"IceCaltrop actual selected packet absent"))return true;s.packets[i]=p->mNetId;}
        int rank=-1;for(int i=0;i<b->mSeedBank->mNumPackets;++i){const int r=NetSeedBankOwnerRank(slot,b->mSeedBank->mSeedPackets[i].mNetOwnerSlot);if(!Require(a,r>=rank,"IceCaltrop native bank lost own-first order"))return true;rank=r;}
        NetTrace("SMOKE_FIC251_POOL role=%s eligible=%zu target=10 quotas=5,5 ownFirst=1 source17=1 source10=1",Role(a),ctx->eligible.size());Advance(a,2);
    }
    else if(s.stage==2){if(bank.phase!=FusionBankPhase::Choosing)Advance(a,3);else if(chooser&&!chooser->mSeedsInFlight&&Ready(a,chooser->mStartButton))Advance(a,3);}
    else if(s.stage==3)
    {
        if(bank.phase==FusionBankPhase::Intro){if(!s.introFirst)s.introFirst=GetTickCount();s.introLast=GetTickCount();++s.introChecks;return true;}
        if(bank.phase!=FusionBankPhase::Playing||a->mGameScene!=SCENE_PLAYING||chooser)return true;
        if(!Require(a,s.introChecks>=10&&s.introLast-s.introFirst>=1000&&b->mSunMoney==500,"IceCaltrop bypassed full native intro/fresh wallet"))return true;
        s.playingTick=b->mMainCounter;Advance(a,4);
    }
    else if(s.stage==4)
    {
        if(b->mMainCounter-s.playingTick<2||b->mPlants.mSize||b->mZombies.mSize||!Capture(a,"playing")||!Barrier(a,"PLAYING"))return true;
        if(a->NetIsHost()&&!s.ambient){b->mSunCountDown=b->mZombieCountDown=b->mZombieCountDownStart=100000;s.ambient=true;NetTrace("SMOKE_FIC251_AMBIENT initialOnly=1 initialSun=500 totalCost=175 finalExpected=325 sunWrites=0");}
        if(Samples(a)>=8)Advance(a,5);
    }
    else if(s.stage==5){if(Purchase(a,0))Advance(a,6);}
    else if(s.stage==6){if(Purchase(a,1)&&Capture(a,"idle"))Advance(a,7);}
    else if(s.stage==7)
    {
        if(a->NetIsHost()&&!s.initialPause){s.initialPause=Escape(a);return true;}
        if(b->mPaused)Advance(a,8);
    }
    else if(s.stage==8)
    {
        if(b->mPaused)
        {
            if(!Setup(a))return true;if(a->NetIsHost()&&!Send(a,s.ids))return true;
            if(!Baseline(a)||!Barrier(a,"TARGETS"))return true;
            if(a->NetIsHost()&&!s.initialResume){s.initialResume=Escape(a);return true;}
        }
        if(s.baseline&&!b->mPaused&&!a->GetDialogCount())Advance(a,9);
    }
    else if(s.stage==9)
    {
        if(!b->mPaused){s.resumedOne=true;Send(a,Tag("RESUMED1"));}
        const bool bothResumed=Has(a,0,Tag("RESUMED1"))&&Has(a,1,Tag("RESUMED1"));
        if(a->NetIsHost()&&bothResumed&&s.first[0]&&s.first[1]&&s.first[2]&&s.frames.contains("fusion_icecaltrop_attack")&&!s.freezePause)
        {s.freezePause=Escape(a);Advance(a,10);return true;}
        if(b->mPaused&&s.resumedOne)Advance(a,10);
    }
    else if(s.stage==10)
    {
        if(b->mPaused)
        {
            if(!PauseProof(a))return true;
            if(a->NetIsHost()&&!s.freezeResume){s.freezeResume=Escape(a);return true;}
        }
        else if(s.freezeReady&&!a->GetDialogCount())Advance(a,11);
    }
    else if(s.stage==11)
    {
        if(!b->mPaused)
        {
            if(!s.resumedTwo){s.resumedTwo=true;NetTrace("SMOKE_FIC251_RESUMED role=%s observedUnpaused=1 beforeFinalRepause=1",Role(a));}
            Send(a,Tag("RESUMED2"));
        }
        const bool bothResumed=Has(a,0,Tag("RESUMED2"))&&Has(a,1,Tag("RESUMED2"));
        if(s.lethalRetired&&s.first[0]&&s.first[1]&&s.frames.contains("fusion_icecaltrop_attack"))Send(a,Tag("RETIRED"));
        const bool bothRetired=Has(a,0,Tag("RETIRED"))&&Has(a,1,Tag("RETIRED"));
        if(a->NetIsHost()&&bothResumed&&bothRetired&&!s.finalPause)
        {s.finalPause=Escape(a);NetTrace("SMOKE_FIC251_FINAL_PAUSE bothResumedACK=1 bothRetiredACK=1 ownedSDL=1");Advance(a,12);return true;}
        if(b->mPaused&&s.resumedTwo&&bothResumed)Advance(a,12);
    }
    else if(s.stage==12)
    {
        if(!b->mPaused)return true;
        if(!s.finalSeen){s.finalSeen=true;s.finalSamples=Samples(a);return true;}
        if(Samples(a)-s.finalSamples<3||!Business(a,2)||!s.lethalRetired)return true;
        if(!s.finalLogged)
        {
            for(int i=0;i<5;++i){auto* z=Target(b,s.targets[i]);const auto q=z?Read(z):Sample{};NetTrace("SMOKE_FIC251_OUTCOME role=%s index=%d id=%u alive=%d body=%d helm=%d shield=%d level=%d threshold=%d slow=%d freeze=%d",Role(a),i,s.targets[i],z?1:0,q.body,q.helm,q.shield,q.level,q.threshold,q.slow,q.freeze);}
            s.finalLogged=true;NetTrace("SMOKE_FIC251_FINAL role=%s kind=1180 id=%u token=%u sun=325 price17=100 price10=75 CD17=750 CD10=5000 postSetupHPWrites=0 postSetupStatusWrites=0 plantTimerWrites=0 nativeBoard=1 secondWorld=0",Role(a),s.plant.id,s.plant.token);
            s.finalSamples=Samples(a);
        }
        if(Samples(a)-s.finalSamples<3||!Capture(a,"complete")||!Barrier(a,"DONE"))return true;
        NetTrace("SMOKE_FIC251_COMPLETE role=%s source1180=1 realPurchases=2 initialSun=500 totalCost=175 finalSun=325 damage20NoFrozenMultiplier=1 shieldBypass=1 postDamageCold10=1 lethalRetirement=1 pause=1 resumedACK=1 sourceZombieAIUnported=1",Role(a));Advance(a,13);a->CoopSmokeWriteResult(true,nullptr);
    }
    return true;
}
}
