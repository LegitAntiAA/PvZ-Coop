#pragma once
#include <cstdint>
namespace LawnFusion
{
// Exact TypeMgr source predicates, primary313B:40D030/40D0F0/40D220.
// Source categories are NOT inferred from native carrier or C# inheritance.
constexpr bool FusionSourceZombieIsAir313(uint16_t kind)
{return kind==25||kind==26||kind==29||kind==119||kind==205||kind==206;}
constexpr bool FusionSourceZombieIsDriver313(uint16_t kind)
{return kind==14||kind==16||kind==18||kind==31||kind==112||kind==200||kind==201||kind==216||kind==222||kind==223;}
constexpr bool FusionSourceZombieIsGargantuar313(uint16_t kind)
{return kind==35||kind==36||kind==38||kind==39||kind==43||kind==44||kind==46||kind==212||(kind>=218&&kind<=224);}
constexpr bool FusionSourcePlantIsBigNut313(uint16_t kind){return kind==251||kind==255;}
constexpr bool FusionSourcePlantIsDoubleBox313(uint16_t kind)
{return kind==915||kind==927||kind==1120||kind==1122||kind==1123||kind==1132||kind==1161||kind==1169||kind==1174||kind==1175;}
}
