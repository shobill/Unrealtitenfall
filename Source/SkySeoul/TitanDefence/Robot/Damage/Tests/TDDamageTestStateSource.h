#pragma once

#include "CoreMinimal.h"
#include "TitanDefence/Robot/Damage/TDBodyDamageReceiver.h"
#include "TDDamageTestStateSource.generated.h"

/** Native State/observer double used only by Titan Defence damage automation tests. */
UCLASS(Transient, NotBlueprintable, HideDropdown)
class UTDDamageTestStateSource : public UObject, public ITDBodyDamageReceiver
{
	GENERATED_BODY()

public:
	virtual bool ApplyResolvedBodyDamage(
		float DamageAmount, const FTDDamageResult& DamageResult) override;

	UFUNCTION()
	void ObservePartDepleted(FGameplayTag PartTag, const FTDDamageResult& DamageResult);

	UFUNCTION()
	void ObservePartResolutionFailed(const FTDDamageResult& DamageResult);

	float TotalAppliedBodyDamage = 0.0f;
	int32 ApplyCallCount = 0;
	int32 PartDepletedCount = 0;
	int32 PartResolutionFailedCount = 0;
	bool bAcceptDamage = true;
};

