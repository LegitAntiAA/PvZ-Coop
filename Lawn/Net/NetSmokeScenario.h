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
	NETSMOKE_SCENARIO_TOWER_DEFENSE_CLIENT = 10
};

inline bool NetIsInteractiveChooserSmokeScenario(NetSmokeScenarioId theScenario)
{
        return theScenario == NETSMOKE_SCENARIO_ADVENTURE_8_SOAK ||
		theScenario == NETSMOKE_SCENARIO_ADVENTURE_8_VICTORY ||
		theScenario == NETSMOKE_SCENARIO_TOWER_DEFENSE_CLIENT;
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
			(uint8_t)GameMode::GAMEMODE_COOP_TOWER_DEFENSE, true }
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
	void OnSlotMachineSound(bool theHost);
	bool IsComplete(uint32_t theRequiredPostActionSnapshots) const;
	bool HasIssuedAction() const { return mActionIssued; }
	bool HasAppliedAction() const { return mActionApplied; }
	uint32_t GetPostActionSamples() const { return mPostActionSamples; }
};

} // namespace LawnNet

#endif // __NETSMOKESCENARIO_H__
