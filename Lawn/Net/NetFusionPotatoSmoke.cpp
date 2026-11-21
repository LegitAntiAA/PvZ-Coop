#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetCommon.h"
#include "NetMystery.h"
#include "NetEndlessWeather.h"
#include "NetZenGarden.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativeWire.h"
#include "../Fusion/FusionPotatoRules.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../SexyAppFramework/DDInterface.h"
#include "../../Resources.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
using namespace LawnFusion;
struct Target
{
    const char* name;
    int row,x,body,first,second;
    bool mind,dead;
    int afterBody,afterFirst,afterSecond;
};
const Target kTargets[]={
    {"contact",2,660,270,0,0,false,true,0,0,0},
    {"blast_armor",2,600,270,5000,5000,false,false,270,3200,3200},
    {"outside",2,520,270,0,0,false,false,270,0,0},
    {"row_minus",1,660,270,0,0,false,false,270,0,0},
    {"row_plus",3,660,270,0,0,false,false,270,0,0},
    {"mindcontrol",2,700,270,0,0,true,false,270,0,0},
    {"highbody",2,630,5000,0,0,false,false,3200,0,0},
    {"shield",2,620,2000,1000,4000,false,false,1200,0,2200}
};
struct PotatoState
{
    int stage=0,waitingOwn=0,firstTick=0,firstCounter=0,riseTick=0,armedTick=0,explosionTick=0,goneTick=0;
    int firstDraw=0,unarmedHealth=300,deathCounter=0;
    uint32_t since=0,lastChat=0,settled=0,introStart=0,introLast=0,introChecks=0,heldSamples=0,purchaseSamples=0,lastTrace=0;
    uint16_t packet=0,plant=0,eater=0;
    bool difficultySet=false,clicked=false,spawned=false,shifted=false,sentExplosion=false,replayed=false,sawRise=false;
    std::array<uint16_t,8> ids{};
    std::string idMessage;
    std::set<std::string> frames;
} s;
const char* Role(LawnApp* app) { return app->NetIsHost() ? "host" : "client"; }
uint32_t Samples(LawnApp* app) { return app->NetIsHost() ? app->mCoopSmokeSnapshotsSent : app->mCoopSmokeSnapshotsApplied; }
bool Require(LawnApp* app, bool value, const char* error)
{ if (!value) app->CoopSmokeFail(error); return value; }
void Advance(LawnApp* app, int stage)
{
    s.stage=stage; s.since=GetTickCount(); s.settled=0; s.clicked=false;
    LawnNet::NetTrace("SMOKE_FUSION_POTATO_STAGE role=%s stage=%d",Role(app),stage);
}
bool Has(LawnApp* app,int slot,const std::string& message)
{
    for (const auto& line:app->mNetManager->mChatLines) if (line.mSlot==slot && line.mText==message) return true;
    return false;
}
void Send(LawnApp* app,const std::string& message)
{
    if (Has(app,app->mNetManager->GetMySlot(),message) || (s.lastChat && GetTickCount()-s.lastChat<900)) return;
    s.lastChat=GetTickCount(); app->mNetManager->SendChat(message);
}
bool Barrier(LawnApp* app,const char* message)
{ Send(app,message); return Has(app,0,message) && Has(app,1,message); }
bool Settled(uint32_t duration=300)
{ if (!s.settled) s.settled=GetTickCount(); return GetTickCount()-s.settled>=duration; }
bool Capture(LawnApp* app,const char* name)
{
    if (s.frames.contains(name)) return true;
    if (!Require(app,app->CoopSmokeCaptureFrame(name),"Potato actual native renderer capture failed")) return false;
    s.frames.insert(name); return true;
}
bool Click(LawnApp* app,float x,float y)
{
    SDL_Event motion{}; motion.type=SDL_EVENT_MOUSE_MOTION; motion.motion.windowID=SDL_GetWindowID(app->mSDLWindow);
    motion.motion.x=x; motion.motion.y=y;
    SDL_Event down{}; down.type=SDL_EVENT_MOUSE_BUTTON_DOWN; down.button.windowID=motion.motion.windowID;
    down.button.button=SDL_BUTTON_LEFT; down.button.clicks=1; down.button.down=true; down.button.x=x; down.button.y=y;
    SDL_Event up=down; up.type=SDL_EVENT_MOUSE_BUTTON_UP; up.button.down=false;
    return Require(app,SDL_PushEvent(&motion) && SDL_PushEvent(&down) && SDL_PushEvent(&up),"Potato owned-window SDL queue failed");
}
bool BoardClick(LawnApp* app,float x,float y)
{ return Click(app,x+app->mBoard->mX+app->mDDInterface->mWideScreenOffsetX,y+app->mBoard->mY+app->mDDInterface->mWideScreenOffsetY); }
bool Button(LawnApp* app,GameButton* button)
{
    if (!button || button->mDisabled || button->mBtnNoDraw) return false;
    const float x=button->mX+button->mWidth*.5f,y=button->mY+button->mHeight*.5f;
    if (!button->IsMouseOver())
    {
        SDL_Event event{}; event.type=SDL_EVENT_MOUSE_MOTION; event.motion.windowID=SDL_GetWindowID(app->mSDLWindow);
        event.motion.x=x; event.motion.y=y;
        Require(app,SDL_PushEvent(&event),"Potato Ready hover SDL queue failed"); return false;
    }
    return Click(app,x,y);
}
bool ArmorTrackMatches(Reanimation* body,const char* name,int expectedGroup,int expectedImage,
    const std::array<Image*,3>& images)
{
    if (!body || !body->TrackExists(name)) return false;
    const auto* track=body->GetTrackInstanceByName(name);
    if (!track || track->mRenderGroup!=expectedGroup) return false;
    const auto* image=track->mImageOverride?track->mImageOverride:body->GetCurrentTrackImage(name);
    if (expectedImage<0)
        return image && (image==images[0] || image==images[1] || image==images[2]);
    ReanimatorTransform transform; body->GetCurrentTransform(body->FindTrackIndex(name),&transform);
    return !track->mForceDontRender && transform.mFrame>=0 && transform.mAlpha>0 &&
        images[expectedImage] && image==images[expectedImage];
}

