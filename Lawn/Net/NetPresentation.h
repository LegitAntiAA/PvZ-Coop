#ifndef __NETPRESENTATION_H__
#define __NETPRESENTATION_H__
#include <cstdint>

class LawnApp;
class Reanimation;

namespace LawnNet
{
struct NetFusionLimbEvent276;
void NetBroadcastFusionLimb276(LawnApp*, const NetFusionLimbEvent276&);
// Source66/69 reuse the original prize/dancer resources. Source volume stays
// per playback; changing a shared sound's base volume would alter Classic.
bool NetSourceSound284Ready(LawnApp*,uint16_t sourceSound);
bool NetPlaySourceSound284(LawnApp*,uint16_t sourceSound,float volume=.5f);
// Explicitly relay standalone effects whose stock producer is host-only.
// Entity attachments are reconstructed from entity snapshots instead.
void NetBroadcastParticle(LawnApp* theApp, float theX, float theY,
	int theRenderOrder, int theEffect);
void NetBroadcastReanimation(LawnApp* theApp, Reanimation* theReanim,
	float theX, float theY, int theRenderOrder, int theType);
void NetBroadcastFwoosh(LawnApp* theApp, int theRow);
}

#endif
