#include "FusionAdventureSession273Tests.h"
#include "../../Lawn/Fusion/FusionAdventureSession273.h"
#include "../../Lawn/Fusion/FusionCampaign272Rules.h"
#include "../../Lawn/Net/NetCommon.h"
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

namespace
{
using namespace LawnFusion;
using LawnNet::NetBuffer;
FusionAdventureSession273TestResult result;
void Check(bool ok,int line)
{++result.mChecks;if(!ok){++result.mFailures;std::printf("    AdventureSession273 failed line=%d\n",line);}}
#define CHECK(value) Check(!!(value),__LINE__)

// Fixed field offsets of the59-byte enabled DTO, independent of native struct
// layout/padding. These probes represent malformed input, not another codec.
constexpr size_t Difficulty=1,CompletedHigh=6,ProfileCycle=10,Frontier=14,RunCycle=16,Level=20,
    TutorialActive=22,TutorialStage=23,TutorialMessage=24,TutorialTimer=25,MessageTicks=27,
    TrophyId=29,TrophyToken=31,RewardTicks=35,SpawnX=37,SpawnY=41,RunFlags=45,
    Wave=46,MaximumWaves=48,Countdown=50,HugeElapsed=54,Flags=58;

FusionAdventureSession273 Tutorial()
{
    FusionAdventureSession273 s;s.enabled=true;s.maximumWaves=10;s.countdown=15;
    CHECK(BeginFusionAdventureRun273(s.profile,1,false,s.run));
    FusionAdventureTutorialEffects273 effects;
    CHECK(StepFusionAdventureTutorial273(s.run,1,false,false,false,effects));return s;
}
FusionAdventureSession273 Reward(bool collected)
{
    FusionAdventureSession273 s;s.enabled=true;s.profile.completedLevels=1;s.profile.frontierLevel=2;
    CHECK(BeginFusionAdventureRun273(s.profile,2,false,s.run));
    s.wave=s.maximumWaves=10;s.countdown=-.375f;
    CHECK(TryArmFusionAdventureReward273(s.run,10,10,0,false));
    s.run.trophySpawnX=410.25f;s.run.trophySpawnY=260.5f;
    CHECK(BindFusionAdventureTrophy273(s.run,911,0x1234abcd));
    if(collected)CHECK(TryCollectFusionAdventureReward273(s.profile,s.run,911,0x1234abcd));
    return s;
}
NetBuffer Encoded(const FusionAdventureSession273& s)
{NetBuffer out;CHECK(EncodeFusionAdventureSession273(s,out));return out;}
template<class T>void Put(NetBuffer& b,size_t offset,T value)
{CHECK(offset+sizeof(T)<=b.Size());if(offset+sizeof(T)<=b.Size())std::memcpy(b.mData.data()+offset,&value,sizeof(T));}
void Rejected(NetBuffer malformed,const FusionAdventureSession273& sentinel)
{
    NetBuffer framed;framed.WriteU8(0xa1);framed.WriteU16(0xb2c3);
    if(malformed.Size())framed.WriteBytes(malformed.Data(),malformed.Size());framed.mReadPos=3;
    const auto originalBytes=framed.mData;const auto originalCursor=framed.mReadPos;
    const auto before=Encoded(sentinel);auto destination=sentinel;
    CHECK(!DecodeFusionAdventureSession273(framed,destination));
    CHECK(framed.mReadPos==originalCursor&&framed.mData==originalBytes);
    const auto after=Encoded(destination);CHECK(after.mData==before.mData);
}
void RoundTrip(const FusionAdventureSession273& source,const FusionAdventureSession273& sentinel)
{
    CHECK(FusionAdventureSessionValid273(source));const auto wire=Encoded(source);
    NetBuffer framed;framed.WriteU8(0x17);framed.WriteU16(0x2839);framed.WriteBytes(wire.Data(),wire.Size());
    framed.WriteU32(0x53647586);framed.mReadPos=3;
    auto decoded=sentinel;CHECK(DecodeFusionAdventureSession273(framed,decoded));
    CHECK(framed.mReadPos==3+wire.Size()&&framed.Remaining()==4);
    CHECK(Encoded(decoded).mData==wire.mData);
    uint32_t trailing=0;CHECK(framed.ReadU32(&trailing)&&trailing==0x53647586);
    // Every missing suffix byte must reject without cursor/destination writes.
    for(size_t length=0;length<wire.Size();++length)
    {
        NetBuffer cut;cut.mData.assign(wire.mData.begin(),wire.mData.begin()+length);Rejected(cut,sentinel);
    }
}
}
FusionAdventureSession273TestResult RunFusionAdventureSession273Tests()
{
    result={};std::printf("[ RUN      ] Fusion.AdventureSession273\n");
    const auto tutorial=Tutorial(),waiting=Reward(false),collected=Reward(true);
    for(int d=1;d<=10;++d)
    {
        auto session=tutorial;session.coopDifficulty=uint8_t(d);int waves=0;
        CHECK(TryGetFusionCoopAdventureWaves272(1,1,d,waves));session.maximumWaves=uint16_t(waves);
        auto bytes=Encoded(session);CHECK(bytes.Size()==59);FusionAdventureSession273 restored;
        CHECK(DecodeFusionAdventureSession273(bytes,restored)&&restored.coopDifficulty==d&&restored.sourceDifficulty==2&&restored.maximumWaves==waves);
        auto invalid=session;invalid.coopDifficulty=11;CHECK(!FusionAdventureSessionValid273(invalid));
    }

    FusionAdventureSession273 disabled;
    CHECK(Encoded(disabled).mData==std::vector<uint8_t>{0});
    CHECK(Encoded(tutorial).Size()==59);
    RoundTrip(disabled,collected);RoundTrip(tutorial,collected);
    RoundTrip(waiting,tutorial);RoundTrip(collected,tutorial);
    NetBuffer decodedWire=Encoded(waiting);FusionAdventureSession273 decoded;
    CHECK(DecodeFusionAdventureSession273(decodedWire,decoded)&&decoded.countdown==-.375f);
    CHECK(decoded.run.over&&!decoded.run.rewardCollected&&decoded.profile.frontierLevel==2);
    decodedWire=Encoded(collected);CHECK(DecodeFusionAdventureSession273(decodedWire,decoded));
    CHECK(decoded.run.rewardCollected&&decoded.run.rewardTicks==425&&decoded.profile.frontierLevel==3&&decoded.profile.completedLevels==3);
    // A legal negative scheduler countdown must remain negative; it is not a
    // malformed value to clamp to zero during snapshot adoption.
    auto negative=tutorial;negative.run.tutorial.active=false;negative.run.tutorial={};
    negative.countdown=-12.125f;RoundTrip(negative,collected);
    for(float countdown:{-3600.0f,3600.0f})
    {auto edge=negative;edge.countdown=countdown;RoundTrip(edge,collected);}
    auto highBit=waiting;highBit.profile.completedLevels|=uint64_t{1}<<44;RoundTrip(highBit,tutorial);
    auto laterCycle=negative;laterCycle.profile.cycle=laterCycle.run.cycle=2;laterCycle.maximumWaves=20;
    RoundTrip(laterCycle,collected);
    auto ready=collected;CHECK(StepFusionAdventureReward273(ready.run,425));RoundTrip(ready,tutorial);
    FusionAdventureTransition273 transition;CHECK(TakeFusionAdventureTransition273(ready.profile,ready.run,transition));
    RoundTrip(ready,tutorial);
    auto replay=waiting;replay.profile.frontierLevel=3;replay.profile.completedLevels=3;replay.run.replay=true;
    CHECK(TryCollectFusionAdventureReward273(replay.profile,replay.run,replay.run.trophyNetId,replay.run.trophyToken));
    RoundTrip(replay,tutorial);

    const auto good=Encoded(tutorial),award=Encoded(waiting);
    for(uint8_t value:{uint8_t(2),uint8_t(255)})
    {
        auto bad=good;Put(bad,0,value);Rejected(bad,collected);
        bad=good;Put(bad,TutorialActive,value);Rejected(bad,collected);
    }
    for(uint8_t bit:{uint8_t(32),uint8_t(64),uint8_t(128)})
    {auto bad=good;Put(bad,RunFlags,bit);Rejected(bad,collected);}
    for(uint8_t bit:{uint8_t(4),uint8_t(8),uint8_t(128)})
    {auto bad=good;Put(bad,Flags,bit);Rejected(bad,collected);}
    auto bad=good;Put(bad,Difficulty,uint8_t(6));Rejected(bad,collected);
    bad=good;Put(bad,CompletedHigh,uint32_t(1U<<13));Rejected(bad,collected); // Completion level46, outside45 bits.
    bad=good;Put(bad,ProfileCycle,uint32_t(0));Rejected(bad,collected);
    bad=good;Put(bad,ProfileCycle,uint32_t(0x80000000U));Rejected(bad,collected);
    bad=good;Put(bad,Frontier,uint16_t(46));Rejected(bad,collected);
    bad=good;Put(bad,Level,uint16_t(46));Rejected(bad,collected);
    bad=good;Put(bad,TutorialStage,uint8_t(7));Rejected(bad,collected);
    bad=good;Put(bad,TutorialMessage,uint8_t(255));Rejected(bad,collected);
    bad=good;Put(bad,TutorialTimer,uint16_t(301));Rejected(bad,collected);
    bad=good;Put(bad,MessageTicks,uint16_t(301));Rejected(bad,collected);
    bad=award;Put(bad,TrophyId,uint16_t(0));Rejected(bad,collected);
    bad=award;Put(bad,TrophyToken,uint32_t(0));Rejected(bad,collected);
    bad=award;Put(bad,RewardTicks,uint16_t(426));Rejected(bad,collected);
    bad=good;Put(bad,Wave,uint16_t(11));Rejected(bad,collected);
    bad=good;Put(bad,MaximumWaves,uint16_t(9));Rejected(bad,collected);
    bad=award;Put(bad,Wave,uint16_t(9));Rejected(bad,collected); // over before final source wave is impossible.
    bad=good;Put(bad,Flags,uint8_t(1));Rejected(bad,collected); // Huge warning is only before each tenth wave.
    for(size_t field:{SpawnX,SpawnY,Countdown,HugeElapsed})
    {
        for(float value:{(std::numeric_limits<float>::quiet_NaN)(),(std::numeric_limits<float>::infinity)(),
            -(std::numeric_limits<float>::infinity)()})
        {bad=good;Put(bad,field,value);Rejected(bad,collected);}
    }
    bad=award;Put(bad,SpawnX,8192.5f);Rejected(bad,collected);
    bad=award;Put(bad,SpawnY,-8192.5f);Rejected(bad,collected);
    bad=good;Put(bad,Countdown,-3600.5f);Rejected(bad,collected);
    bad=good;Put(bad,Countdown,3600.5f);Rejected(bad,collected);
    bad=good;Put(bad,HugeElapsed,-.01f);Rejected(bad,collected);
    bad=good;Put(bad,HugeElapsed,3600.5f);Rejected(bad,collected);

    // Cross-field source identity must also be coherent, not merely valid in
    // isolation. Otherwise an unopened level or another cycle can be adopted.
    auto incoherent=tutorial;incoherent.run.tutorial={};incoherent.run.level=2;
    CHECK(!FusionAdventureSessionValid273(incoherent));
    bad=good;Put(bad,TutorialActive,uint8_t(0));Put(bad,TutorialStage,uint8_t(0));Put(bad,TutorialMessage,uint8_t(0));
    Put(bad,TutorialTimer,uint16_t(0));Put(bad,MessageTicks,uint16_t(0));Put(bad,Level,uint16_t(2));Rejected(bad,collected);
    bad=good;Put(bad,RunCycle,uint32_t(2));Put(bad,TutorialActive,uint8_t(0));Put(bad,TutorialStage,uint8_t(0));
    Put(bad,TutorialMessage,uint8_t(0));Put(bad,TutorialTimer,uint16_t(0));Put(bad,MessageTicks,uint16_t(0));
    Put(bad,MaximumWaves,uint16_t(20));Rejected(bad,collected);
    bad=good;Put(bad,RunFlags,uint8_t(1));Rejected(bad,collected); // frontier level cannot be tagged as replay.
    // Encoding rejection is transactional too: existing caller bytes survive.
    NetBuffer unchanged;unchanged.WriteU32(0x12345678);unchanged.mReadPos=1;const auto old=unchanged.mData;
    auto invalid=disabled;invalid.profile.completedLevels=1;
    CHECK(!EncodeFusionAdventureSession273(invalid,unchanged)&&unchanged.mReadPos==1&&unchanged.mData==old);
    std::printf("[ %s ] Fusion.AdventureSession273 checks=%d failures=%d\n",result.mFailures?"FAILED":"OK",result.mChecks,result.mFailures);
    return result;
}
