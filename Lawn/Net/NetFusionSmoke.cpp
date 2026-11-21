#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Board.h"
#include "../Fusion/FusionRuntime.h"
#include "NetGameplayPolicy.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <set>

namespace
{
using namespace LawnFusion;
int stage = 0;
uint32_t since = 0, observedSince = 0, nextChat = 0;
uint32_t originalPlant = 0, fusedPlant = 0, zombieId = 0, sunId = 0;
uint32_t headlessSince = 0;
uint32_t introSince = 0;
bool seenBullet = false, seenLittleSun = false, seenHeadless = false, seenDeath = false;
std::set<std::string> frames;
uint32_t previousEpoch = 0, restartSamples = 0;
uint16_t mysteryPacketId = 0;
FusionPlantId mysteryKind = kFusionNoPlant;
bool mysteryExtra = false;
int mysteryPrice = 0;
std::array<uint16_t, 2> mysteryPendingIds{};

bool Case(const char* value)
{
    const char* configured = std::getenv("PVZ_COOP_SMOKE_FUSION_CASE");
    return configured && std::strcmp(configured, value) == 0;
}
uint32_t Samples(LawnApp* app) { return app->NetIsHost() ? app->mCoopSmokeSnapshotsSent : app->mCoopSmokeSnapshotsApplied; }

const char* Role(LawnApp* app) { return app->NetIsHost() ? "host" : "client"; }
void Advance(LawnApp* app, int value)
{
    stage = value; since = GetTickCount(); observedSince = 0;
    LawnNet::NetTrace("SMOKE_FUSION_STAGE role=%s stage=%d", Role(app), stage);
}
bool ObservedFor(uint32_t duration = 300)
{
    if (!observedSince) observedSince = GetTickCount();
    return GetTickCount() - observedSince >= duration;
}
bool Has(LawnApp* app, int slot, const char* text)
{
    for (const auto& line : app->mNetManager->mChatLines) if (line.mSlot == slot && line.mText == text) return true;
    return false;
}
bool Both(LawnApp* app, const char* text) { return Has(app, 0, text) && Has(app, 1, text); }
void Send(LawnApp* app, const char* text)
{
    if (Has(app, app->mNetManager->GetMySlot(), text)) return;
    const uint32_t now = GetTickCount();
    if (nextChat && now - nextChat < 850U) return;
    nextChat = now;
    app->mNetManager->SendChat(text);
}
bool Click(LawnApp* app, float x, float y, uint8_t button = SDL_BUTTON_LEFT)
{
    SDL_Event move{};
    move.type = SDL_EVENT_MOUSE_MOTION; move.motion.windowID = SDL_GetWindowID(app->mSDLWindow);
    move.motion.x = x; move.motion.y = y;
    SDL_Event down{};
    down.type = SDL_EVENT_MOUSE_BUTTON_DOWN; down.button.windowID = move.motion.windowID;
    down.button.button = button; down.button.clicks = 1; down.button.down = true;
    down.button.x = x; down.button.y = y;
    SDL_Event up = down; up.type = SDL_EVENT_MOUSE_BUTTON_UP; up.button.down = false;
    if (SDL_PushEvent(&move) && SDL_PushEvent(&down) && SDL_PushEvent(&up)) return true;
    app->CoopSmokeFail("Fusion owned-window SDL mouse queue failed"); return false;
}
bool Click(LawnApp* app, const Sexy::Rect& rect)
{
    return rect.mWidth > 0 && rect.mHeight > 0 && Click(app, rect.mX + rect.mWidth * 0.5f, rect.mY + rect.mHeight * 0.5f);
}
bool Key(LawnApp* app, SDL_Keycode code)
{
    SDL_Event down{};
    down.type = SDL_EVENT_KEY_DOWN; down.key.windowID = SDL_GetWindowID(app->mSDLWindow);
    down.key.key = code; down.key.scancode = SDL_GetScancodeFromKey(code, nullptr); down.key.down = true;
    SDL_Event up = down; up.type = SDL_EVENT_KEY_UP; up.key.down = false;
    if (SDL_PushEvent(&down) && SDL_PushEvent(&up)) return true;
    app->CoopSmokeFail("Fusion owned-window SDL key queue failed"); return false;
}
bool Cell(LawnApp* app, int column, int row)
{
    float x, y;
    GetFusionViewport(app->mWidth, app->mHeight).ProjectWorld(FusionColumnX(column), FusionRowY(row), x, y);
    return Click(app, x, y);
}
const FusionPlantState* FindFusionPlantAt(const FusionRuntime& runtime, int column, int row)
{
    for (const auto& plant : runtime.World().plants) if (plant.column == column && plant.row == row) return &plant;
    return nullptr;
}
const FusionBankPacket* Packet(const FusionRuntime& runtime, int owner, FusionPlantId kind, bool extra)
{
    for (const auto& packet : runtime.Bank().packets)
        if (packet.owner == owner && packet.kind == kind && packet.isExtra == extra && !packet.mysteryPending) return &packet;
    return nullptr;
}
bool Capture(LawnApp* app, const char* name)
{
    if (frames.count(name)) return true;
    if (!app->CoopSmokeCaptureFrame(name)) { app->CoopSmokeFail("Fusion renderer capture failed"); return false; }
    frames.insert(name);
    LawnNet::NetTrace("SMOKE_FUSION_FRAME role=%s name=%s nativeSDL=1", Role(app), name);
    return true;
}
bool Require(LawnApp* app, bool condition, const char* failure)
{
    if (!condition) app->CoopSmokeFail(failure);
    return condition;
}
void ObserveCombat(LawnApp* app, const FusionRuntime& runtime)
{
    const auto& world = runtime.World();
    for (const auto& projectile : world.projectiles) if (projectile.type == 8 && !seenBullet)
    {
        seenBullet = true;
        LawnNet::NetTrace("SMOKE_FUSION_COMBAT role=%s event=bullet8 id=%u tick=%u", Role(app), projectile.id, world.tick);
    }
    for (const auto& sun : world.suns) if (sun.amount == 5 && !seenLittleSun)
    {
        seenLittleSun = true;
        LawnNet::NetTrace("SMOKE_FUSION_COMBAT role=%s event=sun5 id=%u tick=%u", Role(app), sun.id, world.tick);
    }
    bool currentHeadless = false;
    for (const auto& zombie : world.zombies) if (zombie.type == 0 && zombie.row == 2)
    {
        if (!zombieId)
        {
            zombieId = zombie.id;
            LawnNet::NetTrace("SMOKE_FUSION_ZOMBIE role=%s id=%u row=2 tick=%u", Role(app), zombie.id, world.tick);
        }
        if (zombie.headless)
        {
            currentHeadless = true;
            if (!seenHeadless)
            {
                seenHeadless = true; headlessSince = GetTickCount();
                LawnNet::NetTrace("SMOKE_FUSION_COMBAT role=%s event=headless id=%u tick=%u", Role(app), zombie.id, world.tick);
            }
        }
    }
    if (currentHeadless && GetTickCount() - headlessSince >= 300) Capture(app, "fusion_headless");
    if (zombieId && world.killedZombies == 1 && world.zombies.empty() && !seenDeath)
    {
        seenDeath = true;
        LawnNet::NetTrace("SMOKE_FUSION_COMBAT role=%s event=death id=%u tick=%u", Role(app), zombieId, world.tick);
    }
}

bool HiddenMystery(LawnApp* app, const FusionRuntime& runtime)
{
    int pending = 0;
    for (const auto& packet : runtime.Bank().packets) if (packet.owner == 1)
    {
        if (!Require(app, packet.mysteryPending && packet.kind == kFusionNoPlant && !packet.isExtra && packet.price == 0 &&
            packet.rechargeSeconds == 0 && packet.remainingRechargeSeconds == 0 && packet.randomOrigin == 1,
            "Fusion mystery leaked a type, variant, derived price or recharge before Playing")) return false;
        if (pending < 2) mysteryPendingIds[pending] = packet.id;
        ++pending;
    }
    return Require(app, pending == 2, "Fusion mystery quota did not reserve exactly two hidden packets");
}

bool UpdateMystery(LawnApp* app)
{
    if (!app->mFusionRuntime) return true;
    const auto& runtime = *app->mFusionRuntime;
    if (stage == 80)
    {
        Send(app, "FUSION_MYSTERY_RESTART_READY");
        if (app->NetIsHost() && Both(app, "FUSION_MYSTERY_RESTART_READY"))
        {
            restartSamples = Samples(app);
            Advance(app, 81);
            std::string error;
            if (!app->StartFusionValidation(&error)) app->CoopSmokeFail(error.empty() ? "Fusion mystery new activity failed" : error.c_str());
            return true;
        }
        if (app->NetIsClient() && runtime.Descriptor().epoch > previousEpoch)
        { restartSamples = Samples(app); Advance(app, 81); }
        return true;
    }
    const auto& bank = runtime.Bank(); const auto& world = runtime.World();
    if (!Require(app, runtime.Descriptor().epoch > previousEpoch && bank.epoch == runtime.Descriptor().epoch && bank.difficulty == 1,
        "Fusion mystery restart reused the old epoch or changed the intended D1 origin")) return true;
    if (stage == 81)
    {
        if (runtime.Phase() != FusionBankPhase::Choosing || !ObservedFor()) return true;
        if (!Require(app, world.sun == 500 && world.plants.empty() && bank.targetCount == 4 && bank.activeMask == 3 &&
            bank.quotas[0] == 2 && bank.quotas[1] == 2, "Fusion second activity retained old world or impossible quotas")) return true;
        if (!Capture(app, "fusion_mystery_chooser")) return true;
        Advance(app, 82);
        if (app->NetIsHost()) { Click(app, runtime.EligibleCardRect(0)); Click(app, runtime.EligibleCardRect(2)); }
        else Click(app, runtime.MysteryButtonRect());
    }
    else if (stage == 82)
    {
        if (bank.packets.size() != 4 || !(bank.mysteryMask & 2)) return true;
        if (!HiddenMystery(app, runtime) || !ObservedFor() || !Capture(app, "fusion_mystery_pending")) return true;
        LawnNet::NetTrace("SMOKE_FUSION_MYSTERY_PENDING role=%s epoch=%u packets=%u,%u kind=NONE extra=0 price=0 recharge=0 origin=1", Role(app), bank.epoch,
            mysteryPendingIds[0], mysteryPendingIds[1]);
        introSince = 0;
        Advance(app, 83); Click(app, runtime.ReadyButtonRect());
    }
    else if (stage == 83)
    {
        if (runtime.Phase() != FusionBankPhase::Playing)
        {
            if (!HiddenMystery(app, runtime)) return true;
            if (runtime.Phase() == FusionBankPhase::Intro)
            {
                if (!introSince)
                {
                    introSince = GetTickCount();
                    LawnNet::NetTrace("SMOKE_FUSION_MYSTERY_INTRO role=%s epoch=%u pending=2 kind=NONE extra=0 price=0 recharge=0", Role(app), bank.epoch);
                }
                if (!Capture(app, "fusion_mystery_intro")) return true;
            }
            return true;
        }
        if (!Require(app, introSince && world.sun == 500 && world.plants.empty(), "Fusion mystery reveal skipped Intro or changed world before purchase")) return true;
        const FusionBankPacket* first = nullptr; const FusionBankPacket* second = nullptr;
        for (const auto& packet : bank.packets) if (packet.owner == 1)
        {
            const FusionBankEligibleCard* eligible = nullptr;
            for (const auto& card : runtime.Descriptor().eligible) if (card.kind == packet.kind && card.isExtra == packet.isExtra) eligible = &card;
            if (!Require(app, eligible && !packet.mysteryPending && packet.randomOrigin == 1 && packet.remainingRechargeSeconds == 0 &&
                packet.rechargeSeconds == eligible->rechargeSeconds && packet.price == LawnNet::DiscountedPacketCost(eligible->baseCost, 1, 0),
                "Fusion D1 mystery reveal has noncanonical identity, price or recharge")) return true;
            if (!first) first = &packet; else second = &packet;
        }
        if (!Require(app, first && second && first->id == mysteryPendingIds[0] && second->id == mysteryPendingIds[1] &&
            (first->kind != second->kind || first->isExtra != second->isExtra), "Fusion mystery did not retain IDs and owner-unique normal/extra identities")) return true;
        mysteryPacketId = first->id; mysteryKind = first->kind; mysteryExtra = first->isExtra; mysteryPrice = first->price;
        if (!ObservedFor() || !Capture(app, "fusion_mystery_revealed")) return true;
        LawnNet::NetTrace("SMOKE_FUSION_MYSTERY_REVEALED role=%s epoch=%u first=%u kind=%u extra=%d price=%d second=%u kind2=%u extra2=%d origin=1 unique=1",
            Role(app), bank.epoch, first->id, first->kind, first->isExtra ? 1 : 0, first->price, second->id, second->kind, second->isExtra ? 1 : 0);
        Advance(app, 84); Send(app, "FUSION_MYSTERY_REVEALED");
    }
    else if (stage == 84)
    {
        Send(app, "FUSION_MYSTERY_REVEALED");
        if (!Both(app, "FUSION_MYSTERY_REVEALED")) return true;
        Advance(app, 85);
        if (app->NetIsClient()) { Click(app, runtime.PacketRect(mysteryPacketId)); Cell(app, 4, 2); }
    }
    else if (stage == 85)
    {
        const auto* planted = FindFusionPlantAt(runtime, 4, 2);
        if (!planted) return true;
        const FusionBankPacket* used = nullptr; const FusionBankPacket* unused = nullptr;
        for (const auto& packet : bank.packets) if (packet.owner == 1)
        { if (packet.id == mysteryPacketId) used = &packet; else unused = &packet; }
        if (!Require(app, planted->type == mysteryKind && planted->owner == 1 && planted->packetId == mysteryPacketId && planted->randomOrigin == 1 &&
            planted->health == 300 && world.sun == 500 - mysteryPrice && used && used->isExtra == mysteryExtra && used->price == mysteryPrice &&
            used->remainingRechargeSeconds > 0 && used->remainingRechargeSeconds <= used->rechargeSeconds && unused && unused->remainingRechargeSeconds == 0,
            "Fusion mystery SDL purchase lost intrinsic price/provenance or independent recharge")) return true;
        if (!ObservedFor() || !Capture(app, "fusion_mystery_purchase")) return true;
        LawnNet::NetTrace("SMOKE_FUSION_MYSTERY_PURCHASE role=%s epoch=%u packet=%u id=%u kind=%u extra=%d origin=1 price=%d before=500 after=%d recharge=1 independent=1 nativeSDL=1",
            Role(app), bank.epoch, mysteryPacketId, planted->id, planted->type, mysteryExtra ? 1 : 0, mysteryPrice, world.sun);
        Advance(app, 86); Send(app, "FUSION_MYSTERY_PURCHASE_OK");
    }
    else if (stage == 86)
    {
        Send(app, "FUSION_MYSTERY_PURCHASE_OK");
        if (!Both(app, "FUSION_MYSTERY_PURCHASE_OK") || Samples(app) - restartSamples < 8) return true;
        Advance(app, 87); Send(app, "FUSION_MYSTERY_FINAL_ACK");
    }
    else if (stage == 87)
    {
        Send(app, "FUSION_MYSTERY_FINAL_ACK");
        if (!Both(app, "FUSION_MYSTERY_FINAL_ACK") || !ObservedFor()) return true;
        LawnNet::NetTrace("SMOKE_FUSION_MYSTERY_COMPLETE role=%s epoch=%u snapshotsAfterRestart=%u pendingPrivate=1 unique=1 origin=1 canonicalPurchase=1 sourceAdventureComplete=0",
            Role(app), bank.epoch, Samples(app) - restartSamples);
        Advance(app, 88); app->CoopSmokeWriteResult(true, nullptr);
    }
    return true;
}

bool ObserverBaseline(LawnApp* app, const FusionRuntime& runtime)
{
    const auto* plant = FindFusionPlantAt(runtime, 3, 2);
    const auto& bank = runtime.Bank();
    return Require(app, plant && plant->type == 1000 && (!fusedPlant || plant->id == fusedPlant) && plant->firstParent == 0 && plant->secondParent == 1 &&
        plant->health == 300 && plant->owner == 1 && runtime.World().sun == 300 && runtime.Phase() == FusionBankPhase::Playing &&
        bank.epoch == runtime.Descriptor().epoch && bank.activeMask == 3 && bank.targetCount == 4 && bank.packets.size() == 4 &&
        bank.quotas[0] == 2 && bank.quotas[1] == 2 && bank.quotas[2] == 0 && Packet(runtime, 0, 0, false) && Packet(runtime, 0, 1, false) &&
        Packet(runtime, 1, 0, true) && Packet(runtime, 1, 1, true), "Fusion late observer changed canonical fusion, currency or personal bank");
}

bool UpdateObserverPair(LawnApp* app)
{
    if (!app->mFusionRuntime) return true;
    const auto& runtime = *app->mFusionRuntime;
    if (!ObserverBaseline(app, runtime)) return true;
    if (stage == 60)
    {
        Send(app, "FUSION_OBSERVER_HOLD");
        if (!Both(app, "FUSION_OBSERVER_HOLD")) return true;
        Advance(app, 61);
        if (app->NetIsHost()) LawnNet::NetTrace("SMOKE_FUSION_OBSERVER_WAIT role=host epoch=%u id=%u sun=300 bankActive=3", runtime.Descriptor().epoch, fusedPlant);
    }
    else if (stage == 61)
    {
        if (!Has(app, 2, "FUSION_OBSERVER_CHECKED") || !ObservedFor(500)) return true;
        if (!Capture(app, "fusion_observer_pair")) return true;
        LawnNet::NetTrace("SMOKE_FUSION_OBSERVER_PAIR role=%s epoch=%u id=%u parents=0,1 hp=300 sun=300 packets=4 active=3 readonly=1",
            Role(app), runtime.Descriptor().epoch, fusedPlant);
        Advance(app, 62); Send(app, "FUSION_OBSERVER_PAIR_ACK");
    }
    else if (stage == 62)
    {
        Send(app, "FUSION_OBSERVER_PAIR_ACK");
        if (!Has(app, 2, "FUSION_OBSERVER_FINAL")) return true;
        Advance(app, 63); Send(app, "FUSION_OBSERVER_FINAL_ACK");
    }
    else if (stage == 63)
    {
        Send(app, "FUSION_OBSERVER_FINAL_ACK");
        if (!Both(app, "FUSION_OBSERVER_FINAL_ACK") || !ObservedFor()) return true;
        if (!Capture(app, "fusion_observer_after_reject")) return true;
        LawnNet::NetTrace("SMOKE_FUSION_OBSERVER_PAIR_COMPLETE role=%s epoch=%u id=%u stable=1 readonly=1 sourceAdventureComplete=0",
            Role(app), runtime.Descriptor().epoch, fusedPlant);
        Advance(app, 64); app->CoopSmokeWriteResult(true, nullptr);
    }
    return true;
}

bool UpdateLateObserver(LawnApp* app)
{
    if (!stage) Advance(app, 200);
    if (GetTickCount() - since > (stage == 200 ? 60000U : 15000U))
    { app->CoopSmokeFail("Fusion late observer fixture deadline"); return true; }
    if (!app->mNetManager || !app->mNetManager->IsConnectedToHost() || !app->mFusionRuntime) return true;
    const auto& runtime = *app->mFusionRuntime;
    if (runtime.Phase() != FusionBankPhase::Playing) return true;
    if (!Require(app, app->NetIsClient() && app->mNetManager->GetMySlot() == 2 && app->mNetManager->IsSpectator(),
        "Fusion third process was not authenticated as slot2 spectator") || !ObserverBaseline(app, runtime)) return true;
    if (!fusedPlant) fusedPlant = FindFusionPlantAt(runtime, 3, 2)->id;
    if (stage == 200)
    {
        if (!ObservedFor() || !Capture(app, "fusion_observer_join")) return true;
        LawnNet::NetTrace("SMOKE_FUSION_OBSERVER_STATE role=observer slot=2 epoch=%u id=%u parents=0,1 hp=300 sun=300 packets=4 active=3 readonly=1",
            runtime.Descriptor().epoch, fusedPlant);
        FusionCommand denied; denied.action = FusionCommandKind::Shovel; denied.targetId = fusedPlant;
        if (!Require(app, !app->SubmitFusionCommand(denied), "Fusion spectator local submission guard allowed a mutation")) return true;
        Advance(app, 201);
        const auto* packet = Packet(runtime, 0, 0, false);
        Click(app, runtime.PacketRect(packet->id)); Cell(app, 3, 2); Key(app, SDLK_R); Cell(app, 3, 2);
    }
    else if (stage == 201)
    {
        if (!ObservedFor() || !Capture(app, "fusion_observer_readonly")) return true;
        LawnNet::NetTrace("SMOKE_FUSION_OBSERVER_READONLY role=observer slot=2 epoch=%u id=%u nativeSDL=1 localSubmitRejected=1",
            runtime.Descriptor().epoch, fusedPlant);
        FusionCommand forged; forged.action = FusionCommandKind::Shovel; forged.targetId = fusedPlant;
        if (!Require(app, app->mNetManager->SmokeSendSpectatorFusionIntent(forged), "Fusion strict observer negative hook did not send a valid framed intent")) return true;
        LawnNet::NetTrace("SMOKE_FUSION_OBSERVER_FORGED role=observer slot=2 epoch=%u target=%u action=6", runtime.Descriptor().epoch, fusedPlant);
        Advance(app, 202); Send(app, "FUSION_OBSERVER_CHECKED");
    }
    else if (stage == 202)
    {
        Send(app, "FUSION_OBSERVER_CHECKED");
        if (!Both(app, "FUSION_OBSERVER_PAIR_ACK") || Samples(app) < 8 || !ObservedFor(500)) return true;
        Advance(app, 203); Send(app, "FUSION_OBSERVER_FINAL");
    }
    else if (stage == 203)
    {
        Send(app, "FUSION_OBSERVER_FINAL");
        if (!Both(app, "FUSION_OBSERVER_FINAL_ACK") || !ObservedFor()) return true;
        LawnNet::NetTrace("SMOKE_FUSION_OBSERVER_COMPLETE role=observer slot=2 epoch=%u id=%u parents=0,1 hp=300 sun=300 packets=4 readonly=1 snapshots=%u sourceAdventureComplete=0",
            runtime.Descriptor().epoch, fusedPlant, Samples(app));
        Advance(app, 204); app->CoopSmokeWriteResult(true, nullptr);
    }
    return true;
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusion(LawnApp* app)
{
    if (!app || !app->CoopSmokeActive() || app->mCoopSmokeManifest.mScenario != NETSMOKE_SCENARIO_FUSION_VALIDATION) return false;
    if (app->mCoopSmokeResultWritten) return true;
    const char* observer = std::getenv("PVZ_COOP_SMOKE_FUSION_OBSERVER");
    if (observer && std::strcmp(observer, "1") == 0) return UpdateLateObserver(app);
    if (!since) since = GetTickCount();
    const uint32_t limit = stage == 0 || stage == 60 || stage == 61 ? 60000U : stage == 13 || stage == 17 ? 30000U : 15000U;
    if (GetTickCount() - since > limit)
    {
        char failure[120]; sprintf_s(failure, "Fusion fixture deadline stage=%d role=%s", stage, Role(app));
        app->CoopSmokeFail(failure); return true;
    }
    if (!app->mNetManager || !app->mNetManager->IsActive()) return true;
    app->mCoopSmokePeerReady = true;
    if (stage == 0)
    {
        if (app->mNetManager->GetPlayerCount() != 2 || (!app->NetIsHost() && !app->mNetManager->IsConnectedToHost())) return true;
        if (app->NetIsClient())
        {
            Advance(app, 1); // Do not await chat echo across FUSION_START.
            Send(app, "FUSION_START_READY");
        }
        else if (Has(app, 1, "FUSION_START_READY"))
        {
            Advance(app, 1);
            std::string error;
            if (!app->StartFusionValidation(&error)) app->CoopSmokeFail(error.empty() ? "Fusion host start failed" : error.c_str());
        }
        return true;
    }
    if (stage == 1 && app->NetIsClient() && !app->mFusionRuntime) Send(app, "FUSION_START_READY");
    if (!app->mFusionRuntime || !app->mBoard) return true;
    const auto& runtime = *app->mFusionRuntime;
    const auto& world = runtime.World();
    const auto& bank = runtime.Bank();
    const int local = app->mNetManager->GetMySlot();
    if (!Require(app, app->GetContentMode() == ContentMode::Fusion && runtime.Descriptor().activity == FusionActivity::Validation &&
        world.columns == 10 && world.rows == 5 && app->mBoard->mPlants.mSize == 0 && app->mBoard->mZombies.mSize == 0,
        "Fusion validation lost ruleset/geometry isolation or created Classic proxies")) return true;
    if (stage >= 80) return UpdateMystery(app);
    if (stage >= 60) return UpdateObserverPair(app);
    if (stage >= 13) ObserveCombat(app, runtime);
    if (stage == 1)
    {
        if (runtime.Phase() != FusionBankPhase::Choosing || !ObservedFor()) return true;
        if (!Require(app, world.sun == 500 && runtime.Descriptor().eligible.size() == 4 && bank.targetCount == 4 && bank.activeMask == 3 &&
            bank.quotas[0] == 2 && bank.quotas[1] == 2, "Fusion fresh chooser source state differs")) return true;
        if (!Capture(app, "fusion_chooser")) return true;
        Advance(app, 2);
        Click(app, runtime.EligibleCardRect(app->NetIsHost() ? 0 : 1));
        Click(app, runtime.EligibleCardRect(app->NetIsHost() ? 2 : 3));
    }
    else if (stage == 2)
    {
        if (bank.packets.size() != 4) return true;
        if (!Require(app, Packet(runtime, 0, 0, false) && Packet(runtime, 0, 1, false) && Packet(runtime, 1, 0, true) && Packet(runtime, 1, 1, true),
            "Fusion source normal/extra choices did not retain personal ownership")) return true;
        if (!ObservedFor() || !Capture(app, "fusion_picks")) return true;
        NetTrace("SMOKE_FUSION_CHOOSER role=%s quota0=2 quota1=2 target=4 normalExtra=1", Role(app));
        Advance(app, 3);
        Click(app, runtime.ReadyButtonRect());
    }
    else if (stage == 3)
    {
        if (runtime.Phase() == FusionBankPhase::Intro)
        {
            if (!introSince)
            {
                introSince = GetTickCount();
                NetTrace("SMOKE_FUSION_INTRO role=%s phase=2 cards=4 sun=%d", Role(app), world.sun);
            }
            if (!Capture(app, "fusion_intro")) return true;
        }
        if (runtime.Phase() != FusionBankPhase::Playing || app->mGameScene != SCENE_PLAYING) return true;
        if (!Require(app, introSince && world.sun == 500 && bank.readyMask == 3 && bank.packets.size() == 4, "Fusion intro was skipped or changed initial currency/personal bank")) return true;
        if (!ObservedFor() || !Capture(app, "fusion_playing")) return true;
        NetTrace("SMOKE_FUSION_PLAYING role=%s epoch=%u inputEpoch=%u sun=500", Role(app), bank.epoch, app->mNetManager->GetInputEpoch());
        Advance(app, 4); Send(app, "FUSION_PLAYING");
    }
    else if (stage == 4)
    {
        Send(app, "FUSION_PLAYING");
        if (!Both(app, "FUSION_PLAYING")) return true;
        Advance(app, 5);
        if (app->NetIsHost()) { Key(app, SDLK_1); Cell(app, 3, 2); }
    }
    else if (stage == 5)
    {
        const auto* plant = FindFusionPlantAt(runtime, 3, 2);
        if (!plant) return true;
        if (!Require(app, plant->type == 0 && plant->owner == 0 && plant->health == 300 && world.sun == 400,
            "Fusion native host Peashooter input did not charge source price")) return true;
        originalPlant = plant->id;
        if (!ObservedFor() || !Capture(app, "fusion_peashooter")) return true;
        NetTrace("SMOKE_FUSION_PLANT role=%s id=%u kind=0 sun=400 keyboard=1 nativeSDL=1", Role(app), originalPlant);
        Advance(app, 6); Send(app, "FUSION_PEA_SEEN");
    }
    else if (stage == 6)
    {
        Send(app, "FUSION_PEA_SEEN");
        if (!Both(app, "FUSION_PEA_SEEN")) return true;
        Advance(app, 7);
        if (app->NetIsClient())
        {
            const auto* packet = Packet(runtime, local, 1, true);
            if (!Require(app, packet && packet->price == 100 && packet->remainingRechargeSeconds == 0, "Fusion client source extra packet not ready")) return true;
            Click(app, runtime.PacketRect(packet->id)); Cell(app, 3, 2);
        }
    }
    else if (stage == 7)
    {
        const auto* plant = FindFusionPlantAt(runtime, 3, 2);
        if (!plant || plant->type == 0) return true;
        if (!Require(app, plant->type == 1000 && plant->id != originalPlant && plant->firstParent == 0 && plant->secondParent == 1 &&
            plant->owner == 1 && plant->health == 300 && world.sun == 300, "Fusion client placement did not atomically create source PeaSunflower")) return true;
        fusedPlant = plant->id;
        if (!ObservedFor() || !Capture(app, "fusion_fused")) return true;
        NetTrace("SMOKE_FUSION_FUSION role=%s old=%u id=%u kind=1000 parents=0,1 hp=300 sun=300 owner=1 nativeSDL=1", Role(app), originalPlant, fusedPlant);
        Advance(app, 8); Send(app, "FUSION_FUSED");
    }
    else if (stage == 8)
    {
        Send(app, "FUSION_FUSED");
        if (!Both(app, "FUSION_FUSED")) return true;
        if (Case("mystery"))
        {
            previousEpoch = runtime.Descriptor().epoch;
            Advance(app, 80); Send(app, "FUSION_MYSTERY_RESTART_READY");
            return true;
        }
        if (Case("observer"))
        {
            Advance(app, 60); Send(app, "FUSION_OBSERVER_HOLD");
            return true;
        }
        Advance(app, 9);
        if (app->NetIsClient())
        {
            const auto* packet = Packet(runtime, local, 0, true);
            if (!Require(app, packet && packet->remainingRechargeSeconds == 0, "Fusion stale-intent packet unexpectedly cooling")) return true;
            FusionCommand command; command.action = FusionCommandKind::Plant; command.packetId = packet->id;
            command.column = 3; command.row = 2; command.targetId = originalPlant;
            if (!Require(app, app->SubmitFusionCommand(command), "Fusion stale intent did not enter authenticated command stream")) return true;
            NetTrace("SMOKE_FUSION_STALE_SENT role=client old=%u current=%u packet=%u", originalPlant, fusedPlant, packet->id);
        }
    }
    else if (stage == 9)
    {
        if (GetTickCount() - since < 700) return true;
        const auto* plant = FindFusionPlantAt(runtime, 3, 2); const auto* packet = Packet(runtime, 1, 0, true);
        if (!Require(app, plant && plant->id == fusedPlant && plant->type == 1000 && plant->health == 300 && world.sun == 300 &&
            packet && packet->remainingRechargeSeconds == 0, "Fusion stale intent consumed currency/card or replaced current entity")) return true;
        NetTrace("SMOKE_FUSION_STALE role=%s id=%u sun=300 charge=0 recharge=0", Role(app), fusedPlant);
        Advance(app, 10); Send(app, "FUSION_STALE_OK");
    }
    else if (stage == 10)
    {
        Send(app, "FUSION_STALE_OK");
        if (!Both(app, "FUSION_STALE_OK")) return true;
        Advance(app, 11);
        if (app->NetIsHost())
        {
            const auto* packet = Packet(runtime, 0, 1, false);
            if (!Require(app, packet && packet->price == 50, "Fusion source Sunflower packet missing")) return true;
            Click(app, runtime.PacketRect(packet->id)); Cell(app, 1, 1);
        }
    }
    else if (stage == 11)
    {
        const auto* sunflower = FindFusionPlantAt(runtime, 1, 1);
        if (!sunflower) return true;
        if (!Require(app, sunflower->type == 1 && sunflower->owner == 0 && world.sun == 250, "Fusion source Sunflower price/owner differs")) return true;
        NetTrace("SMOKE_FUSION_SUNFLOWER role=%s id=%u sun=250", Role(app), sunflower->id);
        Advance(app, 12); Send(app, "FUSION_SUNFLOWER");
    }
    else if (stage == 12)
    {
        Send(app, "FUSION_SUNFLOWER");
        if (!Both(app, "FUSION_SUNFLOWER")) return true;
        Advance(app, 13);
        if (app->NetIsHost())
        {
            const uint32_t id = app->mFusionRuntime->HostSpawnZombie(0, 2, 3.0f);
            Require(app, id != 0, "Fusion source zombie orchestration failed");
        }
    }
    else if (stage == 13)
    {
        for (const auto& sun : world.suns) if (sun.row == 1 && sun.amount == 25 && sun.landed && !sun.collecting) { sunId = sun.id; break; }
        if (!sunId || !ObservedFor()) return true;
        if (!Require(app, world.sun == 250, "Fusion currency changed before any sun collection")) return true;
        NetTrace("SMOKE_FUSION_SUN_READY role=%s id=%u amount=25 before=250", Role(app), sunId);
        Advance(app, 14); Send(app, "FUSION_SUN_READY");
    }
    else if (stage == 14)
    {
        Send(app, "FUSION_SUN_READY");
        if (!Both(app, "FUSION_SUN_READY")) return true;
        Advance(app, 15);
        if (app->NetIsClient())
        {
            bool found = false;
            for (const auto& sun : world.suns) if (sun.id == sunId)
            {
                float x, y; GetFusionViewport(app->mWidth, app->mHeight).ProjectWorld(sun.x, sun.y, x, y);
                found = Click(app, x, y); break;
            }
            Require(app, found, "Fusion source landed sun disappeared before SDL collection");
        }
    }
    else if (stage == 15)
    {
        if (world.sun != 275) return true;
        for (const auto& sun : world.suns) if (sun.id == sunId) return true;
        Advance(app, 16);
        if (app->NetIsClient())
        {
            FusionCommand command; command.action = FusionCommandKind::CollectSun; command.targetId = sunId;
            if (!Require(app, app->SubmitFusionCommand(command), "Fusion duplicate collection did not enter command stream")) return true;
            NetTrace("SMOKE_FUSION_COLLECT_REPEAT role=client id=%u", sunId);
            Send(app, "FUSION_DUPLICATE_SENT");
        }
    }
    else if (stage == 16)
    {
        if (app->NetIsClient()) Send(app, "FUSION_DUPLICATE_SENT");
        if (!Has(app, 1, "FUSION_DUPLICATE_SENT") || GetTickCount() - since < 700) return true;
        if (!Require(app, world.sun == 275, "Fusion duplicate collection credited the same sun twice")) return true;
        NetTrace("SMOKE_FUSION_COLLECT_SINGLE role=%s id=%u amount=25 before=250 after=275 nativeSDL=1", Role(app), sunId);
        Advance(app, 17); Send(app, "FUSION_COLLECT_OK");
    }
    else if (stage == 17)
    {
        Send(app, "FUSION_COLLECT_OK");
        if (!Both(app, "FUSION_COLLECT_OK") || !seenBullet || !seenLittleSun || !seenHeadless || !seenDeath) return true;
        if (!Require(app, FindFusionPlantAt(runtime, 3, 2) && FindFusionPlantAt(runtime, 3, 2)->id == fusedPlant, "Fusion combat lost canonical fused plant")) return true;
        Advance(app, 30);
        if (app->NetIsHost()) Click(app, runtime.ShovelButtonRect());
    }
    else if (stage == 18)
    {
        if (FindFusionPlantAt(runtime, 1, 1)) return true;
        if (!Require(app, world.sun == 275 && FindFusionPlantAt(runtime, 3, 2) && FindFusionPlantAt(runtime, 3, 2)->id == fusedPlant,
            "Fusion R shovel changed unrelated entity or currency")) return true;
        if (!ObservedFor() || !Capture(app, "fusion_complete")) return true;
        NetTrace("SMOKE_FUSION_SHOVEL role=%s remaining=%u sun=275 mouse=1 nativeSDL=1", Role(app), fusedPlant);
        Advance(app, 19); Send(app, "FUSION_FINAL_READY");
    }
    else if (stage == 19)
    {
        Send(app, "FUSION_FINAL_READY");
        if (!Both(app, "FUSION_FINAL_READY")) return true;
        Advance(app, 20); Send(app, "FUSION_FINAL_ACK");
    }
    else if (stage == 20)
    {
        Send(app, "FUSION_FINAL_ACK");
        const uint32_t samples = app->NetIsHost() ? app->mCoopSmokeSnapshotsSent : app->mCoopSmokeSnapshotsApplied;
        if (!Both(app, "FUSION_FINAL_ACK") || samples < 8 || !ObservedFor()) return true;
        NetTrace("SMOKE_FUSION_COMPLETE role=%s nativeSDL=1 quotas=2,2 normalExtra=1 fusion=1000 staleRejected=1 bullet8=1 sun5=1 headless=1 death=1 collectOnce=1 shovel=1 columns=10 rows=5 sourceAdventureComplete=0", Role(app));
        Advance(app, 21);
        app->CoopSmokeWriteResult(true, nullptr);
    }
    else if (stage >= 30 && stage <= 34)
    {
        if (app->NetIsClient())
        {
            if (Has(app, 0, "FUSION_SHOVEL_DIG_READY")) Advance(app, 18);
            return true;
        }
        if (!ObservedFor()) return true;
        if (!Require(app, app->GetDialogCount() == 0 && world.sun == 275 && FindFusionPlantAt(runtime, 1, 1) &&
            FindFusionPlantAt(runtime, 3, 2) && FindFusionPlantAt(runtime, 3, 2)->id == fusedPlant,
            "Fusion native shovel pickup/cancel changed world or opened a pause dialog")) return true;
        if (stage == 30)
        {
            if (!Capture(app, "fusion_shovel_held")) return true;
            Advance(app, 31); Key(app, SDLK_ESCAPE);
        }
        else if (stage == 31) { Advance(app, 32); Click(app, runtime.ShovelButtonRect()); }
        else if (stage == 32)
        {
            const auto rect = runtime.ShovelButtonRect();
            Advance(app, 33); Click(app, rect.mX + rect.mWidth * 0.5f, rect.mY + rect.mHeight * 0.5f, SDL_BUTTON_RIGHT);
        }
        else if (stage == 33)
        {
            NetTrace("SMOKE_FUSION_SHOVEL_CANCEL role=host escape=1 right=1 worldUntouched=1 nativeSDL=1");
            Advance(app, 34); Click(app, runtime.ShovelButtonRect());
        }
        else
        {
            if (!Capture(app, "fusion_shovel_reselected")) return true;
            Advance(app, 18); Send(app, "FUSION_SHOVEL_DIG_READY");
            Cell(app, 1, 1); Cell(app, 3, 2);
        }
    }
    return true;
}
}
