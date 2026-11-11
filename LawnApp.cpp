#include <time.h>
#include "LawnApp.h"
#include "Lawn/Board.h"
#include "Lawn/CursorObject.h"
#include "Lawn/Plant.h"
#include "Lawn/Zombie.h"
#include "Lawn/Cutscene.h"
#include "GameConstants.h"
#include "Lawn/Challenge.h"
#include "Lawn/ZenGarden.h"
#include "Sexy.TodLib/Trail.h"
#include "Lawn/System/Music.h"
#include "Lawn/System/SaveGame.h"
#include "Sexy.TodLib/TodDebug.h"
#include "Sexy.TodLib/TodFoley.h"
#include "Sexy.TodLib/Attachment.h"
#include "Lawn/System/PlayerInfo.h"
#include "Lawn/System/PoolEffect.h"
#include "Lawn/System/ProfileMgr.h"
#include "Lawn/System/PopDRMComm.h"
#include "Lawn/Widget/GameButton.h"
#include "Sexy.TodLib/Reanimator.h"
#include "Lawn/Widget/UserDialog.h"
#include "Lawn/System/TypingCheck.h"
#include "Sexy.TodLib/TodParticle.h"
#include "Lawn/Widget/AwardScreen.h"
#include "Lawn/Widget/TitleScreen.h"
#include "Lawn/Widget/StoreScreen.h"
#include "Lawn/Widget/CheatDialog.h"
#include "Lawn/Widget/GameSelector.h"
#include "Lawn/Widget/CreditScreen.h"
#include "Sexy.TodLib/EffectSystem.h"
#include "Sexy.TodLib/FilterEffect.h"
#include "SexyAppFramework/Graphics.h"
#include "Sexy.TodLib/TodStringFile.h"
#include "Lawn/Widget/AlmanacDialog.h"
#include "Lawn/Widget/NewUserDialog.h"
#include "Lawn/Widget/ContinueDialog.h"
#include "Lawn/Widget/ZombatarTOS.h"
#include "Lawn/Widget/LanguageScreen.h"
#include "Lawn/System/ReanimationLawn.h"
#include "Lawn/Widget/ChallengeScreen.h"
#include "Lawn/Widget/NewOptionsDialog.h"
#include "Lawn/Widget/SeedChooserScreen.h"
#include "SexyAppFramework/WidgetManager.h"
#include "SexyAppFramework/ResourceManager.h"

#include "SexyAppFramework/Checkbox.h"
#include "SexyAppFramework/BassMusicInterface.h"
#include "SexyAppFramework/Dialog.h"
#include "SexyAppFramework/resource.h"

#include "Lawn/Net/NetManager.h"
#include "Lawn/Net/NetLobbyPanel.h"

#include "Lawn/SeedPacket.h"

#include "Sexy.TodLib/Definition.h"

#include "portaudio.h"

#include "Editor/ParticleScreen.h"
#include "SexyAppFramework/D3DInterface.h"

#include "Lawn/Widget/MoreSettingsDialog.h"
#include "Lawn/Widget/NetOnlineDialog.h"

#include <windows.h>
#include <windowsx.h>

bool gIsPartnerBuild = false;

namespace
{
class CoopTeamDialog : public LawnDialog
{
    LawnStoneButton* mPlants;
    LawnStoneButton* mZombies;
    LawnStoneButton* mReady;
public:
    explicit CoopTeamDialog(LawnApp* theApp) :
        LawnDialog(theApp, Dialogs::DIALOG_COOP_TEAMS, true, _S("PvP: choose team"),
            _S(""), _S("BACK"), Dialog::BUTTONS_FOOTER)
    {
        mPlants = MakeButton(101, this, _S("PLANTS"));
        mZombies = MakeButton(102, this, _S("ZOMBIES"));
        mReady = MakeButton(103, this, _S("READY"));
        mPlants->Resize(70, 120, 210, 42);
        mZombies->Resize(320, 120, 210, 42);
        mReady->Resize(195, 345, 210, 42);
        Resize(0, 0, 600, 480);
        LawnApp::CenterDialog(this, mWidth, mHeight);
        mLawnYesButton->SetVisible(theApp->NetIsHost());
    }
    ~CoopTeamDialog()
    {
        delete mPlants;
        delete mZombies;
        delete mReady;
    }
    void AddedToManager(WidgetManager* theManager) override
    {
        LawnDialog::AddedToManager(theManager);
        AddWidget(mPlants);
        AddWidget(mZombies);
        AddWidget(mReady);
    }
    void RemovedFromManager(WidgetManager* theManager) override
    {
        RemoveWidget(mPlants);
        RemoveWidget(mZombies);
        RemoveWidget(mReady);
        LawnDialog::RemovedFromManager(theManager);
    }
    void Update() override
    {
        LawnDialog::Update();
        LawnNet::NetManager* aNet = mApp->mNetManager;
        if (!aNet)
            return;
        mPlants->SetLabel(aNet->GetMyTeam() == LawnNet::NETTEAM_PLANTS ? _S("[ PLANTS ]") : _S("PLANTS"));
        mZombies->SetLabel(aNet->GetMyTeam() == LawnNet::NETTEAM_ZOMBIES ? _S("[ ZOMBIES ]") : _S("ZOMBIES"));
        mReady->SetLabel(StrFormat(aNet->IsMyReady() ? _S("READY %d/%d") : _S("NOT READY %d/%d"),
            aNet->GetReadyCount(), aNet->GetReadyEligibleCount()));
        mReady->SetDisabled(aNet->IsSpectator() || aNet->GetMyTeam() == LawnNet::NETTEAM_NONE);
        MarkDirty();
    }
    void Draw(Graphics* g) override
    {
        LawnDialog::Draw(g);
        LawnNet::NetManager* aNet = mApp->mNetManager;
        int y = 200;
        for (int i = 0; aNet && i < LawnNet::NET_MAX_PLAYERS; i++)
        {
            const LawnNet::NetPlayerInfo& aPlayer = aNet->GetPlayer(i);
            if (!aPlayer.mActive || aPlayer.mSpectator)
                continue;
            const SexyString aTeam = aPlayer.mTeam == LawnNet::NETTEAM_PLANTS ? _S("PLANTS") :
                aPlayer.mTeam == LawnNet::NETTEAM_ZOMBIES ? _S("ZOMBIES") : _S("CHOOSE TEAM");
            TodDrawString(g, StringToSexyString(aPlayer.mNick) + _S(" - ") + aTeam +
                (aPlayer.mReady ? _S("  READY") : _S("")), 80, y,
                FONT_BRIANNETOD12, aPlayer.mReady ? Color(50, 125, 45) : Color(150, 50, 45), DS_ALIGN_LEFT);
            y += 28;
        }
        TodDrawString(g, _S("Each team needs a player. Everyone must be ready."),
            300, 325, FONT_BRIANNETOD12, Color(80, 75, 95), DS_ALIGN_CENTER);
    }
    void ButtonDepress(int theId) override
    {
        LawnNet::NetManager* aNet = mApp->mNetManager;
        if (!aNet)
            return;
        if (theId == 101 || theId == 102)
        {
            aNet->SetMyTeam(theId == 101 ? LawnNet::NETTEAM_PLANTS : LawnNet::NETTEAM_ZOMBIES);
            aNet->SetMyReady(false);
        }
        else if (theId == 103)
            aNet->SetMyReady(!aNet->IsMyReady());
        else if (theId == 1000 && mApp->NetIsHost())
            mApp->NetClosePvpTeamSelection(true);
    }
    void KeyDown(KeyCode theKey) override
    {
        if (theKey == KEYCODE_RETURN)
            ButtonDepress(103);
        else if (theKey == KEYCODE_LEFT)
            ButtonDepress(101);
        else if (theKey == KEYCODE_RIGHT)
            ButtonDepress(102);
        else if (theKey == KEYCODE_ESCAPE && mApp->NetIsHost())
            mApp->NetClosePvpTeamSelection(true);
    }
};
}
bool gSlowMo = false;  //0x6A9EAA
bool gFastMo = false;  //0x6A9EAB
LawnApp* gLawnApp = nullptr;  //0x6A9EC0
int gSlowMoCounter = 0;  //0x6A9EC4

#include "SexyAppFramework/Debug.h"

#include <SDL3/SDL.h>
#include "SexyAppFramework/SDL3Image.h"
SDL_Window* LawnApp::mSDLWindow = nullptr;
SDL_Renderer* LawnApp::mSDLRenderer = nullptr;
SDL_Cursor* LawnApp::mSDLPointerCursor = nullptr;
SDL_Cursor* LawnApp::mSDLHandCursor = nullptr;
SDL_Cursor* LawnApp::mSDLDraggingCursor = nullptr;
SDL_Cursor* LawnApp::mSDLTextCursor = nullptr;
SDL_Cursor* LawnApp::mSDLWaitCursor = nullptr;
SDL_Cursor* LawnApp::mSDLNoCursor = nullptr;

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include <SDL3_ttf/SDL_ttf.h>

#include <cstdio>
#include <stdexcept>
#include <thread>

bool LawnApp::mIsPlayingVideo = false;

// I wouldn't be able to make this without Codotaku. Huge W for them
bool LawnApp::PlayVideo(std::string url, bool isSkipable, Color bgColor)
{
	mIsPlayingVideo = true;

	AVFormatContext* format_context = NULL;
	const int ret = avformat_open_input(&format_context, url.c_str(), NULL, NULL);
	if (ret < 0) 
	{
		TodTrace("Video: %s is missing or corrupted!\n", url.c_str());
		return false;
	}

	SDL_HideCursor();

	const AVCodec* video_codec = NULL;
	const int video_stream_index = av_find_best_stream(format_context, AVMEDIA_TYPE_VIDEO, -1, -1, &video_codec, 0);
	const AVStream* video_stream = format_context->streams[video_stream_index];

	const AVCodec* audio_codec = NULL;
	const int  audio_stream_index = av_find_best_stream(format_context, AVMEDIA_TYPE_AUDIO, -1, video_stream_index, &audio_codec, 0);
	const AVStream* audio_stream = format_context->streams[audio_stream_index];

	AVCodecContext* video_decoder = avcodec_alloc_context3(video_codec);
	video_decoder->thread_count = 0;
	avcodec_parameters_to_context(video_decoder, video_stream->codecpar);
	avcodec_open2(video_decoder, video_codec, NULL);

	AVCodecContext* audio_decoder = avcodec_alloc_context3(audio_codec);
	audio_decoder->thread_count = 0;
	avcodec_parameters_to_context(audio_decoder, audio_stream->codecpar);
	avcodec_open2(audio_decoder, audio_codec, NULL);

	AVPacket* packet = av_packet_alloc();
	AVFrame* frame = av_frame_alloc();

	SDL_AudioSpec audio_spec = { SDL_AUDIO_F32, audio_decoder->ch_layout.nb_channels, audio_decoder->sample_rate };
	SDL_AudioStream* audio_playback_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audio_spec, NULL, NULL);
	SDL_ResumeAudioStreamDevice(audio_playback_stream);

	SDL_Texture* texture = SDL_CreateTexture(mSDLRenderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, video_decoder->width, video_decoder->height);
	SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

	SDL_SetRenderDrawColor(mSDLRenderer, bgColor.mRed, bgColor.mGreen, bgColor.mBlue, bgColor.mAlpha);
	SDL_RenderClear(mSDLRenderer);

	bool willShutdown = false;

	Uint64 start_ns = 0;
	Uint64 curElapsed = 0;

	while (av_read_frame(format_context, packet) >= 0) {
		mLastUserInputTick = mLastTimerTime;

		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_EVENT_QUIT:
				willShutdown = true;
				break;
			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
				if (!mSDLWindow || event.window.windowID == SDL_GetWindowID(mSDLWindow))
					willShutdown = true;
				break;
			case SDL_EVENT_WINDOW_FOCUS_GAINED:
				mActive = true;
				RehupFocus();
				EnforceCursor();
				break;
			case SDL_EVENT_WINDOW_FOCUS_LOST:
				mActive = false;
				RehupFocus();
				break;
			case SDL_EVENT_KEY_DOWN:
			{
				mLastUserInputTick = mLastTimerTime;
				if (mDebugKeysEnabled)
				{
					if (DebugKeyDown(GetKeyCodeFromCodeSDL(event.key.key)))
						break;
				}
				else
				{
					KeyCode theKey = GetKeyCodeFromCodeSDL(event.key.key);
					if (theKey == KEYCODE_F10)
					{
						TakeScreenshot();
						break;
					}
					else if (theKey == KeyCode::KEYCODE_F11)
					{
						gLawnApp->SwitchScreenMode(!gLawnApp->mIsWindowed, true);
						break;
					}
				}

				if (isSkipable && event.key.key == SDLK_ESCAPE)
				{
					mIsPlayingVideo = false;
					break;
				}

				int theChar = GetKeyCodeFromCodeSDL(event.key.key);

				if ((theChar < KEYCODE_ASCIIBEGIN || theChar > KEYCODE_ASCIIEND) && (theChar < KEYCODE_ASCIIBEGIN2 || theChar > KEYCODE_ASCIIEND2))
				{
					theChar = -1;
				}

				switch (event.key.key)
				{
				case SDLK_KP_PLUS:   theChar = '+'; break;
				case SDLK_KP_MINUS:  theChar = '-'; break;
				case SDLK_KP_MULTIPLY: theChar = '*'; break;
				case SDLK_SLASH:
				case SDLK_KP_DIVIDE: theChar = '/'; break;
				case SDLK_KP_PERIOD: theChar = '.'; break;

				case SDLK_KP_0: theChar = '0'; break;
				case SDLK_KP_1: theChar = '1'; break;
				case SDLK_KP_2: theChar = '2'; break;
				case SDLK_KP_3: theChar = '3'; break;
				case SDLK_KP_4: theChar = '4'; break;
				case SDLK_KP_5: theChar = '5'; break;
				case SDLK_KP_6: theChar = '6'; break;
				case SDLK_KP_7: theChar = '7'; break;
				case SDLK_KP_8: theChar = '8'; break;
				case SDLK_KP_9: theChar = '9'; break;
				}

				if (theChar == 'D' && (mWidgetManager != NULL) && (mWidgetManager->mKeyDown[KEYCODE_CONTROL]) && (mWidgetManager->mKeyDown[KEYCODE_MENU]))
				{
					PlaySoundA("c:\\windows\\media\\Windows XP Menu Command.wav", NULL, SND_ASYNC);
					mDebugKeysEnabled = !mDebugKeysEnabled;
				}

				mWidgetManager->KeyDown(GetKeyCodeFromCodeSDL(event.key.key));

				if (!SDL_TextInputActive(mSDLWindow)) {

					bool shift = (event.key.mod & SDL_KMOD_SHIFT) != 0;
					bool caps = (event.key.mod & SDL_KMOD_CAPS) != 0;

					SexyChar c = theChar;

					if (isalpha(c))
					{
						if (shift ^ caps)
							c = toupper(c);
						else
							c = tolower(c);
					}
					mWidgetManager->KeyChar(c);
				}

				break;
			}
			case SDL_EVENT_KEY_UP:
				mLastUserInputTick = mLastTimerTime;
				mWidgetManager->KeyUp(GetKeyCodeFromCodeSDL(event.key.key));
				break;
			}
		}

		if (!mIsPlayingVideo || willShutdown)
		{
			SDL_RenderClear(mSDLRenderer);
			av_packet_unref(packet);
			break;
		}

		std::vector<std::thread> _jobs;

		curElapsed = SDL_GetTicksNS();

		if (packet->stream_index == video_stream_index && start_ns != 0) {
			const int width = mWidth;
			const int height = mHeight;

			_jobs.emplace_back(std::thread([video_decoder, packet, frame, &start_ns, video_stream, texture, width, height, curElapsed]() {
				avcodec_send_packet(video_decoder, packet);					
				while (avcodec_receive_frame(video_decoder, frame) == 0) {
					const double frame_time_s = (double)frame->pts * av_q2d(video_stream->time_base);
					const Uint64 elapsed_time_ns = curElapsed - start_ns;
					const double elapsed_time_s = (double)elapsed_time_ns / SDL_NS_PER_SECOND;
					const double delay_s = frame_time_s - elapsed_time_s;
					if (delay_s > 0) SDL_Delay((Uint32)(delay_s * SDL_MS_PER_SECOND));
					else if (delay_s < -0.5) continue;

					SDL_UpdateYUVTexture(texture, NULL,
						frame->data[0], frame->linesize[0],
						frame->data[1], frame->linesize[1],
						frame->data[2], frame->linesize[2]);

					const float frame_width = (float)video_decoder->width;
					const float frame_height = (float)video_decoder->height;
					const float scale_w = (float)width / frame_width;
					const float scale_h = (float)height / frame_height;
					const float scale = SDL_max(scale_w, scale_h);
					SDL_FRect dstrect;
					dstrect.w = frame_width * scale;
					dstrect.h = frame_height * scale;
					dstrect.x = ((float)width - dstrect.w) / 2;
					dstrect.y = ((float)height - dstrect.h) / 2;

					SDL_RenderTexture(mSDLRenderer, texture, NULL, &dstrect);
					SDL_RenderPresent(mSDLRenderer);
				}
				av_packet_unref(packet);
			}));
		}
		else if (packet->stream_index == audio_stream_index) {
			_jobs.emplace_back(std::thread([audio_decoder, packet, frame, audio_playback_stream, &start_ns, curElapsed]() {
				avcodec_send_packet(audio_decoder, packet);
				while (avcodec_receive_frame(audio_decoder, frame) == 0) {
					if (start_ns == 0) start_ns = curElapsed;

					SDL_PutAudioStreamPlanarData(audio_playback_stream, (const void* const*)frame->data, audio_decoder->ch_layout.nb_channels, frame->nb_samples);
				}
				av_packet_unref(packet);
			}));
		}

		for (auto& job : _jobs) {
			job.join();
		}
	}
	
	SDL_DestroyTexture(texture);

	mIsPlayingVideo = false;

	SDL_ShowCursor();

	if (willShutdown) Shutdown();

	return true;
}

void LawnApp::MakeWindow()
{
	//SexyAppBase::MakeWindow();
	if (mSDLWindow != nullptr) {
		SDL_DestroyRenderer(mSDLRenderer);
		SDL_DestroyWindow(mSDLWindow);
	}

	if (mDDInterface == nullptr) {
		mDDInterface = new DDInterface(this);
	}

	if (IsScreenSaver())
	{
		mTitle = _S("Zen Garden");
		mIsWindowed = false;
		mFullScreenWindow = true;
	}
	else if (IsPreviewSaver())
	{
		mTitle = _S("Zen Garden Preview");
		mIsWindowed = true;
		mFullScreenWindow = false;
	}
	else if (IsParticleEditor())
	{
		mTitle = _S("Particle Editor v0.1 by @inliothixie");
		mIsWindowed = true;
		mFullScreenWindow = false;
	}

#define _WIDE_SCREEN
	int aBaseWidth = 1066;
	int aBaseHeight = 600;
#ifdef _ULTRA_WIDESCREEN
	aBaseWidth = 1280;
	aBaseHeight = 720;
#endif

	// coop.ini: user-selected window size (e.g. 1366x768 / 1920x1080)
	if (mNetManager && mNetManager->mConfig.mWindowWidth > 0)
		aBaseWidth = mNetManager->mConfig.mWindowWidth;
	if (mNetManager && mNetManager->mConfig.mWindowHeight > 0)
		aBaseHeight = mNetManager->mConfig.mWindowHeight;

	mWidth = aBaseWidth;
	mHeight = aBaseHeight;

	// generic widescreen centering (matches the presets: 1066->133, 1280->240/60)
	mDDInterface->mWideScreenOffsetX = (mWidth - 800) / 2;
	mDDInterface->mWideScreenOffsetY = (mHeight - 600) / 2;

	gBoardBounds = Rect{ 0, 0, mWidth, mHeight };

	unsigned long long windowFlags = 0UL;
	if (!IsParticleEditor()) windowFlags |= SDL_WINDOW_RESIZABLE;
	if (IsScreenSaver() || !mIsWindowed) windowFlags |= SDL_WINDOW_FULLSCREEN;
	windowFlags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
	const bool aBackgroundSmoke = CoopSmokeActive() && getenv("PVZ_COOP_SMOKE_BACKGROUND") &&
		strcmp(getenv("PVZ_COOP_SMOKE_BACKGROUND"), "1") == 0;
	if (aBackgroundSmoke) windowFlags |= SDL_WINDOW_HIDDEN;

	SDL_RendererLogicalPresentation presentationMode;

	mSDLWindow = SDL_CreateWindow(mTitle.c_str(), mWidth, mHeight, windowFlags);
	mSDLRenderer = SDL_CreateRenderer(mSDLWindow, nullptr);
	SDL_SetRenderVSync(mSDLRenderer, mEnableVsync);
	SDL_SetRenderLogicalPresentation(mSDLRenderer, mWidth, mHeight, SDL_LOGICAL_PRESENTATION_LETTERBOX);
	// borderless fullscreen desktop: native monitor resolution with letterboxed scaling
	if (mNetManager && mNetManager->mConfig.mBorderlessFullscreen && !IsParticleEditor())
		SDL_SetWindowFullscreenMode(mSDLWindow, nullptr);
	mHWnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(mSDLWindow), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);

	mWidgetManager->mWidth = mWidth;
	mWidgetManager->mHeight = mHeight;

	mWidgetManager->mImage = new SDL3Image(mSDLRenderer);
	mWidgetManager->mImage->mWidth = mWidth;
	mWidgetManager->mImage->mHeight = mHeight;
	mWidgetManager->mImage->mD3DData = SDL_CreateTexture(mSDLRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, mWidgetManager->mImage->mWidth, mWidgetManager->mImage->mHeight);
	SDL_SetTextureBlendMode((SDL_Texture*)mWidgetManager->mImage->mD3DData, SDL_BLENDMODE_BLEND);
	mWidgetManager->MarkAllDirty();

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui::StyleColorsClassic();

	ImGuiStyle& style = ImGui::GetStyle();
	float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
	style.ScaleAllSizes(main_scale);      
	style.FontScaleDpi = main_scale;

	ImGui_ImplSDL3_InitForSDLRenderer(mSDLWindow, mSDLRenderer);
	ImGui_ImplSDLRenderer3_Init(mSDLRenderer);

	if (!aBackgroundSmoke) SDL_RaiseWindow(mSDLWindow);
}

bool LawnApp::DrawDirtyStuff()
{
	if (mIsPlayingVideo) return true;
	if (IsParticleEditor() && mParticleScreen) mParticleScreen->ImGuiDraw();
	SDL_SetRenderDrawColor(mSDLRenderer, 0, 0, 0, SDL_ALPHA_TRANSPARENT);
	SDL_RenderClear(mSDLRenderer);
	return SexyAppBase::DrawDirtyStuff();
}

bool LawnApp::CoopSmokeCaptureFrame(const char* theBasename)
{
	const char* dir = CoopSmokeActive() ? getenv("PVZ_COOP_SMOKE_CAPTURE_DIR") : nullptr;
	if (!dir || !theBasename || !mCoopSmokeFrame) return false;
	const std::string name(theBasename);
	if (name.empty() || name.size() > 96 || name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string::npos) return false;
	const std::string path = std::string(dir) + "\\" + name + ".bmp";
	return SDL_SaveBMP(mCoopSmokeFrame.get(),path.c_str());
}

void LawnApp::Redraw(Rect* theClipRect)
{
	if (mIsPlayingVideo) return;
	//SexyAppBase::Redraw(theClipRect);
	if (auto drawData = ImGui::GetDrawData())
	{
		ImGui_ImplSDLRenderer3_RenderDrawData(drawData, mSDLRenderer);
	}
	// Co-op overlays are the final visual layer, above widgets and dialogs.
	NetDrawChatOverlay();
	NetDrawCursorOverlay();
	// Read this SDL renderer's actual backbuffer before Present invalidates it.
	// The widget image is only a drawing facade, not a rendered texture.
	const char* aCaptureDir = CoopSmokeActive() ? getenv("PVZ_COOP_SMOKE_CAPTURE_DIR") : nullptr;
	static uint32_t aLastCaptureTick = 0;
	static uint32_t aLastFrameTick = 0;
	static int aLastFrameScene = -1;
	static int aLastCaptureScene = -1;
	if (aCaptureDir && mLoadingThreadCompleted &&
		(aLastFrameScene != (int)mGameScene || GetTickCount() - aLastFrameTick >= 100U))
	{
		SDL_Surface* aFrame = SDL_RenderReadPixels(mSDLRenderer, nullptr);
		if (aFrame)
		{
			mCoopSmokeFrame.reset(aFrame,SDL_DestroySurface);
			aLastFrameTick = GetTickCount(); aLastFrameScene = (int)mGameScene;
		}
	}
	if (aCaptureDir && mCoopSmokeFrame &&
		(aLastCaptureScene != (int)mGameScene || GetTickCount() - aLastCaptureTick >= 2000U))
	{
		aLastCaptureTick = GetTickCount(); aLastCaptureScene = (int)mGameScene;
		CoopSmokeCaptureFrame(("scene_" + std::to_string(aLastCaptureScene)).c_str());
	}
	SDL_RenderPresent(mSDLRenderer);
}

static int mouseMoveCount = 0;
static int lastMouseX = -1;
static int lastMouseY = -1;

bool IsRealMouseMove(const SDL_Event& e)
{
	if (lastMouseX == -1)
	{
		lastMouseX = e.motion.x;
		lastMouseY = e.motion.y;
		return false;
	}

	if (e.motion.x != lastMouseX || e.motion.y != lastMouseY)
	{
		lastMouseX = e.motion.x;
		lastMouseY = e.motion.y;
		mouseMoveCount++;
	}

	return mouseMoveCount >= 4;
}

bool LawnApp::UpdateAppStep(bool* updated)
{
	if (updated != nullptr)
		*updated = false;

	if (mExitToTop)
		return false;

	if (mUpdateAppState == UPDATESTATE_PROCESS_DONE)
		mUpdateAppState = UPDATESTATE_MESSAGES;

	mUpdateAppDepth++;

	auto buttonTrans = [](Uint8 button)
	{
		switch (button)
		{
		case SDL_BUTTON_LEFT: return 1;
		case SDL_BUTTON_RIGHT: return -1;
		case SDL_BUTTON_MIDDLE: return 3;
		}
		return 0;
	};

	if (mUpdateAppState == UPDATESTATE_MESSAGES)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL3_ProcessEvent(&event);
			ImGuiIO& io = ImGui::GetIO();
			bool imguiWantsMouse = io.WantCaptureMouse;
			bool imguiWantsKeyboard = io.WantCaptureKeyboard;
			
			SDL_ConvertEventToRenderCoordinates(mSDLRenderer, &event);

			if (IsScreenSaver()) {
				switch (event.type)
				{
					case SDL_EVENT_QUIT:
						CloseRequestAsync();
						break;
					case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
						if (!mSDLWindow || event.window.windowID == SDL_GetWindowID(mSDLWindow))
							CloseRequestAsync();
						break;

					case SDL_EVENT_MOUSE_MOTION:
						if (IsRealMouseMove(event))
							Shutdown();
						break;

					case SDL_EVENT_MOUSE_BUTTON_DOWN:
					case SDL_EVENT_KEY_DOWN:
					case SDL_EVENT_TEXT_INPUT:
							Shutdown();
						break;

					default:
						break;
				}
			}
			else {
				switch (event.type)
				{
				case SDL_EVENT_QUIT:
					LawnNet::NetTrace("SDL close requested event=quit");
					CloseRequestAsync();
					break;
				case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
					if (!mSDLWindow || event.window.windowID == SDL_GetWindowID(mSDLWindow))
					{
						LawnNet::NetTrace("SDL close requested event=window windowId=%u mainWindowId=%u",
							(unsigned)event.window.windowID,
							mSDLWindow ? (unsigned)SDL_GetWindowID(mSDLWindow) : 0U);
						CloseRequestAsync();
					}
					break;
				case SDL_EVENT_MOUSE_BUTTON_DOWN:
					if ((!gInAssert) && (!mSEHOccured) && !imguiWantsMouse)
					{
						int x = event.button.x;
						int y = event.button.y;
						mWidgetManager->RemapMouse(x, y);
						mLastUserInputTick = mLastTimerTime;
						mWidgetManager->MouseMove(x, y);

						if (!mMouseIn)
						{
							mMouseIn = true;
							EnforceCursor();
						}

						// co-op client: during play all board clicks go to the host
						if (NetHandlePointerInput(LawnNet::NETINPUT_MOUSE_DOWN, buttonTrans(event.button.button), x, y)) break;
						if (NetIsClient() && mBoard && mGameScene == GameScenes::SCENE_PLAYING && GetDialogCount() == 0)
							mNetManager->SendInput(true, buttonTrans(event.button.button), x, y);
						else
							// Use the same remapped logical coordinates as MouseMove and
							// network input.  Feeding the raw SDL window coordinates here
							// displaced local clicks after a resize/letterbox transform.
							mWidgetManager->MouseDown(x, y, buttonTrans(event.button.button));
					}
					break;
				case SDL_EVENT_MOUSE_BUTTON_UP:
					if ((!gInAssert) && (!mSEHOccured) && !imguiWantsMouse)
					{
						int x = event.button.x;
						int y = event.button.y;
						mWidgetManager->RemapMouse(x, y);
						mLastUserInputTick = mLastTimerTime;
						mWidgetManager->MouseMove(x, y);

						if (!mMouseIn)
						{
							mMouseIn = true;
							EnforceCursor();
						}

						// co-op client: during play all board clicks go to the host
						if (NetHandlePointerInput(LawnNet::NETINPUT_MOUSE_UP, buttonTrans(event.button.button), x, y)) break;
						if (NetIsClient() && mBoard && mGameScene == GameScenes::SCENE_PLAYING && GetDialogCount() == 0)
							mNetManager->SendInput(false, buttonTrans(event.button.button), x, y);
						else
							mWidgetManager->MouseUp(x, y, buttonTrans(event.button.button));
					}
					break;
				case SDL_EVENT_MOUSE_MOTION:
					if ((!gInAssert) && (!mSEHOccured) && !imguiWantsMouse)
					{
						int x = event.motion.x;
						int y = event.motion.y;
						mWidgetManager->RemapMouse(x, y);
						mLastUserInputTick = mLastTimerTime;
						mWidgetManager->MouseMove(x, y);
						if (NetHandlePointerInput(LawnNet::NETINPUT_MOUSE_MOVE, 0, x, y)) break;
						if (NetIsClient() && mBoard && mGameScene == GameScenes::SCENE_PLAYING && GetDialogCount() == 0)
						{
							int aDragButton = 0;
							if (event.motion.state & SDL_BUTTON_LMASK)
								aDragButton = 1;
							else if (event.motion.state & SDL_BUTTON_RMASK)
								aDragButton = -1;
							else if (event.motion.state & SDL_BUTTON_MMASK)
								aDragButton = 3;
							mNetManager->SendMouseMove(aDragButton, x, y);
						}

						if (!mMouseIn)
						{
							mMouseIn = true;
							EnforceCursor();
						}
					}
					break;
				case SDL_EVENT_MOUSE_WHEEL:
					if (!imguiWantsMouse)
					{
						mLastUserInputTick = mLastTimerTime;
						mWidgetManager->MouseWheel(event.wheel.y);
						
					}
					break;
				case SDL_EVENT_WINDOW_FOCUS_GAINED:
					mActive = true;
					RehupFocus();
					EnforceCursor();
					break;
				case SDL_EVENT_WINDOW_FOCUS_LOST:
					if (NetIsClient() && mNetManager)
						mNetManager->SendHeatWaveShoutKey(false);
					mActive = false;
					RehupFocus();
					break;
				case SDL_EVENT_WINDOW_RESIZED:
				{
					float scale = 1.0f;
					int pw, ph;
					SDL_GetWindowSizeInPixels(mSDLWindow, &pw, &ph);

					if (pw >= 800 || ph >= 600) scale = max(max(static_cast<float>(pw) / 800.0f, static_cast<float>(ph) / 600.0f), 1.0f);
					SDL3Font::RebuildFonts(scale);
					break;
				}
				case SDL_EVENT_KEY_DOWN:
				{
					if (!imguiWantsKeyboard)
					{
						mLastUserInputTick = mLastTimerTime;
						if (NetHandleChatKey(GetKeyCodeFromCodeSDL(event.key.key)))
							break;
						if (NetIsClient() && mNetManager && event.key.key == SDLK_Q)
							mNetManager->SendHeatWaveShoutKey(true);
						if (mDebugKeysEnabled)
						{
							if (DebugKeyDown(GetKeyCodeFromCodeSDL(event.key.key)))
								break;
						}
						else
						{
							KeyCode theKey = GetKeyCodeFromCodeSDL(event.key.key);
							if (theKey == KEYCODE_F10)
							{
								TakeScreenshot();
								break;
							}
							else if (theKey == KeyCode::KEYCODE_F11)
							{
								gLawnApp->SwitchScreenMode(!gLawnApp->mIsWindowed, true);
								break;
							}
						}

						int theChar = GetKeyCodeFromCodeSDL(event.key.key);

						if ((theChar < KEYCODE_ASCIIBEGIN || theChar > KEYCODE_ASCIIEND) && (theChar < KEYCODE_ASCIIBEGIN2 || theChar > KEYCODE_ASCIIEND2))
						{
							theChar = -1;
						}

						{
							bool shift = (event.key.mod & SDL_KMOD_SHIFT) != 0;
							switch (event.key.key)
							{
							case SDLK_EXCLAIM: theChar = '!'; break;
							case SDLK_AT: theChar = '@'; break;
							case SDLK_KP_HASH:   theChar = '#'; break;
							case SDLK_DOLLAR:   theChar = '$'; break;
							case SDLK_PERCENT:   theChar = '%'; break;
							case SDLK_AMPERSAND:   theChar = '&'; break;
							case SDLK_ASTERISK:   theChar = '*'; break;

							case SDLK_KP_PLUS:   theChar = '+'; break;
							case SDLK_KP_MINUS:  theChar = '-'; break;
							case SDLK_KP_MULTIPLY: theChar = '*'; break;
							case SDLK_SLASH:
							case SDLK_KP_DIVIDE: theChar = '/'; break;
							case SDLK_KP_PERIOD: theChar = '.'; break;

							case SDLK_KP_0: theChar = shift ? ')' : '0'; break;
							case SDLK_KP_1: theChar = shift ? '!' : '1'; break;
							case SDLK_KP_2: theChar = shift ? '@' : '2'; break;
							case SDLK_KP_3: theChar = shift ? '#' : '3'; break;
							case SDLK_KP_4: theChar = shift ? '$' : '4'; break;
							case SDLK_KP_5: theChar = shift ? '%' : '5'; break;
							case SDLK_KP_6: theChar = shift ? '^' : '6'; break;
							case SDLK_KP_7: theChar = shift ? '&' : '7'; break;
							case SDLK_KP_8: theChar = shift ? '*' : '8'; break;
							case SDLK_KP_9: theChar = shift ? '(' : '9'; break;
							}
						}

						if (theChar != -1 && theChar == 'D' && (mWidgetManager != NULL) && (mWidgetManager->mKeyDown[KEYCODE_CONTROL]) && (mWidgetManager->mKeyDown[KEYCODE_MENU]))
						{
							PlaySoundA("c:\\windows\\media\\Windows XP Menu Command.wav", NULL, SND_ASYNC);
							mDebugKeysEnabled = !mDebugKeysEnabled;
						}

						// SDL text input does not emit Ctrl+C/V as characters. Translate
						// the standard edit shortcuts explicitly for focused EditWidgets.
						SexyChar aShortcutChar = 0;
						if ((event.key.mod & SDL_KMOD_CTRL) != 0)
						{
							if (event.key.key == SDLK_C) aShortcutChar = 3;
							else if (event.key.key == SDLK_V) aShortcutChar = 22;
							else if (event.key.key == SDLK_X) aShortcutChar = 24;
							else if (event.key.key == SDLK_Z) aShortcutChar = 26;
						}

						// co-op client: no pause/options/cheat keys during play (host-only control)
						bool aClientBlockKeys = NetIsClient() && mBoard && mGameScene == GameScenes::SCENE_PLAYING && GetDialogCount() == 0;
						if (!aClientBlockKeys)
						{
							if (aShortcutChar != 0)
								mWidgetManager->KeyChar(aShortcutChar);
							else
								mWidgetManager->KeyDown(GetKeyCodeFromCodeSDL(event.key.key));

							if (aShortcutChar == 0 && theChar != -1 && !SDL_TextInputActive(mSDLWindow)) {

								bool shift = (event.key.mod & SDL_KMOD_SHIFT) != 0;
								bool caps = (event.key.mod & SDL_KMOD_CAPS) != 0;

								SexyChar c = theChar;

								if (isalpha(c))
								{
									if (shift ^ caps)
										c = toupper(c);
									else
										c = tolower(c);
								}
								mWidgetManager->KeyChar(c);
							}
						}
					}
					break;
				}
				case SDL_EVENT_KEY_UP:
					if ((event.key.key == SDLK_LCTRL || event.key.key == SDLK_RCTRL) && mNetDrawing)
						NetHandlePointerInput(LawnNet::NETINPUT_MOUSE_UP, 0, mNetLastDrawX, mNetLastDrawY);
					if (NetIsClient() && mNetManager && event.key.key == SDLK_Q)
						mNetManager->SendHeatWaveShoutKey(false);
					if (!imguiWantsKeyboard)
					{
						mLastUserInputTick = mLastTimerTime;
						mWidgetManager->KeyUp(GetKeyCodeFromCodeSDL(event.key.key));
					}
					break;
				case SDL_EVENT_TEXT_INPUT:
					if (!imguiWantsKeyboard)
					{
						mLastUserInputTick = mLastTimerTime;
						if (NetHandleChatText(event.text.text))
							break;
						// SDL delivers UTF-8, while the GOTY build uses wide SexyString.
						// Decode the complete sequence before feeding the focused edit widget.
						try
						{
							SexyString aText = StringToSexyString(std::string(event.text.text));
							for (SexyChar aChar : aText)
								mWidgetManager->KeyChar(aChar);
						}
						catch (const std::range_error&)
						{
							LawnNet::NetTrace("SDL text input rejected malformed UTF-8");
						}
					}
					break;

				}
			}

			
		}
		mUpdateAppState = UPDATESTATE_PROCESS_1;
	}
	else
	{
		if (mStepMode)
		{
			if (mStepMode == 2)
			{
				Sleep(mFrameTime);
				mUpdateAppState = UPDATESTATE_PROCESS_DONE;
			}
			else
			{
				mStepMode = 2;
				DoUpdateFrames();
				DoUpdateFramesF(1.0f);
				DrawDirtyStuff();
			}
		}
		else
		{
			int anOldUpdateCnt = mUpdateCount;
			Process();
			if (updated != NULL)
				*updated = mUpdateCount != anOldUpdateCnt;
		}
	}

	mUpdateAppDepth--;

	return true;
}

