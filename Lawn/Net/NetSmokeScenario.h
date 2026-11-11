#ifndef __NETSMOKESCENARIO_H__
#define __NETSMOKESCENARIO_H__

#include <cstddef>
#include <cstdint>
#include <string>

#include "../../ConstEnums.h"

class Board;
class LawnApp;

namespace LawnNet
{
class NetManager;

struct NetSmokeManifest;

enum NetSmokeScenarioId : uint8_t
{
	NETSMOKE_SCENARIO_INVALID = 0,
	NETSMOKE_SCENARIO_ADVENTURE_1 = 1,
	NETSMOKE_SCENARIO_ADVENTURE_5_BOWLING = 2,
	NETSMOKE_SCENARIO_VASEBREAKER = 3,
	NETSMOKE_SCENARIO_BEGHOULED = 4,
	NETSMOKE_SCENARIO_ADVENTURE_5_BOWLING_CLIENT = 5,
        NETSMOKE_SCENARIO_ADVENTURE_15_WHACK_CLIENT = 6,
        NETSMOKE_SCENARIO_SLOT_MACHINE_CLIENT = 7,
        NETSMOKE_SCENARIO_ADVENTURE_8_SOAK = 8,
        NETSMOKE_SCENARIO_ADVENTURE_8_VICTORY = 9,
	NETSMOKE_SCENARIO_TOWER_DEFENSE_CLIENT = 10,
	NETSMOKE_SCENARIO_PORTAL_COMBAT_CLIENT = 11,
	NETSMOKE_SCENARIO_ENDLESS_ST = 12,
	NETSMOKE_SCENARIO_PVP_1V1 = 13,
	NETSMOKE_SCENARIO_ENDLESS_OVERFLOW = 14
};

// Runtime-only smoke setup.  Manifest/protocol tests never call this function,
// so their header-only scenario resolver has no game-executable dependency.
void NetPrepareSmokeScenarioLobby(NetSmokeScenarioId theScenario, NetManager* theManager);

inline bool NetIsInteractiveChooserSmokeScenario(NetSmokeScenarioId theScenario)
{
        return theScenario == NETSMOKE_SCENARIO_ADVENTURE_8_SOAK ||
		theScenario == NETSMOKE_SCENARIO_ADVENTURE_8_VICTORY ||
                theScenario == NETSMOKE_SCENARIO_TOWER_DEFENSE_CLIENT ||
                theScenario == NETSMOKE_SCENARIO_ENDLESS_ST ||
		theScenario == NETSMOKE_SCENARIO_ENDLESS_OVERFLOW;
}

inline bool NetIsBowlingSmokeScenario(NetSmokeScenarioId theScenario)
{
	return theScenario == NETSMOKE_SCENARIO_ADVENTURE_5_BOWLING ||
		theScenario == NETSMOKE_SCENARIO_ADVENTURE_5_BOWLING_CLIENT;
}

struct NetSmokeScenarioDefinition
{
	NetSmokeScenarioId mId;
	const char* mName;
	const char* mMode;
	int mLevel;
	uint8_t mGameMode;
	bool mActionIsClientOwned;
};

// Resolve the deliberately small, audited smoke allow-list.  No fallback is
// provided: an unknown mode/level pair must fail before normal game startup.
inline const NetSmokeScenarioDefinition* NetSmokeScenarioTable(size_t& theCount)
{
	static const NetSmokeScenarioDefinition aScenarios[] =
	{
		{ NETSMOKE_SCENARIO_ADVENTURE_1, "adventure1", "adventure", 1,
			(uint8_t)GameMode::GAMEMODE_ADVENTURE, false },
		{ NETSMOKE_SCENARIO_ADVENTURE_5_BOWLING, "adventure5_bowling", "adventure", 5,
			(uint8_t)GameMode::GAMEMODE_ADVENTURE, false },
		{ NETSMOKE_SCENARIO_VASEBREAKER, "vasebreaker", "vasebreaker", 1,
			(uint8_t)GameMode::GAMEMODE_CHALLENGE_VASEBREAKER, true },
		{ NETSMOKE_SCENARIO_BEGHOULED, "beghouled", "beghouled", 1,
			(uint8_t)GameMode::GAMEMODE_CHALLENGE_BEGHOULED, true },
		{ NETSMOKE_SCENARIO_ADVENTURE_5_BOWLING_CLIENT,
			"adventure5_bowling_client", "adventure_bowling_client", 5,
			(uint8_t)GameMode::GAMEMODE_ADVENTURE, true },
		{ NETSMOKE_SCENARIO_ADVENTURE_15_WHACK_CLIENT,
			"adventure15_whack_client", "adventure_whack_client", 15,
			(uint8_t)GameMode::GAMEMODE_ADVENTURE, true },
                { NETSMOKE_SCENARIO_SLOT_MACHINE_CLIENT,
                        "slot_machine_client", "slot_machine_client", 1,
                        (uint8_t)GameMode::GAMEMODE_CHALLENGE_SLOT_MACHINE, true },
                { NETSMOKE_SCENARIO_ADVENTURE_8_SOAK,
                        "adventure8_soak", "adventure_soak", 8,
                        (uint8_t)GameMode::GAMEMODE_ADVENTURE, false },
                { NETSMOKE_SCENARIO_ADVENTURE_8_VICTORY,
                        "adventure8_victory", "adventure_victory", 8,
			(uint8_t)GameMode::GAMEMODE_ADVENTURE, false },
		{ NETSMOKE_SCENARIO_TOWER_DEFENSE_CLIENT,
			"tower_defense_client", "tower_defense_client", 1,
			(uint8_t)GameMode::GAMEMODE_COOP_TOWER_DEFENSE, true },
                { NETSMOKE_SCENARIO_PORTAL_COMBAT_CLIENT,
                        "portal_combat_client", "portal_combat_client", 1,
                        (uint8_t)GameMode::GAMEMODE_CHALLENGE_PORTAL_COMBAT, true },
		{ NETSMOKE_SCENARIO_ENDLESS_ST,
			"endless_st", "endless_st", 1,
			(uint8_t)GameMode::GAMEMODE_COOP_ENDLESS_ST, false },
		{ NETSMOKE_SCENARIO_ENDLESS_OVERFLOW,
			"endless_overflow", "endless_overflow", 1,
			(uint8_t)GameMode::GAMEMODE_COOP_ENDLESS_ST, false },
		{ NETSMOKE_SCENARIO_PVP_1V1,
			"pvp_1v1", "pvp_1v1", 1,
			(uint8_t)GameMode::GAMEMODE_COOP_PVP, true }
	};
	theCount = sizeof(aScenarios) / sizeof(aScenarios[0]);
	return aScenarios;
}

inline const NetSmokeScenarioDefinition* NetGetSmokeScenario(NetSmokeScenarioId theId)
{
	size_t aCount = 0;
	const NetSmokeScenarioDefinition* aScenarios = NetSmokeScenarioTable(aCount);
	for (size_t i = 0; i < aCount; ++i)
		if (aScenarios[i].mId == theId)
			return &aScenarios[i];
	return nullptr;
}

inline bool NetResolveSmokeScenario(const std::string& theMode, int theLevel,
	NetSmokeScenarioDefinition& theDefinition, std::string& theError)
{
	size_t aCount = 0;
	const NetSmokeScenarioDefinition* aScenarios = NetSmokeScenarioTable(aCount);
	for (size_t i = 0; i < aCount; ++i)
	{
		if (theMode == aScenarios[i].mMode && theLevel == aScenarios[i].mLevel)
		{
			theDefinition = aScenarios[i];
			theError.clear();
			return true;
		}
	}
	theError = "unsupported smoke scenario; allowed pairs are adventure/1, adventure/5, "
                "adventure_bowling_client/5, adventure_whack_client/15, slot_machine_client/1, "
		"adventure_soak/8, adventure_victory/8, tower_defense_client/1, "
		"portal_combat_client/1, endless_st/1, endless_overflow/1, pvp_1v1/1, "
		"vasebreaker/1, beghouled/1";
	return false;
}

// Test-only runtime controller.  It is inert unless LawnApp loaded a valid
// -coop-smoke-manifest.  Actions use the same Board mouse path as a player;
// the Vasebreaker action is sent through the normal client input protocol.
class NetSmokeScenarioRuntime
{
private:
	NetSmokeScenarioId mScenario;
	Board* mBoardIdentity;
	uint32_t mIntroTick;
	uint32_t mIntroLastActionTick;
	uint32_t mPlayingTick;
	uint32_t mActionTick;
	int mIntroLastCutsceneTime;
	int mIntroLastDaveMessage;
	int mIntroLastTutorialState;
	int mBaselinePlants;
	int mBaselinePots;
	int mBaselineChallengeScore;
	int mBaselineSlotMachineRollCount;
	int mBaselineSunMoney;
	uint32_t mBaselinePlantGridHash;
	uint16_t mBowlingNutId;
	uint16_t mWhackTargetId;
	int mBowlingFirstX;
	int mBowlingFirstY;
	bool mIntroObserved;
	bool mIntroProgressObserved;
	bool mIntroSawDave;
	bool mChooserStartIssued;
	bool mBaselineCaptured;
	bool mActionIssued;
	bool mActionApplied;
	bool mBowlingNutObserved;
	bool mBowlingNutMoved;
	bool mBowlingZombieObserved;
	bool mWhackRemovalObserved;
	bool mSlotMachinePresentationObserved;
	bool mSlotMachineReturnObserved;
        int mSlotMachineSoundCount;
        int mSoakChooserLastCount;
        int mSoakPlantsIssued;
        int mSoakSunClicks;
        uint32_t mSoakLastActionTick;
        bool mSoakChooserVerified;
        bool mSoakLossIssued;
        bool mSoakLossObserved;
        bool mSoakLossCameraObserved;
        bool mSoakLossBrainsObserved;
        bool mSoakLossDialogObserved;
	uint32_t mVictoryTick;
	bool mVictoryIssued;
	bool mVictoryFadeObserved;
	bool mVictoryLevelCompleteSnapshotObserved;
	bool mVictoryAwardObserved;
        bool mVictoryBoardReleasedObserved;
	bool mTdPathVerified;
	bool mTdSpawnAccelerated;
	bool mTdZombieEntryObserved;
	bool mTdZombieTurnObserved;
	bool mTdClientPlantObserved;
	uint16_t mTdZombieId;
	uint16_t mTdPlantId;
	float mTdZombieFirstX;
	bool mPortalLayoutVerified;
	bool mPortalConveyorVerified;
	bool mPortalSpawnAccelerated;
	bool mPortalZombieEntryObserved;
	bool mPortalZombieTeleportObserved;
	bool mPortalClientPlantObserved;
	bool mPortalAdviceShown;
	bool mPortalAdviceCleared;
	uint32_t mPortalAdviceClearTick;
	bool mPortalRelocationVerified;
	bool mPortalCameraVerified;
	bool mPortalCameraMovementVerified;
	int mPortalCameraSteadySamples;
	int mPortalCameraMaxDelta;
	uint16_t mPortalZombieId;
	uint16_t mPortalPlantId;
	uint16_t mPortalHeldPacketId;
	int mPortalHeldPacketIndex;
	int mPortalPlantColumn;
	int mPortalPlantRow;
	bool mPortalHeldPacketEchoed;
	bool mPortalHeldEchoMismatch;
	bool mPortalPlantClickIssued;
	bool mPvpRosterVerified;
	bool mPvpDeckVerified;
	bool mPvpInitialBrainsVerified;
	bool mPvpPassiveBrainsVerified;
	bool mPvpCursorHiddenVerified;
	bool mPvpZombieEntryObserved;
	bool mPvpHostPlantIssued;
	bool mPvpHostPlantObserved;
	uint16_t mPvpZombieId;
	uint16_t mPvpBaselineMaxZombieId;
	int mPvpBrainsAfterAction;
	int mPortalConveyorInitialCounter;
	int mPortalZombieEntryRow;
	uint32_t mPortalZombieEntryTick;
	float mPortalZombieEntryX;
	uint32_t mPortalInitialLayoutHash;
	int mPortalInitialCameraX;
	int mPortalInitialCameraY;
	static const int PORTAL_ZOMBIE_OBSERVATION_CAPACITY = 64;
	uint16_t mPortalObservedZombieIds[PORTAL_ZOMBIE_OBSERVATION_CAPACITY];
	int mPortalObservedZombieRows[PORTAL_ZOMBIE_OBSERVATION_CAPACITY];
	float mPortalObservedZombieXs[PORTAL_ZOMBIE_OBSERVATION_CAPACITY];
	uint32_t mPortalObservedZombieTicks[PORTAL_ZOMBIE_OBSERVATION_CAPACITY];
	bool mPortalObservedZombieEligible[PORTAL_ZOMBIE_OBSERVATION_CAPACITY];
	bool mEndlessModeVerified;
	bool mEndlessWeatherAccelerated;
	bool mEndlessTimeChanged;
	bool mEndlessWaveProgressed;
	bool mEndlessStageTriggered;
	bool mEndlessRepickOpened;
	bool mEndlessRepickLogged;
	bool mEndlessRepickCompleted;
	bool mEndlessHealthScaled;
	bool mEndlessThawed;
	uint8_t mEndlessInitialPhase;
	int mEndlessInitialWave;
	int mEndlessInitialStage;
	uint32_t mEndlessWaveTick;
	uint32_t mEndlessStageTick;
	uintptr_t mEndlessThawReanim;
	float mEndlessThawAnimTime;
	uint32_t mEndlessThawSampleTick;
	uint16_t mEndlessPreRepickMaxZombieId;
	int mEndlessBaseHealth[NUM_ZOMBIE_TYPES];
	bool mOverflowModeVerified;
	bool mOverflowStage255Set;
	bool mOverflowStage255Observed;
	bool mOverflowGargSpawned;
	bool mOverflowGargObserved;
	bool mOverflowTransitionTriggered;
	bool mOverflowRepickOpened;
	bool mOverflowRepickLogged;
	bool mOverflowStage256Observed;
	bool mOverflowRepickCompleted;
	uint16_t mOverflowBaselineMaxZombieId;
	uint16_t mOverflowGargId;
	uint32_t mOverflowStageSetTick;
	uint32_t mOverflowGargTick;
	uint32_t mOverflowTransitionTick;
	int mOverflowGargBodyHealth;
	int mOverflowGargMaxHealth;
	uint32_t mPostActionSamples;

