// Generated from exact serialized source controllers; edit generator, not tables.
#include "FusionSourceGraph313.h"
namespace LawnFusion { namespace {
static const FusionSourceGraph313Condition g209_conditions0[]={{FusionSourceGraph313Param::Attacking,1,0.f},{FusionSourceGraph313Param::Speed,3,0.f}};
static const FusionSourceGraph313Edge g209_edges1[]={{2,0.25f,0.f,0.88589203f,false,true,true,g209_conditions0,2}};
static const FusionSourceGraph313Condition g209_conditions2[]={{FusionSourceGraph313Param::Attacking,2,0.f},{FusionSourceGraph313Param::Speed,3,0.f}};
static const FusionSourceGraph313Edge g209_edges3[]={{1,0.25f,0.f,0.6052631f,false,true,true,g209_conditions2,2}};
static const FusionSourceGraph313Marker g209_markers2[]={{0.5833333f,FusionSourceGraph313Event::Bite,0},{1.5333333f,FusionSourceGraph313Event::Bite,0}};
static const FusionSourceGraph313Marker g209_markers3[]={{1.7333333f,FusionSourceGraph313Event::FallSound,0},{2.f,FusionSourceGraph313Event::Destroy,0}};
static const FusionSourceGraph313Condition g209_conditions4[]={{FusionSourceGraph313Param::Walk,1,0.f}};
static const FusionSourceGraph313Condition g209_conditions5[]={{FusionSourceGraph313Param::Jump,1,0.f}};
static const FusionSourceGraph313Edge g209_edges6[]={{1,0.19166654f,0.f,0.53541666f,false,false,true,g209_conditions4,1},{5,0.033248603f,0.f,0.9667514f,true,true,true,g209_conditions5,1}};
static const FusionSourceGraph313Marker g209_markers4[]={{0.f,FusionSourceGraph313Event::LandSound,0},{0.25f,FusionSourceGraph313Event::JumpFail,0}};
static const FusionSourceGraph313Condition g209_conditions7[]={{FusionSourceGraph313Param::Speed,3,0.f},{FusionSourceGraph313Param::JumpOver,1,0.f}};
static const FusionSourceGraph313Condition g209_conditions8[]={{FusionSourceGraph313Param::Speed,3,0.f},{FusionSourceGraph313Param::Walk,1,0.f}};
static const FusionSourceGraph313Edge g209_edges9[]={{4,0.007444918f,0.f,0.9925551f,false,true,true,g209_conditions7,2},{1,0.25f,0.f,0.875f,false,true,true,g209_conditions8,2}};
static const FusionSourceGraph313Marker g209_markers5[]={{0.f,FusionSourceGraph313Event::BigJump,0}};
static const FusionSourceGraph313State g209_states[]={{"idle","anim_walk",1.f,1.f,true,1,false,nullptr,0,nullptr,0,-1},{"walk2","anim_walk",2.f,0.7f,true,1,false,g209_edges1,1,nullptr,0,-1},{"attack","anim_eat",2.f,1.4f,true,1,false,g209_edges3,1,g209_markers2,2,-1},{"die","anim_death",2.f,1.5f,false,1,false,nullptr,0,g209_markers3,2,-1},{"walk","anim_walk",1.f,1.f,true,2,false,g209_edges6,2,g209_markers4,2,-1},{"jump2","anim_eat",2.f,0.6f,false,2,false,g209_edges9,2,g209_markers5,1,-1}};
static const uint8_t g209_layerStates0[]={0,1,2,3,4,5};
static const FusionSourceGraph313Condition g209_conditions10[]={{FusionSourceGraph313Param::Die,1,0.f}};
static const FusionSourceGraph313Edge g209_edges11[]={{3,0.11184442f,0.f,0.8160744f,false,true,true,g209_conditions10,1}};
static const FusionSourceGraph313Layer g209_layers[]={{0,g209_layerStates0,6,g209_edges11,1,false}};
static const FusionSourceGraph313 g209={209,g209_states,6,g209_layers,1};
}
const FusionSourceGraph313* FindFusionLate363Graph(uint16_t kind){switch(kind){
case 209:return &g209;
default:return nullptr;}}
}
