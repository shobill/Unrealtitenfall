#pragma once

#include "CoreMinimal.h"

class AController;
class AActor;
class UWorld;
class ADeathMatchAIController;
class AAIController;

/**
 * Spawn / respawn helpers for DeathMatch.
 * Keeps AGM_DeathMatch as orchestrator only.
 */
namespace DeathMatchSpawnUtils
{
	void EnsurePlayerStateCreated(AController* Controller);

	AActor* FindPlayerStartByTeam(UWorld* World, AController* Player);

	void GatherControllers(UWorld* World, TArray<AController*>& OutControllers);

	/** Destroy old pawn (if any) and spawn AI character at team start */
	bool RestartDeathMatchAI(UWorld* World, ADeathMatchAIController* AIController);

	/** Spawn one AIController of class, assign team, return controller (not possessed yet) */
	AAIController* SpawnAIController(UWorld* World, TSubclassOf<AAIController> AIControllerClass, uint8 TeamId);
}
