#include "FusionConveyor305Tests.h"
#include "../../Lawn/Fusion/FusionSeedBank.h"
#include "../../Lawn/Fusion/FusionConveyor305Rules.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>

namespace
{
using namespace LawnFusion;
int checks = 0, failures = 0;
void Check(bool value, const char* message)
{ ++checks; if (!value) { ++failures; std::printf("FAIL305 %s\n",message); } }
const std::vector<FusionBankEligibleCard> conveyorPool = {{0,false,0,0},{6,false,0,0},{13,false,0,0},{1031,false,0,0}};
constexpr auto source = FusionBankPolicy::SourceConveyor;
bool Valid(const FusionBankState& state)
{ return FusionSeedBank::ValidateSnapshot(state,conveyorPool,kFusionSourceConveyor305Capacity,source); }

void SelectedRegression()
{
    std::vector<FusionBankEligibleCard> pool;
    for (uint16_t kind=0; kind<14; ++kind)
    { FusionBankEligibleCard card; Check(TryMakeFusionEligibleCard(kind,false,card),"selected catalog fixture"); pool.push_back(card); }
    uint32_t epoch = 1;
    for (uint8_t mask=1; mask<16; ++mask) for (size_t capacity=1; capacity<=14; ++capacity)
    {
        FusionSeedBank bank;
        Check(bank.Open(epoch,10,pool,mask,capacity) && bank.Policy()==FusionBankPolicy::Selected,"selected default policy/quota open");
        uint16_t kind=0;
        for (int slot=0; slot<4; ++slot) if (mask&(1<<slot))
        {
            for (int n=0;n<bank.State().quotas[slot];++n) Check(bank.Choose(epoch,slot,kind++,false),"selected choices fill native quotas");
            Check(bank.SetReady(epoch,slot,true),"selected active slot ready");
        }
        // A later choice clears Ready as before, so mark the completed roster together.
        for (int slot=0; slot<4; ++slot) if (mask&(1<<slot)) Check(bank.SetReady(epoch,slot,true),"selected completed ready barrier");
        Check(bank.CommitIntro(epoch) && bank.BeginPlaying(epoch) && FusionSeedBank::ValidateSnapshot(bank.State(),pool,capacity),"selected intro/playing full roster still valid");
        const auto original=bank.State(); const auto& packet=original.packets.front();
        Check(!bank.AppendConveyor(epoch,0) && !bank.ConsumeConveyor(epoch,packet.owner,packet.id) && bank.State()==original,"selected rejects conveyor mutations atomically");
        Check(bank.CommitPacketUse(epoch,packet.owner,packet.id) && !bank.PeekReadyPacket(epoch,packet.owner,packet.id) &&
            bank.AdvanceRecharge(epoch,packet.rechargeSeconds) && bank.PeekReadyPacket(epoch,packet.owner,packet.id),"selected real cooldown unchanged");
        auto malformed=bank.State(); ++malformed.revision; malformed.packets.front().conveyorUses=1;
        const auto before=bank.State();
        Check(!FusionSeedBank::ValidateSnapshot(malformed,pool,capacity) && !bank.ApplySnapshot(malformed) && bank.State()==before,"selected rejects conveyorUses without weakening validator");
        malformed=before;++malformed.revision;malformed.packets.erase(malformed.packets.begin());
        Check(!bank.ApplySnapshot(malformed) && bank.State()==before,"Selected Playing cannot remove immutable chosen identity");
        ++epoch;
    }
    FusionSeedBank mystery;
    Check(mystery.Open(1000,5,pool,3,6) && mystery.ToggleMystery(1000,0,true) && mystery.ToggleMystery(1000,1,true) &&
        mystery.SetReady(1000,0,true) && mystery.SetReady(1000,1,true) && mystery.CommitIntro(1000) && mystery.BeginPlaying(1000),"selected mystery reaches Playing");
    const auto hidden=mystery.State();
    Check(!mystery.Reveal(1000,[](size_t,size_t&){return false;}) && mystery.State()==hidden,"selected RNG rejection remains atomic");
    Check(mystery.Reveal(1000,[](size_t,size_t& n){n=0;return true;}) && FusionSeedBank::ValidateSnapshot(mystery.State(),pool,6),"selected mystery reveal still valid");
    std::vector<uint16_t> ids; for(const auto& p:mystery.State().packets)ids.push_back(p.id);
    Check(mystery.ReconcilePlayers(1000,1),"selected disconnect reassigns owners");
    bool same=true;for(size_t i=0;i<ids.size();++i)same &= ids[i]==mystery.State().packets[i].id && mystery.State().packets[i].owner==0;
    Check(same,"selected disconnect preserves identities and existing behavior");
}

void BankAndSnapshots()
{
    FusionSeedBank bank;
    Check(bank.OpenConveyor(305,7,conveyorPool,3) && bank.Policy()==source && bank.State().phase==FusionBankPhase::Intro &&
        bank.State().targetCount==13 && bank.State().packets.empty() && Valid(bank.State()),"source opens empty Intro capacity13");
    const auto intro=bank.State();uint16_t untouched=999;
    Check(!bank.AppendConveyor(305,6,&untouched) && untouched==999 && !bank.ConsumeConveyor(305,0,1) && bank.State()==intro,"Intro cannot produce/consume Playing cards");
    Check(!bank.Choose(305,0,6,false) && !bank.ToggleMystery(305,0,true) && !bank.SetReady(305,0,true) && !bank.CommitIntro(305) && bank.State()==intro,"source skips selected controls");
    Check(bank.ReconcilePlayers(305,1) && bank.State().packets.empty() && !bank.ReconcilePlayers(305,3),"Intro disconnect only, no added roster");
    Check(bank.BeginPlaying(305) && !bank.Reveal(305,[](size_t,size_t& n){n=0;return true;}),"existing BeginPlaying works, source rejects mystery reveal");
    for(int i=0;i<13;++i) {uint16_t id=0;Check(bank.AppendConveyor(305,6,&id) && id==i+1,"duplicate kind append creates monotonic identity");}
    const auto full=bank.State();
    Check(Valid(full) && !FusionSeedBank::ValidateSnapshot(full,conveyorPool,13) && !FusionSeedBank::ValidateSnapshot(full,conveyorPool,14,source),"source policy external and exact13, never inferred");
    Check(!bank.AppendConveyor(305,6,&untouched) && untouched==999 && bank.State()==full,"14th packet rejected unchanged including output ID");
    Check(!bank.ConsumeConveyor(305,1,1) && !bank.ConsumeConveyor(304,0,1) && !bank.CommitPacketUse(305,0,1) && bank.State()==full,"authenticated active slot/epoch required and no selected recharge path");
    Check(bank.ConsumeConveyor(305,0,1) && bank.State().packets.front().conveyorUses==1 && bank.PeekReadyPacket(305,0,1),"Puff first use remains ready");
    Check(bank.ConsumeConveyor(305,0,1) && bank.State().packets.front().conveyorUses==2 && bank.State().packets.size()==13,"Puff second use retained in same packet");
    Check(bank.ConsumeConveyor(305,0,1) && bank.State().packets.size()==12 && !bank.PeekReadyPacket(305,0,1),"Puff third use retires identity");
    Check(bank.AppendConveyor(305,0,&untouched) && untouched==14 && bank.ConsumeConveyor(305,0,14) && bank.State().packets.size()==12,"ordinary card one use and no recycled IDs");
    const auto beforeOpen=bank.State();
    auto invalidPool=conveyorPool;invalidPool.front().baseCost=1;
    Check(!bank.OpenConveyor(306,1,invalidPool,1) && !bank.Open(306,1,{},1) && bank.State()==beforeOpen && bank.Policy()==source && bank.AppendConveyor(305,13),"failed opens preserve policy/state/eligibility");
    auto same=bank.State();
    Check(bank.ApplySnapshot(same),"identical revision source replay succeeds");
    auto wrong=same;wrong.packets.front().conveyorUses=1;
    Check(!bank.ApplySnapshot(wrong) && bank.State()==same,"changed same-revision snapshot rejects full usage comparison");

    FusionSeedBank shared;
    Check(shared.OpenConveyor(900,1,conveyorPool,3) && shared.BeginPlaying(900) && shared.AppendConveyor(900,1031),"two-player shared Puff opens");
    Check(shared.ConsumeConveyor(900,0,1) && shared.PeekReadyPacket(900,1,1) && shared.ConsumeConveyor(900,1,1),"both active players consume the same canonical Puff identity");
    const auto sharedCards=shared.State().packets;
    Check(shared.ReconcilePlayers(900,1) && shared.State().packets==sharedCards && shared.State().readyMask==0 && shared.State().quotas==std::array<uint8_t,4>{},"disconnect preserves shared queue order/usage and zero quotas");
    Check(!shared.ConsumeConveyor(900,1,1) && shared.ConsumeConveyor(900,0,1) && shared.State().packets.empty(),"disconnected user rejected; surviving user consumes third use");

    FusionSeedBank client;
    Check(client.OpenConveyor(305,7,conveyorPool,3) && client.ApplySnapshot(same) && client.State()==same,"late client adopts dynamic Playing source queue");
    const auto initial=client.State();
    std::vector<FusionBankState> bad;
    const auto add=[&](auto mutate){auto v=initial;++v.revision;mutate(v);bad.push_back(v);};
    add([](auto& v){v.targetCount=12;}); add([](auto& v){v.activeMask=0;});add([](auto& v){v.activeMask=16;});
    add([](auto& v){v.readyMask=1;});add([](auto& v){v.mysteryMask=1;});add([](auto& v){v.quotas[0]=1;});
    add([](auto& v){v.phase=FusionBankPhase::Choosing;});add([](auto& v){v.nextPacketId=0;});add([](auto& v){v.nextPacketId=65537;});
    add([](auto& v){v.packets[0].owner=0;});add([](auto& v){v.packets[0].price=-1;});add([](auto& v){v.packets[0].isExtra=true;});
    add([](auto& v){v.packets[0].randomOrigin=1;});add([](auto& v){v.packets[0].mysteryPending=true;});
    add([](auto& v){v.packets[0].rechargeSeconds=(std::numeric_limits<float>::quiet_NaN)();});
    add([](auto& v){v.packets[0].remainingRechargeSeconds=(std::numeric_limits<float>::infinity)();});
    add([](auto& v){v.packets[0].kind=65535;});add([](auto& v){v.packets[0].conveyorUses=3;});
    add([](auto& v){v.packets.back().conveyorUses=1;});add([](auto& v){v.packets[0].id=0;});
    add([](auto& v){v.nextPacketId=v.packets.back().id;});add([](auto& v){std::swap(v.packets[0],v.packets[1]);});
    add([](auto& v){v.packets[1].id=v.packets[0].id;});add([](auto& v){v.packets.push_back(v.packets.back());});
    for(const auto& v:bad)Check(!Valid(v) && !client.ApplySnapshot(v) && client.State()==initial,"malformed canonical source snapshot rejects atomically");
    wrong=initial;++wrong.revision;wrong.packets[0].kind=13;
    Check(Valid(wrong) && !client.ApplySnapshot(wrong) && client.State()==initial,"surviving packet cannot be retagged");
    auto used=initial;++used.revision;used.packets[0].conveyorUses=2;
    Check(client.ApplySnapshot(used),"missed Puff usage snapshots can advance directly to2");
    wrong=used;++wrong.revision;wrong.packets[0].conveyorUses=1;
    Check(Valid(wrong) && !client.ApplySnapshot(wrong) && client.State()==used,"Puff usage rollback rejected");
    auto removed=used;++removed.revision;removed.packets.erase(removed.packets.begin());
    Check(client.ApplySnapshot(removed),"source consumes dynamic identity while Playing");
    wrong=used;wrong.revision=removed.revision+1;
    Check(Valid(wrong) && !client.ApplySnapshot(wrong) && client.State()==removed,"removed old identity cannot reappear");
    auto appended=removed;++appended.revision;FusionBankPacket p;p.id=static_cast<uint16_t>(appended.nextPacketId++);p.kind=0;appended.packets.push_back(p);
    Check(client.ApplySnapshot(appended),"Playing snapshot appends fresh monotonic card");
    wrong=appended;++wrong.revision;wrong.activeMask=3;
    Check(Valid(wrong) && !client.ApplySnapshot(wrong),"new active slot requires fresh epoch");
    Check(!client.ApplySnapshot(initial),"old revision cannot restore old queue");

    FusionSeedBank overflow;Check(overflow.OpenConveyor(1001,1,conveyorPool,1) && overflow.BeginPlaying(1001),"overflow fixture opens");
    auto last=overflow.State();++last.revision;last.nextPacketId=65536;p.id=65535;p.kind=6;last.packets={p};
    Check(overflow.ApplySnapshot(last) && Valid(last),"last U16 packet identity representable");
    untouched=123;Check(!overflow.AppendConveyor(1001,0,&untouched) && untouched==123 && overflow.State()==last,"exhausted U16 ID space rejects unchanged");
    Check(overflow.ConsumeConveyor(1001,0,65535),"ID exhaustion does not prohibit existing card use");
    auto exhausted=overflow.State();exhausted.revision=(std::numeric_limits<uint32_t>::max)();
    Check(overflow.ApplySnapshot(exhausted) && !overflow.ConsumeConveyor(1001,0,65535) && !overflow.ReconcilePlayers(1001,1) && overflow.State()==exhausted,"revision exhaustion blocks mutations without wrap");
    std::vector<FusionBankEligibleCard> selected;FusionBankEligibleCard pea;TryMakeFusionEligibleCard(0,false,pea);selected.push_back(pea);
    Check(overflow.Open(1002,1,selected,1,1) && overflow.Policy()==FusionBankPolicy::Selected && overflow.Choose(1002,0,0,false),"successful fresh Selected Open resets external policy");
}

void Feed()
{
    const auto zero=[](size_t,size_t& n){n=0;return true;};
    for(int level:{27,36,45})
    {
        const auto* profile=FindFusionConveyor305Profile(level);Check(profile && profile->orderedPool.size()>1,"campaign profile available");
        for(size_t wanted=0;wanted<profile->orderedPool.size();++wanted)
        {
            FusionConveyor305State state;FusionConveyor305Step step;int calls=0;
            Check(StepFusionConveyor305(level,state,{},0,false,[&](size_t count,size_t& index){++calls;Check(count==(calls==1?profile->orderedPool.size():3),"source pool weighting/rejection draw bounds");index=calls==1?wanted:0;return true;},step) &&
                step.birth && step.kind==profile->orderedPool[wanted] && state.birthCount==1 && state.elapsedSeconds==0 && calls==2,"each ordered weighted pool entry selectable at first dt0 birth");
        }
        FusionConveyor305State state{profile->intervalSeconds,7};FusionConveyor305Step step{true,999};
        Check(StepFusionConveyor305(level,state,{},0,false,{},step) && !step.birth && state.elapsedSeconds==profile->intervalSeconds,"strict elapsed>interval, not >=");
        Check(StepFusionConveyor305(level,state,{},.01f,false,zero,step) && step.birth && state.elapsedSeconds==0 && state.birthCount==8,"source due birth resets overshoot");
        Check(StepFusionConveyor305(level,state,{},10000,false,zero,step) && step.birth && state.birthCount==9 && state.elapsedSeconds==0,"large delta still only one birth");
        std::vector<uint16_t> full(13,profile->orderedPool.front());const auto before=state;int calls=0;
        Check(StepFusionConveyor305(level,state,full,100,false,[&](size_t,size_t&){++calls;return false;},step) && !step.birth && state==before && calls==0,"full13 freezes elapsed and RNG");
        Check(StepFusionConveyor305(level,state,{},100,true,{},step) && !step.birth && state==before,"pause freezes source feed");
        Check(StepFusionConveyor305(level,state,{},profile->intervalSeconds,false,{},step) && !step.birth && state.elapsedSeconds==profile->intervalSeconds,"capacity release resumes without banking full time");
    }
    Check(!FindFusionConveyor305Profile(9) && !FindFusionConveyor305Profile(18) && !FindFusionConveyor305Profile(80),"ordinary finals and unimplemented mini policies not conveyors here");
    FusionConveyor305State state;FusionConveyor305Step step{true,999};const auto before=state;const auto sentinel=step;
    const auto rejects=[&](int level,std::span<const uint16_t> q,float dt,bool pause,const FusionConveyor305RandomIndex& rng)
    {Check(!StepFusionConveyor305(level,state,q,dt,pause,rng,step) && state==before && step==sentinel,"invalid feed/RNG leaves state+output untouched");};
    rejects(9,{},0,false,zero); rejects(27,{},-1,false,zero);rejects(27,{},(std::numeric_limits<float>::quiet_NaN)(),false,zero);
    rejects(27,{},(std::numeric_limits<float>::infinity)(),false,zero);rejects(27,{},0,false,{});
    rejects(27,{},0,false,[](size_t,size_t&){return false;});
    rejects(27,{},0,false,[](size_t count,size_t& n){n=count;return true;});
    rejects(27,{},0,false,[](size_t,size_t&)->bool{throw std::runtime_error("RNG");});
    int calls=0;rejects(27,{},0,false,[&](size_t,size_t& n){n=0;return ++calls!=2;});
    std::vector<uint16_t> tooMany(14,12),unknown{999};rejects(27,tooMany,0,true,zero);rejects(27,unknown,0,false,zero);
    calls=0;const std::vector<uint16_t> retryQueue{12};
    rejects(27,retryQueue,0,false,[&](size_t,size_t& n){n=0;return ++calls!=3;});
    const std::vector<uint16_t> queue{12,12};std::vector<size_t> bounds;calls=0;
    Check(StepFusionConveyor305(27,state,queue,0,false,[&](size_t count,size_t& n){bounds.push_back(count);n=0;++calls;return true;},step) && step.kind==13 && bounds==std::vector<size_t>{10,3,8,3},"source anti-duplicate removes all weighted occurrences before reroll");
    state={};step=sentinel;const std::vector<uint16_t> saturated{27,28,32,26,30,10,16};calls=0;
    Check(StepFusionConveyor305(45,state,saturated,0,false,[&](size_t,size_t& n){n=0;++calls;return true;},step) && step.birth && step.kind==16 && calls==14,"exhausted pool returns last overrepresented kind, no infinite rejection loop");
    state={10,(std::numeric_limits<uint32_t>::max)()};const auto maxCount=state;step=sentinel;calls=0;
    Check(!StepFusionConveyor305(27,state,{},0,false,[&](size_t,size_t&){++calls;return false;},step) && state==maxCount && step==sentinel && calls==0,"birth counter overflow rejected before RNG");
    state={(std::numeric_limits<float>::max)(),0};const auto maxTime=state;
    Check(!StepFusionConveyor305(27,state,{},(std::numeric_limits<float>::max)(),false,zero,step) && state==maxTime && step==sentinel,"finite delta sum overflow rejected atomically");
}
}

FusionConveyor305TestResult RunFusionConveyor305Tests()
{
    checks=failures=0;SelectedRegression();BankAndSnapshots();Feed();
    std::printf("FusionConveyor305 checks=%d failures=%d\n",checks,failures);
    return {checks,failures};
}
