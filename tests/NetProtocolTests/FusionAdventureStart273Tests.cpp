#include "FusionAdventureStart273Tests.h"
#include "../../Lawn/Fusion/FusionAdventureSession273.h"
#include "../../Lawn/Fusion/FusionCampaign272Rules.h"
#include "../../Lawn/Net/NetCommon.h"
#include "../../ConstEnums.h"
#include <cstdio>
#include <cstring>
#include <vector>

namespace
{
using namespace LawnFusion;
using namespace LawnNet;
FusionAdventureStart273TestResult result;
void Check(bool ok,int line)
{++result.mChecks;if(!ok){++result.mFailures;std::printf("    AdventureStart273 failed line=%d\n",line);}}
#define CHECK(value) Check(!!(value),__LINE__)
constexpr int modeCount=int(GameMode::NUM_GAME_MODES),tower=int(GameMode::GAMEMODE_COOP_TOWER_DEFENSE);
constexpr size_t Content=0,Game=1,NativeLevel=2,Players=6,PathLength=11,Epoch=12,Source=16;
NetStartGameState Classic(bool td=false)
{
    NetStartGameState s;s.mGameMode=uint8_t(td?tower:int(GameMode::GAMEMODE_ADVENTURE));
    s.mLevel=37;s.mPlayerCount=2;s.mBoardSeed=0x12345678;
    if(td)s.mTDPath={2,8,2,7};return s;
}
NetStartGameState SourceStart(int level,int frontier,uint64_t completed)
{
    auto s=Classic();s.mContentMode=ContentMode::Fusion;s.mContentEpoch=0x10203040;
    auto& source=s.mSourceAdventure;source.enabled=true;source.profile.frontierLevel=uint16_t(frontier);
    source.profile.completedLevels=completed;
    CHECK(BeginFusionAdventureRun273(source.profile,level,false,source.run));
    int waves=0;CHECK(TryGetFusionSourceAdventureWaves272(level,1,waves));source.maximumWaves=uint16_t(waves);
    source.countdown=15;FusionSourceNativeProjection272 projection{};
    CHECK(TryProjectFusionSourceAdventure272(level,projection));s.mLevel=projection.layoutLevel;return s;
}
NetBuffer Encoded(const NetStartGameState& s)
{NetBuffer b;CHECK(NetEncodeStartGame(s,b,modeCount,tower));return b;}
template<class T>void Put(NetBuffer& b,size_t offset,T value)
{CHECK(offset+sizeof(T)<=b.Size());if(offset+sizeof(T)<=b.Size())std::memcpy(b.mData.data()+offset,&value,sizeof(T));}
void Rejected(NetBuffer malformed,const NetStartGameState& sentinel)
{
    NetBuffer input;input.WriteU8(0xa1);input.WriteU32(0x98765432);
    if(malformed.Size())input.WriteBytes(malformed.Data(),malformed.Size());input.mReadPos=5;
    const auto original=input.mData;const auto before=Encoded(sentinel);auto destination=sentinel;
    CHECK(!NetDecodeStartGame(input,destination,modeCount,tower));
    CHECK(input.mReadPos==5&&input.mData==original);
    CHECK(Encoded(destination).mData==before.mData);
}
void InvalidEncode(const NetStartGameState& bad)
{
    NetBuffer out;out.WriteU32(0x44443333);out.mReadPos=2;const auto original=out.mData;
    CHECK(!NetEncodeStartGame(bad,out,modeCount,tower));CHECK(out.mData==original&&out.mReadPos==2);
}
void RoundTrip(const NetStartGameState& source,const NetStartGameState& sentinel)
{
    const auto bytes=Encoded(source);NetBuffer input;input.WriteU8(0xa1);input.WriteU32(0x98765432);
    input.WriteBytes(bytes.Data(),bytes.Size());input.mReadPos=5;auto decoded=sentinel;
    CHECK(NetDecodeStartGame(input,decoded,modeCount,tower));CHECK(input.Remaining()==0&&input.mReadPos==input.Size());
    CHECK(Encoded(decoded).mData==bytes.mData);
    CHECK(decoded.mContentMode==source.mContentMode&&decoded.mContentEpoch==source.mContentEpoch&&
        decoded.mGameMode==source.mGameMode&&decoded.mLevel==source.mLevel&&
        decoded.mBoardSeed==source.mBoardSeed&&decoded.mPlayerCount==source.mPlayerCount&&decoded.mTDPath==source.mTDPath);
    for(size_t length=0;length<bytes.Size();++length)
    {NetBuffer cut;cut.mData.assign(bytes.mData.begin(),bytes.mData.begin()+length);Rejected(cut,sentinel);}
    auto trailing=bytes;trailing.WriteU8(0);Rejected(trailing,sentinel); // START_GAME owns the complete payload.
}
}
FusionAdventureStart273TestResult RunFusionAdventureStart273Tests()
{
    result={};std::printf("[ RUN      ] Fusion.AdventureStart273\n");
    const auto first=SourceStart(1,1,0),second=SourceStart(2,2,1),replay=SourceStart(2,3,3);
    const auto classic=Classic(),classicTD=Classic(true);
    RoundTrip(first,classic);RoundTrip(second,first);RoundTrip(replay,first);
    const auto firstWire=Encoded(first),secondWire=Encoded(second);
    CHECK(firstWire.Size()==75&&secondWire.Size()==75&&Encoded(replay).Size()==75);
    CHECK(first.mSourceAdventure.run.tutorial.active&&!second.mSourceAdventure.run.tutorial.active&&replay.mSourceAdventure.run.replay);
    CHECK(firstWire.mData[Source]==1&&firstWire.mData[Epoch]==0x40&&firstWire.mData[Epoch+3]==0x10);
    // The projection gap at source10 is intentional geometry reuse, not a
    // source-level renumbering. The descriptor keeps level10 but carries11.
    const auto night=SourceStart(10,10,511);CHECK(night.mLevel==11&&night.mSourceAdventure.run.level==10);
    RoundTrip(night,first);
    auto badState=night;badState.mLevel=10;InvalidEncode(badState);
    auto bad=Encoded(night);Put(bad,NativeLevel,int32_t(10));Rejected(bad,first);
    // Classic's known pre273 wire format stays12+path bytes, without epoch or
    // a source-session disabled marker. Compare golden bytes, not just sizes.
    const std::vector<uint8_t> classicGolden={0,0,37,0,0,0,2,0x78,0x56,0x34,0x12,0};
    CHECK(Encoded(classic).mData==classicGolden&&Encoded(classic).Size()==12);
    auto tdGolden=classicGolden;tdGolden[Game]=uint8_t(tower);tdGolden[PathLength]=4;
    tdGolden.insert(tdGolden.end(),{2,8,2,7});CHECK(Encoded(classicTD).mData==tdGolden);
    RoundTrip(classic,first);RoundTrip(classicTD,second);
    // The retired native50 descriptor remains serializable for internal smoke;
    // public legacy admission is separately gated by LawnApp.
    auto legacy=classic;legacy.mContentMode=ContentMode::Fusion;legacy.mContentEpoch=99;
    CHECK(Encoded(legacy).Size()==17&&Encoded(legacy).mData.back()==0);
    RoundTrip(legacy,second);
    auto legacyTD=Classic(true);legacyTD.mContentMode=ContentMode::Fusion;legacyTD.mContentEpoch=99;
    CHECK(Encoded(legacyTD).Size()==21);RoundTrip(legacyTD,first);

    badState=first;badState.mContentMode=ContentMode::Classic;badState.mContentEpoch=0;InvalidEncode(badState);
    bad=firstWire;Put(bad,Content,uint8_t(ContentMode::Classic));Rejected(bad,second);
    badState=first;badState.mGameMode=uint8_t(tower);badState.mTDPath={2,8,2,7};InvalidEncode(badState);
    bad=firstWire;Put(bad,Game,uint8_t(tower));Rejected(bad,second);
    badState=first;badState.mGameMode=uint8_t(GameMode::GAMEMODE_ADVENTURE+1);InvalidEncode(badState);
    bad=firstWire;Put(bad,Game,uint8_t(GameMode::GAMEMODE_ADVENTURE+1));Rejected(bad,second);
    badState=first;badState.mLevel=2;InvalidEncode(badState);
    bad=firstWire;Put(bad,NativeLevel,int32_t(2));Rejected(bad,second);
    badState=first;badState.mContentEpoch=0;InvalidEncode(badState);
    bad=firstWire;Put(bad,Epoch,uint32_t(0));Rejected(bad,second);
    bad=firstWire;Put(bad,Players,uint8_t(0));Rejected(bad,second);
    bad=firstWire;Put(bad,Players,uint8_t(NET_MAX_PLAYERS+1));Rejected(bad,second);
    bad=firstWire;Put(bad,PathLength,uint8_t(255));Rejected(bad,second);
    // Valid ongoing/reward Session DTOs are not fresh START_GAME descriptors.
    badState=second;badState.mSourceAdventure.wave=1;CHECK(FusionAdventureSessionValid273(badState.mSourceAdventure));InvalidEncode(badState);
    bad=secondWire;Put(bad,Source+46,uint16_t(1));Rejected(bad,first);
    badState=second;badState.mSourceAdventure.wave=badState.mSourceAdventure.maximumWaves;
    CHECK(TryArmFusionAdventureReward273(badState.mSourceAdventure.run,10,10,0,false));
    CHECK(FusionAdventureSessionValid273(badState.mSourceAdventure));InvalidEncode(badState);
    bad=secondWire;Put(bad,Source+45,uint8_t(2));Put(bad,Source+46,uint16_t(10));Rejected(bad,first);
    badState=second;badState.mSourceAdventure.schedulerFailed=true;CHECK(FusionAdventureSessionValid273(badState.mSourceAdventure));InvalidEncode(badState);
    bad=secondWire;Put(bad,Source+58,uint8_t(2));Rejected(bad,first);
    badState=second;badState.mSourceAdventure.wave=9;badState.mSourceAdventure.hugeWarning=true;
    CHECK(FusionAdventureSessionValid273(badState.mSourceAdventure));InvalidEncode(badState);
    bad=secondWire;Put(bad,Source+46,uint16_t(9));Put(bad,Source+58,uint8_t(1));Rejected(bad,first);
    bad=secondWire;Put(bad,Source,uint8_t(2));Rejected(bad,first);
    bad=secondWire;Put(bad,Source+48,uint16_t(20));Rejected(bad,first);
    std::printf("[ %s ] Fusion.AdventureStart273 checks=%d failures=%d\n",result.mFailures?"FAILED":"OK",result.mChecks,result.mFailures);
    return result;
}