//0x44E8A0
bool LawnGetCloseRequest()
{
	if (gLawnApp == nullptr)
		return false;

	return gLawnApp->mCloseRequest;
}

//0x44E8C0
bool LawnHasUsedCheatKeys()
{
	return gLawnApp && gLawnApp->mPlayerInfo && gLawnApp->mPlayerInfo->mHasUsedCheatKeys;
}

//0x44EAA0
LawnApp::LawnApp()
{
	mBoard = nullptr;
	mGameSelector = nullptr;
	mChallengeScreen = nullptr;
	mSeedChooserScreen = nullptr;
	mAwardScreen = nullptr;
	mCreditScreen = nullptr;
	mLanguageScreen = nullptr;
	mParticleScreen = nullptr;
	mTitleScreen = nullptr;
	mSoundSystem = nullptr;
	mKonamiCheck = nullptr;
	mMustacheCheck = nullptr;
	mMoustacheCheck = nullptr;
	mSuperMowerCheck = nullptr;
	mSuperMowerCheck2 = nullptr;
	mFutureCheck = nullptr;
	mPinataCheck = nullptr;
	mDanceCheck = nullptr;
	mDaisyCheck = nullptr;
	mSukhbirCheck = nullptr;
	mMustacheMode = false;
	mSuperMowerMode = false;
	mFutureMode = false;
	mPinataMode = false;
	mDanceMode = false;
	mDaisyMode = false;
	mSukhbirMode = false;
	mGameScene = GameScenes::SCENE_LOADING;
	mPoolEffect = nullptr;
	mZenGarden = nullptr;
	mEffectSystem = nullptr;
	mReanimatorCache = nullptr;
	mCloseRequest = false;
	mWidth = BOARD_WIDTH;
	mHeight = BOARD_HEIGHT;
	mFullscreenBits = 32;
	mAppCounter = 0;
	mAppRandSeed = _time64(nullptr);
	mTrialType = TrialType::TRIALTYPE_NONE;
	mDebugTrialLocked = false;
	mMuteSoundsForCutscene = false;
	mMusicVolume = 0.85;
	mSfxVolume = 0.5525;
	mAutoStartLoadingThread = false;
	mDebugKeysEnabled = false;
	mProdName = "PlantsVsZombies";
	SexyString aTitleName = _S("Plants vs. Zombies");
#ifdef _DEBUG
	aTitleName += _S(" ") + StringToSexyString(mProductVersion);
	aTitleName += _S(" DEBUG");
#endif
	mTitle = aTitleName;
	mCustomCursorsEnabled = false;
	mPlayerInfo = nullptr;
	mNetManager = nullptr;
	mNetLobbyPanel = nullptr;
	mNetBoardSeed = 0;
	mNetBlockProfileSave = false;
	mNetLocalProfileBackup = nullptr;
	mNetLastProfileHash = 0;
	mNetHostUnlockAllMinigames = false;
	mNetPendingPvpLevel = -1;
	mNetPvpTeamsConfirmed = false;
	mNetCoopGameOverHandled = false;
	mNetCursorImage = nullptr;
	mNetChatTyping = false;
	mNetChatDraft.clear();
	mCoopSmokeArgSeen = false;
	mCoopSmokeManifestValid = false;
	mCoopSmokeLifecycleStarted = false;
	mCoopSmokeGameStarted = false;
	mCoopSmokePeerReady = false;
	mCoopSmokeEverConnected = false;
	mCoopSmokeResultWritten = false;
	mCoopSmokeStartedTick = 0;
	mCoopSmokeSnapshotsSent = 0;
	mCoopSmokeSnapshotsReceived = 0;
	mCoopSmokeSnapshotsApplied = 0;
	mCoopSmokeUnicodeChatSent = false;
	mCoopSmokeUnicodeChatReceived = false;
	mCoopSmokeUnicodeChatVisible = false;
	mCoopSmokeUnicodeChatFaded = false;
	mCoopSmokeScenarioRuntime.Reset();
	mLastLevelStats = new LevelStats();
	mFirstTimeGameSelector = true;
	mGameMode = GameMode::GAMEMODE_ADVENTURE;
	mEasyPlantingCheat = false;
	mAutoEnable3D = true;
	Tod_SWTri_AddAllDrawTriFuncs();
	mLoadingZombiesThreadCompleted = true;
	mGamesPlayed = 0;
	mMaxExecutions = 0;
	mMaxPlays = 0;
	mMaxTime = 0;
	mCompletedLoadingThreadTasks = 0;
	mProfileMgr = new ProfileMgr();
	mRegisterResourcesLoaded = false;
	mTodCheatKeys = false;
	mCrazyDaveReanimID = ReanimationID::REANIMATIONID_NULL;
	mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_OFF;
	mCrazyDaveBlinkCounter = 0;
	mCrazyDaveBlinkReanimID = ReanimationID::REANIMATIONID_NULL;
	mCrazyDaveMessageIndex = -1;
	mBigArrowCursor = LoadCursor(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDC_CURSOR1));
	mDRM = nullptr;
	mShowHealthBar = false;
	mVoiceVolume = 0.0f;
	memset(&mFlowersPlucked, false, sizeof(mFlowersPlucked));
	mRIPMode = false;
	mPlayerLevelRef = -1;
}

//0x44EDD0、0x44EDF0
LawnApp::~LawnApp()
{
	const bool aHadBoard = mBoard != nullptr;
	const bool aHadNetProfileOverride = mNetBlockProfileSave || mNetLocalProfileBackup != nullptr;

	delete mNetCursorImage;
	mNetCursorImage = nullptr;

	if (mBoard)
	{
		mBoardResult = BoardResult::BOARDRESULT_QUIT_APP;
		mBoard->TryToSaveGame();
		mWidgetManager->RemoveWidget(mBoard);
		delete mBoard;
		mBoard = nullptr;
	}

	// A client board may still contain host-synchronised progress.  Keep the
	// write guard active through its complete teardown, then restore the local
	// profile before any final profile write.
	NetRestoreLocalProfile();
	if (aHadBoard || aHadNetProfileOverride)
		WriteCurrentUserConfig();

	if (mTitleScreen)
	{
		mWidgetManager->RemoveWidget(mTitleScreen);
		delete mTitleScreen;
	}

	delete mSoundSystem;
	delete mMusic;

	if (mKonamiCheck)
	{
		delete mKonamiCheck;
	}
	if (mMustacheCheck)
	{
		delete mMustacheCheck;
	}
	if (mMoustacheCheck)
	{
		delete mMoustacheCheck;
	}
	if (mSuperMowerCheck)
	{
		delete mSuperMowerCheck;
	}
	if (mSuperMowerCheck2)
	{
		delete mSuperMowerCheck2;
	}
	if (mFutureCheck)
	{
		delete mFutureCheck;
	}
	if (mPinataCheck)
	{
		delete mPinataCheck;
	}
	if (mDanceCheck)
	{
		delete mDanceCheck;
	}
	if (mDaisyCheck)
	{
		delete mDaisyCheck;
	}
	if (mSukhbirCheck)
	{
		delete mSukhbirCheck;
	}

	if (mGameSelector)
	{
		mWidgetManager->RemoveWidget(mGameSelector);
		delete mGameSelector;
	}
	if (mChallengeScreen)
	{
		mWidgetManager->RemoveWidget(mChallengeScreen);
		delete mChallengeScreen;
	}
	if (mSeedChooserScreen)
	{
		mWidgetManager->RemoveWidget(mSeedChooserScreen);
		delete mSeedChooserScreen;
	}
	if (mAwardScreen)
	{
		mWidgetManager->RemoveWidget(mAwardScreen);
		delete mAwardScreen;
	}
	if (mCreditScreen)
	{
		mWidgetManager->RemoveWidget(mCreditScreen);
		delete mCreditScreen;
	}
	if (mLanguageScreen)
	{
		mWidgetManager->RemoveWidget(mLanguageScreen);
		delete mLanguageScreen;
	}
	if (mParticleScreen)
	{
		mWidgetManager->RemoveWidget(mParticleScreen);
		delete mParticleScreen;
	}

	delete mProfileMgr;
	delete mLastLevelStats;

	// NetRestoreLocalProfile deliberately retains this backup on failure so
	// every remaining save path stays fail-closed.  At final object destruction
	// there can be no retry, so only the heap copy itself needs releasing.
	delete mNetLocalProfileBackup;
	mNetLocalProfileBackup = nullptr;

	mResourceManager->DeleteResources("");
#ifdef _DEBUG
	BetaSubmit(true);
#endif
}

//0x44F200
void LawnApp::Shutdown()
{
	if (!mLoadingThreadCompleted)
	{
		mLoadingFailed = true;
		return;
	}
	if (CoopSmokeActive() && !mCoopSmokeResultWritten)
		CoopSmokeWriteResult(false, "process closed before smoke completion");

	const bool aHadBoard = mBoard != nullptr;
	const bool aHadNetProfileOverride = mNetBlockProfileSave || mNetLocalProfileBackup != nullptr;
	NetShutdown();

	if (!mShutdown)
	{
		for (int i = 0; i < Dialogs::NUM_DIALOGS; i++)
		{
			KillDialog(i);
		}

		if (mBoard)
		{
			mBoardResult = BoardResult::BOARDRESULT_QUIT_APP;
			mBoard->TryToSaveGame();
			KillBoard();
		}

		// The host profile mirror must remain read-only until Board/Zen Garden
		// disposal is finished.  Restore even when no Board was ever created.
		NetRestoreLocalProfile();
		if (aHadBoard || aHadNetProfileOverride)
			WriteCurrentUserConfig();

#ifdef _HAS_ZOMBATAR
		if (mGameSelector && mGameSelector->mZombatarWidget && mGameSelector->mZombatarWidget->mZombie) {
			mGameSelector->mZombatarWidget->mZombie->DieNoLoot();
			delete mGameSelector->mZombatarWidget->mZombie;
			mGameSelector->mZombatarWidget->mZombie = nullptr;
		}
#endif

		ProcessSafeDeleteList();

		if (mPoolEffect)
		{
			mPoolEffect->PoolEffectDispose();
			delete mPoolEffect;
			mPoolEffect = nullptr;
		}

		if (mZenGarden)
		{
			delete mZenGarden;
			mZenGarden = nullptr;
		}

		if (mEffectSystem)
		{
			mEffectSystem->EffectSystemDispose();
			delete mEffectSystem;
			mEffectSystem = nullptr;
		}

		if (mReanimatorCache)
		{
			mReanimatorCache->ReanimatorCacheDispose();
			delete mReanimatorCache;
			mReanimatorCache = nullptr;
		}

		DisposeZombatarClothesCache();
		FilterEffectDisposeForApp();
		TodParticleFreeDefinitions();
		ReanimatorFreeDefinitions();
		TrailFreeDefinitions();
		FreeGlobalAllocators();
		UpdateRegisterInfo();

		if (mPortAudioStream)
		{
			Pa_StopStream(mPortAudioStream);
			Pa_CloseStream(mPortAudioStream);
			Pa_Terminate();

			mPortAudioStream = nullptr;
		}

		SexyAppBase::Shutdown();

		ImGui_ImplSDLRenderer3_Shutdown();
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();

		SDL_DestroyRenderer(mSDLRenderer);
		SDL_DestroyWindow(mSDLWindow);
		SDL_Quit();

		if (mDRM)
		{
			delete mDRM;
		}
		mDRM = nullptr;
	}
}

//0x44F380
void LawnApp::KillBoard()
{
	// A remote Board callback may synchronously tear the board down while its
	// per-player cursor is temporarily installed as mCursorObject.  Restore the
	// actual owned host cursor before disposal so Board::~Board cannot delete a
	// remote cursor twice (and cannot leak the host cursor).
	if (mNetManager && mBoard && mNetManager->GetRawActingPlayerSlot() > 0)
	{
		LawnNet::NetTrace("KillBoard restoring remote input guard slot=%d board=%p",
			mNetManager->GetRawActingPlayerSlot(), (void*)mBoard);
		mBoard->mCursorObject = mBoard->mNetPlayerCursors[0];
		mNetManager->SetActingPlayerSlot(-1);
	}

	// Board messages contain ids whose meaning is scoped to one Board.  Clear
	// them before disposing any old entities so no lifecycle path can leak a
	// stale event or snapshot into the next board.
	if (mNetManager)
	{
		mNetManager->ClearPendingBoardMessages("KILL_BOARD");
		mNetManager->PurgePendingInputs("KILL_BOARD");
	}

	if (mPortAudioStream)
	{
		Pa_StopStream(mPortAudioStream);
		Pa_CloseStream(mPortAudioStream);
		Pa_Terminate();

		mPortAudioStream = nullptr;
	}

	FinishModelessDialogs();
	KillSeedChooserScreen();
	if (mBoard)
	{
#ifdef _DEBUG
		BetaRecordLevelStats();
#endif
		mBoard->DisposeBoard();
		mWidgetManager->RemoveWidget(mBoard);
		SafeDeleteWidget(mBoard);
		mBoard = nullptr;
	}

	SetCursor(CURSOR_POINTER);
}

//0x44F410
bool LawnApp::CanPauseNow()
{
	if (mBoard == nullptr)  // 不在关卡内
		return false;

	if (mSeedChooserScreen && mSeedChooserScreen->mMouseVisible)  // 处于选卡界面
		return false;

	if (mBoard->mBoardFadeOutCounter >= 0)  // 退出关卡过程中
		return false;

	if (mCrazyDaveState != CrazyDaveState::CRAZY_DAVE_OFF)  // 存在戴夫
		return false;

	if (mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN || mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)  // 处于禅境花园或智慧树
		return false;

	return GetDialogCount() <= 0;  // 不存在对话
}

void LawnApp::GotFocus()
{
}

//0x44F460
void LawnApp::LostFocus()
{
	// co-op client never pauses on its own (host controls pausing)
	if (NetIsClient())
		return;

	if (!mTodCheatKeys && CanPauseNow())
	{
		DoPauseDialog();
	}
}

//0x44F480
void LawnApp::WriteToRegistry()
{
	if (CoopSmokeActive())
	{
		// The smoke profile may persist only below its isolated appdata root;
		// never change the interactive user's CurUser or display/audio registry.
		if (mPlayerInfo && !mNetBlockProfileSave)
			mPlayerInfo->SaveDetails();
		else if (mPlayerInfo)
			LawnNet::NetTrace("profile save blocked entry=WriteToRegistry smoke=1 client=%d",
				NetIsClient() ? 1 : 0);
		return;
	}
	if (mPlayerInfo)
	{
		RegistryWriteString("CurUser", SexyStringToString(mPlayerInfo->mName));
		if (!mNetBlockProfileSave)
			mPlayerInfo->SaveDetails();
		else
			LawnNet::NetTrace("profile save blocked entry=WriteToRegistry smoke=0 client=%d",
				NetIsClient() ? 1 : 0);
	}

	SexyAppBase::WriteToRegistry();
}

//0x44F530
void LawnApp::ReadFromRegistry()
{
	if (!CoopSmokeActive())
		SexyApp::ReadFromRegistry();
}

//0x44F540
bool LawnApp::WriteCurrentUserConfig()
{
	if (mNetBlockProfileSave)
	{
		LawnNet::NetTrace("profile save blocked entry=WriteCurrentUserConfig client=%d profile=%p",
			NetIsClient() ? 1 : 0, (void*)mPlayerInfo);
		return false;
	}

	if (mPlayerInfo)
		mPlayerInfo->SaveDetails();

	return true;
}

//0x44F560
void LawnApp::PreNewGame(GameMode theGameMode, bool theLookForSavedGame)
{
	if (NetIsClient())
	{
		LawnNet::NetTrace("client PreNewGame rejected mode=%d saved=%d", (int)theGameMode, theLookForSavedGame ? 1 : 0);
		return;
	}

	if (theGameMode == GAMEMODE_COOP_PVP && NetIsHost() && !mNetPvpTeamsConfirmed)
	{
		NetShowPvpTeamSelection();
		return;
	}

	//if (NeedRegister())
	//{
	//	ShowGameSelector();
	//	return;
	//}

	// co-op host: every game start is broadcast (next level, restarts, survival
	// stages...) and always fresh (clients don't have the host's save file)
	if (mNetManager && mNetManager->IsHosting())
	{
		if (theGameMode == GameMode::GAMEMODE_COOP_PVP &&
			(mNetManager->GetTeamCount(LawnNet::NETTEAM_PLANTS) == 0 ||
			 mNetManager->GetTeamCount(LawnNet::NETTEAM_ZOMBIES) == 0))
		{
			DoDialog(Dialogs::DIALOG_INFO, true, "PvP teams",
				"At least one connected player must choose Plants and one must choose Zombies.",
				_S("[OK_LABEL]"), Dialog::BUTTONS_FOOTER);
			return;
		}
		mNetBoardSeed = CoopSmokeActive() ? mCoopSmokeManifest.mSeed : (uint32_t)Rand();
		mNetManager->mStartTDPath.clear();
		if (theGameMode == GameMode::GAMEMODE_COOP_TOWER_DEFENSE)
			Board::GenerateTowerDefensePath(mNetManager->mStartTDPath);
		mNetManager->BroadcastStartGame((int)theGameMode, mPlayerInfo ? mPlayerInfo->mLevel : 1, mNetBoardSeed);
		theLookForSavedGame = false;
	}

	mGameMode = theGameMode;
	if (theLookForSavedGame && TryLoadGame())
		return;

	SexyString aFileName = GetSavedGameName(mGameMode, mPlayerInfo->mId);
	EraseFile(aFileName);
	NewGame();
}

void LawnApp::PreNewGame(GameMode theGameMode, bool theLookForSavedGame, int theLevel)
{
	if (NetIsClient())
	{
		LawnNet::NetTrace("client PreNewGame(level) rejected mode=%d level=%d saved=%d",
			(int)theGameMode, theLevel, theLookForSavedGame ? 1 : 0);
		return;
	}

	if (theGameMode == GAMEMODE_COOP_PVP && NetIsHost() && !mNetPvpTeamsConfirmed)
	{
		NetShowPvpTeamSelection(theLevel);
		return;
	}

	//if (NeedRegister())
	//{
	//	ShowGameSelector();
	//	return;
	//}

	// co-op host: broadcast (see the overload above)
	if (mNetManager && mNetManager->IsHosting())
	{
		if (theGameMode == GameMode::GAMEMODE_COOP_PVP &&
			(mNetManager->GetTeamCount(LawnNet::NETTEAM_PLANTS) == 0 ||
			 mNetManager->GetTeamCount(LawnNet::NETTEAM_ZOMBIES) == 0))
		{
			DoDialog(Dialogs::DIALOG_INFO, true, "PvP teams",
				"At least one connected player must choose Plants and one must choose Zombies.",
				_S("[OK_LABEL]"), Dialog::BUTTONS_FOOTER);
			return;
		}
		mNetBoardSeed = CoopSmokeActive() ? mCoopSmokeManifest.mSeed : (uint32_t)Rand();
		mNetManager->mStartTDPath.clear();
		if (theGameMode == GameMode::GAMEMODE_COOP_TOWER_DEFENSE)
			Board::GenerateTowerDefensePath(mNetManager->mStartTDPath);
		mNetManager->BroadcastStartGame((int)theGameMode, theLevel, mNetBoardSeed);
		theLookForSavedGame = false;
	}

	mGameMode = theGameMode;
	if (theLookForSavedGame && TryLoadGame(theLevel))
		return;

	SexyString aFileName = GetSavedGameName(mGameMode, mPlayerInfo->mId, theLevel);
	EraseFile(aFileName);
	int thePlayerLevel = mPlayerInfo->mLevel;
	bool isReplaying = mPlayerLevelRef > theLevel;
	mPlayerLevelRef = isReplaying ? thePlayerLevel : -1;
	mPlayerInfo->mLevel = theLevel;
	NewGame();
	mBoard->mIsReplay = isReplaying;
	mPlayerInfo->mLevel = thePlayerLevel;
}

//0x44F5F0
void LawnApp::MakeNewBoard()
{
	LawnNet::NetTrace("MakeNewBoard begin mode=%d scene=%d size=%dx%d oldBoard=%p lobby=%p",
		(int)mGameMode, (int)mGameScene, mWidth, mHeight, (void*)mBoard, (void*)mNetLobbyPanel);
	KillBoard();
	// The co-op lobby is a full-screen widget.  It must be genuinely hidden
	// during play; moving it off-screen leaves it in the widget draw/clip chain
	// and can cover a client's board with the renderer's white fallback surface.
	if (mNetLobbyPanel)
		mNetLobbyPanel->mVisible = false;
	mBoard = new Board(this);
	mBoard->Resize(0, 0, mWidth, mHeight);
	mWidgetManager->AddWidget(mBoard);
	mWidgetManager->BringToBack(mBoard);
	mWidgetManager->SetFocus(mBoard);
	LawnNet::NetTrace("MakeNewBoard created board=%p lobbyVisible=%d widescreenOffset=(%d,%d)",
		(void*)mBoard, mNetLobbyPanel && mNetLobbyPanel->mVisible ? 1 : 0,
		mDDInterface ? mDDInterface->mWideScreenOffsetX : 0,
		mDDInterface ? mDDInterface->mWideScreenOffsetY : 0);
	if (!mPortAudioStream && ChallengeUsesMicrophone(mGameMode))
	{
		if (!TryToInitializePA())
		{
			DoDialog(Dialogs::DIALOG_INFO, true, "Error", "Something went wrong when starting Port Audio.\n\n Please check your Audio driver or whether your system meets the requirements to support PortAudio. Please contact the devoleper", _S("[OK_LABEL]"), Dialog::BUTTONS_FOOTER);
			mPortAudioStream = nullptr;
			DoBackToMain();
			return;
		}
	}
}

//0x44F6B0
void LawnApp::StartPlaying()
{
	if (NetIsClient() && mBoard)
		mBoard->NetUpdateIntroPresentation(true);
	KillSeedChooserScreen();
	if (NetIsClient())
	{
		mBoard->StartLevelClientPresentation();
		mGameScene = GameScenes::SCENE_PLAYING;
	}
	else if (mNetManager && mNetManager->IsHosting())
	{
		// START_PLAYING is the ordered TCP barrier.  Clients enter gameplay
		// before Challenge::StartLevel emits authoritative spawn/events, so none
		// of those messages are discarded as intro traffic.
		mGameScene = GameScenes::SCENE_PLAYING;
		LawnNet::NetTrace("host entering gameplay; broadcasting barrier mode=%d level=%d", (int)mGameMode, mBoard ? mBoard->mLevel : -1);
		mNetManager->AdvanceInputPhase(LawnNet::NETINPUT_PLAYING, "START_PLAYING");
		LawnNet::NetBuffer aCameraBootstrap;
		mBoard->NetEncodeCameraBootstrap(aCameraBootstrap);
		mNetManager->BroadcastMessage(LawnNet::NETMSG_START_PLAYING, aCameraBootstrap);
		mBoard->StartLevel();
	}
	else
	{
		mBoard->StartLevel();
		mGameScene = GameScenes::SCENE_PLAYING;
	}
	LawnNet::NetTrace("gameplay entered client=%d board=%p scene=%d seedChooser=%p", NetIsClient() ? 1 : 0, (void*)mBoard, (int)mGameScene, (void*)mSeedChooserScreen);
}

//0x44F700
bool LawnApp::SaveFileExists()
{
	SexyString aFileName = GetSavedGameName(GameMode::GAMEMODE_ADVENTURE, mPlayerInfo->mId);
	return this->FileExists(aFileName);
}

//0x44F7A0
bool LawnApp::TryLoadGame()
{
	return TryLoadGame(mPlayerInfo->mLevel);
}

bool LawnApp::TryLoadGame(int theLevel)
{
	SexyString aSaveName = GetSavedGameName(mGameMode, mPlayerInfo->mId, theLevel);
	mMusic->StopAllMusic();

	if (this->FileExists(aSaveName))
	{
		MakeNewBoard();
		if (mBoard->LoadGame(aSaveName))
		{
			mFirstTimeGameSelector = false;
			DoContinueDialog();
			return true;
		}

		KillBoard();
	}

	return false;
}


//0x44F890
void LawnApp::NewGame()
{
	NetCloseTransientScreens();
	mFirstTimeGameSelector = false;

	MakeNewBoard();
	if (!mBoard) return;
	mBoard->InitLevel();
	mBoardResult = BoardResult::BOARDRESULT_NONE;
	mGameScene = GameScenes::SCENE_LEVEL_INTRO;

	ShowSeedChooserScreen();
	mBoard->mCutScene->StartLevelIntro();
}

//0x44F8E0
void LawnApp::ShowGameSelector()
{
	LawnNet::NetTrace("ShowGameSelector begin board=%p selector=%p", (void*)mBoard, (void*)mGameSelector);
	NetCloseTransientScreens();
	if (NetIsHost())
	{
		NetBroadcastProfile();
		mNetManager->BroadcastSceneCmd(LawnNet::SCENECMD_SHOW_GAME_SELECTOR, 0);
	}
	KillBoard();
	if (mNetLobbyPanel)
	{
		mNetLobbyPanel->Resize(0, 0, mWidth, mHeight);
		mNetLobbyPanel->mVisible = true;
	}
	//UpdateRegisterInfo();
	if (mGameSelector)
	{
		mWidgetManager->RemoveWidget(mGameSelector);
		SafeDeleteWidget(mGameSelector);
	}
	mGameMode = GameMode::GAMEMODE_ADVENTURE;
	mGameScene = GameScenes::SCENE_MENU;
	mGameSelector = new GameSelector(this);
	mGameSelector->Resize(0, 0, mWidth, mHeight);
	mWidgetManager->AddWidget(mGameSelector);
	mWidgetManager->BringToBack(mGameSelector);
	mWidgetManager->SetFocus(mGameSelector);

	//if (NeedRegister())
	//{
	//	DoNeedRegisterDialog();
	//}
}

//0x44F9E0
void LawnApp::KillGameSelector()
{
	if (mGameSelector)
	{
		mWidgetManager->RemoveWidget(mGameSelector);
		SafeDeleteWidget(mGameSelector);
		mGameSelector = nullptr;
	}
}

//0x44FA20
void LawnApp::ShowAwardScreen(AwardType theAwardType, bool theShowAchievements)
{
	KillAwardScreen();
	// co-op host: mirror the award screen on clients
	if (mNetManager && mNetManager->IsHosting())
	{
		NetBroadcastProfile();
		mNetManager->BroadcastSceneCmd(LawnNet::SCENECMD_SHOW_AWARD, (int)theAwardType);
	}

	mGameScene = GameScenes::SCENE_AWARD;
	mAwardScreen = new AwardScreen(this, theAwardType, theShowAchievements);
	mAwardScreen->Resize(0, 0, mWidth, mHeight);
	mWidgetManager->AddWidget(mAwardScreen);
	mWidgetManager->BringToBack(mAwardScreen);
	mWidgetManager->SetFocus(mAwardScreen);
}

//0x44FAF0
void LawnApp::KillAwardScreen()
{
	if (mAwardScreen)
	{
		if (NetIsHost())
			mNetManager->BroadcastSceneCmd(LawnNet::SCENECMD_CLOSE_AWARD, 0);
		mWidgetManager->RemoveWidget(mAwardScreen);
		SafeDeleteWidget(mAwardScreen);
		mAwardScreen = nullptr;
	}
}

//0x44FB30
void LawnApp::ShowCreditScreen()
{
	mCreditScreen = new CreditScreen(this);
	mCreditScreen->Resize(0, 0, mWidth, mHeight);
	mWidgetManager->AddWidget(mCreditScreen);
	mWidgetManager->BringToBack(mCreditScreen);
	mWidgetManager->SetFocus(mCreditScreen);
}

//0x44FBF0
void LawnApp::KillCreditScreen()
{
	if (mCreditScreen)
	{
		mWidgetManager->RemoveWidget(mCreditScreen);
		SafeDeleteWidget(mCreditScreen);
		mCreditScreen = nullptr;
	}
}

//0x44FC30
void LawnApp::ShowChallengeScreen(ChallengePage thePage)
{
	KillAwardScreen();
	KillChallengeScreen();
	// co-op host: mirror the challenge screen on clients
	if (mNetManager && mNetManager->IsHosting())
		mNetManager->BroadcastSceneCmd(LawnNet::SCENECMD_SHOW_CHALLENGE_PAGE, (int)thePage);

	mGameScene = GameScenes::SCENE_CHALLENGE;
	mChallengeScreen = new ChallengeScreen(this, thePage);
	mChallengeScreen->Resize(0, 0, mWidth, mHeight);
	mWidgetManager->AddWidget(mChallengeScreen);
	mWidgetManager->BringToBack(mChallengeScreen);
	mWidgetManager->SetFocus(mChallengeScreen);
}

//0x44FD00
void LawnApp::KillChallengeScreen()
{
	if (mChallengeScreen)
	{
		if (NetIsHost())
			mNetManager->BroadcastSceneCmd(LawnNet::SCENECMD_CLOSE_CHALLENGE, 0);
		mWidgetManager->RemoveWidget(mChallengeScreen);
		SafeDeleteWidget(mChallengeScreen);
		mChallengeScreen = nullptr;
	}
}

//0x44FD40
StoreScreen* LawnApp::ShowStoreScreen()
{
	// co-op host: mirror the store on clients
	if (mNetManager && mNetManager->IsHosting())
		mNetManager->BroadcastSceneCmd(LawnNet::SCENECMD_SHOW_STORE, 0);

	//FinishModelessDialogs();
	TOD_ASSERT(!GetDialog((int)Dialogs::DIALOG_STORE));

	StoreScreen* aStoreScreen = new StoreScreen(this);
	AddDialog(aStoreScreen);
	mWidgetManager->SetFocus(aStoreScreen);

	return aStoreScreen;
}

void LawnApp::KillStoreScreen()
{
	if (GetDialog(Dialogs::DIALOG_STORE))
	{
		KillDialog(Dialogs::DIALOG_STORE);
		ClearUpdateBacklog(false);
	}
}

