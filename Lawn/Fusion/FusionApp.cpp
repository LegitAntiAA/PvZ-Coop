#include "../../LawnApp.h"
#include "FusionRuntime.h"
#include "FusionNative.h"
#include "../Board.h"
#include "../System/PlayerInfo.h"
#include <algorithm>
#include "../Net/NetManager.h"
#include <cstdlib>
#include <cstring>

namespace
{
uint32_t PayloadHash(const LawnNet::NetBuffer& payload)
{
    uint32_t hash = 2166136261U;
    for (uint8_t value : payload.mData) hash = (hash ^ value) * 16777619U;
    return hash;
}

bool InstallRuntime(LawnApp* app, std::unique_ptr<LawnFusion::FusionRuntime> runtime)
{
    app->KillGameSelector();
    app->NetCloseTransientScreens();
    app->mGameMode = GAMEMODE_ADVENTURE;
    app->MakeNewBoard();
    if (!app->mBoard) return false;
    app->mFusionRuntime = std::move(runtime);
    app->mFirstTimeGameSelector = false;
    app->mBoard->InitLevel();
    app->mBoardResult = BOARDRESULT_NONE;
    app->mGameScene = SCENE_LEVEL_INTRO;
    app->mFusionLastSnapshotTick = 0;
    app->mFusionNextLocalCommand = 0;
    return true;
}
}

bool LawnApp::IsFusionValidationFixture() const
{
    const char* campaign=std::getenv("PVZ_COOP_SMOKE_FUSION_CAMPAIGN270");
    return CoopSmokeActive() && mCoopSmokeManifest.mScenario == LawnNet::NETSMOKE_SCENARIO_FUSION_VALIDATION &&
        !(campaign&&std::strcmp(campaign,"1")==0);
}

int LawnApp::GetFusionAdventureLevel() const
{
    const int level=NetIsClient()?mNetFusionAdventureLevel:mNetManager?mNetManager->mConfig.mFusionAdventureLevel:1;
    return (std::max)(1,(std::min)(50,level));
}
void LawnApp::SetFusionAdventureLevel(int level,bool persist)
{
    level=(std::max)(1,(std::min)(50,level));
    if(NetIsClient()){mNetFusionAdventureLevel=level;return;}
    if(!mNetManager)return;
    mNetManager->mConfig.mFusionAdventureLevel=level;
    if(persist)LawnNet::NetSaveConfig(mNetManager->mConfig);
}
bool LawnApp::IsFusionAdventure() const
{return GetContentMode()==LawnFusion::ContentMode::Fusion&&mGameMode==GAMEMODE_ADVENTURE&&!IsFusionValidationFixture();}
bool LawnApp::CanStartFusionClassicCampaign() const
{
    if (LawnFusion::kFusionClassicCampaignEnabled) return true;
    const char* native = std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
    return CoopSmokeActive() && mCoopSmokeManifest.mScenario == LawnNet::NETSMOKE_SCENARIO_FUSION_VALIDATION &&
        native && std::strcmp(native,"1") == 0;
}
int LawnApp::GetActiveAdventureLevel() const
{
    if(GetContentMode()==LawnFusion::ContentMode::Fusion)return mBoard?mBoard->mLevel:GetFusionAdventureLevel();
    return mPlayerInfo?mPlayerInfo->mLevel:1;
}

