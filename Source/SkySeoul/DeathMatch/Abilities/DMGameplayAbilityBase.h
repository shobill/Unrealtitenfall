#pragma once

#include "CoreMinimal.h"
#include "Character/Abilities/CharacterGameplayAbility.h"
#include "DMGameplayAbilityBase.generated.h"

class ADeathMatchRobotCharacter;
class APartsCharacterBase;

/**
 * DeathMatch combat ability base.
 * BT/AI activates by AbilityTags (not player InputTag).
 */
UCLASS(Abstract)
class SKYSEOUL_API UDMGameplayAbilityBase : public UCharacterGameplayAbility
{
	GENERATED_BODY()

public:
	UDMGameplayAbilityBase();

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags) const override;

protected:
	APartsCharacterBase* GetPartsCharacterFromActorInfo() const;
	ADeathMatchRobotCharacter* GetDeathMatchRobotFromActorInfo() const;

	/** If true, Parts CanShoot() must pass (arms not fully disabled). */
	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|Ability")
	bool bRequireCanShoot = true;

	/** Optional focus/target actor for AI (set via event payload later; readable in BP). */
	UFUNCTION(BlueprintPure, Category = "DeathMatch|Ability")
	AActor* GetDesiredTarget() const;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Ability")
	TWeakObjectPtr<AActor> DesiredTarget;
};
