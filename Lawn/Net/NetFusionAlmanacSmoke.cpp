#include "../../LawnApp.h"
#include "NetManager.h"
#include "../Widget/AlmanacDialog.h"
#include "../Widget/GameSelector.h"
#include "../Widget/GameButton.h"
#include "../Plant.h"
#include "../Zombie.h"
#include "../Fusion/FusionCatalog.h"
#include "../Fusion/FusionAlmanacText.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include "../../Sexy.TodLib/TodStringFile.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "../../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <vector>

namespace
{
using namespace LawnFusion;
enum class Action { ClassicMenu, ClassicIndex, ClassicPlants, ClassicZombies, FusionMenu, FusionIndex,
    Page, Target, Boundary, EmptyCell, Wheel, ThumbDown, ThumbMove, ThumbUp, Close, Reopen, Finish };
struct Step
{
    Action action;
    bool plants = true;
    uint16_t id = 0;
    int argument = 0;
    std::string tag;
};
struct State
{
    std::vector<Step> steps;
    size_t index = 0;
    uint32_t started = 0, observed = 0, hoverSince = 0, inputAt = 0;
    Sexy::Rect hover;
    bool queued = false, nameFits = true;
    int captureDraw = -1;
    FusionAlmanacTextProvider translations;
    std::filesystem::path root;
    std::vector<uint16_t> unsupported;
    uint16_t longest = 0;
    std::set<std::string> frames;
} s;

bool Require(LawnApp* app, bool value, const char* reason)
{ if (!value) app->CoopSmokeFail(reason); return value; }
AlmanacDialog* Book(LawnApp* app) { return static_cast<AlmanacDialog*>(app->GetDialog(DIALOG_ALMANAC)); }
const char* Group(bool plants) { return plants ? "plants" : "zombies"; }
const FusionAlmanacEntry* At(bool plants, size_t index)
{ return plants ? FusionPlantAlmanacAt(index) : FusionZombieAlmanacAt(index); }
size_t Count(bool plants) { return plants ? FusionPlantAlmanacCount() : FusionZombieAlmanacCount(); }
size_t Capacity(bool plants) { return plants ? 40U : 25U; }
int Index(bool plants, uint16_t id)
{ for (size_t i = 0; i < Count(plants); ++i) if (At(plants, i)->id == id) return static_cast<int>(i); return -1; }
void Next()
{
    ++s.index; s.started = GetTickCount(); s.observed = s.hoverSince = 0; s.queued = false; s.captureDraw = -1;
    LawnNet::NetTrace("SMOKE_ALMANAC_STEP index=%u noBoard=1", static_cast<unsigned>(s.index));
}
bool Fresh()
{ if (!s.observed) s.observed = GetTickCount(); return GetTickCount()-s.observed >= 300U; }
bool Motion(LawnApp* app, float x, float y, bool held = false)
{
    SDL_Event event{}; event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.windowID = SDL_GetWindowID(app->mSDLWindow);
    event.motion.x = x; event.motion.y = y; event.motion.state = held ? SDL_BUTTON_LMASK : 0;
    return Require(app, SDL_PushEvent(&event), "Almanac owned SDL motion queue failed");
}
bool PointerEdge(LawnApp* app, bool down, float x, float y)
{
    SDL_Event event{}; event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.windowID = SDL_GetWindowID(app->mSDLWindow); event.button.button = SDL_BUTTON_LEFT;
    event.button.down = down; event.button.clicks = 1; event.button.x = x; event.button.y = y;
    return Require(app, SDL_PushEvent(&event), "Almanac owned SDL mouse edge queue failed");
}
bool Click(LawnApp* app, const Sexy::Rect& rect, bool requireHand = false)
{
    const uint32_t now = GetTickCount();
    if (s.inputAt && now-s.inputAt < 150U) return false;
    if (!s.hoverSince || !(s.hover == rect))
    {
        s.hover = rect; s.hoverSince = now;
        Motion(app, rect.mX+rect.mWidth*0.5f, rect.mY+rect.mHeight*0.5f);
        return false;
    }
    if (now-s.hoverSince < 100U || (requireHand && app->mCursorNum != CURSOR_HAND)) return false;
    const float x = rect.mX+rect.mWidth*0.5f, y = rect.mY+rect.mHeight*0.5f;
    if (!PointerEdge(app, true, x, y) || !PointerEdge(app, false, x, y)) return false;
    s.hoverSince = s.observed = 0; s.inputAt = now;
    LawnNet::NetTrace("SMOKE_ALMANAC_INPUT x=%.0f y=%.0f hoverPrimedMs=100 hand=%d nativeSDL=1", x, y, app->mCursorNum == CURSOR_HAND);
    return true;
}
Sexy::Rect Absolute(AlmanacDialog* book, Sexy::Rect rect)
{ const auto p = book->GetAbsPos(); rect.mX += p.mX; rect.mY += p.mY; return rect; }
bool Button(LawnApp* app, GameButton* button)
{
    if (!button || button->mDisabled || button->mBtnNoDraw) return false;
    const auto p = button->mParentWidget->GetAbsPos();
    return Click(app, Sexy::Rect(p.mX+button->mX, p.mY+button->mY, button->mWidth, button->mHeight), true);
}
Sexy::Rect Card(bool plants, size_t cell)
{
    return plants ? Sexy::Rect(26+static_cast<int>(cell%8)*52,92+static_cast<int>(cell/8)*78,50,70) :
        Sexy::Rect(22+static_cast<int>(cell%5)*85,86+static_cast<int>(cell/5)*80,76,76);
}
bool Capture(LawnApp* app, const std::string& tag)
{
    if (!Fresh()) return false;
    if (!s.frames.contains(tag))
    {
        if (s.captureDraw < 0) s.captureDraw = app->mDrawCount;
        const int requestedDraw = s.captureDraw+2;
        if (!app->CoopSmokeEnsureFreshFrameAfterDraw(requestedDraw)) return false;
        if (!Require(app, app->CoopSmokeCaptureFrame(("almanac_"+tag).c_str()), "Almanac actual renderer capture failed")) return false;
        s.frames.insert(tag);
        const auto p = Book(app) ? Book(app)->GetAbsPos() : Sexy::Point(-1,-1);
        LawnNet::NetTrace("SMOKE_ALMANAC_FRAME tag=%s bookX=%d bookY=%d noBoard=1 freshPaintMs=300 nativeSDL=1 requestedDraw=%d capturedDraw=%d", tag.c_str(), p.mX, p.mY, requestedDraw, app->mCoopSmokeFrameDrawCount);
    }
    return true;
}
bool Selection(LawnApp* app, bool plants, uint16_t id, size_t page)
{
    auto* book = Book(app); uint16_t actual = 0; size_t actualPage = 0; bool actualPlants = false;
    return book && book->GetFusionUiSelection(actual,actualPage,actualPlants) && actual == id &&
        actualPage == page && actualPlants == plants && !book->mPlant && !book->mZombie;
}
bool GoTo(LawnApp* app, bool plants, uint16_t id)
{
    auto* book = Book(app); if (!book) return false;
    const int index = Index(plants,id);
    if (!Require(app,index >= 0,"Almanac target is absent from the actual source catalog")) return false;
    if (book->mOpenPage == ALMANAC_PAGE_INDEX)
    { Button(app, plants ? book->mPlantButton : book->mZombieButton); return false; }
    uint16_t selected = 0; size_t page = 0; bool selectedPlants = false;
    if (!Require(app,book->GetFusionUiSelection(selected,page,selectedPlants),"Fusion book did not expose its real source selection")) return false;
    if (selectedPlants != plants) { Button(app,book->mIndexButton); return false; }
    const size_t target = static_cast<size_t>(index)/Capacity(plants);
    if (page != target)
    { Click(app,Absolute(book,Sexy::Rect(page < target ? 292 : 32,505,117,46)),true); return false; }
    if (selected != id)
    { Click(app,Absolute(book,Card(plants,static_cast<size_t>(index)%Capacity(plants)))); return false; }
    return Require(app,!book->mPlant && !book->mZombie,"Fusion book constructed a gameplay preview instead of using its portrait cache");
}
bool Menu(LawnApp* app)
{ return app->mGameSelector && app->mGameScene == SCENE_MENU && app->mGameSelector->MainMenuIconsVisible() && !app->mGameSelector->mLoading && app->GetDialogCount() == 0; }
bool OpenMenuBook(LawnApp* app)
{
    auto* selector = app->mGameSelector; auto* button = selector ? selector->mAlmanacButton : nullptr;
    if (!Require(app,button && button->mVisible && !button->mDisabled && app->CanShowAlmanac(),"Fusion main-menu Almanac button is unavailable")) return false;
    const auto p = button->GetAbsPos();
    return Click(app,Sexy::Rect(p.mX,p.mY,button->mWidth,button->mHeight));
}

bool Prepare(LawnApp* app)
{
    std::wstring executable(32768,L'\0'); const DWORD length = GetModuleFileNameW(nullptr,executable.data(),static_cast<DWORD>(executable.size()));
    if (!Require(app,length && length < executable.size(),"Cannot locate almanac fixture assets")) return false;
    executable.resize(length); s.root = std::filesystem::path(executable).parent_path()/"fusion";
    std::string error;
    const bool loaded = s.translations.Load((s.root/"catalog").string(),error);
    if (!Require(app,loaded,error.c_str())) return false;
    if (!Require(app,FusionPlantAlmanacCount() == 281 && FusionZombieAlmanacCount() == 88 &&
        s.translations.FindRussian(FusionPortraitGroup::Plants,0) && s.translations.FindRussian(FusionPortraitGroup::Plants,925),
        "Almanac source counts/base RU/long RU925 record are missing")) return false;
    size_t longest = 0;
    size_t translated = 0;
    for (bool plants : {true, false}) for (size_t i = 0; i < Count(plants); ++i)
    {
        const auto* entry = At(plants,i);
        const auto* ru = s.translations.FindRussian(plants ? FusionPortraitGroup::Plants : FusionPortraitGroup::Zombies,entry->id);
        if (!Require(app,ru && !ru->nameUtf8.empty() && !ru->infoUtf8.empty(),"Almanac requires Russian text for every actual source entry")) return false;
        const auto text = Sexy::StringToWString(ru->nameUtf8+ru->infoUtf8+ru->loreUtf8+ru->costUtf8);
        for (wchar_t ch : text)
            if (!Require(app,ch < 0x3400 || ch > 0x9fff,"Russian almanac record contains untranslated CJK text")) return false;
        ++translated;
        const size_t size = ru->infoUtf8.size()+ru->loreUtf8.size()+ru->costUtf8.size();
        if (plants && size > longest) { longest = size; s.longest = entry->id; }
    }
    if (!Require(app,translated == 369,"Almanac Russian coverage is incomplete")) return false;
    LawnNet::NetTrace("SMOKE_ALMANAC_RUSSIAN plants=281 zombies=88 checked=369 cjk=0 sourceBound=1");
    std::ifstream file(s.root/"almanac"/"portraits"/"manifest.json");
    const auto manifest = nlohmann::json::parse(file); int ready = 0;
    for (const auto& row : manifest.at("groups").at("zombies"))
    {
        const auto id = row.at("id").get<uint16_t>();
        if (!Require(app,FusionFindZombieAlmanac(id) != nullptr,"Portrait manifest has an unknown zombie ID")) return false;
        if (row.at("status") == "ready" && row.at("publicAccepted").get<bool>()) ++ready;
        else if (id != 11 && id != 12 && id != 13) s.unsupported.push_back(id);
    }
    std::sort(s.unsupported.begin(),s.unsupported.end());
    if (!Require(app,ready == 72 && s.unsupported.size() == 13,"Almanac portrait coverage must be72 static+3 own existing models+13 unavailable")) return false;
    using A = Action;
    s.steps = {{A::ClassicMenu},{A::ClassicIndex},{A::ClassicPlants},{A::ClassicZombies},{A::FusionMenu},{A::FusionIndex}};
    for (int page = 0; page < 8; ++page)
    {
        s.steps.push_back({A::Page,true,At(true,page*40)->id,page,"plants_p"+std::to_string(page+1)});
        if (!page)
        {
            s.steps.push_back({A::Boundary,true,0,-1,"plants_first_bound"});
            s.steps.push_back({A::Target,true,At(true,39)->id,0,"edge_plant_bottom"});
            s.steps.push_back({A::Target,true,At(true,7)->id,0,"edge_plant_top"});
        }
    }
    s.steps.push_back({A::Boundary,true,1192,1,"plants_last_bound"});
    s.steps.push_back({A::EmptyCell,true,1192,39,"plants_empty_last"});
    for (const auto& target : std::vector<std::pair<uint16_t,std::string>>{{1000,"plant_1000"},{0,"plant_ru"},{1025,"plant_new_ru"},{925,"plant_925"},{s.longest,"long_top"}})
        s.steps.push_back({A::Target,true,target.first,0,target.second});
    s.steps.push_back({A::Wheel,true,s.longest,0,"long_bottom"});
    s.steps.push_back({A::ThumbDown,true,s.longest}); s.steps.push_back({A::ThumbMove,true,s.longest});
    s.steps.push_back({A::ThumbUp,true,s.longest,0,"long_drag_top"});
    for (int page = 0; page < 4; ++page)
    {
        s.steps.push_back({A::Page,false,At(false,page*25)->id,page,"zombies_p"+std::to_string(page+1)});
        if (!page)
        {
            s.steps.push_back({A::Target,false,At(false,24)->id,0,"edge_zombie_bottom"});
            s.steps.push_back({A::Target,false,At(false,4)->id,0,"edge_zombie_top"});
        }
    }
    s.steps.push_back({A::EmptyCell,false,At(false,75)->id,24,"zombies_empty_last"});
    s.steps.push_back({A::Target,false,7,0,"portrait_7"});
    for (uint16_t id : {11, 12, 13}) s.steps.push_back({A::Target,false,id,0,"runtime_"+std::to_string(id)});
    for (uint16_t id : s.unsupported) s.steps.push_back({A::Target,false,id,1,id == 106 ? "missing_106" : ""});
    s.steps.push_back({A::Target,false,224,0,"zombie_224"});
    s.steps.push_back({A::Close}); s.steps.push_back({A::Reopen});
    s.steps.push_back({A::Target,true,0,0,"reopened"}); s.steps.push_back({A::Close}); s.steps.push_back({A::Finish});
    s.started = GetTickCount();
    LawnNet::NetTrace("SMOKE_ALMANAC_PLAN plants=281 zombies=88 plantPages=8 zombiePages=4 staticZombiePortraits=72 runtimePortraits=3 unsupported=13 russian=369 longest=%u noBoard=1",s.longest);
    return true;
}
void NameProbe(LawnApp* app)
{
    const auto* book = dynamic_cast<AlmanacDialog*>(app->GetDialog(DIALOG_ALMANAC));
    int fontHeight = 0, height = 0, boxHeight = 0;
    if (!Require(app,book && book->GetFusionNameLayout(fontHeight,height,boxHeight),"Actual almanac name layout has not rendered")) return;
    s.nameFits = height <= boxHeight;
    LawnNet::NetTrace("SMOKE_ALMANAC_NAME id=925 fontHeight=%d requiredHeight=%d boxHeight=%d layoutFits=%d actualRenderer=1",fontHeight,height,boxHeight,s.nameFits ? 1 : 0);
}
}

