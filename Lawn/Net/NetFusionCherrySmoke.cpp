#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../GridItem.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionCherryRules.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../SexyAppFramework/DDInterface.h"
#include "../../Resources.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <climits>
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
    int row, x, body, first, second;
    bool mindControlled, burned;
    int afterBody, afterFirst, afterSecond;
};
// These are explicit fixture controls on real normal native actors, not source
// zombie-family implementations. Walking/ground-track rates are frozen; armor
// uses real types and actors start at the native lane anchor.
const Target kTargets[] = {
    {"inside",2,550,270,5000,5000,false,true,0,0,0},
    {"outside",2,350,270,0,0,false,false,270,0,0},
    {"row_minus",1,600,270,0,0,false,true,0,0,0},
    {"row_plus",3,600,270,0,0,false,true,0,0,0},
    {"row_two_away",0,600,270,0,0,false,false,270,0,0},
    {"mindcontrol",2,700,270,0,0,true,false,270,0,0},
    {"highbody",2,580,5000,0,0,false,false,3200,0,0},
    {"shield",2,600,2000,1000,4000,false,false,1200,0,2200}
};
struct CherryState
{
    int stage = 0, waitingOwn = 0, firstTick = 0, firstCountdown = 0, goneTick = 0, firstDraw = 0;
    uint32_t since = 0, lastChat = 0, settled = 0, introStart = 0, introLast = 0, introChecks = 0;
    uint32_t heldSamples = 0, purchaseSamples = 0;
    uint32_t lastTargetTrace = 0;
    uint16_t packet = 0, plant = 0;
    bool difficultySet = false, spawned = false, clicked = false;
    std::array<uint16_t,10> ids{};
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
    LawnNet::NetTrace("SMOKE_FUSION_CHERRY_STAGE role=%s stage=%d",Role(app),stage);
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
    if (!Require(app,app->CoopSmokeCaptureFrame(name),"Cherry actual native renderer capture failed")) return false;
    s.frames.insert(name); return true;
}
bool Click(LawnApp* app,float x,float y)
{
    SDL_Event motion{}; motion.type=SDL_EVENT_MOUSE_MOTION; motion.motion.windowID=SDL_GetWindowID(app->mSDLWindow);
    motion.motion.x=x; motion.motion.y=y;
    SDL_Event down{}; down.type=SDL_EVENT_MOUSE_BUTTON_DOWN; down.button.windowID=motion.motion.windowID;
    down.button.button=SDL_BUTTON_LEFT; down.button.clicks=1; down.button.down=true; down.button.x=x; down.button.y=y;
    SDL_Event up=down; up.type=SDL_EVENT_MOUSE_BUTTON_UP; up.button.down=false;
    return Require(app,SDL_PushEvent(&motion) && SDL_PushEvent(&down) && SDL_PushEvent(&up),"Cherry owned-window SDL queue failed");
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
        Require(app,SDL_PushEvent(&event),"Cherry Ready hover SDL queue failed"); return false;
    }
    return Click(app,x,y);
}
SeedPacket* Packet(Board* board)
{
    for (int i=0;i<board->mSeedBank->mNumPackets;++i)
    {
        auto* packet=&board->mSeedBank->mSeedPackets[i]; const auto* data=GetNativePacketData(packet);
        if (data && data->owner==1 && data->kind==2 && data->isExtra) return packet;
    }
    return nullptr;
}
Plant* Cherry(Board* board)
{
    Plant* plant=nullptr;
    while (board->mPlants.IterateNext(plant))
        if (!plant->mDead && plant->mIsOnBoard && plant->mPlantCol==9 && plant->mRow==2) return plant;
    return nullptr;
}
Zombie* FindZombie(Board* board,uint16_t id)
{
    Zombie* zombie=nullptr;
    while (board->mZombies.IterateNext(zombie)) if (zombie->mNetId==id) return zombie;
    return nullptr;
}
GridItem* FindLadder(Board* board,uint16_t id)
{
    GridItem* item=nullptr;
    while (board->mGridItems.IterateNext(item)) if (item->mNetId==id) return item;
    return nullptr;
}
bool Held(Board* board)
{
    const auto* cursor=board->mCursorObject;
    return cursor && cursor->mCursorType==CURSOR_TYPE_PLANT_FROM_BANK && cursor->mType==SEED_CHERRYBOMB &&
        cursor->mSeedBankIndex>=0 && cursor->mSeedBankIndex<board->mSeedBank->mNumPackets &&
        board->mSeedBank->mSeedPackets[cursor->mSeedBankIndex].mNetId==s.packet;
}
bool Setup(LawnApp* app)
{
    Board* board=app->mBoard;
    board->mSunCountDown=100000; board->mZombieCountDown=100000; board->mZombieCountDownStart=100000;
    for (size_t i=0;i<8;++i)
    {
        const auto& target=kTargets[i]; auto* zombie=board->AddZombieInRow(ZOMBIE_NORMAL,target.row,0);
        if (!Require(app,zombie && zombie->mNetId,"Cherry fixture native zombie allocation failed")) return false;
        const auto* body=app->ReanimationTryToGet(zombie->mBodyReanimID);
        const float nativeWalkingRate=body?body->mAnimRate:0.0f;
        const float initialOriginalRate=zombie->mOriginalAnimRate;
        zombie->mPosX=static_cast<float>(target.x); zombie->mX=target.x;
        zombie->mPosY=zombie->GetPosYBasedOnRow(target.row); zombie->mY=static_cast<int>(zombie->mPosY);
        zombie->mVelX=0; zombie->mMindControlled=target.mindControlled;
        if (target.first) { zombie->mHelmType=HELMTYPE_BUCKET; zombie->AttachHelmet(); }
        if (target.second) { zombie->mShieldType=SHIELDTYPE_DOOR; zombie->AttachShield(); }
        zombie->mBodyHealth=zombie->mBodyMaxHealth=target.body;
        zombie->mHelmHealth=zombie->mHelmMaxHealth=target.first;
        zombie->mShieldHealth=zombie->mShieldMaxHealth=target.second;
        if (i==0)
        {
            const int armor[]={zombie->mHelmHealth,zombie->mHelmMaxHealth,zombie->mShieldHealth,zombie->mShieldMaxHealth};
            zombie->mHelmMaxHealth=zombie->mShieldMaxHealth=2000000000;
            zombie->mHelmHealth=zombie->mShieldHealth=1200000000;
            const int largeHelmet=zombie->GetHelmDamageIndex(),largeShield=zombie->GetShieldDamageIndex();
            zombie->mHelmHealth=zombie->mHelmMaxHealth=zombie->mShieldHealth=zombie->mShieldMaxHealth=INT_MAX;
            const int fullHelmet=zombie->GetHelmDamageIndex(),fullShield=zombie->GetShieldDamageIndex();
            zombie->mHelmHealth=armor[0]; zombie->mHelmMaxHealth=armor[1];
            zombie->mShieldHealth=armor[2]; zombie->mShieldMaxHealth=armor[3];
            if (!Require(app,largeHelmet==1 && largeShield==1 && fullHelmet==0 && fullShield==0,
                "Native armor damage index overflows at valid large snapshot health")) return false;
            LawnNet::NetTrace("SMOKE_FUSION_CHERRY_ARMOR_INDEX current=1200000000 maximum=2000000000 helmet=%d shield=%d actualNativeGetters=1 restoredBeforeSnapshot=1",largeHelmet,largeShield);
            LawnNet::NetTrace("SMOKE_FUSION_CHERRY_ARMOR_INDEX current=%d maximum=%d helmet=%d shield=%d actualNativeGetters=1 restoredBeforeSnapshot=1",INT_MAX,INT_MAX,fullHelmet,fullShield);
        }
        // StartWalkAnim computes the live walking rate but may leave original0.
        // Preserve that actual native rate, then freeze only the ground track.
        if (body && std::isfinite(nativeWalkingRate) && nativeWalkingRate>0)
            zombie->SetAnimRate(nativeWalkingRate);
        zombie->ApplyAnimRate(0.0f);
        LawnNet::NetTrace("SMOKE_FUSION_CHERRY_FREEZE case=%s id=%u body=%d nativeWalkingRate=%.6f initialOriginalRate=%.6f originalRate=%.6f liveRate=%.6f velocity=%.6f rateSource=nativeWalking fixtureControl=1",
            target.name,zombie->mNetId,body?1:0,nativeWalkingRate,initialOriginalRate,zombie->mOriginalAnimRate,body?body->mAnimRate:-1.0f,zombie->mVelX);
        if (!Require(app,body && std::isfinite(nativeWalkingRate) && nativeWalkingRate>0 && body->mAnimRate==0 &&
            zombie->mOriginalAnimRate==nativeWalkingRate,
            "Cherry fixture did not freeze the native ground track safely")) return false;
        s.ids[i]=zombie->mNetId;
        LawnNet::NetTrace("SMOKE_FUSION_CHERRY_SETUP case=%s id=%u row=%d x=%d body=%d first=%d second=%d mind=%d nativeType=0 zeroVelocity=1 zeroGroundRate=1 nativeLaneAnchor=1 y=%.3f originalRate=%.3f fixtureControl=1",
            target.name,s.ids[i],target.row,target.x,target.body,target.first,target.second,target.mindControlled?1:0,zombie->mPosY,zombie->mOriginalAnimRate);
    }
    GridItem* inside=board->AddALadder(8,2),*outside=board->AddALadder(6,2);
    if (!Require(app,inside && outside && inside->mNetId && outside->mNetId,"Cherry fixture native ladder allocation failed")) return false;
    s.ids[8]=inside->mNetId; s.ids[9]=outside->mNetId;
    std::ostringstream message; message<<"FCH_IDS"; for (auto id:s.ids) message<<' '<<id; s.idMessage=message.str();
    s.spawned=true; return true;
}
bool ReadIds(LawnApp* app)
{
    if (app->NetIsHost()) return s.spawned;
    for (const auto& line:app->mNetManager->mChatLines)
    {
        if (line.mSlot!=0 || line.mText.rfind("FCH_IDS ",0)!=0) continue;
        std::istringstream input(line.mText.substr(8)); std::set<unsigned> unique;
        for (auto& id:s.ids)
        {
            unsigned value=0;
            if (!Require(app,!!(input>>value) && value && value<=65535 && unique.insert(value).second,"Cherry fixture target identities are invalid")) return false;
            id=static_cast<uint16_t>(value);
        }
        std::string extra; if (!Require(app,!(input>>extra),"Cherry fixture target identities have trailing fields")) return false;
        s.idMessage=line.mText; return true;
    }
    return false;
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
bool ArmorMatches(LawnApp* app,Zombie* zombie,const Target& target,bool after)
{
    auto* body=app->ReanimationTryToGet(zombie->mBodyReanimID);
    if (target.first && !ArmorTrackMatches(body,"anim_bucket",after?RENDER_GROUP_HIDDEN:RENDER_GROUP_HELMET,after?-1:0,
        {IMAGE_REANIM_ZOMBIE_BUCKET1,IMAGE_REANIM_ZOMBIE_BUCKET2,IMAGE_REANIM_ZOMBIE_BUCKET3})) return false;
    if (target.second && !ArmorTrackMatches(body,"anim_screendoor",RENDER_GROUP_SHIELD,after?1:0,
        {IMAGE_REANIM_ZOMBIE_SCREENDOOR1,IMAGE_REANIM_ZOMBIE_SCREENDOOR2,IMAGE_REANIM_ZOMBIE_SCREENDOOR3})) return false;
    return true;
}
void TraceArmor(LawnApp* app,bool after)
{
    for (size_t i: {size_t(0),size_t(7)})
    {
        const auto& target=kTargets[i]; auto* zombie=FindZombie(app->mBoard,s.ids[i]);
        if (!zombie || zombie->IsDeadOrDying()) continue;
        auto* body=app->ReanimationTryToGet(zombie->mBodyReanimID);
        const auto* helmet=body && body->TrackExists("anim_bucket")?body->GetTrackInstanceByName("anim_bucket"):nullptr;
        const auto* shield=body && body->TrackExists("anim_screendoor")?body->GetTrackInstanceByName("anim_screendoor"):nullptr;
        LawnNet::NetTrace("SMOKE_FUSION_CHERRY_ARMOR role=%s after=%d case=%s id=%u body=%d helmetGroup=%d helmetForceHidden=%d shieldGroup=%d shieldForceHidden=%d imageAndTrackMatch=%d expectedDoorImageStage=%d",
            Role(app),after?1:0,target.name,s.ids[i],body?1:0,helmet?helmet->mRenderGroup:-99,helmet && helmet->mForceDontRender?1:0,
            shield?shield->mRenderGroup:-99,shield && shield->mForceDontRender?1:0,ArmorMatches(app,zombie,target,after)?1:0,after?1:0);
    }
}
bool TargetsMatch(LawnApp* app,bool after)
{
    for (size_t i=0;i<8;++i)
    {
        const auto& target=kTargets[i]; auto* zombie=FindZombie(app->mBoard,s.ids[i]);
        if (after && target.burned)
        { if (zombie && !zombie->IsDeadOrDying()) return false; continue; }
        if (!zombie || zombie->IsDeadOrDying() || zombie->mZombieType!=ZOMBIE_NORMAL || zombie->mRow!=target.row ||
            std::abs(zombie->mPosX-target.x)>.01f || std::abs(zombie->mPosY-zombie->GetPosYBasedOnRow(target.row))>.01f ||
            zombie->mVelX!=0 || zombie->mOriginalAnimRate<=0 ||
            zombie->mMindControlled!=target.mindControlled || zombie->mBodyHealth!=(after?target.afterBody:target.body) ||
            zombie->mHelmHealth!=(after?target.afterFirst:target.first) || zombie->mShieldHealth!=(after?target.afterSecond:target.second)) return false;
        if (zombie->mHelmType!=((after?target.afterFirst:target.first)>0?HELMTYPE_BUCKET:HELMTYPE_NONE) ||
            zombie->mShieldType!=((after?target.afterSecond:target.second)>0?SHIELDTYPE_DOOR:SHIELDTYPE_NONE)) return false;
        if (!ArmorMatches(app,zombie,target,after)) return false;
    }
    const auto* inside=FindLadder(app->mBoard,s.ids[8]); const auto* outside=FindLadder(app->mBoard,s.ids[9]);
    return (after?(!inside || inside->mDead):(inside && !inside->mDead)) && outside && !outside->mDead;
}
void TraceTargets(LawnApp* app,bool after)
{
    const uint32_t now=GetTickCount();
    if (s.lastTargetTrace && now-s.lastTargetTrace<1000) return;
    s.lastTargetTrace=now;
    auto* board=app->mBoard; FusionCherryArea area;
    TryMakeFusionCherryArea(board->GetCellCenterX(9,2),board->GridToPixelY(9,2)+50,area);
    for (size_t i=0;i<8;++i)
    {
        const auto& target=kTargets[i]; auto* zombie=FindZombie(board,s.ids[i]);
        if (!zombie)
        { LawnNet::NetTrace("SMOKE_FUSION_CHERRY_TARGET_WAIT role=%s after=%d case=%s id=%u missing=1",Role(app),after?1:0,target.name,s.ids[i]); continue; }
        const auto* body=app->ReanimationTryToGet(zombie->mBodyReanimID);
        const auto rect=zombie->GetZombieRect();
        LawnNet::NetTrace("SMOKE_FUSION_CHERRY_TARGET_WAIT role=%s after=%d case=%s id=%u dead=%d phase=%d row=%d x=%.3f y=%.3f expectedX=%d expectedY=%.3f velocity=%.3f originalRate=%.3f groundRate=%.3f rect=%d,%d,%d,%d overlap=%d body=%d first=%d second=%d firstType=%d secondType=%d mind=%d",
            Role(app),after?1:0,target.name,s.ids[i],zombie->IsDeadOrDying()?1:0,static_cast<int>(zombie->mZombiePhase),zombie->mRow,
            zombie->mPosX,zombie->mPosY,target.x,zombie->GetPosYBasedOnRow(target.row),zombie->mVelX,zombie->mOriginalAnimRate,body?body->mAnimRate:-1.0f,
            rect.mX,rect.mY,rect.mWidth,rect.mHeight,FusionCherryRectOverlaps(area,rect.mX,rect.mY,rect.mWidth,rect.mHeight)?1:0,
            zombie->mBodyHealth,zombie->mHelmHealth,zombie->mShieldHealth,static_cast<int>(zombie->mHelmType),static_cast<int>(zombie->mShieldType),zombie->mMindControlled?1:0);
    }
    TraceArmor(app,after);
}
bool Pool(LawnApp* app, const NativeContext& context, bool trace=false)
{
    static uint32_t tracedEpoch=0;
    const bool record=trace && tracedEpoch!=context.epoch;
    const auto& state=context.selection.State();
    const size_t target=(std::min)(kNativeFusionBankCapacity,context.eligible.size());
    if (!Require(app,state.activeMask==3 && state.targetCount==target &&
        state.quotas[0]+state.quotas[1]==target && state.quotas[0]>=4 && state.quotas[1]>=4,
        "Native fixture live-pool target/quotas do not retain mandatory baseline coverage")) return false;
    bool mandatory[4][2]={};
    const FusionPlantId baseline[]={0,1,2,4};
    std::set<std::pair<FusionPlantId,bool>> identities;
    for (size_t i=0;i<context.eligible.size();++i)
    {
        const auto& card=context.eligible[i]; const auto* definition=FusionFindPlant(card.kind);
        FusionBankEligibleCard sourceCard;
        if (!Require(app,definition && definition->hasStats && GetNativeSeedForPlant(card.kind)!=SEED_NONE &&
            identities.emplace(card.kind,card.isExtra).second &&
            TryMakeFusionEligibleCard(card.kind,card.isExtra,sourceCard) && card==sourceCard &&
            card.rechargeSeconds==definition->rechargeSeconds && definition->maxHealth>0,
            "Native fixture eligibility disagrees with source definitions/aliases or repeats an identity")) return false;
        for (size_t j=0;j<4;++j) if (card.kind==baseline[j]) mandatory[j][card.isExtra?1:0]=true;
        if (record) LawnNet::NetTrace("SMOKE_FUSION_CHERRY_ELIGIBLE role=%s index=%zu kind=%u extra=%d baseCost=%d sourceCD=%d maxHP=%d alias=%d",
            Role(app),i,card.kind,card.isExtra?1:0,card.baseCost,static_cast<int>(std::lround(card.rechargeSeconds*100.0f)),
            definition->maxHealth,static_cast<int>(GetNativeSeedForPlant(card.kind)));
    }
    for (const auto& pair:mandatory) if (!Require(app,pair[0] && pair[1],"Native fixture lost mandatory0/1/2/4 normal/extra eligibility")) return false;
    if (record) LawnNet::NetTrace("SMOKE_FUSION_CHERRY_POOL_LAYOUT role=%s eligible=%zu target=%u quota0=%u quota1=%u mandatory=0,1,2,4 unfiltered=1",
        Role(app),context.eligible.size(),state.targetCount,state.quotas[0],state.quotas[1]);
    if (record) tracedEpoch=context.epoch;
    return true;
}
std::vector<int> ManualChoices(const NativeContext& context, int owner)
{
    std::vector<int> choices;
    for (size_t i=0;i<context.eligible.size();++i)
    {
        const auto& card=context.eligible[i];
        if (card.isExtra!=(owner==1)) continue;
        const auto& selected=context.selection.State().packets;
        if (std::none_of(selected.begin(),selected.end(),[&](const auto& packet) {
            return packet.kind==card.kind && packet.isExtra==card.isExtra;
        })) choices.push_back(static_cast<int>(i));
    }
    const auto priority=[](FusionPlantId kind) { return kind==0?0:(kind==1?1:(kind==2?2:(kind==4?3:4))); };
    std::stable_sort(choices.begin(),choices.end(),[&](int a,int b) {
        return priority(context.eligible[a].kind)<priority(context.eligible[b].kind);
    });
    return choices;
}

}

