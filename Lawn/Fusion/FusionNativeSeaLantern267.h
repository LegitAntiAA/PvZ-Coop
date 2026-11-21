#ifndef LAWN_FUSION_NATIVE_SEA_LANTERN267_H
#define LAWN_FUSION_NATIVE_SEA_LANTERN267_H
class Plant;
namespace LawnFusion
{
bool InitializeNativeSeaLantern267Plant(Plant*);
bool UpdateNativeSeaLantern267Plant(Plant*);
bool UpdateNativeSeaLantern267Phase(Plant*);
bool GetNativeSeaLantern267VisualPose(const Plant*,const char*& state,float& seconds);
int NativeSeaLantern267LightRadius(const Plant*);
}
#endif
