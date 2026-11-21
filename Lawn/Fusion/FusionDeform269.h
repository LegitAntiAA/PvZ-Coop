#ifndef LAWN_FUSION_DEFORM269_H
#define LAWN_FUSION_DEFORM269_H
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
namespace Sexy { class MemoryImage;class Graphics;class Image;class Color; }
namespace LawnFusion
{
// Source matrices are row-major affine4x4. No projection or clock is owned here.
struct FusionDeform269Matrix { std::array<double,16> value{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1}; };
struct FusionDeform269Vertex { double x=0,y=0,z=0,u=0,v=0; };
struct FusionDeform269Weight { std::array<uint32_t,4> indices{};std::array<double,4> weights{}; };
enum class FusionSkin269Validation : uint8_t { NoSprite=0,NoBindPoses=1,MissingRoot=3,WrongBoneCount=5,NullBone=6,InvalidWeights=7,Ready=8 };
FusionSkin269Validation FusionDeform269ValidateSkin(bool hasSprite,std::size_t bindCount,bool hasRoot,
    std::span<const int> boneNodes,std::span<const FusionDeform269Weight> weights);
bool FusionDeform269Inverse(const FusionDeform269Matrix&,FusionDeform269Matrix&);
bool FusionDeform269Palette(const FusionDeform269Matrix& inverseRenderer,
    std::span<const FusionDeform269Matrix> boneWorld,std::span<const FusionDeform269Matrix> bindPoses,
    std::vector<FusionDeform269Matrix>& output);
// Source weights are never normalized or replaced with a rigid transform.
bool FusionDeform269Vertices(std::span<const FusionDeform269Vertex>,std::span<const FusionDeform269Weight>,
    std::span<const FusionDeform269Matrix> palette,std::vector<FusionDeform269Vertex>& output);
enum class FusionMask269Filter : uint8_t { Point, Bilinear };
enum class FusionMask269Wrap : uint8_t { Clamp, Repeat };
struct FusionMask269Texture
{
    std::span<const uint32_t> argb;
    int width=0,height=0;
    FusionMask269Filter filter=FusionMask269Filter::Bilinear;
    FusionMask269Wrap wrap=FusionMask269Wrap::Clamp;
};
struct FusionMask269Sort { int layer=0,order=0;uint64_t scope=0; };
struct FusionMask269View
{
    // Already sampled and projected to the same destination coordinates as DrawPose.
    std::span<const FusionDeform269Vertex> vertices;
    std::span<const uint16_t> indices;
    FusionMask269Texture texture;
    FusionMask269Sort back,front;
    double alphaCutoff=.2;
    bool enabled=true;
};
bool FusionMask269Coverage(const FusionMask269View&,double x,double y,bool& covered);
// Runs after original sprite tint/alpha/triangles were rasterized. Invalid input
// rejects atomically. Unmasked pixels retain their exact original ARGB values.
bool FusionMask269Composite(std::span<uint32_t> argb,int width,int height,int destinationX,int destinationY,
    uint8_t interaction,const FusionMask269Sort&,std::span<const FusionMask269View> masks);
// Production CPU raster/composite/blit adapter. Projected vertices use the same
// destination pixel coordinates as DrawPose triangles; scratch is caller-owned.
bool FusionMask269DrawTriangles(Sexy::Graphics&,Sexy::Image&,std::span<const FusionDeform269Vertex>,
    std::span<const uint16_t>,const Sexy::Color&,uint8_t interaction,const FusionMask269Sort&,
    std::span<const FusionMask269View>,Sexy::MemoryImage& scratch);
// Native image adapter; caller uses its existing Graphics clipping/blending blit.
bool FusionMask269CompositeImage(Sexy::MemoryImage&,int destinationX,int destinationY,
    uint8_t interaction,const FusionMask269Sort&,std::span<const FusionMask269View> masks);
}
#endif