bool LawnApp::StartFusionValidation(std::string* error)
{
    using namespace LawnFusion;
    if (error) error->clear();
    const char* nativeFixture = std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
    if (nativeFixture && std::strcmp(nativeFixture, "1") == 0)
    {
        if (!IsFusionValidationFixture() || !NetIsHost()) return false;
        KillGameSelector();
        NetCloseTransientScreens();
        PreNewGame(GAMEMODE_ADVENTURE, false, 1);
        return IsNativeFusionBoard(mBoard);
    }
    if (!IsFusionValidationFixture() || !NetIsHost() || !mNetManager || !mFusionNextEpoch)
    { if (error) *error = "The bounded Fusion validation activity is unavailable."; return false; }
    FusionStartDescriptor descriptor;
    descriptor.epoch = mFusionNextEpoch;
    descriptor.seed = mCoopSmokeManifest.mSeed;
    descriptor.difficulty = (uint8_t)mNetManager->GetDifficulty();
    descriptor.activeMask = 0;
    for (int slot = 0; slot < LawnNet::NET_MAX_PLAYERS; ++slot)
        if (mNetManager->GetPlayer(slot).mActive && !mNetManager->GetPlayer(slot).mSpectator)
            descriptor.activeMask |= (uint8_t)(1U << slot);
    for (int kind : {0, 1})
    {
        for (bool extra : {false, true})
        {
            FusionBankEligibleCard card;
            if (!TryMakeFusionEligibleCard(static_cast<FusionPlantId>(kind), extra, card)) return false;
            descriptor.eligible.push_back(card);
        }
    }
    auto runtime = std::make_unique<FusionRuntime>(this, descriptor);
    if (!runtime->Initialize(error)) return false;
    LawnNet::NetBuffer start;
    if (!EncodeFusionStart(descriptor, start)) return false;
    mNetManager->AdvanceInputPhase(LawnNet::NETINPUT_CHOOSER, "FUSION_START");
    if (!InstallRuntime(this, std::move(runtime))) return false;
    ++mFusionNextEpoch;
    mNetManager->BroadcastFusionStart(start);
    LawnNet::NetTrace("Fusion start host epoch=%u activity=validation columns=%u rows=%u sun=%d", descriptor.epoch,
        descriptor.columns, descriptor.rows, descriptor.initialSun);
    return true;
}

bool LawnApp::NetOnFusionStart(LawnNet::NetBuffer& payload, std::string* error)
{
    using namespace LawnFusion;
    if (error) error->clear();
    auto candidate = payload;
    FusionStartDescriptor descriptor;
    if (!NetIsClient() || !IsFusionValidationFixture() || GetContentMode() != ContentMode::Fusion ||
        !DecodeFusionStart(candidate, descriptor) || descriptor.activity != FusionActivity::Validation)
    { if (error) *error = "Invalid or unavailable Fusion activity."; return false; }
    if (mFusionRuntime && descriptor.epoch <= mFusionRuntime->Descriptor().epoch)
    {
        LawnNet::NetBuffer current, incoming;
        const bool same = EncodeFusionStart(mFusionRuntime->Descriptor(), current) && EncodeFusionStart(descriptor, incoming) && current.mData == incoming.mData;
        if (!same && error) *error = "Stale Fusion start descriptor.";
        if (same) payload.mReadPos = candidate.mReadPos;
        return same;
    }
    auto runtime = std::make_unique<FusionRuntime>(this, descriptor);
    if (!runtime->Initialize(error) || !InstallRuntime(this, std::move(runtime))) return false;
    payload.mReadPos = candidate.mReadPos;
    LawnNet::NetTrace("Fusion start client epoch=%u activity=validation columns=%u rows=%u", descriptor.epoch, descriptor.columns, descriptor.rows);
    return true;
}

bool LawnApp::SubmitFusionCommand(const LawnFusion::FusionCommand& command)
{
    if (!mFusionRuntime) return false;
    if (mNetManager && mNetManager->IsActive()) return mNetManager->SubmitFusionCommand(command);
    auto local = command;
    local.epoch = mFusionRuntime->Descriptor().epoch;
    local.sequence = ++mFusionNextLocalCommand;
    return mFusionRuntime->HandleCommand(0, local);
}

