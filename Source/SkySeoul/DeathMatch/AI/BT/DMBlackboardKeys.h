#pragma once

#include "CoreMinimal.h"

/** Default Blackboard key names for DeathMatch robot BT (Phase 2). */
namespace DMBlackboardKeys
{
	inline const FName AIState(TEXT("AIState"));
	inline const FName CombatTarget(TEXT("CombatTarget"));
	inline const FName TargetVisible(TEXT("TargetVisible"));
	inline const FName HasRememberedTarget(TEXT("HasRememberedTarget"));
	inline const FName LastKnownTargetLocation(TEXT("LastKnownTargetLocation"));
	inline const FName LastDamageOrigin(TEXT("LastDamageOrigin"));
	inline const FName ThreatAwareness(TEXT("ThreatAwareness"));
	inline const FName HealthRatio(TEXT("HealthRatio"));
	inline const FName RiskTier(TEXT("RiskTier"));
	inline const FName SelectedTacticalArea(TEXT("SelectedTacticalArea"));
	inline const FName MoveGoal(TEXT("MoveGoal"));
	inline const FName EQSResultLocation(TEXT("EQSResultLocation"));
	inline const FName ShouldADS(TEXT("ShouldADS"));
	inline const FName ShouldFire(TEXT("ShouldFire"));
	inline const FName FireWhileMoving(TEXT("FireWhileMoving"));

	// Existing asset aliases retained only at the key-name level; no legacy runtime owner remains.
	inline const FName TargetActor(TEXT("TargetActor"));
	inline const FName TargetLocation(TEXT("TargetLocation"));
	inline const FName HasTarget(TEXT("HasTarget"));
	inline const FName CanShoot(TEXT("CanShoot"));
	inline const FName FocusLocation(TEXT("FocusLocation"));
	inline const FName IsFocusSeen(TEXT("IsFocusSeen"));
	inline const FName MoveGoalLocation(TEXT("MoveGoalLocation"));
	inline const FName HasMoveGoal(TEXT("HasMoveGoal"));
	inline const FName WantAttack(TEXT("WantAttack"));
	inline const FName MoveIntent(TEXT("MoveIntent"));
}
