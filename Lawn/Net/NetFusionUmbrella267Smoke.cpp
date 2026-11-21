#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetCommon.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Zombie.h"
#include "../Cutscene.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionRecipe.h"
#include "../Fusion/FusionNativeVisual.h"
#include "../Fusion/FusionNativeFireActionBatch.h"
#include "../Fusion/FusionNativeUmbrellaBatch.h"
#include "../Fusion/FusionNativeAction267.h"
#include "../Fusion/FusionNativeWaterPlants267.h"
#include "../Fusion/FusionNativeHostileProjectile.h"
#include "../Fusion/FusionNativePlantDamage.h"
#include "../Fusion/FusionNativeLobber.h"
#include "../Coin.h"
#include <vector>
#include <algorithm>
#include "../Fusion/FusionNativeCold.h"
#include "../Fusion/FusionNativeProjectileMetadata.h"
#include "../../SexyAppFramework/ScrollbarWidget.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <sstream>
#include <string>

namespace
{
using namespace LawnFusion;
using LawnNet::NetTrace;
struct Spec {uint16_t kind;int col,row;};
int Group(){const char* value=std::getenv("PVZ_COOP_SMOKE_FUSION_UMBRELLA267_GROUP");return value&&std::strcmp(value,"1")==0?1:0;}
const std::vector<Spec> specs=Group()==0?std::vector<Spec>{{30,1,0},{1115,4,0},{1127,7,0},
    {1128,1,2},{1131,4,2},{1143,7,2},{1144,1,5},{1185,4,5},{1186,7,5},
    {931,0,1},{906,3,1},{913,6,1},{925,9,1},{936,6,4},{1077,0,3},{12,3,3},{19,6,3}}:
    std::vector<Spec>{{1181,1,0},{1176,4,5},{1086,7,3}};
int Expected(){return (1<<int(specs.size()))-1;}
struct Ref {uint16_t id=0;uint32_t token=0;int x=0;};
struct State
{
    int stage=0,waiting=0,entry=-1,start=0,introChecks=0,mask=0;
    uint32_t since=0,chat=0,wheel=0,epoch=0,introFirst=0,introLast=0,pauseSamples=0,diag=0,baselineWaitSince=0;
    bool configured=false,setup=false,pauseRequested=false,resumeRequested=false,baseline=false,targetProofsReady=false;
    bool windMotion=false,windFrame=false,windRetired=false;
    uint32_t windMotionSamples=0,windLastAliveSamples=0;
    float windLastX=0,windLastTargetX=0;
    std::array<Ref,20> actors{};Ref lilySun{};std::array<uint16_t,20> zombies{};
    std::array<bool,20> targetProofSent{},targetProofParsed{},targetProofAcked{};
    std::array<std::string,20> targetProofText{};
    std::array<int,20> initialHP{},previousHP{};std::array<int,20> initialZombieHP{};
    std::array<float,20> initialX{};int minimumEnergy=2000;bool depleted=false,recovered=false,reflect=false,reinforce=false,smallSun=false,bigSun=false,money100=false;uint16_t balloon=0;
    std::set<std::string> frames;std::map<std::string,int> draws;
} s;
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool ok,const char* why){if(!ok)a->CoopSmokeFail(why);return ok;}
void Advance(LawnApp* a,int n){s.stage=n;s.since=GetTickCount();s.diag=0;NetTrace("SMOKE_UMBRELLA267_STAGE role=%s epoch=%u stage=%d",Role(a),s.epoch,n);}
std::string Tag(const char* name){return "U267_"+std::to_string(Group())+"_"+std::string(name)+"_"+std::to_string(s.epoch);}
bool Has(LawnApp* a,int who,const std::string& value){for(const auto& l:a->mNetManager->mChatLines)if(l.mSlot==who&&l.mText==value)return true;return false;}
bool Send(LawnApp* a,const std::string& value){if(Has(a,Slot(a),value))return true;if(s.chat&&GetTickCount()-s.chat<800)return false;s.chat=GetTickCount();a->mNetManager->SendChat(value);return Has(a,Slot(a),value);}
bool Barrier(LawnApp* a,const char* name){const auto tag=Tag(name);Send(a,tag);return Has(a,0,tag)&&Has(a,1,tag);}
bool Click(LawnApp* a,float x,float y)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Umbrella267 scoped SDL click failed");
}
bool Escape(LawnApp* a)
{
    SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;
    SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Umbrella267 scoped SDL Escape failed");
}
bool Choice(LawnApp* a,SeedChooserScreen* c,int i)
{
    int x=0,y=0;c->GetSeedPositionInChooser(i,x,y);const float cy=float(y+35-int(c->mScrollbar->mScrollValue));
    if(cy<158||cy>493)
    {
        if(s.wheel&&GetTickCount()-s.wheel<250)return false;s.wheel=GetTickCount();
        SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=float(c->mX+300);m.motion.y=float(c->mY+300);
        SDL_Event w{};w.type=SDL_EVENT_MOUSE_WHEEL;w.wheel.windowID=m.motion.windowID;w.wheel.y=cy<158?1.f:-1.f;
        Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&w),"Umbrella267 scoped SDL chooser wheel failed");return false;
    }
    return std::abs(c->mScrollAmount)<=.05f&&Click(a,float(c->mX+x+25),float(c->mY)+cy);
}
bool Capture(LawnApp* a,const char* suffix)
{
    const std::string name="fusion_umbrella267_"+std::to_string(Group())+"_"+std::string(suffix);if(s.frames.contains(name))return true;
    if(!s.draws.contains(name))s.draws[name]=a->mDrawCount;
    const int requestedDraw=s.draws[name]+2;if(!a->CoopSmokeEnsureFreshFrameAfterDraw(requestedDraw))return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Umbrella267 actual native BMP capture failed"))return false;
    s.frames.insert(name);NetTrace("SMOKE_UMBRELLA267_RENDER role=%s frame=%s freshDraws=%d requestedDraw=%d capturedDraw=%d",Role(a),name.c_str(),a->mDrawCount-s.draws[name],requestedDraw,a->mCoopSmokeFrameDrawCount);return true;
}
Plant* At(Board* b,int col,int row,int kind)
{
    Plant* p=nullptr;while(b->IteratePlants(p)){const auto* d=GetNativePlantData(p);if(!p->mDead&&d&&d->kind==kind&&p->mPlantCol==col&&p->mRow==row)return p;}return nullptr;
}
Plant* Actor(Board* b,int i)
{
    auto* p=At(b,specs[i].col,specs[i].row,specs[i].kind);const auto* d=GetNativePlantData(p);
    return d&&p->mNetId==s.actors[i].id&&d->instanceToken==s.actors[i].token?p:nullptr;
}
Zombie* Target(Board* b,int i){Zombie* z=nullptr;while(b->IterateZombies(z))if(!z->mDead&&z->mNetId==s.zombies[i])return z;return nullptr;}
bool RequiredTarget(int i)
{return Group()||i==1||i==2||i==3||i==4||i==5||i==6||i==7||i==8||i==9||i==10||i==12||i==16;}
bool BaselineWait(LawnApp* a,const char* reason,int index=-1,uint16_t id=0)
{
    const uint32_t now=GetTickCount();if(!s.baselineWaitSince)s.baselineWaitSince=now;
    if(!s.diag||now-s.diag>=5000)
    {
        s.diag=now;Zombie* found=nullptr;Zombie* z=nullptr;
        while(a->mBoard->IterateZombies(z))if(z->mNetId==id){found=z;break;}
        NetTrace("SMOKE_UMBRELLA267_BASELINE_WAIT role=%s group=%d reason=%s index=%d id=%u currentRow=%d currentHP=%d snapshots=%u waitedMs=%u",Role(a),Group(),reason,index,id,found?found->mRow:-1,found?found->mBodyHealth:-1,Samples(a),now-s.baselineWaitSince);
    }
    if(now-s.baselineWaitSince>45000)
    {char why[180];sprintf_s(why,"Umbrella267 baseline wait exceeded45000ms group=%d reason=%s index=%d id=%u",Group(),reason,index,id);a->CoopSmokeFail(why);}
    return false;
}
bool DiscoverTargets(LawnApp* a)
{
    if(s.targetProofsReady)return true;
    // DrawCoopChat expires mChatLines at8seconds. Persist each accepted/parsed
    // proof here and finish its peer ACK before advancing; chat is transport,
    // not durable discovery state. MakeZombie captured these initial values.
    for(int i=0;i<int(specs.size());++i)if(RequiredTarget(i))
    {
        const std::string prefix=Tag(("NPC"+std::to_string(i)).c_str())+" ";
        if(a->NetIsHost())
        {
            if(!Require(a,s.zombies[i]&&s.initialZombieHP[i]>0&&std::isfinite(s.initialX[i]),"Umbrella267 host initial target identity missing"))return false;
            if(s.targetProofText[i].empty())
            {
                char message[128];sprintf_s(message,"%s%u %d %.9g",prefix.c_str(),s.zombies[i],s.initialZombieHP[i],s.initialX[i]);
                std::string normalized;
                if(!Require(a,std::strlen(message)<=LawnNet::NET_MAX_CHAT_CHARS&&
                    LawnNet::NetNormalizeUtf8(message,LawnNet::NET_MAX_CHAT_BYTES,LawnNet::NET_MAX_CHAT_CHARS,true,normalized)&&normalized==message,
                    "Umbrella267 target proof is not canonical native chat text"))return false;
                s.targetProofText[i]=message;
            }
            if(!s.targetProofSent[i])
            {
                if(!Send(a,s.targetProofText[i]))return BaselineWait(a,"sending host target proof",i,s.zombies[i]);
                s.targetProofSent[i]=true;
                NetTrace("SMOKE_UMBRELLA267_TARGET_PROOF role=host phase=sent group=%d index=%d chars=%zu text=%s",Group(),i,s.targetProofText[i].size(),s.targetProofText[i].c_str());
            }
            if(!s.targetProofAcked[i])
            {
                const auto ack=Tag(("NPCACK"+std::to_string(i)).c_str());
                if(!Has(a,1,ack))return BaselineWait(a,"waiting for client target ACK",i,s.zombies[i]);
                s.targetProofAcked[i]=true;
                NetTrace("SMOKE_UMBRELLA267_TARGET_PROOF role=host phase=acked group=%d index=%d chars=%zu text=%s",Group(),i,ack.size(),ack.c_str());
            }
        }
        else
        {
            if(!s.targetProofParsed[i])
            {
                for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==0&&line.mText.starts_with(prefix))
                {
                    unsigned id=0;int hp=0;float x=0;std::istringstream proof(line.mText.substr(prefix.size()));
                    const bool parsed=bool(proof>>id>>hp>>x);proof>>std::ws;
                    if(!Require(a,parsed&&proof.eof()&&id>0&&id<=65535&&hp>0&&std::isfinite(x),"Umbrella267 host target proof malformed"))return false;
                    if(!Require(a,!s.zombies[i]||(s.zombies[i]==id&&s.initialZombieHP[i]==hp&&s.initialX[i]==x),"Umbrella267 host target proof changed identity"))return false;
                    s.zombies[i]=uint16_t(id);s.initialZombieHP[i]=hp;s.initialX[i]=x;s.targetProofText[i]=line.mText;s.targetProofParsed[i]=true;
                    NetTrace("SMOKE_UMBRELLA267_TARGET_PROOF role=client phase=parsed group=%d index=%d chars=%zu text=%s",Group(),i,line.mText.size(),line.mText.c_str());break;
                }
                if(!s.targetProofParsed[i])return BaselineWait(a,"waiting for host target proof",i);
            }
            if(!s.targetProofAcked[i])
            {
                const auto ack=Tag(("NPCACK"+std::to_string(i)).c_str());
                if(!Send(a,ack))return BaselineWait(a,"sending client target ACK",i,s.zombies[i]);
                s.targetProofAcked[i]=true;
                NetTrace("SMOKE_UMBRELLA267_TARGET_PROOF role=client phase=acked group=%d index=%d chars=%zu text=%s",Group(),i,ack.size(),ack.c_str());
            }
        }
    }
    for(int i=0;i<int(specs.size());++i)if(RequiredTarget(i))
    {
        auto* z=Target(a->mBoard,i);if(!z)return BaselineWait(a,"waiting for target UID snapshot",i,s.zombies[i]);
        for(int previous=0;previous<i;++previous)if(RequiredTarget(previous)&&!Require(a,s.zombies[previous]!=s.zombies[i],"Umbrella267 duplicated target UID proof"))return false;
        // Rows, HP, X and statuses are live state: Garlic/area effects may
        // already have changed them before the first paused peer snapshot.
        NetTrace("SMOKE_UMBRELLA267_TARGET_BOUND role=%s group=%d index=%d id=%u initialHP=%d initialX=%.9g currentRow=%d currentHP=%d currentX=%.9g hostIdentity=1",Role(a),Group(),i,s.zombies[i],s.initialZombieHP[i],s.initialX[i],z->mRow,z->mBodyHealth,z->mPosX);
    }
    s.targetProofsReady=true;return true;
}
Plant* MakePlant(LawnApp* a,int kind,int col,int row)
{
    auto* b=a->mBoard;NativePlantData d;d.kind=uint16_t(kind);d.owner=uint8_t(col%2);
    for(size_t i=0;i<GetFusionRecipeCount();++i)
    {const auto& recipe=GetFusionRecipes()[i];if(recipe.result==kind){d.firstParent=recipe.existing;d.secondParent=recipe.used;break;}}
    if(!IsNativeBasePlantKind(d.kind)&&d.firstParent==kFusionNoPlant&&d.secondParent==kFusionNoPlant)
    {
        // Unpaid world fixture, with no fabricated recipe ancestry.
        d.owner=LawnNet::NET_SHARED_OWNER;d.packetId=0;d.randomOrigin=0;
    }
    std::string error;const bool loaded=EnsureNativePlantVisuals(b,uint16_t(kind),&error);
    if(!Require(a,loaded,error.empty()?"Umbrella267 source model missing":error.c_str()))return nullptr;
    auto* p=b->AddPlant(col,row,GetNativeSeedForPlant(uint16_t(kind)),SEED_NONE,&d);const auto* bound=GetNativePlantData(p);
    if(!Require(a,p&&bound&&bound->kind==kind&&bound->instanceToken&&p->mNetId&&!p->mDead,"Umbrella267 initial native typed factory failed"))return nullptr;
    return p;
}
Zombie* MakeZombie(LawnApp* a,int index,int col,int row,bool controlled=false,ZombieType type=ZOMBIE_NORMAL)
{
    auto* b=a->mBoard;auto* z=b->AddZombieInRow(type,row,0);
    if(!Require(a,z&&z->mNetId&&a->ReanimationTryToGet(z->mBodyReanimID),"Umbrella267 initial native control failed"))return nullptr;
    z->mBodyHealth=z->mBodyMaxHealth=32000;z->mHelmHealth=z->mHelmMaxHealth=0;
    z->mPosX=float(b->GetCellCenterX(col,row)-60);z->mX=int(z->mPosX);z->mPosY=z->GetPosYBasedOnRow(row);z->mY=int(z->mPosY);z->mVelX=0;z->mMindControlled=controlled;
    if(index>=0){s.zombies[index]=z->mNetId;s.initialZombieHP[index]=z->mBodyHealth;s.initialX[index]=z->mPosX;}
    NetTrace("SMOKE_UMBRELLA267_INITIAL_NPC group=%d index=%d id=%u nativeType=%d sourceAI=NotClaimed row=%d x=%.9g mc=%d initialOnly=1",Group(),index,z->mNetId,type,row,z->mPosX,controlled?1:0);
    return z;
}
bool Setup(LawnApp* a)
{
    auto* b=a->mBoard;
    if(!Require(a,a->NetIsHost()&&!b->mPaused&&!b->mPlants.mSize&&!b->mZombies.mSize,"Umbrella267 setup requires empty unpaused native world")||
       !Require(a,ConfigureNativeFusionBackground(b,BACKGROUND_3_POOL),"Umbrella267 source Pool6 configuration refused"))return false;
    if(!Group())for(int col:{0,1,4,7})if(!MakePlant(a,12,col,2))return false;
    for(const auto& spec:specs)if(!MakePlant(a,spec.kind,spec.col,spec.row))return false;
    if(!Group())
    {
        if(!MakePlant(a,1,3,3)||!MakePlant(a,26,0,2))return false;
        // Preserve the real Sunflower until Lily's strict90s callback. The
        // SeaShroom receiver can walk naturally after its first eating stop.
        if(!MakePlant(a,12,4,3)||!MakePlant(a,931,4,3))return false;
        if(auto* guard=At(b,4,3,931))
            NetTrace("SMOKE_UMBRELLA267_INITIAL_LILY_GUARD id=%u col=4 row=3 hp=%d max=%d sourceNative931=1 actualLilySupport=1 initialOnly=1",guard->mNetId,guard->mPlantHealth,guard->mPlantMaxHealth);
        // Native contact receivers resume walking when StopEating starts a new
        // walk animation. A real source defender keeps the90s Lily cohort live.
        // The previous terminal capture did not record its triggering UID.
        auto* guard=MakePlant(a,931,0,5);if(!guard)return false;
        NetTrace("SMOKE_UMBRELLA267_INITIAL_GUARD group=0 id=%u kind=931 col=0 row=5 hp=%d max=%d sourceRegen=1500/1500 initialOnly=1",guard->mNetId,guard->mPlantHealth,guard->mPlantMaxHealth);
        // The Gold skill includes adjacent rows; ordinary Umbrella contact
        // does not. Keep its target out of the opening same-row knockbacks.
        for(int i=1;i<=7;++i)if(!MakeZombie(a,i,specs[i].col,i==6?4:specs[i].row))return false;
        for(int i:{9,10,12})if(!MakeZombie(a,i,specs[i].col,specs[i].row))return false;
        if(!MakeZombie(a,3,3,2)||!MakeZombie(a,16,8,3))return false;
        auto* balloon=MakeZombie(a,8,7,5,false,ZOMBIE_BALLOON);if(!balloon)return false;s.balloon=balloon->mNetId;
        At(b,0,1,931)->mPlantHealth=13000;At(b,3,1,906)->mPlantHealth=7000;
        if(!Require(a,HandleNativeAction267Crush(At(b,3,1,906))&&At(b,3,1,906)->mPlantHealth==6950,"Umbrella267 initial source NULL crush failed"))return false;
        // A real source damage callback initializes the threshold boundary;
        // subsequent bites alone must trigger the strict>1000 explosion.
        ApplyNativeSourcePlantDamage(At(b,9,1,925),900,0);
        for(int i=0;i<9;++i)
        {
            auto* p=At(b,specs[i].col,specs[i].row,specs[i].kind);float x=0,y=0;if(!p->GetShadowCenter(x,y))return false;
            auto* shot=TryMakeNativeHostileBasketball(b,p,p->mRow,x+2*float(72.0/1.35),y);
            if(!Require(a,shot&&GetNativeProjectileData(shot),"Umbrella267 actual source58/13 factory refused"))return false;
            NetTrace("SMOKE_UMBRELLA267_INITIAL_SHOT group=0 index=%d projectile=%u kind=58 way=13 zombie=1 row=%d x=%.9g y=%.9g",i,shot->mNetId,shot->mRow,shot->mPosX+20,shot->mPosY+20);
        }
        if(!Require(a,TryActivateNativeUmbrellaBatchSkill(At(b,1,5,1144)),"Umbrella267 initial Gold skill refused"))return false;
    }
    else
    {
        if(!MakeZombie(a,0,4,0)||!MakeZombie(a,1,8,5,true)||!MakeZombie(a,2,8,3))return false;
        auto* balloon=MakeZombie(a,-1,5,4,false,ZOMBIE_BALLOON);if(!balloon)return false;s.balloon=balloon->mNetId;
    }
    s.setup=true;s.start=b->mMainCounter;
    NetTrace("SMOKE_UMBRELLA267_SETUP group=%d kinds=%zu actualPool=1 rows=6 nativeInitialFactory=1 acquisition=NotExecuted sourceAI=NotClaimed source58Factory=proven initialGoldSkill=%d initialOnly=1",Group(),specs.size(),Group()?0:1);return true;
}
bool Baseline(LawnApp* a)
{
    auto* b=a->mBoard;if(b->mBackground!=BACKGROUND_3_POOL||GetNativeFusionRowCount(b)!=6)return BaselineWait(a,"waiting for Pool terrain snapshot");
    for(int i=0;i<int(specs.size());++i)
    {
        const auto& spec=specs[i];auto* p=At(b,spec.col,spec.row,spec.kind);const auto* d=GetNativePlantData(p);if(!d)return BaselineWait(a,"waiting for plant snapshot",i);
        if(!Require(a,p->mNetId&&d->instanceToken&&p->mIsOnBoard,"Umbrella267 source identity differs"))return false;
        if(!s.actors[i].id)
        {
            s.actors[i]={p->mNetId,d->instanceToken,p->mX};s.initialHP[i]=s.previousHP[i]=p->mPlantHealth;
            NetTrace("SMOKE_UMBRELLA267_BIRTH role=%s group=%d index=%d kind=%u id=%u token=%u col=%d row=%d hp=%d max=%d speed=%.9g alias=%d ghost=%d",Role(a),Group(),i,spec.kind,p->mNetId,d->instanceToken,spec.col,spec.row,p->mPlantHealth,p->mPlantMaxHealth,d->animationSpeed,p->mSeedType,p->mNetGhost?1:0);
        }
    }
    if(!Group()&&!s.lilySun.id)
    {
        auto* sun=At(b,3,3,1);const auto* sd=GetNativePlantData(sun);
        if(!sun||!sd)return BaselineWait(a,"waiting for initial Lily Sun snapshot");
        if(!Require(a,sun->mNetId&&sd->instanceToken&&sun->mIsOnBoard,"Umbrella267 initial Lily Sun identity missing"))return false;
        s.lilySun={sun->mNetId,sd->instanceToken,sun->mX};
        NetTrace("SMOKE_UMBRELLA267_LILY_PARENT role=%s id=%u token=%u kind=1 col=3 row=3",Role(a),s.lilySun.id,s.lilySun.token);
    }
    if(!DiscoverTargets(a))return false;s.baselineWaitSince=0;return true;
}
bool HasMoney(Board* b,Plant* p,int amount)
{Coin* c=nullptr;while(b->IterateCoins(c))if(!c->mDead&&GetNativeCoinValue(c,0)==amount&&std::abs(c->mPosX-p->mX)<100&&std::abs(c->mPosY-p->mY)<140)return true;return false;}
void Observe(LawnApp* a)
{
    auto* b=a->mBoard;const int elapsed=b->mMainCounter-s.start;
    Projectile* shot=nullptr;while(b->IterateProjectiles(shot))
    {const auto* d=GetNativeProjectileData(shot);if(!d)continue;if(d->kind==59&&d->specialType==13)s.reflect=true;if(d->umbrellaed)s.reinforce=true;}
    Coin* coin=nullptr;while(b->IterateCoins(coin))
    {const int value=GetNativeCoinValue(coin,0);if(value==15)s.smallSun=true;if(value==50)s.bigSun=true;if(value==100)s.money100=true;}
    for(int i=0;i<int(specs.size());++i)
    {
        auto* p=Actor(b,i);auto* z=Target(b,i);const auto* d=GetNativePlantData(p);bool passed=false;
        if(Group())
        {
            if(i==0){const auto* poison=GetNativeZombieActionData(z);const auto* garlic=GetNativeZombieGarlicData(z);passed=z&&poison&&(poison->poisonTicks>0||poison->poisonLevel>0)&&(z->mRow!=0||(garlic&&FusionGarlicActionActive(*garlic)));}
            if(i==1)passed=z&&z->mMindControlled&&z->mPosX<s.initialX[i]-10;
            if(i==2)
            {
                if(z)
                {
                    s.windLastAliveSamples=Samples(a);s.windLastX=z->mPosX;s.windLastTargetX=z->mNetTargetX;
                    const bool motion=z->mBodyHealth>0&&!z->mMindControlled&&z->mPosX>=s.initialX[i]+72.f&&
                        (a->NetIsHost()?z->mBlowingAway:z->mNetGhost&&z->mVisible&&z->mNetTargetX>=s.initialX[i]+72.f);
                    if(motion&&!s.windMotion)
                    {
                        s.windMotion=true;s.windMotionSamples=Samples(a);
                        NetTrace("SMOKE_UMBRELLA267_WIND_MOTION role=%s group=1 id=%u initialX=%.9g currentX=%.9g targetX=%.9g blowingAway=%d ghost=%d snapshot=%u rightward72=1 alive=1 controlled=0",Role(a),z->mNetId,s.initialX[i],z->mPosX,z->mNetTargetX,z->mBlowingAway?1:0,z->mNetGhost?1:0,s.windMotionSamples);
                    }
                    // This frame must contain the bound living UID moving right,
                    // rather than an elapsed-time image from before its wind event.
                    if(s.windMotion&&!s.windFrame)s.windFrame=Capture(a,"flight");
                }
                else
                {
                    if(!Require(a,s.windMotion&&s.windFrame,"Umbrella267 wind UID disappeared before visible motion/frame proof"))return;
                    if(!s.windRetired&&Samples(a)>s.windLastAliveSamples)
                    {
                        s.windRetired=true;
                        NetTrace("SMOKE_UMBRELLA267_WIND_RETIRED role=%s group=1 id=%u initialX=%.9g lastX=%.9g lastTargetX=%.9g motionSnapshot=%u lastAliveSnapshot=%u retiredSnapshot=%u freshFlight=1 sameFactoryUID=1 omittedAfterNewSnapshot=1",Role(a),s.zombies[i],s.initialX[i],s.windLastX,s.windLastTargetX,s.windMotionSamples,s.windLastAliveSamples,Samples(a));
                    }
                }
                passed=s.windRetired;
            }
            if(passed)s.mask|=1<<i;continue;
        }
        switch(i)
        {
        case 0:passed=d&&(d->batchMode&1);break;
        case 1:passed=z&&z->mBodyHealth<s.initialZombieHP[i]&&z->mButteredCounter>0;break;
        case 2:{const auto* garlic=GetNativeZombieGarlicData(z);passed=z&&z->mBodyHealth<s.initialZombieHP[i]&&(z->mRow!=0||(garlic&&FusionGarlicActionActive(*garlic)));break;}
        case 3:passed=s.reinforce;break;
        case 4:passed=s.reflect;break;
        case 5:passed=p&&HasMoney(b,p,25);break;
        case 6:passed=d&&(d->batchMode&4)&&p&&HasMoney(b,p,25);break;
        case 7:
            if(d){if(d->batchBagCount<2000){s.depleted=true;s.minimumEnergy=(std::min)(s.minimumEnergy,int(d->batchBagCount));}if(s.depleted&&int(d->batchBagCount)>s.minimumEnergy)s.recovered=true;}passed=s.recovered;break;
        case 8:passed=z&&!z->IsFlying();break;
        case 9:
            passed=p&&p->mPlantHealth>s.previousHP[i];
            if(passed&&!(s.mask&(1<<i)))NetTrace("SMOKE_UMBRELLA267_REGEN role=%s group=0 kind=931 id=%u before=%d after=%d tick=%d observedPositiveStep=1",Role(a),p->mNetId,s.previousHP[i],p->mPlantHealth,b->mMainCounter);
            break;
        case 10:passed=p&&p->mPlantHealth>s.initialHP[i]&&z&&z->mBodyHealth<s.initialZombieHP[i];break;
        case 11:passed=p&&p->mPlantHealth<s.initialHP[i];break;
        case 12:passed=d&&d->batchShotCount<100&&z&&z->mBodyHealth<s.initialZombieHP[i];break;
        case 13:passed=s.money100;break;
        case 14:passed=d&&d->batchMode==2&&s.smallSun&&s.bigSun;break;
        case 15:
        {
            // Source SummonUpdate copies LilyType1; the actual1+1 recipe is1033.
            auto* child=At(b,3,3,1033);const auto* cd=GetNativePlantData(child);bool oldConsumed=true;Plant* old=nullptr;
            while(b->IteratePlants(old))if(!old->mDead&&old->mNetId==s.lilySun.id){oldConsumed=false;break;}
            PlantsOnLawn cell{};b->GetPlantsOnLawn(3,3,&cell);
            passed=p&&child&&cd&&s.lilySun.id&&child->mIsOnBoard&&child->mPlantHealth>0&&
                child->mNetId!=s.lilySun.id&&child->mNetId!=p->mNetId&&cd->instanceToken&&cd->instanceToken!=s.lilySun.token&&
                cd->firstParent==1&&cd->secondParent==1&&cd->owner==LawnNet::NET_SHARED_OWNER&&!cd->packetId&&!cd->randomOrigin&&
                oldConsumed&&cell.mUnderPlant==p&&cell.mNormalPlant==child;
            if(passed&&!(s.mask&(1<<i)))NetTrace("SMOKE_UMBRELLA267_LILY_CLONE role=%s lily=%u lilyToken=%u oldSun=%u oldSunToken=%u child=%u childToken=%u kind=1033 parents=1,1 owner=shared oldConsumed=1 supportRetained=1 tick=%d",Role(a),p->mNetId,d->instanceToken,s.lilySun.id,s.lilySun.token,child->mNetId,cd->instanceToken,b->mMainCounter);
            break;
        }
        case 16:passed=z&&z->mBodyHealth<s.initialZombieHP[i];break;
        }
        if(passed)s.mask|=1<<i;if(p)s.previousHP[i]=p->mPlantHealth;
    }
    if(!Group()&&elapsed>60&&elapsed<250)Capture(a,"flight");
    const bool terminal=a->mGameScene==SCENE_ZOMBIES_WON;
    if(terminal||!s.diag||GetTickCount()-s.diag>5000)
    {
        s.diag=GetTickCount();Zombie* leftmost=nullptr;Zombie* z=nullptr;
        while(b->IterateZombies(z))if(!z->mDead&&!z->IsDeadOrDying()&&!z->mMindControlled&&(!leftmost||z->mPosX<leftmost->mPosX))leftmost=z;
        unsigned required=0,present=0;for(int i=0;i<int(specs.size());++i)if(RequiredTarget(i)){required|=1U<<i;if(Target(b,i))present|=1U<<i;}
        auto* guard=Group()?nullptr:At(b,0,5,931);
        if(!Group())
        {
            auto* lily=Actor(b,15);const auto* lilyData=GetNativePlantData(lily);
            auto* original=At(b,3,3,1);const auto* originalData=GetNativePlantData(original);
            if(!originalData||original->mNetId!=s.lilySun.id||originalData->instanceToken!=s.lilySun.token)original=nullptr;
            PlantsOnLawn cell{};b->GetPlantsOnLawn(3,3,&cell);
            auto* body=cell.mNormalPlant;const auto* bodyData=GetNativePlantData(body);
            auto* receiver=Target(b,16);auto* protector=At(b,4,3,931);
            NetTrace("SMOKE_UMBRELLA267_LILY_OBSERVE role=%s tick=%d originalUID=%u originalToken=%u originalHP=%d currentBodyUID=%u currentBodyKind=%u lilyUID=%u lilyHP=%d lilyType=%u lilyCycle=%d receiverUID=%u receiverX=%.9g protectorUID=%u protectorHP=%d",
                Role(a),b->mMainCounter,s.lilySun.id,s.lilySun.token,original?original->mPlantHealth:-1,
                body?body->mNetId:0,bodyData?bodyData->kind:kFusionNoPlant,lily?lily->mNetId:0,lily?lily->mPlantHealth:-1,
                lilyData?lilyData->batchBagCount:65535,lily?lily->mLaunchCounter:-1,receiver?receiver->mNetId:0,receiver?receiver->mPosX:0.f,
                protector?protector->mNetId:0,protector?protector->mPlantHealth:-1);
        }
        NetTrace("SMOKE_UMBRELLA267_OBSERVE role=%s group=%d elapsed=%d mask=%05x expected=%05x reflect=%d reinforce=%d energy=%d/%d sun15=%d sun50=%d scene=%d paused=%d tick=%d leftmostUID=%u leftmostRow=%d leftmostX=%.9g leftmostPhase=%d requiredUIDs=%05x presentUIDs=%05x guardUID=%u guardHP=%d",Role(a),Group(),elapsed,s.mask,Expected(),s.reflect?1:0,s.reinforce?1:0,s.depleted?1:0,s.recovered?1:0,s.smallSun?1:0,s.bigSun?1:0,int(a->mGameScene),b->mPaused?1:0,b->mMainCounter,leftmost?leftmost->mNetId:0,leftmost?leftmost->mRow:-1,leftmost?leftmost->mPosX:0.f,leftmost?int(leftmost->mZombiePhase):-1,required,present,guard?guard->mNetId:0,guard?guard->mPlantHealth:0);
    }
    if(terminal&&elapsed<(Group()?700:10000))
    {char why[180];sprintf_s(why,"Umbrella267 terminal before grouped deadline role=%s group=%d elapsed=%d tick=%d scene=%d mask=%05x triggerUID=NotRecorded",Role(a),Group(),elapsed,b->mMainCounter,int(a->mGameScene),s.mask);a->CoopSmokeFail(why);}
}

}
namespace LawnNet
{
bool NetSmokeUpdateFusionUmbrella267(LawnApp* a)
{
    const auto* enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_UMBRELLA267"),*native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
    if(!a||!enabled||std::strcmp(enabled,"1")||!native||std::strcmp(native,"1")||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return false;
    if(a->mCoopSmokeResultWritten)return true;if(!s.since)s.since=GetTickCount();
    if(GetTickCount()-s.since>200000){char why[120];sprintf_s(why,"Umbrella267 deadline stage=%d mask=%05x expected=fffff",s.stage,s.mask);a->CoopSmokeFail(why);return true;}
    if(!a->mNetManager||!a->mNetManager->IsActive())return true;a->mCoopSmokePeerReady=true;
    if(s.stage==0)
    {
        if(a->NetIsHost()&&!s.configured){a->mNetManager->SetDifficulty(1);s.configured=true;}
        if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
        if(a->NetIsClient()){Advance(a,1);Send(a,"U267_BEGIN");}
        else if(Has(a,1,"U267_BEGIN")){Advance(a,1);std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;
    }
    if(!a->mBoard||!IsNativeFusionBoard(a->mBoard)){if(s.stage==1&&a->NetIsClient())Send(a,"U267_BEGIN");return true;}
    auto* b=a->mBoard;const auto* ctx=GetNativeContext(b);if(!s.epoch)s.epoch=ctx->epoch;
    if(!Require(a,ctx->epoch==s.epoch&&ctx->validation&&ctx->visuals&&!a->mFusionRuntime&&b->GetColumnCount()==10,"Umbrella267 requires native validation world"))return true;
    const auto& bank=ctx->selection.State();auto* chooser=a->mSeedChooserScreen;
    if(s.stage==1)
    {
        if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||chooser->mSeedsInFlight||!b->mCutScene->mSeedChoosing)return true;
        int own=0;for(const auto& p:bank.packets)if(p.owner==Slot(a))++own;
        if(own<s.waiting)return true;
        if(own<5)
        {
            const int wanted=5*Slot(a)+own;int dense=-1;for(size_t i=0;i<ctx->eligible.size();++i)if(ctx->eligible[i].kind==wanted&&!ctx->eligible[i].isExtra)dense=int(i);
            if(!Require(a,dense>=0,"Umbrella267 native chooser source base missing"))return true;if(Choice(a,chooser,dense))s.waiting=own+1;return true;
        }
        if(bank.packets.size()==10&&Capture(a,"chooser"))Advance(a,2);
    }
    else if(s.stage==2)
    {
        if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}
        auto* button=chooser?chooser->mStartButton:nullptr;if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;
        const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
        if(!button->IsMouseOver()){SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"Umbrella267 Ready hover failed");}
        else if(Click(a,x,y))Advance(a,3);
    }
    else if(s.stage==3)
    {
        if(bank.phase==FusionBankPhase::Intro){if(!s.introFirst)s.introFirst=GetTickCount();s.introLast=GetTickCount();++s.introChecks;return true;}
        if(bank.phase!=FusionBankPhase::Playing||chooser||a->mGameScene!=SCENE_PLAYING)return true;
        if(!Require(a,s.introChecks>=10&&s.introLast-s.introFirst>=1000,"Umbrella267 full native intro was skipped"))return true;
        if(s.entry<0){s.entry=a->mAppCounter;return true;}if(a->mAppCounter<=s.entry)return true;
        if(a->NetIsHost()&&!s.setup){if(!Setup(a))return true;s.pauseRequested=Escape(a);}
        if(b->mPaused)Advance(a,4);
    }
    else if(s.stage==4)
    {
        if(!b->mPaused){if(s.baseline){s.pauseRequested=false;s.start=b->mMainCounter;Advance(a,5);}return true;}
        if(!Baseline(a))return true;if(!s.pauseSamples){s.pauseSamples=Samples(a);return true;}if(Samples(a)-s.pauseSamples<2)return true;
        if(!Capture(a,"before")||!Barrier(a,"BASELINE"))return true;s.baseline=true;
        if(a->NetIsHost()&&!s.resumeRequested)s.resumeRequested=Escape(a);
    }
    else if(s.stage==5)
    {
        Observe(a);if(a->mCoopSmokeResultWritten||b->mMainCounter-s.start<(Group()?700:10000))return true;
        if(!Require(a,s.mask==Expected(),"Umbrella267 grouped effects/expiry acceptance incomplete"))return true;
        if(!Barrier(a,"EFFECTS"))return true;
        if(a->NetIsHost()&&!s.pauseRequested)s.pauseRequested=Escape(a);
        if(b->mPaused){s.pauseSamples=Samples(a);Advance(a,6);}
    }
    else if(s.stage==6)
    {
        if(!b->mPaused||Samples(a)-s.pauseSamples<2||!Capture(a,"complete")||!Barrier(a,"FINAL"))return true;
        NetTrace("SMOKE_UMBRELLA267_CASE role=%s group=%d epoch=%u kinds=%zu mask=%05x postSetupActorWrites=0",Role(a),Group(),s.epoch,specs.size(),s.mask);
        NetTrace("SMOKE_UMBRELLA267_COMPLETE role=%s group=%d epoch=%u kinds=%zu mask=%05x sourceAcquisition=NotExecuted sourceAI=NotClaimed actualClassicBalloonControl=1 postSetupActorWrites=0 snapshots=%u",Role(a),Group(),s.epoch,specs.size(),s.mask,Samples(a));
        a->CoopSmokeWriteResult(true,nullptr);
    }
    return true;
}
}
