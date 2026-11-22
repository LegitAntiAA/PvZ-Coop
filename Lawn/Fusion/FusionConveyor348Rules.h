#pragma once
#include "FusionSeedBank.h"
#include "FusionConveyor305Rules.h"
#include "FusionNativePlacement.h"
#include "../Net/NetMystery.h"
#include <algorithm>
#include <span>

namespace LawnFusion
{
// The caller derives SourceConveyor from its validated Adventure descriptor.
// These are temporary raw snapshot records, not another live card queue.
template<class Metadata,class SeedForKind>
bool FusionConveyorRaw348Valid(const FusionBankState& selected,std::span<const Metadata> metadata,
    std::span<const LawnNet::NetMysterySeedSnapshot> raw,const SeedForKind& seedForKind)
{
    if(raw.size()!=kFusionSourceConveyor305Capacity||selected.targetCount!=kFusionSourceConveyor305Capacity||
       selected.packets.size()>raw.size()||metadata.size()!=selected.packets.size())return false;
    for(size_t i=0;i<raw.size();++i)
    {
        const auto& p=raw[i];
        if(p.mOwner!=0xff||p.mOriginLevel||p.mImitater!=255||p.mCooldown!=255||p.mRefreshTime||p.mRefreshing||
           p.mSlotCountdown||p.mSlotNextSeed!=255||p.mSlotPosition!=0||p.mActive>1||p.mOffsetX<0||p.mOffsetX>1024)return false;
        if(i>=selected.packets.size())
        {if(p.mId||p.mType!=255||p.mActive||p.mOffsetX)return false;continue;}
        const auto& s=selected.packets[i];
        if(!p.mId||p.mType!=seedForKind(s.kind))return false;
        for(size_t j=0;j<i;++j)if(raw[j].mId==p.mId)return false;
        const auto found=std::find_if(metadata.begin(),metadata.end(),[&](const auto& m){return m.id==p.mId;});
        if(found==metadata.end())return false;
        const auto& m=found->data;
        if(m.selectionId!=s.id||m.kind!=s.kind||m.isExtra||m.owner!=0xff||m.randomOrigin||m.price||m.rechargeSeconds!=0||
           s.isExtra||s.owner!=0xff||s.randomOrigin||s.mysteryPending||s.price||s.rechargeSeconds!=0||s.remainingRechargeSeconds!=0||
           s.conveyorUses>=(IsFusionSourcePuffKind(s.kind)?3:1))return false;
    }
    return true;
}
}
