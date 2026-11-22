// Generated from exact serialized source controllers; edit generator, not tables.
#include "FusionSourceGraph313.h"
namespace LawnFusion { namespace {
static const FusionSourceGraph313Marker g115_markers1[]={{1.f,FusionSourceGraph313Event::FallSound,0},{1.3333334f,FusionSourceGraph313Event::Destroy,0}};
static const FusionSourceGraph313Condition g115_conditions0[]={{FusionSourceGraph313Param::Speed,3,0.f},{FusionSourceGraph313Param::Jump,1,0.f}};
static const FusionSourceGraph313Edge g115_edges1[]={{0,0.25f,0.f,0.86956525f,true,true,true,g115_conditions0,2}};
static const FusionSourceGraph313State g115_states[]={{"idle","anim_walk",2.f,0.7f,true,1,false,nullptr,0,nullptr,0,-1},{"die","anim_death",1.3333334f,1.f,false,1,false,nullptr,0,g115_markers1,2,-1},{"walk","anim_walk",1.9166666f,2.f,true,1,false,g115_edges1,1,nullptr,0,-1}};
static const uint8_t g115_layerStates0[]={0,1,2};
static const FusionSourceGraph313Condition g115_conditions2[]={{FusionSourceGraph313Param::Die,1,0.f}};
static const FusionSourceGraph313Edge g115_edges3[]={{1,0.25f,0.f,0.75f,false,true,true,g115_conditions2,1}};
static const FusionSourceGraph313Layer g115_layers[]={{0,g115_layerStates0,3,g115_edges3,1,false}};
static const FusionSourceGraph313 g115={115,g115_states,3,g115_layers,1};
static const FusionSourceGraph313Marker g116_markers1[]={{1.f,FusionSourceGraph313Event::FallSound,0},{1.3333334f,FusionSourceGraph313Event::Destroy,0}};
static const FusionSourceGraph313Condition g116_conditions0[]={{FusionSourceGraph313Param::Speed,3,0.f},{FusionSourceGraph313Param::Jump,1,0.f}};
static const FusionSourceGraph313Edge g116_edges1[]={{0,0.25f,0.f,0.86956525f,true,true,true,g116_conditions0,2}};
static const FusionSourceGraph313State g116_states[]={{"idle","anim_walk",2.f,0.7f,true,1,false,nullptr,0,nullptr,0,-1},{"die","anim_death",1.3333334f,1.f,false,1,false,nullptr,0,g116_markers1,2,-1},{"walk","anim_walk",1.9166666f,2.f,true,1,false,g116_edges1,1,nullptr,0,-1}};
static const uint8_t g116_layerStates0[]={0,1,2};
static const FusionSourceGraph313Condition g116_conditions2[]={{FusionSourceGraph313Param::Die,1,0.f}};
static const FusionSourceGraph313Edge g116_edges3[]={{1,0.25f,0.f,0.75f,false,true,true,g116_conditions2,1}};
static const FusionSourceGraph313Layer g116_layers[]={{0,g116_layerStates0,3,g116_edges3,1,false}};
static const FusionSourceGraph313 g116={116,g116_states,3,g116_layers,1};
static const FusionSourceGraph313Marker g117_markers1[]={{1.f,FusionSourceGraph313Event::FallSound,0},{1.3333334f,FusionSourceGraph313Event::Destroy,0}};
static const FusionSourceGraph313Condition g117_conditions0[]={{FusionSourceGraph313Param::Attacking,2,0.f},{FusionSourceGraph313Param::Speed,3,0.f}};
static const FusionSourceGraph313Edge g117_edges1[]={{3,0.25f,0.f,0.75f,false,true,true,g117_conditions0,2}};
static const FusionSourceGraph313Marker g117_markers2[]={{0.33333334f,FusionSourceGraph313Event::Bite,0},{1.1f,FusionSourceGraph313Event::Bite,0}};
static const FusionSourceGraph313Condition g117_conditions2[]={{FusionSourceGraph313Param::Attacking,1,0.f},{FusionSourceGraph313Param::Speed,3,0.f}};
static const FusionSourceGraph313Edge g117_edges3[]={{2,0.25f,0.f,0.86956525f,false,true,true,g117_conditions2,2}};
static const FusionSourceGraph313Condition g117_conditions4[]={{FusionSourceGraph313Param::Attacking,2,0.f},{FusionSourceGraph313Param::Speed,3,0.f}};
static const FusionSourceGraph313Edge g117_edges5[]={{5,0.25f,0.f,0.75f,false,true,true,g117_conditions4,2}};
static const FusionSourceGraph313Marker g117_markers4[]={{0.33333334f,FusionSourceGraph313Event::Bite,0},{1.1f,FusionSourceGraph313Event::Bite,0}};
static const FusionSourceGraph313Condition g117_conditions6[]={{FusionSourceGraph313Param::Attacking,1,0.f},{FusionSourceGraph313Param::Speed,3,0.f}};
static const FusionSourceGraph313Edge g117_edges7[]={{4,0.25f,0.f,0.86956525f,false,true,true,g117_conditions6,2}};
static const FusionSourceGraph313State g117_states[]={{"idle","anim_walk",2.f,0.7f,true,1,false,nullptr,0,nullptr,0,-1},{"die","anim_death",1.3333334f,1.f,false,1,false,nullptr,0,g117_markers1,2,-1},{"attack","anim_eat",1.35f,1.f,true,1,false,g117_edges1,1,g117_markers2,2,-1},{"walk","anim_walk",1.9166666f,0.4f,true,1,false,g117_edges3,1,nullptr,0,-1},{"attack2","anim_eat",1.35f,1.f,true,1,false,g117_edges5,1,g117_markers4,2,-1},{"walk2","anim_walk",1.9166666f,0.5f,true,1,false,g117_edges7,1,nullptr,0,-1},{"New State","anim_head1",0.f,1.f,false,0,true,nullptr,0,nullptr,0,-1},{"Zombie_head/Jalapeno::idle","anim_head1",1.f,1.f,true,1,false,nullptr,0,nullptr,0,-1}};
static const uint8_t g117_layerStates0[]={0,1,2,3,4,5};
static const FusionSourceGraph313Condition g117_conditions8[]={{FusionSourceGraph313Param::Die,1,0.f}};
static const FusionSourceGraph313Edge g117_edges9[]={{1,0.25f,0.f,0.75f,false,true,true,g117_conditions8,1}};
static const uint8_t g117_layerStates1[]={6};
static const uint8_t g117_layerStates2[]={7};
static const FusionSourceGraph313Layer g117_layers[]={{0,g117_layerStates0,6,g117_edges9,1,false},{6,g117_layerStates1,1,nullptr,0,false},{7,g117_layerStates2,1,nullptr,0,false}};
static const FusionSourceGraph313 g117={117,g117_states,8,g117_layers,3};
}
const FusionSourceGraph313* FindFusionFinish363Graph(uint16_t kind){switch(kind){
case 115:return &g115;
case 116:return &g116;
case 117:return &g117;
default:return nullptr;}}
}
