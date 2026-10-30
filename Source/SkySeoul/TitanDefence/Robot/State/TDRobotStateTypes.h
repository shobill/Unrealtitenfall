#pragma once

#include "CoreMinimal.h"
#include "TDRobotStateTypes.generated.h"

/** Detailed reason a robot body-damage request was rejected by State. */
UENUM(BlueprintType)
enum class ETDRobotBodyDamageRejectionReason : uint8
{
	None,
	NotAuthority,
	DamageDisabled,
	StateNotInitialized,
	AlreadyDead,
	InvalidDamage,
	InvalidAttributes,
	BodyDamageEffectNotSet,
	SpecCreationFailed,
	EffectApplicationFailed
};

/** Policy used only when no Blueprint listener accepts queued external GE damage. */
UENUM(BlueprintType)
enum class ETDRobotExternalDamageFallbackPolicy : uint8
{
	ApplyToBody UMETA(DisplayName = "Apply To Body"),
	Reject UMETA(DisplayName = "Reject")
};

