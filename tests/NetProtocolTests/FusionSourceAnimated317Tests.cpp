#include "../../Lawn/Fusion/FusionNativeSourceAnimated317.h"
#include <cstdio>
#include <limits>
#include <set>
int RunFusionSourceAnimated317Tests(int& checks)
{
    using namespace LawnFusion;int failed=0;
    const auto check=[&](bool value,int line){++checks;if(!value){++failed;std::printf("SourceAnimated317:%d failed\n",line);}};
#define CHECK317(v) check(bool(v),__LINE__)
    CHECK317(!FindSourceAnimated317Spec(0)&&!FindSourceAnimated317Spec(5)&&!FindSourceAnimated317Spec(255));
    const float expectedLife[]={3.f,2.59583325f,.8333333f,.8333333f};
    const float expectedPlay[]={2.8f,2.39583325f,.8333333f,.8333333f};
    for(uint8_t variant=1;variant<=4;++variant)
    {
        const auto* spec=FindSourceAnimated317Spec(variant);CHECK317(spec!=nullptr);
        CHECK317(std::abs(spec->LifeSeconds()-expectedLife[variant-1])<1e-6f);
        CHECK317(std::abs(spec->PlaySeconds()-expectedPlay[variant-1])<1e-6f);
        float seconds=99;CHECK317(SourceAnimated317SampleSeconds(variant,0,seconds)&&seconds==0);
        CHECK317(SourceAnimated317SampleSeconds(variant,.5f,seconds)&&std::abs(seconds-expectedLife[variant-1]*.5f)<1e-6f);
        CHECK317(SourceAnimated317SampleSeconds(variant,1,seconds)&&std::abs(seconds-expectedPlay[variant-1])<1e-6f);
        for(float bad:{-.001f,1.001f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()})
        {seconds=77;CHECK317(!SourceAnimated317SampleSeconds(variant,bad,seconds)&&seconds==77);}
    }
    float unchanged=17;CHECK317(!SourceAnimated317SampleSeconds(0,.5f,unchanged)&&unchanged==17);
    CHECK317(FindSourceAnimated317Spec(1)->shadowAnchor&&FindSourceAnimated317Spec(2)->shadowAnchor);
    CHECK317(!FindSourceAnimated317Spec(3)->shadowAnchor&&!FindSourceAnimated317Spec(4)->shadowAnchor);
    CHECK317(std::abs(FindSourceAnimated317Spec(3)->rootScale*FindSourceAnimated317Spec(3)->drawMultiplier-.4f)<1e-7f);
    CHECK317(FindSourceAnimated317Spec(4)->nodes==21&&FindSourceAnimated317Spec(4)->sprites==20);
    float held=0;CHECK317(SourceAnimated317SampleSeconds(1,2.9f/3.f,held)&&std::abs(held-2.8f)<1e-6f);
    CHECK317(SourceAnimated317SampleSeconds(2,2.5f/expectedLife[1],held)&&std::abs(held-expectedPlay[1])<1e-6f);
    SourceCharred311Birth normal;normal.epoch=317;normal.ownerNetID=9;normal.ownerToken=0x23450009;normal.sourceKind=0;
    normal.sourceRow=5;normal.order=353000;normal.shadowX=400;normal.shadowY=520;normal.rootFlip=true;
    CHECK317(SourceCharred311BirthValid(normal));
    auto garg=normal;garg.variant=2;garg.sourceKind=35;CHECK317(SourceCharred311BirthValid(garg));
    garg.sourceKind=36;CHECK317(SourceCharred311BirthValid(garg));
    garg.sourceKind=0;CHECK317(!SourceCharred311BirthValid(garg));
    garg=normal;garg.sourceKind=35;CHECK317(!SourceCharred311BirthValid(garg));
    auto splash=normal;splash.variant=3;splash.eventSerial=1;splash.rootFlip=false;splash.order=357000;
    CHECK317(SourceCharred311BirthValid(splash));
    auto mind=splash;mind.variant=4;mind.order=400040;CHECK317(SourceCharred311BirthValid(mind));
    for(auto value:{normal,splash,mind})
    {
        auto bad=value;bad.epoch=0;CHECK317(!SourceCharred311BirthValid(bad));
        bad=value;bad.ownerNetID=0;CHECK317(!SourceCharred311BirthValid(bad));
        bad=value;bad.ownerToken=0;CHECK317(!SourceCharred311BirthValid(bad));
        bad=value;bad.sourceRow=6;CHECK317(!SourceCharred311BirthValid(bad));
        bad=value;++bad.order;CHECK317(!SourceCharred311BirthValid(bad));
        bad=value;bad.shadowX=std::numeric_limits<float>::quiet_NaN();CHECK317(!SourceCharred311BirthValid(bad));
        bad=value;bad.shadowY=std::numeric_limits<float>::infinity();CHECK317(!SourceCharred311BirthValid(bad));
        bad=value;bad.shadowX=4096.25f;CHECK317(!SourceCharred311BirthValid(bad));
    }
    auto bad=normal;bad.eventSerial=1;CHECK317(!SourceCharred311BirthValid(bad));
    bad=splash;bad.eventSerial=0;CHECK317(!SourceCharred311BirthValid(bad));
    bad=mind;bad.eventSerial=0;CHECK317(!SourceCharred311BirthValid(bad));
    for(auto value:{splash,mind}){value.rootFlip=true;CHECK317(!SourceCharred311BirthValid(value));}
    bad=splash;bad.variant=255;CHECK317(!SourceCharred311BirthValid(bad));
    uint32_t serial=77;CHECK317(TryNextSourceAnimated317Serial(0,serial)&&serial==1);
    CHECK317(TryNextSourceAnimated317Serial(1,serial)&&serial==2);
    CHECK317(TryNextSourceAnimated317Serial((std::numeric_limits<uint32_t>::max)()-1,serial)&&serial==(std::numeric_limits<uint32_t>::max)());
    serial=77;CHECK317(!TryNextSourceAnimated317Serial((std::numeric_limits<uint32_t>::max)(),serial)&&serial==77);
    std::set<decltype(SourceAnimated317SeenKey(normal))> seen;
    CHECK317(seen.insert(SourceAnimated317SeenKey(normal)).second);
    CHECK317(!seen.insert(SourceAnimated317SeenKey(normal)).second);
    CHECK317(seen.insert(SourceAnimated317SeenKey(splash)).second);
    CHECK317(!seen.insert(SourceAnimated317SeenKey(splash)).second);
    ++splash.eventSerial;CHECK317(seen.insert(SourceAnimated317SeenKey(splash)).second);
    splash.ownerToken=0x43210009;CHECK317(seen.insert(SourceAnimated317SeenKey(splash)).second);
    mind.eventSerial=splash.eventSerial;mind.ownerToken=splash.ownerToken;CHECK317(seen.insert(SourceAnimated317SeenKey(mind)).second);
#undef CHECK317
    return failed;
}
