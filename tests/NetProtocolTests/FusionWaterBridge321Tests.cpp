#include "../../Lawn/Fusion/FusionWaterBridge321Rules.h"
#include "../../Lawn/Fusion/FusionWaterZombie313Rules.h"
#include <limits>
using namespace LawnFusion;
int RunFusionWaterBridge321Tests(int& checks)
{
    int failed=0;const auto test=[&](bool pass){++checks;if(!pass)++failed;};
    // Independent preset2 table recovered from InitZombieData3BA5C0.
    for(uint16_t kind:std::array<uint16_t,5>{16,17,19,20,27})
    {
        const auto* p=FindFusionWaterZombie313Profile(uint16_t(kind));test(p&&p->kind==kind);
        test(p&&p->maxHealth==(kind==16?1350:kind==17?270:720));
        test(p&&!p->firstArmor&&!p->secondArmor&&p->biteDamage==(kind==16?500:50));
        test(p&&!p->nativeLocomotion&&!p->waterOnly&&p->noAttack==(kind==16));
    }
    for(uint16_t kind:std::array<uint16_t,8>{0,14,18,24,26,30,255,65535})test(!FindFusionWaterZombie313Profile(uint16_t(kind)));
    for(uint16_t kind:std::array<uint16_t,7>{15,1049,1050,1051,1056,1066,1156})test(FusionDolphin313KelpKind(kind));
    for(uint16_t kind:std::array<uint16_t,5>{14,1048,1052,1065,1155})test(!FusionDolphin313KelpKind(kind));
    const uint8_t pool[]={0,0,1,1,0,0},dry[]={0,0,0,0,0};
    for(uint16_t kind:std::array<uint16_t,3>{17,19,27})
    {
        test(FusionWaterZombie313SourceInWater(kind));
        test(FusionWaterZombie313WorldValid(kind,6,true,pool,2));
        test(!FusionWaterZombie313WorldValid(kind,6,true,pool,1));
        test(FusionWaterZombie313WorldValid(kind,6,true,pool,1,true));
        test(FusionWaterZombie313WorldValid(kind,5,false,dry,0,true));
        test(!FusionWaterZombie313WorldValid(kind,5,false,dry,0));
    }
    for(uint16_t kind:std::array<uint16_t,2>{16,20})
    {test(!FusionWaterZombie313SourceInWater(kind));test(FusionWaterZombie313WorldValid(kind,5,false,dry,1));test(!FusionWaterZombie313WorldValid(kind,6,true,pool,2));}
    const FusionContactQuad321 a{{{0,0},{2,0},{2,2},{0,2}}};
    test(FusionContactQuads321(a,a));
    const FusionContactQuad321 contact{{{2,0},{3,0},{3,1},{2,1}}};test(FusionContactQuads321(a,contact));
    const FusionContactQuad321 separated{{{3,0},{4,0},{4,1},{3,1}}};test(!FusionContactQuads321(a,separated));
    // AABBs overlap at the corner, but actual rotated source box does not.
    const FusionContactQuad321 diamond{{{0,-1},{1,0},{0,1},{-1,0}}};
    const FusionContactQuad321 corner{{{.8f,.8f},{1.1f,.8f},{1.1f,1.1f},{.8f,1.1f}}};
    test(!FusionContactQuads321(diamond,corner));test(!FusionContactQuads321(corner,diamond));
    const FusionContactQuad321 middle{{{-.1f,-.1f},{.1f,-.1f},{.1f,.1f},{-.1f,.1f}}};test(FusionContactQuads321(diamond,middle));
    auto bad=a;bad[0][0]=(std::numeric_limits<float>::quiet_NaN)();test(!FusionContactQuads321(a,bad));
    bad=a;bad[1][1]=(std::numeric_limits<float>::infinity)();test(!FusionContactQuads321(bad,a));
    bad=a;bad[1]=bad[0];test(!FusionContactQuads321(a,bad));
    return failed;
}
