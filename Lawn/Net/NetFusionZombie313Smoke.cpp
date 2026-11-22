#include "NetFusionZombie313Smoke.h"
#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetCommon.h"
#include "../Zombie.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Fusion/FusionNativeVisual.h"
#include "../Fusion/FusionNativePlacement.h"
#include "../Fusion/FusionRecipe.h"
#include "../Fusion/FusionNativeProjection313.h"
#include "../Fusion/FusionNativeZombie313Dispatch.h"
#include "../Fusion/FusionNativeSourceDamage315.h"
#include "../Fusion/FusionNativeExtendedZombie313Bridge.h"
#include "../Fusion/FusionNativeWaterZombie313Bridge.h"
#include "../Fusion/FusionNativeDayZombie273.h"
#include "../Fusion/FusionNativeJackbox309Bridge.h"
#include "../Fusion/FusionNative313SnapshotBridge.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../Board.h"
#include "../Cutscene.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativeSourceClock313.h"
#include "../Fusion/FusionNativeZombieVisual.h"
#include "../Fusion/FusionNativeZombiePreflight280.h"
#include "../Fusion/FusionNativeZombies.h"
#include "../Fusion/FusionNativeWire.h"
#include "../../SexyAppFramework/DDInterface.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include "../../SexyAppFramework/ScrollbarWidget.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
namespace
{
using namespace LawnFusion;using LawnNet::NetTrace;
constexpr std::array<uint16_t,25> ground={24,30,101,102,103,28,31,32,33,34,45,203,223,25,26,29,35,36,37,38,39,40,43,212,213};
constexpr std::array<uint16_t,5> water={16,17,19,20,27};
bool Behavior();int BehaviorGroup329();
struct State
{
    int stage=0,waiting=0,resource=0,resourceFailures=0;uint32_t start=0,since=0,chat=0,wheel=0,epoch=0,samples=0,introStart=0,introEnd=0;
    int introSamples=0;bool logged=false,capacity=false,resourceSummary=false;
    std::map<std::string,int> draws;std::set<std::string> frames;
}s;
bool Enabled(const char* name){const char* v=std::getenv(name);return v&&!std::strcmp(v,"1");}
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool okay,const char* why){if(!okay)a->CoopSmokeFail(why);return okay;}
std::span<const uint16_t> Kinds(int level){return level>=21?std::span<const uint16_t>(water):std::span<const uint16_t>(ground);}
const char* Biome(int level){return level==1?"day":level==11?"night":level==21?"pool":"fog";}
void Advance(LawnApp* a,int next){s.stage=next;s.since=GetTickCount();NetTrace("SMOKE_ZOMBIE313_STAGE role=%s biome=%s stage=%d epoch=%u",Role(a),Biome(LawnNet::NetSmokeFusionZombie313Level325(a)),next,s.epoch);}
bool Has(LawnApp* a,int slot,const std::string& tag){for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==slot&&line.mText==tag)return true;return false;}
bool Send(LawnApp* a,const std::string& tag)
{if(!Require(a,tag.size()<=LawnNet::NET_MAX_CHAT_CHARS,"Zombie313 chat proof exceeds native limit"))return false;if(Has(a,Slot(a),tag))return true;if(s.chat&&GetTickCount()-s.chat<800)return false;s.chat=GetTickCount();a->mNetManager->SendChat(tag);return Has(a,Slot(a),tag);}
bool Barrier(LawnApp* a,const char* tag)
{const auto text="Z313_"+std::to_string(s.epoch)+"_"+tag;Send(a,text);return Has(a,0,text)&&Has(a,1,text);}
bool Pointer(LawnApp* a,float x,float y,bool click=true)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    if(!Require(a,SDL_PushEvent(&m),"Zombie313 owned SDL motion failed"))return false;if(!click)return true;
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;
    return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Zombie313 owned SDL click failed");
}
bool Capture(LawnApp* a,const char* suffix)
{
    const std::string name="fusion_zombie313_"+std::string(Biome(LawnNet::NetSmokeFusionZombie313Level325(a)))+"_"+(Behavior()?"g"+std::to_string(BehaviorGroup329())+"_":"")+suffix;
    if(s.frames.contains(name))return true;if(!s.draws.contains(name))s.draws[name]=a->mDrawCount;
    const int requested=s.draws[name]+2;if(!a->CoopSmokeEnsureFreshFrameAfterDraw(requested))return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Zombie313 fresh owned resource frame failed"))return false;
    s.frames.insert(name);NetTrace("SMOKE_ZOMBIE313_RENDER role=%s frame=%s requestedDraw=%d capturedDraw=%d resourceOnly=%d actorRenderAcceptance=0",Role(a),name.c_str(),requested,a->mCoopSmokeFrameDrawCount,Behavior()?0:1);return true;
}
bool World(LawnApp* a,int level)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);if(!b||!c)return false;
    const int bg=level==1?0:level==11?1:level==21?2:3;
    if(!Require(a,IsNativeSourceWorld313(b)&&c->validation&&!c->adventure.enabled&&!a->mFusionRuntime&&
        a->IsFusionValidationFixture()&&b->mLevel==level&&int(b->mBackground)==bg&&b->GetColumnCount()==10&&
        GetNativeFusionRowCount(b)==(level>=21?6:5),"Zombie313 requires actual private native biome; no source campaign/world projection"))return false;
    if(!s.epoch)s.epoch=c->epoch;
    unsigned zombies=0,plants=0,projectiles=0;Zombie* z=nullptr;Plant* p=nullptr;Projectile* shot=nullptr;
    while(b->mZombies.IterateNext(z))if(!z->mDead)++zombies;
    while(b->mPlants.IterateNext(p))if(!p->mDead)++plants;
    while(b->mProjectiles.IterateNext(shot))if(!shot->mDead)++projectiles;
    const bool emptyRequired=s.stage>=4&&(!Behavior()||s.stage<10);
    const bool epochOkay=c->epoch==s.epoch,empty=!zombies&&!plants&&!projectiles;
    if(!epochOkay||(emptyRequired&&!empty))NetTrace("SMOKE_ZOMBIE313_WORLD role=%s stage=%d expectedEpoch=%u actualEpoch=%u scene=%d bankPhase=%u emptyRequired=%d liveZ=%u livePlants=%u liveP=%u poolZ=%u poolPlants=%u poolP=%u snapshots=%u",Role(a),s.stage,s.epoch,c->epoch,int(a->mGameScene),unsigned(c->selection.State().phase),emptyRequired,zombies,plants,projectiles,b->mZombies.mSize,b->mPlants.mSize,b->mProjectiles.mSize,Samples(a));
    if(!Require(a,epochOkay,"Zombie313 native epoch changed unexpectedly"))return false;
    // Real chooser/Intro preview actors belong to the native cutscene. Never
    // remove them manually or reject them before genuine cleanup/Playing.
    if(emptyRequired&&Samples(a)-s.samples<2)return false;
    return Require(a,!emptyRequired||empty,"Zombie313 resource phase retained live actors after genuine intro cleanup");
}
bool Choose(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);auto* q=a->mSeedChooserScreen;
    if(!q||!q->mNativeFusionChooser||!q->mMouseVisible||q->mSeedsInFlight||!b->mCutScene||!b->mCutScene->mSeedChoosing)return false;
    const auto& bank=c->selection.State();const int slot=Slot(a),wx=a->mDDInterface->mWideScreenOffsetX,wy=a->mDDInterface->mWideScreenOffsetY;
    const auto& target=b->mNetIntroTargets;
    if((a->NetIsClient()&&(!target.mValid||target.mChooserX!=0||target.mChooserY!=0))||q->mX!=wx||q->mY!=wy||
        q->mNativeFusionEpoch!=bank.epoch||q->mNativeFusionRevision!=bank.revision)return false;
    if(!Require(a,bank.phase==FusionBankPhase::Choosing&&bank.activeMask==3&&bank.targetCount==10&&
        bank.quotas[0]==5&&bank.quotas[1]==5&&c->eligible.size()>=10,"Zombie313 real native bank quotas differ"))return false;
    std::vector<FusionBankEligibleCard> wanted;const size_t first=slot?bank.quotas[0]:0;
    for(size_t i=first;i<first+bank.quotas[slot];++i)wanted.push_back(c->eligible[i]);
    int own=0;for(const auto& p:bank.packets)if(p.owner==slot)
    {if(!Require(a,own<int(wanted.size())&&p.kind==wanted[size_t(own)].kind&&p.isExtra==wanted[size_t(own)].isExtra,"Zombie313 SDL picked a different native card identity"))return false;++own;}
    if(own<s.waiting)return false;
    if(own<bank.quotas[slot])
    {
        int index=-1;for(size_t i=0;i<c->eligible.size();++i)if(c->eligible[i].kind==wanted[size_t(own)].kind&&c->eligible[i].isExtra==wanted[size_t(own)].isExtra)index=int(i);
        if(!Require(a,index>=0,"Zombie313 native eligible card vanished"))return false;
        int x=0,y=0;q->GetSeedPositionInChooser(index,x,y);const float cy=float(y+35-int(q->mScrollbar->mScrollValue));
        if(cy<158||cy>493)
        {if(s.wheel&&GetTickCount()-s.wheel<250)return false;s.wheel=GetTickCount();if(!Pointer(a,float(q->mX+300),float(q->mY+300),false))return false;
            SDL_Event e{};e.type=SDL_EVENT_MOUSE_WHEEL;e.wheel.windowID=SDL_GetWindowID(a->mSDLWindow);e.wheel.y=cy<158?1.f:-1.f;Require(a,SDL_PushEvent(&e),"Zombie313 owned chooser scroll failed");return false;}
        if(std::abs(q->mScrollAmount)>.05f)return false;
        const int sx=q->mX+x+25,sy=q->mY+int(cy);int lx=0,ly=0;
        if(a->mWidgetManager->GetWidgetAt(sx,sy,&lx,&ly)!=q||!Sexy::Rect(x,y-int(q->mScrollbar->mScrollValue),50,70).Contains(lx,ly))return false;
        if(a->mWidgetManager->mLastMouseX!=sx||a->mWidgetManager->mLastMouseY!=sy){Pointer(a,float(sx),float(sy),false);return false;}
        NetTrace("SMOKE_ZOMBIE313_PICK role=%s kind=%u extra=%d nativeIndex=%d revision=%u realWidgetHit=1",Role(a),wanted[size_t(own)].kind,wanted[size_t(own)].isExtra,index,bank.revision);
        if(Pointer(a,float(sx),float(sy)))s.waiting=own+1;return false;
    }
    return bank.packets.size()==bank.targetCount&&Capture(a,"chooser")&&Barrier(a,"CHOOSER");
}
bool Resources(LawnApp* a,int level)
{
    auto* b=a->mBoard;const auto kinds=Kinds(level);
    if(s.resource<int(kinds.size()))
    {
        const uint16_t kind=kinds[size_t(s.resource)];std::string error;const int carrier=GetNativeSourceZombieCarrier(kind);
        const auto* profile=FindFusionZombieProfile(kind);
        const bool model=EnsureNativeSourceZombieVisuals(b,kind,&error);
        const bool root=model&&CanQueryNativeSourceZombieRoot273(b,kind);
        const bool closure=root&&profile&&carrier>=0&&EnsureNativeSourceZombieClosure280(b,std::span(&kind,1),&error);
        NetTrace("SMOKE_ZOMBIE313_RESOURCE role=%s biome=%s index=%d kind=%u profile=%d carrier=%d model=%d root=%d closure=%d actorAllocation=0 runtimeAI=NotExecuted error=%s",Role(a),Biome(level),s.resource,kind,profile?1:0,carrier,model,root,closure,error.c_str());
        if(!model||!root||!closure)++s.resourceFailures;
        ++s.resource;return false;
    }
    // Probe every root even after a safe resource refusal. Both roles exchange
    // their observed failure counts before either exits, so one run reports the
    // full batch instead of restarting native peers for each bad model.
    const auto prefix="Z313_"+std::to_string(s.epoch)+"_RESCOUNT_";
    Send(a,prefix+std::to_string(s.resourceFailures));int failures[2]={-1,-1};
    for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot>=0&&line.mSlot<2&&line.mText.starts_with(prefix))
    {
        const auto count=line.mText.substr(prefix.size());
        if(!count.empty()&&count.size()<=3&&count.find_first_not_of("0123456789")==std::string::npos)
        {const int n=std::atoi(count.c_str());if(n>=0&&n<=int(kinds.size()))failures[line.mSlot]=n;}
    }
    if(failures[0]<0||failures[1]<0||!Barrier(a,"RESCHECKED"))return false;
    if(failures[0]||failures[1])
    {
        if(!s.resourceSummary){s.resourceSummary=true;NetTrace("SMOKE_ZOMBIE313_RESOURCE_BATCH_FAILED role=%s biome=%s probed=%zu localFailed=%d hostFailed=%d clientFailed=%d hostCapacity=NotExecuted actorAllocation=0 noFallback=1",Role(a),Biome(level),kinds.size(),s.resourceFailures,failures[0],failures[1]);}
        return Require(a,false,"Zombie313 full resource batch refused; all per-kind results recorded on both peers");
    }
    if(!s.capacity)
    {
        // This API is HOST ONLY. Ghost peers prove resources above and receive
        // native snapshots; they do not claim host allocator capacity.
        std::string error;const bool ready=!a->NetIsHost()||CanAllocateNativeSourceZombieBatch280(b,kinds,&error);
        NetTrace("SMOKE_ZOMBIE313_CAPACITY role=%s biome=%s roots=%zu hostOnly=1 executed=%d ready=%d noDeadCleanupCredit=1 error=%s",Role(a),Biome(level),kinds.size(),a->NetIsHost()?1:0,ready,error.c_str());
        if(!Require(a,ready,"Zombie313 whole actual family batch capacity refused"))return false;s.capacity=true;
    }
    return Capture(a,"resources")&&Barrier(a,"RESOURCES");
}
bool Behavior(){return Enabled("PVZ_COOP_SMOKE_FUSION_ZOMBIE313_BEHAVIOR");}
struct Actor329
{
    uint16_t id=0,kind=0;uint32_t token=0,local=0;
    float x=0,y=0,time=0;int frame=0,loops=0;bool bound=false,moved=false,animated=false;
};
struct BehaviorState329
{
    int group=0;bool setup=false,pause=false,resume=false,startedNext=false,probed=false,caseLogged=false;
    uint32_t sample=0,tick=0,diagnostic=0;std::array<Actor329,5> roots{};
    std::set<uint64_t> initial;std::set<std::string> proofs;std::string bind;bool child=false,childMoved=false;
    Actor329 imp;float probeHealth=0,probeShadowX=0,probeShadowY=0;
}h;
int BehaviorGroup329(){return h.group;}
int Groups329(int level){return level>=21?1:5;}
std::span<const uint16_t> Group329(int level)
{return level>=21?std::span<const uint16_t>(water):std::span<const uint16_t>(ground).subspan(size_t(h.group)*5,5);}
uint64_t Key329(uint16_t id,uint32_t token){return (uint64_t(token)<<16)|id;}
Zombie* Resolve329(Board* b,const Actor329& ref)
{Zombie* z=nullptr;while(b->mZombies.IterateNext(z))if(!z->mDead&&z->mNetId==ref.id)if(const auto* d=GetNativeSourceZombieData(z);d&&d->instanceToken==ref.token)return z;return nullptr;}
bool Escape329(LawnApp* a)
{SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Zombie329 owned SDL pause key failed");}
std::string Tag329(const char* name){return "Z329_"+std::to_string(s.epoch)+"_"+std::to_string(h.group)+"_"+name;}
bool Barrier329(LawnApp* a,const char* name)
{const auto tag=Tag329(name);Send(a,tag);return Has(a,0,tag)&&Has(a,1,tag);}
bool Paused329(LawnApp* a)
{
    if(a->NetIsHost()&&!h.pause){h.pause=Escape329(a);return false;}
    if(!a->mBoard->mPaused)return false;
    if(!h.sample){h.sample=Samples(a);return false;}
    return Samples(a)-h.sample>=3;
}
bool Resumed329(LawnApp* a)
{
    if(a->NetIsHost()&&!h.resume)
    {h.resume=Escape329(a);return false;}
    if(a->mBoard->mPaused||a->GetDialogCount())return false;h.pause=false;h.sample=0;return true;
}
bool Target329(LawnApp* a,uint16_t kind,int col,int row)
{
    NativePlantData data;data.kind=kind;data.owner=LawnNet::NET_SHARED_OWNER;
    for(size_t i=0;i<GetFusionRecipeCount();++i){const auto& recipe=GetFusionRecipes()[i];if(recipe.result==kind){data.firstParent=recipe.existing;data.secondParent=recipe.used;break;}}
    auto* p=TrySpawnNativeFusionPlant(a->mBoard,col,row,data);const auto* d=GetNativePlantData(p);
    if(!Require(a,p&&!p->mDead&&d&&d->kind==kind&&p->mNetId&&d->instanceToken,"Zombie329 genuine passive target factory refused"))return false;
    NetTrace("SMOKE_ZOMBIE329_TARGET role=host group=%d kind=%u id=%u token=%u row=%d col=%d actualHP=%d setupFactory=1 acquisitionNotExecuted=1",h.group,kind,p->mNetId,d->instanceToken,row,col,p->mPlantHealth);return true;
}
bool Setup329(LawnApp* a,int level)
{
    if(!a->NetIsHost()||h.setup)return true;auto* b=a->mBoard;std::string error;
    if(Samples(a)-s.samples<2)return false;
    Zombie* preview=nullptr;while(b->mZombies.IterateNext(preview))if(!Require(a,preview->mDead,"Zombie329 preview survived genuine intro cleanup"))return false;
    if(!Require(a,!b->mPaused,"Zombie329 fresh epoch unexpectedly paused"))return false;
    if(!Require(a,EnsureNativePlantVisuals(b,1027,&error),"Zombie329 genuine TallNut1027 resource missing"))return false;
    for(int row=0;row<GetNativeFusionRowCount(b);++row)
    {if(b->mPlantRow[row]==PLANTROW_POOL&&!Target329(a,12,2,row))return false;if(!Target329(a,1027,2,row))return false;}
    const auto kinds=Group329(level);
    if(!Require(a,CanAllocateNativeSourceZombieBatch280(b,kinds,&error),"Zombie329 whole actual birth group allocation refused")){NetTrace("SMOKE_ZOMBIE329_BIRTH_REFUSED group=%d error=%s",h.group,error.c_str());return false;}
    for(size_t i=0;i<kinds.size();++i)
    {
        NativeSourceZombieSpawnRequest request;request.kind=kinds[i];request.row=level>=21?std::array<int,5>{0,2,2,4,3}[i]:int(i);request.fromWave=0;
        request.positioned=true;request.shadowX=kinds[i]==35?740.f:kinds[i]==26?500.f:level>=21&&i==2?720.f:640.f;
        auto* z=SpawnNativeSourceZombie(b,request);const auto* d=GetNativeSourceZombieData(z);
        if(!Require(a,z&&!z->mDead&&d&&d->kind==kinds[i]&&z->mNetId&&d->instanceToken,"Zombie329 actual typed root factory refused"))return false;
        h.roots[i]={z->mNetId,d->kind,d->instanceToken,b->mZombies.DataArrayGetID(z)};
        h.initial.insert(Key329(z->mNetId,d->instanceToken));
        NetTrace("SMOKE_ZOMBIE329_BIRTH role=host group=%d kind=%u id=%u token=%u localUID=%u row=%d sourceHealth=%.6g max=%d bodyR=%u headR=%u positionedBirth=1 postBirthWrites=0",h.group,d->kind,z->mNetId,d->instanceToken,h.roots[i].local,z->mRow,d->health,z->mBodyMaxHealth,unsigned(z->mBodyReanimID),unsigned(z->mSpecialHeadReanimID));
    }
    LawnNet::NetBuffer identities;for(const auto& ref:h.roots){identities.WriteU16(ref.id);identities.WriteU32(ref.token);}
    static constexpr char hex[]="0123456789abcdef";h.bind=Tag329("BIND_");for(size_t i=0;i<identities.Size();++i){const auto value=identities.Data()[i];h.bind.push_back(hex[value>>4]);h.bind.push_back(hex[value&15]);}
    h.setup=true;h.pause=Escape329(a);return true;
}
bool Bind329(LawnApp* a,int level)
{
    if(a->NetIsHost())Send(a,h.bind);
    else if(h.bind.empty())
    {
        const auto prefix=Tag329("BIND_");
        for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==0&&line.mText.starts_with(prefix)&&line.mText.size()==prefix.size()+60)h.bind=line.mText;
        if(h.bind.empty())return false;LawnNet::NetBuffer bytes;
        const auto nibble=[](char c)->int{return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};
        for(size_t i=prefix.size();i<h.bind.size();i+=2){int hi=nibble(h.bind[i]),lo=nibble(h.bind[i+1]);if(!Require(a,hi>=0&&lo>=0,"Zombie329 malformed host identity proof"))return false;bytes.WriteU8(uint8_t(hi*16+lo));}
        const auto kinds=Group329(level);for(size_t i=0;i<5;++i){h.roots[i].id=bytes.ReadU16Or();h.roots[i].token=bytes.ReadU32Or();h.roots[i].kind=kinds[i];h.initial.insert(Key329(h.roots[i].id,h.roots[i].token));}
    }
    for(auto& ref:h.roots)
    {auto* z=Resolve329(a->mBoard,ref);if(!z)return false;const auto* d=GetNativeSourceZombieData(z);auto* body=a->ReanimationTryToGet(z->mBodyReanimID);
        if(!Require(a,d&&d->kind==ref.kind&&body&&!body->mDead&&body->mFrameCount>0&&z->mZombieType==GetNativeSourceZombieCarrier(ref.kind),"Zombie329 actual full-generation carrier/body differs"))return false;
        ref.local=a->mBoard->mZombies.DataArrayGetID(z);
        if(!ref.bound){if(!Require(a,GetNativeSourceZombie269NodePoint(z,"Shadow",ref.x,ref.y),"Zombie329 bound actual source Shadow unavailable"))return false;
            ref.time=body->mAnimTime;ref.frame=body->mFrameStart;ref.loops=body->mLoopCount;ref.bound=true;
            NetTrace("SMOKE_ZOMBIE329_BOUND role=%s group=%d kind=%u id=%u token=%u localUID=%u actualBody=%d+%d sourceAPIProbe=0",Role(a),h.group,ref.kind,ref.id,ref.token,ref.local,body->mFrameStart,body->mFrameCount);}
    }
    return true;
}
struct Writer329
{LawnNet::NetBuffer& b;bool valid=true;void U8(uint8_t& v){b.WriteU8(v);}void U16(uint16_t& v){b.WriteU16(v);}void U32(uint32_t& v){b.WriteU32(v);}void I32(int& v){b.WriteI32(v);}void F32(float& v){b.WriteF32(v);}void Bool(bool& v){b.WriteU8(v?1:0);}};
#include "../Fusion/FusionNative313Fields.inc"
bool RawClock329(LawnApp* a,ReanimationID id,LawnNet::NetBuffer& bytes)
{
    LawnNet::NetZombieAnimState v;const auto* r=a->ReanimationTryToGet(id);
    if(r){if(r->mFrameStart<0||r->mFrameStart>65535||r->mFrameCount<=0||r->mFrameCount>65535||r->mLoopCount<0)return false;
        v.mPresent=true;v.mDead=r->mDead;v.mFrameStart=uint16_t(r->mFrameStart);v.mFrameCount=uint16_t(r->mFrameCount);v.mAnimTime=r->mAnimTime;v.mAnimRate=r->mAnimRate;v.mLoopType=uint8_t(r->mLoopType);v.mLoopCount=uint8_t((std::min)(r->mLoopCount,255));}
    return LawnNet::NetEncodeZombieAnimState(v,bytes);
}
bool Canonical329(LawnApp* a,const char* stage)
{
    LawnNet::NetBuffer bytes;Writer329 writer{bytes};
    std::vector<Actor329> actual(h.roots.begin(),h.roots.end());if(h.child)actual.push_back(h.imp);
    for(const auto& ref:actual)
    {
        bytes.WriteU16(ref.id);bytes.WriteU32(ref.token);bytes.WriteU16(ref.kind);auto* z=Resolve329(a->mBoard,ref);bytes.WriteU8(z?1:0);if(!z)continue;
        const auto* d=GetNativeSourceZombieData(z);bytes.WriteF32(d->health);bytes.WriteF32(d->originSpeed);bytes.WriteU8(d->beforeDying);bytes.WriteU8(z->mMindControlled);
        bytes.WriteI32(z->mBodyHealth);bytes.WriteI32(z->mHelmHealth);bytes.WriteI32(z->mShieldHealth);bytes.WriteI32(z->mChilledCounter);bytes.WriteI32(z->mIceTrapCounter);
        bytes.WriteU8(uint8_t(d->targetEntity));bytes.WriteU16(d->targetId);bytes.WriteU32(d->targetGeneration);
        if(!Require(a,RawClock329(a,z->mBodyReanimID,bytes)&&RawClock329(a,z->mSpecialHeadReanimID,bytes),"Zombie329 actual body/head clock cannot be canonically encoded"))return false;
        if(IsNativeZombie313(z))
        {Native313Snapshot view;std::string error;if(!Require(a,CaptureNative313(z,view,&error),"Zombie329 genuine current family/FNS capture refused")){NetTrace("SMOKE_ZOMBIE329_CAPTURE_REFUSED role=%s kind=%u error=%s",Role(a),ref.kind,error.c_str());return false;}
            if(view.graph)Fields(writer,*view.graph);if(view.water)Fields(writer,*view.water);if(view.heavy)Fields(writer,*view.heavy);if(view.extended)Fields(writer,*view.extended);}
        else if(const auto* day=GetNativeDayZombie273(z)){bytes.WriteU8(day->status);bytes.WriteU8(day->armorStage);bytes.WriteU8(day->shadowEnabled);bytes.WriteU8(day->shootEvent);}
        else if(const auto* box=GetNativeJackbox309(z))
        {bytes.WriteU8(uint8_t(box->phase));bytes.WriteU8(uint8_t(box->outgoing));bytes.WriteU8(box->boxPresent);bytes.WriteU8(box->popTriggered);bytes.WriteU8(box->audioPlaying);bytes.WriteF32(box->countdown);bytes.WriteF32(box->originalCountdown);bytes.WriteF32(box->outgoingSeconds);bytes.WriteF32(box->transitionElapsed);bytes.WriteF32(box->transitionTotal);}
    }
    if(!Require(a,writer.valid,"Zombie329 exact production family field codec refused"))return false;
    char hash[16];sprintf_s(hash,"%08x",LawnNet::NetFnv1a32(bytes.Data(),bytes.Size()));const auto tag=Tag329(stage)+"_"+hash;
    if(h.proofs.insert(tag).second)NetTrace("SMOKE_ZOMBIE329_CANONICAL role=%s group=%d stage=%s digest=%s bytes=%zu actualNativeCapture=1 codecFields=production noPrivateCursors=1",Role(a),h.group,stage,hash,bytes.Size());
    Send(a,tag);return Has(a,0,tag)&&Has(a,1,tag);
}
bool Observe329(LawnApp* a,bool movement)
{
    bool ready=true;auto* b=a->mBoard;
    for(auto& ref:h.roots)
    {
        auto* z=Resolve329(b,ref);if(!Require(a,z!=nullptr,"Zombie329 root retired before bounded live/movement proof"))return false;
        float rx=0,ry=0,sx=0,sy=0,gx=0,gy=0;auto* body=a->ReanimationTryToGet(z->mBodyReanimID);
        if(!Require(a,body&&!body->mDead&&GetNativeSourceZombie269NodePoint(z,"",rx,ry)&&GetNativeSourceZombie269NodePoint(z,"Shadow",sx,sy)&&NativeSourceGroundAnchor313(z,gx,gy),"Zombie329 actual source model/body/ground query failed"))return false;
        ref.moved|=std::abs(sx-ref.x)>.05f||std::abs(sy-ref.y)>.05f;
        ref.animated|=body->mFrameStart!=ref.frame||body->mLoopCount!=ref.loops||std::abs(body->mAnimTime-ref.time)>.0001f;
        const float ox=float(b->mX+a->mDDInterface->mWideScreenOffsetX),oy=float(b->mY+a->mDDInterface->mWideScreenOffsetY);
        if(!Require(a,std::isfinite(rx+ry+sx+sy+gx+gy)&&sx+ox>0&&sx+ox<a->mWidth&&gy+oy>=0&&gy+oy<a->mHeight,"Zombie329 genuine source/ground anchor escaped captured viewport"))return false;
        if(movement&&body->mAnimRate>0&&!ref.animated)ready=false;
        if(movement&&FusionWaterZombie313Kind(ref.kind)&&!ref.moved)ready=false;
    }
    return ready;
}
void Report329(LawnApp* a,const char* phase)
{
    for(const auto& ref:h.roots){auto* z=Resolve329(a->mBoard,ref);float rx=0,ry=0,sx=0,sy=0;if(z){GetNativeSourceZombie269NodePoint(z,"",rx,ry);GetNativeSourceZombie269NodePoint(z,"Shadow",sx,sy);}const auto* d=GetNativeSourceZombieData(z);
        NetTrace("SMOKE_ZOMBIE329_OBSERVED role=%s group=%d phase=%s kind=%u id=%u token=%u alive=%d moved=%d animated=%d bodyHealth=%.6g status=%d root=(%.3f,%.3f) shadow=(%.3f,%.3f) actualRenderFrame=CapturePending acceptedFullAI=0",Role(a),h.group,phase,ref.kind,ref.id,ref.token,z?1:0,ref.moved,ref.animated,d?d->health:0,z?GetNativeFusionZombieStatus(z):-1,rx,ry,sx,sy);}
}
Actor329* Find329(uint16_t kind){for(auto& r:h.roots)if(r.kind==kind)return &r;return nullptr;}
bool Probe329(LawnApp* a)
{
    if(!a->NetIsHost()||h.probed)return true;auto* b=a->mBoard;
    if(auto* r=Find329(32))
    {auto* z=Resolve329(b,*r);if(!Require(a,z&&GetNativeSourceZombie269NodePoint(z,"Shadow",h.probeShadowX,h.probeShadowY)&&ApplyNativeSourceMindControl313(z,0),"Zombie329 source32 genuine MC API refused"))return false;float x=0,y=0;
        if(!Require(a,z->mMindControlled&&GetNativeSourceZombie269NodePoint(z,"Shadow",x,y)&&std::abs(x-h.probeShadowX)<.01f&&std::abs(y-h.probeShadowY)<.01f,"Zombie329 source32 MC did not convert/preserve actual Shadow"))return false;}
    if(auto* r=Find329(16))
    {auto* z=Resolve329(b,*r);const auto* d=GetNativeSourceZombieData(z);if(!Require(a,z&&d,"Zombie329 source16 probe identity absent"))return false;h.probeHealth=d->health;
        if(!Require(a,NativeZombie313ApplyCold(z,false,200)&&NativeZombie313ApplyCold(z,true,200)&&!z->mChilledCounter&&!z->mIceTrapCounter&&NativeZombie313Caltrop(z),"Zombie329 source16 no-op Cold/Freeze or genuine Caltrop callback differs"))return false;
        const auto* v=GetNativeWaterZombie313State(z);d=GetNativeSourceZombieData(z);
        if(!Require(a,v&&d&&d->beforeDying&&!v->colliderEnabled&&v->driverStopAge>=0&&d->health==h.probeHealth,"Zombie329 Driver Caltrop changed health or failed genuine stop/invoke state"))return false;}
    if(auto* r=Find329(17))
    {auto* z=Resolve329(b,*r);const auto* d=GetNativeSourceZombieData(z);if(!Require(a,z&&d,"Zombie329 source17 probe identity absent"))return false;const float before=d->health;
        if(!Require(a,ApplyNativeSourceTypedDamage313(z,4,1,true),"Zombie329 source17 genuine typed one-damage API refused"))return false;z=Resolve329(b,*r);d=GetNativeSourceZombieData(z);
        if(!Require(a,z&&d&&d->health==before-1,"Zombie329 source17 one-damage before/after differs"))return false;
        const int damage=int(std::ceil(z->mBodyMaxHealth*.75f));
        if(!Require(a,ApplyNativeSourceTypedDamage313(z,4,damage,true),"Zombie329 source17 source head-loss damage refused"))return false;}
    if(auto* r=Find329(35))
    {auto* z=Resolve329(b,*r);const auto* d=GetNativeSourceZombieData(z);if(!Require(a,z&&d,"Zombie329 source35 probe identity absent"))return false;
        const int damage=int(std::ceil(d->health-float(z->mBodyMaxHealth)*.5f))+1;
        if(!Require(a,damage>0&&ApplyNativeSourceTypedDamage313(z,4,damage,true),"Zombie329 Garg genuine half-health trigger damage refused"))return false;}
    h.probed=true;h.tick=uint32_t(b->mMainCounter);
    NetTrace("SMOKE_ZOMBIE329_API_APPLIED role=host group=%d MC32=%d ColdFreezeCaltrop16=%d HeadLoss17=%d ThrowThreshold35=%d sourceAPIProbe=1 naturalPlayerCombat=0 HPWrites=0 forcedCallbacks=0",h.group,Find329(32)?1:0,Find329(16)?1:0,Find329(17)?1:0,Find329(35)?1:0);return true;
}
bool ProbeObserved329(LawnApp* a)
{
    auto* b=a->mBoard;
    if(auto* r=Find329(32)){auto* z=Resolve329(b,*r);const auto* d=GetNativeSourceZombieData(z);if(!z||!z->mMindControlled||!d||d->targetEntity!=NativeZombieTargetEntity::None)return false;}
    if(auto* r=Find329(16))if(Resolve329(b,*r))return false;
    if(auto* r=Find329(17))if(Resolve329(b,*r))return false;
    if(auto* r=Find329(35))
    {
        auto* z=Resolve329(b,*r);const auto* ext=GetNativeExtendedZombie313(z);if(!z||!ext||!ext->carriedImpRemoved)return false;
        Zombie* child=nullptr;unsigned count=0;while(b->mZombies.IterateNext(child))if(!child->mDead)if(const auto* d=GetNativeSourceZombieData(child);d&&d->kind==37&&!h.initial.contains(Key329(child->mNetId,d->instanceToken)))
        {++count;if(!h.child){h.imp={child->mNetId,37,d->instanceToken,b->mZombies.DataArrayGetID(child)};GetNativeSourceZombie269NodePoint(child,"Shadow",h.imp.x,h.imp.y);h.child=true;}}
        if(!Require(a,count<=1,"Zombie329 Garg35 spawned duplicate genuine Imp children"))return false;if(!h.child)return false;
        auto* imp=Resolve329(b,h.imp);float x=0,y=0;if(!imp||!GetNativeSourceZombie269NodePoint(imp,"Shadow",x,y))return false;
        h.childMoved|=std::abs(x-h.imp.x)>.05f||std::abs(y-h.imp.y)>.05f;if(!h.childMoved)return false;
    }
    return true;
}
bool ProbeImmediate329(LawnApp* a)
{
    if(auto* r=Find329(32)){auto* z=Resolve329(a->mBoard,*r);const auto* d=GetNativeSourceZombieData(z);if(!z||!d||!z->mMindControlled||d->targetEntity!=NativeZombieTargetEntity::None)return false;}
    if(auto* r=Find329(16)){auto* z=Resolve329(a->mBoard,*r);const auto* d=GetNativeSourceZombieData(z);const auto* v=GetNativeWaterZombie313State(z);
        // Exact HP preservation is checked inside the Caltrop call above.
        // Its beforeDying flag then permits ordinary source health decay on
        // the real tick before pause; canonical capture compares that HP.
        if(!Require(a,z&&d&&v&&d->health>0&&d->health<=h.probeHealth&&d->beforeDying&&!v->colliderEnabled&&!z->mChilledCounter&&!z->mIceTrapCounter,"Zombie329 replicated Driver stop/no-op/health state differs"))return false;}
    if(auto* r=Find329(17))if(!Require(a,!Resolve329(a->mBoard,*r),"Zombie329 genuine underwater head-loss did not retire source17"))return false;
    if(auto* r=Find329(35)){auto* z=Resolve329(a->mBoard,*r);const auto* d=GetNativeSourceZombieData(z);if(!Require(a,z&&d&&d->health<float(z->mBodyMaxHealth)*.5f,"Zombie329 replicated genuine Garg threshold damage differs"))return false;}
    return true;
}
bool BehaviorUpdate329(LawnApp* a,int level)
{
    auto* b=a->mBoard;
    if(s.stage==10)
    {if(!Setup329(a,level)||!Bind329(a,level)||!Paused329(a))return true;if(!Observe329(a,false)||!Canonical329(a,"INITIAL")||!Capture(a,"initial")||!Barrier329(a,"INITIAL_CAPTURED"))return true;Report329(a,"initial");Advance(a,11);}
    else if(s.stage==11){if(!Resumed329(a))return true;h.tick=uint32_t(b->mMainCounter);h.sample=Samples(a);Advance(a,12);}
    else if(s.stage==12)
    {if(b->mPaused||a->GetDialogCount()){a->CoopSmokeFail("Zombie329 unexpected pause/modal during genuine motion interval");return true;}
        if(!Observe329(a,true))return true;if(uint32_t(b->mMainCounter)-h.tick<200||Samples(a)-h.sample<20)return true;
        if(!Capture(a,"live")||!Barrier329(a,"MOTION_OBSERVED"))return true;h.sample=0;h.resume=false;Advance(a,13);}
    else if(s.stage==13)
    {if(!Paused329(a)||!Observe329(a,true)||!Canonical329(a,"MOVED")||!Capture(a,"moved")||!Barrier329(a,"MOVED_CAPTURED"))return true;Report329(a,"moved");
        if(auto* r=Find329(16))h.probeHealth=GetNativeSourceZombieData(Resolve329(b,*r))->health;
        if(Find329(16)||Find329(32)||Find329(35))Advance(a,14);else Advance(a,16);}
    else if(s.stage==14)
    {if(a->NetIsClient()){h.sample=0;Advance(a,17);return true;}
        if(!Resumed329(a)||!Probe329(a))return true;h.sample=0;h.pause=Escape329(a);Advance(a,17);}
    else if(s.stage==17)
    {if(!Paused329(a)||!ProbeImmediate329(a)||!Canonical329(a,"APPLIED")||!Capture(a,"api")||!Barrier329(a,"APPLIED_CAPTURED"))return true;h.resume=false;Advance(a,18);}
    else if(s.stage==18)
    {if(!Resumed329(a))return true;h.sample=Samples(a);Advance(a,15);}
    else if(s.stage==15)
    {if(b->mPaused||a->GetDialogCount()){a->CoopSmokeFail("Zombie329 unexpected pause/modal during genuine callback interval");return true;}
        if(!ProbeObserved329(a)||Samples(a)-h.sample<20||!Barrier329(a,"CALLBACKS_OBSERVED"))return true;h.sample=0;h.resume=false;Advance(a,16);}
    else if(s.stage==16)
    {if(!Paused329(a)||!ProbeObserved329(a)||!Canonical329(a,"CALLBACKS")||!Capture(a,"paused")||!Barrier329(a,"CALLBACKS_CAPTURED"))return true;
        if(!h.caseLogged){h.caseLogged=true;Report329(a,"api");NetTrace("SMOKE_ZOMBIE329_CASE_COMPLETE role=%s biome=%s group=%d roots=5 actualBirths=5 pausedCanonical=1 sourceAPIProbe=%d MC32=%d ColdFreezeCaltrop16=%d HeadLoss17=%d ThrowThreshold35=%d child37=%u/%u naturalPlayerCombat=NotExecuted fullAIParity=0",Role(a),Biome(level),h.group,(Find329(16)||Find329(32)||Find329(35))?1:0,Find329(32)?1:0,Find329(16)?1:0,Find329(17)?1:0,Find329(35)?1:0,h.imp.id,h.imp.token);}
        Advance(a,29);}
    return true;
}

}
namespace LawnNet
{
int NetSmokeFusionZombie313Level325(const LawnApp* a)
{
    if(!Enabled("PVZ_COOP_SMOKE_FUSION_ZOMBIE313")||!Enabled("PVZ_COOP_SMOKE_FUSION_NATIVE")||!a||!a->CoopSmokeActive()||a->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION)return 0;
    const char* biome=std::getenv("PVZ_COOP_SMOKE_FUSION_ZOMBIE313_BIOME");if(!biome||!std::strcmp(biome,"day"))return 1;
    if(!std::strcmp(biome,"night"))return 11;if(!std::strcmp(biome,"pool"))return 21;if(!std::strcmp(biome,"fog"))return 31;return -1;
}
bool NetSmokeUpdateFusionZombie313(LawnApp* a)
{
    if(!Enabled("PVZ_COOP_SMOKE_FUSION_ZOMBIE313"))return false;
    if(!a||!a->CoopSmokeActive()||a->mCoopSmokeResultWritten||!a->mNetManager)return true;
    const int level=NetSmokeFusionZombie313Level325(a);
    if(!Require(a,level>0&&!Enabled("PVZ_COOP_SMOKE_FUSION_SOURCE273")&&a->IsFusionValidationFixture(),"Zombie313 requires isolated owned native validation flags and a valid biome"))return true;
    a->mCoopSmokePeerReady=true;if(!s.start)s.start=s.since=GetTickCount();
    if(GetTickCount()-s.start>(Behavior()?1200000u:240000u)||GetTickCount()-s.since>120000){a->CoopSmokeFail("Zombie313 bounded native resource/input deadline");return true;}
    if(!s.stage)
    {
        if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
        if(!Require(a,Slot(a)>=0&&Slot(a)<2&&a->mNetManager->GetDifficulty()==1,"Zombie313 requires two real owned peers at difficulty1"))return true;
        if(a->NetIsClient()){if(Send(a,"Z313_BEGIN"))Advance(a,1);return true;}
        if(!Has(a,1,"Z313_BEGIN"))return true;std::string error;
        if(Require(a,a->StartFusionValidation(&error),error.empty()?"Zombie313 native validation start refused":error.c_str()))Advance(a,1);return true;
    }
    if(Behavior()&&s.stage==29)
    {
        const auto* context=GetNativeContext(a->mBoard);
        if(context&&context->epoch!=s.epoch)
        {
            const int next=h.group+1;const uint32_t started=s.start,chat=s.chat;auto frames=std::move(s.frames);auto draws=std::move(s.draws);
            s=State{};s.start=started;s.chat=chat;s.frames=std::move(frames);s.draws=std::move(draws);h=BehaviorState329{};h.group=next;
            Advance(a,1);return true;
        }
        if(!Barrier329(a,"GROUP_COMPLETE"))return true;
        if(h.group+1<Groups329(level))
        {if(a->NetIsHost()&&!h.startedNext){h.startedNext=true;std::string error;if(!a->StartFusionValidation(&error))a->CoopSmokeFail(error.c_str());}return true;}
        if(!s.logged){s.logged=true;NetTrace("SMOKE_ZOMBIE329_COMPLETE role=%s biome=%s groups=%d rootsBorn=%zu resourceKinds=%zu pausedCanonicalStages=3 sourceAPIProbe=1 casesMC32=%d casesDriver16=%d casesHead17=%d casesGarg35=%d fullAIParity=0 fullCampaign=0 naturalPlayerCombat=NotExecuted",Role(a),Biome(level),Groups329(level),Kinds(level).size(),Kinds(level).size(),level<21?1:0,level>=21?1:0,level>=21?1:0,level<21?1:0);}
        if(Barrier329(a,"ALL_COMPLETE"))a->CoopSmokeWriteResult(true,nullptr);return true;
    }
    if(!a->mBoard||!World(a,level))return true;auto* b=a->mBoard;const auto& bank=GetNativeContext(b)->selection.State();
    if(Behavior()&&s.stage>=10)return BehaviorUpdate329(a,level);
    if(s.stage==1){if(Choose(a))Advance(a,2);}
    else if(s.stage==2)
    {
        if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}
        auto* button=a->mSeedChooserScreen?a->mSeedChooserScreen->mStartButton:nullptr;if(!button||button->mDisabled||button->mBtnNoDraw||a->mSeedChooserScreen->mSeedsInFlight)return true;
        const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
        if(!button->IsMouseOver())Pointer(a,x,y,false);else if(Pointer(a,x,y))Advance(a,3);
    }
    else if(s.stage==3)
    {
        if(bank.phase==FusionBankPhase::Intro){if(!s.introStart)s.introStart=GetTickCount();s.introEnd=GetTickCount();++s.introSamples;}
        if(bank.phase!=FusionBankPhase::Playing||a->mSeedChooserScreen||a->mGameScene!=SCENE_PLAYING)return true;
        if(!Require(a,s.introSamples>=10&&s.introEnd-s.introStart>=1000,"Zombie313 genuine native intro was skipped"))return true;
        s.samples=Samples(a);Advance(a,Behavior()&&h.group>0?10:4);
    }
    else if(s.stage==4){if(Resources(a,level))Advance(a,Behavior()?10:5);}
    else if(s.stage==5&&Samples(a)-s.samples>=20)
    {
        // Local completion is logged BEFORE the final peer barrier: no host
        // transition can erase a lagging client's proof after receipt.
        if(!s.logged){s.logged=true;NetTrace("SMOKE_ZOMBIE313_RESOURCES_COMPLETE role=%s biome=%s sourceKinds=%zu actualWorldRows=%d nativeColumns=%d FNS=%u chooser=SDL fullIntro=1 resourceOnly=1 acceptedNPCs=0 runtimeAI=NotExecuted bodyMovement=NotExecuted contact=NotExecuted death=NotExecuted ordinaryProjectiles=ReadinessOnly auxClocks=ReadinessOnly fullCampaign=0 actorWrites=0 clockWrites=0 RNGWrites=0",Role(a),Biome(level),Kinds(level).size(),GetNativeFusionRowCount(b),b->GetColumnCount(),kNativeFusionSnapshotVersion);}
        if(Barrier(a,"COMPLETE"))a->CoopSmokeWriteResult(true,nullptr);
    }
    return true;
}
}
