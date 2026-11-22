#include "FusionNativeTravelBuff360.h"
#include "FusionNative.h"
#include "FusionNativeSourceClock313.h"
#include "../Board.h"
#include "../../LawnApp.h"
namespace LawnFusion {namespace {
bool World(Board* b){const auto* c=GetNativeContext(b);return IsNativeSourceWorld313(b)&&c&&(c->adventure.enabled||c->validation)&&b->mApp->mGameMode==GAMEMODE_ADVENTURE&&!b->mApp->NetIsClient()&&!b->mPaused&&b->mApp->mGameScene==SCENE_PLAYING;}
bool Fail(std::string* e,const char* reason){if(e)*e=reason;return false;}
bool Immediate(FusionTravelBuff360Award a){return a.type==FusionTravelBuff360Type::Advanced?(a.index==10||a.index==20||a.index==25):a.index==24;}
}
bool PreflightNativeTravelBuff360(Board* b,const FusionTravelBuff360Ledger& ledger,const NativeTravelBuff360Hooks& h,std::string* e){
 if(!World(b)||!h.prepare||!h.applyImmediate||!h.showAdvice)return Fail(e,"TravelBuff360 authoritative source world/real effect hooks unavailable");
 FusionTravelBuff360Pool pool;if(!BuildFusionTravelBuff360Pool(ledger,false,pool))return Fail(e,"TravelBuff360 canonical ledger invalid");
 for(auto i:pool.advanced)if(!h.prepare(b,{FusionTravelBuff360Type::Advanced,i},e))return false;
 for(auto i:pool.ultimate)if(!h.prepare(b,{FusionTravelBuff360Type::Ultimate,i},e))return false;
 if(e)e->clear();return true;
}
bool UnlockNativeTravelBuff360(Board* b,FusionTravelBuff360Ledger& ledger,float& cooldown,int round,const NativeTravelBuff360Hooks& h,bool& awarded,std::string* e){
 if(!PreflightNativeTravelBuff360(b,ledger,h,e)||round<0||cooldown!=0)return false;FusionTravelBuff360Pool pool;if(!BuildFusionTravelBuff360Pool(ledger,false,pool))return false;
 const int total=int(pool.advanced.size()+pool.ultimate.size());if(!total){awarded=false;return true;}
 // Actual source Random uses Unity's PRNG. Existing native adaptation uses
 // Board's one authoritative generator, with BOTH source draw boundaries.
 const int group=RandRangeInt(0,total-1);const auto& selected=size_t(group)<pool.advanced.size()?pool.advanced:pool.ultimate;
 const int index=RandRangeInt(0,int(selected.size())-1);FusionTravelBuff360Award award;FusionTravelBuff360Ledger next;
 if(!SelectFusionTravelBuff360Award(pool,group,index,award)||!CommitFusionTravelBuff360Award(ledger,award,round,next))return Fail(e,"TravelBuff360 prepared native random draw violated source pool");
 if(award.type==FusionTravelBuff360Type::Advanced){if(Immediate(award))h.applyImmediate(b,award,ledger);ledger=next;}
 else{ledger=next;if(Immediate(award))h.applyImmediate(b,award,ledger);}
 h.showAdvice(b,award,10);cooldown=60;awarded=true;return true;
}
}