SeedPacket* Packet(Board* board)
{
    for (int i=0;i<board->mSeedBank->mNumPackets;++i)
    {
        auto* packet=&board->mSeedBank->mSeedPackets[i]; const auto* data=GetNativePacketData(packet);
        if (data && data->owner==1 && data->kind==4 && data->isExtra) return packet;
    }
    return nullptr;
}
Plant* Mine(Board* board)
{
    Plant* plant=nullptr;
    while (board->mPlants.IterateNext(plant))
        if (!plant->mDead && plant->mIsOnBoard && plant->mPlantCol==9 && plant->mRow==2) return plant;
    return nullptr;
}
Zombie* ById(Board* board,uint16_t id)
{
    Zombie* zombie=nullptr;
    while (board->mZombies.IterateNext(zombie)) if (zombie->mNetId==id) return zombie;
    return nullptr;
}
bool Held(Board* board)
{
    const auto* cursor=board->mCursorObject;
    return cursor && cursor->mCursorType==CURSOR_TYPE_PLANT_FROM_BANK && cursor->mType==SEED_POTATOMINE &&
        cursor->mSeedBankIndex>=0 && cursor->mSeedBankIndex<board->mSeedBank->mNumPackets &&
        board->mSeedBank->mSeedPackets[cursor->mSeedBankIndex].mNetId==s.packet;
}
Zombie* Spawn(LawnApp* app,int row,int x)
{
    auto* zombie=app->mBoard->AddZombieInRow(ZOMBIE_NORMAL,row,0);
    if (!Require(app,zombie && zombie->mNetId,"Potato fixture native zombie allocation failed")) return nullptr;
    const auto* body=app->ReanimationTryToGet(zombie->mBodyReanimID);
    if (!Require(app,body && std::isfinite(body->mAnimRate) && body->mAnimRate>0,"Potato fixture lacks a native walking rate")) return nullptr;
    zombie->SetAnimRate(body->mAnimRate);
    zombie->mPosX=static_cast<float>(x); zombie->mX=x;
    zombie->mPosY=zombie->GetPosYBasedOnRow(row); zombie->mY=static_cast<int>(zombie->mPosY);
    zombie->mVelX=0; zombie->ApplyAnimRate(0);
    return zombie;
}
bool ArmorMatches(LawnApp* app,Zombie* zombie,const Target& target,bool after)
{
    auto* body=app->ReanimationTryToGet(zombie->mBodyReanimID);
    const bool helmetHidden=after && target.afterFirst==0;
    if (target.first && !ArmorTrackMatches(body,"anim_bucket",helmetHidden?RENDER_GROUP_HIDDEN:RENDER_GROUP_HELMET,
        helmetHidden?-1:after?1:0,{IMAGE_REANIM_ZOMBIE_BUCKET1,IMAGE_REANIM_ZOMBIE_BUCKET2,IMAGE_REANIM_ZOMBIE_BUCKET3})) return false;
    if (target.second && !ArmorTrackMatches(body,"anim_screendoor",RENDER_GROUP_SHIELD,after?1:0,
        {IMAGE_REANIM_ZOMBIE_SCREENDOOR1,IMAGE_REANIM_ZOMBIE_SCREENDOOR2,IMAGE_REANIM_ZOMBIE_SCREENDOOR3})) return false;
    return true;
}
bool NoOpposingTargets(LawnApp* app,const char* phase)
{
    for (auto id:s.ids)
    {
        auto* zombie=ById(app->mBoard,id);
        auto* opponent=zombie?zombie->FindZombieTarget():nullptr;
        if (opponent)
            LawnNet::NetTrace("SMOKE_FUSION_POTATO_OPPONENT role=%s phase=%s id=%u target=%u fixtureControl=1",
                Role(app),phase,id,opponent->mNetId);
        if (!Require(app,zombie && !opponent,"Potato fixture actors can bite each other")) return false;
    }
    LawnNet::NetTrace("SMOKE_FUSION_POTATO_NO_OPPONENTS role=%s phase=%s actualNativeFindZombieTarget=1 fixtureControl=1",Role(app),phase);
    return true;
}
bool SetupTargets(LawnApp* app)
{
    for (size_t i=0;i<s.ids.size();++i)
    {
        const auto& target=kTargets[i]; auto* zombie=Spawn(app,target.row,target.x-200);
        if (!zombie) return false;
        zombie->mMindControlled=target.mind;
        if (target.first) { zombie->mHelmType=HELMTYPE_BUCKET; zombie->AttachHelmet(); }
        if (target.second) { zombie->mShieldType=SHIELDTYPE_DOOR; zombie->AttachShield(); }
        zombie->mBodyHealth=zombie->mBodyMaxHealth=target.body;
        zombie->mHelmHealth=zombie->mHelmMaxHealth=target.first;
        zombie->mShieldHealth=zombie->mShieldMaxHealth=target.second;
        s.ids[i]=zombie->mNetId;
        LawnNet::NetTrace("SMOKE_FUSION_POTATO_SETUP case=%s id=%u row=%d stagedX=%d contactX=%d body=%d first=%d second=%d mind=%d nativeActor=1 fixtureControl=1",
            target.name,s.ids[i],target.row,target.x-200,target.x,target.body,target.first,target.second,target.mind?1:0);
    }
    if (!NoOpposingTargets(app,"staged")) return false;
    std::ostringstream message; message<<"FPT_IDS"; for (auto id:s.ids) message<<' '<<id;
    s.idMessage=message.str(); s.spawned=true; return true;
}
bool ReadIds(LawnApp* app)
{
    if (app->NetIsHost()) return s.spawned;
    for (const auto& line:app->mNetManager->mChatLines)
    {
        if (line.mSlot!=0 || line.mText.rfind("FPT_IDS ",0)!=0) continue;
        std::istringstream input(line.mText.substr(8)); std::set<unsigned> unique;
        for (auto& id:s.ids)
        {
            unsigned value=0;
            if (!Require(app,!!(input>>value) && value && value<=65535 && unique.insert(value).second,"Potato fixture target IDs are invalid")) return false;
            id=static_cast<uint16_t>(value);
        }
        std::string extra; return Require(app,!(input>>extra),"Potato fixture target IDs have trailing fields");
    }
    return false;
}
bool TargetsMatch(LawnApp* app,bool after)
{
    for (size_t i=0;i<s.ids.size();++i)
    {
        const auto& target=kTargets[i]; auto* zombie=ById(app->mBoard,s.ids[i]);
        if (after && target.dead) { if (zombie && !zombie->IsDeadOrDying()) return false; continue; }
        if (!zombie || zombie->IsDeadOrDying() || zombie->mZombieType!=ZOMBIE_NORMAL || zombie->mRow!=target.row || zombie->mMindControlled!=target.mind ||
            zombie->mBodyHealth!=(after?target.afterBody:target.body) || zombie->mHelmHealth!=(after?target.afterFirst:target.first) ||
            zombie->mShieldHealth!=(after?target.afterSecond:target.second) || !ArmorMatches(app,zombie,target,after)) return false;
        if (!after && (std::abs(zombie->mPosX-(target.x-200))>.01f || std::abs(zombie->mPosY-zombie->GetPosYBasedOnRow(target.row))>.01f ||
            zombie->mVelX!=0 || zombie->mOriginalAnimRate<=0)) return false;
        if (zombie->mHelmType!=((after?target.afterFirst:target.first)>0?HELMTYPE_BUCKET:HELMTYPE_NONE) ||
            zombie->mShieldType!=((after?target.afterSecond:target.second)>0?SHIELDTYPE_DOOR:SHIELDTYPE_NONE)) return false;
    }
    return true;
}
void TraceTargets(LawnApp* app,bool after)
{
    const uint32_t now=GetTickCount(); if (s.lastTrace && now-s.lastTrace<1000) return; s.lastTrace=now;
    for (size_t i=0;i<s.ids.size();++i)
    {
        auto* z=ById(app->mBoard,s.ids[i]);
        const auto* body=z?app->ReanimationTryToGet(z->mBodyReanimID):nullptr;
        auto* opponent=z?z->FindZombieTarget():nullptr;
        LawnNet::NetTrace("SMOKE_FUSION_POTATO_WAIT role=%s after=%d case=%s id=%u exists=%d dead=%d x=%.3f y=%.3f expectedX=%d body=%d first=%d second=%d armorVisual=%d velocity=%.3f originalRate=%.3f liveRate=%.3f eating=%d opponent=%u",
            Role(app),after?1:0,kTargets[i].name,s.ids[i],z?1:0,z && z->IsDeadOrDying()?1:0,z?z->mPosX:0,z?z->mPosY:0,kTargets[i].x-(after?0:200),
            z?z->mBodyHealth:0,z?z->mHelmHealth:0,z?z->mShieldHealth:0,z && ArmorMatches(app,z,kTargets[i],after)?1:0,
            z?z->mVelX:0,z?z->mOriginalAnimRate:0,body?body->mAnimRate:-1,z && z->mIsEating?1:0,opponent?opponent->mNetId:0);
    }
}
// This observer reads a COPY of queued authenticated host bytes. It never
// alters/drops the queue or manufactures a snapshot, phase or host clock.
bool IsDoingSpecialSnapshot(LawnApp* app,const LawnNet::NetBuffer& payload)
{
    const auto* context=GetNativeContext(app->mBoard); auto wire=payload;
    NativeSnapshotFrame frame; LawnNet::NetBuffer body;
    if (!DecodeNativeSnapshot(wire,context->eligible,context->epoch,frame,body)) return false;
    if (std::none_of(frame.plants.begin(),frame.plants.end(),[](const auto& r){return r.id==s.plant && r.data.kind==4;})) return false;
    const size_t core=43+MAX_GRID_SIZE_Y*4+9+10+LawnNet::NET_CHALLENGE_RUNTIME_WIRE_BYTES+
        LawnNet::NET_ENDLESS_WEATHER_WIRE_BYTES+LawnNet::NET_ZEN_GARDEN_WIRE_BYTES;
    if (body.Size()<core+7) return false;
    body.mReadPos=core+6; uint8_t packets=0;
    if (!body.ReadU8(&packets) || packets>SEEDBANK_CAPACITY || body.Remaining()<packets*LawnNet::NET_MYSTERY_SEED_SNAPSHOT_BYTES) return false;
    body.mReadPos+=packets*LawnNet::NET_MYSTERY_SEED_SNAPSHOT_BYTES;
    constexpr size_t plantBytes=23+LawnNet::NET_ZEN_PLANT_WIRE_BYTES+8*LawnNet::NET_ZOMBIE_ANIM_WIRE_BYTES;
    uint16_t count=0; if (!body.ReadU16(&count) || count>1024 || count>body.Remaining()/plantBytes) return false;
    for (uint16_t i=0;i<count;++i)
    {
        const size_t start=body.mReadPos; uint16_t id=0; uint8_t type=0,state=0;
        if (!body.ReadU16(&id) || !body.ReadU8(&type)) return false;
        body.mReadPos=start+10; if (!body.ReadU8(&state)) return false;
        if (id==s.plant) return type==SEED_POTATOMINE && state==STATE_DOINGSPECIAL;
        body.mReadPos=start+plantBytes;
    }
    return false;
}
bool ReplayFreshDoingSpecial(LawnApp* app)
{
    if (app->NetIsHost() || s.replayed) return true;
    for (const auto& pending:app->mNetManager->mPendingBoardMsgs)
    {
        if (pending.mKind!=1 || !IsDoingSpecialSnapshot(app,pending.mData)) continue;
        auto* board=app->mBoard; auto* old=board->NetGetGhostPlant(s.plant);
        if (!Require(app,old && IsNativeFusionPotato(old),"Potato pending host replay lost the existing real client mine")) return false;
        const uint32_t oldGeneration=board->mPlants.DataArrayGetID(old);
        old->mDead=true; old->RemoveEffects();
        auto bytes=pending.mData; bool applied=false; board->NetApplySnapshot(bytes,&applied);
        auto* fresh=board->NetGetGhostPlant(s.plant);
        const auto* data=GetNativePlantData(fresh);
        const auto* light=fresh?app->ReanimationTryToGet(fresh->mLightReanimID):nullptr;
        if (!Require(app,applied && fresh && data && data->kind==4 && fresh->mNetGhost && fresh->mState==STATE_DOINGSPECIAL &&
            board->mPlants.DataArrayGetID(fresh)!=oldGeneration && light && !light->mDead && light->mReanimationType==REANIM_POTATOMINE &&
            light->mFrameCount>0 && light->mFrameCount<=10 && board->mSunMoney==450,
            "Actual pending host DOINGSPECIAL failed fresh native metadata/light adoption")) return false;
        s.replayed=true;
        LawnNet::NetTrace("SMOKE_FUSION_POTATO_LATE_REPLAY role=client plant=%u freshNativeGeneration=1 metadataKind=4 light=1 state=DOINGSPECIAL controlledPendingHostSnapshotReplay=1 lateJoinTested=0 queueDrop=0 hostClockJump=0",s.plant);
        return true;
    }
    return true;
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionPotato(LawnApp* app)
{
    const char* native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE"),*enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_POTATO");
    if (!app || !native || std::strcmp(native,"1") || !enabled || std::strcmp(enabled,"1") || !app->CoopSmokeActive() ||
        app->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION) return false;
    if (app->mCoopSmokeResultWritten) return true;
    if (!s.since) s.since=GetTickCount();
    const uint32_t limit=s.stage<=3?60000U:s.stage==7?45000U:20000U;
    if (GetTickCount()-s.since>limit)
    {char error[100];sprintf_s(error,"Potato fixture deadline stage=%d role=%s",s.stage,Role(app));app->CoopSmokeFail(error);return true;}
    if (!app->mNetManager || !app->mNetManager->IsActive()) return true;
    app->mCoopSmokePeerReady=true;
    if (s.stage==0)
    {
        if (app->NetIsHost() && !s.difficultySet)
        {if(!Require(app,!app->mBoard && !app->mSeedChooserScreen,"Potato requires a clean fixture lobby"))return true;app->mNetManager->SetDifficulty(10);s.difficultySet=true;}
        if (app->mNetManager->GetPlayerCount()!=2 || (app->NetIsClient() && !app->mNetManager->IsConnectedToHost())) return true;
        if (app->NetIsClient()) {Advance(app,1);Send(app,"FPT_START_READY");}
        else if (Has(app,1,"FPT_START_READY"))
        {Advance(app,1);std::string error;if(!app->StartFusionValidation(&error))app->CoopSmokeFail(error.c_str());}
        return true;
    }
    if (!app->mBoard || !IsNativeFusionBoard(app->mBoard)) {if(s.stage==1 && app->NetIsClient())Send(app,"FPT_START_READY");return true;}
    auto* board=app->mBoard; const auto* context=GetNativeContext(board);
    if (!Require(app,context && !app->mFusionRuntime && context->validation && board->GetColumnCount()==10,"Potato lost the real native Fusion Board")) return true;
    const auto& state=context->selection.State(); auto* chooser=app->mSeedChooserScreen;
    if (s.stage>=9 && !ReplayFreshDoingSpecial(app)) return true;
    if (s.stage==1)
    {
        if (!chooser || !chooser->mMouseVisible || !board->mCutScene->mSeedChoosing || chooser->mSeedsInFlight) return true;
        const int owner=app->mNetManager->GetMySlot();
        if (!Require(app,state.phase==FusionBankPhase::Choosing && state.activeMask==3 && state.quotas[owner]>0 && state.difficulty==10,
            "Potato chooser does not expose the actual owner quota")) return true;
        if (!Capture(app,"fusion_potato_chooser")) return true;
        std::vector<int> choices;
        for (size_t i=0;i<context->eligible.size();++i) if (context->eligible[i].isExtra==(owner==1)) choices.push_back(static_cast<int>(i));
        std::stable_sort(choices.begin(),choices.end(),[&](int a,int b){return (context->eligible[a].kind==4)>(context->eligible[b].kind==4);});
        if (!Require(app,choices.size()>=state.quotas[owner] && context->eligible[choices[0]].kind==4,"Potato source card is absent from real eligibility")) return true;
        int own=0;for(const auto& p:state.packets)own+=p.owner==owner;
        if(own<s.waitingOwn)return true;
        if(own<state.quotas[owner])
        {int x=0,y=0;chooser->GetSeedPositionInChooser(choices[own],x,y);if(Click(app,static_cast<float>(chooser->mX+x+25),static_cast<float>(chooser->mY+y+35-(int)chooser->mScrollbar->mScrollValue)))s.waitingOwn=own+1;return true;}
        if(state.packets.size()!=state.targetCount || !Packet(board) || !Capture(app,"fusion_potato_picked"))return true;
        s.packet=Packet(board)->mNetId;Advance(app,2);
    }
    else if(s.stage==2)
    {if(state.phase!=FusionBankPhase::Choosing){Advance(app,3);return true;}if(chooser && !chooser->mSeedsInFlight && Button(app,chooser->mStartButton))Advance(app,3);}
    else if(s.stage==3)
    {
        if(state.phase==FusionBankPhase::Intro)
        {if(!s.introStart)s.introStart=GetTickCount();s.introLast=GetTickCount();++s.introChecks;if(s.introLast-s.introStart>=300)Capture(app,"fusion_potato_intro");return true;}
        if(state.phase!=FusionBankPhase::Playing || app->mGameScene!=SCENE_PLAYING || chooser)return true;
        if(!Require(app,s.introChecks>=10 && s.introLast-s.introStart>=1000 && board->mSunMoney==500 && s.frames.contains("fusion_potato_intro"),"Potato bypassed real Ready/native intro"))return true;
        if(app->NetIsHost()){board->mSunCountDown=100000;board->mZombieCountDown=board->mZombieCountDownStart=100000;}
        NetTrace("SMOKE_FUSION_POTATO_PLAYING role=%s epoch=%u coopDifficulty=10 sourceDifficulty=2 nativeBoard=1 secondWorld=0 target=%u",Role(app),context->epoch,state.targetCount);
        Advance(app,4);
    }
    else if(s.stage==4)
    {
        if(!Barrier(app,"FPT_PLAY"))return true;
        if(app->NetIsHost()){Advance(app,5);return true;}
        auto* packet=Packet(board);const auto* data=GetNativePacketData(packet);
        if(!Require(app,packet && data && data->price==50 && data->rechargeSeconds==30 && packet->mPacketType==SEED_POTATOMINE,"Potato real Extra price/recharge mismatch"))return true;
        if(!s.clicked){s.heldSamples=Samples(app);const auto* bank=board->mSeedBank;if(BoardClick(app,static_cast<float>(bank->mX+packet->mX+packet->mOffsetX+packet->mWidth/2),static_cast<float>(bank->mY+packet->mY+packet->mHeight/2)))s.clicked=true;return true;}
        if(!Held(board) || Samples(app)<=s.heldSamples || !Settled())return true;
        if(!Require(app,board->mSunMoney==500 && packet->mActive && !packet->mRefreshing && !Mine(board),"Potato pickup spent sun/cooldown") || !Capture(app,"fusion_potato_held"))return true;
        NetTrace("SMOKE_FUSION_POTATO_HELD role=client packet=%u charge=0 cooldown=0 snapshotEcho=1 nativeSDL=1",s.packet);
        Advance(app,5);BoardClick(app,static_cast<float>(board->GetCellCenterX(9,2)),static_cast<float>(board->GridToPixelY(9,2)+50));
    }
    else if(s.stage==5)
    {
        auto* mine=Mine(board);const auto* data=GetNativePlantData(mine);auto* packet=Packet(board);
        if(!mine || !data)return true;
        if(!Require(app,data->kind==4 && mine->mSeedType==SEED_POTATOMINE && mine->mPlantMaxHealth==300 && mine->mPlantHealth==300 &&
            data->owner==1 && data->packetId==s.packet && !data->randomOrigin && data->firstParent==kFusionNoPlant && data->secondParent==kFusionNoPlant &&
            board->mSunMoney==450 && packet && packet->mNetId==s.packet && packet->mRefreshTime==3000 && packet->mRefreshing && !packet->mActive,
            "Potato native purchase lost identity, price, HP or source cooldown"))return true;
        if(!s.plant)
        {
            s.plant=mine->mNetId;s.firstDraw=app->mDrawCount;s.purchaseSamples=Samples(app);
            if(app->NetIsHost()){s.firstTick=board->mMainCounter;s.firstCounter=mine->mStateCountdown;if(!Require(app,mine->mState==STATE_NOTREADY && s.firstCounter>=1399 && s.firstCounter<=1601,"Potato initial real native preparation is outside source bounds"))return true;}
        }
        if(app->mDrawCount-s.firstDraw<2)return true;
        if(!Capture(app,"fusion_potato_planted"))return true;
        NetTrace("SMOKE_FUSION_POTATO_PURCHASE role=%s plant=%u packet=%u kind=4 owner=1 extra=1 column=9 row=2 price=50 sun=450 sourceCD=3000 nativeActor=1 nativeSDL=1",Role(app),s.plant,s.packet);
        Advance(app,6);
    }
    else if(s.stage==6)
    {
        auto* mine=Mine(board);if(!Require(app,mine && mine->mNetId==s.plant && mine->mState==STATE_NOTREADY,"Potato armed or vanished before its real unarmed-bite probe"))return true;
        if(app->NetIsHost() && !s.eater){auto* eater=Spawn(app,2,660);if(!eater)return true;s.eater=eater->mNetId;NetTrace("SMOKE_FUSION_POTATO_BITE_SETUP role=host zombie=%u fixtureControl=1 nativeEatPlant=1",s.eater);}
        if(mine->mPlantHealth==300)return true;
        if(!Require(app,mine->mPlantHealth>0 && mine->mState==STATE_NOTREADY,"Unarmed Potato was invincible or exploded"))return true;
        s.unarmedHealth=mine->mPlantHealth;
        if(app->NetIsHost()){if(auto* eater=ById(board,s.eater);eater && !eater->mDead)eater->DieNoLoot();}
        if(!Capture(app,"fusion_potato_preparing") || !Barrier(app,"FPT_BITTEN"))return true;
        NetTrace("SMOKE_FUSION_POTATO_UNARMED role=%s plant=%u hp=%d maxHP=300 armed=0 exploded=0 actualNativeBite=1",Role(app),s.plant,s.unarmedHealth);
        Advance(app,7);
    }
    else if(s.stage==7)
    {
        auto* mine=Mine(board);if(!Require(app,mine && mine->mNetId==s.plant && mine->mPlantHealth==s.unarmedHealth,"Potato changed during actual preparation"))return true;
        if(mine->mState==STATE_POTATO_RISING)
        {
            s.sawRise=true;
            if(app->NetIsHost() && !s.riseTick)
            {s.riseTick=board->mMainCounter;if(!Require(app,std::abs((s.riseTick-s.firstTick)-s.firstCounter)<=1,"Potato preparation clock was skipped or extended"))return true;NetTrace("SMOKE_FUSION_POTATO_PREPARATION role=host initial=%d elapsed=%d sourceRange=14,16 checkBeforeDecrement=1 nativeClock=1",s.firstCounter,s.riseTick-s.firstTick);}
            Capture(app,"fusion_potato_rising");return true;
        }
        if(mine->mState!=STATE_POTATO_ARMED)return true;
        if(!Require(app,s.sawRise && app->ReanimationTryToGet(mine->mLightReanimID),"Potato armed without observed rise/native light"))return true;
        if(app->NetIsHost() && !s.armedTick){s.armedTick=board->mMainCounter;if(!Require(app,std::abs((s.armedTick-s.riseTick)-kFusionPotatoRiseTicks)<=1,"Potato rise did not use source84ticks"))return true;NetTrace("SMOKE_FUSION_POTATO_RISE role=host elapsed=%d expected=84 sourceEvent=.8333333 nativeClock=1",s.armedTick-s.riseTick);}
        if(!Capture(app,"fusion_potato_armed") || !Barrier(app,"FPT_ARMED"))return true;
        Advance(app,8);
    }
    else if(s.stage==8)
    {
        if(app->NetIsHost() && !s.spawned && !SetupTargets(app))return true;
        if(app->NetIsHost())Send(app,s.idMessage);
        if(!ReadIds(app))return true;
        if(!TargetsMatch(app,false)){TraceTargets(app,false);return true;}
        if(!Settled() || !Capture(app,"fusion_potato_targets"))return true;
        Advance(app,9);
    }
    else if(s.stage==9)
    {
        if(app->NetIsHost())Send(app,s.idMessage);
        if(!Barrier(app,"FPT_TARGETS"))return true;
        if(app->NetIsHost() && !s.shifted)
        {
            for(size_t i=0;i<s.ids.size();++i)
            {
                auto* z=ById(board,s.ids[i]);if(!Require(app,z && !z->IsDeadOrDying(),"Potato contact activation lost a native actor"))return true;
                z->mPosX=static_cast<float>(kTargets[i].x);z->mX=kTargets[i].x;
                NetTrace("SMOKE_FUSION_POTATO_CONTACT case=%s id=%u x=%d y=%.3f nativeActor=1 fixturePositionControl=1",kTargets[i].name,z->mNetId,kTargets[i].x,z->mPosY);
            }
            if(!NoOpposingTargets(app,"contact"))return true;
            FusionPotatoArea area;
            auto* mind=ById(board,s.ids[5]);const auto rect=mind->GetZombieRect();
            if(!Require(app,TryMakeFusionPotatoArea(board->GetCellCenterX(9,2),board->GridToPixelY(9,2)+50,area) &&
                FusionPotatoBlastOverlaps(area,rect.mX,rect.mY,rect.mWidth,rect.mHeight) &&
                FusionPotatoTriggerOverlaps(area,rect.mX,rect.mY,rect.mWidth,rect.mHeight) &&
                !FusionPotatoCanTrigger(0,true,mind->mMindControlled,false,true,false) &&
                !FusionPotatoCanDamage(0,true,mind->mMindControlled),"Potato mind-control probe lost actual trigger/blast overlap or ignore rule"))return true;
            NetTrace("SMOKE_FUSION_POTATO_MIND_GEOMETRY role=host id=%u x=%.3f rect=%d,%d,%d,%d triggerOverlap=1 blastOverlap=1 mindIgnored=1 actualNativeRect=1 fixtureControl=1",
                mind->mNetId,mind->mPosX,rect.mX,rect.mY,rect.mWidth,rect.mHeight);
            s.shifted=true;
        }
        Advance(app,10);
    }
    else if(s.stage==10)
    {
        auto* mine=Mine(board);
        if(mine && mine->mState==STATE_DOINGSPECIAL)
        {
            if(!Require(app,mine->mPlantHealth==s.unarmedHealth,"Prepared Potato accepted a bite during the source delayed death"))return true;
            if(app->NetIsHost() && !s.sentExplosion)
            {
                s.explosionTick=board->mMainCounter;s.deathCounter=mine->mDisappearCountdown;
                LawnNet::NetBuffer snapshot;board->NetEncodeSnapshot(snapshot);
                if(!Require(app,snapshot.Size() && s.deathCounter>=0 && s.deathCounter<10,"Potato actual delayed-death snapshot is invalid"))return true;
                app->mNetManager->SendSnapshot(snapshot);s.sentExplosion=true;
                NetTrace("SMOKE_FUSION_POTATO_DOINGSPECIAL role=host plant=%u remaining=%d nativeActorRetained=1 actualSnapshotForced=1 sourceInvincible=1",s.plant,s.deathCounter);
            }
            Capture(app,"fusion_potato_after");return true;
        }
        if(mine)return true;
        if(app->NetIsHost() && !s.goneTick)
        {
            s.goneTick=board->mMainCounter;
            if(!Require(app,s.sentExplosion && std::abs((s.goneTick-s.explosionTick)-(s.deathCounter+1))<=1,"Potato did not retain its native actor for ten delayed-death ticks"))return true;
            NetTrace("SMOKE_FUSION_POTATO_DELAY role=host observed=%d initialRemaining=%d expected=10 nativeClock=1",s.goneTick-s.explosionTick,s.deathCounter);
        }
        if(app->NetIsClient() && !s.replayed)return true;
        if(!TargetsMatch(app,true)){TraceTargets(app,true);return true;}
        if(Samples(app)-s.purchaseSamples<2 || !Settled())return true;
        if(!Capture(app,"fusion_potato_after"))return true;
        for(size_t i=0;i<s.ids.size();++i)
        {
            const auto& t=kTargets[i];auto* z=ById(board,s.ids[i]);
            NetTrace("SMOKE_FUSION_POTATO_RESULT role=%s case=%s id=%u dead=%d body=%d first=%d second=%d mind=%d armorImageAndTrackMatch=%d nativeActor=1",
                Role(app),t.name,s.ids[i],t.dead?1:0,z?z->mBodyHealth:0,z?z->mHelmHealth:0,z?z->mShieldHealth:0,t.mind?1:0,z && !t.dead?ArmorMatches(app,z,t,true)?1:0:1);
        }
        Advance(app,11);
    }
    else if(s.stage==11)
    {
        auto* packet=Packet(board);
        if(!Require(app,board->mSunMoney==450 && packet && packet->mNetId==s.packet && packet->mRefreshTime==3000 && !Mine(board),"Potato final identity/balance/recharge drifted"))return true;
        if(!TargetsMatch(app,true) || Samples(app)<8 || !Barrier(app,"FPT_DONE") || !Settled() || !Capture(app,"fusion_potato_complete"))return true;
        NetTrace("SMOKE_FUSION_POTATO_COMPLETE role=%s epoch=%u plant=%u packet=%u sun=450 sourceCD=3000 cases=8 nativeBoard=1 secondWorld=0 collider=nativeRectAdaptation sourceCampaignComplete=0 lateJoinTested=0 samples=%u",Role(app),context->epoch,s.plant,s.packet,Samples(app));
        Advance(app,12);app->CoopSmokeWriteResult(true,nullptr);
    }
    return true;
}
}
