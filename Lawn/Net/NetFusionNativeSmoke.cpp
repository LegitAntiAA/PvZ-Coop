#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Board.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Projectile.h"
#include "../Coin.h"
#include "../SeedPacket.h"
#include "../CursorObject.h"
#include "../Cutscene.h"
#include "../Widget/SeedChooserScreen.h"
#include "../Widget/GameButton.h"
#include "../Fusion/FusionNative.h"
#include "../Fusion/FusionWire.h"
#include "../Fusion/FusionNativeWire.h"
#include "NetMystery.h"
#include "NetEndlessWeather.h"
#include "NetZenGarden.h"
#include "../../Resources.h"
#include "../../SexyAppFramework/Font.h"
#include "../../SexyAppFramework/DDInterface.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <set>
#include <vector>

namespace
{
using namespace LawnFusion;
int stage = 0;
uint32_t since = 0, observedSince = 0, lastChat = 0, headlessSince = 0;
uint16_t originalPlant = 0, fusedPlant = 0, sunflowerId = 0, zombieId = 0, sunId = 0;
uint16_t heldPacketId = 0;
uint32_t staleSamples = 0;
uint16_t gridParentId = 0, gridFusedId = 0, gridHostPacketId = 0, gridClientPacketId = 0;
bool geometryProved = false;
bool decoderProved = false;
std::set<std::string> gridProofs;
std::set<uint16_t> gridPlantBaseline;
struct ReadyPacketState { uint16_t id; int refreshCounter, refreshTime, timesUsed; };
std::vector<ReadyPacketState> gridPacketBaseline;
int fieldRound = 0, fieldCounter = 0, fieldRefreshTime = 0;
bool fieldWasRefreshing = false;
CursorType fieldCursorType = CURSOR_TYPE_NORMAL;
bool introSeen = false, bulletSeen = false, littleSunSeen = false, headlessSeen = false, deathSeen = false;
bool introPhaseObserved = false;
std::set<std::string> frames;
std::map<uint16_t, NativePacketData> packetBaseline;
std::set<uint16_t> cooldownStarted;
std::set<std::string> ownerProofs;
struct SunPickupPlan
{
    int value = 0, x = 0, y = 0;
    uint16_t id = 0;
    uint32_t stableSince = 0;
    std::string message;
};
SunPickupPlan sunPickup;
struct SunObservation { int value; bool collecting; bool dead; };
std::map<uint16_t, SunObservation> sunObservations;
int observedSunMoney = -1;

const char* Role(LawnApp* app) { return app->NetIsHost() ? "host" : "client"; }
uint32_t Samples(LawnApp* app) { return app->NetIsHost() ? app->mCoopSmokeSnapshotsSent : app->mCoopSmokeSnapshotsApplied; }
bool Require(LawnApp* app, bool condition, const char* error)
{ if (!condition) app->CoopSmokeFail(error); return condition; }
void Advance(LawnApp* app, int next)
{
    stage = next; since = GetTickCount(); observedSince = 0;
    LawnNet::NetTrace("SMOKE_FUSION_NATIVE_STAGE role=%s stage=%d", Role(app), next);
}
bool Settled(uint32_t milliseconds = 300)
{
    if (!observedSince) observedSince = GetTickCount();
    return GetTickCount() - observedSince >= milliseconds;
}
bool Has(LawnApp* app, int slot, const char* message)
{
    for (const auto& line : app->mNetManager->mChatLines) if (line.mSlot == slot && line.mText == message) return true;
    return false;
}
void Send(LawnApp* app, const char* message)
{
    if (Has(app, app->mNetManager->GetMySlot(), message) || (lastChat && GetTickCount() - lastChat < 800)) return;
    lastChat = GetTickCount(); app->mNetManager->SendChat(message);
}
bool Barrier(LawnApp* app, const char* message)
{ Send(app, message); return Has(app, 0, message) && Has(app, 1, message); }
bool Capture(LawnApp* app, const char* name)
{
    if (frames.contains(name)) return true;
    if (!Require(app, app->CoopSmokeCaptureFrame(name), "Native Fusion renderer capture failed")) return false;
    frames.insert(name);
    LawnNet::NetTrace("SMOKE_FUSION_NATIVE_FRAME role=%s name=%s nativeSDL=1", Role(app), name);
    return true;
}
bool Click(LawnApp* app, float x, float y, uint8_t button = SDL_BUTTON_LEFT)
{
    SDL_Event motion{}; motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.windowID = SDL_GetWindowID(app->mSDLWindow); motion.motion.x = x; motion.motion.y = y;
    SDL_Event down{}; down.type = SDL_EVENT_MOUSE_BUTTON_DOWN; down.button.windowID = motion.motion.windowID;
    down.button.button = button; down.button.clicks = 1; down.button.down = true; down.button.x = x; down.button.y = y;
    SDL_Event up = down; up.type = SDL_EVENT_MOUSE_BUTTON_UP; up.button.down = false;
    return Require(app, SDL_PushEvent(&motion) && SDL_PushEvent(&down) && SDL_PushEvent(&up), "Native Fusion owned-window SDL mouse queue failed");
}
int WideX(LawnApp* app) { return app->mDDInterface->mWideScreenOffsetX; }
int WideY(LawnApp* app) { return app->mDDInterface->mWideScreenOffsetY; }
bool BoardClick(LawnApp* app, float x, float y, uint8_t button = SDL_BUTTON_LEFT)
{ return Click(app, app->mBoard->mX + WideX(app) + x, app->mBoard->mY + WideY(app) + y, button); }
bool Click(LawnApp* app, const Sexy::Rect& rect)
{ return BoardClick(app, rect.mX + rect.mWidth * 0.5f, rect.mY + rect.mHeight * 0.5f); }
bool Button(LawnApp* app, GameButton* button)
{
    if (!button || button->mDisabled || button->mBtnNoDraw) return false;
    const float x = button->mX + button->mWidth * 0.5f;
    const float y = button->mY + button->mHeight * 0.5f;
    // GameButton computes mIsOver in Update, unlike the board's direct hit
    // tests. Let a real motion event reach that update before pressing it.
    if (!button->IsMouseOver())
    {
        SDL_Event motion{}; motion.type = SDL_EVENT_MOUSE_MOTION;
        motion.motion.windowID = SDL_GetWindowID(app->mSDLWindow); motion.motion.x = x; motion.motion.y = y;
        Require(app, SDL_PushEvent(&motion), "Native Fusion owned-window SDL button motion failed");
        return false;
    }
    LawnNet::NetTrace("SMOKE_FUSION_NATIVE_BUTTON role=%s hoverObserved=1 x=%.0f y=%.0f nativeSDL=1", Role(app), x, y);
    return Click(app, x, y);
}
bool FieldRect(LawnApp* app, Sexy::Rect& rect, bool requireEnabled = true)
{
    const auto* context = GetNativeContext(app->mBoard);
    GameButton* button = context ? context->fieldButton.get() : nullptr;
    GameButton* menu = app->mBoard->mMenuButton;
    if (!Require(app, button && menu && button->mParentWidget == app->mBoard && button->mDrawStoneButton &&
        button->mWidth == 117 && button->mHeight == 46 && button->mX == menu->mX && button->mY == menu->mY+menu->mHeight-4,
        "Live FIELD button does not use the actual native stone button below Menu")) return false;
    if (requireEnabled && (!Require(app, !button->mDisabled && !button->mBtnNoDraw && button->mLabel == _S("FIELD"),
        "Live FIELD button is disabled, hidden or failed to save its preference"))) return false;
    const Sexy::Point position = button->mParentWidget->GetAbsPos();
    rect = Sexy::Rect(position.mX+button->mX, position.mY+button->mY, button->mWidth, button->mHeight);
    return Require(app, rect.mX >= 0 && rect.mY >= 0 && rect.mX+rect.mWidth <= app->mWidth &&
        rect.mY+rect.mHeight <= position.mY+WideY(app)+80,
        "Live top-right FIELD button is clipped or overlaps the first lawn row");
}
bool FieldMouse(LawnApp* app, bool down)
{
    Sexy::Rect rect;
    if (!FieldRect(app, rect, down)) return false;
    const float x = rect.mX+rect.mWidth*0.5f, y = rect.mY+rect.mHeight*0.5f;
    SDL_Event motion{}; motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.windowID = SDL_GetWindowID(app->mSDLWindow); motion.motion.x = x; motion.motion.y = y;
    SDL_Event edge{}; edge.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    edge.button.windowID = motion.motion.windowID; edge.button.x = x; edge.button.y = y;
    edge.button.button = SDL_BUTTON_LEFT; edge.button.clicks = 1; edge.button.down = down;
    return Require(app, SDL_PushEvent(&motion) && SDL_PushEvent(&edge), "Owned FIELD SDL edge queue failed");
}
bool FieldClick(LawnApp* app)
{
    Sexy::Rect rect;
    if (!FieldRect(app, rect)) return false;
    auto* button = GetNativeContext(app->mBoard)->fieldButton.get();
    if (!button->IsMouseOver())
    {
        SDL_Event motion{}; motion.type = SDL_EVENT_MOUSE_MOTION;
        motion.motion.windowID = SDL_GetWindowID(app->mSDLWindow);
        motion.motion.x = rect.mX+rect.mWidth*0.5f; motion.motion.y = rect.mY+rect.mHeight*0.5f;
        Require(app, SDL_PushEvent(&motion), "Owned FIELD SDL hover queue failed");
        return false;
    }
    LawnNet::NetTrace("SMOKE_FUSION_NATIVE_FIELD_BUTTON role=%s round=%d x=%d y=%d width=117 height=46 belowMenu=1 parentBoard=1 hoverObserved=1 nativeSDL=1",
        Role(app), fieldRound, rect.mX, rect.mY);
    return Click(app, rect.mX+rect.mWidth*0.5f, rect.mY+rect.mHeight*0.5f);
}
bool FocusEvent(LawnApp* app, bool gained)
{
    SDL_Event event{}; event.type = gained ? SDL_EVENT_WINDOW_FOCUS_GAINED : SDL_EVENT_WINDOW_FOCUS_LOST;
    event.window.windowID = SDL_GetWindowID(app->mSDLWindow);
    return Require(app, SDL_PushEvent(&event), "Owned FIELD focus event queue failed");
}
bool Key(LawnApp* app, SDL_Keycode code)
{
    SDL_Event down{}; down.type = SDL_EVENT_KEY_DOWN; down.key.windowID = SDL_GetWindowID(app->mSDLWindow);
    down.key.key = code; down.key.scancode = SDL_GetScancodeFromKey(code, nullptr); down.key.down = true;
    SDL_Event up = down; up.type = SDL_EVENT_KEY_UP; up.key.down = false;
    return Require(app, SDL_PushEvent(&down) && SDL_PushEvent(&up), "Native Fusion owned-window SDL key queue failed");
}
bool Cell(LawnApp* app, int column, int row)
{
    Board* board = app->mBoard;
    if (!Require(app, board->IsValidGridCell(column, row), "Native fixture attempted SDL click at an invalid grid cell")) return false;
    return BoardClick(app, (float)board->GetCellCenterX(column, row), (float)(board->GridToPixelY(column, row) + 50));
}
Plant* At(Board* board, int column, int row)
{
    Plant* plant = nullptr;
    while (board->mPlants.IterateNext(plant))
        if (!plant->mDead && plant->mIsOnBoard && plant->mPlantCol == column && plant->mRow == row) return plant;
    return nullptr;
}
SeedPacket* Packet(Board* board, int owner, uint16_t kind, bool extra)
{
    for (int i = 0; i < board->mSeedBank->mNumPackets; ++i)
    {
        SeedPacket* packet = &board->mSeedBank->mSeedPackets[i];
        const auto* data = GetNativePacketData(packet);
        if (data && data->owner == owner && data->kind == kind && data->isExtra == extra) return packet;
    }
    return nullptr;
}
bool FieldCooldownProgressed(LawnApp* app)
{
    SeedPacket* packet = Packet(app->mBoard, 1, 0, true);
    // Native snapshots quantize this750-tick counter to255 steps. Allow its
    // ordinary progress/completion and at most three ticks of rounding.
    return Require(app, packet && packet->mRefreshTime == fieldRefreshTime &&
        (!packet->mRefreshing || (fieldWasRefreshing && packet->mRefreshCounter+3 >= fieldCounter)) &&
        (packet->mRefreshing || packet->mActive), "FIELD interaction restarted or erased the live packet cooldown");
}
bool Pick(LawnApp* app, SeedPacket* packet)
{
    if (!Require(app, packet && packet->mActive && !packet->mRefreshing, "Native Fusion selected card is missing or recharging")) return false;
    const auto* bank = app->mBoard->mSeedBank;
    return BoardClick(app, (float)(bank->mX + packet->mX + packet->mOffsetX + packet->mWidth / 2),
        (float)(bank->mY + packet->mY + packet->mHeight / 2));
}
bool HoldingPacket(const Board* board, uint16_t id)
{
    const CursorObject* cursor = board->mCursorObject;
    if (!cursor || !board->mSeedBank || cursor->mCursorType != CURSOR_TYPE_PLANT_FROM_BANK ||
        cursor->mSeedBankIndex < 0 || cursor->mSeedBankIndex >= board->mSeedBank->mNumPackets) return false;
    const SeedPacket& packet = board->mSeedBank->mSeedPackets[cursor->mSeedBankIndex];
    return packet.mNetId == id && cursor->mType == packet.mPacketType;
}
bool ResourceEnabled()
{
    const char* value=std::getenv("PVZ_COOP_SMOKE_FUSION_RESOURCE");
    return value && !std::strcmp(value,"1");
}
struct ResourceProbe
{
    bool baseline=false, clicked=false, rejected=false, repicked=false, retried=false, clientProbe=false, expectedLost=false;
    uint32_t repickSamples=0;
    uint32_t parentToken=0;
    int packetCounter=0, packetTime=0, packetUsed=0, plantCount=0, errorDraw=0;
    NativePlantData parent;
} resource;
std::filesystem::path ResourceRoot(LawnApp* app)
{
    const char* raw=std::getenv("PVZ_COOP_SMOKE_FUSION_RESOURCE_ROOT");
    if (!raw || !*raw) return {};
    const auto root=std::filesystem::absolute(raw).lexically_normal();
    const auto expected=std::filesystem::absolute(app->mCoopSmokeManifest.mResultPath).parent_path().parent_path().lexically_normal();
    return root==expected ? root : std::filesystem::path{};
}
bool ResourceMarker(LawnApp* app,const char* name)
{
    const auto root=ResourceRoot(app);std::error_code error;
    return !root.empty() && std::filesystem::is_regular_file(root/name,error);
}
bool ResourceUnchanged(LawnApp* app)
{
    auto* b=app->mBoard;auto* p=At(b,3,2);const auto* d=GetNativePlantData(p);
    auto* card=Packet(b,1,1,true);const auto* c=GetNativePacketData(card);
    return Require(app,p && d && p->mNetId==originalPlant && d->instanceToken==resource.parentToken &&
        d->kind==0 && d->owner==resource.parent.owner && d->packetId==resource.parent.packetId &&
        d->firstParent==resource.parent.firstParent && d->secondParent==resource.parent.secondParent &&
        p->mPlantHealth==300 && b->mPlants.mSize==resource.plantCount && b->mSunMoney==400 &&
        card && c && c->kind==1 && c->isExtra && c->owner==1 && c->price==100 && card->mActive && !card->mRefreshing &&
        card->mRefreshCounter==resource.packetCounter && card->mRefreshTime==resource.packetTime && card->mTimesUsed==resource.packetUsed,
        "Resource failed craft changed native parent/SUN/card identity or cooldown");
}
bool ResourceLossFinished(LawnApp* app)
{
    if (!resource.expectedLost) return false;
    if (app->mBoard || app->mSeedChooserScreen || app->mGameScene!=SCENE_MENU ||
        app->mNetManager->IsConnectedToHost() || !app->GetDialog(DIALOG_INFO)) return true;
    if (!resource.errorDraw) resource.errorDraw=app->mDrawCount;
    if (app->mDrawCount-resource.errorDraw<2) return true;
    if (!Capture(app,"fusion_resource_error")) return true;
    LawnNet::NetTrace("SMOKE_FUSION_RESOURCE_TEARDOWN role=client boardGone=1 selector=1 infoDialog=1 deferred=1 actualDraws=2");
    app->CoopSmokeWriteResult(true,nullptr);return true;
}
void ResourceUpdate(LawnApp* app)
{
    auto* b=app->mBoard;const auto* ctx=GetNativeContext(b);auto* card=Packet(b,1,1,true);
    if (!Require(app,!ResourceRoot(app).empty() && ctx && card,"Resource isolated-root/actual incoming card missing")) return;
    if (!resource.baseline)
    {
        auto* p=At(b,3,2);const auto* d=GetNativePlantData(p);
        if (!Require(app,p && d && p->mNetId==originalPlant && d->kind==0 && b->mSunMoney==400 &&
            card->mActive && !card->mRefreshing && GetNativePacketData(card)->price==100,"Resource initial paid precondition differs")) return;
        resource.parent=*d;resource.parentToken=d->instanceToken;resource.plantCount=b->mPlants.mSize;
        resource.packetCounter=card->mRefreshCounter;resource.packetTime=card->mRefreshTime;resource.packetUsed=card->mTimesUsed;
        resource.baseline=true;
    }
    if(stage==60)
    {
        if(Samples(app)<12 || !ResourceUnchanged(app))return;
        if(app->NetIsClient())
        {
            if(!resource.clicked)
            {
                if(!HoldingPacket(b,heldPacketId)||!Settled())return;
                if(!Capture(app,"fusion_resource_before"))return;
                if(!Cell(app,3,2))return;resource.clicked=true;
                LawnNet::NetTrace("SMOKE_FUSION_RESOURCE_ATTEMPT role=client parent=%u token=%u packet=%u price=100 sun=400 nativeSDL=1",originalPlant,resource.parentToken,card->mNetId);
            }
            Send(app,"FR229_ATTEMPT");if(!Has(app,1,"FR229_ATTEMPT"))return;
        }
        else if(!Has(app,1,"FR229_ATTEMPT"))return;
        Advance(app,61);return;
    }
    if(stage==61)
    {
        if(!ResourceMarker(app,"host-rejection-observed.marker") || !ResourceUnchanged(app))return;
        if(!resource.rejected)
        {
            LawnNet::NetTrace("SMOKE_FUSION_RESOURCE_REJECT role=%s parent=%u token=%u sun=400 price=100 packet=%u refresh=%d/%d used=%d parentAlive=1 mutation=0 nativeSDL=1 runtimeRngEquality=NotExecuted",
                Role(app),originalPlant,resource.parentToken,card->mNetId,card->mRefreshCounter,card->mRefreshTime,card->mTimesUsed);
            resource.rejected=true;
        }
        if(!Barrier(app,"FR229_REJECT_OK"))return;
        Advance(app,62);return;
    }
    if(stage==62 && app->NetIsHost())
    {
        const auto* born=GetNativePlantData(At(b,3,2));
        if(born && born->kind==1000) Advance(app,63);
    }
    if(stage==62)
    {
        if(!ResourceMarker(app,"host-restored.marker") || !ResourceUnchanged(app))return;
        if(app->NetIsClient())
        {
            if(!resource.repicked)
            {
                resource.repickSamples=Samples(app);
                if(!Pick(app,card))return;
                resource.repicked=true;
                LawnNet::NetTrace("SMOKE_FUSION_RESOURCE_REPICK role=client packet=%u samples=%u nativeSDL=1",card->mNetId,resource.repickSamples);
                return;
            }
            // Rejected placement clears the old canonical cursor lease. A real
            // new pickup must receive a fresh host echo before the retry click.
            if(!HoldingPacket(b,heldPacketId) || Samples(app)<=resource.repickSamples || !Settled(150))return;
            if(!resource.retried)
            {
                if(!Cell(app,3,2))return;resource.retried=true;
                LawnNet::NetTrace("SMOKE_FUSION_RESOURCE_RETRY role=client packet=%u parent=%u nativeSDL=1 hostFileRestored=1 repickEcho=1",card->mNetId,originalPlant);
            }
            Advance(app,63);return;
        }
        // The host advances on the actual born actor, not a throttled chat.
        return;
    }
    if(stage!=63)return;
    if(app->NetIsHost())
    {
        auto* p=At(b,3,2);const auto* d=GetNativePlantData(p);
        if(!d||d->kind==0)return;
        if(!Require(app,d->kind==1000 && d->firstParent==0 && d->secondParent==1 && d->owner==1 && d->packetId==card->mNetId &&
            p->mNetId!=originalPlant && d->instanceToken!=resource.parentToken && p->mPlantHealth==300 && b->mSunMoney==300 &&
            !card->mActive && card->mRefreshing && card->mRefreshTime==750 && card->mTimesUsed==resource.packetUsed+1,
            "Resource restored positive craft did not charge exactly100/replace parent/start real cooldown"))return;
        LawnNet::NetTrace("SMOKE_FUSION_RESOURCE_POSITIVE role=host old=%u oldToken=%u child=%u token=%u kind=1000 parents=0,1 owner=1 packet=%u price=100 sun=300 recharge=750 actualSDL=1",
            originalPlant,resource.parentToken,p->mNetId,d->instanceToken,card->mNetId);
        // Capture/proof precedes the expected peer drop and roster rebuild.
        app->CoopSmokeWriteResult(true,nullptr);return;
    }
    if(resource.clientProbe)return;
    for(const auto& pending:app->mNetManager->mPendingBoardMsgs)if(pending.mKind==1)
    {
        auto candidate=pending.mData;NativeSnapshotFrame frame;LawnNet::NetBuffer native;
        if(!DecodeNativeSnapshot(candidate,ctx->eligible,ctx->epoch,frame,native))continue;
        const auto child=std::find_if(frame.plants.begin(),frame.plants.end(),[](const auto& r){return r.data.kind==1000;});
        if(child==frame.plants.end())continue;
        if(!Require(app,child->data.firstParent==0 && child->data.secondParent==1 && child->data.owner==1 && child->data.packetId==card->mNetId &&
            ResourceUnchanged(app),"Resource actual queued host child/old client world identity differs"))return;
        LawnNet::NetBuffer before,after,trial;b->NetEncodeSnapshot(before);
        trial.WriteU8(0x77);trial.WriteBytes(pending.mData.Data(),pending.mData.Size());trial.mReadPos=1;
        const auto bytes=trial.mData;bool applied=true;Board* originalBoard=b;b->NetApplySnapshot(trial,&applied);
        if(app->mBoard==originalBoard)b->NetEncodeSnapshot(after);
        const bool connected=app->mNetManager->IsConnectedToHost();
        const auto& reason=app->mNetManager->GetDisconnectReason();
        if(!Require(app,!applied && trial.mReadPos==1 && trial.mData==bytes && app->mBoard==originalBoard && connected &&
            before.mData==after.mData && ResourceUnchanged(app) &&
            reason=="Fusion resources could not be loaded. Check your game installation.",
            "Resource valid snapshot did not reject atomically/defer local resource disconnect"))return;
        resource.clientProbe=true;
        LawnNet::NetTrace("SMOKE_FUSION_RESOURCE_CLIENT_PROBE role=client id=%u kind=1000 actualQueuedSnapshot=1 FNSValid=1 readerUnchanged=1 worldBytesUnchanged=1 parentAlive=1 liveBoard=1 stillConnected=1 applied=0 deferred=1",
            child->id);return;
    }
}

bool NativeGrid(LawnApp* app)
{
    Board* board = app->mBoard;
    const auto* context = GetNativeContext(board);
    if (!Require(app, MAX_GRID_SIZE_X == 9 && MAX_GRID_SIZE_Y == 6 && board->GetColumnCount() == 10 && board->GetCellPitchX() == 72 &&
        board->IsValidGridCell(9, 4) && !board->IsValidGridCell(10, 4) && !board->IsValidGridCell(9, 5) &&
        !board->IsValidGridCell(-1, 0) && !board->IsValidGridCell(0, -1), "Native Fusion playable geometry is not exactly10x5/pitch72")) return false;
    for (int row = 0; row < 5; ++row)
        for (int column = 0; column < 10; ++column)
        {
            const int x = board->GetCellCenterX(column, row), y = board->GridToPixelY(column, row)+50;
            if (!Require(app, board->GridToPixelX(column, row) == 40+72*column && x == 76+72*column && y == 130+100*row &&
                board->PixelToGridX(x, y) == column && board->PixelToGridY(x, y) == row &&
                board->GetGridSquareTypeAt(column, row) == GRIDSQUARE_GRASS &&
                &board->GetGridSquareTypeAt(column, row) == (column == 9 ? &context->lastColumnTerrain[row] : &board->mGridSquareType[column][row]) &&
                &board->GetGridCelOffsetAt(column, row, 0) == (column == 9 ? &context->lastColumnOffsets[row][0] : &board->mGridCelOffset[column][row][0]) &&
                &board->GetGridCelOffsetAt(column, row, 1) == (column == 9 ? &context->lastColumnOffsets[row][1] : &board->mGridCelOffset[column][row][1]),
                "Native Fusion cell roundtrip/grass terrain/offset differs between ordinary and tenth column")) return false;
        }
    if (!Require(app, board->PixelToGridX(39, 530) == -1 && board->PixelToGridX(40, 530) == 0 &&
        board->PixelToGridX(687, 530) == 8 && board->PixelToGridX(688, 530) == 9 && board->PixelToGridX(759, 530) == 9 &&
        board->PixelToGridX(760, 530) == -1 && board->PixelToGridY(724, 79) == -1 && board->PixelToGridY(724, 80) == 0 &&
        board->PixelToGridY(724, 579) == 4 && board->PixelToGridY(724, 580) == -1,
        "Native Fusion pixel bounds clamp an outside gutter or disagree at a cell edge")) return false;
    if (!geometryProved)
    {
        geometryProved = true;
        LawnNet::NetTrace("SMOKE_FUSION_NATIVE_GRID role=%s columns=10 rows=5 pitch=72 left=40 right=760 top=80 bottom=580 roundtrips=50 tenthGrass=1 storageRows=6 rawMaxColumns=9 viewport=%dx%d offset=%d,%d",
            Role(app), app->mWidth, app->mHeight, WideX(app), WideY(app));
    }
    return true;
}
std::set<uint16_t> LivePlantIds(Board* board)
{
    std::set<uint16_t> ids;
    Plant* plant = nullptr;
    while (board->mPlants.IterateNext(plant)) if (!plant->mDead && plant->mIsOnBoard) ids.insert(plant->mNetId);
    return ids;
}
bool GridUnchanged(LawnApp* app)
{
    Board* board = app->mBoard;
    if (!Require(app, board->mSunMoney == 280 && LivePlantIds(board) == gridPlantBaseline &&
        At(board, 3, 2) && At(board, 3, 2)->mNetId == fusedPlant && !At(board, 9, 4),
        "Native invalid coordinate changed currency or live plant identities")) return false;
    for (const auto& old : gridPacketBaseline)
    {
        SeedPacket* packet = nullptr;
        for (int i = 0; i < board->mSeedBank->mNumPackets; ++i)
            if (board->mSeedBank->mSeedPackets[i].mNetId == old.id) packet = &board->mSeedBank->mSeedPackets[i];
        if (!Require(app, packet && packet->mActive && !packet->mRefreshing && packet->mRefreshCounter == old.refreshCounter &&
            packet->mRefreshTime == old.refreshTime && packet->mTimesUsed == old.timesUsed,
            "Native invalid coordinate consumed or reassigned a ready packet/cooldown")) return false;
    }
    return true;
}
bool EmptyClickPoint(LawnApp* app, int x, int y)
{
    Coin* coin = nullptr;
    while (app->mBoard->mCoins.IterateNext(coin))
    {
        HitResult hit{};
        if (!coin->mDead && !coin->mIsBeingCollected && coin->MouseHitTest(x, y, &hit))
            return Require(app, false, "Native boundary SDL point overlaps an unrelated live Coin");
    }
    return true;
}
bool GridRejected(LawnApp* app, const char* name, bool transport, int column, int row, int x, int y)
{
    if (!GridUnchanged(app) || GetTickCount()-since < 1000 || Samples(app)-staleSamples < 2) return false;
    if (gridProofs.insert(name).second)
        LawnNet::NetTrace("SMOKE_FUSION_NATIVE_GRID_REJECT role=%s case=%s transport=%d column=%d row=%d x=%d y=%d sun=280 charge=0 recharge=0 stableIds=1 packet=%u samples=%u",
            Role(app), name, transport ? 1 : 0, column, row, x, y, gridClientPacketId, Samples(app));
    const std::string message = std::string("FUSION_NATIVE_GRID_")+name;
    return Barrier(app, message.c_str());
}
bool GridFusedUnchanged(LawnApp* app)
{
    Board* board = app->mBoard;
    Plant* plant = At(board, 9, 4);
    const auto* data = GetNativePlantData(plant);
    return Require(app, plant && plant->mNetId == gridFusedId && data && data->kind == 1000 && data->firstParent == 1 &&
        data->secondParent == 0 && data->owner == 1 && data->randomOrigin == 0 && data->packetId == gridClientPacketId &&
        plant->mPlantHealth == 300 && plant->mX == board->GetCellCenterX(9, 4)-40 && plant->mY == board->GridToPixelY(9, 4) &&
        At(board, 3, 2) && At(board, 3, 2)->mNetId == fusedPlant && board->mSunMoney == 30,
        "Native field presentation changed tenth-column actor identity, provenance, geometry or currency");
}
template <typename T> void GhostProof(LawnNet::NetBuffer& buffer, DataArray<T>& objects)
{
    T* object = nullptr;
    while (objects.IterateNext(object))
    {
        buffer.WriteU16(object->mNetId); buffer.WriteU8(object->mNetGhost ? 1 : 0);
        buffer.WriteI32(object->mX); buffer.WriteI32(object->mY);
        buffer.WriteF32(object->mNetTargetX); buffer.WriteF32(object->mNetTargetY);
    }
}
LawnNet::NetBuffer DecoderState(Board* board)
{
    LawnNet::NetBuffer state;
    board->NetEncodeSnapshot(state);
    state.WriteI32(board->mNetLastApplyMs); state.WriteF32(board->mNetGhostTimeAcc); state.WriteU16(board->mNextNetId);
    GhostProof(state, board->mPlants); GhostProof(state, board->mZombies);
    GhostProof(state, board->mProjectiles); GhostProof(state, board->mCoins);
    for (int i = 0; i < board->mSeedBank->mNumPackets; ++i)
    {
        const SeedPacket& packet = board->mSeedBank->mSeedPackets[i];
        state.WriteU16(packet.mNetId); state.WriteI32(packet.mRefreshCounter); state.WriteI32(packet.mRefreshTime);
        state.WriteI32(packet.mTimesUsed); state.WriteU8(packet.mActive ? 1 : 0); state.WriteU8(packet.mRefreshing ? 1 : 0);
    }
    return state;
}
bool DecoderRejectsGrid(LawnApp* app, bool invalidColumn)
{
    Board* board = app->mBoard;
    const auto* context = GetNativeContext(board);
    LawnNet::NetBuffer wire, body;
    board->NetEncodeSnapshot(wire);
    NativeSnapshotFrame frame;
    if (!Require(app, wire.Size() && DecodeNativeSnapshot(wire, context->eligible, context->epoch, frame, body),
        "Native decoder-negative setup could not capture/decode its real client Board")) return false;
    const size_t core = 43 + MAX_GRID_SIZE_Y*4 + 9 + 10 + LawnNet::NET_CHALLENGE_RUNTIME_WIRE_BYTES +
        LawnNet::NET_ENDLESS_WEATHER_WIRE_BYTES + LawnNet::NET_ZEN_GARDEN_WIRE_BYTES;
    if (!Require(app, body.Size() >= core+7, "Native decoder-negative core offset exceeds captured body")) return false;
    const size_t plants = core+7 + body.mData[core+6]*LawnNet::NET_MYSTERY_SEED_SNAPSHOT_BYTES;
    if (!Require(app, body.Size() >= plants+8 && (body.mData[plants] || body.mData[plants+1]),
        "Native decoder-negative setup lacks a complete actual plant record")) return false;
    const size_t firstPlant = plants+2;
    body.mData[firstPlant+(invalidColumn ? 5 : 4)] = invalidColumn ? 10 : 5;
    const uint32_t forgedSun = 123456;
    for (int byte = 0; byte < 4; ++byte) body.mData[1+byte] = static_cast<uint8_t>(forgedSun >> (8*byte));
    if (!Require(app, EncodeNativeSnapshot(frame, context->eligible, body), "Native decoder-negative wrapper/checksum encoding failed")) return false;
    const auto before = DecoderState(board);
    const uint32_t appliedCount = app->mCoopSmokeSnapshotsApplied;
    const char* name = invalidColumn ? "column10" : "row5";
    LawnNet::NetTrace("SMOKE_FUSION_NATIVE_DECODER_BEGIN role=client case=%s forgedSun=123456 checksumValid=1 localDecoder=1", name);
    bool applied = true;
    board->NetApplySnapshot(body, &applied);
    const auto after = DecoderState(board);
    if (!Require(app, !applied && app->mCoopSmokeSnapshotsApplied == appliedCount && before.mData == after.mData,
        "Native out-of-grid snapshot mutated money/IDs/selection/cooldowns/ghost positions or applied count")) return false;
    LawnNet::NetTrace("SMOKE_FUSION_NATIVE_DECODER_REJECT role=client case=%s money=280 stableIds=1 stableSelection=1 stableCD=1 stableGhosts=1 applied=0 counterUnchanged=1 localDecoder=1", name);
    return true;
}
bool OwnerFirst(LawnApp* app, const char* checkpoint = nullptr, bool chooserView = false)
{
    Board* board = app->mBoard;
    const auto* context = GetNativeContext(board);
    if (!Require(app, context && board->mSeedBank && board->mSeedBank->mNumPackets == context->selection.State().targetCount,
        "Native owner-first bank does not contain the complete real packet quota")) return false;
    const int viewer = app->mNetManager->GetMySlot();
    const int targetCount = context->selection.State().targetCount;
    const int ownQuota = context->selection.State().quotas[viewer];
    const bool record = checkpoint && !ownerProofs.contains(checkpoint);
    std::vector<SeedPacket*> physical;
    std::set<uint16_t> current;
    for (int i = 0; i < board->mSeedBank->mNumPackets; ++i)
    {
        SeedPacket* packet = &board->mSeedBank->mSeedPackets[i];
        const auto* data = GetNativePacketData(packet);
        if (!Require(app, data && packet->mNetId && packet->mIndex == i && packet->mNetOwnerSlot == data->owner &&
            current.insert(packet->mNetId).second,
            "Native bank reorder lost packet identity/metadata")) return false;
        const auto previous = packetBaseline.find(packet->mNetId);
        if (previous == packetBaseline.end())
        {
            if (!Require(app, checkpoint && std::strcmp(checkpoint, "picks") == 0, "Native bank reorder replaced a stable packet id")) return false;
            packetBaseline.emplace(packet->mNetId, *data);
        }
        else
        {
            const auto& old = previous->second;
            if (!Require(app, data->selectionId == old.selectionId && data->kind == old.kind && data->isExtra == old.isExtra &&
                data->owner == old.owner && data->randomOrigin == old.randomOrigin && data->price == old.price && data->rechargeSeconds == old.rechargeSeconds,
                "Native bank reorder moved identity, ownership, price or source cooldown onto another packet")) return false;
        }
        const int expected = static_cast<int>(std::lround(data->rechargeSeconds*100.0f));
        const bool validRecharge = expected > 0 && (packet->mRefreshTime == 0 || packet->mRefreshTime == expected) &&
            packet->mRefreshCounter >= 0 && packet->mRefreshCounter <= packet->mRefreshTime &&
            (!packet->mRefreshing || (!packet->mActive && packet->mRefreshTime == expected));
        if (!validRecharge)
            LawnNet::NetTrace("SMOKE_FUSION_NATIVE_PACKET_CD_INVALID role=%s packet=%u kind=%u expected=%d time=%d counter=%d active=%d refreshing=%d",
                Role(app), packet->mNetId, data->kind, expected, packet->mRefreshTime, packet->mRefreshCounter,
                packet->mActive ? 1 : 0, packet->mRefreshing ? 1 : 0);
        if (!Require(app, validRecharge,
            "Native bank reorder changed a live packet's cooldown state")) return false;
        if (checkpoint && std::strcmp(checkpoint, "picks") == 0 &&
            !Require(app, packet->mActive && !packet->mRefreshing && packet->mRefreshTime == 0 && packet->mRefreshCounter == 0,
                "Source fresh Fusion card did not start ready before the native intro")) return false;
        if (packet->mRefreshTime > 0) cooldownStarted.insert(packet->mNetId);
        if (!Require(app, !cooldownStarted.contains(packet->mNetId) || packet->mRefreshTime == expected,
            "Native packet cooldown reset across a snapshot/reorder")) return false;
        physical.push_back(packet);
    }
    if (!Require(app, packetBaseline.size() == static_cast<size_t>(targetCount) && current.size() == static_cast<size_t>(targetCount), "Native bank identity baseline is incomplete")) return false;
    std::sort(physical.begin(), physical.end(), [](const auto* a, const auto* b) {
        return a->mY < b->mY || (a->mY == b->mY && a->mX+a->mOffsetX < b->mX+b->mOffsetX);
    });
    int previousRank = -1, own = 0;
    for (size_t index = 0; index < physical.size(); ++index)
    {
        const auto* data = GetNativePacketData(physical[index]);
        const int rank = LawnNet::NetSeedBankOwnerRank(viewer, data->owner);
        if (!Require(app, rank >= previousRank && LawnNet::NetSeedBankOwnerAt(viewer, rank) == data->owner &&
            (index >= static_cast<size_t>(ownQuota) || data->owner == viewer), "Native physical bank is not local-owner-first")) return false;
        own += data->owner == viewer; previousRank = rank;
        if (record)
            LawnNet::NetTrace("SMOKE_FUSION_NATIVE_PACKET_ORDER role=%s checkpoint=%s index=%zu packet=%u selection=%u owner=%u sourceCD=%d liveCD=%d refreshing=%d kind=%u extra=%d",
                Role(app), checkpoint, index, physical[index]->mNetId, data->selectionId, data->owner,
                static_cast<int>(std::lround(data->rechargeSeconds*100.0f)), physical[index]->mRefreshTime, physical[index]->mRefreshing ? 1 : 0, data->kind, data->isExtra ? 1 : 0);
    }
    if (!Require(app, own == ownQuota, "Native viewer lost its ready packet quota")) return false;
    if (chooserView)
    {
        auto* chooser = app->mSeedChooserScreen;
        if (!Require(app, chooser && chooser->mNativeFusionChooser && chooser->mSeedsInFlight == 0, "Native chooser owner order was checked before flights settled")) return false;
        std::vector<const SeedChooserScreen::NativeFusionChoice*> choices;
        for (const auto& choice : chooser->mNativeFusionChoices) if (!choice.removed) choices.push_back(&choice);
        std::sort(choices.begin(), choices.end(), [](const auto* a, const auto* b) {
            return a->motion.mY < b->motion.mY || (a->motion.mY == b->motion.mY && a->motion.mX < b->motion.mX);
        });
        if (!Require(app, choices.size() == physical.size(), "Native chooser lost a visible bank identity")) return false;
        for (size_t index = 0; index < choices.size(); ++index)
        {
            const auto* data = GetNativePacketData(physical[index]);
            if (!Require(app, choices[index]->owner == data->owner && choices[index]->packetId == data->selectionId &&
                (index >= static_cast<size_t>(ownQuota) || choices[index]->owner == viewer), "Native chooser flights/bank disagree with local-owner-first physical packets")) return false;
        }
    }
    if (record)
    {
        LawnNet::NetTrace("SMOKE_FUSION_NATIVE_OWNER_ORDER role=%s checkpoint=%s viewer=%d ownPrefix=%d ownFirst=1 sharedOwnerPolicy=1 stableIds=1 stableCD=1 chooser=%d samples=%u",
            Role(app), checkpoint, viewer, ownQuota, chooserView ? 1 : 0, Samples(app));
        ownerProofs.insert(checkpoint);
    }
    return true;
}
bool HalfReady(LawnApp* app)
{
    auto* chooser = app->mSeedChooserScreen;
    GameButton* button = chooser ? chooser->mStartButton : nullptr;
    if (!Require(app, chooser && chooser->mCoopChooserActive && button && button->mFont && button->mLabel.find(_S("1/2 ready")) != SexyString::npos &&
        app->mNetManager->GetReadyCount() == 1 && app->mNetManager->GetReadyEligibleCount() == 2,
        "Native ready button does not display shared1/2 readiness")) return false;
    if (!Require(app, button->mFont->StringWidth(button->mLabel) == Sexy::FONT_BRIANNETOD12->StringWidth(button->mLabel) &&
        button->mFont->GetHeight() == Sexy::FONT_BRIANNETOD12->GetHeight() && button->mFont->GetAscent() == Sexy::FONT_BRIANNETOD12->GetAscent(),
        "Native ready button does not use the shared native font")) return false;
    const bool ready = app->NetIsHost();
    if (!Require(app, app->mNetManager->IsMyReady() == ready, "Native half-ready label disagrees with local shared ready state")) return false;
    for (int color : {GameButton::COLOR_LABEL, GameButton::COLOR_LABEL_HILITE})
    {
        const auto& actual = button->mColors[color];
        if (!Require(app, actual.mRed == (ready ? 115 : 255) && actual.mGreen == (ready ? 255 : 110) && actual.mBlue == (ready ? 110 : 95),
            "Native ready button does not use shared local-ready colors")) return false;
    }
    return true;
}
Coin* FindSun(Board* board, int value, uint16_t id = 0)
{
    Coin* best = nullptr; Coin* coin = nullptr;
    while (board->mCoins.IterateNext(coin))
    {
        if (coin->mDead || !coin->IsSun() || coin->GetSunValue() != value || (id && coin->mNetId != id)) continue;
        if (!id && (!coin->mHitGround || coin->mIsBeingCollected || coin->mCoinMotion != COIN_MOTION_FROM_PLANT)) continue;
        if (!best || coin->mNetId < best->mNetId) best = coin;
    }
    return best;
}
constexpr int kSunPointMargin = 8;
bool CoinHits(Coin* coin, int x, int y)
{
    HitResult hit{};
    return coin->MouseHitTest(x, y, &hit);
}
bool IsolatedSunPoint(LawnApp* app, Coin* target, int x, int y, int margin = kSunPointMargin)
{
    Board* board = app->mBoard;
    if (!target || !target->mNetId || target->mDead || target->mIsBeingCollected || !target->mHitGround ||
        target->mCoinMotion != COIN_MOTION_FROM_PLANT || target->GetDisappearTime()-target->mDisappearCounter < 200 ||
        x+board->mX+WideX(app)-margin < 0 || x+board->mX+WideX(app)+margin >= app->mWidth ||
        y+board->mY+WideY(app)-margin < 0 || y+board->mY+WideY(app)+margin >= app->mHeight) return false;
    // Native Coin hit regions are rectangles. All four corners must be inside
    // the target; every integer point of the margin must exclude every other
    // coin, including a different sun value or an airborne coin behind it.
    for (int dx : {-margin, margin})
        for (int dy : {-margin, margin}) if (!CoinHits(target, x+dx, y+dy)) return false;
    Coin* other = nullptr;
    while (board->mCoins.IterateNext(other))
    {
        if (other == target || other->mDead || other->mIsBeingCollected) continue;
        if (CoinHits(other, x, y)) return false;
        for (int dx = -margin; dx <= margin; ++dx)
            for (int dy = -margin; dy <= margin; ++dy)
                if (CoinHits(other, x+dx, y+dy)) return false;
    }
    HitResult hit{};
    return board->MouseHitTest(x, y, &hit) && hit.mObjectType == OBJECT_TYPE_COIN && hit.mObject == target;
}
bool FindIsolatedSunPoint(LawnApp* app, int value)
{
    std::vector<Coin*> candidates;
    Coin* coin = nullptr;
    while (app->mBoard->mCoins.IterateNext(coin))
        if (!coin->mDead && coin->IsSun() && coin->GetSunValue() == value && coin->mHitGround && !coin->mIsBeingCollected)
            candidates.push_back(coin);
    std::sort(candidates.begin(), candidates.end(), [](const auto* a, const auto* b) { return a->mNetId < b->mNetId; });
    for (Coin* candidate : candidates)
    {
        // Include the native normal-sun padding; MouseHitTest remains the
        // authority rather than assuming that the visual center is isolated.
        for (int y = static_cast<int>(std::floor(candidate->mPosY))-15; y <= candidate->mPosY+candidate->mHeight+15; y += 4)
            for (int x = static_cast<int>(std::floor(candidate->mPosX))-15; x <= candidate->mPosX+candidate->mWidth+15; x += 4)
                if (IsolatedSunPoint(app, candidate, x, y, kSunPointMargin+4))
                {
                    sunPickup = {value, x, y, candidate->mNetId, GetTickCount(), {}};
                    char message[100]; sprintf_s(message, "FUSION_NATIVE_SUN%d_POINT:%u:%d:%d", value, candidate->mNetId, x, y);
                    sunPickup.message = message;
                    return true;
                }
    }
    return false;
}
bool TraceSunPoint(LawnApp* app, Coin* target, int value)
{
    int count = 0; std::string ids;
    Coin* coin = nullptr;
    while (app->mBoard->mCoins.IterateNext(coin)) if (CoinHits(coin, sunPickup.x, sunPickup.y))
    {
        if (!ids.empty()) ids += ',';
        ids += std::to_string(coin->mNetId); ++count;
    }
    LawnNet::NetTrace("SMOKE_FUSION_NATIVE_SUN_POINT role=%s id=%u amount=%d x=%d y=%d hitCount=%d hitIds=%s margin=%d grounded=%d isolated=1 samples=%u",
        Role(app), target->mNetId, value, sunPickup.x, sunPickup.y, count, ids.c_str(), app->NetIsHost() ? kSunPointMargin+4 : kSunPointMargin,
        target->mHitGround ? 1 : 0, Samples(app));
    return Require(app, count == 1 && ids == std::to_string(target->mNetId), "Native duplicate-click sun point is not isolated");
}
bool PrepareSunClicks(LawnApp* app, int value)
{
    if (sunPickup.value != value) sunPickup = {};
    if (app->NetIsHost())
    {
        Coin* target = FindSun(app->mBoard, value, sunPickup.id);
        if (sunPickup.message.empty() || !IsolatedSunPoint(app, target, sunPickup.x, sunPickup.y, kSunPointMargin+4))
        {
            sunPickup = {};
            if (!FindIsolatedSunPoint(app, value)) return false;
            target = FindSun(app->mBoard, value, sunPickup.id);
        }
        if (GetTickCount()-sunPickup.stableSince < 200) return false;
        Send(app, sunPickup.message.c_str());
        const std::string ack = sunPickup.message+":ACK", go = sunPickup.message+":GO";
        if (!Has(app, 1, ack.c_str())) return false;
        Send(app, go.c_str());
        if (!Has(app, 0, go.c_str()) || !TraceSunPoint(app, target, value)) return false;
        sunId = target->mNetId;
        return true;
    }
    const std::string prefix = "FUSION_NATIVE_SUN"+std::to_string(value)+"_POINT:";
    std::string proposal;
    for (auto line = app->mNetManager->mChatLines.rbegin(); line != app->mNetManager->mChatLines.rend(); ++line)
        if (line->mSlot == 0 && line->mText.starts_with(prefix) && !line->mText.ends_with(":GO"))
        { proposal = line->mText; break; }
    if (proposal.empty()) return false;
    if (proposal != sunPickup.message)
    {
        unsigned id = 0; int amount = 0, x = 0, y = 0; char tail = 0;
        if (!Require(app, sscanf_s(proposal.c_str(), "FUSION_NATIVE_SUN%d_POINT:%u:%d:%d%c", &amount, &id, &x, &y, &tail, 1U) == 4 &&
            amount == value && id > 0 && id <= 65535, "Native host sun point proposal is malformed")) return false;
        sunPickup = {value, x, y, static_cast<uint16_t>(id), 0, proposal};
    }
    Coin* target = FindSun(app->mBoard, value, sunPickup.id);
    if (!IsolatedSunPoint(app, target, sunPickup.x, sunPickup.y)) { sunPickup.stableSince = 0; return false; }
    if (!sunPickup.stableSince) sunPickup.stableSince = GetTickCount();
    if (GetTickCount()-sunPickup.stableSince < 200) return false;
    const std::string ack = proposal+":ACK", go = proposal+":GO";
    Send(app, ack.c_str());
    if (!Has(app, 0, go.c_str()) || !TraceSunPoint(app, target, value)) return false;
    sunId = target->mNetId;
    LawnNet::NetTrace("SMOKE_FUSION_NATIVE_SUN_DOUBLE_CLICK role=client id=%u amount=%d x=%d y=%d clicks=2 nativeSDL=1", sunId, value, sunPickup.x, sunPickup.y);
    return BoardClick(app, (float)sunPickup.x, (float)sunPickup.y) && BoardClick(app, (float)sunPickup.x, (float)sunPickup.y);
}
void ObserveSunStates(LawnApp* app)
{
    std::map<uint16_t, SunObservation> current;
    Coin* coin = nullptr;
    while (app->mBoard->mCoins.IterateNext(coin)) if (coin->mNetId && coin->IsSun())
    {
        const SunObservation now{coin->GetSunValue(), coin->mIsBeingCollected, coin->mDead};
        const auto old = sunObservations.find(coin->mNetId);
        if (old == sunObservations.end() || old->second.collecting != now.collecting || old->second.dead != now.dead)
            LawnNet::NetTrace("SMOKE_FUSION_NATIVE_SUN_STATE role=%s id=%u amount=%d collecting=%d dead=%d present=1 sun=%d",
                Role(app), coin->mNetId, now.value, now.collecting ? 1 : 0, now.dead ? 1 : 0, app->mBoard->mSunMoney);
        current.emplace(coin->mNetId, now);
    }
    for (const auto& [id, old] : sunObservations) if (!current.contains(id))
        LawnNet::NetTrace("SMOKE_FUSION_NATIVE_SUN_STATE role=%s id=%u amount=%d collecting=%d dead=1 present=0 sun=%d",
            Role(app), id, old.value, old.collecting ? 1 : 0, app->mBoard->mSunMoney);
    if (observedSunMoney != app->mBoard->mSunMoney)
        LawnNet::NetTrace("SMOKE_FUSION_NATIVE_SUN_LEDGER role=%s before=%d after=%d target=%u", Role(app), observedSunMoney, app->mBoard->mSunMoney, sunId);
    observedSunMoney = app->mBoard->mSunMoney;
    sunObservations = std::move(current);
}
void ObserveCombat(LawnApp* app)
{
    Board* board = app->mBoard;
    Projectile* projectile = nullptr;
    while (board->mProjectiles.IterateNext(projectile))
    {
        const auto* data = GetNativeProjectileData(projectile);
        if (!projectile->mDead && data && data->kind == 8 && !bulletSeen)
        {
            if (!Require(app, data->damage == 100 && projectile->mProjectileType == PROJECTILE_PEA, "Native Fusion projectile metadata disagrees with real Projectile")) return;
            bulletSeen = true;
            LawnNet::NetTrace("SMOKE_FUSION_NATIVE_COMBAT role=%s event=bullet8 id=%u damage=100 nativeActor=1", Role(app), projectile->mNetId);
        }
    }
    Coin* coin = nullptr;
    while (board->mCoins.IterateNext(coin)) if (!coin->mDead && coin->IsSun() && coin->GetSunValue() == 5 && !littleSunSeen)
    {
        littleSunSeen = true;
        LawnNet::NetTrace("SMOKE_FUSION_NATIVE_COMBAT role=%s event=sun5 id=%u nativeCoin=1", Role(app), coin->mNetId);
    }
    bool alive = false;
    Zombie* zombie = nullptr;
    while (board->mZombies.IterateNext(zombie)) if (!zombie->mDead && zombie->mZombieType == ZOMBIE_NORMAL && zombie->mRow == 2)
    {
        alive = true;
        if (!zombieId) { zombieId = zombie->mNetId; LawnNet::NetTrace("SMOKE_FUSION_NATIVE_ZOMBIE role=%s id=%u row=2 nativeActor=1", Role(app), zombieId); }
        if (!zombie->mHasHead)
        {
            if (!headlessSeen)
            {
                headlessSeen = true; headlessSince = GetTickCount();
                LawnNet::NetTrace("SMOKE_FUSION_NATIVE_COMBAT role=%s event=headless id=%u nativeActor=1", Role(app), zombieId);
            }
            if (GetTickCount() - headlessSince >= 300) Capture(app, "fusion_native_headless");
        }
    }
    if (zombieId && !alive && !deathSeen)
    {
        deathSeen = true;
        LawnNet::NetTrace("SMOKE_FUSION_NATIVE_COMBAT role=%s event=death id=%u nativeActor=1", Role(app), zombieId);
    }
}
bool Pool(LawnApp* app, const NativeContext& context, bool trace=false)
{
    static uint32_t tracedEpoch=0;
    const bool record=trace && tracedEpoch!=context.epoch;
    const auto& state=context.selection.State();
    const size_t target=(std::min)(kNativeFusionBankCapacity,context.eligible.size());
    if (!Require(app,state.activeMask==3 && state.targetCount==target &&
        state.quotas[0]+state.quotas[1]==target && state.quotas[0]>=4 && state.quotas[1]>=4,
        "Native fixture live-pool target/quotas do not retain mandatory baseline coverage")) return false;
    bool mandatory[4][2]={};
    const FusionPlantId baseline[]={0,1,2,4};
    std::set<std::pair<FusionPlantId,bool>> identities;
    for (size_t i=0;i<context.eligible.size();++i)
    {
        const auto& card=context.eligible[i]; const auto* definition=FusionFindPlant(card.kind);
        FusionBankEligibleCard sourceCard;
        if (!Require(app,definition && definition->hasStats && GetNativeSeedForPlant(card.kind)!=SEED_NONE &&
            identities.emplace(card.kind,card.isExtra).second &&
            TryMakeFusionEligibleCard(card.kind,card.isExtra,sourceCard) && card==sourceCard &&
            card.rechargeSeconds==definition->rechargeSeconds && definition->maxHealth>0,
            "Native fixture eligibility disagrees with source definitions/aliases or repeats an identity")) return false;
        for (size_t j=0;j<4;++j) if (card.kind==baseline[j]) mandatory[j][card.isExtra?1:0]=true;
        if (record) LawnNet::NetTrace("SMOKE_FUSION_NATIVE_ELIGIBLE role=%s index=%zu kind=%u extra=%d baseCost=%d sourceCD=%d maxHP=%d alias=%d",
            Role(app),i,card.kind,card.isExtra?1:0,card.baseCost,static_cast<int>(std::lround(card.rechargeSeconds*100.0f)),
            definition->maxHealth,static_cast<int>(GetNativeSeedForPlant(card.kind)));
    }
    for (const auto& pair:mandatory) if (!Require(app,pair[0] && pair[1],"Native fixture lost mandatory0/1/2/4 normal/extra eligibility")) return false;
    if (record) LawnNet::NetTrace("SMOKE_FUSION_NATIVE_POOL_LAYOUT role=%s eligible=%zu target=%u quota0=%u quota1=%u mandatory=0,1,2,4 unfiltered=1",
        Role(app),context.eligible.size(),state.targetCount,state.quotas[0],state.quotas[1]);
    if (record) tracedEpoch=context.epoch;
    return true;
}
std::vector<int> ManualChoices(const NativeContext& context, int owner)
{
    std::vector<int> choices;
    for (size_t i=0;i<context.eligible.size();++i)
    {
        const auto& card=context.eligible[i];
        if (card.isExtra!=(owner==1)) continue;
        const auto& selected=context.selection.State().packets;
        if (std::none_of(selected.begin(),selected.end(),[&](const auto& packet) {
            return packet.kind==card.kind && packet.isExtra==card.isExtra;
        })) choices.push_back(static_cast<int>(i));
    }
    const auto priority=[](FusionPlantId kind) { return kind==0?0:(kind==1?1:(kind==2?2:(kind==4?3:4))); };
    std::stable_sort(choices.begin(),choices.end(),[&](int a,int b) {
        return priority(context.eligible[a].kind)<priority(context.eligible[b].kind);
    });
    return choices;
}

}

