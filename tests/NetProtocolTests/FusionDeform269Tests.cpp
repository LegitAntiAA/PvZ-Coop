#include "FusionDeform269Tests.h"
#include "../../Lawn/Fusion/FusionDeform269.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>
namespace
{
FusionDeform269TestResult result;
void Check(bool okay,int line){++result.mChecks;if(!okay){++result.mFailures;std::printf("    Deform269 check failed line=%d\n",line);}}
#define CHECK(x) Check(bool(x),__LINE__)
bool Near(double a,double b){return std::abs(a-b)<1e-9;}
}
FusionDeform269TestResult RunFusionDeform269Tests()
{
    using namespace LawnFusion;result={};
    FusionDeform269Matrix identity,root;root.value={0,-2,0,10,3,0,0,20,0,0,4,30,0,0,0,1};
    FusionDeform269Matrix inverse;CHECK(FusionDeform269Inverse(root,inverse));
    std::array<FusionDeform269Matrix,1> bones{root},binds{identity};std::vector<FusionDeform269Matrix> palette;
    CHECK(FusionDeform269Palette(inverse,bones,binds,palette));
    for(int i=0;i<16;++i)CHECK(Near(palette[0].value[i],identity.value[i]));
    std::array<FusionDeform269Vertex,1> vertices{{{1,2,3,.25,.75}}};
    std::array<FusionDeform269Weight,1> weights{FusionDeform269Weight{{0,0,0,0},{1,0,0,0}}};
    std::array<int,1> boneNodes{0};
    CHECK(FusionDeform269ValidateSkin(false,0,false,{}, {})==FusionSkin269Validation::NoSprite);
    CHECK(FusionDeform269ValidateSkin(true,0,true,boneNodes,weights)==FusionSkin269Validation::NoBindPoses);
    CHECK(FusionDeform269ValidateSkin(true,1,false,boneNodes,weights)==FusionSkin269Validation::MissingRoot);
    CHECK(FusionDeform269ValidateSkin(true,2,true,boneNodes,weights)==FusionSkin269Validation::WrongBoneCount);
    boneNodes[0]=-1;CHECK(FusionDeform269ValidateSkin(true,1,true,boneNodes,weights)==FusionSkin269Validation::NullBone);
    boneNodes[0]=0;CHECK(FusionDeform269ValidateSkin(true,1,true,boneNodes,weights)==FusionSkin269Validation::Ready);
    std::vector<FusionDeform269Vertex> output;CHECK(FusionDeform269Vertices(vertices,weights,palette,output));
    CHECK(Near(output[0].x,1)&&Near(output[0].y,2)&&Near(output[0].z,3)&&output[0].u==.25&&output[0].v==.75);
    std::array<FusionDeform269Matrix,4> four{identity,identity,identity,identity};
    four[0].value[3]=4;four[1].value[7]=8;four[2].value[11]=12;four[3].value[3]=-4;
    weights[0]=FusionDeform269Weight{{0,1,2,3},{.1,.2,.3,.4}};CHECK(FusionDeform269Vertices(vertices,weights,four,output));
    CHECK(Near(output[0].x,-.2)&&Near(output[0].y,3.6)&&Near(output[0].z,6.6));
    // The source bind pose is applied before boneWorld, not after it.
    binds[0].value[3]=-1;CHECK(FusionDeform269Palette(identity,bones,binds,palette));
    weights[0]=FusionDeform269Weight{{0,0,0,0},{1,0,0,0}};CHECK(FusionDeform269Vertices(vertices,weights,palette,output));
    CHECK(Near(output[0].x,6)&&Near(output[0].y,20)&&Near(output[0].z,42));
    const auto before=output;
    weights[0].indices[0]=4;CHECK(!FusionDeform269Vertices(vertices,weights,palette,output));CHECK(output[0].x==before[0].x);
    weights[0].indices[0]=0;weights[0].weights[0]=-1;CHECK(!FusionDeform269Vertices(vertices,weights,palette,output));
    weights[0].weights[0]=(std::numeric_limits<double>::quiet_NaN)();CHECK(!FusionDeform269Vertices(vertices,weights,palette,output));
    weights[0].weights={0,0,0,0};CHECK(FusionDeform269Vertices(vertices,weights,palette,output));CHECK(output[0].x==0&&output[0].y==0&&output[0].z==0); // No invented rigid fallback.
    auto singular=root;singular.value[0]=singular.value[1]=singular.value[2]=0;const auto old=inverse.value;CHECK(!FusionDeform269Inverse(singular,inverse));CHECK(inverse.value==old);
    auto bad=identity;bad.value[15]=0;CHECK(!FusionDeform269Palette(bad,bones,binds,palette));
    std::array<FusionDeform269Vertex,4> quad{{{0,0,0,0,0},{2,0,0,1,0},{2,2,0,1,1},{0,2,0,0,1}}};
    std::array<uint16_t,6> triangles{0,1,2,0,2,3};std::array<uint32_t,4> alpha{0x00000000,0xff000000,0xff000000,0x00000000};
    FusionMask269View mask;mask.vertices=quad;mask.indices=triangles;mask.texture={alpha,2,2,FusionMask269Filter::Bilinear,FusionMask269Wrap::Clamp};mask.alphaCutoff=.5;
    bool covered=false;CHECK(FusionMask269Coverage(mask,1,1,covered)&&covered);CHECK(FusionMask269Coverage(mask,.5,.5,covered)&&!covered);
    CHECK(FusionMask269Coverage(mask,3,1,covered)&&!covered);
    std::array<uint32_t,1> cutoff{0x33000000};mask.texture={cutoff,1,1};mask.alphaCutoff=.2;CHECK(FusionMask269Coverage(mask,1,1,covered)&&covered);
    std::array<uint32_t,4> pixels{0xff123456,0xff234567,0xff345678,0xff456789};const auto original=pixels;
    std::array<FusionMask269View,1> masks{mask};FusionMask269Sort sorting;
    CHECK(FusionMask269Composite(pixels,2,2,0,0,1,sorting,masks));CHECK(pixels==original);
    pixels=original;CHECK(FusionMask269Composite(pixels,2,2,0,0,2,sorting,masks));for(auto p:pixels)CHECK((p>>24)==0);
    pixels=original;CHECK(FusionMask269Composite(pixels,2,2,0,0,0,sorting,masks));CHECK(pixels==original);
    masks[0].enabled=false;pixels=original;CHECK(FusionMask269Composite(pixels,2,2,0,0,2,sorting,masks));CHECK(pixels==original);
    masks[0]=mask;masks[0].back={0,1,0};masks[0].front={0,2,0};pixels=original;
    sorting.order=0;CHECK(FusionMask269Composite(pixels,2,2,0,0,2,sorting,masks));CHECK(pixels==original);
    sorting.order=1;CHECK(FusionMask269Composite(pixels,2,2,0,0,2,sorting,masks));CHECK((pixels[0]>>24)==0);
    sorting.order=2;pixels=original;CHECK(FusionMask269Composite(pixels,2,2,0,0,2,sorting,masks));CHECK((pixels[0]>>24)==0);
    sorting.order=3;pixels=original;CHECK(FusionMask269Composite(pixels,2,2,0,0,2,sorting,masks));CHECK(pixels==original);
    sorting={0,0,12};masks[0]=mask;masks[0].back.scope=masks[0].front.scope=11;pixels=original;
    CHECK(FusionMask269Composite(pixels,2,2,0,0,2,sorting,masks));CHECK(pixels==original);
    masks[0]=mask;masks[0].alphaCutoff=(std::numeric_limits<double>::quiet_NaN)();pixels=original;
    CHECK(!FusionMask269Composite(pixels,2,2,0,0,2,sorting,masks));CHECK(pixels==original);
    masks[0]=mask;auto invalid=triangles;invalid[5]=4;masks[0].indices=invalid;
    CHECK(!FusionMask269Composite(pixels,2,2,0,0,2,sorting,masks));CHECK(pixels==original);
    CHECK(!FusionMask269Composite(pixels,2,2,0,0,3,sorting,{}));CHECK(!FusionMask269Composite(pixels,3,2,0,0,0,sorting,{}));
    // Two enabled masks form a coverage union at the same source sorting scope.
    masks[0]=mask;std::array<FusionMask269View,2> pair{mask,mask};pair[0].enabled=false;pixels=original;sorting={};
    CHECK(FusionMask269Composite(pixels,2,2,0,0,2,sorting,pair));CHECK((pixels[0]>>24)==0);
    return result;
}
