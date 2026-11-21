#include "FusionZombieMotionTests.h"
#include "../../Lawn/Fusion/FusionZombieMotion.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>

namespace
{
FusionZombieMotionTestResult result;
void Check(bool value,int line)
{++result.mChecks;if(!value){++result.mFailures;std::printf("    SourceZombieMotion check failed line=%d\n",line);}}
#define CHECK(value) Check(!!(value),__LINE__)
bool Close(double a,double b,double epsilon=2e-6){return std::abs(a-b)<=epsilon;}
}

FusionZombieMotionTestResult RunFusionZombieMotionTests()
{
    using namespace LawnFusion;
    result={};std::printf("[ RUN      ] Fusion.SourceZombieRawRootMotion\n");
    const float nan=(std::numeric_limits<float>::quiet_NaN)(),inf=(std::numeric_limits<float>::infinity)();
    struct KeyGolden {bool walk2;float time,x;};
    const KeyGolden keys[]={{false,0,-.81666666f},{false,.16666667f,-.9499999f},{false,.33333334f,-1.39f},
        {false,.5f,-1.7299999f},{false,.6666667f,-1.9133333f},{false,.8333333f,-1.9433333f},
        {false,1,-1.9433333f},{false,1.1666666f,-2.4266665f},{false,1.3333334f,-2.9266667f},
        {false,1.5f,-3.1466665f},{false,1.6666666f,-3.4366665f},
        {true,0,3.1064515f},{true,.38333333f,2.3741934f},{true,.6666667f,1.9709678f},
        {true,.81666666f,2.0225806f},{true,.96666664f,1.9612902f},{true,1.1f,1.6935483f},
        {true,1.3333334f,1.0387096f},{true,1.6166667f,.55806446f}};
    for(uint16_t kind:{uint16_t(0),uint16_t(2)})
    {
        for(const auto key:keys){float x=77;CHECK(TrySampleFusionZombieRootX(kind,key.walk2,key.time,x)&&x==key.x);}
        float sample=77;
        CHECK(TrySampleFusionZombieRootX(kind,false,.9f,sample)&&sample==-1.9433333f);
        CHECK(TrySampleFusionZombieRootX(kind,false,1.8f,sample)&&sample==-3.4366665f);
        CHECK(TrySampleFusionZombieRootX(kind,true,1.8f,sample)&&sample==.55806446f);
        CHECK(TrySampleFusionZombieRootX(kind,false,.25f,sample)&&Close(sample,-1.1545832157));
        CHECK(TrySampleFusionZombieRootX(kind,false,.75f,sample)&&Close(sample,-1.9358333));
        CHECK(TrySampleFusionZombieRootX(kind,true,.75f,sample)&&Close(sample,2.0010579));
        for(bool walk2:{false,true})
        {
            float rate=77;CHECK(TryGetFusionZombieWalkRate(kind,walk2,1.f,1.f,rate)&&rate==(walk2?.5f:.4f));
            CHECK(TryGetFusionZombieWalkRate(kind,walk2,1.8f,.5f,rate)&&rate==(walk2?.5f:.4f)*(1.8f*.5f));
            CHECK(TryGetFusionZombieWalkRate(kind,walk2,.9f,0,rate)&&rate==0);
            double delta=77,reflected=77;
            CHECK(TryGetFusionZombieRootDelta(kind,walk2,{4,.2f},{4,.2f},false,delta)&&delta==0);
            CHECK(TryGetFusionZombieRootDelta(kind,walk2,{4,kFusionZombieWalkClipSeconds},{5,0},false,delta)&&delta==0);
            CHECK(TryGetFusionZombieRootDelta(kind,walk2,{0,0},{1,0},false,delta)&&Close(delta,walk2?-2.54838704:-2.61999984));
            CHECK(TryGetFusionZombieRootDelta(kind,walk2,{0,0},{1,0},true,reflected)&&reflected==-delta);
            double three=77;CHECK(TryGetFusionZombieRootDelta(kind,walk2,{5,0},{8,0},false,three)&&three==delta*3);
            double scaled=77;CHECK(TryGetFusionZombieRootDelta(kind,walk2,{0,0},{1,0},true,scaled,3)&&scaled==reflected*3);
            CHECK(TryGetFusionZombieRootDelta(kind,walk2,{0,0},{1,0},false,scaled,0)&&scaled==0);
            double local=77,huge=77;
            CHECK(TryGetFusionZombieRootDelta(kind,walk2,{0,.2f},{0,.3f},false,local)&&
                TryGetFusionZombieRootDelta(kind,walk2,{(std::numeric_limits<uint32_t>::max)(),.2f},{(std::numeric_limits<uint32_t>::max)(),.3f},false,huge)&&huge==local);
            CHECK(TryGetFusionZombieRootDelta(kind,walk2,{0,0},{(std::numeric_limits<uint32_t>::max)(),0},false,huge)&&std::isfinite(huge)&&huge<0);
            for(double scale:{-1.,(std::numeric_limits<double>::quiet_NaN)(),(std::numeric_limits<double>::infinity)(),(std::numeric_limits<double>::max)()})
            {delta=77;CHECK(!TryGetFusionZombieRootDelta(kind,walk2,{0,0},{1,0},false,delta,scale)&&delta==77);}
        }
        // The original walk2 briefly reverses within the cycle; do not clamp it.
        double bump=77;CHECK(TryGetFusionZombieRootDelta(kind,true,{0,.6666667f},{0,.81666666f},false,bump)&&bump>0);
        for(float bad:{-.1f,std::nextafter(kFusionZombieWalkClipSeconds,3.f),nan,inf})
        {sample=77;CHECK(!TrySampleFusionZombieRootX(kind,false,bad,sample)&&sample==77);double delta=77;CHECK(!TryGetFusionZombieRootDelta(kind,false,{0,0},{0,bad},false,delta)&&delta==77);CHECK(!TryGetFusionZombieRootDelta(kind,false,{0,bad},{1,0},false,delta)&&delta==77);}
        for(float speed:{std::nextafter(.9f,0.f),std::nextafter(1.8f,2.f),nan,inf})
        {float rate=77;CHECK(!TryGetFusionZombieWalkRate(kind,false,speed,1,rate)&&rate==77);}
        for(float status:{-.5f,.4f,1.1f,nan,inf})
        {float rate=77;CHECK(!TryGetFusionZombieWalkRate(kind,false,1,status,rate)&&rate==77);}
        double delta=77;CHECK(!TryGetFusionZombieRootDelta(kind,false,{1,0},{0,kFusionZombieWalkClipSeconds},false,delta)&&delta==77);
        CHECK(!TryGetFusionZombieRootDelta(kind,false,{0,1},{0,.5f},false,delta)&&delta==77);
    }
    for(uint16_t kind:{uint16_t(1),uint16_t(4),uint16_t(65000),uint16_t(65535)})
    {float output=77;CHECK(!TryGetFusionZombieWalkRate(kind,false,1,1,output)&&output==77);CHECK(!TrySampleFusionZombieRootX(kind,false,0,output)&&output==77);double delta=77;CHECK(!TryGetFusionZombieRootDelta(kind,false,{0,0},{1,0},false,delta)&&delta==77);}
    std::printf("[ %s ] Fusion.SourceZombieRawRootMotion checks=%d failures=%d\n",result.mFailures?"FAILED":"OK",result.mChecks,result.mFailures);
    return result;
}
