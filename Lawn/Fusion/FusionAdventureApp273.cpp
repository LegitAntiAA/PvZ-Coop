#include "../../LawnApp.h"
#include "FusionNative.h"
#include "FusionCampaign272Rules.h"
#include "../Board.h"
#include "../Net/NetManager.h"
#include "../../SexyAppFramework/Dialog.h"
#include <cstdlib>
#include <cstring>

namespace
{
// Open only after the real host/client source-level acceptance passes.
constexpr bool kSourceAdventurePublicReady273=false;
bool SourceFixture(const LawnApp* app)
{
    const char* value=std::getenv("PVZ_COOP_SMOKE_FUSION_SOURCE273");
    return app->CoopSmokeActive()&&app->mCoopSmokeManifest.mScenario==LawnNet::NETSMOKE_SCENARIO_FUSION_VALIDATION&&
        value&&std::strcmp(value,"1")==0;
}
}

bool LawnApp::IsFusionSourceAdventure() const
{
    return GetContentMode()==LawnFusion::ContentMode::Fusion&&mSourceAdventure.enabled&&
        mGameMode==GAMEMODE_ADVENTURE&&!mFusionRuntime;
}

bool LawnApp::StartFusionSourceAdventure(int level,bool disableTutorial)
{
    using namespace LawnFusion;
    if(NetIsClient()||GetContentMode()!=ContentMode::Fusion||mStartingSourceAdventure)return false;
    const auto profile=mNetManager?mNetManager->mConfig.mFusionSourceProfile:FusionAdventureProfile273{};
    if(!level)level=profile.frontierLevel;
    if((!kSourceAdventurePublicReady273&&!SourceFixture(this))||level<1||level>3||profile.cycle!=1||
        !CanEnterFusionAdventureLevel273(profile,level))
    {
        if(!mGameSelector&&!mBoard)ShowGameSelector();
        DoDialog(DIALOG_INFO,true,_S("Fusion"),_S("Этот уровень кампании Fusion ещё не готов."),_S("[OK_LABEL]"),Sexy::Dialog::BUTTONS_FOOTER);
        return false;
    }
    FusionAdventureSession273 session;session.enabled=true;session.profile=profile;
    FusionSourceNativeProjection272 projection;int waves=0;
    if(!BeginFusionAdventureRun273(profile,level,disableTutorial,session.run)||
        !TryProjectFusionSourceAdventure272(level,projection)||
        !TryGetFusionSourceAdventureWaves272(level,int(session.run.cycle),waves))return false;
    session.maximumWaves=uint16_t(waves);session.countdown=15;
    if(!FusionAdventureSessionValid273(session))return false;
    KillGameSelector();
    NetCloseTransientScreens();
    mSourceAdventure=session;mStartingSourceAdventure=true;
    PreNewGame(GAMEMODE_ADVENTURE,false,projection.layoutLevel);
    mStartingSourceAdventure=false;
    const auto* context=GetNativeContext(mBoard);
    const bool started=context&&context->adventure.enabled&&context->adventure.run.level==level;
    if(!started)mSourceAdventure={};
    return started;
}

void LawnApp::ProcessFusionSourceAdventureTransition()
{
    using namespace LawnFusion;
    if(NetIsClient()||mUpdateAppDepth>1||!mBoard||mGameScene!=SCENE_PLAYING)return;
    auto* context=GetNativeContext(mBoard);
    if(!context||!context->adventure.enabled||!context->adventure.run.transitionReady)return;
    FusionAdventureTransition273 transition;
    if(!TakeFusionAdventureTransition273(context->adventure.profile,context->adventure.run,transition))return;
    LawnNet::NetTrace("Fusion source Adventure273 transition old=%u next=%u cycle=%u replay=%d",
        context->adventure.run.level,transition.nextLevel,transition.nextCycle,transition.returnToSelector?1:0);
    if(transition.returnToSelector||transition.nextLevel>3||transition.nextCycle!=1)
    {
        DoBackToMain();return;
    }
    StartFusionSourceAdventure(transition.nextLevel,false);
}
