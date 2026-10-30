#pragma once

#include "CoreMinimal.h"
#include "Controller/AIC_EnemyControllerBase.h"
#include "DeathMatchAIController.generated.h"

/**
 * DeathMatch AI common base (not robot-specific).
 * Robot AI should inherit ADeathMatchRobotAIController.
 */
UCLASS()
class SKYSEOUL_API ADeathMatchAIController : public AAIC_EnemyControllerBase
{
	GENERATED_BODY()

public:
	ADeathMatchAIController();

	/** Pawn class used on AI restart/spawn. Override in typed children. */
	UFUNCTION(BlueprintPure, Category = "DeathMatch|AI")
	virtual TSubclassOf<APawn> GetAIPawnClass() const { return nullptr; }
};