//0x44FDC0
void LawnApp::ShowSeedChooserScreen()
{
	if (mSeedChooserScreen)
	{
		LawnNet::NetTrace("ShowSeedChooserScreen ignored duplicate board=%p scene=%d chooser=%p",
			(void*)mBoard, (int)mGameScene, (void*)mSeedChooserScreen);
		return;
	}
	TOD_ASSERT(mSeedChooserScreen == nullptr);

	mSeedChooserScreen = new SeedChooserScreen();
	mSeedChooserScreen->Resize(0, 0, mWidth, mHeight);
	mWidgetManager->AddWidget(mSeedChooserScreen);
	mWidgetManager->BringToBack(mSeedChooserScreen);

	// co-op: tell everyone the chooser is open + the split
	if (mNetManager && mNetManager->IsActive())
	{
		// This widget marks a new chooser generation on the host.  No request
		// queued for the previous generation may be applied to this screen.
		// Client-side authoritative queues are cleared only after a validated
		// CHOOSER_OPEN arrives in NetManager::OnChooserOpen.
		if (mNetManager->IsHosting())
		{
			mNetManager->mRemotePicks.clear();
			mNetManager->mRemoteUnpicks.clear();
		}

		int aBankCount = mBoard ? mBoard->GetNumSeedsInBank() : 10;
		const bool aPvP = mGameMode == GameMode::GAMEMODE_COOP_PVP;
		if (aPvP)
			aBankCount = PVP_PLANT_PACKET_COUNT; // zombie cards are shared, not divided

                if (mNetManager->IsHosting())
                {
                        const int aFixedCount = mSeedChooserScreen->CoopCountGlobalFixedPicks();
                        const int anExpectedFixedCount =
                                SeedChooserScreen::CoopExpectedGlobalFixedPicks(this, mBoard);
                        int aQuotas[LawnNet::NET_MAX_PLAYERS] = {};
                        int anEligibleCount = 0;
                        const bool aValidSplit = aFixedCount == anExpectedFixedCount &&
                                mNetManager->ComputeChooserQuotas(aBankCount, aFixedCount, aPvP,
                                        aQuotas, &anEligibleCount);
                        if (!aValidSplit)
                        {
                                // Do not publish a chooser generation that clients cannot
                                // deterministically validate.  Keep the screen inert instead.
                                mSeedChooserScreen->mCoopChooserActive = false;
                                mSeedChooserScreen->EnableStartButton(false);
                                mNetManager->mChooserOpen = false;
                                LawnNet::NetTrace("ShowSeedChooserScreen rejected host split bank=%d fixed=%d expectedFixed=%d eligible=%d mode=%d",
                                        aBankCount, aFixedCount, anExpectedFixedCount,
                                        anEligibleCount, (int)mGameMode);
                                return;
                        }

                        mNetManager->AdvanceInputPhase(LawnNet::NETINPUT_CHOOSER, "CHOOSER_OPEN");
                        for (int aSlot = 0; aSlot < LawnNet::NET_MAX_PLAYERS; aSlot++)
                                mNetManager->mChooserPickQuota[aSlot] = aQuotas[aSlot];
                        const int aLocalQuota = aQuotas[mNetManager->GetMySlot()];
                        mNetManager->mChooserOpen = true;
                        mNetManager->mChooserPicksPerPlayer = aLocalQuota;
                        mNetManager->mChooserBankCount = aBankCount;
                        mSeedChooserScreen->mCoopChooserActive = true;
                        mSeedChooserScreen->mCoopPicksPerPlayer = aLocalQuota;
                        mSeedChooserScreen->CoopRefreshStartButton();

                        // Commit the host generation before publishing OPEN.  A fast
                        // client may answer from the network thread immediately after
                        // receiving it, and that reply must see the new quotas/open flag.
                        LawnNet::NetBuffer aMsg;
                        aMsg.WriteU8((uint8_t)aBankCount);
                        for (int aSlot = 0; aSlot < LawnNet::NET_MAX_PLAYERS; aSlot++)
                                aMsg.WriteU8((uint8_t)aQuotas[aSlot]);
                        mNetManager->BroadcastMessage(LawnNet::NETMSG_CHOOSER_OPEN, aMsg);

                        LawnNet::NetTrace("ShowSeedChooserScreen role=host mode=%d level=%d localChooserLevel=%d bank=%d fixed=%d mutable=%d quotas=[%d,%d,%d,%d] eligible=%d mySlot=%u localQuota=%d screen=%p pos=(%d,%d) visible=%d mouse=%d focus=%p",
                                (int)mGameMode, mBoard ? mBoard->mLevel : -1,
                                mBoard && mBoard->ChooseSeedsOnCurrentLevel() ? 1 : 0,
                                aBankCount, aFixedCount, aBankCount - aFixedCount,
                                aQuotas[0], aQuotas[1], aQuotas[2], aQuotas[3],
                                anEligibleCount, mNetManager->GetMySlot(), aLocalQuota,
                                (void*)mSeedChooserScreen, mSeedChooserScreen->mX,
                                mSeedChooserScreen->mY, mSeedChooserScreen->mVisible ? 1 : 0,
                                mSeedChooserScreen->mMouseVisible ? 1 : 0,
                                mWidgetManager ? (void*)mWidgetManager->mFocusWidget : nullptr);
                }
                else
                {
                        // A locally-created client widget is only provisional.  Quota zero
                        // is meaningful, so mCoopChooserActive (not the quota) is the
                        // generation marker.  OnChooserOpen enables it after strict checks.
                        mSeedChooserScreen->mCoopChooserActive = false;
                        mSeedChooserScreen->mCoopPicksPerPlayer = 0;
                        mSeedChooserScreen->EnableStartButton(false);
                        mNetManager->mChooserOpen = false;
                        mNetManager->mChooserPicksPerPlayer = 0;
                        mNetManager->mChooserBankCount = 0;
                        for (int aSlot = 0; aSlot < LawnNet::NET_MAX_PLAYERS; aSlot++)
                                mNetManager->mChooserPickQuota[aSlot] = 0;
                        LawnNet::NetTrace("ShowSeedChooserScreen role=client awaiting authoritative CHOOSER_OPEN mode=%d level=%d screen=%p",
                                (int)mGameMode, mBoard ? mBoard->mLevel : -1,
                                (void*)mSeedChooserScreen);
                }
        }
}

//0x44FE70
void LawnApp::KillSeedChooserScreen()
{
	if (mNetManager)
		mNetManager->PurgePendingInputs("KILL_SEED_CHOOSER");
	if (mSeedChooserScreen)
	{
		mWidgetManager->RemoveWidget(mSeedChooserScreen);
		SafeDeleteWidget(mSeedChooserScreen);
		mSeedChooserScreen = nullptr;
	}
}

void LawnApp::EndLevel()
{
	KillBoard();
	if (IsAdventureMode())
	{
		NewGame();
	}

	mFirstTimeGameSelector = true;

	MakeNewBoard();
	mBoard->InitLevel();
	mBoardResult = BoardResult::BOARDRESULT_NONE;
	mGameScene = GameScenes::SCENE_LEVEL_INTRO;
	ShowSeedChooserScreen();
	mBoard->mCutScene->StartLevelIntro();
}

//0x44FEB0
void LawnApp::DoBackToMain()
{
	// co-op host: bring everyone back to the menu
	if (mNetManager && mNetManager->IsHosting())
	{
		mNetManager->AdvanceInputPhase(LawnNet::NETINPUT_MENU, "BACK_TO_MENU");
		LawnNet::NetBuffer aMsg;
		mNetManager->BroadcastMessage(LawnNet::NETMSG_BACK_TO_MENU, aMsg);
	}

	mMusic->StopAllMusic();
	mSoundSystem->CancelPausedFoley();
	WriteCurrentUserConfig();
	KillNewOptionsDialog();
	KillBoard();
	ShowGameSelector();
}

//0x44FF00
void LawnApp::DoConfirmBackToMain()
{
	LawnDialog* aDialog = (LawnDialog*)DoDialog(
		Dialogs::DIALOG_CONFIRM_BACK_TO_MAIN, 
		true, 
		_S("Leave Game?"/*"[LEAVE_GAME]"*/),
		_S("Do you want to return\nto the main menu?\n\nYour game will be saved."/*"[LEAVE_GAME_HEADER]"*/), 
		_S(""), 
		Dialog::BUTTONS_YES_NO
	);

	aDialog->mLawnYesButton->mLabel = TodStringTranslate(_S("[LEAVE_BUTTON]")); 
	aDialog->mLawnNoButton->mLabel = TodStringTranslate(_S("[DIALOG_BUTTON_CANCEL]"));
	//aDialog->CalcSize(0, 0);
}

//0x4500D0
void LawnApp::DoNewOptions(bool theFromGameSelector)
{
	//FinishModelessDialogs();

	NewOptionsDialog* aDialog = new NewOptionsDialog(this, theFromGameSelector);
	CenterDialog(aDialog, IMAGE_OPTIONS_MENUBACK->mWidth, aDialog->GetPreferredHeight(IMAGE_OPTIONS_MENUBACK->mWidth));
	AddDialog(Dialogs::DIALOG_NEWOPTIONS, aDialog);
	mWidgetManager->SetFocus(aDialog);
}

//0x450180
AlmanacDialog* LawnApp::DoAlmanacDialog(SeedType theSeedType, ZombieType theZombieType)
{
	PerfTimer mTimer;
	mTimer.Start();

	//FinishModelessDialogs();

	AlmanacDialog* aDialog = new AlmanacDialog(this);
	AddDialog(Dialogs::DIALOG_ALMANAC, aDialog);
	mWidgetManager->SetFocus(aDialog);

	if (theSeedType != SeedType::SEED_NONE)
	{
		aDialog->ShowPlant(theSeedType);
	}
	else if (theZombieType != ZombieType::ZOMBIE_INVALID)
	{
		aDialog->ShowZombie(theZombieType);
	}

	int aDuration = mTimer.GetDuration();
	TodTrace("almanac load time: %d ms", aDuration);

	return aDialog;
}

//0x450220
void LawnApp::DoContinueDialog()
{
	ContinueDialog* aDialog = new ContinueDialog(this);
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	AddDialog(Dialogs::DIALOG_CONTINUE, aDialog);
}

//0x4502C0
void LawnApp::DoPauseDialog()
{
	mBoard->Pause(true);
	//FinishModelessDialogs();

	LawnDialog* aDialog = (LawnDialog*)DoDialog(
		Dialogs::DIALOG_PAUSED,
		true,
		_S("GAME PAUSED"/*"[GAME_PAUSED]"*/),
		_S("Click to resume game"), 
		_S("Resume Game"/*"[RESUME_GAME]"*/),
		Dialog::BUTTONS_FOOTER
	);

	aDialog->mReanimation->AddReanimation(72.0f, 42.0f, ReanimationType::REANIM_ZOMBIE_NEWSPAPER);
	aDialog->mSpaceAfterHeader = 155;
	aDialog->CalcSize(0, 10);
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
}

//0x4504B0
int LawnApp::LawnMessageBox(int theDialogId, const SexyChar* theHeaderName, const SexyChar* theLinesName, const SexyChar* theButton1Name, const SexyChar* theButton2Name, int theButtonMode)
{
	Widget* aOldFocus = mWidgetManager->mFocusWidget;

	LawnDialog* aDialog = (LawnDialog*)DoDialog(theDialogId, true, theHeaderName, theLinesName, theButton1Name, theButtonMode);
	if (aDialog->mYesButton)
	{
		aDialog->mYesButton->mLabel = TodStringTranslate(theButton1Name);
	}
	if (aDialog->mNoButton)
	{
		aDialog->mNoButton->mLabel = TodStringTranslate(theButton2Name);
	}
	//aDialog->CalcSize(0, 0);

	mWidgetManager->SetFocus(aDialog);
	int aResult = aDialog->WaitForResult(true);
	mWidgetManager->SetFocus(aOldFocus);

	return aResult;
}

//0x450770
Dialog* LawnApp::DoDialog(int theDialogId, bool isModal, const SexyString& theDialogHeader, const SexyString& theDialogLines, const SexyString& theDialogFooter, int theButtonMode)
{
	SexyString aHeader = TodStringTranslate(theDialogHeader);
	SexyString aLines = TodStringTranslate(theDialogLines);
	SexyString aFooter = TodStringTranslate(theDialogFooter);

	Dialog* aDialog = SexyAppBase::DoDialog(theDialogId, isModal, aHeader, aLines, aFooter, theButtonMode);
	if (mWidgetManager->mFocusWidget == nullptr)
	{
		mWidgetManager->mFocusWidget = aDialog;
	}

	return aDialog;
}

Dialog* LawnApp::DoDialogDelay(int theDialogId, bool isModal, const SexyString& theDialogHeader, const SexyString& theDialogLines, const SexyString& theDialogFooter, int theButtonMode)
{
	LawnDialog* aDialog = (LawnDialog*)SexyAppBase::DoDialog(theDialogId, isModal, theDialogHeader, theDialogLines, theDialogFooter, theButtonMode);
	aDialog->SetButtonDelay(30);
	return aDialog;
}

//0x450880
void LawnApp::DoUserDialog()
{
	KillDialog(Dialogs::DIALOG_USERDIALOG);

	UserDialog* aDialog = new UserDialog(this);
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	AddDialog(Dialogs::DIALOG_USERDIALOG, aDialog);
	mWidgetManager->SetFocus(aDialog);
}

//0x450930
void LawnApp::FinishUserDialog(bool isYes)
{
	UserDialog* aUserDialog = (UserDialog*)GetDialog(Dialogs::DIALOG_USERDIALOG);
	if (aUserDialog)
	{
		if (isYes)
		{
			PlayerInfo* aProfile = mProfileMgr->GetProfile(aUserDialog->GetSelName());
			if (aProfile)
			{
				mPlayerInfo = aProfile;
				mWidgetManager->MarkAllDirty();

				if (mGameSelector)
				{
					mGameSelector->SyncProfile(true);
				}
			}
			mPlayerLevelRef = mPlayerInfo ? mPlayerInfo->GetLevel() : -1;
		}

		KillDialog(Dialogs::DIALOG_USERDIALOG);
	}
}

//0x450A10
void LawnApp::DoCreateUserDialog()
{
	KillDialog(Dialogs::DIALOG_CREATEUSER);

	NewUserDialog* aDialog = new NewUserDialog(this, false);
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	AddDialog(Dialogs::DIALOG_CREATEUSER, aDialog);
}

//0x450AC0
void LawnApp::FinishCreateUserDialog(bool isYes)
{
	NewUserDialog* aNewUserDialog = (NewUserDialog*)GetDialog(Dialogs::DIALOG_CREATEUSER);
	if (aNewUserDialog == nullptr)
		return;

	SexyString aName = aNewUserDialog->GetName();

	if (isYes && aName.empty())
	{
		DoDialog(
			Dialogs::DIALOG_CREATEUSERERROR,
			true,
			_S("Enter Your Name"),
			_S("Please enter your name to create a new user profile for storing high score data and game progress"),
			_S("OK"),
			Dialog::BUTTONS_FOOTER
		);
	}
	else if (mPlayerInfo == nullptr && (!isYes || aName.empty()))
	{
		DoDialog(
			Dialogs::DIALOG_CREATEUSERERROR,
			true,
			_S("Enter Your Name"/*"[ENTER_YOUR_NAME]"*/),
			_S("Please enter your name to create a new user profile for storing high score data and game progress"/*"[ENTER_NEW_USER]"*/),
			_S("OK"/*"[DIALOG_BUTTON_OK]"*/),
			Dialog::BUTTONS_FOOTER
		);
	}
	else if (!isYes)
	{
		KillDialog(Dialogs::DIALOG_CREATEUSER);
	}
	else
	{
		PlayerInfo* aProfile = mProfileMgr->AddProfile(aName);
		if (aProfile == nullptr)
		{
			DoDialog(
				Dialogs::DIALOG_CREATEUSERERROR,
				true,
				_S("Name Conflict"/*"[NAME_CONFLICT]"*/),
				_S("The name you entered is already being used.  Please enter a unique player name"/*"[ENTER_UNIQUE_PLAYER_NAME]"*/),
				_S("OK"/*"[DIALOG_BUTTON_OK]"*/),
				Dialog::BUTTONS_FOOTER
			);
		}
		else
		{
			mProfileMgr->Save();
			mPlayerInfo = aProfile;

			/*string name = mPlayerInfo->mName;

			transform(name.begin(), name.end(), name.begin(), ::tolower);
			
			if (name == "patrice")
			{
				mPlayerInfo->mHardMode = true;
				LawnDialog* aDialog = (LawnDialog*)DoDialog(
					Dialogs::DIALOG_PAUSED,
					true,
					_S("WARNING!"),
					_S("You are now playing HARD MODE!"),
					_S("Continue"),
					Dialog::BUTTONS_FOOTER
				);

				aDialog->mReanimation->AddReanimation(72.0f, 67.0f, ReanimationType::REANIM_GARGANTUAR);
				aDialog->mReanimation->mReanim->SetImageOverride("anim_head1", IMAGE_REANIM_ZOMBIE_GARGANTUAR_HEAD_REDEYE);
				aDialog->mSpaceAfterHeader = 225;
				aDialog->CalcSize(0, 10);
				CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
			}*/

			KillDialog(Dialogs::DIALOG_USERDIALOG);
			KillDialog(Dialogs::DIALOG_CREATEUSER);
			mWidgetManager->MarkAllDirty();

			if (mGameSelector)
			{
				mGameSelector->SyncProfile(true);
			}
		}
	}
}

//0x450E20
void LawnApp::DoConfirmDeleteUserDialog(const SexyString& theName)
{
	KillDialog(Dialogs::DIALOG_CONFIRMDELETEUSER);
	DoDialog(
		Dialogs::DIALOG_CONFIRMDELETEUSER, 
		true, 
		_S("Are You Sure"/*"[ARE_YOU_SURE]"*/), 
		// StrFormat(TodStringTranslate(_S("[DELETE_USER_WARNING]")).c_str(), StringToSexyStringFast(theName))
		StrFormat(_S("This will permanently remove '%s' from the player roster!"), theName.c_str()),
		_S(""), 
		Dialog::BUTTONS_YES_NO
	);
}

//0x450F40
void LawnApp::FinishConfirmDeleteUserDialog(bool isYes)
{
	KillDialog(Dialogs::DIALOG_CONFIRMDELETEUSER);
	UserDialog* aUserDialog = (UserDialog*)GetDialog(Dialogs::DIALOG_USERDIALOG);
	if (aUserDialog == nullptr)
		return;

	mWidgetManager->SetFocus(aUserDialog);

	if (!isYes)
		return;

	SexyString aCurName = mPlayerInfo ? mPlayerInfo->mName : _S("");
	SexyString aName = aUserDialog->GetSelName();
	if (aName == aCurName)
	{
		mPlayerInfo = nullptr;
	}

	mProfileMgr->DeleteProfile(aName);
	aUserDialog->FinishDeleteUser();
	if (mPlayerInfo == nullptr)
	{
		mPlayerInfo = mProfileMgr->GetProfile(aUserDialog->GetSelName());
		if (mPlayerInfo == nullptr)
		{
			mPlayerInfo = mProfileMgr->GetAnyProfile();
		}
		mPlayerLevelRef = mPlayerInfo ? mPlayerInfo->GetLevel() : -1;
	}

	mProfileMgr->Save();
	if (mPlayerInfo == nullptr)
	{
		DoCreateUserDialog();
	}

	mWidgetManager->MarkAllDirty();
	if (mGameSelector != nullptr)
	{
		mGameSelector->SyncProfile(true);
	}
}

//0x451180
void LawnApp::DoRenameUserDialog(const SexyString& theName)
{
	KillDialog(Dialogs::DIALOG_RENAMEUSER);

	NewUserDialog* aDialog = new NewUserDialog(this, true);
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	aDialog->SetName(theName);
	AddDialog(Dialogs::DIALOG_RENAMEUSER, aDialog);
}

//0x451260
void LawnApp::FinishRenameUserDialog(bool isYes)
{
	UserDialog* aUserDialog = (UserDialog*)GetDialog(Dialogs::DIALOG_USERDIALOG);
	if (!isYes)
	{
		KillDialog(Dialogs::DIALOG_RENAMEUSER);
		mWidgetManager->SetFocus(aUserDialog);
		return;
	}

	NewUserDialog* aNewUserDialog = (NewUserDialog*)GetDialog(Dialogs::DIALOG_RENAMEUSER);
	if (aUserDialog == nullptr || aNewUserDialog == nullptr)
		return;

	SexyString anOldName = aUserDialog->GetSelName();
	SexyString aNewName = aNewUserDialog->GetName();
	if (aNewName.empty())
		return;
	
	bool isCurrentUser = mProfileMgr->GetProfile(anOldName) == mPlayerInfo;
	if (!mProfileMgr->RenameProfile(anOldName, aNewName))
	{
		DoDialog(
			Dialogs::DIALOG_RENAMEUSERERROR,
			true,
			_S("Name Conflict"/*"[NAME_CONFLICT]"*/),
			_S("The name you entered is already being used.  Please enter a unique player name"/*"[ENTER_UNIQUE_PLAYER_NAME]"*/),
			_S("OK"/*"[DIALOG_BUTTON_OK]"*/),
			Dialog::BUTTONS_FOOTER
		);
		return;
	}

	mProfileMgr->Save();
	if (isCurrentUser)
	{
		mPlayerInfo = mProfileMgr->GetProfile(aNewName);
	}
	mPlayerLevelRef = mPlayerInfo ? mPlayerInfo->GetLevel() : -1;

	aUserDialog->FinishRenameUser(aNewName);
	mWidgetManager->MarkAllDirty();
	KillDialog(Dialogs::DIALOG_RENAMEUSER);
	mWidgetManager->SetFocus(aUserDialog);
}

//0x451490
void LawnApp::FinishNameError(int theId)
{
	KillDialog(theId);

	NewUserDialog* aNewUserDialog = (NewUserDialog*)GetDialog(theId == Dialogs::DIALOG_CREATEUSERERROR ? Dialogs::DIALOG_CREATEUSER : Dialogs::DIALOG_RENAMEUSER);
	if (aNewUserDialog)
	{
		mWidgetManager->SetFocus(aNewUserDialog->mNameEditWidget);
	}
}

//0x4514D0
void LawnApp::FinishRestartConfirmDialog()
{
	mSawYeti = mBoard->mKilledYeti;
	int theLevel = -1;

	KillDialog(Dialogs::DIALOG_CONTINUE);
	KillDialog(Dialogs::DIALOG_RESTARTCONFIRM);
	if (mBoard && mGameMode == GameMode::GAMEMODE_ADVENTURE)
		theLevel = mBoard->mLevel;
	KillBoard();

	if (theLevel > 0)
		PreNewGame(mGameMode, false, theLevel);
	else
		PreNewGame(mGameMode, false);
}

void LawnApp::DoCheatDialog()
{
	KillDialog(Dialogs::DIALOG_CHEAT);

	CheatDialog* aDialog = new CheatDialog(this);
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	AddDialog(Dialogs::DIALOG_CHEAT, aDialog);
}

void LawnApp::FinishCheatDialog(bool isYes)
{
	CheatDialog* aCheatDialog = (CheatDialog*)GetDialog(Dialogs::DIALOG_CHEAT);
	if (aCheatDialog == nullptr)
		return;

	if (isYes && !aCheatDialog->ApplyCheat())
		return;

	KillDialog(Dialogs::DIALOG_CHEAT);
	if (isYes)
	{
		mMusic->StopAllMusic();
		mBoardResult = BoardResult::BOARDRESULT_CHEAT;
		PreNewGame(mGameMode, false);
	}
}

void LawnApp::FinishTimesUpDialog()
{
	KillDialog(Dialogs::DIALOG_TIMESUP);
}

void LawnApp::DoConfirmSellDialog(const SexyString& theMessage)
{
	Dialog* aConfirmDialog = DoDialog(Dialogs::DIALOG_ZEN_SELL, true, _S("[ZEN_SELL_HEADER]"), theMessage, _S(""), Dialog::BUTTONS_YES_NO);
	aConfirmDialog->mYesButton->mLabel = TodStringTranslate(_S("[DIALOG_BUTTON_YES]"));
	aConfirmDialog->mNoButton->mLabel = TodStringTranslate(_S("[DIALOG_BUTTON_NO]"));
}

void LawnApp::DoConfirmPurchaseDialog(const SexyString& theMessage)
{
	LawnDialog* aComfirmDialog = (LawnDialog*)DoDialog(Dialogs::DIALOG_STORE_PURCHASE, true, _S("买下这个物品？"), theMessage, _S(""), Dialog::BUTTONS_YES_NO);
	aComfirmDialog->mLawnYesButton->mLabel = TodStringTranslate(_S("[DIALOG_BUTTON_YES]"));
	aComfirmDialog->mLawnNoButton->mLabel = TodStringTranslate(_S("[DIALOG_BUTTON_NO]"));
}

//0x451580
Dialog* LawnApp::NewDialog(int theDialogId, bool isModal, const SexyString& theDialogHeader, const SexyString& theDialogLines, const SexyString& theDialogFooter, int theButtonMode)
{
	LawnDialog* aDialog = new LawnDialog(
		this, 
		theDialogId, 
		isModal, 
		theDialogHeader, 
		theDialogLines, 
		theDialogFooter, 
		theButtonMode
	);

	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	return aDialog;
}

//0x451630
bool LawnApp::KillNewOptionsDialog()
{
	NewOptionsDialog* aNewOptionsDialog = (NewOptionsDialog*)GetDialog(Dialogs::DIALOG_NEWOPTIONS);
	if (aNewOptionsDialog == nullptr)
		return false;

	bool wantWindowed = !aNewOptionsDialog->mFullscreenCheckbox->IsChecked();
	//bool want3D = aNewOptionsDialog->mHardwareAccelerationCheckbox->IsChecked();
	mEnableVsync = aNewOptionsDialog->mHardwareAccelerationCheckbox->IsChecked();
	RegistryWriteBoolean(_S("EnableVsync"), mEnableVsync);
	SDL_SetRenderVSync(mSDLRenderer, mEnableVsync);
	SwitchScreenMode(wantWindowed, true, false);

	KillDialog(Dialogs::DIALOG_NEWOPTIONS);
	ClearUpdateBacklog();
	return true;
}

//0x4516C0
bool LawnApp::KillAlmanacDialog()
{
	if (GetDialog(Dialogs::DIALOG_ALMANAC))
	{
		KillDialog(Dialogs::DIALOG_ALMANAC);
		ClearUpdateBacklog(false);
		return true;
	}

	return false;
}

//0x4516F0
bool LawnApp::NeedPauseGame()
{
	if (mDialogList.size() == 0)
		return false;

	if (mDialogList.size() == 1 && mDialogList.front()->mId != Dialogs::DIALOG_NEW_GAME)
	{
		int anId = mDialogList.front()->mId;
		if (anId == Dialogs::DIALOG_CHOOSER_WARNING || anId == Dialogs::DIALOG_PURCHASE_PACKET_SLOT || anId == Dialogs::DIALOG_IMITATER)
		{
			return false;
		}
	}

	return (mBoard == nullptr || mGameMode != GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN) && (mBoard == nullptr || mGameMode != GameMode::GAMEMODE_TREE_OF_WISDOM);
}

//0x451780
void LawnApp::ModalOpen()
{
	if (mBoard && NeedPauseGame())
	{
		mBoard->Pause(true);
	}
}

void LawnApp::ModalClose()
{
}

//0x451800
bool LawnApp::KillDialog(int theDialogId)
{
	if (theDialogId == Dialogs::DIALOG_STORE && GetDialog(theDialogId) && NetIsHost())
		mNetManager->BroadcastSceneCmd(LawnNet::SCENECMD_CLOSE_STORE, 0);
	if (SexyAppBase::KillDialog(theDialogId))
	{
		if (mDialogMap.size() == 0)
		{
			if (mBoard)
			{
				mWidgetManager->SetFocus(mBoard);
			}
			else if (mGameSelector)
			{
				mWidgetManager->SetFocus(mGameSelector);
			}
		}

		if (mBoard && !NeedPauseGame())
		{
			mBoard->Pause(false);
		}

		return true;
	}

	return false;
}

//0x451870
void LawnApp::ShowResourceError(bool doExit)
{
	SexyAppBase::ShowResourceError(doExit);
}

void BetaSubmitFunc()
{
	if (gLawnApp)
	{
		gLawnApp->BetaSubmit(false);
	}
}

SDL_Cursor* CreateCursorFromResource(HINSTANCE hInstance, int resourceID, int hotX, int hotY)
{
	HRSRC hRes = FindResource(hInstance, MAKEINTRESOURCE(resourceID), RT_RCDATA);
	HGLOBAL hResData = LoadResource(hInstance, hRes);
	void* pData = LockResource(hResData);
	DWORD size = SizeofResource(hInstance, hRes);
	SDL_IOStream* io = SDL_IOFromConstMem(pData, size);
	SDL_Surface* surface = SDL_LoadBMP_IO(io, true);
	SDL_Cursor* cursor = SDL_CreateColorCursor(surface, hotX, hotY);
	SDL_DestroySurface(surface);
	return cursor;
}

//HotSpot: 11 4
//Size: 32 32
unsigned char mFingerCursorData[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xe7, 0xff, 0xff, 0xff, 0xc3, 0xff, 0xff, 0xff, 0xc3, 0xff, 0xff, 0xff, 0xc3,
	0xff, 0xff, 0xff, 0xc3, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xc0, 0x1f, 0xff, 0xff,
	0xc0, 0x07, 0xff, 0xff, 0xc0, 0x03, 0xff, 0xfc, 0x40, 0x01, 0xff, 0xfc, 0x00, 0x01, 0xff,
	0xfc, 0x00, 0x01, 0xff, 0xfc, 0x00, 0x01, 0xff, 0xff, 0x00, 0x01, 0xff, 0xff, 0x00, 0x01,
	0xff, 0xff, 0x80, 0x01, 0xff, 0xff, 0x80, 0x03, 0xff, 0xff, 0xc0, 0x03, 0xff, 0xff, 0xc0,
	0x03, 0xff, 0xff, 0xe0, 0x07, 0xff, 0xff, 0xe0, 0x07, 0xff, 0xff, 0xe0, 0x07, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18,
	0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00,
	0x18, 0x00, 0x00, 0x00, 0x1b, 0x00, 0x00, 0x00, 0x1b, 0x60, 0x00, 0x00, 0x1b, 0x68, 0x00,
	0x00, 0x1b, 0x6c, 0x00, 0x01, 0x9f, 0xec, 0x00, 0x01, 0xdf, 0xfc, 0x00, 0x00, 0xdf, 0xfc,
	0x00, 0x00, 0x5f, 0xfc, 0x00, 0x00, 0x7f, 0xfc, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x00, 0x3f,
	0xf8, 0x00, 0x00, 0x1f, 0xf8, 0x00, 0x00, 0x1f, 0xf8, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00,
	0x0f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00
};

//HotSpot: 15 10
//Size: 32 32
unsigned char mDraggingCursorData[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xfe, 0x7f, 0xff, 0xff, 0xfc, 0x0f, 0xff, 0xff, 0xf0, 0x07, 0xff, 0xff, 0xe0,
	0x01, 0xff, 0xff, 0xe0, 0x00, 0xff, 0xff, 0xe0, 0x00, 0xff, 0xff, 0xe0, 0x00, 0xff, 0xff,
	0xe0, 0x00, 0xff, 0xfe, 0x60, 0x00, 0xff, 0xfc, 0x20, 0x00, 0xff, 0xfc, 0x00, 0x00, 0xff,
	0xfe, 0x00, 0x00, 0xff, 0xfe, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x80, 0x00,
	0xff, 0xff, 0x80, 0x01, 0xff, 0xff, 0xc0, 0x01, 0xff, 0xff, 0xe0, 0x01, 0xff, 0xff, 0xf0,
	0x03, 0xff, 0xff, 0xf8, 0x03, 0xff, 0xff, 0xf8, 0x03, 0xff, 0xff, 0xf8, 0x03, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
	0x80, 0x00, 0x00, 0x01, 0xb0, 0x00, 0x00, 0x0d, 0xb0, 0x00, 0x00, 0x0d, 0xb6, 0x00, 0x00,
	0x0d, 0xb6, 0x00, 0x00, 0x0d, 0xb6, 0x00, 0x00, 0x0d, 0xb6, 0x00, 0x00, 0x0d, 0xb6, 0x00,
	0x01, 0x8d, 0xb6, 0x00, 0x01, 0xcf, 0xfe, 0x00, 0x00, 0xef, 0xfe, 0x00, 0x00, 0xff, 0xfe,
	0x00, 0x00, 0x7f, 0xfe, 0x00, 0x00, 0x3f, 0xfe, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x00, 0x1f,
	0xfc, 0x00, 0x00, 0x0f, 0xfc, 0x00, 0x00, 0x07, 0xf8, 0x00, 0x00, 0x03, 0xf8, 0x00, 0x00,
	0x03, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00
};

SDL_Cursor* CreateCursorFromRaw(const unsigned char* src, int w, int h, int hotX, int hotY)
{
	size_t maskSize = (w * h) / 8;
	SDL_Surface* surface = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_ARGB8888);

	Uint32* pixels = (Uint32*)surface->pixels;
	int pitch = surface->pitch / sizeof(Uint32);  
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			pixels[y * pitch + x] = 0x00000000;  
		}
	}
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			int byteIndex = y * (w / 8) + (x / 8);
			int bitIndex = 7 - (x % 8); 

			Uint8 andBit = (src[byteIndex] >> bitIndex) & 1;
			Uint8 xorBit = (src[byteIndex + maskSize] >> bitIndex) & 1;

			if (andBit == 0) {  
				if (xorBit == 0) {
					pixels[y * pitch + x] = 0xFF000000;
				}
				else {
					pixels[y * pitch + x] = 0xFFFFFFFF;  
				}
			}
		}
	}
	SDL_UnlockSurface(surface);
	SDL_Cursor* cursor = SDL_CreateColorCursor(surface, hotX, hotY);
	SDL_DestroySurface(surface);
	return cursor;
}

