#include "../../LawnApp.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include "../../Sexy.TodLib/TodDebug.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libavutil/samplefmt.h>
#include <libswscale/swscale.h>
}
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <utility>
#include <vector>

namespace
{
struct VideoFrameDeleter
{
    void operator()(AVFrame* frame) const { av_frame_free(&frame); }
};
using VideoFrame = std::unique_ptr<AVFrame, VideoFrameDeleter>;

struct VideoResources
{
    AVFormatContext* format = nullptr;
    AVCodecContext* video = nullptr;
    AVCodecContext* audio = nullptr;
    AVPacket* packet = nullptr;
    AVFrame* picture = nullptr;
    AVFrame* sound = nullptr;
    AVFrame* converted = nullptr;
    std::deque<VideoFrame> pendingPictures;
    SwsContext* scaler = nullptr;
    SDL_Texture* texture = nullptr;
    SDL_AudioStream* playback = nullptr;
    void Release()
    {
        if (texture) SDL_DestroyTexture(texture);
        if (playback) SDL_DestroyAudioStream(playback);
        texture = nullptr; playback = nullptr;
        sws_freeContext(scaler); scaler = nullptr;
        pendingPictures.clear();
        av_frame_free(&converted); av_frame_free(&picture); av_frame_free(&sound);
        av_packet_free(&packet);
        avcodec_free_context(&video); avcodec_free_context(&audio);
        avformat_close_input(&format);
    }
    ~VideoResources() { Release(); }
};

SDL_AudioFormat AudioFormat(AVSampleFormat format)
{
    switch (av_get_packed_sample_fmt(format))
    {
    case AV_SAMPLE_FMT_U8: return SDL_AUDIO_U8;
    case AV_SAMPLE_FMT_S16: return SDL_AUDIO_S16;
    case AV_SAMPLE_FMT_S32: return SDL_AUDIO_S32;
    case AV_SAMPLE_FMT_FLT: return SDL_AUDIO_F32;
    default: return SDL_AUDIO_UNKNOWN;
    }
}

std::string JsonString(const std::string& value)
{
    std::string result = "\"";
    for (unsigned char c : value)
    {
        if (c == '\\' || c == '"') { result += '\\'; result += static_cast<char>(c); }
        else if (c >= 0x20) result += static_cast<char>(c);
    }
    return result + "\"";
}
}

