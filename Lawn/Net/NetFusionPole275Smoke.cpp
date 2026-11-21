#include "NetFusionPole275Smoke.h"
#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetCommon.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Cutscene.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativeAdventure273.h"
#include "../Fusion/FusionNativeBasePlants.h"
#include "../Fusion/FusionNativeDayZombie273.h"
#include "../Fusion/FusionNativeZombieVisual.h"
#include "../Fusion/FusionNativeVisual.h"
#include "../Fusion/FusionNativeEffects269.h"
#include "../Fusion/FusionBasePlantRules.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/TodParticle.h"
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
constexpr std::array<uint16_t,2> plantKinds={3,243};
struct Ref{uint16_t id=0;uint32_t token=0;};
struct Case
{
    Ref plant{},pole{};int initialHP=0,lastStatus=-1;uint8_t events=0;
    bool run=false,jump=false,land=false,previousJump=false,contactUntouched=true;
    FusionZombieClipPosition previous{};float landingX=0,plantX=0;
};
struct State
{
    int stage=0,waiting=0,playingFrame=-1;uint32_t started=0,since=0,chat=0,epoch=0,samples=0,diagnostic=0;
    bool born=false,pause=false,resume=false,star=false;std::array<Case,2> cases{};
    uint32_t starUID=0;float starX=0,starY=0;
    std::set<std::string> frames;std::map<std::string,int> draws;
}s;
bool Enabled(const char* name){const auto* v=std::getenv(name);return v&&std::strcmp(v,"1")==0;}
int Slot(LawnApp* a){return a->mNetManager->GetMySlot();}
const char* Role(LawnApp* a){return a->NetIsHost()?"host":"client";}
uint32_t Samples(LawnApp* a){return a->NetIsHost()?a->mCoopSmokeSnapshotsSent:a->mCoopSmokeSnapshotsApplied;}
bool Require(LawnApp* a,bool okay,const char* why){if(!okay)a->CoopSmokeFail(why);return okay;}
void Advance(LawnApp* a,int stage)
{s.stage=stage;s.since=GetTickCount();NetTrace("SMOKE_POLE275_STAGE role=%s stage=%d epoch=%u",Role(a),stage,s.epoch);}
std::string Tag(const std::string& name){return "P275_"+std::to_string(s.epoch)+"_"+name;}
bool Has(LawnApp* a,int slot,const std::string& value)
{for(const auto& line:a->mNetManager->mChatLines)if(line.mSlot==slot&&line.mText==value)return true;return false;}
bool Send(LawnApp* a,const std::string& value)
{
    if(!Require(a,value.size()<=LawnNet::NET_MAX_CHAT_CHARS,"Pole275 chat identity proof exceeds limit"))return false;
    if(Has(a,Slot(a),value))return true;
    if(s.chat&&GetTickCount()-s.chat<850)return false;
    s.chat=GetTickCount();a->mNetManager->SendChat(value);return Has(a,Slot(a),value);
}
bool Barrier(LawnApp* a,const std::string& value)
{const auto tag=Tag(value);Send(a,tag);return Has(a,0,tag)&&Has(a,1,tag);}
bool Click(LawnApp* a,float x,float y)
{
    SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;
    SDL_Event d{};d.type=SDL_EVENT_MOUSE_BUTTON_DOWN;d.button.windowID=m.motion.windowID;d.button.button=SDL_BUTTON_LEFT;d.button.clicks=1;d.button.down=true;d.button.x=x;d.button.y=y;
    SDL_Event u=d;u.type=SDL_EVENT_MOUSE_BUTTON_UP;u.button.down=false;
    return Require(a,SDL_PushEvent(&m)&&SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Pole275 owned SDL click failed");
}
bool Escape(LawnApp* a)
{
    SDL_Event d{};d.type=SDL_EVENT_KEY_DOWN;d.key.windowID=SDL_GetWindowID(a->mSDLWindow);d.key.key=SDLK_ESCAPE;d.key.down=true;
    SDL_Event u=d;u.type=SDL_EVENT_KEY_UP;u.key.down=false;
    return Require(a,SDL_PushEvent(&d)&&SDL_PushEvent(&u),"Pole275 owned SDL Escape failed");
}
bool Capture(LawnApp* a,const char* suffix)
{
    const auto name="fusion_pole275_"+std::string(suffix);if(s.frames.contains(name))return true;
    if(!s.draws.contains(name))s.draws[name]=a->mDrawCount;
    const int requested=s.draws[name]+2;if(!a->CoopSmokeEnsureFreshFrameAfterDraw(requested))return false;
    if(!Require(a,a->CoopSmokeCaptureFrame(name.c_str()),"Pole275 fresh owned BMP failed"))return false;
    s.frames.insert(name);NetTrace("SMOKE_POLE275_RENDER role=%s frame=%s requestedDraw=%d capturedDraw=%d",Role(a),name.c_str(),requested,a->mCoopSmokeFrameDrawCount);return true;
}
bool World(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);if(!c)return false;
    if(!Require(a,IsNativeSourceAdventure273(b)&&!c->validation&&!a->mFusionRuntime&&c->adventure.run.level==1&&
        c->adventure.run.cycle==1&&c->adventure.sourceDifficulty==2&&c->adventure.run.tutorial.active&&
        !c->adventure.run.over&&!c->adventure.schedulerFailed&&c->adventure.wave==0&&c->adventure.maximumWaves==10&&
        b->mBackground==BACKGROUND_1_DAY&&b->GetColumnCount()==10&&GetNativeFusionRowCount(b)==5&&c->money==3000,
        "Pole275 genuine source1 tutorial hold/world differs"))return false;
    for(int row=0;row<5;++row)if(!Require(a,b->mPlantRow[row]==PLANTROW_NORMAL,"Pole275 non-Day row leaked"))return false;
    if(!s.epoch)s.epoch=c->epoch;
    return Require(a,s.epoch==c->epoch,"Pole275 source epoch changed");
}
bool Choose(LawnApp* a)
{
    auto* b=a->mBoard;const auto* c=GetNativeContext(b);auto* chooser=a->mSeedChooserScreen;
    if(!chooser||!chooser->mNativeFusionChooser||!chooser->mMouseVisible||chooser->mSeedsInFlight||!b->mCutScene->mSeedChoosing)return false;
    const auto& bank=c->selection.State();
    if(!Require(a,c->eligible.size()==4&&bank.targetCount==4&&bank.quotas[0]==2&&bank.quotas[1]==2,"Pole275 actual chooser pool differs"))return false;
    int own=0;for(const auto& p:bank.packets)if(p.owner==Slot(a))
    {if(!Require(a,p.kind<=1&&p.isExtra==(Slot(a)==1),"Pole275 chooser semantic owner/kind differs"))return false;++own;}
    if(own<s.waiting)return false;
    if(own<2)
    {
        int dense=-1;for(size_t i=0;i<c->eligible.size();++i)if(c->eligible[i].kind==own&&c->eligible[i].isExtra==(Slot(a)==1))dense=int(i);
        if(!Require(a,dense>=0,"Pole275 local chooser card missing"))return false;
        int x=0,y=0;chooser->GetSeedPositionInChooser(dense,x,y);
        if(std::abs(chooser->mScrollAmount)>.05f)return false;
        if(Click(a,float(chooser->mX+x+25),float(chooser->mY+y+35-int(chooser->mScrollbar->mScrollValue))))s.waiting=own+1;
        return false;
    }
    return bank.packets.size()==4&&Capture(a,"chooser")&&Barrier(a,"CHOOSER");
}
bool Birth(LawnApp* a)
{
    if(!a->NetIsHost()||s.born)return true;auto* b=a->mBoard;std::string error;
    if(!Require(a,!b->mPaused&&!b->mPlants.mSize&&!b->mZombies.mSize&&b->mSunMoney==500,
        "Pole275 setup must be the initial empty genuine source opening"))return false;
    for(size_t i=0;i<2;++i)
    {
        NativePlantData fresh;fresh.kind=plantKinds[i];fresh.owner=LawnNet::NET_SHARED_OWNER;
        if(!Require(a,EnsureNativePlantVisuals(b,fresh.kind,&error),"Pole275 genuine target plant model unavailable"))return false;
        auto* p=b->AddPlant(4,int(i)+1,GetNativeSeedForPlant(fresh.kind),SEED_NONE,&fresh);
        const auto* d=GetNativePlantData(p);
        if(!Require(a,p&&d&&!p->mDead&&p->mNetId&&d->instanceToken&&d->kind==fresh.kind,"Pole275 typed target factory failed"))return false;
        NetTrace("SMOKE_POLE275_BIRTH_PLANT case=%zu kind=%u id=%u token=%u row=%d hp=%d initialFactory=1 acquisition=NotExecuted",i,d->kind,p->mNetId,d->instanceToken,p->mRow,p->mPlantHealth);
    }
    const uint16_t batch[]={3,3};
    if(!Require(a,CanAllocateNativeSourceDayBatch274(b,batch,&error),"Pole275 actual whole-pair native capacity rejected"))return false;
    for(size_t i=0;i<2;++i)
    {
        NativeSourceZombieSpawnRequest request;request.kind=3;request.row=int(i)+1;request.fromWave=0;
        request.positioned=true;request.shadowX=600;
        auto* z=SpawnNativeSourceZombie(b,request);const auto* d=GetNativeSourceZombieData(z);
        if(!Require(a,z&&d&&!z->mDead&&z->mNetId&&d->instanceToken&&d->kind==3,"Pole275 actual typed Pole factory failed"))return false;
        NetTrace("SMOKE_POLE275_BIRTH_NPC case=%zu id=%u token=%u row=%d shadowBirthX=600 initialFactory=1 postBirthWrites=0",i,z->mNetId,d->instanceToken,z->mRow);
    }
    s.born=true;s.pause=Escape(a);return true;
}
bool BodyWire(const Reanimation* r,LawnNet::NetBuffer& out)
{
    if(!r||!r->mDefinition||!r->mDefinition->mTracks.tracks||r->mDefinition->mTracks.count<=0||
        r->mDefinition->mTracks.tracks[0].mTransforms.count<=0||r->mFrameStart<0||r->mFrameStart>65535||
        r->mFrameCount<=0||r->mFrameCount>65535||!std::isfinite(r->mAnimTime)||r->mAnimTime<0||r->mAnimTime>1)return false;
    LawnNet::NetZombieAnimState state;state.mPresent=true;state.mDead=r->mDead;state.mFrameStart=uint16_t(r->mFrameStart);
    state.mFrameCount=uint16_t(r->mFrameCount);state.mAnimTime=r->mAnimTime;state.mAnimRate=r->mAnimRate;
    state.mLoopType=uint8_t(r->mLoopType);state.mLoopCount=uint8_t((std::clamp)(r->mLoopCount,0,255));
    return LawnNet::NetZombieAnimStateFitsFrames(state,unsigned(r->mDefinition->mTracks.tracks[0].mTransforms.count))&&LawnNet::NetEncodeZombieAnimState(state,out);
}
bool Observe(LawnApp* a,bool baseline)
{
    auto* b=a->mBoard;std::array<Plant*,2> plants{};std::array<Zombie*,2> poles{};Plant* p=nullptr;Zombie* z=nullptr;int pc=0,zc=0;
    while(b->IteratePlants(p))
    {
        const int i=p->mRow-1;const auto* d=GetNativePlantData(p);
        if(!Require(a,i>=0&&i<2&&p->mPlantCol==4&&d&&d->kind==plantKinds[size_t(i)]&&!plants[size_t(i)],
            "Pole275 unexpected/duplicate plant or Sunflower replaced tutorial hold"))return false;
        plants[size_t(i)]=p;++pc;
    }
    while(b->IterateZombies(z))
    {
        const int i=z->mRow-1;const auto* d=GetNativeSourceZombieData(z);
        if(!Require(a,i>=0&&i<2&&d&&d->kind==3&&!z->mMindControlled&&!z->IsDeadOrDying()&&!poles[size_t(i)],
            "Pole275 cohort gained/replaced/untyped/dying actor"))return false;
        poles[size_t(i)]=z;++zc;
    }
    if(pc!=2||zc!=2)return false;
    for(size_t i=0;i<2;++i)
    {
        auto& c=s.cases[i];p=plants[i];z=poles[i];const auto* pd=GetNativePlantData(p);const auto* zd=GetNativeSourceZombieData(z);const auto* day=GetNativeDayZombie273(z);
        auto* body=a->ReanimationTryToGet(z->mBodyReanimID);const auto* definition=FusionFindPlant(plantKinds[i]);
        if(!Require(a,pd&&zd&&day&&definition&&body&&NativeDayZombie273StateValid(3,*day)&&
            z->mZombieType==ZOMBIE_POLEVAULTER&&z->mBodyMaxHealth==720&&z->mBodyHealth>0&&z->mHasHead&&z->mHasArm&&
            p->mPlantHealth>0&&p->mPlantMaxHealth==definition->maxHealth&&pd->owner==LawnNet::NET_SHARED_OWNER&&
            !pd->packetId&&!pd->randomOrigin&&pd->firstParent==kFusionNoPlant&&pd->secondParent==kFusionNoPlant,
            "Pole275 canonical actual plant/Pole/controller state differs"))return false;
        if(!c.plant.id)
        {c.plant={p->mNetId,pd->instanceToken};c.pole={z->mNetId,zd->instanceToken};c.initialHP=p->mPlantHealth;}
        if(!Require(a,p->mNetId==c.plant.id&&pd->instanceToken==c.plant.token&&z->mNetId==c.pole.id&&zd->instanceToken==c.pole.token&&
            c.plant.id&&c.plant.token&&c.pole.id&&c.pole.token,"Pole275 initial full generation changed"))return false;
        bool shadow=false;float rx=0,ry=0,sx=0,sy=0,px=0,py=0;
        if(!Require(a,GetNativeSourceZombie269NodePoint(z,"",rx,ry)&&GetNativeSourceZombie269NodePoint(z,"Shadow",sx,sy)&&
            GetNativeSourceZombie269NodeActivity(z,"Shadow",shadow)&&shadow==day->shadowEnabled&&
            GetNativeBasePlantGroundAnchor(p,px,py),"Pole275 actual renderer Shadow activity/model anchor differs from source state"))return false;
        const float ox=b->mX+a->mDDInterface->mWideScreenOffsetX,oy=b->mY+a->mDDInterface->mWideScreenOffsetY;
        if(!Require(a,(std::min)(rx+ox,sx+ox)>=32&&(std::max)(rx+ox,sx+ox)<=a->mWidth-64&&
            (std::min)(ry+oy,sy+oy)>=0&&(std::max)(ry+oy,sy+oy)<=a->mHeight-8,
            "Pole275 actual ROOT/Shadow moved outside the captured viewport"))return false;
        FusionDayZombie273Clip clip;const char* sourcePose=nullptr;float sourcePhase=0;
        if(!Require(a,TryGetFusionDayZombie273Clip(3,day->status,z->mIsEating,clip)&&body->TrackExists(clip.nativeTrack)&&
            GetNativeDayZombie273Presentation(z,sourcePose,sourcePhase)&&sourcePose&&std::strcmp(sourcePose,clip.sourceName)==0,
            "Pole275 actual source/native animation pose differs"))return false;
        int frameStart=0,frameCount=0;body->GetFramesForLayer(clip.nativeTrack,frameStart,frameCount);
        if(!Require(a,body->mFrameStart==frameStart&&body->mFrameCount==frameCount,"Pole275 actual native body frame span differs from source state"))return false;
        if(baseline&&!Require(a,day->status==2&&shadow&&p->mPlantHealth==definition->maxHealth,"Pole275 initial approach was skipped or damaged"))return false;
        if(day->status==2){if(!Require(a,!c.jump&&!c.land,"Pole275 source returned to pre-vault state"))return false;c.run=true;}
        else if(day->status==3)
        {
            if(!Require(a,c.run&&!c.land&&!shadow,"Pole275 actual2->3 jump/shadow transition differs"))return false;
            c.jump=true;FusionZombieClipPosition current{uint32_t((std::max)(0,body->mLoopCount)),body->mAnimTime*3.5f};uint8_t events=0;
            if(c.previousJump&&TryGetFusionDayZombie273PoleEvents(c.previous,current,events))c.events|=events;
            c.previous=current;c.previousJump=true;
        }
        else if(day->status==0)
        {
            if(!Require(a,c.run&&c.jump&&shadow,"Pole275 landing skipped the observed native jump"))return false;
            if(!c.land)
            {
                const float expectedOffset=i?float(.5*kFusionBasePixelsX):0.f;
                const bool geometry=i==0?sx<px-1:std::abs(sx-px-expectedOffset)<=8;
                if(!geometry)
                {
                    const auto* head=a->ReanimationTryToGet(z->mSpecialHeadReanimID);
                    NetTrace("SMOKE_POLE275_LANDING_GEOMETRY_FAILED role=%s case=%zu id=%u token=%u branch=%s snapshot=%u tick=%d shadow=(%.4f,%.4f) plant=(%.4f,%.4f) expectedOffset=%.4f rawPos=(%.4f,%.4f) target=(%.4f,%.4f) integerPos=(%d,%d) velocity=%.5f altitude=%.5f status=%u previousStatus=%d shadowState=%d nativePhase=%d bodyFrame=%d+%d bodyTime=%.6f bodyRate=%.5f bodyLoops=%d native=%s source=%s sourcePhase=%.6f hasHead=%d headPresent=%d headFrame=%d+%d headTime=%.6f",
                        Role(a),i,z->mNetId,zd->instanceToken,i?"blocked":"success",Samples(a),b->mMainCounter,sx,sy,px,py,expectedOffset,
                        z->mPosX,z->mPosY,z->mNetTargetX,z->mNetTargetY,z->mX,z->mY,z->mVelX,z->mAltitude,unsigned(day->status),c.lastStatus,
                        day->shadowEnabled?1:0,int(z->mZombiePhase),body->mFrameStart,body->mFrameCount,body->mAnimTime,body->mAnimRate,body->mLoopCount,
                        clip.nativeTrack,sourcePose,sourcePhase,z->mHasHead?1:0,head?1:0,head?head->mFrameStart:0,head?head->mFrameCount:0,head?head->mAnimTime:0.f);
                }
                if(!Require(a,geometry,
                    "Pole275 short vault did not cross plant, or TallNut failure did not land at source plantX+.5"))return false;
                c.land=true;c.landingX=sx;c.plantX=px;
                NetTrace("SMOKE_POLE275_LAND role=%s case=%zu id=%u token=%u branch=%s shadowX=%.4f plantX=%.4f expectedOffset=%.4f priorJumpPhase=%.4f soundEventCrossings=%u HPbefore=%d HPnow=%d",
                    Role(a),i,z->mNetId,zd->instanceToken,i?"blocked":"success",sx,px,i?float(.5*kFusionBasePixelsX):0.f,c.previous.seconds,unsigned(c.events&3),c.initialHP,p->mPlantHealth);
            }
        }
        else return Require(a,false,"Pole275 unsupported source status");
        if(day->status!=0&&p->mPlantHealth!=c.initialHP)c.contactUntouched=false;
        if(!Require(a,c.contactUntouched,"Pole275 pre-vault/jump contact incorrectly damaged the source plant"))return false;
        if(c.lastStatus!=day->status)
        {
            c.lastStatus=day->status;
            NetTrace("SMOKE_POLE275_TRANSITION role=%s case=%zu plantKind=%u plant=%u plantToken=%u id=%u token=%u status=%u shadowState=%d shadowNode=%d native=%s source=%s phase=%.5f frames=%d+%d screenRoot=(%.3f,%.3f) screenShadow=(%.3f,%.3f) plantHP=%d",
                Role(a),i,plantKinds[i],p->mNetId,pd->instanceToken,z->mNetId,zd->instanceToken,unsigned(day->status),day->shadowEnabled?1:0,shadow?1:0,
                clip.nativeTrack,sourcePose,sourcePhase,frameStart,frameCount,rx+ox,ry+oy,sx+ox,sy+oy,p->mPlantHealth);
        }
    }
    return true;
}
bool Star(LawnApp* a)
{
    auto* effects=a->mEffectSystem;if(!effects||!effects->mParticleHolder)return false;
    auto* holder=effects->mParticleHolder;TodParticleSystem* system=nullptr;const int type=NativeSourceEffectParticle269(23);
    while(holder->mParticleSystems.IterateNext(system))if(!system->mDead&&int(system->mEffectType)==type)
    {
        if(!s.star)
        {
            s.star=true;s.starUID=holder->mParticleSystems.DataArrayGetID(system);
            if(auto* node=system->mEmitterList.mHead)if(const auto* emitter=holder->mEmitters.DataArrayTryToGet(unsigned(node->mValue)))
            {s.starX=emitter->mSystemCenter.x;s.starY=emitter->mSystemCenter.y;}
            NetTrace("SMOKE_POLE275_STAR role=%s localSystem=%u nativeEffect=%d sourceFactory=23 x=%.4f y=%.4f reliableReceiver=%d actorWrites=0",Role(a),s.starUID,type,s.starX,s.starY,a->NetIsClient()?1:0);
        }
        return Capture(a,"star");
    }
    return s.star&&s.frames.contains("fusion_pole275_star");
}
bool Proof(LawnApp* a,const char* label,bool clocks)
{
    LawnNet::NetBuffer out;auto* b=a->mBoard;
    for(size_t i=0;i<2;++i)
    {
        const auto& c=s.cases[i];out.WriteU16(c.plant.id);out.WriteU32(c.plant.token);out.WriteU16(c.pole.id);out.WriteU32(c.pole.token);
        if(clocks)
        {
            Zombie* z=nullptr;while(b->IterateZombies(z))if(z->mNetId==c.pole.id)break;
            const auto* day=GetNativeDayZombie273(z);if(!day||!BodyWire(a->ReanimationTryToGet(z->mBodyReanimID),out))return false;
            out.WriteU8(day->status);out.WriteU8(day->shadowEnabled?1:0);
            Plant* p=nullptr;while(b->IteratePlants(p))if(p->mNetId==c.plant.id)break;if(!p)return false;out.WriteI32(p->mPlantHealth);
        }
    }
    char name[64];sprintf_s(name,"%s_%08x",label,LawnNet::NetFnv1a32(out.Data(),out.Size()));return Barrier(a,name);
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionPole275(LawnApp* a)
{
    if(!Enabled("PVZ_COOP_SMOKE_FUSION_POLE275"))return false;
    if(!a||!a->CoopSmokeActive()||!a->mNetManager||a->mCoopSmokeResultWritten)return true;
    if(!Require(a,Enabled("PVZ_COOP_SMOKE_FUSION_NATIVE")&&a->mCoopSmokeManifest.mScenario==NETSMOKE_SCENARIO_FUSION_VALIDATION,
        "Pole275 requires owned native fusion_validation"))return true;
    if(!s.started)s.started=s.since=GetTickCount();
    if(GetTickCount()-s.started>180000||GetTickCount()-s.since>60000)
    {a->CoopSmokeFail("Pole275 source/native transition deadline; inspect status, model nodes and Star23 traces");return true;}
    if(!s.stage)
    {
        if(a->mNetManager->GetPlayerCount()!=2||(a->NetIsClient()&&!a->mNetManager->IsConnectedToHost()))return true;
        if(!Require(a,Slot(a)>=0&&Slot(a)<2&&a->mNetManager->GetDifficulty()==1,"Pole275 requires exactly two owned peers at difficulty1"))return true;
        if(a->NetIsClient()){if(Send(a,"P275_BEGIN"))Advance(a,1);return true;}
        if(!Has(a,1,"P275_BEGIN"))return true;
        if(!Require(a,a->StartFusionSourceAdventure(1,false),"Pole275 genuine source1 start failed"))return true;
        Advance(a,1);return true;
    }
    if(!a->mBoard||!World(a))return true;
    auto* b=a->mBoard;const auto& bank=GetNativeContext(b)->selection.State();
    if(s.stage==1){if(Choose(a))Advance(a,2);}
    else if(s.stage==2)
    {
        if(bank.phase!=FusionBankPhase::Choosing){Advance(a,3);return true;}
        auto* chooser=a->mSeedChooserScreen;auto* button=chooser?chooser->mStartButton:nullptr;
        if(!button||button->mDisabled||button->mBtnNoDraw||chooser->mSeedsInFlight)return true;
        const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
        if(!button->IsMouseOver())
        {SDL_Event m{};m.type=SDL_EVENT_MOUSE_MOTION;m.motion.windowID=SDL_GetWindowID(a->mSDLWindow);m.motion.x=x;m.motion.y=y;Require(a,SDL_PushEvent(&m),"Pole275 Ready hover failed");}
        else if(Click(a,x,y))Advance(a,3);
    }
    else if(s.stage==3)
    {
        if(bank.phase!=FusionBankPhase::Playing||a->mSeedChooserScreen||a->mGameScene!=SCENE_PLAYING)return true;
        if(s.playingFrame<0){s.playingFrame=a->mAppCounter;s.samples=Samples(a);return true;}
        if(a->mAppCounter<=s.playingFrame||Samples(a)-s.samples<2||!Barrier(a,"EMPTY")||!Birth(a))return true;
        if(b->mPaused){s.samples=Samples(a);Advance(a,4);}
    }
    else if(s.stage==4)
    {
        if(!b->mPaused||!Observe(a,true))return true;
        if(!s.samples){s.samples=Samples(a);return true;}
        if(Samples(a)-s.samples<2||!Capture(a,"cohort")||!Proof(a,"COHORT",true))return true;
        if(a->NetIsHost()&&!s.resume)s.resume=Escape(a);
        Advance(a,5);
    }
    else if(s.stage==5)
    {
        if(b->mPaused||!Observe(a,false))return true;
        Star(a);if(a->mCoopSmokeResultWritten)return true;
        // Independent source speeds need not produce a simultaneous air window.
        if((s.cases[0].jump&&!s.cases[0].land)||(s.cases[1].jump&&!s.cases[1].land))Capture(a,"jump");
        if(!s.cases[0].land||!s.cases[1].land||!s.star||!s.frames.contains("fusion_pole275_star")||
            !s.frames.contains("fusion_pole275_jump"))return true;
        if(!Require(a,(s.cases[0].events&3)==3&&(s.cases[1].events&3)==3,"Pole275 jump omitted observed native .75/1.5 event crossings"))return true;
        if(!Capture(a,"landed")||!Proof(a,"LANDED",false))return true;
        s.pause=false;Advance(a,6);
    }
    else if(s.stage==6)
    {
        if(!Observe(a,false))return true;
        if(a->NetIsHost()&&!s.pause)s.pause=Escape(a);
        if(b->mPaused){s.samples=Samples(a);Advance(a,7);}
    }
    else if(s.stage==7&&Samples(a)-s.samples>=2)
    {
        if(!b->mPaused||!Observe(a,false)||!Capture(a,"paused")||!Proof(a,"PAUSED",true)||!Barrier(a,"FINAL"))return true;
        NetTrace("SMOKE_POLE275_COMPLETE role=%s sourceLevel=1 wave=0 tutorialActive=1 actors=2 generations=4 successfulVault=1 blocked243=1 transitions2to3to0=2 actualShadowNode=1 sourceNativePose=1 contactDamageBeforeLanding=0 Star23=1 reliableClientFX=1 sourceSoundEventFamilies=2 soundPlayback=NotExecuted postBirthActorWrites=0 clockWrites=0 acquisition=NotExecuted allocatorExhaustion=NotExecuted",Role(a));
        a->CoopSmokeWriteResult(true,nullptr);
    }
    return true;
}
}