//0x451880
void LawnApp::Init()
{
	DoParseCmdLine();
	if (mCoopSmokeArgSeen && !mCoopSmokeManifestValid)
	{
		std::string aMessage = "PvZ co-op smoke manifest rejected: " + mCoopSmokeManifestError + "\n";
		fputs(aMessage.c_str(), stderr);
		OutputDebugStringA(aMessage.c_str());
		DoExit(2);
	}
	if (CoopSmokeActive())
	{
		LawnNet::NetConfig aSmokeConfig;
		if (!LawnNet::NetLoadConfig(aSmokeConfig) ||
			strcmp(aSmokeConfig.mNick, mCoopSmokeManifest.mNickname.c_str()) != 0 ||
			!aSmokeConfig.mEnableCoop)
		{
			CoopSmokeWriteResult(false,
				"isolated coop.ini is unreadable, disabled, or its nickname does not match the manifest");
			DoExit(2);
		}
	}
	if (!mTodCheatKeys)
	{
		mOnlyAllowOneCopyToRun = false;
	}

	//if (!gSexyCache->Connected() &&
	//	gLawnApp->mTodCheatKeys &&
	//	MessageBox(gLawnApp->mHWnd, _S("Start SexyCache now?"), _S("SexyCache"), MB_YESNO) == IDYES &&
	//	WinExec("SexyCache.exe", SW_MINIMIZE) >= 32)
	//{
	//	gSexyCache = SexyCache();
	//}
	//if (gSexyCache->Connected() && !gLawnApp->mTodCheatKeys)
	//{
	//	gSexyCache->Disconnect();
	//}

	mSessionID = _time32(nullptr);
	mPlayTimeActiveSession = 0;
	mPlayTimeInactiveSession = 0;
	mBoardResult = BoardResult::BOARDRESULT_NONE;
	mSawYeti = false;

	NetInit();

	SexyApp::Init();

#ifdef _DEBUG
	TodAssertInitForApp();
	gBetaSubmitFunc = BetaSubmitFunc;
	TodLog("session id: %u", mSessionID);
#endif

	
	WIN32_FILE_ATTRIBUTE_DATA fileInfo;
	bool IsXMLPackLoaded = mResourceManager->ParseResourcesFile(_S("properties\\resources.xml"));

	if (GetFileAttributesExA("dependency\\properties\\resources.xml", GetFileExInfoStandard, &fileInfo) != 0 || IsFileInPakFile("dependency\\properties\\resources.xml"))
	{
		IsXMLPackLoaded &= mResourceManager->ParseResourcesFile(_S("dependency\\properties\\resources.xml"));
	}
	else
	{
		MsgBox("dependency files missing! Please get the dependency.pak from the Stable Decompile Github Repo. Contact the developers.", "Error");
	}

	if (GetFileAttributesExA("extension\\properties\\resources.xml", GetFileExInfoStandard, &fileInfo) != 0 || IsFileInPakFile("extension\\properties\\resources.xml"))
		IsXMLPackLoaded &= mResourceManager->ParseResourcesFile(_S("extension\\properties\\resources.xml"));

#ifdef _ALLOW_RESOURCE_PACKS
	if (GetFileAttributesExA("resourcepack\\properties\\resources.xml", GetFileExInfoStandard, &fileInfo) != 0 || IsFileInPakFile("resourcepack\\properties\\resources.xml"))
		IsXMLPackLoaded &= mResourceManager->ParseResourcesFile(_S("resourcepack\\properties\\resources.xml"));
#endif

	if (!IsXMLPackLoaded)
	{
		ShowResourceError(true);
		return;
	}

	if (!TodLoadResources("Init"))
	{
		return;
	}

	PerfTimer mTimer;
	mTimer.Start();

	mProfileMgr->Load();

	if (CoopSmokeActive())
	{
		// Never consult the machine-wide CurUser value in automation.  A fresh
		// profile lives solely below the manifest's isolated appdata directory.
		SexyString aSmokeName = StringToSexyString(mCoopSmokeManifest.mNickname);
		mPlayerInfo = mProfileMgr->GetProfile(aSmokeName);
		if (!mPlayerInfo)
			mPlayerInfo = mProfileMgr->AddProfile(aSmokeName);
		if (!mPlayerInfo)
		{
			CoopSmokeWriteResult(false, "failed to create isolated smoke profile");
			DoExit(2);
		}
		mPlayerInfo->SetLevel(mCoopSmokeManifest.mLevel);
		mPlayerLevelRef = mPlayerInfo->GetLevel();
	}
	else
	{
		std::string theUser;
		SexyString aCurUser;
		if (mPlayerInfo == nullptr && RegistryReadString("CurUser", &theUser))
		{
			aCurUser = StringToSexyString(theUser);
			mPlayerInfo = mProfileMgr->GetProfile(aCurUser);
			mPlayerLevelRef = mPlayerInfo ? mPlayerInfo->GetLevel() : -1;
		}
		if (mPlayerInfo == nullptr)
			mPlayerInfo = mProfileMgr->GetAnyProfile();
	}

	mMaxExecutions = GetInteger("MaxExecutions", 0);
	mMaxPlays = GetInteger("MaxPlays", 0);
	mMaxTime = GetInteger("MaxTime", 60);

	mTitleScreen = new TitleScreen(this);
	mTitleScreen->Resize(0, 0, mWidth, mHeight);
	mWidgetManager->AddWidget(mTitleScreen);
	mWidgetManager->SetFocus(mTitleScreen);
	
#ifdef _DEBUG
	int aDuration = mTimer.GetDuration();
	TodTrace("loading: 'profiles' %d ms", aDuration);
#endif
	mTimer.Start();

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
	TTF_Init();
	HINSTANCE hInstance = GetModuleHandle(NULL);
	mSDLPointerCursor = CreateCursorFromResource(hInstance, IDC_CURSOR1, 0, 0);
	mSDLHandCursor = CreateCursorFromRaw(mFingerCursorData, 32, 32, 11, 4);
	mSDLDraggingCursor = CreateCursorFromRaw(mDraggingCursorData, 32, 32, 15, 10);
	mSDLTextCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_TEXT);
	mSDLWaitCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAIT);
	mSDLNoCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NOT_ALLOWED);

	mMusic = new Music();
	mSoundSystem = new TodFoley();
	mEffectSystem = new EffectSystem();
	mEffectSystem->EffectSystemInitialize();

	mKonamiCheck = new TypingCheck();
	mKonamiCheck->AddKeyCode(KeyCode::KEYCODE_UP);
	mKonamiCheck->AddKeyCode(KeyCode::KEYCODE_UP);
	mKonamiCheck->AddKeyCode(KeyCode::KEYCODE_DOWN);
	mKonamiCheck->AddKeyCode(KeyCode::KEYCODE_DOWN);
	mKonamiCheck->AddKeyCode(KeyCode::KEYCODE_LEFT);
	mKonamiCheck->AddKeyCode(KeyCode::KEYCODE_RIGHT);
	mKonamiCheck->AddKeyCode(KeyCode::KEYCODE_LEFT);
	mKonamiCheck->AddKeyCode(KeyCode::KEYCODE_RIGHT);
	mKonamiCheck->AddChar('b');
	mKonamiCheck->AddChar('a');
	mMustacheCheck = new TypingCheck("mustache");
	mMoustacheCheck = new TypingCheck("moustache");
	mSuperMowerCheck = new TypingCheck("trickedout");
	mSuperMowerCheck2 = new TypingCheck("tricked out");
	mFutureCheck = new TypingCheck("future");
	mPinataCheck = new TypingCheck("pinata");
	mDanceCheck = new TypingCheck("dance");
	mDaisyCheck = new TypingCheck("daisies");
	mSukhbirCheck = new TypingCheck("sukhbir");

#ifdef _DEBUG
	aDuration = mTimer.GetDuration();
	TodTrace("loading: 'system' %d ms", aDuration);
#endif
	mTimer.Start();

	ReanimatorLoadDefinitions(gLawnReanimationArray, ReanimationType::NUM_REANIMS);
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_LOADBAR_SPROUT, true);
	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_LOADBAR_ZOMBIEHEAD, true);

#ifdef _DEBUG
	aDuration = mTimer.GetDuration();
	TodTrace("loading: 'loaderbar' %d ms", aDuration);
#endif
	mTimer.Start();

	if (!(CoopSmokeActive() && getenv("PVZ_COOP_SMOKE_BACKGROUND") &&
		strcmp(getenv("PVZ_COOP_SMOKE_BACKGROUND"), "1") == 0))
		SDL_RaiseWindow(LawnApp::mSDLWindow);
	if (!IsScreenSaver() && !IsParticleEditor() && !CoopSmokeActive())
		PlayVideo(StrFormat("%svideos/intro.mp4", SDL_GetBasePath()).c_str(), true);
}

//0x4522A0
bool LawnApp::ChangeDirHook(const char* theIntendedPath)
{
	return false;
}

//0x4522B0
void LawnApp::Start()
{
	if (mLoadingFailed)
		return;

	SexyAppBase::Start();
}

int LawnApp::AudioCallback(const void* inputBuffer, void* outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void* userData) {
	if (!inputBuffer) {
		*(float*)userData = 0.0f;
		return paContinue;
	}

	const float* input = (const float*)inputBuffer;
	float rms = 0.0f;
	for (unsigned long i = 0; i < framesPerBuffer; ++i) {
		rms += input[i] * input[i];  // 
	}

	rms = sqrt(rms / framesPerBuffer);

	float* volume = (float*)userData;
	*volume = rms;

	return paContinue;
}
//0x4522C0
bool LawnApp::DebugKeyDown(int theKey)
{
	return SexyAppBase::DebugKeyDown(theKey);
}

//0x4522E0
void LawnApp::HandleCmdLineParam(const std::string& theParamName, const std::string& theParamValue)
{
	if (theParamName == "-tod")
	{
#ifdef _DEBUG
		mTodCheatKeys = true;
		mDebugKeysEnabled = true;
#endif
	}
	else if (theParamName == "-coop-smoke-manifest")
	{
		if (mCoopSmokeArgSeen)
		{
			mCoopSmokeManifest.Reset();
			mCoopSmokeManifestValid = false;
			mCoopSmokeManifestError = "-coop-smoke-manifest may be specified only once";
			return;
		}

		mCoopSmokeArgSeen = true;
		mCoopSmokeManifestValid = LawnNet::NetLoadSmokeManifest(
			theParamValue.c_str(), mCoopSmokeManifest, mCoopSmokeManifestError);
		if (mCoopSmokeManifestValid)
		{
			// These process-local paths are installed before NetInit and are
			// re-applied from InitHook after SexyAppBase selects its default
			// CommonAppData directory.
			LawnNet::NetSetRuntimePaths(mCoopSmokeManifest.mConfigPath.c_str(),
				mCoopSmokeManifest.mLogDir.c_str());
			SetAppDataFolder(mCoopSmokeManifest.mAppDataDir);
			mCoopSmokeStartedTick = GetTickCount();
		}
	}
	else
	{
		SexyApp::HandleCmdLineParam(theParamName, theParamValue);
	}
}

//0x452310
bool LawnApp::UpdatePlayerProfileForFinishingLevel()
{
	bool aUnlockedNewChallenge = false;

	if (IsAdventureMode())
	{
		if (abs(mBoard->mLevel) == FINAL_LEVEL)
		{
			if (mPlayerInfo->mFinishedAdventure == 1)
			{
				if (mBoard->mLevel >= 0)
				{
					mPlayerInfo->SetLevel(51);
					mPlayerInfo->mFinishedAdventure++;
				}
				else
				{
					mPlayerInfo->SetLevel(-49);
				}
			}
			else
			{
				if (mBoard->mLevel >= 0)
				{
					mPlayerInfo->SetLevel(1);  // 存档回到第 1-1 关
					mPlayerInfo->mFinishedAdventure++;
				}
				else
				{
					mPlayerInfo->SetLevel(-49);
				}
			}

			  // 完成冒险模式周目数增加 1 次
			if (mPlayerInfo->mFinishedAdventure == 1)
			{
				mPlayerInfo->mNeedsMessageOnGameSelector = 1;
			}
			ReportAchievement::GiveAchievement(this, AchievementId::HomeSecurity, false); // @Patoke: add achievement
		}
		else
		{
			if (!mBoard->mIsReplay)
				mPlayerInfo->SetLevel(mBoard->mLevel + 1);  // 存档进入下一关
		}

		if (!HasFinishedAdventure() && abs(mPlayerInfo->mLevel) == 34)
		{
			mPlayerInfo->mNeedsMagicTacoReward = 1;
		}

		// @Patoke: implemented
		if (mBoard->StageIsDayWithPool() && !mBoard->mPeaShooterUsed) 
		{
			ReportAchievement::GiveAchievement(this, AchievementId::DontPea, false);
		}
		if (mBoard->StageHasRoof() && !mBoard->HasConveyorBeltSeedBank() && !mBoard->mCatapultPlantsUsed) 
		{
			ReportAchievement::GiveAchievement(this, AchievementId::Grounded, false);
		}
		if (mBoard->StageIsNight() && !mBoard->mMushroomsUsed && mPlayerInfo->mLevel != 35 && mPlayerInfo->mLevel != 40)
		{
			ReportAchievement::GiveAchievement(this, AchievementId::NoFungusAmongUs, false);
		}
		if (!mBoard->StageIsNight() && mBoard->mMushroomAndCoffeeBeansOnly) 
		{
			ReportAchievement::GiveAchievement(this, AchievementId::GoodMorning, false);
		}
#ifdef _HAS_UNUSED_ACHIEVEMENTS
		if (mBoard->mSpawnedDiggerZombie && !mBoard->mDiggerHasReachLeftSide) 
		{
			ReportAchievement::GiveAchievement(this, AchievementId::FaceToFace, false);
		}
		if (!mBoard->mHadPlantedNuts && mPlayerInfo->mLevel > 3)
		{
			ReportAchievement::GiveAchievement(this, AchievementId::MayNotContainNuts, true);
		}
#endif
	}
	else if (IsSurvivalMode())
	{
		if (mBoard->IsFinalSurvivalStage())
		{
			aUnlockedNewChallenge = !HasBeatenChallenge(mGameMode);
			mBoard->SurvivalSaveScore();

			if (aUnlockedNewChallenge && HasFinishedAdventure())
			{
				int aNumTrophies = GetNumTrophies(ChallengePage::CHALLENGE_PAGE_SURVIVAL);
				if (aNumTrophies != 8 && aNumTrophies != 9)
				{
					mPlayerInfo->mHasNewSurvival = true;
				}
			}
		}
	}
	else if (IsPuzzleMode())
	{
		aUnlockedNewChallenge = !HasBeatenChallenge(mGameMode);
		mPlayerInfo->mChallengeRecords[GetCurrentChallengeIndex()]++;

		if (!HasFinishedAdventure() && (mGameMode == GameMode::GAMEMODE_SCARY_POTTER_3 || mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_3))
		{
			aUnlockedNewChallenge = false;
		}

		if (aUnlockedNewChallenge)
		{
			if (IsScaryPotterLevel())
			{
				mPlayerInfo->mHasNewScaryPotter = 1;
			}
			else
			{
				mPlayerInfo->mHasNewIZombie = 1;
			}
		}
	}
	else
	{
		aUnlockedNewChallenge = !HasBeatenChallenge(mGameMode);
		mPlayerInfo->mChallengeRecords[GetCurrentChallengeIndex()]++;

		if (aUnlockedNewChallenge && HasFinishedAdventure())
		{
			int aNumTrophies = GetNumTrophies(ChallengePage::CHALLENGE_PAGE_CHALLENGE);
			if (aNumTrophies <= 17)
			{
				mPlayerInfo->mHasNewMiniGame = 1;
			}
		}

		// @Patoke: implemented
		int aNumTrophies = GetNumTrophies(ChallengePage::CHALLENGE_PAGE_CHALLENGE);
		if (aNumTrophies == 20)
			ReportAchievement::GiveAchievement(this, AchievementId::BeyondTheGrave, false);
	}

	WriteCurrentUserConfig();

	return aUnlockedNewChallenge;
}


//0x4524F0
//0x4524F0
// GOTY @Patoke: 0x4558E0
void LawnApp::CheckForGameEnd()
{
	// co-op client: the host drives all end-of-game transitions
	if (NetIsClient())
		return;

	if (mBoard == nullptr || !mBoard->mLevelComplete)
		return;

	bool aUnlockedNewChallenge = UpdatePlayerProfileForFinishingLevel();

	if (IsAdventureMode())
	{
		int aLevel = mBoard->mLevel;
		bool isReplaying = mBoard->mIsReplay;
		KillBoard();

		if (!isReplaying)
		{
			if (IsFirstTimeAdventureMode() && aLevel < 50)
			{
				ShowAwardScreen(AwardType::AWARD_FORLEVEL, true);
			}
			else if (aLevel == FINAL_LEVEL && mPlayerInfo->mFinishedAdventure == 1)
			{
				if (mPlayerInfo->mFinishedAdventure > 1)
				{
					ShowAwardScreen(AwardType::AWARD_FORLEVEL, true);
				}
				else
				{
					ShowAwardScreen(AwardType::AWARD_CREDITS_ZOMBIENOTE, true);
				}
			}
			else if (aLevel == 9 || aLevel == 19 || aLevel == 29 || aLevel == 39 || aLevel == 49)
			{
				ShowAwardScreen(AwardType::AWARD_FORLEVEL, true);
			}
			else
			{
				PreNewGame(mGameMode, false);
			}
		}
		else
		{
			DoBackToMain();
		}
	}
	else if (IsLastStandEndless(mGameMode))
	{
		mBoard->mNukeCounter = 0;
		mBoard->mChallenge->mSurvivalStage++;
		KillGameSelector();
		mBoard->InitSurvivalStage();
	}
	else if (IsSurvivalMode())
	{
		if (mBoard->IsFinalSurvivalStage())
		{
			KillBoard();

			if (aUnlockedNewChallenge && HasFinishedAdventure())
			{
				ShowAwardScreen(AwardType::AWARD_FORLEVEL, true);
			}
			else
			{
				ShowChallengeScreen(ChallengePage::CHALLENGE_PAGE_SURVIVAL);
			}
		}
		else
		{
			mBoard->mNukeCounter = 0;
			mBoard->mChallenge->mSurvivalStage++;
			// Endless S/ST re-open the chooser after every ten-wave stage.  Tell
			// clients to enter the survival intro before the ordered INPUT_EPOCH
			// and CHOOSER_OPEN messages emitted by InitSurvivalStage().  A queued
			// board event is too late: chooser messages are consumed immediately.
			if (NetIsHost() && IsCoopEndlessRepick(mGameMode) && mNetManager)
			{
				LawnNet::NetBuffer aRepick;
				const bool aRepickWritten = LawnNet::NetEncodeEndlessRepickEvent(
					(uint32_t)mBoard->mChallenge->mSurvivalStage, aRepick);
				TOD_ASSERT(aRepickWritten);
				mNetManager->BroadcastMessage(LawnNet::NETMSG_BOARD_EVENT, aRepick);
				LawnNet::NetTrace("ENDLESS_REPICK TX stage=%d mode=%d",
					mBoard->mChallenge->mSurvivalStage, (int)mGameMode);
			}
			KillGameSelector();
			mBoard->InitSurvivalStage();
		}
	}
	else if (IsPuzzleMode())
	{
		KillBoard();

		if (aUnlockedNewChallenge)
		{
			ShowAwardScreen(AwardType::AWARD_FORLEVEL, true);
		}
		else
		{
			ShowChallengeScreen(ChallengePage::CHALLENGE_PAGE_PUZZLE);
		}
	}
	else
	{
		KillBoard();
		ShowChallengeScreen(ChallengePage::CHALLENGE_PAGE_CHALLENGE);
	}
}


void LawnApp::UpdatePlayTimeStats()
{
	static int aLastTime = -1;

	int aTickCount = GetTickCount();
	int aSession = (aTickCount - aLastTime) / 1000;

	if (mPlayerInfo && !mPlayerInfo->mHasUsedCheatKeys && !mDebugKeysEnabled && mTodCheatKeys)
	{
		mPlayerInfo->mHasUsedCheatKeys = 1;
	}

	if (aLastTime == -1)
	{
		aLastTime = aTickCount;
		return;
	}

	if (aSession > 0)
	{
		aLastTime = aTickCount;

		if ((mBoard == nullptr || !mBoard->mPaused) && mHasFocus && mLastTimerTime - mLastUserInputTick <= 10000)
		{
			mPlayTimeActiveSession += aSession;

			if (mBoard)
			{
				mBoard->mPlayTimeActiveLevel += aSession;
			}

			if (mPlayerInfo)
			{
				mPlayerInfo->mPlayTimeActivePlayer += aSession;
			}
		}
		else
		{
			mPlayTimeInactiveSession += aSession;

			if (mBoard)
			{
				mBoard->mPlayTimeInactiveLevel += aSession;
			}

			if (mPlayerInfo)
			{
				mPlayerInfo->mPlayTimeInactivePlayer += aSession;
			}
		}
	}
}

//0x452650
void LawnApp::UpdateFrames()
{
	if ((!mActive || mMinimized) && mBoard)
	{
		mBoard->ResetFPSStats();
	}

	// co-op networking pump: process incoming messages first (they may
	// trigger scene changes / board creation), then remote inputs on the host
	if (mNetManager)
	{
		mNetManager->Update();
		NetTickPvpTeamSelection();
		if (mNetManager->IsHosting())
			NetBroadcastProfile(false);
		if (mNetChatTyping &&
			!((mNetManager->IsHosting() && mNetManager->GetPlayerCount() > 1) ||
				mNetManager->IsConnectedToHost()))
		{
			mNetChatTyping = false;
			mNetChatDraft.clear();
			SDL_StopTextInput(mSDLWindow);
		}

		// Use one cursor path in every scene.  SendCursor converts this window's
		// raw widget coordinates to the shared 800x600 canvas, so peers with a
		// different widescreen offset still draw the cursor at the same point.
		if (mNetManager->IsActive() && mWidgetManager)
		{
			int aCursorType = (int)CursorType::CURSOR_TYPE_NORMAL;
			int aHeldSeed = (int)SeedType::SEED_NONE;
			int aHeldImitater = (int)SeedType::SEED_NONE;
			if (mBoard && mBoard->mCursorObject)
			{
				aCursorType = (int)mBoard->mCursorObject->mCursorType;
				aHeldSeed = (int)mBoard->mCursorObject->mType;
				aHeldImitater = (int)mBoard->mCursorObject->mImitaterType;
			}
			mNetManager->SendCursor(aCursorType, aHeldSeed, aHeldImitater, 0,
				mWidgetManager->mLastMouseX, mWidgetManager->mLastMouseY);
		}

		if (mNetManager->IsHosting())
			NetHandleRemoteInputs();

		// menu lobby panel (created once loading finished so fonts are ready)
		if (mWidgetManager && mLoadingThreadCompleted && !mNetLobbyPanel)
		{
			mNetLobbyPanel = new LawnNet::NetLobbyPanel(this);
			mWidgetManager->AddWidget(mNetLobbyPanel);
		}

		// client: host pressed "Let's Rock" -> build the combined seed bank
		if (mNetManager->IsClient() && !mNetManager->mPendingChooserStarts.empty() &&
			mSeedChooserScreen && mBoard)
		{
			LawnNet::NetBuffer aStart = mNetManager->mPendingChooserStarts.front();
			if (mSeedChooserScreen->CoopBuildBankFromPayload(aStart))
			{
				mNetManager->mPendingChooserStarts.pop_front();
				mSeedChooserScreen->mStartButton->SetDisabled(true);
				if (mBoard->mCutScene && mBoard->mCutScene->mSeedChoosing)
					mBoard->mCutScene->EndSeedChooser();
			}
			else
			{
				LawnNet::NetTrace("client rejected malformed queued CHOOSER_START payload");
				mNetManager->mPendingChooserStarts.clear();
			}
		}
	}
	if (CoopSmokeActive())
		CoopSmokeUpdate();

#ifdef _DEBUG
	UpdatePlayTimeStats();
#endif

	int aUpdateCount = 1;
	if (gSlowMo)
	{
		++gSlowMoCounter;
		if (gSlowMoCounter < 4)
		{
			aUpdateCount = 0;
		}
		else
		{
			gSlowMoCounter = 0;
		}
	}
	else if (gFastMo)
	{
		aUpdateCount = 20;
	}

	for (int i = 0; i < aUpdateCount; i++)
	{
		mAppCounter++;
		
		if (mBoard)
		{
			mBoard->ProcessDeleteQueue();

			if (mBoard->mTimeStopCounter > 0)	
			{
				mBoard->mTimeStopCounter--;

				for (int j = 0; j <= 5; j++)
				{
					for (int k = 0; k < mBoard->mSeedBank->mNumPackets; k++)
					{
#ifdef _HAS_BLOOM_AND_DOOM_CONTENTS
						if (mBoard->mSeedBank->mSeedPackets[k].mPacketType == SeedType::SEED_TIMESTOPPER)
							continue;
#endif

						mBoard->mSeedBank->mSeedPackets[k].Update();
					}
				}
			}

			if (mGameMode == GameMode::GAMEMODE_CHALLENGE_SPEED && !mBoard->HasLevelAwardDropped()) {
				if (mBoard->mTimeStopCounter > 0)
				{
					mBoard->mTimeStopCounter--;

					for (int j = 0; j <= 5; j++)
					{
						for (int k = 0; k < mBoard->mSeedBank->mNumPackets; k++)
						{
#ifdef _HAS_BLOOM_AND_DOOM_CONTENTS
							if (mBoard->mSeedBank->mSeedPackets[k].mPacketType == SeedType::SEED_TIMESTOPPER)
								continue;
#endif

							mBoard->mSeedBank->mSeedPackets[k].Update();
						}
					}
				}
			}
		}

		SexyApp::UpdateFrames();
		if (mMusic && mMusic->mMusicInterface)
			mMusic->MusicUpdate();
		if (mLoadingThreadCompleted && mEffectSystem)
		{
			mEffectSystem->ProcessDeleteQueue();
		}

		CheckForGameEnd();
	}
}

void LawnApp::ToggleSlowMo()
{
	gSlowMoCounter = 0;
	gSlowMo = !gSlowMo;
	gFastMo = false;
}

void LawnApp::ToggleFastMo()
{
	gSlowMo = false;
	gFastMo = !gFastMo;
}

//0x452740
void LawnApp::LoadGroup(const char* theGroupName, int theGroupAveMsToLoad)
{
	PerfTimer aTimer;
	aTimer.Start();

	mResourceManager->StartLoadResources(theGroupName);
	while (!mShutdown && !mCloseRequest && !mLoadingFailed && TodLoadNextResource())
	{
		mCompletedLoadingThreadTasks += theGroupAveMsToLoad;
	}

	if (mShutdown || mCloseRequest)
		return;

	if (mResourceManager->HadError() || !ExtractResourcesByName(mResourceManager, theGroupName))
	{
		ShowResourceError();
		mLoadingFailed = true;
	}

	int aTotalGroupWeight = mResourceManager->GetNumResources(theGroupName) * theGroupAveMsToLoad;
	int aGroupTime = max(aTimer.GetDuration(), 0);
	TraceLoadGroup(theGroupName, aGroupTime, aTotalGroupWeight, theGroupAveMsToLoad);
}

//0x4528E0
void LawnApp::LoadingThreadProc()
{
	if (!TodLoadResources("LoaderBar"))
		return;

	WIN32_FILE_ATTRIBUTE_DATA fileInfo;
	if (GetFileAttributesExA("dependency\\properties\\LawnStrings.txt", GetFileExInfoStandard, &fileInfo) != 0 || IsFileInPakFile("dependency\\properties\\LawnStrings.txt"))
		TodStringListLoad(_S("dependency\\properties\\LawnStrings.txt"));

	TodStringListLoad(_S("Properties\\LawnStrings.txt"));

	if (GetFileAttributesExA("extension\\properties\\LawnStrings.txt", GetFileExInfoStandard, &fileInfo) != 0 || IsFileInPakFile("extension\\properties\\LawnStrings.txt"))
		TodStringListLoad(_S("extension\\properties\\LawnStrings.txt"));

#ifdef _ALLOW_RESOURCE_PACKS
	if (GetFileAttributesExA("resourcepack\\properties\\LawnStrings.txt", GetFileExInfoStandard, &fileInfo) != 0 || IsFileInPakFile("resourcepack\\properties\\LawnStrings.txt"))
		TodStringListLoad(_S("resourcepack\\properties\\LawnStrings.txt"));
#endif

#ifdef _HAS_ZOMBATAR
	if (!IsScreenSaver() && !IsParticleEditor())
		TodStringListLoad(_S("Properties\\ZombatarTOS.txt"));
#endif
	
	if (mTitleScreen)
	{
		mTitleScreen->mLoaderScreenIsLoaded = true;
	}

	const char* groups[] = { "LoadingFonts", "LoadingImages", "LoadingSounds" };
	int group_ave_ms_to_load[] = { 54, 9, 54 };
	for (int i = 0; i < 3; i++)
	{
		if (IsParticleEditor()) continue;

		mNumLoadingThreadTasks += mResourceManager->GetNumResources(groups[i]) * group_ave_ms_to_load[i];
	}
	mNumLoadingThreadTasks += 636;
	if (!IsParticleEditor())
		mNumLoadingThreadTasks += GetNumPreloadingTasks();
	mNumLoadingThreadTasks += mMusic->GetNumLoadingTasks();

	PerfTimer aTimer;
	aTimer.Start();

	TodHesitationTrace("start loading");
	TodHesitationBracket aHesitationResources("Resources");
	TodHesitationTrace("loading thread start");

	LoadGroup("LoadingFonts", 54);
	LoadGroup("LoadingImages", 9);

	if (mLoadingFailed || mShutdown || mCloseRequest)
		return;

	aHesitationResources.EndBracket();
	TodTrace("loading '%s' %d ms", "resources", (int)aTimer.GetDuration());

	mMusic->MusicInit();
	int aDuration = max(aTimer.GetDuration(), 0);
	aTimer.Start();

	if (/*!IsScreenSaver() && */!IsParticleEditor())
	{
		mPoolEffect = new PoolEffect();
		mPoolEffect->PoolEffectInitialize();
		mZenGarden = new ZenGarden();
	}

	if (!IsParticleEditor())
	{
		mReanimatorCache = new ReanimatorCache();
		mReanimatorCache->ReanimatorCacheInitialize();
	}

	TodFoleyInitialize(gLawnFoleyParamArray, LENGTH(gLawnFoleyParamArray));

	TodTrace("loading '%s' %d ms", "stuff", (int)aTimer.GetDuration());
	aTimer.Start();

	TrailLoadDefinitions(gLawnTrailArray, LENGTH(gLawnTrailArray));
	TodTrace("loading '%s' %d ms", "trail", (int)aTimer.GetDuration());
	aTimer.Start();
	TodHesitationTrace("trail");

	TodParticleLoadDefinitions(gLawnParticleArray, LENGTH(gLawnParticleArray));
	//aDuration = max(aTimer.GetDuration(), 0);
	aTimer.Start();

	if (!IsParticleEditor())
		PreloadForUser();
	
	if (mLoadingFailed || mShutdown || mCloseRequest)
		return;

	//aDuration = max(aTimer.GetDuration(), 0);
	aTimer.Start();

	//GetNumPreloadingTasks();
	if (!IsParticleEditor())
		LoadGroup("LoadingSounds", 54);
	TodHesitationTrace("finished loading");
}

//0x452C60
void LawnApp::FastLoad(GameMode theGameMode)
{
	if (!mShutdown)
	{
		mWidgetManager->RemoveWidget(mTitleScreen);
		SafeDeleteWidget(mTitleScreen);
		mTitleScreen = nullptr;

		PreNewGame(theGameMode, false);
	}
}

void LawnApp::LoadingThreadCompleted()
{
}

//0x452CB0
void LawnApp::LoadingCompleted()
{
	mWidgetManager->RemoveWidget(mTitleScreen);
	SafeDeleteWidget(mTitleScreen);
	mTitleScreen = nullptr;

	mResourceManager->DeleteImage("IMAGE_TITLESCREEN");
	if (IsScreenSaver()) 
	{
		KillBoard();
		mZenGarden->UpdatePlantNeeds();
		PreNewGame(GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN, false);
	}
	else if (IsParticleEditor())
	{
		KillBoard();
		ShowParticleEditor();
	}
	else
	{
		ShowGameSelector();
		if (CoopSmokeActive())
			CoopSmokeStartLifecycle();
	}
}

//0x452D80
void LawnApp::URLOpenFailed(const std::string& theURL)
{
	SexyAppBase::URLOpenFailed(theURL);
	KillDialog(Dialogs::DIALOG_OPENURL_WAIT);
	CopyToClipboard(theURL);

	std::string aString = 
		"Please open the following URL in your browser\n\n" + 
		theURL + 
		"\n\nFor your convenience, this URL has already been copied to your clipboard.";

	DoDialog(Dialogs::DIALOG_OPENURL_WAIT, true, _S("Open Browser"), _S("OK"), StringToSexyStringFast(aString), Dialog::BUTTONS_FOOTER);
}

//0x452EE0
void LawnApp::URLOpenSucceeded(const std::string& theURL)
{
	SexyAppBase::URLOpenSucceeded(theURL);
	KillDialog(Dialogs::DIALOG_OPENURL_WAIT);
}

//0x452F00
bool LawnApp::OpenURL(const std::string& theURL, bool shutdownOnOpen)
{
	DoDialog(
		Dialogs::DIALOG_OPENURL_WAIT, 
		true, 
		_S("Opening Browser"), 
		StringToSexyString(theURL),
		_S(""), 
		Dialog::BUTTONS_NONE
	);

	DrawDirtyStuff();

	return SexyAppBase::OpenURL(theURL, shutdownOnOpen);
}

//0x453040
void LawnApp::ConfirmQuit()
{
	SexyString aBody = TodStringTranslate(_S("[QUIT_MESSAGE]"));
	SexyString aHeader = TodStringTranslate(_S("[QUIT_HEADER]"));
	LawnDialog* aDialog = (LawnDialog*)DoDialog(Dialogs::DIALOG_QUIT, true, aHeader, aBody, _S(""), Dialog::BUTTONS_OK_CANCEL);
	aDialog->mLawnYesButton->mLabel = TodStringTranslate(_S("[QUIT_BUTTON]"));
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
}

//0x4531D0
void LawnApp::PreDisplayHook()
{
	SexyApp::PreDisplayHook();
}

void LawnApp::ButtonPress(int theId)
{
}

