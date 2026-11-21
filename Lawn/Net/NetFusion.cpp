#include "NetManager.h"
#include "../../LawnApp.h"
#include "../Fusion/FusionRuntime.h"
#include "../Fusion/FusionNative.h"
#include <cstring>

namespace LawnNet
{
namespace
{
uint32_t ActiveFusionEpoch(LawnApp* app)
{
    if (!app) return 0;
    if (const auto* context = LawnFusion::GetNativeContext(app->mBoard)) return context->epoch;
    return app->mFusionRuntime ? app->mFusionRuntime->Descriptor().epoch : 0;
}
}
void NetManager::ClearFusionTraffic()
{
    mFusionCommands.clear();
    std::memset(mFusionCommandCount, 0, sizeof(mFusionCommandCount));
    mFusionRoom.Clear();
    mHasFusionRoom = false;
}

bool NetManager::QueueFusionCommand(uint8_t slot, const LawnFusion::FusionCommand& command)
{
    if (!CanApplyFusionCommand(slot) ||
        !ConsumeExpectedInputSequence(slot, command.inputEpoch, command.sequence, "FUSION_COMMAND")) return false;
    if (command.epoch != ActiveFusionEpoch(mApp) ||
        mFusionCommands.size() >= NET_INPUT_QUEUE_TOTAL || mFusionCommandCount[slot] >= NET_INPUT_QUEUE_PER_PLAYER) return false;
    mFusionCommands.push_back({slot, command});
    ++mFusionCommandCount[slot];
    return true;
}

bool NetManager::CanApplyFusionCommand(int slot) const
{
    return IsHosting() && ActiveFusionEpoch(mApp) && slot >= 0 && slot < NET_MAX_PLAYERS &&
        mPlayers[slot].mActive && !mPlayers[slot].mSpectator && !mKicked[slot] && !mDeferredDrop[slot];
}

void NetManager::ReceiveFusionCommand(NetSession* session, NetBuffer& payload)
{
    if (!session || !mApp || mApp->GetContentMode() != LawnFusion::ContentMode::Fusion ||
        session->mSlot >= NET_MAX_PLAYERS || !mPlayers[session->mSlot].mActive) return;
    if (mPlayers[session->mSlot].mSpectator)
    {
        if (mApp->IsFusionValidationFixture())
        {
            NetBuffer copy = payload;
            LawnFusion::FusionCommand rejected;
            if (LawnFusion::DecodeFusionCommand(copy, rejected))
                NetTrace("SMOKE_FUSION_SPECTATOR_REJECT slot=%u action=%u epoch=%u target=%u",
                    session->mSlot, static_cast<unsigned>(rejected.action), rejected.epoch, rejected.targetId);
        }
        return;
    }
    LawnFusion::FusionCommand command;
    if (!LawnFusion::DecodeFusionCommand(payload, command))
    {
        NetTrace("Fusion malformed command slot=%u; defer disconnect", session->mSlot);
        mDeferredDrop[session->mSlot] = true;
        return;
    }
    QueueFusionCommand(session->mSlot, command);
}

bool NetManager::SubmitFusionCommand(LawnFusion::FusionCommand command)
{
    if (!ActiveFusionEpoch(mApp) || !IsActive() || IsSpectator() || mRejectClientState) return false;
    command.epoch = ActiveFusionEpoch(mApp);
    command.inputEpoch = mInputEpoch;
    command.sequence = mNextInputSequence;
    if (!command.sequence) return false;
    NetBuffer payload;
    if (!LawnFusion::EncodeFusionCommand(command, payload)) return false;
    if (IsHosting()) { ++mNextInputSequence; return QueueFusionCommand(mMySlot, command); }
    if (!mHostConn || !IsConnectedToHost()) return false;
    if (!mHostConn->SendMessage(NETMSG_FUSION_COMMAND, payload))
    {
        RejectHostState("FUSION_SEND", "Fusion input transport failed; reconnect to continue safely.");
        return false;
    }
    ++mNextInputSequence;
    return true;
}

bool NetManager::SmokeSendSpectatorFusionIntent(LawnFusion::FusionCommand command)
{
    if (!mApp || !mApp->IsFusionValidationFixture() || !mApp->mFusionRuntime || !IsClient() ||
        !IsSpectator() || mRejectClientState || !mHostConn || !IsConnectedToHost() || !mNextInputSequence) return false;
    command.epoch = mApp->mFusionRuntime->Descriptor().epoch;
    command.inputEpoch = mInputEpoch;
    command.sequence = mNextInputSequence;
    NetBuffer payload;
    if (!LawnFusion::EncodeFusionCommand(command, payload) || !mHostConn->SendMessage(NETMSG_FUSION_COMMAND, payload)) return false;
    ++mNextInputSequence;
    return true;
}

bool NetManager::PopFusionCommand(uint8_t& slot, LawnFusion::FusionCommand& command)
{
    if (mFusionCommands.empty()) return false;
    const auto pending = mFusionCommands.front();
    mFusionCommands.pop_front();
    if (mFusionCommandCount[pending.slot]) --mFusionCommandCount[pending.slot];
    slot = pending.slot; command = pending.command;
    return true;
}

bool NetManager::TakeFusionRoom(NetBuffer& payload)
{
    if (!mHasFusionRoom) return false;
    payload = std::move(mFusionRoom);
    mFusionRoom.Clear(); mHasFusionRoom = false;
    return true;
}

void NetManager::BroadcastFusionStart(const NetBuffer& payload)
{
    if (IsHosting()) BroadcastMessage(NETMSG_FUSION_START, payload);
}

void NetManager::BroadcastFusionRoom(const NetBuffer& payload)
{
    if (IsHosting()) BroadcastMessage(NETMSG_FUSION_ROOM, payload);
}

void NetManager::ReportInvalidFusionState(const std::string& reason)
{
    if (IsClient()) RejectHostState("FUSION_ROOM", reason);
}

void NetManager::ReportFusionResourceFailure(const std::string& reason)
{
    if (IsClient()) RejectHostState("FUSION_LOCAL_RESOURCE",reason);
}

void NetManager::SendFusionLateJoin(NetSession* session)
{
    if (!session || !mApp || !mApp->mFusionRuntime) return;
    const auto& runtime = *mApp->mFusionRuntime;
    NetBuffer start, state;
    if (!LawnFusion::EncodeFusionStart(runtime.Descriptor(), start) ||
        !LawnFusion::EncodeFusionRoom(runtime.CaptureRoom(), runtime.Descriptor(), state)) return;
    SendInputEpoch(session);
    session->mConn.SendMessage(NETMSG_FUSION_START, start);
    session->mConn.SendMessage(NETMSG_FUSION_ROOM, state);
    NetTrace("Fusion late join sent slot=%u readonly=1", session->mSlot);
}
}
