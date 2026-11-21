#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Plant.h"
#include "../Widget/AlmanacDialog.h"
#include "../Widget/GameSelector.h"
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>

namespace
{
using LawnFusion::ContentMode;
int stage = 0;
uint32_t since = 0, observed = 0, lastChat = 0;
bool opened = false;
std::set<std::string> frames;

bool Host(LawnApp* app) { return app->mCoopSmokeManifest.IsHost(); }
const char* Role(LawnApp* app) { return Host(app) ? "host" : "client"; }
bool Require(LawnApp* app, bool condition, const char* error)
{ if (!condition) app->CoopSmokeFail(error); return condition; }
void Advance(LawnApp* app, int next)
{
    stage = next; since = GetTickCount(); observed = 0; opened = false;
    LawnNet::NetTrace("SMOKE_FUSION_MODE_BOOK_STAGE role=%s stage=%d", Role(app), stage);
}
bool Settled()
{ if (!observed) observed = GetTickCount(); return GetTickCount()-observed >= 350; }
bool Has(LawnApp* app, int slot, const std::string& message)
{ for (const auto& line : app->mNetManager->mChatLines) if (line.mSlot == slot && line.mText == message) return true; return false; }
bool Send(LawnApp* app, const std::string& message)
{
    const int slot = Host(app) ? 0 : 1;
    if (Has(app, slot, message)) return true;
    if (lastChat && GetTickCount()-lastChat < 850) return false;
    lastChat = GetTickCount(); app->mNetManager->SendChat(message);
    return Has(app, slot, message);
}
bool Barrier(LawnApp* app, const char* message)
{
    Send(app, message);
    const std::string release = std::string(message)+"_GO";
    if (!Host(app)) return Has(app, 0, release);
    if (!Has(app, 1, message)) return false;
    return Send(app, release);
}
bool Capture(LawnApp* app, const char* name)
{
    if (frames.contains(name)) return true;
    const std::string basename = std::string("fusion_mode_book_")+name;
    if (!Require(app, app->CoopSmokeCaptureFrame(basename.c_str()), "Mode/book renderer capture failed")) return false;
    frames.insert(name);
    LawnNet::NetTrace("SMOKE_FUSION_MODE_BOOK_FRAME role=%s name=%s width=%d height=%d freshMs=350", Role(app), name, app->mWidth, app->mHeight);
    return true;
}
AlmanacDialog* Book(LawnApp* app)
{ return dynamic_cast<AlmanacDialog*>(app->GetDialog(DIALOG_ALMANAC)); }
bool CheckBook(LawnApp* app, bool fusion)
{
    auto* book = Book(app);
    if (!Require(app, book != nullptr && book->mOpenPage == ALMANAC_PAGE_PLANTS, "Expected native Almanac plants page is not open")) return false;
    uint16_t id = 65535; size_t page = 0; bool plants = false;
    const bool provider = book->GetFusionUiSelection(id, page, plants);
    if (fusion)
        return Require(app, provider && plants && id == 1000 && !book->mPlant && !book->mZombie,
            "Fresh Fusion Almanac did not select source1000 through its own provider");
    return Require(app, !provider && !book->ShowFusionPlant(1000) && book->mSelectedSeed == SEED_SUNFLOWER &&
        book->mPlant && book->mPlant->mSeedType == SEED_SUNFLOWER && !book->mPlant->mBoard && !book->mPlant->mIsOnBoard,
        "Fresh Classic Almanac retained the Fusion provider or lost its real native UI Plant");
}
bool OpenBook(LawnApp* app, bool fusion)
{
    if (!opened)
    {
        if (!Require(app, !Book(app) && app->GetDialogCount() == 0, "A stale modal survived before opening a fresh Almanac")) return false;
        auto* book = app->DoAlmanacDialog();
        if (!Require(app, book != nullptr, "Public DoAlmanacDialog failed")) return false;
        if (fusion)
        {
            if (!Require(app, book->ShowFusionPlant(1000), "Public source1000 Almanac selection failed")) return false;
        }
        else book->ShowPlant(SEED_SUNFLOWER);
        opened = true; observed = 0;
    }
    return CheckBook(app, fusion) && Settled();
}
bool ChangeMode(LawnApp* app, ContentMode mode, int transition)
{
    std::string error;
    if (!Require(app, app->NetIsHost() && app->CanSelectContentMode() && app->SetContentMode(mode, &error),
        "Normal host SetContentMode failed in the connected menu")) return false;
    LawnNet::NetTrace("SMOKE_FUSION_MODE_BOOK_SEND role=host transition=%d mode=%s normalSetter=1 realLobbyBroadcast=1 noBoard=1",
        transition, LawnFusion::ContentModeName(mode));
    return true;
}
void Complete(LawnApp* app)
{
    LawnNet::NetTrace("SMOKE_FUSION_MODE_BOOK_COMPLETE role=%s transitions=3 bothProviders=1 remoteClosure=1 disconnectRestore=1 noBoard=1 snapshots=0 gameplayNotTested=1",
        Role(app));
    Advance(app, 12); app->CoopSmokeWriteResult(true, nullptr);
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionAlmanacMode(LawnApp* app)
{
    const char* enabled = std::getenv("PVZ_COOP_SMOKE_FUSION_ALMANAC_MODE");
    const char* native = std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
    if (!app || !enabled || std::strcmp(enabled, "1") || !native || std::strcmp(native, "1") ||
        !app->CoopSmokeActive() || app->mCoopSmokeManifest.mScenario != NETSMOKE_SCENARIO_FUSION_VALIDATION) return false;
    if (app->mCoopSmokeResultWritten) return true;
    if (!since) since = GetTickCount();
    if (GetTickCount()-since > (stage == 0 ? 60000U : 20000U))
    { app->CoopSmokeFail("Mode/book fixture stage deadline"); return true; }
    if (!app->mNetManager) return true;
    if (!Require(app, !app->mBoard && !app->mSeedChooserScreen && !app->mFusionRuntime && app->mGameScene == SCENE_MENU &&
        app->mCoopSmokeSnapshotsSent == 0 && app->mCoopSmokeSnapshotsApplied == 0,
        "Mode/book regression left the native no-Board menu or entered gameplay")) return true;
    if (!app->mGameSelector || !app->mGameSelector->MainMenuIconsVisible()) return true;
    if (stage < 9 && (!app->mNetManager->IsActive() ||
        (!Host(app) && !app->mNetManager->IsConnectedToHost()) || app->mNetManager->GetPlayerCount() != 2))
    {
        if (stage != 8 || !Host(app)) return true;
    }
    if (stage == 0)
    {
        if (!Require(app, app->GetContentMode() == ContentMode::Fusion && app->IsFusionValidationFixture() &&
            (Host(app) ? app->NetIsHost() : app->NetIsClient()), "Real Fusion lobby negotiation/role is missing")) return true;
        if (!Settled() || !Capture(app, "fusion_menu") || !Barrier(app, "FBM_CONNECTED")) return true;
        app->mCoopSmokePeerReady = true;
        NetTrace("SMOKE_FUSION_MODE_BOOK_HANDSHAKE role=%s slot=%d mode=Fusion players=2 realConnection=1 noBoard=1",
            Role(app), app->mNetManager->GetMySlot());
        Advance(app, 1);
    }
    else if (stage == 1 || stage == 3 || stage == 5 || stage == 7)
    {
        const bool fusion = stage == 1 || stage == 5;
        const char* ack = stage == 1 ? "FBM_FUSION_OPEN" : stage == 3 ? "FBM_CLASSIC_OPEN" :
            stage == 5 ? "FBM_FUSION_REOPEN" : "FBM_RESTORE_ARMED";
        if (!Host(app))
        {
            if (!OpenBook(app, fusion)) return true;
            const char* frame = stage == 1 ? "fusion_first_book" : stage == 3 ? "classic_book" :
                stage == 5 ? "fusion_second_book" : "classic_before_restore";
            if (!Capture(app, frame)) return true;
            NetTrace("SMOKE_FUSION_MODE_BOOK_OPEN role=client round=%d mode=%s selected=%u provider=%s noBoard=1 publicApi=1",
                (stage+1)/2, fusion ? "Fusion" : "Classic", fusion ? 1000U : static_cast<unsigned>(SEED_SUNFLOWER), fusion ? "Fusion" : "Classic");
            // The host may close this dialog as soon as its ACK arrives.
            Advance(app, stage+1); Send(app, ack);
        }
        else if (Has(app, 1, ack))
        {
            if (stage == 7)
            {
                if (!Send(app, "FBM_DISCONNECT_GO")) return true;
            }
            else if (!ChangeMode(app, stage == 3 ? ContentMode::Fusion : ContentMode::Classic, (stage+1)/2)) return true;
            Advance(app, stage+1);
        }
    }
    else if (stage == 2 || stage == 4 || stage == 6)
    {
        const ContentMode expected = stage == 4 ? ContentMode::Fusion : ContentMode::Classic;
        if (!Host(app)) Send(app, stage == 2 ? "FBM_FUSION_OPEN" : stage == 4 ? "FBM_CLASSIC_OPEN" : "FBM_FUSION_REOPEN");
        if (app->GetContentMode() != expected) { observed = 0; return true; }
        if (!Require(app, !Book(app) && app->GetDialogCount() == 0, "Authoritative mode commit left the previous Almanac provider alive")) return true;
        const char* frame = stage == 2 ? "classic_first_menu" : stage == 4 ? "fusion_return_menu" : "classic_final_menu";
        const char* barrier = stage == 2 ? "FBM_CLASSIC_CLOSED" : stage == 4 ? "FBM_FUSION_CLOSED" : "FBM_FINAL_CLOSED";
        if (!Settled() || !Capture(app, frame)) return true;
        if (!frames.contains(std::string(frame)+"_logged"))
        {
            frames.insert(std::string(frame)+"_logged");
            NetTrace("SMOKE_FUSION_MODE_BOOK_CLOSED role=%s transition=%d mode=%s book=0 dialogs=0 freshMs=350 noBoard=1",
                Role(app), stage/2, LawnFusion::ContentModeName(expected));
        }
        if (!Barrier(app, barrier)) return true;
        Advance(app, stage+1);
    }
    else if (stage == 8)
    {
        if (Host(app))
        {
            if (app->mNetManager->GetPlayerCount() != 1) return true;
            Advance(app, 9);
        }
        else
        {
            Send(app, "FBM_RESTORE_ARMED");
            if (!Has(app, 0, "FBM_DISCONNECT_GO")) return true;
            if (!CheckBook(app, false)) return true;
            Advance(app, 9);
            app->mNetManager->Disconnect();
            app->NetRestoreLocalProfile();
        }
    }
    else if (stage == 9)
    {
        if (Host(app))
        {
            if (!Require(app, app->NetIsHost() && app->mNetManager->GetPlayerCount() == 1 &&
                app->GetContentMode() == ContentMode::Classic && !Book(app), "Host did not observe the real menu peer departure")) return true;
            if (!Settled() || !Capture(app, "host_peer_departed")) return true;
            NetTrace("SMOKE_FUSION_MODE_BOOK_DEPART role=host players=1 mode=Classic realDisconnect=1 noBoard=1");
            Complete(app);
        }
        else
        {
            if (!Require(app, !app->mNetManager->IsActive() && app->GetContentMode() == ContentMode::Fusion &&
                !Book(app) && !app->GetDialogCount(), "Real disconnect/restore left the previous Classic Almanac open or lost the original Fusion mode")) return true;
            if (!Settled() || !Capture(app, "fusion_restored_menu")) return true;
            NetTrace("SMOKE_FUSION_MODE_BOOK_RESTORE role=client before=Classic after=Fusion book=0 originalMode=Fusion realDisconnect=1 actualProfileRestore=1 noBoard=1");
            Advance(app, 10);
        }
    }
    else if (stage == 10 && !Host(app))
    {
        if (!OpenBook(app, true) || !Capture(app, "fusion_restored_book")) return true;
        NetTrace("SMOKE_FUSION_MODE_BOOK_REOPEN role=client mode=Fusion selected=1000 provider=Fusion afterRestore=1 noBoard=1");
        Complete(app);
    }
    return true;
}
}