//0x4531E0
void LawnApp::ButtonDepress(int theId)
{
	if (theId % 10000 >= 2000 && theId % 10000 < 3000)  // 按钮编号 theId ∈ [2000, 3000) 时，表示按下 theId - 2000 编号的对话中的“是”按钮
	{
		switch (theId - 2000)
		{
		case Dialogs::DIALOG_NEW_GAME:
			KillDialog(Dialogs::DIALOG_NEW_GAME);
			ShowGameSelector();
			return;

		case Dialogs::DIALOG_NEWOPTIONS:
			KillNewOptionsDialog();
			return;

		case Dialogs::DIALOG_PREGAME_NAG:
			DoRegister();
			return;

		case Dialogs::DIALOG_LOAD_GAME:
			return;

		case Dialogs::DIALOG_CONFIRM_UPDATE_CHECK:
			KillDialog(Dialogs::DIALOG_CONFIRM_UPDATE_CHECK);
			CheckForUpdates();
			return;

		case Dialogs::DIALOG_QUIT:
			KillDialog(Dialogs::DIALOG_QUIT);
			SendMessage(mHWnd, WM_CLOSE, NULL, NULL);
			return;

		case Dialogs::DIALOG_NAG:
			KillDialog(Dialogs::DIALOG_NAG);
			DoRegister();
			return;

		case Dialogs::DIALOG_INFO:
			KillDialog(Dialogs::DIALOG_INFO);
			return;

		case Dialogs::DIALOG_PAUSED:
			KillDialog(Dialogs::DIALOG_PAUSED);
			return;

		case Dialogs::DIALOG_NO_MORE_MONEY:
			KillDialog(Dialogs::DIALOG_NO_MORE_MONEY);
			mBoard->AddSunMoney(100);
			return;

		case Dialogs::DIALOG_BONUS:
			KillDialog(Dialogs::DIALOG_BONUS);
			return;

		case Dialogs::DIALOG_CONFIRM_BACK_TO_MAIN:
			KillDialog(Dialogs::DIALOG_CONFIRM_BACK_TO_MAIN);
			mBoardResult = BoardResult::BOARDRESULT_QUIT;
			mBoard->TryToSaveGame();
			DoBackToMain();
			return;

		case Dialogs::DIALOG_USERDIALOG:
			FinishUserDialog(true);
			return;

		case Dialogs::DIALOG_CREATEUSER:
			FinishCreateUserDialog(true);
			return;

		case Dialogs::DIALOG_CONFIRMDELETEUSER:
			FinishConfirmDeleteUserDialog(true);
			return;

		case Dialogs::DIALOG_RENAMEUSER:
			FinishRenameUserDialog(true);
			return;

		case Dialogs::DIALOG_CREATEUSERERROR:
		case Dialogs::DIALOG_RENAMEUSERERROR:
			FinishNameError(theId - 2000);
			return;

		case Dialogs::DIALOG_CHEAT:
			FinishCheatDialog(true);
			return;

		case Dialogs::DIALOG_RESTARTCONFIRM:
			FinishRestartConfirmDialog();
			return;

		case Dialogs::DIALOG_TIMESUP:
			FinishTimesUpDialog();
			return;

		case 20008:
			KillDialog(20008);
			KillDialog(Dialogs::DIALOG_CHECKING_UPDATES);
			return;

		case Dialogs::DIALOG_CONFIRM_RIP_MODE:
			KillDialog(Dialogs::DIALOG_CONFIRM_RIP_MODE);
			mRIPMode = true;
			mPlayerInfo->mDidRIPMode = true;
			return;
		
		case Dialogs::DIALOG_UNLOCK:
		{
			KillDialog(Dialogs::DIALOG_UNLOCK);
			if (mPlayerInfo)
			{
				mPlayerInfo->SetLevel(1);
				mPlayerInfo->mFinishedAdventure = 2;
				mPlayerInfo->AddCoins(50000);
				mPlayerInfo->mHasUsedCheatKeys = true;
				mPlayerInfo->mHasUnlockedMinigames = true;
				mPlayerInfo->mHasUnlockedPuzzleMode = true;
				mPlayerInfo->mHasUnlockedSurvivalMode = true;

				mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_FERTILIZER] = PURCHASE_COUNT_OFFSET + 5;
				mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_BUG_SPRAY] = PURCHASE_COUNT_OFFSET + 5;
				mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_CHOCOLATE] = PURCHASE_COUNT_OFFSET + 5;
				mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_TREE_FOOD] = PURCHASE_COUNT_OFFSET + 5;
				mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_PLANT_GATLINGPEA] = 1;
				mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_PLANT_TWINSUNFLOWER] = 1;
				mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_PLANT_GLOOMSHROOM] = 1;
				mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_PLANT_CATTAIL] = 1;
				mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_PLANT_WINTERMELON] = 1;
				mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_PLANT_GOLD_MAGNET] = 1;
				mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_PLANT_SPIKEROCK] = 1;
				mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_PLANT_COBCANNON] = 1;
				mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_PLANT_IMITATER] = 1;
				mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_PACKET_UPGRADE] = 4;
				mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_POOL_CLEANER] = 1;
				mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_ROOF_CLEANER] = 1;
				mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_PHONOGRAPH] = 1;
				mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_GARDENING_GLOVE] = 1;
				mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_MUSHROOM_GARDEN] = 1;
				mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_WHEEL_BARROW] = 1;
				mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_AQUARIUM_GARDEN] = 1;
				mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_TREE_OF_WISDOM] = 1;
				mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_FIRSTAID] = 1;
				mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_GOLD_WATERINGCAN] = 1;
				mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_STINKY_THE_SNAIL] = 1;

				if (mGameSelector) mGameSelector->SyncProfile(true);

				EraseFile(GetSavedGameName(GameMode::GAMEMODE_ADVENTURE, mPlayerInfo->mId));
			}
			return;
		}

		//case Dialogs::DIALOG_MORESETTINGS:
		//{
		//	MoreSettingsDialog* aDialog = (MoreSettingsDialog*)GetDialog(Dialogs::DIALOG_MORESETTINGS);
		//	if (aDialog)
		//	{
		//		mWindowCursor = !aAdvanceDialog->mCustomCursor->IsChecked();
		//		RegistryWriteBoolean("WindowCursor", mWindowCursor);
		//		EnforceCursor();
		//		bool prevFPS = mFPSToggled;
		//		mFPSToggled = aAdvanceDialog->mFPSToggle->IsChecked();
		//		RegistryWriteBoolean("FPSToggled", mFPSToggled);
		//		mShowFPS = mFPSToggled;
		//		if (prevFPS != mShowFPS && mShowFPS)
		//		{
		//			mShowFPSMode = 0;
		//			Sexy::gFPSTimer.Start();
		//			Sexy::gFrameCount = 0;
		//			Sexy::gFPSDisplay = 0;
		//			Sexy::gForceDisplay = true;
		//		}
		//		mNoAutoPause = !aAdvanceDialog->mAutoPause->IsChecked();
		//		RegistryWriteBoolean("NoAutoPause", mNoAutoPause);
		//		mShowKeybindHint = aAdvanceDialog->mKeybindHint->IsChecked();
		//		RegistryWriteBoolean("ShowKeybindHint", mShowKeybindHint);
		//		mOptimizedGameplay = aAdvanceDialog->mOptimizedGameplay->IsChecked();
		//		RegistryWriteBoolean("OptimizeGameplay", mOptimizedGameplay);
		//		mClearerSeedCost = aAdvanceDialog->mClearerSeedCost->IsChecked();
		//		RegistryWriteBoolean("ClearerSeedCost", mClearerSeedCost);
		//		mNoTooltip = !aAdvanceDialog->mShowToolTip->IsChecked();
		//		RegistryWriteBoolean("NoTooltip", mNoTooltip);
		//		mNumericRecharge = aAdvanceDialog->mNumericRecharge->IsChecked();
		//		RegistryWriteBoolean("NumericRecharge", mNumericRecharge);
		//		bool prevWide = mWideToggled;
		//		mWideToggled = aAdvanceDialog->mWideScreen->IsChecked();
		//		RegistryWriteBoolean("WideToggled", mWideToggled);
		//		mNoWarnings = !aAdvanceDialog->mShowWarnings->IsChecked();
		//		RegistryWriteBoolean("NoWarnings", mNoWarnings);

		//		bool want3D = aAdvanceDialog->mHardwareAcceleration->IsChecked();
		//		SwitchScreenMode(mIsWindowed, want3D, false);

		//	/*	if (prevWide != mWideToggled) {
		//			SwitchScreenMode(mIsWindowed, mDDInterface->mIs3D, !mPlayerInfo->mHardmodeIsOff, !mPlayerInfo->mIsNotCoop, true);
		//			if (gLawnApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO && mBoard->mCutScene) mBoard->mCutScene->RehupScreen();
		//		}*/

		//		KillDialog(Dialogs::DIALOG_MORESETTINGS);
		//		ClearUpdateBacklog();
		//	}
		//	return;
		//}

		default:
			KillDialog(theId - 2000);
			return;
		}
	}

	if (theId % 10000 >= 3000 && theId < 4000)  // 按钮编号 theId ∈ [3000, 4000) 时，表示按下 theId - 3000 编号的对话中的“否”按钮
	{
		switch (theId - 3000)
		{
		case Dialogs::DIALOG_PREGAME_NAG:
			KillDialog(Dialogs::DIALOG_PREGAME_NAG);
			Shutdown();
			return;

		case Dialogs::DIALOG_LOAD_GAME:
			KillDialog(Dialogs::DIALOG_LOAD_GAME);
			return;

		case Dialogs::DIALOG_USERDIALOG:
			FinishUserDialog(false);
			return;

		case Dialogs::DIALOG_CREATEUSER:
			FinishCreateUserDialog(false);
			return;

		case Dialogs::DIALOG_CONFIRMDELETEUSER:
			FinishConfirmDeleteUserDialog(false);
			return;

		case Dialogs::DIALOG_RENAMEUSER:
			FinishRenameUserDialog(false);
			return;

		case Dialogs::DIALOG_CHEAT:
			FinishCheatDialog(false);
			return;

		case Dialogs::DIALOG_TIMESUP:
			FinishTimesUpDialog();
			return;

		case 10008:
			KillDialog(10008);
			KillDialog(Dialogs::DIALOG_CHECKING_UPDATES);
			return;

		default:
			KillDialog(theId - 3000);
			return;
		}
	}
}

void LawnApp::CenterDialog(Dialog* theDialog, int theWidth, int theHeight)
{
	theDialog->Resize((gLawnApp->mWidth - theWidth) / 2, (gLawnApp->mHeight - theHeight) / 2, theWidth, theHeight);
}

//0x453630
void LawnApp::PlayFoley(FoleyType theFoleyType)
{
	if (!mMuteSoundsForCutscene)
	{
		mSoundSystem->PlayFoley(theFoleyType);

		// co-op: host shares gameplay foley with clients
		if (mNetManager && mNetManager->IsHosting() && !LawnNet::gNetSuppressSounds)
		{
			LawnNet::NetBuffer anEvent;
			anEvent.WriteU16((uint16_t)theFoleyType);
			anEvent.WriteU16(0x4000); // fixed pitch
			mNetManager->SendBoardEvent(anEvent, LawnNet::BOARDEV_FOLEY);
		}
	}
}

//0x453650
void LawnApp::PlayFoleyPitch(FoleyType theFoleyType, float thePitch)
{
	if (!mMuteSoundsForCutscene)
	{
		mSoundSystem->PlayFoleyPitch(theFoleyType, thePitch);

		// co-op: host shares gameplay foley with clients
		if (mNetManager && mNetManager->IsHosting() && !LawnNet::gNetSuppressSounds)
		{
			LawnNet::NetBuffer anEvent;
			anEvent.WriteU16((uint16_t)theFoleyType);
			anEvent.WriteU16((uint16_t)(thePitch * 4096.0f));
			mNetManager->SendBoardEvent(anEvent, LawnNet::BOARDEV_FOLEY);
		}
	}
}

//0x453670
SexyString LawnApp::GetStageString(int theLevel)
{
	int aArea = ClampInt((abs(theLevel) - 1) / LEVELS_PER_AREA + 1, 1, ADVENTURE_AREAS + 1);
	int aSub = abs(theLevel) - (aArea - 1) * LEVELS_PER_AREA;
	SexyString aFormat;
	if (theLevel < 0) aFormat = StrFormat(_S(" -%d-%d"), aArea, aSub);
	else aFormat = StrFormat(_S(" %d-%d"), aArea, aSub);
	return aFormat;
} 

bool LawnApp::IsAdventureMode()
{
	return mGameMode == GameMode::GAMEMODE_ADVENTURE;
}

//0x4536D0
bool LawnApp::IsSurvivalMode()
{
	return mGameMode >= GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1 && mGameMode <= GameMode::GAMEMODE_SURVIVAL_ENDLESS_STAGE_5 || mGameMode >= GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_6 && mGameMode <= GameMode::GAMEMODE_SURVIVAL_ENDLESS_STAGE_HIGHGROUND || IsCoopEndlessMode(mGameMode);
}

//0x4536F0
bool LawnApp::IsPuzzleMode()
{
	return
		(mGameMode >= GameMode::GAMEMODE_SCARY_POTTER_1 && mGameMode <= GameMode::GAMEMODE_SCARY_POTTER_ENDLESS) ||
		(mGameMode >= GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_1 && mGameMode <= GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS);
}

//0x453710
bool LawnApp::IsChallengeMode()
{
	return !IsAdventureMode() && !IsPuzzleMode() && !IsSurvivalMode();
}

bool LawnApp::IsSurvivalNormal(GameMode theGameMode)
{
	int aLevel = theGameMode - GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1;
	int aLimboLevel = theGameMode - GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_6;
	return aLevel >= 0 && aLevel <= 4 || aLimboLevel >= 0 && aLimboLevel <= 1;
}

bool LawnApp::IsSurvivalHard(GameMode theGameMode)
{
	int aLevel = theGameMode - GameMode::GAMEMODE_SURVIVAL_HARD_STAGE_1;
	int aLimboLevel = theGameMode - GameMode::GAMEMODE_SURVIVAL_HARD_STAGE_6;
	return aLevel >= 0 && aLevel <= 4 || aLimboLevel >= 0 && aLimboLevel <= 1;
}

bool LawnApp::IsSurvivalEndless(GameMode theGameMode)
{
	int aLevel = theGameMode - GameMode::GAMEMODE_SURVIVAL_ENDLESS_STAGE_1;
	int aLimboLevel = theGameMode - GameMode::GAMEMODE_SURVIVAL_ENDLESS_STAGE_6;
	return aLevel >= 0 && aLevel <= 4 || aLimboLevel >= 0 && aLimboLevel <= 1;
}

bool LawnApp::IsEndlessScaryPotter(GameMode theGameMode)
{
	return theGameMode == GameMode::GAMEMODE_SCARY_POTTER_ENDLESS;
}

bool LawnApp::IsEndlessIZombie(GameMode theGameMode)
{
	return theGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS;
}

//0x453740
bool LawnApp::IsContinuousChallenge()
{
	return 
		IsArtChallenge() || 
		IsSlotMachineLevel() || 
		IsFinalBossLevel() || 
		mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED || 
		mGameMode == GameMode::GAMEMODE_UPSELL || 
		mGameMode == GameMode::GAMEMODE_INTRO || 
		mGameMode == GameMode::GAMEMODE_CHALLENGE_BEGHOULED_TWIST
#ifdef _DS_MINIGAMES
		|| mGameMode == GameMode::GAMEMODE_CHALLENGE_ZOMBIE_TRAP 
#endif
		;
}

bool LawnApp::IsArtChallenge()
{
	if (mBoard == nullptr)
		return false;

	return 
		mGameMode == GameMode::GAMEMODE_CHALLENGE_ART_CHALLENGE_WALLNUT || 
		mGameMode == GameMode::GAMEMODE_CHALLENGE_ART_CHALLENGE_SUNFLOWER || 
		mGameMode == GameMode::GAMEMODE_CHALLENGE_SEEING_STARS;
}

//0x4537B0
bool LawnApp::IsSquirrelLevel()
{
	return mBoard && mGameMode == GameMode::GAMEMODE_CHALLENGE_SQUIRREL;
}

//0x4537D0
bool LawnApp::IsIZombieLevel()
{
	if (mBoard == nullptr)
		return false;

	return
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_1 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_2 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_3 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_4 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_5 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_6 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_7 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_8 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_9 ||
		mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS;
}

//0x453820
bool LawnApp::IsShovelLevel()
{
	return mBoard && mGameMode == GameMode::GAMEMODE_CHALLENGE_SHOVEL;
}

//0x453840
bool LawnApp::IsWallnutBowlingLevel()
{
	if (mBoard == nullptr)
		return false;

	if (mGameMode == GameMode::GAMEMODE_CHALLENGE_WALLNUT_BOWLING || mGameMode == GameMode::GAMEMODE_CHALLENGE_WALLNUT_BOWLING_2)
		return true;

	return IsAdventureMode() && mPlayerInfo->mLevel == 5;
}

//0x453870
bool LawnApp::IsSlotMachineLevel()
{
	return (mBoard && mGameMode == GameMode::GAMEMODE_CHALLENGE_SLOT_MACHINE);
}

//0x453890
bool LawnApp::IsWhackAZombieLevel()
{
	if (mBoard == nullptr)
		return false;

	if (mGameMode == GameMode::GAMEMODE_CHALLENGE_WHACK_A_ZOMBIE)
		return true;

	return IsAdventureMode() && mPlayerInfo->mLevel == 15;
}

//0x4538C0
bool LawnApp::IsLittleTroubleLevel()
{
	if (mBoard == nullptr)
		return false;

	return mGameMode == GameMode::GAMEMODE_CHALLENGE_LITTLE_TROUBLE || (mGameMode == GameMode::GAMEMODE_ADVENTURE && mPlayerInfo->mLevel == 25);
}

//0x4538F0
bool LawnApp::IsScaryPotterLevel()
{
	if (mGameMode >= GameMode::GAMEMODE_SCARY_POTTER_1 && mGameMode <= GameMode::GAMEMODE_SCARY_POTTER_ENDLESS)
		return true;

#ifdef _MOBILE_MINIGAMES
	if (mGameMode == GameMode::GAMEMODE_CHALLENGE_VASEBREAKER)
		return true;
#endif

	if (mBoard == nullptr)
		return false;

	return IsAdventureMode() && mPlayerInfo->mLevel == 35;
}

//0x453920
bool LawnApp::IsStormyNightLevel()
{
	if (mBoard == nullptr)
		return false;

	if (mGameMode == GameMode::GAMEMODE_CHALLENGE_STORMY_NIGHT)
		return true;

	return IsAdventureMode() && mPlayerInfo->mLevel == 40;
}

//0x453950
bool LawnApp::IsBungeeBlitzLevel()
{
	if (mBoard == nullptr)
		return false;

	if (mGameMode == GameMode::GAMEMODE_CHALLENGE_BUNGEE_BLITZ)
		return true;

	return IsAdventureMode() && mPlayerInfo->mLevel == 45;
}

//0x453980
bool LawnApp::IsMiniBossLevel()
{
	if (mBoard == nullptr)
		return false;

	return
		(IsAdventureMode() && mPlayerInfo->mLevel == 10) ||
		(IsAdventureMode() && mPlayerInfo->mLevel == 20) ||
		(IsAdventureMode() && mPlayerInfo->mLevel == 30) ||
		(IsAdventureMode() && mPlayerInfo->mLevel == 40);
}

//0x4539D0
bool LawnApp::IsFinalBossLevel()
{
	if (mBoard == nullptr)
		return false;

	if (mGameMode == GameMode::GAMEMODE_CHALLENGE_FINAL_BOSS)
		return true;

	return IsAdventureMode() && mPlayerInfo->mLevel == 50;
}

//0x453A00
bool LawnApp::IsChallengeWithoutSeedBank()
{
	return 
		mGameMode == GameMode::GAMEMODE_CHALLENGE_RAINING_SEEDS || 
		mGameMode == GameMode::GAMEMODE_UPSELL || 
		mGameMode == GameMode::GAMEMODE_INTRO || 
		IsWhackAZombieLevel() || 
		IsSquirrelLevel() || 
		IsScaryPotterLevel() || 
		mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN || 
		mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM 
#ifdef _MOBILE_MINIGAMES
		|| mGameMode == GameMode::GAMEMODE_CHALLENGE_BUTTERED_POPCORN
#endif
#ifdef _DS_MINIGAMES
		|| mGameMode == GameMode::GAMEMODE_CHALLENGE_HEAT_WAVE
		|| mGameMode == GameMode::GAMEMODE_CHALLENGE_ZOMBIE_TRAP 
#endif
		;
}

bool LawnApp::IsNight()
{
	if (IsIceDemo() || mPlayerInfo == nullptr /*&& mBoard == nullptr*/)
		return false;

	if (mBoard)
		return (mBoard->mLevel >= 11 && mBoard->mLevel <= 20) || (mBoard->mLevel >= 31 && mBoard->mLevel <= 40) || mBoard->mLevel == 50;

	return (mPlayerInfo->mLevel >= 11 && mPlayerInfo->mLevel <= 20) || (mPlayerInfo->mLevel >= 31 && mPlayerInfo->mLevel <= 40) || mPlayerInfo->mLevel == 50;
}

int LawnApp::GetCurrentChallengeIndex()
{
	return (int)mGameMode - (int)GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1;
}

ChallengeDefinition& LawnApp::GetCurrentChallengeDef()
{
	return GetChallengeDefinition(GetCurrentChallengeIndex());
}

PottedPlant* LawnApp::GetPottedPlantByIndex(int thePottedPlantIndex)
{
	TOD_ASSERT(thePottedPlantIndex >= 0 && thePottedPlantIndex < mPlayerInfo->mNumPottedPlants);
	return &mPlayerInfo->mPottedPlant[thePottedPlantIndex];
}

//0x453A50
bool LawnApp::UpdateApp()
{
	if (mCloseRequest)
	{
		Shutdown();
		return false;
	}

	//if (mLoadingThreadCompleted)
	//{
	//	LoadingThreadCompleted();
	//}

	bool updated = SexyAppBase::UpdateApp();

	//if (mLoadingThreadCompleted && !mExitToTop)
	//{
	//	CheckForUpdates();
	//}

	return updated;
}

//0x453A70
void LawnApp::CloseRequestAsync()
{
	mDeferredMessages.clear();
	mExitToTop = true;
	mCloseRequest = true;
}

//0x453A90
SeedType LawnApp::GetAwardSeedForLevel(int theLevel)
{
	int aArea = (theLevel - 1) / LEVELS_PER_AREA + 1;
	int aSub = (theLevel - 1) % LEVELS_PER_AREA + 1;
	int aSeedsHasGot = (aArea - 1) * 8 + aSub;  // 一般来说，每大关可以获得 8 种植物，每小关可以获得 1 种植物
	if (aSub >= 10)
	{
		aSeedsHasGot -= 2;  // 到达第 10 小关时，本大关中有 2 小关的奖励不是新植物
	}
	else if (aSub >= 5)
	{
		aSeedsHasGot -= 1;  // 到达第 5 小关时，本大关中有 1 小关的奖励不是新植物
	}
	if (aSeedsHasGot > 40)
	{
		aSeedsHasGot = 40;
	}
	
	return (SeedType)aSeedsHasGot;
}

//0x453AC0
// ---- co-op custom mode helpers ----
bool LawnApp::IsCoopCustomMode()
{
	return mGameMode >= GameMode::GAMEMODE_COOP_ENDLESS && mGameMode <= GameMode::GAMEMODE_COOP_PVP;
}

bool LawnApp::IsCoopEndlessMode(GameMode theGameMode)
{
	return theGameMode >= GameMode::GAMEMODE_COOP_ENDLESS && theGameMode <= GameMode::GAMEMODE_COOP_ENDLESS_ST;
}

bool LawnApp::IsCoopEndlessRepick(GameMode theGameMode)
{
	return theGameMode == GameMode::GAMEMODE_COOP_ENDLESS_S || theGameMode == GameMode::GAMEMODE_COOP_ENDLESS_ST;
}

int LawnApp::GetSeedsAvailable()
{
	// co-op custom modes: every seed is available
	if (IsCoopCustomMode())
		return NUM_SEEDS_IN_CHOOSER;

	int aLevel = mBoard && mBoard->mIsReplay && mPlayerLevelRef > 4 ? mPlayerLevelRef : mPlayerInfo->GetLevel();
	int maxPlants = 49;

	if (HasFinishedAdventure() || aLevel > 50 && mPlayerInfo && mPlayerInfo->mHasUsedCheatKeys)
	{
		if (mTodCheatKeys || mDebugKeysEnabled || mPlayerInfo && mPlayerInfo->mHasUsedCheatKeys) maxPlants += NUM_SEEDS_IN_CHOOSER - SEED_IMITATER - 1;
		return maxPlants;
	}

	SeedType aSeedTypeMax = GetAwardSeedForLevel(aLevel);
	return min(maxPlants, aSeedTypeMax); 
}

//0x453B20
bool LawnApp::HasSeedType(SeedType theSeedType)
{
	if (IsTrialStageLocked() && theSeedType >= SeedType::SEED_JALAPENO)
		return false;

	/*  优化
	if (theSeedType >= SeedType::SEED_TWINSUNFLOWER && theSeedType <= SeedType::SEED_IMITATER)
		return mPlayerInfo->mPurchases[theSeedType - SeedType::SEED_GATLINGPEA];
	*/

	if (theSeedType == SeedType::SEED_GATLINGPEA)
	{
		return mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_PLANT_GATLINGPEA] > 0;
	}

	if (theSeedType == SeedType::SEED_TWINSUNFLOWER)
	{
		return mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_PLANT_TWINSUNFLOWER] > 0;
	}
	if (theSeedType == SeedType::SEED_GLOOMSHROOM)
	{
		return mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_PLANT_GLOOMSHROOM] > 0;
	}
	if (theSeedType == SeedType::SEED_CATTAIL)
	{
		return mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_PLANT_CATTAIL] > 0;
	}
	if (theSeedType == SeedType::SEED_WINTERMELON)
	{
		return mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_PLANT_WINTERMELON] > 0;
	}
	if (theSeedType == SeedType::SEED_GOLD_MAGNET)
	{
		return mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_PLANT_GOLD_MAGNET] > 0;
	}
	if (theSeedType == SeedType::SEED_SPIKEROCK)
	{
		return mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_PLANT_SPIKEROCK] > 0;
	}
	if (theSeedType == SeedType::SEED_COBCANNON)
	{
		return mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_PLANT_COBCANNON] > 0;
	}
	if (theSeedType == SeedType::SEED_IMITATER)
	{
		return mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_PLANT_IMITATER] > 0;
	}

	return theSeedType < GetSeedsAvailable();
}

bool LawnApp::SeedTypeAvailable(SeedType theSeedType)
{
	if (IsCoopCustomMode())
		return true;
	return HasSeedType(theSeedType);
}

//0x453C30
Reanimation* LawnApp::AddReanimation(float theX, float theY, int theRenderOrder, ReanimationType theReanimationType)
{
	return mEffectSystem->mReanimationHolder->AllocReanimation(theX, theY, theRenderOrder, theReanimationType);
}

//0x453C80
TodParticleSystem* LawnApp::AddTodParticle(float theX, float theY, int theRenderOrder, ParticleEffect theEffect)
{
	return mEffectSystem->mParticleHolder->AllocParticleSystem(theX, theY, theRenderOrder, theEffect);
}

ParticleSystemID LawnApp::ParticleGetID(TodParticleSystem* theParticle)
{
	return (ParticleSystemID)mEffectSystem->mParticleHolder->mParticleSystems.DataArrayGetID(theParticle);
}

ReanimationID LawnApp::ReanimationGetID(Reanimation* theReanimation)
{
	return (ReanimationID)mEffectSystem->mReanimationHolder->mReanimations.DataArrayGetID(theReanimation);
}

TodParticleSystem* LawnApp::ParticleGet(ParticleSystemID theParticleID)
{
	return mEffectSystem->mParticleHolder->mParticleSystems.DataArrayGet((unsigned int)theParticleID);
}

TodParticleSystem* LawnApp::ParticleTryToGet(ParticleSystemID theParticleID)
{
	return mEffectSystem->mParticleHolder->mParticleSystems.DataArrayTryToGet((unsigned int)theParticleID);
}

Reanimation* LawnApp::ReanimationGet(ReanimationID theReanimationID)
{
	return mEffectSystem->mReanimationHolder->mReanimations.DataArrayGet((unsigned int)theReanimationID);
}

//0x453CB0
Reanimation* LawnApp::ReanimationTryToGet(ReanimationID theReanimationID)
{
	// Presentation callbacks can outlive a board by one deferred widget tick.
	// Treat a torn-down effect system like a missing reanimation instead of
	// dereferencing the holder during transitions/custom-mode startup.
	if (!mEffectSystem || !mEffectSystem->mReanimationHolder)
	{
		static bool aMissingHolderTraced = false;
		if (!aMissingHolderTraced)
		{
			aMissingHolderTraced = true;
			LawnNet::NetTrace("ReanimationTryToGet skipped: effect system unavailable id=%u effect=%p holder=%p scene=%d board=%p",
				(unsigned)theReanimationID, (void*)mEffectSystem,
				mEffectSystem ? (void*)mEffectSystem->mReanimationHolder : nullptr,
				(int)mGameScene, (void*)mBoard);
		}
		return nullptr;
	}
        return mEffectSystem->mReanimationHolder->mReanimations.DataArrayTryToGet((unsigned int)theReanimationID);
}

//0x453CF0
void LawnApp::RemoveReanimation(ReanimationID theReanimationID)
{
	Reanimation* aReanim = ReanimationTryToGet(theReanimationID);
	if (aReanim)
	{
		aReanim->ReanimationDie();
	}
}

void LawnApp::RemoveParticle(ParticleSystemID theParticleID)
{
	TodParticleSystem* aParticle = ParticleTryToGet(theParticleID);
	if (aParticle)
	{
		aParticle->ParticleSystemDie();
	}
}

//0x453D20
bool LawnApp::AdvanceCrazyDaveText()
{
	std::string aMessageName = StrFormat("[CRAZY_DAVE_%d]", mCrazyDaveMessageIndex + 1).c_str();
	if (!TodStringListExists(aMessageName))
	{
		return false;
	}

	CrazyDaveTalkIndex(mCrazyDaveMessageIndex + 1);
	return true;
}

//0x453DC0
SexyString LawnApp::GetCrazyDaveText(int theMessageIndex)
{
	SexyString aMessage = StrFormat(_S("[CRAZY_DAVE_%d]"), theMessageIndex);
	aMessage = TodReplaceString(aMessage, _S("{PLAYER_NAME}"), mPlayerInfo->mName);
	aMessage = TodReplaceString(aMessage, _S("{MONEY}"), GetMoneyString(mPlayerInfo->mCoins));
	int aCost = StoreScreen::GetItemCost(StoreItem::STORE_ITEM_PACKET_UPGRADE);
	aMessage = TodReplaceString(aMessage, _S("{UPGRADE_COST}"), GetMoneyString(aCost));
	return aMessage;
}

//0x454070
bool LawnApp::CanShowAlmanac()
{
	if (IsIceDemo())
		return false;

	if (mPlayerInfo == nullptr)
		return false;

	return HasFinishedAdventure() || mPlayerInfo->mLevel >= 15;
}

//0x454090
bool LawnApp::CanShowStore()
{
	if (IsIceDemo())
		return false;

	if (mPlayerInfo == nullptr)
		return false;

	return HasFinishedAdventure() || mPlayerInfo->mHasSeenUpsell || mPlayerInfo->mLevel >= 25;
}

//0x4540C0
bool LawnApp::CanShowZenGarden()
{
	if (mPlayerInfo == nullptr)
		return false;

	if (IsTrialStageLocked())
		return false;

	return HasFinishedAdventure() || mPlayerInfo->mLevel >= 45;
}

bool LawnApp::CanSpawnYetis()
{
	const ZombieDefinition& aZombieDef = GetZombieDefinition(ZombieType::ZOMBIE_YETI);
	return HasFinishedAdventure() && (mPlayerInfo->mFinishedAdventure >= 2 || mPlayerInfo->mLevel >= aZombieDef.mStartingLevel);
}

//0x454120
bool LawnApp::HasBeatenChallenge(GameMode theGameMode)
{
	if (mPlayerInfo == nullptr)
		return false;

	int aChallengeIndex = theGameMode - GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1;
	TOD_ASSERT(aChallengeIndex >= 0 && aChallengeIndex < NUM_CHALLENGE_MODES);
	if (IsSurvivalNormal(theGameMode))
	{
		return mPlayerInfo->mChallengeRecords[aChallengeIndex] >= SURVIVAL_NORMAL_FLAGS;
	}
	if (IsSurvivalHard(theGameMode))
	{
		return mPlayerInfo->mChallengeRecords[aChallengeIndex] >= SURVIVAL_HARD_FLAGS;
	}
	if (IsSurvivalEndless(theGameMode) || IsEndlessScaryPotter(theGameMode) || IsEndlessIZombie(theGameMode) || IsLastStandEndless(theGameMode))
	{
		return false;
	}

	if (mGameMode == GameMode::GAMEMODE_UPSELL && theGameMode > GameMode::GAMEMODE_CHALLENGE_SLOT_MACHINE) {
		return false;
	}
	return mPlayerInfo->mChallengeRecords[aChallengeIndex] > 0;
}

//0x454170
bool LawnApp::HasFinishedAdventure()
{
	return mPlayerInfo && mPlayerInfo->mFinishedAdventure > 0;
}

//0x454190
bool LawnApp::IsFirstTimeAdventureMode()
{
	return IsAdventureMode() && !HasFinishedAdventure();
}

//0x4541B0
void LawnApp::CrazyDaveEnter()
{
	TOD_ASSERT(mCrazyDaveState == CRAZY_DAVE_OFF);
	TOD_ASSERT(!ReanimationTryToGet(mCrazyDaveReanimID));

	Reanimation* aCrazyDaveReanim = AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_CRAZY_DAVE);
	aCrazyDaveReanim->mIsAttachment = true;
	aCrazyDaveReanim->SetBasePoseFromAnim("anim_idle_handing");
	mCrazyDaveReanimID = ReanimationGetID(aCrazyDaveReanim);
	aCrazyDaveReanim->PlayReanim("anim_enter", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 24.0f);

	mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_ENTERING;
	mCrazyDaveMessageIndex = -1;
	mCrazyDaveMessageText.clear();
	mCrazyDaveBlinkCounter = RandRangeInt(400, 800);

	if (mGameScene == GameScenes::SCENE_LEVEL_INTRO && IsStormyNightLevel())
	{
		aCrazyDaveReanim->mColorOverride = Color(64, 64, 64);
	}
}

//0x4542F0
void LawnApp::CrazyDaveDie()
{
	Reanimation* aCrazyDaveReanim = ReanimationTryToGet(mCrazyDaveReanimID);
	if (aCrazyDaveReanim)
	{
		aCrazyDaveReanim->ReanimationDie();

		mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_OFF;
		mCrazyDaveReanimID = ReanimationID::REANIMATIONID_NULL;
		mCrazyDaveMessageIndex = -1;
		mCrazyDaveMessageText.clear();

		CrazyDaveStopSound();
	}
}

