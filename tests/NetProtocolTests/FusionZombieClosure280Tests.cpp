#include "FusionZombieClosure280Tests.h"
#include "../../Lawn/Fusion/FusionSourceZombieClosure280.h"
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
    const auto before=result;const uint16_t partial[]={0,21,5};
    CHECK(!BuildFusionSourceZombieClosure280(partial,result,&error));CHECK(result==before);
    CHECK(error.find("rootKind=5")!=std::string::npos&&error.find("dependencyKind=5")!=std::string::npos);
    // Source Paper5 is not the implemented SuperPaper15; no carrier fallback.
    const uint16_t paper[]={15,104};CHECK(BuildFusionSourceZombieClosure280(paper,result,&error));CHECK(result==std::vector<uint16_t>({15,104}));
    CHECK(GetNativeSourceZombieCarrier(203)<0);
    const uint16_t staged[]={203};const auto saved=result;CHECK(!BuildFusionSourceZombieClosure280(staged,result,&error));CHECK(result==saved);
    const uint16_t unknown[]={65535};CHECK(!BuildFusionSourceZombieClosure280(unknown,result,&error));CHECK(result==saved);
    const uint16_t implemented[]={0,2,4,8,11,12,13,14,15,18,41,42,104,109,111,3,21,22,23,100,106,107,108};
    CHECK(BuildFusionSourceZombieClosure280(implemented,result,&error));CHECK(result.size()==23);
    for(auto kind:result)for(auto child:FusionDayZombie273NpcDependencies(kind))CHECK(std::find(result.begin(),result.end(),child)!=result.end());
    CHECK(BuildFusionSourceZombieClosure280({},result,&error));CHECK(result.empty()&&error.empty());
#undef CHECK
    return failures;
}
