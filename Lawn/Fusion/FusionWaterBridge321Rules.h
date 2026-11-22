#pragma once
#include "FusionZombieRules.h"
#include <array>
#include <cmath>
namespace LawnFusion
{
using FusionContactQuad321=std::array<std::array<float,2>,4>;
// Actual source weapon oriented box versus the existing native plant collider.
// This shared SAT also handles Driver/Dolphin source boxes. Touching is contact.
inline bool FusionContactQuads321(const FusionContactQuad321& a,const FusionContactQuad321& b)
{
    for(const auto* q:{&a,&b})for(const auto& p:*q)
        if(!std::isfinite(p[0])||!std::isfinite(p[1]))return false;
    for(const auto* q:{&a,&b})for(size_t i=0;i<4;++i)
    {
        const auto& p=(*q)[i];const auto& n=(*q)[(i+1)%4];
        const double nx=double(n[1])-p[1],ny=double(p[0])-n[0];
        if(nx==0&&ny==0)return false;
        double amin=double(a[0][0])*nx+double(a[0][1])*ny,amax=amin;
        double bmin=double(b[0][0])*nx+double(b[0][1])*ny,bmax=bmin;
        for(size_t j=1;j<4;++j)
        {
            const double av=double(a[j][0])*nx+double(a[j][1])*ny;
            const double bv=double(b[j][0])*nx+double(b[j][1])*ny;
            if(av<amin)amin=av;if(av>amax)amax=av;
            if(bv<bmin)bmin=bv;if(bv>bmax)bmax=bv;
        }
        if(amax<bmin||bmax<amin)return false;
    }
    return true;
}
}
