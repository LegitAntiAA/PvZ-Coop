#include "FusionExtendedZombie347Rules.h"
#pragma once
#include "FusionZombieRules.h"
#include "FusionRandomZombie360Rules.h"
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
        if(FusionRandomZombie360Kind(kind))
        {
            std::vector<uint16_t> outcomes;if(!BuildFusionRandom360Pool(kind,false,outcomes,error))return false;
            for(auto child:outcomes)if(!self(self,child,root))return false;
        }
        for(auto child:FusionExtended347Dependencies(kind))if(!self(self,child,root))return false;
        for(auto child:FusionDayZombie273NpcDependencies(kind))if(!self(self,child,root))return false;
        if((kind==35||kind==36||kind==40||kind==43||kind==212)&&!self(self,37,root))return false;
        if((kind==43||kind==212)&&!self(self,40,root))return false;
        if(kind==212&&!self(self,213,root))return false;
        if(kind==223&&!self(self,203,root))return false;
        if(kind==207&&!self(self,6,root))return false;
        if(kind==209&&!self(self,30,root))return false;
        if(kind==117&&!self(self,116,root))return false;
        if(kind==201)for(uint16_t k:{16,18,10})if(!self(self,k,root))return false;
        if(kind==222)for(uint16_t k:{201,207,217})if(!self(self,k,root))return false;
        return true;
    };
    for(auto root:roots)if(!add(add,root,root))return false;
    output.swap(pending);if(error)error->clear();return true;
}
}
