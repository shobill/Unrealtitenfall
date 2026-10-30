#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DMTacticalNavRegionTypes.h"
#include "DMTacticalEventProfile.generated.h"

/** Reusable event tuning. Region generation and AI interpretation deliberately do not live here. */
UCLASS(BlueprintType, Const, meta=(DisplayName="DeathMatch Tactical Event Profile"))
class SKYSEOUL_API UDMTacticalEventProfile : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tactical Event") EDMTacticalRegionEventType EventType = EDMTacticalRegionEventType::Combat;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tactical Event", meta=(ClampMin="0")) float BaseStrength = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tactical Event", meta=(ClampMin="0")) int32 PropagationDistance = 3;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tactical Event", meta=(ClampMin="0.01", ClampMax="1.0")) float DistanceFalloff = .65f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tactical Event", meta=(ClampMin="0.1", Units="s")) float Lifetime = 6.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tactical Event", meta=(ClampMin="0.01")) float TimeDecay = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tactical Event", meta=(ClampMin="0")) float MaximumAccumulatedStrength = 3.f;
};
