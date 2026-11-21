#pragma once
#include <cstdint>
#include <span>
#include <string>
class Board;
namespace LawnFusion
{
// Resource/pose/controller dependency closure, before any scheduler RNG. This
// does not grant terrain/mode admission or reserve future entity pool slots.
bool EnsureNativeSourceZombieClosure280(Board*,std::span<const uint16_t> roots,std::string* error=nullptr);
// Current host whole-wave capacity, before row RNG or actor writes. Existing
// ordinary Day carrier/world limits remain fail-closed; no deletion credit.
bool CanAllocateNativeSourceZombieBatch280(Board*,std::span<const uint16_t> actualKinds,std::string* error=nullptr);
}
