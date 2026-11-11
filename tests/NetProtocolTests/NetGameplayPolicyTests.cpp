#include "../../Lawn/Net/NetGameplayPolicy.h"
#include "NetGameplayPolicyTests.h"
#include <cstdio>
#include <set>

static int checks = 0;
static int failures = 0;
static void Check(bool condition, const char* message)
{
    ++checks;
    if (!condition) { ++failures; std::printf("FAIL %s\n", message); }
}

static void Advance(LawnNet::PvPBalanceState& state, int ticks, int kills, int brains, int level, bool running = true)
{
    for (int i = 0; i < ticks; ++i) LawnNet::UpdatePvPBalance(state, kills, brains, level, running);
}

NetGameplayPolicyTestResult RunNetGameplayPolicyTests()
{
    checks = failures = 0;
    using namespace LawnNet;
    Check(RandomDiscountBasisPoints(0) == 0, "ordinary origin has no random discount");
    Check(RandomDiscountBasisPoints(1) == 500 && RandomDiscountBasisPoints(10) == 2500, "random discount endpoints");
    Check(DiscountedPacketCost(25, 1, 0) == 24 && DiscountedPacketCost(25, 10, 0) == 19, "fractional currency rounds up");
    Check(DiscountedPacketCost(150, 1, 0) == 143 && DiscountedPacketCost(150, 10, 0) == 113, "half currency rounds up");
    Check(DiscountedPacketCost(100, 10, 200) == 65 && DiscountedPacketCost(25, 10, 200) == 17, "random plus adaptive capped at 35 percent");
    Check(DiscountedPacketCost(0, 10, 200) == 0 && DiscountedPacketCost(1, 10, 200) == 1, "free stays free and positive costs cannot become free");
    bool ordinaryStock = true, monotone = true, bounded = true;
    for (int base = 0; base < 1000; ++base)
    {
        ordinaryStock &= DiscountedPacketCost(base, 0, 0) == base;
        int previous = base;
        for (int level = 1; level <= 10; ++level)
        {
            const int cost = DiscountedPacketCost(base, level, 0);
            monotone &= cost <= previous;
            previous = cost;
            const int supported = DiscountedPacketCost(base, level, 200);
            bounded &= supported >= GameplayCeilRatio(base, 65, 100) && supported <= cost;
        }
    }
    Check(ordinaryStock, "every tested ordinary D1 base price is stock");
    Check(monotone, "random price is monotone at all ten levels and integer boundaries");
    Check(bounded, "stacking cap holds across integer boundaries");
    Check(DifficultyWaveCount(40, 1, 100) == 40 && DifficultyWaveCount(40, 10, 100) == 100, "wave growth saturates at real capacity");
    Check(DifficultyWavePoints(100, 1) == 100 && DifficultyWavePoints(100, 10) == 415, "wave points D1 and D10");
    Check(DifficultyInterval(2000, 1, 350) == 2000 && DifficultyInterval(2000, 10, 350) == 1300, "wave cadence D1 and D10");
    Check(DifficultyInterval(1500, 10, 400, 600) == 900, "vase packet lifetime");
    Check(DifficultyInterval(150, 10, 150) == 128, "IZombie fire cadence rounds up");
    Check(DifficultyCount(75, 10, 45) == 120 && DifficultyCount(1000, 10, 250) == 1250 &&
        DifficultyCount(7, 10, 5) == 12, "family objective endpoints");
    Check(DifficultyInterval(100, 10, 300) == 70, "aquarium hunger pressure");
    Check(DifficultyCount(1000, 10, 1000 / 5) == 1200 && DifficultyCount(1500, 10, 1500 / 5) == 1800,
        "aquarium income interval capped at twenty percent longer");
    bool pressureMonotone = true;
    for (int level = 2; level <= 10; ++level)
        pressureMonotone &= DifficultyWaveCount(12, level, 100) >= DifficultyWaveCount(12, level - 1, 100) &&
            DifficultyWavePoints(17, level) >= DifficultyWavePoints(17, level - 1) &&
            DifficultyInterval(2000, level, 350) <= DifficultyInterval(2000, level - 1, 350);
    Check(pressureMonotone, "D2 through D9 wave pressure is monotone");
    Check(DifficultyWaveCount(40, 99, 100) == 100 && DifficultyWavePoints(100, 99) == 415 &&
        DifficultyInterval(2000, 99, 350) == 1300 && DifficultyInterval(2000, 0, 350) == 2000,
        "difficulty values are clamped at stock and D10");

    std::mt19937 random(713);
    const std::vector<int> pool{0, 1, 2};
    std::vector<MysteryRoll> deck;
    Check(RollMysteryDeck(pool, pool, 4, true, 48, random, deck), "full eligible random deck rolls");
    std::set<int> ordinary;
    int imitaterCount = 0;
    bool overlap = false;
    for (const MysteryRoll& card : deck)
    {
        if (card.mType == 48) { ++imitaterCount; overlap = card.mImitater >= 0 && card.mImitater <= 2; }
        else ordinary.insert(card.mType);
    }
    Check(ordinary.size() == 3 && imitaterCount == 1 && overlap, "ordinary unique and imitater may copy rolled ordinary");
    const std::vector<MysteryRoll> before = deck;
    Check(!RollMysteryDeck(pool, pool, 5, true, 48, random, deck) && deck.size() == before.size() &&
        deck[0].mType == before[0].mType && deck[0].mImitater == before[0].mImitater, "insufficient pool fails transactionally");
    Check(!RollMysteryDeck(pool, {}, 4, true, 48, random, deck), "imitater without an eligible target adds no capacity");
    bool restricted = true;
    bool copiedFixed = false;
    const std::vector<int> fixedExcluded{1, 2};
    for (int i = 0; i < 256; ++i)
    {
        if (!RollMysteryDeck(fixedExcluded, {0}, 3, true, 48, random, deck)) { restricted = false; break; }
        for (const MysteryRoll& card : deck)
        {
            restricted &= card.mType != 0;
            if (card.mType == 48) copiedFixed |= card.mImitater == 0;
        }
    }
    Check(restricted && copiedFixed, "ordinary excludes fixed but imitater may target fixed");
    std::vector<MysteryRoll> otherOwner;
    Check(RollMysteryDeck(pool, pool, 3, false, 48, random, deck) &&
        RollMysteryDeck(pool, pool, 3, false, 48, random, otherOwner) && otherOwner.size() == deck.size(),
        "another owner does not consume the first owner's eligible types");

    PvPBalanceState state;
    Advance(state, 10000, 99, 0, 1);
    Check(state.mBeneficiary == PVP_SUPPORT_NONE && state.mStrength == 0 && state.mFilteredLead == 0, "D1 has no adaptive state");
    Advance(state, 500, 99, 0, 10);
    Check(state.mStrength == 0, "a score lead does not instantly grant assistance");
    Advance(state, 9500, 99, 0, 10);
    Check(state.mBeneficiary == PVP_SUPPORT_ZOMBIES && state.mStrength == 1000, "sustained plant lead supports zombies");
    Check(PvPSupportedInterval(2500, state.mStrength) == 2125, "sun and attack cadence bounded at fifteen percent");
    Check(PvPSpawnHealth(270, state.mStrength) == 324 && PvPSpawnHealth(1100, state.mStrength) == 1320,
        "spawn-time body and armor HP capped at twenty percent");
    const PvPBalanceState frozen = state;
    Advance(state, 10000, 0, 4, 10, false);
    Check(state.mStrength == frozen.mStrength && state.mFilteredLead == frozen.mFilteredLead &&
        state.mSampleTicks == frozen.mSampleTicks, "pause and terminal guard freeze all balance clocks");
    Advance(state, 1000, 100, 0, 10);
    Advance(state, 1000, 0, 5, 10);
    Check(state.mStrength == frozen.mStrength && state.mFilteredLead == frozen.mFilteredLead &&
        state.mSampleTicks == frozen.mSampleTicks, "100 kills and five brains independently freeze adaptation");
    bool zeroBeforeSwitch = true, rateBound = true;
    for (int i = 0; i < 200; ++i)
    {
        const int previousSide = state.mBeneficiary;
        const int previousStrength = state.mStrength;
        Advance(state, 100, 0, 4, 10);
        if (previousSide != state.mBeneficiary) zeroBeforeSwitch &= state.mStrength == 0;
        rateBound &= state.mStrength - previousStrength <= 50 && previousStrength - state.mStrength <= 100;
    }
    Check(zeroBeforeSwitch && rateBound, "support changes side only through zero and obeys slew limits");
    Check(state.mBeneficiary == PVP_SUPPORT_PLANTS && state.mStrength == 1000, "sustained brain lead supports plants");
    Check(PvPSpawnHealth(270, 0) == 270, "zero strength preserves stock spawn health");
    PvPBalanceState low;
    Advance(low, 20000, 99, 0, 2);
    Check(low.mStrength == 111, "D2 assistance cap is one ninth of D10");
    bool supportCaps = true;
    int previousCap = 0;
    for (int level = 2; level <= 10; ++level)
    {
        PvPBalanceState support;
        Advance(support, 20000, 99, 0, level);
        supportCaps &= support.mStrength == 1000 * (level - 1) / 9 && support.mStrength >= previousCap;
        previousCap = support.mStrength;
    }
    Check(supportCaps, "D2 through D9 assistance caps are monotone and bounded");
    PvPBalanceState neutral;
    Advance(neutral, 10000, 20, 1, 10);
    Check(neutral.mStrength == 0 && neutral.mBeneficiary == PVP_SUPPORT_NONE, "normalized equivalent objective progress stays neutral");
    Advance(neutral, 100, 20, 4, 10);
    Check(neutral.mStrength == 0, "one brain event cannot instantly flip assistance");
    Check(PvPBalanceWireValid(0, 0, false) && PvPBalanceWireValid(1, 0, true) && PvPBalanceWireValid(2, 1000, true),
        "wire accepts neutral, ramp boundary, and full support");
    Check(!PvPBalanceWireValid(0, 1, true) && !PvPBalanceWireValid(3, 0, true) &&
        !PvPBalanceWireValid(1, 1001, true) && !PvPBalanceWireValid(2, 1, false) &&
        !PvPBalanceWireValid(-1, 0, true) && !PvPBalanceWireValid(1, -1, true), "wire rejects invalid and non-PvP support");
    std::printf("GameplayPolicyTests checks=%d failures=%d\n", checks, failures);
    return { checks, failures };
}
