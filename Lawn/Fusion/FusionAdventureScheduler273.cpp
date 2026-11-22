#include "FusionAdventureScheduler273.h"
#include "FusionCampaign272Rules.h"
#include "../Net/NetGameplayPolicy.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace LawnFusion
{
namespace
{
using Result=FusionAdventureScheduler273Result;
bool Valid(const FusionAdventureScheduler273State& s)
{
    const auto* config=FindFusionSourceAdventure272(s.level);int waves=0;
    return s.initialized&&config&&s.sourceDifficulty>=0&&s.sourceDifficulty<=5&&
        TryGetFusionCoopAdventureWaves272(s.level,s.cycle,s.coopDifficulty,waves)&&waves==s.maximumWaves&&
        s.rows==config->rows&&s.wave>=0&&s.wave<=waves&&s.waves.size()==size_t(waves)&&
        std::isfinite(s.countdown)&&std::isfinite(s.hold)&&s.hold>=0&&
        std::isfinite(s.healthElapsed)&&s.healthElapsed>=0&&
        std::isfinite(s.hugeElapsed)&&s.hugeElapsed>=0&&
        std::isfinite(s.currentHealth)&&std::isfinite(s.latestWaveHealth)&&
        (s.hugeWarning||s.hugeElapsed==0);
}
bool Integer(const FusionAdventureScheduler273Random& random,int minimum,int maximum,int& out)
{
    if(!random.integer)return false;
    const int draw=random.integer(random.user,minimum,maximum);
    if(draw<minimum||draw>=maximum)return false;
    out=draw;return true;
}
bool Real(const FusionAdventureScheduler273Random& random,float minimum,float maximum,float& out)
{
    if(!random.real)return false;
    const float draw=random.real(random.user,minimum,maximum);
    if(!std::isfinite(draw)||draw<minimum||draw>maximum)return false;
    out=draw;return true;
}
Result Fail(FusionAdventureScheduler273State& s,Result error)
{s.failed=true;return error;}
struct Clock
{
    int wave;
    float countdown,hold,healthElapsed,hugeElapsed,currentHealth,latestWaveHealth;
    bool hugeWarning;
};
void Commit(FusionAdventureScheduler273State& state,const Clock& next)
{
    state.wave=next.wave;state.countdown=next.countdown;state.hold=next.hold;
    state.healthElapsed=next.healthElapsed;state.hugeElapsed=next.hugeElapsed;
    state.currentHealth=next.currentHealth;state.latestWaveHealth=next.latestWaveHealth;
    state.hugeWarning=next.hugeWarning;
}
bool ActorHealth(const FusionAdventureScheduler273Health& actor,uint32_t& bits)
{
    if(!std::isfinite(actor.body)||double(actor.body)<double((std::numeric_limits<int32_t>::min)())||
        double(actor.body)>double((std::numeric_limits<int32_t>::max)()))return false;
    bits=uint32_t(int32_t(actor.body))+uint32_t(actor.firstArmor)+uint32_t(actor.secondArmor);
    return true;
}
float SignedFloat(uint32_t bits)
{
    const int64_t value=bits&0x80000000U?int64_t(bits)-0x100000000LL:int64_t(bits);
    return float(value);
}
}
Result InitializeFusionAdventureScheduler273(FusionAdventureScheduler273State& state,bool host,
    int level,int cycle,int difficulty,const FusionAdventureScheduler273Random& random,
    void* user,bool (*ready)(void*,uint16_t),int coopDifficulty)
{
    if(!host)return Result::NotHost;
    const auto* config=FindFusionSourceAdventure272(level);int maximum=0,total=0;
    if(!config||difficulty<0||difficulty>5||!ready||!random.integer||!random.real||
        !TryGetFusionCoopAdventureWaves272(level,cycle,coopDifficulty,maximum))return Result::Invalid;
    // NewZombieUpdate0x180319250 has extra high-D5 slot rewrites here. They
    // require distinct source actors; the ordinary pure tables cannot admit it.
    int sourceMaximum=0;TryGetFusionSourceAdventureWaves272(level,cycle,sourceMaximum);
    if(difficulty==5&&sourceMaximum>40)return Result::NotReady;
    if(!ready(user,0))return Result::NotReady;
    std::vector<uint16_t> roster;if(!BuildFusionCoopAdventureRoster272(level,coopDifficulty,roster))return Result::Invalid;
    for(auto kind:roster)if(kind!=0&&!ready(user,kind))return Result::NotReady;
    std::vector<FusionSourceZombieWeight272> weights;
    for(const auto& row:FusionSourceZombieWeights272())if(std::find(roster.begin(),roster.end(),row.kind)!=roster.end())
    {weights.push_back(row);total+=row.weight;}
    if(total<=0)return Result::Invalid;
    FusionAdventureScheduler273State candidate;
    candidate.level=level;candidate.cycle=cycle;candidate.sourceDifficulty=difficulty;
    candidate.coopDifficulty=coopDifficulty;candidate.rows=config->rows;candidate.maximumWaves=maximum;candidate.countdown=15.f;
    candidate.waves.resize(size_t(maximum));
    for(int wave=1;wave<=maximum;++wave)
    {
        auto& kinds=candidate.waves[size_t(wave-1)];int remaining=0;
        if(!TryGetFusionSourceWaveBudget272(level,cycle,wave,difficulty,remaining,coopDifficulty))return Result::Invalid;
        remaining=LawnNet::DifficultyWavePoints(remaining,coopDifficulty);
        while(remaining>0&&(coopDifficulty==1||kinds.size()<size_t(LawnNet::kGameplayMaximumZombiesPerWave)))
        {
            int draw=0;uint16_t chosen=0;FusionSourceWavePick272 pick;
            if(!Integer(random,1,total+1,draw))return Result::RandomFailed;
            int weight=0;for(const auto& row:weights){weight+=row.weight;if(draw<=weight){chosen=row.kind;break;}}
            if(!TryResolveFusionSourceAdventurePick272(remaining,chosen,pick))return Result::Invalid;
            kinds.push_back(pick.emittedKind);remaining=pick.remainingBudget;
        }
        std::span<const uint16_t> tail;
        if(!TryGetFusionSourceFinalWaveKinds272(level,cycle,wave==maximum?sourceMaximum:(std::min)(wave,sourceMaximum-1),tail))return Result::Invalid;
        if(coopDifficulty>1&&kinds.size()+tail.size()>size_t(LawnNet::kGameplayMaximumZombiesPerWave))
            kinds.resize(size_t(LawnNet::kGameplayMaximumZombiesPerWave)-tail.size());
        kinds.insert(kinds.end(),tail.begin(),tail.end());
    }
    candidate.initialized=true;state=std::move(candidate);return Result::Okay;
}
Result UpdateFusionAdventureScheduler273(FusionAdventureScheduler273State& state,
    const FusionAdventureScheduler273Input& input,const FusionAdventureScheduler273Callbacks& cb,
    FusionAdventureScheduler273Events& events)
{
    events={};
    if(!input.host)return Result::NotHost;
    if(state.failed)return Result::Failed;
    if(!Valid(state)||!std::isfinite(input.deltaSeconds)||input.deltaSeconds<0)
        return Result::Invalid;
    if(input.paused||input.over)return Result::Okay;
    Clock next{state.wave,state.countdown,state.hold,state.healthElapsed,state.hugeElapsed,
        state.currentHealth,state.latestWaveHealth,state.hugeWarning};
    const float dt=input.deltaSeconds;
    next.healthElapsed+=dt;
    if(!std::isfinite(next.healthElapsed))return Fail(state,Result::Invalid);
    if(next.healthElapsed>1.f)
    {
        float health=0;
        if(!cb.health||!cb.health(cb.user,health)||!std::isfinite(health))return Fail(state,Result::HealthFailed);
        next.currentHealth=health;next.healthElapsed=0;events.healthSampled=true;
    }
    // Source LevelUpdate samples health before these NewZombieUpdate gates.
    if(input.tutorial||input.iceDoom||!input.allowSummon||input.boss||input.scaryPot||next.wave==state.maximumWaves)
    {Commit(state,next);return Result::Okay;}
    next.countdown-=dt;next.hold+=dt;
    if(!std::isfinite(next.countdown)||!std::isfinite(next.hold))return Fail(state,Result::Invalid);
    float threshold=0;
    if(!Real(cb.random,3.5f,4.f,threshold))return Fail(state,Result::RandomFailed);
    if(next.currentHealth<next.latestWaveHealth/threshold&&next.wave>0&&next.hold>4.2f)
    {
        float jitter=0;
        if(!Real(cb.random,.95f,1.05f,jitter))return Fail(state,Result::RandomFailed);
        const volatile float shortened=jitter*1.8f;
        next.countdown=(std::min)(next.countdown,float(shortened));next.hold=0;
    }
    if(next.countdown>=0){Commit(state,next);return Result::Okay;}
    const int wave=next.wave+1;
    if(wave%10==0||wave==state.maximumWaves)
    {
        if(!next.hugeWarning){next.hugeWarning=true;events.hugeWarning=true;}
        next.hugeElapsed+=dt;
        if(!std::isfinite(next.hugeElapsed))return Fail(state,Result::Invalid);
        if(next.hugeElapsed<=5.f){Commit(state,next);return Result::Okay;}
        next.hugeElapsed=0;
    }
    const auto& kinds=state.waves[size_t(wave-1)];
    if(kinds.empty()||!cb.canSpawn||!cb.canSpawn(cb.user,kinds))return Fail(state,Result::NotReady);
    std::vector<FusionAdventureScheduler273Spawn> batch;batch.reserve(kinds.size());
    uint8_t used=0;
    for(size_t index=0;index<kinds.size();++index)
    {
        if(index&&index%size_t(state.rows)==0)used=0;
        int row=0;
        do {if(!Integer(cb.random,0,state.rows,row))return Fail(state,Result::RandomFailed);}
        while(used&(1U<<row));
        used|=uint8_t(1U<<row);batch.push_back({kinds[index],uint8_t(row)});
    }
    float initialHealth=0;
    if(!cb.spawn||!cb.spawn(cb.user,wave,batch,initialHealth)||!std::isfinite(initialHealth))
        return Fail(state,Result::SpawnFailed);
    next.wave=wave;next.hugeWarning=false;next.latestWaveHealth=initialHealth;
    next.currentHealth+=initialHealth;next.countdown=30.f;
    if(!std::isfinite(next.currentHealth))return Fail(state,Result::HealthFailed);
    events.firstWave=wave==1;events.finalWave=wave==state.maximumWaves;
    events.spawnedWave=wave;events.spawnedCount=uint32_t(batch.size());
    Commit(state,next);return Result::Okay;
}
bool FusionAdventureScheduler273TutorOver(FusionAdventureScheduler273State& state)
{
    if(state.failed||!Valid(state))return false;
    state.countdown=5.f;return true;
}
bool TrySumFusionAdventureHealth273(std::span<const FusionAdventureScheduler273Health> actors,float& sum)
{
    uint32_t total=0;
    for(const auto& actor:actors)if(!actor.mindControlled)
    {uint32_t bits=0;if(!ActorHealth(actor,bits))return false;total+=bits;}
    sum=SignedFloat(total);return true;
}
bool TrySumFusionAdventureBirthHealth273(std::span<const FusionAdventureScheduler273Health> actors,float& sum)
{
    float total=0;
    for(const auto& actor:actors)
    {uint32_t bits=0;if(!ActorHealth(actor,bits))return false;total+=SignedFloat(bits);}
    if(!std::isfinite(total))return false;
    sum=total;return true;
}
}
