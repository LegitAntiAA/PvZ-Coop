#include "NetFusionDayZombie274Smoke.h"
#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetCommon.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../LawnMower.h"
#include "../Plant.h"
#include "../Coin.h"
#include "../Projectile.h"
#include "../Cutscene.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativeAdventure273.h"
#include "../Fusion/FusionNativeZombies.h"
#include "../Fusion/FusionNativeDayZombie273.h"
#include "../Fusion/NativeSpecialZombie269.h"
#include "../Fusion/FusionNativeNightZombie291Bridge.h"
#include "../Fusion/FusionNativeEffects269.h"
#include "../Fusion/FusionNativeZombiePreflight280.h"
#include "../System/PlayerInfo.h"
#include "../Fusion/FusionNativeZombieVisual.h"
#include "../Fusion/FusionNativeProjectileMetadata.h"
#include "../Fusion/FusionBasePlantRules.h"
#include "../Fusion/FusionNativeEquipment.h"
#include "../Fusion/FusionNativeVisual.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/Attachment.h"
#include "../../Sexy.TodLib/TodParticle.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include "../../SexyAppFramework/ScrollbarWidget.h"
#include "../../SexyAppFramework/DDInterface.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace
{
using namespace LawnFusion;using LawnNet::NetTrace;
constexpr std::array<uint16_t,8> kinds={3,21,22,23,100,106,107,108};
struct Ref{uint16_t id=0;uint32_t token=0;float x=0;};
struct State
{
    int stage=0,waiting=0,playingFrame=-1;uint32_t started=0,since=0,chat=0,epoch=0,startSamples=0,pauseSamples=0,diagnostic=0;
    bool born=false,bound=false,moved=false,pause=false;std::array<Ref,8> actors{};
    std::set<std::string> frames;std::map<std::string,int> draws;
}s;
bool Enabled(const char* name){const auto* value=std::getenv(name);return value&&std::strcmp(value,"1")==0;}
bool Combat(){return Enabled("PVZ_COOP_SMOKE_FUSION_DAY274_COMBAT");}
bool Grass(){return Enabled("PVZ_COOP_SMOKE_FUSION_GRASS284");}
bool Jackson(){return Enabled("PVZ_COOP_SMOKE_FUSION_JACKSON291");}
struct GrassState
{
    std::array<Ref,3> actors{},walls{};
    std::array<uint32_t,3> actorUID{};
    std::array<Ref,4> children{};
    std::array<int,3> wallHP{},bodyFrame{},bodyLoops{};
    std::array<float,3> bodyTime{};
    std::array<bool,3> moved{},animated{};
    std::array<bool,4> childMoved{},childAnimated{};
    std::array<float,4> childTime{};
    std::array<int,4> childLoops{};
    std::array<bool,2> contact{},armor{},arm{},head{},retired{},shot{};
    std::map<std::string,std::string> proofs;
    bool resources=false,setup=false,initial=false,run=false,jump=false,land=false,anchorKnown=false,
        childrenBound=false,firstWeapons=false,footWeapons=false,fall=false,rage=false,
        pauseRequested=false,resumeRequested=false,logged=false;
    float birthAnchor=0;
    int anchorRow=-1;
    uint32_t childSamples=0,diagnostic=0;
}grass;
struct CombatState
{
    int cohort=0,chainIndex=-1,previousNutHP=0;uint32_t oldEpoch=0,diagnostic=0;
    bool setup=false,restarted=false,initial=false,resources=false,armor107=false,armorCaptured=false,dead107=false,
        headless108=false,hit11=false,hitCaptured=false,chomp=false,proofReady=false,completionLogged=false;
    std::array<Ref,4> chain{};std::array<bool,4> chainCaptured{};std::array<bool,3> damaged{};
    std::array<Ref,2> npc{};Ref chomper,wall,bucket;uint16_t seenIronShot=0;
    std::string proofTag;
}combat;
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool okay,const char* reason){if(!okay)a->CoopSmokeFail(reason);return okay;}
void Advance(LawnApp* a,int stage)
{s.stage=stage;s.since=GetTickCount();NetTrace("SMOKE_DAY274_STAGE role=%s stage=%d epoch=%u",Role(a),stage,s.epoch);}
std::string Tag(const std::string& value){return "D274_"+std::to_string(s.epoch)+"_"+value;}
bool Has(LawnApp* a,int slot,const std::string& text)
{for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==slot&&line.mText==text)return true;return false;}
bool Send(LawnApp* a,const std::string& text)
{
    if(!Require(a,text.size()<=LawnNet::NET_MAX_CHAT_CHARS,"Day274 chat proof exceeds limit"))return false;
    if(Has(a,Slot(a),text))return true;
    if(s.chat&&GetTickCount()-s.chat<800)return false;
    s.chat=GetTickCount();a->mNetManager->SendChat(text);return Has(a,Slot(a),text);
}
bool Barrier(LawnApp* a,const std::string& value)
{const auto text=Tag(value);Send(a,text);return Has(a,0,text)&&Has(a,1,text);}
bool CombatProofBarrier(LawnApp* a,const char* name,const Ref* refs,size_t count)
{
    LawnNet::NetBuffer proof;for(size_t i=0;i<count;++i){proof.WriteU16(refs[i].id);proof.WriteU32(refs[i].token);}
    char digest[16];sprintf_s(digest,"%08x",LawnNet::NetFnv1a32(proof.Data(),proof.Size()));
    const auto value=std::string(name)+"_"+digest;
    combat.proofTag=Tag(value);combat.proofReady=true;combat.oldEpoch=s.epoch;
    return Barrier(a,value);
}
bool Click(LawnApp* a,float x,float y)
{
    SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.windowID=SDL_GetWindowID(a->mSDLWindow);motion.motion.x=x;motion.motion.y=y;
    SDL_Event down{};down.type=SDL_EVENT_MOUSE_BUTTON_DOWN;down.button.windowID=motion.motion.windowID;down.button.button=SDL_BUTTON_LEFT;down.button.clicks=1;down.button.down=true;down.button.x=x;down.button.y=y;
    SDL_Event up=down;up.type=SDL_EVENT_MOUSE_BUTTON_UP;up.button.down=false;
    return Require(a,SDL_PushEvent(&motion)&&SDL_PushEvent(&down)&&SDL_PushEvent(&up),"Day274 owned SDL click failed");
}
bool Escape(LawnApp* a)
{
    SDL_Event down{};down.type=SDL_EVENT_KEY_DOWN;down.key.windowID=SDL_GetWindowID(a->mSDLWindow);down.key.key=SDLK_ESCAPE;down.key.down=true;
    SDL_Event up=down;up.type=SDL_EVENT_KEY_UP;up.key.down=false;
    return Require(a,SDL_PushEvent(&down)&&SDL_PushEvent(&up),"Day274 owned SDL Escape failed");
}
bool Capture(LawnApp* a,const char* suffix)
{
    const std::string name=(Jackson()?"fusion_jackson291_":Grass()?"fusion_grass284_":"fusion_day274_")+(Combat()?"combat"+std::to_string(combat.cohort)+"_":"")+std::string(suffix);if(s.frames.contains(name))return true;
    if(!s.draws.contains(name))s.draws[name]=a->mDrawCount;
    const int requested=s.draws[name]+2;if(!a->CoopSmokeEnsureFreshFrameAfterDraw(requested))return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Day274 fresh owned capture failed"))return false;
    s.frames.insert(name);NetTrace("SMOKE_DAY274_RENDER role=%s frame=%s requestedDraw=%d capturedDraw=%d",Role(a),name.c_str(),requested,a->mCoopSmokeFrameDrawCount);return true;
}
struct JacksonActor297
{
    Ref ref;uint32_t uid=0;uint16_t kind=0;int row=-1,loops=0,frame=0,order=0;
    float time=0,rootX=0,rootY=0,shadowX=0,shadowY=0;
    bool moved=false,animated=false,jump=false,land=false,cloud=false;
};
struct JacksonCloud297 {float x=0,y=0;int order=0;bool used=false;};
struct JacksonState297
{
    Ref parent,wall;uint32_t parentUID=0,introStart=0,introLast=0,diagnostic=0;
    int introChecks=0,initialWallHP=0,classicLevel=-1,classicFinished=-1,groups=0,expected=-1;
    bool prepared=false,setup=false,initial=false,blend=false,contact=false,damage=false,gun=false,death=false,retired=false,logged=false;
    std::map<uint64_t,JacksonActor297> actors;
    std::map<uint32_t,JacksonCloud297> clouds;
    std::vector<uint64_t> closure;
    std::map<std::string,std::string> proofs;
}jackson;
uint64_t JacksonKey(uint16_t id,uint32_t token){return (uint64_t(token)<<16)|id;}
bool JacksonWorld(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);if(!c||!c->adventure.enabled)return false;
    const auto& v=c->adventure;int maximum=0;
    if(!Require(a,TryGetFusionSourceAdventureWaves272(10,1,maximum)&&IsNativeSourceAdventure273(b)&&!c->validation&&!a->mFusionRuntime&&
        v.run.level==10&&v.run.cycle==1&&!v.run.replay&&!v.run.over&&!v.schedulerFailed&&v.sourceDifficulty==2&&v.maximumWaves==maximum&&
        v.profile.completedLevels==511&&v.profile.frontierLevel==10&&v.profile.cycle==1&&c->money==3000&&
        b->mBackground==BACKGROUND_2_NIGHT&&b->GetColumnCount()==10&&GetNativeFusionRowCount(b)==5&&b->mGridItems.mSize==0&&
        a->mPlayerInfo&&a->mPlayerInfo->mLevel==jackson.classicLevel&&a->mPlayerInfo->mFinishedAdventure==jackson.classicFinished,
        "Jackson291 genuine Night10 world/profile/Classic progress differs"))return false;
    for(int row=0;row<5;++row)if(!Require(a,b->mPlantRow[row]==PLANTROW_NORMAL,"Jackson291 ordinary Night has non-grass row"))return false;
    if(!s.epoch)s.epoch=c->epoch;return Require(a,c->epoch==s.epoch,"Jackson291 source epoch changed");
}
bool World(LawnApp* a)
{
    if(Jackson())return JacksonWorld(a);
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);if(!c)return false;
    if(!Require(a,IsNativeSourceAdventure273(b)&&!c->validation&&!a->mFusionRuntime&&c->adventure.enabled&&
        c->adventure.run.level==1&&c->adventure.run.cycle==1&&c->adventure.sourceDifficulty==2&&
        !c->adventure.schedulerFailed&&!c->adventure.run.over&&c->adventure.run.tutorial.active&&
        c->adventure.wave==0&&c->adventure.maximumWaves==10&&c->money==3000&&
        b->GetColumnCount()==10&&GetNativeFusionRowCount(b)==5&&b->mBackground==BACKGROUND_1_DAY,
        "Day274 source1 world/tutorial/wave gate differs"))return false;
    for(int row=0;row<5;++row)if(!Require(a,b->mPlantRow[row]==PLANTROW_NORMAL,"Day274 non-Day row leaked"))return false;
    if(!s.epoch)s.epoch=c->epoch;
    return Require(a,s.epoch==c->epoch,"Day274 source epoch changed");
}
bool JacksonChoose(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);auto* q=a->mSeedChooserScreen;
    if(!q||!q->mNativeFusionChooser||!q->mMouseVisible||q->mSeedsInFlight||!b->mCutScene->mSeedChoosing)return false;
    const auto& bank=c->selection.State();const int slot=Slot(a);constexpr uint16_t wanted[]={0,1,3,5,6};
    if(!Require(a,bank.activeMask==3&&bank.targetCount==10&&bank.quotas[0]==5&&bank.quotas[1]==5,
        "Jackson291 real native chooser quotas differ"))return false;
    const auto& intro=b->mNetIntroTargets;const int ox=a->mDDInterface->mWideScreenOffsetX,oy=a->mDDInterface->mWideScreenOffsetY;
    if((a->NetIsClient()&&!intro.mValid)||q->mX!=(a->NetIsClient()?intro.mChooserX+ox:ox)||
        q->mY!=(a->NetIsClient()?intro.mChooserY+oy:oy)||q->mNativeFusionRevision!=bank.revision)return false;
    int own=0;for(const auto& p:bank.packets)if(p.owner==slot)
    {if(!Require(a,own<5&&p.kind==wanted[own]&&p.isExtra==(slot==1),"Jackson291 owned native pick identity differs"))return false;++own;}
    if(own<s.waiting)return false;
    if(own<5)
    {
        int dense=-1;for(size_t i=0;i<c->eligible.size();++i)if(c->eligible[i].kind==wanted[own]&&c->eligible[i].isExtra==(slot==1))dense=int(i);
        if(!Require(a,dense>=0,"Jackson291 unlocked source normal/extra card missing"))return false;
        int x=0,y=0;q->GetSeedPositionInChooser(dense,x,y);y-=int(q->mScrollbar->mScrollValue);
        if(!Require(a,y+35>=123&&y+35<528,"Jackson291 fixed earned chooser card needs an unsupported scroll"))return false;
        const int sx=q->mX+x+25,sy=q->mY+y+35;int lx=0,ly=0;
        if(a->mWidgetManager->GetWidgetAt(sx,sy,&lx,&ly)!=q||!Rect(x,y,50,70).Contains(lx,ly))return false;
        if(a->mWidgetManager->mLastMouseX!=sx||a->mWidgetManager->mLastMouseY!=sy)
        {SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=float(sx);m.motion.y=float(sy);Require(a,SDL_PushEvent(&m),"Jackson291 owned chooser hover failed");return false;}
        if(Click(a,float(sx),float(sy)))s.waiting=own+1;return false;
    }
    return bank.packets.size()==10&&Capture(a,"chooser")&&Barrier(a,"J291_CHOOSER");
}
bool Choose(LawnApp* a)
{
    if(Jackson())return JacksonChoose(a);
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);auto* chooser=a->mSeedChooserScreen;
    if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||chooser->mSeedsInFlight||!b->mCutScene->mSeedChoosing)return false;
    const auto& bank=c->selection.State();
    if(!Require(a,c->eligible.size()==4&&bank.targetCount==4&&bank.quotas[0]==2&&bank.quotas[1]==2,"Day274 source chooser pool differs"))return false;
    int own=0;for(const auto& packet:bank.packets)if(packet.owner==Slot(a))
    {if(!Require(a,packet.kind<=1&&packet.isExtra==(Slot(a)==1),"Day274 local chooser identity differs"))return false;++own;}
    if(own<s.waiting)return false;
    if(own<2)
    {
        int dense=-1;for(size_t i=0;i<c->eligible.size();++i)
            if(c->eligible[i].kind==own&&c->eligible[i].isExtra==(Slot(a)==1))dense=int(i);
        if(!Require(a,dense>=0,"Day274 actual local card missing"))return false;
        int x=0,y=0;chooser->GetSeedPositionInChooser(dense,x,y);
        if(std::abs(chooser->mScrollAmount)>.05f)return false;
        if(Click(a,float(chooser->mX+x+25),float(chooser->mY+y+35-int(chooser->mScrollbar->mScrollValue))))s.waiting=own+1;
        return false;
    }
    return bank.packets.size()==4&&Capture(a,"chooser")&&Barrier(a,"CHOOSER");
}
bool Birth(LawnApp* a)
{
    if(!a->NetIsHost()||s.born)return true;
    auto* b=a->mBoard;std::string error;
    if(!Require(a,b->mPlants.mSize==0&&b->mZombies.mSize==0&&b->mSunMoney==500,"Day274 cohort setup was not an empty real source opening"))return false;
    if(!Require(a,CanAllocateNativeSourceDayBatch274(b,kinds,&error),"Day274 actual whole-cohort allocation preflight rejected"))
    {NetTrace("SMOKE_DAY274_BIRTH_REJECT error=%s",error.c_str());return false;}
    // Only birth parameters position real native actors. No post-birth warp,
    // HP, RNG, animation, status, clocks or tutorial state is written here.
    for(size_t i=0;i<kinds.size();++i)
    {
        NativeSourceZombieSpawnRequest request;request.kind=kinds[i];request.row=int(i/2);request.fromWave=0;
        request.positioned=true;request.shadowX=i%2?520.f:350.f;
        auto* zombie=SpawnNativeSourceZombie(b,request);const auto* data=GetNativeSourceZombieData(zombie);
        if(!Require(a,zombie&&data&&data->kind==kinds[i]&&zombie->mNetId&&data->instanceToken,"Day274 genuine typed cohort factory failed"))return false;
        NetTrace("SMOKE_DAY274_BIRTH kind=%u id=%u token=%u row=%d shadowBirthX=%.2f body=%d armor=%d head=%u postBirthWrites=0",
            data->kind,zombie->mNetId,data->instanceToken,zombie->mRow,request.shadowX,zombie->mBodyHealth,zombie->mHelmHealth,unsigned(zombie->mSpecialHeadReanimID));
    }
    s.born=true;return true;
}
bool CaptureHead(const Reanimation* head,LawnNet::NetZombieAnimState& state,LawnNet::NetBuffer& wire)
{
    state.Reset();
    if(head)
    {
        if(!head->mDefinition||!head->mDefinition->mTracks.tracks||head->mDefinition->mTracks.count<=0||
            head->mDefinition->mTracks.tracks[0].mTransforms.count<=0||
            head->mFrameStart<0||head->mFrameStart>65535||head->mFrameCount<=0||head->mFrameCount>65535||
            !std::isfinite(head->mAnimTime)||head->mAnimTime<0||head->mAnimTime>1||!std::isfinite(head->mAnimRate)||
            int(head->mLoopType)<0||int(head->mLoopType)>5)return false;
        state.mPresent=true;state.mDead=head->mDead;state.mFrameStart=uint16_t(head->mFrameStart);
        state.mFrameCount=uint16_t(head->mFrameCount);state.mAnimTime=head->mAnimTime;state.mAnimRate=head->mAnimRate;
        state.mLoopType=uint8_t(head->mLoopType);state.mLoopCount=uint8_t((std::clamp)(head->mLoopCount,0,255));
        if(!LawnNet::NetZombieAnimStateFitsFrames(state,unsigned(head->mDefinition->mTracks.tracks[0].mTransforms.count)))return false;
    }
    return LawnNet::NetEncodeZombieAnimState(state,wire);
}
bool Observe(LawnApp* a,bool requireMovement,const char* frame)
{
    auto* b=a->mBoard;std::array<Zombie*,8> actual{};Zombie* zombie=nullptr;unsigned count=0;
    while(b->IterateZombies(zombie))
    {
        const auto* source=GetNativeSourceZombieData(zombie);
        if(!Require(a,source&&!zombie->mMindControlled&&!zombie->IsDeadOrDying(),"Day274 untyped/friendly/dying actor replaced the initial cohort"))return false;
        const auto index=std::find(kinds.begin(),kinds.end(),source->kind);
        if(!Require(a,index!=kinds.end(),"Day274 unexpected source NPC kind"))return false;
        const auto i=size_t(index-kinds.begin());if(!Require(a,!actual[i],"Day274 duplicate initial cohort kind"))return false;
        actual[i]=zombie;++count;
    }
    if(count!=8)return false;
    bool visible=true,moving=true;LawnNet::NetBuffer proof;
    const bool trace=!s.bound||(!s.draws.contains("fusion_day274_"+std::string(frame))&&
        (!s.diagnostic||GetTickCount()-s.diagnostic>=1000));
    if(trace)s.diagnostic=GetTickCount();
    for(size_t i=0;i<actual.size();++i)
    {
        auto* z=actual[i];const auto* source=GetNativeSourceZombieData(z);const auto* day=GetNativeDayZombie273(z);
        const auto* profile=FindFusionDayZombie273Profile(kinds[i]);auto* body=a->ReanimationTryToGet(z->mBodyReanimID);
        auto* head=a->ReanimationTryToGet(z->mSpecialHeadReanimID);const bool shooter=FusionDayZombie273Shooter(kinds[i]);
        if(!Require(a,day&&profile&&body&&body->mFrameCount>0&&z->mZombieType==GetNativeSourceZombieCarrier(kinds[i])&&
            z->mBodyMaxHealth==profile->maxHealth&&z->mBodyHealth==z->mBodyMaxHealth&&source->health==float(z->mBodyHealth)&&
            z->mHelmMaxHealth==profile->firstArmor&&z->mHelmHealth==z->mHelmMaxHealth&&z->mFromWave==0&&z->mHasHead&&z->mHasArm&&
            (!shooter||(head&&head->mReanimationType==REANIM_PEASHOOTER&&head->mFrameCount>0)),
            "Day274 actual source body/armor/head contract differs"))return false;
        if(!Require(a,(kinds[i]!=3||day->status==2)&&day->shadowEnabled,"Day274 unprovoked Pole status/shadow differs"))return false;
        LawnNet::NetZombieAnimState actualHead;LawnNet::NetBuffer actualHeadWire,replicatedHeadWire;
        if(!Require(a,CaptureHead(head,actualHead,actualHeadWire),"Day274 actual head has invalid frame/clock state"))return false;
        if(a->NetIsClient()&&shooter)
        {
            if(!Require(a,day->head.mPresent&&NativeDayZombie273StateValid(kinds[i],*day)&&
                LawnNet::NetEncodeZombieAnimState(day->head,replicatedHeadWire)&&
                actualHead.mFrameStart==day->head.mFrameStart&&actualHead.mFrameCount==day->head.mFrameCount&&
                actualHead.mLoopType==day->head.mLoopType&&actualHead.mDead==day->head.mDead&&
                actualHeadWire.mData[6]==replicatedHeadWire.mData[6]&&actualHeadWire.mData[7]==replicatedHeadWire.mData[7],
                "Day274 replicated/actual native shooter head frame/rate contract differs"))return false;
        }
        if(!s.actors[i].id)s.actors[i]={z->mNetId,source->instanceToken,z->mPosX};
        if(!Require(a,z->mNetId==s.actors[i].id&&source->instanceToken==s.actors[i].token,"Day274 initial cohort UID/token changed"))return false;
        float rx=0,ry=0,sx=0,sy=0;
        const bool rootOkay=GetNativeSourceZombie269NodePoint(z,"",rx,ry);
        const bool shadowOkay=GetNativeSourceZombie269NodePoint(z,"Shadow",sx,sy);
        if(!rootOkay||!shadowOkay)
        {
            const char* headClip=nullptr;float headPhase=0;const bool headPose=GetNativeDayZombie273HeadPose(z,headClip,headPhase);
            NetTrace("SMOKE_DAY274_NODE_FAILURE role=%s kind=%u id=%u token=%u row=%d rootOkay=%d root=(%.4f,%.4f) shadowOkay=%d shadow=(%.4f,%.4f) status=%u shootEvent=%u body=(id=%u type=%d dead=%d frames=%d+%d time=%.9g rate=%.9g loop=%d/%d) head=(id=%u present=%d frames=%d+%d time=%.9g rate=%.9g loop=%d/%d clip=%s poseOkay=%d phase=%.9g) wireHead=(present=%d frames=%u+%u time=%.9g rate=%.9g)",
                Role(a),kinds[i],z->mNetId,source->instanceToken,z->mRow,rootOkay?1:0,rx,ry,shadowOkay?1:0,sx,sy,
                unsigned(day->status),unsigned(day->shootEvent),unsigned(z->mBodyReanimID),int(body->mReanimationType),body->mDead?1:0,
                body->mFrameStart,body->mFrameCount,body->mAnimTime,body->mAnimRate,int(body->mLoopType),body->mLoopCount,
                unsigned(z->mSpecialHeadReanimID),head?1:0,head?head->mFrameStart:0,head?head->mFrameCount:0,head?head->mAnimTime:0,
                head?head->mAnimRate:0,head?int(head->mLoopType):0,head?head->mLoopCount:0,headClip?headClip:"none",headPose?1:0,headPhase,day->head.mPresent?1:0,
                unsigned(day->head.mFrameStart),unsigned(day->head.mFrameCount),day->head.mAnimTime,day->head.mAnimRate);
        }
        if(!Require(a,rootOkay&&shadowOkay,"Day274 actual model nodes unavailable"))return false;
        const float dx=float(b->mX+a->mDDInterface->mWideScreenOffsetX),dy=float(b->mY+a->mDDInterface->mWideScreenOffsetY);
        const bool inFrame=(std::min)(rx+dx,sx+dx)>=40&&(std::max)(rx+dx,sx+dx)<=a->mWidth-80&&
            (std::min)(ry+dy,sy+dy)>=0&&(std::max)(ry+dy,sy+dy)<=a->mHeight-8;
        visible&=inFrame;moving&=s.actors[i].x-z->mPosX>=2.f;
        proof.WriteU16(kinds[i]);proof.WriteU16(z->mNetId);proof.WriteU32(source->instanceToken);
        proof.WriteU8(uint8_t(z->mRow));proof.WriteI32(z->mBodyHealth);proof.WriteI32(z->mHelmHealth);
        if(b->mPaused)
        {
            if(a->NetIsClient()&&shooter&&!Require(a,actualHeadWire.mData==replicatedHeadWire.mData,
                "Day274 paused actual head phase/loop differs from canonical source snapshot"))return false;
            proof.WriteU8(day->status);proof.WriteU8(day->shootEvent);proof.WriteU8(day->armorStage);
            proof.WriteU8(day->shadowEnabled?1:0);proof.WriteF32(day->shootCountdown);
            proof.WriteBytes(actualHeadWire.Data(),actualHeadWire.Size());
            if(trace)NetTrace("SMOKE_DAY274_PAUSED_HEAD role=%s kind=%u id=%u token=%u status=%u shootEvent=%u time=%.9g rate=%.9g loop=%u/%u canonicalHash=%08x",
                Role(a),kinds[i],z->mNetId,source->instanceToken,unsigned(day->status),unsigned(day->shootEvent),actualHead.mAnimTime,
                actualHead.mAnimRate,unsigned(actualHead.mLoopType),unsigned(actualHead.mLoopCount),LawnNet::NetFnv1a32(actualHeadWire.Data(),actualHeadWire.Size()));
        }
        if(trace)
            NetTrace("SMOKE_DAY274_BIND role=%s kind=%u id=%u token=%u row=%d carrierX=%.3f initialX=%.3f root=(%.3f,%.3f) shadow=(%.3f,%.3f) screenRoot=(%.3f,%.3f) screenShadow=(%.3f,%.3f) viewport=%dx%d body=%u head=%u status=%u shootEvent=%u headPhase=%.4f visible=%d",
                Role(a),kinds[i],z->mNetId,source->instanceToken,z->mRow,z->mPosX,s.actors[i].x,rx,ry,sx,sy,rx+dx,ry+dy,sx+dx,sy+dy,
                a->mWidth,a->mHeight,unsigned(z->mBodyReanimID),unsigned(z->mSpecialHeadReanimID),unsigned(day->status),unsigned(day->shootEvent),head?head->mAnimTime:0,inFrame?1:0);
    }
    if(!s.bound){s.bound=true;s.startSamples=Samples(a);}
    if(!visible||(requireMovement&&(!moving||Samples(a)-s.startSamples<20)))return false;
    char tag[40];sprintf_s(tag,"%s_%08x",frame,LawnNet::NetFnv1a32(proof.Data(),proof.Size()));
    if(!Capture(a,frame)||!Barrier(a,tag))return false;
    if(requireMovement&&!s.moved)
    {s.moved=true;NetTrace("SMOKE_DAY274_MOVED role=%s actors=8 minimumDelta=2 samples=%u UIDTokenStable=1 rootShadowVisible=1",Role(a),Samples(a)-s.startSamples);}
    return true;
}
bool Shots(LawnApp* a)
{
    if(s.frames.contains("fusion_day274_shots"))return Barrier(a,"SHOTS");
    auto* b=a->mBoard;std::array<Projectile*,2> shots{};Projectile* p=nullptr;
    while(b->IterateProjectiles(p))if(const auto* d=GetNativeProjectileData(p);d&&(d->kind==0||d->kind==11))
    {
        if(p->mDead||!p->mNetId||!d->isZombieBullet||d->specialType!=9||d->damage!=(d->kind==0?20:80)||
            !NativeDay273ProjectileMetadataValid(*d,p->mProjectileType,p->mMotionType))continue;
        const float x=p->mPosX+20+b->mX+a->mDDInterface->mWideScreenOffsetX;
        const float y=p->mPosY+20+p->mPosZ+b->mY+a->mDDInterface->mWideScreenOffsetY;
        if(!std::isfinite(x)||!std::isfinite(y)||x<32||x>a->mWidth-32||y<100||y>a->mHeight-32)continue;
        shots[d->kind==0?0:1]=p;
    }
    if(!shots[0]||!shots[1])return false;
    for(auto* shot:shots)
    {
        const auto* d=GetNativeProjectileData(shot);
        NetTrace("SMOKE_DAY274_SHOT role=%s id=%u kind=%u damage=%d hostile=1 way=9 row=%d age=%d raw=(%.3f,%.3f,%.3f) screen=(%.3f,%.3f) naturallyFired=1",
            Role(a),shot->mNetId,d->kind,d->damage,shot->mRow,shot->mProjectileAge,shot->mPosX,shot->mPosY,shot->mPosZ,
            shot->mPosX+20+b->mX+a->mDDInterface->mWideScreenOffsetX,shot->mPosY+20+shot->mPosZ+b->mY+a->mDDInterface->mWideScreenOffsetY);
    }
    return Capture(a,"shots")&&Barrier(a,"SHOTS");
}
Plant* CombatPlant(Board* b,Ref ref)
{Plant* p=nullptr;while(b->IteratePlants(p))if(const auto* d=GetNativePlantData(p);d&&p->mNetId==ref.id&&d->instanceToken==ref.token)return p;return nullptr;}
Plant* CombatPlantAt(Board* b,uint16_t kind,int col,int row)
{Plant* p=nullptr;while(b->IteratePlants(p))if(const auto* d=GetNativePlantData(p);d&&d->kind==kind&&p->mPlantCol==col&&p->mRow==row)return p;return nullptr;}
Zombie* CombatZombie(Board* b,uint16_t kind)
{Zombie* z=nullptr;while(b->IterateZombies(z))if(!z->IsDeadOrDying())if(const auto* d=GetNativeSourceZombieData(z);d&&d->kind==kind)return z;return nullptr;}
Zombie* CombatZombieRef(Board* b,Ref ref)
{Zombie* z=nullptr;while(b->IterateZombies(z))if(const auto* d=GetNativeSourceZombieData(z);d&&z->mNetId==ref.id&&d->instanceToken==ref.token)return z;return nullptr;}
bool CombatVisible(LawnApp* a,Zombie* z)
{
    float rx=0,ry=0,x=0,y=0;if(!Require(a,GetNativeSourceZombie269NodePoint(z,"",rx,ry)&&
        GetNativeSourceZombie269NodePoint(z,"Shadow",x,y),"Day275 actual combat model nodes unavailable"))return false;
    const float dx=float(a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX),dy=float(a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY);
    return (std::min)(rx+dx,x+dx)>=32&&(std::max)(rx+dx,x+dx)<=a->mWidth-64&&
        (std::min)(ry+dy,y+dy)>=0&&(std::max)(ry+dy,y+dy)<=a->mHeight-8;
}
bool CombatBind(LawnApp* a,Zombie* z,uint16_t kind,Ref& ref)
{
    const auto* d=GetNativeSourceZombieData(z);const auto* p=FindFusionZombieProfile(kind);
    if(!z)return false;
    if(!Require(a,d&&p&&d->kind==kind&&z->mNetId&&d->instanceToken&&!z->mMindControlled&&z->mFromWave==0&&
        z->mZombieType==GetNativeSourceZombieCarrier(kind)&&z->mBodyMaxHealth==p->maxHealth&&
        z->mHelmMaxHealth==p->firstArmor&&z->mBodyHealth>0,"Day275 source combat actor contract differs"))return false;
    if(!ref.id)
    {
        ref={z->mNetId,d->instanceToken,z->mPosX};
        NetTrace("SMOKE_DAY275_BIND role=%s cohort=%d kind=%u id=%u token=%u row=%d body=%d/%d helm=%d/%d",
            Role(a),combat.cohort,kind,ref.id,ref.token,z->mRow,z->mBodyHealth,z->mBodyMaxHealth,z->mHelmHealth,z->mHelmMaxHealth);
    }
    return Require(a,z->mNetId==ref.id&&d->instanceToken==ref.token,"Day275 canonical source combat generation changed");
}
bool CombatPreflight(LawnApp* a)
{
    if(combat.resources)return true;
    auto* b=a->mBoard;std::string error;
    for(uint16_t kind:{uint16_t(1032),uint16_t(5),uint16_t(3)})
    {
        if(!Require(a,EnsureNativePlantVisuals(b,kind,&error),"Day275 actual plant resources unavailable"))return false;
        const auto seed=GetNativeSeedForPlant(kind);
        if(!Require(a,seed!=SEED_NONE,"Day275 source plant has no native carrier"))return false;
        ReanimationPreload(GetPlantDefinition(seed).mReanimationType);
    }
    for(uint16_t kind:{uint16_t(21),uint16_t(22),uint16_t(23),uint16_t(2),uint16_t(106),uint16_t(107),uint16_t(108)})
    {
        if(!Require(a,EnsureNativeSourceZombieVisuals(b,kind,&error),"Day275 actual NPC resources unavailable"))return false;
        ReanimationPreload(GetZombieDefinition(ZombieType(GetNativeSourceZombieCarrier(kind))).mReanimationType);
    }
    if(!Require(a,CanCreateNativeEquipmentVisual(b,FusionEquipmentType::Bucket),"Day275 source Bucket resources unavailable"))return false;
    combat.resources=true;return true;
}
bool CombatPlantBirth(LawnApp* a,uint16_t kind,int col,int row,Ref* saved=nullptr)
{
    NativePlantData d;d.kind=kind;d.owner=LawnNet::NET_SHARED_OWNER;
    auto* p=a->mBoard->AddPlant(col,row,GetNativeSeedForPlant(kind),SEED_NONE,&d);const auto* actual=GetNativePlantData(p);
    if(!Require(a,p&&actual&&actual->kind==kind&&p->mNetId&&actual->instanceToken&&p->mPlantHealth==p->mPlantMaxHealth,
        "Day275 genuine initial source plant factory failed"))return false;
    if(saved)*saved={p->mNetId,actual->instanceToken,0};
    NetTrace("SMOKE_DAY275_PLANT_BIRTH kind=%u id=%u token=%u col=%d row=%d hp=%d/%d initialFactory=1 recipeAcquisition=NotExecuted postBirthWrites=0",
        kind,p->mNetId,actual->instanceToken,col,row,p->mPlantHealth,p->mPlantMaxHealth);return true;
}
bool CombatZombieBirth(LawnApp* a,uint16_t kind,int row,float shadowX,Ref* saved=nullptr)
{
    NativeSourceZombieSpawnRequest request;request.kind=kind;request.row=row;request.fromWave=0;request.positioned=true;request.shadowX=shadowX;
    auto* z=SpawnNativeSourceZombie(a->mBoard,request);const auto* d=GetNativeSourceZombieData(z);
    if(!Require(a,z&&d&&d->kind==kind&&z->mBodyHealth==z->mBodyMaxHealth&&z->mHelmHealth==z->mHelmMaxHealth,
        "Day275 genuine initial typed NPC factory failed"))return false;
    if(saved)*saved={z->mNetId,d->instanceToken,z->mPosX};
    NetTrace("SMOKE_DAY275_BIRTH cohort=%d kind=%u id=%u token=%u row=%d shadowBirthX=%.2f body=%d helm=%d postBirthWrites=0",
        combat.cohort,kind,z->mNetId,d->instanceToken,row,shadowX,z->mBodyHealth,z->mHelmHealth);return true;
}
bool CombatSetup(LawnApp* a)
{
    if(!a->NetIsHost()||combat.setup)return true;
    auto* b=a->mBoard;
    if(!Require(a,!b->mPlants.mSize&&!b->mZombies.mSize&&!b->mPaused,"Day275 initial cohort must use an empty active source world"))return false;
    if(combat.cohort==0)
    {
        for(int col=0;col<5;++col)if(!CombatPlantBirth(a,1032,col,1))return false;
        if(!CombatZombieBirth(a,21,1,660.f))return false;
    }
    else if(combat.cohort==1)
    {
        if(!CombatPlantBirth(a,5,3,1,&combat.chomper)||
            !CombatZombieBirth(a,106,1,float(b->GetCellCenterX(3,1)+240),&combat.npc[0]))return false;
    }
    else
    {
        for(int row:{1,3})for(int col=0;col<2;++col)if(!CombatPlantBirth(a,1032,col,row))return false;
        if(!CombatPlantBirth(a,3,3,3,&combat.wall)||!CombatZombieBirth(a,107,1,660.f,&combat.npc[0])||
            !CombatZombieBirth(a,108,3,660.f,&combat.npc[1]))return false;
    }
    combat.setup=true;return true;
}
bool CombatBucket(LawnApp* a)
{
    auto* b=a->mBoard;Coin* coin=nullptr;Coin* found=nullptr;unsigned count=0;
    while(b->IterateCoins(coin))if(const auto* d=GetNativeEquipmentData(coin);d&&d->factoryType==FusionEquipmentType::Bucket)
    {
        ++count;found=coin;
        if(!Require(a,d->type==FusionEquipmentType::Bucket&&!d->weapon&&!d->sourcePlantId&&d->instanceToken&&
            coin->mType==COIN_NONE&&coin->mNetId&&NativeEquipmentDataValid(*d),"Day275 actual Bucket factory metadata differs"))return false;
    }
    if(!Require(a,count<=1,"Day275 exclusive cohort produced ambiguous multiple Bucket items"))return false;
    if(!found)return false;const auto* d=GetNativeEquipmentData(found);
    if(!combat.bucket.id)
    {
        combat.bucket={found->mNetId,d->instanceToken,0};
        NetTrace("SMOKE_DAY275_BUCKET role=%s cohort=%d id=%u token=%u factory=Bucket phase=%u exclusiveSourceKind=%d sourceOwnershipField=NotPresent",
            Role(a),combat.cohort,found->mNetId,d->instanceToken,unsigned(d->phase),combat.cohort==1?106:108);
    }
    if(!Require(a,found->mNetId==combat.bucket.id&&d->instanceToken==combat.bucket.token,"Day275 Bucket generation changed"))return false;
    const float x=found->mPosX+20+b->mX+a->mDDInterface->mWideScreenOffsetX;
    const float y=found->mPosY+20+b->mY+a->mDDInterface->mWideScreenOffsetY;
    return d->phase==NativeEquipmentPhase::World&&std::isfinite(x)&&std::isfinite(y)&&x>=24&&x<=a->mWidth-32&&y>=100&&y<=a->mHeight-16;
}
bool CombatInitial(LawnApp* a)
{
    if(combat.initial)return true;auto* b=a->mBoard;
    if(combat.cohort==0)
    {auto* z=CombatZombie(b,21);if(!CombatBind(a,z,21,combat.chain[0])||!CombatVisible(a,z))return false;combat.chainIndex=0;}
    else if(combat.cohort==1)
    {
        auto* z=CombatZombie(b,106);auto* p=CombatPlantAt(b,5,3,1);
        if(!p||!CombatBind(a,z,106,combat.npc[0])||!CombatVisible(a,z))return false;
        const auto* pd=GetNativePlantData(p);if(!combat.chomper.id)combat.chomper={p->mNetId,pd->instanceToken,0};
        if(!Require(a,z->mHelmHealth>0&&z->mHasHead,"Day275 Chomper cohort did not bind an initially armored/headed106"))return false;
    }
    else
    {
        auto* z=CombatZombie(b,107);auto* iron=CombatZombie(b,108);auto* p=CombatPlantAt(b,3,3,3);
        if(!p||!CombatBind(a,z,107,combat.npc[0])||!CombatBind(a,iron,108,combat.npc[1])||!CombatVisible(a,z)||!CombatVisible(a,iron))return false;
        const auto* pd=GetNativePlantData(p);if(!combat.wall.id)combat.wall={p->mNetId,pd->instanceToken,0};
        if(!Require(a,z->mHelmHealth>0&&iron->mHasHead&&p->mPlantHealth==4000,"Day275 armor/Iron baseline was not observed"))return false;
        combat.previousNutHP=p->mPlantHealth;
    }
    if(!Capture(a,"start")||!Barrier(a,"COMBAT_INITIAL"))return false;
    combat.initial=true;return true;
}
bool CombatDolls(LawnApp* a)
{
    auto* b=a->mBoard;constexpr uint16_t chain[]={21,22,23,2};
    Zombie* current=nullptr;int step=-1;
    for(int i=0;i<4;++i)if(auto* z=CombatZombie(b,chain[i]))
    {if(!Require(a,!current,"Day275 Doll parent and child stayed alive together"))return false;current=z;step=i;}
    if(current)
    {
        if(!Require(a,step>=combat.chainIndex&&step<=combat.chainIndex+1,"Day275 a typed Doll generation was skipped"))return false;
        if(!CombatBind(a,current,chain[step],combat.chain[step]))return false;
        if(step<3&&current->mHelmHealth<current->mHelmMaxHealth)combat.damaged[step]=true;
        if(step>combat.chainIndex)
        {
            if(!Require(a,combat.chainCaptured[step-1],"Day275 Doll phase retired before fresh milestone capture")||
                !Require(a,combat.damaged[step-1]&&combat.chain[step].id!=combat.chain[step-1].id&&
                combat.chain[step].token!=combat.chain[step-1].token,"Day275 Doll replacement lacks genuine prior armor damage/new generation"))return false;
            NetTrace("SMOKE_DAY275_CHAIN role=%s from=%u to=%u oldId=%u oldToken=%u childId=%u childToken=%u childBody=%d childArmor=%d naturalGatling=1",
                Role(a),chain[step-1],chain[step],combat.chain[step-1].id,combat.chain[step-1].token,current->mNetId,
                GetNativeSourceZombieData(current)->instanceToken,current->mBodyHealth,current->mHelmHealth);
            combat.chainIndex=step;
        }
        const std::string frame="chain"+std::to_string(chain[step]);
        if(!CombatVisible(a,current)||!Capture(a,frame.c_str()))return false;
        combat.chainCaptured[step]=true;
    }
    if(combat.chainIndex!=3)return false;
    for(const bool captured:combat.chainCaptured)if(!Require(a,captured,"Day275 final Doll phase retired before fresh milestone capture"))return false;
    return CombatProofBarrier(a,"DOLL_CHAIN",combat.chain.data(),combat.chain.size());
}
bool CombatChomper(LawnApp* a)
{
    auto* b=a->mBoard;auto* p=CombatPlant(b,combat.chomper);auto* target=CombatZombie(b,106);
    if(!Require(a,p!=nullptr,"Day275 genuine source5 Chomper disappeared"))return false;
    if(target&&!CombatBind(a,target,106,combat.npc[0]))return false;
    if(!target&&(p->mState==STATE_CHOMPER_BITING_GOT_ONE||p->mState==STATE_CHOMPER_DIGESTING))combat.chomp=true;
    if(!combat.chomp||!CombatBucket(a))return false;
    if(!Capture(a,"bucket"))return false;
    if(!combat.proofReady)NetTrace("SMOKE_DAY275_CHOMPER role=%s targetId=%u targetToken=%u source5Id=%u source5Token=%u state=%d digest=%d directArmorDrop=exclusive-cohort bucketId=%u bucketToken=%u",
        Role(a),combat.npc[0].id,combat.npc[0].token,combat.chomper.id,combat.chomper.token,int(p->mState),p->mLaunchCounter,combat.bucket.id,combat.bucket.token);
    const Ref proof[]={combat.npc[0],combat.chomper,combat.bucket};
    return CombatProofBarrier(a,"CHOMP",proof,std::size(proof));
}
bool CombatArmorIron(LawnApp* a)
{
    auto* b=a->mBoard;auto* nut=CombatPlant(b,combat.wall);auto* z=CombatZombie(b,107);auto* iron=CombatZombie(b,108);
    if(!Require(a,nut!=nullptr,"Day275 hostile11 target WallNut disappeared"))return false;
    if(z)
    {
        if(!CombatBind(a,z,107,combat.npc[0]))return false;
        if(!z->mHelmHealth&&z->mBodyHealth>0)
        {
            if(!combat.armor107)NetTrace("SMOKE_DAY275_ARMOR role=%s kind=107 id=%u token=%u helm=0 body=%d sourceArmorStage=%u naturalProjectileDamage=1",
                Role(a),z->mNetId,GetNativeSourceZombieData(z)->instanceToken,z->mBodyHealth,unsigned(GetNativeDayZombie273(z)->armorStage));
            combat.armor107=true;if(!CombatVisible(a,z)||!Capture(a,"armor107"))return false;
            combat.armorCaptured=true;
        }
    }
    else if(combat.armor107)
    {
        if(!Require(a,combat.armorCaptured,"Day275 source107 died before fresh armor-break capture"))return false;
        if(auto* dying=CombatZombieRef(b,combat.npc[0]);dying&&dying->mBodyHealth==0&&dying->IsDeadOrDying())
        {
            if(!combat.dead107)NetTrace("SMOKE_DAY275_DEATH role=%s kind=107 id=%u token=%u body=0 phase=%d dead=%d naturalProjectileDamage=1",
                Role(a),dying->mNetId,combat.npc[0].token,int(dying->mZombiePhase),dying->mDead);
            combat.dead107=true;
        }
    }
    if(iron)
    {
        if(!CombatBind(a,iron,108,combat.npc[1]))return false;
        if(!iron->mHasHead)
        {
            if(!combat.headless108)NetTrace("SMOKE_DAY275_HEADLOSS role=%s kind=108 id=%u token=%u body=%d hasHead=0 naturalProjectileDamage=1",
                Role(a),iron->mNetId,GetNativeSourceZombieData(iron)->instanceToken,iron->mBodyHealth);
            combat.headless108=true;
        }
    }
    Projectile* shot=nullptr;while(b->IterateProjectiles(shot))if(const auto* d=GetNativeProjectileData(shot);
        d&&!shot->mDead&&shot->mNetId&&d->kind==11&&d->damage==80&&d->isZombieBullet&&d->specialType==9&&shot->mRow==3&&
        NativeDay273ProjectileMetadataValid(*d,shot->mProjectileType,shot->mMotionType))combat.seenIronShot=shot->mNetId;
    if(nut->mPlantHealth<combat.previousNutHP)
    {
        const int damage=combat.previousNutHP-nut->mPlantHealth;
        const bool contact=iron&&GetRectOverlap(iron->GetZombieAttackRect(),nut->GetPlantRect())>0;
        if(!Require(a,combat.seenIronShot&&damage>0&&damage%80==0&&!contact,
            "Day275 WallNut damage cannot be attributed exclusively to genuine hostile11"))return false;
        combat.hit11=true;
        NetTrace("SMOKE_DAY275_HIT role=%s sourceKind=108 projectileKind=11 projectileId=%u targetId=%u targetToken=%u before=%d after=%d delta=%d nativeBiteContact=0 exclusiveHostileShooter=1",
            Role(a),combat.seenIronShot,combat.wall.id,combat.wall.token,combat.previousNutHP,nut->mPlantHealth,damage);
        if(!Capture(a,"hit11"))return false;
        combat.hitCaptured=true;
    }
    combat.previousNutHP=nut->mPlantHealth;
    if(!combat.armorCaptured||!combat.dead107||!combat.headless108||!combat.hitCaptured||!CombatBucket(a))return false;
    if(!Capture(a,"bucket"))return false;
    const Ref proof[]={combat.npc[0],combat.npc[1],combat.wall,combat.bucket};
    return CombatProofBarrier(a,"IRON",proof,std::size(proof));
}
void CombatCompletion(LawnApp* a)
{
    if(combat.completionLogged)return;combat.completionLogged=true;
    NetTrace("SMOKE_DAY275_COHORT_COMPLETE role=%s cohort=%d source1=1 tutorialActive=1 wave=0 actorWritesAfterBirth=0",Role(a),combat.cohort);
}
bool CombatUpdate(LawnApp* a)
{
    if(!combat.diagnostic||GetTickCount()-combat.diagnostic>=2000)
    {
        combat.diagnostic=GetTickCount();auto* b=a->mBoard;auto* p=CombatPlant(b,combat.chomper);auto* nut=CombatPlant(b,combat.wall);
        NetTrace("SMOKE_DAY275_WAIT role=%s cohort=%d initial=%d chain=%d armor107=%d armorFrame=%d dead107=%d headless108=%d hit11=%d hitFrame=%d chomp=%d chompState=%d nutHP=%d bucketId=%u proof=%d",
            Role(a),combat.cohort,combat.initial,combat.chainIndex,combat.armor107,combat.armorCaptured,combat.dead107,combat.headless108,
            combat.hit11,combat.hitCaptured,combat.chomp,p?int(p->mState):-1,nut?nut->mPlantHealth:-1,combat.bucket.id,combat.proofReady);
        Zombie* z=nullptr;while(b->IterateZombies(z))if(const auto* d=GetNativeSourceZombieData(z))
            NetTrace("SMOKE_DAY275_ACTOR role=%s cohort=%d kind=%u id=%u token=%u body=%d helm=%d hasHead=%d dying=%d",
                Role(a),combat.cohort,d->kind,z->mNetId,d->instanceToken,z->mBodyHealth,z->mHelmHealth,z->mHasHead,z->IsDeadOrDying());
    }
    if(!CombatInitial(a))return true;
    const bool done=combat.cohort==0?CombatDolls(a):combat.cohort==1?CombatChomper(a):CombatArmorIron(a);
    if(!done)return true;
    CombatCompletion(a);
    if(combat.cohort==2)
    {
        NetTrace("SMOKE_DAY275_COMPLETE role=%s DollChain=21,22,23,2 source106ChomperBucket=1 source107ArmorBreakDeath=1 source108HeadLossBucket=1 hostile11PlantDamage=1 PoleJump=SeparateScenario fullDayAI=0 initialFactoriesOnly=1 postBirthActorWrites=0 clockWrites=0",
            Role(a));a->CoopSmokeWriteResult(true,nullptr);return true;
    }
    combat.oldEpoch=s.epoch;Advance(a,20);return true;
}
bool CombatRestart(LawnApp* a)
{
    const auto* context=GetNativeContext(a->mBoard);
    if(context&&context->epoch!=combat.oldEpoch)
    {
        if(!Require(a,combat.proofReady&&!combat.proofTag.empty(),"Day275 source epoch changed before local combat proof"))return true;
        if(!Has(a,0,combat.proofTag)||!Has(a,1,combat.proofTag))return true;
        CombatCompletion(a);
        const int next=combat.cohort+1;combat=CombatState{};combat.cohort=next;
        s.waiting=0;s.playingFrame=-1;s.epoch=0;s.born=s.bound=s.moved=s.pause=false;s.actors={};s.diagnostic=0;
        Advance(a,1);return true;
    }
    if(a->NetIsHost()&&!combat.restarted)
    {combat.restarted=true;Require(a,a->StartFusionSourceAdventure(1,false),"Day275 genuine source1 fresh-cohort restart failed");}
    return true;
}

