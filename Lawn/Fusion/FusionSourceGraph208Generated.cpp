// Generated from exact serialized source controllers; edit generator, not tables.
#include "FusionSourceGraph313.h"
namespace LawnFusion { namespace {
static const FusionSourceGraph313Condition g208_conditions0[]={{FusionSourceGraph313Param::Speed,3,0.f},{FusionSourceGraph313Param::Attacking,1,0.f}};
static const FusionSourceGraph313Condition g208_conditions1[]={{FusionSourceGraph313Param::QuickMove,1,0.f},{FusionSourceGraph313Param::Speed,3,0.f}};
static const FusionSourceGraph313Edge g208_edges2[]={{3,0.25f,0.f,0.75f,false,true,true,g208_conditions0,2},{2,0.25f,0.f,0.875f,true,true,true,g208_conditions1,2}};
static const FusionSourceGraph313Edge g208_edges3[]={{1,0.25f,0.f,0.75f,true,true,true,nullptr,0}};
static const FusionSourceGraph313Marker g208_markers2[]={{0.5833333f,FusionSourceGraph313Event::StartQuickMove,0},{1.f,FusionSourceGraph313Event::QuickMoveOver,0}};
static const FusionSourceGraph313Condition g208_conditions4[]={{FusionSourceGraph313Param::Speed,3,0.f},{FusionSourceGraph313Param::Attacking,2,0.f}};
static const FusionSourceGraph313Condition g208_conditions5[]={{FusionSourceGraph313Param::Speed,3,0.f},{FusionSourceGraph313Param::QuickMove,1,0.f}};
static const FusionSourceGraph313Edge g208_edges6[]={{1,0.25f,0.f,0.75f,false,true,true,g208_conditions4,2},{2,0.25f,0.f,0.75f,true,true,true,g208_conditions5,2}};
static const FusionSourceGraph313Marker g208_markers3[]={{0.65f,FusionSourceGraph313Event::Bite,0}};
static const FusionSourceGraph313Marker g208_markers4[]={{1.5f,FusionSourceGraph313Event::Destroy,0}};
static const FusionSourceGraph313State g208_states[]={{"idle","anim_walk",2.f,1.f,true,1,false,nullptr,0,nullptr,0,-1},{"walk","anim_walk",2.f,1.f,true,1,false,g208_edges2,2,nullptr,0,-1},{"attack2","anim_eat",1.1666666f,1.f,false,1,false,g208_edges3,1,g208_markers2,2,-1},{"attack","anim_eat",1.f,0.5f,true,1,false,g208_edges6,2,g208_markers3,1,-1},{"die","anim_death",1.5f,2.2f,false,1,false,nullptr,0,g208_markers4,1,-1},{"New State","anim_head1",0.f,1.f,false,0,true,nullptr,0,nullptr,0,-1},{"shake","anim_head1",0.18333334f,1.f,true,1,false,nullptr,0,nullptr,0,-1}};
static const uint8_t g208_layerStates0[]={0,1,2,3,4};
static const FusionSourceGraph313Condition g208_conditions7[]={{FusionSourceGraph313Param::Die,1,0.f}};
static const FusionSourceGraph313Edge g208_edges8[]={{4,0.25f,0.f,0.75f,false,true,true,g208_conditions7,1}};
static const uint8_t g208_layerStates1[]={5,6};
static const FusionSourceGraph313Condition g208_conditions9[]={{FusionSourceGraph313Param::Shake,1,0.f}};
static const FusionSourceGraph313Edge g208_edges10[]={{6,0.024108723f,0.f,0.7500002f,false,true,true,g208_conditions9,1}};
static const FusionSourceGraph313Layer g208_layers[]={{0,g208_layerStates0,5,g208_edges8,1,false},{5,g208_layerStates1,2,g208_edges10,1,true}};
static const FusionSourceGraph313 g208={208,g208_states,7,g208_layers,2};
}
const FusionSourceGraph313* FindFusionSourceGraph208(uint16_t kind){switch(kind){
case 208:return &g208;
default:return nullptr;}}
}
