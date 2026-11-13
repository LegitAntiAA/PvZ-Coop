#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Board.h"
#include "../SeedPacket.h"
#include "../System/PlayerInfo.h"
#include "../Widget/GameSelector.h"
#include "../Widget/SeedChooserScreen.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include <cmath>
#include <cstdlib>
#include <string>

namespace
{
struct CommunicationsSmoke
{
    int stage = 0, captured = -1, diagnosticStage = -1;
    bool prepared = false, issued = false;
    uint32_t tick = 0, lastAck = 0, diagnosticTick = 0;
    uint16_t packet = 0;
    bool pingRendered = false;
    uint32_t pingDrawTick = 0;
    std::shared_ptr<SDL_Surface> pingBeforeFrame;
} s;

bool Motion(LawnApp* app, float x, float y)
{
    SDL_Event e{}; e.type = SDL_EVENT_MOUSE_MOTION;
    e.motion.windowID = SDL_GetWindowID(app->mSDLWindow); e.motion.x = x; e.motion.y = y;
    return SDL_PushEvent(&e);
}

bool Click(LawnApp* app, uint8_t button, float x, float y)
{
    if (!Motion(app,x,y)) return false;
    for (bool down : {true,false})
    {
        SDL_Event e{}; e.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        e.button.windowID = SDL_GetWindowID(app->mSDLWindow); e.button.button = button;
        e.button.down = down; e.button.clicks = 1; e.button.x = x; e.button.y = y;
        if (!SDL_PushEvent(&e)) return false;
    }
    return true;
}

bool Saw(LawnApp* app, int slot, const std::string& text)
{
    for (const auto& line : app->mNetManager->mChatLines)
        if (line.mSlot == slot && line.mText == text) return true;
    return false;
}

bool Checkpoint(LawnApp* app, const char* tag)
{
    if (!s.tick) s.tick = GetTickCount();
    if (GetTickCount() - s.tick < 150U) return false;
    if (s.captured != s.stage)
    {
        if (!app->CoopSmokeCaptureFrame((std::string("communications_") + tag).c_str())) return false;
        s.captured = s.stage;
        LawnNet::NetTrace("SMOKE_COMMUNICATIONS_FRAME role=%s step=%s capture=1",
            app->NetIsHost() ? "host" : "client",tag);
    }
    const std::string ack = std::string("COMM_ACK_") + tag;
    if (!s.lastAck || GetTickCount() - s.lastAck >= 800U)
    { s.lastAck = GetTickCount(); app->mNetManager->SendChat(ack); }
    // A client's outgoing chat can be rate-limited by the host. Seeing the
    // peer's ACK alone must not let it abandon an ACK the host never accepted.
    if (!Saw(app,app->mNetManager->GetMySlot(),ack) || !Saw(app,app->NetIsHost() ? 1 : 0,ack)) return false;
    ++s.stage; s.tick = s.lastAck = 0; s.issued = false;
    return true;
}

bool PointerInside(const LawnNet::NetRemoteCursor& cursor, const Rect& rect)
{
    return cursor.mValid && rect.Contains(cursor.mX,cursor.mY) &&
        rect.Contains((int)cursor.mRenderX,(int)cursor.mRenderY) &&
        std::abs(cursor.mRenderX-cursor.mX) < 1.0f && std::abs(cursor.mRenderY-cursor.mY) < 1.0f;
}
}