//0x454350
void LawnApp::CrazyDaveLeave()
{
	Reanimation* aCrazyDaveReanim = ReanimationTryToGet(mCrazyDaveReanimID);
	if (aCrazyDaveReanim)
	{
		if (mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_TALKING || mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_IDLING)
		{
			CrazyDaveDoneHanding();
		}

		aCrazyDaveReanim->PlayReanim("anim_leave", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 24.0f);
		aCrazyDaveReanim->SetImageOverride("Dave_mouths", nullptr);

		mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_LEAVING;
		mCrazyDaveMessageIndex = -1;
		mCrazyDaveMessageText.clear();

		CrazyDaveStopSound();
	}
}

//0x454430
void LawnApp::CrazyDaveTalkIndex(int theMessageIndex)
{
	mCrazyDaveMessageIndex = theMessageIndex;
	SexyString aMessageText = GetCrazyDaveText(theMessageIndex);
	CrazyDaveTalkMessage(aMessageText);
}

//0x4544A0
void LawnApp::CrazyDaveDoneHanding()
{
	Reanimation* aCrazyDaveReanim = ReanimationGet(mCrazyDaveReanimID);
	ReanimatorTrackInstance* aHandTrackInstance = aCrazyDaveReanim->GetTrackInstanceByName("Dave_handinghand");
	AttachmentDie(aHandTrackInstance->mAttachmentID);

	TodTrace("DoneHanding");
}

//0x454520
void LawnApp::CrazyDaveStopSound()
{
	mSoundSystem->StopFoley(FoleyType::FOLEY_CRAZY_DAVE_SHORT);
	mSoundSystem->StopFoley(FoleyType::FOLEY_CRAZY_DAVE_LONG);
	mSoundSystem->StopFoley(FoleyType::FOLEY_CRAZY_DAVE_EXTRA_LONG);
	mSoundSystem->StopFoley(FoleyType::FOLEY_CRAZY_DAVE_CRAZY);
}

//0x454570
void LawnApp::CrazyDaveTalkMessage(const SexyString& theMessage)
{
	Reanimation* aCrazyDaveReanim = ReanimationGet(mCrazyDaveReanimID);

	bool doHanding = false;
	if (theMessage.find(_S("{HANDING}")) != SexyString::npos)
	{
		doHanding = true;
	}
	if ((mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_TALKING || mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_IDLING) && !doHanding)
	{
		CrazyDaveDoneHanding();
	}

	bool doSound = true;
	if (theMessage.find(_S("{NO_SOUND}")) != SexyString::npos)
	{
		doSound = false;
	}
	else
	{
		CrazyDaveStopSound();
	}

	int aWordsCount = 0;
	bool isControlWord = false;
	for (int i = 0; i < theMessage.size(); i++)
	{
		if (theMessage[i] == _S('{'))
		{
			isControlWord = true;
		}
		else if (theMessage[i] == _S('}'))
		{
			isControlWord = false;
		}
		else if (!isControlWord)
		{
			aWordsCount++;
		}
	}

	aCrazyDaveReanim->SetImageOverride("Dave_mouths", nullptr);

	if (mCrazyDaveState != CrazyDaveState::CRAZY_DAVE_TALKING || doSound)
	{
		if (doHanding)
		{
			aCrazyDaveReanim->PlayReanim("anim_talk_handing", ReanimLoopType::REANIM_LOOP, 50, 12.0f);

			if (doSound)
			{
				if (theMessage.find(_S("{SHORT_SOUND}")) != SexyString::npos)
				{
					PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_SHORT);
				}
				else if (theMessage.find(_S("{SCREAM}")) != SexyString::npos)
				{
					PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_SCREAM);
				}
				else
				{
					PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_LONG);
				}
			}
			
			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_HANDING_TALKING;
		}
		else if (theMessage.find(_S("{SHAKE}")) != SexyString::npos)
		{
			aCrazyDaveReanim->PlayReanim("anim_crazy", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 50, 12.0f);

			if (doSound)
			{
				PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_CRAZY);
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_TALKING;
		}
		else if (theMessage.find(_S("{SCREAM}")) != SexyString::npos)
		{
			aCrazyDaveReanim->PlayReanim("anim_smalltalk", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 50, 12.0f);

			if (doSound)
			{
				PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_SCREAM);
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_TALKING;
		}
		else if (theMessage.find(_S("{SCREAM2}")) != SexyString::npos)
		{
			aCrazyDaveReanim->PlayReanim("anim_mediumtalk", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 50, 12.0f);

			if (doSound)
			{
				PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_SCREAM_2);
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_TALKING;
		}
		else if (theMessage.find(_S("{SHOW_WALLNUT}")) != SexyString::npos)
		{
			aCrazyDaveReanim->PlayReanim("anim_talk_handing", ReanimLoopType::REANIM_LOOP, 50, 12.0f);

			Reanimation* aWallnutReanim = AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_WALLNUT);
			aWallnutReanim->PlayReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 0, 12.0f);
			TodTrace("Handed");

			ReanimatorTrackInstance* aHandTrackInstance = aCrazyDaveReanim->GetTrackInstanceByName("Dave_handinghand");
			AttachEffect* aAttachEffect = AttachReanim(aHandTrackInstance->mAttachmentID, aWallnutReanim, 100.0f, 393.0f);
			aAttachEffect->mOffset.m00 = 1.2f;
			aAttachEffect->mOffset.m11 = 1.2f;

			aCrazyDaveReanim->Update();

			if (doSound)
			{
				PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_SCREAM_2);
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_HANDING_TALKING;
		}
		else if (theMessage.find(_S("{SHOW_HAMMER}")) != SexyString::npos)
		{
			aCrazyDaveReanim->PlayReanim("anim_talk_handing", ReanimLoopType::REANIM_LOOP, 50, 12.0f);

			Reanimation* aHammerReanim = AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_HAMMER);
			aHammerReanim->PlayReanim("anim_whack_zombie", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 24.0f);
			aHammerReanim->mAnimTime = 1.0f;

			ReanimatorTrackInstance* aHandTrackInstance = aCrazyDaveReanim->GetTrackInstanceByName("Dave_handinghand");
			AttachEffect* aAttachEffect = AttachReanim(aHandTrackInstance->mAttachmentID, aHammerReanim, 62.0f, 445.0f);
			aAttachEffect->mOffset.m00 = 1.5f;
			aAttachEffect->mOffset.m11 = 1.5f;

			aCrazyDaveReanim->Update();

			if (doSound)
			{
				PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_LONG);
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_HANDING_TALKING;
		}
		else if (theMessage.find(_S("{SHOW_FERTILIZER}")) != SexyString::npos)
		{
			aCrazyDaveReanim->PlayReanim("anim_talk_handing", ReanimLoopType::REANIM_LOOP, 50, 12.0f);

			Reanimation* aFertilizerReanim = AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_ZENGARDEN_FERTILIZER);
			aFertilizerReanim->PlayReanim("bag", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 24.0f);
			aFertilizerReanim->mAnimRate = 0.0f;

			ReanimatorTrackInstance* aHandTrackInstance = aCrazyDaveReanim->GetTrackInstanceByName("Dave_handinghand");
			AttachReanim(aHandTrackInstance->mAttachmentID, aFertilizerReanim, 102.0f, 412.0f);
			aCrazyDaveReanim->Update();

			if (doSound)
			{
				PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_LONG);
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_HANDING_TALKING;
		}
		else if (theMessage.find(_S("{SHOW_TREE_FOOD}")) != SexyString::npos)
		{
			aCrazyDaveReanim->PlayReanim("anim_talk_handing", ReanimLoopType::REANIM_LOOP, 50, 12.0f);

			Reanimation* aTreeFoodReanim = AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_TREEOFWISDOM_TREEFOOD);
			aTreeFoodReanim->PlayReanim("bag", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 24.0f);
			aTreeFoodReanim->mAnimRate = 0.0f;

			ReanimatorTrackInstance* aHandTrackInstance = aCrazyDaveReanim->GetTrackInstanceByName("Dave_handinghand");
			AttachReanim(aHandTrackInstance->mAttachmentID, aTreeFoodReanim, 102.0f, 412.0f);
			aCrazyDaveReanim->Update();

			if (doSound)
			{
				PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_LONG);
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_HANDING_TALKING;
		}
		else if (theMessage.find(_S("{SHOW_MONEYBAG}")) != SexyString::npos)
		{
			aCrazyDaveReanim->PlayReanim("anim_talk_handing", ReanimLoopType::REANIM_LOOP, 50, 12.0f);

			Reanimation* aMoneyBagReanim = AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_ZENGARDEN_FERTILIZER);
			aMoneyBagReanim->PlayReanim("bag", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 24.0f);
			aMoneyBagReanim->mAnimRate = 0.0f;
			aMoneyBagReanim->SetImageOverride("bag", IMAGE_MONEYBAG);

			ReanimatorTrackInstance* aHandTrackInstance = aCrazyDaveReanim->GetTrackInstanceByName("Dave_handinghand");
			AttachReanim(aHandTrackInstance->mAttachmentID, aMoneyBagReanim, 90.0f, 405.0f);
			aCrazyDaveReanim->Update();
			/*
			v16 = Reanimation::GetTrackInstanceByName(v3, "Dave_handinghand");
			theAnimRate = 405.0;
			v17 = 90.0;
			*/
			if (doSound)
			{
				PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_LONG);
			}

			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_HANDING_TALKING;
		}
		else
		{
			if (aWordsCount < 23)
			{
				aCrazyDaveReanim->PlayReanim("anim_smalltalk", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 50, 12.0f);

				if (doSound)
				{
					PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_SHORT);
				}

				mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_TALKING;
			}
			else if (aWordsCount < 52)
			{
				aCrazyDaveReanim->PlayReanim("anim_mediumtalk", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 50, 12.0f);

				if (doSound)
				{
					PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_LONG);
				}

				mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_TALKING;
			}
			else
			{
				aCrazyDaveReanim->PlayReanim("anim_blahblah", ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 50, 12.0f);

				if (doSound)
				{
					PlayFoley(FoleyType::FOLEY_CRAZY_DAVE_EXTRA_LONG);
				}

				mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_TALKING;
			}
		}
	}

	mCrazyDaveMessageText = theMessage;
}

//0x454ED0
void LawnApp::CrazyDaveStopTalking()
{
	bool aDoneHanding = true;
	if (mGameMode == GameMode::GAMEMODE_UPSELL)
	{
		aDoneHanding = false;
	}
	if (aDoneHanding && mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_TALKING)
	{
		CrazyDaveDoneHanding();
	}

	Reanimation* aCrazyDaveReanim = ReanimationGet(mCrazyDaveReanimID);
	aCrazyDaveReanim->SetImageOverride("Dave_mouths", nullptr);
	if (mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_TALKING && !aDoneHanding)
	{
		aCrazyDaveReanim->PlayReanim("anim_idle_handing", ReanimLoopType::REANIM_LOOP, 20, 12.0f);
		mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_HANDING_IDLING;
	}
	else if (mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_TALKING || mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_TALKING)
	{
		aCrazyDaveReanim->PlayReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 20, 12.0f);
		mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_IDLING;
	}

	mCrazyDaveMessageIndex = -1;
	mCrazyDaveMessageText.clear();
	CrazyDaveStopSound();
}

//0x455040
void LawnApp::UpdateCrazyDave()
{
	Reanimation* aCrazyDaveReanim = ReanimationTryToGet(mCrazyDaveReanimID);
	if (aCrazyDaveReanim == nullptr)
		return;

	if (mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_ENTERING || mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_TALKING)
	{
		if (aCrazyDaveReanim->mLoopCount > 0)
		{
			aCrazyDaveReanim->PlayReanim("anim_idle", ReanimLoopType::REANIM_LOOP, 20, 12.0f);
			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_IDLING;
		}
	}
	else if (mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_TALKING)
	{
		if (aCrazyDaveReanim->mLoopCount > 0)
		{
			aCrazyDaveReanim->PlayReanim("anim_idle_handing", ReanimLoopType::REANIM_LOOP, 20, 12.0f);
			mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_HANDING_IDLING;
		}
	}
	else if (mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_LEAVING && aCrazyDaveReanim->mLoopCount > 0)
	{
		CrazyDaveDie();
	}

	if (mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_IDLING || mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_IDLING)
	{
		if (mCrazyDaveMessageText.find(_S("{MOUTH_BIG_SMILE}")) != std::string::npos)
		{
			aCrazyDaveReanim->SetImageOverride("Dave_mouths", IMAGE_REANIM_CRAZYDAVE_MOUTH1);
		}
		else if (mCrazyDaveMessageText.find(_S("{MOUTH_SMALL_SMILE}")) != std::string::npos)
		{
			aCrazyDaveReanim->SetImageOverride("Dave_mouths", IMAGE_REANIM_CRAZYDAVE_MOUTH5);
		}
		else if (mCrazyDaveMessageText.find(_S("{MOUTH_BIG_OH}")) != std::string::npos)
		{
			aCrazyDaveReanim->SetImageOverride("Dave_mouths", IMAGE_REANIM_CRAZYDAVE_MOUTH4);
		}
		else if (mCrazyDaveMessageText.find(_S("{MOUTH_SMALL_OH}")) != std::string::npos)
		{
			aCrazyDaveReanim->SetImageOverride("Dave_mouths", IMAGE_REANIM_CRAZYDAVE_MOUTH6);
		}
	}

	if (mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_IDLING || mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_TALKING || 
		mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_TALKING || mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_HANDING_IDLING)
	{
		mCrazyDaveBlinkCounter--;
		if (mCrazyDaveBlinkCounter <= 0)
		{
			mCrazyDaveBlinkCounter = RandRangeInt(400, 800);
			Reanimation* aBlinkReanim = AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_CRAZY_DAVE);
			aBlinkReanim->SetFramesForLayer("anim_blink");
			aBlinkReanim->mLoopType = ReanimLoopType::REANIM_PLAY_ONCE_FULL_LAST_FRAME_AND_HOLD;
			aBlinkReanim->mAnimRate = 15.0f;
			aBlinkReanim->AttachToAnotherReanimation(aCrazyDaveReanim, "Dave_head");
			aBlinkReanim->mColorOverride = aCrazyDaveReanim->mColorOverride;
			aCrazyDaveReanim->AssignRenderGroupToTrack("Dave_eye", RENDER_GROUP_HIDDEN);
			mCrazyDaveBlinkReanimID = ReanimationGetID(aBlinkReanim);
		}
	}

	Reanimation* aBlinkReanim = ReanimationTryToGet(mCrazyDaveBlinkReanimID);
	if (aBlinkReanim && aBlinkReanim->mLoopCount > 0)
	{
		aCrazyDaveReanim->AssignRenderGroupToTrack("Dave_eye", RENDER_GROUP_NORMAL);
		RemoveReanimation(mCrazyDaveBlinkReanimID);
		mCrazyDaveBlinkReanimID = ReanimationID::REANIMATIONID_NULL;
	}

	aCrazyDaveReanim->Update();
}

//0x4552F0
void LawnApp::DrawCrazyDave(Graphics* g)
{
	Reanimation* aCrazyDaveReanim = ReanimationTryToGet(mCrazyDaveReanimID);
	if (aCrazyDaveReanim == nullptr)
		return;

	if (mCrazyDaveMessageText.size())
	{
		Image* aBubbleImage = IMAGE_STORE_SPEECHBUBBLE2;
		int aPosX = 285;
		int aPosY = 20;
		if (GetDialog(Dialogs::DIALOG_STORE))
		{
			aBubbleImage = IMAGE_STORE_SPEECHBUBBLE;
			aPosX -= 180;
			aPosY -= 78;
		}
		else if (mGameMode == GameMode::GAMEMODE_UPSELL)
		{
			aPosX += 130;
			aPosY += 70;
		}
		g->DrawImage(aBubbleImage, aPosX, aPosY);

		SexyString aBubbleText = mCrazyDaveMessageText;
		Rect aRect(aPosX + 25, aPosY + 6, 233, 144);
		if (aBubbleText.find(_S("{SHAKE}")) != SexyString::npos)
		{
			aBubbleText = TodReplaceString(aBubbleText, _S("{SHAKE}"), _S(""));
			aRect.mX += rand() % 2;
			aRect.mY += rand() % 2;
		}

		bool clickToContinue = true;
		if (mGameMode == GameMode::GAMEMODE_UPSELL)
		{
			clickToContinue = false;
		}
		else if (aBubbleText.find(_S("{NO_CLICK}")) != SexyString::npos)
		{
			aBubbleText = TodReplaceString(aBubbleText, _S("{NO_CLICK}"), _S(""));
			clickToContinue = false;
		}

		TodDrawStringWrapped(g, aBubbleText, aRect, FONT_BRIANNETOD16, Color::Black, DrawStringJustification::DS_ALIGN_CENTER_VERTICAL_MIDDLE);
		if (clickToContinue)
		{
			TodDrawString(g, _S("click to continue"), aPosX + 139, aPosY + 140, FONT_PICO129, Color::Black, DrawStringJustification::DS_ALIGN_CENTER);
		}
	}

	aCrazyDaveReanim->Draw(g);
}

//0x455670
int LawnApp::GetNumPreloadingTasks()
{
	int aTaskCount = IsScreenSaver() ? 0 : 13;
	if (mPlayerInfo)
	{
		for (SeedType i = SeedType::SEED_PEASHOOTER; i < SeedType::NUM_SEED_TYPES; i = (SeedType)((int)i + 1))
		{
			if (SeedTypeAvailable(i) || HasFinishedAdventure())
			{
				aTaskCount++;
			}

		}

		for (ZombieType i = ZombieType::ZOMBIE_NORMAL; i < ZombieType::NUM_ZOMBIE_TYPES;i = (ZombieType)((int)i + 1))
		{
			if (IsScreenSaver()) break;

			if (HasFinishedAdventure() || mPlayerInfo->mLevel >= GetZombieDefinition(i).mStartingLevel)
			{
				/*if (i != ZombieType::ZOMBIE_BOSS &&
					i != ZombieType::ZOMBIE_CATAPULT &&
					i != ZombieType::ZOMBIE_GARGANTUAR &&
					i != ZombieType::ZOMBIE_DIGGER &&
					i != ZombieType::ZOMBIE_ZAMBONI)*/
				{
					aTaskCount++;
				}
			}
		}
	}
	return aTaskCount * 68;
}

//0x455720
void LawnApp::PreloadForUser()
{
	int aNumTasks = mCompletedLoadingThreadTasks + GetNumPreloadingTasks();
	if (mTitleScreen && mTitleScreen->mQuickLoadKey != KeyCode::KEYCODE_UNKNOWN)
	{
		TodTrace("preload canceled\n");
		mNumLoadingThreadTasks = aNumTasks;
		return;
	}

	if (!IsScreenSaver())
	{
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_PUFF, true);
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_LAWN_MOWERED_ZOMBIE, true);
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_READYSETPLANT, true);
		if (mCompletedLoadingThreadTasks < aNumTasks)
			mCompletedLoadingThreadTasks += 68; // 204
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_FINAL_WAVE, true);
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_SUN, true);
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_TEXT_FADE_ON, true);
		if (mCompletedLoadingThreadTasks < aNumTasks)
			mCompletedLoadingThreadTasks += 68; // 204
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZOMBIE, true);
		if (mCompletedLoadingThreadTasks < aNumTasks)
			mCompletedLoadingThreadTasks += 68;
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZOMBIE_NEWSPAPER, true);
		if (mCompletedLoadingThreadTasks < aNumTasks)
			mCompletedLoadingThreadTasks += 68; // 393
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_SELECTOR_SCREEN, true);
		if (mCompletedLoadingThreadTasks < aNumTasks)
			mCompletedLoadingThreadTasks += 340; // 340
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZOMBIE_HAND, true);
		if (mCompletedLoadingThreadTasks < aNumTasks)
			mCompletedLoadingThreadTasks += 68;
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_TEXT_SLIDE_ON, true);
		if (mCompletedLoadingThreadTasks < aNumTasks)
			mCompletedLoadingThreadTasks += 68;
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_TEXT_SLIDE_DOWN, true);
		if (mCompletedLoadingThreadTasks < aNumTasks)
			mCompletedLoadingThreadTasks += 68;
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_SELECTORSCREEN_SPOTLIGHT, true);
		if (mCompletedLoadingThreadTasks < aNumTasks)
			mCompletedLoadingThreadTasks += 68;
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_BUSHES3, true);
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_BUSHES4, true);
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_BUSHES5, true);
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_NIGHT_BUSHES3, true);
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_NIGHT_BUSHES4, true);
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_NIGHT_BUSHES5, true);
		if (mCompletedLoadingThreadTasks < aNumTasks)
			mCompletedLoadingThreadTasks += 408;
	}

	if (mPlayerInfo)
	{
		for (SeedType i = SeedType::SEED_PEASHOOTER; i < SeedType::NUM_SEED_TYPES; i = (SeedType)((int)i + 1))
		{
			if (SeedTypeAvailable(i) || HasFinishedAdventure())
			{
				Plant::PreloadPlantResources(i);
				if (mCompletedLoadingThreadTasks < aNumTasks)
				{
					mCompletedLoadingThreadTasks += 68;
				}

				if (mTitleScreen && mTitleScreen->mQuickLoadKey != KeyCode::KEYCODE_UNKNOWN)
				{
					TodTrace("preload canceled\n");
					mNumLoadingThreadTasks = aNumTasks;
					return;
				}

				if (mShutdown || mCloseRequest)
				{
					return;
				}
			}
		}

		for (ZombieType i = ZombieType::ZOMBIE_NORMAL; i < ZombieType::NUM_ZOMBIE_TYPES;i = (ZombieType)((int)i + 1))
		{
			if (IsScreenSaver()) break;

			if (mPlayerInfo->mLevel >= GetZombieDefinition(i).mStartingLevel || HasFinishedAdventure())
			{
				if (i == ZombieType::ZOMBIE_BOSS && i == ZombieType::ZOMBIE_CATAPULT && i == ZombieType::ZOMBIE_GARGANTUAR ||
					i == ZombieType::ZOMBIE_DIGGER && i == ZombieType::ZOMBIE_ZAMBONI)
				{
					continue;
				}

				Zombie::PreloadZombieResources(i);
				if (mCompletedLoadingThreadTasks < aNumTasks)
				{
					mCompletedLoadingThreadTasks += 68;
				}

				if (mTitleScreen && mTitleScreen->mQuickLoadKey != KeyCode::KEYCODE_UNKNOWN)
				{
					TodTrace("preload canceled\n");
					mNumLoadingThreadTasks = aNumTasks;
					return;
				}

				if (mShutdown || mCloseRequest)
				{
					return;
				}
			}
		}
	}

	if (mCompletedLoadingThreadTasks != aNumTasks)
	{
		TodTrace("num preload tasks wasn't calculated correctly");
		mCompletedLoadingThreadTasks = aNumTasks;
	}
}


//0x455930
void LawnApp::EnforceCursor()
{
	if (mSEHOccured || !mMouseIn)
	{
		SDL_SetCursor(NULL);
		return;
	}

    LRESULT hitTest = SendMessage(mHWnd, WM_NCHITTEST, 0, MAKELPARAM(GET_X_LPARAM(GetMessagePos()), GET_Y_LPARAM(GetMessagePos())));
	if (hitTest != HTCLIENT)
		return;

	SDL_Cursor* newCursor = nullptr;

	if (mIsPlayingVideo)
	{
		SDL_HideCursor();
	}
	else if (mOverrideCursor)
	{
		SDL_SetCursor(newCursor);
		SDL_ShowCursor();
	}
	else
	{
		switch (mCursorNum)
		{
			//case CURSOR_POINTER: newCursor = mSDLPointerCursor; break;
			case CURSOR_HAND: newCursor = mSDLHandCursor; break;
			case CURSOR_DRAGGING: newCursor = mSDLDraggingCursor; break;
			case CURSOR_TEXT: newCursor = mSDLTextCursor; break;
			case CURSOR_CIRCLE_SLASH: newCursor = mSDLNoCursor; break;
			case CURSOR_WAIT: newCursor = mSDLWaitCursor; break;
			case CURSOR_CUSTOM:
			case CURSOR_NONE: SDL_HideCursor(); break;
			default: newCursor = IsParticleEditor() ? SDL_GetDefaultCursor() : mSDLPointerCursor; break;
		}
	}

	if (newCursor || IsParticleEditor())
	{
		SDL_ShowCursor();
		SDL_SetCursor(newCursor);
	}
}

//0x455AA0
SexyString LawnApp::Pluralize(int theCount, const SexyChar* theSingular, const SexyChar* thePlural)
{
	if (theCount == 1)
	{
		return TodReplaceNumberString(theSingular, _S("{COUNT}"), theCount);
	}

	return TodReplaceNumberString(thePlural, _S("{COUNT}"), theCount);
}

//0x455BA0
int LawnApp::GetNumTrophies(ChallengePage thePage)
{
	int aNumTrophies = 0;

	for (int i = 0; i < NUM_CHALLENGE_MODES; i++)
	{
		const ChallengeDefinition& aDef = GetChallengeDefinition(i);
		if (aDef.mPage == thePage && HasBeatenChallenge(aDef.mChallengeMode))
		{
			aNumTrophies++;
		}
	}

	return aNumTrophies;
}

//0x455C20
int LawnApp::TrophiesNeedForGoldSunflower()
{
	return 48 - GetNumTrophies(CHALLENGE_PAGE_SURVIVAL) - GetNumTrophies(CHALLENGE_PAGE_CHALLENGE) - GetNumTrophies(CHALLENGE_PAGE_PUZZLE);
}

//0x455C50
bool LawnApp::EarnedGoldTrophy()
{
	return HasFinishedAdventure() && TrophiesNeedForGoldSunflower() <= 0;
}

void LawnApp::FinishZenGardenToturial()
{
	mBoardResult = BoardResult::BOARDRESULT_WON;
	KillBoard();
	PreNewGame(GameMode::GAMEMODE_ADVENTURE, false);
}

//0x455C90
bool LawnApp::IsTrialStageLocked()
{
	if (mDebugTrialLocked)
		return true;

	if (mDRM && mDRM->QueryData())
		return false;

	return mTrialType == TrialType::TRIALTYPE_STAGELOCKED;
}

//0x455CC0
void LawnApp::InitHook()
{
	// SexyAppBase::Init selects CommonAppData after command-line parsing.
	// Restore the already validated per-process smoke root before profiles load.
	if (CoopSmokeActive())
		SetAppDataFolder(mCoopSmokeManifest.mAppDataDir);

#ifdef _DEBUG
	mDRM = nullptr;
#else
	mDRM = new PopDRMComm();
	mDRM->DoIPC();
	if (sexystricmp(GetString("MarketingMode", _S("")).c_str(), _S("StageLocked")) == 0)
	{
		mTrialType = TrialType::TRIALTYPE_STAGELOCKED;
		mDRM->EnableLocking();
	}
	else
	{
		mTrialType = TrialType::TRIALTYPE_NONE;
	}
#endif
}

//0x455E10
SexyString LawnApp::GetMoneyString(int theAmount)
{
	int aValue = theAmount * 10;
	if (aValue > 999999)
	{
		return StrFormat(_S("$%d,%03d,%03d"), aValue / 1000000, (aValue - aValue / 1000000 * 1000000) / 1000, aValue - aValue / 1000 * 1000);
	}
	else if (aValue > 9999)
	{
		return StrFormat(_S("$%d,%03d"), aValue / 1000, aValue - aValue / 1000 * 1000);
	}
	else
	{
		return StrFormat(_S("$%d"), aValue);
	}
}

//0x455EE0
SexyString LawnGetCurrentLevelName()
{
	if (gLawnApp == nullptr)
	{
		return _S("Before App");
	}
	if (gLawnApp->mGameScene == GameScenes::SCENE_LOADING)
	{
		return _S("Game Loading");
	}
	if (gLawnApp->mGameScene == GameScenes::SCENE_MENU)
	{
		return _S("Game Selector");
	}
	if (gLawnApp->mGameScene == GameScenes::SCENE_AWARD)
	{
		return _S("Award Screen");
	}
	if (gLawnApp->mGameScene == GameScenes::SCENE_CHALLENGE)
	{
		return _S("Challenge Screen");
	}
	if (gLawnApp->mGameScene == GameScenes::SCENE_CREDIT)
	{
		return _S("Credits");
	}
	if (gLawnApp->mBoard == nullptr)
	{
		return _S("Not Playing");
	}

	if (gLawnApp->IsFirstTimeAdventureMode())
	{
		return gLawnApp->GetStageString(gLawnApp->mBoard->mLevel);
	}
	if (gLawnApp->IsAdventureMode())
	{
		return StrFormat(_S("F%d"), gLawnApp->GetStageString(gLawnApp->mBoard->mLevel).c_str());
	}

	return gLawnApp->GetCurrentChallengeDef().mChallengeName;
}

//0x456060
bool LawnApp::CanDoPinataMode()
{
	if (mPlayerInfo == nullptr)
		return false;

	return mPlayerInfo->mChallengeRecords[(int)GameMode::GAMEMODE_TREE_OF_WISDOM - (int)GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1] >= 1000;
}

//0x456080
bool LawnApp::CanDoDanceMode()
{
	if (mPlayerInfo == nullptr)
		return false;

	return mPlayerInfo->mChallengeRecords[(int)GameMode::GAMEMODE_TREE_OF_WISDOM - (int)GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1] >= 500;
}

//0x4560A0
bool LawnApp::CanDoDaisyMode()
{
	if (mPlayerInfo == nullptr)
		return false;

	return mPlayerInfo->mChallengeRecords[(int)GameMode::GAMEMODE_TREE_OF_WISDOM - (int)GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1] >= 100;
}

//0x4560C0
void LawnApp::PlaySample(int theSoundNum)
{
	if (!mMuteSoundsForCutscene)
	{
		SexyAppBase::PlaySample(theSoundNum);
		if (CoopSmokeActive() && theSoundNum == Sexy::SOUND_SLOT_MACHINE)
			mCoopSmokeScenarioRuntime.OnSlotMachineSound(NetIsHost());

		// co-op: host shares gameplay sounds with clients
		if (mNetManager && mNetManager->IsHosting() && !LawnNet::gNetSuppressSounds)
		{
			LawnNet::NetBuffer anEvent;
			anEvent.WriteU16((uint16_t)theSoundNum);
			mNetManager->SendBoardEvent(anEvent, LawnNet::BOARDEV_SOUND);
		}
	}
}

//0x4560E0
void LawnApp::SwitchScreenMode(bool wantWindowed, bool is3d, bool force)
{
	//SexyAppBase::SwitchScreenMode(wantWindowed, is3d, force);
	mIsWindowed = wantWindowed;
	SDL_SetWindowFullscreen(mSDLWindow, !wantWindowed);
	// borderless fullscreen desktop mode (native resolution, no mode switch)
	if (mNetManager && mNetManager->mConfig.mBorderlessFullscreen)
		SDL_SetWindowFullscreenMode(mSDLWindow, nullptr);

	NewOptionsDialog* aNewOptionsDialog = (NewOptionsDialog*)GetDialog(Dialogs::DIALOG_NEWOPTIONS);
	if (aNewOptionsDialog)
	{
		aNewOptionsDialog->mFullscreenCheckbox->SetChecked(!mIsWindowed);
	}
}

/* #################################################################################################### */

void LawnApp::BetaSubmit(bool theAskForComments)
{

}

void LawnApp::BetaRecordLevelStats()
{

}

void LawnApp::BetaAddFile(std::list<std::string>& theUploadFileList, std::string theFileName, std::string theShortName)
{

}

void LawnApp::TraceLoadGroup(const char* theGroupName, int theGroupTime, int theTotalGroupWeigth, int theTaskWeight)
{

}

/* #################################################################################################### */

void LawnApp::DoHighScoreDialog()
{

}

void LawnApp::DoRegister()
{

}

void LawnApp::DoRegisterError()
{

}

bool LawnApp::CanDoRegisterDialog()
{
	return false;
}

void LawnApp::DoNeedRegisterDialog()
{

}

void LawnApp::FinishModelessDialogs()
{

}

bool LawnApp::NeedRegister()
{
	return false;
}

void LawnApp::UpdateRegisterInfo()
{

}

#ifdef _HAS_ZOMBATAR
void LawnApp::ShowZombatarTOS()
{
	KillDialog(Dialogs::DIALOG_ZOMBATARTOS);

	ZombatarTOS* aDialog = new ZombatarTOS(this);
	aDialog->mApp = this;
	CenterDialog(aDialog, aDialog->mWidth, aDialog->mHeight);
	aDialog->mY -= 16.5f;
	AddDialog(Dialogs::DIALOG_ZOMBATARTOS, aDialog);
	mWidgetManager->SetFocus(aDialog);
}
#endif

void LawnApp::ShowLanagugeScreen()
{
	mLanguageScreen = new LanguageWidget(this);
	mWidgetManager->AddWidget(mLanguageScreen);
	mWidgetManager->BringToBack(mLanguageScreen);
	mWidgetManager->SetFocus(mLanguageScreen);
}

void LawnApp::KillLanguageScreen()
{
	if (mLanguageScreen)
	{
		mWidgetManager->RemoveWidget(mLanguageScreen);
		SafeDeleteWidget(mLanguageScreen);
		mLanguageScreen = nullptr;
	}
}

bool LawnApp::ChallengeUsesMicrophone(GameMode theGameMode)
{
	return
#ifdef _DS_MINIGAMES
		theGameMode == GameMode::GAMEMODE_CHALLENGE_HEAT_WAVE; // ||
#endif
		//theGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN && gLawnApp->IsScreenSaver();
}

bool LawnApp::ChallengeHasScores(GameMode theGameMode)
{
#ifndef _HAS_SCORE_SYSTEM
	return false;
#endif
	return IsEndlessIZombie(theGameMode) || IsEndlessScaryPotter(theGameMode) || IsSurvivalEndless(theGameMode) || IsLastStandEndless(theGameMode);
}

bool LawnApp::IsLastStand() {
	bool aIsLastStand = mGameMode == GameMode::GAMEMODE_CHALLENGE_LAST_STAND;
	aIsLastStand |= (mGameMode >= GameMode::GAMEMODE_LAST_STAND_STAGE_1 && mGameMode <= GameMode::GAMEMODE_LAST_STAND_STAGE_5);
	aIsLastStand |= IsLastStandEndless(mGameMode);
	return aIsLastStand;
}

