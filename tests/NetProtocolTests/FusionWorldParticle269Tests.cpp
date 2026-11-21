#include "FusionWorldParticle269Tests.h"
#include "../../Lawn/Fusion/FusionWorldParticle269Rules.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
int RunFusionWorldParticle269Tests(int& checks)
{
    using namespace LawnFusion;int failures=0;
    const auto check=[&](bool pass,int line){++checks;if(!pass){++failures;std::printf("    WorldParticle269 line=%d\n",line);}};
#define CHECK(x) check(bool(x),__LINE__)
    // Actual LanternCloud Velocity.speedModifier: Hermite 1->0 by t=.3.
    std::array<FusionWorldParticle269Key,3> speed={{{0,1,0,0},{.3f,0,0,0},{1,0,0,0}}};
    float sample=44;CHECK(SampleFusionWorldParticle269Curve(speed,.15f,sample)&&std::abs(sample-.5f)<1e-6f);
    CHECK(SampleFusionWorldParticle269Curve(speed,.3f,sample)&&sample==0);
    CHECK(SampleFusionWorldParticle269Curve(speed,1,sample)&&sample==0);
    sample=44;CHECK(!SampleFusionWorldParticle269Curve(speed,std::numeric_limits<float>::quiet_NaN(),sample)&&sample==44);
    auto bad=speed;bad[1].time=0;CHECK(!SampleFusionWorldParticle269Curve(bad,.1f,sample));
    bad=speed;bad[1].outSlope=std::numeric_limits<float>::infinity();CHECK(!SampleFusionWorldParticle269Curve(bad,.1f,sample));
    std::vector<FusionWorldParticle269Sample> friction;CHECK(BuildFusionWorldParticle269Friction(speed,101,friction)&&friction.size()==101);
    double velocity=7;
    for(int i=0;i<101&&i<int(friction.size());++i)
    {
        velocity*=1-friction[i].low;float expected=0;CHECK(SampleFusionWorldParticle269Curve(speed,float(i)/100,expected));
        CHECK(std::abs(velocity-7*expected)<1e-5);
    }
    CHECK(velocity==0);CHECK(friction[0].low==0&&friction[30].low==1&&friction[100].low==1);
    const auto saved=friction;
    CHECK(!BuildFusionWorldParticle269Friction(speed,1,friction)&&friction.size()==saved.size());
    std::array<FusionWorldParticle269Key,3> resurrection={{{0,1,0,0},{.5f,0,0,0},{1,1,0,0}}};
    CHECK(!BuildFusionWorldParticle269Friction(resurrection,101,friction)&&friction[30].low==saved[30].low);
    // Source size pre-infinity holds the first value; it is not a zero tail.
    std::array<FusionWorldParticle269Key,3> size={{{.3f,1,0,0},{.5f,0,0,0},{1,0,0,0}}};
    CHECK(SampleFusionWorldParticle269Curve(size,0,sample)&&sample==1);
    CHECK(SampleFusionWorldParticle269Curve(size,.4f,sample)&&std::abs(sample-.5f)<1e-6f);
    float circle=0,away=0;CHECK(TryGetFusionWorldParticle269Orbit(-2.5f,.01f,circle,away));
    double x=1,y=0;
    for(int i=0;i<300;++i)
    {
        const double nx=x-.01*circle*y,ny=y+.01*circle*x;
        x=nx*(1+.01*away);y=ny*(1+.01*away);
    }
    CHECK(std::abs(std::hypot(x,y)-1)<1e-5);
    CHECK(std::abs(x-std::cos(-7.5))<1e-5&&std::abs(y-std::sin(-7.5))<1e-5);
    circle=42;away=43;CHECK(!TryGetFusionWorldParticle269Orbit(2,0,circle,away)&&circle==42&&away==43);
    CHECK(!TryGetFusionWorldParticle269Orbit(std::numeric_limits<float>::quiet_NaN(),.01f,circle,away));
    // Rectangle18 is not Donut17. Local scaling ignores parent SCALE while
    // inheriting all rotations; -90X places localY in depth and launches up.
    std::array<FusionWorldParticle269Transform,2> chain;
    chain[0].scale={.27f,.27f,.27f};
    chain[1].rotation={-.7071068f,0,0,.7071068f};chain[1].scale={.6825f,.6825f,.6825f};
    std::array<float,9> basis;
    CHECK(ComposeFusionWorldParticle269LocalBasis(chain,basis));
    CHECK(std::abs(basis[0]-.6825f)<1e-6f&&std::abs(basis[5]-.6825f)<1e-6f);
    FusionWorldParticle269Rectangle rectangle;
    CHECK(ProjectFusionWorldParticle269Rectangle(basis,{.6928823f,.5f},2,3,rectangle));
    CHECK(std::abs(rectangle.halfX-.6928823f*.6825f*2)<1e-6f&&rectangle.halfY==0);
    CHECK(std::abs(rectangle.launchDegrees-180)<1e-5f&&std::abs(rectangle.launchSpeedScale-.6825f*3)<1e-6f);
    chain[1].rotation={.7071068f,0,0,.7071068f};
    CHECK(ComposeFusionWorldParticle269LocalBasis(chain,basis)&&ProjectFusionWorldParticle269Rectangle(basis,{1,1},2,3,rectangle));
    CHECK(rectangle.launchDegrees==360); // Tod angle0 would randomize downward launch.
    chain[1].rotation={0,0,0,1};chain[1].scale={1,1,1};
    CHECK(ComposeFusionWorldParticle269LocalBasis(chain,basis)&&ProjectFusionWorldParticle269Rectangle(basis,{1,.5f},2,3,rectangle));
    CHECK(rectangle.halfX==2&&rectangle.halfY==1.5f&&rectangle.launchSpeedScale==0);
    const auto goodBasis=basis;chain[1].rotation={0,0,0,2};
    CHECK(!ComposeFusionWorldParticle269LocalBasis(chain,basis)&&basis==goodBasis);
    chain[1].rotation={0,0,0,0};CHECK(!ComposeFusionWorldParticle269LocalBasis(chain,basis)&&basis==goodBasis);
    chain[1].rotation={0,0,0,1};chain[1].scale[0]=0;
    CHECK(!ComposeFusionWorldParticle269LocalBasis(chain,basis)&&basis==goodBasis);
    chain[1].scale={1,1,1};chain[0].rotation={0,0,.38268343f,.9238795f};
    CHECK(ComposeFusionWorldParticle269LocalBasis(chain,basis));const auto savedRectangle=rectangle;
    CHECK(!ProjectFusionWorldParticle269Rectangle(basis,{1,1},2,3,rectangle)&&rectangle==savedRectangle); // Rotated rectangle requires correlated XY.
    constexpr float c=.70710678f;
    const std::array<float,9> collapsed={c,-c,0,0,0,-1,c,c,0};
    CHECK(!ProjectFusionWorldParticle269Rectangle(collapsed,{1,1},2,3,rectangle)&&rectangle==savedRectangle); // Sum of two uniform samples is not uniform BOX.
    CHECK(!ProjectFusionWorldParticle269Rectangle(goodBasis,{-1,1},2,3,rectangle)&&rectangle==savedRectangle);
    CHECK(!ProjectFusionWorldParticle269Rectangle(goodBasis,{1,1},0,3,rectangle)&&rectangle==savedRectangle);
    chain[0].rotation={0,0,0,std::numeric_limits<float>::quiet_NaN()};
    CHECK(!ComposeFusionWorldParticle269LocalBasis(chain,basis));
#undef CHECK
    return failures;
}
