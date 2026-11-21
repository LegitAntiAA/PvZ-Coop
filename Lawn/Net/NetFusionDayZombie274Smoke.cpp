#include "NetFusionDayZombie274Smoke.h"
#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetCommon.h"
#include "../Board.h"
#include "../Zombie.h"
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
#include "../Fusion/FusionNativeZombieVisual.h"
#include "../Fusion/FusionNativeProjectileMetadata.h"
#include "../Fusion/FusionNativeEquipment.h"
#include "../Fusion/FusionNativeVisual.h"
#include "../../Sexy.TodLib/Reanimator.h"
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
    const std::string name="fusion_day274_"+(Combat()?"combat"+std::to_string(combat.cohort)+"_":"")+std::string(suffix);if(s.frames.contains(name))return true;
    if(!s.draws.contains(name))s.draws[name]=a->mDrawCount;
    const int requested=s.draws[name]+2;if(!a->CoopSmokeEnsureFreshFrameAfterDraw(requested))return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Day274 fresh owned capture failed"))return false;
    s.frames.insert(name);NetTrace("SMOKE_DAY274_RENDER role=%s frame=%s requestedDraw=%d capturedDraw=%d",Role(a),name.c_str(),requested,a->mCoopSmokeFrameDrawCount);return true;
}
bool World(LawnApp* a)
{
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
bool Choose(LawnApp* a)
{
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
}

namespace LawnNet
{
bool NetSmokeUpdateFusionDayZombie274(LawnApp* a)
{
    if(!Enabled("PVZ_COOP_SMOKE_FUSION_DAY274"))return false;
    if(!a||!a->CoopSmokeActive()||!a->mNetManager||a->mCoopSmokeResultWritten)return true;
    if(!Require(a,Enabled("PVZ_COOP_SMOKE_FUSION_NATIVE")&&a->mCoopSmokeManifest.mScenario==NETSMOKE_SCENARIO_FUSION_VALIDATION,
        "Day274 requires owned native fusion_validation fixture"))return true;
    if(!s.started)s.started=s.since=GetTickCount();
    if(GetTickCount()-s.started>(Combat()?600000u:180000u)||GetTickCount()-s.since>(Combat()?180000u:60000u))
    {a->CoopSmokeFail("Day274 actual cohort/input deadline; inspect stage and model-node traces");return true;}
    if(!s.stage)
    {
        if(a->mNetManager->GetPlayerCount()!=2)return true;
        if(!Require(a,Slot(a)>=0&&Slot(a)<2&&a->mNetManager->GetDifficulty()==1,"Day274 requires two owned peers at coop difficulty1"))return true;
        if(a->NetIsClient()){if(Send(a,"D274_BEGIN"))Advance(a,1);return true;}
        if(!Has(a,1,"D274_BEGIN"))return true;
        if(!Require(a,a->StartFusionSourceAdventure(1,false),"Day274 production source1 start rejected"))return true;
        Advance(a,1);return true;
    }
    if(Combat())
    {
        const auto* context=GetNativeContext(a->mBoard);
        if(s.stage==20||(combat.proofReady&&context&&context->epoch!=s.epoch))return CombatRestart(a);
    }
    if(!a->mBoard||!World(a))return true;
    if(Combat()&&s.stage==10)return CombatUpdate(a);
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
        if(bank.phase!=FusionBankPhase::Playing||a->mSeedChooserScreen||a->mGameScene!=SCENE_PLAYING)return true;
        if(s.playingFrame<0){s.playingFrame=a->mAppCounter;s.startSamples=Samples(a);return true;}
        if(a->mAppCounter<=s.playingFrame||Samples(a)-s.startSamples<2)return true;
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