namespace LawnNet
{
bool NetSmokeUpdateFusionAlmanac(LawnApp* app)
{
    const char* enabled = std::getenv("PVZ_COOP_SMOKE_FUSION_ALMANAC");
    if (!app || !app->CoopSmokeActive() || !enabled || std::strcmp(enabled,"1")) return false;
    if (!Require(app,app->mCoopSmokeManifest.IsHost() && app->mCoopSmokeManifest.mScenario == NETSMOKE_SCENARIO_FUSION_VALIDATION,
        "Almanac fixture requires the strict solo-host FusionValidation manifest")) return true;
    if (!Require(app,!app->mBoard && !app->mFusionRuntime && !app->mSeedChooserScreen && !app->mCoopSmokeGameStarted,
        "Almanac fixture escaped the genuine menu/noBoard scope")) return true;
    try
    {
        if (s.steps.empty()) { if (!Menu(app) || !Prepare(app)) return true; }
        if (s.index >= s.steps.size()) return true;
        if (!Require(app,GetTickCount()-s.started < 20000U,"Almanac SDL UI step timed out")) return true;
        const Step& step = s.steps[s.index]; auto* book = Book(app);
        using A = Action;
        if (step.action == A::ClassicMenu)
        {
            if (!Menu(app)) return true;
            std::string error; const bool selected = app->SetContentMode(ContentMode::Classic,&error);
            if (!Require(app,selected,error.c_str())) return true;
            if (!Capture(app,"menu_classic")) return true;
            NetTrace("SMOKE_ALMANAC_OPEN mode=classic route=dialog_api genuineMenu=1 noBoard=1");
            app->DoAlmanacDialog(); Next();
        }
        else if (step.action == A::ClassicIndex)
        {
            if (!book || book->mOpenPage != ALMANAC_PAGE_INDEX) return true;
            if (!Require(app,book->mPlant && book->mZombie && book->mPlant->mSeedType == SEED_SUNFLOWER && book->mZombie->mZombieType == ZOMBIE_NORMAL,"Classic index previews changed")) return true;
            if (Capture(app,"classic_index") && Button(app,book->mPlantButton)) Next();
        }
        else if (step.action == A::ClassicPlants)
        {
            if (!book || book->mOpenPage != ALMANAC_PAGE_PLANTS) return true;
            if (!Require(app,book->mPlant && book->mSelectedSeed == SEED_PEASHOOTER && book->mPlant->mSeedType == SEED_PEASHOOTER,"Classic plant almanac path changed")) return true;
            if (Capture(app,"classic_plants") && Button(app,book->mIndexButton)) Next();
        }
        else if (step.action == A::ClassicZombies)
        {
            if (!book) return true;
            if (book->mOpenPage == ALMANAC_PAGE_INDEX) { Button(app,book->mZombieButton); return true; }
            if (!Require(app,book->mOpenPage == ALMANAC_PAGE_ZOMBIES && book->mZombie && book->mSelectedZombie == ZOMBIE_NORMAL,"Classic zombie almanac path changed")) return true;
            if (Capture(app,"classic_zombies") && Button(app,book->mCloseButton)) Next();
        }
        else if (step.action == A::FusionMenu)
        {
            if (!Menu(app)) return true;
            std::string error; const bool selected = app->SetContentMode(ContentMode::Fusion,&error);
            if (!Require(app,selected,error.c_str())) return true;
            if (Capture(app,"menu_fusion") && OpenMenuBook(app)) { NetTrace("SMOKE_ALMANAC_OPEN mode=fusion route=menu_native_SDL genuineMenu=1 noBoard=1"); Next(); }
        }
        else if (step.action == A::FusionIndex)
        {
            if (book && book->mOpenPage == ALMANAC_PAGE_INDEX && Capture(app,"fusion_index") && Button(app,book->mPlantButton)) Next();
        }
        else if (step.action == A::Page || step.action == A::Target)
        {
            if (!GoTo(app,step.plants,step.id) || !Fresh()) return true;
            if (!step.tag.empty() && !Capture(app,step.tag)) return true;
            const auto* ru = s.translations.FindRussian(step.plants ? FusionPortraitGroup::Plants : FusionPortraitGroup::Zombies,step.id);
            bool russian = false, portrait = false, runtimeModel = false;
            if (!book->GetFusionRenderedEntryStatus(russian,portrait,runtimeModel)) return true;
            if (!Require(app,ru && russian,"Actual almanac draw did not use its Russian source-bound text")) return true;
            const bool expectedRuntime = !step.plants && (step.id == 11 || step.id == 12 || step.id == 13);
            const bool unavailable = step.action == A::Target && step.argument == 1;
            if (!Require(app,portrait == !unavailable && runtimeModel == expectedRuntime,
                "Actual almanac draw does not match the real portrait/model/unavailable resource")) return true;
            NetTrace("SMOKE_ALMANAC_RENDER group=%s id=%u russian=1 portrait=%d runtimeModel=%d actualDraw=1",Group(step.plants),step.id,portrait ? 1 : 0,runtimeModel ? 1 : 0);
            if (step.tag == "plant_925") NameProbe(app);
            uint16_t id; size_t page; bool plants; book->GetFusionUiSelection(id,page,plants);
            NetTrace("SMOKE_ALMANAC_SELECT group=%s id=%u page=%u expectedText=%s unsupported=%d noGameplayPreview=1 nativeSDL=1",Group(plants),id,static_cast<unsigned>(page),ru ? "ru" : "source",step.argument == 1 && step.action == A::Target);
            if (step.action == A::Page) NetTrace("SMOKE_ALMANAC_PAGE group=%s page=%u firstId=%u nativeSDL=1",Group(plants),static_cast<unsigned>(page),id);
            Next();
        }
        else if (step.action == A::Boundary || step.action == A::EmptyCell)
        {
            if (!s.queued)
            {
                if (!GoTo(app,step.plants,step.id)) return true;
                const Sexy::Rect rect = step.action == A::Boundary ? Sexy::Rect(step.argument < 0 ? 32 : 292,505,117,46) : Card(step.plants,step.argument);
                if (Click(app,Absolute(book,rect))) { s.queued = true; s.observed = 0; }
                return true;
            }
            if (!Fresh()) return true;
            if (!Require(app,Selection(app,step.plants,step.id,static_cast<size_t>(Index(step.plants,step.id))/Capacity(step.plants)),"Off-page/disabled native click changed the source selection")) return true;
            NetTrace("SMOKE_ALMANAC_BOUNDARY tag=%s selectionUnchanged=1 nativeSDL=1",step.tag.c_str()); Next();
        }
        else if (step.action == A::Wheel)
        {
            if (!Require(app,book && book->mScrollbar && !book->mScrollbar->mDisabled && std::isfinite(book->mScrollbar->mScrollRange) && book->mScrollbar->mScrollRange > 0,"Long Fusion description has no readable native scroll range")) return true;
            if (!s.queued)
            {
                const auto p = book->GetAbsPos(); Motion(app,p.mX+615,p.mY+420);
                SDL_Event event{}; event.type = SDL_EVENT_MOUSE_WHEEL; event.wheel.windowID = SDL_GetWindowID(app->mSDLWindow); event.wheel.y = -100;
                if (!Require(app,SDL_PushEvent(&event),"Almanac SDL wheel queue failed")) return true;
                s.queued = true; s.observed = 0; return true;
            }
            if (book->mScrollbar->mScrollValue < book->mScrollbar->mScrollRange-1) { s.observed = 0; return true; }
            if (Capture(app,step.tag)) { NetTrace("SMOKE_ALMANAC_SCROLL kind=wheel bottom=1 value=%.2f range=%.2f nativeSDL=1",book->mScrollbar->mScrollValue,book->mScrollbar->mScrollRange); Next(); }
        }
        else if (step.action == A::ThumbDown || step.action == A::ThumbMove || step.action == A::ThumbUp)
        {
            auto* bar = book ? book->mScrollbar : nullptr;
            if (!Require(app,bar && !bar->mDisabled && std::isfinite(bar->mThumbY) && bar->mThumbHeight > 0,"Native scrollbar thumb became invalid")) return true;
            const auto p = bar->GetAbsPos(); const float x = p.mX+bar->mThumbWidth*0.5f;
            if (step.action == A::ThumbDown)
            {
                const float y = p.mY+bar->mThumbY+bar->mThumbHeight*0.5f;
                if (!bar->isThumbOver()) { Motion(app,x,y); return true; }
                if (PointerEdge(app,true,x,y)) Next();
            }
            else if (step.action == A::ThumbMove)
            { if (bar->isThumbDown() && Motion(app,x,p.mY+bar->mThumbHeight*0.5f,true)) Next(); }
            else
            {
                if (!s.queued)
                {
                    if (bar->mScrollValue > 1) return true;
                    if (PointerEdge(app,false,x,p.mY+bar->mThumbHeight*0.5f)) { s.queued = true; s.observed = 0; }
                    return true;
                }
                if (!Require(app,bar->mScrollValue <= 2,"Native scrollbar drag did not restore the top")) return true;
                if (Capture(app,step.tag)) { NetTrace("SMOKE_ALMANAC_SCROLL kind=thumb top=1 value=%.2f nativeSDL=1",bar->mScrollValue); Next(); }
            }
        }
        else if (step.action == A::Close)
        { if (!book && Menu(app)) Next(); else if (book) Button(app,book->mCloseButton); }
        else if (step.action == A::Reopen)
        { if (book && book->mOpenPage == ALMANAC_PAGE_INDEX) { if (Capture(app,"reopened_index")) Next(); } else if (Menu(app)) OpenMenuBook(app); }
        else if (step.action == A::Finish)
        {
            if (!Menu(app) || !Capture(app,"complete")) return true;
            std::string error; const bool selected = app->SetContentMode(ContentMode::Classic,&error);
            if (!Require(app,selected,error.c_str())) return true;
            NetTrace("SMOKE_ALMANAC_COMPLETE pagesPlants=8 pagesZombies=4 unsupportedSelected=13 boardCreated=0 reopened=1 name925Fits=%d frames=%u snapshotsClaimed=0 russian=369 zombiePortraits=75",s.nameFits ? 1 : 0,static_cast<unsigned>(s.frames.size()));
            app->CoopSmokeWriteResult(s.nameFits,s.nameFits ? nullptr : "Long Russian almanac925 name exceeds its native title viewport"); Next();
        }
    }
    catch (const std::exception& exception) { app->CoopSmokeFail(exception.what()); }
    return true;
}
}