namespace LawnNet
{
bool NetSmokeFusionResourceHostLost(LawnApp* app,const std::string& reason)
{
    if(!ResourceEnabled() || !app || !app->CoopSmokeActive() ||
        app->mCoopSmokeManifest.mScenario!=NETSMOKE_SCENARIO_FUSION_VALIDATION || !resource.clientProbe ||
        !app->mNetManager || app->mNetManager->GetMySlot()!=1 ||
        reason!="Fusion resources could not be loaded. Check your game installation.")return false;
    resource.expectedLost=true;
    NetTrace("SMOKE_FUSION_RESOURCE_DEFERRED role=client reason=FUSION_LOCAL_RESOURCE boardAliveBeforeTeardown=%d",app->mBoard?1:0);
    return true;
}
bool NetSmokeUpdateNativeFusion(LawnApp* app)
{
    const char* enabled = std::getenv("PVZ_COOP_SMOKE_FUSION_NATIVE");
    if (!enabled || std::strcmp(enabled, "1") || !app || !app->CoopSmokeActive() ||
        app->mCoopSmokeManifest.mScenario != NETSMOKE_SCENARIO_FUSION_VALIDATION) return false;
    if (app->mCoopSmokeResultWritten) return true;
    if (!since) since = GetTickCount();
    const uint32_t limit = stage == 0 || stage == 1 || (ResourceEnabled() && stage >= 60) ? 60000U : stage == 3 || stage == 13 || stage == 16 || stage == 18 ? 35000U : 15000U;
    if (GetTickCount() - since > limit)
    {
        char error[128]; sprintf_s(error, "Native Fusion fixture deadline stage=%d role=%s", stage, Role(app));
        app->CoopSmokeFail(error); return true;
    }
    if (ResourceEnabled() && ResourceLossFinished(app)) return true;
    if (!app->mNetManager || !app->mNetManager->IsActive()) return true;
    app->mCoopSmokePeerReady = true;
    if (stage == 0)
    {
        if (app->mNetManager->GetPlayerCount() != 2 || (app->NetIsClient() && !app->mNetManager->IsConnectedToHost())) return true;
        if (app->NetIsClient()) { Advance(app, 1); Send(app, "FUSION_NATIVE_START_READY"); }
        else if (Has(app, 1, "FUSION_NATIVE_START_READY"))
        {
            Advance(app, 1); std::string error;
            if (!app->StartFusionValidation(&error)) app->CoopSmokeFail(error.empty() ? "Native Fusion StartFusionValidation failed" : error.c_str());
        }
        return true;
    }
    if (!app->mBoard || !IsNativeFusionBoard(app->mBoard))
    { if (stage == 1 && app->NetIsClient()) Send(app, "FUSION_NATIVE_START_READY"); return true; }
    Board* board = app->mBoard;
    const auto* context = GetNativeContext(board);
    if (!Require(app, !app->mFusionRuntime && context && context->validation, "Native Fusion fixture entered a second-world runtime")) return true;
    if (!geometryProved && !NativeGrid(app)) return true;
    const auto& selection = context->selection.State();
    if (ResourceEnabled() && stage >= 60) { ResourceUpdate(app); return true; }
    if (packetBaseline.size() == selection.targetCount && !OwnerFirst(app)) return true;
    if (stage >= 12) ObserveSunStates(app);
    if (stage >= 13) ObserveCombat(app);
    if (stage == 1)
    {
        auto* chooser = app->mSeedChooserScreen;
        if (!chooser || !chooser->mNativeFusionChooser || !chooser->mMouseVisible || !board->mCutScene->mSeedChoosing || !Settled()) return true;
        if (!Require(app, selection.phase == FusionBankPhase::Choosing && Pool(app,*context,true) && board->mSunMoney == 500,
            "Native Fusion chooser initial quota/eligibility/sun mismatch") || !Capture(app, "fusion_native_chooser")) return true;
        NetTrace("SMOKE_FUSION_NATIVE_CHOOSER role=%s epoch=%u target=%u quotas=%u,%u nativeWidget=1 nativeGrid=10x5 secondWorld=0", Role(app), context->epoch,selection.targetCount,selection.quotas[0],selection.quotas[1]);
        Advance(app, 2);
        const int owner=app->mNetManager->GetMySlot();
        const auto choices=ManualChoices(*context,owner);
        if (!Require(app,choices.size()>=selection.quotas[owner],"Native chooser cannot fill its observed personal quota")) return true;
        for (int pick=0;pick<selection.quotas[owner];++pick)
        {
            int x, y; chooser->GetSeedPositionInChooser(choices[pick], x, y);
            Click(app, (float)(chooser->mX + x + 25), (float)(chooser->mY + y + 35 - (int)chooser->mScrollbar->mScrollValue));
        }
    }
    else if (stage == 2)
    {
        auto* chooser = app->mSeedChooserScreen;
        if (selection.packets.size() != selection.targetCount || !chooser || chooser->mSeedsInFlight || chooser->mStartButton->mDisabled) return true;
        if (!Require(app, Packet(board, 0, 0, false) && Packet(board, 0, 1, false) && Packet(board, 1, 0, true) && Packet(board, 1, 1, true) &&
            Packet(board, 0, 2, false) && Packet(board, 1, 2, true) && Packet(board, 0, 4, false) && Packet(board, 1, 4, true),
            "Native Fusion personal normal/extra identities not represented by real SeedPackets") || !Settled() ||
            !OwnerFirst(app, "picks", true) || !Capture(app, "fusion_native_picks")) return true;
        if (app->NetIsClient())
        {
            // Do not ready both peers in one polling frame. The host must have
            // rendered the half-ready state before the client observes/captures
            // it and then sends its ordinary native Ready button click.
            if (selection.readyMask != 1 || !Has(app, 0, "FUSION_NATIVE_HALF_CAPTURED")) return true;
            if (!HalfReady(app) || !OwnerFirst(app, "ready_half", true)) return true;
            const bool first = !frames.contains("fusion_native_ready_half");
            if (!Capture(app, "fusion_native_ready_half")) return true;
            if (first) NetTrace("SMOKE_FUSION_NATIVE_READY_HALF role=client mask=1 label=1/2_ready sharedFont=1 sharedColors=1 localReady=0 hostCaptureSeen=1 nativeSDL=1");
            Send(app, "FUSION_NATIVE_HALF_SEEN");
            if (!Has(app, 1, "FUSION_NATIVE_HALF_SEEN")) return true;
        }
        if (!Button(app, chooser->mStartButton)) return true;
        Advance(app, 3);
    }
    else if (stage == 3)
    {
        if (selection.phase == FusionBankPhase::Choosing)
        {
            if (app->NetIsHost() && selection.readyMask == 1)
            {
                if (!Settled() || !HalfReady(app) || !OwnerFirst(app, "ready_half", true)) return true;
                const bool first = !frames.contains("fusion_native_ready_half");
                if (!Capture(app, "fusion_native_ready_half")) return true;
                if (first) NetTrace("SMOKE_FUSION_NATIVE_READY_HALF role=host mask=1 label=1/2_ready sharedFont=1 sharedColors=1 localReady=1 hostFirst=1 nativeSDL=1");
                Send(app, "FUSION_NATIVE_HALF_CAPTURED");
            }
            return true;
        }
        if (selection.phase == FusionBankPhase::Intro)
        {
            if (!Require(app, frames.contains("fusion_native_ready_half") &&
                (!app->NetIsHost() || Has(app, 1, "FUSION_NATIVE_HALF_SEEN")), "Native intro bypassed both viewers' real half-ready capture")) return true;
            if (!introPhaseObserved) { introPhaseObserved = true; observedSince = 0; }
            if (Settled() && Capture(app, "fusion_native_intro") && !introSeen)
            { introSeen = true; NetTrace("SMOKE_FUSION_NATIVE_INTRO role=%s epoch=%u nativeCutScene=1", Role(app), context->epoch); }
            return true;
        }
        if (selection.phase != FusionBankPhase::Playing || app->mGameScene != SCENE_PLAYING || app->mSeedChooserScreen) return true;
        if (!Require(app, introSeen && board->mSunMoney == 500, "Native Fusion skipped full intro or changed sun") ||
            !OwnerFirst(app, "playing") || !Capture(app, "fusion_native_playing")) return true;
        NetTrace("SMOKE_FUSION_NATIVE_PLAYING role=%s epoch=%u nativeBoard=1 nativeBank=1 secondWorld=0", Role(app), context->epoch);
        Advance(app, 4);
    }
    else if (stage == 4)
    {
        if (!Barrier(app, "FUSION_NATIVE_PLAYING")) return true;
        Advance(app, 5);
        if (app->NetIsHost()) { Pick(app, Packet(board, 0, 0, false)); Cell(app, 3, 2); }
    }
    else if (stage == 5)
    {
        Plant* plant = At(board, 3, 2); const auto* data = GetNativePlantData(plant);
        if (!plant || !data) return true;
        if (!Require(app, data->kind == 0 && data->owner == 0 && plant->mSeedType == SEED_PEASHOOTER && plant->mPlantHealth == 300 && board->mSunMoney == 400,
            "Native host Peashooter did not use real Plant and canonical price") || !Settled() || !Capture(app, "fusion_native_peashooter")) return true;
        originalPlant = plant->mNetId;
        NetTrace("SMOKE_FUSION_NATIVE_PLANT role=%s id=%u kind=0 sun=400 nativeActor=1 nativeSDL=1", Role(app), originalPlant);
        Advance(app, 6);
    }
    else if (stage == 6)
    {
        if (!Barrier(app, "FUSION_NATIVE_PEA")) return true;
        Advance(app, 60);
        if (app->NetIsClient())
        {
            SeedPacket* packet = Packet(board, 1, 1, true);
            if (!Pick(app, packet)) return true;
            heldPacketId = packet->mNetId;
        }
    }
    else if (stage == 60)
    {
        if (app->NetIsClient())
        {
            // Native co-op pickup is host-authoritative. Wait for the actual
            // held cursor echo before aiming the next physical mouse click.
            if (!HoldingPacket(board, heldPacketId) || !Settled()) return true;
            SeedPacket* packet = Packet(board, 1, 1, true);
            if (!Require(app, packet && packet->mNetId == heldPacketId && packet->mActive && !packet->mRefreshing &&
                At(board, 3, 2) && At(board, 3, 2)->mNetId == originalPlant && board->mSunMoney == 400,
                "Native bank pickup changed currency/cooldown or lost the observed fusion target") || !Capture(app, "fusion_native_client_card_held")) return true;
            NetTrace("SMOKE_FUSION_NATIVE_HELD role=client packet=%u target=%u cursorEcho=1 charge=0 recharge=0 nativeSDL=1", heldPacketId, originalPlant);
            if (!Cell(app, 3, 2)) return true;
        }
        Advance(app, 7);
    }
    else if (stage == 7)
    {
        Plant* plant = At(board, 3, 2); const auto* data = GetNativePlantData(plant);
        if (!data || data->kind == 0) return true;
        SeedPacket* used = Packet(board, 1, 1, true);
        if (!Require(app, data->kind == 1000 && plant->mNetId != originalPlant && data->firstParent == 0 && data->secondParent == 1 && data->owner == 1 &&
            plant->mSeedType == SEED_PEASHOOTER && plant->mPlantHealth == 300 && board->mSunMoney == 300 && used && data->packetId == used->mNetId &&
            !used->mActive && used->mRefreshing && GetNativePacketData(used)->price == 100,
            "Native Fusion did not atomically replace parent with source1000 and charge/recharge used real packet") || !Settled() ||
            !OwnerFirst(app, "fused") || !Capture(app, "fusion_native_fused")) return true;
        fusedPlant = plant->mNetId;
        NetTrace("SMOKE_FUSION_NATIVE_FUSION role=%s old=%u id=%u kind=1000 parents=0,1 hp=300 sun=300 owner=1 price=100 nativeActor=1", Role(app), originalPlant, fusedPlant);
        Advance(app, 8);
    }
    else if (stage == 8)
    {
        if (!Barrier(app, "FUSION_NATIVE_FUSED")) return true;
        Advance(app, 9);
        if (app->NetIsHost()) { Pick(app, Packet(board, 0, 1, false)); Cell(app, 1, 1); }
    }
    else if (stage == 9)
    {
        Plant* sunflower = At(board, 1, 1); const auto* data = GetNativePlantData(sunflower);
        if (!data) return true;
        if (!Require(app, data->kind == 1 && data->owner == 0 && sunflower->mSeedType == SEED_SUNFLOWER && board->mSunMoney == 250,
            "Native source Sunflower purchase/owner mismatch")) return true;
        sunflowerId = sunflower->mNetId;
        if (!Barrier(app, "FUSION_NATIVE_SUNFLOWER")) return true;
        staleSamples = Samples(app); Advance(app, 10);
        if (app->NetIsClient())
        {
            SeedPacket* packet = Packet(board, 1, 0, true);
            if (!Require(app, packet && packet->mActive && !packet->mRefreshing, "Native stale intent unused extra Pea is not ready")) return true;
            FusionCommand command; command.action = FusionCommandKind::Plant; command.packetId = packet->mNetId;
            command.column = 1; command.row = 1; command.targetId = originalPlant;
            Require(app, app->SubmitNativeFusionCommand(command), "Native stale intent did not enter authenticated stream");
            NetTrace("SMOKE_FUSION_NATIVE_STALE_SENT role=client old=%u target=%u packet=%u validRecipe=1", originalPlant, sunflowerId, packet->mNetId);
        }
    }
    else if (stage == 10)
    {
        if (GetTickCount() - since < 1000 || Samples(app) - staleSamples < 2) return true;
        Plant* sunflower = At(board, 1, 1); SeedPacket* packet = Packet(board, 1, 0, true);
        if (!Require(app, sunflower && sunflower->mNetId == sunflowerId && At(board, 3, 2) && At(board, 3, 2)->mNetId == fusedPlant &&
            board->mSunMoney == 250 && packet && packet->mActive && !packet->mRefreshing,
            "Native stale expected target consumed currency/cooldown or replaced live target")) return true;
        NetTrace("SMOKE_FUSION_NATIVE_STALE role=%s target=%u fused=%u sun=250 charge=0 recharge=0", Role(app), sunflowerId, fusedPlant);
        Advance(app, 11);
    }
    else if (stage == 11)
    {
        if (!Barrier(app, "FUSION_NATIVE_STALE_OK")) return true;
        Advance(app, 12);
        if (app->NetIsHost())
        {
            Zombie* zombie = board->AddZombieInRow(ZOMBIE_NORMAL, 2, 0);
            if (!Require(app, zombie != nullptr, "Native validation zombie setup failed")) return true;
            zombie->mPosX = (float)(board->GridToPixelX(7, 2) + 20);
        }
    }
    else if (stage == 12)
    {
        ObserveCombat(app);
        if (!zombieId || !Settled() || !OwnerFirst(app, "combat") || !Capture(app, "fusion_native_zombie_walk")) return true;
        Advance(app, 13);
    }
    else if (stage == 13)
    {
        if (!bulletSeen || !littleSunSeen || !PrepareSunClicks(app, 5)) return true;
        Advance(app, 14);
    }
    else if (stage == 14)
    {
        if (!Require(app, board->mSunMoney >= 250 && board->mSunMoney <= 255, "Native five-sun collection credited more than once or changed currency unexpectedly")) return true;
        if (board->mSunMoney != 255) return true;
        if (!Settled(1000)) return true;
        NetTrace("SMOKE_FUSION_NATIVE_COLLECT role=%s id=%u amount=5 before=250 after=255 doubleClickSingleCredit=1 nativeCoin=1 nativeSDL=1", Role(app), sunId);
        Advance(app, 15);
    }
    else if (stage == 15)
    {
        if (!Barrier(app, "FUSION_NATIVE_SUN5_COLLECTED")) return true;
        Advance(app, 16);
    }
    else if (stage == 16)
    {
        if (!PrepareSunClicks(app, 25)) return true;
        Advance(app, 17);
    }
    else if (stage == 17)
    {
        if (!Require(app, board->mSunMoney >= 255 && board->mSunMoney <= 280, "Native twenty-five-sun collection credited more than once or changed currency unexpectedly")) return true;
        if (board->mSunMoney != 280) return true;
        if (!Settled(1000) || !Capture(app, "fusion_native_sun_collected")) return true;
        NetTrace("SMOKE_FUSION_NATIVE_COLLECT role=%s id=%u amount=25 before=255 after=280 doubleClickSingleCredit=1 nativeCoin=1 nativeSDL=1", Role(app), sunId);
        Advance(app, 18);
    }
    else if (stage == 18)
    {
        if (!Barrier(app, "FUSION_NATIVE_COLLECTION_OK") || !headlessSeen || !deathSeen || !frames.contains("fusion_native_headless")) return true;
        Advance(app, 19);
        if (app->NetIsHost()) Click(app, board->GetShovelButtonRect());
    }
    else if (stage == 19)
    {
        if (app->NetIsHost())
        {
            if (board->mCursorObject->mCursorType != CURSOR_TYPE_SHOVEL || !Settled() || !Capture(app, "fusion_native_shovel_held")) return true;
            Key(app, SDLK_ESCAPE);
        }
        Advance(app, 20);
    }
    else if (stage == 20)
    {
        if (app->NetIsHost())
        {
            if (board->mCursorObject->mCursorType != CURSOR_TYPE_NORMAL || !Settled()) return true;
            if (!Require(app, At(board, 1, 1) && At(board, 1, 1)->mNetId == sunflowerId && board->mSunMoney == 280 && !app->GetDialogCount(), "Native Escape shovel cancel mutated world or opened a dialog")) return true;
            Click(app, board->GetShovelButtonRect());
        }
        Advance(app, 21);
    }
    else if (stage == 21)
    {
        if (app->NetIsHost())
        {
            if (board->mCursorObject->mCursorType != CURSOR_TYPE_SHOVEL || !Settled()) return true;
            BoardClick(app, (float)board->GetCellCenterX(1, 1), (float)(board->GridToPixelY(1, 1)+50), SDL_BUTTON_RIGHT);
        }
        Advance(app, 22);
    }
    else if (stage == 22)
    {
        if (app->NetIsHost())
        {
            if (board->mCursorObject->mCursorType != CURSOR_TYPE_NORMAL || !Settled()) return true;
            if (!Require(app, At(board, 1, 1) && At(board, 1, 1)->mNetId == sunflowerId && board->mSunMoney == 280, "Native right-click shovel cancel mutated world")) return true;
            NetTrace("SMOKE_FUSION_NATIVE_SHOVEL_CANCEL role=host escape=1 right=1 worldUntouched=1 nativeSDL=1");
            Click(app, board->GetShovelButtonRect());
        }
        Advance(app, 23);
    }
    else if (stage == 23)
    {
        if (app->NetIsHost())
        {
            if (board->mCursorObject->mCursorType != CURSOR_TYPE_SHOVEL || !Settled()) return true;
            Cell(app, 1, 1);
        }
        Advance(app, 24);
    }
    else if (stage == 24)
    {
        if (At(board, 1, 1)) return true;
        if (!Require(app, At(board, 3, 2) && At(board, 3, 2)->mNetId == fusedPlant && board->mSunMoney == 280, "Native shovel changed another plant or currency") || !Settled() || !Capture(app, "fusion_native_complete")) return true;
        NetTrace("SMOKE_FUSION_NATIVE_SHOVEL role=%s removed=%u fused=%u sun=280 nativeSDL=1", Role(app), sunflowerId, fusedPlant);
        Advance(app, 25);
    }
    else if (stage == 25)
    {
        if (!Barrier(app, "FUSION_NATIVE_FINAL_ACK") || Samples(app) < 8 || !Settled()) return true;
        Advance(app, 80);
    }
    else if (stage == 80)
    {
        if (app->NetIsClient() && !decoderProved)
        {
            if (!DecoderRejectsGrid(app, true) || !DecoderRejectsGrid(app, false)) return true;
            decoderProved = true;
        }
        SeedPacket* host = Packet(board, 0, 1, false), *client = Packet(board, 1, 0, true);
        if (!Require(app, board->mSunMoney == 280 && At(board, 3, 2) && At(board, 3, 2)->mNetId == fusedPlant &&
            host && client && host->mActive && !host->mRefreshing && client->mActive && !client->mRefreshing &&
            GetNativePacketData(host)->price == 50 && GetNativePacketData(client)->price == 200 &&
            !app->mNetManager->mConfig.mFusionClassicField,
            "Native tenth-column setup lacks source prices, ready packets, default Fusion field or sun280")) return true;
        if (!Barrier(app, "FUSION_NATIVE_GRID_BOUNDARY_READY")) return true;
        gridHostPacketId = host->mNetId; gridClientPacketId = client->mNetId;
        gridPlantBaseline = LivePlantIds(board);
        for (SeedPacket* packet : {host, client})
            gridPacketBaseline.push_back({packet->mNetId, packet->mRefreshCounter, packet->mRefreshTime, packet->mTimesUsed});
        staleSamples = Samples(app); Advance(app, 81);
        if (app->NetIsClient())
        {
            // These are deliberately valid wire commands with invalid Board
            // coordinates. The authenticated host must reject before payment.
            FusionCommand command; command.action = FusionCommandKind::Plant; command.packetId = gridClientPacketId;
            command.column = 10; command.row = 4;
            if (!Require(app, app->SubmitNativeFusionCommand(command), "Native col10 negative command failed transport validation")) return true;
            NetTrace("SMOKE_FUSION_NATIVE_GRID_SENT role=client case=transport_col10 column=10 row=4 packet=%u price=200 ready=1 sun=280", gridClientPacketId);
            command.column = 9; command.row = 5;
            if (!Require(app, app->SubmitNativeFusionCommand(command), "Native row5 negative command failed transport validation")) return true;
            NetTrace("SMOKE_FUSION_NATIVE_GRID_SENT role=client case=transport_row5 column=9 row=5 packet=%u price=200 ready=1 sun=280", gridClientPacketId);
        }
    }
    else if (stage == 81)
    {
        if (!GridRejected(app, "transport_bounds", true, 10, 5, -1, -1)) return true;
        Advance(app, 82);
        if (app->NetIsClient()) Pick(app, Packet(board, 1, 0, true));
    }
    else if (stage == 82)
    {
        if (!GridUnchanged(app)) return true;
        if (app->NetIsClient())
        {
            if (!HoldingPacket(board, gridClientPacketId) || !Settled()) return true;
            if (!EmptyClickPoint(app, 760, 530) || !BoardClick(app, 760.0f, 530.0f)) return true;
            NetTrace("SMOKE_FUSION_NATIVE_GRID_MOUSE role=client case=right_gutter x=760 y=530 heldPacket=%u nativeSDL=1", gridClientPacketId);
        }
        staleSamples = Samples(app); Advance(app, 83);
    }
    else if (stage == 83)
    {
        if (app->NetIsClient() && !Require(app, HoldingPacket(board, gridClientPacketId), "Native right-gutter click lost the held ready packet")) return true;
        if (!GridRejected(app, "right_gutter", false, -1, 4, 760, 530)) return true;
        if (app->NetIsClient())
        {
            if (!EmptyClickPoint(app, 724, 580) || !BoardClick(app, 724.0f, 580.0f)) return true;
            NetTrace("SMOKE_FUSION_NATIVE_GRID_MOUSE role=client case=bottom_gutter x=724 y=580 heldPacket=%u nativeSDL=1", gridClientPacketId);
        }
        staleSamples = Samples(app); Advance(app, 84);
    }
    else if (stage == 84)
    {
        if (app->NetIsClient() && !Require(app, HoldingPacket(board, gridClientPacketId), "Native bottom-gutter click lost the held ready packet")) return true;
        if (!GridRejected(app, "bottom_gutter", false, 9, -1, 724, 580)) return true;
        if (app->NetIsClient()) BoardClick(app, 724.0f, 530.0f, SDL_BUTTON_RIGHT);
        Advance(app, 85);
    }
    else if (stage == 85)
    {
        if (!gridProofs.contains("boundary_cancelled"))
        {
            if (!GridUnchanged(app)) return true;
            if (app->NetIsClient() && board->mCursorObject->mCursorType != CURSOR_TYPE_NORMAL) return true;
            gridProofs.insert("boundary_cancelled");
        }
        if (!Barrier(app, "FUSION_NATIVE_GRID_BOUNDARY_OK")) return true;
        Advance(app, 86);
        if (app->NetIsHost()) Pick(app, Packet(board, 0, 1, false));
    }
    else if (stage == 86)
    {
        if (app->NetIsHost())
        {
            if (!HoldingPacket(board, gridHostPacketId) || !Settled()) return true;
            if (!Cell(app, 9, 4)) return true;
        }
        Advance(app, 87);
    }
    else if (stage == 87)
    {
        Plant* plant = At(board, 9, 4); const auto* data = GetNativePlantData(plant);
        if (!data) return true;
        SeedPacket* packet = Packet(board, 0, 1, false);
        if (!Require(app, data->kind == 1 && data->owner == 0 && data->randomOrigin == 0 && data->packetId == gridHostPacketId &&
            plant->mSeedType == SEED_SUNFLOWER && plant->mPlantHealth == 300 && board->mSunMoney == 230 &&
            plant->mX == 684 && plant->mY == 480 && packet && packet->mNetId == gridHostPacketId &&
            packet->mRefreshing && !packet->mActive && packet->mRefreshTime == 750,
            "Native tenth-column Sunflower is not a real centered source1 Plant/canonical50 purchase")) return true;
        if (!Settled() || !Capture(app, "fusion_native_col9_planted")) return true;
        gridParentId = plant->mNetId;
        NetTrace("SMOKE_FUSION_NATIVE_COL9_PLANT role=%s id=%u kind=1 column=9 row=4 x=684 y=480 center=724 owner=0 packet=%u price=50 sun=230 sourceCD=750 nativeActor=1 nativeSDL=1 samples=%u",
            Role(app), gridParentId, gridHostPacketId, Samples(app));
        Advance(app, 88);
    }
    else if (stage == 88)
    {
        if (!Barrier(app, "FUSION_NATIVE_GRID_PARENT_READY")) return true;
        Advance(app, 89);
        if (app->NetIsClient()) Pick(app, Packet(board, 1, 0, true));
    }
    else if (stage == 89)
    {
        if (app->NetIsClient())
        {
            if (!HoldingPacket(board, gridClientPacketId) || !Settled()) return true;
            if (!Require(app, At(board, 9, 4) && At(board, 9, 4)->mNetId == gridParentId && board->mSunMoney == 230,
                "Native tenth-column card pickup changed parent/currency before placement") || !Capture(app, "fusion_native_col9_card_held") || !Cell(app, 9, 4)) return true;
        }
        Advance(app, 90);
    }
    else if (stage == 90)
    {
        Plant* plant = At(board, 9, 4); const auto* data = GetNativePlantData(plant);
        if (!data || data->kind == 1) return true;
        SeedPacket* packet = Packet(board, 1, 0, true);
        if (!Require(app, plant->mNetId != gridParentId && plant->mNetId != fusedPlant && plant->mSeedType == SEED_PEASHOOTER &&
            data->kind == 1000 && data->firstParent == 1 && data->secondParent == 0 && data->owner == 1 && data->randomOrigin == 0 &&
            data->packetId == gridClientPacketId && plant->mPlantHealth == 300 && plant->mX == 684 && plant->mY == 480 &&
            board->mSunMoney == 30 && packet && packet->mNetId == gridClientPacketId && GetNativePacketData(packet)->price == 200 &&
            packet->mRefreshing && !packet->mActive && packet->mRefreshTime == 750,
            "Native tenth-column fusion lost source1+0 provenance, canonical200 purchase or native cooldown")) return true;
        if (!Settled() || !Capture(app, "fusion_native_col9_fused")) return true;
        gridFusedId = plant->mNetId;
        NetTrace("SMOKE_FUSION_NATIVE_COL9_FUSION role=%s old=%u id=%u kind=1000 parents=1,0 column=9 row=4 hp=300 owner=1 origin=0 packet=%u price=200 sun=30 sourceCD=750 nativeActor=1 nativeSDL=1 samples=%u",
            Role(app), gridParentId, gridFusedId, gridClientPacketId, Samples(app));
        Advance(app, 91);
    }
    else if (stage == 91)
    {
        if (!GridFusedUnchanged(app) || !Barrier(app, "FUSION_NATIVE_GRID_FUSED")) return true;
        Advance(app, 120);
    }
    else if (stage == 120)
    {
        if (!GridFusedUnchanged(app) || !Require(app, !app->GetDialogCount() &&
            board->mCursorObject->mCursorType == CURSOR_TYPE_NORMAL && !app->mNetManager->mConfig.mFusionClassicField,
            "FIELD cancellation setup changed cursor, skin or dialogs")) return true;
        if (app->NetIsClient() && !app->mActive) { FocusEvent(app, true); return true; }
        SeedPacket* packet = Packet(board, 1, 0, true);
        if (!Require(app, packet && packet->mRefreshTime == 750, "FIELD cancellation lost the used real packet")) return true;
        fieldCounter = packet->mRefreshCounter; fieldRefreshTime = packet->mRefreshTime; fieldWasRefreshing = packet->mRefreshing;
        if (!FieldMouse(app, true)) return true;
        Advance(app, 121);
    }
    else if (stage == 121)
    {
        if (!context->fieldButtonPressed || !context->fieldButtonReleasePending || !Settled()) return true;
        if (!Capture(app, "fusion_native_field_pressed")) return true;
        if (app->NetIsHost()) Key(app, SDLK_ESCAPE);
        else if (!FocusEvent(app, false) || !FocusEvent(app, true)) return true;
        Advance(app, 122);
    }
    else if (stage == 122)
    {
        if (context->fieldButtonPressed) return true;
        if (app->NetIsHost() ? !app->GetDialog(DIALOG_NEWOPTIONS) : !app->mActive) return true;
        if (!Require(app, context->fieldButtonReleasePending && !app->mNetManager->mConfig.mFusionClassicField,
            "Modal/focus interruption lost FIELD release ownership or toggled its skin")) return true;
        if (!FieldMouse(app, false)) return true;
        Advance(app, 123);
    }
    else if (stage == 123)
    {
        if (context->fieldButtonReleasePending || context->fieldButtonPressed) return true;
        if (app->NetIsClient() && !app->mActive) return true;
        if (!Require(app, !app->mNetManager->mConfig.mFusionClassicField,
            "Interrupted FIELD release toggled a local skin")) return true;
        if (app->NetIsHost())
        {
            if (!Require(app, app->GetDialog(DIALOG_NEWOPTIONS) != nullptr,
                "Interrupted FIELD release accidentally dismissed the real modal")) return true;
            if (!Capture(app, "fusion_native_field_cancel_modal") || !Key(app, SDLK_ESCAPE)) return true;
        }
        Advance(app, 124);
    }
    else if (stage == 124)
    {
        if (app->GetDialogCount() || (app->NetIsClient() && !app->mActive)) return true;
        if (!FieldMouse(app, false)) return true;
        Advance(app, 125);
    }
    else if (stage == 125)
    {
        if (!Settled()) return true;
        if (!Require(app, !context->fieldButtonPressed && !context->fieldButtonReleasePending &&
            !app->mNetManager->mConfig.mFusionClassicField && !app->GetDialogCount() &&
            board->mCursorObject->mCursorType == CURSOR_TYPE_NORMAL,
            "Extra FIELD release after cancellation changed skin, cursor or dialogs") || !GridFusedUnchanged(app) || !FieldCooldownProgressed(app)) return true;
        if (gridProofs.insert("field_cancel").second)
            NetTrace("SMOKE_FUSION_NATIVE_FIELD_CANCEL role=%s via=%s interruptedDown=1 ownedUp=1 extraUp=1 toggle=0 stableIds=1 sun=30 sourceCD=750 noCDReset=1 nativeSDL=1",
                Role(app), app->NetIsHost() ? "modal" : "focus");
        if (!Barrier(app, "FUSION_NATIVE_FIELD_CANCELLED")) return true;
        Advance(app, 126);
        if (app->NetIsHost()) Click(app, board->GetShovelButtonRect());
    }
    else if (stage == 126)
    {
        const CursorType expected = app->NetIsHost() ? CURSOR_TYPE_SHOVEL : CURSOR_TYPE_NORMAL;
        if (board->mCursorObject->mCursorType != expected) return true;
        if (!Barrier(app, "FUSION_NATIVE_FIELD_CURSOR_READY")) return true;
        Advance(app, 110);
    }
    else if (stage == 110)
    {
        if (app->GetDialogCount()) return true;
        SeedPacket* packet = Packet(board, 1, 0, true);
        if (!GridFusedUnchanged(app) || !Require(app, packet && packet->mRefreshTime == 750, "Native field-toggle lost used packet cooldown")) return true;
        fieldCounter = packet->mRefreshCounter; fieldRefreshTime = packet->mRefreshTime; fieldWasRefreshing = packet->mRefreshing;
        fieldCursorType = board->mCursorObject->mCursorType;
        if (!Require(app, fieldCursorType == (app->NetIsHost() ? CURSOR_TYPE_SHOVEL : CURSOR_TYPE_NORMAL),
            "Live FIELD test lost its held native shovel/local normal cursor")) return true;
        Advance(app, 111);
    }
    else if (stage == 111)
    {
        const bool target = fieldRound == 0 ? app->NetIsHost() : fieldRound == 1 ? app->NetIsClient() : false;
        if (app->mNetManager->mConfig.mFusionClassicField != target && !FieldClick(app)) return true;
        Advance(app, 112);
    }
    else if (stage == 112)
    {
        const bool target = fieldRound == 0 ? app->NetIsHost() : fieldRound == 1 ? app->NetIsClient() : false;
        Sexy::Rect rect;
        if (!FieldRect(app, rect)) return true;
        if (app->mNetManager->mConfig.mFusionClassicField != target) { observedSince = 0; return true; }
        if (!Settled(300)) return true;
        LawnNet::NetConfig loaded;
        if (!Require(app, LawnNet::NetLoadConfig(loaded) && loaded.mFusionClassicField == target && !app->GetDialogCount() &&
            board->mCursorObject->mCursorType == fieldCursorType,
            "Live FIELD click changed cursor/dialogs or disagrees with persisted local preference")) return true;
        if (!Capture(app, "fusion_native_field_button")) return true;
        Advance(app, 113);
    }
    else if (stage == 113)
    {
        const std::string round = std::to_string(fieldRound);
        const bool target = fieldRound == 0 ? app->NetIsHost() : fieldRound == 1 ? app->NetIsClient() : false;
        if (!Require(app, app->mNetManager->mConfig.mFusionClassicField == target && !app->GetDialogCount() &&
            board->mCursorObject->mCursorType == fieldCursorType,
            "A FIELD change replaced this viewer's local preference/cursor or opened a dialog")) return true;
        if (!gridProofs.contains("field_"+round))
        {
            if (app->GetDialogCount() || !Settled()) return true;
            // Both peers have completed their real local FIELD click before
            // either captures. Hold each skin until both frames are acknowledged.
            const std::string prepared = "FUSION_NATIVE_FIELD_PREPARED_"+round;
            if (!Barrier(app, prepared.c_str())) return true;
            if (!NativeGrid(app) || !GridFusedUnchanged(app)) return true;
            SeedPacket* packet = Packet(board, 1, 0, true);
            if (!FieldCooldownProgressed(app)) return true;
            const bool native = app->mNetManager->mConfig.mFusionClassicField;
            if (!Capture(app, native ? "fusion_native_field_native" : "fusion_native_field_fusion")) return true;
            if (fieldRound < 2 && !Capture(app, fieldRound == 0 ? "fusion_native_field_independent_a" : "fusion_native_field_independent_b")) return true;
            gridProofs.insert("field_"+round);
            NetTrace("SMOKE_FUSION_NATIVE_FIELD role=%s round=%d skin=%s id=%u original=%u packet=%u sun=30 columns=10 pitch=72 persisted=1 beforeCD=%d afterCD=%d sourceCD=%d refreshing=%d stableIds=1 nativeFieldSDL=1 cursor=%s freshPaintMs=300 samples=%u",
                Role(app), fieldRound, native ? "native" : "fusion", gridFusedId, fusedPlant, gridClientPacketId,
                fieldCounter, packet->mRefreshCounter, packet->mRefreshTime, packet->mRefreshing ? 1 : 0,
                app->NetIsHost() ? "shovel" : "normal", Samples(app));
        }
        const std::string message = "FUSION_NATIVE_FIELD_ROUND_"+round;
        if (!Barrier(app, message.c_str())) return true;
        if (++fieldRound < 3) Advance(app, 110);
        else Advance(app, 92);
    }
    else if (stage == 92)
    {
        if (!GridFusedUnchanged(app)) return true;
        Advance(app, 93);
    }
    else if (stage == 93)
    {
        if (app->NetIsHost())
        {
            if (board->mCursorObject->mCursorType != CURSOR_TYPE_SHOVEL || !Settled() || !Capture(app, "fusion_native_col9_shovel_held")) return true;
            if (!Cell(app, 9, 4)) return true;
        }
        Advance(app, 94);
    }
    else if (stage == 94)
    {
        if (At(board, 9, 4)) return true;
        if (!Require(app, At(board, 3, 2) && At(board, 3, 2)->mNetId == fusedPlant && board->mSunMoney == 30 &&
            !app->mNetManager->mConfig.mFusionClassicField, "Native tenth-column shovel changed the original actor/currency/skin") ||
            !Settled() || !Capture(app, "fusion_native_col9_shoveled")) return true;
        NetTrace("SMOKE_FUSION_NATIVE_COL9_SHOVEL role=%s removed=%u original=%u column=9 row=4 sun=30 nativeSDL=1 samples=%u",
            Role(app), gridFusedId, fusedPlant, Samples(app));
        Advance(app, 95);
    }
    else if (stage == 95)
    {
        if (!Barrier(app, "FUSION_NATIVE_GRID_FINAL_ACK") || Samples(app) < 8 || !Settled()) return true;
        NetTrace("SMOKE_FUSION_NATIVE_COMPLETE role=%s epoch=%u fused=%u col9=%u sun=30 snapshots=%u nativeGrid=10x5 nativeBoard=1 nativeActors=1 nativeChooser=1 secondWorld=0 sourceCampaignComplete=0",
            Role(app), context->epoch, fusedPlant, gridFusedId, Samples(app));
        Advance(app, 96); app->CoopSmokeWriteResult(true, nullptr);
    }
    return true;
}
}
