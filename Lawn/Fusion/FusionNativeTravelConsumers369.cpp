#include "FusionNativeTravelConsumers369.h"
#include "FusionNativeTravelBuff369.h"
#include "FusionNative.h"
#include "FusionNativeWorldActors269.h"
#include "../Board.h"
#include "../Plant.h"
#include "../../LawnApp.h"
namespace LawnFusion {
bool HasNativeTravelAdvanced369(const Board* b,unsigned index)
{const auto* l=GetNativeTravelBuff369(b);return l&&index<48&&HasFusionTravelAdvanced360(*l,int(index));}
bool HasNativeTravelUltimate369(const Board* b,unsigned index)
{const auto* l=GetNativeTravelBuff369(b);return l&&index<26&&HasFusionTravelUltimate360(*l,int(index));}
bool NativeTravelConnected369(const Plant* p)
{const auto* d=GetNativePlantData(p);const auto* s=d&&d->kind==937?GetNativeWorld269PlantState(p):nullptr;return s&&s->connected;}
int GetNativeTravelPlantAttack369(const Plant* p,int base)
{
 if(!p||!GetNativePlantData(p))return base;
 int result=base;TryFusionTravelPlantAttack369(base,HasNativeTravelAdvanced369(p->mBoard,10),
   [&]{const auto* state=GetNativeWorld269PlantState(p);return state&&state->connected&&state->connectedAttackBonus369;}(),result);return result;
}
int GetNativeTravelLight369(const Board* b){return FusionTravelLight369(HasNativeTravelAdvanced369(b,20));}
int GetNativeTravelCardStep369(const Board* b){return FusionTravelCardStep369(HasNativeTravelAdvanced369(b,27));}
float GetNativeTravelCardSeconds369(const Board* b,uint16_t kind,float ordinary)
{return FusionTravelImitaterSeconds369(kind,ordinary,HasNativeTravelAdvanced369(b,32),b&&b->mApp&&b->mApp->mGameScene==SCENE_PLAYING);}
int GetNativeTravelHammerStep369(const Board* b){return FusionTravelHammerStep369(HasNativeTravelAdvanced369(b,26));}
bool CreditNativeTravelSun369(Board* b,int count,int multiplier)
{
 if(!b||!b->mApp||b->mApp->mBoard!=b||b->mApp->NetIsClient()||!IsNativeFusionBoard(b)||!GetNativeContext(b))return false;
 int next=0;if(!TryFusionTravelSun369(b->mSunMoney,count,multiplier,HasNativeTravelAdvanced369(b,25),next))return false;
 b->mSunMoney=next;return true;
}
}