bool LawnApp::IsLastStandEndless(GameMode theGameMode)
{
	int aLevel = theGameMode - GameMode::GAMEMODE_LAST_STAND_ENDLESS_STAGE_1;
	return aLevel >= 0 && aLevel <= 4;
}

void LawnApp::ShowParticleEditor()
{
	if (mGameSelector)
	{
		mWidgetManager->RemoveWidget(mGameSelector);
		SafeDeleteWidget(mGameSelector);
	}

	mParticleScreen = new ParticleScreen(this);
	mParticleScreen->Resize(0, 0, mWidth, mHeight);
	mWidgetManager->AddWidget(mParticleScreen);
	mWidgetManager->BringToBack(mParticleScreen);
	mWidgetManager->SetFocus(mParticleScreen);
}

bool LawnApp::TryToInitializePA()
{
	PaError err = Pa_Initialize();
	if (err != paNoError)
		return false;

	err = Pa_OpenDefaultStream(&mPortAudioStream, 1, 0, paFloat32, SAMPLE_RATE, FRAMES_PER_BUFFER, AudioCallback, &mVoiceVolume);
	if (err != paNoError)
		return false;

	err = Pa_StartStream(mPortAudioStream);
	if (err != paNoError)
		return false;

	return true;
}


void LawnApp::DoConfirmRIPMode()
{
	LawnDialog* aDialog = (LawnDialog*)DoDialog(
		Dialogs::DIALOG_CONFIRM_RIP_MODE,
		true,
		_S("Turn On R.I.P. Mode?"),
		_S("Are you sure you are up to the challenge?\nThe game will be extra difficult- one game over and you're back to the beginning."),
		_S(""),
		Dialog::BUTTONS_YES_NO
	);
}

void LawnApp::DoMoreSettingsDialog()
{
	MoreSettingsDialog* aDialog = new MoreSettingsDialog(this);
	AddDialog(Dialogs::DIALOG_MORESETTINGS, aDialog);
}

// ==========================================================================
// Co-op networking
// ==========================================================================

namespace
{
std::string CoopSmokeJsonEscape(const char* theText)
{
	std::string aResult;
	const unsigned char* aChar = (const unsigned char*)(theText ? theText : "");
	while (*aChar)
	{
		switch (*aChar)
		{
		case '\\': aResult += "\\\\"; break;
		case '"': aResult += "\\\""; break;
		case '\b': aResult += "\\b"; break;
		case '\f': aResult += "\\f"; break;
		case '\n': aResult += "\\n"; break;
		case '\r': aResult += "\\r"; break;
		case '\t': aResult += "\\t"; break;
		default:
			if (*aChar < 0x20)
			{
				char aControl[7];
				sprintf_s(aControl, "\\u%04x", (unsigned)*aChar);
				aResult += aControl;
			}
			else
				aResult += (char)*aChar;
			break;
		}
		++aChar;
	}
	return aResult;
}
}

bool LawnApp::CoopSmokeActive() const
{
	return mCoopSmokeManifestValid && mCoopSmokeManifest.mEnabled;
}

