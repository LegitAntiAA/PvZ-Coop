// Generated from exact serialized source controllers; edit generator, not tables.
#include "FusionSourceGraph313.h"
namespace LawnFusion { namespace {
static const FusionSourceGraph313Condition g49_conditions0[]={{FusionSourceGraph313Param::Speed,3,0.f},{FusionSourceGraph313Param::Wait,1,0.f}};
static const FusionSourceGraph313Edge g49_edges1[]={{3,0.25f,0.f,0.9166667f,false,true,true,g49_conditions0,2}};
static const FusionSourceGraph313Marker g49_markers2[]={{1.5f,FusionSourceGraph313Event::FallSound,0},{2.1666667f,FusionSourceGraph313Event::Destroy,0}};
static const FusionSourceGraph313Condition g49_conditions2[]={{FusionSourceGraph313Param::Speed,3,0.f},{FusionSourceGraph313Param::Wait,2,0.f}};
static const FusionSourceGraph313Edge g49_edges3[]={{1,0.25f,0.f,0.8333333f,false,true,true,g49_conditions2,2}};
static const FusionSourceGraph313Marker g49_markers5[]={{0.16666667f,FusionSourceGraph313Event::Shoot,0}};
static const FusionSourceGraph313State g49_states[]={{"idle","anim_walk",1.5f,1.f,true,1,false,nullptr,0,nullptr,0,-1},{"walk","anim_walk",3.f,2.5f,true,1,false,g49_edges1,1,nullptr,0,-1},{"die","anim_death",2.1666667f,1.8f,false,1,false,nullptr,0,g49_markers2,2,-1},{"stand","anim_eat",1.5f,1.f,true,1,false,g49_edges3,1,nullptr,0,-1},{"New State","anim_head1",0.f,1.f,false,0,true,nullptr,0,nullptr,0,-1},{"shoot","anim_head1",0.5f,1.f,false,1,false,nullptr,0,g49_markers5,1,-1}};
static const uint8_t g49_layerStates0[]={0,1,2,3};
static const FusionSourceGraph313Condition g49_conditions4[]={{FusionSourceGraph313Param::Die,1,0.f}};
static const FusionSourceGraph313Edge g49_edges5[]={{2,0.25f,0.f,0.75f,false,true,true,g49_conditions4,1}};
static const uint8_t g49_layerStates1[]={4,5};
static const FusionSourceGraph313Condition g49_conditions6[]={{FusionSourceGraph313Param::Shoot,1,0.f}};
static const FusionSourceGraph313Edge g49_edges7[]={{5,0.25f,0.f,0.75f,false,true,true,g49_conditions6,1}};
static const FusionSourceGraph313Layer g49_layers[]={{0,g49_layerStates0,4,g49_edges5,1,false},{4,g49_layerStates1,2,g49_edges7,1,true}};
static const FusionSourceGraph313 g49={49,g49_states,6,g49_layers,2};
}
const FusionSourceGraph313* FindFusionSnowGun363Graph(uint16_t kind){switch(kind){
case 49:return &g49;
default:return nullptr;}}
}
