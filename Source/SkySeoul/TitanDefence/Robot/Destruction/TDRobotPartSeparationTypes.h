#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TDRobotPartSeparationTypes.generated.h"

/** Result of one transactional part-separation request. */
UENUM(BlueprintType)
enum class ETDRobotPartSeparationResult : uint8
{
	Succeeded UMETA(DisplayName = "Success"),
	ComponentDisabled UMETA(DisplayName = "Component Disabled"),
	DataAssetNotSet UMETA(DisplayName = "Data Asset Not Set"),
	PartNotFound UMETA(DisplayName = "Part Not Found"),
	AlreadySeparated UMETA(DisplayName = "Already Separated"),
	ConfigurationError UMETA(DisplayName = "Configuration Error"),
	PhysicsCreationFailed UMETA(DisplayName = "Physics Creation Failed"),
	InternalProcessingFailed UMETA(DisplayName = "Internal Processing Failed"),
};

/** Runtime separation behavior. This is intentionally not part-definition data. */
USTRUCT(BlueprintType)
struct SKYSEOUL_API FTDRobotPartSeparationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics", meta = (ClampMin = "0.01", Units = "kg"))
	float MassKg = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics", meta = (ClampMin = "0.0"))
	float LinearDamping = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics", meta = (ClampMin = "0.0"))
	float AngularDamping = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics", meta = (ClampMin = "0.0", Units = "deg/s"))
	float MaxAngularVelocityDegrees = 90.0f;

	/** Applied with velocity-change disabled, so body mass participates in the response. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics", meta = (ClampMin = "0.0"))
	float SeparationImpulse = 50000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UpwardImpulseRatio = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics", meta = (ClampMin = "0.0"))
	float Friction = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Restitution = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics", meta = (ClampMin = "0.1", Units = "s"))
	float DebrisLifetime = 20.0f;
};

/** Optional component-local settings replacement for one part. */
USTRUCT(BlueprintType)
struct SKYSEOUL_API FTDRobotPartSeparationOverride
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Override")
	FGameplayTag PartTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Override")
	bool bOverrideSettings = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Override", meta = (EditCondition = "bOverrideSettings"))
	FTDRobotPartSeparationSettings Settings;
};
