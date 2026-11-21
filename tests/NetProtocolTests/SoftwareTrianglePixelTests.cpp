#include "SoftwareTrianglePixelTests.h"
#include <algorithm>
#include <array>
#include <cstdio>
namespace
{
using std::min;using std::max;
using ulong=unsigned long;
// Same fixed-floor coordinate setup as SWTri.cpp; the production pixel kernel
// below is included directly, including its actual texel filtering/modulation.
int FixedFloor(int x){return x>0?x&0xFFFF0000:(x&0xFFFF0000)-0x10000;}
unsigned int Pixel(unsigned int texel,unsigned int globalAlpha,unsigned int destination)
{
    std::array<unsigned int,4> texture={texel,texel,texel,texel};
    const unsigned int* pTexture=texture.data();const int tex_pitch=2;
    const unsigned int tex_endpos=4;const int uMask=0x10000,vMask=0x10000;
    unsigned int u=0x8000,v=0x8000;auto* pix=&destination;
    struct Diffuse {unsigned int a,r,g,b;} globalDiffuse={globalAlpha,256,256,256};
#define TEXTURED
#define TEX_ALPHA
#define GLOBAL_ARGB
#define LINEAR_BLEND
#include "../../SexyAppFramework/SWTri_Pixel8888.cpp"
#undef LINEAR_BLEND
#undef GLOBAL_ARGB
#undef TEX_ALPHA
#undef TEXTURED
    return destination;
}
}
int RunSoftwareTrianglePixelTests(int& checks)
{
    int failures=0;
    const auto check=[&](bool pass,int line){++checks;if(!pass){++failures;std::printf("    SoftwareTrianglePixel line=%d\n",line);}};
#define CHECK(x) check(bool(x),__LINE__)
    // Actual crash: source texel passes the original alpha>8 guard, global
    // alpha1 truncates it to0, transparent destination made finalAlpha0.
    CHECK(Pixel(0xFFFFFFFF,1,0)==0);
    CHECK(Pixel(0x20FFFFFF,1,0)==0);
    for(unsigned int destination:{0U,0x73240C6BU,0xFFFFFFFFU})
    {CHECK(Pixel(0xFFFFFFFF,0,destination)==destination);CHECK(Pixel(0xFFFFFFFF,1,destination)==destination);}
    CHECK(Pixel(0x08FFFFFF,256,0x12345678)==0x12345678); // Existing texel threshold.
    const auto opaque=Pixel(0xFFFFFFFF,256,0);
    CHECK((opaque>>24)==255&&opaque==Pixel(0xFFFFFFFF,256,0x73240C6B));
    const auto partial=Pixel(0xFFFFFFFF,128,0);
    CHECK((partial>>24)>0&&(partial>>24)<255&&(partial&0xFFFFFF)!=0);
#undef CHECK
    return failures;
}
