#include "../../Lawn/Fusion/FusionSourceDamage315Rules.h"
#include "../../Lawn/Net/NetSourceCharred315.h"
#include <cstdio>
#include <limits>
int RunFusionSourceDamage315Tests(int& checks)
{
    using namespace LawnFusion;using namespace LawnNet;int failed=0;
    const auto check=[&](bool v,int line){++checks;if(!v){++failed;std::printf("SourceDamage315:%d failed\n",line);}};
#define CHECK315(v) check(bool(v),__LINE__)
    CHECK315(SourceCharred315Direct(60,1100,60,2));
    CHECK315(!SourceCharred315Direct(60.25f,0,60,2));
    CHECK315(SourceCharred315Direct(50,20,100,4));
    CHECK315(!SourceCharred315Direct(50.01f,20,100,4));
    CHECK315(SourceCharred315Direct(30,10,100,5));
    CHECK315(!SourceCharred315Direct(30.01f,10,100,5));
    CHECK315(!SourceCharred315Direct(std::numeric_limits<float>::quiet_NaN(),0,60,2));
    CHECK315(!SourceType12Charred315(60,60));CHECK315(SourceType12Charred315(59.75f,60));
    int damage=999;CHECK315(TrySourceBaseDamage315(60,false,true,damage)&&damage==90);
    CHECK315(TrySourceBaseDamage315(60,true,true,damage)&&damage==60);
    CHECK315(TrySourceBaseDamage315(0,false,true,damage)&&damage==0);
    CHECK315(TrySourceBaseDamage315(60,false,true,damage,true,false)&&damage==9);
    CHECK315(TrySourceBaseDamage315(60,false,true,damage,true,true)&&damage==90);
    CHECK315(TrySourceBaseDamage315(60,true,true,damage,true,false)&&damage==60);
    CHECK315(TrySourceBaseDamage315(1,false,false,damage,true,false)&&damage==1);
    CHECK315(SourceCharred315Direct(500,1100,(std::numeric_limits<int>::max)(),2));
    CHECK315(!TrySourceBaseDamage315((std::numeric_limits<int>::max)(),false,true,damage));
    SourceCharred311Birth v;v.epoch=315;v.ownerNetID=7;v.sourceKind=24;v.ownerToken=0x10007;
    v.sourceRow=4;v.order=343000;v.shadowX=712.25f;v.shadowY=512.5f;v.rootFlip=true;
    NetBuffer b;CHECK315(NetEncodeSourceCharred315(v,b)&&b.Size()==31);
    SourceCharred311Birth decoded;CHECK315(NetDecodeSourceCharred315(b,decoded)&&decoded.rootFlip&&
        decoded.ownerToken==v.ownerToken&&decoded.shadowX==v.shadowX&&decoded.order==v.order);
    b.mReadPos=0;
    for(size_t n=0;n<31;++n)
    {
        NetBuffer shortPacket;shortPacket.mData.assign(b.mData.begin(),b.mData.begin()+n);
        decoded.epoch=777;CHECK315(!NetDecodeSourceCharred315(shortPacket,decoded)&&shortPacket.mReadPos==0&&decoded.epoch==777);
    }
    auto invalid=b;invalid.mData.push_back(0);CHECK315(!NetDecodeSourceCharred315(invalid,decoded)&&invalid.mReadPos==0);
    for(size_t offset:{size_t(12),size_t(13),size_t(22)})
    {auto bad=b;bad.mData[offset]=255;decoded.epoch=777;CHECK315(!NetDecodeSourceCharred315(bad,decoded)&&bad.mReadPos==0&&decoded.epoch==777);}
    auto badSerial=b;badSerial.mData[27]=1;
    CHECK315(!NetDecodeSourceCharred315(badSerial,decoded)&&badSerial.mReadPos==0);
    auto splash=v;splash.variant=3;splash.rootFlip=false;splash.order=347000;splash.eventSerial=3;
    NetBuffer splashBytes;CHECK315(NetEncodeSourceCharred315(splash,splashBytes)&&splashBytes.Size()==31);
    CHECK315(NetDecodeSourceCharred315(splashBytes,decoded)&&decoded.variant==3&&decoded.eventSerial==3);
    auto garg=v;garg.variant=2;garg.sourceKind=35;
    NetBuffer gargBytes;CHECK315(NetEncodeSourceCharred315(garg,gargBytes));
    CHECK315(NetDecodeSourceCharred315(gargBytes,decoded)&&decoded.variant==2&&!decoded.eventSerial);
    v.shadowX=std::numeric_limits<float>::infinity();NetBuffer unchanged;unchanged.WriteU8(99);
    CHECK315(!NetEncodeSourceCharred315(v,unchanged)&&unchanged.Size()==1&&unchanged.mData[0]==99);
#undef CHECK315
    return failed;
}