// One source world, two factory setup phases and genuine native gameplay.
// No acquisition/full-level claim: source1 tutorial naturally holds wave0.
constexpr std::array<uint16_t,3> grassKinds={5,6,9};
constexpr std::array<int,3> grassRows={0,2,4};
bool GrassPreflight(LawnApp* a)
{
    if(grass.resources)return true;
    auto* b=a->mBoard;std::string error;
    for(uint16_t kind:{uint16_t(0),uint16_t(3),uint16_t(1027),uint16_t(1032)})
    {
        if(!EnsureNativePlantVisuals(b,kind,&error))
        {NetTrace("SMOKE_GRASS284_RESOURCE role=%s kind=%u error=%s",Role(a),kind,error.c_str());return Require(a,false,"Grass284 plant resource unavailable");}
        const auto seed=GetNativeSeedForPlant(kind);
        if(!Require(a,seed!=SEED_NONE,"Grass284 plant carrier unavailable"))return false;
        ReanimationPreload(GetPlantDefinition(seed).mReanimationType);
    }
    for(uint16_t kind:{uint16_t(5),uint16_t(6),uint16_t(7),uint16_t(9)})
    {
        if(!EnsureNativeSourceZombieVisuals(b,kind,&error))
        {NetTrace("SMOKE_GRASS284_RESOURCE role=%s kind=%u error=%s",Role(a),kind,error.c_str());return Require(a,false,"Grass284 source model unavailable");}
        ReanimationPreload(GetZombieDefinition(ZombieType(GetNativeSourceZombieCarrier(kind))).mReanimationType);
    }
    constexpr std::array<uint16_t,7> peak={5,6,9,7,7,7,7};
    if(a->NetIsHost()&&!CanAllocateNativeSourceDayBatch274(b,peak,&error))
    {NetTrace("SMOKE_GRASS284_CAPACITY role=%s error=%s",Role(a),error.c_str());return Require(a,false,"Grass284 genuine seven-body peak rejected");}
    grass.resources=true;return true;
}
void GrassAdvance(LawnApp* a,int stage)
{
    grass.pauseRequested=grass.resumeRequested=false;s.pauseSamples=0;Advance(a,stage);
}
bool GrassPaused(LawnApp* a)
{
    if(a->NetIsHost()&&!grass.pauseRequested)grass.pauseRequested=Escape(a);
    if(!a->mBoard->mPaused)return false;
    if(!s.pauseSamples){s.pauseSamples=Samples(a);return false;}
    return Samples(a)-s.pauseSamples>=2;
}
bool GrassResumed(LawnApp* a)
{
    if(a->NetIsHost()&&!grass.resumeRequested)grass.resumeRequested=Escape(a);
    return !a->NetIsHost()||grass.resumeRequested;
}
bool GrassSetup(LawnApp* a)
{
    if(!a->NetIsHost()||grass.setup)return true;
    auto* b=a->mBoard;
    if(!Require(a,!b->mPlants.mSize&&!b->mZombies.mSize&&!b->mPaused&&b->mSunMoney==500,
        "Grass284 setup requires empty genuine source opening"))return false;
    for(size_t i=0;i<3;++i)
    {
        // Paper's slow calm walk must contact before unarmed backup runners
        // reach the left edge. Dance has a later blocker and room for ±1.5 ROOTs.
        const float birthX=i==0?500.f:i==1?650.f:600.f;
        if(!CombatPlantBirth(a,i==1?3:1027,i==1?5:4,grassRows[i])||
            !CombatZombieBirth(a,grassKinds[i],grassRows[i],birthX))return false;
    }
    grass.setup=true;return true;
}
bool GrassNodes(LawnApp* a,Zombie* z,bool trace)
{
    float rx=0,ry=0,sx=0,sy=0;
    const bool root=GetNativeSourceZombie269NodePoint(z,"",rx,ry),shadow=GetNativeSourceZombie269NodePoint(z,"Shadow",sx,sy);
    const float ox=float(a->mBoard->mX+a->mDDInterface->mWideScreenOffsetX),oy=float(a->mBoard->mY+a->mDDInterface->mWideScreenOffsetY);
    const bool visible=root&&shadow&&(std::min)(rx+ox,sx+ox)>=24&&(std::max)(rx+ox,sx+ox)<=a->mWidth-48&&
        (std::min)(ry+oy,sy+oy)>=0&&(std::max)(ry+oy,sy+oy)<=a->mHeight-6;
    if(trace||!visible)
    {
        const auto* d=GetNativeSourceZombieData(z);
        NetTrace("SMOKE_GRASS284_ANCHORS role=%s kind=%u id=%u token=%u row=%d rootOkay=%d shadowOkay=%d carrier=(%.3f,%.3f) root=(%.3f,%.3f) shadow=(%.3f,%.3f) screenRoot=(%.3f,%.3f) screenShadow=(%.3f,%.3f) viewport=%dx%d visible=%d",
            Role(a),d?d->kind:0xffff,z->mNetId,d?d->instanceToken:0,z->mRow,root,shadow,z->mPosX,z->mPosY,
            rx,ry,sx,sy,rx+ox,ry+oy,sx+ox,sy+oy,a->mWidth,a->mHeight,visible);
    }
    return Require(a,visible,"Grass284 actual model ROOT/Shadow is unavailable or outside capture");
}
bool GrassInitial(LawnApp* a)
{
    auto* b=a->mBoard;
    for(size_t i=0;i<3;++i)
    {
        auto* z=CombatZombie(b,grassKinds[i]);auto* p=CombatPlantAt(b,i==1?3:1027,i==1?5:4,grassRows[i]);
        if(!z||!p)return false;
        const auto* d=GetNativeSourceZombieData(z);const auto* pd=GetNativePlantData(p);const auto* profile=FindFusionZombieProfile(grassKinds[i]);
        auto* body=a->ReanimationTryToGet(z->mBodyReanimID);
        if(!Require(a,d&&pd&&profile&&body&&z->mNetId&&d->instanceToken&&p->mNetId&&pd->instanceToken&&
            z->mRow==grassRows[i]&&!z->mMindControlled&&z->mFromWave==0&&z->mZombieType==GetNativeSourceZombieCarrier(grassKinds[i])&&
            z->mBodyHealth==profile->maxHealth&&z->mBodyMaxHealth==profile->maxHealth&&
            z->mHelmHealth==profile->firstArmor&&z->mShieldHealth==profile->secondArmor&&
            z->mHasHead&&z->mHasArm&&p->mPlantHealth==p->mPlantMaxHealth&&pd->owner==LawnNet::NET_SHARED_OWNER,
            "Grass284 actual initial identity/HP/armor differs"))return false;
        grass.actors[i]={z->mNetId,d->instanceToken,z->mPosX};grass.walls[i]={p->mNetId,pd->instanceToken,0};
        grass.actorUID[i]=b->mZombies.DataArrayGetID(z);grass.wallHP[i]=p->mPlantHealth;
        grass.bodyFrame[i]=body->mFrameStart;grass.bodyLoops[i]=body->mLoopCount;grass.bodyTime[i]=body->mAnimTime;
        if(!GrassNodes(a,z,true))return false;
        NetTrace("SMOKE_GRASS284_BIND role=%s kind=%u id=%u token=%u localUID=%u row=%d body=%d helm=%d shield=%d wallId=%u wallToken=%u wallHP=%d",
            Role(a),grassKinds[i],z->mNetId,d->instanceToken,grass.actorUID[i],z->mRow,z->mBodyHealth,z->mHelmHealth,z->mShieldHealth,
            p->mNetId,pd->instanceToken,p->mPlantHealth);
    }
    const auto* pole=GetNativeDayZombie273(CombatZombieRef(b,grass.actors[1]));
    if(!Require(a,pole&&pole->status==2&&pole->shadowEnabled,"Grass284 Dance initial status is not2"))return false;
    grass.run=grass.initial=true;return true;
}
bool GrassProof(LawnApp* a,const char* name,bool canonical)
{
    // The peer can resume only after our declared local proof. Preserve that
    // proof while its chat echo arrives; never recompute moving live clocks.
    if(const auto found=grass.proofs.find(name);found!=grass.proofs.end())return Barrier(a,found->second);
    LawnNet::NetBuffer out;
    for(size_t i=0;i<3;++i)
    {
        out.WriteU16(grassKinds[i]);out.WriteU16(grass.actors[i].id);out.WriteU32(grass.actors[i].token);
        out.WriteU16(grass.walls[i].id);out.WriteU32(grass.walls[i].token);
        if(canonical)
        {
            auto* z=CombatZombieRef(a->mBoard,grass.actors[i]);out.WriteU8(z?1:0);
            if(z)
            {
                out.WriteI32(z->mBodyHealth);out.WriteI32(z->mHelmHealth);out.WriteI32(z->mShieldHealth);
                out.WriteU8(z->mHasHead);out.WriteU8(z->mHasArm);
                LawnNet::NetZombieAnimState state;LawnNet::NetBuffer body;
                if(!Require(a,CaptureHead(a->ReanimationTryToGet(z->mBodyReanimID),state,body),"Grass284 actual paused native body invalid"))return false;
                out.mData.insert(out.mData.end(),body.mData.begin(),body.mData.end());
            }
        }
    }
    for(const auto& r:grass.children){out.WriteU16(r.id);out.WriteU32(r.token);}
    char hash[16];sprintf_s(hash,"%08x",LawnNet::NetFnv1a32(out.Data(),out.Size()));
    const auto value=std::string("GRASS_")+name+"_"+hash;grass.proofs.emplace(name,value);
    NetTrace("SMOKE_GRASS284_PROOF role=%s phase=%s hash=%s canonicalPausedBody=%d localProofBeforeReceipt=1",Role(a),name,hash,canonical);
    return Barrier(a,value);
}
bool GrassAnchor(LawnApp* a,const NativeDayZombie273State* pole)
{
    const auto prefix=Tag("GRASS_ANCHOR_");
    if(a->NetIsHost()&&!grass.anchorKnown)
    {
        if(!pole||pole->status!=0||!grass.jump)return false;
        if(!Require(a,!pole->danceSpawnFailed&&pole->danceSpawnRow==2&&std::isfinite(pole->danceSpawnShadowX),
            "Grass284 source JumpOver birth batch failed or anchor differs"))return false;
        grass.anchorRow=pole->danceSpawnRow;grass.birthAnchor=pole->danceSpawnShadowX;grass.anchorKnown=true;
    }
    if(a->NetIsHost())
    {
        char number[32];sprintf_s(number,"%.3f",grass.birthAnchor);return Send(a,prefix+number);
    }
    for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==0&&line.mText.starts_with(prefix))
    {
        const auto number=line.mText.substr(prefix.size());char* end=nullptr;
        const float value=std::strtof(number.c_str(),&end);
        if(!Require(a,end&&end!=number.c_str()&&!*end&&std::isfinite(value),"Grass284 host event-anchor proof malformed"))return false;
        grass.birthAnchor=value;grass.anchorRow=2;grass.anchorKnown=true;return true;
    }
    return false;
}
bool GrassChildren(LawnApp* a)
{
    auto* b=a->mBoard;std::vector<Zombie*> found;Zombie* z=nullptr;
    while(b->IterateZombies(z))if(const auto* d=GetNativeSourceZombieData(z);d&&d->kind==7)found.push_back(z);
    if(!Require(a,found.size()<=4,"Grass284 Dance emitted duplicate backup7"))return false;
    if(found.size()!=4)return false;
    std::sort(found.begin(),found.end(),[](const Zombie* x,const Zombie* y){return x->mNetId<y->mNetId;});
    std::array<int,5> rows{};std::array<float,2> same{};size_t sameCount=0;
    for(size_t i=0;i<4;++i)
    {
        z=found[i];const auto* d=GetNativeSourceZombieData(z);const auto* day=GetNativeDayZombie273(z);
        auto* body=a->ReanimationTryToGet(z->mBodyReanimID);float rx=0,ry=0;
        if(!Require(a,d&&day&&body&&z->mNetId&&d->instanceToken&&z->mRow>=1&&z->mRow<=3&&!z->mMindControlled&&
            z->mFromWave==0&&!d->sourceParentId&&!d->sourceParentToken&&!d->dieWithParent&&z->mBodyHealth==270&&
            z->mBodyMaxHealth==270&&!z->mHelmHealth&&!z->mShieldHealth&&z->mZombieType==GetNativeSourceZombieCarrier(7)&&
            NativeDayZombie273ControllerValid(7,*day)&&GetNativeSourceZombie269NodePoint(z,"",rx,ry),
            "Grass284 backup7 source birth identity/hostility/controller differs"))return false;
        ++rows[size_t(z->mRow)];
        if(!grass.childrenBound)
        {
            if(z->mRow==2)
            {if(!Require(a,sameCount<2,"Grass284 too many same-row backups"))return false;same[sameCount++]=rx;}
            else if(!Require(a,std::abs(rx-grass.birthAnchor)<=32,"Grass284 adjacent backup ROOT is not source eventShadowX"))return false;
            grass.children[i]={z->mNetId,d->instanceToken,z->mPosX};grass.childTime[i]=body->mAnimTime;grass.childLoops[i]=body->mLoopCount;
            for(size_t j=0;j<3;++j)if(!Require(a,z->mNetId!=grass.actors[j].id&&d->instanceToken!=grass.actors[j].token,
                "Grass284 backup reused a parent generation"))return false;
            for(size_t j=0;j<i;++j)if(!Require(a,d->instanceToken!=grass.children[j].token,"Grass284 backup tokens duplicated"))return false;
            NetTrace("SMOKE_GRASS284_CHILD role=%s index=%zu id=%u token=%u localUID=%u row=%d root=(%.3f,%.3f) acceptedEventShadow=%.3f sameRowOffset=%.3f hostile=1 parentLink=0 birthProjectionTolerance=32 postBirthWrites=0",
                Role(a),i,z->mNetId,d->instanceToken,b->mZombies.DataArrayGetID(z),z->mRow,rx,ry,grass.birthAnchor,rx-grass.birthAnchor);
            if(!GrassNodes(a,z,true))return false;
        }
        else
        {
            if(!Require(a,z->mNetId==grass.children[i].id&&d->instanceToken==grass.children[i].token,
                "Grass284 backup set changed or duplicated"))return false;
            grass.childMoved[i]=grass.childMoved[i]||std::abs(z->mPosX-grass.children[i].x)>=2;
            grass.childAnimated[i]=grass.childAnimated[i]||body->mAnimTime!=grass.childTime[i]||body->mLoopCount!=grass.childLoops[i];
        }
    }
    if(!Require(a,rows[1]==1&&rows[2]==2&&rows[3]==1,"Grass284 source interior Dance backup rows differ"))return false;
    if(!grass.childrenBound)
    {
        std::sort(same.begin(),same.end());const float offset=float(1.5*kFusionBasePixelsX);
        if(!Require(a,sameCount==2&&std::abs(same[0]-(grass.birthAnchor-offset))<=32&&
            std::abs(same[1]-(grass.birthAnchor+offset))<=32,"Grass284 same-row backup ROOT is not eventShadowX plus/minus1.5"))return false;
        grass.childrenBound=true;grass.childSamples=Samples(a);return false;
    }
    return Samples(a)-grass.childSamples>=20&&std::all_of(grass.childMoved.begin(),grass.childMoved.end(),[](bool v){return v;})&&
        std::all_of(grass.childAnimated.begin(),grass.childAnimated.end(),[](bool v){return v;});
}
bool GrassApproach(LawnApp* a)
{
    auto* b=a->mBoard;
    for(size_t i=0;i<3;++i)
    {
        auto* z=CombatZombieRef(b,grass.actors[i]);auto* p=CombatPlant(b,grass.walls[i]);
        if(!Require(a,z&&p&&!z->IsDeadOrDying(),"Grass284 passive precombat actor retired"))return false;
        auto* body=a->ReanimationTryToGet(z->mBodyReanimID);
        if(!Require(a,body!=nullptr,"Grass284 native body missing"))return false;
        grass.moved[i]=grass.moved[i]||std::abs(z->mPosX-grass.actors[i].x)>=2;
        grass.animated[i]=grass.animated[i]||body->mFrameStart!=grass.bodyFrame[i]||body->mLoopCount!=grass.bodyLoops[i]||body->mAnimTime!=grass.bodyTime[i];
        if(i!=1&&p->mPlantHealth<grass.wallHP[i])
        {
            const int damage=grass.wallHP[i]-p->mPlantHealth;const size_t index=i==0?0:1;const auto* d=GetNativeSourceZombieData(z);
            if(!Require(a,damage>0&&damage%50==0&&z->mIsEating&&d->targetEntity==NativeZombieTargetEntity::Plant&&d->targetId==p->mNetId,
                "Grass284 passive blocker damage is not genuine exclusive50 bite"))return false;
            if(!grass.contact[index])NetTrace("SMOKE_GRASS284_CONTACT role=%s kind=%u id=%u token=%u targetId=%u targetToken=%u before=%d after=%d damage=%d sourceBite50=1 weaponsPresent=0",
                Role(a),grassKinds[i],z->mNetId,grass.actors[i].token,p->mNetId,grass.walls[i].token,grass.wallHP[i],p->mPlantHealth,damage);
            grass.contact[index]=true;
        }
        grass.wallHP[i]=p->mPlantHealth;
    }
    auto* parent=CombatZombieRef(b,grass.actors[1]);const auto* pole=GetNativeDayZombie273(parent);bool shadow=false;
    if(!Require(a,pole&&NativeDayZombie273ControllerValid(6,*pole)&&
        GetNativeSourceZombie269NodeActivity(parent,"Shadow",shadow)&&shadow==pole->shadowEnabled,
        "Grass284 Dance actual Shadow/controller differs"))return false;
    if(pole->status==3)
    {
        if(!Require(a,grass.run&&!grass.land&&!shadow,"Grass284 Dance2->3 transition skipped"))return false;
        grass.jump=true;if(!GrassNodes(a,parent,false)||!Capture(a,"jump"))return false;
    }
    if(pole->status==0)
    {
        if(!Require(a,grass.jump&&shadow,"Grass284 Dance natural jump/landing was not observed"))return false;
        grass.land=true;
    }
    if(!grass.land||!GrassAnchor(a,pole)||!GrassChildren(a))return false;
    return grass.contact[0]&&grass.contact[1]&&std::all_of(grass.moved.begin(),grass.moved.end(),[](bool v){return v;})&&
        std::all_of(grass.animated.begin(),grass.animated.end(),[](bool v){return v;})&&s.frames.contains("fusion_grass284_jump");
}
bool GrassWeapons(LawnApp* a,bool foot)
{
    auto& ready=foot?grass.footWeapons:grass.firstWeapons;
    if(!a->NetIsHost()||ready)return true;
    if(!Require(a,a->mBoard->mPaused,"Grass284 second factory setup requires actual pause"))return false;
    if(foot)
    {for(int col=0;col<2;++col)if(!CombatPlantBirth(a,1032,col,4))return false;}
    else
    {
        if(!CombatPlantBirth(a,0,0,0))return false;
        for(int row=1;row<=3;++row)if(!CombatPlantBirth(a,1032,0,row))return false;
    }
    ready=true;NetTrace("SMOKE_GRASS284_WEAPONS phase=%s actualFactoriesOnly=1 acquisition=NotExecuted HPWrites=0 clockWrites=0",foot?"football":"paper-and-backups");return true;
}
bool GrassBattle(LawnApp* a)
{
    auto* b=a->mBoard;Zombie* actor=nullptr;
    while(b->IterateZombies(actor))
    {
        const auto* d=GetNativeSourceZombieData(actor);if(!Require(a,d!=nullptr,"Grass284 untyped actor appeared"))return false;
        bool known=false;for(const auto& r:grass.actors)known=known||(actor->mNetId==r.id&&d->instanceToken==r.token);
        for(const auto& r:grass.children)known=known||(actor->mNetId==r.id&&d->instanceToken==r.token);
        if(!Require(a,known,"Grass284 extra actor or duplicate Dance generation appeared after combat"))return false;
    }
    for(size_t i=0;i<2;++i)
    {
        const size_t index=i==0?0:2;auto* z=CombatZombieRef(b,grass.actors[index]);
        if(z)
        {
            if(i==0)
            {
                int status=-1;if(!Require(a,GetNativeSpecialZombie269Status(z,status),"Grass284 Paper controller missing"))return false;
                if(status==5)grass.fall=true;
                if(status==6)
                {
                    if(!Require(a,grass.fall,"Grass284 source Paper LosePaper phase was skipped"))return false;
                    if(!grass.rage)NetTrace("SMOKE_GRASS284_RAGE role=%s kind=5 id=%u token=%u status=6 body=%d shield=%d naturalProjectiles=1",Role(a),z->mNetId,grass.actors[0].token,z->mBodyHealth,z->mShieldHealth);
                    grass.rage=true;
                }
            }
            const bool lost=i==0?z->mShieldType==SHIELDTYPE_NONE:z->mHelmType==HELMTYPE_NONE;
            if(lost&&!grass.armor[i])NetTrace("SMOKE_GRASS284_ARMOR role=%s kind=%u id=%u token=%u body=%d helm=%d shield=%d stage=%u naturalProjectiles=1",Role(a),i==0?5:9,z->mNetId,grass.actors[index].token,z->mBodyHealth,z->mHelmHealth,z->mShieldHealth,unsigned(GetNativeSourceZombieData(z)->firstArmorStage));
            grass.armor[i]=grass.armor[i]||lost;grass.arm[i]=grass.arm[i]||!z->mHasArm;grass.head[i]=grass.head[i]||!z->mHasHead;
        }
        if(!b->mZombies.DataArrayTryToGet(grass.actorUID[index])&&!grass.retired[i])
        {
            grass.retired[i]=true;
            NetTrace("SMOKE_GRASS284_RETIRED role=%s kind=%u id=%u token=%u localUID=%u actualDataArrayAbsent=1 armorObserved=%d armObserved=%d headObserved=%d",Role(a),i==0?5:9,grass.actors[index].id,grass.actors[index].token,grass.actorUID[index],grass.armor[i],grass.arm[i],grass.head[i]);
        }
    }
    Projectile* shot=nullptr;while(b->IterateProjectiles(shot))if(const auto* d=GetNativeProjectileData(shot);
        d&&!shot->mDead&&d->kind==0&&d->damage==20&&!d->isZombieBullet&&(shot->mRow==0||shot->mRow==4))
    {
        const size_t i=shot->mRow==0?0:1;if(!grass.shot[i])NetTrace("SMOKE_GRASS284_SHOT role=%s row=%d id=%u kind=0 damage=20 hostile=0 genuineNativeWeapon=1",Role(a),shot->mRow,shot->mNetId);
        grass.shot[i]=true;
    }
    return true;
}
bool GrassUpdate(LawnApp* a)
{
    auto* b=a->mBoard;
    if(!grass.diagnostic||GetTickCount()-grass.diagnostic>=3000)
    {
        grass.diagnostic=GetTickCount();auto* p=CombatZombieRef(b,grass.actors[0]);auto* d=CombatZombieRef(b,grass.actors[1]);auto* f=CombatZombieRef(b,grass.actors[2]);
        const auto* day=GetNativeDayZombie273(d);
        NetTrace("SMOKE_GRASS284_WAIT role=%s stage=%d paused=%d tick=%d snapshots=%u danceStatus=%d jump=%d land=%d children=%d contact=%d/%d paper=(%d,%d) fall=%d rage=%d football=(%d,%d) armor=%d/%d retire=%d/%d",
            Role(a),s.stage,b->mPaused,b->mMainCounter,Samples(a),day?int(day->status):-1,grass.jump,grass.land,grass.childrenBound,
            grass.contact[0],grass.contact[1],p?p->mBodyHealth:-1,p?p->mShieldHealth:-1,grass.fall,grass.rage,f?f->mBodyHealth:-1,f?f->mHelmHealth:-1,
            grass.armor[0],grass.armor[1],grass.retired[0],grass.retired[1]);
    }
    if(s.stage==30)
    {
        if(!GrassPaused(a))return true;
        if(!grass.initial&&!GrassInitial(a))return true;
        if(!Capture(a,"cohort")||!GrassProof(a,"INITIAL",true)||!GrassResumed(a))return true;
        GrassAdvance(a,31);
    }
    else if(s.stage==31)
    {
        if(!GrassApproach(a)||!GrassProof(a,"PASSIVE",false))return true;
        GrassAdvance(a,32);
    }
    else if(s.stage==32)
    {
        if(!GrassPaused(a)||!Capture(a,"backups")||!GrassProof(a,"BACKUPS",true)||!GrassWeapons(a,false)||!GrassResumed(a))return true;
        NetTrace("SMOKE_GRASS284_BACKUPS role=%s source6=1 observedStatus=2,3,0 count=4 kind=7 rows=1,2,2,3 fullGenerations=4 hostile=1 sourceParentLink=0 ROOTProjection=1 movement=4 animations=4 minimumSnapshots=20 duplicateChildren=0",Role(a));
        GrassAdvance(a,33);
    }
    else if(s.stage==33)
    {
        if(!GrassBattle(a)||!grass.rage||!grass.shot[0]||!GrassProof(a,"RAGE",false))return true;
        GrassAdvance(a,34);
    }
    else if(s.stage==34)
    {
        if(!GrassPaused(a)||!GrassProof(a,"FOOT_SETUP",true)||!GrassWeapons(a,true)||!GrassResumed(a))return true;
        GrassAdvance(a,35);
    }
    else if(s.stage==35)
    {
        if(!GrassBattle(a)||!grass.armor[1]||!grass.shot[1])return true;
        // Freeze the genuine armor edge before a fast Gatling can retire Foot9.
        // The client proves the authoritative paused edge after raw adoption.
        if(a->NetIsHost()&&!grass.pauseRequested)grass.pauseRequested=Escape(a);
        if(!b->mPaused)return true;
        GrassAdvance(a,36);grass.pauseRequested=a->NetIsHost();
    }
    else if(s.stage==36)
    {
        if(!GrassPaused(a)||!GrassBattle(a)||!Capture(a,"armor")||!GrassProof(a,"ARMOR",true)||!GrassResumed(a))return true;
        GrassAdvance(a,37);
    }
    else if(s.stage==37)
    {
        if(!GrassBattle(a)||!grass.retired[0]||!grass.retired[1])return true;
        if(!Require(a,grass.armor[0]&&grass.armor[1]&&grass.rage&&grass.arm[0]&&grass.head[0],
            "Grass284 Paper retired before its genuine armor/rage/arm/head proof"))return true;
        if(!GrassProof(a,"RETIRED",false))return true;
        GrassAdvance(a,38);
    }
    else if(s.stage==38)
    {
        if(!GrassPaused(a)||!Capture(a,"paused"))return true;
        if(!grass.logged)
        {
            grass.logged=true;
            NetTrace("SMOKE_GRASS284_COMPLETE role=%s source1=1 tutorialActive=1 wave=0 primaries=5,6,9 backup7=4 motionAnimations=7 contact50=5,9 jump=2,3,0 sourceROOTBirths=1 paperLossRage=1 footballHelmetLoss=1 actualRetired=5,9 reliableParts=RunnerChecks fullCampaign=0 Jackson10=NotExecuted ownerAbsentReliableReplay=NotExecuted setupFactoriesOnly=1 HPWrites=0 clockWrites=0 RNGWrites=0",
                Role(a));
        }
        if(GrassProof(a,"COMPLETE",true))a->CoopSmokeWriteResult(true,nullptr);
    }
    return true;
}