namespace LawnNet
{
void NetSmokeObserveChooserPing(LawnApp* app, int seed)
{
    const char* workflow = app && app->CoopSmokeActive() ? getenv("PVZ_COOP_SMOKE_COMMUNICATIONS") : nullptr;
    if (!workflow || strcmp(workflow,"chooser") || seed != SEED_PEASHOOTER) return;
    s.pingRendered = true; s.pingDrawTick = GetTickCount(); s.pingBeforeFrame = app->mCoopSmokeFrame;
}

bool NetSmokeUpdateCommunications(LawnApp* app)
{
    const char* workflow = getenv("PVZ_COOP_SMOKE_COMMUNICATIONS");
    if (!app->CoopSmokeActive() || !workflow || !*workflow) return false;
    const bool chooserRun = !strcmp(workflow,"chooser"), conveyorRun = !strcmp(workflow,"conveyor");
    if (!chooserRun && !conveyorRun) { app->CoopSmokeFail("unknown communications workflow"); return true; }
    NetManager* net = app->mNetManager;
    if (!net || net->GetPlayerCount() != 2 || !app->mPlayerInfo) return chooserRun;
    if (app->mWidth != 800 || app->mHeight != 600 || app->mCoopSmokeManifest.mSkipIntro)
    { app->CoopSmokeFail("communications fixture requires full-intro800 pair"); return true; }
    const bool host = app->NetIsHost();
    if (s.diagnosticStage != s.stage || GetTickCount()-s.diagnosticTick >= 1000U)
    {
        s.diagnosticStage = s.stage; s.diagnosticTick = GetTickCount();
        const SeedChooserScreen* chooser = app->mSeedChooserScreen;
        const NetRemoteCursor& remote = net->mRemoteCursors[host ? 1 : 0];
        NetTrace("SMOKE_COMMUNICATIONS_WAIT role=%s stage=%d scene=%d phase=%d chooser=%d mouse=%d flight=%d localPea=%d chooserXY=%d,%d remoteValid=%d target=%u,%u,%u point=%d,%d",
            host ? "host" : "client",s.stage,(int)app->mGameScene,(int)net->GetInputPhase(),chooser ? 1 : 0,
            chooser && chooser->mMouseVisible ? 1 : 0,chooser ? chooser->mSeedsInFlight : -1,
            chooser ? (int)chooser->mChosenSeeds[SEED_PEASHOOTER].mSeedState : -1,
            chooser ? chooser->mX : -1,chooser ? chooser->mY : -1,remote.mValid ? 1 : 0,
            remote.mHoverTarget.mKind,remote.mHoverTarget.mOwner,remote.mHoverTarget.mSeed,remote.mX,remote.mY);
    }
    app->mCoopSmokePeerReady = true;
    if (conveyorRun)
    {
        if (!app->mBoard || app->mGameScene != SCENE_PLAYING || !app->mBoard->mSeedBank ||
            !app->mBoard->HasConveyorBeltSeedBank()) return false;
        Board* board = app->mBoard;
        if (s.stage == 0)
        {
            if (!host && !s.issued)
                for (int i=0;i<board->mSeedBank->mNumPackets;++i)
                {
                    SeedPacket& packet = board->mSeedBank->mSeedPackets[i];
                    const int x = board->mSeedBank->mX + packet.mX + packet.mOffsetX + packet.mWidth / 2;
                    if (!packet.mNetId || packet.mPacketType == SEED_NONE || x >= 570) continue;
                    s.packet = packet.mNetId;
                    if (!Motion(app,(float)x,(float)(board->mSeedBank->mY+packet.mY+packet.mHeight/2)))
                    { app->CoopSmokeFail("conveyor SDL motion rejected"); return true; }
                    net->SendChat("COMM_PACKET_"+std::to_string(s.packet)); s.issued = true; break;
                }
            const NetRemoteCursor& cursor = net->mRemoteCursors[1];
            if (host && cursor.mValid && cursor.mHoverPacketId)
                for (int i=0;i<board->mSeedBank->mNumPackets;++i)
                {
                    const SeedPacket& packet = board->mSeedBank->mSeedPackets[i];
                    const Rect rect(board->mSeedBank->mX+packet.mX+packet.mOffsetX,
                        board->mSeedBank->mY+packet.mY,packet.mWidth,packet.mHeight);
                    if (packet.mNetId != cursor.mHoverPacketId || !PointerInside(cursor,rect) ||
                        !Saw(app,1,"COMM_PACKET_"+std::to_string(packet.mNetId))) continue;
                    s.packet = packet.mNetId;
                    if (!Saw(app,0,"COMM_HOVER_OK")) net->SendChat("COMM_HOVER_OK");
                    break;
                }
            if (!(host ? s.packet != 0 : Saw(app,0,"COMM_HOVER_OK"))) return true;
            if (Checkpoint(app,"conveyor_hover"))
                NetTrace("SMOKE_COMMUNICATIONS_CONVEYOR role=%s packetId=%u semantic=1 renderInside=1 sdl=1",
                    host ? "host" : "client",s.packet);
            return true;
        }
        if (s.stage == 1)
        {
            if (app->mCoopSmokeSnapshotsSent < 3 && host) return true;
            if (app->mCoopSmokeSnapshotsApplied < 3 && !host) return true;
            app->CoopSmokeWriteResult(true,"semantic conveyor hover completed");
        }
        return true;
    }
    if (s.stage == 0)
    {
        if (host && (!app->mGameSelector || app->mGameSelector->mSelectorState != SELECTOR_IDLE)) return true;
        if (host && !s.prepared)
        {
            app->mPlayerInfo->SetLevel(8); app->mPlayerInfo->mFinishedAdventure = 0;
            app->mPlayerInfo->mPurchases[STORE_ITEM_PACKET_UPGRADE] = 4;
            app->NetBroadcastProfile(); s.prepared = true;
        }
        if (app->mPlayerInfo->mLevel != 8 || app->mPlayerInfo->mPurchases[STORE_ITEM_PACKET_UPGRADE] != 4) return true;
        if (Checkpoint(app,"profile") && host)
        {
            app->mCoopSmokeGameStarted = true; app->KillGameSelector();
            app->PreNewGame(GAMEMODE_ADVENTURE,false,8);
        }
        return true;
    }
    SeedChooserScreen* chooser = app->mSeedChooserScreen;
    if (!chooser || !chooser->mCoopChooserActive || !chooser->mMouseVisible ||
        chooser->mSeedsInFlight || net->GetInputPhase() != NETINPUT_CHOOSER || app->GetDialogCount()) return true;
    if (s.stage == 1)
    {
        if (!host && !s.issued)
        {
            NetCursorTarget target; target.mKind = NETCURSOR_CHOOSER_CATALOG; target.mSeed = SEED_PEASHOOTER;
            Rect rect;
            if (!chooser->CoopCursorTargetRect(target,rect)) return true;
            SDL_SetModState(SDL_KMOD_LALT);
            if (!Click(app,SDL_BUTTON_RIGHT,(float)(rect.mX+rect.mWidth/2),(float)(rect.mY+rect.mHeight/2)))
            { app->CoopSmokeFail("chooser Alt+right click rejected by SDL"); return true; }
            s.issued = true; s.tick = GetTickCount();
        }
        if (!host && s.tick && GetTickCount()-s.tick >= 100U) SDL_SetModState(SDL_KMOD_NONE);
        bool ping = false; uint32_t pingTick = 0;
        for (const auto& annotation : net->GetAnnotations())
            if (annotation.mKind == NETANNOTATION_CHOOSER_SEED && annotation.mSlot == 1 && annotation.mId == SEED_PEASHOOTER+1)
            { ping = true; pingTick = annotation.mTick; }
        if (!ping || !s.pingRendered || app->mCoopSmokeFrame == s.pingBeforeFrame) return true;
        const auto& cursor = net->mRemoteCursors[1];
        if (host && (!cursor.mValid || cursor.mHoverTarget.mKind != NETCURSOR_CHOOSER_CATALOG ||
            cursor.mHoverTarget.mSeed != SEED_PEASHOOTER)) return true;
        const uint32_t age = GetTickCount()-pingTick;
        if (age < 320U || (age/140U)%2 || (s.pingDrawTick-pingTick)/140U != age/140U) return true;
        if (!s.tick) s.tick = pingTick;
        if (Checkpoint(app,"chooser_ping"))
            NetTrace("SMOKE_COMMUNICATIONS_CHOOSER_PING role=%s seed=%d altRight=1 selected=0 received=1 rendered=1",
                host ? "host" : "client",SEED_PEASHOOTER);
        return true;
    }
    if (s.stage == 2)
    {
        const SeedType type = host ? SEED_SUNFLOWER : SEED_PEASHOOTER;
        if (chooser->mChosenSeeds[type].mSeedState != SEED_IN_BANK && !s.issued)
        {
            NetCursorTarget target; target.mKind = NETCURSOR_CHOOSER_CATALOG; target.mSeed = (uint8_t)type;
            Rect rect;
            if (!chooser->CoopCursorTargetRect(target,rect)) return true;
            if (!Click(app,SDL_BUTTON_LEFT,(float)(rect.mX+rect.mWidth/2),(float)(rect.mY+rect.mHeight/2)))
            { app->CoopSmokeFail("chooser manual SDL pick rejected"); return true; }
            s.issued = true;
        }
        if (chooser->mChosenSeeds[type].mSeedState != SEED_IN_BANK) return true;
        Checkpoint(app,"different_first_picks"); return true;
    }
    if (s.stage == 3)
    {
        NetCursorTarget target; target.mKind = NETCURSOR_CHOOSER_BANK;
        target.mOwner = 1; target.mSeed = SEED_PEASHOOTER;
        Rect rect;
        if (!chooser->CoopCursorTargetRect(target,rect)) return true;
        if (!host && !s.issued)
        {
            if (!Motion(app,(float)(rect.mX+rect.mWidth/2),(float)(rect.mY+rect.mHeight/2)))
            { app->CoopSmokeFail("chooser selected SDL motion rejected"); return true; }
            s.issued = true;
        }
        if (host)
        {
            const NetRemoteCursor& cursor = net->mRemoteCursors[1];
            if (cursor.mHoverTarget.mKind != NETCURSOR_CHOOSER_BANK || cursor.mHoverTarget.mOwner != 1 ||
                cursor.mHoverTarget.mSeed != SEED_PEASHOOTER || !PointerInside(cursor,rect)) return true;
            if (!Saw(app,0,"COMM_CHOOSER_HOVER_OK")) net->SendChat("COMM_CHOOSER_HOVER_OK");
        }
        else if (!Saw(app,0,"COMM_CHOOSER_HOVER_OK")) return true;
        if (Checkpoint(app,"chooser_relative_hover"))
            NetTrace("SMOKE_COMMUNICATIONS_CHOOSER_HOVER role=%s seed=%d owner=1 ownFirst=1 renderInside=1 sdl=1",
                host ? "host" : "client",SEED_PEASHOOTER);
        return true;
    }
    if (s.stage == 4) app->CoopSmokeWriteResult(true,"chooser Alt+right ping and relative hover completed");
    return true;
}
}