namespace LawnNet
{
bool NetSmokeUpdateFusionCherry(LawnApp* app)
{
    const char* native=std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE"),*enabled=std::getenv("PVZ_COOP_SMOKE_FUSION_CHERRY");
    if (!app || !native || std::strcmp(native,"1") || !enabled || std::strcmp(enabled,"1") || !app->CoopSmokeActive() ||
        app->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION) return false;
    if (app->mCoopSmokeResultWritten) return true;
    if (!s.since) s.since=GetTickCount();
    if (GetTickCount()-s.since>(s.stage<=3?60000U:20000U))
    { char error[100]; sprintf_s(error,"Cherry fixture deadline stage=%d role=%s",s.stage,Role(app)); app->CoopSmokeFail(error); return true; }
    if (!app->mNetManager || !app->mNetManager->IsActive()) return true;
    app->mCoopSmokePeerReady=true;
    if (s.stage==0)
    {
        if (app->NetIsHost() && !s.difficultySet)
        {
            if (!Require(app,!app->mBoard && !app->mSeedChooserScreen,"Cherry difficulty requires a clean fixture lobby")) return true;
            app->mNetManager->SetDifficulty(10); s.difficultySet=true;
        }
        if (app->mNetManager->GetPlayerCount()!=2 || (app->NetIsClient() && !app->mNetManager->IsConnectedToHost())) return true;
        if (app->NetIsClient()) { Advance(app,1); Send(app,"FCH_START_READY"); }
        else if (Has(app,1,"FCH_START_READY"))
        { Advance(app,1); std::string error; if (!app->StartFusionValidation(&error)) app->CoopSmokeFail(error.c_str()); }
        return true;
    }
    if (!app->mBoard || !IsNativeFusionBoard(app->mBoard))
    { if (s.stage==1 && app->NetIsClient()) Send(app,"FCH_START_READY"); return true; }
    Board* board=app->mBoard; const auto* context=GetNativeContext(board);
    if (!Require(app,context && !app->mFusionRuntime && context->validation && board->GetColumnCount()==10,
        "Cherry fixture lost the real native Fusion Board")) return true;
    const auto& state=context->selection.State(); auto* chooser=app->mSeedChooserScreen;
    if (s.stage==1)
    {
        if (!chooser || !chooser->mMouseVisible || !board->mCutScene->mSeedChoosing || chooser->mSeedsInFlight) return true;
        if (!Require(app,state.phase==FusionBankPhase::Choosing && Pool(app,*context,true) &&
            state.difficulty==10,"Cherry chooser does not have its observed source pool/quota")) return true;
        if (!Capture(app,"fusion_cherry_chooser")) return true;
        const int owner=app->mNetManager->GetMySlot(); int own=0;
        for (const auto& packet:state.packets) own+=packet.owner==owner;
        if (own<s.waitingOwn) return true;
        if (own<state.quotas[owner])
        {
            const auto choices=ManualChoices(*context,owner);
            if (!Require(app,choices.size()>=state.quotas[owner]-own,"Cherry chooser cannot fill its remaining personal quota")) return true;
            int x=0,y=0; chooser->GetSeedPositionInChooser(choices.front(),x,y);
            if (Click(app,static_cast<float>(chooser->mX+x+25),static_cast<float>(chooser->mY+y+35-static_cast<int>(chooser->mScrollbar->mScrollValue)))) s.waitingOwn=own+1;
            return true;
        }
        if (state.packets.size()!=state.targetCount || !Packet(board) || !Capture(app,"fusion_cherry_picked")) return true;
        s.packet=Packet(board)->mNetId; Advance(app,2);
    }
    else if (s.stage==2)
    {
        if (state.phase!=FusionBankPhase::Choosing) { Advance(app,3); return true; }
        if (chooser && !chooser->mSeedsInFlight && Button(app,chooser->mStartButton)) Advance(app,3);
    }
    else if (s.stage==3)
    {
        if (state.phase==FusionBankPhase::Intro)
        {
            if (!s.introStart) s.introStart=GetTickCount(); s.introLast=GetTickCount(); ++s.introChecks;
            if (s.introLast-s.introStart>=300) Capture(app,"fusion_cherry_intro"); return true;
        }
        if (state.phase!=FusionBankPhase::Playing || app->mGameScene!=SCENE_PLAYING || chooser) return true;
        if (!Require(app,s.introChecks>=10 && s.introLast-s.introStart>=1000 && board->mSunMoney==500 &&
            s.frames.contains("fusion_cherry_intro"),"Cherry bypassed actual native Ready/full intro")) return true;
        ReanimatorEnsureDefinitionLoaded(REANIM_LADDER,true);
        LawnNet::NetTrace("SMOKE_FUSION_CHERRY_PLAYING role=%s epoch=%u coopDifficulty=10 sourceDifficulty=2 nativeBoard=1 secondWorld=0",Role(app),context->epoch);
        Advance(app,4);
    }
    else if (s.stage==4)
    {
        if (!Barrier(app,"FCH_PLAY")) return true;
        if (app->NetIsHost() && !s.spawned && !Setup(app)) return true;
        if (app->NetIsHost()) Send(app,s.idMessage);
        if (!ReadIds(app)) return true;
        if (!TargetsMatch(app,false)) { TraceTargets(app,false); return true; }
        if (!Settled() || !Capture(app,"fusion_cherry_targets")) return true;
        TraceArmor(app,false);
        Advance(app,5);
    }
    else if (s.stage==5)
    {
        if (app->NetIsHost()) Send(app,s.idMessage);
        if (!Barrier(app,"FCH_TARGETS_READY")) return true;
        Advance(app,6);
    }
    else if (s.stage==6)
    {
        if (app->NetIsHost()) { Advance(app,7); return true; }
        auto* packet=Packet(board); const auto* data=GetNativePacketData(packet);
        if (!Require(app,packet && data && packet->mNetId==s.packet && packet->mPacketType==SEED_CHERRYBOMB &&
            data->price==300 && data->rechargeSeconds==50,"Cherry did not use the real Extra packet price/recharge")) return true;
        if (!s.clicked)
        {
            s.heldSamples=Samples(app); const auto* bank=board->mSeedBank;
            if (BoardClick(app,static_cast<float>(bank->mX+packet->mX+packet->mOffsetX+packet->mWidth/2),
                static_cast<float>(bank->mY+packet->mY+packet->mHeight/2))) s.clicked=true;
            return true;
        }
        if (!Held(board) || Samples(app)<=s.heldSamples || !Settled()) return true;
        if (!Require(app,board->mSunMoney==500 && packet->mActive && !packet->mRefreshing && !Cherry(board),
            "Cherry pickup echo spent currency/cooldown before placement") || !Capture(app,"fusion_cherry_held")) return true;
        LawnNet::NetTrace("SMOKE_FUSION_CHERRY_HELD role=client packet=%u charge=0 cooldown=0 snapshotEcho=1 nativeSDL=1",s.packet);
        Advance(app,7);
        BoardClick(app,static_cast<float>(board->GetCellCenterX(9,2)),static_cast<float>(board->GridToPixelY(9,2)+50));
    }
    else if (s.stage==7)
    {
        Plant* plant=Cherry(board); auto* packet=Packet(board); const auto* data=GetNativePlantData(plant);
        if (!plant || !data) return true;
        if (!Require(app,data->kind==2 && plant->mSeedType==SEED_CHERRYBOMB && plant->mPlantMaxHealth==300 &&
            data->owner==1 && data->packetId==s.packet && data->randomOrigin==0 && data->firstParent==kFusionNoPlant &&
            data->secondParent==kFusionNoPlant && board->mSunMoney==200 && packet && packet->mNetId==s.packet &&
            packet->mRefreshing && !packet->mActive && packet->mRefreshTime==5000,"Cherry native purchase lost identity, price, health or source cooldown")) return true;
        if (!s.plant)
        {
            s.plant=plant->mNetId; s.purchaseSamples=Samples(app); s.firstDraw=app->mDrawCount;
            if (app->NetIsHost())
            {
                s.firstTick=board->mMainCounter; s.firstCountdown=plant->mDoSpecialCountdown;
                if (!Require(app,s.firstCountdown>0 && s.firstCountdown<=100,"Cherry native initial fuse is outside100ticks")) return true;
            }
        }
        if (!Require(app,plant->mNetId==s.plant,"Cherry identity changed while awaiting actual purchase rendering")) return true;
        if (app->mDrawCount-s.firstDraw<2) return true;
        if (!Capture(app,"fusion_cherry_planted")) return true;
        LawnNet::NetTrace("SMOKE_FUSION_CHERRY_PLANTED_RENDER role=%s firstDraw=%d currentDraw=%d freshDraws=%d",Role(app),s.firstDraw,app->mDrawCount,app->mDrawCount-s.firstDraw);
        LawnNet::NetTrace("SMOKE_FUSION_CHERRY_PURCHASE role=%s plant=%u packet=%u kind=2 owner=1 extra=1 column=9 row=2 price=300 sun=200 sourceCD=5000 nativeActor=1 nativeSDL=1",Role(app),s.plant,s.packet);
        Advance(app,8);
    }
    else if (s.stage==8)
    {
        if (Cherry(board)) return true;
        if (app->NetIsHost() && !s.goneTick)
        {
            s.goneTick=board->mMainCounter;
            if (!Require(app,s.goneTick-s.firstTick>=s.firstCountdown-1 && s.goneTick-s.firstTick<=s.firstCountdown+1,
                "Cherry disappeared before/after the native100tick fuse")) return true;
            LawnNet::NetTrace("SMOKE_FUSION_CHERRY_FUSE role=host initial=%d observedTicks=%d totalTicks=100 clock=nativeBoard",s.firstCountdown,s.goneTick-s.firstTick);
        }
        if (!TargetsMatch(app,true)) { TraceTargets(app,true); return true; }
        if (Samples(app)-s.purchaseSamples<2 || !Settled(150)) return true;
        auto* packet=Packet(board);
        if (!Require(app,board->mSunMoney==200 && packet && packet->mNetId==s.packet && packet->mRefreshing &&
            packet->mRefreshTime==5000 && packet->mRefreshCounter>0,"Cherry explosion charged twice or reset cooldown")) return true;
        if (!Capture(app,"fusion_cherry_after")) return true;
        TraceArmor(app,true);
        for (size_t i=0;i<8;++i)
        {
            const auto& target=kTargets[i]; auto* zombie=FindZombie(board,s.ids[i]);
            LawnNet::NetTrace("SMOKE_FUSION_CHERRY_RESULT role=%s case=%s id=%u dead=%d body=%d first=%d second=%d mind=%d firstType=%d secondType=%d nativeActor=1 sourceDifficulty=2",
                Role(app),target.name,s.ids[i],target.burned?1:0,zombie?zombie->mBodyHealth:0,zombie?zombie->mHelmHealth:0,
                zombie?zombie->mShieldHealth:0,target.mindControlled?1:0,zombie?static_cast<int>(zombie->mHelmType):0,
                zombie?static_cast<int>(zombie->mShieldType):0);
        }
        LawnNet::NetTrace("SMOKE_FUSION_CHERRY_LADDER role=%s inside=%u removed=1 outside=%u retained=1 sourceCollider=1 nativeGridItems=1",Role(app),s.ids[8],s.ids[9]);
        Advance(app,9);
    }
    else if (s.stage==9)
    {
        if (!TargetsMatch(app,true) || !Barrier(app,"FCH_DONE") || Samples(app)<8 || !Settled()) return true;
        if (!Capture(app,"fusion_cherry_complete")) return true;
        LawnNet::NetTrace("SMOKE_FUSION_CHERRY_COMPLETE role=%s epoch=%u plant=%u packet=%u sun=200 sourceCD=5000 cases=8 nativeBoard=1 secondWorld=0 collider=nativeRectAdaptation sourceCampaignComplete=0 samples=%u",Role(app),context->epoch,s.plant,s.packet,Samples(app));
        Advance(app,10); app->CoopSmokeWriteResult(true,nullptr);
    }
    return true;
}
}
