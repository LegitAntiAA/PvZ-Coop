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
#include "../Fusion/FusionNativeProducer.h"
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
struct Producer
{
    Ref ref{};int kind=0,row=0,tick=0,counter=0,pending=0,hp=0,flags=0;
    int due=-1,triggerTick=-1,triggers=0,callbacks=0,coins=0,rewards=0,biteCallbacks=0;
    bool alive=true,death=false;
};
struct Emission {uint16_t actor=0;int kind=0,row=0,count=0,tick=0;const char* reason="";};
struct State
{
    int stage=0,waitingOwn=0,money=500,sunUses=0,extraUses=0,nutUses=0,peaUses=0,playingTick=-1;
    int moveTick=0,moveCounter=0,movePending=0,moveFlags=0,moveHP=0,moveCD=0,moveDelta=0,heldTick=-1;
    NativePlantData moveData{};Ref flower{},twin{},nut{},pea{};uint16_t eater=0,collectId=0;int collectX=0,collectY=0;
    uint32_t since=0,lastChat=0,introStart=0,introLast=0,introChecks=0,pickSamples=0,purchaseSamples=0,collectSamples=0;
    bool configured=false,clicked=false,placed=false,observed=false,toolClicked=false,collecting=false,collected=false,biterSetup=false,heldActive=false,movePendingProof=false;
    std::map<uint16_t,Producer> producers;std::map<uint16_t,int> coins;std::set<std::string> frames;std::map<std::string,int> firstDraw;
    std::vector<Emission> expected;
} s;
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool ok,const char* why){if(!ok)a->CoopSmokeFail(why);return ok;}
void Advance(LawnApp* a,int stage)
{
    s.stage=stage;s.since=GetTickCount();s.clicked=s.placed=s.observed=s.toolClicked=false;
    LawnNet::NetTrace("SMOKE_FUSION_PRODUCER_STAGE role=%s stage=%d",Role(a),stage);
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
    if(!Require(a,a->CoopSmokeCaptureFrame(name),"Producer native renderer capture failed"))return false;
    s.frames.insert(name);LawnNet::NetTrace("SMOKE_FUSION_PRODUCER_RENDER role=%s frame=%s freshDraws=%d",Role(a),name,a->mDrawCount-s.firstDraw[name]);return true;
}
bool Click(LawnApp* a,float x,float y,uint8_t button=SDL_BUTTON_LEFT,bool click=true)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    if(!click)return Require(a,SDL_PushEvent(&m),"Producer scoped motion failed");
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=button;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;
    return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Producer scoped SDL click failed");
}
bool BoardClick(LawnApp* a,float x,float y,uint8_t button=SDL_BUTTON_LEFT,bool click=true)
{return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY,button,click);}
bool Cell(LawnApp* a,int col,int row=kRow,uint8_t button=SDL_BUTTON_LEFT,bool click=true)
{return BoardClick(a,float(a->mBoard->GetCellCenterX(col,row)),float(a->mBoard->GridToPixelY(col,row)+50),button,click);}
bool Ready(LawnApp* a,GameButton* b)
{
    if(!b||b->mDisabled||b->mBtnNoDraw)return false;float x=b->mX+b->mWidth*.5f,y=b->mY+b->mHeight*.5f;
    if(!b->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"Producer Ready hover failed");return false;}
    LawnNet::NetTrace("SMOKE_FUSION_PRODUCER_READY role=%s nativeSDL=1 hoverObserved=1",Role(a));return Click(a,x,y);
}
Plant* ByRef(Board* b,Ref r)
{Plant* p=nullptr;while(b->mPlants.IterateNext(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&p->mIsOnBoard&&p->mNetId==r.id&&d&&d->instanceToken==r.token)return p;}return nullptr;}
Plant* At(Board* b,int col,int row)
{Plant* p=nullptr;while(b->mPlants.IterateNext(p))if(!p->mDead&&p->mIsOnBoard&&p->mPlantCol==col&&p->mRow==row)return p;return nullptr;}
Ref Identity(Plant* p){const auto* d=GetNativePlantData(p);return p&&d?Ref{p->mNetId,d->instanceToken}:Ref{};}
SeedPacket* Packet(Board* b,int who,int kind,bool extra)
{for(int i=0;i<b->mSeedBank->mNumPackets;++i){auto* p=&b->mSeedBank->mSeedPackets[i];const auto* d=GetNativePacketData(p);if(d&&d->owner==who&&d->kind==kind&&d->isExtra==extra)return p;}return nullptr;}
bool HeldPacket(Board* b,SeedPacket* p)
{const auto* c=b->mCursorObject;return c&&p&&c->mCursorType==CURSOR_TYPE_PLANT_FROM_BANK&&c->mSeedBankIndex>=0&&c->mSeedBankIndex<b->mSeedBank->mNumPackets&&b->mSeedBank->mSeedPackets[c->mSeedBankIndex].mNetId==p->mNetId;}
bool PickPacket(LawnApp* a,SeedPacket* p)
{if(!Require(a,p&&p->mActive&&!p->mRefreshing,"Producer real packet not ready"))return false;auto* b=a->mBoard->mSeedBank;return BoardClick(a,float(b->mX+p->mX+p->mOffsetX+p->mWidth/2),float(b->mY+p->mY+p->mHeight/2));}
Producer* Record(Ref ref){auto i=s.producers.find(ref.id);return i!=s.producers.end()&&i->second.ref.token==ref.token?&i->second:nullptr;}
int Counter(Plant* p,const NativePlantData* d){return d->kind==1000?d->productionTicks:p->mLaunchCounter;}
bool Business(LawnApp* a)
{
    auto* b=a->mBoard;auto* flower=Packet(b,0,1,false);auto* extra=Packet(b,1,1,true);auto* nut=Packet(b,0,3,false);auto* pea=Packet(b,0,0,false);
    const bool money=s.collecting&&!s.collected?(b->mSunMoney==100||b->mSunMoney==125):b->mSunMoney==s.money;
    if(!Require(a,flower&&extra&&nut&&pea&&money&&flower->mRefreshTime==(s.sunUses?750:0)&&extra->mRefreshTime==(s.extraUses?750:0)&&nut->mRefreshTime==(s.nutUses?3000:0)&&pea->mRefreshTime==(s.peaUses?750:0)&&
        (!a->NetIsHost()||(flower->mTimesUsed==s.sunUses&&extra->mTimesUsed==s.extraUses&&nut->mTimesUsed==s.nutUses&&pea->mTimesUsed==s.peaUses)),"Producer actual prices/use/cooldown/ledger drifted"))return false;
    return true;
}
bool Purchase(LawnApp* a,int buyer,int ingredient,bool extra,int col,int row,int result,int before,int after,int first,int second,Ref& out,const char* gate)
{
    auto* b=a->mBoard;auto* card=Packet(b,buyer,ingredient,extra);
    if(Slot(a)==buyer&&!s.clicked){if(!card||!card->mActive||card->mRefreshing)return false;s.pickSamples=Samples(a);if(PickPacket(a,card))s.clicked=true;return false;}
    if(Slot(a)==buyer&&!s.placed){if(!HeldPacket(b,card)||(a->NetIsClient()&&Samples(a)<=s.pickSamples))return false;if(!Require(a,b->mSunMoney==before,"Producer packet pickup charged early"))return false;s.placed=Cell(a,col,row);return false;}
    if(!s.observed)
    {
        auto* p=At(b,col,row);const auto* d=GetNativePlantData(p);if(!d||d->kind!=result)return false;const int hp=result==3||result==1006?4000:300;
        if(!Require(a,card&&d->instanceToken&&d->owner==buyer&&d->packetId==card->mNetId&&!d->randomOrigin&&d->firstParent==first&&d->secondParent==second&&p->mPlantHealth==hp&&b->mSunMoney==after,"Producer purchase/health/recipe/provenance mismatch"))return false;
        out=Identity(p);s.money=after;if(ingredient==1){if(extra)++s.extraUses;else ++s.sunUses;}else if(ingredient==3)++s.nutUses;else ++s.peaUses;s.purchaseSamples=Samples(a);s.observed=true;
    }
    if(!Business(a)||!Barrier(a,gate))return false;
    LawnNet::NetTrace("SMOKE_FUSION_PRODUCER_PURCHASE role=%s kind=%d id=%u token=%u packet=%u owner=%d extra=%d parents=%d,%d sun=%d nativeSDL=1",Role(a),result,out.id,out.token,card->mNetId,buyer,extra?1:0,first,second,after);return true;
}
Zombie* Eater(Board* b){Zombie* z=nullptr;while(b->mZombies.IterateNext(z))if(!z->mDead&&z->mNetId==s.eater)return z;return nullptr;}
Coin* ByCoin(Board* b,uint16_t id){Coin* c=nullptr;while(b->mCoins.IterateNext(c))if(!c->mDead&&c->mNetId==id)return c;return nullptr;}
void Expect(Producer& p,int count,const char* reason,int tick)
{s.expected.push_back({p.ref.id,p.kind,p.row,count,tick,reason});}
bool Observe(LawnApp* a)
{
    auto* b=a->mBoard;s.expected.clear();std::set<uint16_t> live;Plant* p=nullptr;
    while(b->mPlants.IterateNext(p))
    {
        if(p->mDead||!p->mIsOnBoard)continue;const auto* d=GetNativePlantData(p);if(!d||!IsNativeProducerKind(d->kind))continue;live.insert(p->mNetId);
        const int counter=Counter(p,d);const auto* profile=FindFusionProducerProfile(d->kind);
        if(!Require(a,counter>0&&counter<=2701&&d->pendingProductionTicks>=0&&d->pendingProductionTicks<=50&&!(d->producerRewardFlags&~3)&&
            (d->kind==1006||!d->producerRewardFlags)&&(d->kind==1000||counter==d->productionTicks),"Producer snapshot/counter/mirror/flags malformed"))return false;
        auto it=s.producers.find(p->mNetId);
        if(it==s.producers.end())
        {
            if(!Require(a,d->instanceToken&&d->producerRewardFlags==0&&d->pendingProductionTicks==0&&counter>=390&&counter<=701,"Producer initial raw native deadline/state differs"))return false;
            Producer rec;rec.ref=Identity(p);rec.kind=d->kind;rec.row=p->mRow;rec.tick=b->mMainCounter;rec.counter=counter;rec.pending=d->pendingProductionTicks;rec.hp=p->mPlantHealth;rec.flags=d->producerRewardFlags;
            it=s.producers.emplace(p->mNetId,rec).first;
            LawnNet::NetTrace("SMOKE_FUSION_PRODUCER_INITIAL role=%s kind=%u id=%u token=%u observedCounter=%d tick=%d pending=0 flags=0 nativeStore=%s",Role(a),d->kind,p->mNetId,d->instanceToken,counter,b->mMainCounter,d->kind==1000?"metadata":"mLaunchCounter");
        }
        auto& rec=it->second;if(!Require(a,rec.ref.token==d->instanceToken&&rec.kind==d->kind&&rec.alive,"Producer generation/kind resurrected"))return false;
        if(a->NetIsHost()&&b->mMainCounter!=rec.tick)
        {
            const int dt=b->mMainCounter-rec.tick;
            if(!Require(a,dt==1,"Producer native tick observer skipped a timing boundary"))return false;
            if(rec.due==b->mMainCounter)
            {
                if(!Require(a,rec.pending==1&&d->pendingProductionTicks==0,"Producer Invoke did not emit on exact pending50 deadline"))return false;
                Expect(rec,profile->periodicCoinCount,"periodic",b->mMainCounter);++rec.callbacks;
                auto* eater=Eater(b);if(rec.kind==1006&&eater&&eater->mIsEating&&eater->FindPlantTarget(ATTACKTYPE_CHEW)==p)++rec.biteCallbacks;
                LawnNet::NetTrace("SMOKE_FUSION_PRODUCER_CALLBACK kind=%d id=%u token=%u trigger=%d tick=%d delay=50 count=%d HP=%d flags=%u whileBitten=%d",rec.kind,rec.ref.id,rec.ref.token,rec.triggerTick,b->mMainCounter,profile->periodicCoinCount,p->mPlantHealth,d->producerRewardFlags,rec.biteCallbacks);rec.due=-1;
            }
            if(counter>rec.counter)
            {
                const int interval=int(profile->intervalSeconds),jitter=(counter-1)/100-interval;
                if(!Require(a,rec.counter==1&&counter%100==1&&jitter>=-2&&jitter<=2&&d->pendingProductionTicks==50,"Producer strict underflow/reset integer jitter differs"))return false;
                rec.triggerTick=b->mMainCounter;rec.due=b->mMainCounter+50;++rec.triggers;
                LawnNet::NetTrace("SMOKE_FUSION_PRODUCER_TRIGGER kind=%d id=%u token=%u tick=%d cycle=%d interval=%d jitter=%d pending=50 strictNativeCounter=1",rec.kind,rec.ref.id,rec.ref.token,b->mMainCounter,counter,interval,jitter);
            }
            else if(!Require(a,counter==rec.counter-1,"Producer native countdown stopped/reset without source trigger"))return false;
            if(d->producerRewardFlags!=rec.flags)
            {
                const int added=d->producerRewardFlags&~rec.flags;
                if(!Require(a,rec.kind==1006&&(d->producerRewardFlags&rec.flags)==rec.flags&&(added==1||added==2)&&
                    (added==1?(p->mPlantHealth>1333&&p->mPlantHealth<2666):p->mPlantHealth<1333),"SunNut reward flags reset/repeated/wrong strict health band"))return false;
                ++rec.rewards;Expect(rec,1,added==1?"threshold1":"threshold2",b->mMainCounter);
                LawnNet::NetTrace("SMOKE_FUSION_PRODUCER_REWARD kind=1006 id=%u token=%u bit=%d HP=%d flags=%u count=1 value=25 tick=%d",rec.ref.id,rec.ref.token,added,p->mPlantHealth,d->producerRewardFlags,b->mMainCounter);
            }
        }
        rec.tick=b->mMainCounter;rec.counter=counter;rec.pending=d->pendingProductionTicks;rec.hp=p->mPlantHealth;rec.flags=d->producerRewardFlags;
    }
    for(auto& [id,rec]:s.producers)if(rec.alive&&!live.contains(id))
    {
        if(rec.kind==1006)
        {
            // Client cosmetic ticks cannot reconstruct an unsampled final bite.
            // The host proves lethalHP<=4; clients prove last low-band state,
            // retirement of this identity and the shared authoritative Coin IDs.
            const bool lastHealthValid=a->NetIsHost()?rec.hp<=DAMAGE_PER_EAT:rec.hp<1333;
            if(!Require(a,s.biterSetup&&rec.flags==3&&rec.hp>0&&lastHealthValid,"SunNut retired without natural lethal bite/full prior rewards"))return false;
            if(a->NetIsHost())
            {
                if(rec.due==b->mMainCounter){Expect(rec,1,"periodic",b->mMainCounter);++rec.callbacks;}
                Expect(rec,2,"death",b->mMainCounter);LawnNet::NetTrace("SMOKE_FUSION_PRODUCER_DEATH kind=1006 id=%u token=%u previousHP=%d flags=3 count=2 value=25 tick=%d nativeBite=1",id,rec.ref.token,rec.hp,b->mMainCounter);
            }
            rec.death=true;
        }
        rec.alive=false;rec.due=-1;
    }
    std::map<int,std::vector<uint16_t>> born;Coin* coin=nullptr;
    while(b->mCoins.IterateNext(coin))
    {
        if(coin->mDead||!coin->mNetId||s.coins.contains(coin->mNetId))continue;
        if(!Require(a,coin->mType==COIN_SUN&&coin->GetSunValue()==25&&coin->mCoinMotion==COIN_MOTION_FROM_PLANT&&!coin->mIsBeingCollected,"Producer callback created wrong realCoin payload"))return false;
        int row=-1;for(int candidate:{0,2,4})if(std::abs(coin->mPosY-b->GridToPixelY(0,candidate))<75){if(!Require(a,row<0,"Producer newCoin row ambiguous"))return false;row=candidate;}
        if(!Require(a,row>=0,"Producer newCoin outside controlled rows"))return false;s.coins[coin->mNetId]=row;born[row].push_back(coin->mNetId);
        LawnNet::NetTrace("SMOKE_FUSION_PRODUCER_COIN role=%s id=%u row=%d value=25 nativeCoin=1 tick=%d",Role(a),coin->mNetId,row,b->mMainCounter);
    }
    if(a->NetIsHost())
    {
        std::map<int,int> expectedCounts;for(const auto& e:s.expected)expectedCounts[e.row]+=e.count;
        for(int row:{0,2,4})if(!Require(a,int(born[row].size())==expectedCounts[row],"Producer callback did not create its complete exact realCoin count"))return false;
        std::map<int,size_t> offset;for(const auto& e:s.expected)
        {
            auto& rec=s.producers.at(e.actor);rec.coins+=e.count;std::ostringstream ids;
            for(int i=0;i<e.count;++i){if(i)ids<<',';ids<<born[e.row][offset[e.row]++];}
            LawnNet::NetTrace("SMOKE_FUSION_PRODUCER_EMISSION kind=%d actor=%u token=%u reason=%s tick=%d count=%d coinIds=%s value=25 nativeArray=1",e.kind,e.actor,rec.ref.token,e.reason,e.tick,e.count,ids.str().c_str());
        }
    }
    if(auto* nut=ByRef(b,s.nut))
    {
        const auto* d=GetNativePlantData(nut);if(d->producerRewardFlags&1)Capture(a,"fusion_producer_crack1");if(d->producerRewardFlags&2)Capture(a,"fusion_producer_crack2");
    }
    return true;
}
bool SetupEater(LawnApp* a)
{
    auto* b=a->mBoard;auto* p=ByRef(b,s.nut);auto* z=b->AddZombieInRow(ZOMBIE_NORMAL,2,0);auto* body=z?a->ReanimationTryToGet(z->mBodyReanimID):nullptr;const float rate=body?body->mAnimRate:0;
    if(!Require(a,p&&p->mPlantHealth==4000&&z&&z->mNetId&&body&&std::isfinite(rate)&&rate>0,"SunNut native biter setup failed"))return false;
    z->mBodyHealth=z->mBodyMaxHealth=5000;z->SetAnimRate(rate);z->mPosY=z->GetPosYBasedOnRow(2);z->mY=int(z->mPosY);bool found=false;
    for(int offset=80;offset>=-80;--offset){z->mPosX=float(p->mX+offset);z->mX=int(z->mPosX);if(z->FindPlantTarget(ATTACKTYPE_CHEW)==p){found=true;break;}}
    if(!Require(a,found,"SunNut real native chew target not acquired"))return false;s.eater=z->mNetId;s.biterSetup=true;
    LawnNet::NetTrace("SMOKE_FUSION_PRODUCER_BITER id=%u plant=%u token=%u plantHP=4000 biterHP=5000 initialBiterHPOnly=1 nativeEatPlant=1",s.eater,s.nut.id,s.nut.token);return true;
}
bool ReadEater(LawnApp* a)
{
    if(a->NetIsHost())return s.biterSetup;for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==0&&line.mText.rfind("FP89_BITER ",0)==0)
    {std::istringstream input(line.mText.substr(11));unsigned id=0;std::string extra;if(!Require(a,bool(input>>id)&&id&&id<=65535&&!(input>>extra),"Producer biter ID invalid"))return false;s.eater=uint16_t(id);s.biterSetup=true;return true;}return false;
}
bool Collect(LawnApp* a)
{
    auto* b=a->mBoard;s.collecting=true;
    if(!s.collectId)
    {
        if(a->NetIsHost())
        {
            Coin* coin=nullptr;while(b->mCoins.IterateNext(coin))if(!coin->mDead&&!coin->mIsBeingCollected&&coin->mHitGround&&s.coins.contains(coin->mNetId)&&s.coins[coin->mNetId]==4)
            {
                bool found=false;for(int y=int(coin->mPosY)-10;y<=coin->mPosY+coin->mHeight&&!found;y+=4)for(int x=int(coin->mPosX)-10;x<=coin->mPosX+coin->mWidth;x+=4)
                {HitResult hit{};if(b->MouseHitTest(x,y,&hit)&&hit.mObjectType==OBJECT_TYPE_COIN&&hit.mObject==coin){s.collectId=coin->mNetId;s.collectX=x;s.collectY=y;found=true;break;}}
                if(found)break;
            }
        }
        else for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==0&&line.mText.rfind("FP89_COIN ",0)==0)
        {std::istringstream in(line.mText.substr(10));unsigned id=0;std::string extra;if(!Require(a,bool(in>>id>>s.collectX>>s.collectY)&&id&&id<=65535&&!(in>>extra),"Producer collection point invalid"))return false;s.collectId=uint16_t(id);break;}
    }
    if(!s.collectId)return false;const std::string gate="FP89_COIN "+std::to_string(s.collectId)+' '+std::to_string(s.collectX)+' '+std::to_string(s.collectY);
    if(a->NetIsHost()&&!Send(a,gate))return false;
    if(Slot(a)==1&&!s.clicked)
    {auto* c=ByCoin(b,s.collectId);if(!c||!c->mHitGround)return false;HitResult hit{};if(!Require(a,b->MouseHitTest(s.collectX,s.collectY,&hit)&&hit.mObjectType==OBJECT_TYPE_COIN&&hit.mObject==c,"Producer client authoritative Coin point is not actual native hit"))return false;s.clicked=BoardClick(a,float(s.collectX),float(s.collectY));s.collectSamples=Samples(a);return false;}
    if(!Require(a,b->mSunMoney==100||b->mSunMoney==125,"Producer Coin collection credited an invalid amount"))return false;if(b->mSunMoney!=125)return false;s.money=125;s.collected=true;
    if(!Capture(a,"fusion_producer_collected")||!Barrier(a,"FP89_COLLECTED"))return false;
    LawnNet::NetTrace("SMOKE_FUSION_PRODUCER_COLLECT role=%s id=%u before=100 after=125 value=25 nativeClientSDL=1 oneCredit=1 manualSunWrites=0",Role(a),s.collectId);return true;
}
bool PickupNut(LawnApp* a)
{
    auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);auto* p=ByRef(b,s.nut);
    if(!Require(a,p&&GetNativePlantData(p)->kind==1006,"SunNut glove source vanished before pickup"))return false;
    if(Slot(a)==1)
    {
        if(!s.toolClicked){Sexy::Rect rect=b->GetShovelButtonRect();b->GetZenButtonRect(OBJECT_TYPE_GLOVE,rect);s.toolClicked=BoardClick(a,float(rect.mX+rect.mWidth/2),float(rect.mY+rect.mHeight/2));return false;}
        if(!s.clicked){if(b->mCursorObject->mCursorType!=CURSOR_TYPE_GLOVE)return false;s.clicked=Cell(a,p->mPlantCol,p->mRow);return false;}
    }
    const auto& held=ctx->gloveHolds[1];return held.plantId==s.nut.id&&held.instanceToken==s.nut.token&&GetNativeGloveHeldPlant(b,1)==p&&(Slot(a)!=1||b->mCursorObject->mCursorType==CURSOR_TYPE_PLANT_FROM_GLOVE);
}
bool MovePendingNut(LawnApp* a)
{
    auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);auto* p=ByRef(b,s.nut);const auto* d=GetNativePlantData(p);
    if(!Require(a,p&&d&&d->kind==1006&&p->mPlantHealth>0,"SunNut died before pending-preserving move"))return false;
    if(p->mPlantCol==5)
    {
        if(!PickupNut(a))return false;if(s.heldTick<0)s.heldTick=b->mMainCounter;
        if(b->mMainCounter-s.heldTick>=30)s.heldActive=true;
        if(a->NetIsHost())
        {
            s.moveData=*d;s.moveTick=b->mMainCounter;s.moveCounter=Counter(p,d);s.movePending=d->pendingProductionTicks;s.moveFlags=d->producerRewardFlags;s.moveHP=p->mPlantHealth;
            if(s.heldActive&&d->pendingProductionTicks>=40&&!Send(a,"FP89_PENDING_MOVE_READY"))return false;
        }
        if(!Capture(a,"fusion_producer_held")||!Has(a,0,"FP89_PENDING_MOVE_READY"))return false;
        if(Slot(a)==1&&!s.placed){s.placed=Cell(a,4,2);return false;}return false;
    }
    if(!Require(a,p->mPlantCol==4&&p->mRow==2&&!ctx->gloveHolds[1].plantId&&ctx->gloveCooldownTicks>0&&ctx->gloveCooldownTicks<=1000,"SunNut ordinary pending move/lease/cooldown mismatch"))return false;
    if(!s.observed)
    {
        s.moveDelta=a->NetIsHost()?b->mMainCounter-s.moveTick:0;s.moveCD=ctx->gloveCooldownTicks;
        if(a->NetIsHost()&&!Require(a,s.moveDelta>=0&&Counter(p,d)==s.moveCounter-s.moveDelta&&d->pendingProductionTicks==s.movePending-s.moveDelta&&d->pendingProductionTicks>0&&
            (d->producerRewardFlags&s.moveFlags)==s.moveFlags&&d->packetId==s.moveData.packetId&&d->owner==s.moveData.owner&&d->instanceToken==s.moveData.instanceToken&&d->firstParent==s.moveData.firstParent&&d->secondParent==s.moveData.secondParent&&p->mPlantHealth<=s.moveHP&&p->mPlantHealth>=s.moveHP-s.moveDelta*DAMAGE_PER_EAT,
            "SunNut move reset/paused production pending/flags/health/provenance"))return false;s.movePendingProof=true;s.observed=true;
    }
    if(!Business(a)||!Capture(a,"fusion_producer_moved")||!Barrier(a,"FP89_MOVED"))return false;
    LawnNet::NetTrace("SMOKE_FUSION_PRODUCER_MOVE role=%s kind=1006 id=%u token=%u from=5 to=4 sameInstance=1 heldAlive=1 pendingPreserved=1 countdownPreserved=1 flagsPreserved=1 HPNotReset=1 expectedCD=1000 observedCD=%d",Role(a),s.nut.id,s.nut.token,s.moveCD);return true;
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionProducer(LawnApp* a)
{
    const char* native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE"),*enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_PRODUCER");
    if(!a||!native||std::strcmp(native,"1")||!enabled||std::strcmp(enabled,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
    if(a->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();if(GetTickCount()-s.since>(s.stage<=3||s.stage>=15?90000U:30000U)){char error[90];sprintf_s(error,"Producer deadline stage=%d role=%s",s.stage,Role(a));a->CoopSmokeFail(error);return true;}
    if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
    if(s.stage==0)
    {
        if(a->NetIsHost()&&!s.configured){if(!Require(a,!a->mBoard&&!a->mSeedChooserScreen,"Producer requires clean native lobby"))return true;a->mNetManager->SetDifficulty(1);s.configured=true;}
        if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
        if(a->NetIsClient()){Advance(a,1);Send(a,"FP89_START");}else if(Has(a,1,"FP89_START")){Advance(a,1);std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;
    }
    if(!a->mBoard||!IsNativeFusionBoard(a->mBoard)){if(s.stage==1&&a->NetIsClient())Send(a,"FP89_START");return true;}
    auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);auto* chooser=a->mSeedChooserScreen;const int slot=Slot(a);
    if(!Require(a,ctx&&ctx->validation&&!a->mFusionRuntime&&b->GetColumnCount()==10&&b->GetCellPitchX()==72,"Producer lost actual native10x5 Board"))return true;
    const auto& bank=ctx->selection.State();if(s.stage>=5&&!Observe(a))return true;
    if(s.stage==1)
    {
        if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||!b->mCutScene->mSeedChoosing||chooser->mSeedsInFlight)return true;
        const size_t target=(std::min)(kNativeFusionBankCapacity,ctx->eligible.size());if(!Require(a,ctx->eligible.size()>=16&&bank.activeMask==3&&bank.targetCount==target&&bank.quotas[0]+bank.quotas[1]==target,"Producer actual chooser pool/quota mismatch"))return true;
        if(!Capture(a,"fusion_producer_chooser"))return true;std::vector<int> choices;for(size_t i=0;i<ctx->eligible.size();++i)if(ctx->eligible[i].isExtra==(slot==1))choices.push_back(int(i));
        auto priority=[&](int i){const int kind=ctx->eligible[i].kind;return kind==1?0:kind==3?1:kind==0?2:10+kind;};std::stable_sort(choices.begin(),choices.end(),[&](int x,int y){return priority(x)<priority(y);});
        int own=0;for(const auto& packet:bank.packets)own+=packet.owner==slot;if(own<s.waitingOwn)return true;if(!Require(a,choices.size()>=bank.quotas[slot],"Producer chooser candidates missing"))return true;
        if(own<bank.quotas[slot]){int x=0,y=0;chooser->GetSeedPositionInChooser(choices[own],x,y);if(Click(a,float(chooser->mX+x+25),float(chooser->mY+y+35-int(chooser->mScrollbar->mScrollValue))))s.waitingOwn=own+1;return true;}
        if(bank.packets.size()!=target||!Packet(b,0,1,false)||!Packet(b,1,1,true)||!Packet(b,0,3,false)||!Packet(b,0,0,false))return true;int rank=-1;
        for(int i=0;i<b->mSeedBank->mNumPackets;++i){const int next=NetSeedBankOwnerRank(slot,b->mSeedBank->mSeedPackets[i].mNetOwnerSlot);if(!Require(a,next>=rank,"Producer own-first bank layout drifted"))return true;rank=next;}
        if(!Capture(a,"fusion_producer_picked"))return true;NetTrace("SMOKE_FUSION_PRODUCER_POOL role=%s eligible=%zu target=%u quota0=%u quota1=%u ownFirst=1 unfiltered=1 nativeChooser=1",Role(a),ctx->eligible.size(),bank.targetCount,bank.quotas[0],bank.quotas[1]);Advance(a,2);
    }
    else if(s.stage==2){if(bank.phase!=FusionBankPhase::Choosing)Advance(a,3);else if(chooser&&!chooser->mSeedsInFlight&&Ready(a,chooser->mStartButton))Advance(a,3);}
    else if(s.stage==3)
    {
        if(bank.phase==FusionBankPhase::Intro){if(!s.introStart)s.introStart=GetTickCount();s.introLast=GetTickCount();++s.introChecks;Capture(a,"fusion_producer_intro");return true;}
        if(bank.phase!=FusionBankPhase::Playing||a->mGameScene!=SCENE_PLAYING||chooser)return true;if(!Require(a,s.introChecks>=10&&s.introLast-s.introStart>=1000&&s.frames.contains("fusion_producer_intro")&&b->mSunMoney==500,"Producer skipped full native intro/fresh500"))return true;s.playingTick=b->mMainCounter;Advance(a,4);
    }
    else if(s.stage==4)
    {
        if(b->mMainCounter-s.playingTick<2||!Capture(a,"fusion_producer_playing")||!Barrier(a,"FP89_PLAYING"))return true;if(a->NetIsHost()){b->mSunCountDown=100000;b->mZombieCountDown=b->mZombieCountDownStart=100000;}
        NetTrace("SMOKE_FUSION_PRODUCER_PLAYING role=%s sun=500 boardTicksSincePlaying=%d nativeBoard=1 secondWorld=0 manualSunWrites=0",Role(a),b->mMainCounter-s.playingTick);Advance(a,5);
    }
    else if(s.stage==5){if(Purchase(a,0,1,false,0,0,1,500,450,kFusionNoPlant,kFusionNoPlant,s.flower,"FP89_FLOWER"))Advance(a,6);}
    else if(s.stage==6)
    {
        auto* r=Record(s.flower);if(!r)return true;
        if(a->NetIsHost()&&r->callbacks>=1&&!Send(a,"FP89_SINGLE_EMITTED"))return true;if(!Has(a,0,"FP89_SINGLE_EMITTED")||!Capture(a,"fusion_producer_single")||!Barrier(a,"FP89_SINGLE"))return true;Advance(a,7);
    }
    else if(s.stage==7){if(Purchase(a,1,1,true,0,0,1033,450,350,1,1,s.twin,"FP89_TWIN"))Advance(a,8);}
    else if(s.stage==8){if(Purchase(a,0,3,false,5,2,3,350,300,kFusionNoPlant,kFusionNoPlant,s.nut,"FP89_NUT"))Advance(a,9);}
    else if(s.stage==9){if(Purchase(a,0,1,false,5,2,1006,300,250,3,1,s.nut,"FP89_SUNNUT"))Advance(a,10);}
    else if(s.stage==10){if(Purchase(a,0,0,false,0,4,0,250,150,kFusionNoPlant,kFusionNoPlant,s.pea,"FP89_PEA"))Advance(a,11);}
    else if(s.stage==11){if(Purchase(a,0,1,false,0,4,1000,150,100,0,1,s.pea,"FP89_PEASUN"))Advance(a,12);}
    else if(s.stage==12){if(!Business(a)||!Capture(a,"fusion_producer_three")||!Barrier(a,"FP89_THREE"))return true;Advance(a,13);}
    else if(s.stage==13){if(Collect(a))Advance(a,14);}
    else if(s.stage==14)
    {
        if(a->NetIsHost()&&!s.biterSetup&&!SetupEater(a))return true;if(a->NetIsHost()&&!Send(a,"FP89_BITER "+std::to_string(s.eater)))return true;if(!ReadEater(a)||!Eater(b)||!Barrier(a,"FP89_BITER_READY"))return true;Advance(a,15);
    }
    else if(s.stage==15)
    {
        auto* p=ByRef(b,s.nut);const auto* d=GetNativePlantData(p);if(!Require(a,p&&d,"SunNut died before first natural reward band"))return true;if(!(d->producerRewardFlags&1)||!s.frames.contains("fusion_producer_crack1")||!Barrier(a,"FP89_FIRST_REWARD"))return true;Advance(a,16);
    }
    else if(s.stage==16){if(MovePendingNut(a))Advance(a,17);}
    else if(s.stage==17)
    {
        auto* r=Record(s.nut);if(!r||!r->death||!s.frames.contains("fusion_producer_crack2"))return true;if(a->NetIsHost()&&!Require(a,r->rewards==2&&r->biteCallbacks>=1&&r->coins>=5&&s.movePendingProof,"SunNut reward/death/bitten periodic/pending move coverage missing"))return true;
        if(!Capture(a,"fusion_producer_death")||!Barrier(a,"FP89_DEAD"))return true;NetTrace("SMOKE_FUSION_PRODUCER_NUT_DONE role=%s id=%u token=%u flags=3 thresholdsOnce=2 deathCoins=2 bitWhileProducing=1 pendingMove=1 naturalHP4000to0=1",Role(a),s.nut.id,s.nut.token);Advance(a,18);
    }
    else if(s.stage==18)
    {
        if(a->NetIsHost())
        {
            auto* twin=Record(s.twin);auto* pea=Record(s.pea);if(!twin||!pea||twin->callbacks<2||pea->callbacks<2)return true;if(!Send(a,"FP89_CYCLES_DONE"))return true;
        }
        if(!Has(a,0,"FP89_CYCLES_DONE")||!Business(a)||ctx->gloveCooldownTicks||Samples(a)<8||!Barrier(a,"FP89_DONE")||!Capture(a,"fusion_producer_complete"))return true;
        NetTrace("SMOKE_FUSION_PRODUCER_COMPLETE role=%s twin=%u/%u pea=%u/%u nut=%u/%u sun=125 source1NormalUses=3 source1ExtraUses=1 nutUses=1 peaUses=1 collected25=1 sourceProducerKinds=4 manualSunWrites=0 plantHealthWrites=0 plantTimerWrites=0 nativeBoard=1 secondWorld=0",Role(a),s.twin.id,s.twin.token,s.pea.id,s.pea.token,s.nut.id,s.nut.token);Advance(a,19);a->CoopSmokeWriteResult(true,nullptr);
    }
    return true;
}
}
