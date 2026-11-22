// Generated from exact serialized source controllers; edit generator, not tables.
#include "FusionSourceGraph313.h"
namespace LawnFusion { namespace {
static const FusionSourceGraph313Condition g47_conditions0[]={{FusionSourceGraph313Param::Attacking,1,0.f},{FusionSourceGraph313Param::Speed,3,0.f}};
static const FusionSourceGraph313Edge g47_edges1[]={{2,0.25f,0.f,0.9090909f,false,true,true,g47_conditions0,2}};
static const FusionSourceGraph313Condition g47_conditions2[]={{FusionSourceGraph313Param::Speed,3,0.f},{FusionSourceGraph313Param::Attacking,2,0.f}};
static const FusionSourceGraph313Edge g47_edges3[]={{1,0.25f,0.f,0.8888889f,false,true,true,g47_conditions2,2}};
static const FusionSourceGraph313Marker g47_markers2[]={{0.8333333f,FusionSourceGraph313Event::Bite,0},{1.75f,FusionSourceGraph313Event::Bite,0}};
static const FusionSourceGraph313Marker g47_markers3[]={{1.6666666f,FusionSourceGraph313Event::FallSound,0},{2.1666667f,FusionSourceGraph313Event::Destroy,0}};
static const FusionSourceGraph313State g47_states[]={{"idle","anim_walk",1.25f,1.f,true,1,false,nullptr,0,nullptr,0,-1},{"walk","anim_walk",2.75f,0.6f,true,1,false,g47_edges1,1,nullptr,0,-1},{"eat","anim_eat",2.25f,1.5f,true,1,false,g47_edges3,1,g47_markers2,2,-1},{"death","anim_eat",2.1666667f,1.f,false,1,false,nullptr,0,g47_markers3,2,-1}};
static const uint8_t g47_layerStates0[]={0,1,2,3};
static const FusionSourceGraph313Condition g47_conditions4[]={{FusionSourceGraph313Param::Die,1,0.f}};
static const FusionSourceGraph313Edge g47_edges5[]={{3,0.25f,0.f,0.75f,false,true,true,g47_conditions4,1}};
static const FusionSourceGraph313Layer g47_layers[]={{0,g47_layerStates0,4,g47_edges5,1,false}};
static const FusionSourceGraph313 g47={47,g47_states,4,g47_layers,1};
static const FusionSourceGraph313Condition g48_conditions0[]={{FusionSourceGraph313Param::Attacking,1,0.f},{FusionSourceGraph313Param::Speed,3,0.f}};
static const FusionSourceGraph313Edge g48_edges1[]={{2,0.25f,0.f,0.9090909f,false,true,true,g48_conditions0,2}};
static const FusionSourceGraph313Condition g48_conditions2[]={{FusionSourceGraph313Param::Speed,3,0.f},{FusionSourceGraph313Param::Attacking,2,0.f}};
static const FusionSourceGraph313Edge g48_edges3[]={{1,0.25f,0.f,0.8888889f,false,true,true,g48_conditions2,2}};
static const FusionSourceGraph313Marker g48_markers2[]={{0.8333333f,FusionSourceGraph313Event::Bite,0},{1.75f,FusionSourceGraph313Event::Bite,0}};
static const FusionSourceGraph313Marker g48_markers3[]={{1.6666666f,FusionSourceGraph313Event::FallSound,0},{2.1666667f,FusionSourceGraph313Event::Destroy,0}};
static const FusionSourceGraph313State g48_states[]={{"idle","anim_walk",1.25f,1.f,true,1,false,nullptr,0,nullptr,0,-1},{"walk","anim_walk",2.75f,0.6f,true,1,false,g48_edges1,1,nullptr,0,-1},{"eat","anim_eat",2.25f,1.5f,true,1,false,g48_edges3,1,g48_markers2,2,-1},{"death","anim_eat",2.1666667f,1.f,false,1,false,nullptr,0,g48_markers3,2,-1}};
static const uint8_t g48_layerStates0[]={0,1,2,3};
static const FusionSourceGraph313Condition g48_conditions4[]={{FusionSourceGraph313Param::Die,1,0.f}};
static const FusionSourceGraph313Edge g48_edges5[]={{3,0.25f,0.f,0.75f,false,true,true,g48_conditions4,1}};
static const FusionSourceGraph313Layer g48_layers[]={{0,g48_layerStates0,4,g48_edges5,1,false}};
static const FusionSourceGraph313 g48={48,g48_states,4,g48_layers,1};
}
const FusionSourceGraph313* FindFusionSnow363Graph(uint16_t kind){switch(kind){
case 47:return &g47;
case 48:return &g48;
default:return nullptr;}}
}
