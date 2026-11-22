#pragma once
#include "NetCommon.h"
#include "../Fusion/FusionNativeCharred311.h"
namespace LawnNet
{
constexpr size_t NET_SOURCE_CHARRED315_BYTES=31;
inline bool NetEncodeSourceCharred315(const LawnFusion::SourceCharred311Birth& v,NetBuffer& out)
{
    if(!LawnFusion::SourceCharred311BirthValid(v))return false;
    NetBuffer b;b.WriteU32(v.epoch);b.WriteU16(v.ownerNetID);b.WriteU16(v.sourceKind);b.WriteU32(v.ownerToken);
    b.WriteU8(v.variant);b.WriteU8(v.sourceRow);b.WriteF32(v.shadowX);b.WriteF32(v.shadowY);
    b.WriteU8(v.rootFlip?1:0);b.WriteI32(v.order);b.WriteU32(v.eventSerial);
    if(b.Size()!=NET_SOURCE_CHARRED315_BYTES)return false;
    out.WriteBytes(b.Data(),b.Size());return true;
}
inline bool NetDecodeSourceCharred315(NetBuffer& b,LawnFusion::SourceCharred311Birth& out)
{
    if(b.mReadPos>b.Size()||b.Remaining()!=NET_SOURCE_CHARRED315_BYTES)return false;
    const size_t start=b.mReadPos;LawnFusion::SourceCharred311Birth v;uint8_t flip=0;
    if(!b.ReadU32(&v.epoch)||!b.ReadU16(&v.ownerNetID)||!b.ReadU16(&v.sourceKind)||!b.ReadU32(&v.ownerToken)||
       !b.ReadU8(&v.variant)||!b.ReadU8(&v.sourceRow)||!b.ReadF32(&v.shadowX)||!b.ReadF32(&v.shadowY)||
       !b.ReadU8(&flip)||!b.ReadI32(&v.order)||!b.ReadU32(&v.eventSerial)||flip>1||b.Remaining())
    {b.mReadPos=start;return false;}
    v.rootFlip=flip!=0;
    if(!LawnFusion::SourceCharred311BirthValid(v)){b.mReadPos=start;return false;}
    out=v;return true;
}
}
