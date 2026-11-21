#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetCommon.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Coin.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativeVisual.h"
#include "../Fusion/FusionNativeEquipment.h"
#include "../Fusion/FusionNativeEquipment268.h"
#include "../Fusion/FusionNativeExtras268.h"
#include "../Fusion/FusionRecipe.h"
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
    {{25,0,0},{1100,0,5},{1101,7,2},{1154,9,5}},
    {{1092,2,2},{1097,3,1},{1098,4,1},{1099,5,1},{935,6,2},{1129,8,0},{1162,8,4}},
    {{1102,1,0},{1027,3,0},{1028,5,0},{1029,7,0}},
    {{0,2,0},{3,4,0},{0,3,1},{3,5,1}}};
struct Ref {uint16_t id=0;uint32_t token=0;};
struct PulseProof {Ref source{};uint16_t target=0;int before=0,after=0,damage=0;bool ready=false,sent=false;};
struct State
{
    int group=0,stage=0,waiting=0,entry=-1,start=-1,introChecks=0,mask=0,ui=0,uiPhase=0;
    uint32_t since=0,chat=0,wheel=0,epoch=0,previousEpoch=0,introFirst=0,introLast=0,samples=0,diagnostic=0,uiSince=0;
    bool configured=false,setup=false,pauseRequested=false,resumeRequested=false,baseline=false,idsSent=false,idsParsed=false,idsReady=false;
    bool sunlight=false,producedItem=false,balloonBlown=false,blowCapture=false,growSeen=false;
    bool balloonInitialAir=false,balloonMoved=false,balloonRetired=false,blowProofReady=false,blowProofSent=false;
    uint16_t balloonId=0;float balloonStartX=0,balloonMaxX=0;Ref blowItem{};std::string blowProof;
    std::array<Ref,8> actors{},receivers{};
    std::array<Ref,10> items{};
    std::array<Ref,4> bucketFresh{};
    std::array<bool,4> bucketNegative{};
    uint32_t bucketNegativeSamples=0,bucketResultSamples=0;
    Ref protectedSun{},machineParent{},machineFresh{},receiverGuard{};
    bool machineIce=false,machineFire=false,machineOverheal=false,machineProtection=false,mineDamage=false,mineChildDamage=false;
    int machinePreviousHealth=0;
    std::set<uint32_t> mineChildren,baselineMineTokens;
    std::array<Ref,9> mineRefs{};std::array<uint16_t,9> provenMineIds{};
    std::array<PulseProof,5> pulses{};
    std::array<int,5> targetInitialHealth{},previousTargetHealth{};
    bool mineProofReady=false,mineProofSent=false,pumpkinDamageSeen=false;
    std::string mineProof;
    std::array<int,8> previousClock{},initialHealth{};
    std::array<bool,8> heldSeen{},pulseSeen{};
    std::vector<uint16_t> zombies;
    std::set<uint32_t> seaItems;
    std::set<std::string> frames;std::map<std::string,int> draws;
}s;
bool Bucket274(){const char* v=std::getenv("PVZ_COOP_SMOKE_FUSION_BUCKET274");return v&&std::strcmp(v,"1")==0;}
const auto& Specs(){return groups[s.group];}
int Expected(){return (1<<int(Specs().size()))-1;}
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool okay,const char* why){if(!okay)a->CoopSmokeFail(why);return okay;}
void Advance(LawnApp* a,int stage){s.stage=stage;s.since=GetTickCount();s.diagnostic=0;NetTrace("SMOKE_EQUIPMENT268_STAGE role=%s group=%d epoch=%u stage=%d",Role(a),s.group,s.epoch,stage);}
std::string Tag(const char* name){return "E268_"+std::to_string(s.group)+"_"+std::to_string(s.epoch)+"_"+name;}
bool Has(LawnApp* a,int who,const std::string& value){for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==who&&line.mText==value)return true;return false;}
bool Send(LawnApp* a,const std::string& value)
{if(Has(a,Slot(a),value))return true;if(s.chat&&GetTickCount()-s.chat<800)return false;s.chat=GetTickCount();a->mNetManager->SendChat(value);return Has(a,Slot(a),value);}
bool Barrier(LawnApp* a,const char* name){const auto value=Tag(name);Send(a,value);return Has(a,0,value)&&Has(a,1,value);}
bool Click(LawnApp* a,float x,float y)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Equipment268 owned SDL click failed");
}
bool BoardClick(LawnApp* a,float x,float y)
{return Click(a,x+a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX,y+a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY);}
bool Escape(LawnApp* a)
{SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Equipment268 owned SDL Escape failed");}
bool Choice(LawnApp* a,SeedChooserScreen* c,int index)
{
    int x=0,y=0;c->GetSeedPositionInChooser(index,x,y);const float cy=float(y+35-int(c->mScrollbar->mScrollValue));
    if(cy<158||cy>493)
    {if(s.wheel&&GetTickCount()-s.wheel<250)return false;s.wheel=GetTickCount();SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=float(c->mX+300);m.motion.y=float(c->mY+300);SDL_Event w{};w.type=SDL_EVENT_MOUSE_WHEEL;w.wheel.windowID=m.motion.windowID;w.wheel.y=cy<158?1.f:-1.f;Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&w),"Equipment268 chooser wheel failed");return false;}
    return std::abs(c->mScrollAmount)<=.05f&&Click(a,float(c->mX+x+25),float(c->mY)+cy);
}
bool Capture(LawnApp* a,const char* suffix)
{
    const auto name="fusion_equipment268_"+std::to_string(s.group)+"_"+suffix;if(s.frames.contains(name))return true;
    if(!s.draws.contains(name))s.draws[name]=a->mDrawCount;
    const int requested=s.draws[name]+2;if(!a->CoopSmokeEnsureFreshFrameAfterDraw(requested))return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Equipment268 fresh owned BMP failed"))return false;
    s.frames.insert(name);NetTrace("SMOKE_EQUIPMENT268_RENDER role=%s group=%d frame=%s requestedDraw=%d capturedDraw=%d",Role(a),s.group,name.c_str(),requested,a->mCoopSmokeFrameDrawCount);return true;
}
Plant* At(Board* b,int col,int row,int kind)
{Plant* p=nullptr;while(b->IteratePlants(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&d&&d->kind==kind&&p->mPlantCol==col&&p->mRow==row)return p;}return nullptr;}
Plant* Actor(Board* b,int i)
{auto* p=At(b,Specs()[i].col,Specs()[i].row,Specs()[i].kind);const auto* d=GetNativePlantData(p);return p&&d&&p->mNetId==s.actors[i].id&&d->instanceToken==s.actors[i].token?p:nullptr;}
Zombie* ZombieById(Board* b,uint16_t id)
{Zombie* z=nullptr;while(b->IterateZombies(z))if(!z->mDead&&z->mNetId==id)return z;return nullptr;}
Plant* MakePlant(LawnApp* a,uint16_t kind,int col,int row,bool worldGift=false)
{
    NativePlantData data;data.kind=kind;data.owner=worldGift?LawnNet::NET_SHARED_OWNER:uint8_t(col%2);
    if(!worldGift)for(std::size_t i=0;i<GetFusionRecipeCount();++i)if(GetFusionRecipes()[i].result==kind){data.firstParent=GetFusionRecipes()[i].existing;data.secondParent=GetFusionRecipes()[i].used;break;}
    if(!IsNativeBasePlantKind(kind)&&data.firstParent==kFusionNoPlant&&data.secondParent==kFusionNoPlant)data.owner=LawnNet::NET_SHARED_OWNER;
    std::string error;if(!Require(a,EnsureNativePlantVisuals(a->mBoard,kind,&error),error.empty()?"Equipment268 actual source model missing":error.c_str()))return nullptr;
    auto* p=a->mBoard->AddPlant(col,row,GetNativeSeedForPlant(kind),SEED_NONE,&data);const auto* bound=GetNativePlantData(p);
    if(!Require(a,p&&bound&&bound->kind==kind&&bound->instanceToken&&p->mNetId&&!p->mDead,"Equipment268 initial real typed factory failed"))return nullptr;
    if(worldGift&&!Require(a,bound->owner==LawnNet::NET_SHARED_OWNER&&!bound->packetId&&!bound->randomOrigin&&bound->firstParent==kFusionNoPlant&&bound->secondParent==kFusionNoPlant,"Equipment268 initial world gift provenance differs"))return nullptr;
    if(worldGift)NetTrace("SMOKE_EQUIPMENT268_INITIAL_WORLD_GIFT group=%d id=%u token=%u kind=%u column=%d row=%d owner=%u packet=%u random=%u parents=%u,%u initialOnly=1 acquisition=NotExecuted",s.group,p->mNetId,bound->instanceToken,kind,col,row,unsigned(bound->owner),bound->packetId,unsigned(bound->randomOrigin),bound->firstParent,bound->secondParent);
    return p;
}
Zombie* MakeZombie(LawnApp* a,ZombieType kind,int col,int row)
{
    auto* b=a->mBoard;auto* z=b->AddZombieInRow(kind,row,0);
    if(!Require(a,z&&z->mNetId&&a->ReanimationTryToGet(z->mBodyReanimID),"Equipment268 initial native control failed"))return nullptr;
    z->mBodyHealth=z->mBodyMaxHealth=32000;
    if(kind==ZOMBIE_PAIL||kind==ZOMBIE_FOOTBALL)z->mHelmHealth=z->mHelmMaxHealth=32000;
    z->mPosX=float(b->GetCellCenterX(col,row)-60);z->mX=int(z->mPosX);z->mPosY=z->GetPosYBasedOnRow(row);z->mY=int(z->mPosY);z->mVelX=0;
    s.zombies.push_back(z->mNetId);NetTrace("SMOKE_EQUIPMENT268_INITIAL_NPC group=%d index=%zu id=%u type=%d row=%d x=%.9g body=%d helm=%d initialOnly=1 sourceAI=NotClaimed",s.group,s.zombies.size()-1,z->mNetId,kind,row,z->mPosX,z->mBodyHealth,z->mHelmHealth);return z;
}
bool Setup(LawnApp* a)
{
    auto* b=a->mBoard;if(!Require(a,a->NetIsHost()&&!b->mPaused&&!b->mPlants.mSize&&!b->mZombies.mSize,"Equipment268 setup must be initial unpaused empty world")||!Require(a,ConfigureNativeFusionBackground(b,BACKGROUND_3_POOL),"Equipment268 Pool6 configuration refused"))return false;
    if(s.group==0)
    {
        for(int row:{2,3})if(!MakePlant(a,12,8,row)||!MakePlant(a,931,8,row))return false;
        if(!MakePlant(a,931,1,0)||!MakePlant(a,931,2,5))return false;
        if(!MakePlant(a,20,0,4)||!MakePlant(a,20,1,5))return false;
    }
    if(s.group==1)
    {
        for(int col:{2,6})if(!MakePlant(a,12,col,2))return false;
        for(int col:{7,8,9})if(!MakePlant(a,12,col,3))return false;
        // Genuine Fire84 -> Ice151 source events expose both Machine forms.
        if(!MakePlant(a,931,0,0)||!MakePlant(a,16,1,0)||!MakePlant(a,10,0,1)||!MakePlant(a,1,7,0))return false;
    }
    for(const auto& spec:Specs())if(!MakePlant(a,spec.kind,spec.col,spec.row))return false;
    if(s.group==0)
    {if(!MakeZombie(a,ZOMBIE_PAIL,2,0)||!MakeZombie(a,ZOMBIE_FOOTBALL,2,5)||!MakeZombie(a,ZOMBIE_PAIL,9,2)||!MakeZombie(a,ZOMBIE_PAIL,9,3)||!MakeZombie(a,ZOMBIE_PAIL,8,2))return false;}
    else if(s.group==1)
    {
        for(int i=0;i<5;++i)if(!MakeZombie(a,ZOMBIE_PAIL,Specs()[i].col,Specs()[i].row))return false;
        if(!MakeZombie(a,ZOMBIE_NORMAL,7,0)||!MakeZombie(a,ZOMBIE_NORMAL,1,0)||!MakeZombie(a,ZOMBIE_NORMAL,8,4))return false;
    }
    else if(s.group==2)
    {
        At(b,5,0,1028)->mPlantHealth=15000;At(b,7,0,1029)->mPlantHealth=7000;
        if(!MakePlant(a,3,8,0,true)||!MakePlant(a,931,7,5,true))return false;
        if(!MakeZombie(a,ZOMBIE_FOOTBALL,9,5)||!MakeZombie(a,ZOMBIE_BALLOON,9,4))return false;
        for(int i=0;i<5;++i)
        {auto* p=MakePlant(a,uint16_t(i==0?1092:i==1?1097:i==2?1098:i==3?1099:935),i+2,5,true);if(!p)return false;p->mPlantHealth-=600;}
        for(int i=0;i<10;++i)
        {
            const auto type=i==8?FusionEquipmentType::Machine:i==0||i==3?FusionEquipmentType::Helmet:i==2?FusionEquipmentType::Door:FusionEquipmentType::Bucket;
            auto* coin=CreateNativeEquipment(b,type,float(b->GetCellCenterX(i,4)),float(b->GridToPixelY(i,4)+50),false);
            if(!Require(a,coin&&GetNativeEquipmentData(coin),"Equipment268 proven initial usable-item factory refused"))return false;
        }
    }
    else if(s.group==3)
    {
        // Isolated acquisition-neutral world: no enemies, damaged actors or
        // timer/RNG rewrites. Actual factories alone create initial actors/items.
        if(!MakePlant(a,4,8,0,true))return false;
        for(int i=0;i<4;++i)
        {
            auto* coin=CreateNativeEquipment(b,FusionEquipmentType::Bucket,
                float(b->GetCellCenterX(1+2*i,4)),float(b->GridToPixelY(1+2*i,4)+50),false);
            const auto* d=GetNativeEquipmentData(coin);
            if(!Require(a,coin&&d&&d->type==FusionEquipmentType::Bucket&&d->factoryType==FusionEquipmentType::Bucket,
                "Bucket274 initial genuine Bucket factory failed"))return false;
        }
    }
    s.setup=true;s.pauseRequested=Escape(a);
    NetTrace("SMOKE_EQUIPMENT268_SETUP group=%d mainKinds=%zu initialFactory=1 acquisition=NotExecuted sourceAI=NotClaimed actorWritesAfterSetup=0",s.group,Specs().size());return true;
}
bool TargetManifest(LawnApp* a)
{
    if(s.idsReady)return true;const auto prefix=Tag("IDS")+" ";
    if(a->NetIsHost())
    {
        std::string value=prefix;for(auto id:s.zombies)value+=std::to_string(id)+" ";value.pop_back();
        if(!Require(a,value.size()<=LawnNet::NET_MAX_CHAT_CHARS,"Equipment268 host UID proof too long"))return false;
        if(!s.idsSent){if(!Send(a,value))return false;s.idsSent=true;NetTrace("SMOKE_EQUIPMENT268_TARGET_PROOF role=host group=%d text=%s",s.group,value.c_str());}
        if(!Has(a,1,Tag("IDS_ACK")))return false;
    }
    else
    {
        if(!s.idsParsed)for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==0&&line.mText.starts_with(prefix))
        {std::istringstream input(line.mText.substr(prefix.size()));unsigned id=0;while(input>>id){if(!Require(a,id>0&&id<=65535,"Equipment268 invalid authoritative UID"))return false;s.zombies.push_back(uint16_t(id));}s.idsParsed=true;NetTrace("SMOKE_EQUIPMENT268_TARGET_PROOF role=client group=%d text=%s",s.group,line.mText.c_str());break;}
        if(!s.idsParsed||!Send(a,Tag("IDS_ACK")))return false;
    }
    if(!Require(a,s.zombies.size()==std::size_t(s.group==2?2:s.group==1?8:5),"Equipment268 authoritative target count differs"))return false;
    s.idsReady=true;return true;
}
bool BucketBaseline274(LawnApp* a)
{
    auto* b=a->mBoard;if(b->mBackground!=BACKGROUND_3_POOL||GetNativeFusionRowCount(b)!=6)return false;
    for(int i=0;i<4;++i)
    {
        const auto& spec=Specs()[i];auto* p=At(b,spec.col,spec.row,spec.kind);const auto* d=GetNativePlantData(p);
        if(!p||!d)return false;const auto* source=FusionFindPlant(spec.kind);
        if(!Require(a,source&&d->owner==(i<2?0:1)&&!d->packetId&&!d->randomOrigin&&
            d->firstParent==kFusionNoPlant&&d->secondParent==kFusionNoPlant&&
            p->mPlantHealth==source->maxHealth&&p->mPlantMaxHealth==source->maxHealth,
            "Bucket274 initial canonical actor/provenance differs"))return false;
        if(!s.actors[i].id)s.actors[i]={p->mNetId,d->instanceToken};
        if(!Require(a,p->mNetId==s.actors[i].id&&d->instanceToken==s.actors[i].token&&p->mNetId&&d->instanceToken,
            "Bucket274 original actor identity changed before input"))return false;
        if(!s.items[i].id)
        {
            Coin* c=nullptr;while(b->IterateCoins(c))if(const auto* item=GetNativeEquipmentData(c);item&&!c->mDead&&
                item->factoryType==FusionEquipmentType::Bucket&&item->type==FusionEquipmentType::Bucket&&
                item->claimSlot==255&&!item->sourcePlantId&&std::abs(c->mPosX+20-b->GetCellCenterX(1+2*i,4))<35)
            {s.items[i]={c->mNetId,item->instanceToken};break;}
        }
        if(!s.items[i].id)return false;
    }
    auto* guard=At(b,8,0,4);const auto* gd=GetNativePlantData(guard);if(!guard||!gd)return false;
    if(!s.receiverGuard.id)s.receiverGuard={guard->mNetId,gd->instanceToken};
    if(!Require(a,guard->mNetId==s.receiverGuard.id&&gd->instanceToken==s.receiverGuard.token&&
        gd->owner==LawnNet::NET_SHARED_OWNER&&guard->mPlantHealth==300&&guard->mPlantMaxHealth==300,
        "Bucket274 unsupported receiver guard differs"))return false;
    if(s.start<0){s.start=b->mMainCounter;NetTrace("SMOKE_BUCKET274_BASELINE role=%s epoch=%u actors=4 buckets=4 guard=%u guardToken=%u initialFactory=1 acquisition=NotExecuted postSetupActorWrites=0",
        Role(a),s.epoch,s.receiverGuard.id,s.receiverGuard.token);}
    return true;
}
bool Baseline(LawnApp* a)
{
    if(s.group==3)return BucketBaseline274(a);
    auto* b=a->mBoard;if(!TargetManifest(a)||b->mBackground!=BACKGROUND_3_POOL||GetNativeFusionRowCount(b)!=6)return false;
    for(int i=0;i<int(Specs().size());++i)
    {
        auto* p=At(b,Specs()[i].col,Specs()[i].row,Specs()[i].kind);const auto* d=GetNativePlantData(p);
        if(!p||!d||(FindFusionEquipmentProfile(d->kind)&&!GetNativeEquipmentPlantState(p)))return false;
        if(!s.actors[i].id){s.actors[i]={p->mNetId,d->instanceToken};s.previousClock[i]=p->mTargetX;s.initialHealth[i]=p->mPlantHealth;NetTrace("SMOKE_EQUIPMENT268_BIRTH role=%s group=%d index=%d id=%u token=%u kind=%u hp=%d",Role(a),s.group,i,p->mNetId,d->instanceToken,d->kind,p->mPlantHealth);}
    }
    if(s.group==1)
    {
        auto* sun=At(b,7,0,1);const auto* d=GetNativePlantData(sun);if(!d)return false;
        s.protectedSun={sun->mNetId,d->instanceToken};s.machinePreviousHealth=Actor(b,5)->mPlantHealth;
        if(s.start<0)
        {
            for(int i=0;i<5;++i){auto* z=ZombieById(b,s.zombies[i]);if(!z)return false;s.targetInitialHealth[i]=s.previousTargetHealth[i]=z->mBodyHealth+z->mHelmHealth;}
            Plant* p=nullptr;while(b->IteratePlants(p))if(const auto* pd=GetNativePlantData(p);pd&&pd->kind==4)s.baselineMineTokens.insert(pd->instanceToken);
        }
    }
    if(s.group==2)
    {
        if(!s.balloonInitialAir)
        {
            auto* balloon=ZombieById(b,s.zombies[1]);
            if(!Require(a,balloon&&balloon->mZombieType==ZOMBIE_BALLOON&&!balloon->mMindControlled&&balloon->IsFlying(),"Equipment268 baseline air control differs"))return false;
            s.balloonId=balloon->mNetId;s.balloonStartX=s.balloonMaxX=balloon->mPosX;s.balloonInitialAir=true;
            NetTrace("SMOKE_EQUIPMENT268_AIR_BASELINE role=%s group=%d epoch=%u id=%u flying=1 x=%.9g",Role(a),s.group,s.epoch,s.balloonId,s.balloonStartX);
        }
        auto* parent=At(b,8,0,3);const auto* pd=GetNativePlantData(parent);if(!pd)return false;s.machineParent={parent->mNetId,pd->instanceToken};
        auto* guard=At(b,7,5,931);const auto* gd=GetNativePlantData(guard);
        if(!Require(a,guard&&gd&&gd->instanceToken&&guard->mPlantHealth>0&&guard->mPlantMaxHealth==16000,"Equipment268 receiver guard missing at baseline"))return false;
        if(!s.receiverGuard.id){s.receiverGuard={guard->mNetId,gd->instanceToken};NetTrace("SMOKE_EQUIPMENT268_RECEIVER_GUARD role=%s id=%u token=%u column=7 row=5 hp=%d",Role(a),guard->mNetId,gd->instanceToken,guard->mPlantHealth);}
        if(!Require(a,guard->mNetId==s.receiverGuard.id&&gd->instanceToken==s.receiverGuard.token,"Equipment268 receiver guard generation changed"))return false;
        for(int i=0;i<10;++i)
        {
            if(!s.items[i].id){Coin* c=nullptr;while(b->IterateCoins(c)){const auto* d=GetNativeEquipmentData(c);if(d&&!c->mDead&&!d->sourcePlantId&&std::abs(c->mPosX+20-b->GetCellCenterX(i,4))<35){s.items[i]={c->mNetId,d->instanceToken};break;}}}
            if(!s.items[i].id)return false;
        }
        auto* door=ResolveNativeEquipment(b,s.items[2].id,s.items[2].token);const auto* dd=GetNativeEquipmentData(door);
        if(!Require(a,dd&&dd->type==FusionEquipmentType::Bucket&&dd->factoryType==FusionEquipmentType::Door,"Equipment268 source Door appearance/physical payload differs"))return false;
        for(int i=0;i<5;++i)
        {auto* p=At(b,i+2,5,i==0?1092:i==1?1097:i==2?1098:i==3?1099:935);const auto* d=GetNativePlantData(p);if(!d)return false;s.receivers[i]={p->mNetId,d->instanceToken};}
    }
    // Board.mMainCounter is local on the snapshot client. Both peers latch
    // their paused baseline rather than comparing the client's intro counter
    // with the host's reset Pool counter.
    if(s.start<0){s.start=b->mMainCounter;NetTrace("SMOKE_EQUIPMENT268_CLOCK role=%s group=%d epoch=%u start=%d paused=%d samples=%u",Role(a),s.group,s.epoch,s.start,b->mPaused?1:0,Samples(a));}
    return true;
}
bool BucketPreserved274(LawnApp* a,int index)
{
    auto* b=a->mBoard;auto* old=Actor(b,index);const auto* d=GetNativePlantData(old);
    auto* guard=At(b,8,0,4);const auto* gd=GetNativePlantData(guard);
    auto* coin=ResolveNativeEquipment(b,s.items[index].id,s.items[index].token);const auto* item=GetNativeEquipmentData(coin);
    const auto* definition=FusionFindPlant(Specs()[index].kind);
    return Require(a,old&&d&&definition&&old->mPlantHealth==definition->maxHealth&&old->mPlantMaxHealth==definition->maxHealth&&
        guard&&gd&&guard->mNetId==s.receiverGuard.id&&gd->instanceToken==s.receiverGuard.token&&guard->mPlantHealth==300&&
        item&&item->type==FusionEquipmentType::Bucket&&item->factoryType==FusionEquipmentType::Bucket&&
        item->phase==NativeEquipmentPhase::Cursor&&item->claimSlot==(index<2?0:1),
        "Bucket274 unsupported real item use changed the item, receiver or original actor");
}
bool BucketReplacement274(LawnApp* a,int index)
{
    auto* b=a->mBoard;const auto& spec=Specs()[index];const uint16_t kind=spec.kind==0?1020:1029;
    auto* p=At(b,spec.col,spec.row,kind);const auto* d=GetNativePlantData(p);
    if(!p||!d)return false;
    const auto* definition=FusionFindPlant(kind);
    int cellActors=0;Plant* other=nullptr;while(b->IteratePlants(other))if(!other->mDead&&other->mPlantCol==spec.col&&other->mRow==spec.row)++cellActors;
    if(!Require(a,definition&&p->mNetId&&d->instanceToken&&p->mNetId!=s.actors[index].id&&d->instanceToken!=s.actors[index].token&&
        d->owner==LawnNet::NET_SHARED_OWNER&&!d->packetId&&!d->randomOrigin&&
        d->firstParent==kFusionNoPlant&&d->secondParent==kFusionNoPlant&&p->mSeedType==GetNativeSeedForPlant(kind)&&
        p->mPlantHealth==definition->maxHealth&&p->mPlantMaxHealth==definition->maxHealth&&cellActors==1&&!Actor(b,index),
        "Bucket274 replacement canonical HP/type/fresh identity/default provenance differs"))return false;
    if(!s.bucketFresh[index].id)s.bucketFresh[index]={p->mNetId,d->instanceToken};
    return Require(a,p->mNetId==s.bucketFresh[index].id&&d->instanceToken==s.bucketFresh[index].token,
        "Bucket274 replacement identity changed or item produced a duplicate replacement")&&
        !ResolveNativeEquipment(b,s.items[index].id,s.items[index].token);
}
void ObserveBuckets274(LawnApp* a)
{
    auto* b=a->mBoard;if(s.stage!=5||!s.baseline||b->mPaused||s.ui>=4)return;
    const int i=s.ui,owner=i<2?0:1;const auto& spec=Specs()[i];const auto now=GetTickCount();
    if(!s.uiSince)s.uiSince=now;
    if(now-s.uiSince>30000){a->CoopSmokeFail("Bucket274 bounded native pickup/rejection/replacement deadline");return;}
    auto* coin=ResolveNativeEquipment(b,s.items[i].id,s.items[i].token);const auto* item=GetNativeEquipmentData(coin);
    if(i==0&&item&&item->phase==NativeEquipmentPhase::Cursor&&!Capture(a,"flight"))return;
    const auto negative="BUCKET_NEG_"+std::to_string(i),negativeOkay="BUCKET_NEG_OK_"+std::to_string(i);
    if(!s.bucketNegative[i])
    {
        if(Slot(a)==owner)
        {
            if(s.uiPhase==0&&item&&item->phase==NativeEquipmentPhase::World&&b->mCursorObject->mCursorType==CURSOR_TYPE_NORMAL)
            {if(BoardClick(a,coin->mPosX+20,coin->mPosY+20)){s.uiPhase=1;NetTrace("SMOKE_BUCKET274_UI role=%s operation=%d owner=%d phase=pickup coin=%u token=%u",Role(a),i,owner,coin->mNetId,item->instanceToken);}return;}
            if(s.uiPhase==1&&item&&item->phase==NativeEquipmentPhase::Cursor&&item->claimSlot==owner)
            {if(BoardClick(a,float(b->GetCellCenterX(8,0)),float(b->GridToPixelY(8,0)+50))){s.uiPhase=2;s.uiSince=now;s.bucketNegativeSamples=Samples(a);NetTrace("SMOKE_BUCKET274_UI role=%s operation=%d owner=%d phase=unsupported-use guard=%u",Role(a),i,owner,s.receiverGuard.id);}return;}
            // SDL input is processed before this later reliable chat marker.
            // Peer observation and post-input snapshots prove preservation;
            // no fixture calls the production command handler directly.
            if(s.uiPhase!=2||now-s.uiSince<850||Samples(a)-s.bucketNegativeSamples<3||!Send(a,Tag(negative.c_str())))return;
        }
        if(!Has(a,owner,Tag(negative.c_str())))return;
        if(Slot(a)!=owner&&!s.bucketNegativeSamples){s.bucketNegativeSamples=Samples(a);return;}
        if(Samples(a)-s.bucketNegativeSamples<3||!BucketPreserved274(a,i))return;
        s.bucketNegative[i]=true;
        NetTrace("SMOKE_BUCKET274_PRESERVED role=%s operation=%d owner=%d coin=%u token=%u guard=%u original=%u realSDL=1 serverRejectionCode=NotClaimed",Role(a),i,owner,s.items[i].id,s.items[i].token,s.receiverGuard.id,s.actors[i].id);
    }
    if(!Barrier(a,negativeOkay.c_str()))return;
    if(s.uiPhase<3)s.uiPhase=3;
    if(Slot(a)==owner&&s.uiPhase==3)
    {
        if(!BucketPreserved274(a,i))return;
        if(BoardClick(a,float(b->GetCellCenterX(spec.col,spec.row)),float(b->GridToPixelY(spec.col,spec.row)+50)))
        {s.uiPhase=4;NetTrace("SMOKE_BUCKET274_UI role=%s operation=%d owner=%d phase=replace source=%u old=%u oldToken=%u",Role(a),i,owner,spec.kind,s.actors[i].id,s.actors[i].token);}
        return;
    }
    if(!BucketReplacement274(a,i))return;
    if(!s.bucketResultSamples){s.bucketResultSamples=Samples(a);return;}
    if(Samples(a)-s.bucketResultSamples<3)return;
    const auto proof="BUCKET_DONE_"+std::to_string(i)+"_"+std::to_string(s.bucketFresh[i].id)+"_"+std::to_string(s.bucketFresh[i].token);
    if(!Barrier(a,proof.c_str()))return;
    NetTrace("SMOKE_BUCKET274_REPLACED role=%s operation=%d owner=%d source=%u result=%u old=%u oldToken=%u fresh=%u freshToken=%u hp=%d item=%u itemToken=%u consumed=1 stablePostSnapshots=3 sharedOwner=1 defaultParents=1 packet=0 realSDL=1",
        Role(a),i,owner,spec.kind,spec.kind==0?1020:1029,s.actors[i].id,s.actors[i].token,s.bucketFresh[i].id,s.bucketFresh[i].token,
        FusionFindPlant(spec.kind==0?1020:1029)->maxHealth,s.items[i].id,s.items[i].token);
    s.mask|=1<<i;++s.ui;s.uiPhase=0;s.uiSince=0;s.bucketNegativeSamples=s.bucketResultSamples=0;
}
bool ReceiverComplete(Board* b,int index)
{
    if(index>=8)
    {
        auto* p=At(b,8,0,1129);const auto* d=GetNativePlantData(p);
        if(!p||!d||p->mNetId==s.machineParent.id||d->instanceToken==s.machineParent.token||At(b,8,0,3))return false;
        if(index==8)
        {if(p->mPlantHealth!=4000&&p->mPlantHealth!=4500)return false;if(!s.machineFresh.id)s.machineFresh={p->mNetId,d->instanceToken};return true;}
        return p->mNetId==s.machineFresh.id&&d->instanceToken==s.machineFresh.token&&p->mPlantHealth==4500;
    }
    if(index==0){auto* fresh=At(b,3,0,1028);const auto* d=GetNativePlantData(fresh);return fresh&&d&&fresh->mNetId!=s.actors[1].id&&d->instanceToken!=s.actors[1].token&&fresh->mPlantHealth==16000;}
    if(index==1){auto* p=Actor(b,2);return p&&p->mPlantHealth==15500;}
    if(index==2){auto* p=Actor(b,3);return p&&p->mPlantHealth==7500;}
    const int i=index-3;auto* p=At(b,i+2,5,i==0?1092:i==1?1097:i==2?1098:i==3?1099:935);const auto* d=GetNativePlantData(p);
    return p&&d&&p->mNetId==s.receivers[i].id&&d->instanceToken==s.receivers[i].token&&p->mPlantHealth==p->mPlantMaxHealth-100;
}
void UseItems(LawnApp* a)
{
    if(s.group!=2||!a->NetIsClient()||s.ui>=10||a->mBoard->mPaused)return;auto* b=a->mBoard;
    auto* coin=ResolveNativeEquipment(b,s.items[s.ui].id,s.items[s.ui].token);auto* data=GetNativeEquipmentData(coin);
    if(ReceiverComplete(b,s.ui)&&!coin){++s.ui;s.uiPhase=0;s.uiSince=0;return;}
    if(!s.uiSince)s.uiSince=GetTickCount();
    if(GetTickCount()-s.uiSince>10000){a->CoopSmokeFail("Equipment268 authenticated pickup/use did not commit within10s");return;}
    if(s.uiPhase==0&&data&&data->phase==NativeEquipmentPhase::World)
    {if(BoardClick(a,coin->mPosX+20,coin->mPosY+20)){s.uiPhase=1;NetTrace("SMOKE_EQUIPMENT268_UI role=client index=%d coin=%u phase=pickup",s.ui,coin->mNetId);}return;}
    if(s.uiPhase==1&&data&&data->phase==NativeEquipmentPhase::Cursor&&data->claimSlot==1)
    {
        const int col=s.ui>=8?8:s.ui<3?3+2*s.ui:s.ui-1,row=s.ui>=8||s.ui<3?0:5;
        if(BoardClick(a,float(b->GetCellCenterX(col,row)),float(b->GridToPixelY(col,row)+50))){s.uiPhase=2;NetTrace("SMOKE_EQUIPMENT268_UI role=client index=%d coin=%u phase=use column=%d row=%d physicalType=%u",s.ui,coin->mNetId,col,row,unsigned(data->type));}
    }
}
void ObservePumpkinProofs(LawnApp* a,int elapsed)
{
    auto* b=a->mBoard;
    if(a->NetIsClient())
    {
        const auto prefix=Tag("PULSE")+" ";
        for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==0&&line.mText.starts_with(prefix))
        {
            std::istringstream input(line.mText.substr(prefix.size()));unsigned index=0,id=0,token=0,target=0;PulseProof proof;std::string extra;
            if(!Require(a,bool(input>>index>>id>>token>>target>>proof.damage>>proof.before>>proof.after)&&index<5&&id>0&&id<=65535&&token&&target>0&&target<=65535&&
                proof.before>0&&proof.before<=64000&&proof.after>0&&proof.after<=proof.before&&!(input>>extra),"Equipment268 malformed authoritative pulse proof"))return;
            if(s.pulses[index].ready)continue;
            if(!Require(a,id==s.actors[index].id&&token==s.actors[index].token&&target==s.zombies[index],"Equipment268 pulse source generation/target differs"))return;
            const auto* profile=FindFusionEquipmentProfile(Specs()[index].kind);
            if(!Require(a,profile&&(profile->attackDamage==0?proof.damage==0:proof.damage>=200&&proof.damage==proof.before-proof.after),"Equipment268 source pulse damage proof differs"))return;
            proof.source={uint16_t(id),token};proof.target=uint16_t(target);proof.ready=true;s.pulses[index]=proof;
            NetTrace("SMOKE_EQUIPMENT268_PULSE_PROOF role=client text=%s timer=NotClaimed",line.mText.c_str());
        }
    }
    auto* damaged=ZombieById(b,s.zombies[0]);
    if(damaged&&damaged->mHelmType==HELMTYPE_BUCKET&&damaged->mBodyHealth+damaged->mHelmHealth<=s.targetInitialHealth[0]-200)s.pumpkinDamageSeen=true;
    for(int i=0;i<5;++i)
    {
        auto* p=Actor(b,i);auto* z=ZombieById(b,s.zombies[i]);const auto* state=GetNativeEquipmentPlantState(p);const auto* profile=FindFusionEquipmentProfile(Specs()[i].kind);
        if(!p||!z||!profile)continue;const int health=z->mBodyHealth+z->mHelmHealth;auto& proof=s.pulses[i];
        if(a->NetIsHost())
        {
            // Only the authority reads these unsent native counters. The
            // signed zero callbacks do not claim the neighbour's HP damage.
            if(p->mTargetX>s.previousClock[i]&&elapsed>500&&state&&state->componentSize==5&&z->mHelmType==HELMTYPE_BUCKET&&
                (profile->attackDamage==0||s.previousTargetHealth[i]-health>=200))
            {
                s.pulseSeen[i]=true;
                if(!proof.ready){proof.source=s.actors[i];proof.target=z->mNetId;proof.before=s.previousTargetHealth[i];proof.after=health;proof.damage=profile->attackDamage?proof.before-proof.after:0;proof.ready=true;}
            }
            s.previousClock[i]=p->mTargetX;s.previousTargetHealth[i]=health;
        }
        else if(proof.ready&&state&&state->componentSize==5&&z->mHelmType==HELMTYPE_BUCKET&&health<=proof.after&&s.pumpkinDamageSeen)s.pulseSeen[i]=true;
        if(s.pulseSeen[i]&&state&&state->componentSize==5&&z->mHelmType==HELMTYPE_BUCKET&&
            (profile->attackDamage==0||health<=s.targetInitialHealth[i]-200))s.mask|=1<<i;
    }
}
void ObserveMineProof(LawnApp* a,int elapsed)
{
    auto* b=a->mBoard;Plant* child=nullptr;
    while(b->IteratePlants(child))
    {
        const auto* cd=GetNativePlantData(child);if(!cd||cd->kind!=4||child->mDead||std::abs(child->mPlantCol-8)>1||std::abs(child->mRow-4)>1)continue;
        if(child->mSeedType!=SEED_POTATOMINE||s.baselineMineTokens.contains(cd->instanceToken))continue;
        // Client binds real FNS generations and cells, never unsent timers.
        if(a->NetIsHost()&&(child->mStateCountdown<=0||child->mStateCountdown>501))continue;
        if(!Require(a,cd->owner==LawnNet::NET_SHARED_OWNER&&!cd->packetId&&!cd->randomOrigin&&cd->firstParent==kFusionNoPlant&&cd->secondParent==kFusionNoPlant&&
            child->mNetId!=s.actors[6].id&&cd->instanceToken!=s.actors[6].token,"Equipment268 CherryMine child world provenance differs"))return;
        const int cell=(child->mPlantCol-7)*3+child->mRow-3;auto& bound=s.mineRefs[cell];
        if(!bound.id)
        {
            bound={child->mNetId,cd->instanceToken};
            NetTrace("SMOKE_EQUIPMENT268_MINE_BIND role=%s id=%u token=%u column=%d row=%d preparation=%d timer=%s elapsed=%d",Role(a),bound.id,bound.token,child->mPlantCol,child->mRow,
                a->NetIsHost()?child->mStateCountdown:-1,a->NetIsHost()?"Authoritative":"NotClaimed",elapsed);
        }
        if(a->NetIsHost()&&bound.id==child->mNetId&&bound.token==cd->instanceToken)s.mineChildren.insert(cd->instanceToken);
    }
    if(a->NetIsHost())
    {
        if(!s.mineProofReady&&s.mineChildren.size()==9)
        {
            s.mineProof=Tag("MINES")+" "+std::to_string(s.actors[6].id)+" "+std::to_string(s.actors[6].token);
            for(const auto& ref:s.mineRefs){if(!Require(a,ref.id&&ref.token,"Equipment268 incomplete nine-cell mine proof"))return;s.mineProof+=" "+std::to_string(ref.id);}
            if(!Require(a,s.mineProof.size()<=LawnNet::NET_MAX_CHAT_CHARS,"Equipment268 nine-mine proof exceeds native chat"))return;s.mineProofReady=true;
        }
    }
    else
    {
        const auto prefix=Tag("MINES")+" ";
        if(!s.mineProofReady)for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==0&&line.mText.starts_with(prefix))
        {
            std::istringstream input(line.mText.substr(prefix.size()));unsigned parent=0,token=0;std::set<uint16_t> unique;std::string extra;
            if(!Require(a,bool(input>>parent>>token)&&parent==s.actors[6].id&&token==s.actors[6].token,"Equipment268 mine proof parent generation differs"))return;
            for(auto& id:s.provenMineIds){unsigned value=0;if(!Require(a,bool(input>>value)&&value>0&&value<=65535&&unique.insert(uint16_t(value)).second,"Equipment268 mine proof has invalid/duplicate UID"))return;id=uint16_t(value);}
            if(!Require(a,!(input>>extra),"Equipment268 mine proof has extra fields"))return;
            s.mineProofReady=true;NetTrace("SMOKE_EQUIPMENT268_MINE_PROOF role=client text=%s preparationTimer=NotClaimed",line.mText.c_str());break;
        }
        if(s.mineProofReady)for(std::size_t i=0;i<s.mineRefs.size();++i)if(s.mineRefs[i].id==s.provenMineIds[i]&&s.mineRefs[i].token)s.mineChildren.insert(s.mineRefs[i].token);
    }
}
void PublishGroup1Proofs(LawnApp* a)
{
    if(!a->NetIsHost())return;
    // Publish the time-sensitive nine-child birth first; source preparation is
    // five seconds and a child may retire before a later proof is delivered.
    if(s.mineProofReady&&!s.mineProofSent)
    {if(!Send(a,s.mineProof))return;s.mineProofSent=true;NetTrace("SMOKE_EQUIPMENT268_MINE_PROOF role=host text=%s preparation=1..501",s.mineProof.c_str());}
    for(std::size_t i=0;i<s.pulses.size();++i)
    {
        auto& p=s.pulses[i];if(!p.ready||p.sent)continue;
        const auto value=Tag("PULSE")+" "+std::to_string(i)+" "+std::to_string(p.source.id)+" "+std::to_string(p.source.token)+" "+std::to_string(p.target)+" "+std::to_string(p.damage)+" "+std::to_string(p.before)+" "+std::to_string(p.after);
        if(!Require(a,value.size()<=LawnNet::NET_MAX_CHAT_CHARS,"Equipment268 pulse proof exceeds native chat")||!Send(a,value))return;
        p.sent=true;NetTrace("SMOKE_EQUIPMENT268_PULSE_PROOF role=host text=%s counterReset=1 zeroDamageDoesNotClaimNeighbourDamage=1",value.c_str());
    }
}
void ObserveBloverProof(LawnApp* a,int elapsed)
{
    auto* b=a->mBoard;auto* balloon=ZombieById(b,s.balloonId);
    if(balloon)
    {
        if(!Require(a,balloon->mZombieType==ZOMBIE_BALLOON&&!balloon->mMindControlled,"Equipment268 air control identity changed"))return;
        if(balloon->mPosX>s.balloonMaxX)s.balloonMaxX=balloon->mPosX;
        if(!s.balloonMoved&&s.balloonMaxX>s.balloonStartX+20)
        {s.balloonMoved=true;NetTrace("SMOKE_EQUIPMENT268_AIR_MOVE role=%s id=%u initialX=%.9g observedX=%.9g elapsed=%d",Role(a),s.balloonId,s.balloonStartX,s.balloonMaxX,elapsed);}
        if(a->NetIsHost()&&!s.blowProofReady&&balloon->mBlowingAway)
        {
            auto* source=Actor(b,0);const auto* data=GetNativePlantData(source);
            if(!Require(a,s.balloonInitialAir&&source&&data&&data->kind==1102,"Equipment268 blow proof has no live source generation"))return;
            s.blowProof=Tag("BLOW")+" "+std::to_string(source->mNetId)+" "+std::to_string(data->instanceToken)+" "+std::to_string(s.balloonId);
            if(!Require(a,s.blowProof.size()<=LawnNet::NET_MAX_CHAT_CHARS,"Equipment268 blow proof exceeds native chat"))return;s.blowProofReady=true;
            NetTrace("SMOKE_EQUIPMENT268_BLOW_FACT role=host text=%s actualBlowingAway=1 tick=%d",s.blowProof.c_str(),b->mMainCounter);
        }
    }
    else if(s.balloonInitialAir&&!s.balloonRetired)
    {
        // A missing actor alone is never evidence of blow-away. Both peers
        // must have observed this original air UID move right before removal.
        if(!Require(a,s.balloonMoved,"Equipment268 air UID retired without observed rightward delta"))return;
        s.balloonRetired=true;NetTrace("SMOKE_EQUIPMENT268_AIR_RETIRED role=%s id=%u priorRightwardDelta=%.9g elapsed=%d",Role(a),s.balloonId,s.balloonMaxX-s.balloonStartX,elapsed);
    }
    if(a->NetIsHost())
    {
        if(s.blowProofReady&&!s.blowProofSent&&Send(a,s.blowProof))
        {s.blowProofSent=true;NetTrace("SMOKE_EQUIPMENT268_BLOW_PROOF role=host text=%s",s.blowProof.c_str());}
    }
    else if(!s.blowProofReady)
    {
        const auto prefix=Tag("BLOW")+" ";
        for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==0&&line.mText.starts_with(prefix))
        {
            std::istringstream input(line.mText.substr(prefix.size()));unsigned source=0,token=0,target=0;std::string extra;
            if(!Require(a,bool(input>>source>>token>>target)&&source==s.actors[0].id&&token==s.actors[0].token&&target==s.balloonId&&s.balloonInitialAir&&!(input>>extra),"Equipment268 authoritative blow proof identity differs"))return;
            s.blowProofReady=true;NetTrace("SMOKE_EQUIPMENT268_BLOW_PROOF role=client text=%s rawBlowingAwayFlag=NotClaimed",line.mText.c_str());break;
        }
    }
    if(s.blowProofReady&&s.balloonInitialAir&&s.balloonMoved&&s.balloonRetired)s.balloonBlown=true;
}
void TraceCohort(LawnApp* a)
{
    auto* b=a->mBoard;Zombie* z=nullptr;Zombie* winner=nullptr;Zombie* leftmost=nullptr;int winners=0;
    while(b->IterateZombies(z))if(!z->mDead)
    {
        // Board.ZombiesWon marks its actual triggering actor in WalkIntoHouse.
        if(z->mFromWave==Zombie::ZOMBIE_WAVE_WINNER){if(!winner)winner=z;++winners;}
        if(!z->IsDeadOrDying()&&!z->mMindControlled&&(!leftmost||z->mPosX<leftmost->mPosX))leftmost=z;
    }
    unsigned actors=0,targets=0;
    for(int i=0;i<int(Specs().size());++i)if(Actor(b,i))actors|=1U<<i;
    for(std::size_t i=0;i<s.zombies.size();++i)if(ZombieById(b,s.zombies[i]))targets|=1U<<i;
    auto* guard0=s.group==0?At(b,1,0,931):nullptr;auto* guard5=s.group==0?At(b,2,5,931):nullptr;
    NetTrace("SMOKE_EQUIPMENT268_COHORT role=%s group=%d tick=%d start=%d elapsed=%d scene=%d paused=%d winnerCount=%d winnerUID=%u winnerRow=%d winnerX=%.9g winnerEating=%d leftmostUID=%u leftmostRow=%d leftmostX=%.9g leftmostEating=%d leftmostPhase=%d actors=%02x targets=%02x guard0UID=%u guard0HP=%d guard5UID=%u guard5HP=%d",
        Role(a),s.group,b->mMainCounter,s.start,b->mMainCounter-s.start,int(a->mGameScene),b->mPaused?1:0,winners,
        winner?winner->mNetId:0,winner?winner->mRow:-1,winner?winner->mPosX:0.f,winner&&winner->mIsEating?1:0,
        leftmost?leftmost->mNetId:0,leftmost?leftmost->mRow:-1,leftmost?leftmost->mPosX:0.f,leftmost&&leftmost->mIsEating?1:0,leftmost?int(leftmost->mZombiePhase):-1,
        actors,targets,guard0?guard0->mNetId:0,guard0?guard0->mPlantHealth:-1,guard5?guard5->mNetId:0,guard5?guard5->mPlantHealth:-1);
}
void Observe(LawnApp* a)
{
    if(s.group==3){ObserveBuckets274(a);return;}
    auto* b=a->mBoard;const int elapsed=b->mMainCounter-s.start;
    if(a->mGameScene==SCENE_ZOMBIES_WON){TraceCohort(a);a->CoopSmokeFail("Equipment268 natural cohort reached terminal scene");return;}
    for(int i=0;i<int(Specs().size());++i)
    {
        auto* p=Actor(b,i);const auto* state=GetNativeEquipmentPlantState(p);const auto* d=GetNativePlantData(p);
        if(state&&state->targetItemId)
        {auto* coin=ResolveNativeEquipment(b,state->targetItemId,state->targetItemToken);const auto* item=GetNativeEquipmentData(coin);if(item&&item->sourcePlantId==p->mNetId&&item->sourcePlantToken==d->instanceToken){s.heldSeen[i]=true;if(s.group==0&&i==2)s.seaItems.insert(item->instanceToken);if(s.group==2&&i==0&&item->phase==NativeEquipmentPhase::Attached){s.blowCapture=true;s.blowItem={coin->mNetId,item->instanceToken};}}}
        if(s.group==0)
        {
            if(i==0&&s.heldSeen[i]&&state&&!state->targetItemId&&elapsed>1500)s.mask|=1<<i;
            if(i==1&&s.heldSeen[i]&&state&&state->retainedRowRange==4)
            {auto* z=ZombieById(b,s.zombies[1]);if(z&&z->mHelmType==HELMTYPE_NONE&&z->mBodyHealth<=31800)s.mask|=1<<i;}
            if(i==2&&d&&d->batchMode==kEquipmentGrow)s.growSeen=true;
            if(i==2&&state&&state->seaGrown&&state->seaCallbacks>=3&&s.seaItems.size()>=3&&s.growSeen)s.mask|=1<<i;
        }
    }
    if(s.group==1)
    {
        ObservePumpkinProofs(a,elapsed);if(a->mCoopSmokeResultWritten)return;
        auto* machine=Actor(b,5);const auto* md=GetNativePlantData(machine);auto* sun=At(b,7,0,1);const auto* sd=GetNativePlantData(sun);
        if(machine&&md)
        {
            if(md->batchMode==1)s.machineIce=true;if(md->batchMode==2)s.machineFire=true;
            if(machine->mPlantHealth>4000)s.machineOverheal=true;
            if(machine->mPlantHealth<s.machinePreviousHealth&&sd&&sun->mNetId==s.protectedSun.id&&sd->instanceToken==s.protectedSun.token&&sun->mPlantHealth==300)s.machineProtection=true;
            s.machinePreviousHealth=machine->mPlantHealth;
            if(s.machineIce&&s.machineFire&&s.machineOverheal&&s.machineProtection)s.mask|=1<<5;
        }
        auto* mineTarget=ZombieById(b,s.zombies[7]);
        if(mineTarget&&mineTarget->mBodyHealth<=30200)s.mineDamage=true;
        if(mineTarget&&mineTarget->mBodyHealth<=28400)s.mineChildDamage=true;
        ObserveMineProof(a,elapsed);if(a->mCoopSmokeResultWritten)return;PublishGroup1Proofs(a);if(a->mCoopSmokeResultWritten)return;
        if(!Actor(b,6)&&s.mineChildren.size()==9&&s.mineDamage&&s.mineChildDamage)s.mask|=1<<6;
    }
    if(s.group==0)
    {
        auto* producer=Actor(b,3);Coin* c=nullptr;
        while(b->IterateCoins(c))if(!c->mDead&&producer&&std::abs(c->mPosX-producer->mX)<160&&std::abs(c->mPosY-producer->mY)<140)
        {const auto* item=GetNativeEquipmentData(c);if(item&&!item->sourcePlantId&&FusionEquipmentTypeValid(int(item->type)))s.producedItem=true;if(!item&&c->IsSun()&&c->GetSunValue()==25)s.sunlight=true;}
        if(s.producedItem&&s.sunlight)s.mask|=8;
    }
    if(s.group==2)
    {
        ObserveBloverProof(a,elapsed);if(a->mCoopSmokeResultWritten)return;
        if(!Actor(b,0)&&s.blowCapture&&s.balloonBlown&&elapsed>1000)s.mask|=1;
        for(int i=0;i<3;++i)if(ReceiverComplete(b,i)&&!ResolveNativeEquipment(b,s.items[i].id,s.items[i].token))s.mask|=1<<(i+1);
        UseItems(a);
    }
    if(s.stage==5&&s.baseline&&!b->mPaused&&((s.group!=1&&elapsed>20)||(s.group==1&&elapsed>605)))Capture(a,"flight");
    if(!s.diagnostic||GetTickCount()-s.diagnostic>5000)
    {
        s.diagnostic=GetTickCount();NetTrace("SMOKE_EQUIPMENT268_OBSERVE role=%s group=%d elapsed=%d mask=%02x expected=%02x seaItems=%zu grownFrame=%d sun=%d item=%d ui=%d scene=%d",Role(a),s.group,elapsed,s.mask,Expected(),s.seaItems.size(),s.growSeen?1:0,s.sunlight?1:0,s.producedItem?1:0,s.ui,int(a->mGameScene));
        TraceCohort(a);
        if(s.group==1)
        {
            auto* machine=Actor(b,5);const auto* d=GetNativePlantData(machine);auto* sun=At(b,7,0,1);auto* target=ZombieById(b,s.zombies[7]);
            NetTrace("SMOKE_EQUIPMENT268_EXTRAS role=%s elapsed=%d machineHP=%d form=%d ice=%d fire=%d overheal=%d protect=%d protectedHP=%d cherryAlive=%d children=%zu deathHit=%d childHit=%d targetHP=%d",Role(a),elapsed,machine?machine->mPlantHealth:-1,d?int(d->batchMode):-1,s.machineIce?1:0,s.machineFire?1:0,s.machineOverheal?1:0,s.machineProtection?1:0,sun?sun->mPlantHealth:-1,Actor(b,6)?1:0,s.mineChildren.size(),s.mineDamage?1:0,s.mineChildDamage?1:0,target?target->mBodyHealth:-1);
        }
        if(s.group==2)
        {
            NetTrace("SMOKE_EQUIPMENT268_BLOVER role=%s elapsed=%d airUID=%u baselineX=%.9g maxX=%.9g moved=%d retired=%d hostProof=%d attachedCoin=%u attachedToken=%u sourceAlive=%d",Role(a),elapsed,s.balloonId,s.balloonStartX,s.balloonMaxX,s.balloonMoved?1:0,s.balloonRetired?1:0,s.blowProofReady?1:0,s.blowItem.id,s.blowItem.token,Actor(b,0)?1:0);
            unsigned receivers=0,coins=0;for(int i=0;i<10;++i){if(ReceiverComplete(b,i))receivers|=1U<<i;if(ResolveNativeEquipment(b,s.items[i].id,s.items[i].token))coins|=1U<<i;}
            std::array<int,5> health{};for(int i=0;i<5;++i){auto* p=At(b,i+2,5,i==0?1092:i==1?1097:i==2?1098:i==3?1099:935);health[i]=p?p->mPlantHealth:-1;}
            auto* guard=At(b,7,5,931);const auto* gd=GetNativePlantData(guard);auto* machine=At(b,8,0,1129);
            NetTrace("SMOKE_EQUIPMENT268_RECEIVERS role=%s elapsed=%d receiverBits=%03x liveCoinBits=%03x pumpkinHP=%d,%d,%d,%d,%d machineUID=%u machineFreshUID=%u machineHP=%d guardUID=%u guardHP=%d guardSameGeneration=%d blowProofReady=%d blowProofSent=%d effectsOwn=%d effectsPeer=%d",
                Role(a),elapsed,receivers,coins,health[0],health[1],health[2],health[3],health[4],machine?machine->mNetId:0,s.machineFresh.id,machine?machine->mPlantHealth:-1,
                guard?guard->mNetId:0,guard?guard->mPlantHealth:-1,guard&&gd&&guard->mNetId==s.receiverGuard.id&&gd->instanceToken==s.receiverGuard.token?1:0,
                s.blowProofReady?1:0,s.blowProofSent?1:0,Has(a,Slot(a),Tag("EFFECTS"))?1:0,Has(a,1-Slot(a),Tag("EFFECTS"))?1:0);
        }
    }
}
bool Complete(LawnApp* a)
{
    if(s.mask!=Expected())return false;
    if(s.group==3)
    {
        if(s.ui!=4)return false;
        for(int i=0;i<4;++i)if(!s.bucketNegative[i]||!BucketReplacement274(a,i))return false;
        auto* guard=At(a->mBoard,8,0,4);const auto* d=GetNativePlantData(guard);
        return Require(a,guard&&d&&guard->mNetId==s.receiverGuard.id&&d->instanceToken==s.receiverGuard.token&&
            guard->mPlantHealth==300&&guard->mPlantMaxHealth==300,"Bucket274 unsupported guard changed after the accepted item operations");
    }
    if(s.group==1)
    {
        if(!s.mineProofReady||(a->NetIsHost()&&!s.mineProofSent))return false;
        for(const auto& proof:s.pulses)if(!proof.ready||(a->NetIsHost()&&!proof.sent))return false;
    }
    if(s.group==2)
    {
        if(!s.blowProofReady||(a->NetIsHost()&&!s.blowProofSent))return false;
        for(int i=0;i<10;++i)if(!ReceiverComplete(a->mBoard,i)||ResolveNativeEquipment(a->mBoard,s.items[i].id,s.items[i].token))return false;
    }
    return true;
}
}
namespace LawnNet
{
bool NetSmokeUpdateFusionEquipment268(LawnApp* a)
{
    const char* enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_EQUIPMENT268"),*native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
    if(!a||!enabled||std::strcmp(enabled,"1")||!native||std::strcmp(native,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
    if(a->mCoopSmokeResultWritten)return true;if(!s.since){s.since=GetTickCount();if(Bucket274()){s.group=3;NetTrace("SMOKE_BUCKET274_SELECTED role=%s cohort=Bucket274 hostOperations=2 clientOperations=2 isolatedInitialFactory=1",Role(a));}}
    if(GetTickCount()-s.since>150000){char why[160];sprintf_s(why,"Equipment268 bounded deadline group=%d stage=%d mask=%02x expected=%02x ui=%d",s.group,s.stage,s.mask,Expected(),s.ui);a->CoopSmokeFail(why);return true;}
    if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
    if(s.stage==0)
    {
        if(a->NetIsHost()&&!s.configured){a->mNetManager->SetDifficulty(1);s.configured=true;}
        if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
        if(a->NetIsClient()){Advance(a,1);Send(a,"E268_BEGIN");}
        else if(Has(a,1,"E268_BEGIN")){Advance(a,1);std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;
    }
    if(s.stage==10)
    {
        if(!Barrier(a,"NEXT"))return true;
        if(s.group==3){NetTrace("SMOKE_BUCKET274_COMPLETE role=%s operations=4 hostOperations=2 clientOperations=2 unsupportedPreserved=4 source0to1020=2 source3to1029=2 freshUIDToken=1 canonicalHP=1 sharedOwner=1 itemConsumedOnce=1 postSetupActorWrites=0 acquisition=NotExecuted sourceAI=NotClaimed allocatorExhaustion=NotExecuted ironShieldProof=NotExecuted",Role(a));a->CoopSmokeWriteResult(true,nullptr);return true;}
        if(s.group==2){NetTrace("SMOKE_EQUIPMENT268_COMPLETE role=%s groups=3 sourcePlants=15 equipmentPlants=14 deathProducer=1 regressionAuxKind=1162 clientPreparationTimer=NotClaimed freshFrames=12 initialFactory=1 acquisition=NotExecuted sourceAI=NotClaimed postSetupActorWrites=0",Role(a));a->CoopSmokeWriteResult(true,nullptr);return true;}
        State next;next.group=s.group+1;next.previousEpoch=s.epoch;next.chat=s.chat;s=std::move(next);Advance(a,1);
        if(a->NetIsHost()){std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;
    }
    if(!a->mBoard||!IsNativeFusionBoard(a->mBoard))return true;auto* b=a->mBoard;const auto* context=GetNativeContext(b);
    if(context->epoch<=s.previousEpoch)return true;if(!s.epoch)s.epoch=context->epoch;
    if(!Require(a,context->epoch==s.epoch&&context->validation&&context->visuals&&!a->mFusionRuntime&&b->GetColumnCount()==10,"Equipment268 requires real matching native validation world"))return true;
    const auto& bank=context->selection.State();auto* chooser=a->mSeedChooserScreen;
    if(s.stage==1)
    {
        if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||chooser->mSeedsInFlight||!b->mCutScene->mSeedChoosing)return true;
        int own=0;for(const auto& packet:bank.packets)if(packet.owner==Slot(a))++own;if(own<s.waiting)return true;
        if(own<5){const int wanted=5*Slot(a)+own;int dense=-1;for(std::size_t i=0;i<context->eligible.size();++i)if(context->eligible[i].kind==wanted&&!context->eligible[i].isExtra)dense=int(i);if(!Require(a,dense>=0,"Equipment268 native chooser card missing"))return true;if(Choice(a,chooser,dense))s.waiting=own+1;return true;}
        if(bank.packets.size()==10&&Capture(a,"chooser"))Advance(a,2);
    }
    else if(s.stage==2)
    {
        if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}auto* button=chooser?chooser->mStartButton:nullptr;
        if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;
        const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
        if(!button->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"Equipment268 Ready hover failed");}
        else if(Click(a,x,y))Advance(a,3);
    }
    else if(s.stage==3)
    {
        if(bank.phase==FusionBankPhase::Intro){if(!s.introFirst)s.introFirst=GetTickCount();s.introLast=GetTickCount();++s.introChecks;return true;}
        if(bank.phase!=FusionBankPhase::Playing||chooser||a->mGameScene!=SCENE_PLAYING)return true;
        if(!Require(a,s.introChecks>=10&&s.introLast-s.introFirst>=1000,"Equipment268 full native intro skipped"))return true;
        if(s.entry<0){s.entry=a->mAppCounter;return true;}if(a->mAppCounter<=s.entry)return true;
        if(a->NetIsHost()&&!s.setup&&!Setup(a))return true;if(b->mPaused)Advance(a,4);
    }
    else if(s.stage==4)
    {
        if(!b->mPaused||!Baseline(a))
        {
            if(!s.diagnostic||GetTickCount()-s.diagnostic>5000)
            {s.diagnostic=GetTickCount();int actors=0,states=0;for(const auto& spec:Specs()){auto* p=At(b,spec.col,spec.row,spec.kind);if(p)++actors;if(GetNativeEquipmentPlantState(p))++states;}
             NetTrace("SMOKE_EQUIPMENT268_BASELINE_WAIT role=%s group=%d epoch=%u paused=%d background=%d actors=%d/%zu states=%d idsReady=%d targets=%zu samples=%u",Role(a),s.group,s.epoch,b->mPaused?1:0,int(b->mBackground),actors,Specs().size(),states,s.idsReady?1:0,s.zombies.size(),Samples(a));}
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
        const int minimum=s.group==3?20:s.group==0?4700:s.group==1?1800:1200;
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
        NetBuffer digest;for(const auto& spec:Specs()){auto* p=At(b,spec.col,spec.row,s.group==3?(spec.kind==0?1020:1029):s.group==2&&spec.kind==1027?1028:spec.kind);const auto* d=GetNativePlantData(p);digest.WriteU16(p?p->mNetId:0);digest.WriteU32(d?d->instanceToken:0);digest.WriteI32(p?p->mPlantHealth:-1);}
        char hp[40];sprintf_s(hp,"HP_%08x",NetFnv1a32(digest.Data(),digest.Size()));if(!Barrier(a,hp)||!Capture(a,"complete")||!Barrier(a,"FINAL"))return true;
        if(s.group==3)NetTrace("SMOKE_BUCKET274_CASE role=%s epoch=%u operations=4 mask=%02x finalSameIdentityHP=1 snapshots=%u freshFrames=%zu postSetupActorWrites=0",Role(a),s.epoch,s.mask,Samples(a),s.frames.size());
        else NetTrace("SMOKE_EQUIPMENT268_CASE role=%s group=%d epoch=%u kinds=%zu mask=%02x finalSameHP=1 snapshots=%u acquisition=NotExecuted sourceAI=NotClaimed clientPulseTimer=NotClaimed clientPreparationTimer=NotClaimed zeroDamageVariantsDoNotClaimDamage=1 regressionAuxKind=1162 postSetupActorWrites=0",Role(a),s.group,s.epoch,Specs().size(),s.mask,Samples(a));Advance(a,10);
    }
    return true;
}
}