bool JacksonPrepare(LawnApp* a)
{
    if(jackson.prepared)return true;auto* b=a->mBoard;std::string error;
    for(uint16_t kind:{uint16_t(3),uint16_t(1027),uint16_t(1032)})
    {
        if(!Require(a,EnsureNativePlantVisuals(b,kind,&error),"Jackson291 passive/shooter source models unavailable"))return false;
        ReanimationPreload(GetPlantDefinition(GetNativeSeedForPlant(kind)).mReanimationType);
    }
    for(uint16_t kind:{uint16_t(0),uint16_t(2),uint16_t(10),uint16_t(6),uint16_t(7)})
        if(!Require(a,EnsureNativeSourceZombieVisuals(b,kind,&error),"Jackson291 actual source NPC models unavailable"))return false;
    if(!Require(a,EnsureNativeSourceEffect269(b,11,&error),"Jackson291 source cloud11 unavailable"))return false;
    std::vector<uint16_t> peak={10,6,6,6,6};peak.insert(peak.end(),16,7);
    if(a->NetIsHost()&&!CanAllocateNativeSourceZombieBatch280(b,peak,&error))
    {NetTrace("SMOKE_JACKSON291_RESOURCE error=%s",error.c_str());return Require(a,false,"Jackson291 genuine whole closure peak rejected");}
    if(a->NetIsHost())
    {
        // Entire bounded fixture peak, with no deletion credit: initial nine
        // passive plants + six later Pea carriers + six Tutor10 parent/result
        // slots. PlantInitialize Pea is2R/1A; nuts/Tutor10 are1R/0A, NPC peak21R.
        auto* effects=a->mEffectSystem;
        if(!Require(a,effects&&effects->mReanimationHolder&&effects->mAttachmentHolder&&
            GetNativeSeedForPlant(3)==SEED_WALLNUT&&GetNativeSeedForPlant(1027)==SEED_TALLNUT&&
            GetNativeSeedForPlant(1032)==SEED_PEASHOOTER,"Jackson291 fixture initializer resource contract differs"))return false;
        const auto& plants=b->mPlants;const auto& reanims=effects->mReanimationHolder->mReanimations;
        const auto& attachments=effects->mAttachmentHolder->mAttachments;
        if(!Require(a,plants.mBlock&&reanims.mBlock&&attachments.mBlock&&plants.mSize<=plants.mMaxSize&&
            reanims.mSize<=reanims.mMaxSize&&attachments.mSize<=attachments.mMaxSize&&
            plants.mMaxSize-plants.mSize>=21&&reanims.mMaxSize-reanims.mSize>=48&&
            attachments.mMaxSize-attachments.mSize>=6,"Jackson291 complete geometry/shooter/tutorial peak rejected"))return false;
        NetTrace("SMOKE_JACKSON291_SETUP_CAPACITY plants=21 reanimations=48 attachments=6 deletionCredit=0 initialPassive=9 delayedShooters=6 tutorPeak=6 sourceNPCPeak=21");
    }
    jackson.prepared=true;return true;
}
bool JacksonSetup(LawnApp* a)
{
    if(!a->NetIsHost()||jackson.setup)return true;auto* b=a->mBoard;
    if(!Require(a,!b->mZombies.mSize&&b->mSunMoney==150&&!b->mPaused,"Jackson291 expected fresh real Night10 opening"))return false;
    // Near target gives actual NORMAL contact overlap during Moonwalk; the
    // col4 helper still lets the left same-row Dance perform its own vault.
    for(int row=1;row<=3;++row)if(!CombatPlantBirth(a,3,4,row))return false;
    if(!CombatPlantBirth(a,3,6,2,&jackson.wall))return false;
    for(int row=0;row<5;++row)if(!CombatPlantBirth(a,1027,1,row))return false;
    if(!CombatZombieBirth(a,10,2,600.f,&jackson.parent))return false;
    jackson.setup=true;return true;
}
bool JacksonCollect(LawnApp* a)
{
    auto* b=a->mBoard;auto* holder=a->mEffectSystem?a->mEffectSystem->mParticleHolder:nullptr;
    if(!Require(a,holder!=nullptr,"Jackson291 actual particle holder unavailable"))return false;
    TodParticleSystem* ps=nullptr;
    while(holder->mParticleSystems.IterateNext(ps))if(!ps->mDead)
    {
        uint16_t source=0;if(!NativeSourceEffectKind269(int(ps->mEffectType),source)||source!=11)continue;
        const auto uid=uint32_t(a->ParticleGetID(ps));if(jackson.clouds.contains(uid))continue;
        for(auto* n=ps->mEmitterList.mHead;n;n=n->mNext)if(auto* emitter=holder->mEmitters.DataArrayTryToGet(unsigned(n->mValue));emitter&&!emitter->mDead)
        {jackson.clouds.emplace(uid,JacksonCloud297{emitter->mSystemCenter.x,emitter->mSystemCenter.y,ps->mRenderOrder,false});break;}
    }
    Zombie* z=nullptr;
    while(b->IterateZombies(z))
    {
        const auto* d=GetNativeSourceZombieData(z);if(!Require(a,d!=nullptr,"Jackson291 untyped actor leaked into real source world"))return false;
        if(d->kind!=10&&d->kind!=6&&d->kind!=7)
        {if(!Require(a,z->mFromWave>0&&(d->kind==0||d->kind==2),"Jackson291 unrelated actor is not a genuine Night10 wave NPC"))return false;continue;}
        const auto key=JacksonKey(z->mNetId,d->instanceToken);auto it=jackson.actors.find(key);
        if(it==jackson.actors.end())
        {
            if(!Require(a,z->mNetId&&d->instanceToken&&!z->mMindControlled&&z->mFromWave==0&&!d->dieWithParent&&
                !d->sourceParentId&&!d->sourceParentToken,"Jackson291 real independent child/source identity differs"))return false;
            JacksonActor297 value;value.ref={z->mNetId,d->instanceToken,z->mPosX};value.uid=b->mZombies.DataArrayGetID(z);value.kind=d->kind;
            value.row=z->mRow;value.order=z->mRenderOrder;auto* body=a->ReanimationTryToGet(z->mBodyReanimID);
            if(!Require(a,body&&GetNativeSourceZombie269NodePoint(z,"",value.rootX,value.rootY)&&
                GetNativeSourceZombie269NodePoint(z,"Shadow",value.shadowX,value.shadowY),"Jackson291 genuine birth nodes/body unavailable"))return false;
            value.time=body->mAnimTime;value.loops=body->mLoopCount;value.frame=body->mFrameStart;
            it=jackson.actors.emplace(key,value).first;
            NetTrace("SMOKE_JACKSON291_BIND role=%s kind=%u id=%u token=%u localUID=%u row=%d body=%d/%d root=(%.3f,%.3f) shadow=(%.3f,%.3f) rendererSource=1 parentField=0 independent=1",
                Role(a),d->kind,z->mNetId,d->instanceToken,value.uid,z->mRow,z->mBodyHealth,z->mBodyMaxHealth,value.rootX,value.rootY,value.shadowX,value.shadowY);
            if(d->kind==10)
            {
                if(!Require(a,!jackson.parent.id||(jackson.parent.id==z->mNetId&&jackson.parent.token==d->instanceToken),"Jackson291 duplicate setup Jackson"))return false;
                jackson.parent=value.ref;jackson.parentUID=value.uid;
            }
        }
        auto& record=it->second;auto* body=a->ReanimationTryToGet(z->mBodyReanimID);
        if(body){record.moved=record.moved||std::abs(z->mPosX-record.ref.x)>=2;record.animated=record.animated||body->mFrameStart!=record.frame||body->mLoopCount!=record.loops||body->mAnimTime!=record.time;}
        if(d->kind==6)if(const auto* day=GetNativeDayZombie273(z)){record.jump=record.jump||day->status==3;record.land=record.land||(record.jump&&day->status==0);}
        if(d->kind==10)
        {
            const auto* state=GetNativeJackson291(z);const char* pose=nullptr;float phase=0;
            if(!Require(a,state&&FusionNightZombie281ViewValid(*state)&&GetNativeJackson281Presentation(z,*state,pose,phase),
                "Jackson291 actual controller/native body/source pose differs"))return false;
            if(state->phase==FusionNightZombie281Phase::Death)jackson.death=true;
        }
        if(d->kind==6||d->kind==7)
        {
            for(auto& [uid,cloud]:jackson.clouds)if(!record.cloud&&!cloud.used&&cloud.order==record.order+1&&
                std::abs(cloud.x-(d->kind==6?record.shadowX:record.rootX))<=24&&
                std::abs(cloud.y-((d->kind==6?record.shadowY:record.rootY)-float((d->kind==6?.7:.5)*GetNativeFusionPixelsY(b))))<=8)
            {
                record.cloud=true;cloud.used=true;
                NetTrace("SMOKE_JACKSON291_CHILD_FX role=%s kind=%u id=%u token=%u system=%u sourceParticle=11 birth=(%.3f,%.3f) expected=(%.3f,%.3f) anchor=%s sourceOffset=%.1f",
                    Role(a),d->kind,z->mNetId,d->instanceToken,uid,cloud.x,cloud.y,d->kind==6?record.shadowX:record.rootX,
                    (d->kind==6?record.shadowY:record.rootY)-float((d->kind==6?.7:.5)*GetNativeFusionPixelsY(b)),d->kind==6?"Shadow":"ROOT",d->kind==6?.7:.5);
            }
        }
    }
    return true;
}
bool JacksonProof(LawnApp* a,const char* label,bool canonical)
{
    if(const auto it=jackson.proofs.find(label);it!=jackson.proofs.end())return Barrier(a,it->second);
    LawnNet::NetBuffer bytes;bytes.WriteU16(jackson.parent.id);bytes.WriteU32(jackson.parent.token);
    for(const auto key:jackson.closure){bytes.WriteU16(jackson.actors.at(key).ref.id);bytes.WriteU32(jackson.actors.at(key).ref.token);}
    if(canonical)
    {
        auto* z=CombatZombieRef(a->mBoard,jackson.parent);const auto* view=GetNativeJackson291(z);
        bytes.WriteU8(z?1:0);
        if(z)
        {
            if(!Require(a,view&&FusionNightZombie281ViewValid(*view),"Jackson291 canonical view missing"))return false;
            bytes.WriteU8(uint8_t(view->phase));bytes.WriteU8(uint8_t(view->outgoing));
            bytes.WriteU8(uint8_t(view->lookBack)|(uint8_t(view->moonWalkFinished)<<1)|(uint8_t(view->canAttack)<<2)|(uint8_t(view->loseDancer)<<3));
            bytes.WriteF32(view->outgoingSourceSeconds);bytes.WriteF32(view->transitionElapsed);bytes.WriteF32(view->transitionTotal);
            LawnNet::NetZombieAnimState state;LawnNet::NetBuffer body;
            if(!Require(a,CaptureHead(a->ReanimationTryToGet(z->mBodyReanimID),state,body),"Jackson291 actual paused body invalid"))return false;
            bytes.mData.insert(bytes.mData.end(),body.mData.begin(),body.mData.end());
            NetTrace("SMOKE_JACKSON291_VIEW role=%s phase=%u outgoing=%u sourceSeconds=%.9g elapsed=%.9g total=%.9g flags=%u nativeTime=%.9g nativeFrames=%u+%u",
                Role(a),uint8_t(view->phase),uint8_t(view->outgoing),view->outgoingSourceSeconds,view->transitionElapsed,view->transitionTotal,
                uint8_t(view->lookBack)|(uint8_t(view->moonWalkFinished)<<1)|(uint8_t(view->canAttack)<<2)|(uint8_t(view->loseDancer)<<3),state.mAnimTime,state.mFrameStart,state.mFrameCount);
        }
    }
    char hash[16];sprintf_s(hash,"%08x",LawnNet::NetFnv1a32(bytes.Data(),bytes.Size()));
    const auto value=std::string("J291_")+label+"_"+hash;jackson.proofs.emplace(label,value);
    NetTrace("SMOKE_JACKSON291_PROOF role=%s phase=%s hash=%s localProofBeforeReceipt=1 canonicalView=%d",Role(a),label,hash,canonical);
    return Barrier(a,value);
}
size_t JacksonCount(uint16_t kind)
{size_t n=0;for(const auto& [key,r]:jackson.actors)if(r.kind==kind)++n;return n;}
bool JacksonInitial(LawnApp* a)
{
    auto* z=CombatZombie(a->mBoard,10);auto* p=CombatPlantAt(a->mBoard,3,6,2);
    if(!z||!p)return false;const auto* pd=GetNativePlantData(p);const auto* view=GetNativeJackson291(z);
    const bool initialPose=view&&((view->phase==FusionNightZombie281Phase::Moonwalk&&view->lookBack&&!view->transitionTotal)||
        (view->phase==FusionNightZombie281Phase::Point&&view->outgoing==FusionNightZombie281Phase::Moonwalk&&view->transitionElapsed<=.04f));
    if(!Require(a,initialPose&&z->mBodyMaxHealth==720&&z->mBodyHealth>0&&p->mPlantHealth>0,
        "Jackson291 actual initial Moonwalk/first genuine contact transition differs"))return false;
    jackson.wall={p->mNetId,pd->instanceToken,0};jackson.initialWallHP=p->mPlantHealth;
    if(!GrassNodes(a,z,true))return false;jackson.initial=true;return true;
}
bool JacksonClosure(LawnApp* a,bool complete)
{
    auto* b=a->mBoard;auto* parent=CombatZombieRef(b,jackson.parent);const auto* state=GetNativeJackson291(parent);
    if(!state)return false;
    size_t six=0,sevens=0;bool all=true;std::set<uint32_t> initialUIDs;
    for(const auto& [key,r]:jackson.actors)
    {
        if(r.kind==6)
        {
            ++six;all=all&&r.cloud;
            if(complete)all=all&&r.moved&&r.animated&&r.jump&&r.land;
            initialUIDs.insert(r.uid);
        }
        if(r.kind==7){++sevens;if(complete)all=all&&r.cloud&&r.moved&&r.animated;}
    }
    if(six<4)return false;if(!Require(a,six==4,"Jackson291 unexpected extra refill before initial closure"))return false;
    if(a->NetIsHost())
    {
        std::set<uint32_t> actual;
        for(auto uid:state->dancers)if(uid)actual.insert(uid);
        if(!Require(a,actual==initialUIDs,"Jackson291 initial four-child full-generation source refs differ"))return false;
        if(!complete)
        {
            float x=0,y=0;if(!GetNativeSourceZombie269NodePoint(parent,"Shadow",x,y))return false;
            std::array<int,5> rows{};std::array<float,2> same{};size_t count=0;
            for(const auto& [key,r]:jackson.actors)if(r.kind==6)
            {
                if(!Require(a,r.row>=1&&r.row<=3,"Jackson291 summoned source6 into wrong row"))return false;
                ++rows[size_t(r.row)];
                if(r.row==2){if(!Require(a,count<2,"Jackson291 duplicate same-row source6"))return false;same[count++]=r.rootX;}
                else if(!Require(a,std::abs(r.rootX-x)<=24,"Jackson291 side child ROOT differs from source Shadow"))return false;
            }
            std::sort(same.begin(),same.end());
            if(!Require(a,rows[1]==1&&rows[2]==2&&rows[3]==1&&count==2&&
                std::abs(same[0]-(x-float(1.5*kFusionBasePixelsX)))<=24&&std::abs(same[1]-(x+float(kFusionBasePixelsX)))<=24,
                "Jackson291 actual source6 four-slot ROOT projection differs"))return false;
        }
        if(complete)
        {
            int groups=0;
            for(auto uid:actual)if(auto* z=b->mZombies.DataArrayTryToGet(uid))if(const auto* day=GetNativeDayZombie273(z))
            {if(day->danceSpawnFailed)return Require(a,false,"Jackson291 genuine Dance child batch failed");if(day->danceSpawnRow>=0&&!day->danceSpawnPending)++groups;}
            if(!all||groups==0)return false;
            for(const auto& [key,r]:jackson.actors)if(r.kind==7)
            {
                bool match=false;
                for(auto uid:actual)if(auto* z=b->mZombies.DataArrayTryToGet(uid))if(const auto* day=GetNativeDayZombie273(z);day&&day->danceSpawnRow>=0&&!day->danceSpawnPending)
                {
                    const float anchor=day->danceSpawnShadowX;
                    if(r.row==day->danceSpawnRow)match=match||std::abs(r.rootX-(anchor-float(1.5*kFusionBasePixelsX)))<=24||std::abs(r.rootX-(anchor+float(1.5*kFusionBasePixelsX)))<=24;
                    else if(std::abs(r.row-day->danceSpawnRow)==1)match=match||std::abs(r.rootX-anchor)<=24;
                }
                if(!Require(a,match,"Jackson291 backup ROOT does not match any real accepted Dance event anchor"))return false;
            }
            jackson.groups=groups;
            if(!Send(a,Tag("J291_GROUPS_"+std::to_string(groups))))return false;
        }
    }
    if(complete&&a->NetIsClient())
    {
        const auto prefix=Tag("J291_GROUPS_");
        for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==0&&line.mText.starts_with(prefix))jackson.groups=std::atoi(line.mText.c_str()+prefix.size());
        if(!jackson.groups)return false;
    }
    if(!all||(complete&&sevens<size_t(jackson.groups*4)))return false;
    if(complete&&!Require(a,jackson.groups>=1&&jackson.groups<=4&&sevens==size_t(jackson.groups*4),"Jackson291 duplicate/missing accepted Dance batch births"))return false;
    return true;
}
bool JacksonContact(LawnApp* a)
{
    auto* b=a->mBoard;auto* z=CombatZombieRef(b,jackson.parent);auto* wall=CombatPlant(b,jackson.wall);const auto* d=GetNativeSourceZombieData(z);const auto* view=GetNativeJackson291(z);
    if(!z||!wall||!d||!view)return false;
    const bool eating=view->phase==FusionNightZombie281Phase::EatFromWalk||view->phase==FusionNightZombie281Phase::EatFromRaise;
    if(view->canAttack&&eating&&d->targetEntity==NativeZombieTargetEntity::Plant&&d->targetId==wall->mNetId)jackson.contact=true;
    if(wall->mPlantHealth<jackson.initialWallHP&&jackson.contact)
    {
        if(!jackson.damage)NetTrace("SMOKE_JACKSON291_CONTACT role=%s id=%u token=%u targetId=%u targetToken=%u before=%d after=%d sourceEatState=%u actualHPDamage=1 attribution=HostProductionBiteTrace",
            Role(a),z->mNetId,jackson.parent.token,wall->mNetId,jackson.wall.token,jackson.initialWallHP,wall->mPlantHealth,uint8_t(view->phase));
        jackson.damage=true;
    }
    return jackson.contact&&jackson.damage;
}
bool JacksonUpdate(LawnApp* a)
{
    auto* b=a->mBoard;
    // A missing prerequisite is a terminal fixture failure, never a long wait
    // for a bite from an actor already swept by a naturally triggered mower.
    if(jackson.parentUID&&s.stage<49)
    {
        auto* actual=b->mZombies.DataArrayTryToGet(jackson.parentUID);
        auto* target=jackson.wall.id?CombatPlant(b,jackson.wall):nullptr;
        if(!actual||actual->mDead||actual->IsDeadOrDying()||(jackson.initial&&(!target||target->mPlantHealth<=0)))
        {
            const auto* data=GetNativeSourceZombieData(actual);
            NetTrace("SMOKE_JACKSON291_PREREQUISITE_LOST role=%s stage=%d id=%u token=%u allocated=%d nativePhase=%d body=%d sourceHP=%.9g moweredReanim=%u targetPresent=%d targetHP=%d gunsCreated=%d",
                Role(a),s.stage,jackson.parent.id,jackson.parent.token,actual?1:0,actual?int(actual->mZombiePhase):-1,
                actual?actual->mBodyHealth:-1,data?data->health:-1.f,actual?unsigned(actual->mMoweredReanimID):0,
                target?1:0,target?target->mPlantHealth:-1,jackson.gun);
            LawnMower* mower=nullptr;while(b->mLawnMowers.IterateNext(mower))if(mower->mRow==2)
                NetTrace("SMOKE_JACKSON291_MOWER_DIAGNOSTIC role=%s row=%d x=%.3f state=%d",Role(a),mower->mRow,mower->mPosX,int(mower->mMowerState));
            Require(a,false,"Jackson291 parent/target lost before required genuine contact and weapon setup");return true;
        }
    }
    if(!JacksonCollect(a))return true;
    auto* z=CombatZombieRef(b,jackson.parent);const auto* view=GetNativeJackson291(z);
    JacksonContact(a);
    if(!jackson.diagnostic||GetTickCount()-jackson.diagnostic>=3000)
    {
        jackson.diagnostic=GetTickCount();const auto* c=GetNativeContext(b);
        NetTrace("SMOKE_JACKSON291_WAIT role=%s stage=%d paused=%d phase=%d outgoing=%d elapsed=%.5f total=%.5f source6=%zu source7=%zu clouds=%zu contact=%d damage=%d head=%d arm=%d HP=%d tutorial=%u/%d wave=%u",
            Role(a),s.stage,b->mPaused,view?int(view->phase):-1,view?int(view->outgoing):-1,view?view->transitionElapsed:0,view?view->transitionTotal:0,
            JacksonCount(6),JacksonCount(7),jackson.clouds.size(),jackson.contact,jackson.damage,z?z->mHasHead:0,z?z->mHasArm:0,z?z->mBodyHealth:-1,
            unsigned(c->adventure.run.tutorial.stage),c->adventure.run.tutorial.active,c->adventure.wave);
    }
    if(s.stage==40)
    {
        if(!GrassPaused(a)||(!jackson.initial&&!JacksonInitial(a))||!Capture(a,"cohort")||!JacksonProof(a,"INITIAL",true)||!GrassResumed(a))return true;
        GrassAdvance(a,41);
    }
    else if(s.stage==41)
    {
        if(a->NetIsHost()&&view&&view->transitionTotal>0&&view->transitionElapsed>=.04f&&view->transitionElapsed<=.2f&&!grass.pauseRequested)grass.pauseRequested=Escape(a);
        if(b->mPaused){GrassAdvance(a,42);grass.pauseRequested=a->NetIsHost();}
    }
    else if(s.stage==42)
    {
        if(!GrassPaused(a))return true;
        if(!Require(a,view&&view->phase==FusionNightZombie281Phase::Point&&view->outgoing==FusionNightZombie281Phase::Moonwalk&&
            view->transitionElapsed>0&&view->transitionElapsed<view->transitionTotal,"Jackson291 actual first mid-transition view missing"))return true;
        if(!GrassNodes(a,z,true)||!Capture(a,"blend")||!JacksonProof(a,"BLEND",true)||!GrassResumed(a))return true;
        GrassAdvance(a,43);
    }
    else if(s.stage==43)
    {
        if(a->NetIsHost()&&JacksonCount(6)==4&&!grass.pauseRequested)grass.pauseRequested=Escape(a);
        if(b->mPaused){GrassAdvance(a,44);grass.pauseRequested=a->NetIsHost();}
    }
    else if(s.stage==44)
    {
        if(!GrassPaused(a)||!JacksonClosure(a,false))return true;
        jackson.closure.clear();for(const auto& [key,r]:jackson.actors)if(r.kind==6)jackson.closure.push_back(key);
        if(!Capture(a,"summon")||!JacksonProof(a,"SUMMON",true)||!GrassResumed(a))return true;
        GrassAdvance(a,45);
    }
    else if(s.stage==45)
    {
        if(JacksonCount(7)>=4&&!Capture(a,"dance"))return true;
        if(!JacksonClosure(a,true)||!JacksonProof(a,"CLOSURE_READY",false))return true;
        GrassAdvance(a,46);
    }
    else if(s.stage==46)
    {
        if(!GrassPaused(a))return true;
        jackson.closure.clear();for(const auto& [key,r]:jackson.actors)if(r.kind==6||r.kind==7)jackson.closure.push_back(key);
        if(!JacksonProof(a,"CLOSURE",true)||!GrassResumed(a))return true;
        NetTrace("SMOKE_JACKSON291_CLOSURE role=%s initial6=4 acceptedDanceBatches=%d source7=%zu genuineIndependentIDs=1 cloud11PerChild=1 realJumpLand=1 nativeAnimations=1 noDuplicateChildren=1",Role(a),jackson.groups,JacksonCount(7));
        GrassAdvance(a,47);
    }
    else if(s.stage==47)
    {
        if(!JacksonContact(a)||!JacksonProof(a,"CONTACT_READY",false))return true;
        GrassAdvance(a,48);
    }
    else if(s.stage==48)
    {
        if(!GrassPaused(a)||!Capture(a,"contact")||!JacksonProof(a,"CONTACT",true))return true;
        if(a->NetIsHost()&&!jackson.gun)
        {for(int row=0;row<5;++row)if(!CombatPlantBirth(a,1032,0,row))return true;if(!CombatPlantBirth(a,1032,5,2))return true;jackson.gun=true;}
        if(GrassResumed(a))GrassAdvance(a,49);
    }
    else if(s.stage==49)
    {
        if(a->NetIsHost()&&view&&view->phase==FusionNightZombie281Phase::Death&&!grass.pauseRequested)grass.pauseRequested=Escape(a);
        if(b->mPaused){GrassAdvance(a,50);grass.pauseRequested=a->NetIsHost();}
    }
    else if(s.stage==50)
    {
        if(!GrassPaused(a))return true;
        if(!Require(a,jackson.death&&view&&view->phase==FusionNightZombie281Phase::Death&&!z->mHasHead&&!z->mHasArm,
            "Jackson291 natural source arm/head/death was not observed"))return true;
        if(!GrassNodes(a,z,true)||!Capture(a,"death")||!JacksonProof(a,"DEATH",true)||!GrassResumed(a))return true;
        GrassAdvance(a,51);
    }
    else if(s.stage==51)
    {
        if(b->mZombies.DataArrayTryToGet(jackson.parentUID))return true;
        if(!jackson.retired){jackson.retired=true;NetTrace("SMOKE_JACKSON291_RETIRED role=%s id=%u token=%u actualDataArrayAbsent=1 sourceClipDeath=1",Role(a),jackson.parent.id,jackson.parent.token);}
        if(JacksonProof(a,"RETIRED",false))GrassAdvance(a,52);
    }
    else if(s.stage==52)
    {
        if(!GrassPaused(a)||!Capture(a,"paused"))return true;
        if(!jackson.logged)
        {
            jackson.logged=true;const auto* c=GetNativeContext(b);
            NetTrace("SMOKE_JACKSON291_COMPLETE role=%s sourceLevel=10 actualNight=1 initial6=4 acceptedDanceBatches=%d source7=%zu firstBlendCanonical=1 actualContactDamage=1 deathRetired=1 reliableParts=RunnerChecks sourceTutorial=%u/%d wave=%u ClassicUnchanged=1 fullCampaign=0 refill=NotExecuted mindControl=NotExecuted setupFactoriesOnly=1 HPWrites=0 clockWrites=0 RNGWrites=0",
                Role(a),jackson.groups,JacksonCount(7),unsigned(c->adventure.run.tutorial.stage),c->adventure.run.tutorial.active,c->adventure.wave);
        }
        if(JacksonProof(a,"COMPLETE",true))a->CoopSmokeWriteResult(true,nullptr);
    }
    return true;
}

}

