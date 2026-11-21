#include "../../LawnApp.h"
#include "NetManager.h"
#include "NetGameplayPolicy.h"
#include "../Board.h"
#include "../Plant.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionNativeWire.h"
#include "../Fusion/FusionWire.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace
{
using namespace LawnFusion;
struct MysteryState
{
    int stage = 0, difficulty = 0, hostPick = 0, price = 0, recharge = 0, firstCounter = -1;
    uint32_t since = 0, settled = 0, lastChat = 0, introStart = 0, introLast = 0, introChecks = 0;
    uint32_t pickupSamples = 0, purchaseSamples = 0;
    uint32_t oldEpoch = 0, oldInputEpoch = 0, freshInputEpoch = 0, failedIntroChecks = 0, failedIntroSpan = 0;
    uint16_t purchasedPlant = 0;
    std::vector<uint16_t> manual, firstSelection, firstNative, finalSelection, finalNative;
    std::vector<FusionBankPacket> manualCards;
    size_t purchaseIndex = 0;
    bool queued = false;
    bool recovered = false, restartLogged = false;
    std::set<std::string> frames;
} s;

const char* Role(LawnApp* app) { return app->NetIsHost() ? "host" : "client"; }
uint32_t Samples(LawnApp* app) { return app->NetIsHost() ? app->mCoopSmokeSnapshotsSent : app->mCoopSmokeSnapshotsApplied; }
bool Require(LawnApp* app, bool condition, const char* error)
{ if (!condition) app->CoopSmokeFail(error); return condition; }
void Advance(LawnApp* app, int stage)
{
    s.stage = stage; s.since = GetTickCount(); s.settled = 0; s.queued = false;
    LawnNet::NetTrace("SMOKE_FUSION_NATIVE_MYSTERY_STAGE role=%s stage=%d", Role(app), stage);
}
bool Settled(uint32_t duration = 300)
{ if (!s.settled) s.settled = GetTickCount(); return GetTickCount() - s.settled >= duration; }
bool RngFailureCase()
{ const char* fault = std::getenv("PVZ_COOP_SMOKE_MYSTERY_FAULT"); return fault && !std::strcmp(fault, "RngFailure"); }
std::string AttemptName(const std::string& name) { return s.recovered ? "recovery_"+name : name; }
bool Has(LawnApp* app, int slot, const std::string& message)
{ for (const auto& line : app->mNetManager->mChatLines) if (line.mSlot == slot && line.mText == AttemptName(message)) return true; return false; }
void Send(LawnApp* app, const std::string& message)
{
    if (Has(app, app->mNetManager->GetMySlot(), message) || (s.lastChat && GetTickCount()-s.lastChat < 700)) return;
    s.lastChat = GetTickCount(); app->mNetManager->SendChat(AttemptName(message));
}
bool Barrier(LawnApp* app, const char* message)
{
    Send(app, message);
    const std::string release = std::string(message)+"_GO";
    if (app->NetIsClient()) return Has(app, 0, release);
    if (!Has(app, 1, message)) return false;
    // Only release a client action once the host has observed the checkpoint.
    // A symmetric two-ACK barrier can deliver OFF in the same pump as its ACK,
    // erasing pending cards before the host's old stage gets to inspect them.
    Send(app, release);
    return Has(app, 0, release);
}
bool Capture(LawnApp* app, const char* name)
{
    const std::string key = AttemptName(name);
    if (s.frames.contains(key)) return true;
    const std::string file = std::string("fusion_native_mystery_")+key;
    if (!Require(app, app->CoopSmokeCaptureFrame(file.c_str()), "Native mystery renderer capture failed")) return false;
    s.frames.insert(key);
    LawnNet::NetTrace("SMOKE_FUSION_NATIVE_MYSTERY_FRAME role=%s step=%s nativeSDL=1 capture=1", Role(app), key.c_str());
    return true;
}
bool Click(LawnApp* app, float x, float y)
{
    SDL_Event motion{}; motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.windowID = SDL_GetWindowID(app->mSDLWindow); motion.motion.x = x; motion.motion.y = y;
    SDL_Event down{}; down.type = SDL_EVENT_MOUSE_BUTTON_DOWN; down.button.windowID = motion.motion.windowID;
    down.button.button = SDL_BUTTON_LEFT; down.button.clicks = 1; down.button.down = true; down.button.x = x; down.button.y = y;
    SDL_Event up = down; up.type = SDL_EVENT_MOUSE_BUTTON_UP; up.button.down = false;
    return Require(app, SDL_PushEvent(&motion) && SDL_PushEvent(&down) && SDL_PushEvent(&up), "Native mystery SDL click queue failed");
}
bool Button(LawnApp* app, GameButton* button)
{
    if (!button || button->mDisabled || button->mBtnNoDraw) return false;
    const float x = button->mX+button->mWidth*0.5f, y = button->mY+button->mHeight*0.5f;
    if (!button->IsMouseOver())
    {
        SDL_Event motion{}; motion.type = SDL_EVENT_MOUSE_MOTION;
        motion.motion.windowID = SDL_GetWindowID(app->mSDLWindow); motion.motion.x = x; motion.motion.y = y;
        Require(app, SDL_PushEvent(&motion), "Native mystery SDL hover queue failed");
        return false;
    }
    LawnNet::NetTrace("SMOKE_FUSION_NATIVE_MYSTERY_BUTTON role=%s stage=%d hoverObserved=1 nativeSDL=1", Role(app), s.stage);
    return Click(app, x, y);
}
SeedPacket* ByNative(Board* board, uint16_t id)
{
    if (!board || !board->mSeedBank) return nullptr;
    for (int i = 0; i < board->mSeedBank->mNumPackets; ++i)
        if (board->mSeedBank->mSeedPackets[i].mNetId == id) return &board->mSeedBank->mSeedPackets[i];
    return nullptr;
}
SeedPacket* BySelection(Board* board, uint16_t id)
{
    if (!board || !board->mSeedBank) return nullptr;
    for (int i = 0; i < board->mSeedBank->mNumPackets; ++i)
    {
        SeedPacket* packet = &board->mSeedBank->mSeedPackets[i];
        const auto* data = GetNativePacketData(packet);
        if (data && data->selectionId == id) return packet;
    }
    return nullptr;
}
std::vector<FusionBankPacket> Owned(const NativeContext& context, int owner)
{
    std::vector<FusionBankPacket> result;
    for (const auto& packet : context.selection.State().packets) if (packet.owner == owner) result.push_back(packet);
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    return result;
}
bool Manual(LawnApp* app, const NativeContext& context)
{
    const auto cards = Owned(context, 0);
    if (cards.size() != context.selection.State().quotas[0]) return false;
    if (!Require(app,s.manual.size()==cards.size(),"Manual native quota changed within the activity")) return false;
    for (size_t index = 0; index < cards.size(); ++index)
    {
        const auto& selected=cards[index]; const auto kind=selected.kind;
        if (!Require(app,!selected.isExtra && !selected.mysteryPending && selected.randomOrigin==0,"Host manual identities changed during client mystery")) return false;
        SeedPacket* packet = BySelection(app->mBoard, selected.id);
        const auto* data = GetNativePacketData(packet);
        if (!Require(app, packet && data && data->owner == 0 && data->kind == kind && !data->isExtra && !packet->IsMysteryPending() &&
            data->selectionId==selected.id && data->price==selected.price && data->rechargeSeconds==selected.rechargeSeconds && !data->randomOrigin,
            "Host manual real packet is missing or changed source metadata")) return false;
        if (!s.manual[index]) { s.manual[index] = packet->mNetId; s.manualCards[index]=selected; }
        if (!Require(app, s.manual[index] == packet->mNetId && s.manualCards[index]==selected, "Client mystery replaced a host manual packet identity")) return false;
    }
    for (FusionPlantId kind:{FusionPlantId(0),FusionPlantId(1),FusionPlantId(2),FusionPlantId(4)})
        if (!Require(app,std::any_of(cards.begin(),cards.end(),[&](const auto& p){return p.kind==kind && !p.isExtra;}),
            "Host manual bank lost mandatory0/1/2/4 coverage")) return false;
    return true;
}
bool Hidden(LawnApp* app, const NativeContext& context)
{
    const auto cards = Owned(context, 1);
    if (!Require(app, cards.size() == context.selection.State().quotas[1], "Native mystery does not have its complete pending owner1 quota")) return false;
    for (const auto& selected : cards)
    {
        SeedPacket* packet = BySelection(app->mBoard, selected.id);
        const auto* data = GetNativePacketData(packet);
        if (!Require(app, selected.mysteryPending && selected.kind == kFusionNoPlant && !selected.isExtra &&
            selected.price == 0 && selected.rechargeSeconds == 0 && selected.remainingRechargeSeconds == 0 && selected.randomOrigin == s.difficulty &&
            packet && data && data->kind == kFusionNoPlant && !data->isExtra && data->price == 0 && data->rechargeSeconds == 0 &&
            data->owner == 1 && data->randomOrigin == s.difficulty && packet->IsMysteryPending() && !packet->mActive &&
            packet->mRefreshTime == 0 && packet->mRefreshCounter == 0 && !packet->mRefreshing,
            "Native mystery exposed kind/extra/price/recharge before gameplay")) return false;
    }
    return true;
}
Plant* At(Board* board)
{
    Plant* plant = nullptr;
    while (board->mPlants.IterateNext(plant))
        if (!plant->mDead && plant->mIsOnBoard && plant->mPlantCol == 3 && plant->mRow == 2) return plant;
    return nullptr;
}
bool Held(Board* board, uint16_t id)
{
    const auto* cursor = board->mCursorObject;
    if (!cursor || !board->mSeedBank || cursor->mCursorType != CURSOR_TYPE_PLANT_FROM_BANK ||
        cursor->mSeedBankIndex < 0 || cursor->mSeedBankIndex >= board->mSeedBank->mNumPackets) return false;
    const auto& packet = board->mSeedBank->mSeedPackets[cursor->mSeedBankIndex];
    return packet.mNetId == id && cursor->mType == packet.mPacketType;
}
bool Reveal(LawnApp* app, const NativeContext& context)
{
    const auto cards = Owned(context, 1);
    if (!Require(app, cards.size() == context.selection.State().quotas[1] && Manual(app, context), "Native reveal lost bank identities")) return false;
    for (size_t i = 0; i < cards.size(); ++i)
        for (size_t j = 0; j < i; ++j)
            if (!Require(app, cards[i].kind != cards[j].kind || cards[i].isExtra != cards[j].isExtra, "Native mystery repeated an owner card identity")) return false;
    int exactOverlap = 0;
    for (size_t i = 0; i < cards.size(); ++i)
    {
        const auto& selected = cards[i];
        SeedPacket* packet = BySelection(app->mBoard, selected.id);
        const auto* data = GetNativePacketData(packet);
        const auto* definition = FusionFindPlant(selected.kind);
        if (!Require(app, selected.id == s.finalSelection[i] && packet && packet->mNetId == s.finalNative[i] && data &&
            !selected.mysteryPending && definition && definition->hasStats &&
            std::any_of(context.eligible.begin(),context.eligible.end(),[&](const auto& card){return card.kind==selected.kind && card.isExtra==selected.isExtra;}) &&
            data->kind == selected.kind && data->isExtra == selected.isExtra && data->owner == 1 && data->randomOrigin == s.difficulty &&
            packet->mRandomDiscountLevel == s.difficulty && !packet->IsMysteryPending() &&
            data->rechargeSeconds == definition->rechargeSeconds,
            "Native START_PLAYING reveal is partial, changed ids or lost intrinsic origin")) return false;
        FusionBankEligibleCard sourceCard;
        if (!Require(app, TryMakeFusionEligibleCard(data->kind,data->isExtra,sourceCard), "Native random packet has invalid source card policy")) return false;
        const int expected = LawnNet::DiscountedPacketCost(sourceCard.baseCost, s.difficulty, 0);
        if (!Require(app, selected.price == expected && data->price == expected && app->mBoard->GetSeedPacketCost(*packet) == expected,
            "Native random packet price did not use source extra cost and intrinsic difficulty discount")) return false;
        const auto manual=Owned(context,0);
        exactOverlap += std::any_of(manual.begin(),manual.end(),[&](const auto& p){return p.kind==data->kind && p.isExtra==data->isExtra;});
        LawnNet::NetTrace("SMOKE_FUSION_NATIVE_MYSTERY_REVEAL role=%s index=%zu sid=%u packet=%u kind=%u extra=%d origin=%u price=%d cd=%.3f stableIds=1 atomic=1 nativePacket=1",
            Role(app), i, selected.id, packet->mNetId, data->kind, data->isExtra ? 1 : 0, data->randomOrigin, data->price, data->rechargeSeconds);
    }
    const auto manual=Owned(context,0);
    size_t available=0;
    for (const auto& selected:manual)
        available+=std::any_of(context.eligible.begin(),context.eligible.end(),[&](const auto& card){return card.kind==selected.kind && card.isExtra==selected.isExtra;});
    LawnNet::NetTrace("SMOKE_FUSION_NATIVE_MYSTERY_POOL role=%s eligible=%zu hostManualKinds=%zu baseKindOverlap=%zu exactIdentityOverlap=%d manualPoolSubtraction=0", Role(app),context.eligible.size(),manual.size(),available,exactOverlap);
    return true;
}
bool NoPrematureUse(LawnApp* app)
{
    if (!Require(app, app->mBoard->mSunMoney == 500, "Failed mystery intro consumed sun before reveal")) return false;
    for (int i = 0; i < app->mBoard->mSeedBank->mNumPackets; ++i)
    {
        const SeedPacket& packet = app->mBoard->mSeedBank->mSeedPackets[i];
        if (!Require(app, !packet.mRefreshing && packet.mRefreshCounter == 0 && packet.mRefreshTime == 0,
            "Failed mystery intro started a real packet cooldown")) return false;
    }
    Plant* plant = nullptr;
    while (app->mBoard->mPlants.IterateNext(plant))
        if (!plant->mDead && plant->mIsOnBoard)
            if (const auto* data = GetNativePlantData(plant); data && data->packetId)
                return Require(app, false, "Failed mystery intro created a purchased plant");
    return true;
}
bool RejectOldActivityLocally(LawnApp* app, const NativeContext& context)
{
    // Public SubmitFusionCommand deliberately overwrites epochs. Exercise the
    // real host business guard locally, without claiming a forged TCP replay.
    FusionSeedBank positive = context.selection;
    if (!Require(app, positive.Choose(context.epoch, 1, 0, false), "Old-activity negative lacks a valid fresh Choose control")) return false;
    LawnNet::NetBuffer before, after;
    app->mBoard->NetEncodeSnapshot(before);
    FusionCommand command; command.epoch = s.oldEpoch; command.inputEpoch = s.oldInputEpoch;
    command.action = FusionCommandKind::Choose; command.kind = 0;
    const bool accepted = HandleNativeFusionCommand(app->mBoard, 1, command);
    app->mBoard->NetEncodeSnapshot(after);
    if (!Require(app, before.Size() && !accepted && before.mData == after.mData && NoPrematureUse(app),
        "Old-activity local Choose mutated the fresh selection/world")) return false;
    LawnNet::NetTrace("SMOKE_FUSION_NATIVE_MYSTERY_RNG_STALE role=host oldEpoch=%u newEpoch=%u applied=0 fingerprintUnchanged=1 validFreshChoose=1 localBusinessGuard=1 transportReplayNotTested=1",
        s.oldEpoch, context.epoch);
    return true;
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
        if (record) LawnNet::NetTrace("SMOKE_FUSION_NATIVE_MYSTERY_ELIGIBLE role=%s index=%zu kind=%u extra=%d baseCost=%d sourceCD=%d maxHP=%d alias=%d",
            Role(app),i,card.kind,card.isExtra?1:0,card.baseCost,static_cast<int>(std::lround(card.rechargeSeconds*100.0f)),
            definition->maxHealth,static_cast<int>(GetNativeSeedForPlant(card.kind)));
    }
    for (const auto& pair:mandatory) if (!Require(app,pair[0] && pair[1],"Native fixture lost mandatory0/1/2/4 normal/extra eligibility")) return false;
    if (record) LawnNet::NetTrace("SMOKE_FUSION_NATIVE_MYSTERY_POOL_LAYOUT role=%s eligible=%zu target=%u quota0=%u quota1=%u mandatory=0,1,2,4 unfiltered=1",
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
bool NetSmokeUpdateNativeFusionMystery(LawnApp* app)
{
    const char* native = std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
    const char* mystery = std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY");
    if (!app || !native || std::strcmp(native, "1") || !mystery || std::strcmp(mystery, "1") || !app->CoopSmokeActive() ||
        app->mCoopSmokeManifest.mScenario != NETSMOKE_SCENARIO_FUSION_VALIDATION) return false;
    if (app->mCoopSmokeResultWritten) return true;
    if (!s.since) s.since = GetTickCount();
    if (GetTickCount()-s.since > (s.stage == 0 || s.stage == 1 || s.stage == 7 ? 60000U : 20000U))
    { char error[128]; sprintf_s(error, "Native mystery deadline stage=%d role=%s", s.stage, Role(app)); app->CoopSmokeFail(error); return true; }
    if (!app->mNetManager || !app->mNetManager->IsActive()) return true;
    app->mCoopSmokePeerReady = true;
    if (s.stage == 0)
    {
        if (app->NetIsHost() && !s.difficulty)
        {
            const char* requested = std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE_MYSTERY_DIFFICULTY");
            if (!Require(app, requested && (!std::strcmp(requested, "1") || !std::strcmp(requested, "10")) && !app->mBoard && !app->mSeedChooserScreen,
                "Native mystery difficulty must be1/10 in the clean host lobby")) return true;
            s.difficulty = std::strcmp(requested, "10") == 0 ? 10 : 1;
            app->mNetManager->SetDifficulty(s.difficulty);
            NetTrace("SMOKE_FUSION_NATIVE_MYSTERY_DIFFICULTY role=host difficulty=%d cleanLobby=1", s.difficulty);
        }
        if (app->mNetManager->GetPlayerCount() != 2 || (app->NetIsClient() && !app->mNetManager->IsConnectedToHost())) return true;
        if (app->NetIsClient())
        {
            s.difficulty = app->mNetManager->GetDifficulty();
            if (!Require(app, s.difficulty == 1 || s.difficulty == 10, "Native mystery client saw an invalid lobby difficulty")) return true;
            Send(app, s.difficulty == 1 ? "FM_D1" : "FM_D10");
            if (IsNativeFusionBoard(app->mBoard)) Advance(app, 1);
        }
        else if (Has(app, 1, s.difficulty == 1 ? "FM_D1" : "FM_D10"))
        {
            if (!Require(app, app->mNetManager->GetDifficulty() == s.difficulty, "Host lobby difficulty changed before native start")) return true;
            Advance(app, 1); std::string error;
            if (!app->StartFusionValidation(&error)) app->CoopSmokeFail(error.empty() ? "Native mystery game start failed" : error.c_str());
        }
        return true;
    }
    Board* board = app->mBoard;
    const auto* context = GetNativeContext(board);
    if (!context) return true;
    if (!Require(app, !app->mFusionRuntime && context->validation && context->selection.State().difficulty == s.difficulty,
        "Native mystery entered a second world or wrong difficulty")) return true;
    const auto& selection = context->selection.State();
    if (RngFailureCase() && !s.recovered && s.oldEpoch && context->epoch != s.oldEpoch)
    {
        if (!Require(app, s.stage == 7 && selection.phase == FusionBankPhase::Choosing &&
            s.introChecks >= 10 && s.introLast-s.introStart >= 1000 && s.frames.contains("intro_secret"),
            "RNG failure did not follow a full hidden intro into a new native chooser")) return true;
        s.failedIntroChecks = s.introChecks; s.failedIntroSpan = s.introLast-s.introStart;
        s.recovered = true; s.hostPick = 0;
        s.manual = {}; s.firstSelection = {}; s.firstNative = {}; s.finalSelection = {}; s.finalNative = {};
        s.introStart = s.introLast = s.introChecks = 0;
        Advance(app, 40);
        return true;
    }
    if (RngFailureCase() && !s.recovered && selection.phase == FusionBankPhase::Playing)
    { Require(app, false, "Armed RNG failure unexpectedly entered gameplay before recovery"); return true; }
    if ((selection.phase == FusionBankPhase::Choosing || selection.phase == FusionBankPhase::Intro) && (selection.mysteryMask & 2))
        if (!Hidden(app, *context)) return true;
    if (selection.phase == FusionBankPhase::Intro)
    {
        if (!s.introStart) s.introStart = GetTickCount();
        s.introLast = GetTickCount(); ++s.introChecks;
        if (RngFailureCase() && !s.recovered)
        {
            s.oldInputEpoch = app->mNetManager->GetInputEpoch();
            if (!NoPrematureUse(app)) return true;
        }
    }
    auto* chooser = app->mSeedChooserScreen;
    if (s.stage == 1)
    {
        if (!chooser || !chooser->mNativeFusionChooser || !chooser->mMouseVisible || !board->mCutScene->mSeedChoosing || !Settled()) return true;
        if (!Require(app, selection.phase == FusionBankPhase::Choosing && Pool(app,*context,true) && board->mSunMoney == 500,
            "Native mystery initial bank/quota/eligibility/sun mismatch") || !Capture(app, "chooser") || !Barrier(app, "FM_CHOOSER")) return true;
        s.manual.assign(selection.quotas[0],0);
        s.manualCards.assign(selection.quotas[0],FusionBankPacket{});
        s.firstSelection.assign(selection.quotas[1],0); s.firstNative.assign(selection.quotas[1],0);
        s.finalSelection.assign(selection.quotas[1],0); s.finalNative.assign(selection.quotas[1],0);
        NetTrace("SMOKE_FUSION_NATIVE_MYSTERY_CHOOSER role=%s difficulty=%d epoch=%u target=%u nativeChooser=1 secondWorld=0", Role(app), s.difficulty, context->epoch,selection.targetCount);
        if (!s.oldEpoch) s.oldEpoch = context->epoch;
        Advance(app, 2);
    }
    else if (s.stage == 40)
    {
        if (!chooser || !chooser->mNativeFusionChooser || !chooser->mMouseVisible || !board->mCutScene->mSeedChoosing ||
            board->mSeedBank->mNumPackets != 0 || !Settled()) return true;
        const uint32_t inputEpoch = app->mNetManager->GetInputEpoch();
        if (!Require(app, context->epoch != s.oldEpoch && inputEpoch && inputEpoch != s.oldInputEpoch &&
            app->mNetManager->GetInputPhase() == NETINPUT_CHOOSER && !context->selectionCommitted && !context->restartChooserRequested &&
            selection.phase == FusionBankPhase::Choosing && selection.readyMask == 0 && selection.mysteryMask == 0 &&
            selection.packets.empty() && context->packets.empty() && selection.activeMask == 3 &&
            Pool(app,*context) && Samples(app) == 0 && NoPrematureUse(app),
            "RNG recovery retained old activity/input epoch, ready/reveal/cooldown or gameplay snapshots")) return true;
        NetBuffer wire, body; NativeSnapshotFrame frame;
        board->NetEncodeSnapshot(wire);
        if (!Require(app, DecodeNativeSnapshot(wire, context->eligible, context->epoch, frame, body) &&
            frame.epoch == context->epoch && frame.selection.epoch == context->epoch && frame.selection.phase == FusionBankPhase::Choosing &&
            frame.selection.readyMask == 0 && frame.packets.empty(), "RNG recovery snapshot is not bound to the fresh empty chooser")) return true;
        if (app->NetIsHost() && !s.queued)
        {
            if (!RejectOldActivityLocally(app, *context)) return true;
            s.queued = true;
        }
        s.freshInputEpoch = inputEpoch;
        if (!Capture(app, "restarted")) return true;
        if (!s.restartLogged)
        {
            s.restartLogged = true;
            NetTrace("SMOKE_FUSION_NATIVE_MYSTERY_RNG_RESTART role=%s oldEpoch=%u oldInput=%u newEpoch=%u newInput=%u ready=0 packets=0 sun=500 cooldown=0 hiddenChecks=%u spanMs=%u wireEpochFresh=1 failedPlayingSnapshots=0 nativeChooser=1",
                Role(app), s.oldEpoch, s.oldInputEpoch, context->epoch, inputEpoch, s.failedIntroChecks, s.failedIntroSpan);
        }
        if (!Barrier(app, "FM_RNG_RECOVERED")) return true;
        Advance(app, 1);
    }
    else if (s.stage == 2)
    {
        if (!chooser || chooser->mSeedsInFlight) return true;
        if (app->NetIsHost() && s.hostPick < selection.quotas[0])
        {
            if (Owned(*context, 0).size() != static_cast<size_t>(s.hostPick)) return true;
            const auto choices=ManualChoices(*context,0);
            if (!Require(app,choices.size()>=selection.quotas[0]-s.hostPick,"Mystery host cannot fill its remaining manual quota")) return true;
            int x, y; chooser->GetSeedPositionInChooser(choices.front(), x, y);
            if (Click(app, static_cast<float>(chooser->mX+x+25), static_cast<float>(chooser->mY+y+35-(int)chooser->mScrollbar->mScrollValue))) ++s.hostPick;
            return true;
        }
        if (Owned(*context, 0).size() != selection.quotas[0] || !Manual(app, *context) || !Settled() || !Barrier(app, "FM_MANUAL")) return true;
        Advance(app, 3);
    }
    else if (s.stage == 3 || s.stage == 5)
    {
        if (!chooser || chooser->mSeedsInFlight) return true;
        if (app->NetIsClient() && !s.queued)
        { if (Button(app, chooser->mRandomButton)) s.queued = true; return true; }
        const auto cards = Owned(*context, 1);
        if (cards.size() != selection.quotas[1] || !(selection.mysteryMask & 2) || !Hidden(app, *context) || !Manual(app, *context)) return true;
        for (size_t i = 0; i < cards.size(); ++i)
        {
            SeedPacket* packet = BySelection(board, cards[i].id);
            if (s.stage == 3) { s.firstSelection[i] = cards[i].id; s.firstNative[i] = packet->mNetId; }
            else
            {
                if (!Require(app, std::find(s.firstSelection.begin(), s.firstSelection.end(), cards[i].id) == s.firstSelection.end() &&
                    std::find(s.firstNative.begin(), s.firstNative.end(), packet->mNetId) == s.firstNative.end(), "Mystery off/on reused discarded packet ids")) return true;
                s.finalSelection[i] = cards[i].id; s.finalNative[i] = packet->mNetId;
            }
        }
        if (!Settled() || !Capture(app, s.stage == 3 ? "pending_first" : "pending_final") || !Barrier(app, s.stage == 3 ? "FM_FIRST" : "FM_SECRET")) return true;
        NetTrace("SMOKE_FUSION_NATIVE_MYSTERY_SECRET role=%s pass=%d count=%zu hiddenKind=1 hiddenExtra=1 hiddenPrice=1 hiddenRecharge=1 nativePackets=1",
            Role(app),s.stage==3?1:2,cards.size());
        for (size_t i=0;i<cards.size();++i)
            NetTrace("SMOKE_FUSION_NATIVE_MYSTERY_SECRET_ID role=%s pass=%d index=%zu sid=%u packet=%u",
                Role(app),s.stage==3?1:2,i,cards[i].id,BySelection(board,cards[i].id)->mNetId);
        Advance(app, s.stage == 3 ? 4 : 6);
    }
    else if (s.stage == 4)
    {
        if (!chooser || chooser->mSeedsInFlight) return true;
        if (app->NetIsClient() && !s.queued)
        { if (Button(app, chooser->mRandomButton)) s.queued = true; return true; }
        if (!Owned(*context, 1).empty() || (selection.mysteryMask & 2)) return true;
        for (size_t i = 0; i < s.firstSelection.size(); ++i)
            if (!Require(app, !BySelection(board, s.firstSelection[i]) && !ByNative(board, s.firstNative[i]), "Cancelled mystery packet retained hidden identity")) return true;
        if (!Manual(app, *context) || !Settled() || !Capture(app, "cancelled") || !Barrier(app, "FM_OFF")) return true;
        NetTrace("SMOKE_FUSION_NATIVE_MYSTERY_CANCEL role=%s removed=%zu manualStable=1 nativeSDL=1", Role(app),s.firstSelection.size());
        Advance(app, 5);
    }
    else if (s.stage == 6)
    {
        if (selection.phase == FusionBankPhase::Intro) { Advance(app, 7); return true; }
        if (!chooser || chooser->mSeedsInFlight || s.queued) return true;
        if (Button(app, chooser->mStartButton)) s.queued = true;
    }
    else if (s.stage == 7)
    {
        if (selection.phase == FusionBankPhase::Intro)
        { if (s.introLast-s.introStart >= 300) Capture(app, "intro_secret"); return true; }
        if (selection.phase != FusionBankPhase::Playing) return true;
        if (!Require(app, app->mGameScene == SCENE_PLAYING && !chooser && context->selectionCommitted && board->mSunMoney == 500 &&
            s.introChecks >= 10 && s.introLast-s.introStart >= 1000 && s.frames.contains(AttemptName("intro_secret")), "Native mystery bypassed the full hidden intro/atomic start")) return true;
        if (!Reveal(app, *context) || !Capture(app, "revealed")) return true;
        NetTrace("SMOKE_FUSION_NATIVE_MYSTERY_INTRO role=%s hiddenChecks=%u spanMs=%u noLeak=1", Role(app), s.introChecks, s.introLast-s.introStart);
        Advance(app, 8);
    }
    else if (s.stage == 8)
    {
        if (!Require(app, !At(board) && board->mSunMoney == 500, "Native mystery purchase target is not initially empty")) return true;
        if (!Barrier(app, "FM_REVEAL")) return true;
        s.purchaseIndex = 0;
        while (s.purchaseIndex < s.finalNative.size())
        {
            const auto* candidate = GetNativePacketData(ByNative(board, s.finalNative[s.purchaseIndex]));
            const auto* definition=candidate?FusionFindPlant(candidate->kind):nullptr;
            // The implemented base pool has only Cherry2 as a timed instant.
            // Other supported cards persist without a contact target, including
            // future Chomper/Puff/Fume. Do not shrink the production RNG pool.
            if (candidate && candidate->kind!=2 && definition && definition->hasStats &&
                GetNativeSeedForPlant(candidate->kind)!=SEED_NONE &&
                std::any_of(context->eligible.begin(),context->eligible.end(),[&](const auto& card) {
                    return card.kind==candidate->kind && card.isExtra==candidate->isExtra;
                })) break;
            ++s.purchaseIndex;
        }
        if (!Require(app, s.purchaseIndex < s.finalNative.size(), "Full mystery pool lacks a supported non-instant purchase control")) return true;
        const auto* data = GetNativePacketData(ByNative(board, s.finalNative[s.purchaseIndex]));
        if (!Require(app, data && data->owner == 1, "Native mystery purchase packet metadata is missing")) return true;
        s.price = data->price; s.recharge = static_cast<int>(std::lround(data->rechargeSeconds*100.0f));
        Advance(app, 9);
    }
    else if (s.stage == 9)
    {
        if (app->NetIsHost()) { if (At(board)) Advance(app, 10); return true; }
        SeedPacket* packet = ByNative(board, s.finalNative[s.purchaseIndex]);
        if (!s.queued)
        {
            if (!Require(app, packet && packet->mActive && !packet->mRefreshing, "Native mystery card was not ready before pickup")) return true;
            s.pickupSamples = Samples(app);
            SeedBank* bank = board->mSeedBank;
            if (Click(app, static_cast<float>(board->mX+bank->mX+packet->mX+packet->mOffsetX+packet->mWidth/2),
                static_cast<float>(board->mY+bank->mY+packet->mY+packet->mHeight/2))) s.queued = true;
            return true;
        }
        if (!Held(board, s.finalNative[s.purchaseIndex]) || Samples(app) <= s.pickupSamples || !Settled()) return true;
        if (!Require(app, board->mSunMoney == 500 && packet && packet->mActive && !packet->mRefreshing && !At(board), "Native held echo spent money/recharge before planting") || !Capture(app, "held")) return true;
        NetTrace("SMOKE_FUSION_NATIVE_MYSTERY_HELD role=client packet=%u snapshotEcho=1 charge=0 recharge=0 nativeSDL=1", s.finalNative[s.purchaseIndex]);
        Advance(app, 10);
        Click(app, static_cast<float>(board->mX+board->GridToPixelX(3,2)+40), static_cast<float>(board->mY+board->GridToPixelY(3,2)+40));
    }
    else if (s.stage == 10)
    {
        Plant* plant = At(board); const auto* data = GetNativePlantData(plant);
        SeedPacket* packet = ByNative(board, s.finalNative[s.purchaseIndex]); const auto* packetData = GetNativePacketData(packet);
        if (!plant || !data) return true;
        if (!Require(app, packet && packetData && data->kind == packetData->kind && data->owner == 1 && data->randomOrigin == s.difficulty &&
            data->packetId == packet->mNetId && data->firstParent == kFusionNoPlant && data->secondParent == kFusionNoPlant &&
            board->mSunMoney == 500-s.price && packetData->price == s.price && packet->mRefreshTime == s.recharge &&
            !packet->mActive && packet->mRefreshing && packet->mRefreshCounter >= 0 && packet->mRefreshCounter < packet->mRefreshTime,
            "Native mystery purchase did not charge once, retain provenance or start source cooldown")) return true;
        if (!Settled() || !Capture(app, "purchased")) return true;
        s.purchasedPlant = plant->mNetId; s.firstCounter = packet->mRefreshCounter; s.purchaseSamples = Samples(app);
        NetTrace("SMOKE_FUSION_NATIVE_MYSTERY_PURCHASE role=%s difficulty=%d sid=%u packet=%u plant=%u kind=%u extra=%d origin=%u price=%d sun=%d cooldown=%d nativeSDL=1 nativeActor=1",
            Role(app), s.difficulty, packetData->selectionId, packet->mNetId, plant->mNetId, data->kind, packetData->isExtra ? 1 : 0, data->randomOrigin, s.price, board->mSunMoney, s.recharge);
        Advance(app, 11);
    }
    else if (s.stage == 11)
    {
        SeedPacket* packet = ByNative(board, s.finalNative[s.purchaseIndex]); Plant* plant = At(board);
        if (!Require(app, plant && plant->mNetId == s.purchasedPlant && board->mSunMoney == 500-s.price && packet && packet->mRefreshTime == s.recharge,
            "Native mystery final state drifted or charged twice")) return true;
        if (Samples(app) < 8 || Samples(app)-s.purchaseSamples < 2 || packet->mRefreshCounter <= s.firstCounter || !Barrier(app, "FM_FINAL") || !Settled()) return true;
        if (!Capture(app, "complete")) return true;
        if (RngFailureCase())
        {
            if (!Require(app, s.recovered && context->epoch != s.oldEpoch && app->mNetManager->GetInputPhase() == NETINPUT_PLAYING &&
                app->mNetManager->GetInputEpoch() != s.freshInputEpoch, "Recovered mystery did not cross a fresh ordered Playing input barrier")) return true;
            NetTrace("SMOKE_FUSION_NATIVE_MYSTERY_RNG_COMPLETE role=%s oldEpoch=%u newEpoch=%u chooserInput=%u playingInput=%u restarts=1 fullIntros=2 staleLocalReject=1 secondRevealAtomic=1 nativeWorld=1 transportReplayNotTested=1",
                Role(app), s.oldEpoch, context->epoch, s.freshInputEpoch, app->mNetManager->GetInputEpoch());
        }
        NetTrace("SMOKE_FUSION_NATIVE_MYSTERY_COMPLETE role=%s difficulty=%d epoch=%u packet=%u plant=%u price=%d sun=%d cooldown=%d samples=%u cooldownProgress=1 nativeWorld=1 secondWorld=0",
            Role(app), s.difficulty, context->epoch, s.finalNative[s.purchaseIndex], s.purchasedPlant, s.price, board->mSunMoney, s.recharge, Samples(app));
        Advance(app, 12); app->CoopSmokeWriteResult(true, nullptr);
    }
    return true;
}
}
