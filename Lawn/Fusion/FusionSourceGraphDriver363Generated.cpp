// Generated from exact serialized source controllers; edit generator, not tables.
#include "FusionSourceGraph313.h"
namespace LawnFusion { namespace {
static const FusionSourceGraph313State g201_states[]={{"idle","anim_walk",2.f,2.f,true,1,false,nullptr,0,nullptr,0,-1},{"die","anim_death",1.f,0.9f,false,1,false,nullptr,0,nullptr,0,-1},{"New State","anim_head1",0.f,1.f,false,0,true,nullptr,0,nullptr,0,-1},{"shake","anim_head1",0.15f,1.f,true,1,false,nullptr,0,nullptr,0,-1}};
static const uint8_t g201_layerStates0[]={0,1};
static const FusionSourceGraph313Condition g201_conditions0[]={{FusionSourceGraph313Param::Die,1,0.f}};
static const FusionSourceGraph313Edge g201_edges1[]={{1,0.25f,0.f,0.75f,false,true,true,g201_conditions0,1}};
static const uint8_t g201_layerStates1[]={2,3};
static const FusionSourceGraph313Condition g201_conditions2[]={{FusionSourceGraph313Param::Shake,1,0.f}};
static const FusionSourceGraph313Edge g201_edges3[]={{3,0.25f,0.f,0.75f,false,true,true,g201_conditions2,1}};
static const FusionSourceGraph313Layer g201_layers[]={{0,g201_layerStates0,2,g201_edges1,1,false},{2,g201_layerStates1,2,g201_edges3,1,true}};
static const FusionSourceGraph313 g201={201,g201_states,4,g201_layers,2};
static const FusionSourceGraph313Condition g222_conditions0[]={{FusionSourceGraph313Param::JacksonPoint363,1,0.f}};
static const FusionSourceGraph313Edge g222_edges1[]={{2,0.101216495f,0.f,0.8785402f,true,true,true,g222_conditions0,1}};
static const FusionSourceGraph313Marker g222_markers1[]={{0.8333333f,FusionSourceGraph313Event::JacksonTurn363,0}};
static const FusionSourceGraph313Edge g222_edges2[]={{1,0.05470395f,0.f,0.9682137f,true,true,true,nullptr,0}};
static const FusionSourceGraph313Marker g222_markers2[]={{0.6666667f,FusionSourceGraph313Event::JacksonSummon363,0}};
static const FusionSourceGraph313State g222_states[]={{"idle","anim_walk",1.f,1.f,true,1,false,nullptr,0,nullptr,0,-1},{"QuickJacksonZombie::idle","anim_head1",0.8333333f,1.f,true,1,false,g222_edges1,1,g222_markers1,1,-1},{"QuickJacksonZombie::point","anim_head1",2.3333333f,1.f,false,1,false,g222_edges2,1,g222_markers2,1,-1}};
static const uint8_t g222_layerStates0[]={0};
static const uint8_t g222_layerStates1[]={1,2};
static const FusionSourceGraph313Layer g222_layers[]={{0,g222_layerStates0,1,nullptr,0,false},{1,g222_layerStates1,2,nullptr,0,false}};
static const FusionSourceGraph313 g222={222,g222_states,3,g222_layers,2};
}
const FusionSourceGraph313* FindFusionDriver363Graph(uint16_t kind){switch(kind){
case 201:return &g201;
case 222:return &g222;
default:return nullptr;}}
}
