#pragma once

#include "CoreMinimal.h"
#include "GenericTeamAgentInterface.h"

class AController;
class AActor;
class UWorld;

/**
 * Team assignment + attitude helpers for DeathMatch.
 *
 * Unreal Perception affiliation does NOT use raw TeamId compares on the pawn.
 * It calls IGenericTeamAgentInterface::GetTeamAttitudeTowards on the sensing
 * AIController, with the perceived Actor (usually a Pawn). Default AIC impl
 * only Casts Other to IGenericTeamAgentInterface — Pawns fail → Neutral →
 * Sight never fires with bDetectEnemies-only. Resolve Actor → PS/Controller first.
 */
namespace DeathMatchTeamUtils
{
	/** False for explicit debug/non-combat actors; does not exclude ordinary NoTeam actors. */
	bool IsCombatTargetCandidate(const AActor* Actor);

	FGenericTeamId GetTeamIdFromController(const AController* Controller);

	/** Resolve team from Actor, Pawn→PlayerState/Controller, or Controller. */
	FGenericTeamId GetTeamIdFromActor(const AActor* Actor);

	void SetTeamIdToController(AController* Controller, const FGenericTeamId& TeamId);

	/**
	 * Hostile / Friendly / Neutral using FGenericTeamId::GetAttitude.
	 * NoTeam on either side → Neutral (do not treat 255 vs 0 as Hostile).
	 */
	ETeamAttitude::Type GetAttitudeTowards(FGenericTeamId SelfTeam, const AActor& Other);

	const TCHAR* AttitudeToString(ETeamAttitude::Type Attitude);

	/** Round-robin assign if controller has no team yet */
	void AssignTeamIfNone(UWorld* World, AController* NewPlayer, const TArray<uint8>& TeamIds);

	int32 CountControllers(UWorld* World);
}
