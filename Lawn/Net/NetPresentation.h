#ifndef __NETPRESENTATION_H__
#define __NETPRESENTATION_H__

class LawnApp;
class Reanimation;

namespace LawnNet
{
// Explicitly relay standalone effects whose stock producer is host-only.
// Entity attachments are reconstructed from entity snapshots instead.
void NetBroadcastParticle(LawnApp* theApp, float theX, float theY,
	int theRenderOrder, int theEffect);
void NetBroadcastReanimation(LawnApp* theApp, Reanimation* theReanim,
	float theX, float theY, int theRenderOrder, int theType);
}

#endif
