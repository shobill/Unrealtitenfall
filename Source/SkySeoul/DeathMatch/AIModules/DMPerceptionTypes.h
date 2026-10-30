#pragma once

#include "CoreMinimal.h"
#include "DMPerceptionTypes.generated.h"

/** Aggregated hostile awareness for one actor (Phase 1 perception gather). */
USTRUCT(BlueprintType)
struct FDMAwareTarget
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Perception")
	TWeakObjectPtr<AActor> Actor;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Perception")
	bool bCurrentlySensed = false;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Perception")
	bool bKnown = false;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Perception")
	FVector LastSensedLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Perception")
	float LastSensedTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Perception")
	float DistanceSq = TNumericLimits<float>::Max();

	bool IsValid() const { return Actor.IsValid(); }
};
