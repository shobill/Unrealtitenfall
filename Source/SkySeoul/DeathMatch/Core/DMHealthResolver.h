#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DeathMatch/Core/DMHealthTypes.h"
#include "DMHealthResolver.generated.h"

class AActor;

namespace DMHealth
{
	SKYSEOUL_API FDMHealthSnapshot MakeSnapshot(float CurrentHealth, float MaxHealth, bool bExistingAliveState, EDMHealthSource Source);
	SKYSEOUL_API FDMHealthSnapshot Resolve(const AActor* Actor);
}

UCLASS()
class SKYSEOUL_API UDMHealthFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "DeathMatch|Health")
	static FDMHealthSnapshot GetHealthSnapshot(const AActor* Actor);
};

