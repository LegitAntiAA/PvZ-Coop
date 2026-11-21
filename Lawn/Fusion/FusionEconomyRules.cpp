#include "FusionEconomyRules.h"
#include <limits>
namespace LawnFusion
{
bool IsFusionEconomyBatchKind(uint16_t kind)
{ return kind==1031||kind==1184||kind==1136||kind==1137||kind==1188; }
bool FusionEconomyPlantStateValid(uint16_t kind,uint32_t count,uint16_t rage,uint16_t bag,uint8_t mode)
{
    if (!IsFusionEconomyBatchKind(kind)) return false;
    if (kind==1188) return count<=kFusionGoldSunflowerMaximumCharges && !rage && !bag && mode<=1;
    return !count&&!rage&&!bag&&(kind==1031?mode<=1:mode==0);
}
bool TryGetFusionMoneyUpgrade(uint16_t source,int goldCount,uint16_t& result,int& fee)
{
    uint16_t next=0xffff; int base=0;
    switch(source)
    {
    case 1134: next=1135; base=1000; break;
    case 1138: next=1139; base=2000; break;
    case 1141: next=1142; base=3000; break;
    case 1136: next=1137; base=2000; break;
    case 1143: next=1144; base=1000; break;
    case 1145: next=1146; base=1000; break;
    case 1187: next=1188; base=3000; break;
    default: return false;
    }
    if (goldCount<0) return false;
    if (source==1136||source==1187)
        for(int i=0;i<goldCount;++i)
        {
            if(base>(std::numeric_limits<int>::max)()/2) return false;
            base*=2;
        }
    result=next; fee=base; return true;
}
bool TryGetFusionMoneySkillFee(uint16_t kind,int& fee)
{
    switch(kind)
    {
    case 916: case 923: case 934: case 1135: case 1146: case 1188: fee=1000; return true;
    case 1139: case 1142: case 1148: fee=3000; return true;
    case 1144: fee=2000; return true;
    default: return false;
    }
}
bool IsFusionMoneyUpgradeAncestry(uint16_t first,uint16_t second,uint16_t result)
{
    uint16_t expected=0xffff; int fee=0;
    return second==0xffff&&TryGetFusionMoneyUpgrade(first,0,expected,fee)&&expected==result;
}
bool TryGetFusionMoneyActionQuote(uint16_t kind,int count,FusionMoneyActionQuote& output)
{
    if(count<0)return false;
    FusionMoneyActionQuote quote;
    quote.upgrade=TryGetFusionMoneyUpgrade(kind,count,quote.resultKind,quote.fee);
    if(!quote.upgrade)
    {
        if(!TryGetFusionMoneySkillFee(kind,quote.fee))return false;
        quote.resultKind=kind;
    }
    output=quote;return true;
}
int FusionMoneySpendSunCount(int fee,bool isIZ)
{ return fee<=0||isIZ?0:fee/500<40?fee/500:40; }
}
