#pragma once

#include "Character/Abilities/Attributes/CharacterAttributeSetBase.h"
#include "TitanDefence/Robot/Damage/TDDamageTypes.h"
#include "TDRobotAttributeSet.generated.h"

class UGameplayEffect;
struct FGameplayEffectModCallbackData;

DECLARE_MULTICAST_DELEGATE_OneParam(
	FTDRobotExternalGameplayEffectDamageDelegate,
	const FTDRobotExternalGameplayEffectDamage&);

/**
 * Titan Defence robot attributes using the project's shared character contract.
 *
 * Health, MaxHealth, and Damage are intentionally inherited from
 * UCharacterAttributeSetBase so gameplay effects can target robots and player
 * characters through the same attributes.
 */
UCLASS()
class SKYSEOUL_API UTDRobotAttributeSet : public UCharacterAttributeSetBase
{
	GENERATED_BODY()

public:
	virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;

	void SetInternalBodyDamageEffectClass(TSubclassOf<UGameplayEffect> InEffectClass)
	{
		InternalBodyDamageEffectClass = InEffectClass;
	}

	FTDRobotExternalGameplayEffectDamageDelegate& OnExternalGameplayEffectDamage()
	{
		return ExternalGameplayEffectDamageDelegate;
	}

private:
	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> InternalBodyDamageEffectClass;

	FTDRobotExternalGameplayEffectDamageDelegate ExternalGameplayEffectDamageDelegate;
};

