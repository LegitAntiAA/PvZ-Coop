#pragma once

#include "../ConstEnums.h"
#include "../SexyAppFramework/Graphics.h"

using namespace Sexy;

class LawnApp;
class Board;

class GameObject
{
public:
	LawnApp*                        mApp;
	Board*                          mBoard;
    int                             mX;
    int                             mY;
    int                             mWidth;
    int                             mHeight;
    bool                            mVisible;
    int                             mRow;
    int                             mRenderOrder;
    unsigned short                  mNetId;         // co-op: stable id assigned by the host
    bool                            mNetGhost;      // co-op: entity is a client-side mirror (no local simulation)
    float                           mNetTargetX;    // co-op: authoritative target position (interpolation)
    float                           mNetTargetY;

public:
    /*inline*/                      GameObject();
    /*inline*/ bool                 BeginDraw(Graphics* g);
    /*inline*/ void                 EndDraw(Graphics* g);
    /*inline*/ void                 MakeParentGraphicsFrame(Graphics* g);
};
