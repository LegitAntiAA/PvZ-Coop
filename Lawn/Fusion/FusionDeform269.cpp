#include "FusionDeform269.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
namespace LawnFusion
{
namespace
{
bool Finite(const FusionDeform269Vertex& v)
{return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::isfinite(v.u)&&std::isfinite(v.v);}
bool Affine(const FusionDeform269Matrix& m)
{return std::all_of(m.value.begin(),m.value.end(),[](double x){return std::isfinite(x);})&&
    m.value[12]==0&&m.value[13]==0&&m.value[14]==0&&m.value[15]==1;}
FusionDeform269Matrix Multiply(const FusionDeform269Matrix& a,const FusionDeform269Matrix& b)
{
    FusionDeform269Matrix out;out.value.fill(0);
    for(int r=0;r<4;++r)for(int c=0;c<4;++c)for(int k=0;k<4;++k)out.value[r*4+c]+=a.value[r*4+k]*b.value[k*4+c];
    return out;
}
bool Before(const FusionMask269Sort& a,const FusionMask269Sort& b)
{return a.layer<b.layer||(a.layer==b.layer&&a.order<b.order);}
bool ImageSize(int width,int height,size_t count)
{return width>0&&height>0&&width<=16384&&height<=16384&&size_t(width)*size_t(height)==count;}
bool MaskValid(const FusionMask269View& m)
{
    if(!std::isfinite(m.alphaCutoff)||m.alphaCutoff<0||m.alphaCutoff>1||!ImageSize(m.texture.width,m.texture.height,m.texture.argb.size())||
        m.texture.filter>FusionMask269Filter::Bilinear||m.texture.wrap>FusionMask269Wrap::Repeat||
        m.indices.empty()||m.indices.size()%3||m.vertices.empty()||Before(m.front,m.back)||m.back.scope!=m.front.scope)return false;
    for(const auto& v:m.vertices)if(!Finite(v)||std::abs(v.x)>1e9||std::abs(v.y)>1e9||std::abs(v.u)>1e6||std::abs(v.v)>1e6)return false;
    for(auto i:m.indices)if(i>=m.vertices.size())return false;
    return true;
}
int Texel(int x,int count,FusionMask269Wrap wrap)
{return wrap==FusionMask269Wrap::Clamp?std::clamp(x,0,count-1):(x%count+count)%count;}
double Alpha(const FusionMask269Texture& texture,double u,double v)
{
    // Clamp/repeat normalized coordinates before conversion prevents integer overflow.
    if(texture.wrap==FusionMask269Wrap::Clamp){u=std::clamp(u,0.,1.);v=std::clamp(v,0.,1.);}
    else {u-=std::floor(u);v-=std::floor(v);}
    const auto at=[&](int x,int y){return double(texture.argb[size_t(Texel(y,texture.height,texture.wrap))*texture.width+Texel(x,texture.width,texture.wrap)]>>24)/255.;};
    if(texture.filter==FusionMask269Filter::Point)return at(int(std::floor(u*texture.width)),int(std::floor(v*texture.height)));
    const double x=u*texture.width-.5,y=v*texture.height-.5;const int ix=int(std::floor(x)),iy=int(std::floor(y));const double fx=x-ix,fy=y-iy;
    return (at(ix,iy)*(1-fx)+at(ix+1,iy)*fx)*(1-fy)+(at(ix,iy+1)*(1-fx)+at(ix+1,iy+1)*fx)*fy;
}
bool Coverage(const FusionMask269View& m,double x,double y)
{
    if(!m.enabled)return false;
    for(size_t i=0;i<m.indices.size();i+=3)
    {
        const auto& a=m.vertices[m.indices[i]];const auto& b=m.vertices[m.indices[i+1]];const auto& c=m.vertices[m.indices[i+2]];
        const double det=(b.y-c.y)*(a.x-c.x)+(c.x-b.x)*(a.y-c.y);
        if(!std::isfinite(det)||std::abs(det)<1e-15)continue;
        const double wa=((b.y-c.y)*(x-c.x)+(c.x-b.x)*(y-c.y))/det;
        const double wb=((c.y-a.y)*(x-c.x)+(a.x-c.x)*(y-c.y))/det,wc=1-wa-wb;
        if(!std::isfinite(wa)||!std::isfinite(wb)||!std::isfinite(wc)||(std::min)({wa,wb,wc})< -1e-12)continue;
        const double u=wa*a.u+wb*b.u+wc*c.u,v=wa*a.v+wb*b.v+wc*c.v;
        if(Alpha(m.texture,u,v)>=m.alphaCutoff)return true;
    }
    return false;
}
}
FusionSkin269Validation FusionDeform269ValidateSkin(bool sprite,size_t bindCount,bool root,
    std::span<const int> bones,std::span<const FusionDeform269Weight> weights)
{
    if(!sprite)return FusionSkin269Validation::NoSprite;
    if(!bindCount)return FusionSkin269Validation::NoBindPoses;
    if(!root)return FusionSkin269Validation::MissingRoot;
    if(bones.size()!=bindCount)return FusionSkin269Validation::WrongBoneCount;
    for(int node:bones)if(node<0)return FusionSkin269Validation::NullBone;
    for(const auto& weight:weights)for(int i=0;i<4;++i)
        if(!std::isfinite(weight.weights[i])||weight.weights[i]<0||weight.indices[i]>=bindCount)return FusionSkin269Validation::InvalidWeights;
    return weights.empty()?FusionSkin269Validation::InvalidWeights:FusionSkin269Validation::Ready;
}
bool FusionDeform269Inverse(const FusionDeform269Matrix& source,FusionDeform269Matrix& output)
{
    if(!Affine(source))return false;const auto& m=source.value;
    const double determinant=m[0]*(m[5]*m[10]-m[6]*m[9])-m[1]*(m[4]*m[10]-m[6]*m[8])+m[2]*(m[4]*m[9]-m[5]*m[8]);
    if(!std::isfinite(determinant)||std::abs(determinant)<=1e-12)return false;
    FusionDeform269Matrix out;auto& r=out.value;
    r[0]=(m[5]*m[10]-m[6]*m[9])/determinant;r[1]=(m[2]*m[9]-m[1]*m[10])/determinant;r[2]=(m[1]*m[6]-m[2]*m[5])/determinant;
    r[4]=(m[6]*m[8]-m[4]*m[10])/determinant;r[5]=(m[0]*m[10]-m[2]*m[8])/determinant;r[6]=(m[2]*m[4]-m[0]*m[6])/determinant;
    r[8]=(m[4]*m[9]-m[5]*m[8])/determinant;r[9]=(m[1]*m[8]-m[0]*m[9])/determinant;r[10]=(m[0]*m[5]-m[1]*m[4])/determinant;
    for(int row=0;row<3;++row)r[row*4+3]=-(r[row*4]*m[3]+r[row*4+1]*m[7]+r[row*4+2]*m[11]);
    if(!Affine(out))return false;output=out;return true;
}
bool FusionDeform269Palette(const FusionDeform269Matrix& inverseRenderer,std::span<const FusionDeform269Matrix> bones,
    std::span<const FusionDeform269Matrix> binds,std::vector<FusionDeform269Matrix>& output)
{
    if(!Affine(inverseRenderer)||bones.empty()||bones.size()!=binds.size())return false;
    std::vector<FusionDeform269Matrix> result;result.reserve(bones.size());
    for(size_t i=0;i<bones.size();++i)
    {
        if(!Affine(bones[i])||!Affine(binds[i]))return false;
        auto matrix=Multiply(inverseRenderer,Multiply(bones[i],binds[i]));if(!Affine(matrix))return false;result.push_back(matrix);
    }
    output=std::move(result);return true;
}
bool FusionDeform269Vertices(std::span<const FusionDeform269Vertex> vertices,std::span<const FusionDeform269Weight> weights,
    std::span<const FusionDeform269Matrix> palette,std::vector<FusionDeform269Vertex>& output)
{
    if(vertices.empty()||vertices.size()!=weights.size()||palette.empty())return false;
    for(const auto& m:palette)if(!Affine(m))return false;
    std::vector<FusionDeform269Vertex> result;result.reserve(vertices.size());
    for(size_t i=0;i<vertices.size();++i)
    {
        const auto& v=vertices[i];const auto& influence=weights[i];if(!Finite(v))return false;
        FusionDeform269Vertex next;next.u=v.u;next.v=v.v;
        for(int j=0;j<4;++j)
        {
            const double w=influence.weights[j];const auto index=influence.indices[j];
            if(!std::isfinite(w)||w<0||index>=palette.size())return false;
            const auto& m=palette[index].value;
            next.x+=w*(m[0]*v.x+m[1]*v.y+m[2]*v.z+m[3]);
            next.y+=w*(m[4]*v.x+m[5]*v.y+m[6]*v.z+m[7]);
            next.z+=w*(m[8]*v.x+m[9]*v.y+m[10]*v.z+m[11]);
        }
        if(!Finite(next))return false;result.push_back(next);
    }
    output=std::move(result);return true;
}
bool FusionMask269Coverage(const FusionMask269View& mask,double x,double y,bool& covered)
{
    if(!MaskValid(mask)||!std::isfinite(x)||!std::isfinite(y)||std::abs(x)>1e9||std::abs(y)>1e9)return false;
    covered=Coverage(mask,x,y);return true;
}
bool FusionMask269Composite(std::span<uint32_t> pixels,int width,int height,int originX,int originY,uint8_t interaction,
    const FusionMask269Sort& sprite,std::span<const FusionMask269View> masks)
{
    if(!ImageSize(width,height,pixels.size())||interaction>2)return false;
    // Validate every record first; malformed source cannot partly erase a sprite.
    for(const auto& m:masks)if(!MaskValid(m))return false;
    if(interaction==0)return true;
    std::vector<const FusionMask269View*> applicable;
    for(const auto& m:masks)if(m.enabled&&(!m.front.scope||m.front.scope==sprite.scope)&&!Before(sprite,m.back)&&!Before(m.front,sprite))applicable.push_back(&m);
    for(int y=0;y<height;++y)for(int x=0;x<width;++x)
    {
        auto& argb=pixels[size_t(y)*width+x];if(!(argb>>24))continue;
        bool inside=false;
        for(const auto* m:applicable)if(Coverage(*m,double(originX)+x+.5,double(originY)+y+.5)){inside=true;break;}
        if((interaction==1)!=inside)argb&=0x00ffffffU;
    }
    return true;
}
}