bool LawnApp::PlayVideo(std::string url, bool isSkipable, Color bgColor)
{
    VideoResources media;
    bool quit = false, skipped = false, failed = false, completed = false;
    bool screenshot = false, clockStarted = false, audioStarted = false;
    Uint64 clockStart = 0, lastPump = SDL_GetTicksNS();
    double maxPumpGapMs = 0.0, firstFramePts = -1.0, audioStart = 0.0, audioEnd = 0.0;
    uint64_t frameCount = 0, preludeFrames = 0, audioFramesQueued = 0;
    size_t maxPendingVideoFrames = 0;
    uint64_t firstPresentationAudioFramesQueued = 0;
    int textureWidth = 0, textureHeight = 0;
    SDL_AudioSpec audioSpec{};
    std::string error;
    std::error_code pathError;
    const std::filesystem::path source = std::filesystem::absolute(url, pathError);
    const char* proofFlag = std::getenv("PVZ_COOP_SMOKE_STARTUP_VIDEO");
    const bool proof = proofFlag && std::strcmp(proofFlag, "1") == 0 && CoopSmokeActive() &&
        mCoopSmokeManifest.mScenario == LawnNet::NETSMOKE_SCENARIO_ADVENTURE_1 &&
        !pathError && source.filename() == "intro.mp4" &&
        std::filesystem::path(mCoopSmokeManifest.mResultPath).is_absolute();
    const auto proofDirectory = std::filesystem::path(mCoopSmokeManifest.mResultPath).parent_path();
    const std::array<double, 4> captureTimes{0.5, 1.5, 3.0, 4.5};
    const std::array<const char*, 4> captureNames{
        "startup-video-0500.bmp", "startup-video-1500.bmp", "startup-video-3000.bmp", "startup-video-4500.bmp"};
    std::array<bool, 4> captureAttempted{}, captureSaved{};
    std::array<double, 4> capturePts{};

    if (proof)
    {
        // Identify this renderer's actual Win32 window before any demux/decode
        // work. Process-owned helper windows and pre-player Init are distinct.
        HWND window = mSDLWindow ? static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(mSDLWindow),
            SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr)) : nullptr;
        DWORD windowProcess = 0;
        const DWORD windowThread = window ? GetWindowThreadProcessId(window, &windowProcess) : 0;
        FILETIME created{}, exited{}, kernel{}, user{}, now{};
        double processAgeMs = -1.0;
        if (GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user))
        {
            GetSystemTimeAsFileTime(&now);
            ULARGE_INTEGER begin{}, end{};
            begin.LowPart = created.dwLowDateTime; begin.HighPart = created.dwHighDateTime;
            end.LowPart = now.dwLowDateTime; end.HighPart = now.dwHighDateTime;
            processAgeMs = (end.QuadPart - begin.QuadPart) / 10000.0;
        }
        const auto temporary = proofDirectory / "startup-video-started.json.tmp";
        std::ofstream started(temporary, std::ios::binary | std::ios::trunc);
        started << std::fixed << std::setprecision(6) << "{\n  \"phase\": \"entered\",\n  \"pid\": " << GetCurrentProcessId()
            << ",\n  \"windowHandle\": " << JsonString(std::to_string(reinterpret_cast<uintptr_t>(window)))
            << ",\n  \"windowProcessId\": " << windowProcess << ",\n  \"callerThreadId\": " << GetCurrentThreadId()
            << ",\n  \"windowThreadId\": " << windowThread << ",\n  \"primaryThreadId\": " << mPrimaryThreadId
            << ",\n  \"processAgeMs\": " << processAgeMs << ",\n  \"sourceUrl\": " << JsonString(source.string()) << "\n}\n";
        started.close();
        std::error_code startedError;
        if (started) std::filesystem::rename(temporary, proofDirectory / "startup-video-started.json", startedError);
        if (!started || startedError) TodTrace("Could not publish startup video entry proof\n");
    }

    mIsPlayingVideo = true;
    SDL_HideCursor();
    auto fail = [&](const char* message) { failed = true; error = message; return false; };
    auto elapsed = [&]() { return clockStarted ? (SDL_GetTicksNS() - clockStart) / 1e9 : 0.0; };
    auto startClock = [&]() { if (!clockStarted) { clockStart = SDL_GetTicksNS(); clockStarted = true; } };
    auto pump = [&]()
    {
        mLastUserInputTick = mLastTimerTime;
        const Uint64 now = SDL_GetTicksNS();
        maxPumpGapMs = (std::max)(maxPumpGapMs, (now - lastPump) / 1e6);
        lastPump = now;
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT || (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                (!mSDLWindow || event.window.windowID == SDL_GetWindowID(mSDLWindow)))) quit = true;
            else if ((event.type == SDL_EVENT_WINDOW_FOCUS_GAINED || event.type == SDL_EVENT_WINDOW_FOCUS_LOST) &&
                mSDLWindow && event.window.windowID == SDL_GetWindowID(mSDLWindow))
            {
                mActive = event.type == SDL_EVENT_WINDOW_FOCUS_GAINED;
                RehupFocus();
                if (mActive) { EnforceCursor(); SDL_HideCursor(); }
            }
            else if (event.type == SDL_EVENT_KEY_DOWN)
            {
                mLastUserInputTick = mLastTimerTime;
                if (isSkipable && event.key.key == SDLK_ESCAPE) skipped = true;
                else if (event.key.key == SDLK_F11 && !event.key.repeat)
                {
                    // MakeWindow recreates the SDL renderer, invalidating its textures.
                    if (media.texture) SDL_DestroyTexture(media.texture);
                    media.texture = nullptr; textureWidth = textureHeight = 0;
                    SwitchScreenMode(!mIsWindowed, true);
                    SDL_HideCursor();
                }
                else if (event.key.key == SDLK_F10) screenshot = true;
                else if (mDebugKeysEnabled) DebugKeyDown(GetKeyCodeFromCodeSDL(event.key.key));
            }
            else if (event.type == SDL_EVENT_KEY_UP && mWidgetManager)
                mWidgetManager->KeyUp(GetKeyCodeFromCodeSDL(event.key.key));
        }
        if (!mIsPlayingVideo) skipped = true;
        if (clockStarted && media.playback && !audioStarted && elapsed() >= audioStart)
        {
            if (!SDL_ResumeAudioStreamDevice(media.playback)) fail("could not resume video audio");
            else audioStarted = true;
        }
        return !quit && !skipped && !failed;
    };
    auto waitUntil = [&](double time)
    {
        while (pump() && elapsed() < time) SDL_Delay(2);
        return !quit && !skipped && !failed;
    };

    try
    {
        const bool played = [&]() -> bool
        {
            if (!mSDLRenderer || avformat_open_input(&media.format, url.c_str(), nullptr, nullptr) < 0)
                return fail("could not open startup video");
            if (avformat_find_stream_info(media.format, nullptr) < 0) return fail("could not read video streams");
            const AVCodec* videoCodec = nullptr;
            const AVCodec* audioCodec = nullptr;
            const int videoIndex = av_find_best_stream(media.format, AVMEDIA_TYPE_VIDEO, -1, -1, &videoCodec, 0);
            if (videoIndex < 0 || !videoCodec) return fail("startup file has no decodable video stream");
            const int audioIndex = av_find_best_stream(media.format, AVMEDIA_TYPE_AUDIO, -1, videoIndex, &audioCodec, 0);
            const AVStream* videoStream = media.format->streams[videoIndex];
            media.video = avcodec_alloc_context3(videoCodec);
            if (!media.video || avcodec_parameters_to_context(media.video, videoStream->codecpar) < 0 ||
                avcodec_open2(media.video, videoCodec, nullptr) < 0) return fail("could not open video decoder");
            if (audioIndex >= 0 && audioCodec)
            {
                media.audio = avcodec_alloc_context3(audioCodec);
                if (!media.audio || avcodec_parameters_to_context(media.audio, media.format->streams[audioIndex]->codecpar) < 0 ||
                    avcodec_open2(media.audio, audioCodec, nullptr) < 0) return fail("could not open video audio decoder");
            }
            media.packet = av_packet_alloc(); media.picture = av_frame_alloc(); media.sound = av_frame_alloc();
            media.converted = av_frame_alloc();
            if (!media.packet || !media.picture || !media.sound || !media.converted) return fail("video allocation failed");
            double origin = 0.0;
            if (media.format->start_time != AV_NOPTS_VALUE) origin = media.format->start_time / double(AV_TIME_BASE);
            else
            {
                double earliest = (std::numeric_limits<double>::max)();
                for (unsigned int i = 0; i < media.format->nb_streams; ++i)
                {
                    const AVStream* stream = media.format->streams[i];
                    if ((static_cast<int>(i) == videoIndex || static_cast<int>(i) == audioIndex) && stream->start_time != AV_NOPTS_VALUE)
                        earliest = (std::min)(earliest, stream->start_time * av_q2d(stream->time_base));
                }
                if (earliest != (std::numeric_limits<double>::max)()) origin = earliest;
            }
            const AVRational rate = av_guess_frame_rate(media.format, const_cast<AVStream*>(videoStream), nullptr);
            const double frameStep = rate.num > 0 && rate.den > 0 ? av_q2d(av_inv_q(rate)) : 1.0 / 30.0;
            double nextVideoTime = 0.0;
            auto timestamp = [&](const AVFrame* frame, const AVStream* stream, double fallback)
            {
                const int64_t pts = frame->best_effort_timestamp != AV_NOPTS_VALUE ? frame->best_effort_timestamp : frame->pts;
                const double value = pts == AV_NOPTS_VALUE ? fallback : pts * av_q2d(stream->time_base) - origin;
                return std::isfinite(value) ? (std::max)(0.0, value) : fallback;
            };
            auto present = [&](AVFrame* frame) -> bool
            {
                const double pts = timestamp(frame, videoStream, nextVideoTime);
                nextVideoTime = pts + frameStep;
                if (!frameCount) firstPresentationAudioFramesQueued = audioFramesQueued;
                startClock();
                if (!waitUntil(pts)) return false;
                if (elapsed() - pts > 0.5) return true;
                if (frame->width <= 0 || frame->height <= 0) return fail("invalid decoded frame dimensions");
                if (!media.texture || textureWidth != frame->width || textureHeight != frame->height)
                {
                    if (media.texture) SDL_DestroyTexture(media.texture);
                    media.texture = SDL_CreateTexture(mSDLRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, frame->width, frame->height);
                    if (!media.texture) return fail("could not create video texture");
                    textureWidth = frame->width; textureHeight = frame->height;
                }
                if (frame->format != AV_PIX_FMT_YUV420P)
                {
                    AVFrame* converted = media.converted;
                    if (converted->width != frame->width || converted->height != frame->height || !converted->data[0])
                    {
                        av_frame_unref(converted);
                        converted->format = AV_PIX_FMT_YUV420P; converted->width = frame->width; converted->height = frame->height;
                        if (av_frame_get_buffer(converted, 32) < 0) return fail("video conversion allocation failed");
                    }
                    media.scaler = sws_getCachedContext(media.scaler, frame->width, frame->height, static_cast<AVPixelFormat>(frame->format),
                        frame->width, frame->height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, nullptr, nullptr, nullptr);
                    if (!media.scaler || av_frame_make_writable(converted) < 0 ||
                        sws_scale(media.scaler, frame->data, frame->linesize, 0, frame->height, converted->data, converted->linesize) <= 0)
                        return fail("could not convert decoded video format");
                    frame = converted;
                }
                if (!SDL_UpdateYUVTexture(media.texture, nullptr, frame->data[0], frame->linesize[0],
                    frame->data[1], frame->linesize[1], frame->data[2], frame->linesize[2])) return fail("video texture upload failed");
                SDL_SetRenderTarget(mSDLRenderer, nullptr);
                SDL_SetRenderDrawColor(mSDLRenderer, bgColor.mRed, bgColor.mGreen, bgColor.mBlue, bgColor.mAlpha);
                SDL_RenderClear(mSDLRenderer);
                const float scale = (std::max)(float(mWidth) / frame->width, float(mHeight) / frame->height);
                SDL_FRect rect{(mWidth - frame->width * scale) / 2, (mHeight - frame->height * scale) / 2,
                    frame->width * scale, frame->height * scale};
                if (!SDL_RenderTexture(mSDLRenderer, media.texture, nullptr, &rect)) return fail("video rendering failed");
                if (screenshot) { TakeScreenshot(); screenshot = false; }
                if (proof)
                    for (size_t i = 0; i < captureTimes.size(); ++i)
                        if (!captureAttempted[i] && pts >= captureTimes[i])
                        {
                            captureAttempted[i] = true; capturePts[i] = pts;
                            SDL_Surface* surface = SDL_RenderReadPixels(mSDLRenderer, nullptr);
                            if (surface)
                            {
                                captureSaved[i] = SDL_SaveBMP(surface, (proofDirectory / captureNames[i]).string().c_str());
                                SDL_DestroySurface(surface);
                            }
                        }
                if (!SDL_RenderPresent(mSDLRenderer)) return fail("video presentation failed");
                if (!frameCount) firstFramePts = pts;
                ++frameCount;
                if (pts < 4.0) ++preludeFrames;
                return true;
            };
            auto presentNext = [&]() -> bool
            {
                const bool ok = present(media.pendingPictures.front().get());
                media.pendingPictures.pop_front();
                return ok;
            };
            auto queueAudio = [&]() -> bool
            {
                const AVFrame* frame = media.sound;
                const auto format = static_cast<AVSampleFormat>(frame->format);
                const SDL_AudioFormat sdlFormat = AudioFormat(format);
                const int channels = frame->ch_layout.nb_channels;
                if (sdlFormat == SDL_AUDIO_UNKNOWN || channels <= 0 || frame->sample_rate <= 0 || frame->nb_samples < 0)
                    return fail("unsupported decoded audio format");
                const double pts = timestamp(frame, media.format->streams[audioIndex], audioEnd);
                if (!media.playback)
                {
                    audioSpec = {sdlFormat, channels, frame->sample_rate};
                    media.playback = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audioSpec, nullptr, nullptr);
                    if (!media.playback) return fail("could not open video audio device");
                    audioStart = pts;
                }
                if (sdlFormat != audioSpec.format || channels != audioSpec.channels || frame->sample_rate != audioSpec.freq)
                    return fail("decoded audio format changed during playback");
                bool queued;
                if (av_sample_fmt_is_planar(format))
                {
                    std::vector<const void*> planes(channels);
                    for (int channel = 0; channel < channels; ++channel) planes[channel] = frame->extended_data[channel];
                    queued = SDL_PutAudioStreamPlanarData(media.playback, planes.data(), channels, frame->nb_samples);
                }
                else
                {
                    const int bytes = av_samples_get_buffer_size(nullptr, channels, frame->nb_samples, format, 1);
                    queued = bytes >= 0 && SDL_PutAudioStreamData(media.playback, frame->data[0], bytes);
                }
                if (!queued) return fail("could not queue decoded audio");
                ++audioFramesQueued;
                audioEnd = pts + double(frame->nb_samples) / frame->sample_rate;
                return pump();
            };
            auto drain = [&](bool video) -> bool
            {
                AVCodecContext* decoder = video ? media.video : media.audio;
                AVFrame* frame = video ? media.picture : media.sound;
                int result;
                while ((result = avcodec_receive_frame(decoder, frame)) >= 0)
                {
                    bool ok = pump();
                    if (ok && video)
                    {
                        VideoFrame retained(av_frame_clone(frame));
                        if (!retained) ok = fail("could not retain decoded video frame");
                        else
                        {
                            media.pendingPictures.push_back(std::move(retained));
                            maxPendingVideoFrames = (std::max)(maxPendingVideoFrames, media.pendingPictures.size());
                            // Decode ahead by a bounded six frames. Otherwise
                            // waiting for every video PTS can starve following
                            // tightly interleaved audio packets of queue lead.
                            if (media.pendingPictures.size() >= 6) ok = presentNext();
                        }
                    }
                    else if (ok) ok = queueAudio();
                    av_frame_unref(frame);
                    if (!ok) return false;
                }
                return result == AVERROR(EAGAIN) || result == AVERROR_EOF || fail("startup video decoding failed");
            };
            auto submit = [&](bool video, const AVPacket* packet) -> bool
            {
                AVCodecContext* decoder = video ? media.video : media.audio;
                int result = avcodec_send_packet(decoder, packet);
                if (result == AVERROR(EAGAIN))
                {
                    if (!drain(video)) return false;
                    result = avcodec_send_packet(decoder, packet);
                }
                if (result < 0 && result != AVERROR_EOF) return fail("startup video packet rejected");
                return drain(video);
            };
            int readResult = AVERROR_EOF;
            while (pump() && (readResult = av_read_frame(media.format, media.packet)) >= 0)
            {
                bool ok = true;
                // Never discard initial reference frames while waiting for audio.
                if (media.packet->stream_index == videoIndex) ok = submit(true, media.packet);
                else if (media.audio && media.packet->stream_index == audioIndex) ok = submit(false, media.packet);
                av_packet_unref(media.packet);
                if (!ok) return false;
            }
            if (quit || skipped || failed) return false;
            if (readResult != AVERROR_EOF) return fail("could not finish reading startup video");
            if (!submit(true, nullptr) || (media.audio && !submit(false, nullptr))) return false;
            while (!media.pendingPictures.empty()) if (!presentNext()) return false;
            if (media.playback)
            {
                if (!SDL_FlushAudioStream(media.playback)) return fail("could not flush video audio");
                const double tailDeadline = elapsed() + (std::max)(2.0, audioEnd - elapsed() + 1.0);
                while (pump() && (elapsed() < audioEnd || SDL_GetAudioStreamQueued(media.playback) > 0))
                {
                    if (elapsed() > tailDeadline) return fail("video audio drain timed out");
                    SDL_Delay(2);
                }
                if (quit || skipped || failed) return false;
                // The device can own a final short buffer after the stream is empty.
                if (!waitUntil(elapsed() + 0.08)) return false;
            }
            if (!frameCount) return fail("startup video decoded no displayable frames");
            return true;
        }();
        completed = played && !failed && !skipped && !quit;
    }
    catch (const std::exception& exception) { fail(exception.what()); }
    catch (...) { fail("unexpected startup video failure"); }

    if (proof)
    {
        const auto proofPath = proofDirectory / "startup-video.json";
        const auto temporaryPath = proofDirectory / "startup-video.json.tmp";
        std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
        output << std::fixed << std::setprecision(6) << "{\n  \"sourceUrl\": " << JsonString(source.string())
            << ",\n  \"firstFramePts\": " << firstFramePts << ",\n  \"frameCount\": " << frameCount
            << ",\n  \"preludeFrames\": " << preludeFrames << ",\n  \"maxEventPumpGapMs\": " << maxPumpGapMs
            << ",\n  \"audioFramesQueued\": " << audioFramesQueued << ",\n  \"completed\": " << (completed ? "true" : "false")
            << ",\n  \"maxPendingVideoFrames\": " << maxPendingVideoFrames
            << ",\n  \"firstPresentationAudioFramesQueued\": " << firstPresentationAudioFramesQueued
            << ",\n  \"skipped\": " << (skipped ? "true" : "false") << ",\n  \"failed\": " << (failed ? "true" : "false")
            << ",\n  \"error\": " << JsonString(error) << ",\n  \"captures\": [";
        for (size_t i = 0; i < captureTimes.size(); ++i)
            output << (i ? "," : "") << "{\"file\":" << JsonString(captureNames[i]) << ",\"pts\":" << capturePts[i]
                << ",\"success\":" << (captureSaved[i] ? "true" : "false") << "}";
        output << "]\n}\n";
        output.close();
        std::error_code publishError;
        if (output) std::filesystem::rename(temporaryPath, proofPath, publishError);
        if (!output || publishError) TodTrace("Could not publish startup video proof\n");
    }
    if (failed) TodTrace("Video playback failed: %s (%s)\n", error.c_str(), url.c_str());
    media.Release();
    mIsPlayingVideo = false;
    SDL_ShowCursor();
    if (quit) Shutdown();
    return !failed;
}
