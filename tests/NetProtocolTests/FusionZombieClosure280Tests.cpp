#include "FusionZombieClosure280Tests.h"
#include "../../Lawn/Fusion/FusionSourceZombieClosure280.h"
#include "../../ConstEnums.h"
#include <cstdio>
#include <set>

int RunFusionZombieClosure280Tests(int& checks)
{
    using namespace LawnFusion;int failures=0;
    const auto check=[&](bool okay,int line){++checks;if(!okay){++failures;std::printf("FusionZombieClosure280Tests:%d failed\n",line);}};
#define CHECK(x) check(bool(x),__LINE__)
    std::vector<uint16_t> result{999};std::string error="old";
    const uint16_t doll[]={21};CHECK(BuildFusionSourceZombieClosure280(doll,result,&error));
    CHECK(result==std::vector<uint16_t>({21,22,23,2}));CHECK(error.empty());
    const uint16_t overlap[]={23,21,22,2,21,100,100};
    CHECK(BuildFusionSourceZombieClosure280(overlap,result,&error));
    CHECK(result.size()==5&&std::set<uint16_t>(result.begin(),result.end())==std::set<uint16_t>({2,21,22,23,100}));
    // Caller may reuse the output as input; failure must not invalidate aliases.
    result={21,21,23};CHECK(BuildFusionSourceZombieClosure280(result,result,&error));
    CHECK(result==std::vector<uint16_t>({21,22,23,2}));
    const auto before=result;const uint16_t partial[]={0,21,1};
    CHECK(!BuildFusionSourceZombieClosure280(partial,result,&error));CHECK(result==before);
    CHECK(error.find("rootKind=1")!=std::string::npos&&error.find("dependencyKind=1")!=std::string::npos);
    // Paper family members retain distinct source identities.
    const uint16_t paper[]={15,104};CHECK(BuildFusionSourceZombieClosure280(paper,result,&error));CHECK(result==std::vector<uint16_t>({15,104}));
    // Registry availability is distinct from resource/native/co-op admission.
    CHECK(GetNativeSourceZombieCarrier(203)==ZOMBIE_NORMAL&&GetNativeSourceZombieCarrier(223)==ZOMBIE_NORMAL);
    const uint16_t gold[]={223};CHECK(BuildFusionSourceZombieClosure280(gold,result,&error));CHECK(result==std::vector<uint16_t>({223,203}));
    CHECK(GetNativeSourceZombieCarrier(44)==ZOMBIE_BOSS);
    const uint16_t boss[]={44};CHECK(BuildFusionSourceZombieClosure280(boss,result,&error));CHECK(result==std::vector<uint16_t>({44}));
    const auto saved=result;
    const uint16_t unknown[]={65535};CHECK(!BuildFusionSourceZombieClosure280(unknown,result,&error));CHECK(result==saved);
    const uint16_t implemented[]={0,2,4,8,11,12,13,14,15,18,41,42,104,109,111,3,21,22,23,100,106,107,108};
    CHECK(BuildFusionSourceZombieClosure280(implemented,result,&error));CHECK(result.size()==23);
    for(auto kind:result)for(auto child:FusionDayZombie273NpcDependencies(kind))CHECK(std::find(result.begin(),result.end(),child)!=result.end());
    const uint16_t batch313[]={16,17,19,20,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,43,45,101,102,103,203,212,213,223};
    for(auto kind:batch313)CHECK(GetNativeSourceZombieCarrier(kind)==(kind==16?ZOMBIE_ZAMBONI:ZOMBIE_NORMAL));
    CHECK(BuildFusionSourceZombieClosure280(batch313,result,&error));
    CHECK(result.size()==30&&std::set<uint16_t>(result.begin(),result.end())==std::set<uint16_t>(std::begin(batch313),std::end(batch313)));
    const uint16_t transitive[]={212};CHECK(BuildFusionSourceZombieClosure280(transitive,result,&error));
    CHECK(result==std::vector<uint16_t>({212,37,40,213}));
    CHECK(BuildFusionSourceZombieClosure280({},result,&error));CHECK(result.empty()&&error.empty());
#undef CHECK
    return failures;
}
