#pragma once
#include <cstdint>
namespace LawnFusion {
struct FusionQing208View {float attributeCountdown=0;bool quickMove=false;};
bool FusionQing208ViewValid(const FusionQing208View&);
bool FusionQing208TargetStatusAllowed(int status);
bool FusionQing208PlantAllowed(bool controlled,int rowDifference,bool caltrop,bool flying,bool pumpkin,bool covered);
// Source ordinary non-Travel Awake / AttributeEvent intervals. Random source
// sample is supplied only after native resources and callbacks are prepared.
bool FusionQing208Interval(bool initial,float sample01,float& seconds);
}
