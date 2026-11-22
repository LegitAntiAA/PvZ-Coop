#include "FusionTravelBuff360Rules.h"
#include <cmath>
namespace LawnFusion {namespace {
constexpr uint64_t advancedMask=(uint64_t(1)<<48)-1;constexpr uint32_t ultimateMask=(uint32_t(1)<<26)-1;
bool Excluded(unsigned i,bool happy){switch(i){case 8:return !happy;case 9:case 12:case 13:case 14:case 29:case 30:case 34:case 36:case 37:case 40:case 43:case 44:case 45:return true;default:return false;}}
bool PoolValid(const std::vector<uint8_t>& v,unsigned limit){int previous=-1;for(auto i:v){if(i>=limit||int(i)<=previous)return false;previous=i;}return true;}
}
bool FusionTravelBuff360LedgerValid(const FusionTravelBuff360Ledger& s){if((s.advanced&~advancedMask)||(s.ultimate&~ultimateMask))return false;for(int round:s.advancedUnlockRound)if(round<0)return false;return true;}
bool HasFusionTravelAdvanced360(const FusionTravelBuff360Ledger& s,int i){return i>=0&&i<48&&(s.advanced&(uint64_t(1)<<i))!=0;}
bool HasFusionTravelUltimate360(const FusionTravelBuff360Ledger& s,int i){return i>=0&&i<26&&(s.ultimate&(uint32_t(1)<<i))!=0;}
bool BuildFusionTravelBuff360Pool(const FusionTravelBuff360Ledger& s,bool happy,FusionTravelBuff360Pool& out){if(!FusionTravelBuff360LedgerValid(s))return false;FusionTravelBuff360Pool p;
 for(unsigned i=0;i<48;++i)if(!Excluded(i,happy)&&!HasFusionTravelAdvanced360(s,int(i)))p.advanced.push_back(uint8_t(i));
 for(unsigned i=0;i<26;++i)if(!HasFusionTravelUltimate360(s,int(i)))p.ultimate.push_back(uint8_t(i));out=std::move(p);return true;}
bool SelectFusionTravelBuff360Award(const FusionTravelBuff360Pool& p,int groupDraw,int index,FusionTravelBuff360Award& out){
 if(!PoolValid(p.advanced,48)||!PoolValid(p.ultimate,26)||groupDraw<0||size_t(groupDraw)>=p.advanced.size()+p.ultimate.size()||index<0)return false;
 const bool advanced=size_t(groupDraw)<p.advanced.size();const auto& group=advanced?p.advanced:p.ultimate;if(size_t(index)>=group.size())return false;
 out={advanced?FusionTravelBuff360Type::Advanced:FusionTravelBuff360Type::Ultimate,group[size_t(index)]};return true;}
bool CommitFusionTravelBuff360Award(const FusionTravelBuff360Ledger& s,FusionTravelBuff360Award a,int round,FusionTravelBuff360Ledger& out){if(!FusionTravelBuff360LedgerValid(s)||round<0)return false;auto next=s;
 if(a.type==FusionTravelBuff360Type::Advanced){if(a.index>=48||HasFusionTravelAdvanced360(s,a.index))return false;next.advanced|=uint64_t(1)<<a.index;next.advancedUnlockRound[a.index]=round;}
 else if(a.type==FusionTravelBuff360Type::Ultimate){if(a.index>=26||HasFusionTravelUltimate360(s,a.index))return false;next.ultimate|=uint32_t(1)<<a.index;}
 else return false;out=next;return true;}
bool StepFusionTravelBuff360Cooldown(float t,float dt,float& out){if(!std::isfinite(t)||t<0||t>60||!std::isfinite(dt)||dt<0)return false;out=t>dt?t-dt:0;return true;}
}
