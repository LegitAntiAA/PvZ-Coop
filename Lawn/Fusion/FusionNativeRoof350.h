#pragma once
#include <string>
class Board;class Zombie;
namespace LawnFusion
{
struct FusionSourceAdventure272;
bool PreflightNativeRoofPreplants350(Board*,const FusionSourceAdventure272&,std::string* error=nullptr);
// Fresh boot only; uses actual default Plant27 factory in source column/row
// order. Ghosts receive the resulting normal Plant records through FNS.
bool InitializeNativeRoofPreplants350(Board*,const FusionSourceAdventure272&,std::string* error=nullptr);
// Native construction runs before source identity/body initialization. Root
// calls once after Attach and positioned-X, before family/effect callbacks.
bool ReconcileNativeRoofZombie350(Zombie*);
}
