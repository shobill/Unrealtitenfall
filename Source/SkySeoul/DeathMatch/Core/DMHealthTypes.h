#pragma once

#include "CoreMinimal.h"
#include "DMHealthTypes.generated.h"

UENUM(BlueprintType)
enum class EDMHealthSource : uint8
{
	Invalid UMETA(DisplayName = "Invalid"),
	BodyHP UMETA(DisplayName = "Body HP"),
	GAS UMETA(DisplayName = "GAS"),
	PublicData UMETA(DisplayName = "Public Data")
};

USTRUCT(BlueprintType)
struct SKYSEOUL_API FDMHealthSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Health")
	float CurrentHealth = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Health")
	float MaxHealth = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Health")
	float HealthRatio = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Health")
	bool bAlive = false;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Health")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Health")
	EDMHealthSource HealthSource = EDMHealthSource::Invalid;

	bool IsNearlyEqual(const FDMHealthSnapshot& Other, float Tolerance = KINDA_SMALL_NUMBER) const
	{
		return bValid == Other.bValid
			&& bAlive == Other.bAlive
			&& HealthSource == Other.HealthSource
			&& FMath::IsNearlyEqual(CurrentHealth, Other.CurrentHealth, Tolerance)
			&& FMath::IsNearlyEqual(MaxHealth, Other.MaxHealth, Tolerance)
			&& FMath::IsNearlyEqual(HealthRatio, Other.HealthRatio, Tolerance);
	}
};

USTRUCT(BlueprintType)
struct SKYSEOUL_API FDMHealthBarDisplayOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Health Bar")
	bool bShowActorName = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Health Bar")
	bool bShowNumericHealth = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Health Bar")
	bool bShowHealthSource = false;
};

