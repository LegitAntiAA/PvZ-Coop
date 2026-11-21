#ifndef LAWN_FUSION_MORE_PLANTS269_RULES_H
#define LAWN_FUSION_MORE_PLANTS269_RULES_H
#include "FusionShooterTiming.h"
#include <span>
#include <cstdint>
namespace LawnFusion
{
struct FusionHamburger269Shot {uint16_t kind;int damage;uint8_t fire;};
// Duplicate source entries are weights, never de-duplicated/native-filtered.
std::span<const uint16_t> FusionHamburger269Roster(unsigned stage);
bool TryChooseFusionHamburger269Shot(unsigned stage,int sourceDamage,int draw,int peaDraw,FusionHamburger269Shot&);
bool FusionHamburger269EventAllowed(unsigned stage,unsigned event);
const FusionShooterTimingProfile* FindFusionMorePlants269Timing(uint16_t kind,unsigned stage=0);
bool FusionMorePlants269StateValid(uint16_t kind,uint8_t mode,uint16_t bag,uint16_t rage,uint32_t shots,int production,int pending);
// Present.RandomPlant enumerates EVERY integer0..32, regardless of support.
std::span<const uint16_t> FusionPresent269AllCandidates();
bool FusionMorePlants269SquashTarget(int status,bool sameRow,bool attackLandAllowed);
struct FusionSqualour269Drops {bool self=false,upgraded=false;unsigned squash=0;};
FusionSqualour269Drops PlanFusionSqualour269Drops(bool hit,unsigned count);
}
#endif
