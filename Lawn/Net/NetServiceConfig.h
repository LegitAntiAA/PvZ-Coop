#ifndef __NETSERVICECONFIG_H__
#define __NETSERVICECONFIG_H__

namespace LawnNet
{
// Operated for every player by the mod owner; never stored in player settings.
inline constexpr const char* NET_ONLINE_SERVICE_ENDPOINT =
	"https://pvz-coop-signaling.rostov20172017.workers.dev";
inline constexpr const char* NET_ONLINE_STUN_SERVER = "stun:stun.cloudflare.com:3478";
}

#endif
