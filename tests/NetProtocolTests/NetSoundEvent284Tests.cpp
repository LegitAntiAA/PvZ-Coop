#include "../../Lawn/Net/NetCommon.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <limits>

namespace
{
uint32_t SoundFloatBits284(float value)
{uint32_t bits=0;std::memcpy(&bits,&value,sizeof(bits));return bits;}
float SoundFloatFromBits284(uint32_t bits)
{float value=0;std::memcpy(&value,&bits,sizeof(value));return value;}
}
int RunNetSoundEvent284Tests(int& checks)
{
    using namespace LawnNet;int failures=0;
    const auto check=[&](bool okay,int line)
    {++checks;if(!okay){++failures;std::printf("NetSoundEvent284Tests:%d failed\n",line);}};
#define CHECK(x) check(bool(x),__LINE__)
    // Exact payloads exclude Board event type. The source .5 playback volume
    // is appended as little-endian F32; existing two-byte events imply1.
    constexpr std::array<uint8_t,2> legacy={0x34,0x12};
    constexpr std::array<uint8_t,6> custom={0x34,0x12,0x00,0x00,0x00,0x3f};
    uint16_t sound=0xffff;float volume=-17.25f;NetBuffer wire;
    wire.WriteBytes(legacy.data(),legacy.size());const auto legacyInput=wire.mData;
    CHECK(NetDecodeSoundEvent284(wire,sound,volume));
    CHECK(sound==0x1234&&SoundFloatBits284(volume)==0x3f800000U&&wire.mReadPos==2&&wire.mData==legacyInput);
    wire.Clear();wire.WriteBytes(custom.data(),custom.size());const auto customInput=wire.mData;
    CHECK(NetDecodeSoundEvent284(wire,sound,volume));
    CHECK(sound==0x1234&&SoundFloatBits284(volume)==0x3f000000U&&wire.mReadPos==6&&wire.mData==customInput);

    for(bool useCustom:{false,true})
    {
        NetBuffer framed;framed.WriteU8(0xaa);framed.WriteU8(BOARDEV_SOUND);
        if(useCustom)framed.WriteBytes(custom.data(),custom.size());else framed.WriteBytes(legacy.data(),legacy.size());
        framed.mReadPos=1;const auto input=framed.mData;sound=0xabc1;volume=-17.25f;
        // A receiver must consume the event byte first; its payload framing is exact.
        CHECK(!NetDecodeSoundEvent284(framed,sound,volume));
        CHECK(framed.mReadPos==1&&sound==0xabc1&&SoundFloatBits284(volume)==SoundFloatBits284(-17.25f)&&framed.mData==input);
        uint8_t type=0;CHECK(framed.ReadU8(&type)&&type==BOARDEV_SOUND);
        CHECK(NetDecodeSoundEvent284(framed,sound,volume));
        CHECK(sound==0x1234&&volume==(useCustom?.5f:1.f)&&framed.mReadPos==framed.Size()&&framed.mData==input);
    }
    // Decoder owns only wire framing, not the app's sample/resource registry.
    // Explicit custom1 is legal; both signs of zero are finite/in-range.
    for(uint16_t id:{uint16_t(0),uint16_t(65535)})for(float value:{0.f,-0.f,.25f,.5f,1.f})
    {
        NetBuffer b;b.WriteU8(0x77);b.WriteU16(id);b.WriteF32(value);b.mReadPos=1;const auto input=b.mData;
        sound=0xabc1;volume=-17.25f;CHECK(NetDecodeSoundEvent284(b,sound,volume));
        CHECK(sound==id&&SoundFloatBits284(volume)==SoundFloatBits284(value)&&b.mReadPos==7&&b.mData==input);
    }
    for(uint16_t id:{uint16_t(0),uint16_t(65535)})
    {
        NetBuffer b;b.WriteU16(id);sound=0xabc1;volume=.25f;
        CHECK(NetDecodeSoundEvent284(b,sound,volume)&&sound==id&&volume==1&&b.mReadPos==2);
    }

    const auto rejected=[&](NetBuffer& b)
    {
        const auto cursor=b.mReadPos;const auto input=b.mData;
        uint16_t outputSound=0xabc1;float outputVolume=-17.25f;
        CHECK(!NetDecodeSoundEvent284(b,outputSound,outputVolume));
        CHECK(b.mReadPos==cursor&&b.mData==input);
        CHECK(outputSound==0xabc1&&SoundFloatBits284(outputVolume)==SoundFloatBits284(-17.25f));
    };
    // Length2 is intentionally a valid legacy packet even when it equals a
    // custom packet's prefix. Other truncated lengths cannot be accepted.
    for(size_t length:{size_t(0),size_t(1),size_t(3),size_t(4),size_t(5)})
    {
        NetBuffer b;b.WriteBytes(custom.data(),length);b.mData.insert(b.mData.begin(),{0xa1,0xb2,0xc3});b.mReadPos=3;
        rejected(b);
    }
    for(size_t extra=1;extra<=8;++extra)
    {
        NetBuffer b;b.WriteBytes(custom.data(),custom.size());b.mData.resize(custom.size()+extra,0x5a);rejected(b);
    }
    // Legacy+four volume bytes IS a custom6 payload, so only genuine other
    // lengths are malformed extras; do not weaken legacy compatibility.
    for(size_t extra:{size_t(1),size_t(2),size_t(3),size_t(5)})
    {
        NetBuffer b;b.WriteBytes(legacy.data(),legacy.size());b.mData.resize(legacy.size()+extra,0);rejected(b);
    }
    // Independently authored F32 bit patterns exercise late parse failure:
    // the preceding sample U16 must never leak into either output on failure.
    constexpr std::array<uint32_t,7> invalidVolumeBits={
        0x7fc00001U,0xffc00001U,0x7f800000U,0xff800000U,0xbf000000U,0x3f800001U,0x40000000U};
    for(uint32_t bits:invalidVolumeBits)
    {
        NetBuffer b;b.WriteU8(0x77);b.WriteU16(0x1234);b.WriteU32(bits);b.mReadPos=1;rejected(b);
    }
    // Failed parse must preserve pre-existing non-finite output bit-for-bit too.
    {
        NetBuffer b;b.WriteBytes(custom.data(),custom.size());b.mData.back()=0x7f;
        const auto cursor=b.mReadPos;const auto input=b.mData;sound=0xbeef;volume=SoundFloatFromBits284(0x7fc12345U);
        CHECK(!NetDecodeSoundEvent284(b,sound,volume));
        CHECK(b.mReadPos==cursor&&b.mData==input&&sound==0xbeef&&SoundFloatBits284(volume)==0x7fc12345U);
    }
    NetBuffer pastEnd;pastEnd.WriteBytes(legacy.data(),legacy.size());pastEnd.mReadPos=3;rejected(pastEnd);
    NetBuffer wrapped;wrapped.WriteBytes(custom.data(),custom.size());wrapped.mReadPos=(std::numeric_limits<size_t>::max)();rejected(wrapped);
#undef CHECK
    return failures;
}
