#pragma once

#include <cstdint>
#include <cstddef>
#include <array>

namespace LawnNet
{
// Resolved native PottedPlantNeed, independent of the host's private profile.
static const uint8_t NET_ZEN_NEED_MAX = 4;
static const size_t NET_ZEN_PLANT_WIRE_BYTES = 5;
static const size_t NET_ZEN_GARDEN_WIRE_BYTES = 13;

struct NetZenPlantState
{
    uint8_t mNeed = 0, mSeedType = 255, mAge = 0, mFacing = 0, mVariation = 0;
    bool mCanHaveChocolate = false;
};

inline bool NetZenPlantValid(const NetZenPlantState& plant, bool zenGarden,
    uint8_t seedCount, uint8_t variationCount)
{
    if (plant.mSeedType == 255)
        return !plant.mNeed && !plant.mAge && !plant.mFacing && !plant.mVariation && !plant.mCanHaveChocolate;
    return zenGarden && plant.mNeed <= NET_ZEN_NEED_MAX && plant.mSeedType < seedCount &&
        plant.mAge <= 3 && plant.mFacing <= 1 && plant.mVariation < variationCount &&
        (!plant.mCanHaveChocolate || (plant.mAge == 3 && plant.mNeed == 0));
}

template <class Buffer>
bool NetEncodeZenPlant(const NetZenPlantState& plant, Buffer& buffer, bool zenGarden,
    uint8_t seedCount, uint8_t variationCount)
{
    if (!NetZenPlantValid(plant, zenGarden, seedCount, variationCount)) return false;
    buffer.WriteU8(plant.mNeed | (plant.mCanHaveChocolate ? 8 : 0));
    buffer.WriteU8(plant.mSeedType); buffer.WriteU8(plant.mAge);
    buffer.WriteU8(plant.mFacing); buffer.WriteU8(plant.mVariation);
    return true;
}

template <class Buffer>
bool NetDecodeZenPlant(Buffer& buffer, NetZenPlantState& plant, bool zenGarden,
    uint8_t seedCount, uint8_t variationCount)
{
    const auto start = buffer.mReadPos;
    NetZenPlantState candidate;
    uint8_t flags = 0;
    const bool decoded = buffer.ReadU8(&flags) && buffer.ReadU8(&candidate.mSeedType) &&
        buffer.ReadU8(&candidate.mAge) && buffer.ReadU8(&candidate.mFacing) && buffer.ReadU8(&candidate.mVariation);
    candidate.mNeed = flags & 7;
    candidate.mCanHaveChocolate = (flags & 8) != 0;
    if (!decoded || (flags & ~15) || !NetZenPlantValid(candidate, zenGarden, seedCount, variationCount))
    {
        buffer.mReadPos = start;
        return false;
    }
    plant = candidate;
    return true;
}

struct NetZenGardenState
{
    uint8_t mGarden = 255;
    bool mStinkyChocolate = false;
    std::array<uint16_t, 3> mSupplies{}; // fertilizer, bug spray, chocolate
    NetZenPlantState mWheelbarrow;
};

inline bool NetZenGardenValid(const NetZenGardenState& garden, bool zenGarden,
    uint8_t seedCount, uint8_t variationCount)
{
    if ((zenGarden && garden.mGarden != 0 && garden.mGarden != 1 && garden.mGarden != 3) ||
        (!zenGarden && (garden.mGarden != 255 || garden.mStinkyChocolate))) return false;
    for (uint16_t supply : garden.mSupplies)
        if (supply > 2000 || (!zenGarden && supply)) return false;
    return !garden.mWheelbarrow.mNeed && !garden.mWheelbarrow.mCanHaveChocolate &&
        NetZenPlantValid(garden.mWheelbarrow, zenGarden, seedCount, variationCount);
}

template <class Buffer>
bool NetEncodeZenGarden(const NetZenGardenState& garden, Buffer& buffer, bool zenGarden,
    uint8_t seedCount, uint8_t variationCount)
{
    if (!NetZenGardenValid(garden, zenGarden, seedCount, variationCount)) return false;
    buffer.WriteU8(garden.mGarden); buffer.WriteU8(garden.mStinkyChocolate ? 1 : 0);
    for (uint16_t supply : garden.mSupplies) buffer.WriteU16(supply);
    return NetEncodeZenPlant(garden.mWheelbarrow, buffer, zenGarden, seedCount, variationCount);
}

template <class Buffer>
bool NetDecodeZenGarden(Buffer& buffer, NetZenGardenState& garden, bool zenGarden,
    uint8_t seedCount, uint8_t variationCount)
{
    const auto start = buffer.mReadPos;
    NetZenGardenState candidate;
    uint8_t flags = 0;
    bool decoded = buffer.ReadU8(&candidate.mGarden) && buffer.ReadU8(&flags);
    candidate.mStinkyChocolate = flags != 0;
    for (uint16_t& supply : candidate.mSupplies) decoded = decoded && buffer.ReadU16(&supply);
    if (!decoded || flags > 1 ||
        !NetDecodeZenPlant(buffer, candidate.mWheelbarrow, zenGarden, seedCount, variationCount) ||
        !NetZenGardenValid(candidate, zenGarden, seedCount, variationCount))
    {
        buffer.mReadPos = start;
        return false;
    }
    garden = candidate;
    return true;
}
}
