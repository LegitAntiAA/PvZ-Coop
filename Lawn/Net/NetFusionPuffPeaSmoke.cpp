#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../Coin.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionWire.h"
#include "../Fusion/FusionNativeGloveUI.h"
#include "../Fusion/FusionNativeCherryShooter.h"
#include "../Fusion/FusionShooterTiming.h"
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
constexpr int kRow=2;
struct Ref {uint16_t id=0;uint32_t token=0;};
struct Volley {int tick=0;std::vector<int> delays;unsigned fired=0;};
struct Shot {int tick=0,age=0;float x=0;bool age75=false,age76=false,retired=false;};
struct Combat
{
    int kind=0,baseHP=5000,hits=0,previousCounter=0,previousTick=0;
    bool drained=false,parked=false,clientShot=false,longHit=false;
    std::vector<Volley> volleys;std::map<uint16_t,Shot> shots;
};
struct State
{
    int stage=0,waitingOwn=0,money=500,puffUses=0,normalUses=0,extraUses=0,playingTick=-1;
    int moveTick=0,moveLaunch=0,moveHP=0,moveCD=0,moveDelta=0;NativePlantData moveData{};
    uint32_t since=0,lastChat=0,introStart=0,introLast=0,introChecks=0,pickSamples=0,purchaseSamples=0;
    bool configured=false,clicked=false,placed=false,observed=false,toolClicked=false,setup=false;
    Ref current{},parent{},doublePea{};std::array<uint16_t,4> targets{};Combat combat{};
    std::map<std::string,int> firstDraw;std::set<std::string> frames;
} s;
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool ok,const char* why){if(!ok)a->CoopSmokeFail(why);return ok;}
void Advance(LawnApp* a,int stage)
{
    s.stage=stage;s.since=GetTickCount();s.clicked=s.placed=s.observed=s.toolClicked=false;
    LawnNet::NetTrace("SMOKE_FUSION_PUFF_PEA_STAGE role=%s stage=%d",Role(a),stage);
}
bool Has(LawnApp* a,int who,const std::string& text)
{for(const auto& l:a->mNetManager->mChatLines)if(l.mSlot==who&&l.mText==text)return true;return false;}
bool Send(LawnApp* a,const std::string& text)
{
    if(Has(a,Slot(a),text))return true;if(s.lastChat&&GetTickCount()-s.lastChat<800)return false;
    s.lastChat=GetTickCount();a->mNetManager->SendChat(text);return Has(a,Slot(a),text);
}
bool Barrier(LawnApp* a,const std::string& text){Send(a,text);return Has(a,0,text)&&Has(a,1,text);}
bool Capture(LawnApp* a,const char* name)
{
    if(s.frames.contains(name))return true;if(!s.firstDraw.contains(name)){s.firstDraw[name]=a->mDrawCount;return false;}
    if(a->mDrawCount-s.firstDraw[name]<2)return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name),"PuffPea native renderer capture failed"))return false;
    s.frames.insert(name);LawnNet::NetTrace("SMOKE_FUSION_PUFF_PEA_RENDER role=%s frame=%s freshDraws=%d",Role(a),name,a->mDrawCount-s.firstDraw[name]);return true;
}
bool Click(LawnApp* a,float x,float y,uint8_t button=SDL_BUTTON_LEFT,bool click=true)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    if(!click)return Require(a,SDL_PushEvent(&m),"PuffPea scoped motion failed");
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=button;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;
    return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"PuffPea scoped SDL click failed");
}
bool BoardClick(LawnApp* a,float x,float y,uint8_t button=SDL_BUTTON_LEFT,bool click=true)
{return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY,button,click);}
bool Cell(LawnApp* a,int col,int row=kRow,uint8_t button=SDL_BUTTON_LEFT,bool click=true)
{return BoardClick(a,float(a->mBoard->GetCellCenterX(col,row)),float(a->mBoard->GridToPixelY(col,row)+50),button,click);}
bool Ready(LawnApp* a,GameButton* b)
{
    if(!b||b->mDisabled||b->mBtnNoDraw)return false;float x=b->mX+b->mWidth*.5f,y=b->mY+b->mHeight*.5f;
    if(!b->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"PuffPea Ready hover failed");return false;}
    LawnNet::NetTrace("SMOKE_FUSION_PUFF_PEA_READY role=%s nativeSDL=1 hoverObserved=1",Role(a));return Click(a,x,y);
}
Plant* ByRef(Board* b,Ref r)
{Plant* p=nullptr;while(b->mPlants.IterateNext(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&p->mIsOnBoard&&p->mNetId==r.id&&d&&d->instanceToken==r.token)return p;}return nullptr;}
Plant* At(Board* b,int col)
{Plant* p=nullptr;while(b->mPlants.IterateNext(p))if(!p->mDead&&p->mIsOnBoard&&p->mPlantCol==col&&p->mRow==kRow)return p;return nullptr;}
Ref Identity(Plant* p){const auto* d=GetNativePlantData(p);return p&&d?Ref{p->mNetId,d->instanceToken}:Ref{};}
SeedPacket* Packet(Board* b,int who,int kind,bool extra)
{for(int i=0;i<b->mSeedBank->mNumPackets;++i){auto* p=&b->mSeedBank->mSeedPackets[i];const auto* d=GetNativePacketData(p);if(d&&d->owner==who&&d->kind==kind&&d->isExtra==extra)return p;}return nullptr;}
bool HeldPacket(Board* b,SeedPacket* p)
{const auto* c=b->mCursorObject;return c&&p&&c->mCursorType==CURSOR_TYPE_PLANT_FROM_BANK&&c->mSeedBankIndex>=0&&c->mSeedBankIndex<b->mSeedBank->mNumPackets&&b->mSeedBank->mSeedPackets[c->mSeedBankIndex].mNetId==p->mNetId;}
bool PickPacket(LawnApp* a,SeedPacket* p)
{if(!Require(a,p&&p->mActive&&!p->mRefreshing,"PuffPea real packet not ready"))return false;auto* b=a->mBoard->mSeedBank;return BoardClick(a,float(b->mX+p->mX+p->mOffsetX+p->mWidth/2),float(b->mY+p->mY+p->mHeight/2));}
bool Business(LawnApp* a)
{
    auto* b=a->mBoard;auto* puff=Packet(b,0,6,false);auto* pea=Packet(b,1,0,false);auto* extra=Packet(b,0,0,true);
    if(!Require(a,puff&&pea&&extra&&b->mSunMoney==s.money&&puff->mRefreshTime==(s.puffUses?750:0)&&pea->mRefreshTime==(s.normalUses?750:0)&&extra->mRefreshTime==(s.extraUses?750:0)&&
        (!a->NetIsHost()||(puff->mTimesUsed==s.puffUses&&pea->mTimesUsed==s.normalUses&&extra->mTimesUsed==s.extraUses)),"PuffPea real prices/packet use/cooldowns drifted"))return false;
    Coin* c=nullptr;while(b->mCoins.IterateNext(c))if(!Require(a,c->mDead,"PuffPea unexpectedly generated sun/Coin"))return false;return true;
}
bool Purchase(LawnApp* a,int buyer,int ingredient,bool extra,int col,int result,int before,int after,int first,int second,Ref& out,const char* gate)
{
    auto* b=a->mBoard;auto* card=Packet(b,buyer,ingredient,extra);
    if(Slot(a)==buyer&&!s.clicked)
    {if(!card||!card->mActive||card->mRefreshing)return false;s.pickSamples=Samples(a);if(PickPacket(a,card))s.clicked=true;return false;}
    if(Slot(a)==buyer&&!s.placed)
    {if(!HeldPacket(b,card)||(a->NetIsClient()&&Samples(a)<=s.pickSamples))return false;if(!Require(a,b->mSunMoney==before,"PuffPea pickup charged before native placement"))return false;s.placed=Cell(a,col);return false;}
    if(!s.observed)
    {
        auto* p=At(b,col);const auto* d=GetNativePlantData(p);if(!d||d->kind!=result)return false;
        if(!Require(a,card&&d->instanceToken&&d->packetId==card->mNetId&&d->owner==buyer&&!d->randomOrigin&&d->firstParent==first&&d->secondParent==second&&p->mPlantHealth==300&&!p->mIsAsleep&&b->mSunMoney==after,"PuffPea purchase/provenance/awake state differs"))return false;
        out=Identity(p);s.money=after;if(ingredient==6)++s.puffUses;else if(extra)++s.extraUses;else ++s.normalUses;s.purchaseSamples=Samples(a);s.observed=true;
    }
    if(!Business(a)||!Barrier(a,gate))return false;
    LawnNet::NetTrace("SMOKE_FUSION_PUFF_PEA_PURCHASE role=%s kind=%d id=%u token=%u packet=%u owner=%d extra=%d parents=%d,%d sun=%d sourceCD=750 nativeSDL=1",Role(a),result,out.id,out.token,card->mNetId,buyer,extra?1:0,first,second,after);return true;
}
bool Pickup(LawnApp* a,Ref source)
{
    if(Slot(a)==1)
    {
        if(!s.toolClicked){Sexy::Rect rect=a->mBoard->GetShovelButtonRect();a->mBoard->GetZenButtonRect(OBJECT_TYPE_GLOVE,rect);if(!Require(a,rect.mWidth>0&&rect.mHeight>0,"PuffPea native glove tool rectangle absent"))return false;s.toolClicked=BoardClick(a,float(rect.mX+rect.mWidth/2),float(rect.mY+rect.mHeight/2));return false;}
        if(!s.clicked){if(a->mBoard->mCursorObject->mCursorType!=CURSOR_TYPE_GLOVE)return false;auto* p=ByRef(a->mBoard,source);if(!Require(a,p,"PuffPea glove source vanished"))return false;s.clicked=Cell(a,p->mPlantCol);return false;}
    }
    const auto* ctx=GetNativeContext(a->mBoard);const auto& held=ctx->gloveHolds[1];
    return held.plantId==source.id&&held.instanceToken==source.token&&GetNativeGloveHeldPlant(a->mBoard,1)==ByRef(a->mBoard,source)&&(Slot(a)!=1||a->mBoard->mCursorObject->mCursorType==CURSOR_TYPE_PLANT_FROM_GLOVE);
}
bool Move(LawnApp* a,int kind,int from,int to)
{
    auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);auto* p=ByRef(b,s.current);const auto* d=GetNativePlantData(p);
    if(!Require(a,p&&d&&d->kind==kind&&p->mPlantHealth==300&&!p->mIsAsleep,"PuffPea move lost real source actor"))return false;
    const std::string gate="FP80_MOVE_READY_"+std::to_string(kind);
    if(p->mPlantCol==from)
    {
        if(ctx->gloveCooldownTicks&&!ctx->gloveHolds[1].plantId)return false;
        if(!Business(a)||!Pickup(a,s.current))return false;
        if(a->NetIsHost())
        {
            s.moveData=*d;s.moveTick=b->mMainCounter;s.moveLaunch=p->mLaunchCounter;s.moveHP=p->mPlantHealth;
            if(p->mLaunchCounter>45&&!Send(a,gate))return false;
        }
        if(!Has(a,0,gate))return false;
        if(Slot(a)==1&&!s.placed){s.placed=Cell(a,to);return false;}
        return false;
    }
    if(!Require(a,p->mPlantCol==to&&p->mRow==kRow&&!ctx->gloveHolds[1].plantId&&ctx->gloveCooldownTicks>0&&ctx->gloveCooldownTicks<=334,"PuffPea ordinary move did not preserve actor/start334cooldown"))return false;
    if(!s.observed)
    {
        s.moveDelta=a->NetIsHost()?b->mMainCounter-s.moveTick:0;s.moveCD=ctx->gloveCooldownTicks;
        if(a->NetIsHost()&&!Require(a,s.moveDelta>=0&&s.moveDelta<s.moveLaunch&&p->mLaunchCounter==s.moveLaunch-s.moveDelta&&p->mShootingCounter==0&&p->mPlantHealth==s.moveHP&&
            d->instanceToken==s.moveData.instanceToken&&d->packetId==s.moveData.packetId&&d->owner==s.moveData.owner&&d->firstParent==s.moveData.firstParent&&d->secondParent==s.moveData.secondParent&&d->animationSpeed==s.moveData.animationSpeed&&s.moveCD>=334-s.moveDelta,
            "PuffPea move reinitialized counters/provenance or used wrong family cooldown"))return false;s.observed=true;
    }
    const std::string frame="fusion_puff_pea_moved"+std::to_string(kind),done="FP80_MOVED_"+std::to_string(kind);
    if(!Business(a)||!Capture(a,frame.c_str())||!Barrier(a,done))return false;
    LawnNet::NetTrace("SMOKE_FUSION_PUFF_PEA_MOVE role=%s kind=%d id=%u token=%u from=%d to=%d sameInstance=1 HP=300 countersPreserved=1 provenancePreserved=1 expectedCD=334 observedCD=%d deltaTicks=%d nativeClientGlove=1",Role(a),kind,s.current.id,s.current.token,from,to,s.moveCD,s.moveDelta);return true;
}
Zombie* Target(Board* b,uint16_t id){Zombie* z=nullptr;while(b->mZombies.IterateNext(z))if(z->mNetId==id)return z;return nullptr;}
Projectile* LiveShot(Board* b,uint16_t id){Projectile* p=nullptr;while(b->mProjectiles.IterateNext(p))if(!p->mDead&&p->mNetId==id)return p;return nullptr;}
int LiveShots(Board* b){int n=0;Projectile* p=nullptr;while(b->mProjectiles.IterateNext(p))n+=!p->mDead;return n;}
void SetX(Zombie* z,float x){z->mPosX=x;z->mX=int(x);}
void Park(Board* b)
{for(int i=0;i<4;++i)SetX(Target(b,s.targets[i]),i==2?650.0f:1100.0f+100*i);}
bool Setup(LawnApp* a)
{
    for(int i=0;i<4;++i)
    {
        auto* z=a->mBoard->AddZombieInRow(ZOMBIE_NORMAL,i==3?kRow+1:kRow,0);auto* body=z?a->ReanimationTryToGet(z->mBodyReanimID):nullptr;const float rate=body?body->mAnimRate:0;
        if(!Require(a,z&&z->mNetId&&body&&std::isfinite(rate)&&rate>0,"PuffPea real control initialization failed"))return false;
        z->mBodyHealth=z->mBodyMaxHealth=5000;z->mMindControlled=i==2;z->mVelX=0;z->SetAnimRate(rate);z->ApplyAnimRate(0);s.targets[i]=z->mNetId;
        LawnNet::NetTrace("SMOKE_FUSION_PUFF_PEA_TARGET index=%d id=%u HP=5000 MC=%d adjacent=%d boundedNativeTarget=1 initialHPOnly=1",i,z->mNetId,i==2?1:0,i==3?1:0);
    }
    Park(a->mBoard);s.setup=true;return true;
}
std::string Ids(){std::ostringstream out;out<<"FP80_IDS";for(auto id:s.targets)out<<' '<<id;return out.str();}
bool ReadIds(LawnApp* a)
{
    if(a->NetIsHost())return s.setup;for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==0&&line.mText.rfind("FP80_IDS ",0)==0)
    {std::istringstream in(line.mText.substr(9));std::set<unsigned> seen;for(auto& id:s.targets){unsigned value=0;if(!Require(a,bool(in>>value)&&value&&value<=65535&&seen.insert(value).second,"PuffPea control IDs invalid"))return false;id=uint16_t(value);}std::string extra;return Require(a,!(in>>extra),"PuffPea control IDs trailing data");}return false;
}
bool BeginCombat(LawnApp* a,int kind)
{
    auto* b=a->mBoard;auto* p=ByRef(b,s.current);const auto* d=GetNativePlantData(p);const auto* profile=FindFusionShooterTimingProfile(uint16_t(kind));
    if(!Require(a,p&&d&&d->kind==kind&&profile&&profile->projectileKind==10&&profile->eventClipSeconds.size()==(kind==1018?1:kind==1019?2:4)&&
        !std::strcmp(profile->visualState,kind==1065?"shoot_g":"shoot")&&!LiveShots(b),"PuffPea source profile/quiet upgrade boundary differs"))return false;
    s.combat={};s.combat.kind=kind;s.combat.baseHP=Target(b,s.targets[0])->mBodyHealth;s.combat.previousCounter=p->mShootingCounter;s.combat.previousTick=b->mMainCounter;
    if(a->NetIsHost())
    {
        float gx=0,gy=0;const float muzzle=kind==1018?.2695f:kind==1019?.27025f:.28802f;if(!Require(a,p->GetShadowCenter(gx,gy),"PuffPea native shadow anchor missing"))return false;
        auto* front=Target(b,s.targets[0]);const auto rect=front->GetZombieRect();SetX(front,front->mPosX+gx+muzzle*60+256.0f-rect.mX);
        auto* rear=Target(b,s.targets[1]);SetX(rear,front->mPosX+110.0f);auto* mc=Target(b,s.targets[2]);SetX(mc,front->mPosX-128.0f);SetX(Target(b,s.targets[3]),front->mPosX);
        LawnNet::NetTrace("SMOKE_FUSION_PUFF_PEA_RANGE kind=%d nativeShadowX=%.3f muzzleX=%.3f targetLeft=%d distance=256 beyond240=1 positionOnly=1",kind,gx,gx+muzzle*60,front->GetZombieRect().mX);
    }
    return true;
}
bool ObserveCombat(LawnApp* a)
{
    if(!Business(a))return false;auto* b=a->mBoard;auto& q=s.combat;auto* p=ByRef(b,s.current);const auto* d=GetNativePlantData(p);const auto* profile=FindFusionShooterTimingProfile(uint16_t(q.kind));
    if(!Require(a,p&&d&&d->kind==q.kind&&p->mPlantHealth==300&&!p->mIsAsleep,"PuffPea combat actor/HP/awake state changed"))return false;
    const size_t count=profile->eventClipSeconds.size();const unsigned mask=(1U<<count)-1;
    if(a->NetIsHost())
    {
        if(p->mShootingCounter>q.previousCounter)
        {
            std::vector<int> deadlines;for(double event:profile->eventClipSeconds)deadlines.push_back(int(std::ceil(event/(profile->stateSpeed*double(d->animationSpeed))*100)));
            const int initial=deadlines.back()+1,dt=b->mMainCounter-q.previousTick;
            if(!Require(a,q.volleys.size()<2&&(q.volleys.empty()||q.volleys.back().fired==mask)&&p->mShootingCounter<=initial&&initial-p->mShootingCounter<=(std::max)(1,dt),"PuffPea natural profile counter/volley count differs"))return false;
            const int tick=b->mMainCounter-(initial-p->mShootingCounter);q.volleys.push_back({tick,deadlines,0});std::ostringstream list;for(size_t i=0;i<count;++i){if(i)list<<',';list<<deadlines[i];}
            LawnNet::NetTrace("SMOKE_FUSION_PUFF_PEA_TRIGGER kind=%d volley=%zu tick=%d initial=%d events=%zu delays=%s speed=%.9g stateSpeed=%.17g state=%s naturalClock=1",q.kind,q.volleys.size(),tick,initial,count,list.str().c_str(),d->animationSpeed,profile->stateSpeed,profile->visualState);
        }
        q.previousCounter=p->mShootingCounter;q.previousTick=b->mMainCounter;
    }
    Projectile* shot=nullptr;while(b->mProjectiles.IterateNext(shot))
    {
        if(shot->mDead)continue;const auto* meta=GetNativeProjectileData(shot);Sexy::Rect rect;float cx=0,cy=0;
        if(!Require(a,meta&&meta->kind==10&&meta->damage==20&&!meta->sunOnHit&&shot->mProjectileType==PROJECTILE_PEA&&GetNativeTimedPeaProjectileRect(shot,rect)&&rect.mWidth>0&&rect.mHeight>0&&GetNativeTimedPeaProjectileCenter(shot,cx,cy),"PuffPea payload/collider/native actor differs"))return false;
        if(a->NetIsHost())
        {
            if(!Require(a,std::abs(shot->mVelX-3.2f)<.0001f,"PuffPea native velocity differs"))return false;auto old=q.shots.find(shot->mNetId);
            if(old==q.shots.end())
            {
                const int birth=b->mMainCounter-shot->mProjectileAge+1;int volley=-1,event=-1;for(size_t v=0;v<q.volleys.size();++v)for(size_t e=0;e<count;++e)if(q.volleys[v].tick+q.volleys[v].delays[e]==birth){volley=int(v);event=int(e);}
                if(!Require(a,volley>=0&&event>=0&&!(q.volleys[volley].fired&(1U<<event))&&q.shots.size()<2*count,"PuffPea duplicate/early/late projectile event"))return false;
                q.volleys[volley].fired|=1U<<event;old=q.shots.emplace(shot->mNetId,Shot{b->mMainCounter,shot->mProjectileAge,shot->mPosX}).first;
                LawnNet::NetTrace("SMOKE_FUSION_PUFF_PEA_SHOT kind=%d id=%u volley=%d event=%d born=%d trigger=%d delay=%d payload=10 damage=20 sun=0 vx=3.2 nativeActor=1",q.kind,shot->mNetId,volley+1,event+1,birth,q.volleys[volley].tick,q.volleys[volley].delays[event]);
            }
            auto& state=old->second;const int dt=b->mMainCounter-state.tick;
            if(!Require(a,!state.retired&&std::abs(shot->mPosX-state.x-3.2f*dt)<.08f,"PuffPea actual displacement/reused projectile differs"))return false;
            state.tick=b->mMainCounter;state.x=shot->mPosX;state.age=shot->mProjectileAge;
            if(shot->mProjectileAge==75&&!state.age75){state.age75=true;LawnNet::NetTrace("SMOKE_FUSION_PUFF_PEA_AGE kind=%d id=%u age=75 alive=1 x=%.3f",q.kind,shot->mNetId,cx);}
            if(shot->mProjectileAge==76&&!state.age76){state.age76=true;LawnNet::NetTrace("SMOKE_FUSION_PUFF_PEA_AGE kind=%d id=%u age=76 alive=1 x=%.3f",q.kind,shot->mNetId,cx);}
        }
        else q.clientShot=true;
        const std::string frame="fusion_puff_pea_projectile"+std::to_string(q.kind);Capture(a,frame.c_str());
    }
    auto* front=Target(b,s.targets[0]);if(!front)return true;const int loss=q.baseHP-front->mBodyHealth;
    if(!Require(a,loss>=0&&loss%20==0&&loss/20<=int(2*count)&&Target(b,s.targets[1])->mBodyHealth==5000&&Target(b,s.targets[2])->mBodyHealth==5000&&Target(b,s.targets[3])->mBodyHealth==5000,"PuffPea single-hit/noPierce/noAoE/MC controls changed"))return false;q.hits=loss/20;
    if(a->NetIsHost())
    {
        int retired=0;for(auto& [id,state]:q.shots)if(!LiveShot(b,id))
        {
            ++retired;if(!state.retired&&state.age75&&state.age76&&!q.longHit){q.longHit=true;LawnNet::NetTrace("SMOKE_FUSION_PUFF_PEA_LONG_HIT kind=%d id=%u lastAliveAge=%d survived75and76=1 frontHP=%d oneRetirementOne20Hit=1",q.kind,id,state.age,front->mBodyHealth);}state.retired=true;
        }
        if(!Require(a,retired==q.hits,"PuffPea projectile retired without exactly one real20-damage hit"))return false;
        if(q.hits==int(count)&&!q.drained)
        {
            // Keep the whole first volley long. Move this target closer only
            // after those hits and before the second volley's first event.
            if(!Require(a,q.shots.size()==count&&q.longHit,"PuffPea position-only drain missed the quiet inter-volley boundary"))return false;
            float gx=0,gy=0;p->GetShadowCenter(gx,gy);const float muzzle=q.kind==1018?.2695f:q.kind==1019?.27025f:.28802f;
            const auto rect=front->GetZombieRect();SetX(front,front->mPosX+gx+muzzle*60+60.0f-rect.mX);SetX(Target(b,s.targets[1]),1200);SetX(Target(b,s.targets[2]),650);SetX(Target(b,s.targets[3]),1300);q.drained=true;
            LawnNet::NetTrace("SMOKE_FUSION_PUFF_PEA_DRAIN kind=%d afterFirstFullVolleyHit=1 beforeSecondFirstShot=1 targetPositionOnly=1 noHPWrites=1 noClockWrites=1",q.kind);
        }
        if(q.hits==int(2*count)&&!q.parked)
        {
            if(!Require(a,q.longHit&&q.volleys.size()==2&&q.volleys[0].fired==mask&&q.volleys[1].fired==mask&&q.shots.size()==2*count,"PuffPea expected two long-range natural volleys missing"))return false;
            Park(b);q.parked=true;LawnNet::NetTrace("SMOKE_FUSION_PUFF_PEA_DAMAGE kind=%d hits=%d damage=20 front=%d rear=5000 MC=5000 adjacent=5000 noSun=1 noAoE=1 noPierce=1 healthWrites=0",q.kind,q.hits,front->mBodyHealth);
        }
    }
    return true;
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionPuffPea(LawnApp* a)
{
    const char* native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE"),*enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_PUFF_PEA");
    if(!a||!native||std::strcmp(native,"1")||!enabled||std::strcmp(enabled,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
    if(a->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();if(GetTickCount()-s.since>(s.stage<=3?60000U:30000U)){char error[90];sprintf_s(error,"PuffPea deadline stage=%d role=%s",s.stage,Role(a));a->CoopSmokeFail(error);return true;}
    if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
    if(s.stage==0)
    {
        if(a->NetIsHost()&&!s.configured){if(!Require(a,!a->mBoard&&!a->mSeedChooserScreen,"PuffPea requires clean native lobby"))return true;a->mNetManager->SetDifficulty(1);s.configured=true;}
        if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
        if(a->NetIsClient()){Advance(a,1);Send(a,"FP80_START");}else if(Has(a,1,"FP80_START")){Advance(a,1);std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;
    }
    if(!a->mBoard||!IsNativeFusionBoard(a->mBoard)){if(s.stage==1&&a->NetIsClient())Send(a,"FP80_START");return true;}
    auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);auto* chooser=a->mSeedChooserScreen;const int slot=Slot(a);
    if(!Require(a,ctx&&ctx->validation&&!a->mFusionRuntime&&b->GetColumnCount()==10&&b->GetCellPitchX()==72,"PuffPea lost real native10x5 Board"))return true;
    const auto& bank=ctx->selection.State();if((s.stage==9||s.stage==14||s.stage==25)&&!ObserveCombat(a))return true;
    if(s.stage==1)
    {
        if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||!b->mCutScene->mSeedChoosing||chooser->mSeedsInFlight)return true;
        const size_t target=(std::min)(kNativeFusionBankCapacity,ctx->eligible.size());if(!Require(a,ctx->eligible.size()>=16&&bank.activeMask==3&&bank.targetCount==target&&bank.quotas[0]+bank.quotas[1]==target,"PuffPea canonical16pool/10capacity/quota differs"))return true;
        if(!Capture(a,"fusion_puff_pea_chooser"))return true;std::vector<int> choices;
        for(size_t i=0;i<ctx->eligible.size();++i){const auto& e=ctx->eligible[i];if(slot==0?(e.isExtra||(e.kind==6&&!e.isExtra)):(!e.isExtra&&e.kind!=6))choices.push_back(int(i));}
        auto priority=[&](int i){const auto& e=ctx->eligible[i];return slot==0?(e.kind==6&&!e.isExtra?0:e.kind==0&&e.isExtra?1:10+e.kind):(e.kind==0?0:10+e.kind);};
        std::stable_sort(choices.begin(),choices.end(),[&](int x,int y){return priority(x)<priority(y);});int own=0;for(const auto& packet:bank.packets)own+=packet.owner==slot;
        if(!Require(a,choices.size()>=bank.quotas[slot],"PuffPea disjoint chooser candidates missing"))return true;if(own<s.waitingOwn)return true;
        if(own<bank.quotas[slot]){int x=0,y=0;chooser->GetSeedPositionInChooser(choices[own],x,y);if(Click(a,float(chooser->mX+x+25),float(chooser->mY+y+35-int(chooser->mScrollbar->mScrollValue))))s.waitingOwn=own+1;return true;}
        if(bank.packets.size()!=target||!Packet(b,0,6,false)||!Packet(b,1,0,false)||!Packet(b,0,0,true))return true;int rank=-1;
        for(int i=0;i<b->mSeedBank->mNumPackets;++i){const int next=NetSeedBankOwnerRank(slot,b->mSeedBank->mSeedPackets[i].mNetOwnerSlot);if(!Require(a,next>=rank,"PuffPea own-first bank order changed"))return true;rank=next;}
        if(!Capture(a,"fusion_puff_pea_picked"))return true;NetTrace("SMOKE_FUSION_PUFF_PEA_POOL role=%s eligible=%zu target=%u quota0=%u quota1=%u ownFirst=1 unfiltered=1 nativeChooser=1",Role(a),ctx->eligible.size(),bank.targetCount,bank.quotas[0],bank.quotas[1]);Advance(a,2);
    }
    else if(s.stage==2){if(bank.phase!=FusionBankPhase::Choosing)Advance(a,3);else if(chooser&&!chooser->mSeedsInFlight&&Ready(a,chooser->mStartButton))Advance(a,3);}
    else if(s.stage==3)
    {
        if(bank.phase==FusionBankPhase::Intro){if(!s.introStart)s.introStart=GetTickCount();s.introLast=GetTickCount();++s.introChecks;Capture(a,"fusion_puff_pea_intro");return true;}
        if(bank.phase!=FusionBankPhase::Playing||a->mGameScene!=SCENE_PLAYING||chooser)return true;
        if(!Require(a,s.introChecks>=10&&s.introLast-s.introStart>=1000&&s.frames.contains("fusion_puff_pea_intro")&&b->mSunMoney==500,"PuffPea skipped full native intro/fresh500"))return true;
        s.playingTick=b->mMainCounter;Advance(a,4);
    }
    else if(s.stage==4)
    {
        if(b->mMainCounter-s.playingTick<2||!Capture(a,"fusion_puff_pea_playing")||!Barrier(a,"FP80_PLAYING"))return true;
        if(a->NetIsHost()){b->mSunCountDown=100000;b->mZombieCountDown=b->mZombieCountDownStart=100000;}
        NetTrace("SMOKE_FUSION_PUFF_PEA_PLAYING role=%s sun=500 nativeBoard=1 secondWorld=0 boardTicksSincePlaying=%d sunWrites=0",Role(a),b->mMainCounter-s.playingTick);Advance(a,5);
    }
    else if(s.stage==5){if(Purchase(a,0,6,false,0,6,500,500,kFusionNoPlant,kFusionNoPlant,s.current,"FP80_PUFF"))Advance(a,6);}
    else if(s.stage==6){if(Purchase(a,1,0,false,0,1018,500,400,6,0,s.current,"FP80_1018"))Advance(a,7);}
    else if(s.stage==7){if(Move(a,1018,0,1))Advance(a,8);}
    else if(s.stage==8)
    {
        if(a->NetIsHost()&&!s.setup&&!Setup(a))return true;if(a->NetIsHost()&&!Send(a,Ids()))return true;if(!ReadIds(a))return true;
        for(auto id:s.targets)if(!Target(b,id))return true;if(!Barrier(a,"FP80_TARGETS1018"))return true;if(BeginCombat(a,1018))Advance(a,9);
    }
    else if(s.stage==9||s.stage==14||s.stage==25)
    {
        const int kind=s.combat.kind,expected=kind==1018?2:kind==1019?4:8;const std::string shotFrame="fusion_puff_pea_projectile"+std::to_string(kind),frame="fusion_puff_pea_combat"+std::to_string(kind),gate="FP80_COMBAT_"+std::to_string(kind);
        if(s.combat.hits!=expected||LiveShots(b)||!s.frames.contains(shotFrame)||!Capture(a,frame.c_str()))return true;
        if(!Require(a,a->NetIsHost()||s.combat.clientShot,"PuffPea client missed actual authoritative payload10"))return true;
        if(!Barrier(a,gate))return true;NetTrace("SMOKE_FUSION_PUFF_PEA_COMBAT role=%s kind=%d hits=%d front=%d rear=5000 MC=5000 adjacent=5000 damage=20 noAoE=1 noSun=1 noPierce=1",Role(a),kind,expected,Target(b,s.targets[0])->mBodyHealth);Advance(a,s.stage==9?10:s.stage==14?15:26);
    }
    else if(s.stage==10)
    {
        // Fusion retires the ingredient before Purchase observes its replacement.
        // Apply the quiet-source precondition only until that result exists.
        auto* result=At(b,1);const auto* data=GetNativePlantData(result);
        if(!s.observed&&(!data||data->kind!=1019))
        {
            auto* source=ByRef(b,s.current);
            if(!source)NetTrace("SMOKE_FUSION_PUFF_PEA_UPGRADE_SOURCE_MISSING role=%s source=%u token=%u result=%u kind=%u sun=%d",Role(a),s.current.id,s.current.token,result?result->mNetId:0,data?data->kind:kFusionNoPlant,b->mSunMoney);
            if(!Require(a,source!=nullptr,"PuffPea1018 source disappeared without expected1019 replacement"))return true;
            if(source->mShootingCounter||LiveShots(b))return true;
        }
        if(Purchase(a,1,0,false,1,1019,400,300,1018,0,s.current,"FP80_1019"))Advance(a,11);
    }
    else if(s.stage==11){if(Move(a,1019,1,0))Advance(a,12);}
    else if(s.stage==12){if(!Capture(a,"fusion_puff_pea_model1019"))return true;Advance(a,13);}
    else if(s.stage==13){if(Barrier(a,"FP80_TARGETS1019")&&BeginCombat(a,1019))Advance(a,14);}
    else if(s.stage==15)
    {auto* p=ByRef(b,s.current);if(!Require(a,p!=nullptr,"PuffPea1019 disappeared before separate parent purchase"))return true;if(p->mShootingCounter||LiveShots(b))return true;if(Purchase(a,1,0,false,7,0,300,200,kFusionNoPlant,kFusionNoPlant,s.parent,"FP80_OTHER_PARENT"))Advance(a,16);}
    else if(s.stage==16){if(Purchase(a,0,0,true,7,1030,200,0,0,0,s.doublePea,"FP80_1030"))Advance(a,17);}
    else if(s.stage==17)
    {
        if(ctx->gloveCooldownTicks&&!ctx->gloveHolds[1].plantId)return true;if(!Business(a)||!Pickup(a,s.doublePea)||!Capture(a,"fusion_puff_pea_held1030")||!Barrier(a,"FP80_HELD1030"))return true;Advance(a,18);
    }
    else if(s.stage==18)
    {
        if(slot==1&&!s.clicked){s.clicked=Cell(a,0);return true;}auto* p=At(b,0);const auto* d=GetNativePlantData(p);if(!d||d->kind!=1065)return true;
        if(!s.observed)
        {
            if(!Require(a,d->instanceToken&&d->owner==0&&d->packetId==Packet(b,0,0,true)->mNetId&&d->firstParent==1019&&d->secondParent==1030&&!ByRef(b,s.current)&&!ByRef(b,s.doublePea)&&p->mPlantHealth==300&&ctx->gloveCooldownTicks>334&&ctx->gloveCooldownTicks<=1000&&!ctx->gloveHolds[1].plantId,"PuffPea final glove recipe/provenance/incoming1030cooldown differs"))return true;
            s.current=Identity(p);s.moveCD=ctx->gloveCooldownTicks;s.observed=true;
        }
        if(!Business(a)||!Capture(a,"fusion_puff_pea_gatling")||!Barrier(a,"FP80_1065"))return true;
        NetTrace("SMOKE_FUSION_PUFF_PEA_CRAFT role=%s kind=1065 id=%u token=%u owner=0 parents=1019,1030 sun=0 incomingKind=1030 expectedCD=1000 observedCD=%d nativeClientGlove=1",Role(a),s.current.id,s.current.token,s.moveCD);Advance(a,19);
    }
    else if(s.stage==19){if(ctx->gloveCooldownTicks)return true;if(!Barrier(a,"FP80_NATURAL_CD1000"))return true;NetTrace("SMOKE_FUSION_PUFF_PEA_RECHARGED role=%s incoming1030CD=1000 naturalZero=1 noTimerWrites=1",Role(a));Advance(a,20);}
    else if(s.stage==20){if(Move(a,1065,0,1))Advance(a,21);}
    else if(s.stage==21){if(!Capture(a,"fusion_puff_pea_model1065"))return true;Advance(a,22);}
    else if(s.stage==22){if(Barrier(a,"FP80_TARGETS1065")&&BeginCombat(a,1065))Advance(a,25);}
    else if(s.stage==26)
    {
        if(!Business(a)||ctx->gloveCooldownTicks||Samples(a)<8||Samples(a)-s.purchaseSamples<2||!Barrier(a,"FP80_DONE")||!Capture(a,"fusion_puff_pea_complete"))return true;
        NetTrace("SMOKE_FUSION_PUFF_PEA_COMPLETE role=%s kind=1065 id=%u token=%u sun=0 puffUses=1 normalUses=3 extraUses=1 sourceCD=750 gloveCD=0 threeProfiles=1 sunWrites=0 plantHealthWrites=0 plantTimerWrites=0 nativeBoard=1 secondWorld=0",Role(a),s.current.id,s.current.token);Advance(a,27);a->CoopSmokeWriteResult(true,nullptr);
    }
    return true;
}
}