	bool UpdateIntro(LawnApp* theApp, const NetSmokeManifest& theManifest,
		const NetSmokeScenarioDefinition& theDefinition, std::string& theFailure);
	bool IssueAction(LawnApp* theApp, const NetSmokeManifest& theManifest);
	bool ObserveActionApplied(Board* theBoard, const NetSmokeManifest& theManifest,
		std::string& theFailure);

public:
	NetSmokeScenarioRuntime();
	void Reset(NetSmokeScenarioId theScenario = NETSMOKE_SCENARIO_INVALID);
	bool Update(LawnApp* theApp, const NetSmokeManifest& theManifest,
		uint32_t theGameplaySnapshots, std::string& theFailure);
	void OnSnapshotTx(uint32_t theHash, size_t theBytes);
	void OnSnapshotApplied(uint32_t theHash, size_t theBytes);
	void OnLevelCompleteSnapshot(bool theHost, int theFadeCounter);
	void OnAuthoritativeWhackHit(uint16_t theZombieId);
	void OnPortalHeldPacketEcho(uint16_t thePacketId, int thePacketIndex, int theCursorType);
	void OnSlotMachineSound(bool theHost);
	bool IsComplete(uint32_t theRequiredPostActionSnapshots) const;
	bool HasIssuedAction() const { return mActionIssued; }
	bool HasAppliedAction() const { return mActionApplied; }
	uint32_t GetPostActionSamples() const { return mPostActionSamples; }
};

} // namespace LawnNet

#endif // __NETSMOKESCENARIO_H__
