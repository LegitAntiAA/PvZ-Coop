#include "FusionQuickJackson367Rules.h"
#include <cmath>
namespace LawnFusion {
int FusionQuickJackson367RawDamage(int raw,bool fix,bool done){return raw>0&&!fix&&!done?raw/10:raw;}
bool FusionQuickJackson367Motion(float speed,float dt,float& out){if(!std::isfinite(speed)||speed<0||!std::isfinite(dt)||dt<0)return false;float delta=-speed*10*dt;if(!std::isfinite(delta))return false;out=delta;return true;}
}
