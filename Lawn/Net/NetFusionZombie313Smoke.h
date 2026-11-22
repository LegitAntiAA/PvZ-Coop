#pragma once
class LawnApp;
namespace LawnNet
{
// PRIVATE CLI fixture only. Native StartFusionValidation uses the requested
// level1/11/21/31 to initialize the real biome BEFORE entity/context creation.
// Returns0 outside this fixture, -1 for an invalid private biome argument.
int NetSmokeFusionZombie313Level325(const LawnApp*);
bool NetSmokeUpdateFusionZombie313(LawnApp*);
}

// Optional ZOMBIE313_BEHAVIOR=1: fresh five-root epochs, actual factories,
// native clocks/geometry + paused canonical FNS, and explicit source API probes.
