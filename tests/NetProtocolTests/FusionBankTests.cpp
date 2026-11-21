#include "FusionBankTests.h"
#include "../../Lawn/Fusion/FusionSeedBank.h"
#include "../../Lawn/Fusion/FusionWire.h"
#include "../../Lawn/Net/NetCommon.h"
#include "../../Lawn/Net/NetGameplayPolicy.h"
#include <cstdio>
#include <limits>
#include <stdexcept>

namespace
{
using namespace LawnFusion;
int checks = 0, failures = 0;

void Check(bool success, const char* message)
{
    ++checks;
    if (!success) { ++failures; std::printf("FAIL FusionBank %s\n", message); }
}

std::vector<FusionBankEligibleCard> FreshCards()
{
    return {{0, false, 100, 7.5f}, {0, true, 200, 7.5f}, {1, false, 50, 7.5f}, {1, true, 100, 7.5f}};
}

bool Same(const FusionBankState& a, const FusionBankState& b)
{
    if (a.epoch != b.epoch || a.revision != b.revision || a.phase != b.phase || a.difficulty != b.difficulty ||
        a.activeMask != b.activeMask || a.readyMask != b.readyMask || a.mysteryMask != b.mysteryMask ||
        a.targetCount != b.targetCount || a.quotas != b.quotas || a.nextPacketId != b.nextPacketId || a.packets.size() != b.packets.size()) return false;
    for (size_t i = 0; i < a.packets.size(); ++i)
    {
        const auto& x = a.packets[i]; const auto& y = b.packets[i];
        if (x.id != y.id || x.kind != y.kind || x.isExtra != y.isExtra || x.owner != y.owner ||
            x.randomOrigin != y.randomOrigin || x.mysteryPending != y.mysteryPending || x.price != y.price ||
            x.rechargeSeconds != y.rechargeSeconds || x.remainingRechargeSeconds != y.remainingRechargeSeconds) return false;
    }
    return true;
}

bool ReadyAll(FusionSeedBank& bank)
{
    for (int slot = 0; slot < static_cast<int>(kFusionBankPlayers); ++slot)
        if ((bank.State().activeMask & (1U << slot)) && !bank.SetReady(bank.State().epoch, slot, true)) return false;
    return true;
}

bool EnterPlaying(FusionSeedBank& bank)
{
    const uint32_t epoch = bank.State().epoch;
    return ReadyAll(bank) && bank.CommitIntro(epoch) && bank.BeginPlaying(epoch);
}

void SourceEnvironmentPolicy280()
{
    for(uint16_t kind:{uint16_t(6),uint16_t(19)})
    {
        FusionBankEligibleCard day,extra,night,nightExtra,shooting;
        Check(TryMakeFusionEnvironmentEligibleCard280(kind,false,false,false,day)&&day.baseCost==75&&day.rechargeSeconds==7.5f&&
            TryMakeFusionEnvironmentEligibleCard280(kind,true,false,false,extra)&&extra.baseCost==175&&extra.rechargeSeconds==7.5f,
            "source Day Puff/SeaPuff normal75 extra175 preserve recharge7.5");
        Check(TryMakeFusionEnvironmentEligibleCard280(kind,false,true,false,night)&&night.baseCost==0&&
            TryMakeFusionEnvironmentEligibleCard280(kind,true,true,false,nightExtra)&&nightExtra.baseCost==25&&
            TryMakeFusionEnvironmentEligibleCard280(kind,true,false,true,shooting)&&shooting==nightExtra,
            "source Night and shooting exclusions preserve ordinary Puff prices");
    }
    FusionBankEligibleCard pea;
    Check(TryMakeFusionEnvironmentEligibleCard280(0,true,false,false,pea)&&pea.baseCost==200,
        "environment surcharge does not change a Peashooter packet");
    const FusionBankEligibleCard sentinel{7,true,777,13.25f};auto invalid=sentinel;
    Check(!TryMakeFusionEnvironmentEligibleCard280(65535,true,false,false,invalid)&&invalid==sentinel,
        "unknown environment card leaves the full output unchanged");

    FusionBankEligibleCard normal,extra;
    Check(TryMakeFusionEnvironmentEligibleCard280(6,false,false,false,normal)&&
        TryMakeFusionEnvironmentEligibleCard280(6,true,false,false,extra),"Day source packet fixtures built");
    const std::vector<FusionBankEligibleCard> cards={normal,extra};FusionSeedBank host,client;
    uint16_t normalId=0,extraId=0;
    Check(host.Open(280,1,cards,3,2)&&host.Choose(280,0,6,false,&normalId)&&host.Choose(280,1,6,true,&extraId)&&EnterPlaying(host),
        "projected Day cards enter the shared two-owner native bank");
    const auto* normalPacket=host.PeekReadyPacket(280,0,normalId);const auto* extraPacket=host.PeekReadyPacket(280,1,extraId);
    Check(normalPacket&&extraPacket&&normalPacket->price==75&&extraPacket->price==175,
        "both owners use the projected canonical packet prices");
    Check(client.Open(280,1,cards,3,2)&&client.ApplySnapshot(host.State())&&Same(client.State(),host.State()),
        "client applies the same Day eligibility and packet economy");
    const auto before=client.State();auto wrong=before;++wrong.revision;wrong.packets[0].price=0;
    Check(!FusionSeedBank::ValidateSnapshot(wrong,cards,2)&&!client.ApplySnapshot(wrong)&&Same(client.State(),before),
        "Night-priced packet cannot enter the Day bank or partially change its state");
    Check(host.CommitPacketUse(280,1,extraId)&&host.AdvanceRecharge(280,7.49f)&&!host.PeekReadyPacket(280,1,extraId)&&
        host.AdvanceRecharge(280,.02f)&&host.PeekReadyPacket(280,1,extraId),
        "extra Day Puff retains the source7.5-second cooldown");
}

void SourceCardPolicy()
{
    struct Golden { uint16_t kind; int normal, extra; float recharge; };
    for (const auto sample : {Golden{0,100,200,7.5f},Golden{1,50,100,7.5f},
        Golden{6,0,25,7.5f},Golden{19,0,25,7.5f},Golden{1018,25,50,7.5f}})
    {
        FusionBankEligibleCard normal, extra;
        Check(TryMakeFusionEligibleCard(sample.kind,false,normal) &&
            normal.kind==sample.kind && !normal.isExtra && normal.baseCost==sample.normal &&
            normal.rechargeSeconds==sample.recharge, "source normal card price and unchanged recharge golden");
        Check(TryMakeFusionEligibleCard(sample.kind,true,extra) &&
            extra.kind==sample.kind && extra.isExtra && extra.baseCost==sample.extra &&
            extra.rechargeSeconds==sample.recharge, "source extra card golden; fixed25 only kinds6/19");
    }
    const FusionBankEligibleCard sentinel{7,true,777,13.25f};
    auto output=sentinel;
    Check(!TryMakeFusionEligibleCard(kFusionNoPlant,true,output) && output==sentinel &&
        !TryMakeFusionEligibleCard(65534,false,output) && output==sentinel,
        "unknown source IDs reject without changing any output field");

    FusionStartDescriptor descriptor;
    descriptor.epoch=85;
    FusionBankEligibleCard normal, extra;
    Check(TryMakeFusionEligibleCard(6,false,normal) && TryMakeFusionEligibleCard(6,true,extra),
        "actual free normal and paid extra Puff source identities built together");
    descriptor.eligible={normal,extra};
    Check(ValidateFusionDescriptor(descriptor), "reference descriptor accepts canonical ExtraPuff25");
    auto invalid=descriptor;
    invalid.eligible[1].baseCost=0;
    Check(!ValidateFusionDescriptor(invalid), "reference rejects old native free ExtraPuff mismatch");
    for (float badRecharge : {-1.0f,(std::numeric_limits<float>::quiet_NaN)(),
        (std::numeric_limits<float>::infinity)(),(std::numeric_limits<float>::max)()})
    {
        invalid=descriptor; invalid.eligible[1].rechargeSeconds=badRecharge;
        LawnNet::NetBuffer encoded; encoded.WriteU8(0x85);
        Check(!EncodeFusionStart(invalid,encoded) && encoded.Size()==1 && encoded.mData[0]==0x85,
            "invalid recharge rejects descriptor encoding atomically");
    }
    for (int badCost : {-1,(std::numeric_limits<int>::max)()})
    {
        invalid=descriptor; invalid.eligible[1].baseCost=badCost;
        Check(!ValidateFusionDescriptor(invalid), "invalid or forged cost rejects canonical source card");
    }
    FusionSeedBank manual;
    Check(manual.Open(85,1,descriptor.eligible,1) && manual.Choose(85,0,6,false) && manual.Choose(85,0,6,true) &&
        manual.State().packets[0].price==0 && manual.State().packets[1].price==25 &&
        manual.State().packets[1].rechargeSeconds==7.5f,
        "native bank policy keeps normal Puff free and extra Puff25 with independent identity");
    FusionSeedBank mystery;
    Check(mystery.Open(86,5,descriptor.eligible,1) && mystery.ToggleMystery(86,0,true) && EnterPlaying(mystery) &&
        mystery.Reveal(86,[](size_t count,size_t& index) { index=count-1; return true; }),
        "same canonical source pool feeds existing mystery reveal");
    Check(mystery.State().packets.size()==2 && mystery.State().packets[0].isExtra &&
        mystery.State().packets[0].price==LawnNet::DiscountedPacketCost(25,5,0) &&
        mystery.State().packets[1].price==0 && mystery.State().packets[0].rechargeSeconds==7.5f,
        "mystery discount is applied once downstream of source ExtraPuff25");
}

void ManualAndAuthority()
{
    const auto pool = FreshCards();
    FusionSeedBank bank;
    Check(bank.Open(7, 1, pool, 3) && bank.State().targetCount == 4 && bank.State().quotas[0] == 2 && bank.State().quotas[1] == 2,
        "fresh four variant identities produce achievable two-player quotas");
    const auto empty = bank.State();
    Check(!bank.SetReady(7, 0, true) && !bank.CommitIntro(7), "incomplete selection cannot ready or start");
    Check(!bank.Choose(6, 0, 0, false) && !bank.Choose(7, -1, 0, false) && !bank.Choose(7, 2, 0, false) &&
        !bank.ToggleMystery(7, 3, true) && Same(empty, bank.State()), "stale epoch and nonparticipant intents leave bank unchanged");
    uint16_t normal = 0, extra = 0;
    Check(bank.Choose(7, 0, 0, false, &normal), "normal Peashooter picked");
    const auto chosen = bank.State();
    Check(!bank.Choose(7, 1, 0, false) && !bank.Unchoose(7, 1, normal) && Same(chosen, bank.State()),
        "other owner cannot duplicate manual identity or unpick the packet");
    Check(bank.Choose(7, 0, 0, true, &extra) && extra != normal && bank.State().packets[1].price == 200,
        "extra is a separate identity with independent full source price");
    Check(!bank.Choose(7, 0, 1, false) && bank.SetReady(7, 0, true), "owner cannot exceed quota");
    Check(bank.Unchoose(7, 0, extra) && bank.State().readyMask == 0, "unpick invalidates ready state");
    uint16_t replacement = 0;
    Check(bank.Choose(7, 0, 0, true, &replacement) && replacement > extra, "released packet IDs are not reused in the epoch");
    Check(bank.Choose(7, 1, 1, false) && bank.Choose(7, 1, 1, true) && ReadyAll(bank) && bank.CommitIntro(7),
        "settled complete personal banks enter intro");
    const auto intro = bank.State();
    int calls = 0;
    Check(!bank.Reveal(7, [&](size_t, size_t&) { ++calls; return true; }) && calls == 0 &&
        !bank.Choose(7, 0, 1, false) && !bank.ReassignOwners(7, 1) && Same(intro, bank.State()),
        "intro denies random generation, selection and roster redistribution");
    Check(bank.BeginPlaying(7) && bank.Reveal(7, {}) && bank.PeekReadyPacket(7, 0, normal), "manual bank starts ready without RNG");
    Check(!bank.PeekReadyPacket(6, 0, normal) && !bank.PeekReadyPacket(7, 1, normal) && !bank.CommitPacketUse(7, 2, normal),
        "ready lookup and recharge commit authenticate epoch and owner");
    Check(bank.CommitPacketUse(7, 0, normal) && !bank.PeekReadyPacket(7, 0, normal) && bank.PeekReadyPacket(7, 0, replacement),
        "using normal starts only its own recharge; extra remains independent");
    const auto cooling = bank.State();
    Check(!bank.CommitPacketUse(7, 0, normal) && !bank.AdvanceRecharge(7, -1.0f) &&
        !bank.AdvanceRecharge(7, (std::numeric_limits<float>::quiet_NaN)()) &&
        !bank.AdvanceRecharge(7, (std::numeric_limits<float>::infinity)()) && Same(cooling, bank.State()),
        "double use and invalid time cannot consume or alter recharge");
    Check(bank.AdvanceRecharge(7, 7.0f) && !bank.PeekReadyPacket(7, 0, normal) && bank.AdvanceRecharge(7, 0.5f) &&
        bank.PeekReadyPacket(7, 0, normal), "source 7.5-second recharge finishes at the boundary");
    Check(FusionSeedBank::ValidateSnapshot(bank.State(), pool), "manual playing state satisfies snapshot contract");
}

void MysteryPrivacyAndFailure()
{
    const auto pool = FreshCards();
    for (int difficulty : {1, 5, 10})
    {
        FusionSeedBank bank;
        Check(bank.Open(1, difficulty, pool, 3) && bank.Choose(1, 0, 0, false) && bank.Choose(1, 0, 1, false) && bank.ToggleMystery(1, 1, true),
            "manual and mystery share source eligibility");
        bool hidden = true;
        for (const auto& packet : bank.State().packets) if (packet.owner == 1)
            hidden &= packet.kind == kFusionNoPlant && !packet.isExtra && packet.price == 0 && packet.rechargeSeconds == 0.0f &&
                packet.remainingRechargeSeconds == 0.0f && packet.mysteryPending && packet.randomOrigin == difficulty;
        Check(hidden && FusionSeedBank::ValidateSnapshot(bank.State(), pool), "pending public state has no type, variant, derived price or timers");
        Check(EnterPlaying(bank), "pending bank enters Playing before any draw");
        const auto pending = bank.State();
        Check(!bank.PeekReadyPacket(1, 0, pending.packets[0].id) && !bank.PeekReadyPacket(1, 1, pending.packets[2].id),
            "no player can plant while the atomic start reveal remains unresolved");
        int calls = 0;
        Check(!bank.Reveal(1, [&](size_t, size_t& index) { index = 0; return ++calls < 2; }) && calls == 2 && Same(pending, bank.State()),
            "failure after a successful first sample commits no partial reveal");
        Check(!bank.Reveal(1, [](size_t, size_t&) -> bool { throw std::runtime_error("rng unavailable"); }) && Same(pending, bank.State()),
            "RNG exception keeps every pending packet unchanged");
        Check(!bank.Reveal(1, [](size_t bound, size_t& index) { index = bound; return true; }) && Same(pending, bank.State()),
            "out-of-range RNG result is rejected transactionally");
        Check(bank.Reveal(1, [](size_t, size_t& index) { index = 0; return true; }), "retry reveals all owners atomically");
        const auto& revealed = bank.State();
        Check(revealed.packets[2].kind == 0 && !revealed.packets[2].isExtra && revealed.packets[3].kind == 0 && revealed.packets[3].isExtra,
            "mystery may overlap manual picks and owns distinct normal and extra identities");
        Check(revealed.packets[2].id == pending.packets[2].id && revealed.packets[3].id == pending.packets[3].id &&
            revealed.packets[2].price == LawnNet::DiscountedPacketCost(100, difficulty, 0) &&
            revealed.packets[3].price == LawnNet::DiscountedPacketCost(200, difficulty, 0) &&
            revealed.packets[0].price == 100 && revealed.packets[1].price == 50,
            "D1/D5/D10 intrinsic discount belongs only to mystery packet and survives reveal");
        Check(FusionSeedBank::ValidateSnapshot(revealed, pool), "revealed variant deck validates");
        const uint16_t randomId = revealed.packets[2].id;
        Check(bank.CommitPacketUse(1, 1, randomId) && bank.AdvanceRecharge(1, 2.0f), "mystery recharge is ordinary source recharge");
        const auto beforeTransfer = bank.State();
        Check(bank.ReassignOwners(1, 1) && bank.State().quotas[0] == 4 && bank.State().quotas[1] == 0,
            "disconnect redistributes all stable packets to remaining owner");
        bool preserved = true;
        for (size_t i = 0; i < bank.State().packets.size(); ++i)
        {
            const auto& a = beforeTransfer.packets[i]; const auto& b = bank.State().packets[i];
            preserved &= b.owner == 0 && a.id == b.id && a.kind == b.kind && a.isExtra == b.isExtra && a.randomOrigin == b.randomOrigin &&
                a.price == b.price && a.rechargeSeconds == b.rechargeSeconds && a.remainingRechargeSeconds == b.remainingRechargeSeconds;
        }
        Check(preserved && FusionSeedBank::ValidateSnapshot(bank.State(), pool), "merged hands may duplicate identity while origin, price, ID and timers remain intrinsic");
        Check(!bank.PeekReadyPacket(1, 1, randomId) && !bank.PeekReadyPacket(1, 0, randomId) && bank.AdvanceRecharge(1, 5.5f) && bank.PeekReadyPacket(1, 0, randomId),
            "reassigned packet has only new owner and keeps remaining recharge");
    }
    FusionSeedBank bank;
    Check(bank.Open(4, 5, pool, 3) && bank.ToggleMystery(4, 0, true) && bank.ToggleMystery(4, 1, true) && EnterPlaying(bank) &&
        bank.Reveal(4, [](size_t, size_t& index) { index = 0; return true; }), "two owners can both roll the same eligible subdeck");
    Check(bank.State().packets[0].kind == bank.State().packets[2].kind && bank.State().packets[1].isExtra == bank.State().packets[3].isExtra,
        "another mystery owner does not consume the first owner's pool");
}

void QuotasAndGenerations()
{
    FusionSeedBank bank;
    std::vector<FusionBankEligibleCard> pool;
    for (uint16_t id = 1000; id < 1020; ++id) pool.push_back({id, false, 100, 7.5f});
    Check(bank.Open(8, 1, pool, 10) && bank.State().targetCount == 14 && bank.State().quotas[1] == 7 && bank.State().quotas[3] == 7,
        "source fourteen-card capacity uses fair sparse-slot quotas");
    Check(bank.Choose(8, 1, 1000, false) && bank.State().packets[0].kind == 1000, "source IDs above 255 are retained without truncation");
    const auto before = bank.State();
    auto invalid = pool; invalid.push_back(pool[0]);
    Check(!bank.Open(9, 1, invalid, 10) && Same(before, bank.State()), "duplicate eligibility input does not replace live bank");
    invalid = pool; invalid[0].kind = kFusionNoPlant;
    Check(!bank.Open(9, 1, invalid, 10) && !bank.Open(9, 1, {}, 10) && !bank.Open(9, 0, pool, 10) &&
        !bank.Open(9, 11, pool, 10) && !bank.Open(9, 1, pool, 0) && !bank.Open(9, 1, pool, 16) && Same(before, bank.State()),
        "absent, unknown sentinel, invalid difficulty and spectator-only eligibility fail cleanly");
    Check(bank.Open(9, 1, {{0, false, 100, 7.5f}}, 15) && bank.State().quotas[0] == 1 && bank.State().quotas[1] == 0 &&
        bank.Choose(9, 0, 0, false) && ReadyAll(bank) && bank.CommitIntro(9), "zero-quota active peers can ready without impossible picks");
    Check(bank.Open(10, 1, FreshCards(), 5) && bank.State().packets.empty() && bank.State().readyMask == 0 && bank.State().mysteryMask == 0 &&
        !bank.Choose(9, 0, 0, false) && !bank.Open(10, 1, FreshCards(), 5), "pre-Playing roster change requires fresh epoch and invalidates queued old intents");
    FusionBankState exhausted = bank.State();
    exhausted.nextPacketId = 65535;
    ++exhausted.revision;
    Check(bank.ApplySnapshot(exhausted), "prepare last representable stable packet ID");
    uint16_t last = 0;
    Check(bank.Choose(10, 0, 0, false, &last) && last == 65535 && bank.State().nextPacketId == 65536, "last u16 ID is valid without wrapping");
    const auto fullIds = bank.State();
    Check(!bank.Choose(10, 0, 0, true) && !bank.ToggleMystery(10, 2, true) && Same(fullIds, bank.State()), "exhausted ID space rejects new identities and pending deck atomically");
}

void RosterRemoval()
{
    const auto find = [](const FusionBankState& state, uint16_t id) -> const FusionBankPacket*
    { for (const auto& packet : state.packets) if (packet.id == id) return &packet; return nullptr; };
    const auto count = [](const FusionBankState& state, int owner)
    { size_t result=0; for (const auto& packet : state.packets) result += packet.owner == owner; return result; };
    const auto fresh = FreshCards();
    auto pool = fresh;
    pool.push_back({2,false,150,7.5f}); pool.push_back({2,true,300,7.5f});
    pool.push_back({3,false,50,7.5f}); pool.push_back({3,true,100,7.5f});

    FusionSeedBank normal;
    Check(normal.Open(40,1,fresh,7) && normal.Choose(40,0,0,false) && normal.Choose(40,0,0,true) &&
        normal.Choose(40,1,1,false) && normal.Choose(40,2,1,true) && ReadyAll(normal),
        "remove-one fixture has three ready manual players");
    const auto before = normal.State();
    Check(normal.ReconcilePlayers(40,7) && Same(before,normal.State()), "unchanged roster preserves readiness and revision");
    FusionSeedBank manualClient = normal;
    Check(normal.ReconcilePlayers(40,5) && normal.State().activeMask==5 && normal.State().readyMask==0 &&
        normal.State().quotas[0]==2 && normal.State().quotas[2]==2 && normal.State().packets.size()==3 &&
        normal.State().nextPacketId==before.nextPacketId && normal.State().revision==before.revision+1,
        "Choosing removes one departed hand and recomputes quotas without filling manual choices");
    bool retained = true;
    for (const auto& packet : before.packets)
    { const auto* actual=find(normal.State(),packet.id); retained &= packet.owner==1 ? actual==nullptr : actual && *actual==packet; }
    Check(retained && FusionSeedBank::ValidateSnapshot(normal.State(),fresh) &&
        manualClient.ApplySnapshot(normal.State()) && Same(manualClient.State(),normal.State()),
        "manual survivor IDs and all packet fields survive and client accepts the new Choosing snapshot");
    uint16_t replacement=0;
    Check(normal.Choose(40,2,1,false,&replacement) && replacement==before.nextPacketId &&
        replacement!=before.packets[2].id && ReadyAll(normal), "freed normal identity is chosen anew with a monotone ID");

    FusionSeedBank mysteries;
    Check(mysteries.Open(41,5,pool,15,8) && mysteries.ToggleMystery(41,0,true) &&
        mysteries.Choose(41,1,0,false) && mysteries.Choose(41,1,0,true) && mysteries.ToggleMystery(41,2,true) &&
        mysteries.Choose(41,3,1,false) && mysteries.Choose(41,3,1,true) && ReadyAll(mysteries),
        "remove-two fixture has four ready players and two opaque mystery hands");
    const auto oldMystery = mysteries.State();
    Check(mysteries.ReconcilePlayers(41,5) && mysteries.State().readyMask==0 && mysteries.State().mysteryMask==5 &&
        mysteries.State().quotas[0]==4 && mysteries.State().quotas[2]==4 && mysteries.State().packets.size()==8 &&
        mysteries.State().nextPacketId==oldMystery.nextPacketId+4, "two departed players expand surviving mystery hands to exact new quotas");
    bool opaque = true;
    for (const auto& packet : mysteries.State().packets)
        opaque &= packet.mysteryPending && packet.kind==kFusionNoPlant && !packet.isExtra && packet.randomOrigin==5 &&
            packet.price==0 && packet.rechargeSeconds==0 && packet.remainingRechargeSeconds==0;
    for (const auto& packet : oldMystery.packets)
    { const auto* actual=find(mysteries.State(),packet.id); opaque &= (packet.owner==0 || packet.owner==2) ? actual && *actual==packet : actual==nullptr; }
    Check(opaque && FusionSeedBank::ValidateSnapshot(mysteries.State(),pool,8), "mystery refill preserves old pending IDs and exposes no derived kind, cost or timers");
    Check(ReadyAll(mysteries) && mysteries.CommitIntro(41), "refilled mystery hands can ready and commit normally");
    const auto intro = mysteries.State();
    Check(!mysteries.ReconcilePlayers(41,1) && !mysteries.ReconcilePlayers(41,5) &&
        !mysteries.ReassignOwners(41,1) && Same(intro,mysteries.State()), "Intro rejects both roster APIs without revealing or changing frozen identities");

    FusionSeedBank mixed;
    Check(mixed.Open(42,10,pool,15,8) && mixed.Choose(42,0,0,false) && mixed.Choose(42,0,0,true) &&
        mixed.ToggleMystery(42,1,true) && mixed.Choose(42,2,1,false) && mixed.Choose(42,2,1,true) &&
        mixed.ToggleMystery(42,3,true) && ReadyAll(mixed), "mixed surviving manual/mystery fixture prepared");
    const auto oldMixed = mixed.State();
    Check(mixed.ReconcilePlayers(42,9) && mixed.State().mysteryMask==8 && mixed.State().readyMask==0 &&
        count(mixed.State(),0)==2 && count(mixed.State(),3)==4 && mixed.State().packets.size()==6 &&
        mixed.State().nextPacketId==oldMixed.nextPacketId+2 && FusionSeedBank::ValidateSnapshot(mixed.State(),pool,8),
        "mixed Choosing keeps manual cards incomplete while only mystery receives new opaque slots");
    Check(find(mixed.State(),oldMixed.packets[0].id) && *find(mixed.State(),oldMixed.packets[0].id)==oldMixed.packets[0] &&
        find(mixed.State(),oldMixed.packets[1].id) && *find(mixed.State(),oldMixed.packets[1].id)==oldMixed.packets[1],
        "surviving normal and extra semantic identities remain exact");
    const auto incomplete = mixed.State();
    Check(!mixed.SetReady(42,0,true) && Same(incomplete,mixed.State()) && mixed.SetReady(42,3,true) &&
        mixed.Choose(42,0,1,false) && mixed.Choose(42,0,1,true) && ReadyAll(mixed) && mixed.CommitIntro(42),
        "normal survivor must choose its extra quota while mystery can ready without any RNG");

    FusionSeedBank playing;
    Check(playing.Open(43,10,pool,15,8) && playing.Choose(43,0,0,false) && playing.Choose(43,0,0,true) &&
        playing.ToggleMystery(43,1,true) && playing.Choose(43,2,1,false) && playing.Choose(43,2,1,true) &&
        playing.ToggleMystery(43,3,true) && EnterPlaying(playing) &&
        playing.Reveal(43,[](size_t,size_t& index){index=0;return true;}), "four-player resolved Playing roster prepared");
    const uint16_t randomId = playing.State().packets[2].id;
    Check(playing.CommitPacketUse(43,0,playing.State().packets[0].id) && playing.CommitPacketUse(43,1,randomId) &&
        playing.AdvanceRecharge(43,2), "manual and mystery real recharge state prepared before removal");
    const auto oldPlaying = playing.State(); FusionSeedBank playingClient = playing;
    Check(playing.ReconcilePlayers(43,5) && playing.State().phase==FusionBankPhase::Playing &&
        playing.State().activeMask==5 && playing.State().readyMask==0 && playing.State().mysteryMask==0 &&
        count(playing.State(),0)==4 && count(playing.State(),2)==4 &&
        playing.State().revision==oldPlaying.revision+1 && playing.State().nextPacketId==oldPlaying.nextPacketId,
        "Playing delegates redistribution into the two surviving fair hands");
    bool intrinsic = playing.State().packets.size()==oldPlaying.packets.size();
    for (const auto& packet : oldPlaying.packets)
    { const auto* actual=find(playing.State(),packet.id); if(!actual){intrinsic=false;continue;} auto expected=packet;expected.owner=actual->owner;intrinsic &= expected==*actual && (actual->owner==0 || actual->owner==2); }
    Check(intrinsic && FusionSeedBank::ValidateSnapshot(playing.State(),pool,8) &&
        playingClient.ApplySnapshot(playing.State()) && Same(playingClient.State(),playing.State()),
        "Playing retains every ID, kind, variant, discount origin, price and cooldown across host/client snapshots");
    const auto* moved=find(playing.State(),randomId); const int owner=moved?moved->owner:-1;
    Check(moved && !playing.PeekReadyPacket(43,1,randomId) && !playing.PeekReadyPacket(43,owner,randomId) &&
        playing.AdvanceRecharge(43,5.5f) && playing.PeekReadyPacket(43,owner,randomId),
        "departed owner loses authority and new owner waits the preserved remaining recharge");
    const auto twoPlayers=playing.State();
    Check(playing.ReconcilePlayers(43,1) && count(playing.State(),0)==8 && playing.State().packets.size()==8 &&
        playing.State().nextPacketId==twoPlayers.nextPacketId && FusionSeedBank::ValidateSnapshot(playing.State(),pool,8),
        "a later removal keeps all Playing cards, including overlapping rolled identities");

    FusionSeedBank invalid;
    const auto closed=invalid.State();
    Check(!invalid.ReconcilePlayers(1,1) && Same(closed,invalid.State()) && invalid.Open(44,1,fresh,7), "Closed reconciliation rejects without opening a bank");
    const auto unchanged=invalid.State();
    Check(!invalid.ReconcilePlayers(0,3) && !invalid.ReconcilePlayers(43,3) && !invalid.ReconcilePlayers(45,3) &&
        !invalid.ReconcilePlayers(44,0) && !invalid.ReconcilePlayers(44,15) && !invalid.ReconcilePlayers(44,16) &&
        Same(unchanged,invalid.State()), "zero, added, unknown and stale roster changes reject transactionally");
    auto exhaustedRevision=unchanged;exhaustedRevision.revision=(std::numeric_limits<uint32_t>::max)();
    Check(invalid.ApplySnapshot(exhaustedRevision) && !invalid.ReconcilePlayers(44,3) &&
        Same(exhaustedRevision,invalid.State()), "revision exhaustion cannot wrap on reconciliation");

    FusionSeedBank exhausted;
    Check(exhausted.Open(45,5,pool,15,8) && exhausted.ToggleMystery(45,0,true) && exhausted.ToggleMystery(45,2,true), "opaque overflow fixture prepared");
    auto last=exhausted.State();last.nextPacketId=65535;++last.revision;
    Check(exhausted.ApplySnapshot(last), "one representable ID remains before a four-slot refill");
    Check(!exhausted.ReconcilePlayers(45,5) && Same(last,exhausted.State()), "partial mystery refill and departed-card removal both roll back on ID exhaustion");
    FusionSeedBank boundary;
    Check(boundary.Open(46,5,fresh,7) && boundary.Choose(46,0,0,false) && boundary.Choose(46,0,0,true) &&
        boundary.ToggleMystery(46,2,true), "single-slot refill boundary fixture prepared");
    auto finalId=boundary.State();finalId.nextPacketId=65535;++finalId.revision;
    Check(boundary.ApplySnapshot(finalId) && boundary.ReconcilePlayers(46,5) && boundary.State().nextPacketId==65536 &&
        find(boundary.State(),65535) && find(boundary.State(),65535)->mysteryPending &&
        FusionSeedBank::ValidateSnapshot(boundary.State(),fresh), "last u16 ID refills exactly one opaque slot without wrapping");
    FusionSeedBank noAllocation;
    Check(noAllocation.Open(47,1,fresh,7) && noAllocation.Choose(47,0,0,false), "manual exhausted-ID roster fixture prepared");
    auto noIds=noAllocation.State();noIds.nextPacketId=65536;++noIds.revision;
    Check(noAllocation.ApplySnapshot(noIds) && noAllocation.ReconcilePlayers(47,5) &&
        noAllocation.State().nextPacketId==65536 && noAllocation.State().packets==noIds.packets,
        "roster removal without new opaque slots needs no packet ID allocation");
}

void SnapshotValidation()
{
    const auto pool = FreshCards();
    FusionSeedBank bank;
    Check(bank.Open(20, 5, pool, 3) && bank.Choose(20, 0, 0, false) && bank.Choose(20, 0, 0, true) &&
        bank.ToggleMystery(20, 1, true) && ReadyAll(bank) && bank.CommitIntro(20), "snapshot fixture contains manual and private pending cards");
    const auto good = bank.State();
    std::vector<FusionBankState> malformed;
    auto bad = good; ++bad.epoch; malformed.push_back(bad);
    bad = good; bad.phase = FusionBankPhase::Choosing; malformed.push_back(bad);
    bad = good; bad.phase = static_cast<FusionBankPhase>(255); malformed.push_back(bad);
    bad = good; bad.difficulty = 10; malformed.push_back(bad);
    bad = good; bad.quotas[0] = 3; malformed.push_back(bad);
    bad = good; bad.readyMask |= 4; malformed.push_back(bad);
    bad = good; bad.packets[0].owner = 2; malformed.push_back(bad);
    bad = good; bad.packets[0].id = bad.packets[1].id; malformed.push_back(bad);
    bad = good; bad.packets[0].id = 0; malformed.push_back(bad);
    bad = good; ++bad.packets[0].price; malformed.push_back(bad);
    bad = good; bad.packets[0].kind = 1000; malformed.push_back(bad);
    bad = good; bad.packets[0].rechargeSeconds = (std::numeric_limits<float>::infinity)(); malformed.push_back(bad);
    bad = good; bad.packets[0].remainingRechargeSeconds = -1; malformed.push_back(bad);
    bad = good; bad.packets[2].kind = 0; malformed.push_back(bad);
    bad = good; bad.packets[2].isExtra = true; malformed.push_back(bad);
    bad = good; bad.packets[2].price = 80; malformed.push_back(bad);
    bad = good; bad.packets[2].rechargeSeconds = 7.5f; malformed.push_back(bad);
    bad = good; bad.packets[2].randomOrigin = 0; malformed.push_back(bad);
    bad = good; bad.packets[2].remainingRechargeSeconds = (std::numeric_limits<float>::quiet_NaN)(); malformed.push_back(bad);
    bool rejected = true;
    for (auto& state : malformed)
    {
        ++state.revision;
        rejected &= !bank.ApplySnapshot(state) && Same(bank.State(), good);
    }
    Check(rejected, "malformed snapshot and wrong phase/epoch never partially overwrite bank");
    bad = good; bad.packets[0].id = static_cast<uint16_t>(bad.nextPacketId++); ++bad.revision;
    Check(FusionSeedBank::ValidateSnapshot(bad, pool) && !bank.ApplySnapshot(bad) && Same(bank.State(), good),
        "frozen intro packet identity cannot be replaced even by structurally valid snapshot");
    Check(bank.ApplySnapshot(good), "identical snapshot replay is harmless");
    Check(bank.BeginPlaying(20), "snapshot fixture enters Playing");
    const auto playing = bank.State();
    bad = playing; ++bad.revision; auto& reveal = bad.packets[2]; reveal.kind = 1; reveal.mysteryPending = false;
    reveal.price = LawnNet::DiscountedPacketCost(50, 5, 0); reveal.rechargeSeconds = 7.5f;
    Check(!FusionSeedBank::ValidateSnapshot(bad, pool) && !bank.ApplySnapshot(bad) && Same(bank.State(), playing),
        "partly revealed mystery snapshot violates atomic reveal contract");
    Check(bank.Reveal(20, [](size_t, size_t& index) { index = 0; return true; }), "complete reveal produces valid latejoin state");
    FusionSeedBank latejoin;
    Check(latejoin.Open(20, 5, pool, 3) && latejoin.ApplySnapshot(bank.State()) && Same(latejoin.State(), bank.State()),
        "fresh client applies complete Playing snapshot without rerolling");
    const auto latest = bank.State();
    Check(!bank.ApplySnapshot(playing) && !bank.ApplySnapshot(good) && Same(latest, bank.State()), "old intro and pre-reveal snapshots cannot roll back Playing state");
}
}

FusionBankTestResult RunFusionBankTests()
{
    checks = failures = 0;
    SourceCardPolicy();
    SourceEnvironmentPolicy280();
    ManualAndAuthority();
    MysteryPrivacyAndFailure();
    QuotasAndGenerations();
    RosterRemoval();
    SnapshotValidation();
    std::printf("FusionBank checks=%d failures=%d\n", checks, failures);
    return {checks, failures};
}
