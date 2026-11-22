// Generated from exact serialized source controllers; edit generator, not tables.
#include "FusionSourceGraph313.h"
namespace LawnFusion { namespace {
static const FusionSourceGraph313Marker g114_markers1[]={{1.f,FusionSourceGraph313Event::FallSound,0},{1.3333334f,FusionSourceGraph313Event::Destroy,0}};
static const FusionSourceGraph313Condition g114_conditions0[]={{FusionSourceGraph313Param::Attacking,2,0.f},{FusionSourceGraph313Param::Wait,2,0.f},{FusionSourceGraph313Param::Speed,3,0.f}};
static const FusionSourceGraph313Condition g114_conditions1[]={{FusionSourceGraph313Param::Wait,1,0.f},{FusionSourceGraph313Param::Speed,3,0.f}};
static const FusionSourceGraph313Edge g114_edges2[]={{3,0.25f,0.f,0.75f,false,true,true,g114_conditions0,3},{5,0.25f,0.f,0.8148148f,true,true,true,g114_conditions1,2}};
static const FusionSourceGraph313Marker g114_markers2[]={{0.33333334f,FusionSourceGraph313Event::Bite,0},{1.1f,FusionSourceGraph313Event::Bite,0}};
static const FusionSourceGraph313Condition g114_conditions3[]={{FusionSourceGraph313Param::Attacking,1,0.f},{FusionSourceGraph313Param::Speed,3,0.f}};
static const FusionSourceGraph313Condition g114_conditions4[]={{FusionSourceGraph313Param::LosePaper,1,0.f}};
static const FusionSourceGraph313Edge g114_edges5[]={{2,0.25f,0.f,0.86956525f,false,true,true,g114_conditions3,2},{5,0.5685551f,0.f,0.84689003f,false,true,true,g114_conditions4,1}};
static const FusionSourceGraph313Condition g114_conditions6[]={{FusionSourceGraph313Param::Attacking,2,0.f},{FusionSourceGraph313Param::Speed,3,0.f}};
static const FusionSourceGraph313Edge g114_edges7[]={{5,0.25f,0.f,0.75f,false,true,true,g114_conditions6,2}};
static const FusionSourceGraph313Marker g114_markers4[]={{0.33333334f,FusionSourceGraph313Event::Bite,0},{1.1f,FusionSourceGraph313Event::Bite,0}};
static const FusionSourceGraph313Condition g114_conditions8[]={{FusionSourceGraph313Param::Attacking,1,0.f},{FusionSourceGraph313Param::Speed,3,0.f}};
static const FusionSourceGraph313Edge g114_edges9[]={{4,0.25f,0.f,0.86956525f,false,true,true,g114_conditions8,2}};
static const FusionSourceGraph313Condition g114_conditions10[]={{FusionSourceGraph313Param::Shoot,1,0.f}};
static const FusionSourceGraph313Edge g114_edges11[]={{7,0.25f,0.f,0.75f,false,true,true,g114_conditions10,1}};
static const FusionSourceGraph313Edge g114_edges12[]={{6,0.25f,0.f,0.75f,true,true,true,nullptr,0}};
static const FusionSourceGraph313Marker g114_markers7[]={{0.5f,FusionSourceGraph313Event::Shoot,0}};
static const FusionSourceGraph313State g114_states[]={{"idle","anim_walk",2.f,0.7f,true,1,false,nullptr,0,nullptr,0,-1},{"die","anim_death",1.3333334f,1.f,false,1,false,nullptr,0,g114_markers1,2,-1},{"attack2","anim_eat",1.35f,1.f,true,1,false,g114_edges2,2,g114_markers2,2,-1},{"walk","anim_walk",1.9166666f,0.5f,true,1,false,g114_edges5,2,nullptr,0,-1},{"attack","anim_eat",1.35f,1.f,true,1,false,g114_edges7,1,g114_markers4,2,-1},{"walk2","anim_walk",1.9166666f,0.5f,true,1,false,g114_edges9,1,nullptr,0,-1},{"New State","anim_head1",0.f,1.f,false,0,true,g114_edges11,1,nullptr,0,-1},{"shoot","anim_head1",0.6666667f,1.f,false,1,false,g114_edges12,1,g114_markers7,1,-1}};
static const uint8_t g114_layerStates0[]={0,1,2,3,4,5};
static const FusionSourceGraph313Condition g114_conditions13[]={{FusionSourceGraph313Param::Die,1,0.f}};
static const FusionSourceGraph313Edge g114_edges14[]={{1,0.25f,0.f,0.75f,false,true,true,g114_conditions13,1}};
static const uint8_t g114_layerStates1[]={6,7};
static const FusionSourceGraph313Layer g114_layers[]={{0,g114_layerStates0,6,g114_edges14,1,false},{6,g114_layerStates1,2,nullptr,0,true}};
static const FusionSourceGraph313 g114={114,g114_states,8,g114_layers,2};
static const FusionSourceGraph313Condition g118_conditions0[]={{FusionSourceGraph313Param::IdleToWalk,1,0.f}};
static const FusionSourceGraph313Edge g118_edges1[]={{3,0.25f,0.f,0.75f,false,true,true,g118_conditions0,1}};
static const FusionSourceGraph313Marker g118_markers1[]={{1.0833334f,FusionSourceGraph313Event::FallSound,0},{1.4166666f,FusionSourceGraph313Event::Destroy,0}};
static const FusionSourceGraph313Condition g118_conditions2[]={{FusionSourceGraph313Param::Attacking,2,0.f},{FusionSourceGraph313Param::Speed,3,0.f}};
static const FusionSourceGraph313Edge g118_edges3[]={{3,0.25f,0.f,0.75f,false,true,true,g118_conditions2,2}};
static const FusionSourceGraph313Marker g118_markers2[]={{1.0166667f,FusionSourceGraph313Event::Bite,0},{2.4833333f,FusionSourceGraph313Event::Bite,0}};
static const FusionSourceGraph313Condition g118_conditions4[]={{FusionSourceGraph313Param::Attacking,1,0.f},{FusionSourceGraph313Param::Speed,3,0.f}};
static const FusionSourceGraph313Edge g118_edges5[]={{2,0.19000816f,0.21799375f,0.824002f,false,true,true,g118_conditions4,2}};
static const FusionSourceGraph313Marker g118_markers5[]={{0.13333334f,FusionSourceGraph313Event::Shoot,0}};
static const FusionSourceGraph313State g118_states[]={{"idle","anim_walk",1.8333334f,1.f,true,1,false,g118_edges1,1,nullptr,0,-1},{"die","anim_death",1.4166666f,1.2f,false,1,false,nullptr,0,g118_markers1,2,-1},{"attack","anim_eat",2.9166667f,2.f,true,1,false,g118_edges3,1,g118_markers2,2,-1},{"walk","anim_walk",2.f,1.7f,true,1,false,g118_edges5,1,nullptr,0,-1},{"New State","anim_head1",0.f,1.f,false,0,true,nullptr,0,nullptr,0,-1},{"shoot","anim_head1",0.2f,2.f,false,1,false,nullptr,0,g118_markers5,1,-1}};
static const uint8_t g118_layerStates0[]={0,1,2,3};
static const FusionSourceGraph313Condition g118_conditions6[]={{FusionSourceGraph313Param::Die,1,0.f}};
static const FusionSourceGraph313Edge g118_edges7[]={{1,0.25f,0.f,0.75f,false,true,true,g118_conditions6,1}};
static const uint8_t g118_layerStates1[]={4,5};
static const FusionSourceGraph313Condition g118_conditions8[]={{FusionSourceGraph313Param::Shoot,1,0.f}};
static const FusionSourceGraph313Edge g118_edges9[]={{5,0.037580803f,0.f,0.7871652f,false,true,true,g118_conditions8,1}};
static const FusionSourceGraph313Layer g118_layers[]={{0,g118_layerStates0,4,g118_edges7,1,false},{4,g118_layerStates1,2,g118_edges9,1,true}};
static const FusionSourceGraph313 g118={118,g118_states,6,g118_layers,2};
static const FusionSourceGraph313Condition g119_conditions0[]={{FusionSourceGraph313Param::Fall,1,0.f}};
static const FusionSourceGraph313Edge g119_edges1[]={{2,0.f,0.038394954f,0.9888516f,false,true,true,g119_conditions0,1}};
static const FusionSourceGraph313Edge g119_edges2[]={{3,0.f,15.000014f,0.99590135f,true,true,true,nullptr,0}};
static const FusionSourceGraph313Marker g119_markers2[]={{1.f,FusionSourceGraph313Event::FallComplete,0}};
static const FusionSourceGraph313Condition g119_conditions3[]={{FusionSourceGraph313Param::Attacking,1,0.f},{FusionSourceGraph313Param::Speed,3,0.f}};
static const FusionSourceGraph313Edge g119_edges4[]={{4,0.25f,0.f,0.75f,false,true,true,g119_conditions3,2}};
static const FusionSourceGraph313Condition g119_conditions5[]={{FusionSourceGraph313Param::Attacking,2,0.f},{FusionSourceGraph313Param::Speed,3,0.f}};
static const FusionSourceGraph313Edge g119_edges6[]={{3,0.25f,0.f,0.75f,false,true,true,g119_conditions5,2}};
static const FusionSourceGraph313Marker g119_markers4[]={{0.48333332f,FusionSourceGraph313Event::Bite,0},{1.15f,FusionSourceGraph313Event::Bite,0}};
static const FusionSourceGraph313Marker g119_markers5[]={{1.f,FusionSourceGraph313Event::Destroy,0}};
static const FusionSourceGraph313Marker g119_markers8[]={{0.33333334f,FusionSourceGraph313Event::Shoot,0}};
static const FusionSourceGraph313State g119_states[]={{"idle","anim_walk",2.f,1.f,true,1,false,nullptr,0,nullptr,0,-1},{"walk","anim_walk",2.f,1.f,true,1,false,g119_edges1,1,nullptr,0,-1},{"fall","anim_eat",1.f,0.8f,false,1,false,g119_edges2,1,g119_markers2,1,-1},{"walk2","anim_walk",2.0166667f,0.4f,true,1,false,g119_edges4,1,nullptr,0,-1},{"attack","anim_eat",1.3333334f,1.f,true,1,false,g119_edges6,1,g119_markers4,2,-1},{"die","anim_death",1.f,1.f,false,1,false,nullptr,0,g119_markers5,1,-1},{"New State","anim_head1",1.f,1.f,true,1,false,nullptr,0,nullptr,0,-1},{"New State","anim_head1",0.f,1.f,false,0,true,nullptr,0,nullptr,0,-1},{"shoot","anim_head1",0.6666667f,1.f,false,1,false,nullptr,0,g119_markers8,1,-1}};
static const uint8_t g119_layerStates0[]={0,1,2,3,4,5};
static const FusionSourceGraph313Condition g119_conditions7[]={{FusionSourceGraph313Param::Die,1,0.f}};
static const FusionSourceGraph313Edge g119_edges8[]={{5,0.25f,0.f,0.75f,false,true,true,g119_conditions7,1}};
static const uint8_t g119_layerStates1[]={6};
static const uint8_t g119_layerStates2[]={7,8};
static const FusionSourceGraph313Condition g119_conditions9[]={{FusionSourceGraph313Param::Shoot,1,0.f}};
static const FusionSourceGraph313Edge g119_edges10[]={{8,0.25f,0.f,0.75f,false,true,true,g119_conditions9,1}};
static const FusionSourceGraph313Layer g119_layers[]={{0,g119_layerStates0,6,g119_edges8,1,false},{6,g119_layerStates1,1,nullptr,0,true},{7,g119_layerStates2,2,g119_edges10,1,true}};
static const FusionSourceGraph313 g119={119,g119_states,9,g119_layers,3};
}
const FusionSourceGraph313* FindFusionGround343Graph(uint16_t kind){switch(kind){
case 114:return &g114;
case 118:return &g118;
case 119:return &g119;
default:return nullptr;}}
}
