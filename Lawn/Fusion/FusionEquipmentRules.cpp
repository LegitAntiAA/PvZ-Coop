#include "FusionEquipmentRules.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <utility>
namespace LawnFusion
{
namespace
{
constexpr FusionEquipmentProfile profiles[]={
    {25,FusionEquipmentFamily::Capture,300,0,100,750,0,15},
    {1100,FusionEquipmentFamily::Capture,300,200,125,750,0,15},
    {1101,FusionEquipmentFamily::Capture,300,0,50,750,0,15},
    {1102,FusionEquipmentFamily::Capture,300,0,200,750,.5f,1},
    {1166,FusionEquipmentFamily::Capture,300,300,250,750,0,15},
    {1092,FusionEquipmentFamily::Pumpkin,4000,20,225,3000,6,6},
    {1097,FusionEquipmentFamily::Pumpkin,8000,0,275,3000,6,6},
    {1098,FusionEquipmentFamily::Pumpkin,4000,0,275,3000,6,6},
    {1099,FusionEquipmentFamily::Pumpkin,4000,0,275,3000,6,6},
    {935,FusionEquipmentFamily::Pumpkin,8000,0,325,5000,6,6},
    {1027,FusionEquipmentFamily::Defender,8000,0,125,5000,0,0},
    {1028,FusionEquipmentFamily::Defender,16000,0,175,5000,0,0},
    {1029,FusionEquipmentFamily::Defender,8000,0,100,5000,0,0},
    {1154,FusionEquipmentFamily::Producer,300,0,150,750,10,30},
    {1169,FusionEquipmentFamily::Pumpkin,12000,0,450,5000,6,6}};
constexpr uint16_t members[]={25,246,247,909,911,920,922,924,935,1092,1093,1094,1095,1096,
    1097,1098,1099,1100,1101,1102,1103,1104,1154,1157,1158,1159,1160,1166,1169};
}
bool FusionEquipmentTypeValid(int type){return type>=0&&type<=10;}
const FusionEquipmentProfile* FindFusionEquipmentProfile(uint16_t kind)
{for(const auto& p:profiles)if(p.kind==kind)return &p;return nullptr;}
std::size_t FusionEquipmentProfileCount(){return sizeof(profiles)/sizeof(profiles[0]);}
const FusionEquipmentProfile* FusionEquipmentProfileAt(std::size_t i){return i<FusionEquipmentProfileCount()?&profiles[i]:nullptr;}
bool TryGetFusionMagnetCaptureProfile(uint16_t kind,int light,bool grown,int previousRows,FusionMagnetCaptureProfile& out)
{
    const auto* p=FindFusionEquipmentProfile(kind);
    if(!p||p->family!=FusionEquipmentFamily::Capture||light<0||previousRows<2||previousRows>5)return false;
    FusionMagnetCaptureProfile candidate;candidate.initialSeconds=p->initialAttributeSeconds;
    if(kind==1100)
    {
        if(!light){candidate.radius=2;candidate.rowRange=previousRows;}
        else if(light==1){candidate.radius=5;candidate.cooldownSeconds=11;candidate.rowRange=3;}
        else if(light==2){candidate.radius=6;candidate.cooldownSeconds=7.5f;candidate.rowRange=4;}
        else {candidate.radius=7;candidate.cooldownSeconds=5;candidate.rowRange=5;}
        candidate.lightBurst=light>1;
    }
    else if(kind==1101){candidate.waterOnly=true;if(grown)candidate.cooldownSeconds=10;}
    else if(kind==1102){candidate.wholeWorld=true;candidate.cooldownSeconds=1;candidate.dropHeldOnDeath=false;}
    else if(kind==1166){candidate.radius=4;candidate.rowRange=2;candidate.cooldownSeconds=15;}
    out=candidate;return true;
}
bool FusionEquipmentMagnetMember(uint16_t kind)
{return std::find(std::begin(members),std::end(members),kind)!=std::end(members);}
bool FusionEquipmentGraphAdjacent(int ax,int ay,int bx,int by)
{return std::abs(int64_t(ax)-bx)<2&&std::abs(int64_t(ay)-by)<2;}
bool TryGetFusionMagnetComponentSizes(const FusionMagnetGraphNode* nodes,std::size_t count,std::vector<uint32_t>& out)
{
    if((!nodes&&count)||count>(std::numeric_limits<uint32_t>::max)())return false;
    std::vector<std::size_t> parent(count);std::vector<uint32_t> sizes(count,0);
    for(std::size_t i=0;i<count;++i)
    {if(nodes[i].column<0||nodes[i].row<0)return false;parent[i]=i;if(nodes[i].live&&FusionEquipmentMagnetMember(nodes[i].kind))sizes[i]=1;}
    const auto root=[&](std::size_t i){while(parent[i]!=i){parent[i]=parent[parent[i]];i=parent[i];}return i;};
    for(std::size_t i=0;i<count;++i)if(sizes[i])for(std::size_t j=0;j<i;++j)
        if(nodes[j].live&&FusionEquipmentMagnetMember(nodes[j].kind)&&FusionEquipmentGraphAdjacent(nodes[i].column,nodes[i].row,nodes[j].column,nodes[j].row))
        {const auto a=root(i),b=root(j);if(a!=b){parent[a]=b;sizes[b]+=sizes[a];}}
    std::vector<uint32_t> result(count,0);
    for(std::size_t i=0;i<count;++i)if(nodes[i].live&&FusionEquipmentMagnetMember(nodes[i].kind))result[i]=sizes[root(i)];
    out=std::move(result);return true;
}
bool TryGetFusionEquipmentArmorCapture(int slot,int type,FusionEquipmentCapturePlan& out)
{
    FusionEquipmentCapturePlan p;
    if(slot==0)
    {
        if(type==2)p.type=FusionEquipmentType::Bucket;
        else if(type==4)p.type=FusionEquipmentType::Helmet;
        else if(type==7||type==8){p.commit=FusionEquipmentCaptureCommit::MachineObject;p.preserveReturnedBucket=true;}
        else return false;
    }
    else if(slot==1)
    {if(type==1){p.type=FusionEquipmentType::Door;p.commit=FusionEquipmentCaptureCommit::Door;}
     else if(type==3){p.type=FusionEquipmentType::Ladder;p.commit=FusionEquipmentCaptureCommit::Ladder;}else return false;}
    else if(slot==2)
    {
        if(type==1){p.type=FusionEquipmentType::Jackbox;p.commit=FusionEquipmentCaptureCommit::Jackbox;}
        else if(type==2){p.type=FusionEquipmentType::Jumper;p.commit=FusionEquipmentCaptureCommit::Pogo;}
        else if(type==3){p.type=FusionEquipmentType::Pickaxe;p.commit=FusionEquipmentCaptureCommit::Pickaxe;}
        else if(type==4||type==5){p.type=type==4?FusionEquipmentType::IronHead:FusionEquipmentType::RedIronHead;p.commit=FusionEquipmentCaptureCommit::IronHead;}
        else return false;
    }
    else return false;
    out=p;return true;
}
bool TryGetFusionEquipmentPrefabIndex(int type,int& index)
{
    constexpr int indices[]={4,6,7,8,-1,-1,17,42,30,31,5};
    if(!FusionEquipmentTypeValid(type)||indices[type]<0)return false;index=indices[type];return true;
}
bool FusionMagnetPumpkinMetalTarget(int first,int second,int unique,int kind)
{
    if(first==2||first==4||first==7||first==8||second==1||second==3||(unique>=1&&unique<=3))return true;
    switch(kind){case 14:case 16:case 18:case 38:case 39:case 40:case 108:case 200:case 201:case 210:case 219:case 221:case 222:return true;default:return false;}
}
bool TryGetFusionMagnetPumpkinDamage(int base,int count,float speed,bool water,int& damage)
{
    if(base<0||count<1||!std::isfinite(speed)||speed<0)return false;
    const int64_t product=int64_t(base)*count;
    if(product>(std::numeric_limits<int>::max)())return false;
    const float scaled=static_cast<float>(product)*speed;
    if(!std::isfinite(scaled)||double(scaled)>double((std::numeric_limits<int>::max)()))return false;
    const int value=static_cast<int>(scaled);
    if(water&&value>(std::numeric_limits<int>::max)()/2)return false;
    damage=water?value*2:value;return true;
}
bool TryGetFusionBucketReceiverEffect(uint16_t kind,int type,int hp,int maximum,FusionBucketReceiverEffect& out)
{
    if(!FusionEquipmentTypeValid(type)||hp<=0||maximum<=0||hp>maximum)return false;
    FusionBucketReceiverEffect e;
    // Plant.UseItem46A080: a genuine Bucket replaces these base actors
    // through source SetPlant(Default), without recipe ancestry or HP transfer.
    if(kind==0||kind==3){if(type!=0)return false;e.replacementKind=kind==0?1020:1029;}
    else if(kind==1027){if(type!=1)return false;e.replacementKind=1028;}
    else if(kind==1028||kind==1029||kind==1097||kind==1098||kind==1099||kind==935||kind==1169)e.recoverHealth=500;
    else if(kind==1092)
    {if(type==0)e.replacementKind=1097;else if(type==2)e.replacementKind=1098;else if(type==3)e.replacementKind=1099;else {if(hp==maximum)return false;e.recoverHealth=500;}}
    else if(kind==1154)e.sunValue=25;
    else return false;
    out=e;return true;
}
bool TryGetFusionBucketDeathRecovery(int type,int supers,int ordinary,uint16_t& kind,int& amount)
{
    if(!FusionEquipmentTypeValid(type)||supers<0||ordinary<0||(type!=0&&type!=1&&type!=4&&type!=5))return false;
    if(supers){kind=1151;amount=8000/supers;return true;}
    if(ordinary){kind=1129;amount=1000/ordinary;return true;}return false;
}
bool TryGetFusionBucketLifetime(float seconds,bool disappear,bool tutor,bool held,bool flashing,bool& startFlash,bool& expire)
{
    if(!std::isfinite(seconds)||seconds<0)return false;
    startFlash=disappear&&!tutor&&!flashing&&seconds>4;
    expire=disappear&&!tutor&&!held&&seconds>8;return true;
}
bool TryGetFusionSunMagnetWeightedItem(int draw,int& item,FusionEquipmentType& type)
{
    constexpr int items[]={4,4,4,6,6,8,8,7,7,41};
    constexpr FusionEquipmentType types[]={FusionEquipmentType::Bucket,FusionEquipmentType::Bucket,FusionEquipmentType::Bucket,FusionEquipmentType::Helmet,FusionEquipmentType::Helmet,FusionEquipmentType::Pickaxe,FusionEquipmentType::Pickaxe,FusionEquipmentType::Jackbox,FusionEquipmentType::Jackbox,FusionEquipmentType::Machine};
    if(draw<0||draw>=10)return false;item=items[draw];type=types[draw];return true;
}
bool TryGetFusionSunMagnetPeriodicSeconds(int jitter,float& seconds)
{if(jitter< -2||jitter>2)return false;seconds=25.f+jitter;return true;}
}
