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
#include "../Fusion/FusionCherryShooterRules.h"
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
struct Ref { uint16_t id=0;uint32_t token=0; };
struct Volley {int tick=0;std::array<int,4> delay{};unsigned fired=0;};
struct Quad
{
    int kind=0,baseHealth=5000,hits=0,previousCounter=0,previousTick=0;
    bool begun=false,withdrawn=false,restored=false,parked=false,clientProjectile=false;
    std::vector<Volley> volleys;std::map<uint16_t,std::pair<int,float>> projectiles;
} ;
struct State
{
    int stage=0,step=0,waitingOwn=0,normalUses=0,extraUses=0,cherryUses=0,money=1000;
    int moveTick=0,moveLaunch=0,moveHP=0,moveDelta=0,moveCD=0,holdTick=-1;float holdIdle=0;
    NativePlantData moveData{};
    uint32_t since=0,lastChat=0,introStart=0,introLast=0,introChecks=0,pickSamples=0,purchaseSamples=0,rejectSamples=0,rejectSince=0;
    bool configured=false,budget=false,budgetWritten=false,clicked=false,placed=false,observed=false,toolClicked=false,actionSent=false,rejectStarted=false,setup=false;
    Ref parentA{},parentB{},a{},b{},gatling{},cherry{};
    std::array<uint16_t,3> targets{};Quad quad{};
    std::array<Ref,3> layers{};std::array<uint32_t,3> layerLocalIds{};std::array<NativePlantData,3> layerData{};
    int layerIndex=0,layerCountdown=0,layerShooting=0;bool layerSetup=false;
    std::map<std::string,int> firstDraw;std::set<std::string> frames;
} s;
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool ok,const char* why){if(!ok)a->CoopSmokeFail(why);return ok;}
void Advance(LawnApp* a,int stage)
{
    s.stage=stage;s.since=GetTickCount();s.clicked=s.placed=s.observed=s.toolClicked=s.actionSent=s.rejectStarted=false;s.holdTick=-1;
    LawnNet::NetTrace("SMOKE_FUSION_GLOVE_STAGE role=%s stage=%d",Role(a),stage);
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
    if(!Require(a,a->CoopSmokeCaptureFrame(name),"Glove native renderer capture failed"))return false;
    s.frames.insert(name);LawnNet::NetTrace("SMOKE_FUSION_GLOVE_RENDER role=%s frame=%s freshDraws=%d",Role(a),name,a->mDrawCount-s.firstDraw[name]);return true;
}
bool Click(LawnApp* a,float x,float y,uint8_t button=SDL_BUTTON_LEFT,bool click=true)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    if(!click)return Require(a,SDL_PushEvent(&m),"Glove scoped motion failed");
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=button;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;
    return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Glove scoped SDL click failed");
}
bool BoardClick(LawnApp* a,float x,float y,uint8_t button=SDL_BUTTON_LEFT,bool click=true)
{return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY,button,click);}
bool Cell(LawnApp* a,int col,int row=kRow,uint8_t button=SDL_BUTTON_LEFT,bool click=true)
{return BoardClick(a,float(a->mBoard->GetCellCenterX(col,row)),float(a->mBoard->GridToPixelY(col,row)+50),button,click);}
bool Ready(LawnApp* a,GameButton* b)
{
    if(!b||b->mDisabled||b->mBtnNoDraw)return false;float x=b->mX+b->mWidth*.5f,y=b->mY+b->mHeight*.5f;
    if(!b->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"Glove Ready hover failed");return false;}
    LawnNet::NetTrace("SMOKE_FUSION_GLOVE_READY role=%s nativeSDL=1 hoverObserved=1",Role(a));return Click(a,x,y);
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
{if(!Require(a,p&&p->mActive&&!p->mRefreshing,"Glove real packet not ready"))return false;auto* b=a->mBoard->mSeedBank;return BoardClick(a,float(b->mX+p->mX+p->mOffsetX+p->mWidth/2),float(b->mY+p->mY+p->mHeight/2));}
bool Business(LawnApp* a)
{
    auto* b=a->mBoard;auto* p=Packet(b,0,0,false);auto* e=Packet(b,1,0,true);auto* c=Packet(b,0,2,false);
    if(!Require(a,p&&e&&c&&b->mSunMoney==s.money&&p->mRefreshTime==(s.normalUses?750:0)&&e->mRefreshTime==(s.extraUses?750:0)&&c->mRefreshTime==(s.cherryUses?5000:0)&&
        (!a->NetIsHost()||(p->mTimesUsed==s.normalUses&&e->mTimesUsed==s.extraUses&&c->mTimesUsed==s.cherryUses)),"Glove prices/card use/CD conservation failed"))return false;
    Coin* coin=nullptr;while(b->mCoins.IterateNext(coin))if(!Require(a,coin->mDead,"Quad unexpectedly generated sun/Coin"))return false;return true;
}
bool Holds(LawnApp* a,Ref first,Ref second)
{
    auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);const Ref expected[]={first,second};
    for(int slot=0;slot<2;++slot)
    {
        const auto& h=ctx->gloveHolds[slot];if(h.plantId!=expected[slot].id||h.instanceToken!=expected[slot].token)return false;
        auto* p=GetNativeGloveHeldPlant(b,slot);
        if(expected[slot].id){if(!Require(a,p==ByRef(b,expected[slot])&&p&&!p->NotOnGround()&&p->mPlantHealth==300,"Glove preview detached/froze/damaged original actor"))return false;}
        else if(!Require(a,!p,"Glove empty canonical tuple retained a preview actor"))return false;
    }
    return true;
}
bool Pickup(LawnApp* a,int operatorSlot,Ref source)
{
    if(Slot(a)==operatorSlot)
    {
        if(!s.toolClicked){Sexy::Rect r=a->mBoard->GetShovelButtonRect();a->mBoard->GetZenButtonRect(OBJECT_TYPE_GLOVE,r);if(!Require(a,r.mWidth>0&&r.mHeight>0,"Native glove tool rectangle unavailable"))return false;s.toolClicked=BoardClick(a,float(r.mX+r.mWidth/2),float(r.mY+r.mHeight/2));return false;}
        if(!s.clicked){if(a->mBoard->mCursorObject->mCursorType!=CURSOR_TYPE_GLOVE)return false;auto* p=ByRef(a->mBoard,source);if(!Require(a,p,"Glove UI pickup source vanished"))return false;s.clicked=Cell(a,p->mPlantCol,p->mRow);return false;}
    }
    const auto* ctx=GetNativeContext(a->mBoard);const auto& h=ctx->gloveHolds[operatorSlot];
    return h.plantId==source.id&&h.instanceToken==source.token&&(Slot(a)!=operatorSlot||a->mBoard->mCursorObject->mCursorType==CURSOR_TYPE_PLANT_FROM_GLOVE);
}
bool Cancel(LawnApp* a,int operatorSlot)
{
    if(Slot(a)==operatorSlot&&!s.clicked){s.clicked=Cell(a,8,1,SDL_BUTTON_RIGHT);return false;}
    const auto& h=GetNativeContext(a->mBoard)->gloveHolds[operatorSlot];return !h.plantId&&!h.instanceToken;
}
bool Purchase(LawnApp* a,int buyer,int kind,bool extra,int col,int result,int before,int after,Ref& out,const char* gate)
{
    auto* b=a->mBoard;auto* card=Packet(b,buyer,kind,extra);
    if(Slot(a)==buyer&&!s.clicked){s.pickSamples=Samples(a);if(PickPacket(a,card))s.clicked=true;return false;}
    if(Slot(a)==buyer&&!s.placed){if(!HeldPacket(b,card)||(a->NetIsClient()&&Samples(a)<=s.pickSamples))return false;if(!Require(a,b->mSunMoney==before,"Glove card pickup charged before placement"))return false;s.placed=Cell(a,col);return false;}
    if(!s.observed)
    {
        auto* p=At(b,col);const auto* d=GetNativePlantData(p);if(!d||d->kind!=result)return false;
        const int first=result==1030?0:result==1017?1032:kFusionNoPlant,second=result==1030?0:result==1017?2:kFusionNoPlant;
        if(!Require(a,card&&d->instanceToken&&d->owner==buyer&&d->packetId==card->mNetId&&!d->randomOrigin&&d->firstParent==first&&d->secondParent==second&&p->mPlantHealth==300&&b->mSunMoney==after,"Glove actual purchase/ancestry/token differs"))return false;
        out=Identity(p);s.money=after;if(kind==2)++s.cherryUses;else if(extra)++s.extraUses;else ++s.normalUses;s.observed=true;s.purchaseSamples=Samples(a);
    }
    if(!Barrier(a,gate))return false;
    LawnNet::NetTrace("SMOKE_FUSION_GLOVE_PURCHASE role=%s kind=%d id=%u token=%u packet=%u owner=%d extra=%d sun=%d nativeSDL=1",Role(a),result,out.id,out.token,card->mNetId,buyer,extra?1:0,after);return true;
}
bool Reject(LawnApp* a,int sender,FusionCommand command,const std::string& label,Ref hold0,Ref hold1)
{
    const std::string sent="FG74_BAD_"+label,ack=sent+"_OK";
    if(Slot(a)==sender&&!s.actionSent){if(!Require(a,a->SubmitNativeFusionCommand(command),"Glove negative typed intent was not queued"))return false;s.actionSent=true;}
    if(Slot(a)==sender&&!Send(a,sent))return false;if(!Has(a,sender,sent))return false;
    if(!s.rejectStarted){s.rejectStarted=true;s.rejectSamples=Samples(a);s.rejectSince=GetTickCount();}
    if(Samples(a)-s.rejectSamples<2||GetTickCount()-s.rejectSince<250)return false;
    if(!Business(a)||!Require(a,Holds(a,hold0,hold1)&&GetNativeContext(a->mBoard)->gloveCooldownTicks==0,"Glove rejected intent mutated leases/global cooldown"))return false;
    if(!Barrier(a,ack))return false;LawnNet::NetTrace("SMOKE_FUSION_GLOVE_REJECT role=%s case=%s sender=%d queuedAuthenticated=1 noCharge=1 holdsPreserved=1 CD=0",Role(a),label.c_str(),sender);
    s.actionSent=s.rejectStarted=false;return true;
}
Zombie* Target(Board* b,uint16_t id){Zombie* z=nullptr;while(b->mZombies.IterateNext(z))if(z->mNetId==id)return z;return nullptr;}
bool SetupTargets(LawnApp* a)
{
    for(int i=0;i<3;++i)
    {
        auto* z=a->mBoard->AddZombieInRow(ZOMBIE_NORMAL,i==2?kRow+1:kRow,0);if(!Require(a,z&&z->mNetId,"Quad native target allocation failed"))return false;
        auto* body=a->ReanimationTryToGet(z->mBodyReanimID);const float rate=body?body->mAnimRate:0;if(!Require(a,body&&std::isfinite(rate)&&rate>0,"Quad original native walking rate missing"))return false;
        z->mPosX=1100.0f+100*i;z->mX=int(z->mPosX);z->mPosY=z->GetPosYBasedOnRow(z->mRow);z->mY=int(z->mPosY);z->mVelX=0;z->mMindControlled=false;z->mBodyHealth=z->mBodyMaxHealth=5000;
        z->SetAnimRate(rate);z->ApplyAnimRate(0);s.targets[i]=z->mNetId;
        LawnNet::NetTrace("SMOKE_FUSION_GLOVE_TARGET index=%d id=%u HP=5000 type=0 boundedNativeTarget=1 initialHPOnly=1 fixtureControl=1",i,z->mNetId);
    }
    s.setup=true;return true;
}
std::string Ids(){std::ostringstream out;out<<"FG74_IDS";for(auto i:s.targets)out<<' '<<i;return out.str();}
bool ReadIds(LawnApp* a)
{
    if(a->NetIsHost())return s.setup;for(const auto& l:a->mNetManager->mChatLines)if(l.mSlot==0&&l.mText.rfind("FG74_IDS ",0)==0)
    {std::istringstream in(l.mText.substr(9));std::set<unsigned> seen;for(auto& id:s.targets){unsigned n=0;if(!Require(a,bool(in>>n)&&n&&n<=65535&&seen.insert(n).second,"Quad target IDs invalid"))return false;id=uint16_t(n);}std::string extra;return Require(a,!(in>>extra),"Quad target IDs trailing data");}return false;
}
void PositionTargets(Board* b,bool active)
{for(int i=0;i<3;++i){auto* z=Target(b,s.targets[i]);z->mPosX=float(active?(i==1?700:600):1100+100*i);z->mX=int(z->mPosX);}}
int LiveShots(Board* b){int n=0;Projectile* p=nullptr;while(b->mProjectiles.IterateNext(p))n+=!p->mDead;return n;}
void BeginQuad(LawnApp* a,int kind)
{s.quad={};s.quad.kind=kind;s.quad.begun=true;s.quad.baseHealth=Target(a->mBoard,s.targets[0])->mBodyHealth;auto* p=ByRef(a->mBoard,kind==1032?s.gatling:s.cherry);s.quad.previousCounter=p->mShootingCounter;s.quad.previousTick=a->mBoard->mMainCounter;if(a->NetIsHost())PositionTargets(a->mBoard,true);}
bool ObserveQuad(LawnApp* a)
{
    if(!Business(a))return false;auto& q=s.quad;auto* b=a->mBoard;auto* p=ByRef(b,q.kind==1032?s.gatling:s.cherry);const auto* d=GetNativePlantData(p);const int damage=q.kind==1032?20:60;
    if(!Require(a,p&&d&&d->kind==q.kind&&p->mPlantHealth==300,"Quad source actor changed"))return false;
    if(a->NetIsHost())
    {
        if(p->mShootingCounter>q.previousCounter)
        {
            constexpr std::array<float,4> events={.33333334f,.5f,.6666667f,.8333333f};std::array<int,4> delay{};for(int i=0;i<4;++i)delay[i]=int(std::ceil(double(events[i])/double(d->animationSpeed)*100));
            const int initial=delay[3]+1,dt=b->mMainCounter-q.previousTick;
            if(!Require(a,q.volleys.size()<3&&(q.volleys.empty()||q.volleys.back().fired==15)&&p->mShootingCounter<=initial&&initial-p->mShootingCounter<=(std::max)(1,dt),"Quad native counter/previous four-event volley differs"))return false;
            const int tick=b->mMainCounter-(initial-p->mShootingCounter);q.volleys.push_back({tick,delay,0});
            LawnNet::NetTrace("SMOKE_FUSION_GLOVE_QUAD_TRIGGER kind=%d volley=%zu tick=%d initial=%d delays=%d,%d,%d,%d speed=%.9g stateSpeed=1 nativeClock=1",q.kind,q.volleys.size(),tick,initial,delay[0],delay[1],delay[2],delay[3],d->animationSpeed);
            if(q.volleys.size()==1){PositionTargets(b,false);q.withdrawn=true;LawnNet::NetTrace("SMOKE_FUSION_GLOVE_TARGET_GONE kind=%d afterTrigger=1 positionOnly=1 remainingEventsMustFire=4",q.kind);}
        }
        q.previousCounter=p->mShootingCounter;q.previousTick=b->mMainCounter;
    }
    Projectile* shot=nullptr;while(b->mProjectiles.IterateNext(shot))
    {
        if(shot->mDead)continue;const auto* meta=GetNativeProjectileData(shot);
        if(!Require(a,meta&&meta->kind==(q.kind==1032?0:1)&&meta->damage==damage&&!meta->sunOnHit&&shot->mProjectileType==PROJECTILE_PEA,"Quad payload is not ordinary single-hit/noSun"))return false;
        if(a->NetIsHost())
        {
            if(!Require(a,std::abs(shot->mVelX-3.2f)<.0001f,"Quad projectile speed differs"))return false;auto old=q.projectiles.find(shot->mNetId);
            if(old==q.projectiles.end())
            {
                const int born=b->mMainCounter-shot->mProjectileAge+1;int v=-1,e=-1;for(size_t vi=0;vi<q.volleys.size();++vi)for(int ei=0;ei<4;++ei)if(q.volleys[vi].tick+q.volleys[vi].delay[ei]==born){v=int(vi);e=ei;}
                if(!Require(a,v>=0&&e>=0&&!(q.volleys[v].fired&(1U<<e))&&q.projectiles.size()<12,"Quad duplicate/early/late or canceled source event"))return false;
                q.volleys[v].fired|=1U<<e;q.projectiles.emplace(shot->mNetId,std::pair<int,float>{b->mMainCounter,shot->mPosX});
                LawnNet::NetTrace("SMOKE_FUSION_GLOVE_QUAD_SHOT kind=%d id=%u volley=%d event=%d born=%d trigger=%d delay=%d damage=%d sun=0 vx=3.2 nativeActor=1",q.kind,shot->mNetId,v+1,e+1,born,q.volleys[v].tick,q.volleys[v].delay[e],damage);
            }
            else{const int dt=b->mMainCounter-old->second.first;if(!Require(a,std::abs(shot->mPosX-old->second.second-3.2f*dt)<.08f,"Quad actual displacement differs"))return false;old->second={b->mMainCounter,shot->mPosX};}
        }
        else q.clientProjectile=true;
        Capture(a,q.kind==1032?"fusion_glove_quad1032":"fusion_glove_quad1017");
    }
    auto* front=Target(b,s.targets[0]);auto* rear=Target(b,s.targets[1]);auto* adjacent=Target(b,s.targets[2]);if(!front||!rear||!adjacent)return true;
    const int loss=q.baseHealth-front->mBodyHealth;
    if(!Require(a,loss>=0&&loss%damage==0&&loss/damage<=8&&rear->mBodyHealth==5000&&adjacent->mBodyHealth==5000,"Quad damage/noAoE/noPierce control changed"))return false;q.hits=loss/damage;
    if(a->NetIsHost()&&q.withdrawn&&!q.restored&&q.volleys.size()==1&&q.volleys[0].fired==15&&LiveShots(b)==0)
    {if(!Require(a,q.hits==0,"Target-gone volley damaged a parked target"))return false;PositionTargets(b,true);q.restored=true;LawnNet::NetTrace("SMOKE_FUSION_GLOVE_QUAD_UNCANCELED kind=%d events=4 retiredNaturally=1 hits=0 forcedClock=0",q.kind);}
    if(a->NetIsHost()&&q.hits==8&&!q.parked)
    {
        if(!Require(a,q.volleys.size()==3&&q.volleys[0].fired==15&&q.volleys[1].fired==15&&q.volleys[2].fired==15&&q.projectiles.size()==12,"Quad did not complete canceled-target plus two real four-hit volleys"))return false;
        PositionTargets(b,false);q.parked=true;LawnNet::NetTrace("SMOKE_FUSION_GLOVE_QUAD_DAMAGE kind=%d hits=8 damage=%d front=%d rear=5000 adjacent=5000 noSun=1 noAoE=1 noPierce=1 healthWrites=0",q.kind,damage,front->mBodyHealth);
    }
    return true;
}
constexpr std::array<uint16_t,3> layerKinds={0,24,27};
constexpr std::array<int,3> layerColumns={4,6,8}; // Body, then cover, then the uncovered pot.
bool LayerState(LawnApp* a,int moved)
{
    auto* b=a->mBoard;
    for(int i=0;i<3;++i)
    {
        auto* p=ByRef(b,s.layers[i]);const auto* d=GetNativePlantData(p);const auto& before=s.layerData[i];
        if(!Require(a,p&&d&&b->mPlants.DataArrayGetID(p)==s.layerLocalIds[i]&&p->mPlantCol==(i<moved?layerColumns[i]:2)&&p->mRow==0&&
            p->mPlantHealth==p->mPlantMaxHealth&&p->mPlantMaxHealth==(i==1?4000:300)&&d->kind==layerKinds[i]&&
            d->firstParent==before.firstParent&&d->secondParent==before.secondParent&&d->owner==255&&!d->packetId&&!d->randomOrigin&&
            d->animationSpeed==before.animationSpeed&&d->idleSeconds>=before.idleSeconds,
            "Glove layer move changed another actor/HP/native generation/source provenance"))return false;
        PlantsOnLawn cell;b->GetPlantsOnLawn(p->mPlantCol,0,&cell);
        if(!Require(a,(i==0?cell.mNormalPlant:i==1?cell.mPumpkinPlant:cell.mUnderPlant)==p,"Glove actor lost its actual native layer slot"))return false;
    }
    return Business(a);
}
bool SetupLayers(LawnApp* a)
{
    auto* b=a->mBoard;
    if(a->NetIsHost()&&!s.layerSetup)
    {
        if(!Require(a,!b->GetTopPlantAt(2,0,TOPPLANT_ANY),"Glove initial layer fixture cell is occupied"))return false;
        for(uint16_t kind:{uint16_t(27),uint16_t(0),uint16_t(24)})
        {NativePlantData data;data.kind=kind;if(!Require(a,b->AddPlant(2,0,GetNativeSeedForPlant(kind),SEED_NONE,&data)!=nullptr,"Glove initial real layer factory failed"))return false;}
        s.layerSetup=true;LawnNet::NetTrace("SMOKE_FUSION_GLOVE_LAYER_SETUP actors=3 kinds=0,24,27 initialFactoryOnly=1 postSetupHPOrTimerWrites=0");
    }
    if(!s.observed)
    {
        PlantsOnLawn cell;b->GetPlantsOnLawn(2,0,&cell);Plant* actors[]={cell.mNormalPlant,cell.mPumpkinPlant,cell.mUnderPlant};
        for(auto* p:actors)if(!p||!GetNativePlantData(p))return false;
        for(int i=0;i<3;++i){s.layers[i]=Identity(actors[i]);s.layerLocalIds[i]=b->mPlants.DataArrayGetID(actors[i]);s.layerData[i]=*GetNativePlantData(actors[i]);}
        s.observed=true;
    }
    return LayerState(a,0)&&Capture(a,"fusion_glove_layer_stack")&&Barrier(a,"FG187_LAYER_STACK");
}
bool MoveLayer(LawnApp* a)
{
    auto* b=a->mBoard;const int i=s.layerIndex,operatorSlot=i==1?1:0;auto* p=ByRef(b,s.layers[i]);const auto* d=GetNativePlantData(p);
    if(!Require(a,p&&d,"Glove layer source vanished"))return false;
    if(p->mPlantCol==2)
    {
        if(a->NetIsHost())
        {
            if(i==0&&p->mLaunchCounter<=45)return false;
            s.moveTick=b->mMainCounter;s.moveLaunch=p->mLaunchCounter;s.moveHP=p->mPlantHealth;s.moveData=*d;
            s.layerCountdown=p->mStateCountdown;s.layerShooting=p->mShootingCounter;
        }
        if(Slot(a)==operatorSlot&&!s.clicked)s.clicked=Cell(a,layerColumns[i],0);
        return false;
    }
    if(!LayerState(a,i+1))return false;
    if(!s.observed)
    {
        s.moveCD=GetNativeContext(b)->gloveCooldownTicks;
        if(!Require(a,s.moveCD>0&&s.moveCD<=(i==2?334:1000),"Glove layer success did not start its source cooldown"))return false;
        if(a->NetIsHost())
        {
            const int dt=b->mMainCounter-s.moveTick;
            if(!Require(a,dt>=0&&p->mPlantHealth==s.moveHP&&p->mShootingCounter==s.layerShooting&&
                p->mStateCountdown==(std::max)(0,s.layerCountdown-dt)&&p->mLaunchCounter==(i==0?s.moveLaunch-dt:s.moveLaunch)&&
                d->animationSpeed==s.moveData.animationSpeed&&d->idleSeconds>=s.moveData.idleSeconds&&d->productionTicks==s.moveData.productionTicks&&d->pendingProductionTicks==s.moveData.pendingProductionTicks,
                "Glove layer move reinitialized native counters/HP/RNG"))return false;
        }
        s.observed=true;
    }
    const char* frame=i==0?"fusion_glove_layer_body":i==1?"fusion_glove_layer_cover":"fusion_glove_layer_under";
    if(!Capture(a,frame)||!Barrier(a,"FG187_LAYER_MOVED_"+std::to_string(i)))return false;
    LawnNet::NetTrace("SMOKE_FUSION_GLOVE_LAYER_MOVED role=%s index=%d kind=%u id=%u token=%u column=%d row=0 HP=%d owner=255 packet=0 parents=65535,65535 sameInstance=1 otherLayersIntact=1 noCharge=1 observedCD=%d hostCounterProof=%d nativeSDL=1",Role(a),i,d->kind,p->mNetId,d->instanceToken,p->mPlantCol,p->mPlantHealth,s.moveCD,a->NetIsHost()?1:0);
    return true;
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionGlove(LawnApp* a)
{
    const char* native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE"),*enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_GLOVE");
    if(!a||!native||std::strcmp(native,"1")||!enabled||std::strcmp(enabled,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
    if(a->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();
    if(GetTickCount()-s.since>(s.stage<=3?60000U:30000U)){char error[100];sprintf_s(error,"Glove batch deadline stage=%d role=%s step=%d",s.stage,Role(a),s.step);a->CoopSmokeFail(error);return true;}
    if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
    if(s.stage==0)
    {
        if(a->NetIsHost()&&!s.configured){if(!Require(a,!a->mBoard&&!a->mSeedChooserScreen,"Glove batch needs clean lobby"))return true;a->mNetManager->SetDifficulty(1);s.configured=true;}
        if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
        if(a->NetIsClient()){Advance(a,1);Send(a,"FG74_START");}
        else if(Has(a,1,"FG74_START")){Advance(a,1);std::string e;if(!a->StartFusionValidation(&e))a->CoopSmokeFail(e.c_str());}return true;
    }
    if(!a->mBoard||!IsNativeFusionBoard(a->mBoard)){if(s.stage==1&&a->NetIsClient())Send(a,"FG74_START");return true;}
    auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);auto* chooser=a->mSeedChooserScreen;
    if(!Require(a,ctx&&ctx->validation&&!a->mFusionRuntime&&b->GetColumnCount()==10&&b->GetCellPitchX()==72,"Glove batch lost real native10x5 Board"))return true;
    const auto& bank=ctx->selection.State();const int slot=Slot(a);
    if((s.stage==20||s.stage==23)&&!ObserveQuad(a))return true;
    if(s.stage==1)
    {
        if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||!b->mCutScene->mSeedChoosing||chooser->mSeedsInFlight)return true;
        const size_t target=(std::min)(kNativeFusionBankCapacity,ctx->eligible.size());
        if(!Require(a,bank.activeMask==3&&bank.targetCount==target&&bank.quotas[0]+bank.quotas[1]==target&&bank.quotas[0]>=2&&bank.quotas[1]>=1,"Glove batch live chooser quota mismatch"))return true;
        if(!Capture(a,"fusion_glove_chooser"))return true;
        std::vector<int> choices;for(size_t i=0;i<ctx->eligible.size();++i)if(ctx->eligible[i].isExtra==(slot==1))choices.push_back(int(i));
        auto priority=[&](int i){const int k=ctx->eligible[i].kind;return k==0?0:slot==0&&k==2?1:10;};
        std::stable_sort(choices.begin(),choices.end(),[&](int x,int y){return priority(x)<priority(y);});
        std::set<uint16_t> mappedBases;for(const auto& entry:ctx->eligible){if(!Require(a,IsNativeBasePlantKind(entry.kind)&&GetNativeSeedForPlant(entry.kind)!=SEED_NONE,"Glove chooser admitted an unmapped/non-base identity"))return true;mappedBases.insert(entry.kind);}
        if(!Require(a,ctx->eligible.size()==2*mappedBases.size()&&choices.size()>=bank.quotas[slot]&&!choices.empty()&&priority(choices.front())==0,"Glove batch filtered/missing base identities"))return true;
        int own=0;for(const auto& p:bank.packets)own+=p.owner==slot;if(own<s.waitingOwn)return true;
        if(own<bank.quotas[slot]){int x=0,y=0;chooser->GetSeedPositionInChooser(choices[own],x,y);if(Click(a,float(chooser->mX+x+25),float(chooser->mY+y+35-int(chooser->mScrollbar->mScrollValue))))s.waitingOwn=own+1;return true;}
        if(bank.packets.size()!=target||!Packet(b,0,0,false)||!Packet(b,1,0,true)||!Packet(b,0,2,false))return true;
        int rank=-1;for(int i=0;i<b->mSeedBank->mNumPackets;++i){const int next=NetSeedBankOwnerRank(slot,b->mSeedBank->mSeedPackets[i].mNetOwnerSlot);if(!Require(a,next>=rank,"Glove batch lost own-first native bank order"))return true;rank=next;}
        if(!Capture(a,"fusion_glove_picked"))return true;
        NetTrace("SMOKE_FUSION_GLOVE_POOL role=%s eligible=%zu target=%u quota0=%u quota1=%u ownFirst=1 nativeChooser=1 unfiltered=1 mappedBases=%zu",Role(a),ctx->eligible.size(),bank.targetCount,bank.quotas[0],bank.quotas[1],mappedBases.size());Advance(a,2);
    }
    else if(s.stage==2){if(bank.phase!=FusionBankPhase::Choosing)Advance(a,3);else if(chooser&&!chooser->mSeedsInFlight&&Ready(a,chooser->mStartButton))Advance(a,3);}
    else if(s.stage==3)
    {
        if(bank.phase==FusionBankPhase::Intro){if(!s.introStart)s.introStart=GetTickCount();s.introLast=GetTickCount();++s.introChecks;Capture(a,"fusion_glove_intro");return true;}
        if(bank.phase!=FusionBankPhase::Playing||a->mGameScene!=SCENE_PLAYING||chooser)return true;
        if(!Require(a,s.introChecks>=10&&s.introLast-s.introStart>=1000&&s.frames.contains("fusion_glove_intro")&&b->mSunMoney==500,"Glove batch skipped native Ready/full intro/fresh500"))return true;
        NetTrace("SMOKE_FUSION_GLOVE_PLAYING role=%s initialGameSun=500 nativeBoard=1 secondWorld=0",Role(a));Advance(a,4);
    }
    else if(s.stage==4)
    {
        if(!Barrier(a,"FG74_PLAY"))return true;
        if(!s.budget)
        {
            if(a->NetIsHost()&&!s.budgetWritten)
            {
                if(!Require(a,!b->mPlants.mSize&&!b->mZombies.mSize&&b->mSunMoney==500,"Glove fixture budget was not before first purchase"))return true;
                b->mSunMoney=1000;b->mSunCountDown=100000;b->mZombieCountDown=b->mZombieCountDownStart=100000;s.budgetWritten=true;
            }
            if(b->mSunMoney!=1000)return true;s.budget=true;
            NetTrace("SMOKE_FUSION_GLOVE_BUDGET role=%s fixtureInitialSun=1000 sunWritesAfterSetup=0 wavesAndSkyOnlyDeferred=1 campaignInitialSunUnchanged=500",Role(a));
        }
        if(!Barrier(a,"FG74_BUDGET"))return true;
        if(Purchase(a,0,0,false,2,0,1000,900,s.parentA,"FG74_PARENT_A"))Advance(a,5);
    }
    else if(s.stage==5)
    {
        if(Purchase(a,1,0,true,2,1030,900,700,s.a,"FG74_DOUBLE_A"))Advance(a,6);
    }
    else if(s.stage==6)
    {
        if(!Business(a)||!Pickup(a,0,s.a)||!Holds(a,s.a,{}))return true;auto* p=ByRef(b,s.a);const auto* d=GetNativePlantData(p);
        if(s.holdTick<0){s.holdTick=b->mMainCounter;s.holdIdle=d->idleSeconds;}
        if(slot==0&&!s.placed){Cell(a,7,1,SDL_BUTTON_LEFT,false);s.placed=true;}
        if(slot==0&&!Send(a,"FG74_HOVER"))return true;if(!Has(a,0,"FG74_HOVER"))return true;
        if(b->mMainCounter-s.holdTick<25||d->idleSeconds-s.holdIdle<.15f||!Capture(a,"fusion_glove_held"))return true;
        if(!Require(a,p->mPlantCol==2&&p->mRow==kRow&&p->mPlantHealth==300&&d->owner==1&&ctx->gloveCooldownTicks==0,"Cross-owner pickup removed/charged/paused original"))return true;
        if(!Barrier(a,"FG74_HELD"))return true;NetTrace("SMOKE_FUSION_GLOVE_HELD role=%s source=%u token=%u holder=0 owner=1 originalAlive=1 idleAdvanced=1 canonicalPreview=1 CD=0",Role(a),s.a.id,s.a.token);s.step=0;Advance(a,7);
    }
    else if(s.stage==7)
    {
        FusionCommand c;c.sourceId=s.a.id;c.sourceToken=s.a.token;c.action=FusionCommandKind::GlovePickup;int sender=0;const char* label="";
        if(s.step==0){sender=1;label="other_source";}
        else if(s.step==1){c.action=FusionCommandKind::GlovePlace;c.column=2;c.row=kRow;c.targetId=s.a.id;c.targetToken=s.a.token;label="self";}
        else if(s.step==2){c.action=FusionCommandKind::GlovePlace;c.sourceToken=s.a.token==0xffffffffU?1:s.a.token+1;c.column=3;c.row=kRow;label="generation";}
        else{sender=1;c.sourceId=s.a.id==65535?65534:65535;label="source";}
        if(Reject(a,sender,c,label,s.a,{})){if(++s.step==4)Advance(a,8);}return true;
    }
    else if(s.stage==8)
    {
        if(!Cancel(a,0)||!Holds(a,{},{}))return true;
        if(!Require(a,ctx->gloveCooldownTicks==0&&ByRef(b,s.a)&&ByRef(b,s.a)->mPlantCol==2,"Right cancel mutated source/CD"))return true;
        if(!Business(a)||!Barrier(a,"FG74_CANCELED"))return true;NetTrace("SMOKE_FUSION_GLOVE_CANCEL role=%s source=%u token=%u actorUnchanged=1 CD=0 nativeRightClick=1",Role(a),s.a.id,s.a.token);Advance(a,9);
    }
    else if(s.stage==9)
    {if(Pickup(a,0,s.a)&&Holds(a,s.a,{})&&Barrier(a,"FG74_MOVE_READY"))Advance(a,10);}
    else if(s.stage==10)
    {
        auto* p=ByRef(b,s.a);const auto* d=GetNativePlantData(p);if(!Require(a,p&&d,"Move source disappeared"))return true;
        if(slot==0&&!s.clicked)
        {if(p->mLaunchCounter<=45)return true;s.moveData=*d;s.moveTick=b->mMainCounter;s.moveLaunch=p->mLaunchCounter;s.moveHP=p->mPlantHealth;s.clicked=Cell(a,3);return true;}
        if(p->mPlantCol!=3)return true;
        if(!Require(a,p->mRow==kRow&&p->mPlantHealth==300&&d->firstParent==0&&d->secondParent==0&&d->owner==1&&d->packetId==Packet(b,1,0,true)->mNetId&&Holds(a,{},{})&&ctx->gloveCooldownTicks>0&&ctx->gloveCooldownTicks<=1000,
            "Ordinary glove move changed identity/provenance/HP or failed CD"))return true;
        if(!s.observed)
        {
            s.moveDelta=a->NetIsHost()?b->mMainCounter-s.moveTick:0;s.moveCD=ctx->gloveCooldownTicks;
            if(a->NetIsHost()&&!Require(a,s.moveDelta>=0&&s.moveDelta<s.moveLaunch&&p->mLaunchCounter==s.moveLaunch-s.moveDelta&&p->mShootingCounter==0&&p->mPlantHealth==s.moveHP&&d->animationSpeed==s.moveData.animationSpeed&&d->instanceToken==s.moveData.instanceToken,
                "Ordinary glove move reinitialized native counters/RNG/HP"))return true;
            s.observed=true;
        }
        if(!Business(a)||!Capture(a,"fusion_glove_moved")||!Barrier(a,"FG74_MOVED"))return true;
        NetTrace("SMOKE_FUSION_GLOVE_MOVED role=%s id=%u token=%u col=3 row=2 HP=300 owner=1 parents=0,0 sameInstance=1 noInit=1 CDStarted=1 observedCD=%d deltaTicks=%d",Role(a),s.a.id,s.a.token,s.moveCD,s.moveDelta);Advance(a,11);
    }
    else if(s.stage==11)
    {
        auto* normal=Packet(b,0,0,false);auto* extra=Packet(b,1,0,true);if(!normal->mActive||normal->mRefreshing||!extra->mActive||extra->mRefreshing)return true;
        if(!s.observed)NetTrace("SMOKE_FUSION_GLOVE_RECHARGED role=%s normalCD=750 extraCD=750 actualReady=1 noTimerWrites=1",Role(a));
        if(Purchase(a,0,0,false,6,0,700,600,s.parentB,"FG74_PARENT_B"))Advance(a,12);
    }
    else if(s.stage==12)
    {
        if(ctx->gloveCooldownTicks)return true;if(!Business(a)||!Pickup(a,0,s.a)||!Holds(a,s.a,{}))return true;
        FusionCommand c;c.action=FusionCommandKind::GlovePlace;c.sourceId=s.a.id;c.sourceToken=s.a.token;c.targetId=65000;c.targetToken=s.parentB.token;c.column=6;c.row=kRow;
        if(Reject(a,0,c,"target_identity",s.a,{}))Advance(a,13);
    }
    else if(s.stage==13)
    {if(Cancel(a,0)&&Holds(a,{},{})&&Business(a)&&Barrier(a,"FG74_UNSUPPORTED_CANCEL"))Advance(a,14);}
    else if(s.stage==14)
    {if(Purchase(a,1,0,true,6,1030,600,400,s.b,"FG74_DOUBLE_B"))Advance(a,15);}
    else if(s.stage==15)
    {
        if(!Business(a)||!Pickup(a,slot,slot==0?s.a:s.b)||!Holds(a,s.a,s.b)||!Capture(a,"fusion_glove_twohands")||!Barrier(a,"FG74_TWO_HANDS"))return true;
        NetTrace("SMOKE_FUSION_GLOVE_TWOHANDS role=%s first=%u/%u second=%u/%u originalsAlive=1 globalCD=0",Role(a),s.a.id,s.a.token,s.b.id,s.b.token);Advance(a,16);
    }
    else if(s.stage==16)
    {
        FusionCommand c;c.action=FusionCommandKind::GlovePlace;c.sourceId=s.a.id;c.sourceToken=s.a.token;c.targetId=s.b.id;c.targetToken=s.b.token;c.column=6;c.row=kRow;
        if(Reject(a,0,c,"held_target",s.a,s.b))Advance(a,17);
    }
    else if(s.stage==17)
    {if(Cancel(a,1)&&Holds(a,s.a,{})&&Business(a)&&Barrier(a,"FG74_TARGET_FREED"))Advance(a,18);}
    else if(s.stage==18)
    {
        if(slot==0&&!s.clicked){if(!Holds(a,s.a,{}))return true;s.clicked=Cell(a,6);return true;}
        auto* p=At(b,6);const auto* d=GetNativePlantData(p);if(!d||d->kind!=1032)return true;s.gatling=Identity(p);
        if(!Require(a,s.gatling.id!=s.a.id&&s.gatling.id!=s.b.id&&s.gatling.token&&d->firstParent==1030&&d->secondParent==1030&&d->owner==1&&d->packetId==Packet(b,1,0,true)->mNetId&&
            !ByRef(b,s.a)&&!ByRef(b,s.b)&&Holds(a,{},{})&&p->mPlantHealth==300&&ctx->gloveCooldownTicks>0,"Plant-to-plant glove fusion lost originals/ancestry/provenance/CD"))return true;
        if(!Business(a)||!Capture(a,"fusion_glove_gatling")||!Barrier(a,"FG74_1032"))return true;
        NetTrace("SMOKE_FUSION_GLOVE_CRAFT role=%s kind=1032 id=%u token=%u incomingOwner=1 operator=0 parents=1030,1030 sun=400 noCardUse=1 actualGloveUI=1",Role(a),s.gatling.id,s.gatling.token);Advance(a,19);
    }
    else if(s.stage==19)
    {
        if(!Business(a))return true;if(!Barrier(a,"FG74_SETUP"))return true;if(a->NetIsHost()&&!s.setup&&!SetupTargets(a))return true;
        if(a->NetIsHost()&&!Send(a,Ids()))return true;if(!ReadIds(a))return true;for(auto id:s.targets)if(!Target(b,id))return true;
        if(!Barrier(a,"FG74_TARGETS"))return true;BeginQuad(a,1032);Advance(a,20);
    }
    else if(s.stage==20)
    {
        if(s.quad.hits<8||!s.frames.contains("fusion_glove_quad1032")||!Capture(a,"fusion_glove_combat1032")||LiveShots(b))return true;
        if(!Require(a,a->NetIsHost()||s.quad.clientProjectile,"Client missed authoritative four-event1032 shots"))return true;
        if(!Barrier(a,"FG74_COMBAT1032"))return true;NetTrace("SMOKE_FUSION_GLOVE_COMBAT role=%s kind=1032 hits=8 damage=20 front=4840 rear=5000 adjacent=5000",Role(a));s.quad.begun=false;Advance(a,21);
    }
    else if(s.stage==21)
    {
        if(Purchase(a,0,2,false,6,1017,400,250,s.cherry,"FG74_1017")&&Capture(a,"fusion_glove_cherrygatling"))Advance(a,22);
    }
    else if(s.stage==22)
    {if(Barrier(a,"FG74_TARGETS1017")){BeginQuad(a,1017);Advance(a,23);}}
    else if(s.stage==23)
    {
        if(s.quad.hits<8||!s.frames.contains("fusion_glove_quad1017")||!Capture(a,"fusion_glove_combat1017")||LiveShots(b))return true;
        if(!Require(a,a->NetIsHost()||s.quad.clientProjectile,"Client missed authoritative four-event1017 shots"))return true;
        if(!Barrier(a,"FG74_COMBAT1017"))return true;NetTrace("SMOKE_FUSION_GLOVE_COMBAT role=%s kind=1017 hits=8 damage=60 front=4360 rear=5000 adjacent=5000",Role(a));s.quad.begun=false;Advance(a,24);
    }
    else if(s.stage==24)
    {
        if(!Business(a)||ctx->gloveCooldownTicks||!Holds(a,{},{})||Samples(a)<8||Samples(a)-s.purchaseSamples<2||!Barrier(a,"FG74_DONE")||!Capture(a,"fusion_glove_complete"))return true;
        NetTrace("SMOKE_FUSION_GLOVE_COMPLETE role=%s kind=1017 id=%u token=%u sun=250 normalUses=2 extraUses=2 cherryUses=1 sourceCDs=750,750,5000 gloveCD=0 twoQuadKinds=1 noPlantHealthWrites=1 noPlantTimerWrites=1 nativeBoard=1 secondWorld=0",Role(a),s.cherry.id,s.cherry.token);
        Advance(a,25);
    }
    else if(s.stage==25){if(SetupLayers(a))Advance(a,26);}
    else if(s.stage==26)
    {
        const int operatorSlot=s.layerIndex==1?1:0;
        if(!s.observed)
        {
            if(ctx->gloveCooldownTicks||!LayerState(a,s.layerIndex)||!Pickup(a,operatorSlot,s.layers[s.layerIndex]))return true;
            s.observed=true;
            NetTrace("SMOKE_FUSION_GLOVE_LAYER_PICKUP role=%s index=%d kind=%u id=%u token=%u holder=%d canonicalObserved=1 snapshots=%u",Role(a),s.layerIndex,layerKinds[s.layerIndex],s.layers[s.layerIndex].id,s.layers[s.layerIndex].token,operatorSlot,Samples(a));
        }
        // Every peer must have observed this transient hand before its owner
        // can drop. Keep the observation latched while the paced ACK arrives.
        if(Barrier(a,"FG187_LAYER_PICKED_"+std::to_string(s.layerIndex)))Advance(a,27);
    }
    else if(s.stage==27&&MoveLayer(a)){if(++s.layerIndex==3)Advance(a,28);else Advance(a,26);}
    else if(s.stage==28)
    {
        if(ctx->gloveCooldownTicks||!Holds(a,{},{})||!LayerState(a,3)||!Barrier(a,"FG187_LAYER_DONE")||!Capture(a,"fusion_glove_layers_complete"))return true;
        NetTrace("SMOKE_FUSION_GLOVE_LAYERS_COMPLETE role=%s actors=3 moves=3 layers=2 bodyControl=1 sourceKinds=0,24,27 sameGenerations=1 cooldown=0 paidCrafts=0 postSetupHPOrTimerWrites=0",Role(a));
        Advance(a,29);a->CoopSmokeWriteResult(true,nullptr);
    }
    return true;
}
}
