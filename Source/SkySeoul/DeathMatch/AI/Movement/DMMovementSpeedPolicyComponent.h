#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DMMovementPolicyTypes.h"
#include "DMMovementSpeedPolicyComponent.generated.h"

enum class EDMActionIntent : uint8;

UCLASS(ClassGroup=(DeathMatch), meta=(BlueprintSpawnableComponent))
class SKYSEOUL_API UDMMovementSpeedPolicyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDMMovementSpeedPolicyComponent();
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category="DeathMatch|Movement Speed")
	void SetSpeedMultiplier(EDMMovementSpeedSource Source, float Multiplier);

	UFUNCTION(BlueprintCallable, Category="DeathMatch|Movement Speed")
	void ClearSpeedMultiplier(EDMMovementSpeedSource Source);

	UFUNCTION(BlueprintCallable, Category="DeathMatch|Movement Speed")
	void ClearAllSpeedMultipliers();
	void SetActionIntent(EDMActionIntent Intent);
	static float GetActionIntentMultiplier(EDMActionIntent Intent);

	UFUNCTION(BlueprintPure, Category="DeathMatch|Movement Speed")
	float GetAppliedMultiplier() const { return AppliedMultiplier; }

	UFUNCTION(BlueprintPure, Category="DeathMatch|Movement Speed")
	float GetBaseWalkSpeed() const { return BaseWalkSpeed; }

	UFUNCTION(BlueprintPure, Category="DeathMatch|Movement Speed")
	FName GetLimitingSourceName() const { return LimitingSourceName; }
	static float CalculateAppliedMultiplier(const TMap<EDMMovementSpeedSource, float>& InMultipliers,
		EDMMovementSpeedSource* OutLimitingSource = nullptr);

private:
	void CaptureBaseSpeed();
	void ApplyPolicy();

	TMap<EDMMovementSpeedSource, float> Multipliers;
	float BaseWalkSpeed = 0.f;
	float AppliedMultiplier = 1.f;
	FName LimitingSourceName = NAME_None;
};
