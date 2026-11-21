#include "FusionBucketReceivers274Tests.h"
#include "../../Lawn/Fusion/FusionEquipmentRules.h"
#include <cstdio>
#include <limits>

int RunFusionBucketReceivers274Tests(int& checks)
{
    using namespace LawnFusion;int failures=0;
    const auto check=[&](bool okay,int line){++checks;if(!okay){++failures;std::printf("FusionBucketReceivers274Tests:%d failed\n",line);}};
#define CHECK(x) check(bool(x),__LINE__)
    // A wounded source actor still becomes a fresh replacement. Neither its
    // remaining health nor the item enum may accidentally become a heal value.
    for(const uint16_t kind:{uint16_t(0),uint16_t(3)})
    {
        const int maximum=kind==0?300:4000;
        for(const int health:{1,maximum/2,maximum})
        {
            FusionBucketReceiverEffect effect{77,88,99};
            CHECK(TryGetFusionBucketReceiverEffect(kind,int(FusionEquipmentType::Bucket),health,maximum,effect));
            CHECK(effect.replacementKind==(kind==0?1020:1029));
            CHECK(effect.recoverHealth==0&&effect.sunValue==0);
        }
        for(int type=-1;type<=256;++type)
        {
            if(type==int(FusionEquipmentType::Bucket))continue;
            FusionBucketReceiverEffect effect{77,88,99};
            CHECK(!TryGetFusionBucketReceiverEffect(kind,type,maximum,maximum,effect));
            CHECK(effect.replacementKind==77&&effect.recoverHealth==88&&effect.sunValue==99);
        }
        for(const int health:{(std::numeric_limits<int>::min)(),-1,0,maximum+1,(std::numeric_limits<int>::max)()})
        {
            FusionBucketReceiverEffect effect{77,88,99};
            CHECK(!TryGetFusionBucketReceiverEffect(kind,0,health,maximum,effect));
            CHECK(effect.replacementKind==77&&effect.recoverHealth==88&&effect.sunValue==99);
        }
        for(const int maximumBad:{(std::numeric_limits<int>::min)(),-1,0})
        {
            FusionBucketReceiverEffect effect{77,88,99};
            CHECK(!TryGetFusionBucketReceiverEffect(kind,0,1,maximumBad,effect));
            CHECK(effect.replacementKind==77&&effect.recoverHealth==88&&effect.sunValue==99);
        }
    }
    // Catalog relatives and stock SeedType aliases are not the two source
    // base identities. They must not acquire this new item replacement path.
    for(const uint16_t kind:{uint16_t(1),uint16_t(2),uint16_t(1000),uint16_t(1004),uint16_t(65535)})
    {
        FusionBucketReceiverEffect effect{77,88,99};
        CHECK(!TryGetFusionBucketReceiverEffect(kind,0,1,300,effect));
        CHECK(effect.replacementKind==77&&effect.recoverHealth==88&&effect.sunValue==99);
    }
    FusionBucketReceiverEffect effect;
    CHECK(TryGetFusionBucketReceiverEffect(1029,0,1,8000,effect));
    CHECK(effect.replacementKind==65535&&effect.recoverHealth==500&&!effect.sunValue);
    CHECK(TryGetFusionBucketReceiverEffect(1027,1,8000,8000,effect));
    CHECK(effect.replacementKind==1028&&!effect.recoverHealth&&!effect.sunValue);
    CHECK(!TryGetFusionBucketReceiverEffect(1027,0,8000,8000,effect));
    CHECK(effect.replacementKind==1028&&!effect.recoverHealth&&!effect.sunValue);
#undef CHECK
    return failures;
}
