#include "FusionWaterPlants267Rules.h"
namespace LawnFusion
{
bool FusionLily267CanMimic(uint16_t kind)
{return kind==1||kind==13||kind==14||kind==16||kind==18;}
bool FusionLily267DoubleBoxKind(uint16_t kind)
{return kind==915||kind==927||kind==1120||kind==1122||kind==1123||kind==1132||kind==1161||kind==1169||kind==1174||kind==1175;}
bool FusionWater267PlantStateValid(uint16_t kind,uint32_t shots,uint16_t rage,uint16_t lilyType,uint8_t cover)
{return kind==12&&!shots&&!rage&&(lilyType==65535||FusionLily267CanMimic(lilyType))&&cover<=1;}
}
