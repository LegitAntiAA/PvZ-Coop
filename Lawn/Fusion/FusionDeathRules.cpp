#include "FusionDeathRules.h"
#include "FusionCherryRules.h"
#include "FusionPotatoRules.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace LawnFusion
{
namespace
{
constexpr double kPixelsPerSourceX = 72.0 / 1.35;
constexpr double kPixelsPerSourceY = 100.0 / 1.67;
constexpr FusionDeathProfile profiles[] = {
    {1003,4000,FusionDeathBlastPolicy::Cherry,false},
    {1010,4000,FusionDeathBlastPolicy::SmallPotato,false},
    {1190,4000,FusionDeathBlastPolicy::SmallPotato,false},
    {1015,300,FusionDeathBlastPolicy::SmallPotato,false},
    {1163,300,FusionDeathBlastPolicy::SmallPotato,false},
    {1170,300,FusionDeathBlastPolicy::SmallPotato,false},
    {1162,300,FusionDeathBlastPolicy::SmallPotato,true}};
}

const FusionDeathProfile* FindFusionDeathProfile(uint16_t kind)
{
    for (const auto& profile : profiles) if (profile.kind == kind) return &profile;
    return nullptr;
}
bool FusionDeathFlagsValid(uint16_t kind, uint8_t flags)
{ return kind == 1010 || kind == 1190 ? flags <= 3 : flags == 0; }
bool FusionDeathReasonHasEffects(int sourceReason)
{ return sourceReason >= 0 && sourceReason <= 7; }

bool TryConsumeFusionPotatoNutThreshold(int health, int maxHealth, uint8_t& flags, int& damage)
{
    if (maxHealth <= 0 || flags > 3) return false;
    const int lower = maxHealth / 3;
    const int upper = static_cast<int>(static_cast<int64_t>(maxHealth) * 2 / 3);
    uint8_t next = flags;
    int result = 0;
    if (health > lower && health < upper)
    {
        if (!(next & 1)) { next |= 1; result = 500; }
    }
    else if (health < lower && !(next & 2)) { next |= 2; result = 500; }
    flags = next;
    damage = result;
    return true;
}
bool TryApplyFusionDeathHeal(int health, int maxHealth, int amount, int& healedHealth)
{
    if (health < 0 || maxHealth <= 0 || health > maxHealth || amount < 0) return false;
    healedHealth = static_cast<int>((std::min)(static_cast<int64_t>(maxHealth),
        static_cast<int64_t>(health) + amount));
    return true;
}

bool FusionDeathBlastValid(const NativeBlastSpec& spec)
{
    return std::isfinite(spec.centerX) && std::isfinite(spec.centerY) && spec.row >= 0 && spec.row < 6 &&
        (spec.policy == FusionDeathBlastPolicy::Cherry ? spec.damage == 1800 :
         spec.policy == FusionDeathBlastPolicy::SmallPotato && (spec.damage == 300 || spec.damage == 500 || spec.damage == 1800));
}
bool TryMakeFusionDeathBlast(uint16_t kind, double cellCenterX, double cellCenterY,
    int row, bool thresholdBlast, NativeBlastSpec& spec)
{
    const auto* profile = FindFusionDeathProfile(kind);
    if (!profile || (thresholdBlast && kind != 1010 && kind != 1190)) return false;
    NativeBlastSpec result;
    result.policy = profile->policy;
    result.centerX = cellCenterX;
    // Shadow is land+.3; CherryNut BombCherry is another+.5 above Shadow.
    result.centerY = cellCenterY - (kind == 1003 ? .8 : .3) * kPixelsPerSourceY;
    result.row = row;
    result.damage = thresholdBlast ? 500 : kind==1170 ? 300 : 1800;
    result.shake = !thresholdBlast;
    if (!FusionDeathBlastValid(result)) return false;
    spec = result;
    return true;
}
bool FusionDeathBlastAffectsRow(const NativeBlastSpec& spec, int row)
{
    if (!FusionDeathBlastValid(spec) || row < 0 || row >= 6) return false;
    const int distance = row - spec.row;
    return spec.policy == FusionDeathBlastPolicy::Cherry ? distance >= -1 && distance <= 1 : distance == 0;
}
bool FusionDeathBlastRectOverlaps(const NativeBlastSpec& spec,
    double x, double y, double width, double height)
{
    if (!FusionDeathBlastValid(spec)) return false;
    if (spec.policy == FusionDeathBlastPolicy::Cherry)
        return FusionCherryRectOverlaps({spec.centerX,spec.centerY},x,y,width,height);
    FusionPotatoArea area;
    area.centerX = spec.centerX; area.centerY = spec.centerY;
    return FusionPotatoBlastOverlaps(area,x,y,width,height);
}
int FusionCherryNutHealOverlapCount(const NativeBlastSpec& spec,
    double cellCenterX, double cellCenterY, int row)
{
    if (spec.policy != FusionDeathBlastPolicy::Cherry || !FusionDeathBlastAffectsRow(spec,row) ||
        !std::isfinite(cellCenterX) || !std::isfinite(cellCenterY)) return 0;
    // SOURCE122: both BoxCollider2D entries share root scale.33; Shadow(-.01,.47).
    struct Box { double x, y, width, height; };
    constexpr Box boxes[] = {
        {1.1920929e-7,2.147199,3.2122653,3.7848506},
        {.36352903,2.214995,1.8463796,4.1849174}};
    int count = 0;
    for (const auto& box : boxes)
    {
        const double width = box.width * .33 * kPixelsPerSourceX;
        const double height = box.height * .33 * kPixelsPerSourceY;
        const double x = cellCenterX + (box.x + .01) * .33 * kPixelsPerSourceX;
        const double y = cellCenterY - (.3 + (box.y - .47) * .33) * kPixelsPerSourceY;
        if (FusionDeathBlastRectOverlaps(spec,x-width*.5,y-height*.5,width,height)) ++count;
    }
    return count;
}
bool TryGetFusionDeathSpawnCells(int column, int row, int columns, int rows,
    std::array<FusionDeathSpawnCell, 9>& cells, size_t& count)
{
    if (columns <= 0 || columns > 10 || rows <= 0 || rows > 6 ||
        column < 0 || column >= columns || row < 0 || row >= rows) return false;
    std::array<FusionDeathSpawnCell, 9> result{};
    size_t size = 0;
    for (int x = (std::max)(0,column-1); x <= (std::min)(columns-1,column+1); ++x)
        for (int y = (std::max)(0,row-1); y <= (std::min)(rows-1,row+1); ++y)
            result[size++] = {x,y};
    cells = result; count = size;
    return true;
}
}