void LawnApp::FusionRosterChanged()
{
	if (!NetIsHost() || (CoopSmokeActive() && mCoopSmokeResultWritten)) return;
    uint8_t mask = 0;
    for (int slot = 0; slot < LawnNet::NET_MAX_PLAYERS; ++slot)
        if (mNetManager->GetPlayer(slot).mActive && !mNetManager->GetPlayer(slot).mSpectator) mask |= (uint8_t)(1U << slot);
	if (auto* context = LawnFusion::GetNativeContext(mBoard))
	{
		if (mask == context->selection.State().activeMask && !context->pendingRosterMask) return;
		if ((mask & ~context->selection.State().activeMask) &&
			context->selection.State().phase != LawnFusion::FusionBankPhase::Playing)
		{
			// Preserve the existing pregame join restart. Departures retain
			// the sealed Intro and reconcile after its private reveal.
			PreNewGame(GAMEMODE_ADVENTURE, false, context->level);
		}
		else if (LawnFusion::ReconcileNativePlayers(mBoard,mask))
			mBoard->mLastSnapshotTick = 0;
		return;
	}
	if (!mFusionRuntime) return;
    if (mask == mFusionRuntime->Bank().activeMask) return;
    if (mFusionRuntime->Phase() == LawnFusion::FusionBankPhase::Playing)
    {
        mFusionRuntime->ReassignOwners(mask);
        mFusionLastSnapshotTick = 0;
    }
    else
    {
        std::string error;
        if (!StartFusionValidation(&error) && CoopSmokeActive()) CoopSmokeFail(error.c_str());
    }
}

void LawnApp::FusionAdvancePlaying()
{
    if (!mFusionRuntime || NetIsClient() || mFusionRuntime->Phase() != LawnFusion::FusionBankPhase::Playing) return;
    mGameScene = SCENE_PLAYING;
    if (NetIsHost()) mNetManager->AdvanceInputPhase(LawnNet::NETINPUT_PLAYING, "FUSION_PLAYING");
    LawnNet::NetTrace("Fusion playing host epoch=%u mysteryRevealAtomic=1", mFusionRuntime->Descriptor().epoch);
}

void LawnApp::FusionTickNetwork()
{
    using namespace LawnFusion;
    if (!mFusionRuntime) return;
    auto* identity = mFusionRuntime.get();
    if (NetIsClient())
    {
        LawnNet::NetBuffer payload;
        if (mNetManager->TakeFusionRoom(payload))
        {
            const uint32_t hash = PayloadHash(payload);
            CoopSmokeOnSnapshotRx(hash, payload.Size());
            FusionRoomState state;
            std::string error;
            if (!DecodeFusionRoom(payload, identity->Descriptor(), state) || !identity->ApplyRoom(state, &error))
            {
                LawnNet::NetTrace("Fusion room rejected reason=%s", error.c_str());
                mNetManager->ReportInvalidFusionState(error.empty() ? "Malformed Fusion snapshot." : error);
                if (CoopSmokeActive()) CoopSmokeFail("Fusion snapshot was rejected");
                return;
            }
            mGameScene = state.bank.phase == FusionBankPhase::Playing ? SCENE_PLAYING : SCENE_LEVEL_INTRO;
            CoopSmokeOnSnapshotApplied(hash, payload.Size());
        }
    }
    else if (NetIsHost())
    {
        uint8_t slot = 0;
        FusionCommand command;
        for (int count = 0; count < LawnNet::NET_INPUT_APPLY_BUDGET && mFusionRuntime.get() == identity &&
            mNetManager->PopFusionCommand(slot, command); ++count)
        {
            if (command.inputEpoch != mNetManager->GetInputEpoch() || !mNetManager->CanApplyFusionCommand(slot)) continue;
            const bool applied = identity->HandleCommand(slot, command);
            LawnNet::NetTrace("Fusion command slot=%u action=%u sequence=%u applied=%d", slot, (unsigned)command.action, command.sequence, applied ? 1 : 0);
        }
    }
    if (mFusionRuntime.get() != identity) return;
    identity->Update();
    if (NetIsHost())
    {
        const uint32_t now = GetTickCount();
        if (mFusionLastSnapshotTick && now - mFusionLastSnapshotTick < (uint32_t)(1000 / mNetManager->GetSnapshotHz())) return;
        LawnNet::NetBuffer payload;
        const auto state = identity->CaptureRoom();
        if (!EncodeFusionRoom(state, identity->Descriptor(), payload))
        { if (CoopSmokeActive()) CoopSmokeFail("Host Fusion state failed validation"); return; }
        mFusionLastSnapshotTick = now;
        mNetManager->BroadcastFusionRoom(payload);
        CoopSmokeOnSnapshotTx(PayloadHash(payload), payload.Size());
    }
}
