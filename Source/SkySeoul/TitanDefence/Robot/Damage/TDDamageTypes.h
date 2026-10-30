#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TDDamageTypes.generated.h"

class AActor;
class AController;
class UDamageType;
class UGameplayEffect;

/** Damage context captured when an external Gameplay Effect tries to modify robot Health. */
USTRUCT(BlueprintType)
struct SKYSEOUL_API FTDRobotExternalGameplayEffectDamage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	float Damage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	FHitResult HitResult;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	FName HitBoneName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	FVector HitLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	TObjectPtr<AActor> SourceActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	TObjectPtr<AController> InstigatorController = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	TSubclassOf<UGameplayEffect> GameplayEffectClass;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	bool bHasValidHitResult = false;
};

/** Identifies the Unreal damage event that produced a resolved result. */
UENUM(BlueprintType)
enum class ETDDamageEventType : uint8
{
	Generic,
	Point,
	Radial
};

/** Machine-readable reason why a damage request could not be completed. */
UENUM(BlueprintType)
enum class ETDDamageRejectionReason : uint8
{
	None,
	DamageDisabled,
	InvalidDamage,
	InvalidConfiguration,
	BodyDamageReceiverNotSet,
	BodyDamageReceiverInvalid,
	BodyDamageReceiverRejected,
	PartDataAssetNotSet,
	PartStatesNotInitialized,
	PartNotResolved
};

/** Common Blueprint-facing context and outcome for body and robot-part damage. */
USTRUCT(BlueprintType)
struct SKYSEOUL_API FTDDamageResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	float OriginalDamage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	float FinalBodyDamage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	TObjectPtr<AActor> DamageCauser = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	TObjectPtr<AController> InstigatorController = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	TSubclassOf<UDamageType> DamageType;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	FVector HitLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	FName HitBoneName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	FGameplayTag PartTag;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	ETDDamageRejectionReason RejectionReason = ETDDamageRejectionReason::None;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	ETDDamageEventType DamageEventType = ETDDamageEventType::Generic;
};

/** Per-part authoring rule. Maximum health remains owned by the part data asset. */
USTRUCT(BlueprintType)
struct SKYSEOUL_API FTDRobotPartDamageRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	FGameplayTag PartTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0"))
	float PartDamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0"))
	float BodyDamageMultiplier = 1.0f;
};

/** Runtime-only health owned by the robot-part damage component. */
USTRUCT(BlueprintType)
struct SKYSEOUL_API FTDRobotPartRuntimeState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	FGameplayTag PartTag;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	float CurrentHealth = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	bool bHealthDepleted = false;
};

