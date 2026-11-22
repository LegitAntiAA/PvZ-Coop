#pragma once
#include <cmath>
#include <cstdint>
#include <limits>

namespace LawnFusion
{
// Zombie.Charred380E60 checks float body HP before virtual GetDamage/armor.
// The current ordinary source world uses D2. D4/5 thresholds are kept explicit
// so callers cannot silently reuse D2 when those worlds are admitted later.
inline bool SourceCharred315Direct(float body,int first,int damage,int difficulty)
{
    if(!std::isfinite(body)||first<0||damage<0||difficulty<1||difficulty>5)return false;
    if(difficulty==4)return body+float(first)<=float(damage)*.7f;
    if(difficulty==5)return body+float(first)<=float(damage)*.4f;
    return body<=float(damage);
}
// Genuine ordinary D2 GetDamage3854C0: jalaed affects hostile NPCs, including
// fixed damage. The source212 Board counter protects hostile actors only for
// non-fixed damage. Travel/buff/enhanced difficulty worlds have their own gate.
inline bool TrySourceBaseDamage315(int damage,bool controlled,bool jalaed,int& out,bool ultimateProtection=false,bool fix=false)
{
    if(damage<0)return false;
    float value=float(damage)*(!controlled&&jalaed?1.5f:1.f);
    if(!controlled&&!fix&&ultimateProtection)value*=.1f;
    if(!std::isfinite(value)||double(value)>double((std::numeric_limits<int>::max)()))return false;
    out=value<=0?0:int(value<1?1:value);return true;
}
inline bool SourceType12Charred315(float body,int remaining)
{return std::isfinite(body)&&remaining>0&&float(remaining)>body;}
}
