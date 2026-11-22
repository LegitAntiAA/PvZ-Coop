#pragma once

#include "FusionSeedBank.h"
#include "FusionConveyor305Rules.h"
#include "FusionAdventureSession273.h"
#include "FusionCampaign272Rules.h"
#include "../../SexyAppFramework/MTRand.h"
#include <array>
#include <memory>
#include <string>

class Board;
class SeedPacket;
class CursorObject;
namespace LawnFusion
{
// The descriptor, not a client flag/phase/capacity, selects this policy.
inline FusionBankPolicy FusionAdventureBankPolicy308(const FusionAdventureSession273& adventure)
{
    if (adventure.enabled && FusionAdventureSessionValid273(adventure))
        if (const auto* source=FindFusionSourceAdventure272(adventure.run.level);source&&source->conveyor)
            return FusionBankPolicy::SourceConveyor;
    return FusionBankPolicy::Selected;
}
struct NativeConveyor308Geometry
{
    int clipLeft=90,clipWidth=501,firstX=91,step=38,top=8,width=38,height=54;
    float scale=.76f;
    constexpr int PacketX(int index) const { return firstX+step*index; }
    constexpr int EntryOffset(int index) const { return clipLeft+clipWidth-PacketX(index); }
};
inline constexpr NativeConveyor308Geometry kNativeConveyor308Geometry{};

FusionBankPolicy GetNativeBankPolicy308(const Board*);
size_t GetNativeBankCapacity308(const Board*);
bool IsNativeSourceConveyor308(const Board*);
bool InitializeNativeConveyor308(Board*,std::string* error=nullptr);
bool PreflightNativeSourceConveyor308(Board*,std::string* error=nullptr);
bool UpdateNativeSourceConveyor308(Board*); // Consumes source policy even when paused/failed; never stock feed.
bool SyncNativeConveyor308(Board*,std::string* error=nullptr); // Authority only; no feed/RNG.

struct NativeConveyor308CursorState
{
    std::array<CursorObject*,5> cursors{};
    std::array<uint16_t,5> packetIds{};
};
// Local bounded capture only; do not retain across a message pump or Board teardown.
NativeConveyor308CursorState CaptureNativeConveyorCursors308(const Board*);
// Root validates raw packets+canonical state+metadata before any snapshot mutation.
// Call after atomic raw/FNS adoption; this never runs a factory/feed/client RNG.
bool AdoptNativeConveyor308(Board*,const NativeConveyor308CursorState&,std::string* error=nullptr);
bool NativeConveyorPacketHeldByOther308(const Board*,uint16_t packetNetId,int slot);
bool CanUseNativeConveyorPacket308(const Board*,uint16_t packetNetId,int slot);

struct NativeConveyor308State
{
    explicit NativeConveyor308State(uint32_t seed):random(seed){}
    FusionConveyor305State feed;
    Sexy::MTRand random; // Copyable native PRNG; host-private explicit adaptation.
    int lastTick=-1;
    bool failed=false;
};
struct NativeConveyor308Plan;
class NativeConveyor308Use
{
public:
    NativeConveyor308Use();
    ~NativeConveyor308Use();
    NativeConveyor308Use(NativeConveyor308Use&&) noexcept;
    NativeConveyor308Use& operator=(NativeConveyor308Use&&) noexcept;
private:
    std::unique_ptr<NativeConveyor308Plan> plan;
    friend bool PrepareNativeConveyorUse308(Board*,int,uint16_t,NativeConveyor308Use&,std::string*);
    friend void CommitNativeConveyorUse308(Board*,NativeConveyor308Use&) noexcept;
};
// Prepare all controller/map storage before world placement; rejection changes none.
bool PrepareNativeConveyorUse308(Board*,int actingSlot,uint16_t packetNetId,NativeConveyor308Use&,std::string* error=nullptr);
// Synchronous same-Board/same-revision commit AFTER successful real placement.
// Native inline capacity was proven before placement; this contains no RNG/allocation.
void CommitNativeConveyorUse308(Board*,NativeConveyor308Use&) noexcept;
}
