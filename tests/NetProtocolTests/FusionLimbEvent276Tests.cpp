#include "FusionLimbEvent276Tests.h"
#include "../../Lawn/Net/NetCommon.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

namespace
{
using namespace LawnNet;
uint32_t FloatBits(float value)
{uint32_t bits=0;std::memcpy(&bits,&value,sizeof(bits));return bits;}
bool Same(const NetFusionLimbEvent276& a,const NetFusionLimbEvent276& b)
{
    return a.mEpoch==b.mEpoch&&a.mZombieId==b.mZombieId&&a.mZombieToken==b.mZombieToken&&
        a.mSourceKind==b.mSourceKind&&a.mHead==b.mHead&&a.mRow==b.mRow&&
        FloatBits(a.mX)==FloatBits(b.mX)&&FloatBits(a.mY)==FloatBits(b.mY)&&a.mRenderOrder==b.mRenderOrder;
}
NetFusionLimbEvent276 Golden()
{
    NetFusionLimbEvent276 e;e.mEpoch=0x01020304;e.mZombieId=0x0605;e.mZombieToken=0x0a090807;
    e.mSourceKind=3;e.mHead=true;e.mRow=4;e.mX=1.5f;e.mY=-2.25f;e.mRenderOrder=0x010203;return e;
}
// Deliberately bypass the encoder to test malformed but exactly-sized payloads.
NetBuffer Raw(const NetFusionLimbEvent276& e)
{
    NetBuffer b;b.WriteU32(e.mEpoch);b.WriteU16(e.mZombieId);b.WriteU32(e.mZombieToken);
    b.WriteU16(e.mSourceKind);b.WriteU8(e.mHead?1:0);b.WriteU8(e.mRow);
    b.WriteF32(e.mX);b.WriteF32(e.mY);b.WriteI32(e.mRenderOrder);return b;
}
}
int RunFusionLimbEvent276Tests(int& checks)
{
    using namespace LawnNet;int failures=0;
    const auto check=[&](bool okay,int line){++checks;if(!okay){++failures;std::printf("FusionLimbEvent276Tests:%d failed\n",line);}};
#define CHECK(x) check(bool(x),__LINE__)
    CHECK(NET_PROTOCOL_VERSION==63&&BOARDEV_PRESENTATION_FWOOSH==27&&BOARDEV_FUSION_LIMB276==28);
    CHECK(NET_FUSION_LIMB276_WIRE_BYTES==26);
    constexpr std::array<uint8_t,26> golden={
        0x04,0x03,0x02,0x01,0x05,0x06,0x07,0x08,0x09,0x0a,0x03,0x00,0x01,0x04,
        0x00,0x00,0xc0,0x3f,0x00,0x00,0x10,0xc0,0x03,0x02,0x01,0x00};
    const auto valid=Golden();NetBuffer encoded;
    CHECK(NetEncodeFusionLimbEvent276(valid,encoded));
    CHECK(encoded.Size()==golden.size()&&std::memcmp(encoded.Data(),golden.data(),golden.size())==0);
    auto decoded=NetFusionLimbEvent276{};
    CHECK(NetDecodeFusionLimbEvent276(encoded,decoded));
    CHECK(Same(valid,decoded)&&encoded.Remaining()==0);

    // Eight genuine arms and Pole's one genuine falling head, all semantic
    // fields populated. The protocol never carries a resource ID/path.
    constexpr std::array<uint16_t,8> kinds={3,21,22,23,100,106,107,108};
    for(size_t i=0;i<kinds.size();++i)for(int part=0;part<2;++part)
    {
        auto e=valid;e.mSourceKind=kinds[i];e.mHead=part!=0;e.mRow=uint8_t(i%5);
        e.mEpoch+=uint32_t(i);e.mZombieId+=uint16_t(i);e.mZombieToken+=uint32_t(i);
        e.mX=float(i)*17.5f;e.mY=-float(i)*23.25f;e.mRenderOrder=int32_t(i*10000);
        const bool supported=!e.mHead||e.mSourceKind==3;
        CHECK(NetFusionLimbEvent276Valid(e)==supported);
        NetBuffer wire;CHECK(NetEncodeFusionLimbEvent276(e,wire)==supported);
        if(supported)
        {
            CHECK(wire.Size()==26);auto out=valid;
            CHECK(NetDecodeFusionLimbEvent276(wire,out));CHECK(Same(out,e)&&wire.Remaining()==0);
        }
        else CHECK(wire.Size()==0);
    }
    // Inclusive coordinate/render boundaries, opaque full-width identities,
    // all source rows, and bit-preserving negative zero.
    for(int row=0;row<5;++row)for(float x:{-4096.f,-0.f,0.f,4096.f})for(float y:{-4096.f,-0.f,0.f,4096.f})
    {
        auto e=valid;e.mEpoch=UINT32_MAX;e.mZombieId=UINT16_MAX;e.mZombieToken=UINT32_MAX;
        e.mRow=uint8_t(row);e.mX=x;e.mY=y;e.mRenderOrder=row%2?0:1000000;NetBuffer wire;
        CHECK(NetFusionLimbEvent276Valid(e));CHECK(NetEncodeFusionLimbEvent276(e,wire));
        auto out=valid;CHECK(NetDecodeFusionLimbEvent276(wire,out));CHECK(Same(out,e)&&wire.Remaining()==0);
    }
    // Valid payload appends, excludes event type, and respects a nonzero read
    // cursor: a receiver may already have consumed the Board event byte.
    NetBuffer prefixed;prefixed.WriteU8(0xaa);prefixed.WriteU8(BOARDEV_FUSION_LIMB276);prefixed.mReadPos=1;
    CHECK(NetEncodeFusionLimbEvent276(valid,prefixed));
    CHECK(prefixed.Size()==28&&prefixed.mReadPos==1&&prefixed.Data()[0]==0xaa&&prefixed.Data()[1]==28);
    uint8_t type=0;CHECK(prefixed.ReadU8(&type)&&type==BOARDEV_FUSION_LIMB276);
    CHECK(NetDecodeFusionLimbEvent276(prefixed,decoded));CHECK(Same(valid,decoded)&&prefixed.mReadPos==28);

    std::vector<NetFusionLimbEvent276> invalid;
    auto bad=valid;bad.mEpoch=0;invalid.push_back(bad);
    bad=valid;bad.mZombieId=0;invalid.push_back(bad);
    bad=valid;bad.mZombieToken=0;invalid.push_back(bad);
    for(uint16_t kind:{uint16_t(0),uint16_t(1),uint16_t(2),uint16_t(4),uint16_t(8),uint16_t(15),uint16_t(104),uint16_t(65535)})
    {bad=valid;bad.mHead=false;bad.mSourceKind=kind;invalid.push_back(bad);}
    for(uint16_t kind:kinds)if(kind!=3){bad=valid;bad.mSourceKind=kind;invalid.push_back(bad);}
    for(uint8_t row:{uint8_t(5),uint8_t(255)}){bad=valid;bad.mRow=row;invalid.push_back(bad);}
    for(float value:{-4096.5f,4096.5f,std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})
    {bad=valid;bad.mX=value;invalid.push_back(bad);bad=valid;bad.mY=value;invalid.push_back(bad);}
    for(int32_t order:{int32_t(-1),int32_t(1000001),(std::numeric_limits<int32_t>::min)(),(std::numeric_limits<int32_t>::max)()})
    {bad=valid;bad.mRenderOrder=order;invalid.push_back(bad);}
    for(const auto& e:invalid)
    {
        CHECK(!NetFusionLimbEvent276Valid(e));
        NetBuffer target;target.WriteU8(0x91);target.WriteU8(0x82);target.mReadPos=1;const auto saved=target.mData;
        CHECK(!NetEncodeFusionLimbEvent276(e,target));CHECK(target.mData==saved&&target.mReadPos==1);
        auto wire=Raw(e);wire.mData.insert(wire.mData.begin(),0x77);wire.mReadPos=1;const auto input=wire.mData;auto out=valid;
        CHECK(!NetDecodeFusionLimbEvent276(wire,out));CHECK(Same(out,valid)&&wire.mReadPos==1&&wire.mData==input);
    }
    // Noncanonical bool bytes must not become true. Failure is atomic even
    // after every other field has been successfully read.
    for(uint8_t head:{uint8_t(2),uint8_t(127),uint8_t(255)})
    {
        auto wire=Raw(valid);wire.mData[12]=head;auto out=valid;
        CHECK(!NetDecodeFusionLimbEvent276(wire,out));CHECK(Same(out,valid)&&wire.mReadPos==0);
    }
    for(size_t length=0;length<26;++length)
    {
        auto wire=Raw(valid);wire.mData.resize(length);wire.mData.insert(wire.mData.begin(),{0xa1,0xb2,0xc3});wire.mReadPos=3;
        const auto input=wire.mData;auto out=valid;
        CHECK(!NetDecodeFusionLimbEvent276(wire,out));CHECK(wire.mReadPos==3&&Same(out,valid)&&wire.mData==input);
    }
    for(size_t extra=1;extra<=4;++extra)
    {
        auto wire=Raw(valid);wire.mData.resize(26+extra,0x5a);auto out=valid;
        CHECK(!NetDecodeFusionLimbEvent276(wire,out));CHECK(wire.mReadPos==0&&Same(out,valid));
    }
    auto invalidCursor=Raw(valid);invalidCursor.mReadPos=invalidCursor.Size()+1;auto out=valid;
    CHECK(!NetDecodeFusionLimbEvent276(invalidCursor,out));CHECK(invalidCursor.mReadPos==27&&Same(out,valid));
#undef CHECK
    return failures;
}
