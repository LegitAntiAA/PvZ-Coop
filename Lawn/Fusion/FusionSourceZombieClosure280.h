#pragma once
#include "FusionZombieRules.h"
#include "FusionDayZombie273Rules.h"
#include <algorithm>
#include <span>
#include <string>
#include <vector>

namespace LawnFusion
{
// Pure canonical NPC closure. No level list, carrier substitution, world, RNG
// or resource loading. Failure preserves output, including when roots aliases it.
inline bool BuildFusionSourceZombieClosure280(std::span<const uint16_t> roots,
    std::vector<uint16_t>& output,std::string* error=nullptr)
{
    std::vector<uint16_t> pending;
    const auto add=[&](auto&& self,uint16_t kind,uint16_t root)->bool
    {
        if(!FindFusionZombieProfile(kind)||GetNativeSourceZombieCarrier(kind)<0)
        {if(error)*error="Source NPC factory unavailable rootKind="+std::to_string(root)+" dependencyKind="+std::to_string(kind);return false;}
        if(std::find(pending.begin(),pending.end(),kind)!=pending.end())return true;
        pending.push_back(kind);
        for(auto child:FusionDayZombie273NpcDependencies(kind))if(!self(self,child,root))return false;
        if((kind==35||kind==36||kind==40||kind==43||kind==212)&&!self(self,37,root))return false;
        if((kind==43||kind==212)&&!self(self,40,root))return false;
        if(kind==212&&!self(self,213,root))return false;
        if(kind==223&&!self(self,203,root))return false;
        return true;
    };
    for(auto root:roots)if(!add(add,root,root))return false;
    output.swap(pending);if(error)error->clear();return true;
}
}
