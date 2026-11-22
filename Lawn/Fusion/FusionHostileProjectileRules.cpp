#include "FusionHostileProjectileRules.h"
#include "FusionBasePlantRules.h"
#include "FusionCatalog.h"
#include "FusionShooterBatchRules.h"
#include "SpecialZombie269Rules.h"
#include <cmath>

namespace LawnFusion
{

bool FusionSuperCherryProjectileMetadataValid(uint16_t kind,int damage,uint8_t way,
    bool hostile,ProjectileType type,ProjectileMotion motion)
{
    return kind==3&&type==PROJECTILE_PEA&&(hostile?
        damage==300&&((way==9&&motion==MOTION_BACKWARDS)||(way==0&&motion==MOTION_STRAIGHT)):
        (damage==300||damage==1000||damage==2000)&&way==0&&motion==MOTION_STRAIGHT);
}
bool FusionSuperCherryPlantShort(uint16_t kind)
{
    // Source isShort, not FlyingPlants. Audited LoadPlant slots and all281
    // serialized prefabs are retained in paper-hostile3-primary.json.
    switch(kind)
    {
    case 4:case 6:case 12:case 15:case 17:case 19:case 27:case 29:
    case 906:case 908:case 925:case 929:case 936:case 1007:case 1009:case 1010:
    case 1015:case 1018:case 1019:case 1021:case 1022:case 1031:case 1035:case 1036:
    case 1044:case 1050:case 1056:case 1060:case 1061:case 1062:case 1063:case 1064:
    case 1065:case 1074:case 1075:case 1076:case 1077:case 1078:case 1083:case 1086:
    case 1095:case 1101:case 1112:case 1114:case 1125:case 1130:case 1133:case 1136:
    case 1137:case 1145:case 1146:case 1152:case 1153:case 1155:case 1156:case 1162:
    case 1163:case 1170:case 1180:case 1183:case 1184:case 1190:return true;
    default:return false;
    }
}
bool FusionSuperCherryPlantPumpkin(uint16_t kind)
{
    switch(kind)
    {
    case 24:case 911:case 922:case 935:case 1087:case 1088:case 1089:case 1091:
    case 1092:case 1097:case 1098:case 1099:case 1110:case 1164:case 1169:case 1190:return true;
    default:return false;
    }
}
bool FusionSuperCherryPlantSingleTarget(uint16_t kind)
{
    // Source Pumpkin/Nut tags, BigNut, and explicit1195. Ordinary D2;
    // the source GameMode5 exception is not admitted by the current native mode.
    bool caltrop=false,nut=false;int level=0;
    return (TryGetSpecialZombie269SourcePlantTraits(kind,caltrop,nut,level)&&nut)||
        FusionSuperCherryPlantPumpkin(kind)||kind==251||kind==255||kind==1195;
}
bool FusionSuperCherryPlantCollides(uint16_t kind,int shotRow,int plantRow,bool enabled)
{return FusionFindPlant(kind)&&enabled&&shotRow>=0&&shotRow<6&&shotRow==plantRow&&kind!=254&&!FusionSuperCherryPlantShort(kind);}
bool FusionSuperCherryCollateralCell(int dc,int dr)
{return (dc==0&&(dr==1||dr==-1))||(dr==0&&(dc==1||dc==-1));}
FusionSuperCherryPlantPlan PlanFusionSuperCherryPlantHit(uint16_t kind,bool cover,bool graph)
{
    // BombCherry.PlantTakeDamage43FDB0; actual normalD2, EveBalanced absent.
    if(!FusionFindPlant(kind))return {};
    if(kind==12||kind==921||kind==1095||kind==241||kind==912||kind==1082||
        kind==17||kind==906||kind==1060||(kind>=1061&&kind<=1064)||kind==1074||
        kind==1075||kind==1155||kind==1180||kind==1196)return {};
    if(kind==903||kind==922||kind==1003||kind==1164||kind==1169)
        return {FusionSuperCherryPlantEffect::Heal,200};
    if(kind==911&&graph)return {FusionSuperCherryPlantEffect::Damage,500};
    if(cover)return {};
    switch(kind)
    {
    case 935:case 1020:case 1028:case 1029:case 1096:case 1097:case 1129:
    case 1151:case 1152:case 1159:case 1160:return {FusionSuperCherryPlantEffect::Damage,500};
    default:return {FusionSuperCherryPlantEffect::Damage,1000};
    }
}
bool TryPlanFusionSuperCherryRebound(uint16_t kind,bool running,bool pot,int& damage)
{
    if(!running||(kind!=931&&kind!=1131))return false;
    damage=pot?2000:1000;return true;
}

bool FusionCatapultCherry343MetadataValid(uint16_t k,int damage,uint8_t way,bool hostile,ProjectileType type,ProjectileMotion motion)
{return k==3&&damage==60&&way==13&&hostile&&type==PROJECTILE_PEA&&motion==MOTION_LOBBED;}
bool FusionHostilePlanarShotValid(const FusionHostilePlanarShot& s)
{
    return std::isfinite(s.x)&&std::isfinite(s.y)&&std::isfinite(s.velocityX)&&
        std::isfinite(s.velocityY)&&std::isfinite(s.gravityY);
}
bool TryMakeFusionHostileBasketballShot(double x,double y,double tx,double ty,FusionHostilePlanarShot& out,double pixelsY)
{
    if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(tx)||!std::isfinite(ty)||!std::isfinite(pixelsY)||pixelsY<=0)return false;
    FusionHostilePlanarShot s;
    s.x=x;s.y=y-pixelsY;
    s.gravityY=14.715*pixelsY*.0001;
    s.velocityX=(tx-x)/200.;
    s.velocityY=(ty-y)/200.-s.gravityY*100.;
    if(!FusionHostilePlanarShotValid(s))return false;
    out=s;return true;
}
bool AdvanceFusionHostilePlanarShot(FusionHostilePlanarShot& s)
{
    if(!FusionHostilePlanarShotValid(s))return false;
    // Source ThrowBulletUpdate424620 changes velocity before Rigidbody motion.
    s.velocityY+=s.gravityY;s.x+=s.velocityX;s.y+=s.velocityY;
    return FusionHostilePlanarShotValid(s);
}
bool TryMakeFusionHostileProjectileRect(uint16_t kind,double x,double y,double py,FusionRecipientRect& r)
{
    if(kind==3){FusionRecipientRect raw;if(!TryMakeFusionShooterBatchProjectileRect(3,x,y,raw)||!std::isfinite(py)||py<=0)return false;raw.y=y+(raw.y-y)*py/kFusionBasePixelsY;raw.height*=py/kFusionBasePixelsY;r=raw;return true;}
    if((kind!=58&&kind!=59)||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(py)||py<=0)return false;
    // Enabled root BoxCollider2D dimensions, including source rootScale .4.
    const double w=kind==58?.80700292:.88958872,h=kind==58?.52539464:.680243;
    const double ox=kind==58?-.0048348428:-.0013936996,oy=kind==58?-.028471268:-.030191792;
    r={x+(ox-w*.5)*kFusionBasePixelsX,y-(oy+h*.5)*py,w*kFusionBasePixelsX,h*py};
    return true;
}
}
