#pragma once

#include <algorithm>
#include <cstdint>
#include <random>
#include <vector>

namespace LawnNet
{
enum class DifficultyFamily
{
    Waves, FiniteAmmo, Whack, Boss, MatchThree, Aquarium, Vases, IZombie,
    Squirrel, NonCombat, PvP, Scripted
};

inline int GameplayClamp(int value, int low, int high)
{
    return value < low ? low : value > high ? high : value;
}

inline int GameplayCeilRatio(int value, int numerator, int denominator)
{
    return value <= 0 ? 0 : static_cast<int>((static_cast<int64_t>(value) * numerator + denominator - 1) / denominator);
}

inline int DifficultyWaveCount(int stock, int difficulty, int capacity)
{
    const int level = GameplayClamp(difficulty, 1, 10) - 1;
    const int result = stock + GameplayCeilRatio(stock, level, 5);
    return result < capacity ? result : capacity;
}

inline int DifficultyWavePoints(int stock, int difficulty)
{
    return stock + GameplayCeilRatio(stock, (GameplayClamp(difficulty, 1, 10) - 1) * 35, 100);
}

inline int DifficultyInterval(int stock, int difficulty, int reductionPermille, int minimum = 1)
{
    if (difficulty <= 1 || stock <= 0) return stock;
    const int factor = 9000 - GameplayClamp(reductionPermille, 0, 900) * (GameplayClamp(difficulty, 1, 10) - 1);
    const int result = GameplayCeilRatio(stock, factor, 9000);
    return result < minimum ? minimum : result;
}

inline int DifficultyCount(int stock, int difficulty, int extraAtTen)
{
    return stock + GameplayCeilRatio(extraAtTen, GameplayClamp(difficulty, 1, 10) - 1, 9);
}

inline int RandomDiscountBasisPoints(int originLevel)
{
    return originLevel <= 0 ? 0 : 500 + (2000 * (GameplayClamp(originLevel, 1, 10) - 1) + 4) / 9;
}

inline int DiscountedPacketCost(int base, int originLevel, int adaptivePermille)
{
    if (base <= 0) return 0;
    const int randomCost = GameplayCeilRatio(base, 10000 - RandomDiscountBasisPoints(originLevel), 10000);
    const int supportedCost = GameplayCeilRatio(randomCost, 1000 - GameplayClamp(adaptivePermille, 0, 200), 1000);
    const int floor = GameplayCeilRatio(base, 65, 100);
    return supportedCost < floor ? floor : supportedCost;
}

struct MysteryRoll
{
    int mType;
    int mImitater;
};

// The caller supplies the same availability/trial/mode-filtered pool used by
// manual picking. Other owners' choices deliberately do not restrict this pool.
template<class Engine>
bool RollMysteryDeck(const std::vector<int>& ordinaryPool, const std::vector<int>& imitaterTargets,
    int count, bool allowImitater, int imitaterType, Engine& engine, std::vector<MysteryRoll>& result)
{
    std::vector<int> pool = ordinaryPool;
    if (allowImitater && !imitaterTargets.empty()) pool.push_back(imitaterType);
    if (count < 0 || static_cast<size_t>(count) > pool.size()) return false;
    std::shuffle(pool.begin(), pool.end(), engine);
    std::vector<MysteryRoll> rolled;
    for (int i = 0; i < count; ++i)
    {
        MysteryRoll card = { pool[i], -1 };
        if (card.mType == imitaterType)
        {
            std::uniform_int_distribution<size_t> target(0, imitaterTargets.size() - 1);
            card.mImitater = imitaterTargets[target(engine)];
        }
        rolled.push_back(card);
    }
    result.swap(rolled);
    return true;
}

enum PvPBeneficiary
{
    PVP_SUPPORT_NONE = 0,
    PVP_SUPPORT_PLANTS = 1,
    PVP_SUPPORT_ZOMBIES = 2
};

struct PvPBalanceState
{
    int mBeneficiary = PVP_SUPPORT_NONE;
    int mStrength = 0;                 // 0..1000, includes the difficulty cap
    int mFilteredLead = 0;             // -10000..10000; positive means plants lead
    int mTargetSide = PVP_SUPPORT_NONE;
    int mCandidateSide = PVP_SUPPORT_NONE;
    int mCandidateSamples = 0;
    int mSampleTicks = 0;
};

inline bool PvPBalanceWireValid(int beneficiary, int strength, bool isPvP)
{
    return beneficiary >= PVP_SUPPORT_NONE && beneficiary <= PVP_SUPPORT_ZOMBIES &&
        strength >= 0 && strength <= 1000 && (beneficiary != PVP_SUPPORT_NONE || strength == 0) &&
        (isPvP || (beneficiary == PVP_SUPPORT_NONE && strength == 0));
}

inline void UpdatePvPBalance(PvPBalanceState& state, int kills, int brains, int difficulty, bool running)
{
    if (!running || kills >= 100 || brains >= 5) return;
    if (difficulty <= 1)
    {
        state = PvPBalanceState();
        return;
    }
    if (++state.mSampleTicks < 100) return;
    state.mSampleTicks = 0;
    kills = GameplayClamp(kills, 0, 100);
    brains = GameplayClamp(brains, 0, 5);
    const int lead = kills * kills - brains * brains * 400;
    state.mFilteredLead += (lead - state.mFilteredLead) / 10;

    int desired = PVP_SUPPORT_NONE;
    if (state.mFilteredLead >= 1200 ||
        (state.mBeneficiary == PVP_SUPPORT_ZOMBIES && state.mFilteredLead > 500))
        desired = PVP_SUPPORT_ZOMBIES;
    else if (state.mFilteredLead <= -1200 ||
        (state.mBeneficiary == PVP_SUPPORT_PLANTS && state.mFilteredLead < -500))
        desired = PVP_SUPPORT_PLANTS;

    if (desired != state.mCandidateSide)
    {
        state.mCandidateSide = desired;
        state.mCandidateSamples = 1;
    }
    else if (state.mCandidateSamples < 5)
        ++state.mCandidateSamples;
    if (state.mCandidateSamples >= 5) state.mTargetSide = desired;

    const int absoluteLead = state.mFilteredLead < 0 ? -state.mFilteredLead : state.mFilteredLead;
    const int severity = GameplayClamp((absoluteLead - 500) * 1000 / 4500, 0, 1000);
    const int target = state.mTargetSide == PVP_SUPPORT_NONE ? 0 :
        severity * (GameplayClamp(difficulty, 1, 10) - 1) / 9;
    if (state.mBeneficiary != state.mTargetSide)
    {
        state.mStrength = state.mStrength > 100 ? state.mStrength - 100 : 0;
        if (state.mStrength == 0) state.mBeneficiary = state.mTargetSide;
    }
    else if (state.mStrength < target)
        state.mStrength = state.mStrength + 50 < target ? state.mStrength + 50 : target;
    else if (state.mStrength > target)
        state.mStrength = state.mStrength - 100 > target ? state.mStrength - 100 : target;
    if (state.mStrength == 0 && state.mTargetSide == PVP_SUPPORT_NONE)
        state.mBeneficiary = PVP_SUPPORT_NONE;
}

inline int PvPSupportedInterval(int stock, int strength)
{
    return GameplayCeilRatio(stock, 10000 - GameplayClamp(strength, 0, 1000) * 15 / 10, 10000);
}

inline int PvPSpawnHealth(int stock, int strength)
{
    return GameplayCeilRatio(stock, 10000 + GameplayClamp(strength, 0, 1000) * 2, 10000);
}
}
