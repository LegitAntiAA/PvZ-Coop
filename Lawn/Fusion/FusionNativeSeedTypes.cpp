#include "FusionNative.h"
#include "FusionDefenderRules.h"
#include "FusionBatchActionRules.h"

namespace LawnFusion
{
SeedType GetNativeSeedForPlant(FusionPlantId kind)
{
    switch(kind)
    {
    case 900:return SEED_HYPNOSHROOM;
    case 938:return SEED_NONE; // Actual boss dependencies remain gated.
    case 253:return SEED_SUNFLOWER;
    case 937:return SEED_UMBRELLA;
    case 909:return SEED_STARFRUIT;
    case 926:return SEED_CATTAIL;
    case 918:return SEED_PEASHOOTER;
    case 1056:return SEED_TANGLEKELP;
    case 243:return SEED_PEASHOOTER;
    case 248:return SEED_SQUASH;
    case 256:return SEED_IMITATER;
    case 241:return SEED_INSTANT_COFFEE;
    case 242:case 254:return SEED_PEASHOOTER;
    case 249:return SEED_STARFRUIT;
    case 34:return SEED_PLANTERN;
    case 1174:return SEED_CHOMPER;
    case 1151:return SEED_WALLNUT;
    case 244:case 250:return SEED_CHERRYBOMB;
    case 245:return SEED_IMITATER;
    case 1175:return SEED_SEASHROOM;
    case 255:return SEED_WALLNUT;
    case 917:case 1041:return SEED_HYPNOSHROOM;
    case 928:return SEED_DOOMSHROOM;
    case 1147:case 905:return SEED_WALLNUT;
    case 1150:case 1157:case 1166:return SEED_MAGNETSHROOM;
    case 1164:case 911:case 922:case 1110:case 1169:return SEED_PUMPKINSHELL;
    case 912:return SEED_BLOVER;
    case 1178:return SEED_TORCHWOOD;
    case 1082:case 1084:case 920:return SEED_BLOVER;
    case 1093:case 1094:case 1095:case 1104:return SEED_STARFRUIT;
    case 910:return SEED_GLOOMSHROOM;
    case 930:return SEED_SCAREDYSHROOM;
    case 1026:case 924:return SEED_PUFFSHROOM;
    case 934:return SEED_CABBAGEPULT;
    case 25:case 1100:case 1101:return SEED_MAGNETSHROOM;
    case 1102:return SEED_BLOVER;
    case 1092:case 1097:case 1098:case 1099:case 935:return SEED_PUMPKINSHELL;
    case 1154:return SEED_SUNFLOWER;
    case 15:case 1049:case 1050:case 1051:case 1066:case 1156:return SEED_TANGLEKELP;
    case 252:case 1067:case 1068:case 1069:return SEED_CATTAIL;
    case 1076:return SEED_CACTUS;
    case 1083:case 1085:return SEED_STARFRUIT;
    case 1120:case 1122:case 1123:case 1132:case 915:return SEED_COBCANNON;
    case 1139:return SEED_KERNELPULT;
    case 1142:case 914:return SEED_MELONPULT;
    case 1013:case 903:case 921:return SEED_CHOMPER;
    case 33:case 1177:return SEED_PEASHOOTER;
    case 1129:case 1162:return SEED_WALLNUT;
    default:break;
    }
    if(kind==12)return SEED_LILYPAD;
    if (const auto* defender = FindFusionDefenderProfile(kind))
    {
        switch (defender->family)
        {
        case FusionDefenderFamily::WallNut: return SEED_WALLNUT;
        case FusionDefenderFamily::TallNut: return SEED_TALLNUT;
        case FusionDefenderFamily::Pot: return SEED_FLOWERPOT;
        case FusionDefenderFamily::Pumpkin: return SEED_PUMPKINSHELL;
        }
    }
    if (const auto* action = FindFusionBatchActionProfile(kind))
    {
        switch (action->family)
        {
        case FusionBatchActionFamily::Hypno: return SEED_HYPNOSHROOM;
        case FusionBatchActionFamily::Doom: return SEED_DOOMSHROOM;
        case FusionBatchActionFamily::Jalapeno: return SEED_JALAPENO;
        case FusionBatchActionFamily::Squash: return SEED_SQUASH;
        case FusionBatchActionFamily::Caltrop: return SEED_SPIKEWEED;
        case FusionBatchActionFamily::Threepeater: return SEED_THREEPEATER;
        case FusionBatchActionFamily::Scaredy:
        case FusionBatchActionFamily::AdaptiveFume: return SEED_SCAREDYSHROOM;
        }
    }
    switch (kind)
    {
    case 927:return SEED_PEASHOOTER;
    case 1007:return SEED_POTATOMINE;
    case 1172:case 1158:case 904:return SEED_FUMESHROOM;
    case 919:case 1047:return SEED_THREEPEATER;
    case 902:case 932:return SEED_TORCHWOOD;
    case 1103:return SEED_CACTUS;
    case 19:return SEED_SEASHROOM;
    case 1078:return SEED_SEASHROOM;
    case 1077:return SEED_SUNSHROOM;
    case 906:return SEED_SPIKEROCK;
    case 931:return SEED_WALLNUT;
    case 913:case 925:return SEED_TALLNUT;
    case 936:return SEED_FLOWERPOT;
    case 1181:case 1176:case 1086:return SEED_BLOVER;
    case 30:case 916:case 923:case 1148:case 1115:case 1127:case 1128:case 1131:case 1143:case 1144:case 1185:case 1186:return SEED_UMBRELLA;
    case 0:
    case 901:
    case 907:
    case 933:
    case 1005:
    case 1001:
    case 1004:
    case 1008:
    case 1017:
    case 1018:
    case 1019:
    case 1030:
    case 1032:
    case 1065:
    case 1090:
    case 1108:
    case 1153:
    case 908:
    case 1034:
    case 1035:
    case 1105:
    case 1106:
    case 1020:
    case 1152:
    case 1107:
    case 1109:
    case 1161:
    case 1165:
    case 1167:
    case 1168:
    case 1096:
    case 1183:
    case 1191:
    case 929:
    case 1000: return SEED_PEASHOOTER;
    case 1:
    case 1188:
    case 1187: return SEED_SUNFLOWER;
    case 1033: return SEED_TWINSUNFLOWER;
    case 2: return SEED_CHERRYBOMB;
    case 3:
    case 1003:
    case 1010:
    case 1162:
    case 1021:
    case 1006: return SEED_WALLNUT;
    case 4: return SEED_POTATOMINE;
    case 1009:
    case 1163: return SEED_POTATOMINE;
    case 1002: return SEED_CHERRYBOMB;
    case 5:
    case 1011:
    case 1015:
    case 1012:
    case 1014:
    case 1192:
    case 1016: return SEED_CHOMPER;
    case 6:
    case 1170:
    case 1036:
    case 1044: return SEED_PUFFSHROOM;
    case 7:
    case 1159:
    case 1160:
    case 1023:
    case 1037:
    case 1043:
    case 1046: return SEED_FUMESHROOM;
    case 10: return SEED_ICESHROOM;
    case 18:
    case 1052:
    case 1053:
    case 1055:
    case 1059:
    case 1182: return SEED_TORCHWOOD;
    case 1061: return SEED_SPIKEWEED;
    case 20: return SEED_PLANTERN;
    case 22:
    case 1080: return SEED_BLOVER;
    case 1091: return SEED_PUMPKINSHELL;
    case 1173:
    case 1179: return SEED_JALAPENO;
    case 21:
    case 1079: return SEED_CACTUS;
    case 1081: return SEED_STARFRUIT;
    case 1031: return SEED_SUNSHROOM;
    case 1060:case 1074:case 1075:return SEED_SPIKEROCK;
    case 1070:case 1071:case 1072:return SEED_GLOOMSHROOM;
    case 23: return SEED_STARFRUIT;
    case 29: return SEED_GARLIC;
    case 26:
    case 1118:
    case 1135:
    case 1134:
    case 1111: return SEED_CABBAGEPULT;
    case 28:
    case 1117:
    case 1113:
    case 1138:
    case 1121: return SEED_KERNELPULT;
    case 32:
    case 1119:
    case 1126:
    case 1149:
    case 1141:
    case 1124: return SEED_MELONPULT;
    case 1116: return SEED_WINTERMELON;
    case 31:
    case 1140: return SEED_MARIGOLD;
    default: return SEED_NONE;
    }
}

bool IsNativeBasePlantKind(FusionPlantId kind)
{
    return kind==250||kind==243||kind==248||kind==256||kind==241||kind==244||kind==245||kind<=15||kind==25 || kind == 33 || kind == 16 || kind == 17 || kind == 18 || kind == 19 || kind == 30 ||
        kind == 20 || kind == 21 || kind == 22 || kind == 23 || kind == 24 || kind == 26 || kind == 27 || kind == 28 || kind == 29 || kind == 31 || kind == 32;
}
}