bool LawnApp::CoopSmokeWriteResult(bool thePassed, const char* theReason)
{
	if (!CoopSmokeActive() || mCoopSmokeResultWritten)
		return false;

	// Finalize at most once even if the filesystem operation itself fails.  A
	// missing result then remains an unambiguous harness failure instead of a
	// later frame accidentally replacing the first outcome.
	mCoopSmokeResultWritten = true;
	const char* aRole = mCoopSmokeManifest.IsHost() ? "host" : "client";
	const LawnNet::NetSmokeScenarioDefinition* aScenario =
		LawnNet::NetGetSmokeScenario(mCoopSmokeManifest.mScenario);
	std::string aJson = StrFormat(
		"{\"status\":\"%s\",\"role\":\"%s\",\"protocol\":%u,"
		"\"scenario\":\"%s\",\"mode\":\"%s\",\"level\":%d,\"seed\":%u,"
		"\"skipIntro\":%s,\"snapshotsSent\":%u,\"snapshotsReceived\":%u,"
		"\"snapshotsApplied\":%u,\"actionApplied\":%s,\"postActionSnapshots\":%u",
		thePassed ? "pass" : "fail", aRole, LawnNet::NET_PROTOCOL_VERSION,
		aScenario ? aScenario->mName : "invalid", mCoopSmokeManifest.mMode.c_str(),
		mCoopSmokeManifest.mLevel, mCoopSmokeManifest.mSeed,
		mCoopSmokeManifest.mSkipIntro ? "true" : "false",
		mCoopSmokeSnapshotsSent, mCoopSmokeSnapshotsReceived, mCoopSmokeSnapshotsApplied,
		mCoopSmokeScenarioRuntime.HasAppliedAction() ? "true" : "false",
		mCoopSmokeScenarioRuntime.GetPostActionSamples());
	if (theReason && theReason[0])
		aJson += ",\"error\":\"" + CoopSmokeJsonEscape(theReason) + "\"";
	aJson += "}\n";

	std::string aTempPath = mCoopSmokeManifest.mResultPath + ".tmp." +
		std::to_string((unsigned)GetCurrentProcessId());
	HANDLE aFile = CreateFileA(aTempPath.c_str(), GENERIC_WRITE, 0, nullptr,
		CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
	if (aFile == INVALID_HANDLE_VALUE)
	{
		LawnNet::NetTrace("SMOKE_RESULT write failed stage=create error=%lu path=%s",
			GetLastError(), aTempPath.c_str());
		return false;
	}

	DWORD aWritten = 0;
	bool aWriteOk = WriteFile(aFile, aJson.data(), (DWORD)aJson.size(), &aWritten, nullptr) != FALSE &&
		aWritten == aJson.size() && FlushFileBuffers(aFile) != FALSE;
	DWORD aWriteError = aWriteOk ? ERROR_SUCCESS : GetLastError();
	CloseHandle(aFile);
	if (!aWriteOk || !MoveFileExA(aTempPath.c_str(), mCoopSmokeManifest.mResultPath.c_str(),
		MOVEFILE_WRITE_THROUGH))
	{
		DWORD anError = aWriteOk ? GetLastError() : aWriteError;
		DeleteFileA(aTempPath.c_str());
		LawnNet::NetTrace("SMOKE_RESULT write failed stage=publish error=%lu path=%s",
			anError, mCoopSmokeManifest.mResultPath.c_str());
		return false;
	}

	LawnNet::NetTrace("SMOKE_RESULT status=%s role=%s sent=%u received=%u applied=%u path=%s",
		thePassed ? "pass" : "fail", aRole, mCoopSmokeSnapshotsSent,
		mCoopSmokeSnapshotsReceived, mCoopSmokeSnapshotsApplied,
		mCoopSmokeManifest.mResultPath.c_str());
	return true;
}

void LawnApp::CoopSmokeFail(const char* theReason)
{
	if (!CoopSmokeActive() || mCoopSmokeResultWritten)
		return;
	LawnNet::NetTrace("SMOKE_FAIL reason=%s", theReason ? theReason : "unknown");
	CoopSmokeWriteResult(false, theReason ? theReason : "unknown smoke failure");
	CloseRequestAsync();
}

void LawnApp::CoopSmokeStartLifecycle()
{
	if (!CoopSmokeActive() || mCoopSmokeLifecycleStarted || !mNetManager)
		return;
	mCoopSmokeLifecycleStarted = true;
	mCoopSmokeScenarioRuntime.Reset(mCoopSmokeManifest.mScenario);

	bool aStarted = false;
	const char* anOnlineEndpoint = getenv("PVZ_COOP_SMOKE_ONLINE_ENDPOINT");
	if (anOnlineEndpoint && *anOnlineEndpoint)
	{
		LawnNet::NetTraceOpen(mCoopSmokeManifest.IsHost() ? "host" : "client");
		aStarted = mNetManager->ConnectOnlineDirectory(anOnlineEndpoint);
		LawnNet::NetTrace("SMOKE_ONLINE_DIRECTORY_BEGIN role=%s accepted=%d",
			mCoopSmokeManifest.IsHost() ? "host" : "client", aStarted ? 1 : 0);
	}
	else if (mCoopSmokeManifest.IsHost())
		aStarted = mNetManager->StartHost(mCoopSmokeManifest.mPort);
	else if (mCoopSmokeManifest.IsClient())
		aStarted = mNetManager->StartClient(mCoopSmokeManifest.mEndpoint.c_str(),
			mCoopSmokeManifest.mPort);
	if (!aStarted)
	{
		CoopSmokeFail("failed to start smoke network lifecycle");
		return;
	}
	static const char aExpectedMusicLabel[] =
		"\xD0\x9C\xD1\x83\xD0\xB7\xD1\x8B\xD0\xBA\xD0\xB0"; // Музыка
	const std::string aMusicLabel = SexyStringToString(
		TodStringTranslate(_S("[OPTIONS_MUSIC_LABEL]")));
	if (aMusicLabel != aExpectedMusicLabel)
	{
		CoopSmokeFail("localized OPTIONS_MUSIC_LABEL failed the UTF-8 integration check");
		return;
	}
	LawnNet::NetTrace("SMOKE_LOCALIZATION_OK key=OPTIONS_MUSIC_LABEL utf8Bytes=%zu",
		aMusicLabel.size());

	const LawnNet::NetSmokeScenarioDefinition* aScenario =
		LawnNet::NetGetSmokeScenario(mCoopSmokeManifest.mScenario);
	LawnNet::NetTrace("SMOKE_LIFECYCLE role=%s scenario=%s manifest=%s timeoutMs=%u minimumSnapshots=%u skipIntro=%d",
		mCoopSmokeManifest.IsHost() ? "host" : "client",
		aScenario ? aScenario->mName : "invalid",
		mCoopSmokeManifest.mManifestPath.c_str(), mCoopSmokeManifest.mTimeoutMs,
		mCoopSmokeManifest.mMinimumSnapshots, mCoopSmokeManifest.mSkipIntro ? 1 : 0);
}

bool LawnApp::CoopSmokeUpdateUnicodeChat()
{
	const char* aVerify = getenv("PVZ_COOP_SMOKE_UNICODE_CHAT");
	if (!aVerify || strcmp(aVerify, "1") != 0)
		return true;
	if (!mNetManager || mNetManager->GetPlayerCount() != 2 ||
		(!mNetManager->IsHosting() && !mNetManager->IsConnectedToHost()))
		return false;

	static const char* aHostNick = "\xD0\xA5\xD0\xBE\xD1\x81\xD1\x82"; // Host, Russian
	static const char* aClientNick = "\xD0\x9A\xD0\xBB\xD0\xB8\xD0\xB5\xD0\xBD\xD1\x82"; // Client, Russian
	if (strcmp(mNetManager->GetPlayer(0).mNick, aHostNick) != 0 ||
		strcmp(mNetManager->GetPlayer(1).mNick, aClientNick) != 0)
	{
		CoopSmokeFail("Unicode nickname mismatch after lobby replication");
		return false;
	}

	if (!mCoopSmokeUnicodeChatSent)
	{
		const char* aLocalText = mNetManager->GetMySlot() == 0 ? aHostNick : aClientNick;
		if (!NetHandleChatKey(KeyCode::KEYCODE_RETURN) ||
			!NetHandleChatText(aLocalText) ||
			!NetHandleChatText(" ") || SexyStringToStringFast(mNetChatDraft) != std::string(aLocalText) + " " ||
			!NetHandleChatText("\xD1\x91") ||
			!NetHandleChatKey(KeyCode::KEYCODE_BACK) || SexyStringToStringFast(mNetChatDraft) != std::string(aLocalText) + " " ||
			!NetHandleChatKey(KeyCode::KEYCODE_BACK) || SexyStringToStringFast(mNetChatDraft) != aLocalText ||
			!NetHandleChatKey(KeyCode::KEYCODE_RETURN))
		{
			CoopSmokeFail("Enter chat input path rejected smoke message");
			return false;
		}
		mCoopSmokeUnicodeChatSent = true;
		LawnNet::NetTrace("SMOKE_CHAT_DRAFT role=%s space=1 cyrillicBackspace=1 literal=1",
			mCoopSmokeManifest.IsHost() ? "host" : "client");
		LawnNet::NetTrace("SMOKE_CHAT_ENTER role=%s slot=%d utf8Bytes=%zu",
			mCoopSmokeManifest.IsHost() ? "host" : "client",
			mNetManager->GetMySlot(), strlen(aLocalText));
	}

	bool aSawHost = false;
	bool aSawClient = false;
	for (size_t i = 0; i < mNetManager->mChatLines.size(); ++i)
	{
		const LawnNet::NetManager::ChatLine& aLine = mNetManager->mChatLines[i];
		aSawHost = aSawHost || (aLine.mSlot == 0 && aLine.mText == aHostNick);
		aSawClient = aSawClient || (aLine.mSlot == 1 && aLine.mText == aClientNick);
	}
	if (aSawHost && aSawClient && !mCoopSmokeUnicodeChatReceived)
	{
		mCoopSmokeUnicodeChatReceived = true;
		LawnNet::NetTrace("SMOKE_UTF8_NICKS role=%s hostBytes=%zu clientBytes=%zu valid=1",
			mCoopSmokeManifest.IsHost() ? "host" : "client", strlen(aHostNick), strlen(aClientNick));
		LawnNet::NetTrace("SMOKE_CHAT_BROADCAST role=%s host=1 client=1 lines=%zu",
			mCoopSmokeManifest.IsHost() ? "host" : "client", mNetManager->mChatLines.size());
	}
	return mCoopSmokeUnicodeChatReceived && mCoopSmokeUnicodeChatVisible &&
		mCoopSmokeUnicodeChatFaded;
}

void LawnApp::CoopSmokeUpdate()
{
	if (!CoopSmokeActive())
		return;
	// The harness deliberately keeps both peers alive until both result files
	// exist, then closes the client first.  Once a successful host has observed
	// that orderly peer departure it can terminate itself; this avoids relying
	// on Process.CloseMainWindow selecting the correct HWND after PvZ recreates
	// its presentation window for a different resolution.
	if (mCoopSmokeResultWritten)
	{
		if (mCoopSmokeManifest.IsHost() && mCoopSmokePeerReady && mNetManager &&
			mNetManager->IsHosting() && mNetManager->GetPlayerCount() < 2)
		{
			LawnNet::NetTrace("SMOKE_SHUTDOWN role=host reason=completed_client_departure");
			CloseRequestAsync();
		}
		return;
	}

	uint32_t aNow = GetTickCount();
	if (mCoopSmokeStartedTick == 0)
		mCoopSmokeStartedTick = aNow;
	if ((uint32_t)(aNow - mCoopSmokeStartedTick) >= mCoopSmokeManifest.mTimeoutMs)
	{
		CoopSmokeFail("smoke lifecycle timeout");
		return;
	}
	if (!mCoopSmokeLifecycleStarted || !mNetManager)
		return;
	if (!CoopSmokeUpdateOnline())
		return;
	const char* aSpectatorRole = getenv("PVZ_COOP_SMOKE_SPECTATOR_ROLE");
	if (aSpectatorRole && std::string(aSpectatorRole) == "spectator")
	{
		CoopSmokeUpdateSpectator();
		return;
	}
	const bool aSpectatorReady = CoopSmokeUpdateSpectator();
	const bool aFirstAdventureReady = CoopSmokeUpdateFirstAdventure();
	const bool aInteractionsReady = CoopSmokeUpdateInteractions();
	const bool aModesReady = CoopSmokeUpdateModes();
	const bool aUnicodeChatReady = CoopSmokeUpdateUnicodeChat();
	if (mCoopSmokeResultWritten)
		return;
	const LawnNet::NetSmokeScenarioDefinition* aScenario =
		LawnNet::NetGetSmokeScenario(mCoopSmokeManifest.mScenario);
	if (!aScenario)
	{
		CoopSmokeFail("resolved smoke scenario disappeared at runtime");
		return;
	}
	if (mSeedChooserScreen && mSeedChooserScreen->mMouseVisible && mSeedChooserScreen->mCoopChooserActive &&
		mBoard && mBoard->mCutScene && mBoard->mCutScene->mSeedChoosing &&
		mSeedChooserScreen->CoopAllPicksComplete() && !mNetManager->IsMyReady())
	{
		mNetManager->SetMyReady(true);
		LawnNet::NetTrace("SMOKE_CHOOSER_READY role=%s ready=%d eligible=%d",
			mCoopSmokeManifest.IsHost() ? "host" : "client",
			mNetManager->GetReadyCount(), mNetManager->GetReadyEligibleCount());
	}
	if (NetIsChoosingPvpTeams() && !mNetManager->IsMyReady())
		mNetManager->SetMyReady(true);

	if (mCoopSmokeManifest.IsHost())
	{
		if (!mNetManager->IsHosting())
		{
			CoopSmokeFail("host lifecycle stopped unexpectedly");
			return;
		}

		int aPlayerCount = mNetManager->GetPlayerCount();
		const char* aSpectatorRun = getenv("PVZ_COOP_SMOKE_SPECTATOR");
		const int aMaximumPlayers = aSpectatorRun && std::string(aSpectatorRun) == "1" ? 3 : 2;
		if (aPlayerCount > aMaximumPlayers)
		{
			CoopSmokeFail("smoke host accepted more than one client");
			return;
		}
		if (aPlayerCount >= 2)
			mCoopSmokePeerReady = true;
		else if (mCoopSmokePeerReady)
		{
			CoopSmokeFail("authenticated smoke client disconnected");
			return;
		}

		if (mCoopSmokePeerReady && !mCoopSmokeGameStarted)
		{
			if (mGameScene != GameScenes::SCENE_MENU || !mGameSelector || mBoard)
			{
				CoopSmokeFail("host was not in a clean selector scene before deterministic start");
				return;
			}
			mCoopSmokeGameStarted = true;
			LawnNet::NetPrepareSmokeScenarioLobby(mCoopSmokeManifest.mScenario, mNetManager);
			if (mPlayerInfo)
				mPlayerInfo->SetLevel(mCoopSmokeManifest.mLevel);
			LawnNet::NetTrace("SMOKE_START role=host scenario=%s mode=%s gameMode=%u level=%d seed=%u skipIntro=%d",
				aScenario->mName, mCoopSmokeManifest.mMode.c_str(),
				(unsigned)mCoopSmokeManifest.mGameMode, mCoopSmokeManifest.mLevel,
				mCoopSmokeManifest.mSeed, mCoopSmokeManifest.mSkipIntro ? 1 : 0);
			KillGameSelector();
			const GameMode aGameMode = CoopSmokeFirstAdventureEnabled() ?
				GAMEMODE_INTRO : (GameMode)mCoopSmokeManifest.mGameMode;
			if (aGameMode == GameMode::GAMEMODE_ADVENTURE)
				PreNewGame(aGameMode, false, mCoopSmokeManifest.mLevel);
			else
				PreNewGame(aGameMode, false);
			if (NetIsChoosingPvpTeams())
				return;
			if (!mBoard || !mBoard->mCutScene)
			{
				CoopSmokeFail("deterministic host board creation failed");
				return;
			}
			if (mCoopSmokeManifest.mSkipIntro)
				mBoard->mCutScene->CancelIntro();
		}

		std::string aScenarioFailure;
		if (CoopSmokeFirstAdventureRunBaseline() && !mCoopSmokeScenarioRuntime.Update(this, mCoopSmokeManifest,
			mCoopSmokeSnapshotsSent, aScenarioFailure))
		{
			CoopSmokeFail(aScenarioFailure.c_str());
			return;
		}
                const bool aSmokeTerminalScene =
                        mCoopSmokeManifest.mScenario == LawnNet::NETSMOKE_SCENARIO_ADVENTURE_8_SOAK &&
                        mGameScene == GameScenes::SCENE_ZOMBIES_WON;
		const bool aSmokeVictoryScene =
			mCoopSmokeManifest.mScenario == LawnNet::NETSMOKE_SCENARIO_ADVENTURE_8_VICTORY &&
			mGameScene == GameScenes::SCENE_AWARD && mAwardScreen && !mBoard;
                if (mCoopSmokeGameStarted &&
                        (aSmokeVictoryScene || (mBoard &&
			 (mGameScene == GameScenes::SCENE_PLAYING || aSmokeTerminalScene))) &&
			mNetManager->GetPlayerCount() == 2 &&
			mCoopSmokeSnapshotsSent >= mCoopSmokeManifest.mMinimumSnapshots &&
			mCoopSmokeScenarioRuntime.IsComplete(mCoopSmokeManifest.mMinimumSnapshots) &&
			aUnicodeChatReady && aInteractionsReady && aFirstAdventureReady && aSpectatorReady && aModesReady)
		{
			bool aPublished = CoopSmokeWriteResult(true, nullptr);
			if (!aPublished || mCoopSmokeManifest.mCloseOnResult)
				CloseRequestAsync();
		}
	}
	else if (mCoopSmokeManifest.IsClient())
	{
		if (mNetManager->IsConnectedToHost())
			mCoopSmokeEverConnected = true;
		if (mCoopSmokeEverConnected && !mNetManager->IsClient())
		{
			CoopSmokeFail("client network lifecycle stopped unexpectedly");
			return;
		}
		if (!mCoopSmokeEverConnected &&
			strncmp(mNetManager->GetConnectStatus(), "Failed", 6) == 0)
		{
			CoopSmokeFail("client failed to connect to smoke host");
			return;
		}

		std::string aScenarioFailure;
		if (CoopSmokeFirstAdventureRunBaseline() && !mCoopSmokeScenarioRuntime.Update(this, mCoopSmokeManifest,
			mCoopSmokeSnapshotsApplied, aScenarioFailure))
		{
			CoopSmokeFail(aScenarioFailure.c_str());
			return;
		}
                const bool aSmokeTerminalScene =
                        mCoopSmokeManifest.mScenario == LawnNet::NETSMOKE_SCENARIO_ADVENTURE_8_SOAK &&
                        mGameScene == GameScenes::SCENE_ZOMBIES_WON;
		const bool aSmokeVictoryScene =
			mCoopSmokeManifest.mScenario == LawnNet::NETSMOKE_SCENARIO_ADVENTURE_8_VICTORY &&
			mGameScene == GameScenes::SCENE_AWARD && mAwardScreen && !mBoard;
                if ((aSmokeVictoryScene || (mBoard &&
			(mGameScene == GameScenes::SCENE_PLAYING || aSmokeTerminalScene))) &&
			mCoopSmokeSnapshotsApplied >= mCoopSmokeManifest.mMinimumSnapshots &&
			mCoopSmokeScenarioRuntime.IsComplete(mCoopSmokeManifest.mMinimumSnapshots) &&
			aUnicodeChatReady && aInteractionsReady && aFirstAdventureReady && aSpectatorReady && aModesReady)
		{
			bool aPublished = CoopSmokeWriteResult(true, nullptr);
			if (!aPublished || mCoopSmokeManifest.mCloseOnResult)
				CloseRequestAsync();
		}
	}
}

void LawnApp::CoopSmokeOnSnapshotTx(uint32_t theHash, size_t theBytes)
{
	if (!CoopSmokeActive() || !mCoopSmokeManifest.IsHost() || mCoopSmokeResultWritten ||
		!mBoard || mGameScene != GameScenes::SCENE_PLAYING)
		return;
	++mCoopSmokeSnapshotsSent;
	LawnNet::NetTrace("SMOKE_SNAPSHOT_TX index=%u bytes=%zu hash=%08x",
		mCoopSmokeSnapshotsSent, theBytes, theHash);
	mCoopSmokeScenarioRuntime.OnSnapshotTx(theHash, theBytes);
	if (mBoard->mLevelComplete)
		mCoopSmokeScenarioRuntime.OnLevelCompleteSnapshot(true, mBoard->mBoardFadeOutCounter);
}

void LawnApp::CoopSmokeOnSnapshotRx(uint32_t theHash, size_t theBytes)
{
	if (!CoopSmokeActive() || !mCoopSmokeManifest.IsClient() || mCoopSmokeResultWritten ||
		!mBoard || mGameScene != GameScenes::SCENE_PLAYING)
		return;
	++mCoopSmokeSnapshotsReceived;
	LawnNet::NetTrace("SMOKE_SNAPSHOT_RX index=%u bytes=%zu hash=%08x",
		mCoopSmokeSnapshotsReceived, theBytes, theHash);
}

void LawnApp::CoopSmokeOnSnapshotApplied(uint32_t theHash, size_t theBytes)
{
	if (!CoopSmokeActive() || !mCoopSmokeManifest.IsClient() || mCoopSmokeResultWritten ||
		!mBoard || mGameScene != GameScenes::SCENE_PLAYING)
		return;
	++mCoopSmokeSnapshotsApplied;
	LawnNet::NetTrace("SMOKE_SNAPSHOT_APPLIED index=%u bytes=%zu hash=%08x",
		mCoopSmokeSnapshotsApplied, theBytes, theHash);
	mCoopSmokeScenarioRuntime.OnSnapshotApplied(theHash, theBytes);
	if (mBoard->mLevelComplete)
		mCoopSmokeScenarioRuntime.OnLevelCompleteSnapshot(false, mBoard->mBoardFadeOutCounter);
}

void LawnApp::NetInit()
{
	mNetManager = new LawnNet::NetManager();
	mNetManager->Init(this);
}

bool LawnApp::NetActive() const
{
	return mNetManager != nullptr && mNetManager->IsActive();
}

bool LawnApp::NetIsHost() const
{
	return mNetManager != nullptr && mNetManager->IsHosting();
}

bool LawnApp::NetIsClient() const
{
	return mNetManager != nullptr && mNetManager->IsClient();
}

void LawnApp::NetShutdown()
{
	if (mNetLobbyPanel)
	{
		if (mWidgetManager)
			mWidgetManager->RemoveWidget(mNetLobbyPanel);
		delete mNetLobbyPanel;
		mNetLobbyPanel = nullptr;
	}

	if (mNetManager)
	{
		mNetManager->Shutdown();
		delete mNetManager;
		mNetManager = nullptr;
	}
}

void LawnApp::NetHostStartGame(GameMode theGameMode, int theLevel)
{
	if (!mNetManager || !mNetManager->IsHosting())
		return;

	// PreNewGame performs the broadcast + fresh-start logic
	if (theGameMode == GameMode::GAMEMODE_ADVENTURE)
		PreNewGame(theGameMode, false, theLevel);
	else
		PreNewGame(theGameMode, false);
}

void LawnApp::NetOnStartGame(LawnNet::NetBuffer& thePayload)
{
	if (CoopSmokeActive())
	{
		LawnNet::NetBuffer aCheck = thePayload;
		uint8_t aMode = 0, aPlayerCount = 0, aPathLength = 0;
		int32_t aLevel = 0;
		uint32_t aSeed = 0;
		bool aValid = aCheck.ReadU8(&aMode) && aCheck.ReadI32(&aLevel) &&
			aCheck.ReadU8(&aPlayerCount) && aCheck.ReadU32(&aSeed) &&
			aCheck.ReadU8(&aPathLength);
                for (uint8_t i = 0; aValid && i < aPathLength; ++i)
                {
                        uint8_t aCell = 0;
                        aValid = aCheck.ReadU8(&aCell);
                }
		const bool aTowerDefenseStart =
			mCoopSmokeManifest.mGameMode ==
				(uint8_t)GameMode::GAMEMODE_COOP_TOWER_DEFENSE;
		const bool aPathShapeValid = aTowerDefenseStart ?
			(aPathLength >= 4 && (aPathLength & 1U) == 0U) :
			(aPathLength == 0);
                aValid = aValid && aCheck.Remaining() == 0 &&
                        ((aMode == mCoopSmokeManifest.mGameMode && aLevel == mCoopSmokeManifest.mLevel) ||
			 CoopSmokeFirstAdventureAllowsStart(aMode, aLevel)) && aSeed == mCoopSmokeManifest.mSeed &&
			(aPlayerCount == 2 || (getenv("PVZ_COOP_SMOKE_SPECTATOR_ROLE") &&
				strcmp(getenv("PVZ_COOP_SMOKE_SPECTATOR_ROLE"),"spectator") == 0 && aPlayerCount == 3)) && aPathShapeValid;
		if (!aValid)
		{
			LawnNet::NetTrace("SMOKE START_GAME malformed/mismatch bytes=%zu mode=%u level=%d players=%u seed=%u path=%u trailing=%zu",
				thePayload.Size(), aMode, aLevel, aPlayerCount, aSeed, aPathLength,
				aCheck.Remaining());
			CoopSmokeFail("malformed or mismatched deterministic START_GAME");
			return;
		}
	}
	GameMode aMode = (GameMode)thePayload.ReadU8Or();
	int aLevel = thePayload.ReadI32Or();
	/*uint8_t aPlayerCount =*/ thePayload.ReadU8Or();
	uint32_t aBoardSeed = thePayload.ReadU32Or();
	uint8_t aPathLen = thePayload.ReadU8Or();

	std::vector<uint8_t> aPath;
	for (int i = 0; i < aPathLen; i++)
		aPath.push_back(thePayload.ReadU8Or());
	LawnNet::NetTrace("client START_GAME decoded mode=%d level=%d seed=%u tdPathCells=%u size=%dx%d selector=%p challenge=%p board=%p",
		(int)aMode, aLevel, aBoardSeed, aPathLen, mWidth, mHeight,
		(void*)mGameSelector, (void*)mChallengeScreen, (void*)mBoard);

	if (mGameSelector)
		KillGameSelector();
	NetCloseTransientScreens();

	mNetBoardSeed = aBoardSeed;
	mGameMode = aMode;
	mNetCoopGameOverHandled = false;

	// client always runs with the host's adventure level
	if (mPlayerInfo && aMode == GameMode::GAMEMODE_ADVENTURE)
		mPlayerInfo->mLevel = aLevel;

	// picked up by Board::InitLevel (the board is recreated below)
	if (mNetManager)
		mNetManager->mStartTDPath = aPath;

	// Follow the exact same initialization/resource-loading path as a local
	// game.  The former hand-written subset skipped NewGame state and left
	// some lazily loaded board resources unprepared on clients.
	NewGame();
	if (mBoard && mBoard->mCutScene && !mBoard->mCutScene->mPreloaded)
	{
		LawnNet::NetTrace("client preloading board resources immediately after NewGame");
		mBoard->mCutScene->PreloadResources();
	}
	LawnNet::NetTrace("client START_GAME initialized board=%p scene=%d firstSelector=%d background=%d seedChooser=%p lobbyVisible=%d",
		(void*)mBoard, (int)mGameScene, mFirstTimeGameSelector ? 1 : 0,
		mBoard ? (int)mBoard->mBackground : -1, (void*)mSeedChooserScreen,
		mNetLobbyPanel && mNetLobbyPanel->mVisible ? 1 : 0);
}

void LawnApp::NetOnHostLost()
{
	if (!mNetManager)
		return;
	const std::string aDisconnectReason = mNetManager->GetDisconnectReason();
	if (CoopSmokeActive() && !CoopSmokeExpectedSpectatorKick(aDisconnectReason))
		CoopSmokeFail("connection to smoke host was lost");

	// Tear down the board while NetIsClient() and the profile-save guard still
	// identify this as a read-only client.  Zen Garden disposal must never run
	// host-only feeding/coin/profile mutations against snapshot ghosts.
	KillBoard();
	if (mSeedChooserScreen)
		KillSeedChooserScreen();
	mNetManager->Disconnect();
	NetRestoreLocalProfile();
	ShowGameSelector();

	DoDialog(Dialogs::DIALOG_INFO, true, _S("Co-op"),
		aDisconnectReason.empty() ? _S("Connection to the host was lost.") : StringToSexyString(aDisconnectReason),
		_S("[DIALOG_BUTTON_OK]"),
		Dialog::BUTTONS_FOOTER);
}

void LawnApp::NetOnBackToMenu()
{
	const bool aNeedsMenuTransition = mBoard || mGameScene != GameScenes::SCENE_MENU;
	if (aNeedsMenuTransition)
	{
		KillBoard();
		if (mSeedChooserScreen)
			KillSeedChooserScreen();
	}
	if (aNeedsMenuTransition)
		ShowGameSelector();
}

void LawnApp::NetRestoreLocalProfile()
{
	if (!mNetBlockProfileSave && !mNetLocalProfileBackup)
		return;
	if (!mNetLocalProfileBackup)
	{
		// Never fail open: without the original data there is no safe profile to
		// persist.  Retaining the guard protects every central save entry point.
		mNetBlockProfileSave = true;
		LawnNet::NetTrace("local profile restore failed: backup missing; save guard retained current=%p",
			(void*)mPlayerInfo);
		return;
	}

	PlayerInfo* aRestoreTarget = nullptr;
	if (mPlayerInfo && mPlayerInfo->mId == mNetLocalProfileBackup->mId)
		aRestoreTarget = mPlayerInfo;
	else if (mProfileMgr)
	{
		PlayerInfo* aNamedProfile = mProfileMgr->GetProfile(mNetLocalProfileBackup->mName);
		if (aNamedProfile && aNamedProfile->mId == mNetLocalProfileBackup->mId)
			aRestoreTarget = aNamedProfile;
	}

	if (!aRestoreTarget)
	{
		mNetBlockProfileSave = true;
		LawnNet::NetTrace("local profile restore failed: matching target unavailable id=%lu name=%s current=%p; backup and guard retained",
			(unsigned long)mNetLocalProfileBackup->mId,
			SexyStringToString(mNetLocalProfileBackup->mName).c_str(), (void*)mPlayerInfo);
		return;
	}

	*aRestoreTarget = *mNetLocalProfileBackup;
	// If profile selection changed while connected, make the restored local
	// object current again instead of leaving a host-mirrored pointer active.
	mPlayerInfo = aRestoreTarget;
	mPlayerLevelRef = aRestoreTarget->GetLevel();
	LawnNet::NetTrace("local profile restored after client session id=%lu level=%d coins=%d",
		(unsigned long)aRestoreTarget->mId, aRestoreTarget->mLevel, aRestoreTarget->mCoins);

	delete mNetLocalProfileBackup;
	mNetLocalProfileBackup = nullptr;
	mNetBlockProfileSave = false;
	mNetHostUnlockAllMinigames = false;
}

bool LawnApp::NetIsChoosingPvpTeams() const
{
	return const_cast<LawnApp*>(this)->GetDialog(Dialogs::DIALOG_COOP_TEAMS) != nullptr;
}

void LawnApp::NetShowOnlineBrowser()
{
	if (!mNetManager || mBoard)
		return;
	if (Dialog* aDialog = GetDialog(Dialogs::DIALOG_COOP_ONLINE))
	{
		mWidgetManager->SetFocus(aDialog);
		return;
	}
	NetOnlineDialog* aDialog = new NetOnlineDialog(this);
	AddDialog(aDialog);
	mWidgetManager->SetFocus(aDialog);
}

void LawnApp::NetShowPvpTeamSelection(int theLevel)
{
	if (!mNetManager || !mNetManager->IsActive() || NetIsChoosingPvpTeams() || mBoard)
		return;
	mNetPendingPvpLevel = theLevel;
	if (NetIsHost())
	{
		mNetManager->ResetReadyState();
		mNetManager->BroadcastSceneCmd(LawnNet::SCENECMD_TEAM_SELECT, 0);
	}
	CoopTeamDialog* aDialog = new CoopTeamDialog(this);
	AddDialog(aDialog);
	mWidgetManager->SetFocus(aDialog);
	LawnNet::NetTrace("PVP_TEAM_SELECT opened role=%s level=%d",
		NetIsHost() ? "host" : "client", theLevel);
}

void LawnApp::NetClosePvpTeamSelection(bool theCancel)
{
	if (!NetIsChoosingPvpTeams())
		return;
	if (NetIsHost())
	{
		mNetManager->BroadcastSceneCmd(LawnNet::SCENECMD_CLOSE_TEAM_SELECT, 0);
		if (theCancel)
			mNetManager->ResetReadyState();
	}
	KillDialog(Dialogs::DIALOG_COOP_TEAMS);
	if (theCancel && !mGameSelector && !mChallengeScreen)
		ShowGameSelector();
}

void LawnApp::NetTickPvpTeamSelection()
{
	if (!NetIsHost() || !NetIsChoosingPvpTeams() ||
		!mNetManager->AreAllPlayersReady() ||
		mNetManager->GetTeamCount(LawnNet::NETTEAM_PLANTS) == 0 ||
		mNetManager->GetTeamCount(LawnNet::NETTEAM_ZOMBIES) == 0)
		return;
	const int aLevel = mNetPendingPvpLevel;
	NetClosePvpTeamSelection(false);
	mNetManager->ResetReadyState();
	mNetPvpTeamsConfirmed = true;
	if (aLevel >= 0)
		PreNewGame(GAMEMODE_COOP_PVP, false, aLevel);
	else
		PreNewGame(GAMEMODE_COOP_PVP, false);
	mNetPvpTeamsConfirmed = false;
	if (CoopSmokeActive() && mCoopSmokeManifest.mSkipIntro && mBoard && mBoard->mCutScene)
		mBoard->mCutScene->CancelIntro();
	LawnNet::NetTrace("PVP_TEAM_SELECT committed board=%p", (void*)mBoard);
}

void LawnApp::NetCloseTransientScreens()
{
	KillDialog(Dialogs::DIALOG_COOP_TEAMS);
	KillAwardScreen();
	KillChallengeScreen();
	KillSeedChooserScreen();
	KillStoreScreen();
	KillAlmanacDialog();
	KillNewOptionsDialog();
}

bool LawnApp::NetOnSceneCmd(uint8_t aCmd, uint32_t aParam)
{
	switch (aCmd)
	{
	case LawnNet::SCENECMD_SHOW_GAME_SELECTOR:
		KillBoard();
		ShowGameSelector();
		return true;
	case LawnNet::SCENECMD_SHOW_CHALLENGE_PAGE:
		if (mGameSelector)
			KillGameSelector();
		ShowChallengeScreen((ChallengePage)aParam);
		return true;
	case LawnNet::SCENECMD_SHOW_STORE:
		if (!GetDialog(Dialogs::DIALOG_STORE))
		{
			ShowStoreScreen();
			return true;
		}
		return false;
	case LawnNet::SCENECMD_SHOW_AWARD:
		// The host destroys its completed board before broadcasting the award.
		// Mirror that lifecycle boundary so the client cannot retain a live game
		// underneath the award widget (and cannot keep processing stale snapshots).
		if (mAwardScreen && mGameScene == GameScenes::SCENE_AWARD)
			return true;
		KillBoard();
		ShowAwardScreen((AwardType)aParam, false);
		return true;
	case LawnNet::SCENECMD_SHOW_SEED_CHOOSER:
		if (mBoard)
		{
			ShowSeedChooserScreen();
			return true;
		}
		return false;
	case LawnNet::SCENECMD_KILL_BOARD:
		KillBoard();
		return true;
	case LawnNet::SCENECMD_CLOSE_AWARD:
		KillAwardScreen();
		return true;
	case LawnNet::SCENECMD_CLOSE_CHALLENGE:
		KillChallengeScreen();
		return true;
	case LawnNet::SCENECMD_CLOSE_STORE:
		KillStoreScreen();
		return true;
	case LawnNet::SCENECMD_STORE_PAGE:
		if (StoreScreen* aStore = (StoreScreen*)GetDialog(Dialogs::DIALOG_STORE))
		{
			aStore->NetSetPage((StorePages)aParam);
			return true;
		}
		return false;
	case LawnNet::SCENECMD_TEAM_SELECT:
		NetShowPvpTeamSelection();
		return true;
	case LawnNet::SCENECMD_CLOSE_TEAM_SELECT:
		NetClosePvpTeamSelection(false);
		return true;
	case LawnNet::SCENECMD_ADVENTURE_START_FX:
		if (!NetIsClient() || mGameScene != GameScenes::SCENE_MENU || !mGameSelector ||
			mBoard)
			return false;
		return mGameSelector->StartAdventurePresentation(false);
	default:
		return false;
	}
}

bool LawnApp::NetUnlockAllMinigames() const
{
	return NetIsClient() ? mNetHostUnlockAllMinigames :
		(mNetManager && mNetManager->mConfig.mUnlockAllMinigames);
}

void LawnApp::NetBroadcastProfile(bool theForce)
{
	if (!mNetManager || !mNetManager->IsHosting() || !mPlayerInfo)
		return;

	LawnNet::NetBuffer aMsg;
	PlayerInfo* aP = mPlayerInfo;
	aMsg.WriteI32(aP->mLevel);
	aMsg.WriteI32(aP->mCoins);
	aMsg.WriteI32(aP->mFinishedAdventure);
	aMsg.WriteI32(aP->mHasUnlockedMinigames);
	aMsg.WriteI32(aP->mHasUnlockedPuzzleMode);
	aMsg.WriteI32(aP->mHasUnlockedSurvivalMode);
	for (int i = 0; i < 80; i++)
		aMsg.WriteI32((int32_t)aP->mPurchases[i]);
	for (int i = 0; i < 100; i++)
		aMsg.WriteI32(aP->mChallengeRecords[i]);
	std::string aName;
	LawnNet::NetNormalizeUtf8(SexyStringToStringFast(aP->mName), 255, 64, true, aName);
	aMsg.WriteString(aName.c_str());
	aMsg.WriteU8(NetUnlockAllMinigames() ? 1 : 0);
	const uint32_t aHash = LawnNet::NetFnv1a32(aMsg.Data(), aMsg.Size());
	if (!theForce && aHash == mNetLastProfileHash)
		return;
	mNetLastProfileHash = aHash;
	mNetManager->BroadcastMessage(LawnNet::NETMSG_PROFILE_SYNC, aMsg);
}

void LawnApp::NetOnProfileSync(LawnNet::NetBuffer& thePayload)
{
	LawnNet::NetBuffer aCandidate = thePayload;
	if (!NetIsClient())
	{
		LawnNet::NetTrace("PROFILE_SYNC rejected: receiver is not a network client");
		return;
	}

	if (!mPlayerInfo)
	{
		LawnNet::NetTrace("PROFILE_SYNC rejected: player profile is unavailable");
		return;
	}

	static const size_t PROFILE_SYNC_BYTES = (6 + 80 + 100) * sizeof(int32_t);
	if (aCandidate.Remaining() < PROFILE_SYNC_BYTES + 2 ||
		aCandidate.Remaining() > PROFILE_SYNC_BYTES + 257)
	{
		LawnNet::NetTrace("PROFILE_SYNC rejected: expected=%zu bytes actual=%zu",
			PROFILE_SYNC_BYTES, aCandidate.Remaining());
		return;
	}

	int32_t aLevel = aCandidate.ReadI32Or();
	int32_t aCoins = aCandidate.ReadI32Or();
	int32_t aFinishedAdventure = aCandidate.ReadI32Or();
	int32_t aHasUnlockedMinigames = aCandidate.ReadI32Or();
	int32_t aHasUnlockedPuzzleMode = aCandidate.ReadI32Or();
	int32_t aHasUnlockedSurvivalMode = aCandidate.ReadI32Or();
	int32_t aPurchases[80];
	int32_t aChallengeRecords[100];
	for (int i = 0; i < 80; i++)
		aPurchases[i] = aCandidate.ReadI32Or();
	for (int i = 0; i < 100; i++)
		aChallengeRecords[i] = aCandidate.ReadI32Or();
	char aProfileName[256] = {};
	uint8_t anUnlockAll = 0;
	std::string aNormalizedName;
	if (!aCandidate.ReadString(aProfileName, sizeof(aProfileName)) ||
		!aCandidate.ReadU8(&anUnlockAll) || anUnlockAll > 1 ||
		!LawnNet::NetNormalizeUtf8(aProfileName, 255, 64, true, aNormalizedName))
	{
		LawnNet::NetTrace("PROFILE_SYNC rejected: malformed profile identity");
		return;
	}

	if (aCandidate.Remaining() != 0 || aLevel < 0 || aLevel > FINAL_LEVEL)
	{
		LawnNet::NetTrace("PROFILE_SYNC rejected: level=%d trailing=%zu", aLevel, aCandidate.Remaining());
		return;
	}

	if (!mNetBlockProfileSave)
	{
		delete mNetLocalProfileBackup;
		mNetLocalProfileBackup = new PlayerInfo(*mPlayerInfo);
		mNetBlockProfileSave = true;
		LawnNet::NetTrace("local profile backed up before host sync id=%lu level=%d coins=%d",
			(unsigned long)mPlayerInfo->mId, mPlayerInfo->mLevel, mPlayerInfo->mCoins);
	}
	else if (!mNetLocalProfileBackup)
	{
		LawnNet::NetTrace("PROFILE_SYNC rejected: save guard is active but local profile backup is unavailable");
		return;
	}
	PlayerInfo* aP = mPlayerInfo;
	aP->mName = StringToSexyString(aNormalizedName);
	mNetHostUnlockAllMinigames = anUnlockAll != 0;
	aP->mLevel = aLevel;
	aP->mCoins = aCoins;
	aP->mFinishedAdventure = aFinishedAdventure;
	aP->mHasUnlockedMinigames = aHasUnlockedMinigames;
	aP->mHasUnlockedPuzzleMode = aHasUnlockedPuzzleMode;
	aP->mHasUnlockedSurvivalMode = aHasUnlockedSurvivalMode;
	for (int i = 0; i < 80; i++)
		aP->mPurchases[i] = aPurchases[i];
	for (int i = 0; i < 100; i++)
		aP->mChallengeRecords[i] = aChallengeRecords[i];
	mPlayerLevelRef = aP->GetLevel();
	if (mGameSelector)
		mGameSelector->SyncProfile(false);
	if (mChallengeScreen)
		mChallengeScreen->UpdateButtons();
	if (mWidgetManager)
		mWidgetManager->MarkAllDirty();
	thePayload.mReadPos = aCandidate.mReadPos;
	LawnNet::NetTrace("PROFILE_SYNC applied level=%d coins=%d finishedAdventure=%d unlocks=(%d,%d,%d)",
		aP->mLevel, aP->mCoins, aP->mFinishedAdventure,
		aP->mHasUnlockedMinigames, aP->mHasUnlockedPuzzleMode, aP->mHasUnlockedSurvivalMode);
}

void LawnApp::NetHandleRemoteInputs()
{
	if (!mNetManager || !mBoard || mNetManager->GetInputPhase() != LawnNet::NETINPUT_PLAYING)
		return;

	Board* const aBatchBoard = mBoard;
	const uint32_t aBatchEpoch = mNetManager->GetInputEpoch();
	for (int aProcessed = 0; aProcessed < LawnNet::NET_INPUT_APPLY_BUDGET; aProcessed++)
	{
		LawnNet::NetRemoteInput anInput;
		if (!mNetManager->PopPendingInput(anInput))
			break;

		if (mBoard != aBatchBoard || mNetManager->GetInputEpoch() != aBatchEpoch ||
			mNetManager->GetInputPhase() != LawnNet::NETINPUT_PLAYING ||
			mGameScene != GameScenes::SCENE_PLAYING || mSeedChooserScreen ||
			GetDialogCount() > 0 || aBatchBoard->mPaused)
		{
			LawnNet::NetTrace("mouse apply aborted reason=context boardNow=%p boardBatch=%p epochNow=%u epochBatch=%u phase=%u scene=%d chooser=%p dialogs=%d paused=%d",
				(void*)mBoard, (void*)aBatchBoard, mNetManager->GetInputEpoch(), aBatchEpoch,
				(unsigned)mNetManager->GetInputPhase(), (int)mGameScene,
				(void*)mSeedChooserScreen, GetDialogCount(), aBatchBoard->mPaused ? 1 : 0);
			mNetManager->PurgePendingInputs("APPLY_CONTEXT_CHANGED");
			break;
		}
		if (anInput.mEpoch != aBatchEpoch)
		{
			LawnNet::NetTrace("mouse apply dropped slot=%u reason=stale_epoch packet=%u current=%u seq=%u",
				anInput.mSlot, anInput.mEpoch, aBatchEpoch, anInput.mSequence);
			continue;
		}
		if (anInput.mSlot == 0 || anInput.mSlot >= LawnNet::NET_MAX_PLAYERS ||
			!mNetManager->IsPlayerActive(anInput.mSlot) || !aBatchBoard->mNetPlayerCursors[anInput.mSlot])
			continue;

		// A packet may move or the conveyor may compact after the receive phase
		// validates a click but before the main thread applies it.  Resolve the
		// stable host id again at the actual mutation point; never let a queued
		// coordinate select a different identical card that inherited its slot.
		if (anInput.mAction == LawnNet::NETINPUT_MOUSE_DOWN && anInput.mSeedPacketId != 0)
		{
			SeedBank* aBank = aBatchBoard->mSeedBank;
			SeedPacket* aResolvedPacket = nullptr;
			int aMatchCount = 0;
			if (aBank && aBank->mNumPackets >= 0 && aBank->mNumPackets <= SEEDBANK_CAPACITY)
			{
				for (int i = 0; i < aBank->mNumPackets; i++)
				{
					SeedPacket& aCandidate = aBank->mSeedPackets[i];
					if (aCandidate.mNetId == anInput.mSeedPacketId &&
						(uint8_t)aCandidate.mPacketType == anInput.mSeedType &&
						(uint8_t)aCandidate.mImitaterType == anInput.mSeedImitater &&
						aCandidate.mNetOwnerSlot == anInput.mSeedOwner)
					{
						aResolvedPacket = &aCandidate;
						aMatchCount++;
					}
				}
			}
			if (aMatchCount != 1)
			{
				LawnNet::NetTrace("mouse apply dropped slot=%u reason=seed_packet_id_revalidate packetId=%u matches=%d seq=%u",
					anInput.mSlot, anInput.mSeedPacketId, aMatchCount, anInput.mSequence);
				continue;
			}
			const int aWideX = mDDInterface ? mDDInterface->mWideScreenOffsetX : 0;
			const int aWideY = mDDInterface ? mDDInterface->mWideScreenOffsetY : 0;
			anInput.mX = (int16_t)(aBank->mX + aResolvedPacket->mX +
				aResolvedPacket->mOffsetX + aResolvedPacket->mWidth / 2 + aWideX);
			anInput.mY = (int16_t)(aBank->mY + aResolvedPacket->mY +
				aResolvedPacket->mHeight / 2 + aWideY);
			LawnNet::NetTrace("mouse apply seed revalidated slot=%u packetId=%u index=%d local=(%d,%d) seq=%u",
				anInput.mSlot, anInput.mSeedPacketId, aResolvedPacket->mIndex,
				anInput.mX, anInput.mY, anInput.mSequence);
		}

		// Board input can synchronously open/close UI and, in challenge code,
		// even replace the board.  The guard restores host authority on every
		// return path, but never dereferences a board after LawnApp stopped owning
		// it (SafeDeleteWidget may already have queued its destruction).
		struct ScopedRemoteInput
		{
			LawnApp* mApp;
			LawnNet::NetManager* mManager;
			Board* mBoard;
			CursorObject* mOldCursor;
			int mOldActingSlot;
			ScopedRemoteInput(LawnApp* theApp, LawnNet::NetManager* theManager,
				Board* theBoard, int theSlot) :
				mApp(theApp), mManager(theManager), mBoard(theBoard),
				mOldCursor(theBoard->mCursorObject),
				mOldActingSlot(theManager->GetRawActingPlayerSlot())
			{
				mBoard->mCursorObject = mBoard->mNetPlayerCursors[theSlot];
				mManager->SetActingPlayerSlot(theSlot);
			}
			~ScopedRemoteInput()
			{
				if (mApp->mBoard == mBoard)
					mBoard->mCursorObject = mOldCursor;
				if (mApp->mNetManager == mManager)
					mManager->SetActingPlayerSlot(mOldActingSlot);
			}
		};
		{
			ScopedRemoteInput aGuard(this, mNetManager, aBatchBoard, anInput.mSlot);
			// Network mouse coordinates are canonical screen coordinates.  Widget
			// dispatch normally subtracts Board::mX/mY before entering Board::Mouse*,
			// so the direct authoritative path must do the same while the camera pans.
			const int aBoardLocalX = anInput.mX - aBatchBoard->mX;
			const int aBoardLocalY = anInput.mY - aBatchBoard->mY;
			LawnNet::NetTrace("mouse apply slot=%u epoch=%u seq=%u action=%u button=%d screen=(%d,%d) boardLocal=(%d,%d) boardPos=(%d,%d) board=%p",
				anInput.mSlot, anInput.mEpoch, anInput.mSequence, anInput.mAction,
				anInput.mButton, anInput.mX, anInput.mY, aBoardLocalX, aBoardLocalY,
				aBatchBoard->mX, aBatchBoard->mY, (void*)aBatchBoard);
			if (anInput.mAction == LawnNet::NETINPUT_MOUSE_DOWN)
			{
				aBatchBoard->MouseMove(aBoardLocalX, aBoardLocalY);
				aBatchBoard->MouseDown(aBoardLocalX, aBoardLocalY, anInput.mButton);
			}
			else if (anInput.mAction == LawnNet::NETINPUT_MOUSE_MOVE)
			{
				if (anInput.mButton != 0)
					aBatchBoard->MouseDrag(aBoardLocalX, aBoardLocalY);
				else
					aBatchBoard->MouseMove(aBoardLocalX, aBoardLocalY);
			}
			else
			{
				// Apply the release position while challenge capture is still alive.
				// This makes short/faster drags deterministic even if all coalesced
				// motion packets between DOWN and UP were intentionally throttled.
				aBatchBoard->MouseDrag(aBoardLocalX, aBoardLocalY);
				aBatchBoard->MouseUp(aBoardLocalX, aBoardLocalY, anInput.mButton);
			}
		}

		if (!mNetManager)
			break;
		if (mBoard != aBatchBoard || mNetManager->GetInputEpoch() != aBatchEpoch ||
			mNetManager->GetInputPhase() != LawnNet::NETINPUT_PLAYING ||
			mGameScene != GameScenes::SCENE_PLAYING || mSeedChooserScreen || GetDialogCount() > 0)
		{
			mNetManager->PurgePendingInputs("APPLY_REENTRANT_TRANSITION");
			break;
		}
		aBatchBoard->UpdateCursor();
	}
}

bool LawnApp::NetHandleChatKey(int theKey)
{
	const bool aCanChat = mNetManager &&
		((mNetManager->IsHosting() && mNetManager->GetPlayerCount() > 1) ||
			mNetManager->IsConnectedToHost());
	if (!aCanChat)
	{
		if (mNetChatTyping)
		{
			mNetChatTyping = false;
			mNetChatDraft.clear();
			SDL_StopTextInput(mSDLWindow);
		}
		return false;
	}
	if (!mNetChatTyping)
	{
		if (theKey != KeyCode::KEYCODE_RETURN)
			return false;
		mNetChatTyping = true;
		mNetChatDraft.clear();
		SDL_StartTextInput(mSDLWindow);
		return true;
	}

	if (theKey == KeyCode::KEYCODE_RETURN)
	{
		mNetManager->SendChat(SexyStringToStringFast(mNetChatDraft));
		mNetChatDraft.clear();
		mNetChatTyping = false;
		SDL_StopTextInput(mSDLWindow);
	}
	else if (theKey == KeyCode::KEYCODE_ESCAPE)
	{
		mNetChatDraft.clear();
		mNetChatTyping = false;
		SDL_StopTextInput(mSDLWindow);
	}
	else if (theKey == KeyCode::KEYCODE_BACK && !mNetChatDraft.empty())
	{
		// A byte-string build stores UTF-8: remove one full code point.
		if (sizeof(SexyChar) == 1)
		{
			size_t aStart = mNetChatDraft.size() - 1;
			while (aStart > 0 && ((unsigned char)mNetChatDraft[aStart] & 0xC0) == 0x80) --aStart;
			mNetChatDraft.erase(aStart);
			return true;
		}
		mNetChatDraft.pop_back();
		if (!mNetChatDraft.empty() && sizeof(SexyChar) == 2 &&
			mNetChatDraft.back() >= 0xD800 && mNetChatDraft.back() <= 0xDBFF)
			mNetChatDraft.pop_back();
	}
	return true;
}

bool LawnApp::NetHandleChatText(const char* theUtf8Text)
{
	if (!mNetChatTyping)
		return false;
	try
	{
		SexyString aCandidate = mNetChatDraft + StringToSexyString(std::string(theUtf8Text ? theUtf8Text : ""));
		std::string aUtf8 = SexyStringToStringFast(aCandidate);
		std::string aChecked;
		if (LawnNet::NetNormalizeUtf8(aUtf8, LawnNet::NET_MAX_CHAT_BYTES,
			LawnNet::NET_MAX_CHAT_CHARS, true, aChecked, false) && aChecked == aUtf8)
			mNetChatDraft = aCandidate;
	}
	catch (const std::range_error&)
	{
		LawnNet::NetTrace("chat input rejected malformed UTF-8");
	}
	return true;
}

void LawnApp::NetDrawChatOverlay()
{
	if (!mNetManager || !mNetManager->IsActive() || !mWidgetManager ||
		!mWidgetManager->mImage || !Sexy::FONT_BRIANNETOD12)
		return;
	const uint32_t aNow = GetTickCount();
	while (!mNetManager->mChatLines.empty() &&
		aNow - mNetManager->mChatLines.front().mReceivedTick >= 8000U)
		mNetManager->mChatLines.pop_front();

	Graphics g(mWidgetManager->mImage);
	g.ClearClipRect();
	const size_t aCount = mNetManager->mChatLines.size();
	const size_t aFirst = aCount > 6 ? aCount - 6 : 0;
	int aY = 350;
	for (size_t i = aFirst; i < aCount; ++i)
	{
		const LawnNet::NetManager::ChatLine& aLine = mNetManager->mChatLines[i];
		const uint32_t anAge = aNow - aLine.mReceivedTick;
		int anAlpha = anAge <= 6500U ? 230 : (int)((8000U - anAge) * 230U / 1500U);
		anAlpha = max(0, min(230, anAlpha));
		const bool aSystem = aLine.mSlot == LawnNet::NET_SHARED_OWNER;
		SexyString aText = aSystem ? StringToSexyString(aLine.mText) :
			_S("[") + StringToSexyString(aLine.mNick) + _S("] ") + StringToSexyString(aLine.mText);
		LawnNet::NetColor aColor = aSystem ? LawnNet::NetColor{255, 220, 130} :
			LawnNet::GetRelativePlayerColor(mNetManager->GetMySlot(), aLine.mSlot);
		g.SetColor(Color(0, 0, 0, anAlpha * 2 / 3));
		g.FillRect(8, aY - 15, 520, 20);
		// User content is literal: [nickname] must never become a localization key.
		Sexy::FONT_BRIANNETOD12->DrawString(&g,14,aY,aText,
			Color(aColor.r,aColor.g,aColor.b,anAlpha),g.mClipRect);
		aY += 22;
	}
	const char* aVerifyChat = getenv("PVZ_COOP_SMOKE_UNICODE_CHAT");
	if (aVerifyChat && strcmp(aVerifyChat, "1") == 0 &&
		mCoopSmokeUnicodeChatReceived && aCount >= 2)
	{
		uint32_t aYoungestAge = 0xFFFFFFFFU;
		for (size_t i = aFirst; i < aCount; ++i)
			aYoungestAge = min(aYoungestAge, aNow - mNetManager->mChatLines[i].mReceivedTick);
		if (!mCoopSmokeUnicodeChatVisible)
		{
			mCoopSmokeUnicodeChatVisible = true;
			LawnNet::NetTrace("SMOKE_CHAT_OVERLAY role=%s phase=visible lines=%zu alpha=230",
				mCoopSmokeManifest.IsHost() ? "host" : "client", aCount);
		}
		if (!mCoopSmokeUnicodeChatFaded && aYoungestAge > 6500U && aYoungestAge < 8000U)
		{
			const int anAlpha = (int)((8000U - aYoungestAge) * 230U / 1500U);
			mCoopSmokeUnicodeChatFaded = true;
			LawnNet::NetTrace("SMOKE_CHAT_OVERLAY role=%s phase=fade lines=%zu alpha=%d age=%u",
				mCoopSmokeManifest.IsHost() ? "host" : "client", aCount, anAlpha, aYoungestAge);
		}
	}
	if (mNetChatTyping)
	{
		g.SetColor(Color(0, 0, 0, 210));
		g.FillRect(8, 565, 520, 26);
		SexyString aDraft = _S("Chat: ") + mNetChatDraft;
		while (aDraft.size() > 6 && Sexy::FONT_BRIANNETOD12->StringWidth(aDraft) > 492)
		{
			if (sizeof(SexyChar) == 1)
			{
				size_t aNext = 7;
				while (aNext < aDraft.size() && ((unsigned char)aDraft[aNext] & 0xC0) == 0x80) ++aNext;
				aDraft.erase(6, aNext - 6);
			}
			else aDraft.erase(6, 1);
		}
		Sexy::FONT_BRIANNETOD12->DrawString(&g,14,583,aDraft,Color(255,255,255),g.mClipRect);
		if ((aNow / 500U) % 2 == 0)
		{
			g.SetColor(Color(255, 255, 255));
			g.FillRect(16 + Sexy::FONT_BRIANNETOD12->StringWidth(aDraft), 570, 1, 15);
		}
	}
}

void LawnApp::NetDrawCursorOverlay()
{
	if (!mNetManager || !mNetManager->IsActive() || !mWidgetManager || !mWidgetManager->mImage)
		return;
	Graphics anAnnotationLayer(mWidgetManager->mImage);
	anAnnotationLayer.ClearClipRect();
	mNetManager->DrawAnnotations(&anAnnotationLayer);
	// PvP intentionally hides remote cursors over the game board.  Keep the
	// lobby overlay interactive/visible even if an old board is still alive
	// while returning from a match.
	if (mBoard && mBoard->IsPvPMode() && (!mNetLobbyPanel || !mNetLobbyPanel->mVisible))
		return;

        // Cursor13 is only the NORMAL/unsupported fallback.  Stock shovel,
        // hammer and other tool cursors must remain visible even if that
        // optional overlay asset is missing.
        static bool aCursorLoadAttempted = false;

	Graphics g(mWidgetManager->mImage);
	g.ClearClipRect();
	for (int i = 0; i < LawnNet::NET_MAX_PLAYERS; i++)
	{
		if (i == mNetManager->GetMySlot() || !mNetManager->mRemoteCursors[i].mValid)
			continue;
		const LawnNet::NetPlayerInfo& aPlayer = mNetManager->GetPlayer(i);
		if (!aPlayer.mActive)
			continue;

		const LawnNet::NetRemoteCursor& aCursor = mNetManager->mRemoteCursors[i];
		int aCursorX = (int)aCursor.mRenderX;
		int aCursorY = (int)aCursor.mRenderY;
		LawnNet::NetColor aColor = LawnNet::GetRelativePlayerColor(mNetManager->GetMySlot(), i);

                const bool aHoldingPlant = mBoard &&
                        aCursor.mHeldSeed < (uint8_t)SeedType::NUM_SEED_TYPES &&
                        (aCursor.mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_BANK ||
                         aCursor.mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_USABLE_COIN ||
                         aCursor.mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_DUPLICATOR);
                if (aHoldingPlant)
                {
                        SeedType aSeed = (SeedType)aCursor.mHeldSeed;
                        SeedType anImitater = aCursor.mHeldImitater != 255 ? (SeedType)aCursor.mHeldImitater : SeedType::SEED_NONE;
                        // CursorObject lives at mouse-25,-35, then applies these
                        // per-seed offsets while drawing.  This overlay draws in
                        // screen coordinates, so include both parts here.
                        float anOffsetX = -35.0f;
                        float anOffsetY = PlantDrawHeightOffset(mBoard, nullptr, aSeed, -1, -1) - 60.0f;
#ifdef _HAS_BLOOM_AND_DOOM_CONTENTS
                        if (aSeed == SeedType::SEED_STINGER)
                                anOffsetY -= 60.0f;
                        else
#endif
                        if (Plant::IsFlying(aSeed) || aSeed == SeedType::SEED_GRAVEBUSTER)
                                anOffsetY += 30.0f;
                        if (IsIZombieLevel())
                        {
                                anOffsetX -= 55.0f;
                                anOffsetY -= 70.0f;
                                if (aSeed == SeedType::SEED_ZOMBIE_BUNGEE)
                                        anOffsetY -= 600.0f;
                                else if (aSeed == SeedType::SEED_ZOMBONI)
                                {
                                        anOffsetX += 10.0f;
                                        anOffsetY -= 25.0f;
                                }
                                else if (aSeed == SeedType::SEED_ZOMBIE_POLEVAULTER)
                                        anOffsetX -= 50.0f;
                        }
                        Plant::DrawSeedTypeTintedOutline(&g, aSeed, anImitater, VARIATION_NORMAL,
                                (float)aCursorX + anOffsetX, (float)aCursorY + anOffsetY,
                                Color(aColor.r, aColor.g, aColor.b),
                                mNetManager->mConfig.mHeldPlantTintAlpha,
                                mNetManager->mConfig.mHeldPlantOutlineAlpha);
                }

                bool aDrewSpecialCursor = false;
                if (!aHoldingPlant && mBoard &&
                        aCursor.mCursorType != CursorType::CURSOR_TYPE_NORMAL &&
                        i >= 0 && i < LawnNet::NET_MAX_PLAYERS)
                {
                        CursorObject* aVisualCursor = mBoard->mNetPlayerCursors[i];
                        if (aVisualCursor)
                        {
                                const SeedType aRemoteSeed =
                                        aCursor.mHeldSeed < (uint8_t)SeedType::NUM_SEED_TYPES ?
                                                (SeedType)aCursor.mHeldSeed : SeedType::SEED_NONE;
                                const SeedType aRemoteImitater =
                                        aCursor.mHeldImitater < (uint8_t)SeedType::NUM_SEED_TYPES ?
                                                (SeedType)aCursor.mHeldImitater : SeedType::SEED_NONE;
                                aDrewSpecialCursor = aVisualCursor->DrawRemoteAtMouse(
                                        &g, aCursorX, aCursorY,
                                        (CursorType)aCursor.mCursorType,
                                        aRemoteSeed, aRemoteImitater,
                                        Color(aColor.r, aColor.g, aColor.b));

				// A cursor packet alone does not prove that the stock tool made it
				// through the overlay draw path.  Record the first rendered instance
				// of every remote tool per slot for the visible co-op smoke tests.
				if (aDrewSpecialCursor && mCoopSmokeManifest.mEnabled &&
					aCursor.mCursorType < 32)
				{
					static uint32_t aLoggedRemoteTools[LawnNet::NET_MAX_PLAYERS] = {};
					const uint32_t aToolBit = 1u << aCursor.mCursorType;
					if ((aLoggedRemoteTools[i] & aToolBit) == 0)
					{
						aLoggedRemoteTools[i] |= aToolBit;
						LawnNet::NetTrace("SMOKE_REMOTE_CURSOR_RENDER role=%s slot=%d type=%u stock=1 tint=%u,%u,%u outline=1 fallback=0 pos=(%d,%d)",
							mCoopSmokeManifest.IsHost() ? "host" : "client", i,
							(unsigned)aCursor.mCursorType, (unsigned)aColor.r,
							(unsigned)aColor.g, (unsigned)aColor.b, aCursorX, aCursorY);
					}
				}
                        }
                }

                // The stock pointer becomes either the held plant silhouette or
                // the mode-specific tool.  Fall back to Cursor13 for NORMAL and
                // for a special visual whose optional resource is unavailable.
                if (!aHoldingPlant && !aDrewSpecialCursor)
                {
                        if (!mNetCursorImage && !aCursorLoadAttempted)
                        {
                                aCursorLoadAttempted = true;
                                // ImageLib chooses the decoder by appending an
                                // extension, so request the extension-less asset.
                                mNetCursorImage = GetImage(_S("Cursor13"));
                                LawnNet::NetTrace("cursor overlay asset load success=%d image=%p size=%dx%d",
                                        mNetCursorImage ? 1 : 0, (void*)mNetCursorImage,
                                        mNetCursorImage ? mNetCursorImage->GetWidth() : 0,
                                        mNetCursorImage ? mNetCursorImage->GetHeight() : 0);
                        }
                        if (mNetCursorImage)
                        {
                                g.SetColorizeImages(true);
                                g.SetColor(Color(aColor.r, aColor.g, aColor.b));
                                g.DrawImage(mNetCursorImage, aCursorX, aCursorY);
                                g.SetColorizeImages(false);
                        }
                }

                if (aPlayer.mNick[0] && Sexy::FONT_BRIANNETOD12)
		{
			SexyString aLabel = StringToSexyString(aPlayer.mNick) + _S(" ") + StrFormat(_S("%ums"), aPlayer.mPing);
			TodDrawString(&g, aLabel, aCursorX + 18, aCursorY + 23, Sexy::FONT_BRIANNETOD12,
				Color(0, 0, 0, 190), DrawStringJustification::DS_ALIGN_LEFT);
			TodDrawString(&g, aLabel, aCursorX + 17, aCursorY + 22, Sexy::FONT_BRIANNETOD12,
				Color(aColor.r, aColor.g, aColor.b), DrawStringJustification::DS_ALIGN_LEFT);
		}
	}
}
