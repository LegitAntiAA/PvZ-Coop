#include "../../Lawn/Fusion/FusionPlantDisable363Rules.h"
#include <cstdio>
#include <limits>
using namespace LawnFusion;namespace{using uint=unsigned;

bool ReferenceFire363(int param_1)

{
  bool bVar1;
  
  if (param_1 < 0x463) {
    if (param_1 < 0x387) {
      if ((param_1 - 0x10U & 0xfffffffd) == 0) {
        return true;
      }
      return param_1 == 0x386;
    }
    if (param_1 == 0x3a4) {
      return true;
    }
    switch(param_1) {
    case 0x419:
    case 0x41c:
    case 0x41d:
    case 0x41e:
    case 0x41f:
    case 0x420:
    case 0x421:
    case 0x423:
    case 0x425:
    case 0x426:
    case 0x42d:
    case 0x42f:
    case 0x431:
    case 0x433:
      goto code_r0x00018040d16d;
    case 0x41a:
    case 0x41b:
    case 0x422:
    case 0x424:
    case 0x427:
    case 0x428:
    case 0x429:
    case 0x42a:
    case 0x42b:
    case 0x42c:
    case 0x42e:
    case 0x430:
    case 0x432:
      goto code_r0x00018040d1dd;
    default:
      bVar1 = param_1 == 0x462;
    }
  }
  else if (param_1 < 0x48c) {
    if (param_1 == 0x47d) {
      return true;
    }
    if (param_1 - 0x483U < 2) {
      return true;
    }
    bVar1 = param_1 == 0x48b;
  }
  else {
    if (0x495 < param_1) {
      if (param_1 - 0x49aU < 2) {
        return true;
      }
      if (param_1 != 0x49e) {
        return false;
      }
      return true;
    }
    if (param_1 == 0x48f) {
      return true;
    }
    bVar1 = param_1 == 0x495;
  }
  if (!bVar1) {
code_r0x00018040d1dd:
    return false;
  }
code_r0x00018040d16d:
  return true;
}



bool ReferenceIce363(int param_1)

{
  uint uVar1;
  bool bVar2;
  
  if (param_1 < 0x417) {
    if (0x38c < param_1) {
      if (param_1 < 0x398) {
        if (param_1 == 0x38e) {
          return true;
        }
        bVar2 = param_1 == 0x397;
      }
      else {
        if (param_1 - 0x40aU < 8) {
          return true;
        }
        bVar2 = param_1 == 0x416;
      }
      goto code_r0x00018040d340;
    }
    if (param_1 < 0xf2) {
      if (param_1 == 10) {
        return true;
      }
      return param_1 == 0xf1;
    }
    uVar1 = param_1 - 0x388;
  }
  else {
    if (0x453 < param_1) {
      if (0x48d < param_1) {
        if ((param_1 - 0x49cU & 0xfffffffd) == 0) {
          return true;
        }
        if (param_1 == 0x4a5) {
          return true;
        }
        return false;
      }
      if (param_1 == 0x463) {
        return true;
      }
      bVar2 = param_1 == 0x48d;
      goto code_r0x00018040d340;
    }
    if (0x430 < param_1) {
      if (param_1 == 0x432) {
        return true;
      }
      if (param_1 - 0x451U < 3) {
        return true;
      }
      return false;
    }
    uVar1 = param_1 - 0x42c;
  }
  bVar2 = (uVar1 & 0xfffffffb) == 0;
code_r0x00018040d340:
  if (bVar2) {
    return true;
  }
  return false;
}


}
int RunFusionPlantDisable363Tests(int& checks)
{
 int failures=0;auto check=[&](bool yes,int line){++checks;if(!yes){++failures;std::printf("Disable363 line%d failed\n",line);}};
#define test(x) check((x),__LINE__)
 for(unsigned k=0;k<=65535;++k)
 {test(FusionSourcePlantFire363(uint16_t(k))==ReferenceFire363(int(k)));test(FusionSourcePlantIce363(uint16_t(k))==ReferenceIce363(int(k)));}
 test(FusionSourcePlantDisable363Immune(251)&&FusionSourcePlantDisable363Immune(255)&&FusionSourcePlantDisable363Immune(1179));
 test(FusionSourcePlantDisable363Immune(10)&&FusionSourcePlantDisable363Immune(18));
 test(!FusionSourcePlantDisable363Immune(0)&&!FusionSourcePlantDisable363Immune(1)&&!FusionSourcePlantDisable363Immune(1032));
 std::vector<uint16_t> v;test(FusionPlantDisable363ViewValid(v));test(TryAddFusionPlantDisable363(v,v)&&v==std::vector<uint16_t>{500});
 bool active=true;for(int i=0;i<20;++i)test(TickFusionPlantDisable363(v,active)&&!active);
 test(v==std::vector<uint16_t>{480});test(TryAddFusionPlantDisable363(v,v)&&v==std::vector<uint16_t>({480,500}));
 for(int i=0;i<480;++i)test(TickFusionPlantDisable363(v,active)&&!active);
 test(v==std::vector<uint16_t>{20});for(int i=0;i<19;++i)test(TickFusionPlantDisable363(v,active)&&!active);
 test(TickFusionPlantDisable363(v,active)&&active&&v.empty());test(TickFusionPlantDisable363(v,active)&&!active);
 for(auto malformed:{std::vector<uint16_t>{0},std::vector<uint16_t>{501},std::vector<uint16_t>{500,1}})
 {const auto original=malformed;active=true;test(!FusionPlantDisable363ViewValid(malformed));test(!TickFusionPlantDisable363(malformed,active)&&malformed==original&&active);v={123};test(!TryAddFusionPlantDisable363(malformed,v)&&v==std::vector<uint16_t>{123});}
 v.assign(65535,500);const auto original=v;test(FusionPlantDisable363ViewValid(v));test(!TryAddFusionPlantDisable363(v,v)&&v==original);
 v.assign(65536,500);test(!FusionPlantDisable363ViewValid(v));
#undef test
 return failures;
}
