#pragma once

#include "CoreMinimal.h"
#include "DMCombatKnowledgeTypes.generated.h"

UENUM(BlueprintType)
enum class EDMThreatAwareness : uint8
{
	Visible,
	LostGrace,
	PendingSwitch,
	Investigating,
	WatchingLastKnown,
	Disengaged
};

USTRUCT(BlueprintType)
struct FDMRememberedThreat
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="DeathMatch|Combat Knowledge")
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="DeathMatch|Combat Knowledge")
	FVector LastKnownLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="DeathMatch|Combat Knowledge")
	float LastSensedTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="DeathMatch|Combat Knowledge")
	bool bVisible = false;
};
