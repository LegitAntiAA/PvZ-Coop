#include "FusionNative.h"
#include "FusionNativeVisual.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Projectile.h"
#include "../Coin.h"
#include "../../LawnApp.h"

namespace LawnFusion
{
NativeDroppedCard269Data* EditNativeDroppedCard269Data(Coin* coin,bool create)
{
    auto* context=GetNativeContext(coin?coin->mBoard:nullptr);if(!context)return nullptr;
    const auto generation=coin->mBoard->mCoins.DataArrayGetID(coin);if(!generation)return nullptr;
    const auto found=context->droppedCards269.find(generation);
    if(found!=context->droppedCards269.end())return &found->second;
    return create&&!coin->mNetGhost&&!coin->mApp->NetIsClient()?&context->droppedCards269.try_emplace(generation).first->second:nullptr;
}
const NativeDroppedCard269Data* GetNativeDroppedCard269Data(const Coin* coin)
{return EditNativeDroppedCard269Data(const_cast<Coin*>(coin));}
NativeFertilize269Data* EditNativeFertilize269Data(Coin* coin,bool create)
{
    auto* context=GetNativeContext(coin?coin->mBoard:nullptr);if(!context)return nullptr;
    const auto generation=coin->mBoard->mCoins.DataArrayGetID(coin);if(!generation)return nullptr;
    const auto found=context->fertilizeCoins269.find(generation);
    if(found!=context->fertilizeCoins269.end())return &found->second;
    return create&&!coin->mNetGhost&&!coin->mApp->NetIsClient()?&context->fertilizeCoins269.try_emplace(generation).first->second:nullptr;
}
const NativeFertilize269Data* GetNativeFertilize269Data(const Coin* coin)
{return EditNativeFertilize269Data(const_cast<Coin*>(coin));}
namespace
{
template <typename T> T* FindOrCreate(std::unordered_map<uint32_t,T>& states,uint32_t generation,bool create)
{
    if(!generation)return nullptr;
    const auto found=states.find(generation);
    if(found!=states.end())return &found->second;
    return create?&states.try_emplace(generation).first->second:nullptr;
}
bool MayCreate(const GameObject* object,bool requested)
{return requested&&object->mApp&&!object->mApp->NetIsClient()&&!object->mNetGhost;}
}

NativeStar269PlantState* EditNativeStar269PlantState(Plant* plant,bool create)
{
    const auto* data=GetNativePlantData(plant);if(!data)return nullptr;
    const auto* profile=FindFusionStar269Profile(data->kind);
    if(!profile||!profile->storageSlots)return nullptr;
    return FindOrCreate(GetNativeContext(plant->mBoard)->star269Plants,
        plant->mBoard->mPlants.DataArrayGetID(plant),MayCreate(plant,create));
}
const NativeStar269PlantState* GetNativeStar269PlantState(const Plant* plant)
{return EditNativeStar269PlantState(const_cast<Plant*>(plant));}

NativeStar269ProjectileState* EditNativeStar269ProjectileState(Projectile* projectile,bool create)
{
    const auto* data=GetNativeProjectileData(projectile);
    if(!data||!FusionStar269ProjectileClass(data->kind))return nullptr;
    const auto generation=projectile->mBoard->mProjectiles.DataArrayGetID(projectile);
    auto& states=GetNativeContext(projectile->mBoard)->star269Projectiles;
    const bool exists=states.contains(generation);
    auto* state=FindOrCreate(states,generation,MayCreate(projectile,create));
    if(state&&!exists)
    {state->instanceToken=generation;if(data->kind==49)state->trackSpeed=8;}
    return state;
}
const NativeStar269ProjectileState* GetNativeStar269ProjectileState(const Projectile* projectile)
{return EditNativeStar269ProjectileState(const_cast<Projectile*>(projectile));}

NativeDoom269PlantState* EditNativeDoom269PlantState(Plant* plant,bool create)
{
    const auto* data=GetNativePlantData(plant);if(!data||(data->kind!=910&&data->kind!=930))return nullptr;
    return FindOrCreate(GetNativeContext(plant->mBoard)->doom269Plants,
        plant->mBoard->mPlants.DataArrayGetID(plant),MayCreate(plant,create));
}
const NativeDoom269PlantState* GetNativeDoom269PlantState(const Plant* plant)
{return EditNativeDoom269PlantState(const_cast<Plant*>(plant));}
NativeCompanion269State* EditNativeCompanion269State(Projectile* projectile,bool create)
{
    auto* context=GetNativeContext(projectile?projectile->mBoard:nullptr);
    if(!context||GetNativeProjectileData(projectile))return nullptr;
    return FindOrCreate(context->companions269,projectile->mBoard->mProjectiles.DataArrayGetID(projectile),MayCreate(projectile,create));
}
const NativeCompanion269State* GetNativeCompanion269State(const Projectile* projectile)
{return EditNativeCompanion269State(const_cast<Projectile*>(projectile));}
NativeCompanions269PlantState* EditNativeCompanions269PlantState(Plant* plant,bool create)
{
    const auto* data=GetNativePlantData(plant);
    if(!data||!FindFusionCompanions269Profile(data->kind))return nullptr;
    return FindOrCreate(GetNativeContext(plant->mBoard)->companionPlants269,
        plant->mBoard->mPlants.DataArrayGetID(plant),MayCreate(plant,create));
}
const NativeCompanions269PlantState* GetNativeCompanions269PlantState(const Plant* plant)
{return EditNativeCompanions269PlantState(const_cast<Plant*>(plant));}
Projectile* CreateNativeAuxiliary269Carrier(Board* board,int row,float x,float y)
{
    if(!CanApplyNativeFusionGameplay(board)||board->mApp->NetIsClient()||
        row<0||row>=GetNativeFusionRowCount(board)||!std::isfinite(x)||!std::isfinite(y)||std::abs(x)>4096||std::abs(y)>4096||
        board->mProjectiles.mSize>=board->mProjectiles.mMaxSize)return nullptr;
    auto* projectile=board->AddProjectile(int(x-20),int(y-20),Board::MakeRenderOrder(RENDER_LAYER_PROJECTILE,row,1),row,PROJECTILE_PEA);
    if(!projectile)return nullptr;
    projectile->mMotionType=MOTION_STAR;projectile->mPosX=x-20;projectile->mPosY=y-20;projectile->mPosZ=0;
    projectile->mX=int(projectile->mPosX);projectile->mY=int(projectile->mPosY);projectile->mShadowY=y;
    projectile->mVelX=projectile->mVelY=projectile->mVelZ=projectile->mAccZ=0;
    projectile->mRotation=projectile->mRotationSpeed=0;projectile->mProjectileAge=0;
    return projectile;
}
bool CanCreateNativeCompanion269Carrier(Board* board,FusionCompanion269Family family)
{
    if(!CanApplyNativeFusionGameplay(board)||board->mApp->NetIsClient()||
        board->mProjectiles.mSize>=board->mProjectiles.mMaxSize)return false;
    return EnsureNativeCompanion269Visual(board,family,false)&&
        (family!=FusionCompanion269Family::CherryLittleFly||EnsureNativeCompanion269Visual(board,family,true));
}
Projectile* CreateNativeCompanion269Carrier(Board* board,NativeCompanion269State state,int row,float x,float y)
{
    if(!CanCreateNativeCompanion269Carrier(board,state.family))return nullptr;
    auto* projectile=CreateNativeAuxiliary269Carrier(board,row,x,y);if(!projectile)return nullptr;
    state.instanceToken=board->mProjectiles.DataArrayGetID(projectile);state.ageTicks=0;
    if(!NativeCompanion269StateValid(state)){projectile->Die();return nullptr;}
    GetNativeContext(board)->companions269.emplace(state.instanceToken,state);
    return projectile;
}
NativeWorld269PlantState* EditNativeWorld269PlantState(Plant* plant,bool create)
{
    const auto* data=GetNativePlantData(plant);if(!data||(data->kind!=34&&data->kind!=937))return nullptr;
    return FindOrCreate(GetNativeContext(plant->mBoard)->worldPlants269,
        plant->mBoard->mPlants.DataArrayGetID(plant),MayCreate(plant,create));
}
const NativeWorld269PlantState* GetNativeWorld269PlantState(const Plant* plant)
{return EditNativeWorld269PlantState(const_cast<Plant*>(plant));}
const NativeWorld269BeamState* GetNativeWorld269BeamState(const Projectile* projectile)
{
    const auto* context=GetNativeContext(projectile?projectile->mBoard:nullptr);if(!context)return nullptr;
    const auto found=context->worldBeams269.find(projectile->mBoard->mProjectiles.DataArrayGetID(const_cast<Projectile*>(projectile)));
    return found==context->worldBeams269.end()?nullptr:&found->second;
}
Projectile* CreateNativeWorld269BeamCarrier(Board* board,const NativeWorld269BeamState& source)
{
    auto* projectile=CreateNativeAuxiliary269Carrier(board,source.sortingRow,source.startX,source.startY);
    if(!projectile)return nullptr;
    auto state=source;state.instanceToken=board->mProjectiles.DataArrayGetID(projectile);state.ageTicks=0;
    if(!NativeWorld269BeamStateValid(state)){projectile->Die();return nullptr;}
    GetNativeContext(board)->worldBeams269.emplace(state.instanceToken,state);
    return projectile;
}
std::vector<NativeDoom269Cloud>* EditNativeDoom269Clouds(Plant* plant,bool create)
{
    const auto* data=GetNativePlantData(plant);if(!data||data->kind!=910)return nullptr;
    return FindOrCreate(GetNativeContext(plant->mBoard)->doom269Clouds,
        plant->mBoard->mPlants.DataArrayGetID(plant),MayCreate(plant,create));
}
}
