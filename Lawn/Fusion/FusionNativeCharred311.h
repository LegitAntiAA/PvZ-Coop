#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
class Board;class LawnApp;class Zombie;class Reanimation;
namespace Sexy {class Graphics;}
namespace LawnFusion
{
// Source SetCarred3881A0 selection only, separate from Charred damage/threshold.
// 0=no standalone visual, 1=normal, 2=gargantuar. Actual damage is separate.
inline uint8_t SourceCharred311SourceVariant(uint16_t kind,int status,bool hasShadow,bool inWater,bool beforeDying)
{
    if(!hasShadow||inWater||beforeDying||status<0)return 0;
    switch(kind)
    {
    case 16:case 18:case 26:case 31:case 37:case 38:case 39:case 40:case 45:
    case 112:case 201:case 205:case 206:case 210:case 213:case 216:case 219:case 221:case 222:return 0;
    }
    switch(status){case 1:case 3:case 10:case 12:case 20:case 21:return 0;}
    return kind==35||kind==36?2:1;
}
struct SourceCharred311Birth
{
    uint32_t epoch=0;
    uint16_t ownerNetID=0,sourceKind=0;
    uint32_t ownerToken=0;
    uint8_t variant=1,sourceRow=0;
    float shadowX=0,shadowY=0;
    bool rootFlip=false;
    int32_t order=303000;
    uint32_t eventSerial=0; // Appended immutable wire field: charred0; repeated FX3/4 nonzero.
};
// Current native row-layer mapping: Board::MakeRenderOrder(ZOMBIE,row,0).
// This is source NEW SortingGroup order0, never the dying owner's order/+1.
inline bool SourceCharred311BirthValid(const SourceCharred311Birth& v)
{
    if(!v.epoch||!v.ownerNetID||!v.ownerToken||v.sourceRow>5||
       !std::isfinite(v.shadowX)||!std::isfinite(v.shadowY)||std::abs(v.shadowX)>4096||std::abs(v.shadowY)>4096)return false;
    if(v.variant==1||v.variant==2)
        return !v.eventSerial&&SourceCharred311SourceVariant(v.sourceKind,0,true,false,false)==v.variant&&
            v.order==303000+10000*int(v.sourceRow);
    if(v.variant==3)return v.eventSerial&&!v.rootFlip&&v.order==307000+10000*int(v.sourceRow);
    if(v.variant==4)return v.eventSerial&&!v.rootFlip&&v.order==400040;
    return false;
}
// Resources + native full-generation presentation leases, no gameplay world or
// independent clock. Actual native once Reanimation owns all three seconds.
class NativeSourceCharred311
{
public:
    NativeSourceCharred311(Board*,uint32_t epoch);
    ~NativeSourceCharred311();
    NativeSourceCharred311(const NativeSourceCharred311&)=delete;
    NativeSourceCharred311& operator=(const NativeSourceCharred311&)=delete;
    bool Ensure(Board*,size_t wholePeak,std::string&);
    bool EnsureVariant(Board*,uint8_t variant,size_t wholePeak,std::string&);
    // Valid duplicate/cosmetic capacity drop is acknowledged with fullUID=0;
    // canonical seen ownership survives native clock and source actor deletion.
    bool Start(Board*,const SourceCharred311Birth&,uint32_t& fullUID,std::string&);
    // Only true for an owned lease. No native resource/actor allocation,
    // clock advancement, callback or emission.
    bool Draw(Sexy::Graphics*,const Reanimation*) const;
private:
    struct Impl;std::unique_ptr<Impl> mImpl;
};
// Read-only exact source capture, before Die2. Source status/water/beforeDying
// and effective root rotation are supplied by the real source family controller;
// never infer these semantic fields from HP or the generic carrier phase.
bool TryMakeNativeSourceCharred311(Zombie*,int sourceStatus,bool sourceInWater,bool sourceBeforeDying,
    bool sourceRootFlip,SourceCharred311Birth&,std::string* error=nullptr);
bool EnsureNativeSourceCharred311(Board*,size_t wholePeak=1,std::string* error=nullptr);
bool StartNativeSourceCharred311(Board*,const SourceCharred311Birth&,uint32_t* fullUID=nullptr,std::string* error=nullptr);
bool DrawNativeSourceCharred311(Sexy::Graphics*,const Reanimation*);
}