namespace LawnNet
{
bool NetSmokeUpdateFusionDayZombie274(LawnApp* a)
{
    if(!Enabled("PVZ_COOP_SMOKE_FUSION_DAY274"))return false;
    if(!a||!a->CoopSmokeActive()||!a->mNetManager||a->mCoopSmokeResultWritten)return true;
    if(!Require(a,Enabled("PVZ_COOP_SMOKE_FUSION_NATIVE")&&a->mCoopSmokeManifest.mScenario==NETSMOKE_SCENARIO_FUSION_VALIDATION,
        "Day274 requires owned native fusion_validation fixture"))return true;
    if(Grass()&&!Require(a,!Combat()&&Enabled("PVZ_COOP_SMOKE_FUSION_SOURCE273"),
        "Grass284 requires Source273 and a separate non-Combat cohort"))return true;
    if(Jackson()&&!Require(a,!Grass()&&!Combat()&&Enabled("PVZ_COOP_SMOKE_FUSION_SOURCE273"),"Jackson291 requires separate real source Night scenario"))return true;
    if(!s.started)s.started=s.since=GetTickCount();
    if(GetTickCount()-s.started>(Jackson()?360000u:Grass()?450000u:Combat()?600000u:180000u)||GetTickCount()-s.since>((Jackson()||Grass()||Combat())?180000u:60000u))
    {a->CoopSmokeFail("Day274 actual cohort/input deadline; inspect stage and model-node traces");return true;}
    if(!s.stage)
    {
        if(a->mNetManager->GetPlayerCount()!=2)return true;
        if(!Require(a,Slot(a)>=0&&Slot(a)<2&&a->mNetManager->GetDifficulty()==1,"Day274 requires two owned peers at coop difficulty1"))return true;
        if(Jackson()&&jackson.classicLevel<0){if(!a->mPlayerInfo)return true;jackson.classicLevel=a->mPlayerInfo->mLevel;jackson.classicFinished=a->mPlayerInfo->mFinishedAdventure;}
        if(a->NetIsClient()){if(Send(a,"D274_BEGIN"))Advance(a,1);return true;}
        if(!Has(a,1,"D274_BEGIN"))return true;
        if(!Require(a,a->StartFusionSourceAdventure(Jackson()?10:1,false),"Day274 production source entry rejected"))return true;
        Advance(a,1);return true;
    }
    if(Combat())
    {
        const auto* context=GetNativeContext(a->mBoard);
        if(s.stage==20||(combat.proofReady&&context&&context->epoch!=s.epoch))return CombatRestart(a);
    }
    if(!a->mBoard||!World(a))return true;
    if(Combat()&&s.stage==10)return CombatUpdate(a);
    if(Jackson()&&s.stage>=40)return JacksonUpdate(a);
    if(Grass()&&s.stage>=30)return GrassUpdate(a);
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);const auto& bank=c->selection.State();
    if(s.stage==1){if(Choose(a))Advance(a,2);}
    else if(s.stage==2)
    {
        if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}
        auto* chooser=a->mSeedChooserScreen;auto* button=chooser?chooser->mStartButton:nullptr;
        if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;
        const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
        if(!button->IsMouseOver())
        {SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.windowID=SDL_GetWindowID(a->mSDLWindow);motion.motion.x=x;motion.motion.y=y;Require(a,SDL_PushEvent(&motion),"Day274 Ready hover failed");}
        else if(Click(a,x,y))Advance(a,3);
    }
    else if(s.stage==3)
    {
        if(Jackson()&&bank.phase==FusionBankPhase::Intro)
        {if(!jackson.introStart)jackson.introStart=GetTickCount();jackson.introLast=GetTickCount();++jackson.introChecks;}
        if(bank.phase!=FusionBankPhase::Playing||a->mSeedChooserScreen||a->mGameScene!=SCENE_PLAYING)return true;
        if(s.playingFrame<0){s.playingFrame=a->mAppCounter;s.startSamples=Samples(a);return true;}
        if(a->mAppCounter<=s.playingFrame||Samples(a)-s.startSamples<2)return true;
        if(Jackson())
        {
            if(!Require(a,jackson.introChecks>=10&&jackson.introLast-jackson.introStart>=1000,"Jackson291 full real native intro was skipped"))return true;
            if(!JacksonPrepare(a)||!Barrier(a,"J291_PREPARED")||!JacksonSetup(a))return true;GrassAdvance(a,40);return true;
        }
        if(Grass())
        {
            if(!GrassPreflight(a)||!Barrier(a,"EMPTY_SOURCE_OPENING")||!GrassSetup(a))return true;
            GrassAdvance(a,30);return true;
        }
        if(Combat())
        {
            if(!CombatPreflight(a)||!Barrier(a,"EMPTY_SOURCE_OPENING")||!CombatSetup(a))return true;
            Advance(a,10);return true;
        }
        if(!Barrier(a,"EMPTY_SOURCE_OPENING")||!Birth(a))return true;
        if(Observe(a,false,"cohort"))Advance(a,4);
    }
    else if(s.stage==4)
    {
        if(!Observe(a,true,"moved")||!Shots(a))return true;
        Advance(a,5);
    }
    else if(s.stage==5)
    {
        if(a->NetIsHost()&&!s.pause)s.pause=Escape(a);
        if(!b->mPaused)return true;
        s.pauseSamples=Samples(a);Advance(a,6);
    }
    else if(s.stage==6&&Samples(a)-s.pauseSamples>=2)
    {
        if(!Observe(a,false,"paused"))return true;
        NetTrace("SMOKE_DAY274_COMPLETE role=%s sourceLevel=1 wave=0 tutorialActive=1 actors=8 actualBodies=8 nativeShooterHeads=2 movement8=1 minimumSnapshots=20 hostile0=20 hostile11=80 realModelAnchors=1 paused=1 canonicalHeadStateDigest=1 PoleJump=NotExecuted DollChain=NotExecuted ArmorCombat=NotExecuted BucketDrop=NotExecuted fullDayAI=0 postBirthActorWrites=0 clockWrites=0 chooser=SDLOnly",
            Role(a));a->CoopSmokeWriteResult(true,nullptr);
    }
    return true;
}
}
