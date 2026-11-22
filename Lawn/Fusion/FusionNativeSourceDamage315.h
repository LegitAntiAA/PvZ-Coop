#pragma once
#include <string>
class Zombie;
namespace LawnFusion
{
// Base GetDamage only; family virtual overrides/caps are applied by their
// typed bridge after this result. Source212 protection respects fix/MC.
bool ResolveNativeSourceBaseDamage315(Zombie*,int raw,bool fix,int& resolved);
bool ApplyNativeSourceTypedDamage313(Zombie*,int sourceType,int raw,bool fix=false);
// Genuine source Charred, with independent source shield damage and type12
// fallback. No native18U/burn substitution and no native integer-HP threshold.
bool CanApplyNativeSourceCharred315(Zombie*,int,std::string* error=nullptr);
bool ApplyNativeSourceCharred315(Zombie*,int);
}
