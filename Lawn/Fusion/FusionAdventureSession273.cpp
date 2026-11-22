#include "FusionAdventureSession273.h"
#include "FusionCampaign272Rules.h"
#include "../Net/NetCommon.h"
#include <cmath>

namespace LawnFusion
{
bool FusionAdventureSessionValid273(const FusionAdventureSession273& s)
{
    if(!FusionAdventureProfileValid273(s.profile)||!FusionAdventureRunValid273(s.run)||
        s.sourceDifficulty>5||s.coopDifficulty<1||s.coopDifficulty>10||!std::isfinite(s.countdown)||!std::isfinite(s.hugeElapsed)||
        std::abs(s.countdown)>3600||s.hugeElapsed<0||s.hugeElapsed>3600)return false;
    if(!s.enabled)return !s.run.level&&!s.wave&&!s.maximumWaves&&s.countdown==0&&s.hugeElapsed==0&&
        !s.hugeWarning&&!s.schedulerFailed&&s.sourceDifficulty==2&&s.coopDifficulty==1&&
        !s.profile.completedLevels&&s.profile.cycle==1&&s.profile.frontierLevel==1;
    const auto& r=s.run;const auto& p=s.profile;
    if(!r.rewardCollected)
    {
        if(r.cycle!=p.cycle||!CanEnterFusionAdventureLevel273(p,r.level)||
            r.replay!=(r.level!=p.frontierLevel))return false;
    }
    else
    {
        if(!FusionAdventureLevelCompleted273(p,r.level))return false;
        if(r.replay)
        {
            if(r.cycle!=p.cycle||r.level==p.frontierLevel)return false;
        }
        else if(r.level==kFusionAdventureLevels273)
        {
            if(p.frontierLevel!=1||p.cycle!=r.cycle+1)return false;
        }
        else if(p.cycle!=r.cycle||p.frontierLevel!=r.level+1)return false;
    }
    int waves=0;
    return s.run.level&&TryGetFusionCoopAdventureWaves272(s.run.level,int(s.run.cycle),s.coopDifficulty,waves)&&
        s.maximumWaves==waves&&s.wave<=s.maximumWaves&&
        (!s.run.over||s.wave==s.maximumWaves)&&
        (!s.hugeWarning||(s.wave<s.maximumWaves&&((s.wave+1)%10==0||s.wave+1==s.maximumWaves)));
}
bool EncodeFusionAdventureSession273(const FusionAdventureSession273& s,LawnNet::NetBuffer& out)
{
    if(!FusionAdventureSessionValid273(s))return false;
    LawnNet::NetBuffer b;b.WriteU8(s.enabled?1:0);
    if(s.enabled)
    {
        b.WriteU8(uint8_t(s.sourceDifficulty|((s.coopDifficulty-1)<<3)));
        b.WriteU32(uint32_t(s.profile.completedLevels));b.WriteU32(uint32_t(s.profile.completedLevels>>32));
        b.WriteU32(s.profile.cycle);b.WriteU16(s.profile.frontierLevel);
        const auto& r=s.run;
        b.WriteU32(r.cycle);b.WriteU16(r.level);
        b.WriteU8(r.tutorial.active?1:0);b.WriteU8(r.tutorial.stage);b.WriteU8(uint8_t(r.tutorial.message));
        b.WriteU16(r.tutorial.timerTicks);b.WriteU16(r.tutorial.messageTicks);
        b.WriteU16(r.trophyNetId);b.WriteU32(r.trophyToken);b.WriteU16(r.rewardTicks);
        b.WriteF32(r.trophySpawnX);b.WriteF32(r.trophySpawnY);
        b.WriteU8((r.replay?1:0)|(r.over?2:0)|(r.rewardCollected?4:0)|
            (r.transitionReady?8:0)|(r.transitionTaken?16:0));
        b.WriteU16(s.wave);b.WriteU16(s.maximumWaves);b.WriteF32(s.countdown);b.WriteF32(s.hugeElapsed);
        b.WriteU8((s.hugeWarning?1:0)|(s.schedulerFailed?2:0));
    }
    out.WriteBytes(b.Data(),b.Size());return true;
}
bool DecodeFusionAdventureSession273(LawnNet::NetBuffer& in,FusionAdventureSession273& out)
{
    auto b=in;FusionAdventureSession273 s;uint8_t enabled=0;
    if(!b.ReadU8(&enabled)||enabled>1)return false;
    s.enabled=enabled!=0;
    if(s.enabled)
    {
        uint32_t low=0,high=0;uint8_t active=0,message=0,runFlags=0,flags=0;
        auto& r=s.run;
        if(!b.ReadU8(&s.sourceDifficulty)||!b.ReadU32(&low)||!b.ReadU32(&high)||
            !b.ReadU32(&s.profile.cycle)||!b.ReadU16(&s.profile.frontierLevel)||
            !b.ReadU32(&r.cycle)||!b.ReadU16(&r.level)||!b.ReadU8(&active)||active>1||
            !b.ReadU8(&r.tutorial.stage)||!b.ReadU8(&message)||
            !b.ReadU16(&r.tutorial.timerTicks)||!b.ReadU16(&r.tutorial.messageTicks)||
            !b.ReadU16(&r.trophyNetId)||!b.ReadU32(&r.trophyToken)||!b.ReadU16(&r.rewardTicks)||
            !b.ReadF32(&r.trophySpawnX)||!b.ReadF32(&r.trophySpawnY)||!b.ReadU8(&runFlags)||(runFlags&~31U)||
            !b.ReadU16(&s.wave)||!b.ReadU16(&s.maximumWaves)||!b.ReadF32(&s.countdown)||
            !b.ReadF32(&s.hugeElapsed)||!b.ReadU8(&flags)||(flags&~3U))return false;
        s.coopDifficulty=uint8_t((s.sourceDifficulty>>3)+1);s.sourceDifficulty&=7;
        s.profile.completedLevels=uint64_t(low)|(uint64_t(high)<<32);
        r.tutorial.active=active!=0;r.tutorial.message=FusionAdventureMessage273(message);
        r.replay=(runFlags&1)!=0;r.over=(runFlags&2)!=0;r.rewardCollected=(runFlags&4)!=0;
        r.transitionReady=(runFlags&8)!=0;r.transitionTaken=(runFlags&16)!=0;
        s.hugeWarning=(flags&1)!=0;s.schedulerFailed=(flags&2)!=0;
    }
    if(!FusionAdventureSessionValid273(s))return false;
    out=s;in.mReadPos=b.mReadPos;return true;
}
}
