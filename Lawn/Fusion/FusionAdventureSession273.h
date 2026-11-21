#pragma once
#include "FusionAdventureProgress273.h"
#include <cstdint>

namespace LawnNet { class NetBuffer; }
namespace LawnFusion
{
// Replicated view of the one native Board. Wave plans, RNG and health caches
// belong to the host scheduler and are never reconstructed by a client.
struct FusionAdventureSession273
{
    bool enabled=false;
    uint8_t sourceDifficulty=2;
    FusionAdventureProfile273 profile;
    FusionAdventureRun273 run;
    uint16_t wave=0,maximumWaves=0;
    float countdown=0,hugeElapsed=0;
    bool hugeWarning=false,schedulerFailed=false;
};
bool FusionAdventureSessionValid273(const FusionAdventureSession273&);
bool EncodeFusionAdventureSession273(const FusionAdventureSession273&,LawnNet::NetBuffer&);
// Transactional: failed input changes neither the cursor nor the destination.
bool DecodeFusionAdventureSession273(LawnNet::NetBuffer&,FusionAdventureSession273&);
}
