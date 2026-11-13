#include "NetZenGardenTests.h"
#include "../../Lawn/Net/NetZenGarden.h"
#include <cstdio>
#include <vector>

namespace
{
struct Buffer
{
    std::vector<uint8_t> mData;
    size_t mReadPos = 0;
    void WriteU8(uint8_t value) { mData.push_back(value); }
    void WriteU16(uint16_t value) { WriteU8((uint8_t)value); WriteU8((uint8_t)(value >> 8)); }
    bool ReadU8(uint8_t* value)
    {
        if (mReadPos == mData.size()) return false;
        *value = mData[mReadPos++];
        return true;
    }
    bool ReadU16(uint16_t* value)
    {
        if (mData.size() - mReadPos < 2) return false;
        *value = mData[mReadPos] | ((uint16_t)mData[mReadPos + 1] << 8);
        mReadPos += 2;
        return true;
    }
};

bool Same(const LawnNet::NetZenPlantState& a, const LawnNet::NetZenPlantState& b)
{
    return a.mNeed == b.mNeed && a.mSeedType == b.mSeedType && a.mAge == b.mAge &&
        a.mFacing == b.mFacing && a.mVariation == b.mVariation && a.mCanHaveChocolate == b.mCanHaveChocolate;
}
}

NetZenGardenTestResult RunNetZenGardenTests()
{
    NetZenGardenTestResult result{};
    const auto check = [&](bool condition)
    {
        ++result.mChecks;
        if (!condition) { ++result.mFailures; std::printf("    ZenGarden check failed=%d\n", result.mChecks); }
    };
    using namespace LawnNet;
    const uint8_t seeds = 48, variations = 20;
    for (uint8_t need = 0; need <= 4; ++need)
    {
        NetZenPlantState source{need, 47, 3, 1, 19, false}, output;
        Buffer wire;
        check(NetEncodeZenPlant(source, wire, true, seeds, variations));
        check(wire.mData.size() == NET_ZEN_PLANT_WIRE_BYTES);
        check(NetDecodeZenPlant(wire, output, true, seeds, variations) && Same(source, output));
    }
    NetZenPlantState eligible{0, 0, 3, 0, 0, true};
    Buffer valid;
    check(NetEncodeZenPlant(eligible, valid, true, seeds, variations));
    check(valid.mData == std::vector<uint8_t>({8, 0, 3, 0, 0}));
    for (size_t length = 0; length < NET_ZEN_PLANT_WIRE_BYTES; ++length)
    {
        Buffer wire{{91}, 1};
        wire.mData.insert(wire.mData.end(), valid.mData.begin(), valid.mData.begin() + length);
        NetZenPlantState unchanged{4, 4, 2, 1, 5, false}, output = unchanged;
        check(!NetDecodeZenPlant(wire, output, true, seeds, variations));
        check(wire.mReadPos == 1 && Same(output, unchanged));
    }
    const std::vector<std::vector<uint8_t>> invalid = {
        {5,0,3,0,0}, {16,0,3,0,0}, {8,0,2,0,0}, {9,0,3,0,0},
        {0,48,3,0,0}, {0,0,4,0,0}, {0,0,3,2,0}, {0,0,3,0,20},
        {1,255,0,0,0}, {0,255,1,0,0}
    };
    for (const auto& bytes : invalid)
    {
        Buffer wire{bytes}; NetZenPlantState output = eligible;
        check(!NetDecodeZenPlant(wire, output, true, seeds, variations));
        check(wire.mReadPos == 0 && Same(output, eligible));
    }
    Buffer ordinary = valid; NetZenPlantState output;
    check(!NetDecodeZenPlant(ordinary, output, false, seeds, variations) && ordinary.mReadPos == 0);
    Buffer absent;
    check(NetEncodeZenPlant(NetZenPlantState(), absent, false, seeds, variations));
    check(NetDecodeZenPlant(absent, output, false, seeds, variations) && output.mSeedType == 255);

    for (uint8_t garden : {0, 1, 3})
    {
        NetZenGardenState source;
        source.mGarden = garden; source.mStinkyChocolate = true;
        source.mSupplies = {1005, 1001, 2000};
        source.mWheelbarrow = {0, 2, 2, 1, 19, false};
        Buffer wire; NetZenGardenState decoded;
        check(NetEncodeZenGarden(source, wire, true, seeds, variations));
        check(wire.mData.size() == NET_ZEN_GARDEN_WIRE_BYTES);
        check(NetDecodeZenGarden(wire, decoded, true, seeds, variations) &&
            decoded.mGarden == garden && decoded.mStinkyChocolate &&
            decoded.mSupplies == source.mSupplies && Same(decoded.mWheelbarrow, source.mWheelbarrow));
        for (size_t length = 0; length < NET_ZEN_GARDEN_WIRE_BYTES; ++length)
        {
            Buffer truncated{std::vector<uint8_t>(wire.mData.begin(), wire.mData.begin() + length)};
            NetZenGardenState untouched;
            check(!NetDecodeZenGarden(truncated, untouched, true, seeds, variations));
            check(truncated.mReadPos == 0 && untouched.mGarden == 255 && untouched.mSupplies[0] == 0);
        }
        for (const auto& change : std::vector<std::pair<size_t, uint8_t>>{{0,2}, {1,2}, {3,255}, {8,1}, {9,255}})
        {
            Buffer invalidGarden = wire; invalidGarden.mReadPos = 0; invalidGarden.mData[change.first] = change.second;
            NetZenGardenState untouched;
            check(!NetDecodeZenGarden(invalidGarden, untouched, true, seeds, variations));
            check(invalidGarden.mReadPos == 0 && untouched.mGarden == 255);
        }
    }
    Buffer neutral;
    check(NetEncodeZenGarden(NetZenGardenState(), neutral, false, seeds, variations));
    NetZenGardenState decoded;
    check(NetDecodeZenGarden(neutral, decoded, false, seeds, variations));
    return result;
}
