#include "FusionNativeDryGround353.h"
#include "FusionNativeJackbox300.h"
#include "FusionNativeJackbox309Bridge.h"
#include "FusionNative.h"
#include "FusionNativeSourceClock313.h"
#include "FusionNativeZombies.h"
#include "FusionNativeZombieVisual.h"
#include "FusionBasePlantRules.h"
#include "../Board.h"
#include "../Zombie.h"
#include "../../LawnApp.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace LawnFusion
{
namespace
{
using Phase=FusionJackbox300Phase;
bool Authority(const Zombie* z)
{
    const auto* d=GetNativeSourceZombieData(z);const auto* state=GetNativeJackbox309(z);
    return z&&d&&FusionJackbox300Kind(d->kind)&&z->mApp&&z->mBoard&&z->mApp->mBoard==z->mBoard&&
        !z->mNetGhost&&!z->mApp->NetIsClient()&&IsNativeSourceWorld313(z->mBoard)&&
        (NativeSourceDryRow353(z->mBoard,z->mRow)||(state&&state->rowMoving&&z->mRow>=0&&z->mRow<GetNativeFusionRowCount(z->mBoard)));
}
bool Ready(const NativeJackbox300Hooks& h)
{return h.stateFor&&h.input&&h.randomInt&&h.randomFloat&&h.preflight&&h.canPlayLoop&&h.audio&&h.sound&&
    h.bite&&h.moveRootDelta&&h.explode&&h.retire&&h.moveChildToTargetRow&&h.armorFall&&h.failed;}
struct Run
{LawnApp* app;Board* board;uint32_t uid;uint16_t kind;const NativeJackbox300Hooks& h;Zombie* z;NativeJackbox300State* s;};
Run Capture(Zombie* z,NativeJackbox300State& s,const NativeJackbox300Hooks& h)
{return {z->mApp,z->mBoard,z->mBoard->mZombies.DataArrayGetID(z),GetNativeSourceZombieData(z)->kind,h,z,&s};}
bool Refresh(Run& r)
{
    if(r.app->mBoard!=r.board)return false;
    r.z=r.board->mZombies.DataArrayTryToGet(r.uid);
    const auto* d=GetNativeSourceZombieData(r.z);
    if(!r.z||r.z->mDead||!d||d->kind!=r.kind||d->instanceToken!=r.uid)return false;
    r.s=r.h.stateFor(r.z);return r.s&&r.s->initialized&&r.s->localUID==r.uid;
}
bool Fail(Run& r,const char* reason)
{
    if(Refresh(r))r.s->failed=true;
    if(r.app->mBoard==r.board)r.h.failed(r.board,reason);
    return false;
}
Reanimation* Body(const Zombie* z)
{return z->mApp->ReanimationTryToGet(z->mBodyReanimID);}
bool BodyMatches(const Reanimation* b,const FusionJackbox300Clip& c)
{
    if(!b||b->mDead||!const_cast<Reanimation*>(b)->TrackExists(c.nativeTrack))return false;
    int first=0,count=0;const_cast<Reanimation*>(b)->GetFramesForLayer(c.nativeTrack,first,count);
    return count>0&&b->mFrameStart==first&&b->mFrameCount==count;
}
bool Read(Zombie* z,Phase phase,FusionZombieClipPosition& out)
{
    auto* b=Body(z);FusionJackbox300Clip c;
    if(!TryGetFusionJackbox300Clip(phase,c)||!BodyMatches(b,c)||b->mLoopCount<0||!std::isfinite(b->mAnimTime)||b->mAnimTime<0||b->mAnimTime>1)return false;
    FusionZombieClipPosition p{uint32_t(b->mLoopCount),b->mAnimTime*c.seconds};
    if(!c.loop){if(p.loop==1&&p.seconds==c.seconds)p.loop=0;if(p.loop)return false;}
    out=p;return true;
}
bool Write(Zombie* z,Phase phase,FusionZombieClipPosition p)
{
    FusionJackbox300Clip c;auto* b=Body(z);
    if(!TryGetFusionJackbox300Clip(phase,c)||!BodyMatches(b,c)||p.loop>uint32_t((std::numeric_limits<int>::max)())||
        !std::isfinite(p.seconds)||p.seconds<0||p.seconds>c.seconds)return false;
    float normalized=p.seconds/c.seconds;if(normalized*c.seconds<p.seconds)normalized=std::nextafter(normalized,1.f);
    b->mAnimTime=normalized;b->mLastFrameTime=normalized;b->mLoopCount=int(p.loop);return true;
}
void ClearBlend(NativeJackbox300State& s)
{s.outgoing=Phase::Walk;s.outgoingSeconds=s.transitionElapsed=s.transitionTotal=0;s.outgoingPosition={};}
bool Enter(Run& r,Phase phase,bool blend)
{
    const Phase old=r.s->phase;const auto cursor=r.s->incomingPosition;FusionJackbox300Clip c;
    if(!TryGetFusionJackbox300Clip(phase,c)||!Body(r.z)||!Body(r.z)->TrackExists(c.nativeTrack))return false;
    if(blend){r.s->outgoing=old;r.s->outgoingPosition=cursor;r.s->outgoingSeconds=cursor.seconds;r.s->transitionElapsed=0;r.s->transitionTotal=.25f;}
    else ClearBlend(*r.s);
    r.s->phase=phase;r.s->incomingPosition={};r.z->mVelX=0;
    r.z->PlayZombieReanim(c.nativeTrack,c.loop?REANIM_LOOP:REANIM_PLAY_ONCE_AND_HOLD,0,0);
    // Phases can intentionally share a NORMAL clock track; reset it explicitly.
    return Write(r.z,phase,{})&&UpdateNativeJackbox300Animation(r.z,*r.s,r.h);
}
bool Explosion(Zombie* z,const NativeJackbox300Input& input,NativeJackbox300Explosion& out)
{
    const auto* data=GetNativeSourceZombieData(z);float sx=0,sy=0;
    if(!data||!z->mNetId||!GetNativeSourceZombie269NodePoint(z,"Shadow",sx,sy)||!std::isfinite(sx)||!std::isfinite(sy))return false;
    NativeJackbox300Explosion e;e.ownerId=z->mNetId;e.ownerToken=data->instanceToken;e.sourceKind=data->kind;
    e.row=z->mRow;e.fromWave=z->mFromWave;e.targetRow=input.targetRow;e.renderOrder=z->mRenderOrder;
    e.shadowX=e.centerX=sx;e.shadowY=sy;e.centerY=sy-.7f*GetNativeFusionPixelsY(z->mBoard);e.controlled=z->mMindControlled;
    if(!std::isfinite(e.centerY))return false;out=e;return true;
}
bool Input(Run& r,NativeJackbox300Input& out)
{
    NativeJackbox300Input v;
    if(!r.h.input(r.z,v)||!std::isfinite(v.sourceSpeed)||!std::isfinite(v.animatorSpeed)||v.animatorSpeed<0||
        v.targetRow< -1||v.targetRow>=GetNativeFusionRowCount(r.board))return false;
    out=v;return true;
}
bool PopEvent(Run& r)
{
    if(r.s->popEventHandled)return true;
    NativeJackbox300Input input;NativeJackbox300Explosion e;std::string error;
    if(!Input(r,input)||!Explosion(r.z,input,e)||!r.h.preflight(r.z,NativeJackbox300Operation::Explosion,&e,false,&error))
        return Fail(r,"Jackbox prepared AnimExplode dependencies unavailable before mutation.");
    float doubled=0;
    if(r.kind==30&&!TryDoubleFusionJackbox300Speed(30,GetNativeSourceZombieData(r.z)->originSpeed,doubled))
        return Fail(r,"SuperJackbox initial source speed is not a valid once-only double.");
    r.s->popEventHandled=true; // Consume BEFORE any recursive damage/Die callback.
    if(r.kind==24)
    {
        // Everything used after retirement is an immutable value/Board ref.
        // Retire may delete z AND s. Never dereference either afterwards.
        r.s->deathEventHandled=true;r.h.retire(r.z,2);
        if(r.app->mBoard==r.board&&!r.h.explode(r.board,e))
            r.h.failed(r.board,"Jackbox24 post-retirement shared explosion failed; cannot replay.");
        return true;
    }
    if(!r.h.explode(r.board,e))return Fail(r,"SuperJackbox explosion failed after mutation; cannot replay.");
    if(!Refresh(r))return true; // A synchronous source callback retired this exact generation.
    if(!TryDoubleFusionJackbox300Speed(30,GetNativeSourceZombieData(r.z)->originSpeed,doubled))
        return Fail(r,"SuperJackbox callback changed source speed outside the prepared once-only double.");
    // Source Play(walk), then unique item0/1 Destroy, status0, originSpeed*=2.
    // A recipient callback can synchronously start this owner's common death.
    // Preserve that lifecycle; do not resurrect its raw walk track afterwards.
    const bool dying=r.s->phase==Phase::Death||r.z->IsDeadOrDying();
    if(!dying&&!Enter(r,Phase::Walk,false))return Fail(r,"SuperJackbox post-explosion source walk clock unavailable.");
    r.s->boxPresent=false;r.s->popTriggered=false;r.s->audioPlaying=false;r.s->countdown=0;
    GetNativeSourceZombieData(r.z)->originSpeed=doubled;
    r.h.audio(r.z,NativeJackbox300Audio::Pause);
    if(!Refresh(r))return true;
    return UpdateNativeJackbox300Animation(r.z,*r.s,r.h);
}
bool Events(Run& r,Phase phase,FusionZombieClipPosition a,FusionZombieClipPosition b)
{
    uint8_t bits=0;if(!TryGetFusionJackbox300Events(phase,a,b,bits))return false;
    if(bits&Jackbox300Bite){r.h.bite(r.z);if(!Refresh(r))return true;}
    if(bits&Jackbox300Explode){if(!PopEvent(r))return false;if(!Refresh(r))return true;}
    if(bits&Jackbox300Destroy){r.h.retire(r.z,2);return true;}
    return true;
}
double Distance(FusionZombieClipPosition a,FusionZombieClipPosition b,const FusionJackbox300Clip& c)
{return double(b.loop-a.loop)*c.seconds+double(b.seconds)-a.seconds;}
float Wait(Phase phase,FusionZombieClipPosition at,float rate)
{
    FusionZombieClipPosition event;FusionJackbox300Clip c;
    if(rate<=0||!TryGetFusionJackbox300NextEvent(phase,at,event)||!TryGetFusionJackbox300Clip(phase,c))return (std::numeric_limits<float>::infinity)();
    return float((std::max)(0.0,Distance(at,event,c))/rate);
}
bool Step(Run& r,FusionZombieClipPosition observed,const NativeJackbox300Input& input)
{
    FusionJackbox300Clip c;if(!TryGetFusionJackbox300Clip(r.s->phase,c))return false;
    if(observed.loop<r.s->incomingPosition.loop)return false;
    const double span=Distance(r.s->incomingPosition,observed,c);
    if(!std::isfinite(span)||span<0)return false;
    float rate=float(span/.01),remaining=.01f;Phase next;
    if(TrySelectFusionJackbox300Phase(r.kind,*r.s,input.attacking,input.animatorSpeed>0,next))
    {
        if(!Enter(r,next,true))return false;
        TryGetFusionJackbox300Clip(r.s->phase,c);rate=input.animatorSpeed*c.multiplier;
    }
    for(int split=0;remaining>0;++split)
    {
        if(split>=32)return false;
        const Phase phase=r.s->phase;TryGetFusionJackbox300Clip(phase,c);
        float dt=(std::min)(remaining,Wait(phase,r.s->incomingPosition,rate));
        float oldRate=0;FusionJackbox300Clip old;
        if(r.s->transitionTotal>0)
        {
            TryGetFusionJackbox300Clip(r.s->outgoing,old);oldRate=old.multiplier*input.animatorSpeed;
            dt=(std::min)({dt,r.s->transitionTotal-r.s->transitionElapsed,Wait(r.s->outgoing,r.s->outgoingPosition,oldRate)});
        }
        if(!std::isfinite(dt)||dt<0)return false;
        const auto ia=r.s->incomingPosition,oa=r.s->outgoingPosition;FusionZombieClipPosition ib,ob=oa;
        if(!TryAdvanceFusionJackbox300Clock(phase,ia,rate/c.multiplier,dt,ib))return false;
        if(dt==Wait(phase,ia,rate)&&!TryGetFusionJackbox300NextEvent(phase,ia,ib))return false;
        if(r.s->transitionTotal>0)
        {
            if(!TryAdvanceFusionJackbox300Clock(r.s->outgoing,oa,input.animatorSpeed,dt,ob))return false;
            if(dt==Wait(r.s->outgoing,oa,oldRate)&&!TryGetFusionJackbox300NextEvent(r.s->outgoing,oa,ob))return false;
        }
        double delta=0,oldDelta=0;if(!TryGetFusionJackbox300RootDelta(phase,ia,ib,delta))return false;
        if(r.s->transitionTotal>0)
        {
            if(!TryGetFusionJackbox300RootDelta(r.s->outgoing,oa,ob,oldDelta))return false;
            const double weight=(r.s->transitionElapsed+dt*.5)/r.s->transitionTotal;
            delta=oldDelta*(1-weight)+delta*weight;
        }
        if(!Write(r.z,phase,ib))return false;
        r.s->incomingPosition=ib;
        const bool blending=r.s->transitionTotal>0;const Phase outgoing=r.s->outgoing;
        if(blending)
        {r.s->outgoingPosition=ob;r.s->outgoingSeconds=ob.seconds;r.s->transitionElapsed=(std::min)(r.s->transitionTotal,r.s->transitionElapsed+dt);}
        if(delta&&!r.h.moveRootDelta(r.z,delta))return false;
        if(!Refresh(r))return true;
        if(blending&&!Events(r,outgoing,oa,ob))return false;
        if(!Refresh(r))return true;
        if(r.s->phase==phase&&!Events(r,phase,ia,ib))return false;
        if(!Refresh(r))return true;
        remaining=(std::max)(0.f,remaining-dt);
        if(r.s->phase!=phase)
        {TryGetFusionJackbox300Clip(r.s->phase,c);rate=c.multiplier*input.animatorSpeed;continue;}
        if(r.s->transitionTotal>0&&r.s->transitionElapsed>=r.s->transitionTotal)ClearBlend(*r.s);
        if(dt==0&&ib.loop==ia.loop&&ib.seconds==ia.seconds&&ob.loop==oa.loop&&ob.seconds==oa.seconds)return false;
    }
    return true;
}
}
bool InitializeNativeJackbox300(Zombie* z,NativeJackbox300State& s,const NativeJackbox300Hooks& h)
{
    if(!Authority(z)||!Ready(h))return false;auto run=Capture(z,s,h);std::string error;NativeJackbox300Input input;
    if(s.initialized)return s.localUID==run.uid&&!s.failed&&FusionJackbox300ViewValid(run.kind,s);
    if(!run.uid||!Input(run,input)||!h.preflight(z,NativeJackbox300Operation::Initialize,nullptr,false,&error))return false;
    const int chance=h.randomInt(z->mBoard,0,100);if(chance<0||chance>=100)return false;
    const float draw=h.randomFloat(z->mBoard,chance<5?4.4f:13.22f,chance<5?7.54f:22.68f);
    FusionJackbox300View initial;if(!TryInitializeFusionJackbox300(run.kind,chance,draw,input.instant,initial))return false;
    s=NativeJackbox300State{};static_cast<FusionJackbox300View&>(s)=initial;s.localUID=run.uid;s.initialized=true;
    if(!Enter(run,Phase::Walk,false))return Fail(run,"Jackbox genuine body clock initialization failed after source RNG.");
    if(input.sourcePlaying&&h.canPlayLoop(z)){s.audioPlaying=true;h.audio(z,NativeJackbox300Audio::Play);}
    return true;
}
bool UpdateNativeJackbox300Animation(Zombie* z,const NativeJackbox300State& s,const NativeJackbox300Hooks& h)
{
    if(!Authority(z)||!Ready(h))return false;NativeJackbox300Input input;FusionJackbox300Clip c;auto* b=Body(z);
    if(!h.input(z,input)||!TryGetFusionJackbox300Clip(s.phase,c)||!BodyMatches(b,c)||
        !std::isfinite(input.animatorSpeed)||input.animatorSpeed<0)return false;
    z->mOriginalAnimRate=b->mFrameCount*c.multiplier*input.animatorSpeed/c.seconds;b->mAnimRate=z->mOriginalAnimRate;return true;
}
bool UpdateNativeJackbox300(Zombie* z,NativeJackbox300State& s,const NativeJackbox300Hooks& h)
{
    if(!Authority(z)||!Ready(h))return false;
    if(z->mDead||z->mBoard->mPaused||z->mApp->mGameScene!=SCENE_PLAYING||!CanApplyNativeFusionGameplay(z->mBoard))return true;
    if(s.failed||!s.initialized)return true;auto run=Capture(z,s,h);
    if(s.tick==z->mBoard->mMainCounter)return true;s.tick=z->mBoard->mMainCounter;
    if(!FusionJackbox300ViewValid(run.kind,s))return Fail(run,"Invalid canonical Jackbox controller.");
    NativeJackbox300Input input;FusionZombieClipPosition observed;
    if(!Input(run,input)||!Read(z,s.phase,observed))return Fail(run,"Jackbox actual source input/body span unavailable.");
    FusionJackbox300View next;
    if(!TryStepFusionJackbox300Countdown(run.kind,s,{input.sourcePlaying,input.beforeDying,input.sourceSpeed!=0,input.attacking},.01f,next))return Fail(run,"Jackbox countdown invalid.");
    if(!s.popTriggered&&next.popTriggered)
    {
        std::string error;if(!h.preflight(z,NativeJackbox300Operation::Pop,nullptr,false,&error))return Fail(run,"Jackbox Pop resource preflight failed.");
        const int sound=h.randomInt(run.board,90,92);if(sound<90||sound>=92)return Fail(run,"Jackbox source Pop RNG invalid.");
        static_cast<FusionJackbox300View&>(s)=next;h.sound(z,uint16_t(sound),.5f);
        if(!Refresh(run))return true;h.audio(run.z,NativeJackbox300Audio::Pause);if(!Refresh(run))return true;
    }
    else static_cast<FusionJackbox300View&>(s)=next;
    if(!Step(run,observed,input))return Fail(run,"Jackbox source event/motion step failed; no replay.");
    if(!Refresh(run))return true;
    return UpdateNativeJackbox300Animation(run.z,*run.s,h)||Fail(run,"Jackbox source clock rate invalid.");
}
NativeJackbox300DeathResult BeginNativeJackbox300Death(Zombie* z,NativeJackbox300State& s,int reason,const NativeJackbox300Hooks& h)
{
    if(!Authority(z)||!Ready(h))return NativeJackbox300DeathResult::NotHandled;
    if(!s.initialized||s.failed)return NativeJackbox300DeathResult::Refused;
    if(s.deathEventHandled)return NativeJackbox300DeathResult::Proceed;
    auto run=Capture(z,s,h);NativeJackbox300Input input;NativeJackbox300Explosion e;std::string error;
    const bool possible=run.kind==30&&s.boxPresent&&reason==0&&s.countdown<s.originalCountdown;
    if(!Input(run,input)||!Explosion(z,input,e)||!h.preflight(z,NativeJackbox300Operation::Death,&e,possible,&error))
    {Fail(run,"Jackbox death callback/child closure unavailable BEFORE RNG/Die.");return NativeJackbox300DeathResult::Refused;}
    bool explode=false;
    if(run.kind==30)
    {
        const float roll=h.randomFloat(run.board,0,s.originalCountdown);
        if(!TryPlanFusionJackbox300Death(run.kind,s,reason,roll,explode))
        {Fail(run,"SuperJackbox genuine death RNG invalid.");return NativeJackbox300DeathResult::Refused;}
    }
    s.deathEventHandled=true; // Re-entrant damage cannot repeat this source DieEvent.
    if(explode)
    {
        if(!h.explode(run.board,e)){Fail(run,"SuperJackbox death explosion failed after mutation.");return NativeJackbox300DeathResult::Refused;}
        if(run.app->mBoard!=run.board)return NativeJackbox300DeathResult::OwnerRetired;
        // Source reads row/Shadow/targetRow AFTER Explode. Re-resolve by exact
        // generation first, because a recipient callback may move/retire owner.
        // Preflight must cover those recipient callbacks/row outcomes up front.
        if(Refresh(run))
        {
            NativeJackbox300Input after;NativeJackbox300Explosion current;
            if(!Input(run,after)||!Explosion(run.z,after,current))
            {Fail(run,"SuperJackbox post-explosion live owner projection unavailable.");return NativeJackbox300DeathResult::Refused;}
            e.row=current.row;e.shadowX=current.shadowX;e.shadowY=current.shadowY;e.targetRow=current.targetRow;e.controlled=current.controlled;
        }
        NativeSourceZombieSpawnRequest request;request.kind=24;request.row=e.row;request.fromWave=e.fromWave;request.friendly=e.controlled;
        Zombie* child=SpawnNativeSourceZombie(run.board,request);float rootX=0,rootY=0;
        auto* childState=child?h.stateFor(child):nullptr;const auto* childData=GetNativeSourceZombieData(child);
        if(!child||!childData||childData->kind!=24||!childState||!childState->initialized||childState->failed||
            !GetNativeSourceZombie269NodePoint(child,"",rootX,rootY)||!std::isfinite(child->mPosX+e.shadowX-rootX))
        {Fail(run,"SuperJackbox prepared real child24 birth failed; cannot retry.");return NativeJackbox300DeathResult::Refused;}
        const uint32_t childUID=run.board->mZombies.DataArrayGetID(child);
        child->mPosX+=e.shadowX-rootX;child->mX=int(child->mPosX); // Source SetZombie uses parent ShadowX as new ROOT X.
        if(e.targetRow!=-1&&!h.moveChildToTargetRow(child,e.targetRow))
        {Fail(run,"SuperJackbox prepared exact child target-row continuation failed.");return NativeJackbox300DeathResult::Refused;}
        child=run.board->mZombies.DataArrayTryToGet(childUID);childState=child?h.stateFor(child):nullptr;
        if(!child||!childState){Fail(run,"SuperJackbox actual child generation retired during initialization.");return NativeJackbox300DeathResult::Refused;}
        const float shortCountdown=h.randomFloat(run.board,4.4f,7.54f);
        if(!std::isfinite(shortCountdown)||shortCountdown<4.4f||shortCountdown>7.54f)
        {Fail(run,"SuperJackbox child countdown RNG invalid after birth.");return NativeJackbox300DeathResult::Refused;}
        childState->countdown=shortCountdown; // Source changes ONLY current; preserve child's Awake originalCountdown.
    }
    if(!Refresh(run))return NativeJackbox300DeathResult::OwnerRetired;
    // Nonzero source Die reasons belong to common immediate/terminal paths,
    // including SetCarred->Die2. Never impose an ordinary Death animation.
    if(reason!=0)return NativeJackbox300DeathResult::Proceed;
    FusionZombieClipPosition now;if(!Read(run.z,run.s->phase,now))
    {Fail(run,"Jackbox death outgoing raw clock unavailable before reset.");return NativeJackbox300DeathResult::Refused;}
    run.s->incomingPosition=now;
    if(!Enter(run,Phase::Death,true)){Fail(run,"Jackbox genuine Death clock unavailable.");return NativeJackbox300DeathResult::Refused;}
    return NativeJackbox300DeathResult::Proceed;
}
bool NativeJackbox300AudioStatus(Zombie* z,NativeJackbox300State& s,bool pause,const NativeJackbox300Hooks& h)
{
    if(!Authority(z)||!Ready(h)||!s.initialized)return false;
    if(pause){s.audioPlaying=false;h.audio(z,NativeJackbox300Audio::Pause);}
    else if(h.canPlayLoop(z)){s.audioPlaying=true;h.audio(z,NativeJackbox300Audio::Unpause);}
    return true;
}
bool NativeJackbox300LoseHead(Zombie* z,NativeJackbox300State& s,const NativeJackbox300Hooks& h)
{return NativeJackbox300AudioStatus(z,s,true,h);}
bool NativeJackbox300FirstArmorFall(Zombie* z,NativeJackbox300State& s,const NativeJackbox300Hooks& h)
{
    const auto* data=GetNativeSourceZombieData(z);if(!data||data->kind!=30||!Authority(z)||!Ready(h))return false;
    std::string error;if(!h.preflight(z,NativeJackbox300Operation::ArmorFall,nullptr,false,&error))return false;
    return h.armorFall(z); // Genuine source LoseCone node, sorting=owner+1 and row collision plane.
}
bool CommitNativeJackbox300Disarm(Zombie* z,NativeJackbox300State& s,const NativeJackbox300Hooks& h)
{
    const auto* data=GetNativeSourceZombieData(z);FusionJackbox300View next;
    if(!data||!Authority(z)||!Ready(h)||!s.initialized||!TryDisarmFusionJackbox300(data->kind,s,next))return false;
    static_cast<FusionJackbox300View&>(s)=next;h.audio(z,NativeJackbox300Audio::Pause);return true;
}
bool GetNativeJackbox300Presentation(const Zombie* z,const NativeJackbox300State& s,const char*& state,float& phase)
{
    const auto* data=GetNativeSourceZombieData(z);FusionJackbox300Clip c;FusionZombieClipPosition at;
    if(!data||!FusionJackbox300ViewValid(data->kind,s)||!TryGetFusionJackbox300Clip(s.phase,c)||
        !Read(const_cast<Zombie*>(z),s.phase,at))return false;
    state=c.state;phase=Body(z)->mAnimTime;return true;
}
bool GetNativeJackbox300Blend(const Zombie* z,const NativeJackbox300State& s,const char*& state,float& seconds,float& weight)
{
    const char* incoming=nullptr;float normalized=0;if(!GetNativeJackbox300Presentation(z,s,incoming,normalized))return false;
    const char* name=nullptr;float time=0,w=1;
    if(s.transitionTotal>0){FusionJackbox300Clip c;if(!TryGetFusionJackbox300Clip(s.outgoing,c))return false;name=c.state;time=s.outgoingSeconds/c.multiplier;w=s.transitionElapsed/s.transitionTotal;}
    state=name;seconds=time;weight=w;return true;
}
}
