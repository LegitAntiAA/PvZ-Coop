// Include INSIDE NativeWire's anonymous namespace after Reader/Writer.
// Root's Reader.Bool rejects nonbinary bytes. Frame decode remains transactional
// in the existing outer candidate/cursor commit, not in individual Fields calls.
template<class IO> void Fields(IO& io,LawnNet::NetZombieAnimState& v)
{
    io.Bool(v.mPresent);io.Bool(v.mDead);io.U16(v.mFrameStart);io.U16(v.mFrameCount);
    io.F32(v.mAnimTime);io.F32(v.mAnimRate);io.U8(v.mLoopType);io.U8(v.mLoopCount);
}
template<class IO> void Fields(IO& io,FusionSourceGraph313View& v)
{
    io.U8(v.state);io.U8(v.outgoing);io.F32(v.outgoingSeconds);
    io.F32(v.fadeElapsed);io.F32(v.fadeDuration);io.Bool(v.fadeNormalized);
}
template<class IO> void Fields(IO& io,NativeSourceGraph313Snapshot& v)
{
    io.U8(v.layerCount);if(v.layerCount<1||v.layerCount>v.layers.size()){io.valid=false;return;}
    // Fixed bounded storage is serialized in full. Unused slots MUST be zero;
    // validation rejects covert stale state instead of silently dropping it.
    for(auto& layer:v.layers)Fields(io,layer);
    for(auto& aux:v.auxiliary)Fields(io,aux);
}
template<class IO> void Fields(IO& io,FusionWaterZombie313View& v)
{
    uint8_t phase=uint8_t(v.phase),outgoing=uint8_t(v.outgoing);io.U8(phase);io.U8(outgoing);
    v.phase=FusionWaterZombie313Phase(phase);v.outgoing=FusionWaterZombie313Phase(outgoing);
    io.U8(v.sourceStatus);io.U8(v.driverStage);io.Bool(v.facingRight);io.Bool(v.colliderEnabled);io.Bool(v.auxiliaryActive);
    io.F32(v.driverSpeed);io.F32(v.outgoingSourceSeconds);io.F32(v.transitionElapsed);io.F32(v.transitionTotal);
    io.F32(v.auxiliaryTransitionElapsed);io.F32(v.auxiliaryTransitionTotal);Fields(io,v.auxiliary);
    Fields(io,v.shoot343);Fields(io,v.shootClock343);io.Bool(v.gatlingReady343);
}
template<class IO> void Fields(IO& io,FusionExtendedZombie313View& v)
{
    uint8_t status=uint8_t(v.sourceStatus);io.U8(status);if(status>127){io.valid=false;return;}v.sourceStatus=int8_t(status);
    io.U8(v.bodyStage);io.U8(v.weapon);io.U8(v.machineType);io.U8(v.uniqueType);io.U8(v.firstArmorStage);
    io.Bool(v.carriedImpRemoved);io.Bool(v.ballRemoved);io.Bool(v.bombPresent);io.Bool(v.fallen);io.Bool(v.visible);io.Bool(v.adjustingImp);
    io.Bool(v.beforeDying);io.Bool(v.rootFlip);io.F32(v.impVerticalSpeed);io.F32(v.impMaximumVx);io.F32(v.uniqueSpeed);
    io.F32(v.attributeCountdown);io.F32(v.throwTime);io.Bool(v.isThrow);io.I32(v.attackDamage);io.U8(v.secondArmorStage343);io.Bool(v.quickMove208);io.Bool(v.squashLaunched363);
}
template<class IO> void Fields(IO& io,FusionBungee313BView& v)
{
    io.U8(v.status);io.I32(v.targetColumn);io.I32(v.targetRow);io.F32(v.targetX);io.F32(v.targetY);
    io.F32(v.cordScaleX);io.F32(v.cordScaleY);io.F32(v.cordScaleZ);io.F32(v.markerY);
    io.Bool(v.setZombie);io.Bool(v.targetLanded);io.Bool(v.markerVisible);
}
template<class IO> void Fields(IO& io,FusionHeavyZombie313BView& v)
{
    io.U8(v.status);io.U8(v.ammo);io.U8(v.armorStage);io.U8(v.pogoWait);io.U8(v.catapultFlags343);io.U16(v.deliveryKind);
    io.Bool(v.shooting);io.Bool(v.ladderVisible);io.Bool(v.axePresent);io.Bool(v.lookBack);io.Bool(v.caltropDying);
    io.Bool(v.pogoPaused);io.Bool(v.pogoStarted);io.Bool(v.jumped);io.Bool(v.lostJumper);io.Bool(v.shadowEnabled);
    io.F32(v.jumpSpeed);Fields(io,v.bungee);
}
template<class IO> void Fields(IO& io,FusionHeavyGold313BView& v)
{io.U8(v.status);io.U8(v.firstStage);io.U8(v.secondStage);}
template<class IO> void Fields(IO& io,NativeHeavyZombie313Snapshot& v)
{Fields(io,v.actor);Fields(io,v.gold);}
