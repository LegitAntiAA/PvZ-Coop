#include "FusionDeform269.h"
#include "../../SexyAppFramework/MemoryImage.h"
#include "../../SexyAppFramework/Graphics.h"
#include "../../SexyAppFramework/TriVertex.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <cstring>
namespace LawnFusion
{
bool FusionMask269CompositeImage(Sexy::MemoryImage& image,int x,int y,uint8_t interaction,
    const FusionMask269Sort& sorting,std::span<const FusionMask269View> masks)
{
    static_assert(sizeof(*image.GetBits())==sizeof(uint32_t));
    if(image.mWidth<=0||image.mHeight<=0||image.mWidth>16384||image.mHeight>16384)return false;
    auto* bits=image.GetBits();if(!bits)return false;
    const size_t count=size_t(image.mWidth)*image.mHeight;
    // Sexy::ulong and uint32_t are distinct Windows types. Copy bytes rather
    // than alias the native allocation through an unrelated integer pointer.
    std::vector<uint32_t> pixels(count);std::memcpy(pixels.data(),bits,count*sizeof(uint32_t));
    if(!FusionMask269Composite(pixels,image.mWidth,image.mHeight,x,y,interaction,sorting,masks))return false;
    std::memcpy(bits,pixels.data(),count*sizeof(uint32_t));
    image.BitsChanged();image.CommitBits();return true;
}
bool FusionMask269DrawTriangles(Sexy::Graphics& graphics,Sexy::Image& texture,
    std::span<const FusionDeform269Vertex> vertices,std::span<const uint16_t> indices,
    const Sexy::Color& color,uint8_t interaction,const FusionMask269Sort& sorting,
    std::span<const FusionMask269View> masks,Sexy::MemoryImage& scratch)
{
    if(vertices.empty()||indices.empty()||indices.size()%3||interaction>2)return false;
    double minX=1e9,minY=1e9,maxX=-1e9,maxY=-1e9;
    for(const auto& v:vertices)
    {
        if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.u)||!std::isfinite(v.v)||std::abs(v.x)>1e9||std::abs(v.y)>1e9)return false;
        minX=(std::min)(minX,v.x);minY=(std::min)(minY,v.y);maxX=(std::max)(maxX,v.x);maxY=(std::max)(maxY,v.y);
    }
    for(auto index:indices)if(index>=vertices.size())return false;
    for(const auto& mask:masks){bool ignored=false;if(!FusionMask269Coverage(mask,0,0,ignored))return false;}
    const auto& clip=graphics.mClipRect;
    const int left=int((std::max)(std::floor(minX),double(clip.mX))),top=int((std::max)(std::floor(minY),double(clip.mY)));
    const int right=int((std::min)(std::ceil(maxX),double(clip.mX)+clip.mWidth)),bottom=int((std::min)(std::ceil(maxY),double(clip.mY)+clip.mHeight));
    if(right<=left||bottom<=top)return true;
    const int width=right-left,height=bottom-top;if(width>16384||height>16384)return false;
    if(scratch.mWidth!=width||scratch.mHeight!=height)scratch.Create(width,height);
    scratch.Clear();
    std::vector<std::array<Sexy::TriVertex,3>> triangles(indices.size()/3);
    for(size_t i=0;i<indices.size();++i)
    {
        const auto& v=vertices[indices[i]];
        triangles[i/3][i%3]=Sexy::TriVertex(float(v.x-left),float(v.y-top),float(v.u),float(v.v));
    }
    const auto raw=reinterpret_cast<const Sexy::TriVertex (*)[3]>(triangles.data());
    scratch.BltTrianglesTex(&texture,raw,int(triangles.size()),Sexy::Rect(0,0,width,height),color,Sexy::Graphics::DRAWMODE_NORMAL,0,0,graphics.mLinearBlend);
    if(!FusionMask269CompositeImage(scratch,left,top,interaction,sorting,masks))return false;
    // The scratch pixels already use integer coordinates in the destination.
    // Use clipped integer blitting for software targets as well as SDL.
    Sexy::Graphics output(graphics);
    output.SetColorizeImages(false);
    output.mTransX=output.mTransY=0;
    output.DrawImage(&scratch,left,top);
    return true;
}

}
