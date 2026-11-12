#include "../Board.h"
#include "../../LawnApp.h"
#include "../System/Music.h"
#include "../../Sexy.TodLib/TodFoley.h"
#include <cmath>

void Board::UpdateEndlessWeatherPresentation()
{
	if (!IsEndlessT() || (mApp->NetIsClient() && !mNetWeatherSnapshotReady)) return;
	const bool aPlaying = mApp->mGameScene == SCENE_PLAYING && !mPaused &&
		mApp->mBoardResult == BOARDRESULT_NONE && !mLevelComplete && !mLevelAwardSpawned && mBoardFadeOutCounter < 0;
	const bool aFirstSample = !mNetWeatherPresentationInitialized;
	if (aFirstSample)
	{
		// A late join starts at the accepted snapshot, rather than fading from day.
		mNetVisualNightBlend = (float)mNetNightBlend;
		mNetWeatherPresentationInitialized = true;
	}
	else if (mApp->NetIsClient() && mPaused)
		mNetVisualNightBlend = (float)mNetNightBlend;
	else if (aPlaying)
	{
		if (mApp->NetIsClient())
		{
			const float aDelta = (float)mNetNightBlend - mNetVisualNightBlend;
			mNetVisualNightBlend += max(-4.0f, min(4.0f, aDelta));
		}
		else mNetVisualNightBlend = (float)mNetNightBlend;
	}

	if (!aFirstSample && aPlaying && mNetStormTimer && mNetStormFlashKind == mNetWeatherLastStormKind &&
		mNetStormFlashCounter <= mNetWeatherLastStormCounter)
	{
		for (int anEdge : {300, 150})
		{
			const bool aFlashEdge = anEdge == 300 ? (mNetStormFlashKind == 1 || mNetStormFlashKind == 2) :
				(mNetStormFlashKind == 1 || mNetStormFlashKind == 3);
			if (aFlashEdge && mNetWeatherLastStormCounter > anEdge && mNetStormFlashCounter <= anEdge &&
				mApp->mSoundSystem && !mApp->mMuteSoundsForCutscene)
			{
				// Every viewer follows the same counter. Bypass LawnApp's network
				// foley broadcast so the client does not receive a second thunder.
				mApp->mSoundSystem->PlayFoley(FOLEY_THUNDER);
				LawnNet::NetTrace("WEATHER_THUNDER role=%s kind=%u edge=%d counter=%u local=1",
					mApp->NetIsClient() ? "client" : "host", mNetStormFlashKind, anEdge, mNetStormFlashCounter);
			}
		}
	}
	mNetWeatherLastStormKind = mNetStormFlashKind;
	mNetWeatherLastStormCounter = mNetStormFlashCounter;

	Music* aMusic = mApp->mMusic;
	if (aPlaying && aMusic && !aMusic->mPaused)
	{
		const MusicTune aTune = mNetVisualNightBlend >= 500.0f ? MUSIC_TUNE_NIGHT_MOONGRAINS : MUSIC_TUNE_DAY_GRASSWALK;
		if (aMusic->mCurMusicTune != aTune)
		{
			// Enforce silence at the crossing even if a sparse sample straddles 500.
			aMusic->SetWeatherGain(0.0f);
			aMusic->MakeSureMusicIsPlaying(aTune);
		}
	}
	if (aMusic && mApp->mGameScene == SCENE_PLAYING)
		aMusic->SetWeatherGain(fabsf(2.0f * mNetVisualNightBlend / 1000.0f - 1.0f));
}
