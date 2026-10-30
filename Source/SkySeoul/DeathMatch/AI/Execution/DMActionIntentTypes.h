#pragma once

#include "CoreMinimal.h"
#include "DMActionIntentTypes.generated.h"

/** High-level behavior selected by the Behavior Tree. Execution details live in runtime policy code. */
UENUM(BlueprintType)
enum class EDMActionIntent : uint8
{
	None,
	Roam,
	Observe,
	Attack,
	AttackMove,
	FastReposition,
	CombatCover,
	Hide,
	Retreat,
	Investigate
};

/** Root Behavior Tree state. This is intentionally separate from the active sub-branch intent. */
UENUM(BlueprintType)
enum class EDMBehaviorState : uint8
{
	None,
	Roam,
	LostTarget,
	Combat,
	Retreat,
	Hide,
	Dead
};

/** Optional branch execution policy. It changes execution only, never position scoring. */
USTRUCT(BlueprintType)
struct FDMActionIntentExecutionSettings
{
	GENERATED_BODY()

	/** Moving-only override. ADS-required intents still enter ADS after they stop. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Execution") bool bUseADSWhileMoving = false;
	/** Moving-only fire permission. It never makes a non-firing intent eligible to fire. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Execution") bool bAllowFireWhileMoving = false;
};
