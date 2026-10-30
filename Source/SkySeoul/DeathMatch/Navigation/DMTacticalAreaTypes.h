#pragma once

#include "CoreMinimal.h"
#include "DMTacticalAreaTypes.generated.h"

/** Coarse designer-authored strategic area. Type is reserved for later policy expansion. */
UENUM(BlueprintType)
enum class EDMTacticalAreaType : uint8
{
	General,
	Objective,
	Transit
};

/** Event inputs accepted by the tactical-area world model. */
UENUM(BlueprintType)
enum class EDMTacticalAreaEventType : uint8
{
	CharacterSpawn,
	CharacterDeath,
	VolumeEnter,
	VolumeExit,
	CombatStart,
	CombatEnd,
	EnemySeen,
	DamageReceived,
	Sound
};

/** Compact reason used by Roam diagnostics and per-AI failure cooldowns. */
UENUM(BlueprintType)
enum class EDMRoamSelectionFailureReason : uint8
{
	None,
	NoNavProjection,
	RandomNavQueryFailed,
	DuplicateRandomPoint,
	OutsideTargetVolume,
	NoCompletePath,
	PartialPath,
	VolumeAttemptLimit,
	VolumeCooldown,
	MoveRequestRejected,
	TargetVolumeDisabled,
	GoalNavInvalid
};
