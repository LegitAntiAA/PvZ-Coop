#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetCommon.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Projectile.h"
#include "../Coin.h"
#include "../SeedPacket.h"
#include "../Cutscene.h"
#include "../Widget/GameButton.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativeVisual.h"
#include "../Fusion/FusionNativeShooterBatch.h"
#include "../Fusion/FusionNativeBasePlants.h"
#include "../Fusion/FusionRecipe.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../SexyAppFramework/ScrollbarWidget.h"
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
constexpr uint16_t kinds[4][6]={{1117,1118,1119,1149,1126,0},{1079,1081,1159,1160,1011,0},{1135,1005,901,907,933,0},{1079,1081,0,0,0,0}};
constexpr int counts[]={5,5,5,2};
constexpr int targetHP=20000000;
constexpr int armorHP=10000;
struct Ref { uint16_t id=0;uint32_t token=0; };
struct State
{
    int group=0,stage=0,waiting=0,playingFrame=-1,baselineTick=0,introChecks=0;
    uint32_t epoch=0,previousEpoch=0,since=0,chat=0,wheel=0,diagnostic=0,pauseSample=0,introFirst=0,introLast=0;
    bool configured=false,setup=false,pauseRequested=false,resumed=false,finalPause=false,finalLogged=false;
    std::array<Ref,6> actors{};
    std::array<bool,6> phaseSeen{},rageSeen{};
    std::array<uint32_t,6> sequence{};
    std::array<int,6> callbacks{};
    std::vector<uint16_t> targets;
    std::set<uint16_t> payloads,projectiles;
    std::set<std::string> frames;
    std::map<std::string,int> draws;
    bool tier101=false,tier301=false,skill=false,skillBullet=false,chomp=false,pit=false,sun=false;
    bool moneyTool=false,moneyClicked=false,moneyUpgrade=false,goldMiusa=false,split=false;
    int moneySun=0;
    std::string ids;
} s;
int Count(){return counts[s.group];}
//1081 atcol7: vertical rays stay x>=558, right rays x>=589; armor34 ends
//atx385. The left ray stays y<=253 and is row1-only, so all five source
//rays (+90,-90,180,+30,-30) miss the strict row3/col4 armor receiver.
int Column(int i){return s.group==1&&i==1?7:2;}
int Row(int i){return i;}
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool ok,const char* why){if(!ok)a->CoopSmokeFail(why);return ok;}
void Advance(LawnApp* a,int next){s.stage=next;s.since=GetTickCount();NetTrace("SMOKE_EXTENDED_SHOOTER40_STAGE role=%s group=%d epoch=%u stage=%d",Role(a),s.group,s.epoch,next);}
std::string Tag(const char* name){return "A266_"+std::to_string(s.group)+"_"+std::to_string(s.epoch)+"_"+name;}
bool Has(LawnApp* a,int slot,const std::string& text){for(const auto& l:a->mNetManager->mChatLines)if(l.mSlot==slot&&l.mText==text)return true;return false;}
bool Send(LawnApp* a,const std::string& text)
{if(Has(a,Slot(a),text))return true;if(s.chat&&GetTickCount()-s.chat<800)return false;s.chat=GetTickCount();a->mNetManager->SendChat(text);return Has(a,Slot(a),text);}
bool Barrier(LawnApp* a,const char* name){const auto tag=Tag(name);Send(a,tag);return Has(a,0,tag)&&Has(a,1,tag);}
bool Click(LawnApp* a,float x,float y)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;
    return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"ExtendedShooter40 owned SDL click failed");
}
bool Key(LawnApp* a)
{SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"ExtendedShooter40 owned SDL Escape failed");}
bool Choice(LawnApp* a,SeedChooserScreen* c,int index)
{
    int x=0,y=0;c->GetSeedPositionInChooser(index,x,y);const float cy=float(y+35-int(c->mScrollbar->mScrollValue));
    if(cy<158||cy>493)
    {
        if(s.wheel&&GetTickCount()-s.wheel<250)return false;s.wheel=GetTickCount();
        SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=float(c->mX+300);m.motion.y=float(c->mY+300);
        SDL_Event w{};w.type=SDL_EVENT_MOUSE_WHEEL;w.wheel.windowID=m.motion.windowID;w.wheel.y=cy<158?1.f:-1.f;
        Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&w),"ExtendedShooter40 owned SDL chooser scroll failed");return false;
    }
    return std::abs(c->mScrollAmount)<=.05f&&Click(a,float(c->mX+x+25),float(c->mY)+cy);
}
bool Capture(LawnApp* a,const char* suffix)
{
    const auto name="fusion_extended_shooter40_"+std::to_string(s.group)+"_"+suffix;
    if(s.frames.contains(name))return true;
    if(!s.draws.contains(name)){s.draws[name]=a->mDrawCount;return false;}
    if(a->mDrawCount-s.draws[name]<2)return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"ExtendedShooter40 actual native renderer capture failed"))return false;
    s.frames.insert(name);NetTrace("SMOKE_EXTENDED_SHOOTER40_RENDER role=%s group=%d epoch=%u frame=%s freshDraws=%d",Role(a),s.group,s.epoch,name.c_str(),a->mDrawCount-s.draws[name]);return true;
}
Plant* Actor(Board* b,int index)
{
    Plant* p=nullptr;while(b->mPlants.IterateNext(p))
    {
        const auto* d=GetNativePlantData(p);
        if(!p->mDead&&p->mIsOnBoard&&d&&(d->kind==kinds[s.group][index]||(s.group==2&&index==0&&d->kind==1134))&&p->mPlantCol==Column(index)&&p->mRow==Row(index))return p;
    }
    return nullptr;
}
Zombie* Target(Board* b,uint16_t id)
{Zombie* z=nullptr;while(b->mZombies.IterateNext(z))if(!z->mDead&&z->mNetId==id)return z;return nullptr;}
bool SpawnTarget(LawnApp* a,int row,float center,int health,bool armor=false)
{
    auto* b=a->mBoard;auto* z=b->AddZombieInRow(ZOMBIE_NORMAL,row,0);
    if(!Require(a,z&&z->mNetId,"ExtendedShooter40 native normal target factory failed"))return false;
    z->mBodyHealth=z->mBodyMaxHealth=health;z->mHelmHealth=z->mHelmMaxHealth=z->mShieldHealth=z->mShieldMaxHealth=0;
    if(armor)
    {
        z->mHelmType=HELMTYPE_BUCKET;z->mShieldType=SHIELDTYPE_DOOR;
        z->mHelmHealth=z->mHelmMaxHealth=z->mShieldHealth=z->mShieldMaxHealth=armorHP;
    }
    z->mPosY=z->GetPosYBasedOnRow(row);z->mY=int(z->mPosY);const auto r=z->GetZombieRect();
    z->mPosX+=center-(r.mX+r.mWidth*.5f);z->mX=int(z->mPosX);z->mVelX=0;z->ApplyAnimRate(0);
    s.targets.push_back(z->mNetId);
    NetTrace("SMOKE_EXTENDED_SHOOTER40_TARGET_SETUP group=%d epoch=%u index=%zu id=%u row=%d hp=%d helm=%d shield=%d x=%.6g y=%.6g initialOnly=1 nativeNormalControl=1 armorComponentFixture=%d sourceNPCParity=NotExecuted",s.group,s.epoch,s.targets.size()-1,z->mNetId,row,health,z->mHelmHealth,z->mShieldHealth,z->mPosX,z->mPosY,armor?1:0);
    return true;
}
bool Setup(LawnApp* a)
{
    auto* b=a->mBoard;
    if(a->NetIsHost()&&!s.setup)
    {
        if(!Require(a,!b->mPaused&&!b->mPlants.mSize&&!b->mZombies.mSize,"ExtendedShooter40 setup requires fresh unpaused native world"))return false;
        constexpr uint16_t batchKinds[]={1117,1118,1119,1149,1126,1079,1081,1159,1160,1011,1134,1135,1005,901,907,933,20};
        std::string error;
        const bool loaded=EnsureNativeVisuals(b,batchKinds,{},&error);
        if(!Require(a,loaded,error.c_str()))return false;
        NetTrace("SMOKE_EXTENDED_SHOOTER40_PREFLIGHT sourceModels=17 nativeLoad=1");
        GetNativeContext(b)->money=3000; // Explicit initial ledger fixture, before either peer's baseline.
        b->mSunCountDown=b->mZombieCountDown=b->mZombieCountDownStart=100000;
        for(int i=0;i<Count();++i)
        {
            NativePlantData typed;typed.kind=s.group==2&&i==0?1134:kinds[s.group][i];typed.owner=uint8_t(s.group==2&&i==0?1:i%2);
            for(size_t n=0;n<GetFusionRecipeCount();++n)
            {
                const auto& recipe=GetFusionRecipes()[n];
                if(recipe.result==typed.kind){typed.firstParent=recipe.existing;typed.secondParent=recipe.used;break;}
            }
            if(!IsNativeBasePlantKind(typed.kind)&&typed.firstParent==kFusionNoPlant&&typed.secondParent==kFusionNoPlant)
                typed.owner=LawnNet::NET_SHARED_OWNER; // Unpaid world fixture, never fabricated parent ancestry.
            std::string error;
            if(!Require(a,EnsureNativePlantVisuals(b,typed.kind,&error),"ExtendedShooter40 source visual admission failed"))return false;
            // Explicit initial fixture factory. Paid acquisition, recipes and
            // the three source standalone unlocks are not acceptance claims.
            auto* p=b->AddPlant(Column(i),Row(i),GetNativeSeedForPlant(typed.kind),SEED_NONE,&typed);
            const auto* d=GetNativePlantData(p);const auto* definition=FusionFindPlant(typed.kind);
            if(!Require(a,p&&d&&definition&&p->mNetId&&d->instanceToken&&d->kind==typed.kind&&p->mPlantHealth==definition->maxHealth&&p->mPlantMaxHealth==definition->maxHealth,"ExtendedShooter40 actual typed source factory bind failed"))return false;
            NetTrace("SMOKE_EXTENDED_SHOOTER40_PLANT_SETUP group=%d epoch=%u kind=%u id=%u token=%u owner=%u parents=%u,%u row=%d column=%d hp=%d launch=%d state=%d shooting=%d factoryOnly=1 acquisitionNotClaimed=1",s.group,s.epoch,d->kind,p->mNetId,d->instanceToken,d->owner,d->firstParent,d->secondParent,p->mRow,p->mPlantCol,p->mPlantHealth,p->mLaunchCounter,p->mStateCountdown,p->mShootingCounter);
        }
        for(int row=0;row<Count();++row)
        {
            //1081 has five source rays (+90,-90,180,+30,-30), no forward
            // horizontal ray. Its own-row receiver belongs behind the plant.
            const int column=s.group==1?(row==1||row==4?1:row==3?4:5):5;
            //1160's col4 receiver is outside the crossing Star ray from row1;
            // its three native HP components therefore measure only Real6.
            if(!SpawnTarget(a,row,float(b->GetCellCenterX(column,row)),targetHP,s.group==1&&row==3))return false;
        }
        if(s.group==1)
        {
            if(!SpawnTarget(a,4,float(b->GetCellCenterX(2,4)+70),270))return false; // Actual Chomper bite.
            if(!SpawnTarget(a,4,float(b->GetCellCenterX(0,4)),targetHP))return false; // Backward4/5 receiver.
        }
        if(s.group==2&&!SpawnTarget(a,0,float(b->GetCellCenterX(3,0)),270))return false;
        if(s.group==3)
            for(const auto cell:std::array<std::array<int,2>,3>{{{1,0},{3,0},{1,1}}})
            {
                NativePlantData typed;typed.kind=20;typed.owner=0;
                auto* light=b->AddPlant(cell[0],cell[1],GetNativeSeedForPlant(20),SEED_NONE,&typed);
                if(!Require(a,light&&GetNativePlantData(light),"ExtendedShooter40 initial Plantern light fixture failed"))return false;
            }
        std::ostringstream ids;ids<<Tag("IDS");for(auto id:s.targets)ids<<' '<<id;s.ids=ids.str();s.setup=true;
        s.pauseRequested=Key(a);
    }
    if(a->NetIsHost()){Send(a,s.ids);}
    else if(!s.setup)
    {
        const auto prefix=Tag("IDS")+" ";
        for(const auto& l:a->mNetManager->mChatLines)if(l.mSlot==0&&l.mText.rfind(prefix,0)==0)
        {std::istringstream ids(l.mText.substr(prefix.size()));unsigned id=0;while(ids>>id){if(!Require(a,id>0&&id<=65535,"ExtendedShooter40 malformed target ID barrier"))return false;s.targets.push_back(uint16_t(id));}s.setup=true;break;}
    }
    if(!s.setup||!b->mPaused||s.targets.size()!=size_t(Count()+(s.group==1?2:s.group==2?1:0)))return false;
    for(auto id:s.targets)if(!Target(b,id))return false;
    if(s.group==1)
    {
        const auto* z=Target(b,s.targets[3]);
        if(!Require(a,z&&z->mBodyHealth==targetHP&&z->mBodyMaxHealth==targetHP&&
            z->mHelmType==HELMTYPE_BUCKET&&z->mShieldType==SHIELDTYPE_DOOR&&
            z->mHelmHealth==armorHP&&z->mHelmMaxHealth==armorHP&&
            z->mShieldHealth==armorHP&&z->mShieldMaxHealth==armorHP,
            "ExtendedShooter40 initial armor component snapshot differs"))return false;
    }
    for(int i=0;i<Count();++i)
    {
        auto* p=Actor(b,i);const auto* d=GetNativePlantData(p);if(!p||!d)return false;
        if(!s.actors[i].id)
        {
            s.actors[i]={p->mNetId,d->instanceToken};
            NetTrace("SMOKE_EXTENDED_SHOOTER40_BOUND role=%s group=%d epoch=%u kind=%u id=%u token=%u owner=%u parents=%u,%u row=%d column=%d hp=%d ghost=%d",Role(a),s.group,s.epoch,d->kind,p->mNetId,d->instanceToken,d->owner,d->firstParent,d->secondParent,p->mRow,p->mPlantCol,p->mPlantHealth,p->mNetGhost?1:0);
        }
    }
    return true;
}
bool Observe(LawnApp* a)
{
    auto* b=a->mBoard;
    if(s.group==2 && !s.moneyUpgrade)
    {
        auto* plant=Actor(b,0);const auto* data=GetNativePlantData(plant);
        if(data && data->kind==1135)
        {
            if(!Require(a,plant->mNetId!=s.actors[0].id&&data->instanceToken!=s.actors[0].token&&data->owner==1&&
                data->firstParent==1134&&data->secondParent==kFusionNoPlant&&GetNativeContext(b)->money==2000,
                "ExtendedShooter40 client Money command replacement/provenance/ledger differs"))return false;
            s.moneyUpgrade=true;s.actors[0]={plant->mNetId,data->instanceToken};
            NetTrace("SMOKE_EXTENDED_SHOOTER40_MONEY role=%s newId=%u token=%u ledger=2000 fee=1000 clientCommand=1",Role(a),plant->mNetId,data->instanceToken);
        }
        else if(a->NetIsClient()&&plant)
        {
            if(!s.moneyTool)
            {auto rect=b->GetShovelButtonRect();b->GetZenButtonRect(OBJECT_TYPE_MONEY_SIGN,rect);s.moneyTool=Click(a,float(rect.mX+rect.mWidth/2),float(rect.mY+rect.mHeight/2));return true;}
            if(!s.moneyClicked)
            {s.moneyClicked=Click(a,float(b->GetCellCenterX(2,0)),float(b->GridToPixelY(2,0)+50));return true;}
        }
    }
    for(int i=0;i<Count();++i)
    {
        auto* p=Actor(b,i);const auto* d=GetNativePlantData(p);
        if(!Require(a,p&&d&&p->mNetId==s.actors[i].id&&d->instanceToken==s.actors[i].token&&p->mPlantHealth>0,"ExtendedShooter40 actor identity lost"))return false;
        if(d->kind==1134)continue;
        if(!s.phaseSeen[i]&&(d->shootSeconds>=0||(d->kind==1011&&p->mTargetY>0)))
        {s.phaseSeen[i]=true;NetTrace("SMOKE_EXTENDED_SHOOTER40_PHASE role=%s group=%d kind=%u sourceSeconds=%.6g mode=%u",Role(a),s.group,d->kind,d->shootSeconds,d->batchMode);}
        if(d->kind==1011&&(p->mState==STATE_CHOMPER_DIGESTING||p->mState==STATE_CHOMPER_BITING_GOT_ONE))s.chomp=true;
        if(d->kind==933&&d->batchMode)s.skill=true;
        if(d->kind==907&&d->batchShotCount>=15)s.tier101=true;
    }
    Projectile* p=nullptr;bool anyFlight=false;
    while(b->mProjectiles.IterateNext(p))
    {
        const auto* d=GetNativeProjectileData(p);if(p->mDead||!d)continue;anyFlight=true;
        if(s.projectiles.insert(p->mNetId).second)
        {s.payloads.insert(d->kind);NetTrace("SMOKE_EXTENDED_SHOOTER40_PROJECTILE role=%s group=%d id=%u payload=%u damage=%d way=%u effect=%u nativeType=%d motion=%d",Role(a),s.group,p->mNetId,d->kind,d->damage,d->specialType,d->effectVariant,int(p->mProjectileType),int(p->mMotionType));}
        if(d->kind==100&&d->specialType==18)s.skillBullet=true;
        if(d->kind==23&&d->effectVariant)s.tier301=true;
        if(d->kind==75&&d->effectVariant&&d->damage==160)s.goldMiusa=true;
        if(d->kind==70)s.split=true;
    }
    if(anyFlight)Capture(a,"flight");
    int sunCount=0;Coin* coin=nullptr;while(b->mCoins.IterateNext(coin))if(!coin->mDead&&coin->mType==COIN_SUN)++sunCount;
    s.moneySun=(std::max)(s.moneySun,sunCount);
    return true;
}
bool ArmorLossReady(const Zombie* z)
{
    if(!z)return false;
    const int bodyLoss=targetHP-z->mBodyHealth;
    return bodyLoss>0&&bodyLoss==armorHP-z->mHelmHealth&&bodyLoss==armorHP-z->mShieldHealth&&bodyLoss%80==0;
}
uint32_t MissingProof(Board* b)
{
    // Bits0..4: source phases;8..16: the group conditions below;
    //20+targetIndex: body damage (or accepted consumption for target5).
    uint32_t missing=0;
    const auto need=[&](unsigned bit,bool okay){if(!okay)missing|=uint32_t(1)<<bit;};
    for(int i=0;i<Count();++i)need(unsigned(i),s.phaseSeen[i]);
    if(s.group==0)
    {
        unsigned bit=8;for(uint16_t kind:{61,62,63,64,86,71,70})need(bit++,s.payloads.contains(kind));
        need(15,s.split);
    }
    else if(s.group==1)
    {
        need(8,s.payloads.contains(38));need(9,s.payloads.contains(39));
        need(10,s.payloads.contains(4)||s.payloads.contains(5));need(11,s.chomp);
        need(12,s.targets.size()>3&&ArmorLossReady(Target(b,s.targets[3])));
    }
    else if(s.group==2)
    {
        need(8,s.moneyUpgrade);need(9,s.goldMiusa);need(10,s.moneySun>=2);
        need(11,s.tier101);need(12,s.tier301);need(13,s.skill);need(14,s.skillBullet);
        need(15,s.payloads.contains(3));need(16,s.payloads.contains(75));
    }
    else need(8,s.payloads.contains(40));
    for(size_t i=0;i<s.targets.size();++i)
    {
        const auto* z=Target(b,s.targets[i]);
        need(unsigned(20+i),((s.group==1||s.group==2)&&i==5)?!z:z&&z->mBodyHealth<targetHP);
    }
    return missing;
}
void LogProof(LawnApp* a,const char* reason)
{
    std::ostringstream payloads;bool first=true;
    for(auto kind:s.payloads){if(!first)payloads<<',';first=false;payloads<<kind;}
    uint32_t phases=0;for(int i=0;i<Count();++i)if(s.phaseSeen[i])phases|=uint32_t(1)<<i;
    NetTrace("SMOKE_EXTENDED_SHOOTER40_PROOF role=%s group=%d stage=%d reason=%s missing=%08x phaseMask=%02x payloads=%s chomp=%d armorReady=%d moneyUpgrade=%d goldMiusa=%d sunItems=%d tier15=%d bigDoom=%d skill=%d skillBullet=%d split=%d targetBitsStart=20",
        Role(a),s.group,s.stage,reason,MissingProof(a->mBoard),phases,payloads.str().c_str(),s.chomp?1:0,
        s.group==1&&s.targets.size()>3&&ArmorLossReady(Target(a->mBoard,s.targets[3]))?1:0,
        s.moneyUpgrade?1:0,s.goldMiusa?1:0,s.moneySun,s.tier101?1:0,s.tier301?1:0,s.skill?1:0,s.skillBullet?1:0,s.split?1:0);
    for(size_t i=0;i<s.targets.size();++i)
    {
        const auto* z=Target(a->mBoard,s.targets[i]);
        NetTrace("SMOKE_EXTENDED_SHOOTER40_TARGET_GATE role=%s group=%d index=%zu id=%u alive=%d body=%d bodyMax=%d helm=%d helmMax=%d shield=%d shieldMax=%d expectedConsumed=%d",
            Role(a),s.group,i,s.targets[i],z?1:0,z?z->mBodyHealth:0,z?z->mBodyMaxHealth:0,
            z?z->mHelmHealth:0,z?z->mHelmMaxHealth:0,z?z->mShieldHealth:0,z?z->mShieldMaxHealth:0,
            (s.group==1||s.group==2)&&i==5?1:0);
    }
}
bool Complete(LawnApp* a,bool log)
{
    for(size_t i=0;i<s.targets.size();++i)
    {
        if((s.group==1||s.group==2)&&i==5)continue;
        auto* z=Target(a->mBoard,s.targets[i]);
        if(!Require(a,z&&z->mBodyMaxHealth==targetHP,"ExtendedShooter40 controlled target lost unexpectedly"))return false;
        if(s.group==1&&i==3)
        {
            const int bodyLoss=targetHP-z->mBodyHealth,helmLoss=armorHP-z->mHelmHealth,shieldLoss=armorHP-z->mShieldHealth;
            if(!Require(a,z->mHelmType==HELMTYPE_BUCKET&&z->mShieldType==SHIELDTYPE_DOOR&&
                z->mHelmMaxHealth==armorHP&&z->mShieldMaxHealth==armorHP&&bodyLoss>=0&&
                bodyLoss==helmLoss&&bodyLoss==shieldLoss&&bodyLoss%80==0,
                "ExtendedShooter40 Real6 did not damage body/helmet/shield independently by80"))
            {LogProof(a,"armor-mismatch");return false;}
        }
    }
    if(log)LogProof(a,"outcome");
    return MissingProof(a->mBoard)==0;
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionExtendedShooter40(LawnApp* a)
{
    const auto* enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_EXTENDED_SHOOTER40"),*native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
    if(!a||!enabled||std::strcmp(enabled,"1")||!native||std::strcmp(native,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
    if(a->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();
    if(GetTickCount()-s.since>180000)
    {
        const bool world=a->mBoard&&IsNativeFusionBoard(a->mBoard)&&s.setup;
        if(world)LogProof(a,"deadline");
        char why[120];sprintf_s(why,"ExtendedShooter40 deadline group=%d stage=%d missing=%08x",s.group,s.stage,world?MissingProof(a->mBoard):0);
        a->CoopSmokeFail(why);return true;
    }
    if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
    if(s.stage==0)
    {
        if(a->NetIsHost()&&!s.configured){a->mNetManager->SetDifficulty(1);s.configured=true;}
        if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
        if(a->NetIsClient()){Advance(a,1);Send(a,"A266_BEGIN");}
        else if(Has(a,1,"A266_BEGIN")){Advance(a,1);std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;
    }
    if(s.stage==10)
    {
        if(!Barrier(a,"DONE"))return true;
        if(s.group==3){NetTrace("SMOKE_EXTENDED_SHOOTER40_COMPLETE role=%s activities=4 sourcePlants=15 factoryOnly=1 nativeCombat=1 postSetupActorWrites=0 paidGoldUpgrade=clientSDL sourceNPCParity=NotExecuted",Role(a));a->CoopSmokeWriteResult(true,nullptr);return true;}
        State next;next.group=s.group+1;next.previousEpoch=s.epoch;next.chat=s.chat;s=std::move(next);Advance(a,1);
        if(a->NetIsHost()){std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;
    }
    if(!a->mBoard||!IsNativeFusionBoard(a->mBoard)){if(s.stage==1&&a->NetIsClient()&&!s.previousEpoch)Send(a,"A266_BEGIN");return true;}
    auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);if(ctx->epoch<=s.previousEpoch)return true;
    if(!s.epoch){s.epoch=ctx->epoch;NetTrace("SMOKE_EXTENDED_SHOOTER40_EPOCH role=%s group=%d epoch=%u previous=%u",Role(a),s.group,s.epoch,s.previousEpoch);}
    if(!Require(a,ctx->epoch==s.epoch&&ctx->validation&&!a->mFusionRuntime&&b->GetColumnCount()==10,"ExtendedShooter40 actual native validation epoch differs"))return true;
    const auto& bank=ctx->selection.State();auto* chooser=a->mSeedChooserScreen;
    if(!s.diagnostic||GetTickCount()-s.diagnostic>=10000)
    {
        s.diagnostic=GetTickCount();NetTrace("SMOKE_EXTENDED_SHOOTER40_GATE role=%s group=%d stage=%d epoch=%u tick=%d samples=%u paused=%d projectiles=%zu tier301=%d rage=%d,%d callbacks=%d,%d skill=%d chomp=%d pit=%d",Role(a),s.group,s.stage,s.epoch,b->mMainCounter,Samples(a),b->mPaused?1:0,s.projectiles.size(),s.tier301?1:0,s.rageSeen[1]?1:0,s.rageSeen[2]?1:0,s.callbacks[3],s.callbacks[4],s.skill?1:0,s.chomp?1:0,s.pit?1:0);
        if(s.setup&&s.stage>=6)LogProof(a,"gate");
    }
    if(s.stage==1)
    {
        if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||chooser->mSeedsInFlight||!b->mCutScene->mSeedChoosing)return true;
        if(!Require(a,bank.activeMask==3&&bank.targetCount==10&&bank.quotas[0]==5&&bank.quotas[1]==5,"ExtendedShooter40 shared native chooser quota differs"))return true;
        int own=0;for(const auto& p:bank.packets)own+=p.owner==Slot(a);if(own<s.waiting)return true;
        if(own<5)
        {
            const int wanted=5*Slot(a)+own;int dense=-1;
            for(size_t i=0;i<ctx->eligible.size();++i)if(ctx->eligible[i].kind==wanted&&!ctx->eligible[i].isExtra)dense=int(i);
            if(!Require(a,dense>=0,"ExtendedShooter40 baseline chooser card missing"))return true;if(Choice(a,chooser,dense))s.waiting=own+1;return true;
        }
        if(bank.packets.size()!=10)return true;int rank=-1;
        for(int i=0;i<b->mSeedBank->mNumPackets;++i){const int next=NetSeedBankOwnerRank(Slot(a),b->mSeedBank->mSeedPackets[i].mNetOwnerSlot);if(!Require(a,next>=rank,"ExtendedShooter40 own-first native bank order differs"))return true;rank=next;}
        if(Capture(a,"chooser")){NetTrace("SMOKE_EXTENDED_SHOOTER40_POOL role=%s group=%d epoch=%u target=10 quotas=5,5 ownFirst=1 nativeChooser=1",Role(a),s.group,s.epoch);Advance(a,2);}
    }
    else if(s.stage==2)
    {
        if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}
        auto* button=chooser?chooser->mStartButton:nullptr;if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;
        const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
        if(!button->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"ExtendedShooter40 owned Ready hover failed");}
        else if(Click(a,x,y))Advance(a,3);
    }
    else if(s.stage==3)
    {
        if(bank.phase==FusionBankPhase::Intro)
        {if(!s.introFirst)s.introFirst=GetTickCount();s.introLast=GetTickCount();++s.introChecks;return true;}
        if(bank.phase!=FusionBankPhase::Playing||chooser||a->mGameScene!=SCENE_PLAYING)return true;
        if(!Require(a,s.introChecks>=10&&s.introLast-s.introFirst>=1000,"ExtendedShooter40 actual native full intro not observed"))return true;
        if(s.playingFrame<0){s.playingFrame=a->mAppCounter;return true;}if(a->mAppCounter<=s.playingFrame)return true;
        if(a->NetIsHost()&&!Setup(a))return true;
        Advance(a,4);
    }
    else if(s.stage==4)
    {
        if(!Setup(a))return true;if(!s.pauseSample){s.pauseSample=Samples(a);return true;}
        if(Samples(a)-s.pauseSample<2||!Capture(a,"before")||!Barrier(a,"BEFORE"))return true;
        s.baselineTick=b->mMainCounter;Advance(a,5);
    }
    else if(s.stage==5)
    {
        if(a->NetIsHost()&&!s.resumed){s.resumed=Key(a);return true;}
        if(!b->mPaused)Advance(a,6);
    }
    else if(s.stage==6)
    {
        if(!Observe(a))return true;
        if(Complete(a,false)&&s.frames.contains("fusion_extended_shooter40_"+std::to_string(s.group)+"_flight")&&Barrier(a,"OUTCOME"))Advance(a,7);
    }
    else if(s.stage==7)
    {
        if(!Observe(a))return true;
        if(a->NetIsHost()&&!s.finalPause){s.finalPause=Key(a);return true;}
        if(b->mPaused){s.pauseSample=0;Advance(a,8);}
    }
    else if(s.stage==8)
    {
        if(!s.pauseSample){s.pauseSample=Samples(a);return true;}if(Samples(a)-s.pauseSample<2)return true;
        if(!Complete(a,false))return true;
        if(s.group==1)
        {
            const auto* z=Target(b,s.targets[3]);
            const auto hpTag="ARMOR_HP_"+std::to_string(z->mBodyHealth)+"_"+std::to_string(z->mHelmHealth)+"_"+std::to_string(z->mShieldHealth);
            if(!Barrier(a,hpTag.c_str()))return true;
            if(!s.finalLogged)NetTrace("SMOKE_EXTENDED_SHOOTER40_ARMOR_SAME role=%s id=%u body=%d helm=%d shield=%d bodyLoss=%d helmLoss=%d shieldLoss=%d damageUnit=80 bothPeerTag=1 initialOnly=1 sourceNPCParity=NotExecuted",
                Role(a),z->mNetId,z->mBodyHealth,z->mHelmHealth,z->mShieldHealth,targetHP-z->mBodyHealth,armorHP-z->mHelmHealth,armorHP-z->mShieldHealth);
        }
        if(!s.finalLogged){if(!Complete(a,true))return true;s.finalLogged=true;}
        if(!Capture(a,"complete")||!Barrier(a,"FINAL"))return true;
        NetTrace("SMOKE_EXTENDED_SHOOTER40_CASE role=%s group=%d epoch=%u plants=%d nativeCombat=1 snapshots=%u elapsedTicks=%d tier101=%d tier301=%d naturalRage=%d,%d sniperCallbacks=%d,%d skill=%d skillBullet=%d swallow=%d crater=%d sun5=%d postSetupActorWrites=0 factoryOnly=1",Role(a),s.group,s.epoch,Count(),Samples(a),b->mMainCounter-s.baselineTick,s.tier101?1:0,s.tier301?1:0,s.rageSeen[1]?1:0,s.rageSeen[2]?1:0,s.callbacks[3],s.callbacks[4],s.skill?1:0,s.skillBullet?1:0,s.chomp?1:0,s.pit?1:0,s.sun?1:0);Advance(a,10);
    }
    return true;
}
}
