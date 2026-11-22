// Bounded owning presentation data; included inside NativeWire's namespace.
void Fields(Writer& io,std::string& v)
{
    if(v.size()>512){io.valid=false;return;}auto n=uint16_t(v.size());io.U16(n);
    io.b.WriteBytes(v.data(),n);
}
void Fields(Reader& io,std::string& v)
{
    uint16_t n=0;io.U16(n);if(!io.valid||n>512||n>io.b.Remaining()){io.valid=false;return;}
    v.resize(n);if(n&&!io.b.ReadBytes(v.data(),n))io.valid=false;
}
template<class T>void CapturedList314(Writer&,std::vector<T>&,size_t maximum=2048);
template<class T>void CapturedList314(Reader&,std::vector<T>&,size_t maximum=2048);
template<class IO>void Fields(IO& io,FusionAnimationDrawOptions::BaseBlend& v)
{Fields(io,v.outgoingState);io.F32(v.outgoingElapsedSeconds);io.F32(v.incomingWeight);}
template<class IO,class T>void CapturedOptional314(IO& io,std::optional<T>& v)
{
    bool present=v.has_value();io.Bool(present);if(!io.valid)return;
    if(present){if(!v)v.emplace();Fields(io,*v);}else v.reset();
}
template<class IO>void Fields(IO& io,FusionAnimationDrawOptions::ExtraState& v)
{Fields(io,v.stateName);io.F32(v.elapsedSeconds);io.I32(v.layer);io.F32(v.weight);CapturedOptional314(io,v.baseBlend);}
template<class IO>void Fields(IO& io,FusionAnimationDrawOptions::SpriteEnabled& v)
{Fields(io,v.nodePath);io.Bool(v.enabled);}
template<class IO>void Fields(IO& io,FusionAnimationDrawOptions::SpriteSorting& v)
{Fields(io,v.nodePath);io.I32(v.layer);io.I32(v.order);}
template<class IO>void Fields(IO& io,FusionAnimationDrawOptions::SortingGroup& v)
{Fields(io,v.nodePath);io.I32(v.layer);io.I32(v.order);}
template<class IO>void Fields(IO& io,FusionAnimationDrawOptions::MaskSorting& v)
{Fields(io,v.nodePath);io.I32(v.backLayer);io.I32(v.backOrder);io.I32(v.frontLayer);io.I32(v.frontOrder);}
template<class IO>void Fields(IO& io,FusionAnimationDrawOptions::NodeActivity& v)
{Fields(io,v.nodePath);io.Bool(v.active);}
template<class IO>void Fields(IO& io,FusionAnimationDrawOptions::NodeRotation& v)
{Fields(io,v.nodePath);io.F32(v.radiansZ);io.Bool(v.replace);}
template<class IO>void Fields(IO& io,FusionAnimationDrawOptions::NodeWorldPosition& v)
{Fields(io,v.nodePath);io.F32(v.x);io.F32(v.y);}
template<class IO>void Fields(IO& io,FusionAnimationDrawOptions::SpriteReplacement& v)
{Fields(io,v.nodePath);Fields(io,v.spriteGuid);io.F32(v.scaleX);io.F32(v.scaleY);io.F32(v.scaleZ);io.Bool(v.overrideScale);}
template<class IO>void Fields(IO& io,CapturedSourceOptions314& v)
{
    io.Bool(v.rootFlipX);for(auto& c:v.colorMultiplierRGBA)io.I32(c);io.F32(v.attackSpeed);Fields(io,v.spritePathPrefix);
    CapturedList314(io,v.spriteEnabledOverrides);CapturedList314(io,v.spriteSortingOverrides);
    CapturedList314(io,v.sortingGroupOverrides);CapturedList314(io,v.maskSortingOverrides);
    CapturedList314(io,v.hiddenNodePaths);CapturedList314(io,v.nodeActivityOverrides);
    CapturedList314(io,v.nodeRotations);CapturedList314(io,v.nodeWorldPositions);
    CapturedList314(io,v.extraStates,128);CapturedList314(io,v.spriteReplacements);CapturedOptional314(io,v.baseBlend);
}
template<class IO>void Fields(IO& io,FrozenSourcePose314& v)
{Fields(io,v.baseState);Fields(io,v.actionState);io.F32(v.baseSeconds);io.F32(v.actionSeconds);io.F32(v.speed);Fields(io,v.options);}
template<class IO>void Fields(IO& io,CapturedNativeTrack314& v)
{
    Fields(io,v.name);io.I32(v.imageResource);io.I32(v.renderGroup);io.I32(v.blendCounter);io.I32(v.blendTime);
    for(auto& x:v.blendTransform)io.F32(x);for(auto& x:v.shake)io.F32(x);for(auto& x:v.color)io.I32(x);
    io.Bool(v.ignoreClip);io.Bool(v.truncate);io.Bool(v.ignoreColor);io.Bool(v.ignoreAdditive);io.Bool(v.inBack);io.Bool(v.forceHidden);
}
template<class IO>void Fields(IO& io,CapturedNativeReanim314& v)
{
    io.U16(v.type);Fields(io,v.clock);for(auto& x:v.relative)io.F32(x);
    for(auto& x:v.color)io.I32(x);for(auto& x:v.additive)io.I32(x);for(auto& x:v.overlay)io.I32(x);
    io.Bool(v.additiveEnabled);io.Bool(v.overlayEnabled);io.I32(v.basePose);io.I32(v.filter);
    io.F32(v.offsetX);io.F32(v.offsetY);CapturedList314(io,v.tracks);
}
template<class IO>void Fields(IO& io,CapturedNativeEdge314& v)
{io.U16(v.parent);io.U16(v.child);io.U16(v.track);for(auto& x:v.offset)io.F32(x);io.Bool(v.hideWithParent);io.Bool(v.noColor);}
template<class IO>void Fields(IO& io,CapturedSourcePlant314View& v)
{
    io.U32(v.epoch);io.U32(v.captureToken);io.U32(v.ownerToken);io.U32(v.plantToken);
    io.U16(v.ownerId);io.U16(v.plantId);io.U16(v.kind);io.U16(v.modelKind);io.Bool(v.nativeArtwork);io.Bool(v.retainedActor);
    for(auto& x:v.localPlacement)io.F32(x);Fields(io,v.pose);
    for(auto& x:v.roles){uint16_t role=uint16_t(x);io.U16(role);x=role==65535?-1:role<=32767?int16_t(role):-2;}
    CapturedList314(io,v.reanimations,128);CapturedList314(io,v.edges);
}
template<class T>void CapturedList314(Writer& io,std::vector<T>& xs,size_t maximum)
{
    if(xs.size()>maximum){io.valid=false;return;}auto n=uint16_t(xs.size());io.U16(n);
    for(auto& x:xs)Fields(io,x);
}
template<class T>void CapturedList314(Reader& io,std::vector<T>& xs,size_t maximum)
{
    uint16_t n=0;io.U16(n);if(!io.valid||n>maximum){io.valid=false;return;}
    xs.resize(n);for(auto& x:xs){if(!io.valid)break;Fields(io,x);}
}
