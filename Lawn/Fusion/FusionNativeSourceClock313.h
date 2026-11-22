#pragma once
#include <cstdint>
#include <vector>
class Board;class Reanimation;class Zombie;class LawnApp;
namespace LawnFusion
{
struct NativeSourceGraph313State;
// Actual ordinary Day/Night/Pool/Fog geometry. Admission of a particular
// controller, row and resource closure remains the family's responsibility.
bool IsNativeSourceWorld313(const Board*);
NativeSourceGraph313State* EditNativeSourceGraph313(Zombie*,bool create=false);
const NativeSourceGraph313State* GetNativeSourceGraph313(const Zombie*);
bool IsNativeSourceZombieInitializing313(const Zombie*);
bool ReadNativeSourceStatusLevel313(const Zombie*,int&);
class NativeSourceZombieInitialization313
{
public:
    explicit NativeSourceZombieInitialization313(Zombie*);
    ~NativeSourceZombieInitialization313();
    void Commit() { committed=true; }
    NativeSourceZombieInitialization313(const NativeSourceZombieInitialization313&)=delete;
    NativeSourceZombieInitialization313& operator=(const NativeSourceZombieInitialization313&)=delete;
private:
    Board* board=nullptr;LawnApp* app=nullptr;uint32_t uid=0,epoch=0;bool inserted=false,committed=false;
};
bool RegisterNativeSourceGraphClock313(Reanimation*,bool owned);
bool NativeSourceGraph313OwnsClock(const Reanimation*);
void PruneNativeSourceGraph313Clocks(Board*);
// Ascending actual host birth order; callers may reverse this for source
// zombieList reverse iteration. Opaque network IDs/full UID magnitude are not order.
bool GetNativeSourceZombieBirthOrder313(Board*,std::vector<uint32_t>&);
}
